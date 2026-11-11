#include "AeroraSqliteAuditSinkTest.h"

#include <QtCore/QByteArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryDir>
#include <QtCore/QVariantMap>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtTest/QTest>

#include "AeroraAuditChain.h"
#include "AeroraAuditSink.h"
#include "AeroraLocalStore.h"
#include "AeroraSqliteAuditSink.h"
#include "QGCSqlHelper.h"

namespace {

AeroraLocalStore::SessionRecord sampleSession()
{
    AeroraLocalStore::SessionRecord record;
    record.startedUtcMs = 1700000000000LL;
    record.baseCommit = QStringLiteral("base");
    record.overlayCommit = QStringLiteral("overlay");
    record.customBuild = true;
    record.authorizationEnforced = false;
    return record;
}

/// A stamped event. Stamping once per event matters: the stamps feed the chain digest, so the test
/// must hold the exact object it recorded to reconstruct and verify the chain.
AeroraAuditEvent makeEvent(AeroraAuditEventType type, const QString& operatorId = QString(),
                           const QString& reasonCode = QString())
{
    AeroraAuditEvent event;
    event.type = type;
    event.operatorId = operatorId;
    event.reasonCode = reasonCode;
    event.vehicleMavlinkSystemId = 1;
    event.vehicleResolved = true;
    event.authorizationEnforced = true;
    event.detail = QVariantMap{{QStringLiteral("cmd"), QStringLiteral("arm")}};
    event.stampNow();
    return event;
}

struct AuditRow
{
    QString actor;
    QString action;
    QString subject;
    int outcome = -1;
    int enforced = -1;
    QString detail;
    QString prevHash;
    QString recordHash;
};

QList<AuditRow> readAuditRows(const QString& path)
{
    QList<AuditRow> rows;
    QGCSqlHelper::ScopedConnection connection(path);
    if (!connection.isValid()) {
        return rows;
    }
    QSqlQuery query(connection.database());
    if (!query.exec(QStringLiteral("SELECT actor, action, subject, outcome, enforced, detail, prevHash, recordHash "
                                   "FROM AuditRecords ORDER BY recordId"))) {
        return rows;
    }
    while (query.next()) {
        AuditRow row;
        row.actor = query.value(0).toString();
        row.action = query.value(1).toString();
        row.subject = query.value(2).toString();
        row.outcome = query.value(3).toInt();
        row.enforced = query.value(4).toInt();
        row.detail = query.value(5).toString();
        row.prevHash = query.value(6).toString();
        row.recordHash = query.value(7).toString();
        rows.append(row);
    }
    return rows;
}

/// Runs \a sql with no bound parameters on a fresh connection, for a test that tampers with stored
/// rows. Returns whether it executed.
bool runSql(const QString& path, const QString& sql)
{
    QGCSqlHelper::ScopedConnection connection(path);
    if (!connection.isValid()) {
        return false;
    }
    QSqlQuery query(connection.database());
    return query.exec(sql);
}

QString genesisHex()
{
    return QString::fromLatin1(AeroraAuditChain::genesisDigest().toHex());
}

}  // namespace

void AeroraSqliteAuditSinkTest::_openStore()
{
    _dir.reset(new QTemporaryDir());
    QVERIFY(_dir->isValid());
    _dbPath = _dir->filePath(QStringLiteral("aerora.db"));
    _store.reset(new AeroraLocalStore());
    QVERIFY(_store->open(_dbPath, false).opened);
}

void AeroraSqliteAuditSinkTest::_beginSession()
{
    _sessionId = _store->beginSession(sampleSession());
    QVERIFY(_sessionId > 0);
}

void AeroraSqliteAuditSinkTest::_openSink()
{
    _sink.reset(new AeroraSqliteAuditSink(*_store));
    QVERIFY(_sink->open());
}

void AeroraSqliteAuditSinkTest::_openCommonFixture()
{
    _openStore();
    _beginSession();
    _openSink();
}

void AeroraSqliteAuditSinkTest::cleanup()
{
    // Close in dependency order: the sink's connection, then the store's, then delete the files.
    _sink.reset();
    _store.reset();
    _dir.reset();
    _sessionId = -1;
    _dbPath.clear();

    UnitTest::cleanup();
}

void AeroraSqliteAuditSinkTest::_recordIsAcceptedAndPersisted()
{
    _openCommonFixture();

    const AeroraAuditEvent event = makeEvent(AeroraAuditEventType::AuthorizationDecision, QStringLiteral("op-7"));
    QCOMPARE(_sink->record(event), AeroraAuditWriteOutcome::Accepted);

    const QList<AuditRow> rows = readAuditRows(_dbPath);
    QCOMPARE(rows.count(), 1);
    QCOMPARE(rows.at(0).actor, QStringLiteral("op-7"));
    QCOMPARE(rows.at(0).action, AeroraAuditEvent::typeToString(AeroraAuditEventType::AuthorizationDecision));
    QCOMPARE(rows.at(0).outcome, static_cast<int>(AeroraAuditOutcome::Allowed));
    QCOMPARE(rows.at(0).enforced, 1);
    QCOMPARE(rows.at(0).subject, QStringLiteral("vehicle 1"));
    QCOMPARE(rows.at(0).prevHash, genesisHex());
    QVERIFY(!rows.at(0).recordHash.isEmpty());
}

void AeroraSqliteAuditSinkTest::_emptyOperatorBecomesUnresolved()
{
    _openCommonFixture();

    QCOMPARE(_sink->record(makeEvent(AeroraAuditEventType::CommandRequested, QString())),
             AeroraAuditWriteOutcome::Accepted);
    QCOMPARE(_sink->record(makeEvent(AeroraAuditEventType::CommandRequested, QStringLiteral("named"))),
             AeroraAuditWriteOutcome::Accepted);

    const QList<AuditRow> rows = readAuditRows(_dbPath);
    QCOMPARE(rows.count(), 2);
    // The writer substitutes the literal; the producer never backfills it (AeroraAuditSink.h §2.8).
    QCOMPARE(rows.at(0).actor, QStringLiteral("unresolved"));
    QCOMPARE(rows.at(1).actor, QStringLiteral("named"));
}

void AeroraSqliteAuditSinkTest::_classifyOutcomeMatchesTheApprovedTable()
{
    using Type = AeroraAuditEventType;

    const auto classify = [](Type type, const QString& reason = QString()) {
        AeroraAuditEvent event;
        event.type = type;
        event.reasonCode = reason;
        return AeroraSqliteAuditSink::classifyOutcome(event);
    };

    QCOMPARE(classify(Type::CommandDenied), AeroraAuditOutcome::Denied);
    QCOMPARE(classify(Type::ManualAuthorityRefused), AeroraAuditOutcome::Denied);
    QCOMPARE(classify(Type::AuthorizationDecision, QStringLiteral("role")), AeroraAuditOutcome::Denied);

    QCOMPARE(classify(Type::AuthorizationDecision), AeroraAuditOutcome::Allowed);
    QCOMPARE(classify(Type::CommandTransmitted), AeroraAuditOutcome::Allowed);
    QCOMPARE(classify(Type::ManualAuthorityIssued), AeroraAuditOutcome::Allowed);
    QCOMPARE(classify(Type::ManualAuthorityRefreshed), AeroraAuditOutcome::Allowed);

    QCOMPARE(classify(Type::CommandRequested), AeroraAuditOutcome::Informational);
    QCOMPARE(classify(Type::OperatorIdentityResolved), AeroraAuditOutcome::Informational);
    QCOMPARE(classify(Type::VehicleResponse), AeroraAuditOutcome::Informational);
    QCOMPARE(classify(Type::SecurityEvent), AeroraAuditOutcome::Informational);
    QCOMPARE(classify(Type::SystemEvent), AeroraAuditOutcome::Informational);
    QCOMPARE(classify(Type::ManualAuthorityExpired), AeroraAuditOutcome::Informational);
}

void AeroraSqliteAuditSinkTest::_deniedAuthorizationDecisionIsPersistedAsDenied()
{
    _openCommonFixture();

    // An AuthorizationDecision carrying a reason is a denial; the same type with no reason is an
    // allow. The reasonCode is what the outcome turns on for this type.
    QCOMPARE(_sink->record(makeEvent(AeroraAuditEventType::AuthorizationDecision, QStringLiteral("op"),
                                     QStringLiteral("role not resolved"))),
             AeroraAuditWriteOutcome::Accepted);

    const QList<AuditRow> rows = readAuditRows(_dbPath);
    QCOMPARE(rows.count(), 1);
    QCOMPARE(rows.at(0).outcome, static_cast<int>(AeroraAuditOutcome::Denied));
}

void AeroraSqliteAuditSinkTest::_detailRoundTripsThroughJson()
{
    _openCommonFixture();

    QCOMPARE(_sink->record(makeEvent(AeroraAuditEventType::CommandRequested, QStringLiteral("op"))),
             AeroraAuditWriteOutcome::Accepted);

    const QList<AuditRow> rows = readAuditRows(_dbPath);
    QCOMPARE(rows.count(), 1);

    const QJsonDocument parsed = QJsonDocument::fromJson(rows.at(0).detail.toUtf8());
    QVERIFY(parsed.isObject());
    QCOMPARE(parsed.object().value(QStringLiteral("cmd")).toString(), QStringLiteral("arm"));
}

void AeroraSqliteAuditSinkTest::_firstRecordChainsOntoGenesisAndRecordsLink()
{
    _openCommonFixture();

    for (int i = 0; i < 3; ++i) {
        QCOMPARE(_sink->record(makeEvent(AeroraAuditEventType::AuthorizationDecision)),
                 AeroraAuditWriteOutcome::Accepted);
    }

    const QList<AuditRow> rows = readAuditRows(_dbPath);
    QCOMPARE(rows.count(), 3);

    QCOMPARE(rows.at(0).prevHash, genesisHex());
    for (int i = 1; i < rows.count(); ++i) {
        QCOMPARE(rows.at(i).prevHash, rows.at(i - 1).recordHash);
        QVERIFY(!rows.at(i).recordHash.isEmpty());
    }
}

void AeroraSqliteAuditSinkTest::_chainContinuesAcrossReopen()
{
    _openCommonFixture();

    QCOMPARE(_sink->record(makeEvent(AeroraAuditEventType::AuthorizationDecision)), AeroraAuditWriteOutcome::Accepted);
    QCOMPARE(_sink->record(makeEvent(AeroraAuditEventType::AuthorizationDecision)), AeroraAuditWriteOutcome::Accepted);

    // A close and reopen must re-seed the head from the last stored record, not restart at genesis.
    _sink->close();
    QVERIFY(_sink->open());

    QCOMPARE(_sink->record(makeEvent(AeroraAuditEventType::AuthorizationDecision)), AeroraAuditWriteOutcome::Accepted);

    const QList<AuditRow> rows = readAuditRows(_dbPath);
    QCOMPARE(rows.count(), 3);
    QCOMPARE(rows.at(2).prevHash, rows.at(1).recordHash);
}

void AeroraSqliteAuditSinkTest::_storedChainVerifiesWithOriginalEvents()
{
    _openCommonFixture();

    QList<AeroraAuditEvent> events;
    for (int i = 0; i < 4; ++i) {
        const AeroraAuditEvent event =
            makeEvent(AeroraAuditEventType::AuthorizationDecision, QStringLiteral("op-%1").arg(i));
        events.append(event);
        QCOMPARE(_sink->record(event), AeroraAuditWriteOutcome::Accepted);
    }

    const QList<AuditRow> rows = readAuditRows(_dbPath);
    QCOMPARE(rows.count(), events.count());

    QList<AeroraAuditChain::Entry> entries;
    for (int i = 0; i < rows.count(); ++i) {
        AeroraAuditChain::Entry entry;
        entry.event = events.at(i);
        entry.previousDigest = QByteArray::fromHex(rows.at(i).prevHash.toLatin1());
        entry.digest = QByteArray::fromHex(rows.at(i).recordHash.toLatin1());
        entries.append(entry);
    }

    const AeroraAuditChain::Verification verification = AeroraAuditChain::verify(entries);
    QVERIFY2(verification.intact, qPrintable(verification.reason));
    QCOMPARE(verification.brokenAt, qsizetype(-1));
}

void AeroraSqliteAuditSinkTest::_editingAStoredHashBreaksVerification()
{
    _openCommonFixture();

    QList<AeroraAuditEvent> events;
    for (int i = 0; i < 3; ++i) {
        const AeroraAuditEvent event = makeEvent(AeroraAuditEventType::AuthorizationDecision);
        events.append(event);
        QCOMPARE(_sink->record(event), AeroraAuditWriteOutcome::Accepted);
    }

    // Edit the second record's digest in place. recordId is AUTOINCREMENT from 1 on a fresh store,
    // so the second row is recordId 2.
    const QString tamperedHash = QString::fromLatin1(QByteArray(32, '\0').toHex());
    QVERIFY(runSql(_dbPath,
                   QStringLiteral("UPDATE AuditRecords SET recordHash = '%1' WHERE recordId = 2").arg(tamperedHash)));

    const QList<AuditRow> rows = readAuditRows(_dbPath);
    QCOMPARE(rows.count(), 3);

    QList<AeroraAuditChain::Entry> entries;
    for (int i = 0; i < rows.count(); ++i) {
        AeroraAuditChain::Entry entry;
        entry.event = events.at(i);
        entry.previousDigest = QByteArray::fromHex(rows.at(i).prevHash.toLatin1());
        entry.digest = QByteArray::fromHex(rows.at(i).recordHash.toLatin1());
        entries.append(entry);
    }

    const AeroraAuditChain::Verification verification = AeroraAuditChain::verify(entries);
    QVERIFY(!verification.intact);
    QCOMPARE(verification.brokenAt, qsizetype(1));
}

void AeroraSqliteAuditSinkTest::_deletingARecordBreaksVerification()
{
    _openCommonFixture();

    QList<AeroraAuditEvent> events;
    for (int i = 0; i < 3; ++i) {
        const AeroraAuditEvent event = makeEvent(AeroraAuditEventType::AuthorizationDecision);
        events.append(event);
        QCOMPARE(_sink->record(event), AeroraAuditWriteOutcome::Accepted);
    }

    // Remove the middle record. Deleting an audit row is permitted; ON DELETE RESTRICT guards the
    // parent session, not the audit rows themselves.
    QVERIFY(runSql(_dbPath, QStringLiteral("DELETE FROM AuditRecords WHERE recordId = 2")));

    const QList<AuditRow> rows = readAuditRows(_dbPath);
    QCOMPARE(rows.count(), 2);

    // The remaining rows are the first and third records, so they carry the first and third events.
    QList<AeroraAuditChain::Entry> entries;
    const QList<int> remaining = {0, 2};
    for (int i = 0; i < rows.count(); ++i) {
        AeroraAuditChain::Entry entry;
        entry.event = events.at(remaining.at(i));
        entry.previousDigest = QByteArray::fromHex(rows.at(i).prevHash.toLatin1());
        entry.digest = QByteArray::fromHex(rows.at(i).recordHash.toLatin1());
        entries.append(entry);
    }

    const AeroraAuditChain::Verification verification = AeroraAuditChain::verify(entries);
    QVERIFY(!verification.intact);
    QCOMPARE(verification.brokenAt, qsizetype(1));
}

void AeroraSqliteAuditSinkTest::_recordWithoutAnActiveSessionFailsStorage()
{
    // Store open, sink open, but no session begun: every AuditRecords row needs one.
    _openStore();
    _openSink();

    QCOMPARE(_store->currentSessionId(), qint64(-1));
    QCOMPARE(_sink->record(makeEvent(AeroraAuditEventType::AuthorizationDecision)),
             AeroraAuditWriteOutcome::FailedStorage);

    QCOMPARE(readAuditRows(_dbPath).count(), 0);
}

void AeroraSqliteAuditSinkTest::_recordBeforeOpenIsSinkUnavailable()
{
    _openStore();
    _beginSession();
    _sink.reset(new AeroraSqliteAuditSink(*_store));  // constructed, deliberately not opened

    QCOMPARE(_sink->record(makeEvent(AeroraAuditEventType::AuthorizationDecision)),
             AeroraAuditWriteOutcome::SinkUnavailable);
    QVERIFY(!_sink->isDurable());
}

void AeroraSqliteAuditSinkTest::_isDurableOnlyWhenOpenAndNeverTamperEvident()
{
    _openCommonFixture();

    QVERIFY(_sink->isOpen());
    QVERIFY2(_sink->isDurable(), "an open synchronous=FULL audit sink is durable (WP1-C §7)");
    QVERIFY2(!_sink->isTamperEvident(), "a hash chain is not tamper-evidence - no protected anchor");
    QVERIFY(!_sink->sinkName().isEmpty());

    _sink->close();
    QVERIFY2(!_sink->isDurable(), "a closed sink writes nothing and must not claim durability");
}

void AeroraSqliteAuditSinkTest::_openFailsWhenStoreIsNotOpen()
{
    _dir.reset(new QTemporaryDir());
    QVERIFY(_dir->isValid());
    _store.reset(new AeroraLocalStore());  // constructed, deliberately not opened
    _sink.reset(new AeroraSqliteAuditSink(*_store));

    expectLogMessage("aerora.audit", QtWarningMsg, QRegularExpression(QStringLiteral("store is not open")));
    QVERIFY(!_sink->open());
    verifyExpectedLogMessage();

    QVERIFY(!_sink->isOpen());
}

UT_REGISTER_TEST(AeroraSqliteAuditSinkTest, TestLabel::Unit)

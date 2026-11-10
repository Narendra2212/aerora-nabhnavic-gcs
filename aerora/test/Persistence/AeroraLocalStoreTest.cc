#include "AeroraLocalStoreTest.h"

#include <optional>

#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QTemporaryDir>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtTest/QTest>

#include "AeroraLocalStore.h"
#include "QGCSqlHelper.h"

namespace {

/// A populated session record, so a round-trip proves every column survives, not just the id.
AeroraLocalStore::SessionRecord sampleSession()
{
    AeroraLocalStore::SessionRecord record;
    record.startedUtcMs = 1700000000000LL;
    record.baseCommit = QStringLiteral("basecommit0123");
    record.overlayCommit = QStringLiteral("overlaycommit4567");
    record.customBuild = true;
    record.authorizationEnforced = false;
    return record;
}

/// Whether a `type`/`name` object exists in the file's schema, read through an independent
/// connection so the assertion sees what was committed rather than what the store holds.
bool schemaObjectExists(const QString& dbPath, const QString& type, const QString& name)
{
    QGCSqlHelper::ScopedConnection connection(dbPath);
    if (!connection.isValid()) {
        return false;
    }
    QSqlQuery query(connection.database());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE type = ? AND name = ?"));
    query.addBindValue(type);
    query.addBindValue(name);
    if (!query.exec() || !query.next()) {
        return false;
    }
    return query.value(0).toInt() > 0;
}

}  // namespace

void AeroraLocalStoreTest::_disabledPersistenceOpensNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    const AeroraLocalStore::OpenResult result = store.open(path, true);

    QVERIFY(!result.opened);
    QVERIFY(result.persistenceDisabled);
    QVERIFY(store.isPersistenceDisabled());
    QVERIFY(!store.isOpen());
    // P7-DAT-005: nothing is written to disk when persistence is disabled.
    QVERIFY(!QFile::exists(path));
}

void AeroraLocalStoreTest::_openingAFreshFileCreatesSchemaAndSetsVersion()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    const AeroraLocalStore::OpenResult result = store.open(path, false);

    QVERIFY(result.opened);
    QVERIFY(!result.versionUnsupported);
    QVERIFY(store.isOpen());
    QVERIFY(QFile::exists(path));

    QGCSqlHelper::ScopedConnection connection(path);
    QVERIFY(connection.isValid());
    QSqlDatabase db = connection.database();
    const std::optional<int> version = QGCSqlHelper::userVersion(db);
    QVERIFY(version.has_value());
    QCOMPARE(*version, AeroraLocalStore::kSchemaVersion);
}

void AeroraLocalStoreTest::_schemaCarriesEveryTableAndIndex()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    const QStringList tables = {QStringLiteral("Sessions"),     QStringLiteral("Vehicles"),
                                QStringLiteral("Events"),       QStringLiteral("EventValues"),
                                QStringLiteral("AuditRecords"), QStringLiteral("WriteFailures")};
    for (const QString& table : tables) {
        QVERIFY2(schemaObjectExists(path, QStringLiteral("table"), table), qPrintable(table));
    }

    const QStringList indices = {QStringLiteral("idx_events_session_monotonic"), QStringLiteral("idx_events_category"),
                                 QStringLiteral("idx_eventvalues_event"), QStringLiteral("idx_audit_session_monotonic"),
                                 QStringLiteral("idx_vehicles_identity")};
    for (const QString& index : indices) {
        QVERIFY2(schemaObjectExists(path, QStringLiteral("index"), index), qPrintable(index));
    }
}

void AeroraLocalStoreTest::_reopeningAnExistingStoreProceeds()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    {
        AeroraLocalStore store;
        QVERIFY(store.open(path, false).opened);
        store.close();
    }

    AeroraLocalStore reopened;
    const AeroraLocalStore::OpenResult result = reopened.open(path, false);
    QVERIFY(result.opened);
    QVERIFY(!result.versionUnsupported);
}

void AeroraLocalStoreTest::_newerSchemaVersionIsRefusedAndPreserved()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    {
        QGCSqlHelper::ScopedConnection connection(path);
        QVERIFY(connection.isValid());
        QSqlDatabase db = connection.database();
        QVERIFY(QGCSqlHelper::setUserVersion(db, AeroraLocalStore::kSchemaVersion + 1));
    }

    AeroraLocalStore store;
    expectLogMessage("aerora.persistence", QtWarningMsg, QRegularExpression(QStringLiteral("newer build")));
    const AeroraLocalStore::OpenResult result = store.open(path, false);
    verifyExpectedLogMessage();

    QVERIFY(!result.opened);
    QVERIFY(result.versionUnsupported);
    QCOMPARE(result.schemaVersionFound, AeroraLocalStore::kSchemaVersion + 1);
    QVERIFY(!store.isOpen());
    // The file this build cannot read is left untouched (design §5).
    QVERIFY(QFile::exists(path));
}

void AeroraLocalStoreTest::_legacyUnversionedDatabaseIsRefusedAndPreserved()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    {
        QGCSqlHelper::ScopedConnection connection(path);
        QVERIFY(connection.isValid());
        QSqlQuery query(connection.database());
        // A table with user_version left at 0: a legacy unversioned file holding data.
        QVERIFY(query.exec(QStringLiteral("CREATE TABLE Legacy (x INTEGER)")));
    }

    AeroraLocalStore store;
    expectLogMessage("aerora.persistence", QtWarningMsg, QRegularExpression(QStringLiteral("unversioned database")));
    const AeroraLocalStore::OpenResult result = store.open(path, false);
    verifyExpectedLogMessage();

    QVERIFY(!result.opened);
    QVERIFY(result.versionUnsupported);
    QCOMPARE(result.schemaVersionFound, 0);
    QVERIFY(QFile::exists(path));
}

void AeroraLocalStoreTest::_databasePathIsReported()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);
    QCOMPARE(store.databasePath(), path);
}

void AeroraLocalStoreTest::_beginSessionPersistsARow()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    const AeroraLocalStore::SessionRecord record = sampleSession();
    const qint64 sessionId = store.beginSession(record);
    QVERIFY(sessionId > 0);
    QCOMPARE(store.currentSessionId(), sessionId);

    QGCSqlHelper::ScopedConnection connection(path);
    QVERIFY(connection.isValid());
    QSqlQuery query(connection.database());
    query.prepare(
        QStringLiteral("SELECT startedUtcMs, endedUtcMs, baseCommit, overlayCommit, customBuild, "
                       "authzEnforced FROM Sessions WHERE sessionId = ?"));
    query.addBindValue(sessionId);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toLongLong(), record.startedUtcMs);
    QVERIFY(query.value(1).isNull());  // endedUtcMs is NULL while the session runs
    QCOMPARE(query.value(2).toString(), record.baseCommit);
    QCOMPARE(query.value(3).toString(), record.overlayCommit);
    QCOMPARE(query.value(4).toInt(), 1);
    QCOMPARE(query.value(5).toInt(), 0);
}

void AeroraLocalStoreTest::_endSessionStampsTheEndTime()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    const qint64 sessionId = store.beginSession(sampleSession());
    QVERIFY(sessionId > 0);

    const qint64 endedUtcMs = 1700000001000LL;
    QVERIFY(store.endSession(sessionId, endedUtcMs));

    QGCSqlHelper::ScopedConnection connection(path);
    QVERIFY(connection.isValid());
    QSqlQuery query(connection.database());
    query.prepare(QStringLiteral("SELECT endedUtcMs FROM Sessions WHERE sessionId = ?"));
    query.addBindValue(sessionId);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toLongLong(), endedUtcMs);
}

void AeroraLocalStoreTest::_recordedWriteFailureIsPersistedAndCounted()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);
    QVERIFY(store.beginSession(sampleSession()) > 0);

    store.recordWriteFailure(QStringLiteral("Events"), QStringLiteral("disk full"));

    QCOMPARE(store.recordedWriteFailureCount(), 1);
    QCOMPARE(store.unrecordedWriteFailureCount(), 0);

    QGCSqlHelper::ScopedConnection connection(path);
    QVERIFY(connection.isValid());
    QSqlQuery query(connection.database());
    QVERIFY(query.exec(QStringLiteral("SELECT targetTable, reason FROM WriteFailures")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("Events"));
    QCOMPARE(query.value(1).toString(), QStringLiteral("disk full"));
}

void AeroraLocalStoreTest::_writeFailureWithoutASessionIsUnrecorded()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    // No beginSession(): WriteFailures needs a session to reference, so this cannot be persisted.
    store.recordWriteFailure(QStringLiteral("Events"), QStringLiteral("no session yet"));

    QCOMPARE(store.unrecordedWriteFailureCount(), 1);
    QCOMPARE(store.recordedWriteFailureCount(), 0);
}

void AeroraLocalStoreTest::_writeFailureWithoutAStoreIsUnrecorded()
{
    AeroraLocalStore store;  // never opened

    store.recordWriteFailure(QStringLiteral("Events"), QStringLiteral("no store"));

    QCOMPARE(store.unrecordedWriteFailureCount(), 1);
    QCOMPARE(store.recordedWriteFailureCount(), 0);
}

UT_REGISTER_TEST(AeroraLocalStoreTest, TestLabel::Unit)

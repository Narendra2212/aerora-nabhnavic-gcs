#include "AeroraStoreRetentionTest.h"

#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QTemporaryDir>
#include <QtCore/QVariant>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtTest/QTest>

#include "AeroraLocalStore.h"
#include "AeroraStoreRetention.h"
#include "QGCSqlHelper.h"

namespace {

/// A populated session the store can open an active session from.
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

/// Inserts a `Sessions` row with an explicit id, so a test can seed sessions a prune must keep or
/// remove without going through the store's own session bookkeeping.
bool seedSession(QSqlDatabase db, qint64 sessionId)
{
    QSqlQuery query(db);
    query.prepare(
        QStringLiteral("INSERT INTO Sessions (sessionId, startedUtcMs, endedUtcMs, baseCommit, overlayCommit, "
                       "customBuild, authzEnforced) VALUES (?, ?, NULL, ?, ?, 0, 0)"));
    query.addBindValue(sessionId);
    query.addBindValue(1700000000000LL);
    query.addBindValue(QStringLiteral("base"));
    query.addBindValue(QStringLiteral("overlay"));
    return query.exec();
}

/// Inserts one `Events` row and returns its id, or -1. vehicleId/stateClass/detail are left NULL:
/// the prune reads only occurredUtcMs and the session reference.
qint64 seedEvent(QSqlDatabase db, qint64 sessionId, qint64 occurredUtcMs)
{
    QSqlQuery query(db);
    query.prepare(
        QStringLiteral("INSERT INTO Events (sessionId, vehicleId, occurredUtcMs, monotonicUs, category, subject, "
                       "stateClass, detail) VALUES (?, NULL, ?, ?, 0, ?, NULL, NULL)"));
    query.addBindValue(sessionId);
    query.addBindValue(occurredUtcMs);
    query.addBindValue(occurredUtcMs);  // monotonicUs: any value, not read by the prune
    query.addBindValue(QStringLiteral("seed.event"));
    if (!query.exec()) {
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

bool seedEventValue(QSqlDatabase db, qint64 eventId, const QString& itemId)
{
    QSqlQuery query(db);
    query.prepare(
        QStringLiteral("INSERT INTO EventValues (eventId, itemId, stateClass, derived, valueText, ageMs) "
                       "VALUES (?, ?, 0, 0, NULL, NULL)"));
    query.addBindValue(eventId);
    query.addBindValue(itemId);
    return query.exec();
}

bool seedAudit(QSqlDatabase db, qint64 sessionId)
{
    QSqlQuery query(db);
    query.prepare(
        QStringLiteral("INSERT INTO AuditRecords (sessionId, occurredUtcMs, monotonicUs, actor, action, subject, "
                       "outcome, enforced, detail, prevHash, recordHash) VALUES (?, ?, 0, ?, ?, ?, 1, 1, NULL, "
                       "NULL, NULL)"));
    query.addBindValue(sessionId);
    query.addBindValue(1700000000000LL);
    query.addBindValue(QStringLiteral("operator"));
    query.addBindValue(QStringLiteral("action"));
    query.addBindValue(QStringLiteral("subject"));
    return query.exec();
}

bool seedWriteFailure(QSqlDatabase db, qint64 sessionId, qint64 occurredUtcMs)
{
    QSqlQuery query(db);
    query.prepare(
        QStringLiteral("INSERT INTO WriteFailures (sessionId, occurredUtcMs, targetTable, reason) "
                       "VALUES (?, ?, ?, ?)"));
    query.addBindValue(sessionId);
    query.addBindValue(occurredUtcMs);
    query.addBindValue(QStringLiteral("Events"));
    query.addBindValue(QStringLiteral("seed"));
    return query.exec();
}

int rowCount(const QString& path, const QString& table)
{
    QGCSqlHelper::ScopedConnection connection(path);
    if (!connection.isValid()) {
        return -1;
    }
    QSqlQuery query(connection.database());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table)) || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

bool sessionExists(const QString& path, qint64 sessionId)
{
    QGCSqlHelper::ScopedConnection connection(path);
    if (!connection.isValid()) {
        return false;
    }
    QSqlQuery query(connection.database());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM Sessions WHERE sessionId = ?"));
    query.addBindValue(sessionId);
    if (!query.exec() || !query.next()) {
        return false;
    }
    return query.value(0).toInt() > 0;
}

}  // namespace

void AeroraStoreRetentionTest::_pruneEventsRemovesOldAndCascadesValues()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    const qint64 cutoff = 1700000000000LL;
    {
        QGCSqlHelper::ScopedConnection seed(path);
        QVERIFY(seed.isValid());
        QSqlDatabase db = seed.database();
        QVERIFY(seedSession(db, 1));

        const qint64 oldEvent = seedEvent(db, 1, cutoff - 1000);  // before the cutoff: pruned
        QVERIFY(oldEvent > 0);
        QVERIFY(seedEventValue(db, oldEvent, QStringLiteral("battery.voltage")));
        QVERIFY(seedEventValue(db, oldEvent, QStringLiteral("gps.fix")));

        const qint64 newEvent = seedEvent(db, 1, cutoff + 1000);  // after the cutoff: kept
        QVERIFY(newEvent > 0);
        QVERIFY(seedEventValue(db, newEvent, QStringLiteral("altitude")));
    }

    QCOMPARE(rowCount(path, QStringLiteral("Events")), 2);
    QCOMPARE(rowCount(path, QStringLiteral("EventValues")), 3);

    AeroraStoreRetention retention(store);
    QVERIFY(retention.open());
    QCOMPARE(retention.pruneEventsOlderThan(cutoff), 1);

    QCOMPARE(rowCount(path, QStringLiteral("Events")), 1);
    // The pruned event took its two values; the kept event's single value remains.
    QCOMPARE(rowCount(path, QStringLiteral("EventValues")), 1);
}

void AeroraStoreRetentionTest::_pruneEventsSpansMultipleBatches()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    const qint64 cutoff = 1700000000000LL;
    const int count = 600;  // larger than one batch, so the loop must run more than once
    {
        QGCSqlHelper::ScopedConnection seed(path);
        QVERIFY(seed.isValid());
        QSqlDatabase db = seed.database();
        QGCSqlHelper::Transaction transaction(db);
        QVERIFY(transaction.ok());
        QVERIFY(seedSession(db, 1));
        for (int i = 0; i < count; ++i) {
            QVERIFY(seedEvent(db, 1, cutoff - 1000) > 0);
        }
        QVERIFY(transaction.commit());
    }

    QCOMPARE(rowCount(path, QStringLiteral("Events")), count);

    AeroraStoreRetention retention(store);
    QVERIFY(retention.open());
    QCOMPARE(retention.pruneEventsOlderThan(cutoff), count);
    QCOMPARE(rowCount(path, QStringLiteral("Events")), 0);
}

void AeroraStoreRetentionTest::_pruneWriteFailuresRemovesOldRows()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    const qint64 cutoff = 1700000000000LL;
    {
        QGCSqlHelper::ScopedConnection seed(path);
        QVERIFY(seed.isValid());
        QSqlDatabase db = seed.database();
        QVERIFY(seedSession(db, 1));
        QVERIFY(seedWriteFailure(db, 1, cutoff - 5000));  // old: pruned
        QVERIFY(seedWriteFailure(db, 1, cutoff - 1));     // old: pruned
        QVERIFY(seedWriteFailure(db, 1, cutoff + 5000));  // recent: kept
    }

    QCOMPARE(rowCount(path, QStringLiteral("WriteFailures")), 3);

    AeroraStoreRetention retention(store);
    QVERIFY(retention.open());
    QCOMPARE(retention.pruneWriteFailuresOlderThan(cutoff), 2);
    QCOMPARE(rowCount(path, QStringLiteral("WriteFailures")), 1);
}

void AeroraStoreRetentionTest::_pruneEmptySessionsKeepsReferencedAndActive()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    // The store's own session is the active one: empty, but kept because it is current.
    const qint64 activeSession = store.beginSession(sampleSession());
    QVERIFY(activeSession > 0);

    const qint64 emptySession = 101;
    const qint64 eventSession = 102;
    const qint64 auditSession = 103;
    {
        QGCSqlHelper::ScopedConnection seed(path);
        QVERIFY(seed.isValid());
        QSqlDatabase db = seed.database();
        QVERIFY(seedSession(db, emptySession));
        QVERIFY(seedSession(db, eventSession));
        QVERIFY(seedSession(db, auditSession));
        QVERIFY(seedEvent(db, eventSession, 1700000000000LL) > 0);
        QVERIFY(seedAudit(db, auditSession));
    }

    QCOMPARE(rowCount(path, QStringLiteral("Sessions")), 4);

    AeroraStoreRetention retention(store);
    QVERIFY(retention.open());
    // Only the empty, non-active session is swept.
    QCOMPARE(retention.pruneEmptySessions(), 1);

    QCOMPARE(rowCount(path, QStringLiteral("Sessions")), 3);
    QVERIFY(sessionExists(path, activeSession));
    QVERIFY(sessionExists(path, eventSession));
    QVERIFY(sessionExists(path, auditSession));
    QVERIFY(!sessionExists(path, emptySession));
}

void AeroraStoreRetentionTest::_pruneBeforeOpenReturnsMinusOne()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    AeroraStoreRetention retention(store);  // deliberately not opened
    QCOMPARE(retention.pruneEventsOlderThan(1700000000000LL), -1);
    QCOMPARE(retention.pruneWriteFailuresOlderThan(1700000000000LL), -1);
    QCOMPARE(retention.pruneEmptySessions(), -1);
}

void AeroraStoreRetentionTest::_openFailsWhenStoreIsNotOpen()
{
    AeroraLocalStore store;  // deliberately not opened
    AeroraStoreRetention retention(store);

    expectLogMessage("aerora.persistence.retention", QtWarningMsg,
                     QRegularExpression(QStringLiteral("store is not open")));
    QVERIFY(!retention.open());
    verifyExpectedLogMessage();

    QVERIFY(!retention.isOpen());
}

UT_REGISTER_TEST(AeroraStoreRetentionTest, TestLabel::Unit)

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

    // The size prune and its measurement answer the same way before open, and neither probes a
    // connection it does not have.
    QCOMPARE(retention.eventBytes(), -1);
    QCOMPARE(retention.pruneEventsToFitBytes(1024), -1);
    QVERIFY(!retention.canMeasureEventBytes());
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

void AeroraStoreRetentionTest::_eventBytesAgreesWithTheMeasurementProbe()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    AeroraStoreRetention retention(store);
    QVERIFY(retention.open());

    // The two must not drift apart: a reported measurement the prune would refuse to trust, or a
    // prune willing to run against a measurement that is not available, are both failures.
    const bool measurable = retention.canMeasureEventBytes();
    const qint64 bytes = retention.eventBytes();

    QCOMPARE(bytes >= 0, measurable);

    // And the probe is stable. It is cached after the first call, so a second must not disagree.
    QCOMPARE(retention.canMeasureEventBytes(), measurable);
}

void AeroraStoreRetentionTest::_aSizePruneNeverDeletesAnAuditRecord()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    {
        QGCSqlHelper::ScopedConnection seed(path);
        QVERIFY(seed.isValid());
        QSqlDatabase db = seed.database();
        QVERIFY(seedSession(db, 1));

        for (int i = 0; i < 40; ++i) {
            const qint64 eventId = seedEvent(db, 1, 1700000000000LL + i);
            QVERIFY(eventId > 0);
            QVERIFY(seedEventValue(db, eventId, QStringLiteral("battery.voltage")));
        }
        for (int i = 0; i < 10; ++i) {
            QVERIFY(seedAudit(db, 1));
        }
    }

    QCOMPARE(rowCount(path, QStringLiteral("Events")), 40);
    QCOMPARE(rowCount(path, QStringLiteral("AuditRecords")), 10);

    AeroraStoreRetention retention(store);
    QVERIFY(retention.open());

    // A cap of zero is the most aggressive request the interface can make: no event log is small
    // enough, so the prune removes everything it is permitted to remove. Asserted without
    // depending on whether this build can measure bytes, because the property has to hold either
    // way - if the prune ran it must have spared the audit trail, and if it refused it must have
    // deleted nothing at all.
    const bool measurable = retention.canMeasureEventBytes();

    // Tolerated rather than required, because which of the two fires depends on the build and, on
    // a measuring build, on whether the emptied tables still report pages. The return value and
    // the row counts below are the assertions; these only keep strict log mode from failing on a
    // warning that is correct either way.
    ignoreLogMessage("aerora.persistence.retention", QtWarningMsg,
                     QRegularExpression(QStringLiteral("refusing to enforce a size cap")));
    ignoreLogMessage("aerora.persistence.retention", QtWarningMsg,
                     QRegularExpression(QStringLiteral("no prunable event remains")));

    const int deleted = retention.pruneEventsToFitBytes(0);

    // The one thing that must be true in both worlds.
    QCOMPARE(rowCount(path, QStringLiteral("AuditRecords")), 10);
    QVERIFY(sessionExists(path, 1));

    if (measurable) {
        QCOMPARE(deleted, 40);
        QCOMPARE(rowCount(path, QStringLiteral("Events")), 0);
        // The values went with their events rather than being orphaned.
        QCOMPARE(rowCount(path, QStringLiteral("EventValues")), 0);
    } else {
        QCOMPARE(deleted, -1);
        QCOMPARE(rowCount(path, QStringLiteral("Events")), 40);
        QCOMPARE(rowCount(path, QStringLiteral("EventValues")), 40);
    }
}

void AeroraStoreRetentionTest::_aSizePruneRefusesANegativeCap()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    {
        QGCSqlHelper::ScopedConnection seed(path);
        QVERIFY(seed.isValid());
        QSqlDatabase db = seed.database();
        QVERIFY(seedSession(db, 1));
        QVERIFY(seedEvent(db, 1, 1700000000000LL) > 0);
    }

    AeroraStoreRetention retention(store);
    QVERIFY(retention.open());

    // A negative cap is a caller error, not a request to delete everything. Checked before the
    // measurement probe, so it reports the same way on every build.
    QCOMPARE(retention.pruneEventsToFitBytes(-1), -1);
    QCOMPARE(rowCount(path, QStringLiteral("Events")), 1);
}

void AeroraStoreRetentionTest::_aSizePruneRemovesOldestEventsFirst()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    AeroraStoreRetention retention(store);
    QVERIFY(retention.open());

    if (!retention.canMeasureEventBytes()) {
        QSKIP(
            "this build's SQLite has no dbstat, so a size cap cannot be enforced and the "
            "ordering of a size prune cannot be exercised; the refusal path is covered by "
            "_aSizePruneNeverDeletesAnAuditRecord");
    }

    const qint64 oldest = 1700000000000LL;
    const int seeded = 1200;  // several batches' worth, so the prune can stop part-way
    {
        QGCSqlHelper::ScopedConnection seed(path);
        QVERIFY(seed.isValid());
        QSqlDatabase db = seed.database();
        QVERIFY(seedSession(db, 1));

        for (int i = 0; i < seeded; ++i) {
            const qint64 eventId = seedEvent(db, 1, oldest + i);
            QVERIFY(eventId > 0);
            QVERIFY(seedEventValue(db, eventId, QStringLiteral("a.reasonably.long.item.identifier")));
        }
    }

    const qint64 full = retention.eventBytes();
    QVERIFY(full > 0);

    const qint64 cap = full / 2;
    const int deleted = retention.pruneEventsToFitBytes(cap);
    QVERIFY2(deleted > 0,
             qPrintable(QStringLiteral("deleted %1 at a cap of %2 from %3 bytes").arg(deleted).arg(cap).arg(full)));

    // Nothing lost or double-counted, whichever batch the prune stopped on. The cap is re-checked
    // only between batches, so the count is a multiple of the batch size unless it ran out of rows.
    const int remaining = rowCount(path, QStringLiteral("Events"));
    QCOMPARE(remaining + deleted, seeded);
    QCOMPARE(rowCount(path, QStringLiteral("EventValues")), remaining);

    if (remaining == 0) {
        // It exhausted the table rather than reaching the cap, so there is no surviving order to
        // check. Still a pass: the prune is permitted to delete every event, and never more.
        return;
    }

    // The cap was genuinely reached rather than the loop running out of rows.
    QVERIFY(retention.eventBytes() <= cap);

    // Oldest first: every surviving event is newer than every deleted one, which for a contiguous
    // oldest-first delete means the minimum remaining timestamp moved forward by exactly the
    // number deleted. A prune taking an arbitrary slice would not satisfy this.
    QGCSqlHelper::ScopedConnection check(path);
    QVERIFY(check.isValid());
    QSqlQuery query(check.database());
    QVERIFY(query.exec(QStringLiteral("SELECT MIN(occurredUtcMs) FROM Events")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toLongLong(), oldest + deleted);
}

UT_REGISTER_TEST(AeroraStoreRetentionTest, TestLabel::Unit)

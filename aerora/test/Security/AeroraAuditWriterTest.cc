#include "AeroraAuditWriterTest.h"

#include <QtCore/QDateTime>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QTemporaryDir>
#include <QtCore/QVariant>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtTest/QTest>

#include "AeroraAuditSink.h"
#include "AeroraAuditWriter.h"
#include "AeroraLocalStore.h"
#include "QGCSqlHelper.h"

namespace {

/// The timeout for a drain to complete. Generous for a loaded CI runner; QTRY polls and returns as
/// soon as the worker has caught up, so the figure only bounds a failure.
constexpr int kDrainTimeoutMs = 5000;

AeroraLocalStore::SessionRecord sampleSession()
{
    AeroraLocalStore::SessionRecord record;
    record.startedUtcMs = 1700000000000LL;
    record.baseCommit = QStringLiteral("base");
    record.overlayCommit = QStringLiteral("overlay");
    record.customBuild = true;
    record.authorizationEnforced = true;
    return record;
}

/// An event tagged by \a n in its ordering fields and its operator id, so the persisted rows can be
/// checked for order and fidelity. An empty reasonCode on an AuthorizationDecision is an allow.
AeroraAuditEvent sampleEvent(quint64 n)
{
    AeroraAuditEvent event;
    event.type = AeroraAuditEventType::AuthorizationDecision;
    event.utc = QDateTime::fromMSecsSinceEpoch(1700000000000LL + static_cast<qint64>(n));
    event.monotonicUs = n * 1000;
    event.sequence = n;
    event.operatorId = QStringLiteral("operator-%1").arg(n);
    event.authorizationEnforced = true;
    return event;
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

/// The `actor` column of every `AuditRecords` row, ordered by `recordId` - the insertion order.
QList<QString> actorsInOrder(const QString& path)
{
    QList<QString> actors;
    QGCSqlHelper::ScopedConnection connection(path);
    if (!connection.isValid()) {
        return actors;
    }
    QSqlQuery query(connection.database());
    if (!query.exec(QStringLiteral("SELECT actor FROM AuditRecords ORDER BY recordId"))) {
        return actors;
    }
    while (query.next()) {
        actors.append(query.value(0).toString());
    }
    return actors;
}

}  // namespace

void AeroraAuditWriterTest::_recordedEventsArePersistedInOrder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);
    QVERIFY(store.beginSession(sampleSession()) > 0);

    AeroraAuditWriter writer(store, 16);
    writer.start();

    for (quint64 n = 1; n <= 3; ++n) {
        QCOMPARE(writer.record(sampleEvent(n)), AeroraAuditWriteOutcome::Accepted);
    }

    QTRY_COMPARE_WITH_TIMEOUT(writer.persistedCount(), quint64(3), kDrainTimeoutMs);
    QVERIFY(writer.isDurable());
    writer.stop();

    QCOMPARE(rowCount(path, QStringLiteral("AuditRecords")), 3);
    const QList<QString> actors = actorsInOrder(path);
    QCOMPARE(actors, (QList<QString>{QStringLiteral("operator-1"), QStringLiteral("operator-2"),
                                     QStringLiteral("operator-3")}));
}

void AeroraAuditWriterTest::_stopFlushesPendingRecords()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);
    QVERIFY(store.beginSession(sampleSession()) > 0);

    AeroraAuditWriter writer(store, 64);
    writer.start();

    const int count = 20;
    for (quint64 n = 1; n <= static_cast<quint64>(count); ++n) {
        QCOMPARE(writer.record(sampleEvent(n)), AeroraAuditWriteOutcome::Accepted);
    }

    // No QTRY: stop() must drain whatever the worker has not yet written before it closes.
    writer.stop();

    QCOMPARE(writer.persistedCount(), quint64(count));
    QCOMPARE(writer.storageFailureCount(), quint64(0));
    QCOMPARE(rowCount(path, QStringLiteral("AuditRecords")), count);
}

void AeroraAuditWriterTest::_recordsBufferedBeforeStartAreDrained()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);
    QVERIFY(store.beginSession(sampleSession()) > 0);

    AeroraAuditWriter writer(store, 16);

    // Enqueued before the worker runs: the records buffer in the queue and drain once it starts.
    QCOMPARE(writer.record(sampleEvent(1)), AeroraAuditWriteOutcome::Accepted);
    QCOMPARE(writer.record(sampleEvent(2)), AeroraAuditWriteOutcome::Accepted);

    writer.start();

    QTRY_COMPARE_WITH_TIMEOUT(writer.persistedCount(), quint64(2), kDrainTimeoutMs);
    writer.stop();

    QCOMPARE(rowCount(path, QStringLiteral("AuditRecords")), 2);
}

void AeroraAuditWriterTest::_queueFullIsDroppedWithoutBlocking()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    // Not started, so nothing drains and the capacity bound is exact: the first two records fill the
    // queue and the third is refused for fullness, never blocking the caller.
    AeroraAuditWriter writer(store, 2);
    QCOMPARE(writer.record(sampleEvent(1)), AeroraAuditWriteOutcome::Accepted);
    QCOMPARE(writer.record(sampleEvent(2)), AeroraAuditWriteOutcome::Accepted);

    const AeroraAuditWriteOutcome overflow = writer.record(sampleEvent(3));
    QCOMPARE(overflow, AeroraAuditWriteOutcome::DroppedQueueFull);
    QVERIFY(overflow != AeroraAuditWriteOutcome::FailedStorage);
    QVERIFY(overflow != AeroraAuditWriteOutcome::SinkUnavailable);
}

void AeroraAuditWriterTest::_durabilityAndNameAreProjected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    AeroraAuditWriter writer(store, 8);

    // Before the worker opens the sink, nothing is durable; the name is stable regardless.
    QVERIFY(!writer.isDurable());
    QCOMPARE(writer.sinkName(), QStringLiteral("AsyncSqliteAudit"));
    QVERIFY(!writer.isTamperEvident());

    writer.start();
    QTRY_VERIFY_WITH_TIMEOUT(writer.isDurable(), kDrainTimeoutMs);
    QVERIFY(writer.isRunning());

    writer.stop();
    QVERIFY(!writer.isDurable());
    QVERIFY(!writer.isRunning());
    // The hash chain is integrity checking, never tamper-evidence - fronting it does not change that.
    QVERIFY(!writer.isTamperEvident());
}

UT_REGISTER_TEST(AeroraAuditWriterTest, TestLabel::Unit)

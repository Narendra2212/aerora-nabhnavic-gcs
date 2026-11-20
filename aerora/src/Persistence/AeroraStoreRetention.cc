#include "AeroraStoreRetention.h"

#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "AeroraLocalStore.h"
#include "QGCLoggingCategory.h"
#include "QGCSqlHelper.h"

QGC_LOGGING_CATEGORY(AeroraRetentionLog, "aerora.persistence.retention")

AeroraStoreRetention::AeroraStoreRetention(AeroraLocalStore& store)
    : _store(store)
{}

AeroraStoreRetention::~AeroraStoreRetention() = default;

bool AeroraStoreRetention::open()
{
    if (_ready) {
        return true;
    }

    if (!_store.isOpen()) {
        qCWarning(AeroraRetentionLog) << "cannot open retention: the store is not open";
        return false;
    }

    const QString path = _store.databasePath();
    if (path.isEmpty()) {
        qCWarning(AeroraRetentionLog) << "cannot open retention: the store has no database path";
        return false;
    }

    _connection = std::make_unique<QGCSqlHelper::ScopedConnection>(path, false, QStringLiteral("AeroraRetention"));
    if (!_connection->isValid()) {
        _connection.reset();
        qCWarning(AeroraRetentionLog) << "cannot open retention: the database did not open" << path;
        return false;
    }

    _ready = true;
    return true;
}

void AeroraStoreRetention::close()
{
    _connection.reset();
    _ready = false;
    _canMeasureEventBytes.reset();
}

bool AeroraStoreRetention::canMeasureEventBytes()
{
    if (!_ready) {
        return false;
    }
    if (_canMeasureEventBytes.has_value()) {
        return *_canMeasureEventBytes;
    }

    // A read that touches the virtual table rather than a pragma or a version check: whether the
    // module is registered is exactly what a query against it answers, and nothing else does.
    QSqlDatabase db = _connection->database();
    QSqlQuery probe(db);
    const bool available = probe.exec(QStringLiteral("SELECT pgsize FROM dbstat LIMIT 1"));
    if (!available) {
        qCDebug(AeroraRetentionLog) << "this build cannot attribute bytes to a table (no dbstat), so a size cap "
                                       "cannot be enforced:"
                                    << probe.lastError().text();
    }

    _canMeasureEventBytes = available;
    return available;
}

qint64 AeroraStoreRetention::eventBytes()
{
    if (!canMeasureEventBytes()) {
        return -1;
    }

    QSqlDatabase db = _connection->database();
    QSqlQuery query(db);
    // The two tables and the three indexes over them. Named explicitly rather than matched by
    // prefix so a table added later is not swept into a cap that was never meant to govern it.
    query.prepare(
        QStringLiteral("SELECT SUM(pgsize) FROM dbstat WHERE name IN "
                       "('Events', 'EventValues', 'idx_events_session_monotonic', "
                       "'idx_events_category', 'idx_eventvalues_event')"));
    if (!query.exec() || !query.next()) {
        qCWarning(AeroraRetentionLog) << "could not measure the event log size:" << query.lastError().text();
        return -1;
    }

    const QVariant total = query.value(0);
    if (total.isNull()) {
        // SUM over no rows. An empty store occupies no event pages, which is zero rather than
        // unknown - the tables exist, they simply hold nothing.
        return 0;
    }

    return total.toLongLong();
}

int AeroraStoreRetention::pruneEventsToFitBytes(qint64 maxBytes)
{
    if (!_ready || (maxBytes < 0)) {
        return -1;
    }
    if (!canMeasureEventBytes()) {
        // Fail closed. See the header: the whole-database substitute would delete the entire event
        // log to chase bytes held by audit records, which this must never reclaim.
        qCWarning(AeroraRetentionLog) << "refusing to enforce a size cap of" << maxBytes
                                      << "bytes: this build cannot measure the event log size, and pruning "
                                         "against the whole database size would delete events to reclaim "
                                         "bytes that are not theirs.";
        return -1;
    }

    QSqlDatabase db = _connection->database();
    const int batchSize = 500;
    int deleted = 0;

    for (;;) {
        const qint64 used = eventBytes();
        if (used < 0) {
            return -1;
        }
        if (used <= maxBytes) {
            break;
        }

        QGCSqlHelper::Transaction transaction(db);
        if (!transaction.ok()) {
            return -1;
        }

        // Oldest first. The LIMIT lives in the subquery for the same reason the age prune's does:
        // SQLite's DELETE ... LIMIT needs a compile-time option this build does not require.
        QSqlQuery query(db);
        query.prepare(
            QStringLiteral("DELETE FROM Events WHERE eventId IN "
                           "(SELECT eventId FROM Events ORDER BY occurredUtcMs, eventId LIMIT ?)"));
        query.addBindValue(batchSize);
        if (!query.exec()) {
            return -1;  // the transaction rolls back on destruction, leaving the store consistent
        }

        const int affected = query.numRowsAffected();
        if (!transaction.commit()) {
            return -1;
        }

        if (affected <= 0) {
            qCWarning(AeroraRetentionLog) << "the event log is" << used << "bytes, above the cap of" << maxBytes
                                          << "and no prunable event remains. Audit records are never pruned, so "
                                             "this is where a size prune stops.";
            break;
        }

        deleted += affected;
    }

    return deleted;
}

int AeroraStoreRetention::pruneEventsOlderThan(qint64 cutoffUtcMs)
{
    if (!_ready) {
        return -1;
    }

    QSqlDatabase db = _connection->database();
    QGCSqlHelper::Transaction transaction(db);
    if (!transaction.ok()) {
        return -1;
    }

    // Delete in bounded batches so a large backlog is never one enormous statement. The LIMIT lives
    // in the subquery because SQLite's DELETE ... LIMIT needs a compile-time option this build does
    // not require; EventValues follow by ON DELETE CASCADE (foreign_keys is ON per connection).
    const int batchSize = 500;
    int deleted = 0;
    for (;;) {
        QSqlQuery query(db);
        query.prepare(
            QStringLiteral("DELETE FROM Events WHERE eventId IN "
                           "(SELECT eventId FROM Events WHERE occurredUtcMs < ? ORDER BY eventId LIMIT ?)"));
        query.addBindValue(cutoffUtcMs);
        query.addBindValue(batchSize);
        if (!query.exec()) {
            return -1;  // the transaction rolls back on destruction, leaving the store consistent
        }

        const int affected = query.numRowsAffected();
        if (affected <= 0) {
            break;
        }
        deleted += affected;
        if (affected < batchSize) {
            break;
        }
    }

    if (!transaction.commit()) {
        return -1;
    }

    return deleted;
}

int AeroraStoreRetention::pruneWriteFailuresOlderThan(qint64 cutoffUtcMs)
{
    if (!_ready) {
        return -1;
    }

    QSqlDatabase db = _connection->database();
    QGCSqlHelper::Transaction transaction(db);
    if (!transaction.ok()) {
        return -1;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM WriteFailures WHERE occurredUtcMs < ?"));
    query.addBindValue(cutoffUtcMs);
    if (!query.exec()) {
        return -1;
    }

    const int deleted = query.numRowsAffected();
    if (!transaction.commit()) {
        return -1;
    }

    return deleted;
}

int AeroraStoreRetention::pruneEmptySessions()
{
    if (!_ready) {
        return -1;
    }

    QSqlDatabase db = _connection->database();
    QGCSqlHelper::Transaction transaction(db);
    if (!transaction.ok()) {
        return -1;
    }

    // The active session is kept whether or not it is empty; a currentSessionId of -1 excludes
    // nothing, which is right when there is no active session. A session any Events, AuditRecords or
    // WriteFailures reference is kept - the three subqueries, with ON DELETE RESTRICT as the backstop.
    QSqlQuery query(db);
    query.prepare(
        QStringLiteral("DELETE FROM Sessions WHERE sessionId <> ? "
                       "AND sessionId NOT IN (SELECT sessionId FROM Events) "
                       "AND sessionId NOT IN (SELECT sessionId FROM AuditRecords) "
                       "AND sessionId NOT IN (SELECT sessionId FROM WriteFailures)"));
    query.addBindValue(_store.currentSessionId());
    if (!query.exec()) {
        return -1;
    }

    const int deleted = query.numRowsAffected();
    if (!transaction.commit()) {
        return -1;
    }

    return deleted;
}

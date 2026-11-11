#include "AeroraStoreRetention.h"

#include <QtCore/QString>
#include <QtSql/QSqlDatabase>
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

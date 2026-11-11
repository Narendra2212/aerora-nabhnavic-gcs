#pragma once

#include <memory>

class AeroraLocalStore;

namespace QGCSqlHelper {
class ScopedConnection;
}

/// \brief Operator-controlled pruning of the store. `P7-DAT-005`, design
/// 09-local-persistence-architecture.md §8.
///
/// \section what What prunes, and what never does
///
/// `Events` (and, by cascade, their `EventValues`) and `WriteFailures` are prunable by age; a
/// session is prunable once it carries none of those and no audit record. `AuditRecords` are
/// **never** pruned here - no method deletes them, and `AuditRecords.sessionId`'s `ON DELETE
/// RESTRICT` is the backstop that makes "audit is not auto-prunable" a database property rather
/// than a rule this class must remember (design §4, §8). `Vehicles` are not pruned automatically
/// either.
///
/// \section caps The caps are not here
///
/// The age and size caps, and whether a size cap is counted in bytes or rows, are `D-DAT-01` and
/// `OPEN`: a retention period carries regulatory weight and `P7-NOINV-002` forbids inventing one.
/// So this takes an explicit cutoff from the caller and ships no default. The row-exact age prune
/// is the mechanism; a byte-exact size prune waits on that decision.
///
/// \section how Deletes in bounded batches inside a transaction
///
/// Each prune runs inside one `QGCSqlHelper::Transaction`, so an interrupted prune leaves a
/// consistent database, and the event prune deletes in bounded batches so a large backlog does not
/// become one enormous statement. Pruning is an explicit operator or session-start action, never
/// opportunistic mid-flight (design §8); this class performs it and does not schedule it.
///
/// Like the other store writers it opens its own connection to the store's database and takes the
/// store only for its path and active session id.
class AeroraStoreRetention
{
public:
    explicit AeroraStoreRetention(AeroraLocalStore& store);
    ~AeroraStoreRetention();

    AeroraStoreRetention(const AeroraStoreRetention&) = delete;
    AeroraStoreRetention& operator=(const AeroraStoreRetention&) = delete;
    AeroraStoreRetention(AeroraStoreRetention&&) = delete;
    AeroraStoreRetention& operator=(AeroraStoreRetention&&) = delete;

    bool open();
    void close();

    [[nodiscard]] bool isOpen() const { return _ready; }

    /// Deletes `Events` with `occurredUtcMs` older than \a cutoffUtcMs, cascading their
    /// `EventValues`, in bounded batches inside one transaction. Returns the number of events
    /// deleted, or -1 on failure.
    [[nodiscard]] int pruneEventsOlderThan(qint64 cutoffUtcMs);

    /// Deletes `WriteFailures` older than \a cutoffUtcMs. Returns the number deleted, or -1.
    [[nodiscard]] int pruneWriteFailuresOlderThan(qint64 cutoffUtcMs);

    /// Deletes sessions that carry no `Events`, no `AuditRecords` and no `WriteFailures`, other than
    /// the store's active session. Returns the number deleted, or -1. A session an audit record
    /// references is kept - by this query, and by `ON DELETE RESTRICT` if a query ever missed it.
    [[nodiscard]] int pruneEmptySessions();

private:
    AeroraLocalStore& _store;
    std::unique_ptr<QGCSqlHelper::ScopedConnection> _connection;
    bool _ready = false;
};

#pragma once

#include <memory>
#include <optional>

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
/// The age and size caps are `D-DAT-01`, **signed** on 2026-11-05 in
/// `closures/DG-STO.retention.json` as operator-adjustable settings defaulting to 365 days of age
/// and 2 GB of total size across `Events` and `EventValues`. The values live with the settings that
/// carry them, not here: this takes an explicit cutoff or cap from the caller and ships no default,
/// so no retention value is compiled into the mechanism (`P7-NOINV-002`).
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

    /// Whether this build can attribute bytes to individual tables, probed once per connection.
    ///
    /// `dbstat` is the only thing in SQLite that answers "how many bytes does this table occupy",
    /// and it exists only when the library was compiled with `SQLITE_ENABLE_DBSTAT_VTAB`. Qt's
    /// bundled SQLite may or may not carry it and a deployment may link a system library instead,
    /// so this is probed at runtime rather than assumed at compile time.
    [[nodiscard]] bool canMeasureEventBytes();

    /// Bytes `Events` and `EventValues` occupy, including the three indexes over them, or -1 when
    /// canMeasureEventBytes() is false.
    ///
    /// Indexes are counted deliberately: they are part of what keeping those rows costs on disk,
    /// so excluding them would report a size smaller than the one a size cap is meant to bound.
    /// `AuditRecords`, `Sessions` and `Vehicles` are excluded, because the signed cap governs the
    /// operational event log and nothing else.
    [[nodiscard]] qint64 eventBytes();

    /// Deletes the oldest `Events`, cascading their `EventValues`, in bounded batches until
    /// eventBytes() is at or below \a maxBytes. Returns the number of events deleted, or -1.
    ///
    /// \section closed Refuses rather than approximates
    ///
    /// Returns -1 without deleting anything when canMeasureEventBytes() is false. The tempting
    /// substitute is the whole database size, which needs only two standard pragmas - and it is
    /// wrong in a way that costs data: a store whose bytes are mostly `AuditRecords` can never be
    /// brought under the cap by deleting events, so a loop measuring the whole file would delete
    /// the entire event log chasing bytes it must never reclaim. Refusing leaves the operator with
    /// an unenforced size cap and an intact log, which is the safe direction.
    ///
    /// \section batches Commits per batch, not once overall
    ///
    /// Unlike the age prunes this commits each batch, because freed pages are only visible to the
    /// measurement after a commit, so the number deciding whether to continue cannot be read inside
    /// the transaction that deleted. An interrupted size prune therefore leaves a consistent store
    /// at a batch boundary having deleted fewer events than the cap asked for.
    ///
    /// Stops when no prunable event remains even if still over \a maxBytes, and says so. Oldest
    /// first, so the cap costs the least recent history rather than an arbitrary slice.
    [[nodiscard]] int pruneEventsToFitBytes(qint64 maxBytes);

private:
    AeroraLocalStore& _store;
    std::unique_ptr<QGCSqlHelper::ScopedConnection> _connection;
    bool _ready = false;

    /// Cached result of the `dbstat` probe. Reset by close() so a reopened instance probes the
    /// connection it actually has.
    std::optional<bool> _canMeasureEventBytes;
};

#pragma once

#include <memory>

#include <QtCore/QString>

namespace QGCSqlHelper {
class ScopedConnection;
}

/// \brief The single Aerora local store: the SQLite file, its schema, and its lifecycle. `P7-DAT-002`.
///
/// \section what What this is, and what it is not
///
/// This is the foundation 09-local-persistence-architecture.md §13 assigns to Tab 1: open the
/// database, create and version the schema, and account for writes that fail. It owns the file and
/// the six-table schema; it does **not** own the semantics of the rows. The per-table writers -
/// the `Events`/`EventValues` writer, the `AuditRecords` writer that is an `AeroraAuditSink`, the
/// `Vehicles` writer - belong to the domains that generate those records and build on this store.
/// So the only typed write this store performs itself is the one row that is its own bookkeeping:
/// a `Sessions` row per application run, which every other table references.
///
/// \section durability What durability this gives, stated with its limit
///
/// The connection carries `QGCSqlHelper::applySqlitePragmas()` - WAL journal mode and
/// `synchronous=NORMAL`. That makes a committed row survive an application crash (WAL replay) and
/// survive an OS crash or power loss without corruption. It does **not** make a row reported
/// committed survive power loss: `synchronous=NORMAL` does not fsync the WAL on every commit, so a
/// recently committed row can still be lost (design §6). This store therefore exposes no
/// `isDurable()` flag - a guarantee stated without its caveat is the claim `P7-NOINV-001` forbids,
/// and the per-audit-record `synchronous=FULL` deviation that §6 describes belongs to the audit
/// writer that opens its own connection, not to this foundation.
///
/// \section nottamper Integrity, never tamper-evidence
///
/// The `AuditRecords` table carries `prevHash`/`recordHash` columns for the keyless hash chain
/// `AeroraAuditChain` computes, which detects accidental corruption and in-place edits. It is
/// **not** tamper-evidence: an actor with write access recomputes the chain, and tail truncation is
/// undetectable (design §7, §12). This store makes no tamper-evidence claim and exposes no flag
/// that could be read as one.
///
/// \section disabled disableAllPersistence is honoured before the file is touched
///
/// `P7-DAT-005`: when persistence is disabled the store opens nothing - no file is created and
/// every write is a no-op - rather than opening a file and discarding writes. The caller passes the
/// setting in; the store does not read `AppSettings` so it stays unit-testable against a temporary
/// path.
///
/// \section notthreadsafe Not thread-safe
///
/// One store instance is used from one thread. It holds no lock, the same way `AeroraAuditQueue`
/// does not; a drain thread, if one is ever introduced, provides its own synchronisation.
class AeroraLocalStore
{
public:
    /// The schema this build creates and will open. A single constant, bumped by a forward
    /// migration on the first schema change - not a reset, which would lose the only copy of
    /// records that are not reconstructible (design §5).
    static constexpr int kSchemaVersion = 1;

    /// The outcome of an `open()` attempt, modelled so the UI can report *why* there is no store
    /// rather than showing an empty timeline that reads as "nothing happened" (design §11).
    ///
    /// At most one of the three flags is set. All three false with `opened` false means the file
    /// could not be opened (an I/O failure); `detail` always carries the operator-facing reason.
    struct OpenResult
    {
        /// A usable store is open and its schema is current.
        bool opened = false;

        /// `disableAllPersistence` was set, so nothing was opened and no file was created. This is
        /// the configured posture, not a failure.
        bool persistenceDisabled = false;

        /// The file carried a schema version this build does not recognise, so it was refused and
        /// **left untouched** rather than reset (design §5). Run with no store, loud not quiet.
        bool versionUnsupported = false;

        /// The `PRAGMA user_version` read from the file when `versionUnsupported`, else -1. Carried
        /// so the UI can report the version it found rather than a bare refusal.
        int schemaVersionFound = -1;

        /// Operator-facing explanation. Always set.
        QString detail;
    };

    /// One application run, as the `Sessions` row records it. The provenance columns are supplied
    /// by the caller - WP1-D `AeroraProvenance` is their source - so the store needs no knowledge
    /// of the build to persist them.
    struct SessionRecord
    {
        qint64 startedUtcMs = 0;
        QString baseCommit;
        QString overlayCommit;

        /// Whether `QGC_CUSTOM_BUILD` was actually on at boot (ADR-012), so a run with the overlay
        /// compiled out is distinguishable from one with it active.
        bool customBuild = false;

        /// `AeroraCorePlugin::isAuthorizationEnforced()` at boot, recorded per run.
        bool authorizationEnforced = false;
    };

    AeroraLocalStore();
    ~AeroraLocalStore();

    AeroraLocalStore(const AeroraLocalStore&) = delete;
    AeroraLocalStore& operator=(const AeroraLocalStore&) = delete;
    AeroraLocalStore(AeroraLocalStore&&) = delete;
    AeroraLocalStore& operator=(AeroraLocalStore&&) = delete;

    /// Opens the store at \a databasePath, creating and versioning the schema on a fresh file.
    ///
    /// When \a persistenceDisabled is true, opens nothing and returns a result flagged
    /// `persistenceDisabled`. On an unrecognised schema version, refuses and preserves the file. On
    /// an I/O failure, runs with no store and does not delete the file. Any previously open
    /// connection is closed first.
    OpenResult open(const QString& databasePath, bool persistenceDisabled);

    /// Closes the connection and resets per-run accounting. Safe to call when not open.
    void close();

    [[nodiscard]] bool isOpen() const { return _open; }

    [[nodiscard]] bool isPersistenceDisabled() const { return _persistenceDisabled; }

    /// The path passed to the last `open()`, whether or not it opened. Empty before any open.
    [[nodiscard]] QString databasePath() const { return _databasePath; }

    /// Inserts a `Sessions` row and returns its id, or -1 on failure. The id every other table
    /// references, so it is taken before any domain writer runs.
    [[nodiscard]] qint64 beginSession(const SessionRecord& record);

    /// Stamps `endedUtcMs` on the session \a sessionId. Returns false on failure. A session left
    /// unstamped is how an unclean exit reads (design §4), so this is called on an orderly close.
    bool endSession(qint64 sessionId, qint64 endedUtcMs);

    /// The session the store is writing into, or -1 before `beginSession()`.
    [[nodiscard]] qint64 currentSessionId() const { return _currentSessionId; }

    /// Records a write a domain writer could not complete, counted rather than silently lost
    /// (`P7-AUDIT-003`). Persisted to `WriteFailures` against the current session when one is open;
    /// otherwise, or if that persistence itself fails, counted in memory only so the UI can report
    /// a non-zero unrecorded count (design §11).
    void recordWriteFailure(const QString& targetTable, const QString& reason);

    /// Rows in `WriteFailures` for this run, or 0 when not open.
    [[nodiscard]] int recordedWriteFailureCount() const;

    /// Failures that could not be persisted - no open store, no session, or a `WriteFailures` write
    /// that itself failed. In memory only; reset by `open()`/`close()`.
    [[nodiscard]] int unrecordedWriteFailureCount() const { return _unrecordedWriteFailures; }

private:
    /// Reads the file's `user_version` and prepares the schema, filling \a result. Creates the
    /// schema on a fresh file, proceeds on the current version, and refuses every other version
    /// without touching the file. Returns true only when the store is usable.
    bool _prepareSchema(OpenResult& result);

    /// Creates the six tables and their indices inside one transaction, so an interrupted creation
    /// leaves no half-built schema. Returns false on the first failed statement.
    bool _createSchema();

    /// Whether the file already carries a user table - the test that tells a fresh file from a
    /// legacy unversioned one that must be preserved rather than overwritten.
    [[nodiscard]] bool _hasUserTable() const;

    QString _databasePath;
    std::unique_ptr<QGCSqlHelper::ScopedConnection> _connection;
    bool _open = false;
    bool _persistenceDisabled = false;
    qint64 _currentSessionId = -1;
    int _unrecordedWriteFailures = 0;
};

#pragma once

#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include "AeroraAuditSink.h"

class AeroraLocalStore;

namespace QGCSqlHelper {
class ScopedConnection;
}

/// \brief The coarse result an `AuditRecords.outcome` row records.
///
/// `AuditRecords.outcome` is an integer, and its semantics are a compliance-facing classification
/// (`AERORA-COMPLY-REQ-002`/`-003`): which event kinds count as an allow, a denial, or neither. The
/// classification was an owner decision rather than an engineering one, for the same reason the
/// retention defaults were left open (`P7-NOINV-002`), and the mapping below is the one the owner
/// approved. The authoritative, reader-facing event vocabulary stays the `action` column
/// (`typeToString`); this is only the coarse result beside it.
///
/// The integer values are pinned so a persisted row keeps its meaning if the enumeration is ever
/// reordered.
enum class AeroraAuditOutcome
{
    /// The station said no: `CommandDenied`, `ManualAuthorityRefused`, and an `AuthorizationDecision`
    /// carrying a non-empty `reasonCode`.
    Denied = 0,

    /// The station said yes, or the act occurred: an `AuthorizationDecision` with an empty
    /// `reasonCode`, `CommandTransmitted`, `ManualAuthorityIssued`, `ManualAuthorityRefreshed`.
    Allowed = 1,

    /// Neither an allow nor a denial: `CommandRequested`, `OperatorIdentityResolved`,
    /// `VehicleResponse`, `SecurityEvent`, `SystemEvent`, `ManualAuthorityExpired`.
    Informational = 2,
};

/// \brief The first real `AeroraAuditSink`: it persists `AuditRecords` rows through
/// `AeroraLocalStore`, hash-chained with `AeroraAuditChain`. `P7-DAT-002` writer, `Q-037` durability.
///
/// \section what What this writes
///
/// One `AuditRecords` row per `record()`, translating `AeroraAuditEvent` onto the columns exactly as
/// `AeroraAuditSink.h`'s mapping table specifies: `actor` from `operatorId` (the literal `unresolved`
/// substituted for an empty id, here in the writer and never in the producer), `action` from
/// `typeToString()`, `enforced` and `detail` direct, `occurredUtcMs`/`monotonicUs` from the stamps,
/// `outcome` from the approved `AeroraAuditOutcome` classification, and `prevHash`/`recordHash` from
/// the chain. `recordId` and `sessionId` come from the store, never the producer.
///
/// \section durable Durability — claimed, with the limit WP1-C states
///
/// `isDurable()` is true while open, and this is the first sink entitled to it. The records run on a
/// connection with `synchronous=FULL` applied after the shared WAL pragmas, which is the deviation
/// 09-local-persistence-architecture.md §6 specifies for audit: a committed row survives process
/// death (WAL) and host power loss (fsync per commit), **subject to the hardware honouring fsync** —
/// a device that acknowledges fsync without committing to media defeats it, undetectably. The
/// boolean cannot carry that caveat; it lives here and in §6/§12, and `isDurable()` returns true
/// because WP1-C §7 and the WP2-A contract (§2.9) settle that durability is claimable once a sink is
/// implemented and tested, which this one now is.
///
/// \section nottamper Integrity checking, never tamper-evidence
///
/// `isTamperEvident()` stays false, and implementing the chain does not change that. A hash chain is
/// tamper-evident only against an actor who cannot recompute it; one with write access to the file
/// recomputes every hash, and a truncated tail is a shorter valid chain. `AeroraAuditSink.h` and
/// WP1-C §7 both state this, and the field names are `prevHash`/`recordHash` rather than anything
/// containing "signature" for exactly this reason. Tamper-evidence waits on a protected anchor,
/// which waits on `AeroraPlatformKeyProtector`, which waits on `D-006`.
///
/// The chain detects what integrity checking detects: an edited hash column, a removed or reordered
/// record. A re-verification recomputes `AeroraAuditChain::digestFor` over the **complete** event,
/// and the `AuditRecords` columns are a lossy projection of it (they do not carry `sequence`,
/// `reasonCode`, `recordsLostBeforeThis` or the resolved flag in recoverable form), so a full
/// re-verification needs the complete events an export carries, not the row alone. That is an
/// export/verifier concern and not this writer's; the writer's duty is to compute and store the
/// chain correctly, which its suite checks against the original events.
///
/// \section notlive Why this is not installed on the live command gate
///
/// `record()` performs a synchronous, `fsync`-backed write. `IF-LOG-014` and the WP2-A contract
/// (§2.7) require the sink installed on the command gate to **never block** the caller, because
/// `record()` runs on the vehicle thread; a sink that needs file I/O hands off to its own queue and
/// thread (`AeroraAuditWriter`). That thread is the deferred concurrency decision `AeroraAuditQueue`
/// was built without. So this synchronous writer is the durable half, to be driven by that async
/// front once it exists, or used where blocking is acceptable — as its tests do. Nothing in the
/// overlay installs it, exactly as nothing installs the store: the shipped audit posture is
/// unchanged, and the producer still counts what it cannot write.
///
/// \section notthreadsafe Not thread-safe
///
/// One instance, one thread. It holds its own connection and chain head and no lock.
class AeroraSqliteAuditSink : public AeroraAuditSink
{
public:
    /// \a store supplies the database path, the active session id each record references, and must
    /// outlive this sink. The sink opens its own connection to the same file rather than reusing
    /// the store's, because audit runs at `synchronous=FULL` and the store's other tables do not.
    explicit AeroraSqliteAuditSink(AeroraLocalStore& store);
    ~AeroraSqliteAuditSink() override;

    AeroraSqliteAuditSink(const AeroraSqliteAuditSink&) = delete;
    AeroraSqliteAuditSink& operator=(const AeroraSqliteAuditSink&) = delete;
    AeroraSqliteAuditSink(AeroraSqliteAuditSink&&) = delete;
    AeroraSqliteAuditSink& operator=(AeroraSqliteAuditSink&&) = delete;

    /// Opens the durable audit connection and seeds the chain head from the last stored record, so
    /// the chain is continuous across restarts. Requires the store to be open. Idempotent; returns
    /// true when the sink is ready to record.
    bool open();

    /// Closes the connection. `record()` reports `SinkUnavailable` afterwards.
    void close();

    [[nodiscard]] bool isOpen() const { return _ready; }

    [[nodiscard]] AeroraAuditWriteOutcome record(const AeroraAuditEvent& event) override;

    /// True while open: a record accepted on the `synchronous=FULL` connection is durable, with the
    /// hardware-fsync caveat in the durability section above.
    [[nodiscard]] bool isDurable() const override { return _ready; }

    /// False, and the hash chain does not change that. See the integrity section above.
    [[nodiscard]] bool isTamperEvident() const override { return false; }

    [[nodiscard]] QString sinkName() const override;

    /// The approved `AuditRecords.outcome` classification for \a event. Static and public so the
    /// suite asserts the owner-approved mapping directly, without a database.
    [[nodiscard]] static AeroraAuditOutcome classifyOutcome(const AeroraAuditEvent& event);

private:
    AeroraLocalStore& _store;
    std::unique_ptr<QGCSqlHelper::ScopedConnection> _connection;
    QByteArray _head;
    bool _ready = false;
};

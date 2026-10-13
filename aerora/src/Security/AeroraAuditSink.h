#pragma once

#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QVariantMap>

/// \file AeroraAuditSink.h
/// The audit-record writer interface for Aerora.
///
/// This file defines the *interface* and its contract. It chooses no storage mechanism, and it
/// makes no durability or tamper-evidence claim, because neither mechanism exists: the integrity
/// mechanism is Q-037/Q4-003 and may depend on key storage (D-002, risk R4-002), and the local
/// store itself is P7-DAT-002, owned by Tab 1 and unresolved. See
/// `.kiro/specs/03-implementation-specification/34-audit-sink-and-security-status-contract.md`
/// for the full contract, the acceptance criteria and the open decisions.
///
/// \section boundary Where this interface ends
///
/// `AeroraAuditSink` is the seam between a *record producer* and a *record destination*. It is one
/// method wide on purpose.
///
/// \code
///   producer                     this interface              the storage layer (does not exist)
///   AeroraCommandAuthorizer  ->  AeroraAuditSink::record  ->  AeroraAuditWriter   IF-LOG-014
///                                                             AeroraAuditStore    IF-LOG-003..013
///                                                             AeroraAuditIntegrity IF-LOG-020 OPEN
/// \endcode
///
/// The three types on the right are specified in 13-audit-implementation.md and **none of them is
/// implemented**. Everything they own is deliberately absent from this interface:
///
/// \li storage location, schema and record encoding
/// \li the bounded writer queue and its thread (`IF-LOG-014`)
/// \li rotation, retention, pruning and deletion records (`IF-LOG-023`, Q-039)
/// \li export and the integrity material carried with it (`IF-LOG-024`, `IF-LOG-020`)
/// \li upload of a *copy* after the local write (`IF-LOG-016`, `IF-LOG-017`)
///
/// A sink therefore never transmits. Aerora is local-first (`P7-ARCH-010`) and `P7-DAT-004`
/// prohibits automatic outbound transmission of logs; an implementation that sends a record
/// anywhere as a side effect of `record()` violates both, and `IF-LOG-016` additionally fixes the
/// ordering - the local write is the record, an upload is a later copy of it.

/// \brief Classes of auditable event.
///
/// The set distinguishes request, decision, denial and transmission separately so a denied command
/// is never conflated with a transmitted one, and so an authorization decision is recorded even
/// when no command follows. `AERORA-COMPLY-REQ-003` requires a *request* and the reported
/// *effective* change to stay separate events, which is why there is no single "command" value.
///
enum class AeroraAuditEventType {
    OperatorIdentityResolved,
    AuthorizationDecision,
    CommandRequested,
    CommandDenied,
    CommandTransmitted,
    VehicleResponse,
    SecurityEvent,
    SystemEvent,
};

/// \brief What a sink did with a record.
///
/// `record()` returns this instead of `void` so a producer can account for every record it could
/// not get written. Without a result there is no difference, at the call site, between a record
/// that reached a store and one that vanished - and `AERORA-GCS-REQ-025` requires that every
/// dropped record be accounted for, which is impossible if the drop is unobservable.
///
/// \warning `Accepted` is **not** a persistence claim. `IF-LOG-014` requires the writer queue to be
///          asynchronous and to never block the main thread, so a sink that returns promptly
///          cannot have completed durable I/O. `Accepted` means *this sink took responsibility for
///          the record*; whether the record survives a crash is answered by `isDurable()`, which is
///          false for every sink that exists today.
///
enum class AeroraAuditWriteOutcome {
    /// The sink took the record. Says nothing about durability - see the warning above.
    Accepted,

    /// The sink's bounded queue was full and the record was discarded. `IF-LOG-014` fixes this
    /// behaviour: a sink drops, it never blocks, because a full disk must degrade the record
    /// rather than freeze the operator interface.
    DroppedQueueFull,

    /// The sink attempted to record and the attempt failed - no space, I/O error, a store that
    /// will not open. Distinct from `DroppedQueueFull` because the cause and the operator's
    /// remedy differ, and because `IF-LOG-016` makes a failed local write a decision point: no
    /// upload may be attempted for a record that was not written locally.
    FailedStorage,

    /// The sink is installed but is not accepting records at all - not started, shut down, or
    /// refusing to run. `AeroraAuditIntegrity` is specified to make a writer *refuse to start*
    /// without an injected integrity implementation rather than run as a no-op, so "installed but
    /// not accepting" is a real state and not a defensive placeholder.
    SinkUnavailable,
};

/// \brief One audit record.
///
/// \section fields Why each field is here
///
/// Every field answers a question an existing requirement asks. Nothing is present for a question
/// that has not been asked - the record is deliberately narrower than 13-audit-implementation.md's
/// eventual `AeroraAuditRecord`, because the fields that document specifies beyond these need a
/// correlation identity (`IF-LOG-022`) and a session identity that this layer is not given.
///
/// | Question | Field | When unavailable | Source |
/// | --- | --- | --- | --- |
/// | Who requested it | `operatorId` | Empty. A gap, never backfilled | `AERORA-SEC-REQ-019` |
/// | What was requested | `detail` | - | `AERORA-COMPLY-REQ-002` |
/// | When, for a human | `utc` | - | `IF-LOG-021` |
/// | When, for ordering | `monotonicUs` | Zero means unstamped | `IF-LOG-021` |
/// | In what exact order | `sequence` | Zero means unstamped | `IF-LOG-021` |
/// | Allowed or denied | `type` | - | `AERORA-COMPLY-REQ-002` |
/// | Why | `reasonCode` | Empty on allow | `AERORA-COMPLY-REQ-002` |
/// | Which vehicle | `vehicleMavlinkSystemId` + `vehicleResolved` | `vehicleResolved == false` | `IF-LOG-022` |
/// | Was a policy evaluating | `authorizationEnforced` | - | `P7-AUTH-002` |
/// | How much was already lost | `recordsLostBeforeThis` | - | `IF-LOG-015` |
///
/// \section ordering Record ordering
///
/// Three rules, because wall-clock time cannot carry any of them.
///
/// \li **Producers assign order, not sinks.** All three time fields are stamped when the event
///     occurs, which is what 13-audit-implementation.md's write ordering requires ("record
///     composed, timestamped at the event"). A sink must not re-stamp any of them; a sink that
///     re-timestamps on write records when it got around to the record, not when the event
///     happened. `stampNow()` sets all three together so a producer cannot set one and forget
///     another.
/// \li **`utc` is for reading, never for ordering.** Wall-clock time steps backwards on an NTP
///     correction, a GNSS time fix or an operator clock change, which would silently reorder a
///     trail. `monotonicUs` is the ordering source and is immune to all three. This is the same
///     split, for the same reason, as `AuditRecords.occurredUtcMs` / `monotonicUs` in WP1-C's
///     09-local-persistence-architecture.md.
/// \li **`sequence` is what makes the order *strict*.** `monotonicUs` is non-decreasing but two
///     reads microseconds apart can return the same value on a coarse clock, and
///     `AeroraCommandAuthorizer::authorize()` emits up to four records per command in a fixed,
///     meaning-bearing order (`CommandRequested` -> `OperatorIdentityResolved` ->
///     `AuthorizationDecision` -> `CommandDenied`). A tie there makes that order unrecoverable, so
///     a dense counter supplies the tiebreak a clock cannot.
///
/// A sink must preserve the order in which records were handed to it. It is **not** required to
/// establish a total order across threads: the record already carries one, so a reader
/// reconstructs order from the record rather than trusting the sink's write order.
///
/// \section loss Detecting loss from the records themselves
///
/// `sequence` is dense, so a gap in the sequence of a recovered set is positive evidence that a
/// record is missing. `recordsLostBeforeThis` carries the producer's loss count at composition
/// time, which is `IF-LOG-015`'s requirement that the drop count be "itself written into the
/// record" - a reader of an exported set learns records were lost without needing the live
/// counter. Together they distinguish "nothing was lost" from "the counter was never read".
///
/// \section mapping How this maps onto the designed store
///
/// WP1-C designs an `AuditRecords` table and assigns its writer - the first real
/// `AeroraAuditSink` implementation - to Tab 2. The mapping is recorded here so the writer is a
/// translation rather than a redesign, and so a missing producer-side field is visible now rather
/// than at implementation time.
///
/// | `AuditRecords` column | From | Note |
/// | --- | --- | --- |
/// | `recordId` | the store | `AUTOINCREMENT`, assigned on insert. Not a producer concern |
/// | `sessionId` | the store | The producer has no session identity |
/// | `occurredUtcMs` | `utc` | - |
/// | `monotonicUs` | `monotonicUs` | Direct |
/// | `actor` | `operatorId` | Column is `NOT NULL`; the **writer** substitutes the literal
///   `unresolved` for an empty id. The producer never backfills it |
/// | `action` | `type` | Via `typeToString()` |
/// | `subject` | `detail` + `vehicleMavlinkSystemId` | - |
/// | `outcome` | `type` + `reasonCode` | `CommandDenied` and `AuthorizationDecision` stay distinct |
/// | `enforced` | `authorizationEnforced` | Captured at composition, because the posture can
///   change between composing a record and writing it |
/// | `detail` | `detail` | - |
/// | `prevHash`, `recordHash` | the store | Integrity material is **not** a producer concern, and
///   is **not** tamper-evidence - see below |
///
/// Nothing in this record supplies integrity material, and nothing should: WP1-C is explicit that
/// the hash chain is **integrity checking, never tamper-evidence**, because an actor with write
/// access can recompute the whole chain. Implementing it therefore does **not** entitle a sink to
/// return true from `isTamperEvident()`. That stays false until a protected anchor exists, which
/// is blocked on `AeroraPlatformKeyProtector` having a backend, which is blocked on `D-006`.
///
struct AeroraAuditEvent {
    AeroraAuditEventType type = AeroraAuditEventType::SystemEvent;

    /// Wall-clock time of the event, for human reading and for `AuditRecords.occurredUtcMs`.
    /// **Not** an ordering key - see the ordering section.
    QDateTime utc;

    /// Monotonic microseconds in a process-local domain, for ordering. Zero means unstamped,
    /// matching the zero-stamp-is-not-an-observation convention used elsewhere in the overlay.
    ///
    /// The domain is this process's start, so values are not comparable across restarts and are
    /// not a wall clock. That is sufficient for `AuditRecords.monotonicUs`, whose index is
    /// per-session.
    quint64 monotonicUs = 0;

    /// Process-local, dense composition counter, for a strict order. Zero means unstamped.
    ///
    /// Not a correlation identity: it says nothing about which command a record belongs to.
    /// `IF-LOG-022`'s correlation identity is a separate, unimplemented concern.
    quint64 sequence = 0;

    QString operatorId;
    QString reasonCode;

    /// MAVLink system id of the vehicle this event concerns. Meaningful only when
    /// \c vehicleResolved is true.
    ///
    /// This is **not** the Aerora fleet identity, which is OPEN. It is recorded because it
    /// is the only vehicle identifier available at the command gate, and a denial that does
    /// not say which vehicle it concerned is not auditable on a multi-vehicle station.
    int vehicleMavlinkSystemId = 0;

    /// False means no vehicle was available to the gate. Recorded as an explicit gap rather
    /// than defaulted to a vehicle that was never involved.
    bool vehicleResolved = false;

    /// Whether an authorization policy was installed and evaluating when this record was composed.
    ///
    /// Captured here rather than read at write time because the two can differ: a record composed
    /// while nothing was enforced, then written after a policy was installed, would otherwise be
    /// stored as though it had been evaluated. `P7-AUTH-002` requires the station report when it
    /// is not enforcing, and a record that misreports it retroactively is worse than one that
    /// omits it. Maps to `AuditRecords.enforced`.
    bool authorizationEnforced = false;

    /// The producer's cumulative count of records it failed to get written, as of this record.
    /// `IF-LOG-015`: a gap with a count is usable evidence; a gap with no count is not.
    quint64 recordsLostBeforeThis = 0;

    QVariantMap detail;

    static QString typeToString(AeroraAuditEventType type);

    /// Sets `utc`, `monotonicUs` and `sequence` to now, in one call.
    ///
    /// One call rather than three assignments at each producer, because the three fields are only
    /// meaningful together: a record with a wall-clock stamp and no monotonic stamp is unorderable,
    /// and the failure would be invisible until someone tried to reconstruct a trail.
    void stampNow();

    /// Allocates the next process-local sequence number. First call returns 1, so zero stays
    /// available as "unstamped".
    ///
    /// Lives here rather than in a producer so that every producer shares one order. A
    /// per-producer counter would make two records from different producers unorderable, which is
    /// the problem `sequence` exists to solve.
    [[nodiscard]] static quint64 nextSequence();

    /// Monotonic microseconds since process start. The ordering clock domain.
    [[nodiscard]] static quint64 nowMonotonicUs();
};

/// \brief Destination for audit records.
///
/// \section impl What an implementation must do
///
/// \li **Report the outcome.** Return the value that describes what actually happened. Returning
///     `Accepted` for a record that was discarded defeats every loss count downstream.
/// \li **Never block the caller.** `AERORA-GCS-REQ-025` and `IF-LOG-014`. `record()` is called
///     synchronously on the thread that runs the command gate - the vehicle's thread for both
///     call sites that exist (`MavCommandQueue::sendWorker()` and `Vehicle::startCalibration()`).
///     A sink that needs file I/O hands the record to its own queue and thread, which is
///     `AeroraAuditWriter`'s job, and returns.
/// \li **Never drop silently.** Discarding a record is permitted - `IF-LOG-014` requires it in
///     preference to blocking - but it must be reported through the return value.
/// \li **Never discard an incomplete record.** `IF-LOG-022`: a record with no vehicle identity, or
///     no operator identity, is written with the gap marked. Refusing it would destroy the
///     evidence that the gap occurred, so there is no "rejected as invalid" outcome.
/// \li **Never re-enter the producer.** `record()` is called from inside the authorization path.
///     A sink that calls back into `AeroraCommandAuthorizer` would re-enter the gate.
/// \li **Never throw.** The call sites are command paths with no exception handling.
/// \li **Never transmit.** See the file-level note on `P7-DAT-004` and `IF-LOG-016`.
///
/// \section lifetime Ownership and lifetime
///
/// A sink is owned by whatever installs it, not by the producer. `AeroraCommandAuthorizer` holds it
/// through a `std::shared_ptr` so that replacing or clearing a sink cannot destroy it while a
/// record is in flight through `record()`.
///
/// **No sink installed is a valid configuration, not an error.** It is the shipped state: nothing
/// in this overlay constructs a sink, because no store exists to put records in. The producer
/// counts what it could not write and continues - see
/// `AeroraCommandAuthorizer::lostAuditRecordCount()`.
///
/// \section notblocking What a failed write does not do
///
/// It does not deny the command. `P7-AUDIT-003` records the question - must an unauditable *allow*
/// be denied? - as a **product** decision that is `OPEN`. Answering it here, in either direction,
/// would be inventing a security policy: denying would make an audit outage a flight outage, and
/// silently allowing without a count would hide it. So the producer counts the loss, surfaces the
/// count, and blocks nothing.
///
class AeroraAuditSink
{
public:
    virtual ~AeroraAuditSink() = default;

    /// Records \a event.
    ///
    /// \return What the sink did with the record. `[[nodiscard]]` because an ignored outcome is an
    ///         unaccounted record, which is the failure `AERORA-GCS-REQ-025` prohibits.
    [[nodiscard]] virtual AeroraAuditWriteOutcome record(const AeroraAuditEvent& event) = 0;

    /// True only when records survive process and host restart. Default-false by design.
    [[nodiscard]] virtual bool isDurable() const { return false; }

    /// True only when undetected modification of a stored record is prevented or detectable.
    /// Remains false until Q-037/Q4-003 is resolved and a mechanism is implemented and tested.
    [[nodiscard]] virtual bool isTamperEvident() const { return false; }

    [[nodiscard]] virtual QString sinkName() const = 0;

    static QString outcomeToString(AeroraAuditWriteOutcome outcome);

    /// True when \a outcome means the record was not taken. One place, so no caller has to
    /// enumerate the failure values and miss one added later.
    [[nodiscard]] static bool isLoss(AeroraAuditWriteOutcome outcome)
    {
        return outcome != AeroraAuditWriteOutcome::Accepted;
    }
};

#pragma once

#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QVariantMap>

/// \file AeroraAuditSink.h
/// Audit event interface for Aerora.
///
/// This file defines the *interface* only. It makes no durability claim and no
/// tamper-evidence claim, because neither mechanism exists yet: the integrity
/// mechanism is Q-037/Q4-003 and may depend on key storage (D-002, risk R4-002).
///
/// A sink reports its own properties through \c isDurable() and \c isTamperEvident().
/// Both default to false so an in-memory or log-only sink can never be mistaken for
/// an evidential record.

/// \brief Classes of auditable event.
///
/// The set distinguishes request, decision, denial and transmission separately so a
/// denied command is never conflated with a transmitted one, and so an authorization
/// decision is recorded even when no command follows.
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

/// \brief One audit record.
///
/// The field set exists to answer six questions about a decision: who requested it
/// (\c operatorId), what was requested (\c detail), when (\c utc), whether it was allowed
/// (\c type), why (\c reasonCode), and against which vehicle (\c vehicleMavlinkSystemId).
///
/// \c operatorId is the opaque identity value, or empty when no identity was available -
/// which is itself an auditable fact and must not be backfilled with a placeholder.
///
struct AeroraAuditEvent {
    AeroraAuditEventType type = AeroraAuditEventType::SystemEvent;
    QDateTime utc;
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

    QVariantMap detail;

    static QString typeToString(AeroraAuditEventType type);
};

/// \brief Destination for audit records.
///
/// Implementations must not silently drop records. A sink that cannot record must say so
/// through \c isDurable() and should surface the failure to its installer.
///
class AeroraAuditSink
{
public:
    virtual ~AeroraAuditSink() = default;

    virtual void record(const AeroraAuditEvent& event) = 0;

    /// True only when records survive process and host restart. Default-false by design.
    [[nodiscard]] virtual bool isDurable() const { return false; }

    /// True only when undetected modification of a stored record is prevented or detectable.
    /// Remains false until Q-037/Q4-003 is resolved and a mechanism is implemented and tested.
    [[nodiscard]] virtual bool isTamperEvident() const { return false; }

    [[nodiscard]] virtual QString sinkName() const = 0;
};

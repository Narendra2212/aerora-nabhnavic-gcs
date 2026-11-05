#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

/// \file AeroraAuthorization.h
/// Authorization abstraction for Aerora. Deliberately contains no role names, no
/// permission names and no credential handling.
///
/// Q-029 (the operator role set) is OPEN. Every type here is role-agnostic: roles are
/// opaque identifiers supplied by configuration, and the policy is an interface with no
/// built-in content. Nothing in this file may grow a hard-coded role comparison.
///
/// The chain is:
///
///   Operator Identity -> Role Resolution -> Authorization Policy -> Command Gate -> MAVLink
///
/// Every stage fails closed. An unknown identity, an unresolved role, a missing policy
/// and an unavailable policy all deny.

/// \brief Result of an authorization evaluation.
///
/// Values distinguish *why* a decision was reached so the audit trail records a cause
/// rather than a bare boolean. None of them names a role.
///
enum class AeroraAuthorizationOutcome {
    Allowed,                  ///< Policy permitted the operation
    DeniedNoIdentity,         ///< No authenticated operator identity is available
    DeniedRoleUnresolved,     ///< Identity is known but its role could not be resolved
    DeniedByPolicy,           ///< Policy evaluated the operation and refused it
    DeniedPolicyUnavailable,  ///< A policy is installed but could not evaluate
    DeniedNotConfigured,      ///< No policy installed at all - fail closed, never allow
};

/// \brief An operator identity.
///
/// \c opaqueId carries no assumed structure and is not a credential. Aerora does not
/// invent an identity scheme here; the provider supplies whatever its configured source
/// yields. An identity is usable only when it is both authenticated and non-empty.
///
struct AeroraOperatorIdentity {
    QString opaqueId;
    bool authenticated = false;

    [[nodiscard]] bool isValid() const { return authenticated && !opaqueId.isEmpty(); }
};

/// \brief Roles resolved for an identity.
///
/// \c roleIds are opaque strings originating from configuration. This type never
/// enumerates or validates specific role names, because the role set is Q-029 and
/// remains an unresolved product decision.
///
struct AeroraRoleAssignment {
    QStringList roleIds;
    bool resolved = false;

    /// Resolved with at least one role. An empty resolved assignment grants nothing,
    /// which is intentional: "resolved to no roles" is a valid, authority-free answer.
    [[nodiscard]] bool hasAnyRole() const { return resolved && !roleIds.isEmpty(); }
};

/// \brief Which vehicle an operation concerns.
///
/// \c mavlinkSystemId is the only vehicle identifier evidenced at the command gate. It is
/// **not** the Aerora fleet identity, which is OPEN: nothing here may be treated as a
/// durable fleet-wide identifier, and no consumer may derive one from it.
///
/// \c resolved false is a real, recordable state - the gate was handed no vehicle - and is
/// never backfilled with a placeholder, because an invented identifier in an audit record is
/// worse than an acknowledged gap.
///
struct AeroraVehicleContext {
    int mavlinkSystemId = 0;
    bool resolved = false;
};

/// \brief To whom a gated operation applies. This axis grows independently of what is done.
///
/// A single member is an Independent_Operation. More than one member with \c allOrNothing set is
/// a Fleet_Operation, whose authorization applies to every member or to none.
///
struct AeroraOperationAddress {
    QList<AeroraVehicleContext> vehicles;
    bool allOrNothing = false;

    /// The capability a Fleet_Operation requires, resolved against each vehicle reported
    /// inventory. Carried as an opaque string for the reason \c mavCommand is carried as an int:
    /// no capability identifier is invented, declared or redefined by Aerora. Empty for an
    /// operation addressed directly rather than by capability.
    QString capabilityRequired;

    /// The one addressed vehicle, for a reader that handles a single vehicle only.
    ///
    /// An address carrying no vehicle yields an unresolved context, which is the state the gate
    /// is in on paths holding no vehicle and is what this struct replaced. An address carrying
    /// more than one yields unresolved too, deliberately: a single-vehicle reader must not report
    /// one member of an addressed set as though it were the whole operation, and unresolved is
    /// visibly wrong where a first member would be quietly wrong.
    [[nodiscard]] AeroraVehicleContext soleVehicle() const
    {
        return vehicles.size() == 1 ? vehicles.constFirst() : AeroraVehicleContext{};
    }
};

/// \brief What a gated operation does. Exactly one form today.
///
/// Mirrors the core hook parameters. \c mavCommand is a MAV_CMD value carried as int: no MAVLink
/// identifier is invented, declared or redefined by Aerora. Parameters are included because they
/// carry the operation meaning - arm versus disarm is distinguished only by param1 - and a policy
/// that cannot see them cannot decide correctly.
///
/// ADR-014 may add a second form for a parameter write. It would sit beside this one and touch no
/// addressing code, which is the whole purpose of the separation.
///
struct AeroraCommandSubject {
    int mavCommand = 0;
    int targetCompId = 0;
    bool commandInt = false;
    float param1 = 0.0f;
    float param2 = 0.0f;
    float param3 = 0.0f;
    float param4 = 0.0f;
    double param5 = 0.0;
    double param6 = 0.0;
    float param7 = 0.0f;
};

/// \brief The operation being gated: whom it applies to, and what it does.
///
/// The two axes vary independently. Addressing grows from one vehicle to a set without the
/// subject changing, and ADR-014 may add a subject form without the addressing changing.
///
struct AeroraGatedOperation {
    AeroraOperationAddress address;
    AeroraCommandSubject command;
};

/// \brief A decision, with the cause retained for audit.
struct AeroraAuthorizationDecision {
    AeroraAuthorizationOutcome outcome = AeroraAuthorizationOutcome::DeniedNotConfigured;

    /// Machine-readable cause supplied by the policy. Never a role name.
    QString reasonCode;

    [[nodiscard]] bool allowed() const { return outcome == AeroraAuthorizationOutcome::Allowed; }

    static QString outcomeToString(AeroraAuthorizationOutcome outcome);
};

/// \brief Whether one addressed vehicle received a transmitted Fleet_Operation.
///
/// Separate from the authorization outcome, and the separation is the whole point. Authorization is
/// atomic - Requirement 3 criterion 3 applies an operation to every addressed aircraft or to none -
/// and transmission cannot be, because a link can fail after the decision was reached. Folding the
/// two into one value would make a partial delivery read as a partial authorization, which is the
/// state Requirement 3 criterion 7 exists to name.
///
enum class AeroraFleetTransmissionOutcome {
    NotAttempted,  ///< Nothing was sent. The state of every member while DG-ROL is open
    Delivered,     ///< The link accepted the operation for this vehicle
    Failed,        ///< Transmission was attempted and the link did not accept it
};

/// \brief The outcome of authorizing one Fleet_Operation.
///
/// \c permitted is the all-or-nothing verdict of Requirement 3 criterion 3. \c perVehicle carries
/// one decision per addressed vehicle so that a refusal names which, per criterion 4.
///
/// \c excludedByCapability is distinct from a refusal and must stay distinct. An aircraft reporting
/// no satisfying capability is removed from the addressed set *before* authorization and reported,
/// which Requirement 4 criterion 3 requires: it was not refused, it was never addressed. Each entry
/// is a rendered MAVLink system id rather than an integer, because the exclusion is reported to an
/// operator and \c mavlinkSystemId is not a durable fleet identity for a consumer to key on.
///
struct AeroraFleetDecision {
    bool permitted = false;
    QList<AeroraAuthorizationDecision> perVehicle;
    QStringList excludedByCapability;
};

/// \brief The per-vehicle transmission result of an authorized Fleet_Operation.
///
/// One outcome per addressed vehicle, in the order the address carried them. Nothing produces a
/// value other than \c NotAttempted today: while DG-ROL holds no accepted Closure_Record the
/// station originates no command, so this type records the shape a transmit path must report
/// against rather than any transmission that happened.
///
struct AeroraFleetTransmission {
    QList<AeroraFleetTransmissionOutcome> perVehicle;

    /// Whether every addressed vehicle received the operation.
    ///
    /// False for an empty list. An operation transmitted to nobody did not reach everybody, and
    /// the vacuous-truth reading would report a fleet of none as fully delivered.
    [[nodiscard]] bool deliveredToEvery() const
    {
        if (perVehicle.isEmpty()) {
            return false;
        }
        for (const AeroraFleetTransmissionOutcome outcome : perVehicle) {
            if (outcome != AeroraFleetTransmissionOutcome::Delivered) {
                return false;
            }
        }
        return true;
    }
};

/// \brief Whether the recorded capability vocabulary is accepted.
///
/// False, and pinned false by the Gate_Register. The capability part of `DG-PAY` settles the form
/// of a capability identifier and the means by which a vehicle reports capability presence
/// separately from capability state, and it holds no accepted Closure_Record.
///
/// Requirement 4 criterion 7 then requires a Capability_Address to report unresolvable rather than
/// to address every aircraft. That direction is not a detail: resolving an unsettled vocabulary to
/// everything would send a hover command to a fixed wing, which is the exact failure addressing by
/// capability exists to prevent.
///
constexpr bool kCapabilityVocabularyAccepted = false;

/// \brief A time-bounded authorization under which a Flight_Controller may accept manual input.
///
/// The station issues it and the Flight_Controller enforces it. Requirement 2 criterion 1 forbids
/// the station carrying manual control input at all, so **this type holds no stick value**, and
/// that absence is the prohibition rather than a rule somebody has to remember. A field that could
/// hold an axis would make the prohibition a review matter; having nowhere to put one makes it
/// structural.
///
/// The reason the grant exists at all is a hazard the register records as
/// `manual-control-latency`: a path through a general-purpose operating system has no bounded
/// worst-case latency, and a host that stalls delivers stick positions late or not at all. Sticks
/// therefore reach the aircraft directly and the station grants and revokes the mode, which makes
/// it a single point of *authority* rather than a single point of *control*.
///
/// Nothing here issues a grant. Issuing is an operator command and `DG-ROL` holds every origination
/// path, so this type and its expiry are what is buildable while that Gate is open.
///
struct AeroraManualAuthorityGrant {
    /// Opaque operator identity. Not a credential, and never a role name.
    QString operatorId;

    /// Monotonic microseconds at issue. Zero means no grant was ever issued, which is distinct
    /// from a grant that has expired, and the two must stay distinct: one is a station that has
    /// never granted manual authority and the other is one whose grant lapsed.
    qint64 issuedAtMonotonicUs = 0;

    /// Monotonic microseconds at which the grant lapses without refresh.
    qint64 expiresAtMonotonicUs = 0;

    /// Whether the grant is in force at \a nowMonotonicUs.
    ///
    /// Requirement 2 criterion 3: a grant carries an expiry and requires refresh to remain in
    /// force. Expiry is therefore read from the clock on every call rather than latched, because a
    /// latched flag would need something to clear it and whatever failed to refresh the grant is
    /// the same thing that would have failed to clear the flag.
    ///
    /// An unissued grant is never in force. Criterion 5 makes a lost link expire the grant without
    /// refresh, and an unissued grant reading as in force would invert exactly that.
    [[nodiscard]] bool inForce(qint64 nowMonotonicUs) const
    {
        if (issuedAtMonotonicUs == 0) {
            return false;
        }
        return nowMonotonicUs >= issuedAtMonotonicUs && nowMonotonicUs < expiresAtMonotonicUs;
    }
};

/// \brief What the Flight_Controller reports about manual control, as an observation.
///
/// Requirement 2 criterion 7: the Flight_Controller reports stick positions and the grant state as
/// telemetry, so that the station observes manual control without carrying it. Every value here
/// arrived inbound and none of it can be sent: there is no path from this type to a link, which is
/// what makes observing and carrying different things rather than one thing named twice.
///
/// \c axes is an opaque ordered list rather than named members. The Aerora dialect does not exist,
/// so the axis set is unsettled, and naming members here would author part of it. A consumer
/// renders the list it was given and labels nothing it was not told.
///
struct AeroraManualControlReport {
    /// Monotonic microseconds at which this report was received. Zero means nothing has been
    /// observed, which is **not** a report of centred sticks: one is the absence of telemetry and
    /// the other is telemetry saying the operator is holding still.
    qint64 observedAtMonotonicUs = 0;

    /// Whether the Flight_Controller reports a grant in force.
    ///
    /// The answer from the aircraft, not a copy of what the station believes it granted. The
    /// station issues and the Flight_Controller enforces, so the two disagreeing is a real
    /// condition an operator needs to see, and resolving it by preferring the station would hide
    /// the only case where the enforcement actually matters.
    bool grantInForceAtVehicle = false;

    /// The stick axis values the report carried, in the order the dialect carried them.
    QList<float> axes;

    /// Whether any report has been observed. Distinct from the content of a report.
    [[nodiscard]] bool observed() const { return observedAtMonotonicUs != 0; }
};

/// \brief What resolving a Capability_Address against the reported inventories produced.
///
/// Three states, because collapsing any two loses the repair. An address naming no capability was
/// never resolved; an address that could not be resolved waits on a signature; a resolved address
/// has an answer. Reporting the first two the same way would tell an operator that an explicitly
/// addressed fleet was blocked by a Gate it never touched.
///
enum class AeroraCapabilityResolutionState {
    NotAddressedByCapability,  ///< The address names no capability, so every member stays addressed
    Unresolvable,              ///< The vocabulary is unaccepted, so no inventory can be matched
    Resolved,                  ///< Every member was matched against its reported inventory
};

/// \brief The result of resolving one Capability_Address.
///
/// \c excluded is populated only in the \c Resolved state. An unresolvable address excludes
/// nobody: it is not a claim that every aircraft lacks the capability, it is the absence of any
/// basis for the claim, and excluding the whole fleet would be indistinguishable from resolving it.
///
struct AeroraCapabilityResolution {
    AeroraCapabilityResolutionState state = AeroraCapabilityResolutionState::Unresolvable;
    QStringList excluded;
};

/// Resolve the Capability_Address \a address carries, if it carries one.
///
/// Inline beside the types for the reason `soleVehicle()` is: while the vocabulary is unaccepted
/// there is no matching to do, and a translation unit of its own would carry one branch. It moves
/// to a source file when `DG-PAY:capability` closes and matching becomes real work.
[[nodiscard]] inline AeroraCapabilityResolution resolveCapabilityAddress(
    const AeroraOperationAddress& address)
{
    AeroraCapabilityResolution resolution;
    if (address.capabilityRequired.isEmpty()) {
        resolution.state = AeroraCapabilityResolutionState::NotAddressedByCapability;
        return resolution;
    }
    resolution.state = kCapabilityVocabularyAccepted
                           ? AeroraCapabilityResolutionState::Resolved
                           : AeroraCapabilityResolutionState::Unresolvable;
    return resolution;
}

/// \brief Supplies the current operator identity. Replaceable.
class AeroraIdentityProvider
{
public:
    virtual ~AeroraIdentityProvider() = default;
    [[nodiscard]] virtual AeroraOperatorIdentity currentIdentity() const = 0;
};

/// \brief Resolves an identity to opaque role identifiers. Replaceable and configuration-driven.
///
/// Implementations must not embed a role set in code. A resolver that cannot answer must
/// return an unresolved assignment rather than guessing or substituting a default.
///
class AeroraRoleResolver
{
public:
    virtual ~AeroraRoleResolver() = default;
    [[nodiscard]] virtual AeroraRoleAssignment resolve(const AeroraOperatorIdentity& identity) const = 0;
};

/// \brief Decides whether a role assignment may perform an operation.
///
/// This interface is the only place a role matrix will ever live, and it is injected.
/// \c isConfigured() exists so a station with no approved policy denies rather than
/// guesses: absence of policy is not permission.
///
class AeroraAuthorizationPolicy
{
public:
    virtual ~AeroraAuthorizationPolicy() = default;

    [[nodiscard]] virtual bool isConfigured() const = 0;

    [[nodiscard]] virtual AeroraAuthorizationDecision evaluate(const AeroraRoleAssignment& roles,
                                                               const AeroraGatedOperation& operation) const = 0;
};

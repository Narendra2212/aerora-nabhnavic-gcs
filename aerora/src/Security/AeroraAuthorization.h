#pragma once

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

/// \brief The operation being gated.
///
/// Mirrors the core hook's parameters. \c mavCommand is a MAV_CMD value carried as int:
/// no MAVLink identifier is invented, declared or redefined by Aerora. Parameters are
/// included because they carry the operation's meaning - for example arm versus disarm
/// is distinguished only by param1 - and a policy that cannot see them cannot decide
/// correctly.
///
struct AeroraGatedOperation {
    int mavCommand = 0;
    int targetCompId = 0;
    bool commandInt = false;
    AeroraVehicleContext vehicle;
    float param1 = 0.0f;
    float param2 = 0.0f;
    float param3 = 0.0f;
    float param4 = 0.0f;
    double param5 = 0.0;
    double param6 = 0.0;
    float param7 = 0.0f;
};

/// \brief A decision, with the cause retained for audit.
struct AeroraAuthorizationDecision {
    AeroraAuthorizationOutcome outcome = AeroraAuthorizationOutcome::DeniedNotConfigured;

    /// Machine-readable cause supplied by the policy. Never a role name.
    QString reasonCode;

    [[nodiscard]] bool allowed() const { return outcome == AeroraAuthorizationOutcome::Allowed; }

    static QString outcomeToString(AeroraAuthorizationOutcome outcome);
};

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

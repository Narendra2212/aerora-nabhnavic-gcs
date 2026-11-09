#include "AeroraRolePolicy.h"

#include <utility>

AeroraRolePolicy::AeroraRolePolicy(QString gatedClass, RoleGrants grants)
    : _gatedClass(std::move(gatedClass))
    , _grants(std::move(grants))
{}

bool AeroraRolePolicy::isConfigured() const
{
    return !_gatedClass.isEmpty() && !_grants.isEmpty();
}

AeroraAuthorizationDecision AeroraRolePolicy::evaluate(const AeroraRoleAssignment& roles,
                                                       const AeroraGatedOperation&) const
{
    AeroraAuthorizationDecision decision;

    // Installed but holding no table. DeniedPolicyUnavailable rather than DeniedNotConfigured:
    // the latter means no policy is installed at all, and reporting it here would hide a
    // misconfigured deployment behind the posture of an unconfigured one.
    if (!isConfigured()) {
        decision.outcome = AeroraAuthorizationOutcome::DeniedPolicyUnavailable;
        decision.reasonCode = QStringLiteral("policy-table-absent");
        return decision;
    }

    // An assignment resolved to no roles is a valid, authority-free answer rather than an error,
    // which is what AeroraRoleAssignment::hasAnyRole() records. It grants nothing.
    if (!roles.hasAnyRole()) {
        decision.outcome = AeroraAuthorizationOutcome::DeniedByPolicy;
        decision.reasonCode = QStringLiteral("no-role-held");
        return decision;
    }

    for (const QString& roleId : roles.roleIds) {
        const RoleGrants::const_iterator granted = _grants.constFind(roleId);
        if (granted != _grants.constEnd() && granted->contains(_gatedClass)) {
            decision.outcome = AeroraAuthorizationOutcome::Allowed;
            return decision;
        }
    }

    // Reached when every held role is absent from the table, or present and granting other
    // classes. Both are the same verdict and deliberately carry the same code: distinguishing them
    // would put the shape of the deployment's table into an audit record.
    decision.outcome = AeroraAuthorizationOutcome::DeniedByPolicy;
    decision.reasonCode = QStringLiteral("class-not-granted-to-held-roles");
    return decision;
}

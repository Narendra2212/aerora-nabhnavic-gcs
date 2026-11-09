#include "AeroraAssignedRoleResolver.h"

#include <utility>

AeroraAssignedRoleResolver::AeroraAssignedRoleResolver(Assignment assignment)
    : _assignment(std::move(assignment))
{}

void AeroraAssignedRoleResolver::setAssignment(Assignment assignment)
{
    // Whole replacement, not a merge. IF-SEC-003: "refreshed state replaces it wholly, never
    // merges". A merge would let a role the issuer has withdrawn survive in the station's view of
    // an operator, and that is the direction that fails unsafely.
    _assignment = std::move(assignment);
}

AeroraRoleAssignment AeroraAssignedRoleResolver::resolve(const AeroraOperatorIdentity& identity) const
{
    AeroraRoleAssignment resolved;

    // Nothing was handed over to resolve. Left unresolved so AeroraCommandAuthorizer reports
    // DeniedRoleUnresolved rather than a policy refusal: an unusable identity is an
    // identity-provider fault and not an unprovisioned operator.
    if (!identity.isValid()) {
        return resolved;
    }

    // The assignment was consulted, so this is resolved whatever it found. An operator absent from
    // it holds no roles, which AeroraRoleAssignment records as a valid authority-free answer and
    // which the policy then refuses by its own rule.
    resolved.resolved = true;
    resolved.roleIds = _assignment.value(identity.opaqueId);

    return resolved;
}

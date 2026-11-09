#pragma once

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "AeroraAuthorization.h"

/// \brief An \c AeroraRoleResolver that answers from the deployment's recorded role assignment.
///
/// \section why Why this can exist now
///
/// `Q-029` is settled and signed: `DG-ROL:role-set` records four roles and the command classes each
/// may authorise. The owner has since stated that the deployment provisions `Remote Pilot` to
/// pilots and `Maintenance Technician` to a maintainer, which
/// [product-completion
/// 15](../../../.kiro/specs/05-product-completion/15-operator-identity-and-the-single-role-deployment.md) records as a
/// **deployment** rather than as a change to the signed set.
///
/// So the signed set says which roles may exist; the deployment says who holds which. This type is
/// the second of those and holds no part of the first: it carries opaque operator identifiers
/// mapped to opaque role identifiers and compares strings it never interprets. No role name is
/// compiled in, for the reason `AeroraRoleAssignment` already gives - a role name compiled in here
/// could not be changed without a release.
///
/// \section replace Why an assignment replaces rather than merges
///
/// `IF-SEC-003` records role resolution as happening "on sign-in, and on credential refresh", and
/// its freshness semantics as "carried with the session; refreshed state replaces it wholly, never
/// merges". `setAssignment()` is that rule in code: a refresh that merged would let a role the
/// cloud has withdrawn survive in the station's view of an operator, which is the one direction
/// that fails unsafely.
///
/// \section closed The three answers, kept apart
///
/// Two of them look alike and mean different things, so the resolver returns different states and
/// the audit trail records different causes.
///
/// | Input | Answer | Why |
/// | --- | --- | --- |
/// | An identity that is not usable - unauthenticated, or an empty id | **Unresolved** | Nothing was handed over to
/// resolve. `AeroraCommandAuthorizer` renders this as `DeniedRoleUnresolved` | | A usable identity the assignment does
/// not carry | **Resolved to no roles** | The assignment *was* consulted and this operator holds nothing.
/// `AeroraRoleAssignment` records that as "a valid, authority-free answer" | | A usable identity the assignment carries
/// | Resolved, with that operator's role identifiers | - |
///
/// The middle row is the one worth keeping distinct. An operator who exists and was never
/// provisioned is a deployment omission with an obvious remedy; an identity that could not be
/// resolved is an identity-provider fault with a different one. Folding them would report the wrong
/// repair into the trail.
///
/// \section unconfigured A resolver with no assignment denies everything
///
/// It resolves every usable identity to no roles, so every command is refused by the policy. That
/// is the correct posture for a station nobody has provisioned, and it does not falsely claim
/// enforcement: `AeroraCommandAuthorizer::isFullyConfigured()` asks the *policy* whether it is
/// configured, and `AeroraRolePolicy::isConfigured()` requires a non-empty table.
class AeroraAssignedRoleResolver : public AeroraRoleResolver
{
public:
    /// Opaque operator identifier to the opaque role identifiers that operator holds.
    using Assignment = QHash<QString, QStringList>;

    AeroraAssignedRoleResolver() = default;
    explicit AeroraAssignedRoleResolver(Assignment assignment);

    [[nodiscard]] AeroraRoleAssignment resolve(const AeroraOperatorIdentity& identity) const override;

    /// Replaces the whole assignment. Never merges - see \section replace.
    void setAssignment(Assignment assignment);

    /// How many operators the assignment carries. For a status projection, never for a decision.
    [[nodiscard]] qsizetype operatorCount() const { return _assignment.size(); }

private:
    Assignment _assignment;
};

#pragma once

#include "UnitTest.h"

/// \brief Asserts `AeroraAssignedRoleResolver`: that it keeps the three answers apart, and that a
/// refresh cannot leave a withdrawn role behind.
///
/// \section why What is worth testing here
///
/// The resolver is a map lookup, and the lookup is not the part that can be wrong. Three things
/// are:
///
/// \li **The unresolved / no-roles distinction.** Both deny, so a defect here changes no verdict
///     and changes the recorded cause - which is the kind of failure nothing notices until an
///     investigation needs the trail. One case per state, asserting the state rather than the
///     denial.
/// \li **Whole replacement on refresh.** `IF-SEC-003` requires a refreshed assignment to replace
///     wholly and never merge. A merge is the unsafe direction: a role the issuer withdrew would
///     survive. Asserted by withdrawing one and requiring it to be gone.
/// \li **That no role name is compiled in.** The fixtures use opaque tokens, so a resolver that
///     special-cased a signed role name would not be exercised by them and the suite would still
///     pass - which is why there is also a case that the resolver returns whatever it was given,
///     including a token that is not in the signed set at all.
///
/// \section pairing Why this pairs with AeroraRolePolicyTest
///
/// The resolver answers *which roles*, the policy answers *whether those roles may*. Neither
/// suite asserts the other's rule, and the one case that crosses the boundary is here: an operator
/// resolved to no roles must reach the policy as resolved, because the policy's refusal reason for
/// that case is different from the authorizer's reason for an unresolved identity.
class AeroraAssignedRoleResolverTest : public UnitTest
{
    Q_OBJECT

private slots:
    // The three answers.
    void _anUnauthenticatedIdentityIsUnresolved();
    void _anEmptyIdentityIsUnresolved();
    void _anIdentityAbsentFromTheAssignmentResolvesToNoRoles();
    void _anAssignedIdentityResolvesToItsRoles();

    // The distinction those two states exist to keep.
    void _unresolvedAndResolvedToNoRolesAreDifferentStates();

    // Refresh semantics.
    void _refreshingReplacesTheWholeAssignment();
    void _refreshingWithdrawsARoleRatherThanKeepingIt();
    void _refreshingToAnEmptyAssignmentRevokesEveryone();

    // Opacity.
    void _anyRoleTokenIsCarriedThroughUninterpreted();
    void _severalRolesForOneOperatorAreAllCarried();
};

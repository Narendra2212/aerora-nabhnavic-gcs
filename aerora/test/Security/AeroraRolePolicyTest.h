#pragma once

#include "UnitTest.h"

/// \brief Asserts `AeroraRolePolicy`: that it grants only what its table grants, and denies on
/// every other path.
///
/// \section why Why this suite can exist now
///
/// WP2-C prohibited a policy implementation while `Q-029` was open, and the shared test doubles
/// carry the matching warning that no double holds a role matrix "because there is none to hold".
/// `DG-ROL` is `CLOSED` and the role set is signed, so a table exists to evaluate against. The
/// fixtures below still use opaque tokens rather than the signed role names: the signed set is
/// deployment configuration, and a suite that hard-coded it would fail the day a deployment
/// differed from it while the evaluation it tests stayed correct.
///
/// \section closed Why the denials outnumber the grants
///
/// One path allows and five deny, and the five are asserted separately because they are five
/// different deployment faults with five different repairs: no table installed, an identity whose
/// roles did not resolve, an identity resolved to no roles, a held role absent from the table, and
/// a held role present but granting other classes. A policy that collapsed any of them into
/// another would still return "deny" and would report the wrong cause into the audit trail.
///
/// \section notinstalled What this suite does not do
///
/// It never installs the policy on `AeroraCommandAuthorizer`. The authorizer is a process-wide
/// singleton and an installed policy changes the authorization posture of every later test in the
/// run; more to the point, `isAuthorizationEnforced()` is asserted false elsewhere and must stay
/// false until a deployment installs a chain deliberately. This suite evaluates the policy as the
/// pure function it is.
class AeroraRolePolicyTest : public UnitTest
{
    Q_OBJECT

private slots:
    // Configuration.
    void _aPolicyWithNoTableIsNotConfigured();
    void _aPolicyWithNoGatedClassIsNotConfigured();
    void _aPolicyWithATableAndAClassIsConfigured();

    // The one allowing path.
    void _aHeldRoleGrantingTheGatedClassIsAllowed();
    void _anyOneOfSeveralHeldRolesGrantingTheClassIsAllowed();

    // The five denying paths, each with its own cause.
    void _anUnconfiguredPolicyDeniesAsUnavailableRatherThanUnconfigured();
    void _anUnresolvedAssignmentIsDenied();
    void _anAssignmentResolvedToNoRolesIsDenied();
    void _aHeldRoleAbsentFromTheTableIsDenied();
    void _aHeldRoleGrantingOtherClassesIsDenied();

    // Properties that must hold whatever the table says.
    void _theGatedClassIsComparedExactly();
    void _noReasonCodeCarriesARoleName();
    void _theSubjectDoesNotChangeTheVerdict();
};

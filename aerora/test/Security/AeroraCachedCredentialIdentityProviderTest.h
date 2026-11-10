#pragma once

#include "UnitTest.h"

/// \brief Asserts `AeroraCachedCredentialIdentityProvider`: that identity follows presence and
/// freshness follows the window, and that the two are not the same answer.
///
/// \section why The split is the point
///
/// The owner's entry-not-continuation rule turns on identity and freshness being distinct. If a
/// lapsed credential stripped the identity, every command after a mid-flight expiry would be denied
/// for want of an operator - the continuation-blocking failure the rule forbids. So the case that
/// carries the weight is the one where a present-but-expired credential still reports an
/// authenticated operator while reporting not fresh. The rest pins the two axes independently:
///
/// \li **Identity follows presence.** Logged in - a credential cached with a non-empty id - is
///     authenticated, fresh or not. Absent, empty-id, or signed-out is unauthenticated.
/// \li **Freshness follows the window.** Fresh from the issue instant up to but not through expiry,
///     and never without a present credential.
///
/// \section clock Why freshness takes a now
///
/// `isCredentialFreshAt(nowUtcMs)` is the tested core, mirroring `AeroraManualAuthorityGrant::inForce`.
/// `isAuthenticationFresh()` reads the wall clock and is exercised only where the answer is
/// clock-independent - no credential, and a window spanning any plausible present - so the suite
/// never depends on what time it runs. Identity needs no clock at all.
class AeroraCachedCredentialIdentityProviderTest : public UnitTest
{
    Q_OBJECT

private slots:
    // Identity follows presence, not freshness.
    void _withNoCredentialTheOperatorIsNotIdentified();
    void _aPresentCredentialIdentifiesItsOperator();
    void _anExpiredButPresentCredentialStillIdentifiesItsOperator();
    void _aCredentialWithNoOperatorIdIsNotPresent();
    void _anIdentityIsNeverAuthenticatedWithAnEmptyId();

    // Freshness follows the window.
    void _withNoCredentialNothingIsFresh();
    void _aCredentialInsideItsWindowIsFresh();
    void _theFreshnessWindowIsHalfOpenAtBothEnds();
    void _aCredentialBeforeItsIssueInstantIsNotFresh();

    // The split case that the rule turns on.
    void _anExpiredCredentialIsIdentifiedYetNotFresh();

    // Lifecycle.
    void _clearingDropsBothIdentityAndFreshness();
    void _settingACredentialReplacesThePreviousOneWholly();
    void _aShorterReloginDoesNotInheritTheOlderWindow();
    void _aPresentButExpiredCredentialIsStillHeldAndDistinctFromNeverLoggedIn();

    // The wall-clock wrapper, where the answer does not depend on the clock.
    void _theWallClockFreshnessIsFalseWithNoCredential();
    void _theWallClockFreshnessIsTrueForAGenerouslyValidCredential();
};

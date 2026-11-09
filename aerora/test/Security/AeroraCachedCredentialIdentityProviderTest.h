#pragma once

#include "UnitTest.h"

/// \brief Asserts `AeroraCachedCredentialIdentityProvider`: that identity tracks the cached
/// credential's window exactly, and that the mid-flight-expiry case is reported rather than hidden.
///
/// \section why What is worth testing
///
/// The lookup is trivial; three things are not.
///
/// \li **The window boundary.** A credential is valid until its expiry, not through the expiry
///     instant, and an off-by-one there is the difference between a window that closes on time and
///     one that never does. Asserted on both edges through the now-taking core.
/// \li **Present-but-expired versus never-cached.** Both report unauthenticated, so a defect that
///     collapsed them would change no verdict and would change what a status surface can say -
///     "logged in, expired" and "never logged in" have different remedies. `hasCredential()` keeps
///     them apart and is asserted doing so.
/// \li **That it reports the expiry rather than acting on it.** When a credential expires mid-flight
///     the provider reports unauthenticated; it does not and must not decide what happens to an
///     airborne aircraft. That decision is the owner's entry-not-continuation rule, enforced above
///     this type, and the test says so in a comment so no one wires the continuation block in here.
///
/// \section clock Why a now is passed in
///
/// `currentIdentityAt(nowUtcMs)` is the tested core, mirroring `AeroraManualAuthorityGrant::inForce`.
/// The parameterless `currentIdentity()` reads the wall clock and is exercised only where the
/// answer is clock-independent - no credential, and a credential whose window spans any plausible
/// present - so the suite never depends on what time it runs.
class AeroraCachedCredentialIdentityProviderTest : public UnitTest
{
    Q_OBJECT

private slots:
    // No credential.
    void _withNoCredentialTheIdentityIsUnauthenticatedAtAnyTime();
    void _withNoCredentialThereIsNoCredentialHeld();

    // A valid credential.
    void _aCredentialInsideItsWindowAuthenticatesAndCarriesTheOperatorId();
    void _theWindowIsHalfOpenAtBothEnds();

    // Expiry, including the case that matters.
    void _anExpiredCredentialIsUnauthenticatedButStillHeld();
    void _aCredentialBeforeItsIssueInstantIsUnauthenticated();

    // Malformed input.
    void _aCredentialWithNoOperatorIdIsNeverValid();
    void _anIdentityIsNeverAuthenticatedWithAnEmptyId();

    // Lifecycle.
    void _clearingRevokesTheCredential();
    void _settingACredentialReplacesThePreviousOneWholly();
    void _aShorterReloginDoesNotInheritTheOlderExpiry();

    // The wall-clock wrapper, exercised where the answer does not depend on the clock.
    void _theWallClockWrapperIsUnauthenticatedWithNoCredential();
    void _theWallClockWrapperAuthenticatesAGenerouslyValidCredential();
};

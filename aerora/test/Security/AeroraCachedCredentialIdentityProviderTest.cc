#include "AeroraCachedCredentialIdentityProviderTest.h"

#include <limits>

#include <QtCore/QString>
#include <QtTest/QTest>

#include "AeroraAuthorization.h"
#include "AeroraCachedCredentialIdentityProvider.h"

namespace {

// A plausible login time and a window 24 hours wide, in wall-clock milliseconds. The values are
// fixtures, not a compiled-in policy: the type reads whatever issuer set them.
constexpr qint64 kIssued = 1'762'000'000'000;  // a 2025 wall-clock instant
constexpr qint64 kDayMs = 24LL * 60LL * 60LL * 1000LL;
constexpr qint64 kExpires = kIssued + kDayMs;

const QString kOperatorId = QStringLiteral("opaque-operator-cloud-1");

AeroraOperatorCredential credential(const QString& operatorId, qint64 issuedUtcMs, qint64 expiresUtcMs)
{
    AeroraOperatorCredential value;
    value.operatorId = operatorId;
    value.issuedUtcMs = issuedUtcMs;
    value.expiresUtcMs = expiresUtcMs;
    return value;
}

AeroraOperatorCredential validCredential()
{
    return credential(kOperatorId, kIssued, kExpires);
}

}  // namespace

void AeroraCachedCredentialIdentityProviderTest::_withNoCredentialTheIdentityIsUnauthenticatedAtAnyTime()
{
    const AeroraCachedCredentialIdentityProvider provider;

    // A default provider is a station on which no one has logged in. It must authenticate nobody at
    // any instant, including the epoch and the far future.
    QVERIFY(!provider.currentIdentityAt(0).isValid());
    QVERIFY(!provider.currentIdentityAt(kIssued).isValid());
    QVERIFY(!provider.currentIdentityAt(std::numeric_limits<qint64>::max()).isValid());
}

void AeroraCachedCredentialIdentityProviderTest::_withNoCredentialThereIsNoCredentialHeld()
{
    const AeroraCachedCredentialIdentityProvider provider;

    QVERIFY(!provider.hasCredential());
    QCOMPARE(provider.credentialExpiresAtUtcMs(), qint64(0));
}

void AeroraCachedCredentialIdentityProviderTest::_aCredentialInsideItsWindowAuthenticatesAndCarriesTheOperatorId()
{
    const AeroraCachedCredentialIdentityProvider provider(validCredential());

    const AeroraOperatorIdentity identity = provider.currentIdentityAt(kIssued + kDayMs / 2);

    QVERIFY(identity.isValid());
    QVERIFY(identity.authenticated);
    QCOMPARE(identity.opaqueId, kOperatorId);
}

void AeroraCachedCredentialIdentityProviderTest::_theWindowIsHalfOpenAtBothEnds()
{
    const AeroraCachedCredentialIdentityProvider provider(validCredential());

    // Valid at the issue instant and up to but not including the expiry instant. The expiry edge is
    // the one that matters: a credential in force *through* its expiry is a window that never closes.
    QVERIFY(provider.currentIdentityAt(kIssued).isValid());
    QVERIFY(provider.currentIdentityAt(kExpires - 1).isValid());
    QVERIFY(!provider.currentIdentityAt(kExpires).isValid());
    QVERIFY(!provider.currentIdentityAt(kExpires + 1).isValid());
}

void AeroraCachedCredentialIdentityProviderTest::_anExpiredCredentialIsUnauthenticatedButStillHeld()
{
    const AeroraCachedCredentialIdentityProvider provider(validCredential());

    // The 24-hour mark reached while airborne. The provider reports unauthenticated - and nothing
    // more. What happens to the aircraft is the owner's entry-not-continuation rule, enforced above
    // this type; a continuation block wired in here would be exactly the failure that rule forbids.
    QVERIFY(!provider.currentIdentityAt(kExpires + 1).isValid());

    // Still held, so a status surface can say "logged in, expired" rather than "never logged in".
    QVERIFY(provider.hasCredential());
    QCOMPARE(provider.credentialExpiresAtUtcMs(), kExpires);
}

void AeroraCachedCredentialIdentityProviderTest::_aCredentialBeforeItsIssueInstantIsUnauthenticated()
{
    const AeroraCachedCredentialIdentityProvider provider(validCredential());

    // The wall clock reading earlier than the login. A backward clock step is the recorded limit of
    // a wall-clock window, and the defensive answer is that a credential is not yet valid before the
    // instant it was issued rather than silently in force.
    QVERIFY(!provider.currentIdentityAt(kIssued - 1).isValid());
}

void AeroraCachedCredentialIdentityProviderTest::_aCredentialWithNoOperatorIdIsNeverValid()
{
    // A credential carrying a window but no operator identity is malformed, not a valid anonymous
    // session. It must not authenticate even squarely inside its window.
    const AeroraCachedCredentialIdentityProvider provider(credential(QString(), kIssued, kExpires));

    QVERIFY(!provider.currentIdentityAt(kIssued + kDayMs / 2).isValid());
    QVERIFY(!provider.hasCredential());
}

void AeroraCachedCredentialIdentityProviderTest::_anIdentityIsNeverAuthenticatedWithAnEmptyId()
{
    // The invariant the downstream chain relies on: authenticated true never travels with an empty
    // id, so a consumer cannot be handed a usable-looking identity that names nobody.
    const AeroraCachedCredentialIdentityProvider provider(validCredential());

    const AeroraOperatorIdentity inside = provider.currentIdentityAt(kIssued);
    const AeroraOperatorIdentity outside = provider.currentIdentityAt(kExpires);

    QVERIFY(!(inside.authenticated && inside.opaqueId.isEmpty()));
    QVERIFY(!(outside.authenticated && outside.opaqueId.isEmpty()));
}

void AeroraCachedCredentialIdentityProviderTest::_clearingRevokesTheCredential()
{
    AeroraCachedCredentialIdentityProvider provider(validCredential());
    QVERIFY(provider.currentIdentityAt(kIssued).isValid());

    provider.clearCredential();

    // Sign-out: unauthenticated regardless of the clock, and nothing held.
    QVERIFY(!provider.currentIdentityAt(kIssued).isValid());
    QVERIFY(!provider.hasCredential());
}

void AeroraCachedCredentialIdentityProviderTest::_settingACredentialReplacesThePreviousOneWholly()
{
    AeroraCachedCredentialIdentityProvider provider(validCredential());

    const QString secondOperator = QStringLiteral("opaque-operator-cloud-2");
    provider.setCredential(credential(secondOperator, kExpires, kExpires + kDayMs));

    // The first operator's window is gone, not retained alongside the second.
    QVERIFY(!provider.currentIdentityAt(kIssued).isValid());

    const AeroraOperatorIdentity identity = provider.currentIdentityAt(kExpires + 1);
    QVERIFY(identity.isValid());
    QCOMPARE(identity.opaqueId, secondOperator);
}

void AeroraCachedCredentialIdentityProviderTest::_aShorterReloginDoesNotInheritTheOlderExpiry()
{
    // The unsafe direction of a merge, asserted directly. A re-login with a shorter window must not
    // keep the previous credential's later expiry.
    AeroraCachedCredentialIdentityProvider provider(validCredential());

    const qint64 shorterExpiry = kIssued + kDayMs / 4;
    provider.setCredential(credential(kOperatorId, kIssued, shorterExpiry));

    QCOMPARE(provider.credentialExpiresAtUtcMs(), shorterExpiry);
    QVERIFY(provider.currentIdentityAt(shorterExpiry - 1).isValid());

    // A time the old credential would still have covered, which the shorter one must not.
    QVERIFY(!provider.currentIdentityAt(shorterExpiry + 1).isValid());
    QVERIFY(!provider.currentIdentityAt(kExpires - 1).isValid());
}

void AeroraCachedCredentialIdentityProviderTest::_theWallClockWrapperIsUnauthenticatedWithNoCredential()
{
    // Exercises the real wall-clock wrapper where the answer does not depend on the time: with no
    // credential cached, no instant authenticates.
    const AeroraCachedCredentialIdentityProvider provider;

    QVERIFY(!provider.currentIdentity().isValid());
}

void AeroraCachedCredentialIdentityProviderTest::_theWallClockWrapperAuthenticatesAGenerouslyValidCredential()
{
    // A window from just after the epoch to the far future covers any plausible present, so the
    // wrapper's answer is clock-independent and the suite stays time-agnostic while still running
    // the production path that reads QDateTime.
    const AeroraCachedCredentialIdentityProvider provider(
        credential(kOperatorId, 1, std::numeric_limits<qint64>::max()));

    const AeroraOperatorIdentity identity = provider.currentIdentity();
    QVERIFY(identity.isValid());
    QCOMPARE(identity.opaqueId, kOperatorId);
}

UT_REGISTER_TEST(AeroraCachedCredentialIdentityProviderTest, TestLabel::Unit)

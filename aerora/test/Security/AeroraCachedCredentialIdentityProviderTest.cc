#include "AeroraCachedCredentialIdentityProviderTest.h"

#include <limits>

#include <QtCore/QString>
#include <QtTest/QTest>

#include "AeroraAuthorization.h"
#include "AeroraCachedCredentialIdentityProvider.h"

namespace {

// A plausible login time and a 24-hour window, in wall-clock milliseconds. Fixtures, not a
// compiled-in policy: the type reads whatever the issuer set.
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

AeroraOperatorCredential freshCredential()
{
    return credential(kOperatorId, kIssued, kExpires);
}

}  // namespace

void AeroraCachedCredentialIdentityProviderTest::_withNoCredentialTheOperatorIsNotIdentified()
{
    const AeroraCachedCredentialIdentityProvider provider;

    QVERIFY(!provider.currentIdentity().isValid());
    QVERIFY(!provider.hasCredential());
    QCOMPARE(provider.credentialExpiresAtUtcMs(), qint64(0));
}

void AeroraCachedCredentialIdentityProviderTest::_aPresentCredentialIdentifiesItsOperator()
{
    const AeroraCachedCredentialIdentityProvider provider(freshCredential());

    const AeroraOperatorIdentity identity = provider.currentIdentity();
    QVERIFY(identity.isValid());
    QVERIFY(identity.authenticated);
    QCOMPARE(identity.opaqueId, kOperatorId);
}

void AeroraCachedCredentialIdentityProviderTest::_anExpiredButPresentCredentialStillIdentifiesItsOperator()
{
    // A credential whose window closed long ago, still cached. Identity follows presence, so the
    // operator is still identified - they are logged in, their login is just stale. This is what
    // keeps the command path resolving a role through a continuation.
    const AeroraCachedCredentialIdentityProvider provider(credential(kOperatorId, kIssued, kIssued + 1));

    const AeroraOperatorIdentity identity = provider.currentIdentity();
    QVERIFY2(identity.isValid(), "a present credential identifies its operator even when expired");
    QCOMPARE(identity.opaqueId, kOperatorId);
}

void AeroraCachedCredentialIdentityProviderTest::_aCredentialWithNoOperatorIdIsNotPresent()
{
    // A window but no operator identity is malformed, not an anonymous session.
    const AeroraCachedCredentialIdentityProvider provider(credential(QString(), kIssued, kExpires));

    QVERIFY(!provider.currentIdentity().isValid());
    QVERIFY(!provider.hasCredential());
    QVERIFY(!provider.isCredentialFreshAt(kIssued + kDayMs / 2));
}

void AeroraCachedCredentialIdentityProviderTest::_anIdentityIsNeverAuthenticatedWithAnEmptyId()
{
    // The invariant the chain relies on: authenticated true never travels with an empty id, so a
    // consumer is never handed a usable-looking identity that names nobody.
    const AeroraCachedCredentialIdentityProvider present(freshCredential());
    const AeroraCachedCredentialIdentityProvider absent;

    const AeroraOperatorIdentity a = present.currentIdentity();
    const AeroraOperatorIdentity b = absent.currentIdentity();
    QVERIFY(!(a.authenticated && a.opaqueId.isEmpty()));
    QVERIFY(!(b.authenticated && b.opaqueId.isEmpty()));
}

void AeroraCachedCredentialIdentityProviderTest::_withNoCredentialNothingIsFresh()
{
    const AeroraCachedCredentialIdentityProvider provider;

    QVERIFY(!provider.isCredentialFreshAt(0));
    QVERIFY(!provider.isCredentialFreshAt(kIssued));
    QVERIFY(!provider.isCredentialFreshAt(std::numeric_limits<qint64>::max()));
}

void AeroraCachedCredentialIdentityProviderTest::_aCredentialInsideItsWindowIsFresh()
{
    const AeroraCachedCredentialIdentityProvider provider(freshCredential());

    QVERIFY(provider.isCredentialFreshAt(kIssued + kDayMs / 2));
}

void AeroraCachedCredentialIdentityProviderTest::_theFreshnessWindowIsHalfOpenAtBothEnds()
{
    const AeroraCachedCredentialIdentityProvider provider(freshCredential());

    // Fresh at the issue instant and up to but not through expiry. The expiry edge is the one that
    // matters: fresh *through* its expiry is a window that never closes.
    QVERIFY(provider.isCredentialFreshAt(kIssued));
    QVERIFY(provider.isCredentialFreshAt(kExpires - 1));
    QVERIFY(!provider.isCredentialFreshAt(kExpires));
    QVERIFY(!provider.isCredentialFreshAt(kExpires + 1));
}

void AeroraCachedCredentialIdentityProviderTest::_aCredentialBeforeItsIssueInstantIsNotFresh()
{
    // A backward clock step is the recorded limit of a wall-clock window; the defensive answer is
    // that a credential is not yet fresh before the instant it was issued.
    const AeroraCachedCredentialIdentityProvider provider(freshCredential());

    QVERIFY(!provider.isCredentialFreshAt(kIssued - 1));
}

void AeroraCachedCredentialIdentityProviderTest::_anExpiredCredentialIsIdentifiedYetNotFresh()
{
    // The whole reason the type is shaped this way, in one assertion. Airborne, 24 hours elapsed:
    // the operator is still identified (logged in) and no longer fresh (window closed). Identity
    // true keeps the command path resolving their role for a continuation; freshness false blocks
    // only a new entry.
    const AeroraCachedCredentialIdentityProvider provider(freshCredential());

    QVERIFY2(provider.currentIdentity().isValid(), "an expired credential must still identify its operator");
    QVERIFY2(!provider.isCredentialFreshAt(kExpires + 1), "an expired credential must report not fresh");
}

void AeroraCachedCredentialIdentityProviderTest::_clearingDropsBothIdentityAndFreshness()
{
    AeroraCachedCredentialIdentityProvider provider(freshCredential());
    QVERIFY(provider.currentIdentity().isValid());
    QVERIFY(provider.isCredentialFreshAt(kIssued));

    provider.clearCredential();

    // Sign-out: neither identified nor fresh, and nothing held.
    QVERIFY(!provider.currentIdentity().isValid());
    QVERIFY(!provider.isCredentialFreshAt(kIssued));
    QVERIFY(!provider.hasCredential());
}

void AeroraCachedCredentialIdentityProviderTest::_settingACredentialReplacesThePreviousOneWholly()
{
    AeroraCachedCredentialIdentityProvider provider(freshCredential());

    const QString secondOperator = QStringLiteral("opaque-operator-cloud-2");
    provider.setCredential(credential(secondOperator, kExpires, kExpires + kDayMs));

    // The identity is the second operator, and the first operator's window is gone.
    QCOMPARE(provider.currentIdentity().opaqueId, secondOperator);
    QVERIFY(!provider.isCredentialFreshAt(kIssued));
    QVERIFY(provider.isCredentialFreshAt(kExpires + 1));
}

void AeroraCachedCredentialIdentityProviderTest::_aShorterReloginDoesNotInheritTheOlderWindow()
{
    // The unsafe direction of a merge, asserted directly: a re-login with a shorter window must not
    // keep the previous credential's later expiry.
    AeroraCachedCredentialIdentityProvider provider(freshCredential());

    const qint64 shorterExpiry = kIssued + kDayMs / 4;
    provider.setCredential(credential(kOperatorId, kIssued, shorterExpiry));

    QCOMPARE(provider.credentialExpiresAtUtcMs(), shorterExpiry);
    QVERIFY(provider.isCredentialFreshAt(shorterExpiry - 1));
    QVERIFY(!provider.isCredentialFreshAt(shorterExpiry + 1));
    QVERIFY(!provider.isCredentialFreshAt(kExpires - 1));
}

void AeroraCachedCredentialIdentityProviderTest::_aPresentButExpiredCredentialIsStillHeldAndDistinctFromNeverLoggedIn()
{
    // "Logged in, expired" and "never logged in" are different conditions with different remedies -
    // re-login versus first login - and hasCredential keeps them apart while both report not fresh.
    const AeroraCachedCredentialIdentityProvider expired(credential(kOperatorId, kIssued, kIssued + 1));
    const AeroraCachedCredentialIdentityProvider never;

    QVERIFY(expired.hasCredential());
    QVERIFY(!never.hasCredential());

    QVERIFY(!expired.isCredentialFreshAt(kExpires));
    QVERIFY(!never.isCredentialFreshAt(kExpires));
}

void AeroraCachedCredentialIdentityProviderTest::_theWallClockFreshnessIsFalseWithNoCredential()
{
    const AeroraCachedCredentialIdentityProvider provider;

    QVERIFY(!provider.isAuthenticationFresh());
}

void AeroraCachedCredentialIdentityProviderTest::_theWallClockFreshnessIsTrueForAGenerouslyValidCredential()
{
    // A window from just after the epoch to the far future covers any plausible present, so the
    // wrapper's answer is clock-independent while still running the production path that reads
    // QDateTime.
    const AeroraCachedCredentialIdentityProvider provider(
        credential(kOperatorId, 1, std::numeric_limits<qint64>::max()));

    QVERIFY(provider.isAuthenticationFresh());
}

UT_REGISTER_TEST(AeroraCachedCredentialIdentityProviderTest, TestLabel::Unit)

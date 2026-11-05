#include "AeroraManualAuthorityTest.h"

#include <QtCore/QString>
#include <QtTest/QTest>

#include "AeroraAuditSink.h"
#include "AeroraAuthorization.h"

namespace {

constexpr qint64 kIssued = 1000;
constexpr qint64 kExpires = 5000;

AeroraManualAuthorityGrant issuedGrant()
{
    AeroraManualAuthorityGrant grant;
    grant.operatorId = QStringLiteral("a-fixture-operator");
    grant.issuedAtMonotonicUs = kIssued;
    grant.expiresAtMonotonicUs = kExpires;
    return grant;
}

}  // namespace

void AeroraManualAuthorityTest::_anUnissuedGrantIsNeverInForce()
{
    // A default grant is the state of a station that has never granted manual authority. It must
    // not read as in force at any instant, including zero: criterion 5 expires a grant when the
    // link drops, and an unissued grant reading as in force would invert exactly that.
    const AeroraManualAuthorityGrant grant;

    QVERIFY(!grant.inForce(0));
    QVERIFY(!grant.inForce(kIssued));
    QVERIFY(!grant.inForce(kExpires));
    QVERIFY(!grant.inForce(kExpires * 2));
}

void AeroraManualAuthorityTest::_anIssuedGrantIsInForceBeforeItsExpiry()
{
    const AeroraManualAuthorityGrant grant = issuedGrant();

    QVERIFY(grant.inForce(kIssued));
    QVERIFY(grant.inForce(kIssued + 1));
    QVERIFY(grant.inForce(kExpires - 1));
}

void AeroraManualAuthorityTest::_aGrantIsNotInForceAtOrAfterItsExpiryInstant()
{
    // The boundary is asserted because an off-by-one here is the difference between a grant that
    // lapses and one that never does. The expiry instant itself is out of force: a grant is in
    // force *until* it expires, not through the moment it expires.
    const AeroraManualAuthorityGrant grant = issuedGrant();

    QVERIFY(!grant.inForce(kExpires));
    QVERIFY(!grant.inForce(kExpires + 1));
}

void AeroraManualAuthorityTest::_aGrantIsNotInForceBeforeItWasIssued()
{
    // The monotonic domain is process-local, so a reader comparing against a stamp from before
    // issue is a real possibility rather than a hypothetical, and the answer must be no.
    const AeroraManualAuthorityGrant grant = issuedGrant();

    QVERIFY(!grant.inForce(kIssued - 1));
    QVERIFY(!grant.inForce(1));
}

void AeroraManualAuthorityTest::_refreshingAGrantExtendsItWithoutReissuing()
{
    // Criterion 3: refresh keeps a grant in force. What refresh moves is the expiry, not the issue
    // time, so the trail still records when manual authority was first granted to this operator
    // rather than when it was last kept alive.
    AeroraManualAuthorityGrant grant = issuedGrant();
    QVERIFY(!grant.inForce(kExpires));

    grant.expiresAtMonotonicUs = kExpires * 2;

    QVERIFY(grant.inForce(kExpires));
    QCOMPARE(grant.issuedAtMonotonicUs, kIssued);
}

void AeroraManualAuthorityTest::_anUnobservedReportIsNotAReportOfCentredSticks()
{
    // The absence of telemetry and telemetry saying the operator is holding still are different
    // facts. A station that reported them the same way would show a centred stick for an aircraft
    // it has heard nothing from, which is the most misleading display available here.
    const AeroraManualControlReport report;

    QVERIFY(!report.observed());
    QCOMPARE(report.observedAtMonotonicUs, 0);
    QVERIFY(report.axes.isEmpty());
    QVERIFY(!report.grantInForceAtVehicle);
}

void AeroraManualAuthorityTest::_anObservedReportCarriesItsAgeAndTheVehicleAnswer()
{
    AeroraManualControlReport report;
    report.observedAtMonotonicUs = kIssued;
    report.grantInForceAtVehicle = true;
    report.axes = {0.0f, 0.25f, -0.5f, 1.0f};

    QVERIFY(report.observed());
    QCOMPARE(report.observedAtMonotonicUs, kIssued);
    QVERIFY(report.grantInForceAtVehicle);
    QCOMPARE(report.axes.size(), 4);
}

void AeroraManualAuthorityTest::_theVehicleGrantAnswerIsIndependentOfTheStationGrant()
{
    // The case that matters: the station believes the grant lapsed and the aircraft reports it in
    // force. That disagreement is the only state in which enforcement is observably doing
    // anything, so neither value may be derived from the other and the display must be able to
    // show both.
    const AeroraManualAuthorityGrant grant = issuedGrant();

    AeroraManualControlReport report;
    report.observedAtMonotonicUs = kExpires + 1;
    report.grantInForceAtVehicle = true;

    QVERIFY(!grant.inForce(kExpires + 1));
    QVERIFY(report.grantInForceAtVehicle);
}

void AeroraManualAuthorityTest::_everyGrantTransitionRendersItsOwnName()
{
    // Requirement 2 criterion 8 records a grant issued, refused, refreshed and expired. Four
    // distinct renderings, because a trail that could not tell a refusal from an expiry could not
    // tell an operator who was told no from one who lost a grant to a dropped link.
    QCOMPARE(AeroraAuditEvent::typeToString(AeroraAuditEventType::ManualAuthorityIssued),
             QStringLiteral("ManualAuthorityIssued"));
    QCOMPARE(AeroraAuditEvent::typeToString(AeroraAuditEventType::ManualAuthorityRefused),
             QStringLiteral("ManualAuthorityRefused"));
    QCOMPARE(AeroraAuditEvent::typeToString(AeroraAuditEventType::ManualAuthorityRefreshed),
             QStringLiteral("ManualAuthorityRefreshed"));
    QCOMPARE(AeroraAuditEvent::typeToString(AeroraAuditEventType::ManualAuthorityExpired),
             QStringLiteral("ManualAuthorityExpired"));
}

void AeroraManualAuthorityTest::_noGrantTransitionRendersAsUnknown()
{
    // The switch returns Unknown for an unhandled value, so this is the case that fails if a
    // transition is added to the enumeration and not to the renderer. An audit record reading
    // Unknown is a record that cannot be queried for the thing it was written to prove.
    const AeroraAuditEventType transitions[] = {
        AeroraAuditEventType::ManualAuthorityIssued,
        AeroraAuditEventType::ManualAuthorityRefused,
        AeroraAuditEventType::ManualAuthorityRefreshed,
        AeroraAuditEventType::ManualAuthorityExpired,
    };

    for (const AeroraAuditEventType transition : transitions) {
        const QString rendered = AeroraAuditEvent::typeToString(transition);
        QVERIFY(!rendered.isEmpty());
        QVERIFY(rendered != QStringLiteral("Unknown"));
    }
}

UT_REGISTER_TEST(AeroraManualAuthorityTest, TestLabel::Unit)

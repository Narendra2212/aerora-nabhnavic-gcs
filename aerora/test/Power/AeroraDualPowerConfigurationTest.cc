#include "AeroraDualPowerConfigurationTest.h"

#include <QtTest/QTest>

#include "AeroraDualPowerConfiguration.h"

UT_REGISTER_TEST(AeroraDualPowerConfigurationTest, TestLabel::Unit)

void AeroraDualPowerConfigurationTest::_interlockIsConsistentOnlyWhenBothParametersAgree_data()
{
    QTest::addColumn<bool>("tetherPower");
    QTest::addColumn<bool>("tetherFence");
    QTest::addColumn<bool>("expectConsistent");

    // Both halves of the agreement, and both misconfigurations. The first row is untethered
    // operation and the second is fully tethered operation; both are coherent.
    QTest::newRow("battery power, untethered fence") << false << false << true;
    QTest::newRow("tether power, tethered fence") << true << true << true;

    // The dangerous one: tether power disarms the battery failsafe while the fence still permits
    // flight beyond tether length.
    QTest::newRow("tether power, untethered fence") << true << false << false;

    // The benign one, still a misconfiguration: the fence is clamped to tether length while the
    // battery failsafe is live.
    QTest::newRow("battery power, tethered fence") << false << true << false;
}

void AeroraDualPowerConfigurationTest::_interlockIsConsistentOnlyWhenBothParametersAgree()
{
    QFETCH(bool, tetherPower);
    QFETCH(bool, tetherFence);
    QFETCH(bool, expectConsistent);

    QCOMPARE(AeroraDualPowerConfiguration::interlockConsistentFrom(tetherPower, tetherFence), expectConsistent);
}

void AeroraDualPowerConfigurationTest::_reserveCapacityIsUnknownWhenTetheredOrUnread_data()
{
    QTest::addColumn<bool>("powerSourceKnown");
    QTest::addColumn<bool>("tetherPower");
    QTest::addColumn<bool>("expectUnknown");

    // The only case where the reserve is tracked: the configuration was read and says battery
    // power, so the battery sensor and failsafe are live.
    QTest::newRow("read, battery power") << true << false << false;

    // Tether power disables the sensor, so the reserve is carried but not observed.
    QTest::newRow("read, tether power") << true << true << true;

    // Fail closed. An unread configuration must not imply a monitored reserve, whichever way the
    // stale value happens to sit.
    QTest::newRow("unread, value false") << false << false << true;
    QTest::newRow("unread, value true") << false << true << true;
}

void AeroraDualPowerConfigurationTest::_reserveCapacityIsUnknownWhenTetheredOrUnread()
{
    QFETCH(bool, powerSourceKnown);
    QFETCH(bool, tetherPower);
    QFETCH(bool, expectUnknown);

    QCOMPARE(AeroraDualPowerConfiguration::reserveCapacityUnknownFrom(powerSourceKnown, tetherPower), expectUnknown);
}

void AeroraDualPowerConfigurationTest::_noVehicleLeavesEveryReadingUnknown()
{
    AeroraDualPowerConfiguration configuration(nullptr);

    QVERIFY(!configuration.isPowerSourceKnown());
    QVERIFY(!configuration.isFenceModeKnown());
    QVERIFY(!configuration.isFenceRadiusKnown());
    QVERIFY(!configuration.isInterlockKnown());

    // The values behind the unknowns carry no claim, and are the inert defaults rather than
    // anything read.
    QVERIFY(!configuration.isTetherPowerConfigured());
    QVERIFY(!configuration.isTetherFenceConfigured());
    QCOMPARE(configuration.tetherFenceRadiusMetres(), 0.0F);
}

void AeroraDualPowerConfigurationTest::_noVehicleStillReportsTheReserveAsUnknown()
{
    AeroraDualPowerConfiguration configuration(nullptr);

    // The fail-closed direction, asserted at the object rather than only at the static seam: an
    // aircraft this build cannot read must not be presented as having a monitored reserve.
    QVERIFY(configuration.isReserveCapacityUnknown());
}

void AeroraDualPowerConfigurationTest::_anUnknownInterlockIsNotReportedConsistent()
{
    AeroraDualPowerConfiguration configuration(nullptr);

    // Guards the specific misreading this class exists to prevent: with nothing read, both
    // parameters default to false, so the raw comparison would say "consistent". The known-ness
    // gate must suppress that verdict instead of publishing a reassuring one.
    QVERIFY(!configuration.isInterlockKnown());
    QVERIFY(!configuration.isInterlockConsistent());
    QVERIFY(AeroraDualPowerConfiguration::interlockConsistentFrom(false, false));
}

void AeroraDualPowerConfigurationTest::_prohibitionsAndThresholdsArePinned()
{
    // A threshold here would be a power limit nobody specified, enforced by the one component with
    // no authority to enforce it.
    static_assert(AeroraDualPowerConfiguration::kDefinedThresholdCount == 0);

    // The station reports the interlock disagreement and does not correct it: which of the two
    // parameters is the mistaken one is not knowable here, and the aircraft accepts a write only
    // while disarmed.
    static_assert(!AeroraDualPowerConfiguration::kStationInitiatedPowerActionImplemented);

    // The sources are diode-OR'd onto one rail and a 48 V converter output sits inside a 14S
    // pack's own 46.2-58.8 V range, so no measurement available here separates them.
    static_assert(!AeroraDualPowerConfiguration::kLivePowerSourceInferenceImplemented);

    static_assert(!AeroraDualPowerConfiguration::kReserveCapacityEstimationImplemented);

    QCOMPARE(AeroraDualPowerConfiguration::kDefinedThresholdCount, 0);
}

void AeroraDualPowerConfigurationTest::_parameterNamesMatchTheFirmwareTable()
{
    // Pinned against fw_lib/param_defs.m, which is the source of truth for anything matched
    // against a MAVLink param_id. These are the one part of this module that goes on the wire, so a
    // rename silently stops reading the aircraft rather than failing loudly - this test is what
    // makes it loud.
    QCOMPARE(QString::fromLatin1(AeroraDualPowerConfiguration::kTetheredPowerParameter),
             QStringLiteral("BAT_TETHERED"));
    QCOMPARE(QString::fromLatin1(AeroraDualPowerConfiguration::kTetherFenceModeParameter),
             QStringLiteral("TETHER_MODE"));

    // Deliberately NOT the dictionary name. PARAM_TABLE.md calls parameter 204
    // FENCE_TETHER_RADIUS_M, which is 21 characters and cannot be a param_id; param_defs.m:245
    // sends FENCE_TETH_R_M. Reading the dictionary name would leave this permanently unknown with
    // no error raised anywhere, which is the failure this row exists to prevent.
    QCOMPARE(QString::fromLatin1(AeroraDualPowerConfiguration::kTetherFenceRadiusParameter),
             QStringLiteral("FENCE_TETH_R_M"));
    QVERIFY(QStringLiteral("FENCE_TETHER_RADIUS_M").size() > AeroraDualPowerConfiguration::kMaxParameterNameLength);

    // Every name this module sends must fit the protocol field, checked rather than assumed.
    for (const char* name : {AeroraDualPowerConfiguration::kTetheredPowerParameter,
                             AeroraDualPowerConfiguration::kTetherFenceModeParameter,
                             AeroraDualPowerConfiguration::kTetherFenceRadiusParameter}) {
        QVERIFY(QString::fromLatin1(name).size() <= AeroraDualPowerConfiguration::kMaxParameterNameLength);
    }
}

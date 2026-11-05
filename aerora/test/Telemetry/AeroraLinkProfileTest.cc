#include "AeroraLinkProfileTest.h"

#include <optional>

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtTest/QTest>

#include "AeroraLinkProfile.h"

namespace {

constexpr const char *kFixtureClass = "a-fixture-link-class";
constexpr const char *kFixtureRevision = "r0-values-absent";
constexpr const char *kFixtureTier = "a-fixture-tier";
constexpr const char *kFixtureParameter = "aFixtureParameter";

AeroraLinkProfile unsettledProfile()
{
    return AeroraLinkProfile(QString::fromLatin1(kFixtureClass), {}, QStringLiteral("Q-015"),
                             QString::fromLatin1(kFixtureRevision));
}

AeroraLinkProfile populatedProfile()
{
    QHash<QString, double> parameters;
    parameters.insert(QString::fromLatin1(kFixtureParameter), 3.0);
    return AeroraLinkProfile(QString::fromLatin1(kFixtureClass), parameters,
                             QStringLiteral("Q-015"), QString::fromLatin1(kFixtureRevision));
}

}  // namespace

void AeroraLinkProfileTest::_aProfileCarryingNoValuesSaysSo()
{
    // The state of every profile the register records today. heldBy is asserted present because an
    // empty parameter set with nothing named reads as an omission rather than as the recorded open
    // decision Requirement 5 criterion 12 asks for.
    const AeroraLinkProfile profile = unsettledProfile();

    QVERIFY(!profile.carriesValues());
    QCOMPARE(profile.linkClass(), QString::fromLatin1(kFixtureClass));
    QCOMPARE(profile.heldBy(), QStringLiteral("Q-015"));
    QCOMPARE(profile.revision(), QString::fromLatin1(kFixtureRevision));
}

void AeroraLinkProfileTest::_aProfileCarryingValuesSaysSoAndKeepsItsRevision()
{
    const AeroraLinkProfile profile = populatedProfile();

    QVERIFY(profile.carriesValues());
    QCOMPARE(profile.revision(), QString::fromLatin1(kFixtureRevision));
}

void AeroraLinkProfileTest::_noFigureIsYieldedWhileTheProfileCarriesNoValues()
{
    const AeroraLinkProfile profile = unsettledProfile();
    const QString tier = QString::fromLatin1(kFixtureTier);

    QVERIFY(!profile.sustainedThroughput().has_value());
    QVERIFY(!profile.achievableUpdateInterval(tier).has_value());
    QVERIFY(!profile.aircraftSustainedAtTier(tier).has_value());
}

void AeroraLinkProfileTest::_noFigureIsYieldedAsAZero()
{
    // The distinction the whole optional exists for. A zero returned here would reach an operator
    // as a measured zero - a link sustaining no aircraft, a throughput of nothing - which is worse
    // than a blank because it is actionable and wrong. Asserted on a populated profile too, because
    // the arithmetic Q-015 holds must not be stubbed to zero when it arrives.
    const std::optional<AeroraDerivedLinkFigure> unsettled = unsettledProfile().sustainedThroughput();
    const std::optional<AeroraDerivedLinkFigure> populated = populatedProfile().sustainedThroughput();

    QVERIFY(unsettled == std::nullopt);
    QVERIFY(populated == std::nullopt);
}

void AeroraLinkProfileTest::_noTierIsReportedSustainableWhileTheMembershipsAreUnrecorded()
{
    // Requirement 6 criterion 9 forbids offering a tier a class cannot sustain. Not knowing must
    // not read as yes, and it must not read as no either: a boolean would have had to say one of
    // them, and NotSustainable would claim this link cannot do it rather than that nobody has said.
    const QString tier = QString::fromLatin1(kFixtureTier);

    QCOMPARE(unsettledProfile().tierSupport(tier), AeroraTierSupport::NotDeterminable);
    QCOMPARE(populatedProfile().tierSupport(tier), AeroraTierSupport::NotDeterminable);
}

void AeroraLinkProfileTest::_anAbsentIntervalOrThresholdDeterminesNothing_data()
{
    QTest::addColumn<bool>("hasInterval");
    QTest::addColumn<bool>("hasThreshold");

    QTest::newRow("neither") << false << false;
    QTest::newRow("interval only") << true << false;
    QTest::newRow("threshold only") << false << true;
}

void AeroraLinkProfileTest::_anAbsentIntervalOrThresholdDeterminesNothing()
{
    // Three rows rather than one, because either input alone is enough to make the comparison
    // impossible and a rule that only checked one of them would silently compare against a default.
    QFETCH(bool, hasInterval);
    QFETCH(bool, hasThreshold);

    const std::optional<double> interval =
        hasInterval ? std::optional<double>(500.0) : std::nullopt;
    const std::optional<double> threshold =
        hasThreshold ? std::optional<double>(500.0) : std::nullopt;

    QCOMPARE(thresholdAchievability(interval, threshold),
             AeroraThresholdAchievability::NotDeterminable);
}

void AeroraLinkProfileTest::_aThresholdShorterThanTheIntervalIsUnachievable()
{
    // The case Requirement 7 criterion 8 exists for: a threshold written for a faster link. Without
    // this the station reports every value the class carries STALE, and an operator shown permanent
    // staleness learns to ignore the indicator.
    QCOMPARE(thresholdAchievability(1000.0, 999.0), AeroraThresholdAchievability::Unachievable);
}

void AeroraLinkProfileTest::_aThresholdEqualToTheIntervalIsAchievable()
{
    // The boundary, asserted because it is a judgement rather than an accident. A threshold equal
    // to the interval is met exactly, and rejecting it would reject a threshold a reviewer set
    // deliberately to the link rate.
    QCOMPARE(thresholdAchievability(1000.0, 1000.0), AeroraThresholdAchievability::Achievable);
}

void AeroraLinkProfileTest::_aThresholdLongerThanTheIntervalIsAchievable()
{
    QCOMPARE(thresholdAchievability(1000.0, 1001.0), AeroraThresholdAchievability::Achievable);
}

// Property 7: An unachievable threshold is named, not applied.
// Validates: Requirements 7.8.
//
// Enumerated exhaustively rather than sampled. The state space is small and finite - each input is
// present or absent, and when both are present the threshold is below, equal to or above the
// interval - so every combination is reachable by listing them, and a list covers what a sample
// only probably covers. Nine rows, and the three that matter are the orderings.
void AeroraLinkProfileTest::_anUnachievableThresholdIsNamedNotApplied_data()
{
    QTest::addColumn<bool>("hasInterval");
    QTest::addColumn<bool>("hasThreshold");
    QTest::addColumn<double>("intervalMs");
    QTest::addColumn<double>("thresholdMs");
    QTest::addColumn<int>("expected");

    const int notDeterminable = static_cast<int>(AeroraThresholdAchievability::NotDeterminable);
    const int achievable = static_cast<int>(AeroraThresholdAchievability::Achievable);
    const int unachievable = static_cast<int>(AeroraThresholdAchievability::Unachievable);

    // Absence on either side determines nothing, whatever the other side holds. Three rows rather
    // than one, because a rule checking only one presence would compare against a default.
    QTest::newRow("neither") << false << false << 0.0 << 0.0 << notDeterminable;
    QTest::newRow("interval only") << true << false << 1000.0 << 0.0 << notDeterminable;
    QTest::newRow("threshold only") << false << true << 0.0 << 1000.0 << notDeterminable;

    // The three orderings. Below is the case Requirement 7 criterion 8 exists for.
    QTest::newRow("threshold below interval") << true << true << 1000.0 << 999.0 << unachievable;
    QTest::newRow("threshold equals interval") << true << true << 1000.0 << 1000.0 << achievable;
    QTest::newRow("threshold above interval") << true << true << 1000.0 << 1001.0 << achievable;

    // The boundary from both sides at a different magnitude, so the equality case is not passing
    // on one convenient pair of numbers.
    QTest::newRow("small equal") << true << true << 1.0 << 1.0 << achievable;
    QTest::newRow("small below") << true << true << 2.0 << 1.0 << unachievable;
    QTest::newRow("large below") << true << true << 60000.0 << 59999.0 << unachievable;
}

void AeroraLinkProfileTest::_anUnachievableThresholdIsNamedNotApplied()
{
    QFETCH(bool, hasInterval);
    QFETCH(bool, hasThreshold);
    QFETCH(double, intervalMs);
    QFETCH(double, thresholdMs);
    QFETCH(int, expected);

    const std::optional<double> interval =
        hasInterval ? std::optional<double>(intervalMs) : std::nullopt;
    const std::optional<double> threshold =
        hasThreshold ? std::optional<double>(thresholdMs) : std::nullopt;

    const AeroraThresholdAchievability achievability = thresholdAchievability(interval, threshold);

    QCOMPARE(static_cast<int>(achievability), expected);

    // The half the property name turns on: an unachievable threshold is *named* as unachievable
    // and never silently applied. There is no path by which this function reports a threshold it
    // cannot meet as merely achievable, which is what would mark every value on the link STALE.
    if (achievability == AeroraThresholdAchievability::Unachievable) {
        QVERIFY(interval.has_value());
        QVERIFY(threshold.has_value());
        QVERIFY(threshold.value() < interval.value());
    }
}

UT_REGISTER_TEST(AeroraLinkProfileTest, TestLabel::Unit)

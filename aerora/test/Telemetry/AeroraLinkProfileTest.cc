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

UT_REGISTER_TEST(AeroraLinkProfileTest, TestLabel::Unit)

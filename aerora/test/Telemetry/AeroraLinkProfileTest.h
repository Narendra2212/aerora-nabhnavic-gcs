#pragma once

#include "UnitTest.h"

/// \brief Asserts the Link_Profile derivation shape and the freshness-threshold achievability rule.
///
/// \section held What is deliberately not tested, because it is not written
///
/// The throughput arithmetic is held by `Q-015`: the parameter set a Link_Class records is
/// unsettled, so a formula written now would be written against parameters that do not exist. The
/// tier memberships are held by `DG-TLM`. What is testable, and is the half that decides whether an
/// operator sees unknown or sees a number that is wrong, is the shape around the arithmetic.
///
/// \section absent Why absent is asserted and not assumed
///
/// Requirement 5 criterion 9 forbids presenting a default for a figure the profile does not yield.
/// A zero returned from a derivation reaches an operator as a measured zero, which is worse than a
/// blank because it is actionable and wrong. So every figure returns an optional and the cases
/// below assert the optional is empty rather than that the value is zero.
///
/// \section flood Why achievability exists at all
///
/// Requirement 7 criterion 8. A freshness threshold shorter than the interval a link can sustain
/// marks every value it carries `STALE` -- not because anything is wrong, but because the threshold
/// was written for a faster link. An operator shown a permanent staleness indicator learns to
/// ignore it, which costs the one case where it was telling the truth. The rule is pure logic over
/// two optionals and is fully implementable today even though both inputs are absent, so it is the
/// one part of Requirement 7 that needs no signature.
class AeroraLinkProfileTest : public UnitTest
{
    Q_OBJECT

private slots:
    // The recorded profile, and what it reports about itself.
    void _aProfileCarryingNoValuesSaysSo();
    void _aProfileCarryingValuesSaysSoAndKeepsItsRevision();

    // The absent-value semantics of every derived figure.
    void _noFigureIsYieldedWhileTheProfileCarriesNoValues();
    void _noFigureIsYieldedAsAZero();
    void _noTierIsReportedSustainableWhileTheMembershipsAreUnrecorded();

    // The achievability rule, which is implementable now.
    void _anAbsentIntervalOrThresholdDeterminesNothing_data();
    void _anAbsentIntervalOrThresholdDeterminesNothing();
    void _aThresholdShorterThanTheIntervalIsUnachievable();
    void _aThresholdEqualToTheIntervalIsAchievable();
    void _aThresholdLongerThanTheIntervalIsAchievable();

    // Property 7: an unachievable threshold is named, not applied.
    void _anUnachievableThresholdIsNamedNotApplied_data();
    void _anUnachievableThresholdIsNamedNotApplied();
};

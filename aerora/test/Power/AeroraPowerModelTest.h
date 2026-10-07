#pragma once

#include "UnitTest.h"

class AeroraReportedValue;

/// \brief Verifies that the power abstraction represents what the vehicle reports and nothing more:
/// configurations stay independent, no transition is inferred, effective and reported never
/// converge, the watchdog stays separate, and no threshold, arbitration, estimation or outbound
/// action exists.
///
/// Every test runs against a deterministic clock (\c ManualScheduler) through \c AeroraPowerMock, so
/// the staleness transition is produced by advancing time explicitly rather than by waiting for it.
///
/// Three of these are prohibition tests. They name the regressions they catch: a future change that
/// reads two configuration reports as a source transition, that lets a stale effective state be
/// replaced by a fresh ordinary report, or that introduces a voltage or current threshold the
/// ground station would then be enforcing without authority.
class AeroraPowerModelTest : public UnitTest
{
    Q_OBJECT

private slots:
    // The item set and its separation.
    void _itemsStartUnknownAndAreSeparate();
    void _reportsDoNotLeakBetweenItems_data();
    void _reportsDoNotLeakBetweenItems();
    void _unknownAndUnavailableStayDistinct();

    // The two prohibitions that are easiest to breach by accident.
    void _noTransitionIsInferredFromAConfigurationChange();
    void _staleEffectiveStateIsNeverReplacedByAReport();

    // Capability gating — IF-PWR-012.
    void _capabilityGatesExtendedSurface();

    // The refusals, pinned — AERORA-POWER-REQ-008, Q-024, Q-026, IF-PWR-008.
    void _prohibitionsAndThresholdsArePinned();

    // Teardown.
    void _disconnectClearsReportsAndRestoresNothing();

    // The vehicle-composition contract. M-Vehicle asserts two vehicles get two evaluators; this
    // asserts the complement — within one vehicle, two models share one evaluator and neither owns
    // it, which is where a double-ownership defect would hide.
    void _twoModelsShareOneEvaluatorAndNeitherOwnsIt();

    /// Permanent regression guard for the DEG-04 invariant across evaluator destruction, at model
    /// level. HO-TAB6-01, fixed by the staleness latch in `AeroraReportedValue`; the primitive
    /// itself is covered by `AeroraReportedValueTest`.
    void _destroyedEvaluatorMustNotRefreshAStaleObservation();

private:
    /// Forces and consumes the one-per-class warning `AeroraFreshness` emits the first time it
    /// evaluates a freshness class with no configured threshold.
    ///
    /// Q-036 leaves every Aerora class unconfigured, so that warning is the expected state of this
    /// build rather than a fault — but it is still a warning, and strict log checking fails any
    /// test that lets one through unexpected. Each test gets its own evaluator, so the warning
    /// fires exactly once per test, on whichever evaluation of an **observed** item happens first.
    /// Calling this immediately after the first report makes that position explicit instead of
    /// incidental.
    ///
    /// @a value must already be observed; an unobserved item never reaches the evaluator. Tests
    /// that register a threshold before reporting never reach this path at all.
    void _consumeUnconfiguredThresholdWarning(AeroraReportedValue *value);
};

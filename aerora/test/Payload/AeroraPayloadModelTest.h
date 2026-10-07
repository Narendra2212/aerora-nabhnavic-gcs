#pragma once

#include "UnitTest.h"

class AeroraReportedValue;

/// \brief Verifies that the payload abstraction keeps its five reported items separate, gates
/// controls on reported capability, never turns a ground-station request into payload state, and
/// never turns a reported condition into an action.
///
/// Every test runs against a deterministic clock (\c ManualScheduler) through \c AeroraPayloadMock,
/// and touches no vehicle, no link and no wall clock.
///
/// Four of these are prohibition tests — they assert that something does **not** happen. They are
/// the conversions 10-payload-implementation.md and R3-007 ask for, and each names the regression it
/// catches: a future change that wires a payload condition to a command, that promotes a request to
/// state, that presents unknown health as healthy, or that restores payload state across a
/// reconnection.
class AeroraPayloadModelTest : public UnitTest
{
    Q_OBJECT

private slots:
    // The item set and its separation.
    void _itemsStartUnknownAndAreSeparate();
    void _reportsDoNotLeakBetweenItems_data();
    void _reportsDoNotLeakBetweenItems();
    void _unknownAndUnavailableStayDistinct();

    // Capability gating — IF-PAY-002.
    void _capabilityGatesControls();

    // The request boundary — IF-PAY-003, IF-PAY-009, AERORA-PAYLOAD-REQ-011.
    void _acceptedRequestDoesNotBecomePayloadState();
    void _requestsFailClosed_data();
    void _requestsFailClosed();

    // The prohibitions — IF-PAY-015, AERORA-PAYLOAD-REQ-010.
    void _noReportedConditionProducesARequest();
    void _noActuatorPathIsInstalledByDefault();

    // Teardown — IF-PAY-014.
    void _disconnectClearsReportsAndRestoresNothing();

    // The vehicle-composition contract. M-Vehicle asserts the composition and re-asserts the
    // prohibitions at its root; these assert the ownership contract inside the model, and that the
    // model survives its evaluator.
    void _compositionBorrowsFreshnessAndOwnsItsItems();
    void _outlivingTheFreshnessEvaluatorIsSafe();

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
    /// @a value must already be observed; an unobserved item never reaches the evaluator.
    void _consumeUnconfiguredThresholdWarning(AeroraReportedValue *value);
};

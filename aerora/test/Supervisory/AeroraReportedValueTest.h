#pragma once

#include "UnitTest.h"

/// \brief Verifies the staleness-verdict latch in \c AeroraReportedValue, and that adding it
/// changed no other state semantics.
///
/// The invariant under test, from \c DEG-04 and \c IF-TLM-011:
///
/// > Once a value has been judged \c STALE against a configured threshold, loss of the freshness
/// > evaluator must not make it appear current.
///
/// This is the direct unit test for the defect recorded as HO-TAB6-01. The payload and power suites
/// exercise the same invariant through their models; this one drives \c AeroraReportedValue on its
/// own, so a regression is attributed to the primitive rather than to a consumer.
///
/// Every test injects \c ManualScheduler and advances time explicitly. None sleeps, waits on a wall
/// clock, or names a protocol identifier.
class AeroraReportedValueTest : public UnitTest
{
    Q_OBJECT

private slots:
    // The latch itself.
    void _staleSurvivesEvaluatorDestruction_data();
    void _staleSurvivesEvaluatorDestruction();
    void _staleSurvivesThresholdRemoval();
    void _expiryLatchesTheVerdictWithoutAConsumerRead();

    // The two ways a verdict is withdrawn, and only these two.
    void _newReportWithdrawsTheStaleVerdict();
    void _clearingReportProducesUnknownNotStale();

    // Evaluator absence on its own must never mean stale.
    void _evaluatorLossWithoutAPriorVerdictIsNotStale();

    // The semantics the latch must not have disturbed.
    void _noObservationIsUnknown();
    void _capabilityAbsentIsUnavailable();
    void _freshObservationResolvesToDeclaredKind_data();
    void _freshObservationResolvesToDeclaredKind();
    void _staleIsNeverCurrentForAnyKind_data();
    void _staleIsNeverCurrentForAnyKind();
};

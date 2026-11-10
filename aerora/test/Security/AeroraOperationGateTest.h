#pragma once

#include "UnitTest.h"

/// \brief Asserts `AeroraOperationGate`: that credential freshness blocks exactly one transition,
/// and that a command in flight is never blocked for a stale credential.
///
/// \section why The case that must not regress
///
/// The gate exists to put the credential check at the operation's entry and nowhere else. The
/// dangerous failure is the opposite - a continuation command refused mid-flight because the
/// credential lapsed - because that is the one the owner's answer 1 forbids and the one whose
/// consequence is an aircraft that stops answering the sticks. So the suite's weight is on the
/// in-operation rows: every one of them, at every freshness, must come back not blocked, including
/// the command that lands the aircraft.
///
/// \section table Why it is exhaustive
///
/// The decision is a small function of three booleans over two phases, so the suite walks the whole
/// space rather than sampling it. A gate that blocked the wrong combination would be a safety
/// defect that a representative sample could miss, and the space is small enough that there is no
/// reason to sample.
///
/// \section pure Evaluate is pure
///
/// `evaluate()` returns a decision and changes nothing; `applyAuthorized()` commits a phase, and
/// only for a command that was authorized and sent. The suite asserts the split: evaluating does
/// not move the phase, so a refused or dropped arm cannot leave the gate believing an operation
/// began.
class AeroraOperationGateTest : public UnitTest
{
    Q_OBJECT

private slots:
    // Entry - the one transition freshness guards.
    void _aFreshArmFromOutsideEntersTheOperation();
    void _aStaleArmFromOutsideIsBlockedAndDoesNotEnter();
    void _aBlockedArmCarriesAnAuditReasonThatIsNotARoleName();

    // Continuation - the case that must never block.
    void _aCommandInOperationIsNeverBlockedWhateverTheFreshness();
    void _anArmWhileAlreadyInOperationIsNotReFreshnessChecked();
    void _theLandingCommandSurvivesAnExpiredCredentialInFlight();

    // Exit and re-entry.
    void _aDisarmEndsTheOperation();
    void _aReArmAfterDisarmIsFreshnessCheckedAgain();

    // Outside an operation, not entering.
    void _aPreArmGroundCommandIsNotFreshnessGated();
    void _aDisarmWhileNotInOperationIsBenign();

    // The pure/commit split and lifecycle.
    void _evaluateDoesNotChangeThePhase();
    void _applyAuthorizedCommitsThePhase();
    void _resetReturnsToNotInOperation();
    void _aFullLifecycleHolds();

    // Defensive.
    void _bothEntersAndExitsResolvesToEntering();
};

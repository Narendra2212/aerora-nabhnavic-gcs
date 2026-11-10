#include "AeroraOperationGateTest.h"

#include <QtCore/QString>
#include <QtTest/QTest>

#include "AeroraOperationGate.h"

namespace {

constexpr bool kEnters = true;
constexpr bool kExits = true;
constexpr bool kFresh = true;
constexpr bool kStale = false;

// A gate already in operation, for the continuation cases. Reached the honest way - through an
// authorized fresh arm - so the state under test is one the gate can actually be in.
AeroraOperationGate inOperationGate()
{
    AeroraOperationGate gate;
    gate.applyAuthorized(gate.evaluate(kEnters, !kExits, kFresh));
    return gate;
}

}  // namespace

void AeroraOperationGateTest::_aFreshArmFromOutsideEntersTheOperation()
{
    const AeroraOperationGate gate;

    const AeroraOperationGate::Decision decision = gate.evaluate(kEnters, !kExits, kFresh);

    QVERIFY(!decision.credentialBlocks);
    QVERIFY(decision.nextInOperation);
}

void AeroraOperationGateTest::_aStaleArmFromOutsideIsBlockedAndDoesNotEnter()
{
    const AeroraOperationGate gate;

    const AeroraOperationGate::Decision decision = gate.evaluate(kEnters, !kExits, kStale);

    QVERIFY(decision.credentialBlocks);
    QVERIFY2(!decision.nextInOperation, "a blocked arm must not enter the operation");
}

void AeroraOperationGateTest::_aBlockedArmCarriesAnAuditReasonThatIsNotARoleName()
{
    const AeroraOperationGate gate;

    const AeroraOperationGate::Decision blocked = gate.evaluate(kEnters, !kExits, kStale);
    QVERIFY(!blocked.reasonCode.isEmpty());

    // The signed role names, which a reason code must never carry - a denial reason in the trail
    // must not publish the deployment's role set.
    for (const QString& role : {QStringLiteral("Remote Pilot"), QStringLiteral("Payload Operator"),
                                QStringLiteral("Mission Planner"), QStringLiteral("Maintenance Technician")}) {
        QVERIFY(!blocked.reasonCode.contains(role));
    }

    // And an allowed command carries no reason at all.
    QVERIFY(gate.evaluate(kEnters, !kExits, kFresh).reasonCode.isEmpty());
}

void AeroraOperationGateTest::_aCommandInOperationIsNeverBlockedWhateverTheFreshness()
{
    const AeroraOperationGate gate = inOperationGate();

    // A within-operation command, both freshnesses. Neither blocks: freshness is not consulted
    // once in operation. This is the row that answer 1 turns on.
    QVERIFY(!gate.evaluate(!kEnters, !kExits, kFresh).credentialBlocks);
    QVERIFY(!gate.evaluate(!kEnters, !kExits, kStale).credentialBlocks);

    // And the phase stays in operation either way.
    QVERIFY(gate.evaluate(!kEnters, !kExits, kStale).nextInOperation);
}

void AeroraOperationGateTest::_anArmWhileAlreadyInOperationIsNotReFreshnessChecked()
{
    const AeroraOperationGate gate = inOperationGate();

    // An arm sent while already armed is unusual, but it must not re-trigger the freshness check:
    // the operation is already in force, and A2 says nothing in force is freshness-blocked.
    const AeroraOperationGate::Decision decision = gate.evaluate(kEnters, !kExits, kStale);

    QVERIFY(!decision.credentialBlocks);
    QVERIFY(decision.nextInOperation);
}

void AeroraOperationGateTest::_theLandingCommandSurvivesAnExpiredCredentialInFlight()
{
    // The concrete safety case in one test. Airborne (in operation), credential expired (stale),
    // operator commands something - land, RTL, disarm, any within-operation command. It must not
    // be blocked, because the aircraft that stops answering is the failure the whole rule exists
    // to prevent.
    const AeroraOperationGate gate = inOperationGate();

    const AeroraOperationGate::Decision land = gate.evaluate(!kEnters, !kExits, kStale);
    QVERIFY2(!land.credentialBlocks, "an expired credential must not block a command in flight");

    const AeroraOperationGate::Decision disarm = gate.evaluate(!kEnters, kExits, kStale);
    QVERIFY2(!disarm.credentialBlocks, "an expired credential must not block the disarm that ends the flight");
    QVERIFY(!disarm.nextInOperation);
}

void AeroraOperationGateTest::_aDisarmEndsTheOperation()
{
    const AeroraOperationGate gate = inOperationGate();

    const AeroraOperationGate::Decision decision = gate.evaluate(!kEnters, kExits, kFresh);

    QVERIFY(!decision.credentialBlocks);
    QVERIFY2(!decision.nextInOperation, "disarm must end the operation");
}

void AeroraOperationGateTest::_aReArmAfterDisarmIsFreshnessCheckedAgain()
{
    AeroraOperationGate gate = inOperationGate();

    // Disarm, which ends the operation.
    gate.applyAuthorized(gate.evaluate(!kEnters, kExits, kFresh));
    QVERIFY(!gate.inOperation());

    // The next arm is a new entry, so freshness is checked again: stale blocks, fresh enters.
    QVERIFY(gate.evaluate(kEnters, !kExits, kStale).credentialBlocks);
    QVERIFY(!gate.evaluate(kEnters, !kExits, kFresh).credentialBlocks);
}

void AeroraOperationGateTest::_aPreArmGroundCommandIsNotFreshnessGated()
{
    const AeroraOperationGate gate;  // not in operation

    // A command that is neither an arm nor a disarm, sent on the ground with a stale credential.
    // Not blocked: freshness is checked only at entry, so an operator whose credential just lapsed
    // can still configure and upload, and is refused only when they try to arm. B, earlier.
    const AeroraOperationGate::Decision decision = gate.evaluate(!kEnters, !kExits, kStale);

    QVERIFY(!decision.credentialBlocks);
    QVERIFY(!decision.nextInOperation);
}

void AeroraOperationGateTest::_aDisarmWhileNotInOperationIsBenign()
{
    const AeroraOperationGate gate;  // not in operation

    const AeroraOperationGate::Decision decision = gate.evaluate(!kEnters, kExits, kStale);

    QVERIFY(!decision.credentialBlocks);
    QVERIFY(!decision.nextInOperation);
}

void AeroraOperationGateTest::_evaluateDoesNotChangeThePhase()
{
    const AeroraOperationGate gate;  // not in operation

    // Evaluating an entering command does not move the phase - only applyAuthorized does. A const
    // gate that stayed const is the guarantee a refused or dropped arm leaves no trace.
    static_cast<void>(gate.evaluate(kEnters, !kExits, kFresh));
    QVERIFY(!gate.inOperation());
}

void AeroraOperationGateTest::_applyAuthorizedCommitsThePhase()
{
    AeroraOperationGate gate;

    gate.applyAuthorized(gate.evaluate(kEnters, !kExits, kFresh));
    QVERIFY(gate.inOperation());

    gate.applyAuthorized(gate.evaluate(!kEnters, kExits, kFresh));
    QVERIFY(!gate.inOperation());
}

void AeroraOperationGateTest::_resetReturnsToNotInOperation()
{
    AeroraOperationGate gate = inOperationGate();
    QVERIFY(gate.inOperation());

    gate.reset();
    QVERIFY(!gate.inOperation());
}

void AeroraOperationGateTest::_aFullLifecycleHolds()
{
    AeroraOperationGate gate;

    // Out -> fresh arm -> in.
    AeroraOperationGate::Decision step = gate.evaluate(kEnters, !kExits, kFresh);
    QVERIFY(!step.credentialBlocks);
    gate.applyAuthorized(step);
    QVERIFY(gate.inOperation());

    // In -> continuation with a stale credential -> still in, not blocked.
    step = gate.evaluate(!kEnters, !kExits, kStale);
    QVERIFY(!step.credentialBlocks);
    gate.applyAuthorized(step);
    QVERIFY(gate.inOperation());

    // In -> disarm -> out.
    step = gate.evaluate(!kEnters, kExits, kStale);
    QVERIFY(!step.credentialBlocks);
    gate.applyAuthorized(step);
    QVERIFY(!gate.inOperation());

    // Out -> stale re-arm -> blocked, still out.
    step = gate.evaluate(kEnters, !kExits, kStale);
    QVERIFY(step.credentialBlocks);
    gate.applyAuthorized(step);
    QVERIFY(!gate.inOperation());
}

void AeroraOperationGateTest::_bothEntersAndExitsResolvesToEntering()
{
    // A caller error - a command cannot arm and disarm at once. Defined behaviour rather than
    // undefined: entry takes precedence, so the result is deterministic if it ever occurs.
    const AeroraOperationGate gate;

    const AeroraOperationGate::Decision decision = gate.evaluate(kEnters, kExits, kFresh);
    QVERIFY(decision.nextInOperation);
}

UT_REGISTER_TEST(AeroraOperationGateTest, TestLabel::Unit)

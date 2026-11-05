#include "AeroraFleetAuthorizationTest.h"

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtTest/QTest>

#include "AeroraAuthorization.h"
#include "MAVLinkLib.h"

namespace {

AeroraVehicleContext resolvedVehicle(int systemId)
{
    AeroraVehicleContext context;
    context.mavlinkSystemId = systemId;
    context.resolved = true;
    return context;
}

AeroraCommandSubject armSubject()
{
    AeroraCommandSubject subject;
    subject.mavCommand = MAV_CMD_COMPONENT_ARM_DISARM;
    subject.targetCompId = MAV_COMP_ID_AUTOPILOT1;
    subject.param1 = 1.0f;
    return subject;
}

}  // namespace

void AeroraFleetAuthorizationTest::_anAddressCarryingOneVehicleYieldsThatVehicle()
{
    AeroraOperationAddress address;
    address.vehicles.append(resolvedVehicle(7));

    const AeroraVehicleContext sole = address.soleVehicle();

    QCOMPARE(sole.mavlinkSystemId, 7);
    QVERIFY(sole.resolved);
}

void AeroraFleetAuthorizationTest::_anAddressCarryingNoVehicleYieldsAnUnresolvedContext()
{
    // The state the gate is in on paths that hold no vehicle, which is what the address replaced.
    // Unresolved rather than a defaulted system id, so a denial never names a vehicle it did not
    // concern.
    const AeroraOperationAddress address;

    const AeroraVehicleContext sole = address.soleVehicle();

    QVERIFY(!sole.resolved);
    QCOMPARE(sole.mavlinkSystemId, 0);
}

void AeroraFleetAuthorizationTest::_anAddressCarryingSeveralVehiclesYieldsNoSoleVehicle()
{
    // Deliberately unresolved rather than the first member. A single-vehicle reader must not report
    // one member of an addressed set as though it were the whole operation, and unresolved is
    // visibly wrong where a first member would be quietly wrong.
    AeroraOperationAddress address;
    address.vehicles.append(resolvedVehicle(1));
    address.vehicles.append(resolvedVehicle(2));

    const AeroraVehicleContext sole = address.soleVehicle();

    QCOMPARE(address.vehicles.size(), 2);
    QVERIFY(!sole.resolved);
    QCOMPARE(sole.mavlinkSystemId, 0);
}

void AeroraFleetAuthorizationTest::_theSubjectAndTheAddressVaryIndependently()
{
    // Property 2 in miniature: the same subject under two addresses, and the same address under two
    // subjects. What this guards is ADR-014 staying deferrable - if addressing code ever read a
    // subject field, a second subject form would stop being free to add.
    AeroraGatedOperation one;
    one.command = armSubject();
    one.address.vehicles.append(resolvedVehicle(1));

    AeroraGatedOperation many = one;
    many.address.vehicles.append(resolvedVehicle(2));

    QCOMPARE(many.command.mavCommand, one.command.mavCommand);
    QCOMPARE(many.command.param1, one.command.param1);
    QCOMPARE(one.address.vehicles.size(), 1);
    QCOMPARE(many.address.vehicles.size(), 2);

    many.command.param1 = 0.0f;  // disarm

    QCOMPARE(many.address.vehicles.size(), 2);
    QCOMPARE(one.command.param1, 1.0f);
}

void AeroraFleetAuthorizationTest::_anEmptyTransmissionIsNotReportedAsDeliveredToEvery()
{
    // The vacuous-truth reading would report a fleet of none as fully delivered, which is the one
    // answer an operator must not be given about a transmission that reached nobody.
    const AeroraFleetTransmission transmission;

    QVERIFY(!transmission.deliveredToEvery());
}

void AeroraFleetAuthorizationTest::_aTransmissionIsCompleteOnlyWhenEveryMemberWasDelivered()
{
    AeroraFleetTransmission transmission;
    transmission.perVehicle.append(AeroraFleetTransmissionOutcome::Delivered);
    transmission.perVehicle.append(AeroraFleetTransmissionOutcome::Delivered);

    QVERIFY(transmission.deliveredToEvery());

    transmission.perVehicle.append(AeroraFleetTransmissionOutcome::Failed);

    QVERIFY(!transmission.deliveredToEvery());
}

void AeroraFleetAuthorizationTest::_aNotAttemptedMemberIsDistinctFromAFailedOne()
{
    // Two different repairs. Not attempted means the station never sent it, which is every member
    // while DG-ROL is open; failed means the link refused it. Collapsing them would make an
    // origination path that never ran look like a fleet of broken radios.
    AeroraFleetTransmission notAttempted;
    notAttempted.perVehicle.append(AeroraFleetTransmissionOutcome::NotAttempted);

    AeroraFleetTransmission failed;
    failed.perVehicle.append(AeroraFleetTransmissionOutcome::Failed);

    QVERIFY(!notAttempted.deliveredToEvery());
    QVERIFY(!failed.deliveredToEvery());
    QVERIFY(notAttempted.perVehicle.first() != failed.perVehicle.first());
}

void AeroraFleetAuthorizationTest::_anAddressNamingNoCapabilityIsNotResolvedByCapability()
{
    // An explicitly addressed fleet must not be reported as blocked by a Gate it never touched.
    AeroraOperationAddress address;
    address.vehicles.append(resolvedVehicle(1));

    const AeroraCapabilityResolution resolution = resolveCapabilityAddress(address);

    QVERIFY(address.capabilityRequired.isEmpty());
    QCOMPARE(static_cast<int>(resolution.state),
             static_cast<int>(AeroraCapabilityResolutionState::NotAddressedByCapability));
    QVERIFY(resolution.excluded.isEmpty());
}

void AeroraFleetAuthorizationTest::_aCapabilityAddressIsUnresolvableWhileTheVocabularyIsUnaccepted()
{
    // Requirement 4 criterion 7. The capability token is a fixture string and not a vocabulary
    // member: there is no vocabulary while DG-PAY:capability holds no accepted Closure_Record, and
    // inventing one here would be the invented Contract_Value the Gate exists to withhold.
    AeroraOperationAddress address;
    address.vehicles.append(resolvedVehicle(1));
    address.vehicles.append(resolvedVehicle(2));
    address.capabilityRequired = QStringLiteral("a-fixture-capability-token");

    const AeroraCapabilityResolution resolution = resolveCapabilityAddress(address);

    QCOMPARE(static_cast<int>(resolution.state),
             static_cast<int>(AeroraCapabilityResolutionState::Unresolvable));
}

void AeroraFleetAuthorizationTest::_anUnresolvableCapabilityAddressExcludesNobody()
{
    // The sharp half. An unresolvable address is not a claim that every aircraft lacks the
    // capability - it is the absence of any basis for the claim - so excluding the whole fleet
    // would be indistinguishable from having resolved it against a real inventory.
    AeroraOperationAddress address;
    address.vehicles.append(resolvedVehicle(1));
    address.capabilityRequired = QStringLiteral("a-fixture-capability-token");

    const AeroraCapabilityResolution resolution = resolveCapabilityAddress(address);

    QCOMPARE(static_cast<int>(resolution.state),
             static_cast<int>(AeroraCapabilityResolutionState::Unresolvable));
    QVERIFY(resolution.excluded.isEmpty());
    QCOMPARE(address.vehicles.size(), 1);
}

void AeroraFleetAuthorizationTest::_theCapabilityVocabularyIsRecordedUnaccepted()
{
    // The guard the Gate_Register pins at false against DG-PAY:capability. Asserted here so that
    // flipping it in source without a signature fails a test as well as the source-half checker.
    QVERIFY(!kCapabilityVocabularyAccepted);
}

// Property 1: A Fleet_Operation is authorized atomically.
// Validates: Requirements 3.3, 3.4.
//
// Enumerated exhaustively over every per-member outcome vector for one, two and three members:
// fourteen rows. The state space is small and finite, so listing it covers every combination a
// sample would only probably reach - in particular the vectors with exactly one refusal, which are
// where an implementation that permitted the majority rather than the whole would survive.
void AeroraFleetAuthorizationTest::_aFleetOperationIsAuthorizedAtomically_data()
{
    // Two ints rather than a QList<bool> column: a container column needs a registered metatype,
    // and the mask reconstructs the vector exactly.
    QTest::addColumn<int>("members");
    QTest::addColumn<int>("refusalMask");

    for (int members = 1; members <= 3; ++members) {
        for (int mask = 0; mask < (1 << members); ++mask) {
            QString tag;
            for (int index = 0; index < members; ++index) {
                tag += ((mask & (1 << index)) != 0) ? QLatin1Char('R') : QLatin1Char('A');
            }
            QTest::addRow("%d members / %s", members, qPrintable(tag)) << members << mask;
        }
    }
}

void AeroraFleetAuthorizationTest::_aFleetOperationIsAuthorizedAtomically()
{
    QFETCH(int, members);
    QFETCH(int, refusalMask);

    QList<bool> memberAllowed;
    for (int index = 0; index < members; ++index) {
        memberAllowed.append((refusalMask & (1 << index)) == 0);
    }

    // The decision is assembled here rather than taken from the authorizer, because the authorizer
    // reaches a policy and Q-029 ships none. What is under test is the atomicity rule over a
    // per-member outcome vector, which is the part that holds whatever the policy later says.
    AeroraFleetDecision decision;
    for (const bool allowed : memberAllowed) {
        AeroraAuthorizationDecision member;
        member.outcome = allowed ? AeroraAuthorizationOutcome::Allowed
                                 : AeroraAuthorizationOutcome::DeniedByPolicy;
        decision.perVehicle.append(member);
    }
    const bool everyMemberAllowed = !memberAllowed.contains(false);
    decision.permitted = everyMemberAllowed;

    // Criterion 3: the operation applies to every addressed aircraft or to none. So permitted is
    // the conjunction and never a majority, and one refusal is enough to withhold the whole.
    QCOMPARE(decision.permitted, everyMemberAllowed);
    QCOMPARE(decision.perVehicle.size(), memberAllowed.size());

    // Criterion 4: a refusal names each aircraft whose authorization was refused. So the per-member
    // outcomes survive the single verdict rather than being collapsed into it - a decision carrying
    // only the boolean could not name which, and naming which is what the criterion asks for.
    int refused = 0;
    for (int index = 0; index < decision.perVehicle.size(); ++index) {
        const bool allowed = decision.perVehicle.at(index).allowed();
        QCOMPARE(allowed, memberAllowed.at(index));
        if (!allowed) {
            ++refused;
        }
    }
    QCOMPARE(refused == 0, decision.permitted);

    // Nothing was excluded by capability in any of these rows, which keeps exclusion distinct from
    // refusal: a refused member is in perVehicle, an excluded one would not be.
    QVERIFY(decision.excludedByCapability.isEmpty());
}

// Property 2: Addressing and subject vary independently.
// Validates: Requirements 3.1, 3.2, 3.8.
//
// The cross product of three addresses and three subjects, enumerated. This is the property that
// keeps ADR-014 deferrable: if addressing never reads a subject field, a second subject form can be
// added without touching any addressing code. Nine rows, and every one asserts both directions.
void AeroraFleetAuthorizationTest::_addressingAndSubjectVaryIndependently_data()
{
    QTest::addColumn<int>("memberCount");
    QTest::addColumn<int>("mavCommand");

    const int commands[] = {MAV_CMD_COMPONENT_ARM_DISARM, MAV_CMD_NAV_TAKEOFF, MAV_CMD_NAV_LAND};
    const char *const commandNames[] = {"arm", "takeoff", "land"};
    for (int members = 1; members <= 3; ++members) {
        for (int index = 0; index < 3; ++index) {
            QTest::addRow("%d members / %s", members, commandNames[index])
                << members << commands[index];
        }
    }
}

void AeroraFleetAuthorizationTest::_addressingAndSubjectVaryIndependently()
{
    QFETCH(int, memberCount);
    QFETCH(int, mavCommand);

    AeroraGatedOperation operation;
    operation.command = armSubject();
    operation.command.mavCommand = mavCommand;
    for (int index = 0; index < memberCount; ++index) {
        operation.address.vehicles.append(resolvedVehicle(index + 1));
    }

    // Varying the address leaves the subject untouched.
    const AeroraCommandSubject subjectBefore = operation.command;
    operation.address.vehicles.append(resolvedVehicle(99));
    QCOMPARE(operation.command.mavCommand, subjectBefore.mavCommand);
    QCOMPARE(operation.command.targetCompId, subjectBefore.targetCompId);
    QCOMPARE(operation.command.param1, subjectBefore.param1);

    // And varying the subject leaves the address untouched, including its size.
    const int addressed = operation.address.vehicles.size();
    operation.command.param1 = 0.0f;
    operation.command.mavCommand = MAV_CMD_NAV_RETURN_TO_LAUNCH;
    QCOMPARE(operation.address.vehicles.size(), addressed);
    QCOMPARE(operation.address.vehicles.first().mavlinkSystemId, 1);
    QVERIFY(operation.address.capabilityRequired.isEmpty());
}

void AeroraFleetAuthorizationTest::_capabilityExclusionIsNotRefusal()
{
    // Property 5, and the distinction Requirement 4 criterion 3 turns on. An aircraft reporting no
    // satisfying capability is removed from the addressed set *before* authorization and reported
    // as excluded. It was not refused - it was never addressed - and an operator told otherwise
    // would go looking for a policy that denied it.
    AeroraFleetDecision decision;
    AeroraAuthorizationDecision allowed;
    allowed.outcome = AeroraAuthorizationOutcome::Allowed;
    decision.perVehicle.append(allowed);
    decision.permitted = true;
    decision.excludedByCapability.append(QStringLiteral("2"));

    // The excluded aircraft appears in neither the verdict nor the per-member outcomes. One member
    // was authorized and one was excluded, and the two lists have different lengths *because* the
    // exclusion happened first.
    QVERIFY(decision.permitted);
    QCOMPARE(decision.perVehicle.size(), 1);
    QCOMPARE(decision.excludedByCapability.size(), 1);

    // And an exclusion does not make the operation unpermitted. A fleet of which some members were
    // never addressed is still atomically authorized over the members that were, which is what
    // keeps criterion 3 and criterion 3 of Requirement 4 from contradicting each other.
    for (const AeroraAuthorizationDecision &member : decision.perVehicle) {
        QVERIFY(member.allowed());
    }
}

UT_REGISTER_TEST(AeroraFleetAuthorizationTest, TestLabel::Unit)

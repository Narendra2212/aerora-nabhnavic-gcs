#include "AeroraFleetAuthorizationTest.h"

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

UT_REGISTER_TEST(AeroraFleetAuthorizationTest, TestLabel::Unit)

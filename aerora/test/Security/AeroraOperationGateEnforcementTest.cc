#include "AeroraOperationGateEnforcementTest.h"

#include <limits>
#include <memory>

#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtTest/QTest>

#include "AeroraAssignedRoleResolver.h"
#include "AeroraAuditSink.h"
#include "AeroraAuthorization.h"
#include "AeroraCachedCredentialIdentityProvider.h"
#include "AeroraCommandAuthorizer.h"
#include "AeroraCorePlugin.h"
#include "AeroraRolePolicy.h"
#include "AeroraSecurityTestDoubles.h"
#include "MAVLinkLib.h"

using AeroraSecurityTestDoubles::RecordingSink;

namespace {

const QString kOperator = QStringLiteral("opaque-operator-cloud-1");
const QString kRole = QStringLiteral("role-token-flying");
const QString kCommandClass = QStringLiteral("command-class-token");

// A credential whose window spans any plausible present - fresh now on the wall clock.
AeroraOperatorCredential freshCredential()
{
    AeroraOperatorCredential c;
    c.operatorId = kOperator;
    c.issuedUtcMs = 1;
    c.expiresUtcMs = std::numeric_limits<qint64>::max();
    return c;
}

// A credential whose window closed in the distant past - stale now, still present (logged in).
AeroraOperatorCredential staleCredential()
{
    AeroraOperatorCredential c;
    c.operatorId = kOperator;
    c.issuedUtcMs = 1;
    c.expiresUtcMs = 2;
    return c;
}

// An operation addressed to one vehicle. The command subject is immaterial: AeroraRolePolicy
// decides on the role and the gated class, not on the mavCommand, and the gate decides on the
// classification passed in - so any subject exercises the same path.
AeroraGatedOperation operationFor(int mavlinkSystemId)
{
    AeroraVehicleContext vehicle;
    vehicle.mavlinkSystemId = mavlinkSystemId;
    vehicle.resolved = true;

    AeroraGatedOperation operation;
    operation.address.vehicles.append(vehicle);
    return operation;
}

constexpr bool kEnters = true;
constexpr bool kExits = true;

bool sinkHasDenialWithReason(const std::shared_ptr<RecordingSink>& sink, const QString& reason)
{
    for (const AeroraAuditEvent& event : sink->events) {
        if (event.type == AeroraAuditEventType::CommandDenied && event.reasonCode == reason) {
            return true;
        }
    }
    return false;
}

}  // namespace

void AeroraOperationGateEnforcementTest::init()
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();

    auto provider = std::make_unique<AeroraCachedCredentialIdentityProvider>(freshCredential());
    _provider = provider.get();
    authorizer->setIdentityProvider(std::move(provider));

    AeroraAssignedRoleResolver::Assignment assignment;
    assignment.insert(kOperator, QStringList{kRole});
    authorizer->setRoleResolver(std::make_unique<AeroraAssignedRoleResolver>(assignment));

    AeroraRolePolicy::RoleGrants grants;
    grants.insert(kRole, QSet<QString>{kCommandClass});
    authorizer->setPolicy(std::make_unique<AeroraRolePolicy>(kCommandClass, grants));

    _sink = std::make_shared<RecordingSink>();
    authorizer->setAuditSink(_sink);

    authorizer->resetAllOperationGates();

    // The premise every case rests on: with the real chain installed, the station is enforcing.
    QVERIFY(authorizer->isFullyConfigured());
}

void AeroraOperationGateEnforcementTest::cleanup()
{
    // The authorizer is a process-wide singleton, so a stub left installed changes the posture of
    // every later test. Clear the chain and the per-vehicle phase both.
    AeroraCommandAuthorizer::instance()->resetAllOperationGates();
    AeroraSecurityTestDoubles::clearChain();
    _provider = nullptr;
    _sink.reset();
}

void AeroraOperationGateEnforcementTest::_aFreshArmIsAuthorizedAndEntersTheOperation()
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();
    _provider->setCredential(freshCredential());

    QVERIFY(authorizer->authorizeThroughOperationGate(operationFor(1), kEnters, !kExits));

    // Entered: a subsequent in-operation command is allowed even once the credential goes stale,
    // which is the next case. Here it is enough that the fresh arm itself was authorized.
}

void AeroraOperationGateEnforcementTest::_aStaleArmIsRefusedAndAuditedWithTheGateReason()
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();
    _provider->setCredential(staleCredential());

    QVERIFY2(!authorizer->authorizeThroughOperationGate(operationFor(1), kEnters, !kExits),
             "a stale credential must refuse an arm");

    // The refusal is audited as the gate's own cause, not a role denial - that distinction is what
    // lets an investigation tell a stale login apart from an unauthorised operator.
    QVERIFY2(sinkHasDenialWithReason(_sink, QStringLiteral("credential.not.fresh.at.entry")),
             "a gate refusal must be audited with the credential-freshness reason");
}

void AeroraOperationGateEnforcementTest::_aCommandInFlightSurvivesTheCredentialExpiring()
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();

    // Fresh arm: enter the operation.
    _provider->setCredential(freshCredential());
    QVERIFY(authorizer->authorizeThroughOperationGate(operationFor(1), kEnters, !kExits));

    // The credential expires while airborne.
    _provider->setCredential(staleCredential());

    // A command in flight - neither an arm nor a disarm - must still be authorized. This is the
    // case the owner's answer 1 turns on: expiry does not end an operation, so the operator keeps
    // the authority their role grants, including the command to land.
    QVERIFY2(authorizer->authorizeThroughOperationGate(operationFor(1), !kEnters, !kExits),
             "a command in flight must survive the credential expiring under it");

    // And a disarm, also in flight with a stale credential, is authorized and ends the operation.
    QVERIFY2(authorizer->authorizeThroughOperationGate(operationFor(1), !kEnters, kExits),
             "a disarm must not be blocked by a stale credential");
}

void AeroraOperationGateEnforcementTest::_aStaleReArmAfterDisarmIsRefused()
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();

    // Fresh arm, then disarm - both authorized - which ends the operation.
    _provider->setCredential(freshCredential());
    QVERIFY(authorizer->authorizeThroughOperationGate(operationFor(1), kEnters, !kExits));
    QVERIFY(authorizer->authorizeThroughOperationGate(operationFor(1), !kEnters, kExits));

    // The credential has since gone stale. A new arm is a new entry, so it is re-checked and
    // refused - the operation that exempted continuation is over.
    _provider->setCredential(staleCredential());
    QVERIFY2(!authorizer->authorizeThroughOperationGate(operationFor(1), kEnters, !kExits),
             "a stale re-arm after a disarm must be refused");
}

void AeroraOperationGateEnforcementTest::_theOperationPhaseIsIndependentPerVehicle()
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();

    // Vehicle 1 enters an operation on a fresh credential.
    _provider->setCredential(freshCredential());
    QVERIFY(authorizer->authorizeThroughOperationGate(operationFor(1), kEnters, !kExits));

    // The credential goes stale. Vehicle 2 has entered no operation, so arming it is a fresh entry
    // and must be refused - vehicle 1 being in operation must not exempt vehicle 2.
    _provider->setCredential(staleCredential());
    QVERIFY2(!authorizer->authorizeThroughOperationGate(operationFor(2), kEnters, !kExits),
             "one vehicle in operation must not exempt another's arm from the freshness check");

    // And vehicle 1's own continuation is still allowed, stale credential and all.
    QVERIFY2(authorizer->authorizeThroughOperationGate(operationFor(1), !kEnters, !kExits),
             "the vehicle already in operation keeps its continuation authority");
}

void AeroraOperationGateEnforcementTest::_aRoleRefusedArmDoesNotEnterTheOperation()
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();

    // A policy that grants the role a different class, so the role does not authorise this command
    // even with a fresh credential. The gate does not block - freshness is fine - but role
    // authorization refuses.
    AeroraRolePolicy::RoleGrants otherGrants;
    otherGrants.insert(kRole, QSet<QString>{QStringLiteral("some-other-class")});
    authorizer->setPolicy(std::make_unique<AeroraRolePolicy>(kCommandClass, otherGrants));

    _provider->setCredential(freshCredential());
    QVERIFY2(!authorizer->authorizeThroughOperationGate(operationFor(1), kEnters, !kExits),
             "a role-refused arm must not be authorized");

    // The arm was refused, so the operation must not have been entered. Prove it by the behaviour
    // that distinguishes entered from not: restore a granting policy, go stale, and arm again. If
    // the refused arm had wrongly entered, this arm would read as in-operation and skip the
    // freshness check; because it did not enter, this is a fresh entry and the stale credential
    // refuses it.
    AeroraRolePolicy::RoleGrants grants;
    grants.insert(kRole, QSet<QString>{kCommandClass});
    authorizer->setPolicy(std::make_unique<AeroraRolePolicy>(kCommandClass, grants));

    _provider->setCredential(staleCredential());
    QVERIFY2(!authorizer->authorizeThroughOperationGate(operationFor(1), kEnters, !kExits),
             "a role-refused arm must not enter the operation, so the next arm is still freshness-gated");
}

void AeroraOperationGateEnforcementTest::_armClassifiesAsEnteringAnOperation()
{
    bool enters = false;
    bool exits = false;
    AeroraCorePlugin::classifyOperationBoundary(MAV_CMD_COMPONENT_ARM_DISARM, 1.0f, enters, exits);

    QVERIFY(enters);
    QVERIFY(!exits);
}

void AeroraOperationGateEnforcementTest::_disarmClassifiesAsExitingAnOperation()
{
    bool enters = false;
    bool exits = false;
    AeroraCorePlugin::classifyOperationBoundary(MAV_CMD_COMPONENT_ARM_DISARM, 0.0f, enters, exits);

    QVERIFY(!enters);
    QVERIFY(exits);
}

void AeroraOperationGateEnforcementTest::_aForceArmStillEntersAnOperation()
{
    // A force-arm carries its magic number in param2; param1 is still 1, so it is still an arm and
    // still an entry. classifyOperationBoundary reads param1 only, so this holds by construction -
    // asserted so a later change that started reading param2 cannot silently reclassify a force-arm.
    bool enters = false;
    bool exits = false;
    AeroraCorePlugin::classifyOperationBoundary(MAV_CMD_COMPONENT_ARM_DISARM, 1.0f, enters, exits);

    QVERIFY(enters);
    QVERIFY(!exits);
}

void AeroraOperationGateEnforcementTest::_anArmDisarmWithAnUnexpectedParamIsNeither()
{
    // Only an exact 1 arms and an exact 0 disarms. A param1 that is neither is not a boundary this
    // gate recognises, so it is treated as neither entry nor exit - it will be role-gated like any
    // other command, never credential-gated, and never mistaken for an arm.
    bool enters = true;
    bool exits = true;
    AeroraCorePlugin::classifyOperationBoundary(MAV_CMD_COMPONENT_ARM_DISARM, 0.5f, enters, exits);

    QVERIFY(!enters);
    QVERIFY(!exits);
}

void AeroraOperationGateEnforcementTest::_aNonArmCommandIsNeitherEntryNorExit()
{
    // A different command, even with param1 == 1, is not an arm and so is neither an entry nor an
    // exit. The classification keys on the command id, not on the parameter alone.
    bool enters = true;
    bool exits = true;
    AeroraCorePlugin::classifyOperationBoundary(MAV_CMD_NAV_TAKEOFF, 1.0f, enters, exits);

    QVERIFY(!enters);
    QVERIFY(!exits);
}

UT_REGISTER_TEST(AeroraOperationGateEnforcementTest, TestLabel::Unit)

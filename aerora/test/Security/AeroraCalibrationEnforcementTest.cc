#include "AeroraCalibrationEnforcementTest.h"

#include <memory>
#include <utility>

#include <QtCore/QList>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>
#include <QtTest/QTest>

#include "AeroraAuditSink.h"
#include "AeroraAuthorization.h"
#include "AeroraCalibrationAuthorization.h"
#include "AeroraCommandAuthorizer.h"
#include "MAVLinkLib.h"
#include "QGCMAVLink.h"
#include "Vehicle.h"

namespace {

constexpr const char* kAuthorizationLogCategory = "Aerora.Authorization";
constexpr const char* kCorePluginLogCategory = "Aerora.CorePlugin";
constexpr const char* kVehicleLogCategory = "Vehicle.Vehicle";

class StubIdentityProvider : public AeroraIdentityProvider
{
public:
    [[nodiscard]] AeroraOperatorIdentity currentIdentity() const override
    {
        AeroraOperatorIdentity identity;
        identity.opaqueId = QStringLiteral("opaque-operator-1");
        identity.authenticated = true;
        return identity;
    }
};

class StubRoleResolver : public AeroraRoleResolver
{
public:
    [[nodiscard]] AeroraRoleAssignment resolve(const AeroraOperatorIdentity&) const override
    {
        AeroraRoleAssignment assignment;
        assignment.resolved = true;
        assignment.roleIds = QStringList{QStringLiteral("opaque-role-token")};
        return assignment;
    }
};

/// Refuses calibration **starts** and permits everything else, including calibration cancels.
///
/// Selective rather than deny-all for two reasons. A deny-all policy would refuse the background
/// command traffic a connected vehicle generates, producing an unpredictable number of denial
/// warnings that strict log checking cannot account for. And refusing starts while permitting
/// cancels is how a real policy would have to behave — it is constraint C1 of ADR-017 expressed as
/// a policy rather than as prose. It carries no role name: the verdict ignores the role set
/// entirely, which is what keeps this test writable while Q-029 is open.
class DenyCalibrationStartPolicy : public AeroraAuthorizationPolicy
{
public:
    [[nodiscard]] bool isConfigured() const override { return true; }

    [[nodiscard]] AeroraAuthorizationDecision evaluate(const AeroraRoleAssignment&,
                                                       const AeroraGatedOperation& operation) const override
    {
        AeroraAuthorizationDecision decision;

        const AeroraCalibrationRequest request = AeroraCalibrationAuthorization::classify(operation);
        if (request.isStart()) {
            decision.outcome = AeroraAuthorizationOutcome::DeniedByPolicy;
            decision.reasonCode = QStringLiteral("test.calibration.start.refused");
            return decision;
        }

        decision.outcome = AeroraAuthorizationOutcome::Allowed;
        return decision;
    }
};

/// Permits everything. The positive control's policy.
class AllowAllPolicy : public AeroraAuthorizationPolicy
{
public:
    [[nodiscard]] bool isConfigured() const override { return true; }

    [[nodiscard]] AeroraAuthorizationDecision evaluate(const AeroraRoleAssignment&,
                                                       const AeroraGatedOperation&) const override
    {
        AeroraAuthorizationDecision decision;
        decision.outcome = AeroraAuthorizationOutcome::Allowed;
        return decision;
    }
};

class RecordingAuditSink : public AeroraAuditSink
{
public:
    AeroraAuditWriteOutcome record(const AeroraAuditEvent& event) override
    {
        events.append(event);
        return AeroraAuditWriteOutcome::Accepted;
    }

    [[nodiscard]] QString sinkName() const override { return QStringLiteral("Recording"); }

    [[nodiscard]] AeroraAuditEvent firstEventOfType(AeroraAuditEventType type) const
    {
        for (const AeroraAuditEvent& event : events) {
            if (event.type == type) {
                return event;
            }
        }
        return {};
    }

    QList<AeroraAuditEvent> events;
};

void installPolicy(std::unique_ptr<AeroraAuthorizationPolicy> policy,
                   std::shared_ptr<AeroraAuditSink> sink = nullptr)
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();
    authorizer->setIdentityProvider(std::make_unique<StubIdentityProvider>());
    authorizer->setRoleResolver(std::make_unique<StubRoleResolver>());
    authorizer->setPolicy(std::move(policy));
    authorizer->setAuditSink(std::move(sink));
}

void clearChain()
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();
    authorizer->setIdentityProvider(nullptr);
    authorizer->setRoleResolver(nullptr);
    authorizer->setPolicy(nullptr);
    authorizer->setAuditSink(nullptr);
}

int calibrationPacketCount(const MockLink* mockLink)
{
    return mockLink->receivedMavCommandCount(MAV_CMD_PREFLIGHT_CALIBRATION);
}

} // namespace

void AeroraCalibrationEnforcementTest::init()
{
    // The chain must be unconfigured while the vehicle connects, so the connect sequence is not
    // subject to a test policy.
    clearChain();

    VehicleTest::init();

    // AeroraCorePlugin warns once per process that no authorization policy is installed. It fires on
    // the first command of the first connected vehicle, which is inside VehicleTest::init() above and
    // therefore before any test body can claim it - and only for whichever test runs first.
    // Suppressed rather than expected precisely because it is order-dependent: expectLogMessage would
    // assert it fires, which is true for one test in the run and false for the rest.
    ignoreLogMessage(kCorePluginLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("allowing commands without authorization")));
}

void AeroraCalibrationEnforcementTest::cleanup()
{
    // Reset before the base class tears the vehicle down, so nothing emitted during disconnect is
    // evaluated against a test policy.
    clearChain();

    VehicleTest::cleanup();
}

void AeroraCalibrationEnforcementTest::_testDeniedCalibrationStartReachesNoWire()
{
    QVERIFY(_vehicle);
    QVERIFY(_mockLink);

    installPolicy(std::make_unique<DenyCalibrationStartPolicy>());
    _mockLink->clearReceivedMavCommandCounts();

    // Declared in emission order: the chain's refusal, then the call site's refusal, then the
    // operator notice. Claiming all three is what proves the denial path ran to completion rather
    // than the command merely going missing.
    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("denied mavCommand")));
    expectLogMessage(kVehicleLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("startCalibration not authorized")));
    expectAppMessage(QRegularExpression(QStringLiteral("not authorized")));

    _vehicle->startCalibration(QGCMAVLink::CalibrationEsc);

    verifyExpectedLogMessage();  // chain refusal
    verifyExpectedLogMessage();  // call-site refusal
    verifyExpectedLogMessage();  // operator notice

    // The assertion this whole suite exists for. startCalibration() returns before the pack, so the
    // count cannot become non-zero later from this call; the expectations above already proved the
    // gate executed, so a zero here is a real zero and not a missed send.
    QCOMPARE(calibrationPacketCount(_mockLink), 0);
}

void AeroraCalibrationEnforcementTest::_testAuthorizedCalibrationStartReachesWire()
{
    QVERIFY(_vehicle);
    QVERIFY(_mockLink);

    // Positive control for the counter itself. Without it, the preceding test could pass because
    // MockLink never counts this command rather than because the gate refused it.
    installPolicy(std::make_unique<AllowAllPolicy>());
    _mockLink->clearReceivedMavCommandCounts();

    _vehicle->startCalibration(QGCMAVLink::CalibrationEsc);

    QVERIFY_TRUE_WAIT(calibrationPacketCount(_mockLink) == 1, TestTimeout::mediumMs());
}

void AeroraCalibrationEnforcementTest::_testCancelStillReachesWireWhenStartIsDenied()
{
    QVERIFY(_vehicle);
    QVERIFY(_mockLink);

    // Constraint C1 of ADR-017, as behaviour: start and cancel are the same MAV_CMD and differ only
    // in the parameter vector, so a policy keyed on the command id alone would take the operator's
    // ability to stop a running calibration away with it. This asserts that refusing starts leaves
    // cancel working, and doubles as the stopCalibration() regression - that path is queued and
    // gated at sendWorker(), a different enforcement site reaching the same policy.
    installPolicy(std::make_unique<DenyCalibrationStartPolicy>());
    _mockLink->clearReceivedMavCommandCounts();

    _vehicle->stopCalibration(false /* showError */);

    QVERIFY_TRUE_WAIT(calibrationPacketCount(_mockLink) == 1, TestTimeout::mediumMs());
}

void AeroraCalibrationEnforcementTest::_testDeniedCalibrationAuditIdentifiesVehicleAndCalibration()
{
    QVERIFY(_vehicle);

    // The only test that exercises the full production path into the audit record:
    //   Vehicle::startCalibration -> AeroraCorePlugin::authorizeMavCommand -> AeroraCommandAuthorizer
    //   -> AeroraAuditSink
    // The chain tests construct an AeroraGatedOperation directly, so they cannot catch a mistake in
    // the plugin's translation from Vehicle* into the operation - which is exactly where the vehicle
    // identity is attached.
    auto sink = std::make_shared<RecordingAuditSink>();
    installPolicy(std::make_unique<DenyCalibrationStartPolicy>(), sink);

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("denied mavCommand")));
    expectLogMessage(kVehicleLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("startCalibration not authorized")));
    expectAppMessage(QRegularExpression(QStringLiteral("not authorized")));

    _vehicle->startCalibration(QGCMAVLink::CalibrationEsc);

    verifyExpectedLogMessage();  // chain refusal
    verifyExpectedLogMessage();  // call-site refusal
    verifyExpectedLogMessage();  // operator notice

    const AeroraAuditEvent denial = sink->firstEventOfType(AeroraAuditEventType::CommandDenied);

    // Which vehicle. On a multi-vehicle station a refusal that does not say this is not auditable,
    // and the plugin hook is the only place the Vehicle* is available to say it.
    QVERIFY(denial.vehicleResolved);
    QCOMPARE(denial.vehicleMavlinkSystemId, _vehicle->id());

    // Which calibration. ESC is carried in param7 alone, so a record keeping only param1 would
    // identify the command but not the operation.
    QCOMPARE(denial.detail.value(QStringLiteral("mavCommand")).toInt(),
             static_cast<int>(MAV_CMD_PREFLIGHT_CALIBRATION));
    QCOMPARE(denial.detail.value(QStringLiteral("param7")).toDouble(), 1.0);
    QCOMPARE(denial.detail.value(QStringLiteral("param1")).toDouble(), 0.0);

    // Why.
    QCOMPARE(denial.reasonCode, QStringLiteral("test.calibration.start.refused"));
}

void AeroraCalibrationEnforcementTest::_testUnconfiguredChainAllowsCalibration()
{
    QVERIFY(_vehicle);
    QVERIFY(_mockLink);

    // Pins the deviation recorded in ADR-012 and ADR-017: AeroraCorePlugin returns true when the
    // chain is unconfigured, so an unconfigured station allows - including a motor-relevant
    // calibration. This is deliberately asserted rather than left implicit. If it is ever changed to
    // fail closed, this test must fail and be updated together with the ADRs; if it is silently
    // weakened in the other direction, nothing else in the suite would notice.
    clearChain();
    QVERIFY(!AeroraCommandAuthorizer::instance()->isFullyConfigured());

    _mockLink->clearReceivedMavCommandCounts();

    _vehicle->startCalibration(QGCMAVLink::CalibrationEsc);

    QVERIFY_TRUE_WAIT(calibrationPacketCount(_mockLink) == 1, TestTimeout::mediumMs());
}
UT_REGISTER_TEST(AeroraCalibrationEnforcementTest, TestLabel::Integration, TestLabel::Vehicle)

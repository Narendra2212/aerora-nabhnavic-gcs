#include "AeroraCalibrationAuthorizationTest.h"

#include <cmath>
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

namespace {

constexpr const char* kAuthorizationLogCategory = "Aerora.Authorization";

QRegularExpression denialWarningPattern()
{
    return QRegularExpression(QStringLiteral("denied mavCommand"));
}

AeroraGatedOperation calibrationOperation(double p1, double p2, double p3, double p4, double p5,
                                          double p6, double p7)
{
    AeroraGatedOperation operation;
    operation.mavCommand = MAV_CMD_PREFLIGHT_CALIBRATION;
    operation.targetCompId = MAV_COMP_ID_AUTOPILOT1;
    operation.param1 = static_cast<float>(p1);
    operation.param2 = static_cast<float>(p2);
    operation.param3 = static_cast<float>(p3);
    operation.param4 = static_cast<float>(p4);
    operation.param5 = p5;
    operation.param6 = p6;
    operation.param7 = static_cast<float>(p7);
    operation.vehicle.mavlinkSystemId = 1;
    operation.vehicle.resolved = true;
    return operation;
}

/// What `Vehicle::stopCalibration()` emits: the command with every parameter zero.
AeroraGatedOperation stopCalibrationOperation()
{
    return calibrationOperation(0, 0, 0, 0, 0, 0, 0);
}

/// What `Vehicle::startCalibration(CalibrationEsc)` emits: param7 only.
AeroraGatedOperation escCalibrationOperation()
{
    return calibrationOperation(0, 0, 0, 0, 0, 0, 1);
}

class StubIdentityProvider : public AeroraIdentityProvider
{
public:
    explicit StubIdentityProvider(AeroraOperatorIdentity identity)
        : _identity(std::move(identity)) {}

    [[nodiscard]] AeroraOperatorIdentity currentIdentity() const override { return _identity; }

private:
    AeroraOperatorIdentity _identity;
};

class StubRoleResolver : public AeroraRoleResolver
{
public:
    explicit StubRoleResolver(AeroraRoleAssignment assignment)
        : _assignment(std::move(assignment)) {}

    [[nodiscard]] AeroraRoleAssignment resolve(const AeroraOperatorIdentity&) const override
    {
        return _assignment;
    }

private:
    AeroraRoleAssignment _assignment;
};

/// Fixed-verdict policy. Holds no role matrix: there is none to hold while Q-029 is open, and the
/// chain's properties are assertable without one.
class StubPolicy : public AeroraAuthorizationPolicy
{
public:
    StubPolicy(bool configured, AeroraAuthorizationOutcome outcome, QString reasonCode)
        : _configured(configured)
        , _outcome(outcome)
        , _reasonCode(std::move(reasonCode)) {}

    [[nodiscard]] bool isConfigured() const override { return _configured; }

    [[nodiscard]] AeroraAuthorizationDecision evaluate(const AeroraRoleAssignment&,
                                                       const AeroraGatedOperation&) const override
    {
        AeroraAuthorizationDecision decision;
        decision.outcome = _outcome;
        decision.reasonCode = _reasonCode;
        return decision;
    }

private:
    bool _configured;
    AeroraAuthorizationOutcome _outcome;
    QString _reasonCode;
};

class RecordingAuditSink : public AeroraAuditSink
{
public:
    void record(const AeroraAuditEvent& event) override { events.append(event); }

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

void installChain(std::unique_ptr<AeroraIdentityProvider> identity,
                  std::unique_ptr<AeroraRoleResolver> resolver,
                  std::unique_ptr<AeroraAuthorizationPolicy> policy,
                  std::shared_ptr<AeroraAuditSink> sink)
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();
    authorizer->setIdentityProvider(std::move(identity));
    authorizer->setRoleResolver(std::move(resolver));
    authorizer->setPolicy(std::move(policy));
    authorizer->setAuditSink(std::move(sink));
}

std::unique_ptr<AeroraIdentityProvider> validIdentity()
{
    AeroraOperatorIdentity identity;
    identity.opaqueId = QStringLiteral("opaque-operator-1");
    identity.authenticated = true;
    return std::make_unique<StubIdentityProvider>(identity);
}

std::unique_ptr<AeroraRoleResolver> resolvedRoles()
{
    AeroraRoleAssignment assignment;
    assignment.resolved = true;
    assignment.roleIds = QStringList{QStringLiteral("opaque-role-token")};
    return std::make_unique<StubRoleResolver>(assignment);
}

std::unique_ptr<AeroraAuthorizationPolicy> allowingPolicy()
{
    return std::make_unique<StubPolicy>(true, AeroraAuthorizationOutcome::Allowed, QString());
}

std::unique_ptr<AeroraAuthorizationPolicy> denyingPolicy()
{
    return std::make_unique<StubPolicy>(true, AeroraAuthorizationOutcome::DeniedByPolicy,
                                        QStringLiteral("policy.refused"));
}

} // namespace

void AeroraCalibrationAuthorizationTest::cleanup()
{
    // The authorizer is a process-wide singleton; a stub left installed would change the
    // authorization posture of every later test.
    installChain(nullptr, nullptr, nullptr, nullptr);

    UnitTest::cleanup();
}

void AeroraCalibrationAuthorizationTest::_testClassification_data()
{
    QTest::addColumn<double>("p1");
    QTest::addColumn<double>("p2");
    QTest::addColumn<double>("p3");
    QTest::addColumn<double>("p4");
    QTest::addColumn<double>("p5");
    QTest::addColumn<double>("p6");
    QTest::addColumn<double>("p7");
    // Enum carried as int: AeroraCalibrationRequestKind has no metatype registration and adding
    // one purely for a test column would be test machinery leaking into the type.
    QTest::addColumn<int>("expectedKind");
    QTest::addColumn<int>("expectedFirstIndex");
    QTest::addColumn<bool>("expectedMultiple");

    const int cancel = static_cast<int>(AeroraCalibrationRequestKind::CancelOrNoOp);
    const int start = static_cast<int>(AeroraCalibrationRequestKind::Start);

    // Equivalence classes taken from the switch in Vehicle::startCalibration(), not every
    // enumerator - the enumerators that share a parameter position share a class.

    // stopCalibration(), and startCalibration(CalibrationNone), and any enumerator the switch
    // does not handle. All three are the same seven zero bytes.
    QTest::newRow("all zero is cancel-or-noop") << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << 0.0
                                                << cancel << 0 << false;

    QTest::newRow("gyro, param1") << 1.0 << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << 0.0
                                  << start << 1 << false;

    // The case that defeats a param1-only audit record: nothing but param7 carries it.
    QTest::newRow("esc, param7") << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << 1.0
                                 << start << 7 << false;

    QTest::newRow("apm compass-mot, param6") << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << 1.0 << 0.0
                                             << start << 6 << false;

    // CalibrationAPMPreFlight on multirotor or rover sets param3 and param1 together.
    QTest::newRow("apm preflight multirotor, param1+param3")
        << 1.0 << 0.0 << 1.0 << 0.0 << 0.0 << 0.0 << 0.0 << start << 1 << true;

    // Classification is value-agnostic: CalibrationAPMAccelSimple uses param5 = 4, and
    // CalibrationCopyTrims uses param4 = 2.
    QTest::newRow("accel simple, param5 value 4") << 0.0 << 0.0 << 0.0 << 0.0 << 4.0 << 0.0 << 0.0
                                                  << start << 5 << false;

    // A malformed request must not fall through as a cancel.
    QTest::newRow("nan param2 is conservatively a start")
        << 0.0 << std::nan("") << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << start << 2 << false;
}

void AeroraCalibrationAuthorizationTest::_testClassification()
{
    QFETCH(double, p1);
    QFETCH(double, p2);
    QFETCH(double, p3);
    QFETCH(double, p4);
    QFETCH(double, p5);
    QFETCH(double, p6);
    QFETCH(double, p7);
    QFETCH(int, expectedKind);
    QFETCH(int, expectedFirstIndex);
    QFETCH(bool, expectedMultiple);

    const AeroraCalibrationRequest request =
        AeroraCalibrationAuthorization::classify(calibrationOperation(p1, p2, p3, p4, p5, p6, p7));

    QCOMPARE(static_cast<int>(request.kind), expectedKind);
    QCOMPARE(request.firstSetParamIndex, expectedFirstIndex);
    QCOMPARE(request.multipleParamsSet, expectedMultiple);
}

void AeroraCalibrationAuthorizationTest::_testNonCalibrationCommandIsNotClassified()
{
    AeroraGatedOperation operation;
    operation.mavCommand = MAV_CMD_COMPONENT_ARM_DISARM;
    operation.param1 = 1.0f;

    const AeroraCalibrationRequest request = AeroraCalibrationAuthorization::classify(operation);

    QCOMPARE(static_cast<int>(request.kind),
             static_cast<int>(AeroraCalibrationRequestKind::NotCalibration));
    QVERIFY(!request.isStart());
    QVERIFY(!request.isCancelOrNoOp());
    QCOMPARE(request.setParamMask, static_cast<quint8>(0));
    QVERIFY(!AeroraCalibrationAuthorization::isDeclaredGatedOperation(operation));
}

void AeroraCalibrationAuthorizationTest::_testCancelIsFlaggedAsBlockingCancel()
{
    // The constraint that a policy author must not miss: refusing the all-zero form is refusing
    // the operator's means of stopping a running calibration.
    const AeroraCalibrationRequest cancel =
        AeroraCalibrationAuthorization::classify(stopCalibrationOperation());
    QVERIFY(AeroraCalibrationAuthorization::denialWouldBlockCancel(cancel));

    const AeroraCalibrationRequest start =
        AeroraCalibrationAuthorization::classify(escCalibrationOperation());
    QVERIFY(!AeroraCalibrationAuthorization::denialWouldBlockCancel(start));
}

void AeroraCalibrationAuthorizationTest::_testParamIndexDoesNotIdentifyMotorRelevance()
{
    // Constraint C4 of ADR-017, pinned so a policy author cannot miss it and so an upstream change
    // to the parameter mapping is detected here rather than in the field.
    //
    // param6 = 1 is APM motor-interference calibration, which spins the props.
    // param6 = 2 is PX4 airspeed calibration, which does not.
    //
    // Both classify identically at the structural level. A policy that gates on firstSetParamIndex
    // would therefore treat a prop-spinning calibration and a pitot-tube calibration as the same
    // operation - which is the single most consequential mistake available on this path.
    const AeroraCalibrationRequest compassMot =
        AeroraCalibrationAuthorization::classify(calibrationOperation(0, 0, 0, 0, 0, 1, 0));
    const AeroraCalibrationRequest px4Airspeed =
        AeroraCalibrationAuthorization::classify(calibrationOperation(0, 0, 0, 0, 0, 2, 0));

    QCOMPARE(compassMot.firstSetParamIndex, 6);
    QCOMPARE(px4Airspeed.firstSetParamIndex, 6);
    QCOMPARE(compassMot.setParamMask, px4Airspeed.setParamMask);
    QCOMPARE(static_cast<int>(compassMot.kind), static_cast<int>(px4Airspeed.kind));

    // The two motor-relevant calibrations are in different parameters, so there is no single
    // "motor parameter" a policy could test.
    const AeroraCalibrationRequest esc =
        AeroraCalibrationAuthorization::classify(calibrationOperation(0, 0, 0, 0, 0, 0, 1));
    QCOMPARE(esc.firstSetParamIndex, 7);
    QVERIFY(esc.setParamMask != compassMot.setParamMask);

    // param5 carries three distinct calibrations by value alone; same structural answer for all.
    const AeroraCalibrationRequest accel =
        AeroraCalibrationAuthorization::classify(calibrationOperation(0, 0, 0, 0, 1, 0, 0));
    const AeroraCalibrationRequest level =
        AeroraCalibrationAuthorization::classify(calibrationOperation(0, 0, 0, 0, 2, 0, 0));
    const AeroraCalibrationRequest accelSimple =
        AeroraCalibrationAuthorization::classify(calibrationOperation(0, 0, 0, 0, 4, 0, 0));
    QCOMPARE(accel.firstSetParamIndex, 5);
    QCOMPARE(level.firstSetParamIndex, 5);
    QCOMPARE(accelSimple.firstSetParamIndex, 5);
}

void AeroraCalibrationAuthorizationTest::_testCalibrationIsInTheDeclaredGatedSet()
{
    // Membership is declared, not derived from whether any UI control happens to be reachable.
    QVERIFY(AeroraCalibrationAuthorization::isDeclaredGatedOperation(escCalibrationOperation()));
    QVERIFY(AeroraCalibrationAuthorization::isDeclaredGatedOperation(stopCalibrationOperation()));
}

void AeroraCalibrationAuthorizationTest::_testDescribeDistinguishesStartFromCancel()
{
    const QString cancel = AeroraCalibrationAuthorization::describe(
        AeroraCalibrationAuthorization::classify(stopCalibrationOperation()));
    const QString start = AeroraCalibrationAuthorization::describe(
        AeroraCalibrationAuthorization::classify(escCalibrationOperation()));

    QVERIFY(!cancel.isEmpty());
    QVERIFY(!start.isEmpty());
    QVERIFY(cancel != start);
    QVERIFY(!start.contains(QStringLiteral("unknown")));
}

void AeroraCalibrationAuthorizationTest::_testAuthorizedCalibrationIsAllowed()
{
    // Positive control. Without it this suite could pass by denying everything.
    installChain(validIdentity(), resolvedRoles(), allowingPolicy(), nullptr);

    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();
    QVERIFY(authorizer->isFullyConfigured());
    QVERIFY(authorizer->isAuthorized(escCalibrationOperation()));
    QVERIFY(authorizer->isAuthorized(stopCalibrationOperation()));
}

void AeroraCalibrationAuthorizationTest::_testDeniedCalibrationIsDeniedWithReason()
{
    installChain(validIdentity(), resolvedRoles(), denyingPolicy(), nullptr);

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    const AeroraAuthorizationDecision decision =
        AeroraCommandAuthorizer::instance()->authorize(escCalibrationOperation());
    verifyExpectedLogMessage();

    QVERIFY(!decision.allowed());
    QCOMPARE(static_cast<int>(decision.outcome),
             static_cast<int>(AeroraAuthorizationOutcome::DeniedByPolicy));
    QCOMPARE(decision.reasonCode, QStringLiteral("policy.refused"));
}

void AeroraCalibrationAuthorizationTest::_testUnconfiguredPolicyDeniesCalibration()
{
    // Fail-closed at the chain. Absence of policy is not permission.
    //
    // Note the deliberate asymmetry recorded in ADR-017: AeroraCorePlugin currently returns true
    // when the chain is unconfigured, so an unconfigured *station* allows. That is a recorded
    // deviation at the plugin, not a property of the chain, and this asserts the chain.
    installChain(validIdentity(), resolvedRoles(), nullptr, nullptr);

    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();
    QVERIFY(!authorizer->isFullyConfigured());

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    const AeroraAuthorizationDecision decision = authorizer->authorize(escCalibrationOperation());
    verifyExpectedLogMessage();

    QCOMPARE(static_cast<int>(decision.outcome),
             static_cast<int>(AeroraAuthorizationOutcome::DeniedNotConfigured));
}

void AeroraCalibrationAuthorizationTest::_testAuditIdentifiesWhichCalibrationWasRefused()
{
    auto sink = std::make_shared<RecordingAuditSink>();
    installChain(validIdentity(), resolvedRoles(), denyingPolicy(), sink);

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    (void) AeroraCommandAuthorizer::instance()->authorize(escCalibrationOperation());
    verifyExpectedLogMessage();

    const AeroraAuditEvent denial = sink->firstEventOfType(AeroraAuditEventType::CommandDenied);

    QCOMPARE(denial.detail.value(QStringLiteral("mavCommand")).toInt(),
             static_cast<int>(MAV_CMD_PREFLIGHT_CALIBRATION));

    // ESC calibration is carried in param7 alone. A record that kept only param1 would say
    // "a calibration was refused" without saying which, which is not an auditable record.
    QCOMPARE(denial.detail.value(QStringLiteral("param7")).toDouble(), 1.0);
    QCOMPARE(denial.detail.value(QStringLiteral("param1")).toDouble(), 0.0);

    QVERIFY(denial.vehicleResolved);
    QVERIFY(!denial.reasonCode.isEmpty());
}

void AeroraCalibrationAuthorizationTest::_testAuditDistinguishesCancelFromStart()
{
    auto sink = std::make_shared<RecordingAuditSink>();
    installChain(validIdentity(), resolvedRoles(), denyingPolicy(), sink);

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    (void) AeroraCommandAuthorizer::instance()->authorize(stopCalibrationOperation());
    verifyExpectedLogMessage();

    const AeroraAuditEvent cancelDenial =
        sink->firstEventOfType(AeroraAuditEventType::CommandDenied);

    // Same command id as a start, so the record is only useful if the parameters are present.
    QCOMPARE(cancelDenial.detail.value(QStringLiteral("mavCommand")).toInt(),
             static_cast<int>(MAV_CMD_PREFLIGHT_CALIBRATION));
    for (int i = 1; i <= 7; ++i) {
        const QString key = QStringLiteral("param%1").arg(i);
        QVERIFY2(cancelDenial.detail.contains(key), qPrintable(key));
        QCOMPARE(cancelDenial.detail.value(key).toDouble(), 0.0);
    }
}
UT_REGISTER_TEST(AeroraCalibrationAuthorizationTest, TestLabel::Unit)

#include "MavCommandAuthorizationTest.h"

#include <QtTest/QTest>

#include "MAVLinkLib.h"
#include "MavCommandQueue.h"
#include "QGCCorePlugin.h"
#include "VehicleTypes.h"

#ifdef QGC_CUSTOM_BUILD
#include <algorithm>
#include <memory>
#include <utility>

#include <QtCore/QList>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>

#include "AeroraAuditSink.h"
#include "AeroraAuthorization.h"
#include "AeroraCommandAuthorizer.h"
#endif

void MavCommandAuthorizationTest::_testDenialCodeIsDistinctFromTransportFailures()
{
    // The central property of ADR-012: a refused command must not be reportable as an
    // unresponsive vehicle or as a duplicate.
    QVERIFY(VehicleTypes::MavCmdResultFailureAuthorizationDenied
            != VehicleTypes::MavCmdResultFailureNoResponseToCommand);
    QVERIFY(VehicleTypes::MavCmdResultFailureAuthorizationDenied
            != VehicleTypes::MavCmdResultFailureDuplicateCommand);
    QVERIFY(VehicleTypes::MavCmdResultFailureAuthorizationDenied
            != VehicleTypes::MavCmdResultCommandResultOnly);
}

void MavCommandAuthorizationTest::_testDenialCodeRenders()
{
    const QString text =
        MavCommandQueue::failureCodeToString(VehicleTypes::MavCmdResultFailureAuthorizationDenied);

    QVERIFY(!text.isEmpty());
    QVERIFY(!text.startsWith(QStringLiteral("Unknown")));
}

void MavCommandAuthorizationTest::_testAllFailureCodesRender()
{
    const VehicleTypes::MavCmdResultFailureCode_t all[] = {
        VehicleTypes::MavCmdResultCommandResultOnly,
        VehicleTypes::MavCmdResultFailureNoResponseToCommand,
        VehicleTypes::MavCmdResultFailureDuplicateCommand,
        VehicleTypes::MavCmdResultFailureAuthorizationDenied,
    };

    for (const VehicleTypes::MavCmdResultFailureCode_t code : all) {
        const QString text = MavCommandQueue::failureCodeToString(code);
        QVERIFY(!text.isEmpty());
        QVERIFY(!text.startsWith(QStringLiteral("Unknown")));
    }
}

void MavCommandAuthorizationTest::_testDefaultHookAuthorizesCommands()
{
    // Regression guard for the core edit: stock QGC must be unaffected by the new hook.
    QGCCorePlugin* const plugin = QGCCorePlugin::instance();
    QVERIFY(plugin != nullptr);

    QVERIFY(plugin->authorizeMavCommand(nullptr, MAV_COMP_ID_AUTOPILOT1, MAV_CMD_NAV_TAKEOFF, false,
                                        0.0f, 0.0f, 0.0f, 0.0f, 0.0, 0.0, 0.0f));
    QVERIFY(plugin->authorizeMavCommand(nullptr, MAV_COMP_ID_AUTOPILOT1, MAV_CMD_COMPONENT_ARM_DISARM, false,
                                        1.0f, 0.0f, 0.0f, 0.0f, 0.0, 0.0, 0.0f));
    QVERIFY(plugin->authorizeMavCommand(nullptr, MAV_COMP_ID_AUTOPILOT1, MAV_CMD_DO_SET_MODE, true,
                                        0.0f, 0.0f, 0.0f, 0.0f, 0.0, 0.0, 0.0f));
}

void MavCommandAuthorizationTest::_testDefaultHookIgnoresNullVehicle()
{
    // The hook is a decision point, not an action point: the base implementation must not
    // dereference anything it is handed.
    QGCCorePlugin* const plugin = QGCCorePlugin::instance();
    QVERIFY(plugin != nullptr);
    QVERIFY(plugin->authorizeMavCommand(nullptr, 0, 0, false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0, 0.0, 0.0f));
}

#ifdef QGC_CUSTOM_BUILD

namespace {

/// Yields whatever identity the test sets, including a deliberately unusable one.
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

/// Answers with a fixed outcome. Carries no role matrix, because there is none to carry:
/// the role set is Q-029 and the chain must be assertable without it.
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

    [[nodiscard]] bool hasEventOfType(AeroraAuditEventType type) const
    {
        return std::any_of(events.cbegin(), events.cend(),
                           [type](const AeroraAuditEvent& event) { return event.type == type; });
    }

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

AeroraGatedOperation makeOperation(int systemId, bool vehicleResolved)
{
    AeroraGatedOperation operation;
    operation.mavCommand = MAV_CMD_COMPONENT_ARM_DISARM;
    operation.targetCompId = MAV_COMP_ID_AUTOPILOT1;
    operation.param1 = 1.0f;  // arm
    operation.vehicle.mavlinkSystemId = systemId;
    operation.vehicle.resolved = vehicleResolved;
    return operation;
}

/// The authorizer logs exactly one warning per denial. Strict log checking requires every
/// denial to be claimed, and expectLogMessage also asserts the warning really fired - so a
/// regression that silently stops reporting denials fails too.
constexpr const char* kAuthorizationLogCategory = "Aerora.Authorization";

QRegularExpression denialWarningPattern()
{
    return QRegularExpression(QStringLiteral("denied mavCommand"));
}

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

void MavCommandAuthorizationTest::cleanup()
{
    // The authorizer is a process-wide singleton. Leaving a stub installed would silently
    // change the authorization posture of every later test.
    installChain(nullptr, nullptr, nullptr, nullptr);

    UnitTest::cleanup();
}

void MavCommandAuthorizationTest::_testOutcomeStringsAreNeverUnknown()
{
    const AeroraAuthorizationOutcome all[] = {
        AeroraAuthorizationOutcome::Allowed,
        AeroraAuthorizationOutcome::DeniedNoIdentity,
        AeroraAuthorizationOutcome::DeniedRoleUnresolved,
        AeroraAuthorizationOutcome::DeniedByPolicy,
        AeroraAuthorizationOutcome::DeniedPolicyUnavailable,
        AeroraAuthorizationOutcome::DeniedNotConfigured,
    };

    for (const AeroraAuthorizationOutcome outcome : all) {
        const QString text = AeroraAuthorizationDecision::outcomeToString(outcome);
        QVERIFY(!text.isEmpty());
        QVERIFY(!text.endsWith(QStringLiteral("Unknown")));
    }
}

void MavCommandAuthorizationTest::_testUnconfiguredChainDenies()
{
    // Nothing installed at all. This is the state a station ships in while Q-029 is open,
    // and it must deny rather than default to permitting.
    installChain(nullptr, nullptr, nullptr, nullptr);

    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();
    QVERIFY(!authorizer->isFullyConfigured());

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    const AeroraAuthorizationDecision decision = authorizer->authorize(makeOperation(1, true));
    verifyExpectedLogMessage();

    QVERIFY(!decision.allowed());
    QCOMPARE(static_cast<int>(decision.outcome), static_cast<int>(AeroraAuthorizationOutcome::DeniedNotConfigured));
    QVERIFY(!decision.reasonCode.isEmpty());
}

void MavCommandAuthorizationTest::_testMissingCollaboratorDenies_data()
{
    QTest::addColumn<bool>("withIdentity");
    QTest::addColumn<bool>("withResolver");
    QTest::addColumn<bool>("policyConfigured");
    // Carried as int: the outcome is a plain enum class with no metatype registration, and
    // registering one purely for a test column would be test machinery leaking into the type.
    QTest::addColumn<int>("expectedOutcome");

    // Each row removes exactly one stage. No combination may produce an allow: a permissive
    // path must not be reconstructable by assembling the chain differently.
    QTest::newRow("no policy")
        << true << true << false << static_cast<int>(AeroraAuthorizationOutcome::DeniedNotConfigured);
    QTest::newRow("no identity provider")
        << false << true << true << static_cast<int>(AeroraAuthorizationOutcome::DeniedNoIdentity);
    QTest::newRow("no role resolver")
        << true << false << true << static_cast<int>(AeroraAuthorizationOutcome::DeniedRoleUnresolved);
}

void MavCommandAuthorizationTest::_testMissingCollaboratorDenies()
{
    QFETCH(bool, withIdentity);
    QFETCH(bool, withResolver);
    QFETCH(bool, policyConfigured);
    QFETCH(int, expectedOutcome);

    installChain(withIdentity ? validIdentity() : nullptr,
                 withResolver ? resolvedRoles() : nullptr,
                 policyConfigured
                     ? allowingPolicy()
                     : std::make_unique<StubPolicy>(false, AeroraAuthorizationOutcome::Allowed, QString()),
                 nullptr);

    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();
    QVERIFY(!authorizer->isFullyConfigured());

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    const AeroraAuthorizationDecision decision = authorizer->authorize(makeOperation(1, true));
    verifyExpectedLogMessage();

    QVERIFY(!decision.allowed());
    QCOMPARE(static_cast<int>(decision.outcome), expectedOutcome);
}

void MavCommandAuthorizationTest::_testInvalidIdentityDenies()
{
    // Unauthenticated identity. The chain is otherwise complete, so this isolates the
    // identity stage.
    AeroraOperatorIdentity unauthenticated;
    unauthenticated.opaqueId = QStringLiteral("opaque-operator-1");
    unauthenticated.authenticated = false;

    installChain(std::make_unique<StubIdentityProvider>(unauthenticated), resolvedRoles(),
                 allowingPolicy(), nullptr);

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    const AeroraAuthorizationDecision decision =
        AeroraCommandAuthorizer::instance()->authorize(makeOperation(1, true));
    verifyExpectedLogMessage();

    QCOMPARE(static_cast<int>(decision.outcome), static_cast<int>(AeroraAuthorizationOutcome::DeniedNoIdentity));
}

void MavCommandAuthorizationTest::_testUnresolvedRoleDenies()
{
    // An unresolved role must never fall back to a default role set.
    AeroraRoleAssignment unresolved;
    unresolved.resolved = false;
    unresolved.roleIds = QStringList{QStringLiteral("ignored-because-unresolved")};

    installChain(validIdentity(), std::make_unique<StubRoleResolver>(unresolved), allowingPolicy(),
                 nullptr);

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    const AeroraAuthorizationDecision decision =
        AeroraCommandAuthorizer::instance()->authorize(makeOperation(1, true));
    verifyExpectedLogMessage();

    QCOMPARE(static_cast<int>(decision.outcome), static_cast<int>(AeroraAuthorizationOutcome::DeniedRoleUnresolved));
}

void MavCommandAuthorizationTest::_testResolvedButEmptyRoleSetReachesPolicy()
{
    // "Resolved to no roles" and "could not resolve" are different answers and must not be
    // collapsed. Doc 14 says an identity with no role is denied every gated operation - but it must
    // be denied *by policy*, carrying a policy reason, not pre-judged by the plumbing. The chain
    // therefore has to hand an empty-but-resolved set to the policy rather than short-circuiting it.
    AeroraRoleAssignment resolvedEmpty;
    resolvedEmpty.resolved = true;
    resolvedEmpty.roleIds = QStringList();
    QVERIFY(!resolvedEmpty.hasAnyRole());

    installChain(validIdentity(), std::make_unique<StubRoleResolver>(resolvedEmpty), denyingPolicy(),
                 nullptr);

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    const AeroraAuthorizationDecision decision =
        AeroraCommandAuthorizer::instance()->authorize(makeOperation(1, true));
    verifyExpectedLogMessage();

    // DeniedByPolicy with the policy's own reason code is only reachable through
    // AeroraAuthorizationPolicy::evaluate(). Had the chain short-circuited on the empty set it
    // would have reported DeniedRoleUnresolved and no reason code of the policy's choosing, so this
    // pair is what proves the policy was actually consulted.
    QCOMPARE(static_cast<int>(decision.outcome),
             static_cast<int>(AeroraAuthorizationOutcome::DeniedByPolicy));
    QCOMPARE(decision.reasonCode, QStringLiteral("policy.refused"));
}

void MavCommandAuthorizationTest::_testConfiguredChainAllows()
{
    // The positive control. Without it the suite could pass by denying unconditionally.
    installChain(validIdentity(), resolvedRoles(), allowingPolicy(), nullptr);

    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();
    QVERIFY(authorizer->isFullyConfigured());
    QVERIFY(authorizer->isAuthorized(makeOperation(1, true)));
}

void MavCommandAuthorizationTest::_testDenialIsAuditableWithReasonAndVehicle()
{
    auto sink = std::make_shared<RecordingAuditSink>();
    installChain(validIdentity(), resolvedRoles(), denyingPolicy(), sink);

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    const AeroraAuthorizationDecision decision =
        AeroraCommandAuthorizer::instance()->authorize(makeOperation(42, true));
    verifyExpectedLogMessage();

    QVERIFY(!decision.allowed());

    // A denial that is not recorded is indistinguishable from a command never sent.
    QVERIFY(sink->hasEventOfType(AeroraAuditEventType::CommandRequested));
    QVERIFY(sink->hasEventOfType(AeroraAuditEventType::AuthorizationDecision));
    QVERIFY(sink->hasEventOfType(AeroraAuditEventType::CommandDenied));

    const AeroraAuditEvent denial = sink->firstEventOfType(AeroraAuditEventType::CommandDenied);
    QCOMPARE(denial.reasonCode, QStringLiteral("policy.refused"));
    QCOMPARE(denial.operatorId, QStringLiteral("opaque-operator-1"));
    QVERIFY(denial.utc.isValid());

    // Which vehicle the denial concerned, without which the record is not usable on a
    // multi-vehicle station.
    QVERIFY(denial.vehicleResolved);
    QCOMPARE(denial.vehicleMavlinkSystemId, 42);
    QCOMPARE(denial.detail.value(QStringLiteral("mavCommand")).toInt(),
             static_cast<int>(MAV_CMD_COMPONENT_ARM_DISARM));

    // The sink must not overstate what it is. Durability and tamper-evidence are unresolved
    // (Q-037 / Q4-003) and nothing may claim them by default.
    QVERIFY(!sink->isDurable());
    QVERIFY(!sink->isTamperEvident());
}

void MavCommandAuthorizationTest::_testUnresolvedVehicleIsNotBackfilled()
{
    auto sink = std::make_shared<RecordingAuditSink>();
    installChain(validIdentity(), resolvedRoles(), denyingPolicy(), sink);

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    (void) AeroraCommandAuthorizer::instance()->authorize(makeOperation(0, false));
    verifyExpectedLogMessage();

    const AeroraAuditEvent denial = sink->firstEventOfType(AeroraAuditEventType::CommandDenied);

    // An absent vehicle is recorded as absent, never substituted with a plausible id.
    QVERIFY(!denial.vehicleResolved);
    QCOMPARE(denial.vehicleMavlinkSystemId, 0);
}

void MavCommandAuthorizationTest::_testMissingAuditSinkIsAccountedNotSilent()
{
    installChain(validIdentity(), resolvedRoles(), denyingPolicy(), nullptr);

    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();

    // The counter is monotonic across the process by design, so assert the delta.
    const quint64 before = authorizer->droppedAuditRecordCount();

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    (void) authorizer->authorize(makeOperation(1, true));
    verifyExpectedLogMessage();

    QVERIFY(authorizer->droppedAuditRecordCount() > before);
}

#endif // QGC_CUSTOM_BUILD
UT_REGISTER_TEST(MavCommandAuthorizationTest, TestLabel::Unit, TestLabel::Vehicle)

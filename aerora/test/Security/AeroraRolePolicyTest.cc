#include "AeroraRolePolicyTest.h"

#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtTest/QTest>

#include "AeroraAuthorization.h"
#include "AeroraRolePolicy.h"

namespace {

// Opaque tokens, not the signed role names. See the suite header: the signed set is deployment
// configuration, so a fixture naming it would couple this suite to one deployment.
const QString kGatedClass = QStringLiteral("gated-class-token");
const QString kOtherClass = QStringLiteral("other-class-token");
const QString kGrantingRole = QStringLiteral("role-token-granting");
const QString kOtherRole = QStringLiteral("role-token-other");
const QString kUnknownRole = QStringLiteral("role-token-absent-from-table");

AeroraRolePolicy::RoleGrants table()
{
    AeroraRolePolicy::RoleGrants grants;
    grants.insert(kGrantingRole, QSet<QString>{kGatedClass, kOtherClass});
    grants.insert(kOtherRole, QSet<QString>{kOtherClass});
    return grants;
}

AeroraRoleAssignment holding(const QStringList& roleIds)
{
    AeroraRoleAssignment assignment;
    assignment.resolved = true;
    assignment.roleIds = roleIds;
    return assignment;
}

AeroraGatedOperation anOperation()
{
    AeroraVehicleContext vehicle;
    vehicle.mavlinkSystemId = 1;
    vehicle.resolved = true;

    AeroraGatedOperation operation;
    operation.address.vehicles.append(vehicle);
    return operation;
}

}  // namespace

void AeroraRolePolicyTest::_aPolicyWithNoTableIsNotConfigured()
{
    const AeroraRolePolicy policy(kGatedClass, {});

    QVERIFY(!policy.isConfigured());
    QCOMPARE(policy.roleCount(), qsizetype(0));
}

void AeroraRolePolicyTest::_aPolicyWithNoGatedClassIsNotConfigured()
{
    // A table with nothing to compare it against grants nothing, so reporting it configured would
    // report enforcement for a station that denies every command.
    const AeroraRolePolicy policy(QString(), table());

    QVERIFY(!policy.isConfigured());
}

void AeroraRolePolicyTest::_aPolicyWithATableAndAClassIsConfigured()
{
    const AeroraRolePolicy policy(kGatedClass, table());

    QVERIFY(policy.isConfigured());
    QCOMPARE(policy.gatedClass(), kGatedClass);
    QCOMPARE(policy.roleCount(), qsizetype(2));
}

void AeroraRolePolicyTest::_aHeldRoleGrantingTheGatedClassIsAllowed()
{
    const AeroraRolePolicy policy(kGatedClass, table());

    const AeroraAuthorizationDecision decision = policy.evaluate(holding({kGrantingRole}), anOperation());

    QVERIFY(decision.allowed());
    QCOMPARE(decision.outcome, AeroraAuthorizationOutcome::Allowed);
}

void AeroraRolePolicyTest::_anyOneOfSeveralHeldRolesGrantingTheClassIsAllowed()
{
    // An operator may hold several roles: AeroraRoleAssignment carries a list rather than one
    // value. One granting role is enough, and the non-granting ones must not veto it.
    const AeroraRolePolicy policy(kGatedClass, table());

    QVERIFY(policy.evaluate(holding({kOtherRole, kGrantingRole}), anOperation()).allowed());
    QVERIFY(policy.evaluate(holding({kUnknownRole, kGrantingRole}), anOperation()).allowed());
}

void AeroraRolePolicyTest::_anUnconfiguredPolicyDeniesAsUnavailableRatherThanUnconfigured()
{
    // The distinction is the point. DeniedNotConfigured means no policy is installed at all, and
    // returning it from an installed policy would hide a deployment that shipped without a table
    // behind the posture of one that was never configured.
    const AeroraRolePolicy policy(kGatedClass, {});

    const AeroraAuthorizationDecision decision = policy.evaluate(holding({kGrantingRole}), anOperation());

    QVERIFY(!decision.allowed());
    QCOMPARE(decision.outcome, AeroraAuthorizationOutcome::DeniedPolicyUnavailable);
}

void AeroraRolePolicyTest::_anUnresolvedAssignmentIsDenied()
{
    const AeroraRolePolicy policy(kGatedClass, table());

    // Unresolved, and carrying a role that would otherwise grant. The resolution flag has to be
    // load-bearing on its own: a resolver that failed and left its output populated must not grant.
    AeroraRoleAssignment unresolved;
    unresolved.resolved = false;
    unresolved.roleIds = QStringList{kGrantingRole};

    const AeroraAuthorizationDecision decision = policy.evaluate(unresolved, anOperation());

    QVERIFY(!decision.allowed());
    QCOMPARE(decision.outcome, AeroraAuthorizationOutcome::DeniedByPolicy);
}

void AeroraRolePolicyTest::_anAssignmentResolvedToNoRolesIsDenied()
{
    const AeroraRolePolicy policy(kGatedClass, table());

    const AeroraAuthorizationDecision decision = policy.evaluate(holding({}), anOperation());

    QVERIFY(!decision.allowed());
    QCOMPARE(decision.reasonCode, QStringLiteral("no-role-held"));
}

void AeroraRolePolicyTest::_aHeldRoleAbsentFromTheTableIsDenied()
{
    const AeroraRolePolicy policy(kGatedClass, table());

    const AeroraAuthorizationDecision decision = policy.evaluate(holding({kUnknownRole}), anOperation());

    QVERIFY(!decision.allowed());
    QCOMPARE(decision.outcome, AeroraAuthorizationOutcome::DeniedByPolicy);
}

void AeroraRolePolicyTest::_aHeldRoleGrantingOtherClassesIsDenied()
{
    // The case a deployment gets wrong: a real role, in the table, authorised for something else.
    const AeroraRolePolicy policy(kGatedClass, table());

    const AeroraAuthorizationDecision decision = policy.evaluate(holding({kOtherRole}), anOperation());

    QVERIFY(!decision.allowed());
    QCOMPARE(decision.reasonCode, QStringLiteral("class-not-granted-to-held-roles"));
}

void AeroraRolePolicyTest::_theGatedClassIsComparedExactly()
{
    // No prefix matching, no case folding, no trimming. A class token is an opaque identifier and
    // a near-match is a different class, so an almost-right table entry must deny rather than
    // nearly grant.
    AeroraRolePolicy::RoleGrants nearly;
    nearly.insert(kGrantingRole, QSet<QString>{kGatedClass.toUpper()});
    QVERIFY(!AeroraRolePolicy(kGatedClass, nearly).evaluate(holding({kGrantingRole}), anOperation()).allowed());

    AeroraRolePolicy::RoleGrants padded;
    padded.insert(kGrantingRole, QSet<QString>{kGatedClass + QStringLiteral(" ")});
    QVERIFY(!AeroraRolePolicy(kGatedClass, padded).evaluate(holding({kGrantingRole}), anOperation()).allowed());
}

void AeroraRolePolicyTest::_noReasonCodeCarriesARoleName()
{
    // AeroraAuthorizationDecision records that the reason code is "never a role name". An audit
    // record naming the role that failed would publish the deployment's role set into every log.
    const AeroraRolePolicy policy(kGatedClass, table());

    const QStringList codes{
        AeroraRolePolicy(kGatedClass, {}).evaluate(holding({kGrantingRole}), anOperation()).reasonCode,
        policy.evaluate(holding({}), anOperation()).reasonCode,
        policy.evaluate(holding({kUnknownRole}), anOperation()).reasonCode,
        policy.evaluate(holding({kOtherRole}), anOperation()).reasonCode,
    };

    for (const QString& code : codes) {
        QVERIFY(!code.isEmpty());
        QVERIFY(!code.contains(kGrantingRole));
        QVERIFY(!code.contains(kOtherRole));
        QVERIFY(!code.contains(kUnknownRole));
    }
}

void AeroraRolePolicyTest::_theSubjectDoesNotChangeTheVerdict()
{
    // Asserted rather than assumed. Every operation reaching one seam carries that seam's class by
    // construction, and no accepted record distinguishes one MAV_CMD from another within a class -
    // so a verdict that moved with the command or its parameters would be deciding something no
    // owner has decided. This is the test that fails if somebody adds that distinction here
    // instead of recording it.
    const AeroraRolePolicy policy(kGatedClass, table());
    const AeroraRoleAssignment granting = holding({kGrantingRole});
    const AeroraRoleAssignment refused = holding({kOtherRole});

    AeroraGatedOperation armed = anOperation();
    armed.command.mavCommand = 400;
    armed.command.param1 = 1.0F;

    AeroraGatedOperation disarmed = anOperation();
    disarmed.command.mavCommand = 400;
    disarmed.command.param1 = 0.0F;

    AeroraGatedOperation other = anOperation();
    other.command.mavCommand = 22;
    other.command.commandInt = true;

    QVERIFY(policy.evaluate(granting, armed).allowed());
    QVERIFY(policy.evaluate(granting, disarmed).allowed());
    QVERIFY(policy.evaluate(granting, other).allowed());

    QVERIFY(!policy.evaluate(refused, armed).allowed());
    QVERIFY(!policy.evaluate(refused, disarmed).allowed());
    QVERIFY(!policy.evaluate(refused, other).allowed());
}

UT_REGISTER_TEST(AeroraRolePolicyTest, TestLabel::Unit)

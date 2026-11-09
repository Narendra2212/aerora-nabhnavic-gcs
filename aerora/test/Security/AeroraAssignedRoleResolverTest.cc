#include "AeroraAssignedRoleResolverTest.h"

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtTest/QTest>

#include "AeroraAssignedRoleResolver.h"
#include "AeroraAuthorization.h"

namespace {

// Opaque tokens, not the signed role names. The signed set is deployment configuration, so a
// fixture naming it would couple this suite to one deployment - and a resolver that special-cased a
// real role name would not be caught by a suite that only ever handed it real ones.
const QString kPilotId = QStringLiteral("opaque-operator-pilot");
const QString kMaintainerId = QStringLiteral("opaque-operator-maintainer");
const QString kUnknownId = QStringLiteral("opaque-operator-never-provisioned");
const QString kFlyingRole = QStringLiteral("role-token-flying");
const QString kParameterRole = QStringLiteral("role-token-parameter");

AeroraOperatorIdentity usableIdentity(const QString& opaqueId)
{
    AeroraOperatorIdentity identity;
    identity.opaqueId = opaqueId;
    identity.authenticated = true;
    return identity;
}

AeroraAssignedRoleResolver::Assignment twoOperators()
{
    AeroraAssignedRoleResolver::Assignment assignment;
    assignment.insert(kPilotId, QStringList{kFlyingRole});
    assignment.insert(kMaintainerId, QStringList{kParameterRole});
    return assignment;
}

}  // namespace

void AeroraAssignedRoleResolverTest::_anUnauthenticatedIdentityIsUnresolved()
{
    const AeroraAssignedRoleResolver resolver(twoOperators());

    // Authenticated false, and carrying an id the assignment does hold. The flag has to be
    // load-bearing on its own: an identity provider that failed and left its output populated must
    // not resolve to the roles that id would otherwise get.
    AeroraOperatorIdentity identity;
    identity.opaqueId = kPilotId;
    identity.authenticated = false;

    const AeroraRoleAssignment assignment = resolver.resolve(identity);

    QVERIFY(!assignment.resolved);
    QVERIFY(!assignment.hasAnyRole());
}

void AeroraAssignedRoleResolverTest::_anEmptyIdentityIsUnresolved()
{
    const AeroraAssignedRoleResolver resolver(twoOperators());

    AeroraOperatorIdentity identity;
    identity.opaqueId = QString();
    identity.authenticated = true;

    QVERIFY(!resolver.resolve(identity).resolved);
}

void AeroraAssignedRoleResolverTest::_anIdentityAbsentFromTheAssignmentResolvesToNoRoles()
{
    const AeroraAssignedRoleResolver resolver(twoOperators());

    const AeroraRoleAssignment assignment = resolver.resolve(usableIdentity(kUnknownId));

    // Resolved, because the assignment was consulted. Holding nothing, because it was not in it.
    QVERIFY(assignment.resolved);
    QVERIFY(assignment.roleIds.isEmpty());
    QVERIFY(!assignment.hasAnyRole());
}

void AeroraAssignedRoleResolverTest::_anAssignedIdentityResolvesToItsRoles()
{
    const AeroraAssignedRoleResolver resolver(twoOperators());

    const AeroraRoleAssignment pilot = resolver.resolve(usableIdentity(kPilotId));
    QVERIFY(pilot.hasAnyRole());
    QCOMPARE(pilot.roleIds, QStringList{kFlyingRole});

    const AeroraRoleAssignment maintainer = resolver.resolve(usableIdentity(kMaintainerId));
    QVERIFY(maintainer.hasAnyRole());
    QCOMPARE(maintainer.roleIds, QStringList{kParameterRole});

    QCOMPARE(resolver.operatorCount(), qsizetype(2));
}

void AeroraAssignedRoleResolverTest::_unresolvedAndResolvedToNoRolesAreDifferentStates()
{
    // The assertion this suite exists for. Both deny, so a defect that collapsed them would change
    // no verdict and would change the cause the audit trail records: an unprovisioned operator
    // reads as an identity-provider fault, and the remedies are different.
    const AeroraAssignedRoleResolver resolver(twoOperators());

    AeroraOperatorIdentity unusable;
    unusable.opaqueId = kUnknownId;
    unusable.authenticated = false;

    const AeroraRoleAssignment unresolved = resolver.resolve(unusable);
    const AeroraRoleAssignment noRoles = resolver.resolve(usableIdentity(kUnknownId));

    QVERIFY(!unresolved.resolved);
    QVERIFY(noRoles.resolved);

    // Identical in authority and distinguishable in cause, which is the whole point.
    QCOMPARE(unresolved.hasAnyRole(), noRoles.hasAnyRole());
    QVERIFY(unresolved.resolved != noRoles.resolved);
}

void AeroraAssignedRoleResolverTest::_refreshingReplacesTheWholeAssignment()
{
    AeroraAssignedRoleResolver resolver(twoOperators());

    AeroraAssignedRoleResolver::Assignment refreshed;
    refreshed.insert(kUnknownId, QStringList{kFlyingRole});
    resolver.setAssignment(refreshed);

    QCOMPARE(resolver.operatorCount(), qsizetype(1));
    QVERIFY(resolver.resolve(usableIdentity(kUnknownId)).hasAnyRole());

    // Both previous operators are gone rather than retained alongside the new one.
    QVERIFY(!resolver.resolve(usableIdentity(kPilotId)).hasAnyRole());
    QVERIFY(!resolver.resolve(usableIdentity(kMaintainerId)).hasAnyRole());
}

void AeroraAssignedRoleResolverTest::_refreshingWithdrawsARoleRatherThanKeepingIt()
{
    // The unsafe direction, asserted directly. A merge would leave the withdrawn role in force,
    // which is what IF-SEC-003's "replaces it wholly, never merges" exists to prevent.
    AeroraAssignedRoleResolver resolver;

    AeroraAssignedRoleResolver::Assignment both;
    both.insert(kPilotId, QStringList{kFlyingRole, kParameterRole});
    resolver.setAssignment(both);
    QCOMPARE(resolver.resolve(usableIdentity(kPilotId)).roleIds.size(), 2);

    AeroraAssignedRoleResolver::Assignment narrowed;
    narrowed.insert(kPilotId, QStringList{kFlyingRole});
    resolver.setAssignment(narrowed);

    const AeroraRoleAssignment after = resolver.resolve(usableIdentity(kPilotId));
    QCOMPARE(after.roleIds, QStringList{kFlyingRole});
    QVERIFY2(!after.roleIds.contains(kParameterRole),
             "a withdrawn role must not survive a refresh, which is what a merge would do");
}

void AeroraAssignedRoleResolverTest::_refreshingToAnEmptyAssignmentRevokesEveryone()
{
    AeroraAssignedRoleResolver resolver(twoOperators());

    resolver.setAssignment({});

    QCOMPARE(resolver.operatorCount(), qsizetype(0));

    // Still resolved, still holding nothing. A station whose assignment was revoked has consulted
    // it and found nothing, which is not the same as having failed to consult it.
    const AeroraRoleAssignment assignment = resolver.resolve(usableIdentity(kPilotId));
    QVERIFY(assignment.resolved);
    QVERIFY(!assignment.hasAnyRole());
}

void AeroraAssignedRoleResolverTest::_anyRoleTokenIsCarriedThroughUninterpreted()
{
    // A token that is in no signed set at all. The resolver must carry it through unchanged: it
    // holds the deployment's assignment and validates no name, so a resolver that filtered against
    // a compiled-in set would fail here.
    const QString arbitrary = QStringLiteral("a-token-no-closure-record-mentions");

    AeroraAssignedRoleResolver::Assignment assignment;
    assignment.insert(kPilotId, QStringList{arbitrary});

    const AeroraAssignedRoleResolver resolver(assignment);

    QCOMPARE(resolver.resolve(usableIdentity(kPilotId)).roleIds, QStringList{arbitrary});
}

void AeroraAssignedRoleResolverTest::_severalRolesForOneOperatorAreAllCarried()
{
    // AeroraRoleAssignment carries a list, so one operator may hold several roles - which is how a
    // deployment gives one person both the flying and the maintenance role without the signed set
    // changing. Order is preserved so the trail records what configuration said.
    AeroraAssignedRoleResolver::Assignment assignment;
    assignment.insert(kPilotId, QStringList{kFlyingRole, kParameterRole});

    const AeroraAssignedRoleResolver resolver(assignment);
    const AeroraRoleAssignment resolved = resolver.resolve(usableIdentity(kPilotId));

    QCOMPARE(resolved.roleIds, (QStringList{kFlyingRole, kParameterRole}));
}

UT_REGISTER_TEST(AeroraAssignedRoleResolverTest, TestLabel::Unit)

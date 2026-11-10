#pragma once

#include "UnitTest.h"

class AeroraCachedCredentialIdentityProvider;

namespace AeroraSecurityTestDoubles {
class RecordingSink;
}

/// \brief Proves the composed authorization - the three installables plus the operation gate -
/// enforces the owner's entry-not-continuation rule end to end, on a built binary.
///
/// \section why Why this suite exists
///
/// The gate, the provider split, the resolver and the policy are each unit-tested in isolation.
/// This is the one that installs all of them on `AeroraCommandAuthorizer` and drives
/// `authorizeThroughOperationGate()`, so it proves the composition rather than the parts: that a
/// stale credential refuses an arm, that it refuses a re-arm after a disarm, and - the case the
/// whole rule turns on - that it refuses nothing once an operation is in force, so a command in
/// flight survives the credential expiring under it.
///
/// \section real Why the real collaborators
///
/// No stubs. The identity provider is the real `AeroraCachedCredentialIdentityProvider` whose
/// window the test moves to simulate a login and an expiry; the resolver and policy are the real
/// `AeroraAssignedRoleResolver` and `AeroraRolePolicy` over opaque tokens. A stub chain could pass
/// while the real composition failed at a seam between two of them, which is exactly what an
/// end-to-end suite is for.
///
/// \section notlive What it does not cover
///
/// The live command path. `AeroraCorePlugin::authorizeMavCommand` does not yet route through
/// `authorizeThroughOperationGate` - that is the next step - so this drives the authorizer's API
/// directly with the arm/disarm classification the call site will later supply. The shipped build
/// still enforces nothing, because nothing installs the chain.
class AeroraOperationGateEnforcementTest : public UnitTest
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void _aFreshArmIsAuthorizedAndEntersTheOperation();
    void _aStaleArmIsRefusedAndAuditedWithTheGateReason();
    void _aCommandInFlightSurvivesTheCredentialExpiring();
    void _aStaleReArmAfterDisarmIsRefused();
    void _theOperationPhaseIsIndependentPerVehicle();
    void _aRoleRefusedArmDoesNotEnterTheOperation();

private:
    // Non-owning; the authorizer owns the installed provider, valid while the chain is installed.
    AeroraCachedCredentialIdentityProvider* _provider = nullptr;
    std::shared_ptr<AeroraSecurityTestDoubles::RecordingSink> _sink;
};

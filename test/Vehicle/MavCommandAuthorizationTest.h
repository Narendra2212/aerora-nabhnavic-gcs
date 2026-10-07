#pragma once

#include "UnitTest.h"

/// Covers the command authorization gate introduced by ADR-012.
///
/// The end-to-end denial path requires a build in which QGCCorePlugin::instance() resolves to
/// an overriding subclass. In a stock build that symbol is fixed at compile time, so the
/// refusal path is reachable only from a custom-build test target - which is Q-035 / R-021,
/// still unresolved. What is asserted here is what a stock build can genuinely observe: the
/// distinctness and rendering of the denial failure code, and that the default hook does not
/// change stock behaviour.
///
/// The QGC_CUSTOM_BUILD section asserts the Aerora authorization chain itself. Those cases
/// compile only when the overlay is part of the build, because the chain lives in the overlay
/// and a stock build does not contain it. They exercise the authorizer directly rather than
/// through a Vehicle: fail-closed is a property of the chain, so asserting it needs no link,
/// no MockLink and no vehicle.
class MavCommandAuthorizationTest : public UnitTest
{
    Q_OBJECT

#ifdef QGC_CUSTOM_BUILD
protected slots:
    void cleanup() override;
#endif

private slots:
    void _testDenialCodeIsDistinctFromTransportFailures();
    void _testDenialCodeRenders();
    void _testAllFailureCodesRender();
    void _testDefaultHookAuthorizesCommands();
    void _testDefaultHookIgnoresNullVehicle();

#ifdef QGC_CUSTOM_BUILD
    void _testOutcomeStringsAreNeverUnknown();
    void _testUnconfiguredChainDenies();
    void _testMissingCollaboratorDenies_data();
    void _testMissingCollaboratorDenies();
    void _testInvalidIdentityDenies();
    void _testUnresolvedRoleDenies();
    void _testResolvedButEmptyRoleSetReachesPolicy();
    void _testConfiguredChainAllows();
    void _testDenialIsAuditableWithReasonAndVehicle();
    void _testUnresolvedVehicleIsNotBackfilled();
    void _testMissingAuditSinkIsAccountedNotSilent();
#endif
};

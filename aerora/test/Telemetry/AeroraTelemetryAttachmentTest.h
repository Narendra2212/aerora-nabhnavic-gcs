#pragma once

#include "UnitTest.h"

/// \brief Verifies the telemetry attachment contract: per-vehicle registration through a registrar,
/// sealing against dynamic groups, and the lifetime degradations that must not fabricate state.
///
/// These are the Q4-002 properties that are checkable without the authorised core edit. The
/// registrar is an interface precisely so the publication contract can be exercised against a test
/// implementation; the one concrete implementation lives with the core edit.
class AeroraTelemetryAttachmentTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _serviceNameIsOwnedByTheTelemetryType();

    // Publication through the registrar.
    void _publishHandsEveryGroupToTheRegistrar();
    void _publishSealsAgainstDynamicGroups();
    void _publishIsRefusedTwice();
    void _refusedGroupIsReportedNotSwallowed();

    // The concrete vehicle registrar.
    void _concreteRegistrarRefusesWithoutAVehicle();
    void _concreteRegistrarHoldsNoSharedState();
    void _publishFailureThroughConcreteRegistrarIsObservable();

    // Two vehicles, two sets.
    void _twoSetsShareNoGroupsOrEvaluator();

    // Lifetime: the degradations, and the one that must not happen.
    void _staleVerdictSurvivesEvaluatorDestruction();
    void _staleVerdictSurvivesThresholdRemoval();
    void _newReportWithdrawsTheStaleVerdict();
};

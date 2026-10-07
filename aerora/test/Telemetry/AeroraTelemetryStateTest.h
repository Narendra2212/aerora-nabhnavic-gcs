#pragma once

#include "UnitTest.h"

/// \brief Verifies that the seven Aerora telemetry state classes stay separate, that freshness is
/// never invented, and that a non-authoritative source cannot become effective aircraft state.
///
/// Every test here runs against a deterministic clock (\c ManualScheduler) and touches no vehicle, no
/// link and no wall clock, so a staleness transition is produced by advancing time explicitly rather
/// than by waiting for it.
class AeroraTelemetryStateTest : public UnitTest
{
    Q_OBJECT

private slots:
    // The state model itself.
    void _resolutionTable_data();
    void _resolutionTable();
    void _classPredicates_data();
    void _classPredicates();

    // Authority: the promotions that must not exist.
    void _nonAuthoritativeKindsAreNeverEffective_data();
    void _nonAuthoritativeKindsAreNeverEffective();

    // Freshness.
    void _freshnessWithoutThresholdIsNotAVerdict();
    void _freshnessTransitionsAcrossTheThreshold();

    // The two non-values.
    void _unknownAndUnavailableStayDistinct();

    // The value triple.
    void _reportingSourceIsAttributedToTheSender();
    void _receiveTimeIsStampedBeforeTheValueIsVisible();

    // Attachment boundary properties.
    void _groupSharedBySecondVehicleIsDetected();
    void _setRefusesGroupsAfterSealing();
};

#pragma once

#include "UnitTest.h"

/// \brief Asserts the Manual_Authority_Grant, its expiry, and the read path by which the station
/// observes manual control without carrying it.
///
/// \section why Why a grant exists instead of passthrough
///
/// The register records the hazard as `manual-control-latency`: a manual control path through a
/// general-purpose operating system has no bounded worst-case latency, and a host that stalls
/// delivers stick positions late or not at all with the operator unable to tell which. Sticks
/// therefore reach the aircraft directly and the station grants and revokes the mode, which makes
/// it a single point of authority rather than a single point of control.
///
/// \section expiry Why expiry is read and not latched
///
/// Requirement 2 criterion 3 requires refresh to keep a grant in force, and criterion 5 expires it
/// when the link drops. A latched expiry flag would need something to clear it, and whatever failed
/// to refresh the grant is the same thing that would have failed to clear the flag - so expiry is
/// derived from the clock on every read. The cases below pin the boundary, because an off-by-one at
/// the expiry instant is the difference between a grant that lapses and one that never does.
///
/// \section absence Why three absences are kept apart
///
/// Never issued, issued and expired, and no telemetry observed are three different conditions with
/// three different repairs, and each is asserted separately. A station that has never granted
/// manual authority is not one whose grant lapsed, and neither is a station that has heard nothing
/// from the aircraft.
class AeroraManualAuthorityTest : public UnitTest
{
    Q_OBJECT

private slots:
    // The grant and its expiry.
    void _anUnissuedGrantIsNeverInForce();
    void _anIssuedGrantIsInForceBeforeItsExpiry();
    void _aGrantIsNotInForceAtOrAfterItsExpiryInstant();
    void _aGrantIsNotInForceBeforeItWasIssued();
    void _refreshingAGrantExtendsItWithoutReissuing();

    // The mirrored read path.
    void _anUnobservedReportIsNotAReportOfCentredSticks();
    void _anObservedReportCarriesItsAgeAndTheVehicleAnswer();
    void _theVehicleGrantAnswerIsIndependentOfTheStationGrant();

    // The audit vocabulary for the four transitions.
    void _everyGrantTransitionRendersItsOwnName();
    void _noGrantTransitionRendersAsUnknown();
};

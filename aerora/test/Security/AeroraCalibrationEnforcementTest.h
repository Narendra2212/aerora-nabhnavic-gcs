#pragma once

#include "BaseClasses/VehicleTest.h"

/// End-to-end enforcement of the calibration authorization gate (ADR-015 seam, ADR-017 policy).
///
/// This is the test the two preceding sessions kept specifying but could not write: until the core
/// edit landed in `Vehicle::startCalibration()` there was nothing to deny, so "a denied calibration
/// puts no bytes on the wire" would have passed for the wrong reason.
///
/// It asserts against the wire rather than against internal state, using MockLink's received-command
/// counters. `Vehicle::startCalibration()` never enters `MavCommandQueue`, so there is no queue entry
/// to inspect — the packet count is the only honest observable.
///
/// `QGCMAVLink::CalibrationEsc` is used throughout. It is the motor-relevant variant that matters
/// most for security (it is carried in `param7` alone), and it is also the quietest: MockLink
/// acknowledges it without starting a calibration simulation or emitting status text, so a refusal
/// and an acceptance differ only in the thing under test.
///
/// Lives in the overlay test tree because it needs `QGCCorePlugin::instance()` to resolve to
/// `AeroraCorePlugin`, which only happens in a `QGC_CUSTOM_BUILD`.
class AeroraCalibrationEnforcementTest : public VehicleTest
{
    Q_OBJECT

protected slots:
    void init() override;
    void cleanup() override;

private slots:
    void _testDeniedCalibrationStartReachesNoWire();
    void _testAuthorizedCalibrationStartReachesWire();
    void _testCancelStillReachesWireWhenStartIsDenied();
    void _testDeniedCalibrationAuditIdentifiesVehicleAndCalibration();
    void _testUnconfiguredChainAllowsCalibration();
};

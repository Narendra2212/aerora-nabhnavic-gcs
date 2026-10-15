#pragma once

#include "BaseClasses/VehicleTest.h"

/// Covers the Aerora mission bridge against QGroundControl's real mission stack.
///
/// Uses a `MAV_AUTOPILOT_GENERIC` MockLink because that is the autopilot value an Aerora build
/// claims while Q-018 is open, and because the bridge attaches through `AeroraVehicleServices`,
/// which only exists on a vehicle routed to the Aerora firmware plugin. A PX4-flavoured MockLink
/// would resolve to a different plugin and assert nothing about Aerora.
///
/// The initial connect sequence is skipped, for the same reason the vehicle lifecycle test skips
/// it: waiting for it adds a failure mode this test does not care about. What this test does care
/// about is that no mission transaction is running when it starts its own, so it waits on that
/// condition explicitly instead.
///
/// The properties asserted here are the ones that cannot be asserted against the mock, because
/// they are properties of the real `PlanManager` completion path: that an upload replaces the
/// vehicle mission, that it resets the vehicle's current index, and that a successful upload is
/// still not a verified one.
class AeroraMissionSyncTest : public VehicleTest
{
    Q_OBJECT

public:
    explicit AeroraMissionSyncTest(QObject *parent = nullptr)
        : VehicleTest(parent)
    {
        setAutopilotType(MAV_AUTOPILOT_GENERIC);
        setWaitForInitialConnect(false);
    }

protected slots:
    void init() override;

private slots:
    void _missionObservationAttachesToARealVehicle();
    void _aRealUploadReplacesTheVehicleMissionAndResetsTheCurrentIndex();
};

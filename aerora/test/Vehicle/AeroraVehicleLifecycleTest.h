#pragma once

#include "BaseClasses/VehicleTest.h"

/// Covers the Aerora integration point in QGroundControl's real vehicle lifecycle.
///
/// Uses a `MAV_AUTOPILOT_GENERIC` MockLink because that is the autopilot value an Aerora build
/// claims while Q-018 is open — see AeroraFirmwareClaim. A PX4-flavoured MockLink would resolve to
/// a different plugin and assert nothing about Aerora.
///
/// The initial-connect sequence is deliberately skipped. `FirmwarePlugin::initializeVehicle()` is
/// called from the `Vehicle` constructor, so everything asserted here exists before the connect
/// sequence runs, and waiting for it would add a failure mode the test does not care about.
class AeroraVehicleLifecycleTest : public VehicleTest
{
    Q_OBJECT

public:
    explicit AeroraVehicleLifecycleTest(QObject *parent = nullptr)
        : VehicleTest(parent)
    {
        setAutopilotType(MAV_AUTOPILOT_GENERIC);
        setWaitForInitialConnect(false);
    }

protected slots:
    void init() override;

private slots:
    void _aeroraServicesAttachToADiscoveredVehicle();
    void _domainServicesAttachToADiscoveredVehicle();
    void _missionBridgeIsAttachedByTheLifecycleAndOnlyOnce();
    void _autopilotPluginPublishesOnlyPagesThatExist();
    void _toolIndicatorsAreResolvedPerVehicle();
};

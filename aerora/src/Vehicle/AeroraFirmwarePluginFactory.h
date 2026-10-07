#pragma once

#include <QtCore/QList>

#include "FirmwarePluginFactory.h"
#include "QGCMAVLink.h"

class AeroraFirmwarePlugin;
class FirmwarePlugin;

/// Registers Aerora's firmware abstraction with QGroundControl.
///
/// Registration happens in the `FirmwarePluginFactory` base constructor, which appends `this` to
/// `FirmwarePluginFactoryRegister`. The mechanism therefore needs a non-local static object
/// (AeroraFirmwarePluginFactoryImp below) whose constructor runs before `main()`. No core edit is
/// involved.
///
/// ### The linkage hazard
///
/// A self-registering global in a **static library** can be discarded by the linker, because no
/// symbol in its translation unit is referenced. There is no diagnostic: the build succeeds, the
/// application starts, the Aerora plugin is simply never registered, and the operator sees a
/// generic vehicle.
///
/// Two consequences, both load-bearing:
///
/// - This translation unit must reach the **application target**, via the overlay's
///   `CUSTOM_SOURCES` export. It must never be compiled into an Aerora static library.
/// - The header carries an `extern` declaration of the instance, which anchors it.
class AeroraFirmwarePluginFactory : public FirmwarePluginFactory
{
    Q_OBJECT

public:
    AeroraFirmwarePluginFactory();
    ~AeroraFirmwarePluginFactory() override;

    /// @return The single firmware class Aerora claims — AeroraFirmwareClaim::kFirmwareClass.
    QList<QGCMAVLink::FirmwareClass_t> supportedFirmwareClasses() const final;

    /// @return The shared AeroraFirmwarePlugin for the claimed class, nullptr otherwise.
    FirmwarePlugin *firmwarePluginForAutopilot(MAV_AUTOPILOT autopilotType, MAV_TYPE vehicleType) final;

    // supportedVehicleClasses() is deliberately not overridden, so it keeps the base-class
    // default of every vehicle class.
    //
    // QGroundControl's reference overlay narrows it to multirotor to match one product. Aerora is
    // intended to serve several product families whose airframe classes are not evidenced, so
    // narrowing now would encode a guess about future hardware into the seam that decides whether
    // a vehicle is usable at all. The current aircraft being a hexacopter is not a reason to
    // reject a future fixed-wing inspection airframe at the factory.

private:
    AeroraFirmwarePlugin *_pluginInstance = nullptr;
};

/// Anchors the self-registering factory object so its translation unit is not discarded.
extern AeroraFirmwarePluginFactory AeroraFirmwarePluginFactoryImp;

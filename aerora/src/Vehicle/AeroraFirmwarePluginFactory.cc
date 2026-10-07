#include "AeroraFirmwarePluginFactory.h"

#include "AeroraFirmwareClaim.h"
#include "AeroraFirmwarePlugin.h"
#include "AeroraVehicleLog.h"

/// The self-registering instance. Its construction before main() is what puts Aerora into
/// FirmwarePluginFactoryRegister.
AeroraFirmwarePluginFactory AeroraFirmwarePluginFactoryImp;

AeroraFirmwarePluginFactory::AeroraFirmwarePluginFactory() = default;

AeroraFirmwarePluginFactory::~AeroraFirmwarePluginFactory() = default;

QList<QGCMAVLink::FirmwareClass_t> AeroraFirmwarePluginFactory::supportedFirmwareClasses() const
{
    return {AeroraFirmwareClaim::kFirmwareClass};
}

FirmwarePlugin *AeroraFirmwarePluginFactory::firmwarePluginForAutopilot(MAV_AUTOPILOT autopilotType,
                                                                       MAV_TYPE vehicleType)
{
    Q_UNUSED(vehicleType)

    // Compares the raw MAV_AUTOPILOT value, mirroring FirmwarePluginManager::_findPluginFactory()
    // exactly. Normalising through QGCMAVLink::firmwareClass() here would make this method claim
    // autopilot values the manager would never route to it, which would be a difference that only
    // shows up if the manager's lookup ever changes.
    if (static_cast<QGCMAVLink::FirmwareClass_t>(autopilotType) != AeroraFirmwareClaim::kFirmwareClass) {
        return nullptr;
    }

    if (!_pluginInstance) {
        // Parented to the factory, mirroring FirmwarePluginManager's own generic fallback. One
        // instance per process, owned rather than leaked.
        _pluginInstance = new AeroraFirmwarePlugin(this);
        qCDebug(AeroraVehicleLog) << "created Aerora firmware plugin"
                                  << "claimedFirmwareClass:" << AeroraFirmwareClaim::kFirmwareClass;
    }

    return _pluginInstance;
}

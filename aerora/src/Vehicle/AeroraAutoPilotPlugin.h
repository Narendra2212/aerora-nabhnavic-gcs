#pragma once

#include <QtCore/QString>
#include <QtCore/QVariantList>

#include "AutoPilotPlugin.h"

class Vehicle;
class VehicleComponent;

/// Aerora's vehicle setup plugin: the page index behind the Vehicle Configuration view.
///
/// Returned from `AeroraFirmwarePlugin::autopilotPlugin()`, which is QGroundControl's only seam for
/// supplying a firmware class's setup pages (`FirmwarePlugin.h:122`). `Vehicle` re-parents the
/// instance to itself and deletes it in its own destructor (`Vehicle.cc:145-146`, `:407`), so this
/// class has no lifetime management of its own.
///
/// ### Why it derives from `AutoPilotPlugin` and re-creates the joystick page
///
/// The obvious base would be `GenericAutoPilotPlugin`, which is what
/// `FirmwarePlugin::autopilotPlugin()` returns by default. It cannot be used: it declares
/// `vehicleComponents()` **`final`** (`GenericAutoPilotPlugin.h:18`), so an override cannot append
/// to its list.
///
/// That matters because `GenericAutoPilotPlugin`'s only content is the joystick setup page, and
/// overriding `autopilotPlugin()` without reproducing it would silently remove joystick setup from
/// every Aerora vehicle. `JoystickComponent` lives in `AutoPilotPlugins/Common` and is firmware
/// neutral, so it is re-created here rather than reimplemented.
///
/// This is the opposite conclusion from `AeroraFirmwarePlugin`, which deliberately does **not**
/// derive from a vendor plugin, and for the same reason: inherit what is firmware neutral and
/// correct, decline what is vendor specific and would become an Aerora default by accident.
///
/// ### The page list is the honest set, not the intended set
///
/// `vehicleComponents()` publishes an Aerora page only when its QML exists in the build. See
/// AeroraVehicleComponent for why, and for the one page that is withheld by decision.
///
/// ### This class reconfigures nothing
///
/// It constructs page descriptors and hands them to QGroundControl. It writes no parameter, sends
/// no command, and has no path to one. `prerequisiteSetup()` returns no prerequisite, which is a
/// statement about ordering and not permission to act.
class AeroraAutoPilotPlugin : public AutoPilotPlugin
{
    Q_OBJECT

public:
    /// Creates the plugin for @a vehicle.
    ///
    /// @a vehicle must not be null: `AutoPilotPlugin`'s constructor reads
    /// `vehicle->firmwarePlugin()` without a null check. AeroraFirmwarePlugin::autopilotPlugin()
    /// is the only caller and guards it.
    explicit AeroraAutoPilotPlugin(Vehicle *vehicle, QObject *parent = nullptr);
    ~AeroraAutoPilotPlugin() override;

    /// The setup pages for this vehicle: the publishable Aerora pages in declaration order,
    /// followed by the joystick page inherited from the generic plugin.
    ///
    /// Built once and cached. The cache is latched explicitly rather than inferred from the list
    /// being empty, because an empty Aerora page set is a legitimate outcome and an
    /// emptiness-keyed cache would re-create every component on each call, parenting a new set
    /// onto this object every time.
    const QVariantList &vehicleComponents() override;

    /// @return An empty string for every component: no Aerora page gates another.
    ///
    /// Aerora pages are read-only displays, so there is no setup order among them, and the
    /// joystick page has no prerequisite upstream either
    /// (`GenericAutoPilotPlugin::prerequisiteSetup()` returns an empty string).
    QString prerequisiteSetup(VehicleComponent *component) const override;

private:
    QVariantList _components;
    bool _componentsBuilt = false;
};

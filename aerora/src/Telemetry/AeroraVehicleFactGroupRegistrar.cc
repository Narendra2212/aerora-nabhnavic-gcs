#include "AeroraVehicleFactGroupRegistrar.h"

#include "AeroraTelemetryState.h"
#include "Vehicle.h"

AeroraVehicleFactGroupRegistrar::AeroraVehicleFactGroupRegistrar(Vehicle *vehicle)
    : _vehicle(vehicle)
{
    if (!vehicle) {
        qCWarning(AeroraTelemetryLog) << "Constructed with no vehicle; every registration will be refused";
    }
}

AeroraVehicleFactGroupRegistrar::~AeroraVehicleFactGroupRegistrar() = default;

Vehicle *AeroraVehicleFactGroupRegistrar::vehicle() const
{
    return _vehicle;
}

bool AeroraVehicleFactGroupRegistrar::isRegistrationAvailable()
{
#ifdef AERORA_VEHICLE_FACTGROUP_FRIENDSHIP
    return true;
#else
    return false;
#endif
}

bool AeroraVehicleFactGroupRegistrar::_refuse(const QString &name, const char *reason)
{
    _refusedNames.append(name);

    qCWarning(AeroraTelemetryLog) << "Refused fact group registration"
                                  << "name:" << name
                                  << "reason:" << reason;

    return false;
}

bool AeroraVehicleFactGroupRegistrar::registerFactGroup(const QString &name, FactGroup *factGroup)
{
    if (name.isEmpty()) {
        return _refuse(name, "empty name");
    }

    if (!factGroup) {
        return _refuse(name, "null fact group");
    }

    if (!_vehicle) {
        return _refuse(name, "no vehicle");
    }

    // Checked through the public accessor, so detecting a collision needs no privilege. Worth
    // checking here rather than leaving it to _addFactGroup(), which warns and returns void - this
    // way the refusal is a value the caller can act on.
    if (_vehicle->factGroupNames().contains(name)) {
        return _refuse(name, "vehicle already has a fact group with this name");
    }

#ifdef AERORA_VEHICLE_FACTGROUP_FRIENDSHIP
    // The one privileged statement in the telemetry module. Requires
    // `friend class AeroraVehicleFactGroupRegistrar;` on Vehicle - see the class documentation.
    // Ownership stays with the AeroraTelemetrySet: _addFactGroup() stores the pointer and does not
    // reparent.
    _vehicle->_addFactGroup(factGroup, name);
    _registeredNames.append(name);

    qCDebug(AeroraTelemetryLog) << "Registered fact group on vehicle"
                                << "name:" << name
                                << "vehicleId:" << _vehicle->id();

    return true;
#else
    return _refuse(name, "AERORA_VEHICLE_FACTGROUP_FRIENDSHIP is not defined; the Q4-002 core edit "
                         "has not been applied, so Aerora fact groups cannot enter the vehicle map");
#endif
}

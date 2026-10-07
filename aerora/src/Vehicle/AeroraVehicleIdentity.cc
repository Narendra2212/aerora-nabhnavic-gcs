#include "AeroraVehicleIdentity.h"

#include "AeroraVehicleLog.h"

AeroraVehicleIdentity::AeroraVehicleIdentity(int mavlinkSystemId,
                                             int mavlinkComponentId,
                                             QObject *parent)
    : QObject(parent)
    , _mavlinkSystemId(mavlinkSystemId)
    , _mavlinkComponentId(mavlinkComponentId)
{
}

AeroraVehicleIdentity::~AeroraVehicleIdentity() = default;

bool AeroraVehicleIdentity::reportFleetIdentity(const QString &fleetIdentity)
{
    if (fleetIdentity.isEmpty()) {
        qCWarning(AeroraVehicleLog) << "Fleet identity report rejected, empty token"
                                    << "mavlinkSystemId:" << _mavlinkSystemId;
        return false;
    }

    if (_fleetIdentity == fleetIdentity) {
        return true;
    }

    if (!_fleetIdentity.isEmpty()) {
        qCWarning(AeroraVehicleLog) << "Fleet identity replaced for a live vehicle"
                                    << "mavlinkSystemId:" << _mavlinkSystemId;
    }

    _fleetIdentity = fleetIdentity;

    emit fleetIdentityChanged();

    return true;
}

void AeroraVehicleIdentity::clearFleetIdentity()
{
    if (_fleetIdentity.isEmpty()) {
        return;
    }

    _fleetIdentity.clear();

    emit fleetIdentityChanged();
}

#include "AeroraVehicleMock.h"

#include "AeroraVehicleLog.h"
#include "AeroraVehicleServices.h"

AeroraVehicleMock::AeroraVehicleMock(QObject *parent)
    : QObject(parent)
{
}

AeroraVehicleMock::~AeroraVehicleMock() = default;

void AeroraVehicleMock::discover()
{
    if (_services) {
        return;
    }

    // Null vehicle on purpose: a mock that fabricated a Vehicle would be pretending to be
    // QGroundControl, not standing in for it.
    _services = new AeroraVehicleServices(nullptr, this);

    emit vehicleDiscovered();
}

void AeroraVehicleMock::connectVehicle()
{
    if (!_services) {
        qCWarning(AeroraVehicleLog) << "Mock connect ignored, nothing discovered";
        return;
    }

    if (_connected) {
        return;
    }

    _connected = true;

    emit vehicleConnected();
}

void AeroraVehicleMock::disconnectVehicle()
{
    if (!_services) {
        qCWarning(AeroraVehicleLog) << "Mock disconnect ignored, nothing discovered";
        return;
    }

    if (!_connected) {
        return;
    }

    _connected = false;
    _services->capabilities()->clearReports();

    emit vehicleDisconnected();
}

void AeroraVehicleMock::forget()
{
    if (!_services) {
        return;
    }

    delete _services;
    _services = nullptr;
    _connected = false;

    emit vehicleForgotten();
}

bool AeroraVehicleMock::reportCapabilityAvailable(AeroraVehicleCapabilities::Category category,
                                                  const QString &key)
{
    if (!_services) {
        qCWarning(AeroraVehicleLog) << "Mock capability report ignored, nothing discovered"
                                    << "key:" << key;
        return false;
    }

    return _services->capabilities()->report(category, key, AeroraVehicleCapabilities::State::Available);
}

bool AeroraVehicleMock::reportCapabilityUnavailable(AeroraVehicleCapabilities::Category category,
                                                    const QString &key)
{
    if (!_services) {
        qCWarning(AeroraVehicleLog) << "Mock capability report ignored, nothing discovered"
                                    << "key:" << key;
        return false;
    }

    return _services->capabilities()->report(category, key, AeroraVehicleCapabilities::State::Unavailable);
}

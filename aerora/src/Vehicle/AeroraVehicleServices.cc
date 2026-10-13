#include "AeroraVehicleServices.h"

#include "AeroraProductProfile.h"
#include "AeroraVehicleCapabilities.h"
#include "AeroraVehicleIdentity.h"
#include "AeroraVehicleLog.h"
#include "Vehicle.h"

AeroraVehicleServices::AeroraVehicleServices(Vehicle *vehicle, QObject *parent)
    : QObject(parent)
    , _vehicle(vehicle)
    , _identity(new AeroraVehicleIdentity(vehicle ? vehicle->id() : 0,
                                          vehicle ? vehicle->defaultComponentId() : 0,
                                          this))
    , _capabilities(new AeroraVehicleCapabilities(this))
{
    setObjectName(QStringLiteral("AeroraVehicleServices"));

    if (_vehicle) {
        // Selection of the Aerora firmware plugin is all that is known at construction. It is a
        // build-configuration outcome, so it must not be recorded as Aerora hardware.
        _classification = Classification::SelectedBySeam;
    }

    (void) connect(_capabilities, &AeroraVehicleCapabilities::capabilityChanged,
                   this, &AeroraVehicleServices::_onCapabilityReportChanged);
    (void) connect(_capabilities, &AeroraVehicleCapabilities::reportsCleared,
                   this, &AeroraVehicleServices::_onCapabilityReportChanged);
}

AeroraVehicleServices::~AeroraVehicleServices()
{
    qCDebug(AeroraVehicleLog) << "~AeroraVehicleServices"
                              << "mavlinkSystemId:" << (_identity ? _identity->mavlinkSystemId() : 0);
}

AeroraVehicleServices *AeroraVehicleServices::attachTo(Vehicle *vehicle)
{
    if (!vehicle) {
        qCWarning(AeroraVehicleLog) << "Cannot attach Aerora vehicle services, no vehicle";
        return nullptr;
    }

    if (AeroraVehicleServices *const existing = forVehicle(vehicle)) {
        qCDebug(AeroraVehicleLog) << "Aerora vehicle services already attached"
                                  << "mavlinkSystemId:" << vehicle->id();
        return existing;
    }

    AeroraVehicleServices *const services = new AeroraVehicleServices(vehicle, vehicle);

    qCDebug(AeroraVehicleLog) << "attached Aerora vehicle services"
                              << "mavlinkSystemId:" << vehicle->id()
                              << "mavlinkComponentId:" << vehicle->defaultComponentId();

    return services;
}

AeroraVehicleServices *AeroraVehicleServices::forVehicle(Vehicle *vehicle)
{
    if (!vehicle) {
        return nullptr;
    }

    return vehicle->findChild<AeroraVehicleServices *>(QString(), Qt::FindDirectChildrenOnly);
}

const AeroraVehicleServices *AeroraVehicleServices::forVehicle(const Vehicle *vehicle)
{
    if (!vehicle) {
        return nullptr;
    }

    return vehicle->findChild<AeroraVehicleServices *>(QString(), Qt::FindDirectChildrenOnly);
}

void AeroraVehicleServices::reportAeroraCorroboration()
{
    if (_classification == Classification::NotAerora) {
        qCWarning(AeroraVehicleLog) << "Aerora corroboration contradicts an earlier NotAerora classification"
                                    << "mavlinkSystemId:" << _identity->mavlinkSystemId();
    }

    _setClassification(Classification::CorroboratedAerora);
}

void AeroraVehicleServices::reportNotAerora()
{
    if (_classification == Classification::CorroboratedAerora) {
        qCWarning(AeroraVehicleLog) << "NotAerora contradicts an earlier Aerora corroboration"
                                    << "mavlinkSystemId:" << _identity->mavlinkSystemId();
    }

    _setClassification(Classification::NotAerora);
}

QString AeroraVehicleServices::productProfileId() const
{
    return _productProfile ? _productProfile->profileId() : QString();
}

void AeroraVehicleServices::resolveProductProfile()
{
    if (_productProfile) {
        return;
    }

    AeroraProductProfile *const profile = AeroraProductProfileRegistry::instance()->resolve(*_capabilities);
    if (!profile) {
        return;
    }

    _productProfile = profile;
    profile->attachToVehicle(*this);

    qCDebug(AeroraVehicleLog) << "resolved product profile"
                              << "profileId:" << profile->profileId()
                              << "mavlinkSystemId:" << _identity->mavlinkSystemId();

    emit productProfileChanged();
}

bool AeroraVehicleServices::attachService(const QString &name, QObject *service)
{
    if (name.isEmpty()) {
        qCWarning(AeroraVehicleLog) << "Service attachment rejected, empty name";
        return false;
    }

    if (!service) {
        qCWarning(AeroraVehicleLog) << "Service attachment rejected, null service"
                                    << "name:" << name;
        return false;
    }

    if (_services.contains(name)) {
        qCWarning(AeroraVehicleLog) << "Service attachment rejected, name already attached"
                                    << "name:" << name;
        return false;
    }

    service->setParent(this);
    _services.insert(name, service);
    _serviceOrder.append(name);

    qCDebug(AeroraVehicleLog) << "attached service"
                              << "name:" << name
                              << "mavlinkSystemId:" << _identity->mavlinkSystemId();

    emit serviceAttached(name);

    return true;
}

QObject *AeroraVehicleServices::service(const QString &name) const
{
    return _services.value(name, nullptr);
}

QStringList AeroraVehicleServices::serviceNames() const
{
    return _serviceOrder;
}

void AeroraVehicleServices::_onCapabilityReportChanged()
{
    // A capability report can complete the evidence a profile needs, so resolution is retried.
    // It deliberately does not touch the classification: capability reports say what a vehicle
    // can do, never whether it is Aerora hardware.
    resolveProductProfile();
}

void AeroraVehicleServices::_setClassification(Classification classification)
{
    if (_classification == classification) {
        return;
    }

    _classification = classification;

    qCDebug(AeroraVehicleLog) << "classification changed"
                              << "classification:" << classification
                              << "mavlinkSystemId:" << _identity->mavlinkSystemId();

    emit classificationChanged(_classification);
}

#include "AeroraVehicleUiContext.h"

#include "AeroraMissionObserver.h"
#include "AeroraPayloadModel.h"
#include "AeroraPowerModel.h"
#include "AeroraUiLog.h"
#include "AeroraVehicleCapabilities.h"
#include "AeroraVehicleIdentity.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"

AeroraVehicleUiContext::AeroraVehicleUiContext(QObject *parent)
    : QObject(parent)
{
    MultiVehicleManager *const manager = MultiVehicleManager::instance();
    if (!manager) {
        // Not reachable in the application, which creates the manager before any QML loads. Guarded
        // rather than assumed so a host that constructs this object early reads as unavailable
        // instead of crashing.
        qCWarning(AeroraUiLog) << "no MultiVehicleManager: Aerora services are unreachable";
        return;
    }

    (void) connect(manager, &MultiVehicleManager::activeVehicleChanged,
                   this, &AeroraVehicleUiContext::_onActiveVehicleChanged);

    _rebind(manager->activeVehicle());
}

AeroraVehicleUiContext::~AeroraVehicleUiContext()
{
    qCDebug(AeroraUiLog) << this;
}

void AeroraVehicleUiContext::_onActiveVehicleChanged(Vehicle *vehicle)
{
    _rebind(vehicle);
}

void AeroraVehicleUiContext::_rebind(Vehicle *vehicle)
{
    if (!_services.isNull()) {
        (void) disconnect(_services, nullptr, this, nullptr);
    }

    // forVehicle() tolerates a null vehicle and returns null for one that was never composed -
    // the offline-editing vehicle is deliberately not composed, so this is a normal outcome.
    _services = vehicle ? AeroraVehicleServices::forVehicle(vehicle) : nullptr;

    if (!_services.isNull()) {
        (void) connect(_services, &AeroraVehicleServices::classificationChanged,
                       this, &AeroraVehicleUiContext::classificationChanged);
        (void) connect(_services, &AeroraVehicleServices::productProfileChanged,
                       this, &AeroraVehicleUiContext::productProfileChanged);
        // A service attached after this rebind - the composition root attaches in dependency
        // order - must still reach the interface, so the typed properties re-read.
        (void) connect(_services, &AeroraVehicleServices::serviceAttached,
                       this, &AeroraVehicleUiContext::servicesChanged);
    }

    qCDebug(AeroraUiLog) << "rebound. vehicle:" << static_cast<void *>(vehicle)
                         << "servicesAvailable:" << isAvailable()
                         << "productProfileId:" << productProfileId();

    emit servicesChanged();
    emit classificationChanged();
    emit productProfileChanged();
}

QObject *AeroraVehicleUiContext::_service(const char *serviceName) const
{
    if (_services.isNull() || !serviceName) {
        return nullptr;
    }

    return _services->service(QString::fromUtf8(serviceName));
}

AeroraVehicleIdentity *AeroraVehicleUiContext::identity() const
{
    return _services.isNull() ? nullptr : _services->identity();
}

AeroraVehicleCapabilities *AeroraVehicleUiContext::capabilities() const
{
    return _services.isNull() ? nullptr : _services->capabilities();
}

AeroraPayloadModel *AeroraVehicleUiContext::payload() const
{
    return qobject_cast<AeroraPayloadModel *>(_service(AeroraPayloadModel::kServiceName));
}

AeroraPowerModel *AeroraVehicleUiContext::power() const
{
    return qobject_cast<AeroraPowerModel *>(_service(AeroraPowerModel::kServiceName));
}

AeroraMissionObserver *AeroraVehicleUiContext::mission() const
{
    return qobject_cast<AeroraMissionObserver *>(_service(AeroraMissionObserver::kServiceName));
}

AeroraVehicleServices::Classification AeroraVehicleUiContext::classification() const
{
    return _services.isNull() ? AeroraVehicleServices::Classification::Unknown : _services->classification();
}

QString AeroraVehicleUiContext::productProfileId() const
{
    return _services.isNull() ? QString() : _services->productProfileId();
}

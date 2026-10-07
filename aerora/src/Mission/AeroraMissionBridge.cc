#include "AeroraMissionBridge.h"

#include <QtCore/QStringLiteral>

#include "AeroraMissionLog.h"
#include "AeroraMissionObserver.h"
#include "AeroraMissionSnapshot.h"
#include "AeroraVehicleServices.h"
#include "MissionManager.h"
#include "Vehicle.h"

AeroraMissionBridge::AeroraMissionBridge(MissionManager *missionManager, AeroraMissionObserver *observer)
    : QObject(observer)
    , _missionManager(missionManager)
    , _observer(observer)
{
    qCDebug(AeroraMissionLog) << this;

    connect(_missionManager, &MissionManager::inProgressChanged, this, &AeroraMissionBridge::_inProgressChanged);
    connect(_missionManager, &MissionManager::error, this, &AeroraMissionBridge::_error);
    connect(_missionManager,
            &MissionManager::newMissionItemsAvailable,
            this,
            &AeroraMissionBridge::_newMissionItemsAvailable);
    connect(_missionManager, &MissionManager::sendComplete, this, &AeroraMissionBridge::_sendComplete);
    connect(_missionManager, &MissionManager::removeAllComplete, this, &AeroraMissionBridge::_removeAllComplete);
    connect(_missionManager, &MissionManager::currentIndexChanged, this, &AeroraMissionBridge::_currentIndexChanged);
}

AeroraMissionObserver *AeroraMissionBridge::observerFor(Vehicle *vehicle)
{
    if (!vehicle) {
        return nullptr;
    }

    AeroraVehicleServices *const services = AeroraVehicleServices::forVehicle(vehicle);
    if (!services) {
        return nullptr;
    }

    return qobject_cast<AeroraMissionObserver *>(
        services->service(QString::fromUtf8(AeroraMissionObserver::kServiceName)));
}

AeroraMissionBridge *AeroraMissionBridge::attachTo(Vehicle *vehicle)
{
    if (!vehicle) {
        qCWarning(AeroraMissionLog) << "refusing to attach mission observation to a null vehicle";
        return nullptr;
    }

    MissionManager *const missionManager = vehicle->missionManager();
    if (!missionManager) {
        qCWarning(AeroraMissionLog) << "vehicle has no mission manager, mission observation not attached";
        return nullptr;
    }

    AeroraVehicleServices *const services = AeroraVehicleServices::forVehicle(vehicle);
    if (!services) {
        // The services object is attached by the Aerora firmware plugin. Creating one here would
        // take a lifecycle decision owned by the vehicle integration layer.
        qCDebug(AeroraMissionLog) << "vehicle has no Aerora vehicle services, mission observation not attached";
        return nullptr;
    }

    const QString serviceName = QString::fromUtf8(AeroraMissionObserver::kServiceName);

    if (auto *const existingObserver = qobject_cast<AeroraMissionObserver *>(services->service(serviceName))) {
        // Already attached. Return the existing bridge rather than connecting a second one, which
        // would double-report every signal into one observer.
        AeroraMissionBridge *const existingBridge = existingObserver->findChild<AeroraMissionBridge *>();
        if (!existingBridge) {
            qCWarning(AeroraMissionLog) << "mission observer is attached without a bridge, not reporting";
        }
        return existingBridge;
    }

    auto *const observer = new AeroraMissionObserver();
    if (!services->attachService(serviceName, observer)) {
        qCWarning(AeroraMissionLog) << "failed to attach mission observer as service:" << serviceName;
        delete observer;
        return nullptr;
    }

    // attachService() reparented the observer to the services object, so the bridge parented to
    // the observer is destroyed with the vehicle.
    return new AeroraMissionBridge(missionManager, observer);
}

void AeroraMissionBridge::_inProgressChanged(bool inProgress)
{
    if (inProgress) {
        _observer->reportTransferStarted();
        return;
    }

    // A transaction can finish without a completion signal: the ArduPilot guided-item write path
    // clears the transaction but deliberately skips sendComplete. Reporting idle here keeps the
    // activity state from sticking at InProgress in that case. The completion handlers below run
    // afterwards and set the same idle state, so this is not a double report.
    //
    // The latched transaction error is deliberately not cleared here. PlanManager emits its
    // error, then clears the transaction, then emits the completion signal, so the latch has to
    // survive this call to reach the handler that needs it.
    _observer->reportTransferEnded();
}

void AeroraMissionBridge::_error(int errorCode, const QString &errorMessage)
{
    _observer->reportTransferError(QStringLiteral("code=%1:%2").arg(errorCode).arg(errorMessage));
}

void AeroraMissionBridge::_newMissionItemsAvailable(bool removeAllRequested)
{
    Q_UNUSED(removeAllRequested)

    // PlanManager has already swapped its item list before emitting, so this reads the mission
    // the vehicle just reported.
    _observer->reportDownloadComplete(
        AeroraMissionSnapshot::fromMissionItems(AeroraMissionSnapshot::VehicleStore, _missionManager->missionItems()));
}

void AeroraMissionBridge::_sendComplete(bool error)
{
    // On success PlanManager has already promoted the write list into its item list, so this is
    // what the vehicle now holds. On failure the snapshot is ignored by the observer, which marks
    // the vehicle mission indeterminate instead.
    _observer->reportUploadComplete(
        error,
        AeroraMissionSnapshot::fromMissionItems(AeroraMissionSnapshot::VehicleStore, _missionManager->missionItems()));
}

void AeroraMissionBridge::_removeAllComplete(bool error)
{
    _observer->reportRemoveAllComplete(error);
}

void AeroraMissionBridge::_currentIndexChanged(int currentIndex)
{
    _observer->reportVehicleCurrentIndex(currentIndex);
}

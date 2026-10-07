#include "AeroraMissionMock.h"

#include "AeroraMissionObserver.h"

AeroraMissionMock::AeroraMissionMock(QObject *parent)
    : QObject(parent)
    , _observer(new AeroraMissionObserver(this))
{
}

AeroraMissionMock::~AeroraMissionMock() = default;

AeroraMissionSnapshot AeroraMissionMock::buildMission(AeroraMissionSnapshot::Origin origin,
                                                      const QList<MAV_CMD> &commands)
{
    AeroraMissionSnapshot mission = AeroraMissionSnapshot::empty(origin);

    int sequenceNumber = 0;
    for (const MAV_CMD command : commands) {
        AeroraMissionItemFingerprint fingerprint;
        fingerprint.sequenceNumber = sequenceNumber++;
        fingerprint.command = command;
        fingerprint.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
        fingerprint.autoContinue = true;
        mission.append(fingerprint);
    }

    return mission;
}

void AeroraMissionMock::loadPlan(const AeroraMissionSnapshot &plan)
{
    _observer->reportGcsPlan(plan);
}

void AeroraMissionMock::selectPlanViewItem(int sequenceNumber)
{
    _observer->reportPlanViewSelection(sequenceNumber);
}

void AeroraMissionMock::beginUpload(const AeroraMissionSnapshot &mission)
{
    _pendingUpload = mission;
    _observer->reportTransferStarted();
}

void AeroraMissionMock::acceptUpload()
{
    _observer->reportTransferEnded();
    _observer->reportUploadComplete(false, _pendingUpload);

    // A successful write resets the current index before the vehicle has reported anything about
    // the mission it now holds.
    _observer->reportVehicleCurrentIndex(-1);

    _pendingUpload = AeroraMissionSnapshot();
}

void AeroraMissionMock::rejectUpload(const QString &reason)
{
    _observer->reportTransferError(reason);
    _observer->reportTransferEnded();
    _observer->reportUploadComplete(true, AeroraMissionSnapshot());

    _pendingUpload = AeroraMissionSnapshot();
}

void AeroraMissionMock::beginDownload()
{
    _observer->reportTransferStarted();
}

void AeroraMissionMock::completeDownload(const AeroraMissionSnapshot &vehicleMission)
{
    _observer->reportTransferEnded();
    _observer->reportDownloadComplete(vehicleMission);
}

void AeroraMissionMock::failDownload(const QString &reason)
{
    _observer->reportTransferError(reason);
    _observer->reportTransferEnded();

    // Upstream clears the partial list and emits the same completion observation as a success.
    _observer->reportDownloadComplete(AeroraMissionSnapshot::empty(AeroraMissionSnapshot::VehicleStore));
}

void AeroraMissionMock::beginRemoveAll()
{
    _observer->reportTransferStarted();
}

void AeroraMissionMock::completeRemoveAll(bool error)
{
    _observer->reportTransferEnded();
    _observer->reportRemoveAllComplete(error);
}

void AeroraMissionMock::reportCurrentIndex(int sequenceNumber)
{
    _observer->reportVehicleCurrentIndex(sequenceNumber);
}

void AeroraMissionMock::reportResumeCandidate(int sequenceNumber)
{
    _observer->reportResumeCandidate(sequenceNumber);
}

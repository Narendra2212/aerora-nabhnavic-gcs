#include "AeroraMissionObserver.h"

#include "AeroraMissionLog.h"

AeroraMissionObserver::AeroraMissionObserver(QObject *parent)
    : QObject(parent)
{
    qCDebug(AeroraMissionLog) << this;
}

AeroraMissionObserver::~AeroraMissionObserver()
{
    qCDebug(AeroraMissionLog) << this;
}

AeroraMissionAgreement AeroraMissionObserver::verify(AeroraMissionAgreement::Depth depth)
{
    _agreement = AeroraMissionSnapshot::compare(_gcsPlan, _vehicleMission, depth);

    qCDebug(AeroraMissionLog) << "verify depth:" << AeroraMissionAgreement::depthToString(depth)
                              << "verdict:" << AeroraMissionAgreement::verdictToString(_agreement.verdict())
                              << "firstDifferenceIndex:" << _agreement.firstDifferenceIndex()
                              << "reason:" << _agreement.reason();

    emit agreementChanged();

    return _agreement;
}

void AeroraMissionObserver::reportTransferStarted()
{
    _errorDuringTransaction = false;
    _setTransferActivity(InProgress);
}

void AeroraMissionObserver::reportTransferEnded()
{
    _setTransferActivity(Idle);
}

void AeroraMissionObserver::reportTransferError(const QString &errorMessage)
{
    _errorDuringTransaction = true;

    qCWarning(AeroraMissionLog) << "mission transfer error reported:" << errorMessage;
}

void AeroraMissionObserver::reportDownloadComplete(const AeroraMissionSnapshot &vehicleMission)
{
    // A download reports no error flag of its own, so the latched error is the only evidence that
    // distinguishes a failed read from a genuinely empty mission.
    const bool failed = _errorDuringTransaction;
    _errorDuringTransaction = false;

    if (!failed) {
        // The download describes the vehicle, so it resolves any previous indeterminacy.
        _setVehicleMission(vehicleMission);
        _setVehicleMissionIndeterminate(false);
    }

    _setLastTransfer(Download, failed ? Failed : Succeeded);
    _setTransferActivity(Idle);
}

void AeroraMissionObserver::reportUploadComplete(bool error, const AeroraMissionSnapshot &vehicleMission)
{
    _errorDuringTransaction = false;

    if (error) {
        // The write list is discarded without rolling the vehicle back, and the vehicle may have
        // accepted part of the new mission. Neither the old mission nor the new one describes
        // what is on board, so the previous snapshot must not be left standing as current.
        _setVehicleMission(AeroraMissionSnapshot());
        _setVehicleMissionIndeterminate(true);
    } else {
        _setVehicleMission(vehicleMission);
        _setVehicleMissionIndeterminate(false);
    }

    _setLastTransfer(Upload, error ? Failed : Succeeded);
    _setTransferActivity(Idle);
}

void AeroraMissionObserver::reportRemoveAllComplete(bool error)
{
    _errorDuringTransaction = false;

    if (error) {
        _setVehicleMission(AeroraMissionSnapshot());
        _setVehicleMissionIndeterminate(true);
    } else {
        _setVehicleMission(AeroraMissionSnapshot::empty(AeroraMissionSnapshot::VehicleStore));
        _setVehicleMissionIndeterminate(false);
    }

    _setLastTransfer(RemoveAll, error ? Failed : Succeeded);
    _setTransferActivity(Idle);
}

void AeroraMissionObserver::reportVehicleCurrentIndex(int sequenceNumber)
{
    const AeroraMissionIndex reported = AeroraMissionIndex::fromVehicleReport(sequenceNumber);
    if (reported == _vehicleCurrentIndex) {
        return;
    }

    _vehicleCurrentIndex = reported;

    qCDebug(AeroraMissionLog) << "vehicleCurrentIndex:" << _vehicleCurrentIndex.toString();

    emit vehicleCurrentIndexChanged();
}

void AeroraMissionObserver::reportResumeCandidate(int sequenceNumber)
{
    const AeroraMissionIndex candidate = AeroraMissionIndex::resumeCandidate(sequenceNumber);
    if (candidate == _resumeCandidate) {
        return;
    }

    _resumeCandidate = candidate;
    emit resumeCandidateChanged();
}

void AeroraMissionObserver::reportGcsPlan(const AeroraMissionSnapshot &plan)
{
    if (plan.origin() != AeroraMissionSnapshot::GcsPlan) {
        qCWarning(AeroraMissionLog) << "rejecting reported plan with wrong origin:"
                                    << AeroraMissionSnapshot::originToString(plan.origin());
        return;
    }

    _gcsPlan = plan;

    // The plan changed, so any previous verdict describes a comparison that no longer applies.
    _invalidateAgreement();

    emit gcsPlanChanged();
}

void AeroraMissionObserver::reportPlanViewSelection(int sequenceNumber)
{
    const AeroraMissionIndex selection = AeroraMissionIndex::fromPlanViewSelection(sequenceNumber);
    if (selection == _planViewSelection) {
        return;
    }

    _planViewSelection = selection;
    emit planViewSelectionChanged();
}

void AeroraMissionObserver::_setTransferActivity(TransferActivity activity)
{
    if (_transferActivity == activity) {
        return;
    }

    _transferActivity = activity;
    emit transferActivityChanged();
}

void AeroraMissionObserver::_setLastTransfer(LastOperation operation, LastOutcome outcome)
{
    if ((_lastOperation == operation) && (_lastOutcome == outcome)) {
        return;
    }

    _lastOperation = operation;
    _lastOutcome = outcome;
    emit lastTransferChanged();
}

void AeroraMissionObserver::_setVehicleMissionIndeterminate(bool indeterminate)
{
    if (_vehicleMissionIndeterminate == indeterminate) {
        return;
    }

    _vehicleMissionIndeterminate = indeterminate;
    emit vehicleMissionIndeterminateChanged();
}

void AeroraMissionObserver::_setVehicleMission(const AeroraMissionSnapshot &vehicleMission)
{
    _vehicleMission = vehicleMission;

    // The vehicle side changed, so any previous verdict describes a comparison that no longer
    // applies. Leaving it would present a verified upload after the mission it verified was
    // replaced.
    _invalidateAgreement();

    emit vehicleMissionChanged();
}

void AeroraMissionObserver::_invalidateAgreement()
{
    if (_agreement.verdict() == AeroraMissionAgreement::NotCompared) {
        return;
    }

    _agreement = AeroraMissionAgreement();
    emit agreementChanged();
}

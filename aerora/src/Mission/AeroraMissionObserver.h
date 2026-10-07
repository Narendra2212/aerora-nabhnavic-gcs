#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtQmlIntegration/QtQmlIntegration>

#include "AeroraMissionIndex.h"
#include "AeroraMissionSnapshot.h"

/// Read-only Aerora view of one vehicle's mission situation.
///
/// This is the type the operator interface should bind to. It holds the three things the mission
/// architecture insists on keeping apart — the ground station's plan, the vehicle's mission, and
/// the vehicle's execution position — plus what is actually knowable about transfers between
/// them.
///
/// \section notauthority What this class cannot do
///
/// It has **no command API at all**: no upload, no download, no clear, no resume, no set-current,
/// no start, no mode change. Every method either reports an observation into it or reads state
/// out of it. That is deliberate and structural rather than a convention:
///
/// \li Mission state reporting is not aircraft safety authority. Nothing here can decide RTL,
///     LAND, HOLD, DISARM or termination, because nothing here can send anything.
/// \li Mission planning is not autonomy authority. The companion proposes, the flight controller
///     decides, the ground station observes. This class is on the observing side of that line.
///
/// Commanding stays where QGroundControl already put it: `PlanMasterController`,
/// `MissionController` and `MissionManager`. Aerora reuses those unchanged.
///
/// \section feeds Who reports what
///
/// The vehicle-side observations come from AeroraMissionBridge, which is the only Aerora mission
/// file that touches `Vehicle` or `MissionManager`. The plan-side observations
/// (reportGcsPlan(), reportPlanViewSelection()) come from whoever owns the plan, because a plan
/// belongs to a `PlanMasterController` in a view rather than to a vehicle — binding them here
/// would invent a relationship QGroundControl does not have.
///
/// \section telemetry Relationship to the telemetry state model
///
/// This class does **not** restate the telemetry value classes. REPORTED, EFFECTIVE, REQUESTED,
/// PROPOSED, STALE, UNKNOWN and UNAVAILABLE belong to AeroraTelemetryState and apply to telemetry
/// *values*. What this class models is different: whether a transfer is happening, what the last
/// one did, and whether two missions agree. No freshness verdict is reached here, and none should
/// be inferred from these states.
class AeroraMissionObserver : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Attached to a vehicle by the Aerora mission layer")

    Q_PROPERTY(AeroraMissionIndex vehicleCurrentIndex READ vehicleCurrentIndex NOTIFY vehicleCurrentIndexChanged)
    Q_PROPERTY(AeroraMissionIndex resumeCandidate READ resumeCandidate NOTIFY resumeCandidateChanged)
    Q_PROPERTY(AeroraMissionIndex planViewSelection READ planViewSelection NOTIFY planViewSelectionChanged)
    Q_PROPERTY(AeroraMissionSnapshot vehicleMission READ vehicleMission NOTIFY vehicleMissionChanged)
    Q_PROPERTY(AeroraMissionSnapshot gcsPlan READ gcsPlan NOTIFY gcsPlanChanged)
    Q_PROPERTY(AeroraMissionAgreement agreement READ agreement NOTIFY agreementChanged)
    Q_PROPERTY(TransferActivity transferActivity READ transferActivity NOTIFY transferActivityChanged)
    Q_PROPERTY(LastOperation lastOperation READ lastOperation NOTIFY lastTransferChanged)
    Q_PROPERTY(LastOutcome lastOutcome READ lastOutcome NOTIFY lastTransferChanged)
    Q_PROPERTY(bool vehicleMissionIndeterminate READ isVehicleMissionIndeterminate
                   NOTIFY vehicleMissionIndeterminateChanged)

public:
    /// The service name this observer attaches under on AeroraVehicleServices.
    static constexpr const char *kServiceName = "mission";

    /// Whether a mission transfer is in flight.
    ///
    /// There is no per-kind in-progress state, and that is a limitation of what is observable
    /// rather than a simplification. `PlanManager` keeps its transaction kind in a protected
    /// member and exposes only a boolean `inProgress()`, so an observer outside the class can
    /// tell that *something* is transferring but not what. Inventing a kind here would mean
    /// guessing.
    enum TransferActivity {
        /// Nothing has been observed. Not "idle" — idle is a positive observation.
        NotEvidencedActivity = 0,
        /// A transfer was observed to finish and none has started since.
        Idle,
        /// A transfer is in flight. Its kind is not observable while it runs.
        InProgress,
    };
    Q_ENUM(TransferActivity)

    /// Which operation the last completed transfer was.
    ///
    /// Unlike the in-flight kind, this *is* observable: each operation has its own completion
    /// signal on `PlanManager`.
    enum LastOperation {
        NotEvidencedOperation = 0,
        Download,
        Upload,
        RemoveAll,
    };
    Q_ENUM(LastOperation)

    /// How the last completed transfer ended.
    ///
    /// \warning For a **download**, Failed is only reachable when an error was also reported
    /// during the transaction. `PlanManager::_finishTransaction()` emits
    /// `newMissionItemsAvailable(false)` on both success and failure, with no error flag, and
    /// clears the item list on failure — so a failed download and a genuinely empty mission are
    /// otherwise indistinguishable from outside the class. The error correlation is the best
    /// available evidence, and where it is absent this reports Succeeded with an empty mission
    /// rather than guessing.
    enum LastOutcome {
        NotEvidencedOutcome = 0,
        Succeeded,
        Failed,
    };
    Q_ENUM(LastOutcome)

    explicit AeroraMissionObserver(QObject *parent = nullptr);
    ~AeroraMissionObserver() override;

    // ---------------------------------------------------------------------------------------
    // Observations out
    // ---------------------------------------------------------------------------------------

    /// The execution position the aircraft reports. Origin::VehicleReported once anything has
    /// been reported; unattributed before that.
    ///
    /// Attributed-but-unknown is a real and important state here: a successful mission write
    /// resets this to "no current index" before the vehicle has said anything about the new
    /// mission. It must not be rendered as item 0.
    [[nodiscard]] AeroraMissionIndex vehicleCurrentIndex() const { return _vehicleCurrentIndex; }

    /// The ground station's proposal for where a resume would start. Never authoritative.
    [[nodiscard]] AeroraMissionIndex resumeCandidate() const { return _resumeCandidate; }

    /// The operator's editor selection. Never the aircraft's position.
    [[nodiscard]] AeroraMissionIndex planViewSelection() const { return _planViewSelection; }

    /// The mission as the vehicle last reported it.
    [[nodiscard]] AeroraMissionSnapshot vehicleMission() const { return _vehicleMission; }

    /// The ground station plan, flattened as an upload would flatten it.
    [[nodiscard]] AeroraMissionSnapshot gcsPlan() const { return _gcsPlan; }

    /// The latest comparison result. Verdict::NotCompared until verify() is called, and reset to
    /// NotCompared whenever either side changes, so a stale verdict can never be presented as
    /// current. An upload that has not been verified is therefore presented as unverified, which
    /// is what `AERORA-MISSION-REQ-007` requires.
    [[nodiscard]] AeroraMissionAgreement agreement() const { return _agreement; }

    [[nodiscard]] TransferActivity transferActivity() const { return _transferActivity; }
    [[nodiscard]] LastOperation lastOperation() const { return _lastOperation; }
    [[nodiscard]] LastOutcome lastOutcome() const { return _lastOutcome; }

    /// true when the vehicle's mission is known to be in a state no plan describes.
    ///
    /// Set by a failed upload. `PlanManager::_finishTransaction()` discards the ground station's
    /// write list on failure and does **not** roll the vehicle back, and the vehicle may already
    /// have accepted part of the new mission, so neither the previous mission nor the new one
    /// describes what is on board. Cleared only by a completed download or a successful upload.
    [[nodiscard]] bool isVehicleMissionIndeterminate() const { return _vehicleMissionIndeterminate; }

    /// Compares gcsPlan() against vehicleMission() at @a depth and latches the verdict.
    ///
    /// @return The verdict, which is also available afterwards from agreement().
    [[nodiscard]] Q_INVOKABLE AeroraMissionAgreement verify(AeroraMissionAgreement::Depth depth);

    // ---------------------------------------------------------------------------------------
    // Observations in — vehicle side, reported by AeroraMissionBridge
    // ---------------------------------------------------------------------------------------

    /// A transfer started. The kind is not reported because it is not observable.
    void reportTransferStarted();

    /// A transfer is no longer in flight.
    ///
    /// Reported separately from the completion observations because a transaction can end without
    /// one: the ArduPilot guided-item write path clears the transaction and deliberately skips
    /// `sendComplete`. This leaves lastOperation() and lastOutcome() untouched, and does not clear
    /// the latched transaction error, because the completion observation that needs the latch is
    /// emitted after the transaction has already been cleared.
    void reportTransferEnded();

    /// An error was reported during the current transaction.
    ///
    /// Latched for the duration of the transaction so that a completion handler with no error
    /// flag of its own can still reach the right outcome.
    void reportTransferError(const QString &errorMessage);

    /// A mission download completed, yielding @a vehicleMission.
    void reportDownloadComplete(const AeroraMissionSnapshot &vehicleMission);

    /// A mission upload completed. On success @a vehicleMission is what the vehicle now holds;
    /// on failure it is ignored and the vehicle mission becomes indeterminate.
    void reportUploadComplete(bool error, const AeroraMissionSnapshot &vehicleMission);

    /// A remove-all completed.
    void reportRemoveAllComplete(bool error);

    /// The aircraft reported its execution position. @a sequenceNumber may be negative for
    /// "no current index".
    void reportVehicleCurrentIndex(int sequenceNumber);

    /// The ground station recomputed where a resume would start.
    void reportResumeCandidate(int sequenceNumber);

    // ---------------------------------------------------------------------------------------
    // Observations in — plan side, reported by whoever owns the plan
    // ---------------------------------------------------------------------------------------

    /// The flattened ground-station plan changed. Rejects a snapshot that is not
    /// AeroraMissionSnapshot::GcsPlan.
    void reportGcsPlan(const AeroraMissionSnapshot &plan);

    /// The operator's editor selection changed.
    void reportPlanViewSelection(int sequenceNumber);

signals:
    void vehicleCurrentIndexChanged();
    void resumeCandidateChanged();
    void planViewSelectionChanged();
    void vehicleMissionChanged();
    void gcsPlanChanged();
    void agreementChanged();
    void transferActivityChanged();
    void lastTransferChanged();
    void vehicleMissionIndeterminateChanged();

private:
    void _setTransferActivity(TransferActivity activity);
    void _setLastTransfer(LastOperation operation, LastOutcome outcome);
    void _setVehicleMissionIndeterminate(bool indeterminate);
    void _setVehicleMission(const AeroraMissionSnapshot &vehicleMission);
    void _invalidateAgreement();

    AeroraMissionIndex _vehicleCurrentIndex;
    AeroraMissionIndex _resumeCandidate;
    AeroraMissionIndex _planViewSelection;
    AeroraMissionSnapshot _vehicleMission;
    AeroraMissionSnapshot _gcsPlan;
    AeroraMissionAgreement _agreement;
    TransferActivity _transferActivity = NotEvidencedActivity;
    LastOperation _lastOperation = NotEvidencedOperation;
    LastOutcome _lastOutcome = NotEvidencedOutcome;
    bool _vehicleMissionIndeterminate = false;
    bool _errorDuringTransaction = false;
};

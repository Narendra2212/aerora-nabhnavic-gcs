#pragma once

#ifndef QGC_UNITTEST_BUILD
#error "AeroraMissionMock is MOCK / TEST ONLY and must not be compiled into a shipped build"
#endif

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>

#include "AeroraMissionSnapshot.h"
#include "QGCMAVLink.h"

class AeroraMissionObserver;

/// **MOCK / TEST ONLY**
///
/// Deterministic driver for the Aerora mission observation surface, so mission behaviour can be
/// exercised without hardware, without a MockLink connection, without a `Vehicle` and without a
/// `MissionManager`.
///
/// It is compiled only when `QGC_BUILD_TESTING` is on, and the guard at the top of this header
/// fails the build if it reaches a shipped configuration.
///
/// \section scenarios Scenarios it drives
///
/// Empty mission, plan loaded, upload requested, upload accepted, upload rejected, mission
/// replaced, current index changed, mission download, download failure, mission mismatch, and the
/// observable states around a resume. Each is a direct state change on the observer, mirroring
/// the order the real `MissionManager` signals arrive in.
///
/// \section notdone What it does not do
///
/// \li **It does not model an aircraft.** No position, attitude, airspeed, battery, flight mode,
///     arming state or link quality. None of those is needed to exercise mission observation, and
///     a plausible-looking invented value is worse than no value.
/// \li **It does not generate a resume mission.** Building the resume item set is
///     `MissionManager::generateResumeMission()`'s job, it has no seam, and Aerora does not
///     reimplement or modify it. What this mock drives is the observable state around a resume —
///     the candidate index, the replacement, and the index reset — not the generation.
/// \li **It models no payload.** No pump, nozzle, actuator or payload state, and no payload
///     replay. Payload state persistence across a mission interruption is `NOT-EVIDENCED`, so
///     there is nothing here to simulate and simulating it would manufacture the evidence.
/// \li **It has no timers and no randomness.** Every method is a direct state change, so a given
///     call sequence always produces the same observable sequence.
class AeroraMissionMock : public QObject
{
    Q_OBJECT

public:
    explicit AeroraMissionMock(QObject *parent = nullptr);
    ~AeroraMissionMock() override;

    /// @return The observer this mock drives. Never null, and owned by the mock.
    [[nodiscard]] AeroraMissionObserver *observer() const { return _observer; }

    /// Builds a deterministic flattened mission from @a commands, numbering items from 0.
    ///
    /// Every parameter is zero and the frame is `MAV_FRAME_GLOBAL_RELATIVE_ALT`, which is the most
    /// common real frame. No coordinate, altitude, speed or geometry is invented: no Aerora
    /// mission geometry is evidenced (Q-021, Q-023), and a test that needs varied parameters
    /// should append its own fingerprints rather than have them guessed here.
    [[nodiscard]] static AeroraMissionSnapshot buildMission(AeroraMissionSnapshot::Origin origin,
                                                            const QList<MAV_CMD> &commands);

    // ---------------------------------------------------------------------------------------
    // Plan side
    // ---------------------------------------------------------------------------------------

    /// Reports @a plan as the ground station's flattened plan.
    void loadPlan(const AeroraMissionSnapshot &plan);

    /// Reports an operator selection in the plan editor.
    void selectPlanViewItem(int sequenceNumber);

    // ---------------------------------------------------------------------------------------
    // Vehicle side
    // ---------------------------------------------------------------------------------------

    /// Starts an upload of @a mission. Held until acceptUpload() or rejectUpload().
    void beginUpload(const AeroraMissionSnapshot &mission);

    /// The vehicle accepted the upload.
    ///
    /// Mirrors the verified completion order: the vehicle mission becomes the uploaded mission,
    /// and the current index is reset to "no current index" before the vehicle has reported
    /// anything about the new mission.
    void acceptUpload();

    /// The vehicle rejected the upload, or the transaction failed.
    ///
    /// Mirrors the verified failure path: the ground station's write list is discarded with no
    /// rollback, so the vehicle mission becomes indeterminate rather than reverting.
    void rejectUpload(const QString &reason);

    /// Starts a mission download.
    void beginDownload();

    /// The download completed, yielding @a vehicleMission.
    void completeDownload(const AeroraMissionSnapshot &vehicleMission);

    /// The download failed.
    ///
    /// Reports an error and then the same completion observation the success path uses, because
    /// that is what upstream does: the download completion carries no error flag, so the error is
    /// the only thing that distinguishes failure from an empty mission.
    void failDownload(const QString &reason);

    /// Starts a remove-all.
    void beginRemoveAll();

    /// The remove-all completed.
    void completeRemoveAll(bool error = false);

    /// The aircraft reported its execution position. Negative means "no current index".
    void reportCurrentIndex(int sequenceNumber);

    /// The ground station recomputed where a resume would start.
    void reportResumeCandidate(int sequenceNumber);

private:
    AeroraMissionObserver *const _observer = nullptr;
    AeroraMissionSnapshot _pendingUpload;
};

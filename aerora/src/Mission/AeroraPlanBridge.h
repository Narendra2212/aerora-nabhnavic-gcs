#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>

class AeroraMissionObserver;
class MissionController;
class PlanMasterController;

/// Feeds the plan side of AeroraMissionObserver from QGroundControl's plan controllers.
///
/// The counterpart to AeroraMissionBridge. That class reports what the *vehicle* says, from
/// `MissionManager` signals; this one reports what the *ground station* holds, from
/// `PlanMasterController` / `MissionController`. Together they populate the two sides
/// `AeroraMissionSnapshot::compare()` requires, which is what makes `AERORA-MISSION-REQ-007`
/// answerable at all: until this class existed, `gcsPlan()` was never captured and every
/// comparison returned `NotCompared` regardless of what the operator had planned.
///
/// \section why Why a second bridge rather than an extension of the first
///
/// A plan does not belong to a vehicle. It belongs to a `PlanMasterController` owned by a view,
/// and QGroundControl runs two of them for the whole session — one per view, with different
/// meanings. AeroraMissionBridge is per-vehicle and created from
/// `FirmwarePlugin::initializeVehicle()`; it has no route to either controller and should not
/// acquire one. So the lifetimes are different, the owners are different, and they are separate
/// classes.
///
/// \section roles The two roles, and why they are not interchangeable
///
/// The role follows the controller's `flyView` property, and getting it wrong is the worst
/// available failure, so it is read from the controller rather than passed in:
///
/// \li **`flyView == false`** — the Plan view's controller. This is the operator's editing buffer
///     and the *only* legitimate source of `AeroraMissionSnapshot::GcsPlan`. It also owns
///     `currentPlanViewSeqNum`: every `setCurrentPlanViewSeqNum()` call site in the tree is Plan
///     view code, and `MissionController::start()` seeds it only when `!_flyView`.
/// \li **`flyView == true`** — the Fly view's controller. This one is a *reconstruction of the
///     vehicle's own mission* (`_newMissionItemsAvailableFromVehicle` accepts every download
///     unconditionally when `_flyView`), so reporting it as the ground-station plan would compare
///     the vehicle against itself. It is used for one thing only: `resumeMissionIndex()`, which
///     returns 0 unconditionally on a Plan view controller and is therefore unobtainable anywhere
///     else.
///
/// \section nocommand No command path
///
/// Nothing here writes a mission, uploads, downloads, clears, resumes or sets a current item, and
/// nothing here is reachable from QML — `AeroraUiSafetyTest` holds the observer's `report*()`
/// methods off the QML surface, and this class is plain C++ with no QML registration at all. It
/// reads two controllers and reports into an observer.
class AeroraPlanBridge : public QObject
{
    Q_OBJECT

public:
    /// Which side of the plan/vehicle boundary a controller speaks for. Derived from the
    /// controller's own `flyView` property, never supplied by the caller.
    enum class Role
    {
        /// The Plan view's controller: reports the ground-station plan and the editor selection.
        GcsPlan,
        /// The Fly view's controller: reports the resume candidate only.
        ResumeCandidate,
    };

    /// Attaches plan observation to @a controller.
    ///
    /// Idempotent: a controller that already has a bridge keeps it and that instance is returned,
    /// so a repeated attach cannot double-report into one observer.
    ///
    /// @return The bridge, or nullptr when @a controller is null or has no mission controller.
    static AeroraPlanBridge* attachTo(PlanMasterController* controller);

    /// Finds the session plan controllers under @a sceneRoot and attaches one bridge to each.
    ///
    /// This is the discovery seam. `PlanMasterController` is `QML_ELEMENT` with no C++ factory,
    /// registry or singleton, so a pointer to one is reachable only by walking the QML object
    /// tree. Called from `AeroraCorePlugin::createQmlApplicationEngine()` via
    /// `QQmlApplicationEngine::objectCreated`, which is the first moment the Plan view's and Fly
    /// view's controllers exist — both are instantiated eagerly by the main window rather than
    /// through a `Loader`, so neither is deferred.
    ///
    /// At most one bridge per role is attached. The per-vehicle controllers that the Fly view map
    /// and the 3D view create inside a `Repeater` are deliberately out of scope: they are not
    /// session-lifetime, and the vehicle list is empty at this point, so they do not exist yet.
    /// A scene that somehow offers a second controller for a role is logged and ignored rather
    /// than allowed to overwrite the first, because silently binding the wrong controller to
    /// Role::GcsPlan would make the observer compare the vehicle against itself.
    ///
    /// @return The number of bridges attached.
    static int attachToQmlScene(QObject* sceneRoot);

    /// @return The bridge attached to @a controller, or nullptr when there is none.
    static AeroraPlanBridge* bridgeFor(PlanMasterController* controller);

    /// @return The role @a controller speaks for, read from its own `flyView` property.
    ///
    /// Public and static because it is the single decision that makes the difference between
    /// reporting the operator's plan and reporting the vehicle's own mission back at itself, and
    /// it is worth asserting directly rather than only through its effects.
    [[nodiscard]] static Role roleFor(const PlanMasterController* controller);

    /// Translates `MissionController::resumeMissionIndex()` into the sequence number to report.
    ///
    /// @return @a resumeMissionIndex when it is a usable candidate, or -1 when there is none.
    ///
    /// `resumeMissionIndex()` documents -1 for "not available" and never returns it: it either
    /// decrements a value it has already established is greater than 1, or assigns 0 outright. So
    /// the real contract is 0 means unavailable and a usable candidate is always >= 1. Reporting 0
    /// verbatim would make `AeroraMissionIndex::isKnown()` true at item 0 and the interface would
    /// show a resume proposal the ground station never computed; -1 reports
    /// attributed-but-unknown, which is "computed, and there is none".
    [[nodiscard]] static int resumeSequenceNumberFor(int resumeMissionIndex);

    [[nodiscard]] Role role() const { return _role; }

    /// @return The controller this bridge reads, or nullptr once it has been destroyed.
    ///
    /// Defined out of line deliberately. Returning the pointer inline would instantiate
    /// `QPointer<PlanMasterController>::operator PlanMasterController*()` here, which needs the
    /// pointee complete, and this header only forward-declares it. Same failure `52380853b` fixed
    /// for the telemetry and vehicle headers.
    [[nodiscard]] PlanMasterController* controller() const;

    /// Reports the current state of this bridge's role immediately, bypassing the coalescing
    /// timer. Used on attach and on a vehicle change, and by tests that do not want to spin an
    /// event loop for a zero-delay timer.
    void reportNow();

private slots:
    /// Coalesces a burst of change signals into one report. See \section coalesce in the
    /// implementation for why reporting synchronously is wrong.
    void _scheduleReport();

    void _managerVehicleChanged();

private:
    AeroraPlanBridge(PlanMasterController* controller, MissionController* missionController, Role role);

    /// Resolves the observer for the controller's current manager vehicle, or nullptr.
    ///
    /// Re-resolved on every report rather than cached: the observer is attached to a `Vehicle` and
    /// dies with it, while this bridge outlives every vehicle. nullptr is the normal case, not an
    /// error - no vehicle connected, a vehicle not routed to the Aerora firmware plugin, or the
    /// offline editing vehicle, which the composition root deliberately does not compose.
    [[nodiscard]] AeroraMissionObserver* _observer() const;

    void _reportGcsPlan(AeroraMissionObserver* observer);
    void _reportPlanViewSelection(AeroraMissionObserver* observer);
    void _reportResumeCandidate(AeroraMissionObserver* observer);

    QPointer<PlanMasterController> _controller;
    QPointer<MissionController> _missionController;
    const Role _role;
    QTimer _reportTimer;
};

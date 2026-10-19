#include "AeroraPlanBridge.h"

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QVariant>

#include "AeroraMissionBridge.h"
#include "AeroraMissionLog.h"
#include "AeroraMissionObserver.h"
#include "AeroraMissionSnapshot.h"
#include "MissionController.h"
#include "MissionItem.h"
#include "PlanMasterController.h"

namespace {

[[nodiscard]] const char* roleToString(AeroraPlanBridge::Role role)
{
    return (role == AeroraPlanBridge::Role::GcsPlan) ? "gcsPlan" : "resumeCandidate";
}

}  // namespace

AeroraPlanBridge::AeroraPlanBridge(PlanMasterController* controller, MissionController* missionController, Role role)
    : QObject(controller)
    , _controller(controller)
    , _missionController(missionController)
    , _role(role)
{
    qCDebug(AeroraMissionLog) << this << "role:" << roleToString(role);

    // \section coalesce Why the report is deferred by one event-loop turn
    //
    // Every trigger below can fire while the model is still being mutated. `dirtyChanged` comes
    // from QmlObjectListModel's own dirty flag, which an edit sets part-way through a sequence
    // that also appends items and recalculates sequence numbers; snapshotting synchronously would
    // capture the plan mid-edit and report it as current. A zero-interval single-shot timer runs
    // after the current call stack unwinds, so one settled snapshot is reported per burst instead
    // of several partial ones. MissionController uses its own _updateTimer for the same reason.
    _reportTimer.setSingleShot(true);
    _reportTimer.setInterval(0);
    connect(&_reportTimer, &QTimer::timeout, this, &AeroraPlanBridge::reportNow);

    connect(controller, &PlanMasterController::managerVehicleChanged, this, &AeroraPlanBridge::_managerVehicleChanged);

    if (_role == Role::GcsPlan) {
        // Three triggers, because no single signal means "the plan content changed":
        // dirtyChanged is an edge on a latch, visualItemsReset covers wholesale rebuilds, and
        // newItemsFromVehicle covers an editor populated by a download.
        connect(missionController, &MissionController::dirtyChanged, this, &AeroraPlanBridge::_scheduleReport);
        connect(missionController, &MissionController::visualItemsReset, this, &AeroraPlanBridge::_scheduleReport);
        connect(missionController, &MissionController::newItemsFromVehicle, this, &AeroraPlanBridge::_scheduleReport);
        // Aggregated by upstream: one emit covers currentPlanViewSeqNum and the rest of the
        // plan-view state group.
        connect(missionController, &MissionController::planViewStateChanged, this, &AeroraPlanBridge::_scheduleReport);
    } else {
        connect(missionController, &MissionController::resumeMissionIndexChanged, this,
                &AeroraPlanBridge::_scheduleReport);
    }
}

AeroraPlanBridge::Role AeroraPlanBridge::roleFor(const PlanMasterController* controller)
{
    if (!controller) {
        // The conservative direction. GcsPlan is the role that can misreport the vehicle's own
        // mission as the operator's plan, so an unreadable controller must not land there.
        return Role::ResumeCandidate;
    }

    // Through the meta-object because `flyView` is declared
    // `Q_PROPERTY(bool flyView MEMBER _flyView)` and has no getter. Adding one would be a core
    // edit for something already readable.
    return controller->property("flyView").toBool() ? Role::ResumeCandidate : Role::GcsPlan;
}

int AeroraPlanBridge::resumeSequenceNumberFor(int resumeMissionIndex)
{
    return (resumeMissionIndex >= 1) ? resumeMissionIndex : -1;
}

PlanMasterController* AeroraPlanBridge::controller() const
{
    return _controller;
}

AeroraPlanBridge* AeroraPlanBridge::bridgeFor(PlanMasterController* controller)
{
    if (!controller) {
        return nullptr;
    }

    return controller->findChild<AeroraPlanBridge*>(QString(), Qt::FindDirectChildrenOnly);
}

AeroraPlanBridge* AeroraPlanBridge::attachTo(PlanMasterController* controller)
{
    if (!controller) {
        qCWarning(AeroraMissionLog) << "refusing to attach plan observation to a null controller";
        return nullptr;
    }

    if (AeroraPlanBridge* const existing = bridgeFor(controller)) {
        // Already attached. Returning the existing bridge rather than connecting a second one,
        // which would report every change twice into one observer.
        return existing;
    }

    MissionController* const missionController = controller->missionController();
    if (!missionController) {
        qCWarning(AeroraMissionLog) << "plan controller has no mission controller, plan observation not attached";
        return nullptr;
    }

    auto* const bridge = new AeroraPlanBridge(controller, missionController, roleFor(controller));

    // The controller may already hold a plan and already be bound to a vehicle by the time this
    // runs, so the initial state is reported rather than waiting for the next change.
    bridge->reportNow();

    return bridge;
}

int AeroraPlanBridge::attachToQmlScene(QObject* sceneRoot)
{
    if (!sceneRoot) {
        qCWarning(AeroraMissionLog) << "refusing to attach plan observation to a null scene root";
        return 0;
    }

    const QList<PlanMasterController*> controllers = sceneRoot->findChildren<PlanMasterController*>();
    if (controllers.isEmpty()) {
        qCWarning(AeroraMissionLog) << "no plan controller found in the QML scene, plan observation not attached."
                                    << "gcsPlan stays uncaptured and every mission comparison stays NotCompared.";
        return 0;
    }

    bool gcsPlanTaken = false;
    bool resumeTaken = false;
    int attached = 0;

    for (PlanMasterController* const controller : controllers) {
        const Role role = roleFor(controller);
        bool& taken = (role == Role::GcsPlan) ? gcsPlanTaken : resumeTaken;

        if (taken) {
            // Not an overwrite. Binding the wrong controller to Role::GcsPlan would report the
            // Fly view's reconstruction of the vehicle's own mission as the ground-station plan,
            // and the observer would then be comparing the vehicle against itself.
            qCWarning(AeroraMissionLog) << "ignoring an additional plan controller for role" << roleToString(role)
                                        << ": one is already attached";
            continue;
        }

        if (attachTo(controller)) {
            taken = true;
            attached++;
        }
    }

    if (!gcsPlanTaken) {
        qCWarning(AeroraMissionLog) << "no Plan view controller found, so the ground-station plan is never captured"
                                    << "and every mission comparison stays NotCompared.";
    }

    qCDebug(AeroraMissionLog) << "plan observation attached. bridges:" << attached << "gcsPlan:" << gcsPlanTaken
                              << "resumeCandidate:" << resumeTaken;

    return attached;
}

AeroraMissionObserver* AeroraPlanBridge::_observer() const
{
    if (_controller.isNull()) {
        return nullptr;
    }

    return AeroraMissionBridge::observerFor(_controller->managerVehicle());
}

void AeroraPlanBridge::_scheduleReport()
{
    _reportTimer.start();
}

void AeroraPlanBridge::_managerVehicleChanged()
{
    // The observer resolved from here on belongs to a different vehicle and has never been told
    // anything about this controller, so the current state is reported immediately rather than
    // left until the next edit. Deferring would leave a freshly connected vehicle's observer
    // showing an uncaptured plan while the editor plainly holds one.
    reportNow();

    // Then once more on the next event-loop turn, because the two attachments race. The observer
    // is created by AeroraMissionBridge::attachTo() from
    // AeroraFirmwarePlugin::initializeVehicle(), which runs while the Vehicle is being built and
    // so normally precedes this signal - but nothing enforces that order. If the observer did not
    // exist a moment ago, reportNow() above found nothing and returned, and without this retry the
    // plan would read uncaptured until the operator happened to edit it. Exactly one retry: this is
    // the only place that schedules without a change having occurred, so it cannot spin.
    _reportTimer.start();
}

void AeroraPlanBridge::reportNow()
{
    _reportTimer.stop();

    if (_controller.isNull() || _missionController.isNull()) {
        return;
    }

    if (!_missionController->visualItems()) {
        // The controller has not been started, so its mission controller holds no item model and
        // no mission manager. resumeMissionIndex() would dereference the latter. Nothing to
        // report yet either way - start() emits the signals that bring us back here.
        return;
    }

    AeroraMissionObserver* const observer = _observer();
    if (!observer) {
        // The normal case, not an error: no vehicle connected, a vehicle that resolved to a
        // different firmware plugin, or the offline editing vehicle. Offline there is no vehicle
        // mission to compare against either, so nothing is lost by having nowhere to report.
        return;
    }

    if (_role == Role::GcsPlan) {
        _reportGcsPlan(observer);
        _reportPlanViewSelection(observer);
    } else {
        _reportResumeCandidate(observer);
    }
}

void AeroraPlanBridge::_reportGcsPlan(AeroraMissionObserver* observer)
{
    // Parents the flattened items so they are released when this scope ends. The snapshot copies
    // the fields it needs, so nothing outlives the call.
    QObject itemParent;
    QList<MissionItem*> items;

    // The upload path's own conversion, through the narrow accessor added for this, rather than a
    // reimplementation. A second flattener would drift from the real one and the comparison would
    // then report differences the vehicle cannot see - the one failure AeroraMissionSnapshot
    // exists to prevent.
    _missionController->flattenForUpload(items, &itemParent);

    observer->reportGcsPlan(AeroraMissionSnapshot::fromMissionItems(AeroraMissionSnapshot::GcsPlan, items));
}

void AeroraPlanBridge::_reportPlanViewSelection(AeroraMissionObserver* observer)
{
    observer->reportPlanViewSelection(_missionController->currentPlanViewSeqNum());
}

void AeroraPlanBridge::_reportResumeCandidate(AeroraMissionObserver* observer)
{
    const int resumeIndex = _missionController->resumeMissionIndex();

    // \section sentinel Why 0 is translated and not forwarded
    //
    // MissionController::resumeMissionIndex() documents -1 for "resume not available" and never
    // returns it. The implementation either decrements a value it has already established is
    // greater than 1, or assigns 0 outright, so the real contract is: 0 means unavailable, and a
    // usable candidate is always >= 1. Forwarding 0 verbatim would make AeroraMissionIndex report
    // isKnown() == true at item 0 and the operator interface would show a resume proposal the
    // ground station never computed. -1 is reported instead, which is attributed-but-unknown: the
    // ground station did compute, and there is no candidate.
    observer->reportResumeCandidate(resumeSequenceNumberFor(resumeIndex));
}

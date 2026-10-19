#include "AeroraPlanBridgeTest.h"

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtPositioning/QGeoCoordinate>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "AeroraMissionBridge.h"
#include "AeroraMissionIndex.h"
#include "AeroraMissionObserver.h"
#include "AeroraMissionSnapshot.h"
#include "AeroraPlanBridge.h"
#include "MissionController.h"
#include "MissionItem.h"
#include "PlanMasterController.h"
#include "QmlObjectListModel.h"

namespace {

/// Exact logging category of AeroraPlanBridge, from AeroraMissionLog.cc. expectLogMessage()
/// matches the category literally, so a mismatch here is a silent test failure rather than a
/// compile error.
constexpr const char* kLogCategory = "Aerora.Mission";

/// Somewhere valid and unremarkable. Nothing here asserts on position.
const QGeoCoordinate kWaypoint(47.6, -122.3, 50.0);

}  // namespace

void AeroraPlanBridgeTest::init()
{
    VehicleTest::init();

    // Both warnings come from the vehicle connect inside VehicleTest::init() above, which has
    // already returned by the time this line runs, so expectLogMessage() cannot match them - it
    // records the captured-message count as its start index. Same reasoning and same pair as
    // AeroraMissionSyncTest::init(); see the longer note there.
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("allowing commands without authorization")));
    ignoreLogMessage("ComponentInformation.RequestMetaDataTypeStateMachine", QtWarningMsg,
                     QRegularExpression(QStringLiteral("failed to load metadata")));
}

void AeroraPlanBridgeTest::cleanup()
{
    // Before VehicleTest::cleanup() disconnects the vehicle: a controller outliving its manager
    // vehicle would rebind to its offline vehicle and report again during teardown.
    const QList<PlanMasterController*> controllers = findChildren<PlanMasterController*>();
    for (PlanMasterController* const controller : controllers) {
        delete controller;
    }

    VehicleTest::cleanup();
}

PlanMasterController* AeroraPlanBridgeTest::_makeController(bool flyView)
{
    auto* const controller = new PlanMasterController(this);
    controller->setFlyView(flyView);
    controller->start();
    return controller;
}

void AeroraPlanBridgeTest::_resumeSentinelNeverManufacturesACandidate_data()
{
    QTest::addColumn<int>("resumeMissionIndex");
    QTest::addColumn<int>("expectedSequenceNumber");

    // The case this test exists for. resumeMissionIndex() returns 0 for "no resume available" and
    // never the -1 its header documents, so forwarding it verbatim would report item 0 as a
    // proposal the ground station never computed.
    QTest::newRow("unavailable is zero, not minus one") << 0 << -1;

    // Defensive: the documented sentinel, and anything else below 1, must land in the same place
    // rather than becoming a sequence number.
    QTest::newRow("documented sentinel") << -1 << -1;
    QTest::newRow("unexpected negative") << -7 << -1;

    // A usable candidate is always >= 1, because the only non-zero path decrements a value already
    // established to be greater than 1.
    QTest::newRow("first usable candidate") << 1 << 1;
    QTest::newRow("mid mission") << 5 << 5;
}

void AeroraPlanBridgeTest::_resumeSentinelNeverManufacturesACandidate()
{
    QFETCH(int, resumeMissionIndex);
    QFETCH(int, expectedSequenceNumber);

    QCOMPARE(AeroraPlanBridge::resumeSequenceNumberFor(resumeMissionIndex), expectedSequenceNumber);

    // The translation is only half the contract. What matters to the interface is that the
    // reported index renders as "none proposed" rather than as item 0, which is a property of
    // AeroraMissionIndex rather than of the number.
    const AeroraMissionIndex index =
        AeroraMissionIndex::resumeCandidate(AeroraPlanBridge::resumeSequenceNumberFor(resumeMissionIndex));

    QVERIFY2(index.isAttributed(), "a computed resume answer is always attributed, even when empty");
    QCOMPARE(index.isKnown(), expectedSequenceNumber >= 0);
    QVERIFY2(!index.isVehicleAuthoritative(), "a resume candidate is never the aircraft's position");
}

void AeroraPlanBridgeTest::_roleFollowsTheControllerFlyViewFlag_data()
{
    QTest::addColumn<bool>("flyView");
    QTest::addColumn<int>("expectedRole");

    QTest::newRow("plan view reports the plan") << false << static_cast<int>(AeroraPlanBridge::Role::GcsPlan);
    QTest::newRow("fly view reports the resume candidate")
        << true << static_cast<int>(AeroraPlanBridge::Role::ResumeCandidate);
}

void AeroraPlanBridgeTest::_roleFollowsTheControllerFlyViewFlag()
{
    QFETCH(bool, flyView);
    QFETCH(int, expectedRole);

    auto* const controller = new PlanMasterController(this);
    controller->setFlyView(flyView);

    // Read from the controller, not supplied by the caller. Binding the Fly view's controller to
    // Role::GcsPlan would report the vehicle's own mission back as the operator's plan, and the
    // observer would then be comparing the vehicle against itself.
    QCOMPARE(static_cast<int>(AeroraPlanBridge::roleFor(controller)), expectedRole);

    delete controller;
}

void AeroraPlanBridgeTest::_roleForNullControllerIsNotGcsPlan()
{
    // Fails towards the role that cannot misreport. Asserted because the safe direction is the
    // silent one: a null controller landing in GcsPlan would be invisible until a comparison lied.
    QCOMPARE(AeroraPlanBridge::roleFor(nullptr), AeroraPlanBridge::Role::ResumeCandidate);
}

void AeroraPlanBridgeTest::_attachToNullControllerIsRefused()
{
    expectLogMessage(kLogCategory, QtWarningMsg, QRegularExpression(QStringLiteral("null controller")));
    QVERIFY(AeroraPlanBridge::attachTo(nullptr) == nullptr);
    verifyExpectedLogMessage();

    QVERIFY(AeroraPlanBridge::bridgeFor(nullptr) == nullptr);
}

void AeroraPlanBridgeTest::_attachIsIdempotentAndNeverDoubleConnects()
{
    PlanMasterController* const controller = _makeController(false /* flyView */);

    AeroraPlanBridge* const first = AeroraPlanBridge::attachTo(controller);
    QVERIFY(first);
    QCOMPARE(first->role(), AeroraPlanBridge::Role::GcsPlan);
    QCOMPARE(first->controller(), controller);

    // The property that matters: a second attach must not connect a second bridge, because two
    // bridges on one controller would report every edit twice into one observer.
    QCOMPARE(AeroraPlanBridge::attachTo(controller), first);
    QCOMPARE(AeroraPlanBridge::bridgeFor(controller), first);
    QCOMPARE(static_cast<int>(controller->findChildren<AeroraPlanBridge*>().size()), 1);
}

void AeroraPlanBridgeTest::_bridgeIsDestroyedWithItsController()
{
    PlanMasterController* const controller = _makeController(false /* flyView */);

    QPointer<AeroraPlanBridge> bridge = AeroraPlanBridge::attachTo(controller);
    QVERIFY(!bridge.isNull());

    delete controller;

    // Parented to the controller, so there is no teardown code and no window in which the bridge
    // reads a destroyed controller.
    QVERIFY2(bridge.isNull(), "the bridge must not outlive the controller it reads");
}

void AeroraPlanBridgeTest::_planViewReportsTheFlattenedPlanAndTheSelection()
{
    QVERIFY(vehicle());

    AeroraMissionObserver* const observer = AeroraMissionBridge::observerFor(vehicle());
    QVERIFY2(observer, "the Aerora firmware plugin should have attached a mission observer");

    PlanMasterController* const controller = _makeController(false /* flyView */);
    QCOMPARE(controller->managerVehicle(), vehicle());

    MissionController* const missionController = controller->missionController();
    QVERIFY(missionController);

    // A deterministic plan of our own rather than whatever the vehicle happened to hold.
    controller->removeAll();
    QVERIFY(missionController->insertSimpleMissionItem(kWaypoint, -1 /* append */));

    QVERIFY(AeroraPlanBridge::attachTo(controller));

    const int visualItemCount = missionController->visualItems()->count();
    QVERIFY2(visualItemCount > 1, "the plan must hold more than the settings item for this to assert anything");

    // Captured, and attributed to the ground-station side. "Captured and empty" and "absent" are
    // different facts and the snapshot keeps them apart, so this asserts the origin as well.
    const AeroraMissionSnapshot plan = observer->gcsPlan();
    QVERIFY2(plan.isCaptured(), "the plan side must be captured once a Plan view controller is attached");
    QCOMPARE(plan.origin(), AeroraMissionSnapshot::GcsPlan);

    // The same flattening an upload performs, asserted against the production algorithm rather
    // than against a hand-counted number: a second flattener in this test would drift exactly the
    // way a second flattener in the bridge would.
    QObject itemParent;
    QList<MissionItem*> expectedItems;
    missionController->flattenForUpload(expectedItems, &itemParent);
    QCOMPARE(plan.itemCount(), static_cast<int>(expectedItems.size()));
    QVERIFY2(plan.itemCount() > 0, "a plan holding a waypoint must flatten to at least one wire item");

    // The editor selection, which is the operator's cursor and never the aircraft's position.
    missionController->setCurrentPlanViewSeqNum(1, true /* force */);
    AeroraPlanBridge::bridgeFor(controller)->reportNow();

    const AeroraMissionIndex selection = observer->planViewSelection();
    QCOMPARE(selection.sequenceNumber(), missionController->currentPlanViewSeqNum());
    QVERIFY(selection.isAttributed());
    QVERIFY2(!selection.isVehicleAuthoritative(), "an editor selection must never read as the aircraft's position");
}

void AeroraPlanBridgeTest::_flattenedPlanMatchesTheUploadPathForASettingsOnlyPlan()
{
    PlanMasterController* const controller = _makeController(false /* flyView */);
    MissionController* const missionController = controller->missionController();

    controller->removeAll();
    QCOMPARE(missionController->visualItems()->count(), 1);

    QObject itemParent;
    QList<MissionItem*> items;
    missionController->flattenForUpload(items, &itemParent);

    // sendToVehicle() uploads an empty model when only the MissionSettingsItem is present, so a
    // bogus home position is never written. The captured plan has to say the same thing, or the
    // comparison would find a difference the vehicle cannot see.
    QVERIFY2(items.isEmpty(), "a settings-item-only plan must flatten to nothing, as the upload path sends nothing");
}

void AeroraPlanBridgeTest::_flyViewReportsNoResumeCandidateWhileNoMissionIsFlown()
{
    QVERIFY(vehicle());

    AeroraMissionObserver* const observer = AeroraMissionBridge::observerFor(vehicle());
    QVERIFY(observer);

    PlanMasterController* const controller = _makeController(true /* flyView */);
    QVERIFY(AeroraPlanBridge::attachTo(controller));

    // No mission has been flown, so resumeMissionIndex() is 0. The reported candidate must be
    // attributed - the ground station did compute an answer - and unknown, not item 0.
    const AeroraMissionIndex candidate = observer->resumeCandidate();
    QCOMPARE(controller->missionController()->resumeMissionIndex(), 0);
    QVERIFY2(candidate.isAttributed(), "the ground station computed an answer, so the index is attributed");
    QVERIFY2(!candidate.isKnown(), "item 0 must never be presented as a resume proposal");
    QCOMPARE(candidate.sequenceNumber(), -1);
}

void AeroraPlanBridgeTest::_repeatedIdenticalReportsAreNotRepublished()
{
    QVERIFY(vehicle());

    AeroraMissionObserver* const observer = AeroraMissionBridge::observerFor(vehicle());
    QVERIFY(observer);

    PlanMasterController* const controller = _makeController(true /* flyView */);
    AeroraPlanBridge* const bridge = AeroraPlanBridge::attachTo(controller);
    QVERIFY(bridge);

    QSignalSpy spy(observer, &AeroraMissionObserver::resumeCandidateChanged);
    QVERIFY(spy.isValid());

    bridge->reportNow();
    bridge->reportNow();

    // The observer drops a report equal to what it already holds, so the coalescing in the bridge
    // is an optimisation rather than the thing preventing churn. Asserted because a binding that
    // re-evaluated on every unchanged report would be a live-view performance defect that no
    // functional test would catch.
    QVERIFY2(spy.isEmpty(), "an unchanged resume candidate must not re-notify");
}

void AeroraPlanBridgeTest::_sceneDiscoveryAttachesOneBridgePerRole()
{
    QObject sceneRoot;

    auto* const planViewController = new PlanMasterController(&sceneRoot);
    planViewController->setFlyView(false);

    auto* const flyViewController = new PlanMasterController(&sceneRoot);
    flyViewController->setFlyView(true);

    // Deliberately not started. Discovery runs at QQmlApplicationEngine::objectCreated, and this
    // asserts that attachment itself does not require a started controller - reportNow() returns
    // early until the controller has a model, and start() brings it back.
    QCOMPARE(AeroraPlanBridge::attachToQmlScene(&sceneRoot), 2);

    AeroraPlanBridge* const planBridge = AeroraPlanBridge::bridgeFor(planViewController);
    AeroraPlanBridge* const flyBridge = AeroraPlanBridge::bridgeFor(flyViewController);
    QVERIFY(planBridge);
    QVERIFY(flyBridge);
    QCOMPARE(planBridge->role(), AeroraPlanBridge::Role::GcsPlan);
    QCOMPARE(flyBridge->role(), AeroraPlanBridge::Role::ResumeCandidate);
}

void AeroraPlanBridgeTest::_sceneDiscoveryIgnoresASecondControllerForATakenRole()
{
    QObject sceneRoot;

    auto* const planViewController = new PlanMasterController(&sceneRoot);
    planViewController->setFlyView(false);

    auto* const flyViewController = new PlanMasterController(&sceneRoot);
    flyViewController->setFlyView(true);

    // Stands in for the per-vehicle controllers the Fly view map and the 3D view create inside a
    // Repeater. They are not session-lifetime and must not displace the Fly view's own.
    auto* const perVehicleController = new PlanMasterController(&sceneRoot);
    perVehicleController->setFlyView(true);

    expectLogMessage(kLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("ignoring an additional plan controller")));
    QCOMPARE(AeroraPlanBridge::attachToQmlScene(&sceneRoot), 2);
    verifyExpectedLogMessage();

    // Ignored, not preferred: the first controller found for a role keeps it. Overwriting would be
    // the silent failure, because a displaced GcsPlan role is a comparison against the vehicle
    // itself.
    QVERIFY(AeroraPlanBridge::bridgeFor(planViewController));
    QVERIFY(AeroraPlanBridge::bridgeFor(flyViewController));
    QVERIFY2(AeroraPlanBridge::bridgeFor(perVehicleController) == nullptr,
             "a third controller for a taken role must be left unattached");
}

UT_REGISTER_TEST(AeroraPlanBridgeTest, TestLabel::Integration, TestLabel::Vehicle, TestLabel::MissionManager)

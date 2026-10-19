#include "AeroraMissionProgressUITest.h"

#include <memory>

#include <QtCore/QFile>
#include <QtCore/QList>
#include <QtCore/QRegularExpression>
#include <QtCore/QScopeGuard>
#include <QtCore/QUrl>
#include <QtCore/QVariant>
#include <QtCore/QVariantList>
#include <QtPositioning/QGeoCoordinate>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtTest/QTest>

#include "AeroraMissionBridge.h"
#include "AeroraMissionIndex.h"
#include "AeroraMissionObserver.h"
#include "AeroraMissionSnapshot.h"
#include "AeroraPlanBridge.h"
#include "MissionController.h"
#include "MockLink.h"
#include "MultiVehicleManager.h"
#include "PlanMasterController.h"
#include "QGCCorePlugin.h"
#include "QmlComponentInfo.h"
#include "Vehicle.h"

UT_REGISTER_TEST(AeroraMissionProgressUITest, TestLabel::Integration)

namespace {

/// The resource URL AeroraCorePlugin::analyzePages() registers. Flat under the module prefix
/// because qgc_set_qt_resource_alias() aliases every AERORA_UI_QML_FILES entry to its bare name.
/// Spelled out rather than read from the plugin so a silent change to either breaks this test.
constexpr const char* kPageUrl = "qrc:/qml/Aerora/Controls/AeroraMissionProgressPage.qml";

/// Same path as a resource, for existence checking. A registered URL that resolves to nothing is
/// a menu entry that opens an empty page, which is the failure analyzePages() cannot report.
constexpr const char* kPageResource = ":/qml/Aerora/Controls/AeroraMissionProgressPage.qml";

/// Somewhere valid and unremarkable. Nothing here asserts on position.
const QGeoCoordinate kWaypoint(47.6, -122.3, 50.0);

}  // namespace

void AeroraMissionProgressUITest::init()
{
    QmlUITestBase::init();

    // Suppressed rather than expected, for the same reason AeroraMissionSyncTest suppresses the
    // first of these: both are emitted by application startup, which has already happened by the
    // time a test slot runs, and expectLogMessage() only matches messages captured after the call
    // that registers it.
    //
    // Both are also latched once per process - _warnedUnconfigured for the first, the key-store
    // install for the second - so only whichever slot runs first could observe them, and asserting
    // them would be asserting something absent for every later slot.
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("allowing commands without authorization")));
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("no platform key protector available")));
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("command authorization is NOT being enforced")));

    // Emitted on every generic-autopilot connect: the generic MockLink serves no component
    // metadata. Upstream suppresses the same warning at its own generic-autopilot call sites.
    ignoreLogMessage("ComponentInformation.RequestMetaDataTypeStateMachine", QtWarningMsg,
                     QRegularExpression(QStringLiteral("failed to load metadata")));
}

PlanMasterController* AeroraMissionProgressUITest::_sceneController(bool flyView)
{
    // Searched from the window rather than its content item, because the window is what
    // QQmlApplicationEngine::objectCreated hands the production hook. Using the same root keeps
    // this test's view of the scene identical to the one discovery actually sees.
    if (!_window) {
        return nullptr;
    }

    const QList<PlanMasterController*> controllers = _window->findChildren<PlanMasterController*>();
    for (PlanMasterController* const controller : controllers) {
        if (controller->property("flyView").toBool() == flyView) {
            return controller;
        }
    }

    return nullptr;
}

void AeroraMissionProgressUITest::_planBridgeAttachesToTheLiveSceneControllers()
{
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    // Both are instantiated eagerly by the main window rather than through a Loader, so both exist
    // by the time objectCreated fires. If either were deferred, discovery would silently find one
    // controller and the plan side would never be captured.
    PlanMasterController* const planViewController = _sceneController(false /* flyView */);
    PlanMasterController* const flyViewController = _sceneController(true /* flyView */);
    QVERIFY2(planViewController, "the booted scene must contain a Plan view PlanMasterController");
    QVERIFY2(flyViewController, "the booted scene must contain a Fly view PlanMasterController");

    // The assertion this suite exists for: nobody in the test called attachTo(). If a bridge is
    // here, AeroraCorePlugin's objectCreated hook ran against the real scene.
    AeroraPlanBridge* const planBridge = AeroraPlanBridge::bridgeFor(planViewController);
    AeroraPlanBridge* const flyBridge = AeroraPlanBridge::bridgeFor(flyViewController);
    QVERIFY2(planBridge, "AeroraCorePlugin must attach a plan bridge to the Plan view controller");
    QVERIFY2(flyBridge, "AeroraCorePlugin must attach a plan bridge to the Fly view controller");

    // Roles the right way round. Reversed, the Fly view's reconstruction of the vehicle's own
    // mission would be reported as the operator's plan and the observer would compare the vehicle
    // against itself.
    QCOMPARE(planBridge->role(), AeroraPlanBridge::Role::GcsPlan);
    QCOMPARE(flyBridge->role(), AeroraPlanBridge::Role::ResumeCandidate);

    QCOMPARE(planBridge->controller(), planViewController);
    QCOMPARE(flyBridge->controller(), flyViewController);
}

void AeroraMissionProgressUITest::_missionProgressPageIsRegisteredAndInstantiates()
{
    // Registration first, which needs no engine. A page absent from analyzePages() is unreachable
    // no matter how well it loads.
    const QVariantList pages = QGCCorePlugin::instance()->analyzePages();
    bool registered = false;
    for (const QVariant& page : pages) {
        const auto* const info = page.value<QmlComponentInfo*>();
        if (info && (info->url().toString() == QString::fromUtf8(kPageUrl))) {
            registered = true;
            break;
        }
    }
    QVERIFY2(registered, "AeroraMissionProgressPage must be registered in analyzePages()");

    // A registered URL pointing at nothing would open an empty Analyze page, which is exactly the
    // failure the registry cannot detect for itself.
    QVERIFY2(QFile::exists(QString::fromUtf8(kPageResource)),
             "the registered URL must resolve to a compiled-in resource");

    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    // Loaded on the application's own engine, so the import paths, the resource alias and the
    // Aerora QML module are the real ones. This is the first assertion in the suite that resolves
    // Aerora types by QML *type name* rather than through a context property: AnalyzePage,
    // AeroraUi, AeroraMissionObserver and AeroraSelfReportRow all have to be in scope for the
    // document to compile, which is the Q7-001 question.
    QQmlComponent component(_engine, QUrl(QString::fromUtf8(kPageUrl)));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    const std::unique_ptr<QObject> page(component.create());
    QVERIFY2(page != nullptr, qPrintable(component.errorString()));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    // AnalyzePage roots on an Item, so a successful create() that is not a QQuickItem would mean
    // the page changed shape under us.
    QVERIFY(qobject_cast<QQuickItem*>(page.get()));
}

void AeroraMissionProgressUITest::_missionProgressPageReportsNoObserverWithNoVehicle()
{
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    // No MockLink in this function, so no vehicle, so AeroraUi.mission is null and every row must
    // read as unattached. This is the state the interface is in for most of its life.
    QVERIFY2(AeroraMissionBridge::observerFor(nullptr) == nullptr, "no vehicle means no observer");

    QQmlComponent component(_engine, QUrl(QString::fromUtf8(kPageUrl)));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    const std::unique_ptr<QObject> page(component.create());
    QVERIFY2(page != nullptr, qPrintable(component.errorString()));

    // Read back through the page's own property rather than recomputing the condition: this is the
    // value every row's `unchecked` binding is driven from, so it is the page's actual reading.
    const QVariant attached = page->property("_attached");
    QVERIFY2(attached.isValid(), "the page must expose its attachment reading");
    QVERIFY2(!attached.toBool(), "with no vehicle the page must report no mission observer");

    // And the agreement row must not read as reassurance in that state either.
    const QVariant agreementMatched = page->property("_agreementMatched");
    QVERIFY2(agreementMatched.isValid(), "the page must expose its agreement reading");
    QVERIFY2(!agreementMatched.toBool(), "an unattached page must never report an agreement match");
}

void AeroraMissionProgressUITest::_repeatedSceneDiscoveryCreatesNoDuplicateBridges()
{
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    PlanMasterController* const planViewController = _sceneController(false /* flyView */);
    QVERIFY(planViewController);
    QCOMPARE(planViewController->findChildren<AeroraPlanBridge*>().size(), qsizetype(1));

    AeroraPlanBridge* const before = AeroraPlanBridge::bridgeFor(planViewController);
    QVERIFY(before);

    // objectCreated is not single-shot - an engine may load more than one root - so discovery has
    // to be safe to repeat. A second bridge on one controller would report every edit twice into
    // one observer. Re-run against the same root the production hook used.
    const int attached = AeroraPlanBridge::attachToQmlScene(_window);
    QVERIFY2(attached >= 2, "repeat discovery must still find both roles");

    QCOMPARE(AeroraPlanBridge::bridgeFor(planViewController), before);
    QCOMPARE(planViewController->findChildren<AeroraPlanBridge*>().size(), qsizetype(1));
}

void AeroraMissionProgressUITest::_livePlanAndSelectionChangesReachTheObserver()
{
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    // Generic rather than PX4-flavoured: the observer is reached through AeroraVehicleServices,
    // which only exists on a vehicle routed to the Aerora firmware plugin. A PX4 MockLink would
    // resolve to a different plugin and the observer would be null, asserting nothing.
    MockLink* const mockLink = MockLink::startGenericMockLink();
    QVERIFY(mockLink);
    const auto disconnectLink = qScopeGuard([mockLink] { mockLink->disconnect(); });

    Vehicle* vehicle = nullptr;
    QVERIFY(UnitTest::waitForCondition(
        [&vehicle]() {
            vehicle = MultiVehicleManager::instance()->activeVehicle();
            return (vehicle != nullptr) && (AeroraMissionBridge::observerFor(vehicle) != nullptr);
        },
        TestTimeout::longMs(), QStringLiteral("an Aerora vehicle with a mission observer")));

    AeroraMissionObserver* const observer = AeroraMissionBridge::observerFor(vehicle);
    QVERIFY(observer);

    PlanMasterController* const planViewController = _sceneController(false /* flyView */);
    QVERIFY(planViewController);

    // The controller tracks the global active vehicle, so it must have rebound to the MockLink
    // vehicle. Without this the bridge would be reporting into a different vehicle's observer.
    QVERIFY(UnitTest::waitForCondition(
        [planViewController, vehicle]() { return planViewController->managerVehicle() == vehicle; },
        TestTimeout::mediumMs(), QStringLiteral("the Plan view controller to rebind")));

    MissionController* const missionController = planViewController->missionController();
    QVERIFY(missionController);

    // A deterministic plan of our own rather than whatever the vehicle happened to hold.
    planViewController->removeAll();
    QVERIFY(missionController->insertSimpleMissionItem(kWaypoint, -1 /* append */));

    // Reported through the bridge's coalescing timer, so this waits rather than reading straight
    // away: a synchronous read would be asserting the timer's interval, not the behaviour.
    QVERIFY(UnitTest::waitForCondition([observer]() { return observer->gcsPlan().isCaptured(); },
                                       TestTimeout::mediumMs(), QStringLiteral("the plan to reach the observer")));

    const AeroraMissionSnapshot plan = observer->gcsPlan();
    QCOMPARE(plan.origin(), AeroraMissionSnapshot::GcsPlan);
    QVERIFY2(plan.itemCount() > 0, "a plan holding a waypoint must flatten to at least one wire item");

    // Selection, which is the operator's cursor and never the aircraft's position.
    missionController->setCurrentPlanViewSeqNum(1, true /* force */);
    QVERIFY(UnitTest::waitForCondition([observer]() { return observer->planViewSelection().isKnown(); },
                                       TestTimeout::mediumMs(), QStringLiteral("the selection to reach the observer")));

    const AeroraMissionIndex selection = observer->planViewSelection();
    QCOMPARE(selection.sequenceNumber(), missionController->currentPlanViewSeqNum());
    QVERIFY2(!selection.isVehicleAuthoritative(), "an editor selection must never read as the aircraft's position");

    // No mission is being flown, so the Fly view bridge must report no candidate - and must not
    // present item 0 as one.
    const AeroraMissionIndex candidate = observer->resumeCandidate();
    if (candidate.isAttributed()) {
        QVERIFY2(!candidate.isKnown() || (candidate.sequenceNumber() >= 1),
                 "item 0 must never be presented as a resume proposal");
    }

    // The page, loaded against this live state, must now read as attached.
    QQmlComponent component(_engine, QUrl(QString::fromUtf8(kPageUrl)));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));
    const std::unique_ptr<QObject> page(component.create());
    QVERIFY2(page != nullptr, qPrintable(component.errorString()));
    QVERIFY2(page->property("_attached").toBool(),
             "with an Aerora vehicle connected the page must report an attached observer");

    // Still no verdict: nothing in this build calls verify(), and the page must not imply one.
    QVERIFY2(!page->property("_agreementMatched").toBool(),
             "no comparison has been performed, so the page must report no agreement match");
}

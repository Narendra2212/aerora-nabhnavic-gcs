#include "AeroraMissionProgressUITest.h"

#include <memory>

#include <QtCore/QFile>
#include <QtCore/QList>
#include <QtCore/QRegularExpression>
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
#include "PlanMasterController.h"
#include "QGCCorePlugin.h"
#include "QmlComponentInfo.h"
#include "Vehicle.h"

UT_REGISTER_TEST(AeroraMissionProgressUITest, TestLabel::Integration, TestLabel::Vehicle)

namespace {

/// The resource URL AeroraCorePlugin::analyzePages() registers. Flat under the module prefix
/// because qgc_set_qt_resource_alias() aliases every AERORA_UI_QML_FILES entry to its bare name.
/// Spelled out rather than read from the plugin so a silent change to either breaks this test.
constexpr const char* kPageUrl = "qrc:/qml/Aerora/Controls/AeroraMissionProgressPage.qml";

/// The same path as a resource, for existence checking. A registered URL that resolves to nothing
/// is a menu entry opening an empty page, which is the one failure analyzePages() cannot report.
constexpr const char* kPageResource = ":/qml/Aerora/Controls/AeroraMissionProgressPage.qml";

/// Somewhere valid and unremarkable. Nothing here asserts on position.
const QGeoCoordinate kWaypoint(47.6, -122.3, 50.0);

}  // namespace

void AeroraMissionProgressUITest::init()
{
    VehicleTest::init();

    // Both warnings are emitted by the vehicle connect inside VehicleTest::init() above, which has
    // already returned by the time this line runs, so expectLogMessage() cannot match them - it
    // records the captured-message count as its start index. Same pair and same reasoning as
    // AeroraMissionSyncTest::init(); the longer note lives there.
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("allowing commands without authorization")));
    ignoreLogMessage("ComponentInformation.RequestMetaDataTypeStateMachine", QtWarningMsg,
                     QRegularExpression(QStringLiteral("failed to load metadata")));
}

void AeroraMissionProgressUITest::cleanup()
{
    // Through the production teardown hook, which releases the Aerora QML override interceptor.
    // Deleting the engine directly would leave it installed and the next slot's install would be
    // refused as a duplicate.
    if (_engine) {
        QGCCorePlugin::instance()->destroyQmlApplicationEngine(_engine);
        _engine = nullptr;
    }

    VehicleTest::cleanup();
}

QQmlApplicationEngine* AeroraMissionProgressUITest::_applicationEngine()
{
    if (!_engine) {
        // The production factory rather than a bare QQmlEngine: it is what adds the qrc:/qml
        // import path the Aerora module resolves from, and it is the AeroraCorePlugin override, so
        // the engine under test is configured exactly as the application's is.
        _engine = QGCCorePlugin::instance()->createQmlApplicationEngine(this);
    }

    return _engine;
}

void AeroraMissionProgressUITest::_missionProgressPageIsRegisteredWithAResolvableUrl()
{
    // Registration first, which needs no engine: a page absent from analyzePages() is unreachable
    // no matter how well its document loads.
    const QVariantList pages = QGCCorePlugin::instance()->analyzePages();
    QVERIFY2(!pages.isEmpty(), "analyzePages() must publish at least the Aerora screens");

    bool registered = false;
    for (const QVariant& page : pages) {
        const auto* const info = page.value<QmlComponentInfo*>();
        if (info && (info->url().toString() == QString::fromUtf8(kPageUrl))) {
            registered = true;

            // Vehicle-independent on purpose: the page reports whether an observer is attached at
            // all, so "no vehicle is connected" is one of its readings. Gating it on a vehicle
            // would hide the screen in the state an operator most needs to read it.
            QVERIFY2(!info->requiresVehicle(), "the mission progress page must not be vehicle-gated");
            break;
        }
    }

    QVERIFY2(registered, "AeroraMissionProgressPage must be registered in analyzePages()");

    QVERIFY2(QFile::exists(QString::fromUtf8(kPageResource)),
             "the registered URL must resolve to a compiled-in resource");
}

void AeroraMissionProgressUITest::_missionProgressPageInstantiatesOnAnApplicationEngine()
{
    QQmlApplicationEngine* const engine = _applicationEngine();
    QVERIFY(engine);

    QQmlComponent component(engine, QUrl(QString::fromUtf8(kPageUrl)));

    // The assertion this suite exists for. Everything the page needs has to resolve for the
    // document to compile: the resource alias, `import QGroundControl.AnalyzeView`, and the
    // Aerora types by QML *type name* - AeroraUi, AeroraMissionObserver, AeroraSelfReportRow.
    // A context property, which is how every earlier test reached the observer, would prove none
    // of that.
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    const std::unique_ptr<QObject> page(component.create());
    QVERIFY2(page != nullptr, qPrintable(component.errorString()));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    // AnalyzePage roots on an Item. A create() that returned something else would mean the page
    // changed shape under us, which is how an Analyze entry silently stops rendering.
    QVERIFY(qobject_cast<QQuickItem*>(page.get()));

    // Read through the page's own properties rather than recomputing the conditions: these are the
    // values every row's binding is driven from, so they are the page's actual readings. An
    // Aerora vehicle is connected by VehicleTest::init(), so the observer is attached.
    const QVariant attached = page->property("_attached");
    QVERIFY2(attached.isValid(), "the page must expose its attachment reading");
    QVERIFY2(attached.toBool(), "with an Aerora vehicle connected the page must report an observer");

    // And it must still reach no verdict. Nothing in this build calls verify(), so a true reading
    // here would mean the page had invented one.
    const QVariant agreementMatched = page->property("_agreementMatched");
    QVERIFY2(agreementMatched.isValid(), "the page must expose its agreement reading");
    QVERIFY2(!agreementMatched.toBool(), "no comparison has been performed, so there is no agreement match");
}

void AeroraMissionProgressUITest::_livePlanAndSelectionReachTheObserverAndThePage()
{
    QVERIFY(vehicle());

    AeroraMissionObserver* const observer = AeroraMissionBridge::observerFor(vehicle());
    QVERIFY2(observer, "the Aerora firmware plugin should have attached a mission observer");

    // A real controller, started and bound to the connected vehicle, with a bridge attached the
    // way AeroraCorePlugin attaches one. The application does this from the QML scene; the scene
    // is not booted here, so the attachment is made explicitly against the same API.
    auto* const controller = new PlanMasterController(this);
    controller->setFlyView(false);
    controller->start();
    QCOMPARE(controller->managerVehicle(), vehicle());

    AeroraPlanBridge* const bridge = AeroraPlanBridge::attachTo(controller);
    QVERIFY(bridge);
    QCOMPARE(bridge->role(), AeroraPlanBridge::Role::GcsPlan);

    MissionController* const missionController = controller->missionController();
    QVERIFY(missionController);

    // A deterministic plan of our own rather than whatever the vehicle happened to hold.
    controller->removeAll();
    QVERIFY(missionController->insertSimpleMissionItem(kWaypoint, -1 /* append */));

    // Reported through the bridge's coalescing timer, so this waits rather than reading straight
    // away: a synchronous read would be asserting the timer's interval instead of the behaviour.
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

    // No mission is being flown, so there is no resume candidate - and item 0 must not be offered
    // as one. resumeMissionIndex() returns 0 for "unavailable", which is why this matters.
    const AeroraMissionIndex candidate = observer->resumeCandidate();
    QVERIFY2(!candidate.isKnown() || (candidate.sequenceNumber() >= 1),
             "item 0 must never be presented as a resume proposal");

    // Finally the page, loaded against this live state: its plan row must now read as captured.
    QQmlApplicationEngine* const engine = _applicationEngine();
    QVERIFY(engine);

    QQmlComponent component(engine, QUrl(QString::fromUtf8(kPageUrl)));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    const std::unique_ptr<QObject> page(component.create());
    QVERIFY2(page != nullptr, qPrintable(component.errorString()));
    QVERIFY2(page->property("_attached").toBool(), "the page must report the attached observer");

    // Captured, not empty. Read back through AeroraUi the way the row's binding does, so this
    // asserts the route the page uses rather than the observer pointer the test already holds.
    QVERIFY2(!page->property("_agreementMatched").toBool(),
             "a captured plan is still not a verified one while nothing calls verify()");

    delete controller;
}

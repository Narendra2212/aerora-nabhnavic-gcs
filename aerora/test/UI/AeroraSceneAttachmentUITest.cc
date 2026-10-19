#include "AeroraSceneAttachmentUITest.h"

#include <QtCore/QList>
#include <QtCore/QRegularExpression>
#include <QtCore/QVariant>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest/QTest>

#include "AeroraPlanBridge.h"
#include "PlanMasterController.h"

UT_REGISTER_TEST(AeroraSceneAttachmentUITest, TestLabel::Integration)

namespace {

/// The Analyze entry's button objectName, which upstream builds as
/// `analyzeButton_<QmlComponentInfo title>`. The title comes from
/// AeroraCorePlugin::analyzePages(), so a rename there breaks this and should.
constexpr const char* kAnalyzeEntryButton = "analyzeButton_Aerora Mission Progress";

/// The page root's objectName. Clicking the entry proves the menu item exists; finding this proves
/// the click loaded the page rather than silently loading nothing.
constexpr const char* kPageObjectName = "aeroraMissionProgressPage";

}  // namespace

void AeroraSceneAttachmentUITest::init()
{
    QmlUITestBase::init();

    // Emitted once per process by AeroraCorePlugin::init() and latched there, so only whichever
    // slot runs first could observe it - asserting it would be asserting something absent for
    // every later slot. Same reasoning as AeroraMissionSyncTest::init().
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("no platform key protector available")));
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("command authorization is NOT being enforced")));
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("allowing commands without authorization")));
}

PlanMasterController* AeroraSceneAttachmentUITest::_sceneController(bool flyView)
{
    // Searched from the window, because the window is what QQmlApplicationEngine::objectCreated
    // hands the production hook. Using the same root keeps this test's view of the scene identical
    // to the one discovery actually sees.
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

void AeroraSceneAttachmentUITest::_planBridgeAttachesToTheSceneControllers()
{
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    // Both are instantiated eagerly by the main window rather than through a Loader, so both exist
    // by the time objectCreated fires. If either were deferred, discovery would find one
    // controller and the plan side would never be captured.
    PlanMasterController* const planViewController = _sceneController(false /* flyView */);
    PlanMasterController* const flyViewController = _sceneController(true /* flyView */);
    QVERIFY2(planViewController, "the booted scene must contain a Plan view PlanMasterController");
    QVERIFY2(flyViewController, "the booted scene must contain a Fly view PlanMasterController");

    // The assertion this suite exists for: nothing in this test called attachTo(). A bridge being
    // here means AeroraCorePlugin's objectCreated hook ran against the real scene.
    AeroraPlanBridge* const planBridge = AeroraPlanBridge::bridgeFor(planViewController);
    AeroraPlanBridge* const flyBridge = AeroraPlanBridge::bridgeFor(flyViewController);
    QVERIFY2(planBridge, "AeroraCorePlugin must attach a plan bridge to the Plan view controller");
    QVERIFY2(flyBridge, "AeroraCorePlugin must attach a plan bridge to the Fly view controller");

    // Roles the right way round. Reversed, the Fly view's reconstruction of the vehicle's own
    // mission would be reported as the operator's plan and the observer would be comparing the
    // vehicle against itself - the one failure the role split exists to prevent.
    QCOMPARE(planBridge->role(), AeroraPlanBridge::Role::GcsPlan);
    QCOMPARE(flyBridge->role(), AeroraPlanBridge::Role::ResumeCandidate);

    QCOMPARE(planBridge->controller(), planViewController);
    QCOMPARE(flyBridge->controller(), flyViewController);
}

void AeroraSceneAttachmentUITest::_repeatedSceneDiscoveryCreatesNoDuplicateBridges()
{
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    PlanMasterController* const planViewController = _sceneController(false /* flyView */);
    QVERIFY(planViewController);

    AeroraPlanBridge* const before = AeroraPlanBridge::bridgeFor(planViewController);
    QVERIFY(before);
    QCOMPARE(planViewController->findChildren<AeroraPlanBridge*>().size(), qsizetype(1));

    // objectCreated is not single-shot - an engine may load more than one root - so discovery has
    // to be safe to repeat. A second bridge on one controller would report every edit twice into
    // one observer. Re-run against the same root the production hook used.
    QVERIFY2(AeroraPlanBridge::attachToQmlScene(_window) >= 2, "repeat discovery must still find both roles");

    QCOMPARE(AeroraPlanBridge::bridgeFor(planViewController), before);
    QCOMPARE(planViewController->findChildren<AeroraPlanBridge*>().size(), qsizetype(1));
}

void AeroraSceneAttachmentUITest::_missionProgressPageIsReachableFromTheAnalyzeMenu()
{
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    // The operator's actual route. Registration in analyzePages() is asserted by
    // AeroraMissionProgressUITest; what it cannot tell is whether the entry becomes a button a
    // person can reach, which is a property of the Analyze view consuming the registry.
    QVERIFY2(clickToolSelectDropdownButton(QStringLiteral("toolbar_viewAnalyze")),
             "the Analyze view must be reachable from the tool-select dropdown");

    QVERIFY2(clickButton(QString::fromUtf8(kAnalyzeEntryButton)),
             "the Aerora Mission Progress entry must appear in the Analyze page list");

    // Clicking is not loading. Without this the test would pass against an entry that opens an
    // empty pane, which is exactly the failure analyzePages() cannot report for itself.
    QQuickItem* const page = findVisibleItem(_rootItem, QString::fromUtf8(kPageObjectName), TestTimeout::mediumMs());
    QVERIFY2(page, "clicking the Aerora entry must load the mission progress page");

    // Loaded into the live scene, so these are the readings an operator would be looking at. No
    // vehicle is connected in this slot, so the page must report no observer rather than rendering
    // an empty mission as a captured one.
    QVERIFY2(!page->property("_attached").toBool(),
             "with no vehicle connected the page must report no mission observer");
    QVERIFY2(!page->property("_agreementMatched").toBool(), "an unattached page must never report an agreement match");
}

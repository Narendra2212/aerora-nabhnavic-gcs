#include "AeroraSceneAttachmentUITest.h"

#include <cstdio>

#include <QtCore/QChar>
#include <QtCore/QList>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QStringList>
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

/// SCR-HLP-02's Analyze entry and the flickable inside its loaded page. The title comes from
/// AeroraCorePlugin::analyzePages(), so a rename there breaks this and should.
constexpr const char* kVocabularyEntryButton = "analyzeButton_Aerora State Vocabulary";
constexpr const char* kVocabularyFlickable = "aeroraVocabulary_flickable";

/// Every enumerator of AeroraTelemetryState::Class. The count is what the vocabulary page must
/// render a row for; AeroraUiFoundationTest holds this set against the meta-object.
constexpr int kStateClassCount = 7;

/// Counts the AeroraStateText samples in the visual subtree rooted at @a item.
///
/// Walked through childItems() rather than QObject::findChildren(), and that is the correction that
/// makes this measurable at all. A Repeater's delegates are visual children of the Repeater's
/// parent item, but they are not QObject children of anything in the page: QQmlDelegateModel
/// creates each delegate parentless and QQuickRepeater only calls setParentItem(). A findChildren()
/// sweep from the flickable therefore sees the page's declarative content and none of its rows -
/// which is what the first version of this slot did, and why it counted zero rather than seven.
/// Upstream's own scene helpers walk childItems() for the same reason
/// (test/QmlUITests/QmlUITestBase.cc:45, test/QmlUITests/TopLevelViewsTest.cc:55).
///
/// A sample is identified by its own classLabel property rather than by type name: QGCLabel is an
/// implementation detail of AeroraStateText, and the property is the contract.
///
/// No visibility filter. UNAVAILABLE's sample renders by making itself invisible, so a visible-only
/// count would be six by design - and would hide a seventh row that never instantiated.
int countStateSamples(QQuickItem* item)
{
    if (!item) {
        return 0;
    }

    int count = item->property("classLabel").isValid() ? 1 : 0;

    const QList<QQuickItem*> children = item->childItems();
    for (QQuickItem* const child : children) {
        count += countStateSamples(child);
    }

    return count;
}

/// Appends one line per item of the visual subtree rooted at @a item, for a failure to report with.
///
/// A bare count tells you the rows are missing but not whether they were never created or merely
/// not recognised, and those need different fixes. The dump separates them: an empty page body says
/// the Repeater produced nothing, a body full of rows says the predicate above is wrong.
void describeSubtree(QQuickItem* item, int depth, QStringList& lines)
{
    if (!item) {
        return;
    }

    QString line(depth * 2, QLatin1Char(' '));
    line += QString::fromUtf8(item->metaObject()->className());
    line += QStringLiteral(" name=");
    line += item->objectName().isEmpty() ? QStringLiteral("-") : item->objectName();
    line += item->isVisible() ? QStringLiteral(" visible") : QStringLiteral(" hidden");
    if (item->property("classLabel").isValid()) {
        line += QStringLiteral(" [sample]");
    }
    lines.append(line);

    const QList<QQuickItem*> children = item->childItems();
    for (QQuickItem* const child : children) {
        describeSubtree(child, depth + 1, lines);
    }
}

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

void AeroraSceneAttachmentUITest::_stateVocabularyPageIsReachableAndRendersEveryClass()
{
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    // No vehicle is connected and none is needed: the page is reference material derived from the
    // state singleton's own enumeration, which is exactly why it is registered without a vehicle
    // gate. Reaching it with nothing connected is the normal case.
    QVERIFY2(clickToolSelectDropdownButton(QStringLiteral("toolbar_viewAnalyze")),
             "the Analyze view must be reachable from the tool-select dropdown");
    QVERIFY2(clickButton(QString::fromUtf8(kVocabularyEntryButton)),
             "the Aerora State Vocabulary entry must appear in the Analyze page list");

    QQuickItem* const flickable =
        findVisibleItem(_rootItem, QString::fromUtf8(kVocabularyFlickable), TestTimeout::mediumMs());
    QVERIFY2(flickable, "clicking the entry must load the state vocabulary page");

    // Polled, because Repeater delegates instantiate over following frames rather than during the
    // click that loaded the page.
    const bool rendered =
        UnitTest::waitForCondition([flickable]() { return countStateSamples(flickable) == kStateClassCount; },
                                   TestTimeout::mediumMs(), QStringLiteral("every state class to render a sample"));

    if (!rendered) {
        // Dumped to stderr rather than folded into the assertion message: it is tens of lines, and
        // CTest's --output-on-failure already shows it. fprintf rather than a Qt logging macro so
        // the strict-mode log check does not count it as an unexpected message.
        QStringList lines;
        describeSubtree(flickable, 0, lines);
        fprintf(stderr, "AeroraStateVocabularyPage visual subtree:\n%s\n", qPrintable(lines.join(QLatin1Char('\n'))));
        fflush(stderr);
    }

    // One sample per class, including UNAVAILABLE - whose sample is instantiated and deliberately
    // invisible, so it is counted by existence rather than by visibility.
    QVERIFY2(rendered, qPrintable(QStringLiteral("expected %1 rendered state-class samples, found %2")
                                      .arg(kStateClassCount)
                                      .arg(countStateSamples(flickable))));
}

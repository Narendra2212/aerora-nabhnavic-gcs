#pragma once

#include "QmlUITestBase.h"

class PlanMasterController;

/// Runtime coverage of the Aerora overlay inside a booted application scene.
///
/// This is the half `AeroraMissionProgressUITest` cannot reach. That suite loads the page on a
/// bare engine, which proves the document compiles and its types resolve; it cannot prove that the
/// *application* wires anything up, because nothing boots MainWindow. The production chain is:
///
/// \code
/// QGCApplication loads MainWindow.qml
///     -> AeroraCorePlugin::createQmlApplicationEngine()
///         -> QQmlApplicationEngine::objectCreated
///             -> AeroraPlanBridge::attachToQmlScene(root)
///                 -> the scene's own Plan view and Fly view PlanMasterControllers
/// \endcode
///
/// and separately that `analyzePages()` produces a menu entry an operator can actually click. No
/// test asserted either until now; a green build says nothing about them.
///
/// \section harness Why this is a separate suite
///
/// `QmlUITestBase` boots and tears down a full QML window per slot, which `VehicleTest` does not
/// do and cannot be combined with. Splitting keeps the cheap engine-only assertions in
/// `AeroraMissionProgressUITest` and confines the expensive scene boot to the three questions that
/// genuinely require it.
///
/// \section windows Still not registered on Windows: the click path
///
/// The scene boot itself is no longer the obstacle. `AeroraSceneHarnessProbeTest` measured both
/// headless plugins on Windows (run `39265133278`): `offscreen` aborts on the first boot
/// (`qnumeric.h:523`, exit `0xc0000409`), but `minimal` boots the scene and this suite reported
/// "3 passed, 1 failed" under it.
///
/// The one that failed is `_missionProgressPageIsReachableFromTheAnalyzeMenu`, which drives mouse
/// clicks through the tool-select dropdown - so what remains unfixed on Windows is the click and
/// popup path, not scene construction. Two of the three slots here already pass under `minimal`.
///
/// The suite stays withheld rather than registered-and-failing, because registering it would add a
/// known failure for the sake of the two slots that would pass. `AeroraFlyViewOverlayUITest`, which
/// never clicks, is registered on Windows under `minimal` instead.
///
/// Nothing here is weakened to make Windows pass, and nothing here rots while it waits: the file
/// compiles on every platform.
class AeroraSceneAttachmentUITest : public QmlUITestBase
{
    Q_OBJECT

public:
    AeroraSceneAttachmentUITest() = default;

protected slots:
    void init() override;

private slots:
    // The production attachment path: did the core plugin's hook find the application's own
    // controllers, and bind each to the role that cannot misreport?
    void _planBridgeAttachesToTheSceneControllers();

    // objectCreated is not single-shot, so discovery must be safe to repeat.
    void _repeatedSceneDiscoveryCreatesNoDuplicateBridges();

    // The operator-facing route: Analyze menu -> the Aerora entry -> the page actually loads.
    void _missionProgressPageIsReachableFromTheAnalyzeMenu();

    // SCR-HLP-02, by the same route. Its content is derived from AeroraTelemetryState at runtime,
    // so a page that loaded with an empty class list would look like a working screen - this
    // asserts the rows are actually there.
    void _stateVocabularyPageIsReachableAndRendersEveryClass();

private:
    /// @return The scene's controller for @a flyView, or nullptr when absent.
    PlanMasterController* _sceneController(bool flyView);
};

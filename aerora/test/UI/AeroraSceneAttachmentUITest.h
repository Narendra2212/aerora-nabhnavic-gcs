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
/// \section windows Not registered on Windows: no headless plugin produces a usable scene
///
/// `AeroraSceneHarnessProbeTest` measured both candidates there and neither works, for different
/// reasons - `offscreen` aborts during scene construction, `minimal` constructs the scene but lays
/// nothing out, so the window's root item comes up zero-sized. The per-plugin evidence is recorded
/// once, at the registration site in `aerora/CMakeLists.txt`, rather than restated here.
///
/// An earlier revision of this comment read a probe run as "`minimal` boots the scene", on the
/// strength of this suite reporting three passing slots under it. That was wrong: every slot here
/// looks objects up and none reads a geometry, so all three pass against a zero-sized scene.
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

    // SCR-HLP-02, by the same route. Its rows are built by a Repeater over a list derived from
    // AeroraTelemetryState, so a page that loaded with an empty list would still show a heading and
    // a scope note - a screen that looks like it works and teaches nothing. This counts the rows.
    void _stateVocabularyPageIsReachableAndRendersEveryClass();

    // SCR-SEC-01's reason for existing: P7-SEC-005 requires a security screen that can say *not
    // enforced*, which is this build's actual state. A screen that could not say it would be worse
    // than no screen.
    void _securityPosturePageIsReachableAndReportsNotEnforced();

    // SCR-HLP-03 assembles its report from live station state. An empty or stub report would load a
    // page that looks like it works, so this requires the real build version to be in the text.
    void _errorReportPageIsReachableAndAssemblesRealStationState();

    // SCR-SHL-04. The page's every row reads a QML singleton, so a registration that did not happen
    // renders as a build with no recorded provenance rather than as an error. This compares the
    // singleton the page reads against a C++ instance of the same type.
    void _provenancePageIsReachableAndReadsTheRegisteredSingleton();

private:
    /// @return The scene's controller for @a flyView, or nullptr when absent.
    PlanMasterController* _sceneController(bool flyView);
};

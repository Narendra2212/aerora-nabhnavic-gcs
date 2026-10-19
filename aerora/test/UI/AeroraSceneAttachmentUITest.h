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
/// \section windows Not registered on Windows
///
/// `QmlUITestBase::startUI()` aborts partway through the scene boot on the Windows offscreen
/// platform - `ASSERT: "value < maximalPlusOne" in qnumeric.h`, process exit `0xc0000409` - and
/// that is a property of the shared harness, not of this suite: `windows.yml` runs
/// `include-labels: Portable` only, so none of the QML UI tests in `test/QmlUITests` has ever run
/// on Windows either.
///
/// An empty offscreen font database was the first hypothesis and is ruled out; `QT_QPA_FONTDIR` is
/// set in `cmake/QGCTest.cmake` and the abort is unchanged with fonts present.
///
/// So the registration is guarded in `aerora/CMakeLists.txt` and this file still compiles, which
/// is what keeps it honest in both directions: nothing here is weakened to make Windows pass, and
/// nothing here rots while it waits for a leg that can run it.
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

private:
    /// @return The scene's controller for @a flyView, or nullptr when absent.
    PlanMasterController* _sceneController(bool flyView);
};

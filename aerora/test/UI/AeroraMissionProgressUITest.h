#pragma once

#include "QmlUITestBase.h"

class PlanMasterController;

/// Runtime coverage of the mission-progress production path, in a booted application scene.
///
/// Every other Aerora mission test asserts a piece in isolation: `AeroraMissionTest` the snapshot
/// logic with no vehicle, `AeroraPlanBridgeTest` the bridge against controllers it constructs
/// itself, `AeroraUiFoundationTest` the bound names through a bare `QQmlEngine` and a context
/// property. None of them exercises the path the application actually takes, and a green build
/// says nothing about it:
///
/// \code
/// QGCApplication loads MainWindow.qml
///     -> AeroraCorePlugin::createQmlApplicationEngine()
///         -> QQmlApplicationEngine::objectCreated
///             -> AeroraPlanBridge::attachToQmlScene(root)
///                 -> finds PlanView's and FlyView's PlanMasterController
///                     -> observations reach AeroraMissionObserver
///                         -> AeroraMissionProgressPage.qml renders them
/// \endcode
///
/// `QmlUITestBase::startUI()` boots that whole chain, so the assertions below are about the real
/// scene rather than a reconstruction of it. In particular this is the first test that resolves
/// Aerora QML types **by type name** from a loaded document - `AeroraUi`, `AeroraMissionObserver`,
/// `AeroraSelfReportRow`, `AnalyzePage` - which is the `Q7-001` registration question that a
/// context property cannot answer.
///
/// \section notcovered What this still does not cover
///
/// Pixels. The page is instantiated and its rendered row text is read, but nothing here asserts
/// that two states are visually distinguishable - that is `Q-040` and `DEMO` verification. It also
/// does not click through the Analyze menu to the page; it asserts the page is registered in
/// `analyzePages()` with a URL that resolves to a real resource, and instantiates that URL on the
/// live engine.
class AeroraMissionProgressUITest : public QmlUITestBase
{
    Q_OBJECT

public:
    AeroraMissionProgressUITest() = default;

protected slots:
    void init() override;

private slots:
    // The production attachment path: did the core plugin's objectCreated hook actually find the
    // application's own controllers, and bind each to the right role?
    void _planBridgeAttachesToTheLiveSceneControllers();

    // Reachability and loadability of the page itself, including every import and the resource
    // alias the URL depends on.
    void _missionProgressPageIsRegisteredAndInstantiates();

    // With no vehicle there is no observer, and the page must say so rather than render an empty
    // mission as a captured one.
    void _missionProgressPageReportsNoObserverWithNoVehicle();

    // Discovery is wired to a signal that can fire more than once per engine.
    void _repeatedSceneDiscoveryCreatesNoDuplicateBridges();

    // End to end on a connected vehicle: edit the live Plan view plan, and the observer reached
    // through AeroraUi.mission must show it.
    void _livePlanAndSelectionChangesReachTheObserver();

private:
    /// @return The live scene's controller for @a flyView, or nullptr when absent.
    PlanMasterController* _sceneController(bool flyView);
};

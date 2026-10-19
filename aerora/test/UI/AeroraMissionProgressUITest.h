#pragma once

#include "BaseClasses/VehicleTest.h"

class QQmlApplicationEngine;

/// Runtime coverage of the mission-progress page against a real QML engine.
///
/// Every other Aerora mission test asserts a piece without ever loading the page:
/// `AeroraMissionTest` the snapshot logic with no vehicle, `AeroraPlanBridgeTest` the bridge
/// against controllers it constructs itself, `AeroraUiFoundationTest` the bound names through a
/// bare `QQmlEngine` and a **context property**. A context property needs no QML type
/// registration at all, so none of them can tell whether the page's document actually compiles.
///
/// This one loads `AeroraMissionProgressPage.qml` from the resource URL `analyzePages()` publishes,
/// on an engine built by `QGCCorePlugin::createQmlApplicationEngine()` so the import paths and the
/// Aerora QML module are the application's own. A successful `create()` therefore establishes
/// several things that were previously `NOT-EVIDENCED`:
///
/// \li the resource alias puts the page where the registered URL says it is,
/// \li every import in the document resolves, including `QGroundControl.AnalyzeView`,
/// \li `AnalyzePage`, `AeroraUi`, `AeroraMissionObserver` and `AeroraSelfReportRow` all resolve
///     **by QML type name**, which is the `Q7-001` registration question,
/// \li and the page's own readings derive from live singleton state rather than literals.
///
/// \section nowindow Why no window is booted
///
/// An earlier revision derived from `QmlUITestBase` and called `startUI()` to boot MainWindow,
/// which would additionally have proven that the core plugin's `objectCreated` hook finds the
/// *application's* two controllers. That harness aborts inside Qt's own `qnumeric.h` saturation
/// assert on the Windows runner, which is the only runner this overlay has; upstream deliberately
/// runs just the `Portable` suite there, so the QML UI harness has never executed on Windows.
/// Rather than ship a test that cannot run, the window dependency is dropped: `AnalyzePage` roots
/// on an `Item`, and an `Item` instantiates without a window.
///
/// What that costs is recorded rather than hidden - see the class comment on
/// `AeroraPlanBridge::attachToQmlScene()`. Attachment and role assignment are covered against real
/// `PlanMasterController` instances by `AeroraPlanBridgeTest`, including repeat discovery; what
/// stays unevidenced is specifically that the *booted application scene* yields those two
/// controllers to the hook.
class AeroraMissionProgressUITest : public VehicleTest
{
    Q_OBJECT

public:
    explicit AeroraMissionProgressUITest(QObject* parent = nullptr)
        : VehicleTest(parent)
    {
        // Generic rather than PX4-flavoured: the observer is reached through
        // AeroraVehicleServices, which only exists on a vehicle routed to the Aerora firmware
        // plugin. A PX4 MockLink would resolve to a different plugin, the observer would be null
        // and the assertions below would hold vacuously.
        setAutopilotType(MAV_AUTOPILOT_GENERIC);
        setWaitForInitialConnect(false);
    }

protected slots:
    void init() override;
    void cleanup() override;

private slots:
    // Reachability: the page is in the Analyze registry and its URL resolves to a real resource.
    void _missionProgressPageIsRegisteredWithAResolvableUrl();

    // The page's document compiles and instantiates on an application engine.
    void _missionProgressPageInstantiatesOnAnApplicationEngine();

    // End to end: edit the plan on a real controller bound to the connected vehicle, and the
    // page's readings follow.
    void _livePlanAndSelectionReachTheObserverAndThePage();

private:
    /// Builds an engine through the production factory, so import paths and the Aerora QML module
    /// are the application's. Owned by the test and released in cleanup().
    QQmlApplicationEngine* _applicationEngine();

    QQmlApplicationEngine* _engine = nullptr;
};

#pragma once

#include "BaseClasses/VehicleTest.h"

class PlanMasterController;

/// Covers AeroraPlanBridge against QGroundControl's real plan controllers.
///
/// Uses a `MAV_AUTOPILOT_GENERIC` MockLink for the same reason `AeroraMissionSyncTest` does: the
/// bridge reports into an observer reached through `AeroraVehicleServices`, which only exists on a
/// vehicle routed to the Aerora firmware plugin. A PX4-flavoured MockLink would resolve to a
/// different plugin and the observer would be null for every case, so nothing would be asserted.
///
/// A real `PlanMasterController` rather than a double, because the whole point of this class is
/// that it reads the controller QGroundControl actually runs: the role comes from the controller's
/// own `flyView` property, the plan comes from the upload path's own flattener, and the resume
/// index comes from a computation that only yields a value on a Fly view controller. A double
/// would let all three drift without failing.
class AeroraPlanBridgeTest : public VehicleTest
{
    Q_OBJECT

public:
    explicit AeroraPlanBridgeTest(QObject* parent = nullptr)
        : VehicleTest(parent)
    {
        setAutopilotType(MAV_AUTOPILOT_GENERIC);
        setWaitForInitialConnect(false);
    }

protected slots:
    void init() override;
    void cleanup() override;

private slots:
    // The sentinel. Pure translation, so it is asserted directly rather than through a vehicle:
    // this is the one place a "no resume available" answer could become a confident proposal at
    // item 0, and the upstream doc/implementation disagreement makes it easy to get wrong.
    void _resumeSentinelNeverManufacturesACandidate_data();
    void _resumeSentinelNeverManufacturesACandidate();

    // Role assignment, the decision that separates reporting the operator's plan from reporting
    // the vehicle's own mission back at itself.
    void _roleFollowsTheControllerFlyViewFlag_data();
    void _roleFollowsTheControllerFlyViewFlag();
    void _roleForNullControllerIsNotGcsPlan();

    // Attachment lifecycle.
    void _attachToNullControllerIsRefused();
    void _attachIsIdempotentAndNeverDoubleConnects();
    void _bridgeIsDestroyedWithItsController();

    // Reporting, end to end through a real controller and a real observer.
    void _planViewReportsTheFlattenedPlanAndTheSelection();
    void _flattenedPlanMatchesTheUploadPathForASettingsOnlyPlan();
    void _flyViewReportsNoResumeCandidateWhileNoMissionIsFlown();
    void _repeatedIdenticalReportsAreNotRepublished();

    // Scene discovery, including the conflict that must not silently overwrite.
    void _sceneDiscoveryAttachesOneBridgePerRole();
    void _sceneDiscoveryIgnoresASecondControllerForATakenRole();

private:
    /// Creates a started controller with @a flyView, owned by the test.
    PlanMasterController* _makeController(bool flyView);
};

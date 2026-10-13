#pragma once

#include <QtCore/QList>

#include "AeroraMissionIndex.h"
#include "AeroraMissionSnapshot.h"
#include "AeroraMissionValidationReport.h"
#include "QGCMAVLink.h"

/// Runs the mission checks this module can justify, and reports the ones it cannot run.
///
/// Operates on an AeroraMissionSnapshot rather than on `MissionController`'s visual items, for the
/// same reason the rest of this module does: a snapshot needs no `Vehicle`, no `MissionManager`,
/// no link and no event loop, so every check below is directly unit-testable. The snapshot is
/// already the flattened, wire-shaped form, which is also the form the vehicle would actually fly.
///
/// ### What this class refuses to do
///
/// It invents no threshold. There is no spacing minimum, no altitude floor or ceiling, no geofence
/// rule, no turn-radius limit and no battery-margin calculation anywhere in it. Those are product
/// and airframe decisions; a check against a guessed number would hand an operator a confident
/// verdict derived from nothing, which is worse than reporting that the check could not run.
/// `AeroraMissionCheck::GeometryLimits` is therefore always `NotPerformed`, with
/// `NoApprovedOperatingLimits` as the reason.
///
/// It also decides nothing. The report is a report: nothing here blocks a save, an upload or a
/// flight, and nothing here is on a command path.
///
/// ### Where each performable check's semantics come from
///
/// \li `MissionPresence` — a mission with no items has nothing to fly. No threshold involved.
/// \li `SequenceNumbering` — QGroundControl renumbers densely from 0 on upload and on resume
///     generation, so a non-monotonic or negative sequence is a corrupt flattening rather than a
///     style choice.
/// \li `FrameValidity` — `MAV_FRAME_ENUM_END` is AeroraMissionItemFingerprint's "never populated"
///     default, and is not a frame `PlanManager` can pack.
/// \li `CoordinateRange` — `PlanManager` treats every frame except `MAV_FRAME_MISSION` as carrying
///     scaled latitude/longitude (`PlanManager.cc:404-405`), so for those frames param5/param6
///     must be a real coordinate. The bound checked is geodetic, not an operating limit.
/// \li `ResumeCompatibility` — `MissionManager::generateResumeMission()` scans the whole mission
///     and **refuses outright** on any `MAV_CMD_DO_JUMP` (`MissionManager.cc:84-86`), so a mission
///     containing one cannot be resumed at all.
///
/// The remaining four checks are named and reported as `NotPerformed`. See validate().
class AeroraMissionValidator
{
public:
    /// Everything the pass is allowed to know. Each field exists because some check needs it, and
    /// a check whose field is absent reports `NotPerformed` rather than assuming a value.
    struct Inputs {
        /// The mission to check. Either origin is meaningful: a plan can be checked before upload
        /// and a downloaded mission can be checked as received.
        AeroraMissionSnapshot mission;

        /// Where a resume would start. An unattributed or unknown index means the caller asked no
        /// resume question, and the resume carry-forward check reports that rather than assuming
        /// index 0.
        AeroraMissionIndex resumeIndex;

        /// Whether any mission extension declared a command set — from
        /// `AeroraMissionExtensionRegistry::isMissionCommandSetDeclared()`.
        ///
        /// Passed as a separate flag rather than inferred from declaredMissionCommandSet being
        /// non-empty, because the distinction is load-bearing: an empty declared set is not
        /// "every command is supported". That conflation is the trap
        /// `FirmwarePlugin::supportedMissionCommands()` already contains.
        bool missionCommandSetDeclared = false;

        /// The declared supported command set. Only consulted when missionCommandSetDeclared.
        QList<MAV_CMD> declaredMissionCommandSet;

        /// Whether the vehicle has reported any capability at all — from
        /// `AeroraVehicleCapabilities::hasAnyReport()`. False in every current build (`Q3-009`).
        bool anyCapabilityReported = false;

        /// Commands the caller classifies as takeoff, from `MissionCommandTree`. Used only to
        /// enumerate resume omissions; passing none over-reports, which is the safe direction.
        QList<MAV_CMD> takeoffCommands;

        /// Whether the firmware takes a home item at index 0, from
        /// `FirmwarePlugin::sendHomePositionToVehicle()`.
        bool firmwareTakesHomeItem = true;
    };

    /// Runs every named check against @a inputs.
    ///
    /// Every check in `AeroraMissionCheck::CheckId` appears in the returned report exactly once,
    /// in enum order, whether or not it could run. That is deliberate: a check omitted from a
    /// report is indistinguishable from a check that passed, and the whole purpose of the report
    /// is to make the difference visible (`P7-CHK-002`).
    ///
    /// Four checks are `NotPerformed` in every current build, each for a recorded reason:
    ///
    /// \li `ResumeStateCarryForward` (`Q-023`) — the omitted items can be enumerated but not
    ///     classified. Telling a navigation item the aircraft already flew from a state-setting
    ///     item whose effect is silently lost needs the Aerora payload command set. The omission
    ///     count is reported in the detail so the gap is quantified, but a count is not a verdict.
    /// \li `MissionCommandSupport` (`Q-023`) — no mission command set is declared, so there is
    ///     nothing to check a command against.
    /// \li `VehicleCapabilityAgreement` (`Q3-009`, `Q-023`) — no capability is reported, and no
    ///     command-to-capability mapping is defined even if one were.
    /// \li `GeometryLimits` (product decision) — no operating limit is approved.
    ///
    /// `MissionCommandSupport` is the one of those four with a working performable branch: supply
    /// a declared set and it runs. The other three have no implementable branch yet, and none is
    /// faked.
    [[nodiscard]] static AeroraMissionValidationReport validate(const Inputs &inputs);

private:
    [[nodiscard]] static AeroraMissionCheck _checkMissionPresence(const Inputs &inputs);
    [[nodiscard]] static AeroraMissionCheck _checkSequenceNumbering(const Inputs &inputs);
    [[nodiscard]] static AeroraMissionCheck _checkFrameValidity(const Inputs &inputs);
    [[nodiscard]] static AeroraMissionCheck _checkCoordinateRange(const Inputs &inputs);
    [[nodiscard]] static AeroraMissionCheck _checkResumeCompatibility(const Inputs &inputs);
    [[nodiscard]] static AeroraMissionCheck _checkResumeStateCarryForward(const Inputs &inputs);
    [[nodiscard]] static AeroraMissionCheck _checkMissionCommandSupport(const Inputs &inputs);
    [[nodiscard]] static AeroraMissionCheck _checkVehicleCapabilityAgreement(const Inputs &inputs);
    [[nodiscard]] static AeroraMissionCheck _checkGeometryLimits(const Inputs &inputs);

    /// @return The reason a per-item check cannot examine @a inputs, or
    ///         `AeroraMissionCheck::NothingMissing` when it can.
    [[nodiscard]] static AeroraMissionCheck::Unavailable _perItemAvailability(const Inputs &inputs);
};

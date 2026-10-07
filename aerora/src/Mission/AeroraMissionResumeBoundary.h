#pragma once

#include <QtCore/QList>
#include <QtCore/QString>

#include "QGCMAVLink.h"

class AeroraMissionSnapshot;

/// One mission item that an upstream resume would not carry into the generated resume mission.
///
/// "Omission" rather than "drop" is deliberate. For a navigation item before the resume point the
/// omission is the entire purpose of resuming — the aircraft has already flown it. For a
/// state-setting item it is the exposure recorded as R-002. The distinction is a presentation
/// decision and belongs to the consumer, so this type reports the fact and classifies nothing.
struct AeroraMissionResumeOmission
{
    int sequenceNumber = -1;
    MAV_CMD command = MAV_CMD_ENUM_END;
};

/// What QGroundControl's mission resume does and does not carry, stated so it can be asserted.
///
/// Everything in this class is a **statement about upstream behaviour**, mirrored from
/// `MissionManager::generateResumeMission()`. Nothing here changes that behaviour, and nothing
/// here is on a command path: this class sends no message, builds no mission and holds no state.
///
/// \section why Why the facts are mirrored at all
///
/// `includedResumeCommands` is a plain function-local `QList<MAV_CMD>` inside
/// `generateResumeMission()`. It is not static, not const, not a member, and not reachable from
/// outside the function, and `MissionManager` declares no virtual members, so there is no
/// subclass, factory or plugin seam through which the list could be read or extended. A caller
/// that needs to know what resume will carry therefore has no choice but to restate it.
///
/// The mirror exists to make the Aerora side **honest and testable**, not to influence resume. It
/// is used to tell an operator what a resume will and will not re-establish
/// (`AERORA-MISSION-REQ-004`) and to let a test assert that no Aerora or payload command has been
/// smuggled into the set. The hazard of a mirror is that it goes stale against upstream; that is
/// recorded as a drift-detection obligation rather than pretended away — see this module's README.
///
/// \section notmodified What is deliberately not done here
///
/// \li The upstream list is **not modified**. `IF-MIS-016` carries nothing, by decision (ADR-005).
/// \li No Aerora, painting or payload command is added to the mirror. There is no evidenced
///     Aerora mission command to add (Q-023), and adding one would misstate upstream anyway.
/// \li **No ground-station payload-state replay exists.** `IF-MIS-015` carries nothing, by
///     decision. kGcsPayloadStateReplayImplemented pins that as an assertable constant.
/// \li Payload state persistence across a mission interruption is **not assumed**.
///     kPayloadStatePersistenceEvidenced pins that too.
/// \li No payload-state *classification* of commands is attempted. Identifying which commands set
///     Aerora payload state is blocked on Q-023, so this class classifies nothing and reports
///     only what upstream's filter does.
///
/// \section behaviour The upstream behaviour being stated
///
/// A resume rebuilds the vehicle mission from deep copies of existing plan items. An item is kept
/// when any of four conditions holds: it is index 0 and the firmware takes a home item, its index
/// is at or after the resume index, its command is in the replayed set, or it is a takeoff
/// command. There is no `else`, so an unlisted command **before** the resume index is omitted with
/// no log line, no warning, no counter and no operator notification. The kept prefix is then
/// de-duplicated latest-wins, with one-shot actions removed, which is what shows the mechanism's
/// purpose is state restoration rather than command replay.
///
/// The consequence that matters: resume reproduces the mission **from the resume point**, not the
/// full mission history.
class AeroraMissionResumeBoundary
{
public:
    /// Ground-station replay of payload state is not implemented and must not become implemented.
    ///
    /// Pinned as a constant so the prohibition is an assertion rather than an inspection. The
    /// mission module reaches no payload controller, issues no payload command and stores no
    /// payload state, so there is no code path for this to become true through.
    static constexpr bool kGcsPayloadStateReplayImplemented = false;

    /// Whether any evidence establishes that the vehicle retains payload state across a mission
    /// interruption.
    ///
    /// `false`, and it is a verified `false` rather than a placeholder: no baseline document
    /// states retention, the firmware payload elements are placeholders, and no bench or flight
    /// evidence has been produced (`IF-MIS-014`, evidence matrix item 14). Ownership of payload
    /// state by the payload controller is not retention of it.
    ///
    /// Therefore: **mission resume is not payload-state replay.** Any code that would read this as
    /// `true` must not be written until the constant changes on actual evidence.
    static constexpr bool kPayloadStatePersistenceEvidenced = false;

    /// The number of commands upstream replays. Pinned so a silent change to the mirror fails a
    /// test instead of passing unnoticed.
    static constexpr int kReplayedCommandCount = 14;

    /// The commands an upstream resume replays when they occur before the resume index.
    ///
    /// Mirrored from `MissionManager::generateResumeMission()`, in upstream order. Every entry is
    /// camera, region-of-interest, gimbal-mount, geofence-enable or speed related. Notably
    /// **absent**, and absent upstream too: every actuator and payload-actuation command, and
    /// every region-of-interest variant other than the legacy `DO_SET_ROI`.
    [[nodiscard]] static const QList<MAV_CMD> &replayedCommands();

    /// @return true when @a command is in the replayed set.
    [[nodiscard]] static bool isReplayed(MAV_CMD command);

    /// @return true when an item carrying @a command, positioned **before** the resume index,
    ///         would survive into the generated resume mission.
    ///
    /// @a isTakeoffCommand must be supplied by the caller because takeoff classification lives in
    /// `MissionCommandTree`, which this class deliberately does not depend on so that it stays a
    /// pure, table-testable statement of the filter.
    [[nodiscard]] static bool survivesBeforeResumeIndex(MAV_CMD command, bool isTakeoffCommand);

    /// Lists the items in @a flattenedPlan positioned before @a resumeSequenceNumber that an
    /// upstream resume would not carry forward.
    ///
    /// The result includes navigation items, because upstream genuinely does not carry them
    /// forward — that is what resuming means. A consumer that wants only state-setting commands
    /// must filter the result, using `MissionCommandTree::getUIInfo()` to tell a coordinate
    /// command from an action command. This class does not filter, because choosing what to show
    /// an operator is a presentation decision and because the payload-specific classification it
    /// would need is blocked on Q-023.
    ///
    /// @param flattenedPlan          A snapshot of the flattened mission. Origin is not
    ///                               constrained: the question is equally meaningful about a plan
    ///                               and about what the vehicle currently holds.
    /// @param resumeSequenceNumber   The resume start point. A negative value yields an empty list.
    /// @param takeoffCommands        Commands the caller classifies as takeoff. Pass the set from
    ///                               `MissionCommandTree`; passing none over-reports rather than
    ///                               under-reports, which is the safe direction for a warning.
    /// @param firmwareTakesHomeItem  Whether the firmware takes a home item at index 0, from
    ///                               `FirmwarePlugin::sendHomePositionToVehicle()`. When true,
    ///                               index 0 is upstream's home slot and is excluded. When false,
    ///                               index 0 is an ordinary mission item and is filtered like any
    ///                               other.
    [[nodiscard]] static QList<AeroraMissionResumeOmission> notCarriedForwardBeforeResumeIndex(
        const AeroraMissionSnapshot &flattenedPlan,
        int resumeSequenceNumber,
        const QList<MAV_CMD> &takeoffCommands = QList<MAV_CMD>(),
        bool firmwareTakesHomeItem = true);
};

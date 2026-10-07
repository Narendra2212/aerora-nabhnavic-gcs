#include "AeroraMissionResumeBoundary.h"

#include "AeroraMissionSnapshot.h"

namespace {

/// Mirror of the function-local `includedResumeCommands` built in
/// `MissionManager::generateResumeMission()`, in upstream order.
///
/// Kept in upstream order rather than sorted so that a comparison against the upstream source is
/// a line-for-line read.
const QList<MAV_CMD> &includedResumeCommandsMirror()
{
    static const QList<MAV_CMD> commands = {
        MAV_CMD_DO_CONTROL_VIDEO,
        MAV_CMD_DO_SET_ROI,
        MAV_CMD_DO_DIGICAM_CONFIGURE,
        MAV_CMD_DO_DIGICAM_CONTROL,
        MAV_CMD_DO_MOUNT_CONFIGURE,
        MAV_CMD_DO_MOUNT_CONTROL,
        MAV_CMD_DO_SET_CAM_TRIGG_DIST,
        MAV_CMD_DO_FENCE_ENABLE,
        MAV_CMD_IMAGE_START_CAPTURE,
        MAV_CMD_IMAGE_STOP_CAPTURE,
        MAV_CMD_VIDEO_START_CAPTURE,
        MAV_CMD_VIDEO_STOP_CAPTURE,
        MAV_CMD_DO_CHANGE_SPEED,
        MAV_CMD_SET_CAMERA_MODE,
    };

    return commands;
}

} // namespace

const QList<MAV_CMD> &AeroraMissionResumeBoundary::replayedCommands()
{
    return includedResumeCommandsMirror();
}

bool AeroraMissionResumeBoundary::isReplayed(MAV_CMD command)
{
    return includedResumeCommandsMirror().contains(command);
}

bool AeroraMissionResumeBoundary::survivesBeforeResumeIndex(MAV_CMD command, bool isTakeoffCommand)
{
    return isTakeoffCommand || isReplayed(command);
}

QList<AeroraMissionResumeOmission> AeroraMissionResumeBoundary::notCarriedForwardBeforeResumeIndex(
    const AeroraMissionSnapshot &flattenedPlan,
    int resumeSequenceNumber,
    const QList<MAV_CMD> &takeoffCommands,
    bool firmwareTakesHomeItem)
{
    QList<AeroraMissionResumeOmission> omissions;

    if (resumeSequenceNumber < 0) {
        return omissions;
    }

    for (const AeroraMissionItemFingerprint &item : flattenedPlan.items()) {
        if (item.sequenceNumber >= resumeSequenceNumber) {
            // Upstream keeps everything at or after the resume index, untouched.
            continue;
        }

        if (firmwareTakesHomeItem && (item.sequenceNumber == 0)) {
            // Upstream's home slot, kept unconditionally when the firmware takes one.
            continue;
        }

        if (survivesBeforeResumeIndex(item.command, takeoffCommands.contains(item.command))) {
            continue;
        }

        omissions.append(AeroraMissionResumeOmission{item.sequenceNumber, item.command});
    }

    return omissions;
}

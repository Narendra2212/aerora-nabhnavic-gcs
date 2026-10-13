#include "AeroraMissionValidator.h"

#include <cmath>

#include <QtCore/QStringLiteral>

#include "AeroraMissionLog.h"
#include "AeroraMissionResumeBoundary.h"

namespace {

/// Geodetic bounds for a scaled latitude/longitude parameter.
///
/// These are the definition of the coordinate system, not an operating limit: a latitude outside
/// +/-90 is not a mission anyone could fly at any site, under any product policy. Nothing in this
/// file carries a threshold that a product decision could legitimately change - that distinction
/// is what keeps `GeometryLimits` honestly `NotPerformed` rather than quietly implemented here.
constexpr double kLatitudeLimitDeg = 90.0;
constexpr double kLongitudeLimitDeg = 180.0;

/// Machine-readable position/command fragment for a check detail. Not operator-facing text.
QString itemDetail(const AeroraMissionItemFingerprint &item)
{
    return QStringLiteral("index=%1 command=%2").arg(item.sequenceNumber).arg(static_cast<int>(item.command));
}

} // namespace

AeroraMissionCheck::Unavailable AeroraMissionValidator::_perItemAvailability(const Inputs &inputs)
{
    // Absent is not empty. A snapshot nobody captured says nothing about the items it does not
    // have, so a per-item check over it would pass vacuously and read as a clean result.
    if (!inputs.mission.isCaptured()) {
        return AeroraMissionCheck::MissionNotCaptured;
    }

    return AeroraMissionCheck::NothingMissing;
}

AeroraMissionCheck AeroraMissionValidator::_checkMissionPresence(const Inputs &inputs)
{
    if (!inputs.mission.isCaptured()) {
        return AeroraMissionCheck::notPerformed(AeroraMissionCheck::MissionPresence,
                                                AeroraMissionCheck::MissionNotCaptured);
    }

    if (inputs.mission.itemCount() == 0) {
        return AeroraMissionCheck::failed(AeroraMissionCheck::MissionPresence,
                                          QStringLiteral("captured=%1 itemCount=0")
                                              .arg(AeroraMissionSnapshot::originToString(inputs.mission.origin())));
    }

    return AeroraMissionCheck::passed(AeroraMissionCheck::MissionPresence,
                                      QStringLiteral("itemCount=%1").arg(inputs.mission.itemCount()));
}

AeroraMissionCheck AeroraMissionValidator::_checkSequenceNumbering(const Inputs &inputs)
{
    const AeroraMissionCheck::Unavailable availability = _perItemAvailability(inputs);
    if (availability != AeroraMissionCheck::NothingMissing) {
        return AeroraMissionCheck::notPerformed(AeroraMissionCheck::SequenceNumbering, availability);
    }

    int previous = -1;
    for (const AeroraMissionItemFingerprint &item : inputs.mission.items()) {
        if (item.sequenceNumber < 0) {
            return AeroraMissionCheck::failed(AeroraMissionCheck::SequenceNumbering,
                                              QStringLiteral("negative sequence: %1").arg(itemDetail(item)));
        }

        if (item.sequenceNumber <= previous) {
            return AeroraMissionCheck::failed(
                AeroraMissionCheck::SequenceNumbering,
                QStringLiteral("not increasing: previous=%1 %2").arg(previous).arg(itemDetail(item)));
        }

        previous = item.sequenceNumber;
    }

    return AeroraMissionCheck::passed(AeroraMissionCheck::SequenceNumbering,
                                      QStringLiteral("itemCount=%1").arg(inputs.mission.itemCount()));
}

AeroraMissionCheck AeroraMissionValidator::_checkFrameValidity(const Inputs &inputs)
{
    const AeroraMissionCheck::Unavailable availability = _perItemAvailability(inputs);
    if (availability != AeroraMissionCheck::NothingMissing) {
        return AeroraMissionCheck::notPerformed(AeroraMissionCheck::FrameValidity, availability);
    }

    for (const AeroraMissionItemFingerprint &item : inputs.mission.items()) {
        // MAV_FRAME_ENUM_END is the fingerprint's never-populated default, so it means the frame
        // was not captured rather than that the item chose an exotic frame.
        if (item.frame == MAV_FRAME_ENUM_END) {
            return AeroraMissionCheck::failed(AeroraMissionCheck::FrameValidity,
                                              QStringLiteral("unpopulated frame: %1").arg(itemDetail(item)));
        }
    }

    return AeroraMissionCheck::passed(AeroraMissionCheck::FrameValidity,
                                      QStringLiteral("itemCount=%1").arg(inputs.mission.itemCount()));
}

AeroraMissionCheck AeroraMissionValidator::_checkCoordinateRange(const Inputs &inputs)
{
    const AeroraMissionCheck::Unavailable availability = _perItemAvailability(inputs);
    if (availability != AeroraMissionCheck::NothingMissing) {
        return AeroraMissionCheck::notPerformed(AeroraMissionCheck::CoordinateRange, availability);
    }

    int examined = 0;
    for (const AeroraMissionItemFingerprint &item : inputs.mission.items()) {
        // PlanManager places param5/param6 in the integer fields unscaled for MAV_FRAME_MISSION
        // and scaled by 1e7 for every other frame (PlanManager.cc:404-405), so only the scaled
        // frames carry a coordinate to range-check.
        if (item.frame == MAV_FRAME_MISSION) {
            continue;
        }

        ++examined;

        if (std::isnan(item.param5) || std::isnan(item.param6)) {
            return AeroraMissionCheck::failed(AeroraMissionCheck::CoordinateRange,
                                              QStringLiteral("NaN coordinate: %1").arg(itemDetail(item)));
        }

        if ((std::abs(item.param5) > kLatitudeLimitDeg) || (std::abs(item.param6) > kLongitudeLimitDeg)) {
            return AeroraMissionCheck::failed(AeroraMissionCheck::CoordinateRange,
                                              QStringLiteral("out of range: %1").arg(itemDetail(item)));
        }
    }

    return AeroraMissionCheck::passed(AeroraMissionCheck::CoordinateRange,
                                      QStringLiteral("coordinateItems=%1").arg(examined));
}

AeroraMissionCheck AeroraMissionValidator::_checkResumeCompatibility(const Inputs &inputs)
{
    const AeroraMissionCheck::Unavailable availability = _perItemAvailability(inputs);
    if (availability != AeroraMissionCheck::NothingMissing) {
        return AeroraMissionCheck::notPerformed(AeroraMissionCheck::ResumeCompatibility, availability);
    }

    // MissionManager::generateResumeMission() scans the whole mission and returns without
    // generating anything on the first MAV_CMD_DO_JUMP (MissionManager.cc:84-86). The mission is
    // not unflyable; it is unresumable, which is why this reports rather than blocks.
    for (const AeroraMissionItemFingerprint &item : inputs.mission.items()) {
        if (item.command == MAV_CMD_DO_JUMP) {
            return AeroraMissionCheck::failed(AeroraMissionCheck::ResumeCompatibility,
                                              QStringLiteral("resume refused on DO_JUMP: %1").arg(itemDetail(item)));
        }
    }

    return AeroraMissionCheck::passed(AeroraMissionCheck::ResumeCompatibility,
                                      QStringLiteral("itemCount=%1").arg(inputs.mission.itemCount()));
}

AeroraMissionCheck AeroraMissionValidator::_checkResumeStateCarryForward(const Inputs &inputs)
{
    const AeroraMissionCheck::Unavailable availability = _perItemAvailability(inputs);
    if (availability != AeroraMissionCheck::NothingMissing) {
        return AeroraMissionCheck::notPerformed(AeroraMissionCheck::ResumeStateCarryForward, availability);
    }

    if (!inputs.resumeIndex.isKnown()) {
        return AeroraMissionCheck::notPerformed(AeroraMissionCheck::ResumeStateCarryForward,
                                                AeroraMissionCheck::NoResumeIndexSupplied,
                                                inputs.resumeIndex.toString());
    }

    // The omissions are enumerable: this is upstream's filter, mirrored. What is not available is
    // the classification - telling a navigation item the aircraft already flew from a
    // state-setting item whose effect is silently lost needs the Aerora payload command set.
    // Q-023. The count is reported so the gap is quantified rather than merely named, and the
    // check stays NotPerformed because a count is not a verdict.
    const QList<AeroraMissionResumeOmission> omissions =
        AeroraMissionResumeBoundary::notCarriedForwardBeforeResumeIndex(inputs.mission,
                                                                       inputs.resumeIndex.sequenceNumber(),
                                                                       inputs.takeoffCommands,
                                                                       inputs.firmwareTakesHomeItem);

    return AeroraMissionCheck::notPerformed(AeroraMissionCheck::ResumeStateCarryForward,
                                            AeroraMissionCheck::NoDeclaredMissionCommandSet,
                                            QStringLiteral("resumeIndex=%1 omittedItems=%2 classified=0")
                                                .arg(inputs.resumeIndex.sequenceNumber())
                                                .arg(omissions.count()));
}

AeroraMissionCheck AeroraMissionValidator::_checkMissionCommandSupport(const Inputs &inputs)
{
    // An empty declared set is not "everything is supported". The flag and the set are separate
    // inputs precisely so this cannot be inferred wrongly.
    if (!inputs.missionCommandSetDeclared) {
        return AeroraMissionCheck::notPerformed(AeroraMissionCheck::MissionCommandSupport,
                                                AeroraMissionCheck::NoDeclaredMissionCommandSet);
    }

    const AeroraMissionCheck::Unavailable availability = _perItemAvailability(inputs);
    if (availability != AeroraMissionCheck::NothingMissing) {
        return AeroraMissionCheck::notPerformed(AeroraMissionCheck::MissionCommandSupport, availability);
    }

    for (const AeroraMissionItemFingerprint &item : inputs.mission.items()) {
        if (!inputs.declaredMissionCommandSet.contains(item.command)) {
            return AeroraMissionCheck::failed(AeroraMissionCheck::MissionCommandSupport,
                                              QStringLiteral("undeclared command: %1").arg(itemDetail(item)));
        }
    }

    return AeroraMissionCheck::passed(AeroraMissionCheck::MissionCommandSupport,
                                      QStringLiteral("declaredCommands=%1 itemCount=%2")
                                          .arg(inputs.declaredMissionCommandSet.count())
                                          .arg(inputs.mission.itemCount()));
}

AeroraMissionCheck AeroraMissionValidator::_checkVehicleCapabilityAgreement(const Inputs &inputs)
{
    if (!inputs.anyCapabilityReported) {
        return AeroraMissionCheck::notPerformed(AeroraMissionCheck::VehicleCapabilityAgreement,
                                                AeroraMissionCheck::NoReportedCapability);
    }

    // Reported capability is necessary but not sufficient. Agreement needs a mapping from a
    // mission command to the capability it depends on, and no such mapping is defined (Q-023).
    // Reporting NotPerformed here rather than Passed is the whole point: a vehicle that reports
    // capabilities must not make an uncheckable check look checked.
    return AeroraMissionCheck::notPerformed(AeroraMissionCheck::VehicleCapabilityAgreement,
                                            AeroraMissionCheck::NoDeclaredMissionCommandSet,
                                            QStringLiteral("capabilityReported=true commandMapping=none"));
}

AeroraMissionCheck AeroraMissionValidator::_checkGeometryLimits(const Inputs &inputs)
{
    Q_UNUSED(inputs)

    // Always NotPerformed, and deliberately unconditional. Spacing, altitude, geofence and
    // turn-radius limits are product and airframe decisions; there is no input that could make
    // this check run today, and inventing one number would produce confident verdicts from
    // nothing. P7-SAFE-003.
    return AeroraMissionCheck::notPerformed(AeroraMissionCheck::GeometryLimits,
                                            AeroraMissionCheck::NoApprovedOperatingLimits);
}

AeroraMissionValidationReport AeroraMissionValidator::validate(const Inputs &inputs)
{
    AeroraMissionValidationReport report;

    // Every check appears exactly once, in CheckId order, performable or not. A check omitted
    // from a report is indistinguishable from one that passed. P7-CHK-002.
    report.append(_checkMissionPresence(inputs));
    report.append(_checkSequenceNumbering(inputs));
    report.append(_checkFrameValidity(inputs));
    report.append(_checkCoordinateRange(inputs));
    report.append(_checkResumeCompatibility(inputs));
    report.append(_checkResumeStateCarryForward(inputs));
    report.append(_checkMissionCommandSupport(inputs));
    report.append(_checkVehicleCapabilityAgreement(inputs));
    report.append(_checkGeometryLimits(inputs));

    qCDebug(AeroraMissionLog) << "mission validation complete"
                              << "verdict:" << AeroraMissionValidationReport::verdictToString(report.verdict())
                              << "report:" << report.toString();

    return report;
}

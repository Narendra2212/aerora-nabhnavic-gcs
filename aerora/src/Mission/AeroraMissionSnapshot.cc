#include "AeroraMissionSnapshot.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include <QtCore/QStringLiteral>

#include "AeroraMissionLog.h"
#include "MissionItem.h"

namespace {

/// `param1`-`param4` and `param7` occupy `float` fields in `MISSION_ITEM_INT`.
[[nodiscard]] float toWireFloat(double value)
{
    return static_cast<float>(value);
}

/// `param5`/`param6` occupy `int32` fields.
///
/// The scaling mirrors the pack site exactly, including the truncating conversion: QGroundControl
/// passes a `double` into an `int32_t` parameter, which truncates toward zero rather than
/// rounding. Rounding here would disagree with the vehicle by one quantum at some coordinates and
/// report a difference the aircraft cannot see.
[[nodiscard]] std::int32_t toWireCoordinate(double value, MAV_FRAME frame)
{
    const double scaled = (frame == MAV_FRAME_MISSION) ? value : (value * 1e7);

    // Guard the conversion: a non-finite or out-of-range value has no int32 representation, and
    // letting it convert would be undefined behaviour.
    if (!std::isfinite(scaled)) {
        return 0;
    }
    if (scaled >= static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
        return std::numeric_limits<std::int32_t>::max();
    }
    if (scaled <= static_cast<double>(std::numeric_limits<std::int32_t>::min())) {
        return std::numeric_limits<std::int32_t>::min();
    }

    return static_cast<std::int32_t>(scaled);
}

/// Equality for a `float` wire field. Bit-identical comparison is correct here rather than a
/// tolerance: both operands have already been reduced to the same representation, so any
/// remaining difference is a real difference in what was transmitted. NaN is treated as equal to
/// NaN so that two items carrying an unused parameter do not compare unequal.
[[nodiscard]] bool sameWireFloat(double lhs, double rhs)
{
    const float lhsWire = toWireFloat(lhs);
    const float rhsWire = toWireFloat(rhs);

    if (std::isnan(lhsWire) && std::isnan(rhsWire)) {
        return true;
    }

    return lhsWire == rhsWire;
}

} // namespace

bool AeroraMissionItemFingerprint::sameCommand(const AeroraMissionItemFingerprint &other) const
{
    return (sequenceNumber == other.sequenceNumber) && (command == other.command);
}

bool AeroraMissionItemFingerprint::sameAtWirePrecision(const AeroraMissionItemFingerprint &other) const
{
    if (!sameCommand(other)) {
        return false;
    }

    if ((frame != other.frame) || (autoContinue != other.autoContinue)) {
        return false;
    }

    if (!sameWireFloat(param1, other.param1) || !sameWireFloat(param2, other.param2)
        || !sameWireFloat(param3, other.param3) || !sameWireFloat(param4, other.param4)
        || !sameWireFloat(param7, other.param7)) {
        return false;
    }

    // Frames already agree, so either side's frame selects the same transform.
    return (toWireCoordinate(param5, frame) == toWireCoordinate(other.param5, other.frame))
        && (toWireCoordinate(param6, frame) == toWireCoordinate(other.param6, other.frame));
}

AeroraMissionAgreement AeroraMissionAgreement::matches(Depth depth)
{
    AeroraMissionAgreement agreement;
    agreement._verdict = Matches;
    agreement._depth = depth;
    return agreement;
}

AeroraMissionAgreement AeroraMissionAgreement::differs(Depth depth, int firstDifferenceIndex, const QString &reason)
{
    AeroraMissionAgreement agreement;
    agreement._verdict = Differs;
    agreement._depth = depth;
    agreement._firstDifferenceIndex = firstDifferenceIndex;
    agreement._reason = reason;
    return agreement;
}

QString AeroraMissionAgreement::verdictToString(Verdict verdict)
{
    switch (verdict) {
    case NotCompared:
        return QStringLiteral("notCompared");
    case Matches:
        return QStringLiteral("matches");
    case Differs:
        return QStringLiteral("differs");
    }

    return QStringLiteral("notCompared");
}

QString AeroraMissionAgreement::depthToString(Depth depth)
{
    switch (depth) {
    case DepthNone:
        return QStringLiteral("none");
    case DepthItemCount:
        return QStringLiteral("itemCount");
    case DepthCommandSequence:
        return QStringLiteral("commandSequence");
    case DepthFullParameters:
        return QStringLiteral("fullParameters");
    }

    return QStringLiteral("none");
}

AeroraMissionSnapshot AeroraMissionSnapshot::empty(Origin origin)
{
    AeroraMissionSnapshot snapshot;
    snapshot._origin = origin;
    return snapshot;
}

AeroraMissionSnapshot AeroraMissionSnapshot::fromMissionItems(Origin origin, const QList<MissionItem *> &items)
{
    AeroraMissionSnapshot snapshot;
    snapshot._origin = origin;

    for (const MissionItem *const item : items) {
        if (!item) {
            qCWarning(AeroraMissionLog) << "skipping null mission item while capturing snapshot. origin:"
                                        << originToString(origin);
            continue;
        }

        AeroraMissionItemFingerprint fingerprint;
        fingerprint.sequenceNumber = item->sequenceNumber();
        fingerprint.command = item->command();
        fingerprint.frame = item->frame();
        fingerprint.param1 = item->param1();
        fingerprint.param2 = item->param2();
        fingerprint.param3 = item->param3();
        fingerprint.param4 = item->param4();
        fingerprint.param5 = item->param5();
        fingerprint.param6 = item->param6();
        fingerprint.param7 = item->param7();
        fingerprint.autoContinue = item->autoContinue();

        snapshot._items.append(fingerprint);
    }

    return snapshot;
}

void AeroraMissionSnapshot::append(const AeroraMissionItemFingerprint &fingerprint)
{
    if (_origin == NotCaptured) {
        qCWarning(AeroraMissionLog) << "rejecting append to an unattributed snapshot: a captured"
                                    << "mission must state which side it came from";
        return;
    }

    _items.append(fingerprint);
}

AeroraMissionAgreement AeroraMissionSnapshot::compare(const AeroraMissionSnapshot &plan,
                                                      const AeroraMissionSnapshot &vehicleMission,
                                                      AeroraMissionAgreement::Depth depth)
{
    if (depth == AeroraMissionAgreement::DepthNone) {
        return AeroraMissionAgreement();
    }

    if ((plan._origin != GcsPlan) || (vehicleMission._origin != VehicleStore)) {
        qCWarning(AeroraMissionLog) << "refusing mission comparison with wrong origins. plan:"
                                    << originToString(plan._origin)
                                    << "vehicleMission:" << originToString(vehicleMission._origin);
        return AeroraMissionAgreement();
    }

    const int planCount = plan.itemCount();
    const int vehicleCount = vehicleMission.itemCount();

    if (planCount != vehicleCount) {
        return AeroraMissionAgreement::differs(
            depth,
            -1,
            QStringLiteral("itemCountMismatch:plan=%1,vehicle=%2").arg(planCount).arg(vehicleCount));
    }

    if (depth == AeroraMissionAgreement::DepthItemCount) {
        return AeroraMissionAgreement::matches(depth);
    }

    for (int i = 0; i < planCount; i++) {
        const AeroraMissionItemFingerprint &planItem = plan._items.at(i);
        const AeroraMissionItemFingerprint &vehicleItem = vehicleMission._items.at(i);

        if (!planItem.sameCommand(vehicleItem)) {
            return AeroraMissionAgreement::differs(
                depth,
                i,
                QStringLiteral("commandMismatch:plan=%1/%2,vehicle=%3/%4")
                    .arg(planItem.sequenceNumber)
                    .arg(static_cast<int>(planItem.command))
                    .arg(vehicleItem.sequenceNumber)
                    .arg(static_cast<int>(vehicleItem.command)));
        }

        if ((depth == AeroraMissionAgreement::DepthFullParameters) && !planItem.sameAtWirePrecision(vehicleItem)) {
            return AeroraMissionAgreement::differs(depth,
                                                   i,
                                                   QStringLiteral("parameterMismatch:seq=%1,command=%2")
                                                       .arg(planItem.sequenceNumber)
                                                       .arg(static_cast<int>(planItem.command)));
        }
    }

    return AeroraMissionAgreement::matches(depth);
}

QString AeroraMissionSnapshot::originToString(Origin origin)
{
    switch (origin) {
    case NotCaptured:
        return QStringLiteral("notCaptured");
    case GcsPlan:
        return QStringLiteral("gcsPlan");
    case VehicleStore:
        return QStringLiteral("vehicleStore");
    }

    return QStringLiteral("notCaptured");
}

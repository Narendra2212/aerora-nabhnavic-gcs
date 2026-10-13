#include "AeroraMissionValidationReport.h"

#include <QtCore/QStringList>

#include "AeroraMissionLog.h"

AeroraMissionCheck AeroraMissionCheck::passed(CheckId id, const QString &detail)
{
    AeroraMissionCheck check;
    check._id = id;
    check._outcome = Passed;
    check._unavailableReason = NothingMissing;
    check._detail = detail;

    return check;
}

AeroraMissionCheck AeroraMissionCheck::failed(CheckId id, const QString &detail)
{
    AeroraMissionCheck check;
    check._id = id;
    check._outcome = Failed;
    check._unavailableReason = NothingMissing;
    check._detail = detail;

    return check;
}

AeroraMissionCheck AeroraMissionCheck::notPerformed(CheckId id, Unavailable reason, const QString &detail)
{
    AeroraMissionCheck check;
    check._id = id;
    check._outcome = NotPerformed;
    check._detail = detail;

    // NothingMissing would make a not-performed check indistinguishable from a performed one in
    // the one field that is supposed to explain it, so it is refused rather than stored.
    if (reason == NothingMissing) {
        qCWarning(AeroraMissionLog) << "check recorded as not performed with no missing input"
                                    << "check:" << checkIdToString(id);
        check._unavailableReason = MissionNotCaptured;
        return check;
    }

    check._unavailableReason = reason;

    return check;
}

QString AeroraMissionCheck::checkIdToString(CheckId id)
{
    switch (id) {
    case MissionPresence:
        return QStringLiteral("missionPresence");
    case SequenceNumbering:
        return QStringLiteral("sequenceNumbering");
    case FrameValidity:
        return QStringLiteral("frameValidity");
    case CoordinateRange:
        return QStringLiteral("coordinateRange");
    case ResumeCompatibility:
        return QStringLiteral("resumeCompatibility");
    case ResumeStateCarryForward:
        return QStringLiteral("resumeStateCarryForward");
    case MissionCommandSupport:
        return QStringLiteral("missionCommandSupport");
    case VehicleCapabilityAgreement:
        return QStringLiteral("vehicleCapabilityAgreement");
    case GeometryLimits:
        return QStringLiteral("geometryLimits");
    }

    return QStringLiteral("unknown");
}

QString AeroraMissionCheck::outcomeToString(Outcome outcome)
{
    switch (outcome) {
    case NotPerformed:
        return QStringLiteral("notPerformed");
    case Passed:
        return QStringLiteral("passed");
    case Failed:
        return QStringLiteral("failed");
    }

    return QStringLiteral("notPerformed");
}

QString AeroraMissionCheck::unavailableToString(Unavailable reason)
{
    switch (reason) {
    case NothingMissing:
        return QStringLiteral("nothingMissing");
    case MissionNotCaptured:
        return QStringLiteral("missionNotCaptured");
    case MissionEmpty:
        return QStringLiteral("missionEmpty");
    case NoApplicableItems:
        return QStringLiteral("noApplicableItems");
    case NoDeclaredMissionCommandSet:
        return QStringLiteral("noDeclaredMissionCommandSet");
    case NoReportedCapability:
        return QStringLiteral("noReportedCapability");
    case NoApprovedOperatingLimits:
        return QStringLiteral("noApprovedOperatingLimits");
    case NoResumeIndexSupplied:
        return QStringLiteral("noResumeIndexSupplied");
    }

    return QStringLiteral("unknown");
}

void AeroraMissionValidationReport::append(const AeroraMissionCheck &check)
{
    _checks.append(check);
}

int AeroraMissionValidationReport::passedCount() const
{
    return static_cast<int>(checksWithOutcome(AeroraMissionCheck::Passed).count());
}

int AeroraMissionValidationReport::failedCount() const
{
    return static_cast<int>(checksWithOutcome(AeroraMissionCheck::Failed).count());
}

int AeroraMissionValidationReport::notPerformedCount() const
{
    return static_cast<int>(checksWithOutcome(AeroraMissionCheck::NotPerformed).count());
}

QList<AeroraMissionCheck> AeroraMissionValidationReport::checksWithOutcome(AeroraMissionCheck::Outcome outcome) const
{
    QList<AeroraMissionCheck> matching;

    for (const AeroraMissionCheck &check : _checks) {
        if (check.outcome() == outcome) {
            matching.append(check);
        }
    }

    return matching;
}

AeroraMissionValidationReport::Verdict AeroraMissionValidationReport::verdict() const
{
    // An empty report is not a pass. Order matters below: a failure outranks a gap, because a
    // found problem is more actionable than an unexamined one - but a gap must still not collapse
    // into Passed, which is the fold P7-CHK-002 exists to prevent.
    if (_checks.isEmpty()) {
        return NoChecksPerformed;
    }

    if (hasFailure()) {
        return Failed;
    }

    if (notPerformedCount() > 0) {
        if (passedCount() == 0) {
            // Nothing ran at all, even though checks were named. Reporting PassedWithGaps here
            // would imply something was examined.
            return NoChecksPerformed;
        }
        return PassedWithGaps;
    }

    return Passed;
}

QString AeroraMissionValidationReport::toString() const
{
    return QStringLiteral("%1 checks=%2 passed=%3 failed=%4 notPerformed=%5")
        .arg(verdictToString(verdict()))
        .arg(checkCount())
        .arg(passedCount())
        .arg(failedCount())
        .arg(notPerformedCount());
}

QString AeroraMissionValidationReport::verdictToString(Verdict verdict)
{
    switch (verdict) {
    case NoChecksPerformed:
        return QStringLiteral("noChecksPerformed");
    case Failed:
        return QStringLiteral("failed");
    case PassedWithGaps:
        return QStringLiteral("passedWithGaps");
    case Passed:
        return QStringLiteral("passed");
    }

    return QStringLiteral("noChecksPerformed");
}

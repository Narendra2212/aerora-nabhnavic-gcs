#include "AeroraMissionIndex.h"

#include <QtCore/QStringLiteral>

AeroraMissionIndex AeroraMissionIndex::fromVehicleReport(int sequenceNumber)
{
    AeroraMissionIndex index;
    index._origin = VehicleReported;
    // A negative report is QGroundControl's "no current index". It is normalised to -1 so that
    // isKnown() is false for every negative value rather than only for -1.
    index._sequenceNumber = (sequenceNumber < 0) ? -1 : sequenceNumber;
    return index;
}

AeroraMissionIndex AeroraMissionIndex::fromPlanViewSelection(int sequenceNumber)
{
    AeroraMissionIndex index;
    index._origin = PlanViewSelection;
    index._sequenceNumber = (sequenceNumber < 0) ? -1 : sequenceNumber;
    return index;
}

AeroraMissionIndex AeroraMissionIndex::resumeCandidate(int sequenceNumber)
{
    AeroraMissionIndex index;
    index._origin = ResumeCandidate;
    index._sequenceNumber = (sequenceNumber < 0) ? -1 : sequenceNumber;
    return index;
}

QString AeroraMissionIndex::originToString(Origin origin)
{
    switch (origin) {
    case Unattributed:
        return QStringLiteral("unattributed");
    case VehicleReported:
        return QStringLiteral("vehicleReported");
    case PlanViewSelection:
        return QStringLiteral("planViewSelection");
    case ResumeCandidate:
        return QStringLiteral("resumeCandidate");
    }

    return QStringLiteral("unattributed");
}

QString AeroraMissionIndex::toString() const
{
    return QStringLiteral("%1:%2")
        .arg(originToString(_origin), isKnown() ? QString::number(_sequenceNumber) : QStringLiteral("none"));
}

bool AeroraMissionIndex::operator==(const AeroraMissionIndex &other) const
{
    return (_origin == other._origin) && (_sequenceNumber == other._sequenceNumber);
}

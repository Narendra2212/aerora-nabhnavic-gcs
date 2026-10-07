#include "AeroraTelemetrySource.h"

AeroraTelemetrySource AeroraTelemetrySource::fromSendingComponent(int systemId, int componentId)
{
    AeroraTelemetrySource source;
    source._systemId = systemId;
    source._componentId = componentId;
    source._attribution = SendingComponent;

    return source;
}

QString AeroraTelemetrySource::toString() const
{
    switch (_attribution) {
    case Unidentified:
        return QStringLiteral("unidentified");
    case SendingComponent:
        return QStringLiteral("sending:%1.%2").arg(_systemId).arg(_componentId);
    case OriginatingComponent:
        return QStringLiteral("originating:%1.%2").arg(_systemId).arg(_componentId);
    }

    return QStringLiteral("unidentified");
}

bool AeroraTelemetrySource::operator==(const AeroraTelemetrySource &other) const
{
    // An unidentified source carries no meaningful identifiers, so two unidentified sources are
    // equal regardless of what happens to be in their unused fields.
    if ((_attribution == Unidentified) || (other._attribution == Unidentified)) {
        return _attribution == other._attribution;
    }

    return (_attribution == other._attribution) && (_systemId == other._systemId)
        && (_componentId == other._componentId);
}

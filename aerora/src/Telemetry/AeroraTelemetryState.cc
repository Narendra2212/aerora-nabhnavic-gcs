#include "AeroraTelemetryState.h"

#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(AeroraTelemetryLog, "aerora.telemetry")

AeroraTelemetryState::AeroraTelemetryState(QObject *parent)
    : QObject(parent)
{
}

AeroraTelemetryState::~AeroraTelemetryState() = default;

AeroraTelemetryState::Class AeroraTelemetryState::resolve(Kind declaredKind, Observation observation)
{
    // Observation outranks kind for the three conditions that are not the producer's to declare.
    // Ordering matters: an absent capability is reported before anything is observed, and a value
    // that has aged out is stale no matter how authoritative its source was when it arrived.
    switch (observation) {
    case SourceUnavailable:
        return Unavailable;
    case NotObserved:
        return Unknown;
    case ObservedStale:
        return Stale;
    case ObservedFresh:
    case ObservedFreshnessUnevaluated:
        // Freshness that has not been evaluated does not become freshness that passed. The class is
        // the declared kind either way; it is the consumer's obligation to surface the age, and
        // AeroraStateValue exposes freshnessEvaluated so that obligation is checkable.
        return classForKind(declaredKind);
    }

    qCCritical(AeroraTelemetryLog) << "Internal Error: unhandled observation:" << static_cast<int>(observation);
    return Unknown;
}

AeroraTelemetryState::Class AeroraTelemetryState::classForKind(Kind kind)
{
    switch (kind) {
    case KindReported:
        return Reported;
    case KindEffective:
        return Effective;
    case KindRequested:
        return Requested;
    case KindProposed:
        return Proposed;
    }

    qCCritical(AeroraTelemetryLog) << "Internal Error: unhandled kind:" << static_cast<int>(kind);
    return Unknown;
}

bool AeroraTelemetryState::isAuthoritative(Class stateClass)
{
    return stateClass == Effective;
}

bool AeroraTelemetryState::isCurrent(Class stateClass)
{
    return (stateClass == Reported) || (stateClass == Effective);
}

bool AeroraTelemetryState::isSubordinate(Class stateClass)
{
    return (stateClass == Requested) || (stateClass == Proposed);
}

bool AeroraTelemetryState::carriesValue(Class stateClass)
{
    return !isNonValue(stateClass);
}

bool AeroraTelemetryState::isNonValue(Class stateClass)
{
    return (stateClass == Unknown) || (stateClass == Unavailable);
}

QString AeroraTelemetryState::classToString(Class stateClass)
{
    switch (stateClass) {
    case Unknown:
        return QStringLiteral("UNKNOWN");
    case Unavailable:
        return QStringLiteral("UNAVAILABLE");
    case Reported:
        return QStringLiteral("REPORTED");
    case Effective:
        return QStringLiteral("EFFECTIVE");
    case Requested:
        return QStringLiteral("REQUESTED");
    case Proposed:
        return QStringLiteral("PROPOSED");
    case Stale:
        return QStringLiteral("STALE");
    }

    qCCritical(AeroraTelemetryLog) << "Internal Error: unhandled state class:" << static_cast<int>(stateClass);
    return QStringLiteral("UNKNOWN");
}

QString AeroraTelemetryState::kindToString(Kind kind)
{
    switch (kind) {
    case KindReported:
        return QStringLiteral("REPORTED");
    case KindEffective:
        return QStringLiteral("EFFECTIVE");
    case KindRequested:
        return QStringLiteral("REQUESTED");
    case KindProposed:
        return QStringLiteral("PROPOSED");
    }

    qCCritical(AeroraTelemetryLog) << "Internal Error: unhandled kind:" << static_cast<int>(kind);
    return QStringLiteral("UNKNOWN");
}

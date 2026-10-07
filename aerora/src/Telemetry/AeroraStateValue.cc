#include "AeroraStateValue.h"

#include "AeroraFreshness.h"
#include "Fact.h"
#include "ScheduledTask.h"

AeroraStateValue::AeroraStateValue(Fact *fact,
                                  AeroraTelemetryState::Kind declaredKind,
                                  const QString &freshnessClass,
                                  AeroraFreshness *freshness,
                                  QObject *parent)
    : QObject(parent)
    , _fact(fact)
    , _declaredKind(declaredKind)
    , _freshnessClass(freshnessClass)
    , _freshness(freshness)
{
    if (!fact) {
        qCWarning(AeroraTelemetryLog) << "Constructed with no Fact; the value will have no datum"
                                     << "freshnessClass:" << freshnessClass;
    }

    if (!freshness) {
        qCWarning(AeroraTelemetryLog) << "Constructed with no freshness evaluator; freshness cannot be evaluated"
                                     << "freshnessClass:" << freshnessClass;
        return;
    }

    // A threshold registered after construction must take effect on the values already bound,
    // otherwise configuration arriving late would leave items permanently unevaluated.
    (void) connect(freshness, &AeroraFreshness::thresholdChanged, this, [this](const QString &changedClass) {
        if (changedClass != _freshnessClass) {
            return;
        }
        _rescheduleExpiry();
        emit stateChanged();
    });
}

AeroraStateValue::~AeroraStateValue() = default;

QString AeroraStateValue::name() const
{
    return _fact ? _fact->name() : QString();
}

AeroraTelemetryState::Observation AeroraStateValue::_observation() const
{
    if (_receiveTimeUs == 0) {
        _staleVerdictLatched = false;
        return AeroraTelemetryState::NotObserved;
    }

    if (!_freshness) {
        // The evaluator is gone, so no new verdict can be reached. A verdict already reached is not
        // withdrawn: losing the ability to evaluate must not turn a stale value back into a current
        // one. See the latch note in the header.
        return _staleVerdictLatched ? AeroraTelemetryState::ObservedStale
                                    : AeroraTelemetryState::ObservedFreshnessUnevaluated;
    }

    const AeroraTelemetryState::Observation observation =
        _freshness->evaluate(_freshnessClass, _receiveTimeUs);

    switch (observation) {
    case AeroraTelemetryState::ObservedStale:
        _staleVerdictLatched = true;
        break;
    case AeroraTelemetryState::ObservedFresh:
        // Only an evaluation against a live threshold withdraws the verdict, and it can only do so
        // because a newer receive time put the value back inside the window.
        _staleVerdictLatched = false;
        break;
    case AeroraTelemetryState::ObservedFreshnessUnevaluated:
        // Reachable while a value is still observed if the threshold for its class was cleared.
        // Removing a threshold is not evidence the value became current again.
        if (_staleVerdictLatched) {
            return AeroraTelemetryState::ObservedStale;
        }
        break;
    case AeroraTelemetryState::SourceUnavailable:
    case AeroraTelemetryState::NotObserved:
        break;
    }

    return observation;
}

AeroraTelemetryState::Class AeroraStateValue::stateClass() const
{
    // Capability absence is the only input that produces UNAVAILABLE, and it outranks observation:
    // an aircraft that reports it lacks the capability has nothing to be stale or fresh about.
    // CapabilityUnknown deliberately falls through, so an unreported item resolves to UNKNOWN.
    if (_capability == CapabilityAbsent) {
        return AeroraTelemetryState::Unavailable;
    }

    return AeroraTelemetryState::resolve(_declaredKind, _observation());
}

bool AeroraStateValue::freshnessEvaluated() const
{
    const AeroraTelemetryState::Observation observation = _observation();

    return (observation == AeroraTelemetryState::ObservedFresh)
        || (observation == AeroraTelemetryState::ObservedStale);
}

qint64 AeroraStateValue::ageMs() const
{
    if (!_freshness) {
        return -1;
    }

    return _freshness->ageMs(_receiveTimeUs);
}

void AeroraStateValue::setCapability(Capability capability)
{
    if (_capability == capability) {
        return;
    }

    _capability = capability;

    emit stateChanged();
}

void AeroraStateValue::recordObservation(quint64 receiveTimeUs, const AeroraTelemetrySource &source)
{
    if (receiveTimeUs == 0) {
        // Zero is the encoding of "never observed". Storing it here would silently un-observe an
        // item that had a real reading, which is worse than refusing the call.
        qCWarning(AeroraTelemetryLog) << "Refusing an observation with a zero receive time"
                                     << "name:" << name()
                                     << "freshnessClass:" << _freshnessClass;
        return;
    }

    _receiveTimeUs = receiveTimeUs;
    _source = source;
    // A new report is a new life for this item: whatever staleness verdict the previous receive
    // time earned no longer describes anything.
    _staleVerdictLatched = false;

    _rescheduleExpiry();

    emit stateChanged();
}

void AeroraStateValue::clearObservation()
{
    if ((_receiveTimeUs == 0) && !_source.isIdentified()) {
        return;
    }

    _receiveTimeUs = 0;
    _source = AeroraTelemetrySource();
    _staleVerdictLatched = false;

    if (_expiryTask) {
        _expiryTask->cancel();
    }

    emit stateChanged();
}

void AeroraStateValue::_rescheduleExpiry()
{
    if (_expiryTask) {
        _expiryTask->cancel();
    }

    if (!_freshness || (_receiveTimeUs == 0)) {
        return;
    }

    const std::optional<std::chrono::microseconds> threshold = _freshness->threshold(_freshnessClass);
    if (!threshold.has_value()) {
        // No threshold, so nothing expires and there is no transition to announce. The value stays
        // ObservedFreshnessUnevaluated and consumers read the age directly.
        return;
    }

    if (!_expiryTask) {
        _expiryTask = std::make_unique<ScheduledTask>(_freshness->scheduler(), this);
    }

    // Fire at the moment the value crosses its threshold so a QML binding sees STALE without
    // polling. Already-expired observations get a zero delay, which the scheduler defers to its
    // next dispatch rather than dropping.
    const std::chrono::microseconds remaining = _freshness->remainingUs(_receiveTimeUs, threshold.value());

    (void) _expiryTask->schedule(remaining, [this]() {
        // Evaluate before signalling, so the staleness verdict is recorded at the instant it
        // becomes true rather than whenever a consumer next happens to read. Without this the latch
        // would depend on someone having queried, and an evaluator destroyed before the first read
        // would take the verdict with it.
        (void) _observation();
        emit stateChanged();
    });
}

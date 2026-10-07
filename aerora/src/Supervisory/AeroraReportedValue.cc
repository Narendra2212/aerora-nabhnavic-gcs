#include "AeroraReportedValue.h"

#include "AeroraFreshness.h"
#include "QGCLoggingCategory.h"
#include "ScheduledTask.h"

QGC_LOGGING_CATEGORY(AeroraSupervisoryLog, "aerora.supervisory")

AeroraReportedValue::AeroraReportedValue(const QString &itemId,
                                         AeroraTelemetryState::Kind declaredKind,
                                         const QString &freshnessClass,
                                         AeroraFreshness *freshness,
                                         QObject *parent)
    : QObject(parent)
    , _itemId(itemId)
    , _declaredKind(declaredKind)
    , _freshnessClass(freshnessClass)
    , _freshness(freshness)
{
    if (itemId.isEmpty()) {
        qCWarning(AeroraSupervisoryLog) << "Constructed with no item id; logs and records cannot name this item";
    }

    if (!freshness) {
        qCWarning(AeroraSupervisoryLog) << "Constructed with no freshness evaluator; freshness cannot be evaluated"
                                        << "itemId:" << itemId;
        return;
    }

    // A threshold registered after construction must take effect on the items already created,
    // otherwise configuration arriving late would leave them permanently unevaluated.
    (void) connect(freshness, &AeroraFreshness::thresholdChanged, this, [this](const QString &changedClass) {
        if (changedClass != _freshnessClass) {
            return;
        }
        _rescheduleExpiry();
        emit reportChanged();
    });
}

AeroraReportedValue::~AeroraReportedValue() = default;

AeroraTelemetryState::Observation AeroraReportedValue::_observation() const
{
    if (_receiveTimeUs == 0) {
        // Nothing observed, so there is no reading for a staleness verdict to describe.
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

    const AeroraTelemetryState::Observation observation = _freshness->evaluate(_freshnessClass, _receiveTimeUs);

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

AeroraTelemetryState::Class AeroraReportedValue::stateClass() const
{
    // Capability absence is the only input that produces Unavailable, and it outranks observation.
    // CapabilityUnknown deliberately falls through, so an item nothing has been reported about
    // resolves to Unknown rather than Unavailable.
    if (_capability == AeroraStateValue::CapabilityAbsent) {
        return AeroraTelemetryState::Unavailable;
    }

    return AeroraTelemetryState::resolve(_declaredKind, _observation());
}

bool AeroraReportedValue::freshnessEvaluated() const
{
    const AeroraTelemetryState::Observation observation = _observation();

    return (observation == AeroraTelemetryState::ObservedFresh)
        || (observation == AeroraTelemetryState::ObservedStale);
}

qint64 AeroraReportedValue::ageMs() const
{
    if (!_freshness) {
        return -1;
    }

    return _freshness->ageMs(_receiveTimeUs);
}

void AeroraReportedValue::setCapability(AeroraStateValue::Capability capability)
{
    if (_capability == capability) {
        return;
    }

    _capability = capability;

    emit reportChanged();
}

void AeroraReportedValue::recordReport(const QString &reportedToken,
                                       quint64 receiveTimeUs,
                                       const AeroraTelemetrySource &source)
{
    if (receiveTimeUs == 0) {
        // Zero is the encoding of "never observed". Storing it would silently un-observe an item
        // that had a real reading, which is worse than refusing the call.
        qCWarning(AeroraSupervisoryLog) << "Refusing a report with a zero receive time"
                                        << "itemId:" << _itemId;
        return;
    }

    _reportedToken = reportedToken;
    _receiveTimeUs = receiveTimeUs;
    _source = source;
    // A new report is a new freshness epoch: whatever staleness verdict the previous receive time
    // earned no longer describes anything. Withdrawn here, before the report becomes observable,
    // so no consumer can read the new value carrying the old verdict.
    _staleVerdictLatched = false;

    _rescheduleExpiry();

    // Emitted only after the token and the stamp are both in place, so no consumer can observe a
    // value without its freshness.
    emit reportChanged();
}

void AeroraReportedValue::clearReport()
{
    if ((_receiveTimeUs == 0) && _reportedToken.isEmpty() && !_source.isIdentified()) {
        return;
    }

    _reportedToken.clear();
    _receiveTimeUs = 0;
    _source = AeroraTelemetrySource();
    // With nothing observed the resolution rule yields UNKNOWN, and a retained verdict would make
    // it STALE instead - a staleness claim about a reading that no longer exists.
    _staleVerdictLatched = false;

    if (_expiryTask) {
        _expiryTask->cancel();
    }

    emit reportChanged();
}

void AeroraReportedValue::_rescheduleExpiry()
{
    if (_expiryTask) {
        _expiryTask->cancel();
    }

    if (!_freshness || (_receiveTimeUs == 0)) {
        return;
    }

    const std::optional<std::chrono::microseconds> threshold = _freshness->threshold(_freshnessClass);
    if (!threshold.has_value()) {
        // No threshold, so nothing expires and there is no transition to announce. The item stays
        // ObservedFreshnessUnevaluated and consumers read the age directly. Q-036.
        return;
    }

    if (!_expiryTask) {
        _expiryTask = std::make_unique<ScheduledTask>(_freshness->scheduler(), this);
    }

    // Fire at the moment the item crosses its threshold so a binding sees Stale without polling.
    const std::chrono::microseconds remaining = _freshness->remainingUs(_receiveTimeUs, threshold.value());

    (void) _expiryTask->schedule(remaining, [this]() {
        // Evaluate before signalling, so the staleness verdict is latched at the instant the
        // transition occurs rather than whenever a consumer next happens to read. Without this the
        // latch would depend on someone having queried, and an evaluator destroyed before the first
        // read would take the verdict with it.
        (void) _observation();
        emit reportChanged();
    });
}

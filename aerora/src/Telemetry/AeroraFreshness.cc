#include "AeroraFreshness.h"

#include <algorithm>

#include "MonotonicClock.h"
#include "QtRuntimeScheduler.h"

AeroraFreshness::AeroraFreshness(RuntimeScheduler *scheduler, QObject *parent)
    : QObject(parent)
    , _scheduler(scheduler ? scheduler : new QtRuntimeScheduler(this))
{
}

AeroraFreshness::~AeroraFreshness() = default;

void AeroraFreshness::setThreshold(const QString &freshnessClass, std::chrono::microseconds threshold)
{
    if (freshnessClass.isEmpty()) {
        qCWarning(AeroraTelemetryLog) << "Ignoring threshold for an empty freshness class";
        return;
    }

    if (threshold.count() <= 0) {
        qCWarning(AeroraTelemetryLog) << "Ignoring non-positive freshness threshold"
                                     << "freshnessClass:" << freshnessClass
                                     << "thresholdUs:" << static_cast<qint64>(threshold.count());
        return;
    }

    const auto existing = _thresholds.constFind(freshnessClass);
    if ((existing != _thresholds.constEnd()) && (*existing == threshold)) {
        return;
    }

    _thresholds.insert(freshnessClass, threshold);
    // A class that was previously unconfigured may legitimately warn again if it is later cleared.
    _unconfiguredClassesWarned.remove(freshnessClass);

    qCDebug(AeroraTelemetryLog) << "freshnessClass:" << freshnessClass
                                << "thresholdUs:" << static_cast<qint64>(threshold.count());

    emit thresholdChanged(freshnessClass);
}

void AeroraFreshness::clearThreshold(const QString &freshnessClass)
{
    if (_thresholds.remove(freshnessClass) == 0) {
        return;
    }

    emit thresholdChanged(freshnessClass);
}

bool AeroraFreshness::hasThreshold(const QString &freshnessClass) const
{
    return _thresholds.contains(freshnessClass);
}

std::optional<std::chrono::microseconds> AeroraFreshness::threshold(const QString &freshnessClass) const
{
    const auto it = _thresholds.constFind(freshnessClass);
    if (it == _thresholds.constEnd()) {
        return std::nullopt;
    }

    return *it;
}

QStringList AeroraFreshness::configuredClasses() const
{
    QStringList classes = _thresholds.keys();
    std::sort(classes.begin(), classes.end());

    return classes;
}

AeroraTelemetryState::Observation AeroraFreshness::evaluate(const QString &freshnessClass,
                                                            quint64 receiveTimeUs) const
{
    // A zero stamp is the absence of an observation, never a fresh one. Same invariant as
    // ReadTimestamp: "A provider's zero timestamp remains unknown; it must not become a fresh receipt."
    if (receiveTimeUs == 0) {
        return AeroraTelemetryState::NotObserved;
    }

    const quint64 now = nowUs();

    // A receipt stamped ahead of the clock means the stamp and the evaluator disagree about their
    // clock domain. That is a defect, and the honest response is to decline the verdict rather than
    // report a fresh or stale value derived from an age that cannot be computed. IF-TLM-010 makes
    // this class of defect detectable by test rather than silent.
    if (receiveTimeUs > now) {
        qCWarning(AeroraTelemetryLog) << "Receive time is ahead of the evaluator clock; freshness not evaluated"
                                     << "freshnessClass:" << freshnessClass
                                     << "receiveTimeUs:" << receiveTimeUs
                                     << "nowUs:" << now;
        return AeroraTelemetryState::ObservedFreshnessUnevaluated;
    }

    const std::optional<std::chrono::microseconds> classThreshold = threshold(freshnessClass);
    if (!classThreshold.has_value()) {
        if (!_unconfiguredClassesWarned.contains(freshnessClass)) {
            _unconfiguredClassesWarned.insert(freshnessClass);
            // Q-036 is open, so this is the expected state of every class today. Warned once per
            // class so the condition is visible without flooding a telemetry-rate log.
            qCWarning(AeroraTelemetryLog) << "No freshness threshold configured; freshness not evaluated"
                                         << "freshnessClass:" << freshnessClass;
        }
        return AeroraTelemetryState::ObservedFreshnessUnevaluated;
    }

    const std::chrono::microseconds remaining =
        MonotonicClock::remaining(receiveTimeUs, now, classThreshold.value());

    return (remaining.count() > 0) ? AeroraTelemetryState::ObservedFresh : AeroraTelemetryState::ObservedStale;
}

qint64 AeroraFreshness::ageMs(quint64 receiveTimeUs) const
{
    return MonotonicClock::ageMilliseconds(receiveTimeUs, nowUs());
}

std::chrono::microseconds AeroraFreshness::remainingUs(quint64 receiveTimeUs,
                                                       std::chrono::microseconds threshold) const
{
    return MonotonicClock::remaining(receiveTimeUs, nowUs(), threshold);
}

quint64 AeroraFreshness::nowUs() const
{
    return _scheduler ? _scheduler->nowUs() : 0;
}

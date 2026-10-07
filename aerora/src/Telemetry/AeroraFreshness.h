#pragma once

#include <chrono>
#include <optional>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtQmlIntegration/QtQmlIntegration>

#include "AeroraTelemetryState.h"
#include "RuntimeScheduler.h"

/// \brief Evaluates telemetry freshness against per-class thresholds, and reports honestly when no
/// threshold exists.
///
/// \c IF-TLM-011 specifies that staleness is evaluated per class, by this module rather than by each
/// fact group, and that **a stale value is never presented as current**. It also specifies that the
/// threshold *values* do not exist: 11-telemetry-icd.md states the per-class thresholds have their
/// "shape specified; values not", and Q-036 is the open question that would supply them.
///
/// \section nodefault Why no default threshold is chosen here
///
/// Picking a number would be the one failure this class exists to prevent. Message rates are not
/// evidenced anywhere, and the ICD is explicit about the consequence of guessing: a wrong threshold
/// produces "either false staleness on a slow link, or an operator waiting for a value that will
/// never update". Both are worse than admitting the verdict has not been made.
///
/// So with no threshold registered for a class, \c evaluate() returns
/// \c ObservedFreshnessUnevaluated. That is a third outcome, not a lenient reading of \c ObservedFresh:
///
/// \li It never claims the value passed a freshness check, because no check ran.
/// \li It never claims the value is stale, because nothing says it is.
/// \li The age is still available from \c ageMs(), so a consumer can show how old the value is - which
///     is what \c DEG-04 and \c IF-UIS-004 require of a value whose currency is in question.
///
/// \c hasThreshold() and \c configuredClasses() exist so the startup self-check (\c IF-UIS-015) can
/// assert which classes are configured in a given build rather than discovering it in flight.
///
/// \section class What a freshness class is
///
/// An opaque caller-supplied name. The ICD specifies that thresholds are per class but does not
/// enumerate the classes, so no enumeration is invented here. Producers pass the name they declare;
/// thresholds are registered against the same names from configuration.
///
/// \section capability What this class does not decide
///
/// \c evaluate() never returns \c SourceUnavailable. Unavailability is a capability statement owned
/// by \c M-Capability, and whether a capability is absent is unrelated to how old a value is.
/// \c AeroraStateValue combines the two. Keeping them apart is what keeps \c UNKNOWN and
/// \c UNAVAILABLE from collapsing into each other.
class AeroraFreshness : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("AeroraFreshness is owned by the telemetry module")

public:
    /// \param scheduler The monotonic clock domain to evaluate in. Pass a deterministic scheduler in
    ///                  tests; when null a \c QtRuntimeScheduler is created and owned by this
    ///                  object. Receive times handed to this class must come from the same domain.
    ///
    ///                  An injected scheduler is not owned and is held through a \c QPointer. If it
    ///                  is destroyed first, \c nowUs() returns zero and \c evaluate() stops
    ///                  reaching verdicts rather than reaching wrong ones; values that already hold
    ///                  a staleness verdict keep it, because \c AeroraStateValue latches it.
    explicit AeroraFreshness(RuntimeScheduler *scheduler = nullptr, QObject *parent = nullptr);
    ~AeroraFreshness() override;

    /// Registers the staleness threshold for a class. Supplied by configuration, never by this module.
    ///
    /// A non-positive threshold is rejected with a warning rather than stored, because a zero or
    /// negative lifetime would make every value instantly stale - which looks like a threshold being
    /// enforced while actually being a configuration error.
    void setThreshold(const QString &freshnessClass, std::chrono::microseconds threshold);

    /// Removes a threshold, returning the class to \c ObservedFreshnessUnevaluated.
    void clearThreshold(const QString &freshnessClass);

    [[nodiscard]] bool hasThreshold(const QString &freshnessClass) const;

    /// \return The configured threshold, or \c std::nullopt when the class has none.
    [[nodiscard]] std::optional<std::chrono::microseconds> threshold(const QString &freshnessClass) const;

    /// Every class with a registered threshold, sorted. For the startup self-check.
    [[nodiscard]] QStringList configuredClasses() const;

    /// Evaluates one receive time against its class threshold.
    ///
    /// \param freshnessClass The producer-declared class name.
    /// \param receiveTimeUs  Receive time in this object's clock domain, stamped at parse. Zero means
    ///                       nothing has been observed - a zero stamp never becomes a fresh receipt,
    ///                       matching the invariant in \c src/Utilities/IO/ReadTimestamp.h.
    ///
    /// \return \c NotObserved for a zero stamp; \c ObservedFreshnessUnevaluated when the class has no
    ///         threshold or the stamp is in the future (a clock-domain defect, warned about, never
    ///         resolved into a fresh or stale verdict); otherwise \c ObservedFresh or \c ObservedStale.
    [[nodiscard]] AeroraTelemetryState::Observation evaluate(const QString &freshnessClass,
                                                             quint64 receiveTimeUs) const;

    /// \return Age in milliseconds, or -1 when the stamp is absent or in the future. A value with no
    ///         valid age must be presented without one rather than with a fabricated zero.
    [[nodiscard]] qint64 ageMs(quint64 receiveTimeUs) const;

    /// Current time in this object's clock domain. Producers stamp with this at parse.
    [[nodiscard]] quint64 nowUs() const;

    /// Time remaining before a receipt crosses the given threshold. Zero when the receipt is absent,
    /// in the future, or already past the threshold - an expired receipt never acquires a new
    /// lifetime.
    [[nodiscard]] std::chrono::microseconds remainingUs(quint64 receiveTimeUs,
                                                       std::chrono::microseconds threshold) const;

    /// The clock domain this evaluator reads. Exposed so a bound value can schedule its own
    /// staleness-expiry callback in the same domain rather than inventing a second one.
    [[nodiscard]] RuntimeScheduler *scheduler() const { return _scheduler; }

signals:
    /// Emitted when a threshold is registered, changed or cleared, so cached evaluations can be
    /// recomputed. Carries the class whose threshold changed.
    void thresholdChanged(const QString &freshnessClass);

private:
    QPointer<RuntimeScheduler> _scheduler;
    QHash<QString, std::chrono::microseconds> _thresholds;

    /// Classes already warned about, so an unconfigured threshold is reported once rather than on
    /// every sample. Mutable because the warning is a side effect of a logically const query.
    mutable QSet<QString> _unconfiguredClassesWarned;
};

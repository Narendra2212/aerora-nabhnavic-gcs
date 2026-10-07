#pragma once

#include <memory>

#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtQmlIntegration/QtQmlIntegration>

#include "AeroraStateValue.h"
#include "AeroraTelemetrySource.h"
#include "AeroraTelemetryState.h"

/// Shared by the payload and power abstractions. Defined in AeroraReportedValue.cc.
Q_DECLARE_LOGGING_CATEGORY(AeroraSupervisoryLog)

class AeroraFreshness;
class ScheduledTask;

/// \brief One vehicle-reported item whose value has no evidenced representation yet: the state
/// class, the receive time and the reporting source, carried around an opaque reported token.
///
/// \section why Why this exists instead of an AeroraStateValue
///
/// `M-Telemetry`'s \c AeroraStateValue is the right type for an Aerora value and this is not a
/// replacement for it. It binds a \c Fact, and a \c Fact is unusable outside a fact group with a
/// metadata resource: \c Fact::setRawValue is a no-op that warns when \c _metaData is null
/// (\c src/FactSystem/Fact.cc:135-161). A metadata file requires fact names, units, bounds and
/// enumeration members, and for payload and power **none of those is specified anywhere** — Q-023
/// and Q-024. Declaring them would be inventing them, which is why \c AeroraPayloadFactGroup and
/// \c AeroraPowerFactGroup remain \c CONTRACT-ONLY.
///
/// What *is* evidenced for both domains is the **separation** the ground station must preserve:
/// which items are distinct from each other, which is authoritative, which is merely reported, and
/// that nothing may be inferred. That separation is expensive to retrofit and cheap to establish
/// now, so it is established here, over a token rather than a typed value.
///
/// \section notasecondmodel What this is not
///
/// It is not a second state model. There is exactly one resolution rule in the build and this type
/// calls it: \c stateClass() delegates to \c AeroraTelemetryState::resolve(), freshness to
/// \c AeroraFreshness::evaluate(), the capability tri-state is \c AeroraStateValue::Capability, and
/// the reporting source is \c AeroraTelemetrySource. Nothing is reimplemented.
///
/// When a dialect lands, each item migrates to a \c Fact plus an \c AeroraStateValue inside its
/// fact group, and this type's consumers change from reading \c reportedToken() to reading
/// \c fact(). The state semantics do not change, because they were never owned here. This type is
/// also a reasonable candidate for absorption into \c M-Telemetry if Tab 1 prefers one home for
/// the state binding; it lives under \c Supervisory/ only so that neither \c M-Payload nor
/// \c M-Power depends on the other. **It is not a new module.**
///
/// \section token What the reported token is, and is not
///
/// Whatever the vehicle reported, carried verbatim as text. It is **not** parsed, compared against
/// a known set, mapped to a meaning, ranged, or defaulted. \c IF-PAY-003 and \c IF-PWR-003 both
/// record their enumeration members as \c UNKNOWN, and 24-phase-3.1-correction-audit.md's isolation
/// strategy for an unknown enumeration is to show an unrecognised value raw. An empty token means
/// nothing has been reported; it never means "nominal", "off", "healthy" or zero.
///
/// \section ordering The stamping order a producer must follow
///
/// One call does both, so the order cannot be got wrong:
///
/// \code
/// const quint64 receiveTimeUs = freshness->nowUs();   // read the clock once, at parse
/// value->recordReport(decodedToken, receiveTimeUs, AeroraTelemetrySource::fromSendingComponent(sysId, compId));
/// \endcode
///
/// \c reportChanged is emitted after both the token and the stamp are in place, so a consumer
/// reacting to it never sees a value without its freshness.
///
/// \section evaluatorloss What happens when the evaluator goes away
///
/// The evaluator is owned by \c M-Telemetry's \c AeroraTelemetrySet and borrowed here. On a vehicle
/// teardown the set is destroyed **before** the payload and power models that borrow it, because
/// \c AeroraVehicleServices attaches it first and Qt deletes children in attachment order. So this
/// window is not exotic: it happens on every disconnect.
///
/// | Concern | Behaviour once \c _freshness is null |
/// | --- | --- |
/// | \c ageMs() | Returns -1. No age is fabricated from a clock that no longer exists |
/// | No staleness verdict yet | \c ObservedFreshnessUnevaluated — still not a claim of freshness |
/// | Already judged \c STALE | **Stays \c STALE.** The verdict is latched |
/// | A pending expiry callback | Suppressed. \c ScheduledTask holds the scheduler by \c QPointer |
///
/// The third row is the one that matters, and it is why \c _staleVerdictLatched exists. Without it,
/// destroying the evaluator would resolve a stale value back to \c REPORTED or \c EFFECTIVE and
/// \c AeroraTelemetryState::isCurrent() would answer true for a reading known to be old — a stale
/// value silently treated as current, which is exactly what \c DEG-04 and \c IF-TLM-011 forbid.
///
/// The latch is deliberately one-directional and narrow. Evaluator absence alone **never** means
/// \c STALE: the latch can only be set by an evaluation that actually reached \c ObservedStale
/// against a configured threshold, so it cannot make a value stale that was never judged stale. It
/// withdraws only on a new report or a clear, and it introduces no threshold of its own.
///
/// This mirrors the latch \c M-Telemetry implements in \c AeroraStateValue, deliberately: the two
/// types carry the same observation semantics over different value stores, and a divergence between
/// them would be a defect that only showed up in one of the two domains.
///
/// \section safety What this type must never do
///
/// It reports a condition and decides nothing. There is no path from a token, a staleness
/// transition, a capability becoming absent, or an observation being cleared, to any command, mode
/// change, disarm, land, return-to-launch, payload action or power action.
/// 18-error-degradation-model.md records "none" in the authority column of every aircraft-reported
/// condition.
class AeroraReportedValue : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("AeroraReportedValue is created by the payload or power model that owns it")

    Q_PROPERTY(QString itemId READ itemId CONSTANT)
    Q_PROPERTY(AeroraTelemetryState::Kind declaredKind READ declaredKind CONSTANT)
    Q_PROPERTY(QString freshnessClass READ freshnessClass CONSTANT)

    Q_PROPERTY(QString reportedToken READ reportedToken NOTIFY reportChanged)
    Q_PROPERTY(AeroraTelemetryState::Class stateClass READ stateClass NOTIFY reportChanged)
    Q_PROPERTY(bool freshnessEvaluated READ freshnessEvaluated NOTIFY reportChanged)
    Q_PROPERTY(bool observed READ isObserved NOTIFY reportChanged)
    Q_PROPERTY(quint64 receiveTimeUs READ receiveTimeUs NOTIFY reportChanged)
    Q_PROPERTY(AeroraTelemetrySource source READ source NOTIFY reportChanged)
    Q_PROPERTY(AeroraStateValue::Capability capability READ capability NOTIFY reportChanged)

public:
    /// \param itemId         Stable machine-readable identifier for this item, conventionally its
    ///                       interface-contract id. For logs, records and diagnostics. **Not** an
    ///                       operator-facing label, and **not** a protocol value: it is never sent,
    ///                       parsed or matched against a MAVLink field.
    /// \param declaredKind   The item's kind from its interface contract. Fixed for the item's
    ///                       life — see \c declaredKind().
    /// \param freshnessClass The class name whose threshold governs this item's staleness. An
    ///                       opaque producer-declared name, as \c M-Telemetry defines it.
    /// \param freshness      The evaluator, shared per vehicle. Not owned; must outlive this
    ///                       object.
    /// \param parent         The owning model.
    explicit AeroraReportedValue(const QString &itemId,
                                 AeroraTelemetryState::Kind declaredKind,
                                 const QString &freshnessClass,
                                 AeroraFreshness *freshness,
                                 QObject *parent = nullptr);
    ~AeroraReportedValue() override;

    [[nodiscard]] QString itemId() const { return _itemId; }

    /// The item's declared kind, \c const with no setter.
    ///
    /// The absence of a setter is the guarantee. A \c REQUESTED or \c PROPOSED item cannot become
    /// an \c EFFECTIVE one by any call sequence, because the API that would do it does not exist.
    /// For payload that is \c AERORA-PAYLOAD-REQ-011 (an acknowledgement is not an effect); for
    /// power it is what keeps \c IF-PWR-003's effective source separate from the last report.
    [[nodiscard]] AeroraTelemetryState::Kind declaredKind() const { return _declaredKind; }

    [[nodiscard]] QString freshnessClass() const { return _freshnessClass; }

    /// The vehicle's reported value, verbatim. Empty until something has been reported, and an
    /// empty token is never a value — read \c stateClass() to tell the difference.
    [[nodiscard]] QString reportedToken() const { return _reportedToken; }

    /// The class to present now, resolved from capability, observation and freshness.
    ///
    /// Recomputed on each call against the current clock, so an item that has aged past a
    /// configured threshold reports \c Stale without anything having signalled.
    ///
    /// \return Never \c Effective unless \c declaredKind() is \c KindEffective.
    [[nodiscard]] AeroraTelemetryState::Class stateClass() const;

    /// False when no freshness verdict has been reached — because nothing has been observed, or
    /// because the item's class has no configured threshold (Q-036).
    ///
    /// A consumer presenting a \c Reported or \c Effective value while this is false must surface
    /// the age: the currency of the value has not been established.
    [[nodiscard]] bool freshnessEvaluated() const;

    /// True once a report has been observed for this item.
    [[nodiscard]] bool isObserved() const { return _receiveTimeUs != 0; }

    /// Receive time in the evaluator's clock domain, stamped at parse. Zero means never observed.
    [[nodiscard]] quint64 receiveTimeUs() const { return _receiveTimeUs; }

    [[nodiscard]] AeroraTelemetrySource source() const { return _source; }

    /// Age in milliseconds, or -1 when never observed or when the stamp cannot be aged.
    ///
    /// A function rather than a notifying property: age advances continuously with no event to
    /// signal, so a consumer displaying it must re-read it.
    [[nodiscard]] Q_INVOKABLE qint64 ageMs() const;

    [[nodiscard]] AeroraStateValue::Capability capability() const { return _capability; }

    /// Set by the capability model from a vehicle report. Never inferred from telemetry arriving or
    /// failing to arrive: \c CapabilityAbsent is the only input that produces \c Unavailable, and
    /// manufacturing one the vehicle never reported is the derivation \c AERORA-GCS-REQ-029
    /// prohibits.
    void setCapability(AeroraStateValue::Capability capability);

    /// Records a report observed from the vehicle.
    ///
    /// \param reportedToken The vehicle's value, verbatim. May be empty when the report carries no
    ///                      value for this item; the observation still counts, because "reported
    ///                      with nothing in it" and "never reported" are different facts.
    /// \param receiveTimeUs Receive time in the evaluator's clock domain. A zero stamp is refused
    ///                      with a warning rather than stored: zero encodes "never observed", and
    ///                      storing one would silently erase a real prior observation.
    /// \param source        The observed reporting source, attributed to the sending component.
    ///
    /// A new report opens a new freshness epoch: any staleness verdict the previous receive time
    /// earned is withdrawn **before** the new report becomes observable, because that verdict
    /// described a reading this one replaces.
    void recordReport(const QString &reportedToken, quint64 receiveTimeUs, const AeroraTelemetrySource &source);

    /// Returns the item to never-observed, clears the token, and withdraws any staleness verdict.
    ///
    /// For connection teardown and identity rebinding, where retaining the previous aircraft's
    /// report would attribute it to the new one. The token is cleared as well as the stamp because
    /// an un-aged token has no honest presentation: unlike a \c Fact, which \c DEG-03 requires keep
    /// its last value with its age, there is no metadata here to present a stale token with.
    ///
    /// The resulting state is \c UNKNOWN, not \c STALE: with nothing observed there is no reading
    /// for a staleness verdict to describe, so the latch is cleared rather than carried across.
    ///
    /// This is also the only reset path. There is deliberately no save, restore, serialise or
    /// replay: payload state persistence across an interruption is \c NOT-EVIDENCED
    /// (\c IF-PAY-014), and the mission side pins the same position in
    /// \c AeroraMissionResumeBoundary.
    void clearReport();

signals:
    /// Emitted when the token, the observation, the source or the capability changes, and at the
    /// instant a configured freshness threshold expires.
    void reportChanged();

private:
    /// Reschedules the expiry callback that makes a fresh-to-stale transition observable without
    /// polling. A no-op when the class has no configured threshold: with nothing to expire
    /// against there is no transition to announce.
    void _rescheduleExpiry();

    /// What has been observed, before capability is applied.
    [[nodiscard]] AeroraTelemetryState::Observation _observation() const;

    const QString _itemId;
    const AeroraTelemetryState::Kind _declaredKind;
    const QString _freshnessClass;
    QPointer<AeroraFreshness> _freshness;

    QString _reportedToken;
    quint64 _receiveTimeUs = 0;
    AeroraTelemetrySource _source;
    AeroraStateValue::Capability _capability = AeroraStateValue::CapabilityUnknown;

    /// True once this item has been judged \c ObservedStale against a configured threshold, and
    /// until a new report or a clear withdraws it.
    ///
    /// **Invariant: once a report has been judged \c STALE against a configured threshold,
    /// destruction or loss of the evaluator must not cause the report to become current again.**
    ///
    /// Mutable because the verdict is recorded during the logically const \c _observation() query.
    /// Set only by an evaluation that reached \c ObservedStale, so it can never make a value stale
    /// that was never judged stale, and evaluator absence on its own does not set it.
    mutable bool _staleVerdictLatched = false;

    std::unique_ptr<ScheduledTask> _expiryTask;
};

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtQmlIntegration/QtQmlIntegration>

#include <memory>

#include "AeroraTelemetrySource.h"
#include "AeroraTelemetryState.h"

class AeroraFreshness;
class Fact;
class ScheduledTask;

/// \brief Binds one Fact to its state class: the value triple, as a consumable object.
///
/// This is the \c AeroraStateValue named in 05-qml-implementation-map.md as \c M-Telemetry's
/// contribution to state-class rendering, and it exists to make one rule structural rather than
/// procedural:
///
/// > The state class is a property of the binding, not a decision made per screen.
///
/// A screen author cannot accidentally render a companion proposal as effective aircraft state,
/// because the class is not theirs to choose - it is resolved here from the item's declared kind and
/// what has actually been observed. A rule each screen must remember is a rule one screen will
/// forget, and the forgetting is invisible.
///
/// \section split What this holds, and what the Fact holds
///
/// The Fact remains the single store for the value itself (\c AERORA-GCS-REQ-026: every Aerora value
/// is a Fact, with units, bounds and enumerations from metadata rather than code). There is no
/// parallel value store here. What this object adds is precisely what \c Fact has no room for and
/// what \c AERORA-GCS-REQ-027 requires be carried without reduction:
///
/// \li the receive time, stamped at parse,
/// \li the reporting source,
/// \li the declared kind, and the class resolved from it.
///
/// \section kind Why the declared kind is const
///
/// \c _declaredKind is set once at construction, from the interface contract's "Kind" column, and
/// there is no setter. A proposal cannot become effective state by any sequence of calls, because
/// the API that would do it does not exist. The same applies to a request. This is the defence
/// against the failure 11-telemetry-icd.md identifies as the one the operator pays for.
///
/// \section ordering The stamping order a producer must follow
///
/// Freshness must be in place before a consumer sees the value, so a fact group's \c handleMessage
/// stamps first and sets the Fact second:
///
/// \code
/// void MyFactGroup::handleMessage(Vehicle *vehicle, const mavlink_message_t &message)
/// {
///     if (message.msgid != MAVLINK_MSG_ID_SOMETHING) {
///         return;
///     }
///     const quint64 receiveTimeUs = _stampNowUs();       // read the clock once, at parse
///     // ... decode ...
///     _recordObservation(&_someValue, message, receiveTimeUs);   // stamp + source first
///     _someValue.fact()->setRawValue(decoded.field);             // then the value becomes visible
///     _setTelemetryAvailable(true);
/// }
/// \endcode
///
/// With a fact group update rate of zero, \c setRawValue emits \c valueChanged synchronously, so this
/// order is what makes "stamped at parse, before any consumer sees the value" true rather than
/// merely intended. \c AeroraFactGroupBase provides both helpers and the unit test asserts the order.
///
/// \anchor lifetime
/// \section lifetime Lifetime: expected ordering, and what happens without it
///
/// In the composed graph the evaluator outlives every value bound to it. The \c AeroraTelemetrySet
/// that owns the evaluator is attached to \c AeroraVehicleServices first, so it is that object's
/// first child and therefore its **last** destroyed; the groups and models holding values attach
/// later and are destroyed earlier. Both are reparented under the \c Vehicle, so the whole graph
/// goes away with the vehicle and no participant has teardown code.
///
/// That ordering is the expectation, not an assumption this type relies on. \c _fact and
/// \c _freshness are \c QPointer, so an out-of-order destruction degrades rather than dangles:
///
/// | If the evaluator is destroyed first | Behaviour |
/// | --- | --- |
/// | \c ageMs() | Returns -1. No age is fabricated from a clock that no longer exists |
/// | No staleness verdict yet | \c ObservedFreshnessUnevaluated — still not a claim of freshness |
/// | Already judged \c STALE | **Stays \c STALE.** The verdict is latched |
/// | A pending expiry callback | Suppressed. \c ScheduledTask holds the scheduler by \c QPointer |
///
/// The third row is the one that matters and it is why the latch exists. Without it, destroying the
/// evaluator would move a stale value back to \c REPORTED or \c EFFECTIVE, which is precisely
/// "a stale value silently treated as current" — the thing this module exists to prevent. The latch
/// withdraws only on a new observation, and it introduces no threshold: it cannot make a value stale
/// that was never judged stale against a configured threshold.
class AeroraStateValue : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("AeroraStateValue is created by the fact group that owns its Fact")

    Q_PROPERTY(Fact *fact READ fact CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(AeroraTelemetryState::Kind declaredKind READ declaredKind CONSTANT)
    Q_PROPERTY(QString freshnessClass READ freshnessClass CONSTANT)

    Q_PROPERTY(AeroraTelemetryState::Class stateClass READ stateClass NOTIFY stateChanged)
    Q_PROPERTY(bool freshnessEvaluated READ freshnessEvaluated NOTIFY stateChanged)
    Q_PROPERTY(bool observed READ isObserved NOTIFY stateChanged)
    Q_PROPERTY(quint64 receiveTimeUs READ receiveTimeUs NOTIFY stateChanged)
    Q_PROPERTY(AeroraTelemetrySource source READ source NOTIFY stateChanged)
    Q_PROPERTY(Capability capability READ capability NOTIFY stateChanged)

public:
    /// Whether the aircraft has the capability this item depends on.
    ///
    /// Tri-state on purpose. \c IF-UIS-005 and \c IF-UIS-006 are \c BLOCKED under finding F3.1-05
    /// because nothing evidenced reports capability presence separately from capability state
    /// (Q3-009). Collapsing "we have not been told" into "the aircraft does not have it" would
    /// manufacture an \c UNAVAILABLE the vehicle never reported, which is exactly the derivation
    /// \c AERORA-GCS-REQ-029 prohibits.
    enum Capability {
        /// Nothing has been reported about this capability. Resolves through to the observation, so
        /// an unreported item is \c UNKNOWN - not \c UNAVAILABLE. The initial state of every item.
        CapabilityUnknown = 0,
        /// The aircraft reported it has the capability.
        CapabilityPresent,
        /// The aircraft reported it does not have the capability. The only input that produces
        /// \c UNAVAILABLE.
        CapabilityAbsent,
    };
    Q_ENUM(Capability)

    /// \param fact           The Fact holding the value. Not owned. Expected to outlive this
    ///                       object, and held through a \c QPointer so that if it does not, this
    ///                       object reports an empty name and no datum rather than dangling.
    /// \param declaredKind   The item's kind from its interface contract. Fixed for the item's life.
    /// \param freshnessClass The class name whose threshold governs this item's staleness.
    /// \param freshness      The evaluator, shared per vehicle. Not owned. Expected to outlive this
    ///                       object — see \ref lifetime for what happens when it does not.
    /// \param parent         Owning fact group.
    explicit AeroraStateValue(Fact *fact,
                              AeroraTelemetryState::Kind declaredKind,
                              const QString &freshnessClass,
                              AeroraFreshness *freshness,
                              QObject *parent = nullptr);
    ~AeroraStateValue() override;

    [[nodiscard]] Fact *fact() const { return _fact; }

    /// The Fact's name, or an empty string if the Fact has gone away.
    [[nodiscard]] QString name() const;

    [[nodiscard]] AeroraTelemetryState::Kind declaredKind() const { return _declaredKind; }
    [[nodiscard]] QString freshnessClass() const { return _freshnessClass; }

    /// The class to present now, resolved from capability, observation and freshness.
    ///
    /// Recomputed on each call against the current clock, so a value that has aged past its
    /// threshold reports \c STALE even if nothing has signalled. \c stateChanged is additionally
    /// emitted at the expiry instant when a threshold is configured, so QML bindings update without
    /// polling.
    [[nodiscard]] AeroraTelemetryState::Class stateClass() const;

    /// False when no freshness verdict has been reached - either because the item's class has no
    /// configured threshold (Q-036) or because nothing has been observed.
    ///
    /// A consumer that presents a value whose \c stateClass is \c REPORTED or \c EFFECTIVE while this
    /// is false must surface the age; the currency of the value has not been established.
    [[nodiscard]] bool freshnessEvaluated() const;

    /// True once a message has been observed for this item.
    [[nodiscard]] bool isObserved() const { return _receiveTimeUs != 0; }

    /// Receive time in the evaluator's clock domain, stamped at parse. Zero means never observed.
    [[nodiscard]] quint64 receiveTimeUs() const { return _receiveTimeUs; }

    [[nodiscard]] AeroraTelemetrySource source() const { return _source; }

    /// Age in milliseconds, or -1 when never observed or when the stamp cannot be aged.
    ///
    /// Deliberately a function and not a notifying property: age advances continuously with no event
    /// to signal, so a consumer displaying it must re-read it. Presenting a stale age as current
    /// would reintroduce the problem staleness marking exists to solve.
    [[nodiscard]] Q_INVOKABLE qint64 ageMs() const;

    [[nodiscard]] Capability capability() const { return _capability; }

    /// Set by the capability model from a vehicle report. Not inferred from telemetry arriving or
    /// failing to arrive.
    ///
    /// C++ only. The \c capability property is readable from QML and carries no \c WRITE, matching
    /// \c AeroraReportedValue: an interface write could present a reported-absent capability as
    /// available, or manufacture the \c CapabilityAbsent that is the only input producing
    /// \c UNAVAILABLE — the derivation \c AERORA-GCS-REQ-029 and \c IF-UIS-005 prohibit. The
    /// classification is the vehicle's to report, not a screen's to choose, so there is deliberately
    /// no QML path to it: no \c WRITE, no second writable property, no \c Q_INVOKABLE.
    void setCapability(Capability capability);

    /// Records that a message carrying this item was observed. Called at parse, before the Fact's
    /// value is set - see the ordering note in the class documentation.
    ///
    /// \param receiveTimeUs Receive time in the evaluator's clock domain. A zero stamp is refused
    ///                      with a warning rather than stored, because a zero stamp means "never
    ///                      observed" and recording one would erase a real prior observation.
    /// \param source        The observed reporting source.
    void recordObservation(quint64 receiveTimeUs, const AeroraTelemetrySource &source);

    /// Returns the item to never-observed: no value has been reported on this connection.
    ///
    /// For connection teardown and identity rebinding, where retaining the previous aircraft's
    /// reading would attribute it to the new one. This does not clear the Fact; an operator still
    /// sees the last reading, now classed \c UNKNOWN with no age, per \c DEG-03's requirement that
    /// last-known values be retained rather than discarded.
    void clearObservation();

signals:
    /// Emitted when the resolved class, the observation, the source or the capability changes, and at
    /// the instant a configured freshness threshold expires.
    void stateChanged();

private:
    /// Reschedules the expiry callback that makes a fresh-to-stale transition observable. A no-op
    /// when the class has no configured threshold: with nothing to expire against there is no
    /// transition to announce.
    void _rescheduleExpiry();

    /// What has been observed, before capability is applied.
    [[nodiscard]] AeroraTelemetryState::Observation _observation() const;

    QPointer<Fact> _fact;
    const AeroraTelemetryState::Kind _declaredKind;
    const QString _freshnessClass;
    QPointer<AeroraFreshness> _freshness;

    quint64 _receiveTimeUs = 0;
    AeroraTelemetrySource _source;
    Capability _capability = CapabilityUnknown;

    /// True once this observation has been judged \c STALE against a configured threshold.
    ///
    /// Withdrawn only by a new observation or by clearing the observation. It exists so that losing
    /// the ability to evaluate — the evaluator destroyed, or the class threshold cleared — cannot
    /// return a stale value to a current class. Mutable because the verdict is recorded during the
    /// logically const \c _observation() query.
    mutable bool _staleVerdictLatched = false;

    std::unique_ptr<ScheduledTask> _expiryTask;
};

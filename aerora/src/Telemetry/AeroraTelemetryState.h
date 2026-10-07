#pragma once

#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtQmlIntegration/QtQmlIntegration>

/// Shared by every type in the Aerora telemetry module. Defined in AeroraTelemetryState.cc.
Q_DECLARE_LOGGING_CATEGORY(AeroraTelemetryLog)

/// \brief The Aerora telemetry state classes, and the resolution rule that produces one.
///
/// Every Aerora value the operator interface presents belongs to exactly one \c Class, and the class
/// is visible. The seven classes come from 16-ui-state-icd.md (five classes plus the two non-values)
/// and 11-telemetry-icd.md (the reported/effective/proposed separation). They are deliberately not
/// collapsed into each other:
///
/// \li \c Unknown and \c Unavailable are distinct. Collapsing them leaves an operator unable to tell
///     "the payload has nothing to report" from "the payload panel is missing" (\c IF-UIS-005).
/// \li \c Requested and \c Proposed are distinct from \c Effective and are never promoted to it.
///     That promotion is the second architectural principle violated, and in a binding expression it
///     is the easiest mistake in the system to make by accident.
/// \li \c Stale is distinct from \c Reported and \c Effective. A stale value keeps its last reading
///     and its age; it is never presented as current (\c DEG-04).
///
/// \c Kind is the narrower, declarable subset: the "Kind" column of the canonical item model in
/// 11-telemetry-icd.md. A telemetry item's kind is declared once, from the interface contract, and is
/// a \c const property of the binding thereafter. There is no API that turns a \c Proposed item into
/// an \c Effective one, because the type system is a better guarantee than a review rule.
///
/// This class holds no state and performs no I/O. \c resolve() is a pure function over two
/// enumerations so that the whole state model is table-testable without a vehicle, a link or a clock.
class AeroraTelemetryState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    // Creating an instance is only meant to be used for the QML singleton. The class holds no state;
    // every member is an enumeration or a pure function.
    explicit AeroraTelemetryState(QObject *parent = nullptr);
    ~AeroraTelemetryState();

    /// The class a value is presented as. Exactly one applies to any value at any moment.
    enum Class {
        /// The item exists in the interface contract but nothing has been observed for it.
        /// Never rendered as zero, dash-as-zero, or an empty string that reads as a value.
        Unknown = 0,
        /// The capability is reported absent, so the value cannot be obtained at all.
        /// Presented by absence of the element - not as unknown, and not as a disabled control.
        Unavailable,
        /// A component says this is true. Primary, with its age available.
        Reported,
        /// What the aircraft is actually doing, as reported by the deciding authority.
        /// The only authoritative class.
        Effective,
        /// The ground station asked for this and the vehicle has not reported it in force.
        /// Subordinate and labelled. Never promoted to \c Effective.
        Requested,
        /// A non-authoritative source suggests this and nobody has accepted it.
        /// Subordinate and labelled. Never promoted to \c Effective.
        Proposed,
        /// Observed once, now past its freshness threshold. Shown with its age, never as current.
        Stale,
    };
    Q_ENUM(Class)

    /// The declarable kind of a telemetry item, fixed by its interface contract.
    ///
    /// Narrower than \c Class on purpose: \c Stale, \c Unknown and \c Unavailable are conditions a
    /// value falls into at runtime, never things a producer may declare itself to be.
    enum Kind {
        /// A component reports it. The authority for the value is the reporting component.
        KindReported = 0,
        /// The deciding authority reports it in force.
        KindEffective,
        /// The ground station requested it. Produced by the command tracker, not by telemetry parsing.
        KindRequested,
        /// A non-authoritative source proposed it. Produced by companion telemetry.
        KindProposed,
    };
    Q_ENUM(Kind)

    /// What has actually been observed for an item. The second input to \c resolve().
    ///
    /// \c ObservedFreshnessUnevaluated is not a placeholder for "probably fresh". It is the honest
    /// state of every Aerora item today: the receive time is known, and the threshold it would be
    /// compared against is \c OPEN under Q-036. Consumers must surface the age rather than imply a
    /// freshness verdict that has not been made.
    enum Observation {
        /// The capability is reported absent.
        SourceUnavailable = 0,
        /// Nothing has been observed for this item yet.
        NotObserved,
        /// Observed, and within the freshness threshold configured for its class.
        ObservedFresh,
        /// Observed, and past the freshness threshold configured for its class.
        ObservedStale,
        /// Observed, but no freshness threshold is configured, so no verdict has been reached.
        ObservedFreshnessUnevaluated,
    };
    Q_ENUM(Observation)

    /// The state-class resolution rule, in one place.
    ///
    /// Deliberately uniform in \c observation: an absent capability outranks everything, nothing
    /// observed is \c Unknown for every kind, and a stale observation is \c Stale for every kind -
    /// including a stale proposal and a stale effective value. Only an observation that is fresh, or
    /// whose freshness has not been evaluated, resolves to the item's declared kind.
    ///
    /// \return The class to present. Never \c Effective unless \c declaredKind is \c KindEffective.
    [[nodiscard]] Q_INVOKABLE static Class resolve(Kind declaredKind, Observation observation);

    /// The class that corresponds one-to-one with a declared kind, ignoring observation.
    /// Exposed for the record writer, which stores the declared kind alongside the resolved class.
    [[nodiscard]] static Class classForKind(Kind kind);

    /// True only for \c Effective. The deciding authority's report is the only authoritative class.
    [[nodiscard]] Q_INVOKABLE static bool isAuthoritative(Class stateClass);

    /// True for \c Reported and \c Effective only.
    ///
    /// False for \c Stale (aged out), for \c Requested and \c Proposed (never vehicle state), and for
    /// the two non-values. This is the predicate a consumer should use before treating a value as
    /// describing the aircraft now.
    [[nodiscard]] Q_INVOKABLE static bool isCurrent(Class stateClass);

    /// True for \c Requested and \c Proposed: classes that must be presented subordinate to vehicle
    /// state and explicitly labelled.
    [[nodiscard]] Q_INVOKABLE static bool isSubordinate(Class stateClass);

    /// True when the class carries a datum at all. False for \c Unknown and \c Unavailable.
    [[nodiscard]] Q_INVOKABLE static bool carriesValue(Class stateClass);

    /// True for \c Unknown and \c Unavailable - the two ways a class can be empty. They remain
    /// distinguishable; this predicate is for consumers that genuinely treat both as "no datum",
    /// never for collapsing them in a display.
    [[nodiscard]] Q_INVOKABLE static bool isNonValue(Class stateClass);

    /// Stable machine-readable identifier, for logs, telemetry records and audit records.
    ///
    /// Not an operator-facing label. Labels, colours and icons are presentation decisions owned by
    /// \c M-Ui and unresolved under Q-040; nothing here chooses one.
    [[nodiscard]] Q_INVOKABLE static QString classToString(Class stateClass);

    /// Stable machine-readable identifier for a declared kind. Not an operator-facing label.
    [[nodiscard]] Q_INVOKABLE static QString kindToString(Kind kind);
};

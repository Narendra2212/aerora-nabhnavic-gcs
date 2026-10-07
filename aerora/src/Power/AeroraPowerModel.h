#pragma once

#include <QtCore/QList>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtQmlIntegration/QtQmlIntegration>

#include "AeroraReportedValue.h"
#include "AeroraVehicleCapabilities.h"

/// Defined in AeroraPowerModel.cc.
Q_DECLARE_LOGGING_CATEGORY(AeroraPowerLog)

class AeroraFreshness;

/// \brief The product-independent power abstraction: a configuration and state model, with no
/// arbitration, no transition logic and no thresholds.
///
/// \section confirmed What is confirmed, and what follows from it
///
/// Baseline 49 establishes two physical **operating configurations**:
///
/// \code
/// TETHER CONFIGURATION
///   480 V DC from the ground  →  tether  →  onboard DC-DC stage  →  48 V aircraft domain
///
/// BATTERY CONFIGURATION
///   onboard battery  →  aircraft power domain
/// \endcode
///
/// That is an architectural fact about the aircraft. It establishes **nothing** about minimum
/// tether voltage, maximum current, converter efficiency, thermal limits, allowable voltage drop, a
/// transition threshold, or emergency behaviour, and this module defines none of those — see
/// \c kDefinedThresholdCount.
///
/// Baseline 49 also states the two configurations are "not yet a claim of automatic in-flight
/// source switching", and that any future transition or hybrid operation "requires separate
/// engineering and verification". So:
///
/// \code
/// Battery ──┐
///           ├──  AeroraPowerModel  ──►  display, logging
/// Tether  ──┘
/// \endcode
///
/// The model represents what the vehicle reports about each configuration. It does not arbitrate
/// between them, rank them, or move between them.
///
/// \section separation Effective and reported, kept separate
///
/// \c effectiveState() and \c reportedState() answer different questions — "what is in force" and
/// "what was last said" — and they diverge the moment a report goes stale. A single value would
/// force a choice, and it would choose wrongly under exactly the conditions that matter.
/// \c effectiveState() is the only \c KindEffective item here, so it is the only one
/// \c AeroraTelemetryState::isAuthoritative() will ever accept.
///
/// \section notransition Why a reported transition item exists but no inferred one
///
/// \c IF-PWR-004 (source transition) is a real interface item, so \c sourceTransition() exists to
/// carry it — but **only** when the vehicle reports one. Nothing in this class derives a transition
/// from a configuration report changing, from a report going stale, from a fault, or from a
/// watchdog event. A tether configuration becoming absent while a battery configuration becomes
/// present is two reports, not a transition; calling it one would be the ground station asserting
/// an event the aircraft never reported.
///
/// Q-026 is therefore not blocking: if transitions exist the vehicle reports them and the item
/// populates; if they do not, the item is never populated and the display omits it. Either answer
/// fits without redesign.
///
/// \section nocausalfield Why the watchdog event carries no relationship
///
/// \c IF-PWR-011 is carried here because the watchdog is reported *alongside* power state, not
/// because it acts on power. Three candidate relationships — watchdog fault causing a source
/// transition, watchdog fault causing a converter state change, and a power fault causing a
/// watchdog fault — are each \c NOT-EVIDENCED, and 07-power-telemetry-icd.md records that
/// explicitly after Phase 3.1 found the absence had been left to inference.
///
/// So \c watchdogEvent() is a separate item with its own receive time, and there is no field
/// relating it to anything. It is an independent cut, not an enumeration member of a power state:
/// folding it in would also lose the ability to record what the power state was when the watchdog
/// fired.
///
/// \section battery What this does not duplicate
///
/// QGroundControl's battery telemetry is reused unchanged —
/// \c src/Vehicle/FactGroups/BatteryFactGroupListModel.{h,cc}, which parses \c BATTERY_STATUS and
/// the high-latency messages into per-battery Facts with metadata-supplied units, and encodes
/// unknown as \c NaN. No voltage, current, consumption, percentage, temperature, charge state or
/// instantaneous power appears in this class.
///
/// \c batteryConfiguration() is \c IF-PWR-002 — battery present or absent — which is a different
/// statement from any battery measurement, and it is reported rather than inferred from a battery
/// message arriving.
///
/// \section notimplemented What is deliberately absent
///
/// 11-power-implementation.md lists seven refusals and this module honours all seven: no automatic
/// source switching, no source arbitration, no diode-OR behaviour, no tether-loss response, no
/// transition behaviour, no power failover, no remaining-energy estimation. There is also no
/// outbound path of any kind: the ground station initiates no power transition
/// (\c AERORA-POWER-REQ-008), so unlike the payload side there is not even an empty command
/// interface to install one through.
///
/// Converter state (\c IF-PWR-010), degraded mode (\c IF-PWR-009), remaining energy
/// (\c IF-PWR-008) and the high-voltage domain scalars (\c IF-PWR-006) have no item here. The
/// first two have no evidenced reporting path; the third is presented only where the **vehicle**
/// validates it; the fourth needs units and ranges from metadata, and whether the high-voltage
/// domain is reported to the ground station at all is the open conflict Q3-007. Each is
/// \c CONTRACT-ONLY and adding a slot would be shape with no substance behind it.
class AeroraPowerModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Attached to a vehicle through AeroraVehicleServices::attachService()")

    Q_PROPERTY(AeroraReportedValue *tetherConfiguration READ tetherConfiguration CONSTANT)
    Q_PROPERTY(AeroraReportedValue *batteryConfiguration READ batteryConfiguration CONSTANT)
    Q_PROPERTY(AeroraReportedValue *effectiveState READ effectiveState CONSTANT)
    Q_PROPERTY(AeroraReportedValue *reportedState READ reportedState CONSTANT)
    Q_PROPERTY(AeroraReportedValue *sourceTransition READ sourceTransition CONSTANT)
    Q_PROPERTY(AeroraReportedValue *fault READ fault CONSTANT)
    Q_PROPERTY(AeroraReportedValue *watchdogEvent READ watchdogEvent CONSTANT)

    Q_PROPERTY(bool extendedSurfacePermitted READ isExtendedSurfacePermitted NOTIFY extendedSurfacePermittedChanged)

public:
    /// The service name this model is attached under, for
    /// \c AeroraVehicleServices::attachService() and \c service().
    static constexpr const char *kServiceName = "power";

    /// The ground station initiates no power action.
    ///
    /// \c AERORA-POWER-REQ-008 and \c IF-SYS-007. Pinned as a constant so the prohibition is an
    /// assertion rather than an inspection: this class has no command interface, no sink, no
    /// outbound method and no way to acquire one without a reviewable change here.
    static constexpr bool kStationInitiatedPowerActionImplemented = false;

    /// No source arbitration is implemented. Arbitration is a vehicle-side electrical function,
    /// and baseline 49 states the schematic "does not, by itself" prove the topology.
    static constexpr bool kPowerArbitrationImplemented = false;

    /// No automatic source transition is implemented or inferred. Q-026; baseline 49 requires
    /// separate engineering and verification before transition behaviour can be specified at all.
    static constexpr bool kAutomaticSourceTransitionImplemented = false;

    /// The ground station computes no remaining-energy estimate. \c IF-PWR-008 presents the figure
    /// only where the **vehicle** validates it; an estimate computed here would be a
    /// ground-station opinion displayed next to vehicle facts.
    static constexpr bool kRemainingEnergyEstimationImplemented = false;

    /// Whether any evidence establishes a relationship between a watchdog event and power state.
    ///
    /// A verified \c false rather than a placeholder: no baseline document, model description or
    /// schematic record states one, in either direction. The two are displayed side by side with
    /// their own receive times and nothing is drawn between them.
    static constexpr bool kWatchdogToPowerRelationshipEvidenced = false;

    /// The number of voltage, current, power, temperature or timing thresholds this module
    /// defines.
    ///
    /// Zero, and pinned so it stays zero. Baseline 13 places high-voltage isolation, converter
    /// protection, grounding, connector interlock and tether electrical fault protection outside
    /// the ground station entirely; the station "may display validated power-system status and
    /// faults" and nothing more. A threshold here would be a limit nobody specified being enforced
    /// by the one component with no authority to enforce it.
    static constexpr int kDefinedThresholdCount = 0;

    /// The two physical operating configurations baseline 49 confirms.
    ///
    /// An addressing key for \c configuration(), and nothing else. It is **not** a protocol value,
    /// **not** the value set of \c IF-PWR-003's active source (whose enumeration is \c UNKNOWN),
    /// and **not** an arbitration priority. Adding a member would require a third confirmed
    /// physical configuration, not a software decision.
    enum class Configuration {
        Tether,  ///< IF-PWR-001 — tether connected or disconnected, as reported
        Battery, ///< IF-PWR-002 — battery present or absent, as reported
    };
    Q_ENUM(Configuration)

    /// \param freshness    The vehicle's shared freshness evaluator, from \c M-Telemetry. Not
    ///                     owned; must outlive this object. May be null on the mock and test path,
    ///                     in which case items are observable but never acquire a freshness
    ///                     verdict.
    /// \param capabilities The vehicle's capability inventory, from \c M-Vehicle. Not owned; must
    ///                     outlive this object. May be null, in which case
    ///                     \c isExtendedSurfacePermitted() is permanently false and consumers show
    ///                     the standard QGroundControl battery model only.
    /// \param parent       Owner, conventionally the \c AeroraVehicleServices this is attached to.
    explicit AeroraPowerModel(AeroraFreshness *freshness,
                              AeroraVehicleCapabilities *capabilities,
                              QObject *parent = nullptr);
    ~AeroraPowerModel() override;

    /// Tether connected or disconnected, as reported. \c IF-PWR-001.
    ///
    /// Never inferred from a voltage reading, from the battery configuration, or from traffic
    /// stopping. Unknown until the vehicle says otherwise.
    [[nodiscard]] AeroraReportedValue *tetherConfiguration() const { return _tetherConfiguration; }

    /// Battery present or absent, as reported. \c IF-PWR-002. Distinct from battery telemetry,
    /// which stays in QGroundControl's own battery fact groups.
    [[nodiscard]] AeroraReportedValue *batteryConfiguration() const { return _batteryConfiguration; }

    /// \return The configuration item for \a configuration, never null.
    [[nodiscard]] Q_INVOKABLE AeroraReportedValue *configuration(Configuration configuration) const;

    /// What the vehicle reports is in force now. \c IF-PWR-003, declared \c KindEffective.
    ///
    /// The only authoritative item in this model. Its content enumeration is \c UNKNOWN (Q-024),
    /// so the value is an opaque token.
    [[nodiscard]] AeroraReportedValue *effectiveState() const { return _effectiveState; }

    /// The last reported power state with its receive time. \c IF-PWR-003 and \c IF-PWR-009,
    /// declared \c KindReported, so it can never resolve to \c EFFECTIVE however stale
    /// \c effectiveState() becomes.
    [[nodiscard]] AeroraReportedValue *reportedState() const { return _reportedState; }

    /// A source transition **the vehicle reported**. \c IF-PWR-004, content \c CONTRACT-ONLY.
    ///
    /// Populated only by \c recordReport() on this item. Nothing derives it; see the class note on
    /// why no transition is inferred.
    [[nodiscard]] AeroraReportedValue *sourceTransition() const { return _sourceTransition; }

    /// A reported power-system fault. \c IF-PWR-007.
    ///
    /// Shown with its reported detail and **never cleared locally** — there is no clear, acknowledge
    /// or reset method here, because clearing a fault the vehicle still holds would misreport the
    /// aircraft. \c clearReports() exists for connection teardown, not for dismissing a fault.
    ///
    /// No value of this item reaches a command. A reported power fault produces no land, no
    /// return-to-launch, no disarm, no mode change and no source change
    /// (18-error-degradation-model.md records "none" in the authority column).
    [[nodiscard]] AeroraReportedValue *fault() const { return _fault; }

    /// A reported watchdog event. \c IF-PWR-011, and a separate item rather than a value of
    /// \c fault() or \c reportedState(). Carries no relationship to either — see
    /// \c kWatchdogToPowerRelationshipEvidenced.
    [[nodiscard]] AeroraReportedValue *watchdogEvent() const { return _watchdogEvent; }

    /// Every item this model owns, in declaration order.
    [[nodiscard]] QList<AeroraReportedValue *> reportedValues() const { return _reportedValues; }

    /// \return The item with the given id, or nullptr when this model has none.
    [[nodiscard]] Q_INVOKABLE AeroraReportedValue *reportedValue(const QString &itemId) const;

    /// Whether an Aerora power surface may be presented beyond the standard battery model.
    /// \c IF-PWR-012.
    ///
    /// True only when the capability inventory reports at least one capability **available** in the
    /// category power capability belongs to. When false, a consumer offers the generic
    /// QGroundControl battery model only — which is correct behaviour, not degraded behaviour, for
    /// an aircraft that has told us nothing about its power configuration.
    ///
    /// \note **Why the Vehicle category and not a Power one.** 17-capability-model.md defines five
    ///       categories — Vehicle, Payload, Mission, Telemetry, Command — and there is no Power
    ///       category. \c IF-PWR-012's producer is the flight controller reporting on connection,
    ///       which the capability model's source table routes through \c IF-FC-017, the **vehicle
    ///       capability** inventory. The documented consequence of an absent vehicle capability —
    ///       "the flight interface degrades to the standard QGroundControl set" — is exactly
    ///       \c IF-PWR-012's failure behaviour. So this reads \c Category::Vehicle rather than
    ///       inventing a sixth category, which would change the capability hierarchy and fail
    ///       17-capability-model.md's own second-product test.
    [[nodiscard]] bool isExtendedSurfacePermitted() const { return _extendedSurfacePermitted; }

    /// Vehicle capability keys reported for this vehicle, in the order they were first reported.
    ///
    /// Opaque strings; this module interprets none of them, and no key is known to denote a power
    /// capability — capability identifier values are \c NOT-EVIDENCED. Rule 6 of
    /// 17-capability-model.md requires an unrecognised identifier be retained and recorded rather
    /// than treated as absent or as an error, which is what returning the keys unfiltered does.
    [[nodiscard]] QStringList reportedCapabilityKeys() const;

    /// Returns every item to never-observed. For connection teardown, so the next connection
    /// cannot inherit the previous aircraft's reports. Not a fault-acknowledgement path.
    void clearReports();

signals:
    void extendedSurfacePermittedChanged(bool extendedSurfacePermitted);

private slots:
    void _onCapabilityReportsChanged();

private:
    AeroraReportedValue *_addReportedValue(const QString &itemId, AeroraTelemetryState::Kind declaredKind);

    void _recomputeExtendedSurfacePermitted();

    QPointer<AeroraFreshness> _freshness;
    QPointer<AeroraVehicleCapabilities> _capabilities;

    // Created in the constructor body so that _reportedValues and _itemIdToValueMap are already
    // constructed when _addReportedValue() populates them. Each is parented to this model and
    // never replaced, so the accessors are CONSTANT from QML's point of view.
    AeroraReportedValue *_tetherConfiguration = nullptr;
    AeroraReportedValue *_batteryConfiguration = nullptr;
    AeroraReportedValue *_effectiveState = nullptr;
    AeroraReportedValue *_reportedState = nullptr;
    AeroraReportedValue *_sourceTransition = nullptr;
    AeroraReportedValue *_fault = nullptr;
    AeroraReportedValue *_watchdogEvent = nullptr;

    QList<AeroraReportedValue *> _reportedValues;
    QMap<QString, AeroraReportedValue *> _itemIdToValueMap;

    bool _extendedSurfacePermitted = false;
};

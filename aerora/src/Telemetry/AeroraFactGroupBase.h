#pragma once

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "AeroraStateValue.h"
#include "AeroraTelemetryState.h"
#include "FactGroup.h"

class AeroraFreshness;

/// \brief Base for every Aerora fact group: freshness stamping at parse, and nothing invented.
///
/// Realises the shared per-group contract in 06-factgroup-implementation.md. The six Aerora groups
/// (\c aeroraSystem, \c aeroraPayload, the product group, \c aeroraPower, \c aeroraLink,
/// \c aeroraAutonomy) derive from this and differ only in which facts they declare and which
/// messages they parse.
///
/// \section status Why no concrete group ships here
///
/// All six are \c CONTRACT-ONLY in 04-cpp-implementation-map.md for one reason: none has a message to
/// parse. The Aerora dialect does not exist, so no \c handleMessage body can be written, and no Aerora
/// fact name, unit, range or enumeration member is specified anywhere. Declaring facts now would mean
/// inventing them.
///
/// The base class is the part that is buildable and testable today, and that is deliberate - the
/// state model, the stamping order and the staleness contract are the things that are expensive to
/// retrofit once six groups exist.
///
/// \section rate Why the update rate is zero
///
/// \c FactGroup's constructor takes a rate in milliseconds, and a non-zero rate coalesces
/// \c Fact::valueChanged onto a timer. For a value whose freshness is part of its meaning, coalescing
/// moves the apparent receive time: the stamp says one instant and the consumer is notified at
/// another. Upstream groups using a non-zero rate are display conveniences; Aerora values feed the
/// audit record, so the timestamp must be the parse timestamp. Hence rate zero, always.
///
/// \section construction Why groups must be built during vehicle construction
///
/// The telemetry CSV writer enumerates the vehicle's fact groups once to write its header
/// (\c Vehicle::_initializeCsv) and re-enumerates them for every row (\c Vehicle::_writeCsvLine). A
/// group that appears after the header is written shifts every later column. That is R-027 and
/// \c AERORA-GCS-REQ-022: **no Aerora fact group is created dynamically.** The free consequence is
/// that every Aerora fact reaches the telemetry record with no Aerora logging code at all.
///
/// \section multivehicle The multi-vehicle sharing guard
///
/// 06-factgroup-implementation.md records a hazard in the chosen attachment seam:
/// \c FirmwarePluginManager caches one \c FirmwarePlugin instance per firmware class, so a plugin that
/// returns its own members from \c FirmwarePlugin::factGroups() hands the *same* fact group objects to
/// every vehicle of that class. Each vehicle's dispatch loop then writes into them, and the operator
/// sees one aircraft's values attributed to another. The in-tree \c ArduSubFirmwarePlugin has this
/// shape today. Phase 4 declined to choose a fix unilaterally; it is Q4-002 and R4-001.
///
/// This class cannot resolve Q4-002 - the seam signature takes no \c Vehicle*, so a shared plugin
/// genuinely cannot tell which vehicle is asking. What it can do is refuse to let the failure be
/// silent. \c handleMessage is \c final and records the first vehicle to dispatch into this group;
/// a second vehicle produces a warning naming the hazard. Derived groups override
/// \c _handleAeroraMessage instead, so the guard cannot be bypassed by forgetting to call a base
/// implementation.
///
/// The guard is a diagnostic, not a fix. It compares object identity only and makes no attempt to
/// separate the two vehicles' values.
///
/// \section safety What this base class does not do
///
/// It reports conditions and decides nothing about the aircraft. There is no path from a value going
/// stale, or from telemetry going absent, to a mode change, a return-to-launch, a land, a disarm or
/// any other action. 18-error-degradation-model.md specifies "none" in the authority column of all
/// twelve aircraft-reported conditions, and the ground station's response to each is representational.
class AeroraFactGroupBase : public FactGroup
{
    Q_OBJECT

public:
    /// \param metaDataFile  Fact metadata resource path, conventionally
    ///                      \c ":/json/Aerora/<Name>Fact.json". Units, bounds and enumerations come
    ///                      from here rather than from code (\c AERORA-GCS-REQ-026); a fact with no
    ///                      metadata entry is not presented rather than presented without units.
    /// \param freshnessClass The freshness class name governing this group's items by default.
    /// \param freshness      The shared evaluator, owned by the vehicle's \c AeroraTelemetrySet. Not
    ///                       owned here. Expected to outlive this group, and held through a
    ///                       \c QPointer so that if it does not, \c _stampNowUs() yields zero and
    ///                       the resulting observation is refused with a warning rather than stored
    ///                       as a fresh receipt. See \c AeroraStateValue's lifetime section for the
    ///                       full degradation table.
    /// \param parent         Owning vehicle.
    explicit AeroraFactGroupBase(const QString &metaDataFile,
                                 const QString &freshnessClass,
                                 AeroraFreshness *freshness,
                                 QObject *parent = nullptr);
    ~AeroraFactGroupBase() override;

    /// Applies the multi-vehicle sharing guard, then delegates to \c _handleAeroraMessage.
    /// Final so the guard cannot be bypassed; derived groups override \c _handleAeroraMessage.
    void handleMessage(Vehicle *vehicle, const mavlink_message_t &message) final;

    [[nodiscard]] QString freshnessClass() const { return _freshnessClass; }

    /// Every state value registered by this group, in registration order.
    [[nodiscard]] QList<AeroraStateValue *> stateValues() const { return _stateValues; }

    /// \return The state value bound to the named fact, or nullptr when the group has none.
    [[nodiscard]] Q_INVOKABLE AeroraStateValue *stateValue(const QString &factName) const;

    /// Names of the facts that have a bound state value.
    [[nodiscard]] QStringList stateValueNames() const;

    /// Returns every item to never-observed. For connection teardown, so the next connection cannot
    /// inherit the previous aircraft's readings. Does not clear the facts themselves: \c DEG-03
    /// requires last-known values be retained with their age rather than discarded.
    void clearObservations();

    /// True once a second vehicle has dispatched into this group - the Q4-002 hazard, observed.
    /// Exposed so the unit test can assert detection and the startup self-check can report it.
    [[nodiscard]] bool sharedAcrossVehiclesDetected() const { return _sharedAcrossVehiclesDetected; }

protected:
    /// Message handling for a derived group. Default is a no-op: with no Aerora dialect there is no
    /// message to parse, which is why all six groups are \c CONTRACT-ONLY.
    virtual void _handleAeroraMessage(Vehicle *vehicle, const mavlink_message_t &message);

    /// Registers a fact and binds it to a state class in one step.
    ///
    /// Use this instead of \c FactGroup::_addFact for any Aerora value, so that no fact can reach a
    /// consumer without a declared kind. Registration happens in the constructor; nothing is added
    /// afterwards.
    ///
    /// \param fact         The fact, owned by the derived group.
    /// \param declaredKind The item's kind from its interface contract.
    /// \param itemFreshnessClass Overrides the group's class for items whose staleness is governed
    ///                     separately; empty means use the group's class.
    /// \return The bound state value, owned by this group.
    AeroraStateValue *_addStateFact(Fact *fact,
                                    AeroraTelemetryState::Kind declaredKind,
                                    const QString &itemFreshnessClass = QString());

    /// Reads the clock once, at parse. Call at the top of \c _handleAeroraMessage and pass the result
    /// to every \c _recordObservation for that message, so one message produces one receive time
    /// rather than a spread of per-field times.
    [[nodiscard]] quint64 _stampNowUs() const;

    /// Stamps an item's receive time and reporting source.
    ///
    /// Call **before** setting the fact's value - see the ordering note on \c AeroraStateValue. The
    /// source is attributed to the sending component, which for a relayed value is not the
    /// originating subsystem; that distinction stays open under \c IF-MAV-022.
    void _recordObservation(AeroraStateValue *value, const mavlink_message_t &message, quint64 receiveTimeUs);

    /// The observed reporting source of a message: its system and component identifiers, attributed
    /// to the sender. No role is inferred and no component identifier is invented.
    [[nodiscard]] static AeroraTelemetrySource _sourceOf(const mavlink_message_t &message);

    /// Records which vehicle dispatched into this group and warns the first time a second one
    /// appears. Called by \c handleMessage; separated out so the guard is testable without
    /// fabricating a \c Vehicle*.
    ///
    /// Null identities are ignored. Production always dispatches from a real vehicle
    /// (\c Vehicle::_mavlinkMessageReceived passes \c this), and ignoring null keeps a group drivable
    /// from a test that has no vehicle at all.
    void _noteDispatchIdentity(const void *vehicleIdentity);

    [[nodiscard]] AeroraFreshness *_freshnessEvaluator() const { return _freshness; }

private:
    const QString _freshnessClass;
    QPointer<AeroraFreshness> _freshness;
    QList<AeroraStateValue *> _stateValues;
    QMap<QString, AeroraStateValue *> _nameToStateValueMap;

    /// Identity of the first vehicle to dispatch here. Compared as an opaque pointer only - never
    /// dereferenced - so a destroyed vehicle cannot be followed. A diagnostic, not a fix.
    const void *_boundVehicle = nullptr;
    bool _sharedAcrossVehiclesDetected = false;
};

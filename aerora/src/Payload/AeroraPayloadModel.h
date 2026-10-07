#pragma once

#include <QtCore/QList>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtQmlIntegration/QtQmlIntegration>

#include "AeroraPayloadCommandSink.h"
#include "AeroraReportedValue.h"
#include "AeroraVehicleCapabilities.h"

/// Defined in AeroraPayloadModel.cc.
Q_DECLARE_LOGGING_CATEGORY(AeroraPayloadLog)

class AeroraFreshness;

/// \brief The product-independent payload abstraction: what the ground station may say about a
/// payload, and the boundary it may not cross.
///
/// \section authority Who owns what
///
/// \code
/// Payload controller (aircraft)   owns payload state, local interlocks, actuator behaviour,
///         │                       emergency behaviour  —  NOT the ground station
///         ▼  reports
/// Flight controller               relays
///         │
///         ▼  parsed, stamped at parse
/// AeroraPayloadModel (this)       representation only
///         │
///         ▼  gated by capability
/// GCS UI                          display, and operator-originated requests
/// \endcode
///
/// The ground station appears in 06-payload-icd.md's ownership table in exactly two rows, both as
/// an observer or a relay. This class is that observer. It holds no authoritative payload state,
/// decides nothing about the payload, and has no path to an actuator.
///
/// \section separation The five values, kept separate
///
/// 10-payload-implementation.md requires identity, health, safety state and emergency-stop state be
/// **separate** values, and payload state is a fifth. Each is an \c AeroraReportedValue with its
/// own class, receive time and reporting source, because each arrives in its own report and none
/// implies another.
///
/// The emergency stop is the separation that matters most. Folding it into a payload state
/// enumeration would imply it is one state in a sequence the payload moves through; it is an
/// **independent cut**. It would also make it impossible to record "the payload was in state X when
/// the stop fired", which is exactly what a debrief needs (\c AERORA-SAFE-REQ-011).
///
/// \section nocausalfield Why there is no causal field
///
/// Baseline 13's firmware model shows a watchdog fault reaching disarm logic, and separately
/// reaching payload emergency stop directly. Those are aircraft-side paths on \c MODEL evidence.
/// What is **reported** to the ground station is separate items, and no evidenced message carries
/// the linkage between them — Phase 3.1 made that explicit after finding the absence had been left
/// to inference.
///
/// So this class has no field relating the stop to a watchdog event, to an arming change, or to
/// anything else. There is nothing to populate one from, and asserting causation the vehicle never
/// reported is the derivation \c AERORA-GCS-REQ-029 prohibits. Temporal order is not causal order
/// either: two reports arriving in sequence is not a reported relationship.
///
/// \section gating Capability gating
///
/// \c IF-PAY-002 makes the capability set the gating mechanism: with no payload capability reported,
/// **no payload control is presented at all**, and the ground station is correct rather than broken.
/// \c controlsPermitted() is that gate, and \c submitRequest() enforces it rather than trusting a
/// caller to check.
///
/// \section nomapping Why no capability key is mapped to an item
///
/// \c AeroraVehicleCapabilities carries opaque keys because no Aerora capability identifier is
/// evidenced (Q-023), and whether the vehicle reports capability *presence* separately from
/// capability *state* is itself unresolved (Q3-009, finding F3.1-05). Deciding here that some key
/// governs, say, the health item would be inventing the mapping and would manufacture an
/// \c UNAVAILABLE the vehicle never reported.
///
/// This class therefore never sets an item's capability from a capability report. Whoever
/// eventually knows the mapping sets \c AeroraReportedValue::setCapability() directly. Until then
/// every item resolves \c UNKNOWN rather than \c UNAVAILABLE, which is the honest answer.
///
/// \section notimplemented What is deliberately absent
///
/// No payload state enumeration member, actuator command, pump, valve, nozzle, pressure, flow,
/// tank level, spray rate, colour, gimbal axis, calibration procedure or diagnostic schema appears
/// anywhere in this class or its module. None is specified in any interface artefact; writing them
/// would mean inventing them. Painting is a product profile (\c M-ProfilePainting), not this layer:
/// there is no product branch here and no place to add one.
///
/// No persistence either. There is no save, restore, serialise or replay API, because payload state
/// retention across a mission interruption is \c NOT-EVIDENCED (\c IF-PAY-014) and ground-station
/// replay is rejected by decision. The mission side pins the same position in
/// \c AeroraMissionResumeBoundary; this side simply offers nothing to replay from.
class AeroraPayloadModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Attached to a vehicle through AeroraVehicleServices::attachService()")

    Q_PROPERTY(AeroraReportedValue *identity READ identity CONSTANT)
    Q_PROPERTY(AeroraReportedValue *payloadState READ payloadState CONSTANT)
    Q_PROPERTY(AeroraReportedValue *health READ health CONSTANT)
    Q_PROPERTY(AeroraReportedValue *safetyState READ safetyState CONSTANT)
    Q_PROPERTY(AeroraReportedValue *emergencyStop READ emergencyStop CONSTANT)

    Q_PROPERTY(bool controlsPermitted READ controlsPermitted NOTIFY controlsPermittedChanged)
    Q_PROPERTY(bool commandPathInstalled READ isCommandPathInstalled NOTIFY commandPathChanged)
    Q_PROPERTY(int requestCount READ requestCount NOTIFY requestCountChanged)

public:
    /// The service name this model is attached under, for
    /// \c AeroraVehicleServices::attachService() and \c service().
    static constexpr const char *kServiceName = "payload";

    /// Ground-station access to payload actuators is not implemented and must not become
    /// implemented.
    ///
    /// \c IF-PAY-015 is \c CONFIRMED with direction "None — prohibited", and \c IF-PAY-013 places
    /// the actuator interface inside the vehicle. Pinned as a constant so the prohibition is an
    /// assertion rather than an inspection: this module declares no actuator type, addresses no
    /// actuator, and its only outbound seam
    /// (\c AeroraPayloadCommandSink) reaches the payload controller and has no implementation.
    static constexpr bool kGroundStationActuatorPathImplemented = false;

    /// The ground station originates no payload request of its own.
    ///
    /// Every request passed to \c submitRequest() comes from an operator act. There is no timer, no
    /// telemetry handler and no fault path in this module that calls it, which is what
    /// \c AERORA-PAYLOAD-REQ-010 requires: the ground station is not the payload safety controller.
    static constexpr bool kAutonomousPayloadActionImplemented = false;

    /// Why a submitted request was or was not accepted for transmission.
    ///
    /// A ground-station-internal outcome, not a protocol result and not a payload state. \c
    /// Submitted means the request reached the command path — never that the payload acted.
    enum class RequestOutcome {
        Submitted,                   ///< Handed to the installed command path
        RefusedInvalidRequest,       ///< The request carried no token, so identifies no operation
        RefusedNoCapabilityReported, ///< No payload capability has been reported — IF-PAY-002
        RefusedNoCommandPath,        ///< No command path is installed — the IF-PAY-015 default
        RefusedByCommandPath,        ///< The command path declined it
    };
    Q_ENUM(RequestOutcome)

    /// \param freshness    The vehicle's shared freshness evaluator, from \c M-Telemetry. Not
    ///                     owned; must outlive this object. May be null on the mock and test path,
    ///                     in which case items are observable but never acquire a freshness
    ///                     verdict.
    /// \param capabilities The vehicle's capability inventory, from \c M-Vehicle. Not owned; must
    ///                     outlive this object. May be null, in which case \c controlsPermitted()
    ///                     is permanently false — absence of evidence gates controls off.
    /// \param parent       Owner, conventionally the \c AeroraVehicleServices this is attached to.
    explicit AeroraPayloadModel(AeroraFreshness *freshness,
                                AeroraVehicleCapabilities *capabilities,
                                QObject *parent = nullptr);
    ~AeroraPayloadModel() override;

    /// Payload identity and firmware version, as reported. \c IF-PAY-001.
    [[nodiscard]] AeroraReportedValue *identity() const { return _identity; }

    /// The payload's own reported state. \c IF-PAY-003. Enumeration members are \c OPEN (Q-023),
    /// so the value is carried as an opaque token and never inferred from a request this station
    /// sent.
    [[nodiscard]] AeroraReportedValue *payloadState() const { return _payloadState; }

    /// Reported payload health. \c IF-PAY-004. Unknown is presented as unknown and **never as
    /// healthy** — that is the dangerous default, and it is avoided structurally: the item starts
    /// \c UNKNOWN and only a report moves it.
    [[nodiscard]] AeroraReportedValue *health() const { return _health; }

    /// Reported payload safety state. \c IF-PAY-005. Never inferred, and no value of it reaches a
    /// flight command.
    [[nodiscard]] AeroraReportedValue *safetyState() const { return _safetyState; }

    /// Reported payload emergency-stop state. \c IF-PAY-006, and a **separate item** from
    /// \c payloadState() rather than a member of its enumeration.
    ///
    /// The stop's initiating source is shown only where the vehicle reports one, and that field is
    /// itself \c NOT-EVIDENCED — so it arrives, if ever, inside this item's reported token and is
    /// not a field here.
    [[nodiscard]] AeroraReportedValue *emergencyStop() const { return _emergencyStop; }

    /// Every item this model owns, in declaration order. For the startup self-check and for a
    /// consumer that renders the set without naming each one.
    [[nodiscard]] QList<AeroraReportedValue *> reportedValues() const { return _reportedValues; }

    /// \return The item with the given id, or nullptr when this model has none.
    [[nodiscard]] Q_INVOKABLE AeroraReportedValue *reportedValue(const QString &itemId) const;

    /// Whether any payload control may be presented or submitted at all. \c IF-PAY-002.
    ///
    /// True only when the capability inventory reports at least one payload capability
    /// **available**. A capability reported *unavailable* does not permit controls, and neither
    /// does an empty inventory: "we have not been told" is not permission.
    [[nodiscard]] bool controlsPermitted() const;

    /// Payload capability keys reported for this vehicle, in the order they were first reported.
    /// Opaque strings; this module interprets none of them.
    [[nodiscard]] QStringList reportedCapabilityKeys() const;

    /// True when an outbound command path has been installed. False in this build — see
    /// \c kGroundStationActuatorPathImplemented and \c AeroraPayloadCommandSink.
    [[nodiscard]] bool isCommandPathInstalled() const { return _commandSink != nullptr; }

    /// Installs the outbound command path.
    ///
    /// Nothing in the overlay implements \c AeroraPayloadCommandSink, so in this build the sink is
    /// always null and \c submitRequest() always refuses. An implementation must satisfy the
    /// authorization and command-tracking obligations documented on the interface.
    ///
    /// \param sink Not owned; must outlive this object, or be replaced with nullptr first.
    void setCommandSink(AeroraPayloadCommandSink *sink);

    /// Submits an operator-originated payload request, gated and fail-closed.
    ///
    /// Refuses an invalid request, refuses when no payload capability is reported, and refuses when
    /// no command path is installed. **Changes no reported value under any outcome**: a request is
    /// not payload state, and \c IF-PAY-003 requires state never be inferred from a command the
    /// station sent. Tracking the request's lifecycle is \c M-Authority's eight-state model, not
    /// this model's.
    ///
    /// \return The outcome, so the caller and the audit record get a cause rather than a bare
    ///         boolean. \c Submitted means transmitted, never effective.
    RequestOutcome submitRequest(const AeroraPayloadRequest &request);

    /// Submitted request count, for diagnostics and for the test that asserts nothing in this
    /// module originates one.
    [[nodiscard]] int requestCount() const { return _requestCount; }

    /// The last request handed to \c submitRequest(), whatever its outcome. Carries no state field
    /// and is never read back as payload state.
    [[nodiscard]] AeroraPayloadRequest lastRequest() const { return _lastRequest; }

    /// Returns every item to never-observed. For connection teardown, so the next connection
    /// cannot inherit the previous aircraft's reports.
    ///
    /// This is a reset, not a checkpoint. Nothing is stored anywhere for a later resume to restore
    /// from, because payload state retention is \c NOT-EVIDENCED (\c IF-PAY-014).
    void clearReports();

signals:
    void controlsPermittedChanged(bool controlsPermitted);
    void commandPathChanged();
    void requestCountChanged(int requestCount);

    /// Emitted after a request has been handed to \c submitRequest(), with its outcome. For the
    /// audit trail and the operator notification that a request is not an effect
    /// (\c AERORA-GCS-REQ-016). Carries no payload state.
    void requestOutcome(const AeroraPayloadRequest &request, RequestOutcome outcome);

private slots:
    void _onCapabilityReportsChanged();

private:
    /// Creates an item, registers it in declaration order and parents it to this model.
    AeroraReportedValue *_addReportedValue(const QString &itemId, AeroraTelemetryState::Kind declaredKind);

    void _recomputeControlsPermitted();

    QPointer<AeroraFreshness> _freshness;
    QPointer<AeroraVehicleCapabilities> _capabilities;

    // Created in the constructor body, in this order, so that _reportedValues and
    // _itemIdToValueMap are already constructed when _addReportedValue() populates them. Each is
    // parented to this model and lives for its lifetime; none is ever replaced, so the accessors
    // are CONSTANT from QML's point of view.
    AeroraReportedValue *_identity = nullptr;
    AeroraReportedValue *_payloadState = nullptr;
    AeroraReportedValue *_health = nullptr;
    AeroraReportedValue *_safetyState = nullptr;
    AeroraReportedValue *_emergencyStop = nullptr;

    QList<AeroraReportedValue *> _reportedValues;
    QMap<QString, AeroraReportedValue *> _itemIdToValueMap;

    AeroraPayloadCommandSink *_commandSink = nullptr;
    AeroraPayloadRequest _lastRequest;
    int _requestCount = 0;
    bool _controlsPermitted = false;
};

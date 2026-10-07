#pragma once

#ifndef QGC_UNITTEST_BUILD
#error "AeroraPayloadMock is MOCK / TEST ONLY and must not be compiled into a shipped build"
#endif

#include <QtCore/QObject>
#include <QtCore/QString>

#include "AeroraStateValue.h"
#include "AeroraTelemetrySource.h"

class AeroraFreshness;
class AeroraPayloadModel;
class AeroraVehicleCapabilities;
class RuntimeScheduler;

/// **MOCK / TEST ONLY**
///
/// Deterministic driver for the Aerora payload abstraction, so payload behaviour can be exercised
/// without a payload, without hardware, without a MockLink connection and without a `Vehicle`.
///
/// Compiled only when `QGC_BUILD_TESTING` is on, and the guard at the top of this header fails the
/// build if it reaches a shipped configuration.
///
/// ### What it does not do
///
/// - **It does not model a payload.** It produces no pump state, valve state, nozzle position,
///   pressure, flow, tank level, spray rate, colour, temperature or gimbal angle, because no such
///   value is evidenced and a plausible-looking invented number is worse than no number.
/// - **It supplies no token of its own.** Every `report*()` method takes the reported value from
///   the caller. There is no default, no example and no enumeration anywhere in this class: payload
///   state, health and safety-state enumeration members are `OPEN` (Q-023), and a mock that shipped
///   its own would make them look settled.
/// - **It invents no component identifier.** Whether the payload controller is a distinct MAVLink
///   component is unresolved (Q3-006), so reports carry an unidentified source until a test calls
///   `setReportingSource()` with identifiers it chose itself.
/// - **It never acts.** No reported condition produces a request, a command, or a change to any
///   other item. There is no "fault causes stop" path here, because there is none in the model
///   either.
/// - **It has no timers and no randomness.** Every method is a direct state change stamped from the
///   injected scheduler, so a given call sequence always produces the same observable sequence.
///
/// ### There is no payload fault item, deliberately
///
/// 06-payload-icd.md gives the payload health (`IF-PAY-004`) and safety state (`IF-PAY-005`) as the
/// items a fault condition is reported through, and leaves their enumeration members `OPEN`. So a
/// fault scenario is `reportHealth()` or `reportSafetyState()` with whatever token the test chose,
/// not a separate `reportFault()` that would imply a fault enumeration exists.
class AeroraPayloadMock : public QObject
{
    Q_OBJECT

public:
    /// \param scheduler The deterministic clock. Pass a `ManualScheduler` so reports are stamped
    ///                  at a time the test controls. When null the freshness evaluator creates a
    ///                  real one, which makes the mock non-deterministic — tests should not do
    ///                  that.
    explicit AeroraPayloadMock(RuntimeScheduler *scheduler, QObject *parent = nullptr);
    ~AeroraPayloadMock() override;

    [[nodiscard]] AeroraPayloadModel *model() const { return _model; }
    [[nodiscard]] AeroraVehicleCapabilities *capabilities() const { return _capabilities; }
    [[nodiscard]] AeroraFreshness *freshness() const { return _freshness; }

    /// Sets the reporting source subsequent reports carry.
    ///
    /// Identifiers come from the caller. This class holds no notion of a payload component id, and
    /// the source is attributed to the *sending* component as `AeroraTelemetrySource` requires —
    /// never to an originating subsystem (`IF-MAV-022`).
    void setReportingSource(int systemId, int componentId);

    /// Returns to an unidentified reporting source, as if nothing had been observed.
    void clearReportingSource();

    /// Scenario: payload available. Reports \a key present in the payload capability category,
    /// which is what un-gates the payload surface (`IF-PAY-002`).
    /// \return true if the report was accepted.
    bool reportCapabilityAvailable(const QString &key);

    /// Scenario: payload unavailable. Reports \a key absent.
    ///
    /// Note this does **not** make any item resolve `UNAVAILABLE`: no mapping from a capability key
    /// to an item is evidenced (Q-023), so the model performs none. Use
    /// `markItemCapabilityAbsent()` for that, which is the vehicle-reported-absence path.
    /// \return true if the report was accepted.
    bool reportCapabilityUnavailable(const QString &key);

    /// Discards every capability report, as a reconnection must (`17-capability-model.md` rule 5).
    void clearCapabilityReports();

    // Scenario: payload state reported. One method per item, each taking the reported value from
    // the caller. Stamped from the injected scheduler before the token becomes visible.

    void reportIdentity(const QString &reportedToken);
    void reportPayloadState(const QString &reportedToken);
    void reportHealth(const QString &reportedToken);
    void reportSafetyState(const QString &reportedToken);

    /// Scenario: emergency stop reported.
    ///
    /// A separate item from payload state, and recording one changes nothing else — no payload
    /// state, no health, no safety state, and no causal field, because the model has none
    /// (`AERORA-SAFE-REQ-011`, and the watchdog linkage is `NOT-EVIDENCED`).
    void reportEmergencyStop(const QString &reportedToken);

    /// Scenario: the vehicle reports it does not have the capability an item depends on.
    ///
    /// The only input that makes an item resolve `UNAVAILABLE`. Applied to a named item because
    /// nothing maps a capability key onto an item.
    /// \return true when the item exists and was updated.
    bool markItemCapabilityAbsent(const QString &itemId);

    /// Sets an item's capability tri-state directly, including back to `CapabilityUnknown`.
    /// \return true when the item exists and was updated.
    bool setItemCapability(const QString &itemId, AeroraStateValue::Capability capability);

    /// Scenario: payload disconnect. Clears every report and every capability report, so the next
    /// connection starts from no evidence rather than the previous payload's.
    ///
    /// Nothing is retained for a later restore: payload state persistence is `NOT-EVIDENCED`
    /// (`IF-PAY-014`) and this mock offers nothing to replay from.
    void disconnectPayload();

    /// The scheduler's current time, for a test that wants to assert a stamp it caused.
    [[nodiscard]] quint64 nowUs() const;

private:
    AeroraFreshness *_freshness = nullptr;
    AeroraVehicleCapabilities *_capabilities = nullptr;
    AeroraPayloadModel *_model = nullptr;
    AeroraTelemetrySource _source;
};

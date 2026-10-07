#pragma once

#ifndef QGC_UNITTEST_BUILD
#error "AeroraPowerMock is MOCK / TEST ONLY and must not be compiled into a shipped build"
#endif

#include <QtCore/QObject>
#include <QtCore/QString>

#include "AeroraPowerModel.h"
#include "AeroraStateValue.h"
#include "AeroraTelemetrySource.h"

class AeroraFreshness;
class AeroraVehicleCapabilities;
class RuntimeScheduler;

/// **MOCK / TEST ONLY**
///
/// Deterministic driver for the Aerora power abstraction, so power representation can be exercised
/// without an aircraft, a tether, a battery or a link.
///
/// Compiled only when `QGC_BUILD_TESTING` is on, and the guard at the top of this header fails the
/// build if it reaches a shipped configuration.
///
/// ### It does not simulate source switching
///
/// This is the load-bearing property. There is **no method that changes one configuration as a
/// consequence of changing another**, and no method that produces a transition. Reporting the
/// tether configuration absent does not report the battery configuration present, does not change
/// the effective state, and does not populate `sourceTransition()`.
///
/// A test that wants a transition must call `reportSourceTransition()` explicitly, which models the
/// vehicle reporting one. Baseline 49 states the two configurations are "not yet a claim of
/// automatic in-flight source switching" and that any transition "requires separate engineering and
/// verification" (Q-026), so a mock that switched sources on its own would be the first place the
/// unevidenced behaviour entered the codebase.
///
/// ### What else it does not do
///
/// - **No values.** No voltage, current, power, temperature, state of charge, remaining energy or
///   converter reading. The high-voltage domain's ranges are `UNKNOWN` and whether it is reported
///   at all is the open conflict Q3-007.
/// - **No tokens of its own.** Every `report*()` method takes the reported value from the caller.
///   Power state and fault enumeration members are `UNKNOWN` (Q-024); a mock shipping its own would
///   make them look settled.
/// - **No thresholds.** Nothing here compares a value against a limit, because
///   `AeroraPowerModel::kDefinedThresholdCount` is zero and no threshold is evidenced.
/// - **No component identifier.** Reports carry an unidentified source until a test calls
///   `setReportingSource()` with identifiers it chose itself.
/// - **No action.** No reported fault or watchdog event produces a command, a land, a
///   return-to-launch, a disarm or a source change. There is no outbound path in the model to
///   produce one through.
/// - **No timers and no randomness.** Every method is a direct state change stamped from the
///   injected scheduler.
class AeroraPowerMock : public QObject
{
    Q_OBJECT

public:
    /// \param scheduler The deterministic clock. Pass a `ManualScheduler` so reports are stamped at
    ///                  a time the test controls.
    explicit AeroraPowerMock(RuntimeScheduler *scheduler, QObject *parent = nullptr);
    ~AeroraPowerMock() override;

    [[nodiscard]] AeroraPowerModel *model() const { return _model; }
    [[nodiscard]] AeroraVehicleCapabilities *capabilities() const { return _capabilities; }
    [[nodiscard]] AeroraFreshness *freshness() const { return _freshness; }

    /// Sets the reporting source subsequent reports carry, attributed to the sending component.
    void setReportingSource(int systemId, int componentId);

    /// Returns to an unidentified reporting source, as if nothing had been observed.
    void clearReportingSource();

    /// Reports \a key present in the vehicle capability category, which is what un-gates the
    /// extended power surface (`IF-PWR-012`).
    /// \return true if the report was accepted.
    bool reportCapabilityAvailable(const QString &key);

    /// Reports \a key absent in the vehicle capability category.
    /// \return true if the report was accepted.
    bool reportCapabilityUnavailable(const QString &key);

    /// Discards every capability report, as a reconnection must.
    void clearCapabilityReports();

    /// Scenario: tether available or not, as the vehicle reports it. `IF-PWR-001`.
    ///
    /// Changes the tether configuration item only.
    void reportTetherConfiguration(const QString &reportedToken);

    /// Scenario: battery available or not, as the vehicle reports it. `IF-PWR-002`.
    ///
    /// Changes the battery configuration item only. In particular it does not report the tether
    /// configuration absent, and it does not touch the effective state.
    void reportBatteryConfiguration(const QString &reportedToken);

    /// Reports the given configuration item. Equivalent to the two methods above, for a test that
    /// iterates the configurations.
    void reportConfiguration(AeroraPowerModel::Configuration configuration, const QString &reportedToken);

    /// Scenario: power state reported, as what the vehicle says is in force. `IF-PWR-003`, the one
    /// authoritative item.
    void reportEffectiveState(const QString &reportedToken);

    /// Scenario: power state reported, as the last report. `IF-PWR-003` and `IF-PWR-009`. Never
    /// becomes the effective state however stale the effective item gets.
    void reportReportedState(const QString &reportedToken);

    /// Scenario: the **vehicle** reported a source transition. `IF-PWR-004`.
    ///
    /// The only way `sourceTransition()` becomes observed. Nothing else in this mock or in the
    /// model populates it.
    void reportSourceTransition(const QString &reportedToken);

    /// Scenario: power fault reported. `IF-PWR-007`. Displayed and logged; reaches no command, and
    /// there is no local clear.
    void reportFault(const QString &reportedToken);

    /// Scenario: watchdog event reported. `IF-PWR-011`.
    ///
    /// A separate item, and recording one changes no power state, no fault and no configuration —
    /// no relationship between the watchdog and power is evidenced in either direction.
    void reportWatchdogEvent(const QString &reportedToken);

    /// Scenario: source unavailable. The vehicle reports it does not have the capability an item
    /// depends on, which is the only input that makes an item resolve `UNAVAILABLE`.
    /// \return true when the item exists and was updated.
    bool markItemCapabilityAbsent(const QString &itemId);

    /// Sets an item's capability tri-state directly, including back to `CapabilityUnknown` — which
    /// is the "source unknown" scenario, and the initial state of every item.
    /// \return true when the item exists and was updated.
    bool setItemCapability(const QString &itemId, AeroraStateValue::Capability capability);

    /// Scenario: vehicle disconnect. Clears every report and every capability report.
    void disconnectPower();

    /// The scheduler's current time, for a test that wants to assert a stamp it caused.
    [[nodiscard]] quint64 nowUs() const;

private:
    AeroraFreshness *_freshness = nullptr;
    AeroraVehicleCapabilities *_capabilities = nullptr;
    AeroraPowerModel *_model = nullptr;
    AeroraTelemetrySource _source;
};

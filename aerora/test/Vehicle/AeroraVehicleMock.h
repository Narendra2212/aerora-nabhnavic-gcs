#pragma once

#ifndef QGC_UNITTEST_BUILD
#error "AeroraVehicleMock is MOCK / TEST ONLY and must not be compiled into a shipped build"
#endif

#include <QtCore/QObject>
#include <QtCore/QString>

#include "AeroraVehicleCapabilities.h"

class AeroraVehicleServices;

/// **MOCK / TEST ONLY**
///
/// Deterministic driver for the Aerora vehicle integration surface, so other Aerora workstreams
/// can exercise that surface without hardware, without a MockLink connection and without a
/// `Vehicle`.
///
/// It is compiled only when `QGC_BUILD_TESTING` is on, and the guard at the top of this header
/// fails the build if it reaches a shipped configuration.
///
/// ### What it does not do
///
/// - **It does not model an aircraft.** It produces no position, attitude, battery, tether,
///   payload, pressure, flow, temperature or link-quality value, because no such value is
///   evidenced and a plausible-looking invented number is worse than no number.
/// - **It does not model telemetry state.** The REPORTED / EFFECTIVE / REQUESTED / PROPOSED /
///   STALE / UNKNOWN / UNAVAILABLE model belongs to the telemetry workstream, and the vehicle
///   integration layer holds no telemetry value for it to apply to. What this mock drives is the
///   state the integration layer actually has: discovery, connection, capability evidence,
///   classification and service attachment.
/// - **It has no timers and no randomness.** Every method is a direct state change, so a given
///   call sequence always produces the same observable sequence.
class AeroraVehicleMock : public QObject
{
    Q_OBJECT

public:
    explicit AeroraVehicleMock(QObject *parent = nullptr);
    ~AeroraVehicleMock() override;

    /// @return The mocked services object, or nullptr before discover().
    ///
    /// Its `vehicle()` is always nullptr: this mock does not fabricate a `Vehicle`, so a caller
    /// that dereferences it without a null check fails here rather than in the field.
    AeroraVehicleServices *services() const { return _services; }

    bool isDiscovered() const { return _services != nullptr; }
    bool isConnected() const { return _connected; }

    /// Creates the services object and emits vehicleDiscovered(). No-op if already discovered.
    void discover();

    /// Marks the mocked vehicle connected and emits vehicleConnected(). Requires discover().
    void connectVehicle();

    /// Marks the mocked vehicle disconnected and emits vehicleDisconnected().
    ///
    /// Capability reports are discarded, because evidence belongs to the connection that produced
    /// it. The services object survives so a test can assert the post-disconnect state; a real
    /// disconnect destroys the `Vehicle` and the services object with it, which forget() models.
    void disconnectVehicle();

    /// Destroys the services object, modelling the real teardown where `Vehicle` deletion takes
    /// the Aerora services with it.
    void forget();

    /// Reports @a key present in @a category.
    /// @return true if the report was accepted.
    bool reportCapabilityAvailable(AeroraVehicleCapabilities::Category category, const QString &key);

    /// Reports @a key absent in @a category.
    /// @return true if the report was accepted.
    bool reportCapabilityUnavailable(AeroraVehicleCapabilities::Category category, const QString &key);

signals:
    void vehicleDiscovered();
    void vehicleConnected();
    void vehicleDisconnected();
    void vehicleForgotten();

private:
    AeroraVehicleServices *_services = nullptr;
    bool _connected = false;
};

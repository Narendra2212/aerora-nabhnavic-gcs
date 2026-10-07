#pragma once

#include <QtCore/QObject>

class AeroraMissionObserver;
class MissionManager;
class Vehicle;

/// Feeds AeroraMissionObserver from QGroundControl's mission manager, and nothing else.
///
/// This is the **only** file in the Aerora mission module that includes `Vehicle` or
/// `MissionManager`. Everything else in the module is pure, which is what lets the observer, the
/// snapshot comparison and the resume boundary be tested without a vehicle, a link or an event
/// loop — and what lets the mission mock drive the same surface the real vehicle drives.
///
/// \section observes What it connects
///
/// Only `MissionManager` signals, and only to report into the observer:
///
/// | Signal | Reported as |
/// | --- | --- |
/// | `inProgressChanged` | transfer started / finished |
/// | `error` | an error during the current transaction |
/// | `newMissionItemsAvailable` | a download completed, with the vehicle's item list |
/// | `sendComplete` | an upload completed, with its error flag |
/// | `removeAllComplete` | a remove-all completed, with its error flag |
/// | `currentIndexChanged` | the aircraft's execution position |
///
/// \section notconnected What it deliberately does not connect, and why
///
/// \li **`lastCurrentIndexChanged`.** It is the *input* to QGroundControl's resume index
///     computation, not the resume index. The real computation lives in
///     `MissionController::resumeMissionIndex()`, needs the visual item list, applies a
///     firmware-dependent home offset and then backs up one item. Reproducing any part of that
///     here would be inventing synchronisation behaviour, so the resume candidate is reported by
///     whoever holds the `MissionController` instead.
/// \li **`progressPctChanged`.** Already exposed by `MissionController`; duplicating it adds a
///     second source of truth for a progress bar.
///
/// \section nocommand No command path
///
/// The bridge calls nothing on the vehicle. It never writes mission items, never generates a
/// resume mission, never sets the current mission sequence and never starts a mission. Those
/// remain QGroundControl's, driven from `PlanMasterController` / `MissionController` as before.
/// The bridge holds the `MissionManager` pointer only to connect to it and to read its item list.
///
/// \section lifetime Lifetime and dependency
///
/// Attaches the observer through `AeroraVehicleServices::attachService()` and parents itself to
/// that observer, so the whole graph is destroyed with the `Vehicle` and needs no teardown code.
///
/// It requires `AeroraVehicleServices` to already be attached to the vehicle, which the Aerora
/// firmware plugin does from `FirmwarePlugin::initializeVehicle()`. Creating the services object
/// here instead would take a lifecycle decision that belongs to the vehicle integration layer,
/// so attachTo() declines rather than reaching across that boundary. The consequence is that
/// mission observation is present only on vehicles routed to the Aerora firmware plugin.
class AeroraMissionBridge : public QObject
{
    Q_OBJECT

public:
    /// Attaches mission observation to @a vehicle.
    ///
    /// Idempotent: a vehicle that already has it keeps it and that instance is returned, so a
    /// repeated attach cannot produce two bridges double-reporting into one observer.
    ///
    /// @return The bridge, or nullptr when @a vehicle is null, has no mission manager, or has no
    ///         `AeroraVehicleServices` attached.
    static AeroraMissionBridge *attachTo(Vehicle *vehicle);

    /// @return The observer attached to @a vehicle, or nullptr when there is none. This is the
    ///         entry point for consumers that only want to read mission state.
    static AeroraMissionObserver *observerFor(Vehicle *vehicle);

    /// @return The observer this bridge reports into. Never null for a bridge obtained from
    ///         attachTo().
    [[nodiscard]] AeroraMissionObserver *observer() const { return _observer; }

private slots:
    void _inProgressChanged(bool inProgress);
    void _error(int errorCode, const QString &errorMessage);
    void _newMissionItemsAvailable(bool removeAllRequested);
    void _sendComplete(bool error);
    void _removeAllComplete(bool error);
    void _currentIndexChanged(int currentIndex);

private:
    AeroraMissionBridge(MissionManager *missionManager, AeroraMissionObserver *observer);

    MissionManager *const _missionManager = nullptr;
    AeroraMissionObserver *const _observer = nullptr;
};

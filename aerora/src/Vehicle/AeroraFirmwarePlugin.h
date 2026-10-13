#pragma once

#include <QtCore/QVariantList>

#include "FirmwarePlugin.h"

class AeroraVehicleCapabilities;
class AeroraVehicleServices;
class AutoPilotPlugin;
class Vehicle;

/// Aerora's firmware abstraction.
///
/// Derives from `FirmwarePlugin` directly and **not** from a vendor plugin. Deriving from
/// `PX4FirmwarePlugin`, as QGroundControl's reference overlay does, would inherit PX4 custom-mode
/// decoding, PX4 flight-mode tables and PX4 parameter conventions. Those would then be Aerora's
/// defaults, and wrong wherever Aerora firmware differs — a defect that is hard to see, because
/// the interface populates with plausible mode names rather than failing.
///
/// Deriving from the base instead makes every firmware-specific behaviour an explicit, reviewable
/// override. It also means the base-class defaults apply where Aerora has no evidence, and those
/// defaults are the safe ones:
///
/// - Flight-mode accessors return an empty string, so QGroundControl hides the dependent guided
///   action. A hidden action cannot be invoked by an operator who believes it works, whereas
///   guessing a mapping from Aerora authority modes to QGC flight-mode names would put invented
///   values on the path that decides whether a Return-to-Launch button appears. That mapping is
///   Q-007 and it is open.
/// - `isCapable()` returns false, so no optional capability is claimed before it is evidenced.
/// - `adjustIncomingMavlinkMessage()` returns true, so no message is dropped. The plugin observes;
///   a veto here would silently break `ParameterManager` and FTP for every vehicle of this class.
/// - `factGroups()` returns nullptr, so no Aerora fact group is registered. See below.
///
/// Those four are therefore deliberately **not** overridden: an override that restates the base
/// default is dead code that still has to be reviewed. `AeroraVehicleIntegrationTest` asserts each
/// of them instead, so the property is checked rather than merely intended, and a future change of
/// base class cannot alter it silently.
///
/// ### Aerora fact groups reach the vehicle through a registrar, never through `factGroups()`
///
/// `FirmwarePlugin::factGroups()` would be the only route to a vehicle's fact-group map needing no
/// core edit, and it is the wrong one. The plugin is a **singleton shared by every vehicle** of a
/// firmware class (`FirmwarePluginManager::firmwarePluginForAutopilot()` caches it), while
/// `Vehicle::_commonInit()` inserts whatever `factGroups()` returns into each vehicle's own map.
/// Groups held as plugin members would therefore be shared across vehicles, and two aircraft would
/// overwrite each other's telemetry. `ArduSubFirmwarePlugin` has exactly that shape upstream.
///
/// That was Q4-002. It is resolved in favour of per-vehicle registration (ADR-016), and the
/// authorized core edit is one friend declaration — `friend class AeroraVehicleFactGroupRegistrar;`
/// at `src/Vehicle/Vehicle.h`, following the same pattern upstream already grants to
/// `FactGroupListModel` and `GimbalController` for this exact purpose.
///
/// So the publication chain is:
///
/// ```text
/// AeroraTelemetrySet::publishTo(registrar)
///         ↓
/// AeroraVehicleFactGroupRegistrar          one per vehicle, holds that vehicle only
///         ↓
/// Vehicle::_addFactGroup(group, name)
///         ↓
/// that vehicle's own fact-group map
/// ```
///
/// A registrar is handed one vehicle by a caller that already has it, which is why cross-vehicle
/// sharing is structurally absent here rather than avoided by discipline.
///
/// **`factGroups()` is therefore not overridden, and must not be.** The base returning `nullptr` is
/// the guard against the shared-plugin architecture being wired back in by accident, and
/// `AeroraVehicleIntegrationTest` asserts it so the guard is checked rather than merely intended.
///
/// Note what this delivers today: the set is published, and it is **empty**, because no Aerora
/// dialect exists and so no concrete fact group does either. Publication of an empty set succeeds
/// and seals the set. The wiring is live and exercised; what flows through it is nothing yet.
///
/// ### This class is the per-vehicle composition root
///
/// `initializeVehicle()` is the only seam in QGroundControl's vehicle lifecycle that both runs
/// once per vehicle and receives the `Vehicle*`. That makes it the place where the Aerora domain
/// modules are composed onto a vehicle, and it is why each of those modules publishes an
/// attachment contract instead of reaching into `Vehicle` itself.
///
/// The composition is deliberately flat and ordered by dependency, not by domain importance.
/// Telemetry is first because it owns the freshness evaluator that the payload and power models
/// take as a constructor argument.
class AeroraFirmwarePlugin : public FirmwarePlugin
{
    Q_OBJECT

public:
    explicit AeroraFirmwarePlugin(QObject *parent = nullptr);
    ~AeroraFirmwarePlugin() override;

    /// Service name the per-vehicle `AeroraTelemetrySet` is attached under.
    ///
    /// Attaches AeroraVehicleServices to @a vehicle, resolves its product profile, and composes
    /// the per-vehicle Aerora domain services onto it.
    ///
    /// This is the Aerora integration point in QGroundControl's vehicle lifecycle. It is called
    /// once from each `Vehicle` constructor, after `_commonInit()` has resolved this plugin and
    /// built the vehicle's managers — which is why `Vehicle::missionManager()` is already
    /// available to the mission bridge by the time this runs.
    void initializeVehicle(Vehicle *vehicle) final;

    /// Composes the per-vehicle Aerora domain services onto @a services.
    ///
    /// Attachment order is dependency order: the telemetry set first, because it owns the
    /// `AeroraFreshness` evaluator that the payload and power models take as a constructor
    /// argument; then payload, power, and the mission bridge.
    ///
    /// Idempotent. A vehicle that already carries the telemetry service is left alone, so a
    /// repeated initialize cannot produce two models reporting into one vehicle.
    ///
    /// Two steps need the vehicle itself and are skipped without one: the mission bridge, which
    /// reads a `MissionManager` from it, and telemetry publication, which registers fact groups
    /// onto it. Everything else attaches either way, which is what lets the composition be tested
    /// without a MockLink connection — and it means a services object with no vehicle is left
    /// unpublished rather than publishing into nothing.
    ///
    /// Telemetry is published last, so every module composed before it has already had its chance
    /// to register a group. Publication seals the set, and sealing is one-way.
    ///
    /// Static, and taking the services object rather than the vehicle, for two reasons: the
    /// composition needs nothing from the plugin instance, and a test can drive it against a
    /// services object with no vehicle.
    static void attachDomainServices(AeroraVehicleServices *services);

    /// @return A new AeroraAutoPilotPlugin owned by @a vehicle, or nullptr when @a vehicle is null.
    ///
    /// This is QGroundControl's only seam for supplying a firmware class's vehicle setup pages.
    /// The base returns a `GenericAutoPilotPlugin`, whose single page is joystick setup;
    /// AeroraAutoPilotPlugin reproduces that page rather than dropping it. See that class for why
    /// it cannot simply derive from the generic one.
    ///
    /// Null is returned rather than delegating to the base when @a vehicle is null, because the
    /// base would construct a `GenericAutoPilotPlugin`, whose `AutoPilotPlugin` constructor reads
    /// `vehicle->firmwarePlugin()` unguarded. `Vehicle` always passes itself
    /// (`Vehicle.cc:145-146`), so the null case is reachable only from a direct call.
    AutoPilotPlugin *autopilotPlugin(Vehicle *vehicle) const final;

    /// The toolbar indicators for @a vehicle: the base set, plus any Aerora indicator whose
    /// capability this vehicle has reported available.
    ///
    /// Recomputed per call into this object's own member, and deliberately **not** appended to the
    /// inherited `_toolIndicatorList`. `FirmwarePluginManager::firmwarePluginForAutopilot()` caches
    /// one plugin instance per firmware class, so a capability-dependent list cached in the
    /// inherited member would be computed for whichever vehicle asked first and then served to
    /// every other vehicle of the class. That is the same shared-singleton hazard that keeps
    /// `factGroups()` unoverridden here.
    ///
    /// The returned reference is valid until the next call, which is the contract the base already
    /// has: it too returns a reference to a single shared member.
    const QVariantList &toolIndicators(const Vehicle *vehicle) final;

protected:
    /// @return The autopilot type matching the claimed firmware class, so parameter metadata
    ///         lookup follows AeroraFirmwareClaim::kFirmwareClass rather than being hard-coded.
    MAV_AUTOPILOT _autopilotType() const final;

private:
    /// Makes a capability report re-reach the toolbar by emitting `toolIndicatorsChanged()`.
    ///
    /// Indicator membership depends on reported capability, and capability reports arrive after
    /// connect. Without this the answer to the first query would become the permanent one, because
    /// nothing in QGroundControl's `src/` ever emits that signal — `Vehicle` only relays it
    /// (`Vehicle.cc:231`).
    ///
    /// A no-op while no Aerora indicator is declared: there is no membership to change, so the
    /// connection would emit on every capability report and alter nothing.
    void _installToolIndicatorInvalidation(AeroraVehicleCapabilities *capabilities);

    QVariantList _aeroraToolIndicators;
};

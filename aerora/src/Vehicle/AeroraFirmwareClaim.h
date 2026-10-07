#pragma once

#include "QGCMAVLink.h"

/// The single firmware class an Aerora build claims.
///
/// **Q-018 is OPEN.** The `MAV_AUTOPILOT` value Aerora flight-controller firmware reports is not
/// evidenced, so no Aerora-specific value is invented. Until it is answered, plugin selection is
/// made deterministic by **exclusion** rather than by identification: an Aerora build forces
/// `QGC_DISABLE_PX4_PLUGIN_FACTORY` and `QGC_DISABLE_APM_PLUGIN_FACTORY` on, so the Aerora factory
/// is the only one registered and cannot lose a first-match race.
///
/// This is a provisional build constant, not a decision. When Q-018 is answered, this one line
/// changes and nothing else does.
///
/// ### How selection actually works, source-verified at QGC `9459a4b5`
///
/// `FirmwarePluginManager::firmwarePluginForAutopilot()` passes the raw `MAV_AUTOPILOT` value from
/// the heartbeat into `_findPluginFactory(FirmwareClass_t)`, which tests
/// `factory->supportedFirmwareClasses().contains(firmwareClass)`
/// (`src/FirmwarePlugin/FirmwarePluginManager.cc`). It does **not** normalise the value through
/// `QGCMAVLink::firmwareClass()` first, and `FirmwareClass_t` is `typedef int`
/// (`src/MAVLink/QGCMAVLinkTypes.h`).
///
/// So a factory claiming `FirmwareClassGeneric` — which is `MAV_AUTOPILOT_GENERIC`, value 0 — is
/// selected **only** for a vehicle reporting exactly `MAV_AUTOPILOT_GENERIC`. Any other
/// unrecognised autopilot value matches no factory at all and falls through to the lazily created
/// base `FirmwarePlugin`, so it does not receive the Aerora plugin.
///
/// That is narrower than the mixed-fleet hazard recorded against Q-017, and it is narrower in the
/// safe direction. It is recorded here because it is the behaviour this constant depends on.
namespace AeroraFirmwareClaim {

/// The firmware class the Aerora factory claims. Provisional pending Q-018.
inline constexpr QGCMAVLink::FirmwareClass_t kFirmwareClass = QGCMAVLink::FirmwareClassGeneric;

} // namespace AeroraFirmwareClaim

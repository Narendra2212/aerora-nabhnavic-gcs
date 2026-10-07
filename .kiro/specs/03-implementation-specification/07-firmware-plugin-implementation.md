# Firmware Plugin Implementation

The registration chain, the selection hazard, the linkage requirement, and what happens when the
autopilot identifier is unknown.

## The chain

```text
FirmwarePluginFactory                     src/FirmwarePlugin/FirmwarePluginFactory.h:9
       ↓  subclass
AeroraFirmwarePluginFactory               static object, self-registering
       ↓  firmwarePluginForAutopilot()
AeroraFirmwarePlugin : FirmwarePlugin     cached singleton
       ↓  factGroups()                    FirmwarePlugin.h:353
Aerora FactGroups                         six groups
       ↓  attached per vehicle            Vehicle.cc:364-370
Aerora capabilities                       M-Capability gates presentation
       ↓
Aerora QML / UI                           M-Ui
```

## What Aerora must implement

Exactly two pure virtuals. Verified at `FirmwarePluginFactory.h:21,24`.

| Member | Signature | Aerora behaviour |
| --- | --- | --- |
| `firmwarePluginForAutopilot` | `FirmwarePlugin *(MAV_AUTOPILOT autopilotType, MAV_TYPE vehicleType)` | Return the cached `AeroraFirmwarePlugin` for the Aerora autopilot value; `nullptr` otherwise |
| `supportedFirmwareClasses` | `QList<QGCMAVLink::FirmwareClass_t>() const` | Return the single class Aerora claims |
| `supportedVehicleClasses` | Optional, defaults to all | Return multirotor only, matching the product |

## Registration

| Requirement | Mechanism | Evidence |
| --- | --- | --- |
| The factory registers itself | The base constructor appends to the registry | `FirmwarePluginFactory.cc:16` |
| A non-local static object must exist | `AeroraFirmwarePluginFactory AeroraFirmwarePluginFactoryImp;` | Pattern at `custom-example/src/FirmwarePlugin/CustomFirmwarePluginFactory.cc:4` |
| The translation unit must not be dropped | `extern` declaration in the header | `CustomFirmwarePluginFactory.h:27` |
| It must be in the **application target** | `target_sources(${CMAKE_PROJECT_NAME} ...)` via `CUSTOM_SOURCES` | `src/CMakeLists.txt:207-208` |

### Why linkage is the first-day hazard

A self-registering global in a **static library** can be discarded by the linker because nothing
references it. There is no diagnostic: the build succeeds, the application starts, and the Aerora
plugin is simply never registered. The operator sees a generic vehicle.

`AERORA-GCS-REQ-009` exists for this. The overlay must export its factory source through
`CUSTOM_SOURCES`, never compile it into an Aerora library. The structural overlay proof verifies it,
and the startup self-check verifies it again in a deployed build where no test runs.

## Selection, and the first-match hazard

`FirmwarePluginManager::_findPluginFactory` — `FirmwarePluginManager.cc:91-102` — returns the
**first** factory whose `supportedFirmwareClasses()` contains the requested class.

| Property | Consequence |
| --- | --- |
| Two factories claiming one class | First registered wins, **silently** |
| Registration order | Static-initialisation order across translation units; unspecified by the standard |
| Detection | None. No warning, no error |
| This is R-010 | Now located exactly |

### Mitigation, in order

| Step | Action | Core edit? |
| --- | --- | --- |
| 1 | Force `QGC_DISABLE_APM_PLUGIN_FACTORY ON` and `QGC_DISABLE_PX4_PLUGIN_FACTORY ON` in `CustomOverrides.cmake` | **No** — the options exist at `cmake/CustomOptions.cmake:187,190` |
| 2 | Claim a class no other factory claims, once Q-018 resolves | No |
| 3 | Assert the selected plugin type in an architectural-property test | No |
| 4 | Assert it again in the startup self-check | No |

Step 1 alone removes the hazard, because with both vendor factories disabled only Aerora's remains
registered. The reference overlay does exactly this at
`custom-example/cmake/CustomOverrides.cmake:43,46`.

### The side effect of step 1

Disabling the factories removes ArduPilot and PX4 from
`FirmwarePluginManager::supportedFirmwareClasses()`, so the setup UI stops offering them
(`src/Settings/AppSettings.cc:64-68` prunes unsupported classes). **The plugin implementations are
still compiled** — only the factory translation units are guarded — so this is a runtime
availability change, not a code removal.

That is the correct behaviour for a single-stack product. It is also irreversible at runtime: an
Aerora build cannot connect to a PX4 vehicle as PX4. Recorded, intended, and the subject of Q-017.

## Single-stack configuration

| Mechanism | Verified behaviour | Aerora use |
| --- | --- | --- |
| `FirmwarePluginManager::singleFirmwareSupport()` | True when exactly one class is supported | **Unreachable for UI simplification** — R-029, informational |
| `QGCOptions::multiVehicleEnabled()` | The correct seam for a single-vehicle product | Available if the product decides so |
| `supportedFirmwareClasses()` returning one entry | Prunes the setup UI | The actual mechanism |

R-029 recorded that the single-firmware UI simplification path is structurally unreachable. Phase 4
confirms it and uses the `QGCOptions` route instead — `AERORA-UI-REQ-004`'s lowest-tier rule.

## The autopilot identifier dependency

**Q-018 is OPEN. Phase 4 invents no `MAV_AUTOPILOT` value.**

| Question | State |
| --- | --- |
| Which `MAV_AUTOPILOT` value will Aerora firmware report? | **UNKNOWN** — Q-018, `IMPL-BLOCKING` |
| Which `FirmwareClass_t` will Aerora claim? | Follows from the above |
| Is a new enumeration member needed? | **Not determined.** That is a dialect and upstream question |

### Fallback while Q-018 is open

| Behaviour | Detail |
| --- | --- |
| Both vendor factories disabled | Only Aerora's factory is registered |
| Aerora claims one existing class provisionally | A build-time constant in the overlay, **one line to change** |
| Any heartbeat selects the Aerora plugin | Because no other factory can match |
| A genuinely unknown vehicle | Falls through to the generic `FirmwarePlugin` at `FirmwarePluginManager.cc:84-87` |
| The provisional claim is recorded | As a build constant, not as a decision |

This is the isolation strategy for `IF-MAV-021`: selection is made **deterministic by exclusion**
rather than by identification. It works today and becomes correct, not different, when Q-018
resolves.

### Fallback behaviour for unknown vehicles

| Situation | Result |
| --- | --- |
| A vehicle reports an autopilot Aerora does not claim | Generic `FirmwarePlugin`; standard QGC behaviour; no Aerora fact groups |
| No capability inventory reported | No Aerora capability presented; interface degrades to the standard set |
| Aerora fact groups absent | Panels not presented — **not presented empty** |

Graceful degradation to a working standard ground station is the floor. An Aerora build connected to
a non-Aerora vehicle must be useful, not broken.

## What `AeroraFirmwarePlugin` overrides

| Virtual | Aerora behaviour | Requirement |
| --- | --- | --- |
| `factGroups()` | Returns the Aerora group map — **subject to Q4-002** | `AERORA-GCS-REQ-010` |
| Flight-mode accessors | Return **empty** where no confirmed Aerora equivalent exists, so dependent actions are hidden | `AERORA-GCS-REQ-011` |
| `adjustIncomingMavlinkMessage` | Observation only; never vetoes. A veto would be logged | `AERORA-MAV-REQ-010` |
| `supportedMissionCommands` | The standard set until a dialect exists | `IF-MIS-012` |
| Metadata adjustment | Aerora parameter metadata where supplied | `IF-PAR-005` |

### What it must not override

| Virtual | Reason |
| --- | --- |
| Anything deriving from a vendor plugin | `AERORA-GCS-REQ-008` — inheriting a vendor plugin imports flight-mode and custom-mode assumptions invisible at the point of use |
| Failsafe-related behaviour | `AERORA-SAFE-REQ-008`; the ground station decides no failsafe action |
| Arming authority | Vehicle-owned |

`AeroraFirmwarePlugin` derives from `FirmwarePlugin` directly. The reference overlay derives from
`PX4FirmwarePlugin`; Aerora deliberately does not.

## Status

| Item | Status | Gate |
| --- | --- | --- |
| `AeroraFirmwarePluginFactory` | `IMPLEMENTATION-PLANNED` | Build host |
| `AeroraFirmwarePlugin` skeleton | `IMPLEMENTATION-PLANNED` | Build host |
| `factGroups()` override | `IMPLEMENTATION-PLANNED` single-vehicle; **`BLOCKED`** multi-vehicle | Q4-002 |
| Flight-mode accessors returning empty | `IMPLEMENTATION-PLANNED` | Build host |
| Aerora `MAV_AUTOPILOT` claim | `CONTRACT-ONLY` | Q-018 |
| Mission command set | `CONTRACT-ONLY` | Dialect |
| Plugin-selection property test | `IMPLEMENTATION-PLANNED` | Build host |

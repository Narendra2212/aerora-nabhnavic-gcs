# Vehicle Integration

How Aerora attaches to QGroundControl's vehicle model: the firmware abstraction, the FactGroup
route for Aerora telemetry, parameters, the setup views, and the power model.

This is the single most important integration document, because the `Vehicle` object is where
QGroundControl concentrates vehicle state and where almost every Aerora domain module needs a
foothold.

## Attachment summary

| Aerora element | Seam | Core edit |
| --- | --- | --- |
| `AeroraFirmwarePluginFactory` | Self-registering global static object | None |
| `AeroraFirmwarePlugin` | `FirmwarePlugin` subclass returned by the factory | None |
| Aerora FactGroups | `FirmwarePlugin::factGroups()` override | None |
| Aerora MAVLink handling | `FirmwarePlugin::adjustIncomingMavlinkMessage()` | None |
| Aerora module binding to a vehicle | `FirmwarePlugin::initializeVehicle()` | None |
| `AeroraAutoPilotPlugin` and setup pages | `FirmwarePlugin::autopilotPlugin()` | None |
| Single-stack configuration | Two existing CMake options | None |
| `singleFirmwareSupport` UI simplification | Not reachable — see below | Would require one |

Eight of nine attach with no modification to QGroundControl. The ninth is not needed; the same
outcome is reachable by another seam.

## Firmware plugin attachment

### The mechanism

QGroundControl's firmware abstraction is selected through a self-registering factory. The mechanism
is static-initialisation based and is confirmed in source:

```text
AeroraFirmwarePluginFactory.cc
  AeroraFirmwarePluginFactory AeroraFirmwarePluginFactoryImp;   // file-scope global static
        │
        │ constructor runs before main()
        ▼
  FirmwarePluginFactory::FirmwarePluginFactory()                 FirmwarePluginFactory.cc:16
        │
        ▼
  FirmwarePluginFactoryRegister::instance()->registerPluginFactory(this)
        │
        ▼
  FirmwarePluginManager::_findPluginFactory(firmwareClass)       FirmwarePluginManager.cc:94-104
        │   first factory whose supportedFirmwareClasses() contains the class
        ▼
  factory->firmwarePluginForAutopilot(...)                       FirmwarePluginManager.cc:81
        │
        ▼
  Vehicle::_firmwarePlugin
```

The register itself is a `Q_GLOBAL_STATIC` (`FirmwarePluginFactory.cc:26`), so it is constructed on
first use and the ordering hazard is confined to the factories, not the register.

### Two hard constraints on where Aerora code lives

This mechanism places two real constraints on the build layout. Both come from the same cause: a
global object nobody references is a linker's candidate for removal.

| Constraint | Reason |
| --- | --- |
| The factory translation unit must be in the overlay's `CUSTOM_SOURCES`, compiled into the main application target | A self-registering object inside a static library can be discarded, because no symbol in it is referenced |
| The factory header must carry an `extern` declaration of the instance | It anchors the object — the pattern `custom-example` uses at `CustomFirmwarePluginFactory.h:27` |

The overlay already separates these two cases: `CUSTOM_SOURCES` is exported with `PARENT_SCOPE` into
the main target, while `CUSTOM_LIBRARIES` holds only QML modules. Aerora must keep that split.
**Any Aerora C++ that relies on static self-registration belongs in `CUSTOM_SOURCES`, never in an
Aerora static library.** This is a build-layout rule with a silent failure mode, which is why it is
recorded here rather than left to convention.

### Which firmware class Aerora claims

`FirmwareClass_t` is an alias for `MAV_AUTOPILOT`, and `QGCMAVLink::isGenericFirmwareClass()`
returns true for every value that is neither PX4 nor ArduPilot. There is no vendor slot.

`_findPluginFactory` returns the **first** factory claiming the class, and registration order is
static-initialisation order across translation units — unspecified in C++. Two factories claiming
the same class is therefore a silent, non-deterministic conflict. This is R-010.

The architecture avoids the conflict rather than ordering around it:

| Build configuration | Aerora factory claims | Conflict risk |
| --- | --- | --- |
| Single-stack, PX4 and APM factories disabled | `FirmwareClassGeneric` | None — no other factory is registered |
| Multi-stack, PX4 and APM factories enabled | `FirmwareClassGeneric` | None — neither upstream factory claims Generic |

Both configurations are safe, because upstream's PX4 factory claims only `FirmwareClassPX4` and the
APM factory only `FirmwareClassArduPilot`. `FirmwareClassGeneric` is appended by
`FirmwarePluginManager::supportedFirmwareClasses()` itself (`FirmwarePluginManager.cc:35`) but is
claimed by no upstream factory, so an Aerora factory claiming it wins unambiguously and takes
precedence over the `_genericFirmwarePlugin` fallback at `FirmwarePluginManager.cc:83-88`.

The cost is the converse case: in a multi-stack build, **any** unrecognised autopilot also resolves
to `FirmwareClassGeneric` and would be handed the Aerora plugin. That is acceptable in a
single-stack product and a hazard in a mixed fleet. This is the substance of Q-017, and it is why
the Aerora plugin must not assume the vehicle is Aerora hardware without corroboration from the
Aerora system status message.

Q-018 — what `MAV_AUTOPILOT` value the Aerora flight controller reports — remains open. The
architecture does not depend on the answer: any value that is not PX4 or ArduPilot maps to Generic.
What the answer changes is only whether Aerora needs to distinguish itself from other generic
autopilots, which the capability inventory already does.

### Derive from `FirmwarePlugin`, not from `PX4FirmwarePlugin`

`custom-example` derives its plugin from `PX4FirmwarePlugin`, and the PX4 plugin source is compiled
unconditionally — only the factory translation unit is gated by the CMake option. So the
PX4-derived route is available to Aerora.

It should not be taken. `PX4FirmwarePlugin` carries PX4 custom-mode decoding, PX4 flight-mode
tables and PX4 parameter conventions. Inheriting them would mean Aerora's flight-mode behaviour is
PX4's by default and silently wrong wherever Aerora firmware differs — exactly the class of defect
that is hardest to detect, because the UI would populate with plausible mode names.

**`AeroraFirmwarePlugin` derives from `FirmwarePlugin` directly.** Every firmware-specific
behaviour is then an explicit, reviewable override rather than an inherited assumption.

### The `singleFirmwareSupport` finding

QGroundControl simplifies several UI paths when only one firmware class is supported, exposed to QML
as `QGroundControl.singleFirmwareSupport` and `singleVehicleSupport`, and consumed by
`PlanInfoEditor.qml`, `InitialSetupPrompt.qml` and `QGCCorePlugin::showInitialSetupVehiclePreferences()`.

Neither flag can ever be true in an Aerora build. `supportedFirmwareClasses()` appends the classes
reported by every registered factory and then unconditionally appends `FirmwareClassGeneric`
(`FirmwarePluginManager.cc:33-35`) into a plain `QList` that does not de-duplicate. With the Aerora
factory registered the list has at least two entries, so `count() == 1` is unreachable — including
in a single-stack build, and including when the Aerora factory claims Generic itself.

This does not block anything. The same simplification is reachable through two seams Aerora already
owns:

| Target | Route | Core edit |
| --- | --- | --- |
| `showInitialSetupVehiclePreferences` | `virtual` on `QGCCorePlugin` — override in `AeroraPlugin` | None |
| `PlanInfoEditor.qml`, `InitialSetupPrompt.qml` | Overlay QML override by resource alias | None |

Recorded so that nobody spends time setting a CMake option expecting the flags to flip. An upstream
fix — de-duplicating the list — would be a small, clearly correct contribution, and is listed in
[risk-gates.md](risk-gates.md) as an optional upstream item rather than an Aerora dependency.

## What `AeroraFirmwarePlugin` overrides

| Virtual | Aerora behaviour | Status |
| --- | --- | --- |
| `factGroups()` | Returns the six Aerora FactGroups | `PROPOSED` |
| `initializeVehicle()` | Binds Aerora domain modules to the vehicle | `PROPOSED` |
| `isCapable()` | Derived from the reported Aerora capability inventory | `PROPOSED` |
| `adjustIncomingMavlinkMessage()` | Aerora message handling that is not telemetry | `PROPOSED` |
| `autopilotPlugin()` | Returns `AeroraAutoPilotPlugin` | `PROPOSED` |
| `supportedMissionCommands()` | Aerora mission command set | `OPEN` — depends on Q-023 |
| `flightModes()` and the named mode accessors | Aerora flight modes | `OPEN` — depends on Q-007 |
| `paramNameRemapMajorVersionMap()` | Not used; Aerora has no legacy parameter names to remap | `PROPOSED` |

### Flight modes are deliberately not specified here

`FirmwarePlugin` exposes roughly a dozen named mode accessors — `pauseFlightMode()`,
`missionFlightMode()`, `rtlFlightMode()`, `landFlightMode()`, `takeOffFlightMode()`,
`takeControlFlightMode()`, `gotoFlightMode()` and others — each returning an empty string in the
base class. QGroundControl's guided actions call them by name and hide the corresponding action when
the string is empty.

**This document does not populate them.** The Aerora baseline defines firmware authority modes with
sentinel values, flags its safe or default mode code as requiring confirmation, and leaves several
mode semantics marked as pending. Guessing a mapping from Aerora authority modes to QGroundControl
flight-mode names would put invented values on the path that decides whether an operator sees a
Return-to-Launch button.

What the architecture does commit to is the shape:

| Commitment | Consequence |
| --- | --- |
| Each accessor is an explicit override with a documented Aerora source | No inherited PX4 default can leak in |
| An accessor with no confirmed Aerora equivalent returns empty | The dependent action is hidden rather than mis-wired |
| Hiding is the safe failure, not disabling | A hidden action cannot be invoked by an operator who believes it works |
| The mapping table lives in one file | A single reviewable artifact for the firmware team to confirm |

Tracked as Q-007, ranked eleventh in [phase-2-open-question-resolution.md](phase-2-open-question-resolution.md).
It is architecture-shaping, not architecture-blocking: the interface exists either way.

Note the asymmetry with the Authority module. Flight mode in QGroundControl's sense and Aerora
authority mode are **not the same concept** and must not be conflated. Authority mode is who may
command the aircraft; flight mode is what the aircraft is doing. Whether they map one-to-one is
itself part of Q-007. [authority-model.md](authority-model.md) treats authority as its own state
machine for this reason.

## Aerora telemetry through FactGroups

### The route, and why it is not the obvious one

`Vehicle` inherits `_addFactGroup` from `FactGroup`, where it is protected, and grants access to
exactly two collaborators through `friend` declarations (`Vehicle.h:108` and `Vehicle.h:115`).
Adding `friend class AeroraSomething;` would be a one-line core edit, and it is tempting because it
is so small.

Aerora does not take that route. `FirmwarePlugin::factGroups()` already exists for precisely this
purpose, and `Vehicle` consumes it during construction:

```text
Vehicle::Vehicle(...)
  ├── _addFactGroup(_gpsFactGroup, ...)                      Vehicle.cc:345-362   (core groups)
  └── fwFactGroups = _firmwarePlugin->factGroups()           Vehicle.cc:365
        for each (name, group):
            _addFactGroup(group, name)                       Vehicle.cc:368
```

`ArduSubFirmwarePlugin` uses the same seam for its `apmSubInfo` group
(`ArduSubFirmwarePlugin.cc:194`), which confirms it is a supported extension point and not an
accident.

### What this buys for free

Registering through `factGroups()` yields three things an Aerora-owned container would not:

| Benefit | Mechanism |
| --- | --- |
| Automatic MAVLink delivery | `Vehicle` dispatches every inbound message to every FactGroup — `Vehicle.cc:587-590` |
| Appearance in the telemetry value picker | `InstrumentValueData::factGroupNames()` enumerates the vehicle's groups |
| Inclusion in telemetry logging | `Vehicle` enumerates group facts when writing values — `Vehicle.cc:2814` and `:2845` |

The second and third matter more than they look. An operator-configurable telemetry panel and a
recorded flight log that contains Aerora values are both baseline expectations, and both come from
being on the `Vehicle` rather than beside it.

### The six Aerora FactGroups

| Group | Carries | Primary consumer |
| --- | --- | --- |
| `aeroraSystem` | Aerora system status, dialect version, capability inventory | Diagnostics, Authority |
| `aeroraPayload` | Payload state, actuator status, payload health | Payload |
| `aeroraPaint` | Product-specific application telemetry | UI, Mission |
| `aeroraPower` | Tether and power-chain state | Power |
| `aeroraLink` | Vehicle-reported link arbitration and per-radio status | LinkState |
| `aeroraAutonomy` | Companion proposals, perception status, freshness | Autonomy |

Each group is a `FactGroup` subclass with a `handleMessage` override, which is the only code path by
which Aerora telemetry enters the model. That single entry point is what makes the freshness rule in
[runtime-architecture.md](runtime-architecture.md) enforceable: every Aerora value is stamped where
it is parsed, because there is one place where parsing happens.

### Message delivery is by system id, not component id

`Vehicle::_mavlinkMessageReceived` filters on `sysid` only (`Vehicle.cc:525`). Messages from any
component sharing the vehicle's system id reach the vehicle and therefore reach every FactGroup.
Packet-loss accounting is scoped to the first-heard component id (`Vehicle.cc:541-543`), so traffic
from other components does not corrupt the loss counter.

This is the property that makes companion-computer observability free. It is developed in
[gpu-integration.md](gpu-integration.md).

## Parameters

All Aerora vehicle parameters are Facts obtained through `ParameterManager`. There is no Aerora
parameter cache, no Aerora parameter store, and no Aerora code that reads a parameter from anywhere
other than a Fact. This is the first architectural principle and it is not negotiable; Phase 1
found the parameter subsystem to be one of QGroundControl's better-engineered areas, including FTP
bulk download, caching and metadata.

| Element | Aerora approach |
| --- | --- |
| Parameter transport | Unchanged — `ParameterManager`, FTP where the vehicle supports it |
| Parameter metadata | An Aerora parameter metadata JSON, supplied through the overlay |
| Units, bounds, enums, translation | From metadata, as for any firmware |
| Access from Aerora modules | `getParameterFact()`, never a raw MAVLink read |
| Access from QML | Fact binding |

### The component-id assumption

Parameter handling across QGroundControl assumes the autopilot is component 1, both in
`ParameterManager` and in `FactPanelController`. An Aerora architecture that put flight-relevant
parameters on a non-autopilot component would be fighting the framework at many points.

The architecture therefore requires that **flight-controller parameters live on component 1**.
Payload and companion parameters, if they exist as MAVLink parameters at all, are a separate
question — and the preferred answer is that they do not, because payload state belongs to the
payload controller. See [payload-architecture.md](payload-architecture.md).

## Setup views

`FirmwarePlugin::autopilotPlugin()` returns the `AutoPilotPlugin` that supplies the Vehicle Setup
page list. `AeroraAutoPilotPlugin` returns an Aerora component list, built on the same
`VehicleComponent` pattern the PX4 and APM plugins use.

| Aerora setup component | Content | Status |
| --- | --- | --- |
| Aerora system | Identity, firmware and dialect versions, capability inventory | `PROPOSED` |
| Radios | Per-radio configuration and observed state | `PROPOSED` |
| Power and tether | Power-chain configuration | `PROPOSED` |
| Payload | Payload configuration, where it is parameter-backed | `OPEN` — Q-023 |
| Security | Signing, identity, credential state | `PROPOSED` |

Two patterns from upstream are worth copying deliberately. Both PX4 and APM build their component
list only after `parametersReady`, and both gate individual pages on the existence of specific
parameters — `PX4AutoPilotPlugin.cc:122` tests for a named parameter before adding a page. Aerora
should gate pages on the **capability inventory** rather than on parameter existence, which is the
same technique applied to a more explicit signal, and which is what lets one GCS build serve
multiple Aerora products.

## Power model

The Aerora baseline defines a set of power states for the tethered power chain, together with a
watchdog fault path that disarms and independently triggers a payload emergency stop.

### Representation

| Element | Representation |
| --- | --- |
| Power state | A Fact in `aeroraPower` with an enum metadata table, one entry per baseline state |
| Power state source | The reporting component, carried alongside the value |
| Freshness | Receive time, as for all Aerora values |
| Watchdog fault | A distinct Fact, not a power state value |
| Tether voltage and current | Facts with units from metadata |

The watchdog fault is separated from the power state deliberately. The baseline describes it as
acting directly and independently — disarm, and a payload emergency stop that does not route
through the payload state machine. Folding it into the power state enum would imply it is one of a
sequence of states the system moves through, which misrepresents an independent safety path.

### What the GCS does with it

Nothing authoritative. The GCS displays power state, logs it, and raises operator-facing alerts. It
does not initiate power transitions as a safety response, and it does not infer a power state the
vehicle has not reported. Power sequencing is vehicle-side; this follows directly from the
supervisory-GCS principle.

### Requirement prefix reconciliation

Phase 1 published power requirements under `AERORA-PWR-REQ-`. The Phase 2 directive names the
domain `AERORA-POWER-REQ-`. The directive is the higher authority, so **`AERORA-POWER-REQ-` is
canonical from Phase 2 onward.** The six Phase 1 identifiers are carried as superseded aliases in
[traceability.md](traceability.md); Phase 1 documents are not edited, because they are a frozen
reviewed baseline and rewriting them would invalidate that review.

## Single-stack and multi-stack configuration

Q-017 asks whether the Aerora build supports only Aerora vehicles or also PX4 and ArduPilot. The
answer is a build-time configuration, and the mechanism already exists:

| Option | Declared | Honoured |
| --- | --- | --- |
| `QGC_DISABLE_APM_PLUGIN_FACTORY` | `cmake/CustomOptions.cmake:187` | `src/FirmwarePlugin/APM/CMakeLists.txt:1` |
| `QGC_DISABLE_PX4_PLUGIN_FACTORY` | `cmake/CustomOptions.cmake:190` | `src/FirmwarePlugin/PX4/CMakeLists.txt:1` |

Both options gate only the factory translation unit; the plugin sources themselves remain compiled.
`custom-example/cmake/CustomOverrides.cmake:43` and `:46` force both ON, which is the pattern to
follow.

| Configuration | Options | Consequence |
| --- | --- | --- |
| Single-stack (recommended default) | Both ON | Only Aerora vehicles connect; no factory conflict; smaller UI surface |
| Multi-stack | Both OFF | PX4 and ArduPilot vehicles usable for development and SITL; unrecognised autopilots resolve to the Aerora plugin |

The architecture supports either, with **zero core edits in both cases**. The recommendation is
single-stack for release builds and multi-stack for development builds, because the development
value of SITL is real and the mixed-fleet hazard only matters in the field.

Recorded as ADR-001, which covers overlay strategy including this configuration.

## Vehicle identity

The Aerora fleet identifier is not the MAVLink system id. The mapping between them, and why the
distinction is load-bearing for the cloud registry and the audit trail, is in
[data-model.md](data-model.md). R-017 is gated at `G-ARCH` for this reason.

One constraint belongs here, because it is a `Vehicle` property: the architecture must not assume
MAVLink system id is stable across a vehicle's lifetime or unique across a fleet. Any Aerora
artifact that outlives a connection — a log, a cloud record, a configuration binding — keys on the
Aerora identifier, never on the system id.

## What Aerora must not do here

| Prohibition | Reason |
| --- | --- |
| Store a vehicle parameter anywhere other than a Fact | First architectural principle |
| Dereference a `Vehicle*` without a null check | Enforced by the repository's own lint rule |
| Add Aerora state to `Vehicle` itself | It belongs in an Aerora FactGroup or module, reachable through the extension layer |
| Infer a flight mode or authority mode the vehicle has not reported | Telemetry is the only source of vehicle state |
| Derive `AeroraFirmwarePlugin` from a vendor plugin | Inherits assumptions that are invisible at the point of use |
| Put self-registering code in a static library | Silent linker-dependent failure |

## Core edits required

**None.** Every attachment point above is an existing virtual, an existing seam, or an existing
build option.

One optional upstream contribution is identified — de-duplicating
`FirmwarePluginManager::supportedFirmwareClasses()` so that `singleFirmwareSupport` becomes
reachable. It is not on the Aerora critical path and Aerora does not wait for it.

## Open questions

| Question | Subject | Impact |
| --- | --- | --- |
| Q-007 | Aerora flight-mode and authority-mode mapping | `ARCH-SHAPING` |
| Q-017 | Single-stack or multi-stack build | `ARCH-SHAPING` |
| Q-018 | Aerora `MAV_AUTOPILOT` identifier | `IMPL-BLOCKING` |
| Q-023 | Payload actuator model, which decides the payload setup page | `ARCH-SHAPING` |
| Q-024 | The frozen power architecture, which gates the power state model's content | `DEFERRABLE` |

## Requirements derived

| Identifier | Statement |
| --- | --- |
| `AERORA-GCS-REQ-007` | Aerora shall attach its firmware abstraction through a self-registering `FirmwarePluginFactory` requiring no core modification |
| `AERORA-GCS-REQ-008` | `AeroraFirmwarePlugin` shall derive from `FirmwarePlugin` and shall not derive from a vendor firmware plugin |
| `AERORA-GCS-REQ-009` | Aerora C++ relying on static self-registration shall be compiled into the main application target, not into an Aerora static library |
| `AERORA-GCS-REQ-010` | Aerora telemetry shall be exposed as FactGroups registered through `FirmwarePlugin::factGroups()` |
| `AERORA-GCS-REQ-011` | A flight-mode accessor with no confirmed Aerora equivalent shall return empty so that the dependent action is hidden |
| `AERORA-GCS-REQ-012` | Aerora flight-controller parameters shall be addressed on MAVLink component 1 |
| `AERORA-GCS-REQ-013` | Aerora setup pages shall be gated on the reported capability inventory rather than on hard-coded product assumptions |
| `AERORA-GCS-REQ-014` | Aerora artifacts that outlive a connection shall key on the Aerora vehicle identifier and not on the MAVLink system id |
| `AERORA-POWER-REQ-007` | The watchdog fault shall be represented separately from the power state and shall not be modelled as a power state value |
| `AERORA-POWER-REQ-008` | The GCS shall not initiate a power transition as a safety response |

Full records in [requirements.md](requirements.md).

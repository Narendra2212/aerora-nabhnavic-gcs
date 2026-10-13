# Aerora Vehicle Integration

How Aerora attaches to QGroundControl's vehicle model, what it deliberately does not do, and what
the overlay root must provide for this subtree to build.

Everything in this document was read from this repository at QGC `HEAD 9459a4b5`. Where an earlier
phase document and the current source disagree, the source wins and the difference is recorded under
[Corrections to earlier phases](#corrections-to-earlier-phases).

## Status

| Item | Status |
| --- | --- |
| QGC vehicle architecture | `VERIFIED` — read at `9459a4b5`, cited below |
| Integration seam selection | `VERIFIED` — the composition seam itself needs no core edit |
| Source in this subtree | `IMPLEMENTED-UNCOMPILED` |
| Tests in this subtree | `IMPLEMENTED-UNCOMPILED` |
| QGC core modifications by this workstream | **NONE.** Two authorized core edits exist — the `AeroraVehicleFactGroupRegistrar` friend declaration (Q4-002/ADR-016) and the `startCalibration()` authorization gate — both made by M-Plugin / M-Security, not here |
| Wiring into `aerora/CMakeLists.txt` | Serviced by M-Plugin. This module exports `AERORA_VEHICLE_TEST_NAMES`; the `add_qgc_test` lines are M-Plugin's |
| Vehicle setup pages — `AeroraAutoPilotPlugin` (U4) | `IMPLEMENTED-UNCOMPILED` — seam complete; **no page publishes**, because the Aerora setup QML does not exist. See [Vehicle setup pages and tool indicators](#vehicle-setup-pages-and-tool-indicators-u4-u5) |
| Vehicle setup pages — the JSON-driven `vehicleConfigJson()` path | `CONTRACT-ONLY` — the override exists and returns empty. A page definition names real parameters, and no Aerora parameter is evidenced |
| Vehicle setup pages — calibration (`SCR-VEH-05`) | **WITHHELD** by decision while `Q-029` is open, with the reason in source and a test asserting the absence |
| Per-vehicle tool indicators — `toolIndicators()` (U5) | `IMPLEMENTED-UNCOMPILED` — gate and per-vehicle resolution complete; **the declared set is empty** |
| Mission attachment | `IMPLEMENTED-UNCOMPILED` |
| Payload attachment | `IMPLEMENTED-UNCOMPILED` |
| Power attachment | `IMPLEMENTED-UNCOMPILED` |
| Telemetry attachment — per-vehicle ownership and freshness | `IMPLEMENTED-UNCOMPILED` |
| Telemetry attachment — Aerora FactGroup publication (Q4-002) | `IMPLEMENTED-UNCOMPILED` — wired through the registrar; publishes an empty set until a dialect exists |
| `Vehicle::startCalibration()` authorization bypass | **Closed** — Option A authorized and implemented in core by M-Plugin / M-Security. One residual audit observation below |
| Aerora `MAV_AUTOPILOT` value | `OPEN` — Q-018 |
| Aerora fleet identity format | `OPEN / NOT-EVIDENCED` |
| Aerora capability identifiers | `OPEN / NOT-EVIDENCED` |
| Product profiles | `CONTRACT-ONLY` — interface exists, no profile implemented |

**Nothing here has been compiled.** This host has no compiler, no CMake, no Ninja, no Qt and
2.02 GB free on its only drive against the 40 GB required, so no build or test has run. Verification
that did happen is listed
under [What was actually verified](#what-was-actually-verified).

## What this layer is

A per-vehicle Aerora object graph attached to QGroundControl's `Vehicle` through two existing
virtuals, plus the attachment points other Aerora workstreams hang their own code from.

It is not a product. It contains no painting, firefighting, agriculture or inspection behaviour,
because painting is a product profile rather than the architecture.

## QGroundControl's vehicle lifecycle

Source-verified. Function names are exact; this is the path an Aerora vehicle actually takes.

```text
HEARTBEAT
    │  MAVLinkProtocol emits vehicleHeartbeatInfo(link, sysid, compid, autopilot, type)
    ▼
MultiVehicleManager::_vehicleHeartbeatInfo()
    │  ignores anything that is not MAV_COMP_ID_AUTOPILOT1, then duplicate/ignore checks
    ▼
new Vehicle(link, vehicleId, componentId, firmwareType, vehicleType, this)
    │  the only runtime Vehicle construction site
    ▼
Vehicle::_commonInit(link)
    ├── _firmwarePlugin = FirmwarePluginManager::instance()
    │                         ->firmwarePluginForAutopilot(_firmwareType, _vehicleType)
    ├── builds MissionManager, ParameterManager, VehicleLinkManager, MavCommandQueue, ...
    ├── _addFactGroup(...) for each core fact group
    └── _addFactGroup(...) for each entry in _firmwarePlugin->factGroups()
    ▼
_autopilotPlugin = _firmwarePlugin->autopilotPlugin(this)
    ▼
_initialConnectStateMachine->start()
    ▼
_firmwarePlugin->initializeVehicle(this)        ◄── the Aerora integration point
    ▼
_firmwarePlugin->adjustMetaData() for each top-level Vehicle fact
    ▼
MultiVehicleManager emits vehicleAdded(vehicle)
```

Teardown is two-phase: `VehicleLinkManager::allLinksRemoved` →
`MultiVehicleManager::_deleteVehiclePhase1` → `vehicleRemoved` → a short single-shot timer →
`_deleteVehiclePhase2`. Nothing in this subtree hooks either phase, because everything it owns is a
`QObject` child of the `Vehicle` and dies with it.

Inbound traffic reaches a vehicle through `Vehicle::_mavlinkMessageReceived`, in this order:

1. `_firmwarePlugin->adjustIncomingMavlinkMessage(this, &message)` — returning false drops the
   message for the whole application, including `ParameterManager` and FTP.
2. `QGCCorePlugin::instance()->mavlinkMessage(this, link, message)`.
3. Terrain, FTP, parameter, image, RemoteID and request-message handlers.
4. `handleMessage(this, message)` on every registered child `FactGroup`.
5. The vehicle's own message switch.

Filtering is by **system id only**, so messages from any component sharing the vehicle's system id
reach every fact group. That is what makes companion-computer observability free, and it is also why
a fact group must not assume a message came from the autopilot.

## The Aerora integration point

Two existing virtuals, no core edit:

| Aerora element | Seam | Core edit |
| --- | --- | --- |
| `AeroraFirmwarePluginFactory` | Non-local static; the `FirmwarePluginFactory` base constructor registers it | None |
| `AeroraFirmwarePlugin` | `FirmwarePlugin` subclass returned by the factory | None |
| `AeroraVehicleServices` and everything under it | `FirmwarePlugin::initializeVehicle()` | None |
| Parameter metadata lookup | `FirmwarePlugin::_autopilotType()` | None |
| Single-stack configuration | `QGC_DISABLE_PX4_PLUGIN_FACTORY`, `QGC_DISABLE_APM_PLUGIN_FACTORY` | None |

`initializeVehicle()` was chosen over `factGroups()` as the attachment point because it is the only
seam that **receives the `Vehicle` pointer**. `factGroups()` takes no argument, so a shared plugin
cannot tell which vehicle is asking — the root of Q4-002.

### Why not a vendor base class

`AeroraFirmwarePlugin` derives from `FirmwarePlugin` directly. QGroundControl's reference overlay
derives from `PX4FirmwarePlugin`, and that route is available, but it inherits PX4 custom-mode
decoding, PX4 flight-mode tables and PX4 parameter conventions. Those would become Aerora's
defaults and be wrong wherever Aerora firmware differs, with the interface populating with
plausible mode names rather than failing. Deriving from the base makes every firmware-specific
behaviour an explicit override, and leaves the safe defaults in place where there is no evidence.

### The linkage hazard

`AeroraFirmwarePluginFactory.cc` must reach the **application target** through the overlay's
`CUSTOM_SOURCES` export. A self-registering global inside a static library can be discarded by the
linker, and the failure is silent: the build succeeds, the application starts, the plugin is never
registered, and the operator sees a generic vehicle.

`AeroraVehicleIntegrationTest::_factoryRegistersItselfAndClaimsOneFirmwareClass` finds the factory
through `FirmwarePluginFactoryRegister` rather than by naming the global, specifically so that it
fails when the translation unit is dropped instead of forcing it to link.

## Files

| File | Purpose |
| --- | --- |
| `AeroraFirmwareClaim.h` | The one firmware class an Aerora build claims. One line to change when Q-018 resolves |
| `AeroraFirmwarePluginFactory.{h,cc}` | Self-registering factory |
| `AeroraFirmwarePlugin.{h,cc}` | `FirmwarePlugin` subclass; attaches services in `initializeVehicle()`, supplies the autopilot plugin and the tool indicators |
| `AeroraAutoPilotPlugin.{h,cc}` | `AutoPilotPlugin` subclass; the vehicle setup page index |
| `AeroraVehicleComponent.{h,cc}` | The setup page type and the page catalogue |
| `AeroraToolIndicators.{h,cc}` | Per-vehicle toolbar indicator set and its capability gate |
| `AeroraVehicleServices.{h,cc}` | The per-vehicle hub, parented to the `Vehicle` |
| `AeroraVehicleIdentity.{h,cc}` | MAVLink identity and Aerora fleet identity, kept apart |
| `AeroraVehicleCapabilities.{h,cc}` | Five capability categories with a three-state evidence model |
| `AeroraProductProfile.{h,cc}` | Product-family interface and registry. No profile implemented |
| `AeroraVehicleLog.{h,cc}` | `Aerora.Vehicle` logging category |
| `Vehicle.cmake` | Variable-only source manifest. Defines no target and no `PARENT_SCOPE` |

Under `aerora/test/Vehicle/`:

| File | Purpose |
| --- | --- |
| `AeroraVehicleMock.{h,cc}` | **MOCK / TEST ONLY** deterministic driver |
| `AeroraVehicleIntegrationTest.{h,cc}` | Architectural-property tests, `Unit` |
| `AeroraVehicleSetupTest.{h,cc}` | Setup page catalogue and indicator gate, `Unit` |
| `AeroraProfileRegistryTest.{h,cc}` | Profile registration and selection direction, `Unit` |
| `AeroraVehicleLifecycleTest.{h,cc}` | Lifecycle on a real `Vehicle`, `Integration Vehicle` |

The layout and the manifest style follow `aerora/src/Telemetry/Telemetry.cmake`, which established
the convention for this overlay: production sources under `aerora/src/<Module>/`, tests under
`aerora/test/<Module>/`, and a variable-only `<Module>.cmake` that the overlay root aggregates.

## Classification

`AeroraVehicleServices::Classification` separates two things that are easy to conflate:

- `SelectedBySeam` — QGroundControl routed this vehicle to the Aerora plugin. That is a
  **build-configuration outcome**, not evidence about the aircraft.
- `CorroboratedAerora` — an Aerora evidence source confirmed Aerora hardware.

The distinction matters because an Aerora build makes plugin selection deterministic by
**exclusion**: it disables the two upstream factories so only Aerora's remains registered. Nothing
about that says what the aircraft is.

Capability reports deliberately do **not** promote the classification. Capability reports say what a
vehicle can do, never whose vehicle it is.

No evidence source for `CorroboratedAerora` exists yet, so nothing in the current build calls
`reportAeroraCorroboration()`. That is correct rather than incomplete: the interface exists, the
claim does not.

## Capability attachment

`AeroraVehicleCapabilities` models the five categories the architecture defines — Vehicle, Payload,
Mission, Telemetry, Command — and nothing else. **No capability identifier is invented.** A key is
an opaque string supplied by whichever evidence source reports it.

The three-state model is the point:

| State | Meaning |
| --- | --- |
| `NotEvidenced` | Nothing has been reported. The default, and never a capability |
| `Unavailable` | A source reported the capability is absent |
| `Available` | A source reported the capability is present |

`isAvailable()` is true only for `Available`, so an unreported capability can never present as a
usable one. `NotEvidenced` is not reportable — passing it is rejected rather than stored, because
storing it would make "reported as unknown" indistinguishable from "never reported".
`clearReports()` returns to `NotEvidenced`, not to `Unavailable`, so a disconnected vehicle never
looks like a vehicle that answered.

## Identity boundary

```text
MAVLink system identity          ≠          Aerora fleet identity
protocol addressing                         product identifier
per connection, not stable,                 format OPEN / NOT-EVIDENCED
not fleet-unique
```

`AeroraVehicleIdentity` holds both and keeps them apart structurally rather than by convention:

- There is no API that derives a fleet identity from a MAVLink identity.
- `reportFleetIdentity()` takes a `QString`, never an integer, so a system id cannot be passed to
  it by accident.
- `fleetIdentity()` is empty while unknown. It never falls back to the system id.
- The reported token is stored verbatim and parsed against no format, because no format is
  evidenced.

Anything that outlives a connection — a log, a cloud record, a configuration binding — must key on
the Aerora identifier. The MAVLink system id is for addressing a connected vehicle and nothing else.

## State boundary

The telemetry workstream owns the REPORTED / EFFECTIVE / REQUESTED / PROPOSED / STALE / UNKNOWN /
UNAVAILABLE model. This layer does not define it, does not store a telemetry value, and performs no
state translation, so it structurally cannot promote `PROPOSED` to `EFFECTIVE` or treat `STALE` as
current. The telemetry engine attaches through `AeroraVehicleServices::attachService()`.

The one anti-collapse guarantee this layer does make is its own: the capability tri-state above,
where "no evidence" is a distinct value and never decays into a boolean.

## Service attachment

`attachService(name, object)` is the single mechanism for every Aerora domain — telemetry, mission,
payload, power, link health and any future one. The service is reparented to the hub, so it is
destroyed with the vehicle and no workstream needs teardown code.

A per-domain accessor on `AeroraVehicleServices` was rejected: it would make the vehicle layer
depend on every domain, which is exactly the coupling this design exists to avoid. Other
workstreams reach their service with:

```cpp
AeroraVehicleServices *const services = AeroraVehicleServices::forVehicle(vehicle);
if (!services) {
    return;
}
```

## Per-vehicle domain service composition

`AeroraFirmwarePlugin::initializeVehicle()` is the only seam in QGroundControl's vehicle lifecycle
that both runs once per vehicle **and** receives the `Vehicle*`. That makes it the composition root
for the Aerora domain modules, and `AeroraFirmwarePlugin::attachDomainServices()` is the
composition.

```text
Vehicle
  └── AeroraVehicleServices                        (child of Vehicle)
        ├── AeroraTelemetrySet      "telemetry"    owns AeroraFreshness
        ├── AeroraPayloadModel      "payload"      freshness + capabilities
        ├── AeroraPowerModel        "power"        freshness + capabilities
        └── AeroraMissionObserver   "mission"      attached by AeroraMissionBridge
              └── AeroraMissionBridge              (child of the observer)

then, for a vehicle only:

  AeroraTelemetrySet::publishTo(AeroraVehicleFactGroupRegistrar(vehicle))
        → Vehicle::_addFactGroup()  → that vehicle's own fact-group map
```

Attachment order is **dependency order, not domain importance**. Telemetry is first because
`AeroraPayloadModel` and `AeroraPowerModel` both take an `AeroraFreshness *` as a constructor
argument, and that evaluator is owned by the telemetry set. Mission is last and is the only one
that needs the vehicle, because the bridge reads a `MissionManager` from it.

| Property | How it is achieved |
| --- | --- |
| One instance per vehicle | Every service is constructed inside `attachDomainServices()` and reparented to that vehicle's own `AeroraVehicleServices`. Nothing is static, cached or process-wide |
| Correct lifetime | `attachService()` reparents, and the services object is a child of the `Vehicle`, so vehicle destruction destroys the whole graph. No module has teardown code |
| No cross-vehicle state | Asserted by `_domainServicesAreComposedPerVehicle`, which composes two services objects and checks that every object **and** both freshness evaluators differ |
| Idempotent | `attachDomainServices()` returns early when the telemetry service is already present, so a repeated initialize cannot produce two models reporting into one vehicle |
| Duplicate names refused | `attachService()` rejects a name already attached. The composition root deletes the refused object rather than leaking it, mirroring `AeroraMissionBridge::attachTo()` |
| Service names not invented | `"mission"`, `"payload"` and `"power"` come from `kServiceName` on the owning type. `"telemetry"` is the exception — see below |

### The one service name this layer had to supply

`AeroraMissionObserver`, `AeroraPayloadModel` and `AeroraPowerModel` each publish
`static constexpr const char *kServiceName`. `AeroraTelemetrySet` publishes none, so the
composition root declares `AeroraFirmwarePlugin::kTelemetryServiceName = "telemetry"` rather than
leaving consumers to guess a literal.

It is a service-registry key, not a protocol identifier. Reported to the orchestrator so
M-Telemetry can adopt it on the type like the other three, after which this constant should follow
it rather than lead it.

### The offline-editing vehicle is excluded

`Vehicle::isOfflineEditingVehicle()` returns true for the disconnected vehicle
`MultiVehicleManager` creates at startup so a plan can be built with no aircraft present. It has no
link, reports nothing, and its firmware class follows a settings value rather than a heartbeat.

Composing payload, power and telemetry onto it would create models that can only ever read
`UNKNOWN`, plus a freshness evaluator and its scheduler, for a vehicle that is not one. Identity
and capabilities stay attached, because plan editing legitimately reads them.

### This module is no longer standalone

The composition references four sibling modules by header and by symbol, so
`AERORA_VEHICLE_SOURCES` no longer links on its own. `Vehicle.cmake` declares the set in
`AERORA_VEHICLE_REQUIRED_MODULES` and the aggregation point must also link the telemetry module's
`AERORA_TELEMETRY_LIBRARIES` (`QGC::Timing`, which carries `RuntimeScheduler.h`).

Composing a vehicle out of modules that are not in the build is a link error. That is the correct
failure: silently attaching three services out of four would be worse than not building.

## FactGroup attachment — Q4-002, and why `factGroups()` was rejected

`FirmwarePlugin::factGroups()` is the only route into a vehicle's fact-group map that needs no core
edit. `FactGroup::_addFactGroup()` is protected, and upstream `Vehicle` grants friendship to
`InitialConnectStateMachine`, `VehicleLinkManager`, `FactGroupListModel`, `GimbalController` and
four unit-test classes — two of them specifically so `_addFactGroup` can be called.

The seam has a defect:

```text
Vehicle A ──┐
            ├──►  shared FirmwarePlugin  ──►  one set of FactGroup objects
Vehicle B ──┘                                         ▲         ▲
                                                      │         │
            A's dispatch loop writes here ────────────┘         │
            B's dispatch loop writes here ──────────────────────┘
```

`FirmwarePluginManager::firmwarePluginForAutopilot()` caches one plugin per firmware class, while
`Vehicle::_commonInit()` inserts whatever `factGroups()` returns into **each** vehicle's own map.
Fact groups held as plugin members are therefore shared, and two vehicles of the same class
overwrite each other's telemetry. `ArduSubFirmwarePlugin` has the same shape upstream.

That is why this seam is not used, and why `AeroraFirmwarePlugin` does not override `factGroups()`.
Choosing the alternative changed the standing no-core-edit position, so the decision belonged to
the orchestrator rather than to this workstream.

### Resolution — per-vehicle registration (ADR-016), now wired

Q4-002 was resolved outside this workstream in favour of a per-vehicle registrar, and the
authorized core edit is one friend declaration at `src/Vehicle/Vehicle.h`:

```cpp
friend class AeroraVehicleFactGroupRegistrar;
```

following the pattern upstream already grants to `FactGroupListModel` and `GimbalController` for
this exact purpose. The privileged call is additionally gated on
`AERORA_VEHICLE_FACTGROUP_FRIENDSHIP`, which the overlay defines in `CUSTOM_DEFINITIONS`. Both
halves are applied, so `AeroraVehicleFactGroupRegistrar::isRegistrationAvailable()` is true in an
Aerora build.

The composition root now invokes publication, which is the gap this module closed:

```text
AeroraFirmwarePlugin::attachDomainServices()
        ↓
AeroraTelemetrySet::publishTo(registrar)          the set already attached as a service
        ↓
AeroraVehicleFactGroupRegistrar                   one per vehicle, stack-local, holds that vehicle
        ↓
Vehicle::_addFactGroup(group, name)
        ↓
that vehicle's own fact-group map
```

The registrar is stack-local because that is its whole required lifetime: the registrar contract
forbids retaining the groups, and `_addFactGroup()` stores the `FactGroup` pointers rather than the
registrar. Group ownership stays with the `AeroraTelemetrySet`, which is a child of the vehicle's
services object. Keeping the registrar alive would mean putting telemetry-specific state on the
generic services hub for diagnostics the set already exposes through `isPublished()`.

Publication runs **last** in the composition, so every module attached before it has had its
chance to register a group, and it runs **inside the `Vehicle` constructor** via
`initializeVehicle()` — ahead of the CSV header, so R-027 holds.

**What it publishes today is nothing.** No Aerora dialect exists, so `AeroraTelemetrySet` declares
no concrete group; publishing an empty set succeeds and seals the set. The wiring is live and
asserted; what flows through it is empty. No Fact name, unit, metadata or message id was invented
to make that look otherwise.

Two states are now distinguished in the tests, which previously could not be told apart:

| State | Assertion |
| --- | --- |
| The root invoked publication | `telemetry->isPublished()` — set only by `publishTo()` succeeding |
| Publication sealed the set | `telemetry->isSealed()` |
| There was nothing to publish | `telemetry->groupNames().isEmpty()` |
| The forbidden seam delivered nothing | `vehicle()->firmwarePlugin()->factGroups() == nullptr` |

An empty vehicle fact-group map is therefore now explained by the empty set, not by missing wiring.

### What the registrar route delivers

Because the groups now reach the vehicle's own map, the three things that map membership carries
come back, with no shared-plugin exposure:

| Capability | Mechanism |
| --- | --- |
| Automatic MAVLink dispatch to each group | `Vehicle::_mavlinkMessageReceived` calls `handleMessage()` on every registered child group (`Vehicle.cc:588-590`) |
| Appearance in the telemetry value picker | `InstrumentValueData::factGroupNames()` enumerates the vehicle's groups |
| Inclusion in the telemetry CSV log | `Vehicle::_initializeCsv` / `_writeCsvLine` enumerate the same groups |

All three are inherited rather than implemented: being on the vehicle rather than beside it is what
buys them. None is exercised yet, because the published set is empty.

One ownership detail that makes the route safe: `FactGroup::_addFactGroup()` stores the raw pointer
and **does not reparent**, so registration transfers no ownership. The groups stay owned by the
`AeroraTelemetrySet`, which is a child of the vehicle's services object, which is a child of the
`Vehicle`. One chain, one lifetime, no teardown code.

### The alternative that was rejected

`FirmwarePlugin::factGroups()` has exactly **one** caller in the whole tree — `Vehicle.cc:365`,
inside `_commonInit`, called synchronously once per vehicle and before `initializeVehicle()`. That
made a per-call handover mechanically possible: `factGroups()` could build a fresh set and
`initializeVehicle()` could adopt it, with no core edit at all.

It was rejected, and the rejection still holds. It makes a seam whose header documents a
*singleton* return carry hidden per-call state, and it depends on an undocumented ordering inside
core. If that ordering ever changed, the failure would be a leak plus fact groups outliving their
owner — silent, and worse than the problem it solves. One friend declaration is the cheaper and
more honest price.

## `Vehicle::startCalibration()` bypasses the authorization gate

A Vehicle-side finding, reported rather than fixed: the fix options all require either a QGC core
edit or security policy, and neither is this workstream's to make.

### The two paths, source-verified at `9459a4b5`

```text
stopCalibration(showError)                    startCalibration(calType)
      │                                             │
      ▼                                             │  builds COMMAND_LONG in place
sendMavCommand()                 Vehicle.cc:2409    │  mavlink_msg_command_long_pack_chan
      ▼                                             │                    Vehicle.cc:2396
MavCommandQueue::sendCommand()                      │
      ▼                                             │
MavCommandQueue::sendWorker()                       │
      ▼                                             │
QGCCorePlugin::authorizeMavCommand()  ◄── GATE      │   ✗ never reached
                              MavCommandQueue.cc:294│
      ▼                                             │
_list.append()                MavCommandQueue.cc:381│
      ▼                                             ▼
Vehicle::sendMessageOnLinkThreadSafe()  ◄────────────┘           Vehicle.cc:2405
      ▼
FirmwarePlugin::adjustOutgoingMavlinkMessageThreadSafe()         Vehicle.cc:1400
      ▼
LinkInterface::sendMessageThreadSafe()  — signs, serializes   LinkInterface.cc:150-162
      ▼
wire
```

Both commands are `MAV_CMD_PREFLIGHT_CALIBRATION`. `stopCalibration()` is authorized;
`startCalibration()` is not. The two paths converge only *below* the gate.

What the direct path does **not** skip: firmware-plugin outgoing adjustment and MAVLink signing
both still happen, because `LinkInterface::sendMessageThreadSafe()` is the single message-level
chokepoint and its own header requires all sends to route through it. The bypass is specific to the
`MavCommandQueue` layer — authorization, deduplication, `MAV_COMP_ID_ALL` rejection, ack
bookkeeping and `commandResult` reporting.

### Scope of the bypass class

Grepping `src/**` for `mavlink_msg_command_(long|int)_(pack|encode)` outside `src/Comms/MockLink/`
returns three hits: `Vehicle.cc:2396` (this one) and the queue's own two encoders at
`MavCommandQueue.cc:449` and `:469`.

**`Vehicle::startCalibration()` is the only production code path in the tree that sends a `MAV_CMD`
outside `MavCommandQueue`.** Around twenty other direct senders exist in `src/Vehicle/**`, but all
send non-command messages (SET_MODE, MANUAL_CONTROL, RC_CHANNELS_OVERRIDE, PARAM_MAP_RC, FTP,
OPEN_DRONE_ID, …). Notably `Vehicle::motorTest()` — which does spin motors — uses
`sendMavCommand(MAV_CMD_DO_MOTOR_TEST, …)` and **is** gated.

### Why upstream sends directly, and what the source actually supports

The comment at `Vehicle.cc:2393-2394` is the only stated reason:

> We can't use sendMavCommand here since we have no idea how long it will be before the command
> returns a result. This in turn causes the retry logic to break down.

Re-deriving that against the queue as it now stands gives a more precise picture, and the stated
reason is **not** mechanically accurate:

| Claim | What the source shows |
| --- | --- |
| Queuing would cause retries | **No.** `MavCommandQueue::_shouldRetry()` (`:213-243`) lists only `REQUEST_AUTOPILOT_CAPABILITIES`, `REQUEST_MESSAGE`, `PREFLIGHT_STORAGE` and `RUN_PREARM_CHECKS`. `PREFLIGHT_CALIBRATION` is absent, so a queued entry gets `maxTries = 1` and is never retransmitted |
| So queuing is harmless | **Also no.** At the first ack timeout (`_ackTimeoutMSecsDefault = 1200` ms) `_sendFromList` takes the give-up branch, emits `MavCmdResultFailureNoResponseToCommand` and, with `showError`, shows "Vehicle did not respond to command". A calibration that is running normally would report as a failure |
| Any other queue side effect? | **Yes, and it is the stronger argument.** `_canBeDuplicated(PREFLIGHT_CALIBRATION)` is false, so while a queued start is pending, `stopCalibration()` — the same `MAV_CMD` — is refused as `MavCmdResultFailureDuplicateCommand`. Queuing start can therefore block cancel |

So the direct send is defensible, but for reasons the comment does not give: spurious failure
reporting and cancel-blocking, not retransmission.

### Progress and completion are not acked

Nothing records the direct-sent command, so `MavCommandQueue::handleCommandAck()` drops any
`PREFLIGHT_CALIBRATION` ack at `:533-537` ("Ack not in list"), including `MAV_RESULT_IN_PROGRESS`.
Completion is derived from STATUSTEXT parsing in the autopilot controllers
(`SensorsComponentController::_handleUASTextMessage`, `PowerComponentController::_handleVehicleTextMessage`),
and cancellation is best-effort by design — `SensorsComponentController.cc:396` records that "the
firmware doesn't allow us to cancel calibration".

### Actuator relevance

Reported literally, without interpreting MAVLink `PREFLIGHT_CALIBRATION` semantics:

| Calibration type | Params set | Source signal about actuators |
| --- | --- | --- |
| `CalibrationEsc` | `param7 = 1` | Reached from `PowerComponentController::calibrateEsc()`. ESC calibration exists as a type |
| `CalibrationAPMCompassMot` | `param6 = 1` | Reached from `APMSensorsComponentController::calibrateMotorInterference()`, whose own operator text reads "Raise the throttle slowly to between 50% ~ 75% (**the props will spin!**)" (`:346`) |
| `CalibrationRadio` | `param4 = 1` | Call-site comment: "Let the mav known we are starting calibration. This should turn off motors and so forth" (`RadioComponentController.cc:41-42`) |
| `CalibrationNone` / unlisted | all zero | Byte-identical payload to what `stopCalibration()` sends, but emitted on the **unauthorized** path |

That last row is worth naming on its own: the same wire bytes are authorized or not depending only
on which function was called.

### Where authorization could be inserted

| Insertion point | Core edit? | Can it veto? |
| --- | --- | --- |
| `Vehicle::startCalibration()`, before the `pack_chan` call | **Yes** — `src/Vehicle/Vehicle.cc` | Yes |
| `Vehicle::sendMessageOnLinkThreadSafe()` | **Yes** — and it would gate ~20 non-command senders too | Yes |
| `LinkInterface::sendMessageThreadSafe()` | **Yes** — and it is on the link thread | Yes |
| `AeroraFirmwarePlugin::adjustOutgoingMavlinkMessageThreadSafe()` | **No** — overlay-owned | **No.** Returns `void`; it can observe or mutate but cannot refuse. It is also called for every outgoing message on a thread-safety-sensitive path |
| The calling controllers in `src/AutoPilotPlugins/**` | **Yes**, several files | Yes, but one gate per caller — the shape that produces a missed one |

The only zero-core-edit seam that sees the traffic is `adjustOutgoingMavlinkMessageThreadSafe()`,
and it **cannot refuse**. A detector there would be defence-in-depth at best and a false sense of
enforcement at worst, so none was added.

### Options, with consequences

No option is selected here. Selecting one requires either authorizing a core edit or setting
security policy.

| Option | Core edit | Retry/operator effect | Notes |
| --- | --- | --- | --- |
| **A.** Authorization check immediately before direct transmission | 1 site, `Vehicle.cc` | None — the direct path is unchanged apart from the check | Smallest change that actually enforces. Needs a decision hook callable from `Vehicle`; `QGCCorePlugin::authorizeMavCommand()` already has the right signature and is documented as decision-only |
| **B.** Vehicle-specific authorization seam for direct sends, consumed by security | 1 site + a new virtual | None | Generalises to the other direct senders later. More surface than A for the same immediate result |
| **C.** Route calibration through the queue with retry suppressed | `_shouldRetry` / dedup changes | Introduces the spurious "did not respond" report and the cancel-blocking described above unless both are also addressed | Gets authorization for free but changes operator-visible behaviour. Not free, despite looking like it |
| **D.** Record calibration as an explicit, documented exception to the command-authorization scope | None | None | Honest and cheap, but leaves a motor-relevant command unauthorized. Only acceptable as a deliberate, recorded risk acceptance |
| **E.** Gate at `sendMessageOnLinkThreadSafe()` | 1 site, but widest blast radius | None for calibration; changes the contract for ~20 other senders | Closes the whole direct-send class at once. Needs a policy for non-command messages, which does not exist |

**Recommendation from the Vehicle side, as technical input only:** A is the smallest change that
enforces, keeps the upstream direct-send rationale intact, and adds no operator-visible behaviour.
It requires one authorized core edit in `src/Vehicle/Vehicle.cc` and a policy decision about
calibration from the security workstream. **Owner: Tab 1 for the core edit, Tab 2 for the policy.**

### Outcome — Option A was authorized and implemented

The decision was taken outside this workstream and the core edit now exists in
`src/Vehicle/Vehicle.cc`, placed after the parameter switch and before the `pack_chan`:

```cpp
if (!QGCCorePlugin::instance()->authorizeMavCommand(this, defaultComponentId(),
                                                    MAV_CMD_PREFLIGHT_CALIBRATION, false,
                                                    param1, param2, param3, param4,
                                                    param5, param6, param7)) {
    qCWarning(VehicleLog) << "startCalibration not authorized, calType:" << calType;
    QGC::showAppMessage(tr("Calibration not authorized."));
    return;
}
```

Reviewed from the Vehicle side and assessed as sound:

- **Direct-send semantics preserved.** No reroute through `sendMavCommand()`, so neither the
  spurious "did not respond" report nor the cancel-blocking described above is introduced.
- **Placed after the switch, which is the only correct position.** The calibration type lives
  entirely in `param1`–`param7`, so a decision taken before the switch could not distinguish an
  ESC calibration (`param7`) or CompassMot (`param6`) from a gyro calibration.
- **Refuses before the pack**, so an unauthorized calibration is never encoded or transmitted.
- **Reuses the existing decision hook** rather than adding a second authorization surface. The
  `float` → `double` widening on `param5`/`param6` is exact and harmless.
- **Closes the all-zero gap.** `CalibrationNone` falls through the switch to all-zero parameters —
  the same payload `stopCalibration()` sends — and is now gated on both paths, so identical wire
  bytes are no longer authorized or not depending only on which function was called.

### Residual observation — audit asymmetry when no link is present

Not a security gap, and not fixed here. `startCalibration()` locks the primary link and returns at
`Vehicle.cc:2330-2334`, **before** the gate. `MavCommandQueue::sendWorker()` is the other way
round: the gate is at `:294` and its link check at `:331`.

So an operator attempting a calibration on a vehicle whose link has gone is refused without the
authorization hook ever being called, while the same attempt on any queued command does reach the
hook. Nothing can be transmitted either way, so enforcement is unaffected — but if the audit record
is written inside the hook, this one path records no attempt where every other path records one.

Owner: M-Security for whether the audit record should cover unsendable attempts, M-Plugin for the
core file if it should. Reported, not changed.

## Vehicle setup pages and tool indicators (U4, U5)

Two `FirmwarePlugin` virtuals, both previously unoverridden:

```text
Vehicle ctor ──► FirmwarePlugin::autopilotPlugin(this)      FirmwarePlugin.h:122
                      ↓
                 AeroraAutoPilotPlugin                      the setup page index, SCR-VEH-01
                      ↓  vehicleComponents()
                 AeroraVehicleComponent × n   +   JoystickComponent

Toolbar ──────► FirmwarePlugin::toolIndicators(vehicle)     FirmwarePlugin.h:344
                      ↓
                 base list  +  AeroraToolIndicators::urlsFor(capabilities)
```

### Three corrections to the Phase 7 plan, from reading the source

| Plan statement | What the source says |
| --- | --- |
| `vehicleConfigJson()` is an `AutoPilotPlugin`-level path (plan docs 03 and 07) | It is a virtual on **`VehicleComponent`** (`VehicleComponent.h:52`). The section engine is in `VehicleComponent.cc` |
| Subclass `AutoPilotPlugin` | Forced, not a choice. `GenericAutoPilotPlugin` declares `vehicleComponents()` **`final`** (`GenericAutoPilotPlugin.h:18`), so it cannot be extended |
| "`toolIndicators` — ~10 lines" | ~10 lines only if the list is appended to the inherited `_toolIndicatorList`, which is wrong. `FirmwarePluginManager` caches one plugin per firmware class, so a capability-dependent list cached there is computed for whichever vehicle asks first and then served to all of them |

The third is the same shared-singleton hazard that keeps `factGroups()` unoverridden in this module,
reached by a different route. The list is therefore recomputed per call into this plugin's own
member, never into the inherited one.

### A page publishes only when its QML exists

A descriptor names resources; it does not create them. The Aerora setup QML is M-Ui's, and until it
exists a published page would load a panel that renders nothing —
`VehicleConfigView.qml:209` calls `panelLoader.setSource(vehicleComponent.setupSource, ...)` with no
existence check of its own, and `FlyViewToolBarIndicators.qml` binds `visible: item.showIndicator`
on the loaded item with no null guard.

So both seams gate on the resource being present, and the honest result today is **no Aerora setup
page and no Aerora indicator**. This is the declared-intent-plus-existence shape
`AeroraQmlOverrides::verify()` already established in this overlay: the catalogue is the declaration
of intent, and `unresolvedPageResources()` names what is declared but absent, so an absent page is
reportable rather than silent. A page appears with no C++ change once its QML lands.

The resource root is `:/qml/Aerora/Controls`, which is not a guess: it follows
`AERORA_UI_QML_MODULE_URI` and `RESOURCE_PREFIX /qml` from `aerora/src/UI/Ui.cmake:133`, and the
alias that flattens it to that path landed in `a73699796`. A stale path fails closed — the page is
withheld — rather than publishing a panel that cannot load.

### The six `SCR-VEH-*` integration points

| Screen | Treatment |
| --- | --- |
| `SCR-VEH-01` page index | `AeroraAutoPilotPlugin::vehicleComponents()`. The list itself, not a page |
| `SCR-VEH-02` identity and profile | Declared. Backed by `AeroraVehicleIdentity` and the resolved profile |
| `SCR-VEH-03` capability report | Declared. Backed by `AeroraVehicleCapabilities`' three-state model |
| `SCR-VEH-04` sensor and health | Declared, with the `vehicleConfigJson()` seam implemented and empty |
| `SCR-VEH-05` calibration | **WITHHELD.** See below |
| `SCR-VEH-06` service diagnostics | Declared. Backed by `AeroraVehicleServices::serviceNames()` |

Calibration is the one `SCR-VEH-*` screen that acts on the aircraft, and the ADR-015 gate in front
of it permits everything while the operator role set is undecided (`Q-029`). An Aerora calibration
page would present a gated operation whose gate evaluates nothing, so none is published.
`AeroraVehicleComponent::withheldPageReason()` carries the reason in source and
`AeroraVehicleSetupTest` asserts the absence, so the omission is a decision with a test behind it
rather than a gap in the numbering.

Upstream calibration pages are unaffected and remain reachable through their own firmware plugins.

### Every published page is read-only

`requiresSetup()` returns false and `setupComplete()` returns true for every Aerora page. Returning
true from `requiresSetup()` would badge a page that has no action on it and would make
`AutoPilotPlugin::parametersReadyPreChecks()` raise "Configuration tasks remain before this vehicle
is ready to fly" for a vehicle with no outstanding task. `setupCompleteChangedTriggerList()` is
empty because a trigger list is parameter names and no Aerora parameter is evidenced.

The joystick page is reproduced from `GenericAutoPilotPlugin` rather than dropped. It is the entire
content of the plugin this one displaces, and `JoystickComponent` is firmware neutral
(`AutoPilotPlugins/Common`), so omitting it would remove joystick setup from every Aerora vehicle
with no error attached.

### Indicator membership is not indicator visibility

`showIndicator` inside a component answers "is there something to show right now" from live state.
`AeroraToolIndicators::isPublishable()` answers a different question: "does this vehicle have the
capability this indicator reports on at all". The gate requires
`AeroraVehicleCapabilities::isAvailable()`, which is true only for `State::Available` — so
`NotEvidenced` and `Unavailable` both withhold the indicator, and a null inventory withholds it too.
An attached-but-silent capabilities object is not evidence, and the absence of one is not
permission.

This is the sanctioned place for a capability-driven indicator: `toolIndicators()` is the one
indicator registry with a `toolIndicatorsChanged()` signal, where `QGCCorePlugin::toolBarIndicators`
is `CONSTANT` and never re-read after first bind.

Nothing in QGroundControl's `src/` emits `toolIndicatorsChanged()` — `Vehicle` only relays it
(`Vehicle.cc:231`) — so this module emits it on a capability report, otherwise the answer to the
first query would become the permanent one. That connection is not installed while the declared set
is empty, because there is no membership it could change.

### What the declared sets contain, and why

| Set | Contents | Why |
| --- | --- | --- |
| Page catalogue | 4 descriptors, 0 publishable | The QML is M-Ui's and does not exist |
| Indicator set | **empty** | Two prerequisites absent: no Aerora toolbar QML, and no capability identifier to gate on (`Q3-009`). An entry written now would invent a capability key |

### One limit on the profile-capability claim

`P7-FLEET-002` — "a profile never grants a capability" — is asserted of the two pieces that can
enforce it: the registry, whose `resolve()` takes a `const AeroraVehicleCapabilities &`, and this
layer, which reports nothing on a profile's behalf. It is **not** structurally enforced against a
profile implementation: `AeroraProductProfile::attachToVehicle()` receives a mutable
`AeroraVehicleServices &`, whose `capabilities()` hands out a non-const pointer with a public
`report()`. A profile that called it would succeed.

Recorded rather than tested, because a test asserting it would enshrine it. Closing it means
narrowing either `capabilities()` or `report()`, which touches the evidence-source contract and is
not this change's scope.

## Product-family attachment

```text
Vehicle
   ↓
AeroraVehicleServices
   ↓
AeroraProductProfile        resolved from the reported capability inventory
```

A profile declares its own id, decides whether it matches a capability inventory, and attaches its
own services. The vehicle layer knows only the interface, so a new product family is a new file
rather than a change to shared code. There is deliberately **no enumeration of families** anywhere
in this subtree — adding one would reintroduce the conditional the design exists to avoid.

Resolution refuses ambiguity: two matching profiles resolve to nothing and log a warning, rather
than letting registration order pick a winner. Registration order is link order, and a defect that
depends on link order is a defect that reproduces on one machine out of five.

Profiles register explicitly rather than by static self-registration. The firmware plugin factory
has no choice about that, but a profile set assembled by static-initialisation order would be
unordered and unreviewable.

## Boundaries this layer does not cross

| Boundary | What this layer does |
| --- | --- |
| **Flight authority** | Nothing. It issues no command, requests no mode change, and arms nothing. The flight controller keeps local authority |
| **Failsafe** | Nothing. No RTL, LAND, HOLD, DISARM or termination is initiated or inferred. Reported failsafe state may be observed, displayed and logged by other workstreams |
| **Companion / GPU** | Nothing. The companion computer proposes and the flight controller decides. No proposal format, autonomy state enum, acceptance threshold or GPU message id is defined here |
| **Authority modes** | Nothing. The evidenced sentinels (manual 100, autonomous coverage 101, semi-auto supervised 102) are not reinterpreted, extended or embedded in this subtree. Authority state may be exposed through a service; the decision is not this layer's |
| **Command authorization** | Nothing. Owned by the security workstream through `QGCCorePlugin::authorizeMavCommand()` |
| **Radio** | Nothing. No transport, no USB VID/PID, no enumeration assumption. Radio remains deferred |
| **Telemetry state engine** | Nothing beyond an attachment point |
| **Payload, power, mission** | Nothing beyond an attachment point |
| **Cloud** | Nothing. Cloud is never on the command path |

## Overlay-root contract

This subtree is **not** a complete overlay. `Vehicle.cmake` creates no target and sets no
`CUSTOM_*` variable, because `aerora/CMakeLists.txt` is shared infrastructure owned by M-Plugin and
also written to by the security and telemetry workstreams.

`aerora/CMakeLists.txt` and `aerora/cmake/CustomOverrides.cmake` now exist, so the overlay is
usable with `-DQGC_CUSTOM_DIR=aerora`. **This module is not yet wired into it.** The aggregation
point needs these lines added:

```cmake
include(${CMAKE_CURRENT_SOURCE_DIR}/src/Vehicle/Vehicle.cmake)

list(APPEND CUSTOM_SOURCES ${AERORA_VEHICLE_SOURCES})
list(APPEND CUSTOM_INCLUDE_DIRECTORIES ${AERORA_VEHICLE_INCLUDE_DIRECTORIES})

if(QGC_BUILD_TESTING)
    list(APPEND CUSTOM_SOURCES ${AERORA_VEHICLE_TEST_SOURCES})
    list(APPEND CUSTOM_INCLUDE_DIRECTORIES ${AERORA_VEHICLE_TEST_INCLUDE_DIRECTORIES})
    include(QGCTest)
    add_qgc_test(AeroraVehicleIntegrationTest LABELS Unit)
    add_qgc_test(AeroraVehicleLifecycleTest LABELS Integration Vehicle)
endif()
```

The `list(APPEND ...)` calls must precede the existing `set(CUSTOM_SOURCES ... PARENT_SCOPE)`, or
that `set` must be repeated afterwards: `list(APPEND)` does not propagate to the parent scope.

`CUSTOM_DEFINITIONS` already supplies `QGC_CUSTOM_BUILD`, `CUSTOMHEADER="AeroraCorePlugin.h"` and
`CUSTOMCLASS=AeroraCorePlugin`, which is what routes `QGCCorePlugin::instance()` through the
overlay. This module needs nothing from that and adds nothing to it.

### Single-stack configuration is a product decision, not a safety mitigation

`aerora/cmake/CustomOverrides.cmake` deliberately leaves `QGC_DISABLE_PX4_PLUGIN_FACTORY` and
`QGC_DISABLE_APM_PLUGIN_FACTORY` unset, on the grounds that "routing every unknown autopilot into
Aerora via the generic plugin is explicitly out of bounds" (Q4-004).

**That decision is compatible with this implementation, and the stated hazard does not occur.**
Source, read at `9459a4b5`:

- `APMFirmwarePluginFactory::supportedFirmwareClasses()` returns exactly
  `{FirmwareClassArduPilot}`, and its `firmwarePluginForAutopilot()` returns nullptr unless
  `autopilotType == MAV_AUTOPILOT_ARDUPILOTMEGA`.
- `PX4FirmwarePluginFactory::supportedFirmwareClasses()` returns exactly `{FirmwareClassPX4}`, with
  the same guard for `MAV_AUTOPILOT_PX4`.
- `AeroraFirmwarePluginFactory::supportedFirmwareClasses()` returns exactly
  `{FirmwareClassGeneric}`, which is `MAV_AUTOPILOT_GENERIC`.
- `_findPluginFactory()` compares the **raw** `MAV_AUTOPILOT` value against those lists.

The three claimed classes are distinct enumerators of the same MAVLink enum and so are pairwise
distinct values. Two consequences:

1. **No factory claims a class another factory claims, so there is no first-match race** — with or
   without the disable options. Mitigation by exclusion was never needed for conflict avoidance.
2. **An unknown autopilot is not routed into Aerora.** A vehicle reporting an autopilot that is
   none of the three matches no factory and falls through to the lazily created base
   `FirmwarePlugin`. Only a vehicle reporting exactly `MAV_AUTOPILOT_GENERIC` reaches the Aerora
   plugin.

What the options actually change is `FirmwarePluginManager::supportedFirmwareClasses()`, which
prunes the firmware choices offered in the setup UI. That is a single-stack product decision for
the product and UI workstreams, and nothing in this module depends on it either way.

Residual exposure, stated plainly: a non-Aerora vehicle that reports `MAV_AUTOPILOT_GENERIC` will
be handed the Aerora plugin. That is why `AeroraFirmwarePlugin` claims no capability, populates no
flight mode, and records `SelectedBySeam` rather than `CorroboratedAerora`. Such a vehicle gets a
working generic ground station, which is the intended floor.

### A separate consequence, if the options are ever set

Disabling the upstream factories would change the behaviour of the **existing QGC test suite**, not
only of the product. `VehicleTest` defaults to `MAV_AUTOPILOT_PX4`, and `UnitTest` has an
`apmFirmwareSupported()` guard that lets ArduPilot tests `QSKIP` in a single-firmware build but has
no PX4 equivalent. PX4-flavoured vehicle tests would still connect, resolve to the generic fallback
plugin, and fail their PX4-specific assertions.

That is a suite-wide question rather than a vehicle-integration one, and it is reported rather than
worked around here. It is also a concrete cost to weigh in Q4-004.

## Build and test

Once the overlay root exists:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DQGC_BUILD_TESTING=ON -DQGC_CUSTOM_DIR=aerora
cmake --build build
ctest --test-dir build --output-on-failure -R AeroraVehicle
```

All four tests also self-register through `UT_REGISTER_TEST`, so they are reachable without CTest:

```bash
./QGroundControl --unittest:AeroraVehicleIntegrationTest
./QGroundControl --unittest:AeroraVehicleSetupTest
./QGroundControl --unittest:AeroraProfileRegistryTest
./QGroundControl --unittest:AeroraVehicleLifecycleTest
```

`add_qgc_test()` comes from `cmake/QGCTest.cmake`, which `test/CMakeLists.txt` includes, and the
overlay is processed before `test/`. The aggregation point therefore has to either `include(QGCTest)`
itself — the approach the telemetry manifest documents — or defer the calls with
`cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL add_qgc_test ...)`, which runs after
`test/` and needs no include. `Vehicle.cmake` records both; the choice is M-Plugin's, and
consistency with the telemetry manifest is the better reason to pick one. The only difference is
that `include(QGCTest)` from the overlay moves upstream's `check` / `check-unit` convenience targets
out of `test/`, which is cosmetic because custom targets are global by name.

### Lint scope

These files live under `aerora/`, not `test/`, so the pre-commit hooks that exclude `^test/` —
`clang-tidy`, `clazy`, `check-no-qassert`, `vehicle-null-check` — apply to the tests here as well
as to the production code. That is stricter than the core test tree and is intended.

## What was actually verified

| Check | Result |
| --- | --- |
| `python tools/analyzers/vehicle_null_check.py aerora/` | **PASS**, exit 0, no violations |
| Line endings, trailing whitespace, final newline, tabs, 120-column limit | **PASS** |
| Banned patterns: `Q_ASSERT`, `QTest::ignoreMessage`, fixed `qWait`, uncategorized logging | **PASS**, none present |
| Every `#include "..."` resolves to a real header in `src/`, `test/` or `aerora/` | **PASS** |
| `Vehicle.cmake` source list matches the files on disk, both directions | **PASS** |
| `UT_REGISTER_TEST` names match `AERORA_VEHICLE_TEST_NAMES` | **PASS** |
| Brace, paren and bracket balance per file | **PASS** |
| Every `Class::method` defined in a `.cc` is declared in its `.h` | **PASS** |
| Every cross-module symbol the composition root references is declared in the owning module's header (26 checks) | **PASS** |
| Compile | **NOT RUN** — no compiler on this host |
| Link | **NOT RUN** |
| Test execution | **NOT RUN** |
| `clang-format`, `clang-tidy`, `clazy`, `cmake-format`, `cmake-lint` | **NOT RUN** — not installed |

Re-run for the U4 / U5 change, on the files it touches:

| Check | Result |
| --- | --- |
| `.kiro/tools/aerora-manifest-check.ps1` | **PASS**, 0 problems across all 6 checks, 127 sources, 0 undeclared, every `_TEST_NAMES` entry registered |
| `python tools/analyzers/vehicle_null_check.py` on all 10 changed production files | **PASS**, exit 0 |
| `python tools/analyzers/qt_translate_noop_check.py` on all 16 changed files | **PASS**, exit 0 |
| `check-no-qassert`, `check-no-qtest-ignore-message`, `check-no-fixed-qwait`, `check-categorized-logging`, no `qCInfo`, no tabs | **PASS**, hook patterns replicated locally |
| 120-column limit, LF endings, no trailing whitespace, final newline | **PASS** on all 16 files |
| `python tools/check_clang_format.py` | **NOT RUN** — fails with `WinError 2`; the `clang-format` binary is absent |
| `.kiro/tools/aerora-preflight.ps1` | **NO-GO**, 8 blocking failures: no CMake, no Ninja, no MSVC, no Qt, no GStreamer, 2.02 GB free of 40 GB required, no elevation |
| Compile, link, test execution | **NOT RUN** — see above |

The text-level checks are not a compile and are not claimed to be one. **Every test in this subtree
is written and unexecuted.** The honest status is `IMPLEMENTED-UNCOMPILED`.

## Corrections to earlier phases

### Plugin selection is narrower than recorded

`.kiro/specs/01-aerora-architecture/vehicle-integration.md` states that in a multi-stack build
"any unrecognised autopilot also resolves to `FirmwareClassGeneric` and would be handed the Aerora
plugin", and treats that as the substance of Q-017.

Source says otherwise. `FirmwarePluginManager::firmwarePluginForAutopilot()` passes the **raw**
`MAV_AUTOPILOT` value into `_findPluginFactory(FirmwareClass_t)`, which tests
`supportedFirmwareClasses().contains(firmwareClass)`. It does not normalise through
`QGCMAVLink::firmwareClass()` first, and `FirmwareClass_t` is `typedef int`
(`src/MAVLink/QGCMAVLinkTypes.h`).

Since `FirmwareClassGeneric` is `MAV_AUTOPILOT_GENERIC`, value 0, a factory claiming it is selected
**only** for a vehicle reporting exactly `MAV_AUTOPILOT_GENERIC`. Any other unrecognised autopilot
value matches no factory at all and falls through to the lazily created base `FirmwarePlugin`.

The mixed-fleet hazard is therefore narrower than recorded, and narrower in the safe direction.

The consequence for the recorded mitigation is larger than a detail. Because the PX4, ArduPilot and
Aerora factories claim three pairwise-distinct values, **no first-match race exists to mitigate**,
so disabling the upstream factories is not a selection-safety measure. It is a single-stack product
and UI decision. See
[Single-stack configuration is a product decision](#single-stack-configuration-is-a-product-decision-not-a-safety-mitigation).

### Deviations from the Phase 4 firmware-plugin plan

`.kiro/specs/03-implementation-specification/07-firmware-plugin-implementation.md` plans overrides
that this implementation deliberately omits. Each omission is a reduction in code, not in behaviour.

| Planned | Implemented | Reason |
| --- | --- | --- |
| Override the flight-mode accessors to return empty | Not overridden | Deriving from `FirmwarePlugin` already returns empty. An override restating the base default is dead code. `_firmwarePluginKeepsBaseClassSafeDefaults` asserts the property instead, so it is checked rather than intended |
| Override `adjustIncomingMavlinkMessage` to never veto | Not overridden | Same reason; the base returns true. Asserted by the same test |
| Override `isCapable` | Not overridden | Same reason; the base returns false for every capability. Asserted by the same test |
| Narrow `supportedVehicleClasses()` to multirotor | Not overridden | The GCS must serve four product families whose airframe classes are not evidenced. Narrowing would encode a guess about future hardware into the seam that decides whether a vehicle is usable at all |
| `AeroraAutoPilotPlugin` returning Aerora setup pages | Not implemented | `GenericAutoPilotPlugin`, the inherited default, already returns an empty component list, which is what an Aerora plugin with no evidenced setup page would return. Setup views are the UI workstream's surface |

## Unresolved questions

Only genuine blockers. Nothing here is answerable from this repository.

| Question | Subject | Blocks |
| --- | --- | --- |
| **`startCalibration()` audit on an absent link** | The gate sits after the link check, so an unsendable attempt is not offered to the hook. Enforcement unaffected | Nothing. An audit-completeness decision for M-Security |
| **Concrete Aerora fact groups** | Q4-002 is resolved and publication is wired, but the set is empty: no dialect exists, so no group, Fact name, unit or metadata does either | Anything Aerora-specific appearing in the value picker, the CSV record or MAVLink dispatch. Owned by M-Telemetry, gated on the dialect |
| **Telemetry service name** | `AeroraTelemetrySet` publishes no `kServiceName`; the composition root supplies `"telemetry"` | Nothing. One constant, to be adopted on the type by M-Telemetry |
| ~~Overlay aggregation~~ | **Closed.** `aerora/CMakeLists.txt` includes every module manifest, this one among them, and registers this module's four tests | — |
| **Q-018** | The `MAV_AUTOPILOT` value Aerora firmware reports | Nothing structural. `AeroraFirmwareClaim::kFirmwareClass` is one line |
| **Q-007** | Aerora flight-mode and authority-mode mapping | Every named flight-mode accessor. Empty is the safe state meanwhile |
| Aerora fleet identity format | Serial, UUID, registration number — none evidenced | Nothing structural. The token is opaque |
| Aerora capability identifiers | No capability key is evidenced | Nothing structural. Keys are opaque strings |
| **Q4-004** | Whether to disable the upstream factories | Nothing in this module. Evidence that narrows it is above: there is no selection race, so this is a product and UI decision with a known test-suite cost |
| Aerora setup-page and indicator QML | Four page panels and any toolbar component. Owned by M-Ui | Only the *display* of `SCR-VEH-02`..`04`, `06` and `SCR-SHL-02`. The C++ seams are in place and publish nothing until the QML exists |
| Build host | 2.02 GB free of 40 GB required, no toolchain, no elevation (`D-006`) | Every compile, link and test claim in the programme |

## Status vocabulary

`CONTRACT-ONLY`, `IMPLEMENTATION-PLANNED`, `IMPLEMENTED`, `IMPLEMENTED-UNCOMPILED`, `VERIFIED`,
`BLOCKED`, `OPEN`, `NOT-EVIDENCED`.

`VERIFIED` is used in this document only for facts read directly from QGC source at `9459a4b5`. It
is not used for anything about this subtree's behaviour, because nothing here has run.

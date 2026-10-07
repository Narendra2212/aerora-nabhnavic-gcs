# Vehicle Architecture

The vehicle model, the Fact system, firmware abstraction, and communication-loss handling.

## Scale

| File | Lines |
| --- | --- |
| `src/Vehicle/Vehicle.cc` | 3573 |
| `src/Vehicle/Vehicle.h` | 1327 |
| `src/FactSystem/ParameterManager.cc` | 1947 |
| `src/FactSystem/FactMetaData.cc` | 1664 |
| `src/FirmwarePlugin/APM/APMFirmwarePlugin.cc` | 1431 |
| `src/FactSystem/Fact.cc` | 945 |
| `src/FirmwarePlugin/PX4/PX4FirmwarePlugin.cc` | 874 |
| `src/FirmwarePlugin/FirmwarePlugin.cc` | 664 |
| `src/FirmwarePlugin/FirmwarePlugin.h` | 477 |
| `src/Vehicle/VehicleLinkManager.cc` | 416 |
| `src/Vehicle/MultiVehicleManager.cc` | 371 |

`Vehicle` is a god-object, and the class declaration itself is unusual:

```cpp
class Vehicle : public VehicleFactGroup, public VehicleTypes   // Vehicle.h:82
```

The `Vehicle` **is** a FactGroup. This is why `_addFactGroup(_vehicleFactGroup, ...)` is commented
out at `Vehicle.cc:344` and `vehicleFactGroup()` returns `this` (`Vehicle.h:550`).

## Lifecycle

### Creation

Only `MultiVehicleManager` constructs connected vehicles. `init()`
(`MultiVehicleManager.cc:50`) connects `MAVLinkProtocol::vehicleHeartbeatInfo` to
`_vehicleHeartbeatInfo` at line 58. That slot (`MultiVehicleManager.cc:68`) is the sole
construction site, at **line 115**:

```cpp
Vehicle *const vehicle = new Vehicle(link, vehicleId, componentId,
                                     (MAV_AUTOPILOT)vehicleFirmwareType,
                                     (MAV_TYPE)vehicleType, this);
```

Admission filters applied before construction, in order:

| Filter | Line | Behaviour |
| --- | --- | --- |
| `componentId != MAV_COMP_ID_AUTOPILOT1` | 70 | Ignored — non-autopilot components never become vehicles |
| `MAV_TYPE_GENERIC` + `MAV_AUTOPILOT_ARDUPILOTMEGA` | 83 | Rewritten to `MAV_TYPE_QUADROTOR` (freshly flashed ArduCopter sets no `FRAME_CLASS`) |
| `MAV_TYPE_GCS`, `ONBOARD_CONTROLLER`, `GIMBAL`, `ADSB` | 87-91 | Rejected — not vehicles |
| Second vehicle while `multiVehicleEnabled()` is false | 97 | Rejected |
| `_ignoreVehicleIds.contains(id)`, existing id, or `id == 0` | 101 | Rejected |
| System id collides with the GCS system id | 111-113 | User warning, but creation proceeds |

Immediately after construction (lines 116-131): connect
`VehicleLinkManager::allLinksRemoved` to `_deleteVehiclePhase1`; connect
`ParameterManager::parametersReadyChanged` to `_vehicleParametersReadyChanged`; append to the
`_vehicles` model; send a GCS heartbeat immediately (the comment notes this lets PX4 start
accepting commands); write `FirmwareUpgradeSettings::defaultFirmwareType`; emit `vehicleAdded`;
and call `setActiveVehicle(vehicle)` **only when it is the first vehicle**. Additional vehicles
raise a "Connected to Vehicle N" message instead.

### Identity

A vehicle is identified by its MAVLink **system id** only — `_systemID`, exposed as
`Q_PROPERTY(int id ... CONSTANT)`. `_compID` is the default component id.
`MultiVehicleManager::getVehicleById()` does a linear scan comparing `vehicle->id()`.

`vehicleUID` and `vehicleUIDStr` properties exist but are **not** used to key the vehicle list.
There is no serial-number or UID-based vehicle matching. For an Aerora fleet this matters and is
recorded as `OPEN` in [open-questions.md](open-questions.md).

### Offline editing vehicle

A second constructor creates a disconnected vehicle for offline planning
(`Vehicle.h:128-131`), instantiated once at `MultiVehicleManager.cc:55` using sentinel values
`Vehicle::MAV_AUTOPILOT_TRACK = MAV_AUTOPILOT_ENUM_END` and
`Vehicle::MAV_TYPE_TRACK = MAV_TYPE_ENUM_END` (`Vehicle.h:120-126`). Kept as valid enum values
deliberately, to avoid undefined-behaviour-sanitizer failures on enum loads. It is exposed as
`offlineEditingVehicle` and is **not** in the `vehicles` model.

### Destruction — two-phase, deliberately deferred

```text
VehicleLinkManager::allLinksRemoved
  └── _deleteVehiclePhase1            MultiVehicleManager.cc:148
        ├── remove from _vehicles, deselectVehicle()
        ├── clear activeVehicleAvailable / parameterReadyVehicleAvailable
        ├── emit vehicleRemoved
        └── QTimer::singleShot(20, ...) ──┐
                                          │
  ┌───────────────────────────────────────┘
  └── _deleteVehiclePhase2            MultiVehicleManager.cc:190
        ├── promote _vehicles->get(0) to active
        └── vehicle->deleteLater()
```

The 20 ms hop exists because QML may still hold references until the event loop unwinds. The code
comment explains that the signals must flow through the system and return to the main loop event
queue before deletion is safe. `setActiveVehicle()` (line 211) uses the same deferral through
`_setActiveVehiclePhase2` (line 231): availability is dropped first to force QML to disconnect,
then the new vehicle is installed.

**Aerora consequence.** Any Aerora component holding a `Vehicle*` must connect to
`vehicleRemoved` or `QObject::destroyed` and null its pointer. The `vehicle-null-check`
pre-commit hook enforces the null-check discipline on `activeVehicle()` access.

### MultiVehicleManager responsibilities

Vehicle factory and registry (`vehicles` and `selectedVehicles` as `QmlObjectListModel`), active
vehicle arbiter, owner of the offline editing vehicle, and GCS heartbeat transmitter.
`kGCSHeartbeatRateMSecs = 1000` (`MultiVehicleManager.h:78`); `_sendGCSHeartbeat()`
(`.cc:255`) packs a `MAV_TYPE_GCS` HEARTBEAT onto every connected, non-high-latency link, gated
on `MavlinkSettings::sendGCSHeartbeat`.

## Decomposition

FactGroups are created in `Vehicle::_commonInit()` at `Vehicle.cc:311-330` and registered at
`Vehicle.cc:345-362`. QML names are the string constants at `Vehicle.h:1072-1090`; members at
`Vehicle.h:1092-1111`.

### Static FactGroups

| QML name | Class | File stem under `src/Vehicle/FactGroups/` |
| --- | --- | --- |
| `vehicle` | `VehicleFactGroup` | `VehicleFactGroup` + `VehicleFact.json` |
| `gps` | `VehicleGPSFactGroup` | `VehicleGPSFactGroup` + `GPSFact.json` |
| `gps2` | `VehicleGPS2FactGroup` | `VehicleGPS2FactGroup` |
| `gpsAggregate` | `VehicleGPSAggregateFactGroup` | `VehicleGPSAggregateFactGroup` |
| `wind` | `VehicleWindFactGroup` | `VehicleWindFactGroup` + `WindFact.json` |
| `vibration` | `VehicleVibrationFactGroup` | `VehicleVibrationFactGroup` + `VibrationFact.json` |
| `temperature` | `VehicleTemperatureFactGroup` | `VehicleTemperatureFactGroup` + `TemperatureFact.json` |
| `clock` | `VehicleClockFactGroup` | `VehicleClockFactGroup` + `ClockFact.json` |
| `setpoint` | `VehicleSetpointFactGroup` | `VehicleSetpointFactGroup` + `SetpointFact.json` |
| `distanceSensor` | `VehicleDistanceSensorFactGroup` | `VehicleDistanceSensorFactGroup` + `DistanceSensorFact.json` |
| `localPosition` | `VehicleLocalPositionFactGroup` | `VehicleLocalPositionFactGroup` + `LocalPositionFact.json` |
| `localPositionSetpoint` | `VehicleLocalPositionSetpointFactGroup` | `VehicleLocalPositionSetpointFactGroup` |
| `estimatorStatus` | `VehicleEstimatorStatusFactGroup` | `VehicleEstimatorStatusFactGroup` + `EstimatorStatusFactGroup.json` |
| `hygrometer` | `VehicleHygrometerFactGroup` | `VehicleHygrometerFactGroup` + `HygrometerFact.json` |
| `generator` | `VehicleGeneratorFactGroup` | `VehicleGeneratorFactGroup` + `GeneratorFact.json` |
| `efi` | `VehicleEFIFactGroup` | `VehicleEFIFactGroup` + `EFIFact.json` |
| `rpm` | `VehicleRPMFactGroup` | `VehicleRPMFactGroup` + `RPMFact.json` |
| `terrain` | `TerrainFactGroup` | `TerrainFactGroup` + `TerrainFactGroup.json` |
| `radioStatus` | `RadioStatusFactGroup` | `RadioStatusFactGroup` + `RadioStatusFact.json` |

### Dynamic FactGroup list models

Exposed as `QmlObjectListModel*`, created at `Vehicle.cc:329-330`, fed per message by
`handleMessageForFactGroupCreation()` at `Vehicle.cc:584-585`:

- `batteries` — `BatteryFactGroupListModel` + `BatteryFact.json`
- `escs` — `EscStatusFactGroupListModel` + `EscStatusFactGroup.json`

Two precise notes. First, `rpm` has an accessor and is registered but has **no**
`Q_PROPERTY(FactGroup* rpm ...)` in the property block at `Vehicle.h:237-257`; QML must reach it
via `getFactGroup("rpm")`. Second, firmware plugins can inject their own groups:
`FirmwarePlugin::factGroups()` returns `QMap<QString, FactGroup*>*` and the result is merged at
`Vehicle.cc:366-370`.

### Aggregated sub-managers

Accessors at `Vehicle.h:574-584`, members at `Vehicle.h:1115-1140` plus later per-section private
blocks:

`ParameterManager`, `VehicleLinkManager`, `MissionManager`, `GeoFenceManager`,
`RallyPointManager`, `FTPManager`, `ComponentInformationManager`, `VehicleObjectAvoidance`,
`Autotune`, `RemoteIDManager`, `StandardModes`, `InitialConnectStateMachine`, `Actuators`,
`TerrainProtocolHandler`, `TerrainQueryCoordinator`, `MavCommandQueue`,
`RequestMessageCoordinator`, `MessageIntervalManager`, `ImageProtocolManager`,
`MAVLinkLogManager`, `QGCCameraManager`, `VehicleSigningController`, `GimbalController`,
`MAVLinkEventManager`, `VehicleSupports`, `TrajectoryPoints`, `SysStatusSensorInfo`,
`MAVLinkStreamConfig`, `QGCMapCircle` (orbit), `HealthAndArmingCheckReport`.

## Fact system

### Fact

`src/FactSystem/Fact.h`, a `QML_ELEMENT`. The atomic value holder: `_name`, `_componentId`,
`_rawValue` (a `QVariant` guarded by a `mutable QRecursiveMutex`, see the inline `rawValue()` at
`Fact.h:95-99`), `_type` (`FactMetaData::ValueType_t`), and a non-owning `FactMetaData
*_metaData`.

### Raw versus cooked

Raw is the vehicle's native representation — exactly what goes into `PARAM_SET` and comes out of
`PARAM_VALUE`. Cooked is raw passed through a translator for display in user-selected units.

The mechanism is a plain function pointer:

```cpp
typedef QVariant (*Translator)(const QVariant &from);   // FactMetaData.h:42
```

Both `_rawTranslator` and `_cookedTranslator` default to the identity `_defaultTranslator`
(`FactMetaData.h:279`, `362-363`). Wiring comes from `setTranslators()` (line 205),
`setBuiltInTranslator()` (208), and `_setAppSettingsTranslators()` (249), with lookup tables
`AppSettingsTranslation_s` (329-336) and `BuiltInTranslation_s` (391-396). Metadata carries both
`rawUnits()` and `cookedUnits()` (148-149), and every bound — `min`, `max`, `userMin`, `userMax`,
`increment`, `defaultValue` — exists in both raw and cooked form.

Mixing the two is listed as a common pitfall in `CODING_STYLE.md`. Aerora code must be explicit
about which it is using.

### FactGroup

`src/FactSystem/FactGroup.h` is the container and namespace node: `_nameToFactMap`,
`_nameToFactGroupMap`, `_nameToFactMetaDataMap`, with `Q_INVOKABLE factExists`, `getFact`,
`getFactGroup`, the `factNames` and `factGroupNames` properties, and a `telemetryAvailable` flag.

Telemetry groups implement `virtual void handleMessage(Vehicle*, const mavlink_message_t&)` to
decode MAVLink into their facts. The constructor takes `updateRateMsecs` plus an optional metadata
JSON file; a `QTimer` with `Fact::setSendValueChangedSignals()` and
`sendDeferredValueChangedSignal()` provides rate-limited UI notification so high-rate telemetry
does not flood QML.

**This is the correct insertion point for Aerora telemetry.** A new FactGroup with its own
metadata JSON and `handleMessage` implementation gives Aerora payload, power, and autonomy
telemetry first-class Fact semantics — units, bounds, translation, QML binding, rate limiting —
without bespoke storage, satisfying the Fact System golden rule.

### Parameter to Fact

`ParameterManager::mavlinkMessageReceived()` (`ParameterManager.cc:107`) dispatches to
`_handleParamValue()` (`.cc:138`). The creation block is `.cc:239-254`:

```cpp
fact = new Fact(componentId, parameterName, mavTypeToFactType(mavParamType), this);
FactMetaData *const factMetaData = _vehicle->compInfoManager()
        ->compInfoParam(componentId)->factMetaDataForName(parameterName, fact->type());
fact->setMetaData(factMetaData);
_mapCompId2FactMap[componentId][parameterName] = fact;
connect(fact, &Fact::containerRawValueChanged, this, &ParameterManager::_factRawValueUpdated);
emit factAdded(componentId, fact);
// ...
fact->containerSetRawValue(parameterValue);
```

Storage is `QMap<int /*compId*/, QMap<QString /*name*/, Fact*>> _mapCompId2FactMap`.

The round trip is asymmetric on purpose. QML writes go `setRawValue`/`setCookedValue` to
`containerRawValueChanged` to `_factRawValueUpdated` (`.cc:536`) to `_mavlinkParamSet` (`.cc:306`).
Vehicle-originated values use `containerSetRawValue()`, which does **not** re-emit the container
signal (documented at `Fact.h:176`), preventing write loops.

### Metadata sources

`ParameterManager` delegates. `CompInfoParam::_getParameterMetaData()`
(`src/Vehicle/ComponentInformation/CompInfoParam.cc:139-149`) calls
`vehicle->firmwarePlugin()->loadParameterMetaData(vehicle)`, which lands in
`FirmwarePlugin::loadParameterMetaData()` (`FirmwarePlugin.cc:523`). That builds a
firmware-specific parser via `_createParameterMetaData()` and feeds it the file chosen by
`_cachedParameterMetaDataFile()` (`.cc:542`).

| Firmware | Metadata file |
| --- | --- |
| PX4 | `:/FirmwarePlugin/PX4/PX4ParameterFactMetaData.json` (`PX4FirmwarePlugin.h:57`) |
| APM | Per vehicle class, selected in `APMFirmwarePlugin::_internalParameterMetaDataFile()` (`APMFirmwarePlugin.cc:710`) |
| Generic base | Empty string (`FirmwarePlugin.h:417`) |

Downloaded metadata is cached under `<QStandardPaths::CacheLocation>/ParameterMetaData`
(`FirmwarePlugin.cc:512-520`), matched by the wildcard
`ParameterFactMetaData_<autopilot>.<major>.*.json` (lines 566-567), with newer downloads stored by
`cacheParameterMetaDataFile()` (lines 625-628).

Note that `CompInfoParam` only loads firmware metadata for `MAV_COMP_ID_AUTOPILOT1`; other
components rely on metadata supplied via `COMPONENT_INFORMATION`.

Firmware plugins also get a final pass: `FirmwarePlugin::adjustMetaData(MAV_TYPE, FactMetaData*)`,
applied to all Vehicle facts at `Vehicle.cc:163-164`.

### parametersReady

`Q_PROPERTY(bool parametersReady READ parametersReady NOTIFY parametersReadyChanged)`
(`ParameterManager.h:26`). Set `true` on four distinct paths:

| Line | Condition |
| --- | --- |
| `ParameterManager.cc:655` | High-latency link or log replay — download skipped, `_missingParameters = true` |
| `ParameterManager.cc:1434` | Normal completion in `_checkInitialLoadComplete()`, then calls `autopilotPlugin()->parametersReadyPreChecks()` and emits the change |
| `ParameterManager.cc:1508` | `_checkOtherComponentsLoadComplete()` |
| `ParameterManager.cc:1634` | Offline editing vehicle after `_loadOfflineEditingParams()` |

It means "the parameter set is usable", **not** "complete" — `missingParameters` is the separate
completeness flag. `MultiVehicleManager` mirrors it as `parameterReadyVehicleAvailable`, and
`AutoPilotPlugin::vehicleComponents()` refuses to build the setup UI before it
(`APMAutoPilotPlugin.cc:63`, warning at line 184).

Accessing Facts before `parametersReady` is the second common pitfall in `CODING_STYLE.md`.

### QML binding

`Fact`'s `Q_PROPERTY` block (`Fact.h:22-68`) is the binding surface: `value` maps to
`cookedValue`/`setCookedValue` with `NOTIFY valueChanged`; `rawValue` maps to
`rawValue`/`setRawValue` with `NOTIFY rawValueChanged`; plus read-only metadata properties `min`,
`max`, `units`, `enumStrings`, `enumValues`, `decimalPlaces`, `shortDescription`,
`bitmaskStrings`, `valueString`.

QML obtains a `Fact*` from a FactGroup (`vehicle.gps.lat.value`), from
`vehicle.parameterManager`, or through the wrapper components in `src/FactSystem/FactControls`.
`vehicleRebootRequired` and `qgcRebootRequired` drive the reboot prompt via
`Fact::_checkForRebootMessaging()`.

## Parameter download

Timers configured in the `ParameterManager` constructor (`.cc:57-70`), all single-shot:

| Timer | Production | Unit test |
| --- | --- | --- |
| `_hashCheckTimer` | `kHashCheckTimeoutMs = 1000` ms | `kTestHashCheckTimeoutMs = 200` |
| `_paramRequestListTimer` | `kParamRequestListTimeoutMs = 5000` ms | `kTestInitialRequestIntervalMs = 500` |
| `_waitingParamTimeoutTimer` | 3000 ms hard-coded | 500 |

Four download branches in `_startParameterDownload()` (`.cc:646-731`):

1. **Skip.** Primary link is high-latency, or log replay. Sets `_parametersReady = true`,
   `_missingParameters = true`, `_initialLoadComplete = true`. No parameters are fetched
   (lines 654-658).
2. **PX4 hash check.** `_requestHashCheck()` issues a `PARAM_REQUEST_READ` for `_HASH_CHECK` to
   try the local cache first (lines 665-672).
3. **FTP.** When `_tryftp` (initialised to `vehicle->apmFirmware() || vehicle->px4Firmware()` at
   `.cc:44`), download `@PARAM/param.pck?withdefaults=1` through `FTPManager` (lines 673-700).
   Completion in `_ftpDownloadComplete()` (`.cc:547`), binary parse in `_parseParamFile()`
   (`.cc:1709`, magic `0x671B`). A "File not Found" reply means no FTP parameter support, so
   `_tryftp = false` and the code falls through (lines 580-591).
4. **Classic stream.** `mavlink_msg_param_request_list_pack_chan(...)` at `.cc:719`.

### Retry and robustness constants

All public for test use, `ParameterManager.h:116-130`:

| Constant | Value |
| --- | --- |
| `kParamSetRetryCount` | 2 |
| `kParamRequestReadRetryCount` | 2 |
| `kWaitForParamValueAckMs` | 1000 (50 in unit tests) |
| `kMaxInitialRequestListRetry` | 4 |
| `kMaxInitialLoadRetrySingleParam` | 5 |
| `kIndexBatchMaxOutstanding` | 10 |
| `kUnresponsiveMinOutstanding` | 5 |
| `kUnresponsiveSilentCycles` | 2 |

Gap filling uses `_waitingReadParamIndexMap` (component id to index to retry count);
`_fillIndexBatchQueue()` (`.cc:852`) keeps at most `kIndexBatchMaxOutstanding` index re-reads in
flight; `_waitingParamTimeout()` (`.cc:900`) drives retry rounds;
`_giveUpOnUnresponsiveComponents()` (`.cc:935`) abandons non-default components silent for
`kUnresponsiveSilentCycles` cycles, never the default component.
`_paramRequestListTimeout()` (`.cc:1501`) handles a vehicle that never answers
`PARAM_REQUEST_LIST` and ultimately emits `initialParametersRequestFailed()`.

ArduPilot's habit of streaming unrequested parameters before answering the list is explicitly
filtered at `.cc:151-155`.

### Parameter cache

A binary `QDataStream` dump of `QMap<QString, QPair<int, QVariant>>`.

```text
parameterCacheDir()  = <dir of QSettings().fileName()> / <settings completeBaseName> / ParamCache
parameterCacheFile() = parameterCacheDir() / "<vehicleId>_<componentId>.v2"
```

Definitions at `ParameterManager.cc:1152-1164`. On Windows this resolves under
`%APPDATA%\<Org>\<AppName>\ParamCache\1_1.v2`. The per-application subdirectory is deliberate, to
isolate parallel test runs.

Written by `_writeLocalParamCache()` (`.cc:1133`), loaded by `_tryCacheHashLoad()` (`.cc:1166`),
which CRC32s the cached set and compares against the `_HASH_CHECK` value (`.cc:1213`). A match
stops `_paramRequestListTimer` and skips the download. Facts with `volatileValue()` are excluded
from the CRC.

**The cache is PX4-only**, guarded by `!_logReplay && _vehicle->px4Firmware()` at `.cc:251` and
`.cc:1916`, because ArduPilot and Solo have volatile parameters that invalidate it.

### Dead code worth recording

`void _loadMetaData();` and `void _clearMetaData();` are declared at `ParameterManager.h:165-166`,
and `bool _metaDataAddedToFacts` at line 216, but a search across `src/**/*.{h,cc}` found **no
definition and no call site** for any of them. Metadata attachment now happens inline via
`CompInfoParam` at `.cc:244`. These are vestigial declarations from before the ComponentInformation
refactor.

## Firmware plugin

### Contract

`src/FirmwarePlugin/FirmwarePlugin.h`, 477 lines. Important: it is **not abstract**. The base class
is a working MAVLink-generic implementation and is instantiated as the fallback
(`FirmwarePluginManager.cc:82-86`).

Plugin instances are **shared singletons per firmware class**. That is why per-vehicle plugin state
is pushed into the separate `FirmwarePluginInstanceData` class (`FirmwarePlugin.h:440-470`), used
for MAV_CMD support probing. Aerora must not store per-vehicle state on a firmware plugin.

### Selection path

```text
Vehicle::_commonInit()                                                    Vehicle.cc:229
  └── FirmwarePluginManager::instance()->firmwarePluginForAutopilot(...)
        └── FirmwarePluginManager::firmwarePluginForAutopilot()           FirmwarePluginManager.cc:76
              └── _findPluginFactory(firmwareClass)                       FirmwarePluginManager.cc:94
                    │  iterates FirmwarePluginFactoryRegister::pluginFactories()
                    │  returns the FIRST factory claiming the class
                    └── factory->firmwarePluginForAutopilot(...)          returns cached singleton
```

Re-resolved for the offline vehicle at `Vehicle.cc:483`. If no factory matches, the generic
`FirmwarePlugin` is used.

### Registration — extensible without core edits

**Confirmed: a vendor can register a new firmware plugin with no core factory edit.** The mechanism
is constructor self-registration, not a hard-coded table.

`src/FirmwarePlugin/FirmwarePluginFactory.cc:16`:

```cpp
FirmwarePluginFactory::FirmwarePluginFactory(QObject *parent)
{
    FirmwarePluginFactoryRegister::instance()->registerPluginFactory(this);
}
```

`FirmwarePluginFactoryRegister` is a `Q_GLOBAL_STATIC` holding only
`QList<FirmwarePluginFactory*> _factoryList` (`FirmwarePluginFactory.h:32-47`). There is no switch
statement, no enum registry, and no core list to edit. Registration is triggered by declaring a
file-scope static factory instance — `PX4FirmwarePluginFactory.cc:4` and
`APMFirmwarePluginFactory.cc:10` are the stock examples.

The pure virtuals a vendor factory must implement (`FirmwarePluginFactory.h`):

```cpp
virtual FirmwarePlugin *firmwarePluginForAutopilot(MAV_AUTOPILOT, MAV_TYPE) = 0;
virtual QList<QGCMAVLink::FirmwareClass_t> supportedFirmwareClasses() const = 0;
virtual QList<QGCMAVLink::VehicleClass_t> supportedVehicleClasses() const;  // defaults to all
```

`custom-example/` demonstrates the whole pattern:

- `src/FirmwarePlugin/CustomFirmwarePluginFactory.h:13` — the factory subclass.
- `src/FirmwarePlugin/CustomFirmwarePluginFactory.h:27` —
  `extern CustomFirmwarePluginFactory CustomFirmwarePluginFactoryImp;` paired with the definition
  at `.cc:4`. The `extern` declaration keeps the linker from discarding the static object whose
  constructor performs registration.
- `src/FirmwarePlugin/CustomFirmwarePlugin.h:8` — `class CustomFirmwarePlugin : public
  PX4FirmwarePlugin`, overriding `autopilotPlugin()`, `toolIndicators()`, `hasGimbal()`,
  `updateAvailableFlightModes()`.

Two caveats, stated precisely:

- **Link visibility.** Registration happens only if the factory's translation unit is linked in
  and its static initialiser survives. Stock sources are added directly to the main target. A
  vendor placing the factory in a separate static library must force-link it.
- **First match wins, no priority.** `_findPluginFactory()` returns the first factory claiming the
  firmware class, and registration order is static-initialisation order across translation units,
  which is effectively unspecified. Two factories claiming the same `FirmwareClass_t` is a silent
  conflict. `custom-example` sidesteps this by setting `QGC_DISABLE_PX4_PLUGIN_FACTORY` and
  `QGC_DISABLE_APM_PLUGIN_FACTORY`. Recorded in [risk-register.md](risk-register.md).

### Key virtuals for a vendor

From `FirmwarePlugin.h`: `autopilotPlugin()`, `initializeVehicle()`, `isCapable()` (gates the
`FirmwareCapabilities` bitmask at lines 80-91), `flightModes()`, `flightMode()`,
`setFlightMode()`, `updateAvailableFlightModes()`, the named-mode accessors (`pauseFlightMode`,
`missionFlightMode`, `rtlFlightMode`, `smartRTLFlightMode`, `landFlightMode`, `takeOffFlightMode`,
`stabilizedFlightMode`, `gotoFlightMode`, `followFlightMode`, `takeControlFlightMode`,
`motorDetectionFlightMode`), `isGuidedMode`/`setGuidedMode`, the `guidedMode*` family (RTL, Land,
Takeoff, GotoLocation, ChangeAltitude, ChangeHeading, ChangeGroundSpeed, ROI),
`adjustIncomingMavlinkMessage()`, `adjustOutgoingMavlinkMessageThreadSafe()`,
`sendHomePositionToVehicle()`, `supportedMissionCommands()`, `missionCommandOverrides()`,
`paramNameRemapMajorVersionMap()`, `offlineEditingParamFile()`, `vehicleImageOpaque()`,
`vehicleImageOutline()`, `toolIndicators()`, `createCameraManager()`, `createCameraControl()`,
`factGroups()`, `adjustMetaData()`, `hasGimbal()`, `getHobbsMeter()`, `createAutotune()`,
`batteryConsumptionData()`, `autoDisarmParameter()`, and the protected metadata triple
`_internalParameterMetaDataFile()`, `_autopilotType()`, `_createParameterMetaData()`.

**Important for Aerora.** The named flight-mode accessors are how QGC decides what to command.
Because Aerora's final failsafe behaviour is `OPEN` per the v4.1 baseline, these must not be
populated with guessed Aerora mode names. See [open-questions.md](open-questions.md).

## AutoPilot plugin

`FirmwarePlugin` is flight-stack **behaviour** — one shared singleton per firmware class.
`AutoPilotPlugin` (`src/AutoPilotPlugins/AutoPilotPlugin.h`) is the per-vehicle **setup and
configuration UI model**, owned by the Vehicle. It is created *by* the firmware plugin:

```cpp
_autopilotPlugin = _firmwarePlugin->autopilotPlugin(this);   // Vehicle.cc:145
_autopilotPlugin->setParent(this);                           // Vehicle.cc:147
```

Deleted in the destructor at `Vehicle.cc:408`. Exposed as
`Q_PROPERTY(AutoPilotPlugin* autopilotPlugin MEMBER _autopilotPlugin CONSTANT)`.

Abstract on two members:

```cpp
virtual const QVariantList &vehicleComponents() = 0;
Q_INVOKABLE virtual QString prerequisiteSetup(VehicleComponent*) const = 0;
```

It publishes `Q_PROPERTY(QVariantList vehicleComponents ...)` and
`Q_PROPERTY(bool setupComplete ...)`, plus a firmware-agnostic lookup via the
`KnownVehicleComponent` enum (`AutoPilotPlugin.h:31-41`: Radio, FlightModes, Sensors, Safety,
Power, Joystick, ESC, Unknown) with `findKnownVehicleComponent()` and
`knownVehicleComponentAvailable()`.

### Driving the setup UI

`vehicleComponents()` lazily builds the list on first request, and only once `parametersReady`.
Canonical implementation: `APMAutoPilotPlugin::vehicleComponents()`
(`src/AutoPilotPlugins/APM/APMAutoPilotPlugin.cc:61-196`). Each entry is
`new <X>Component(_vehicle, this)`, then `setupTriggerSignals()`, then appended as
`QVariant::fromValue(qobject_cast<VehicleComponent*>(...))`.

Inclusion is conditional on live vehicle facts — `parameterExists(-1, "ARSPD_TYPE")` for airspeed
(line 84), `MOT_PWM_TYPE`/`Q_M_PWM_TYPE` for ESC (95), `SERVO1_MIN` for servos (108),
`MAV_TYPE_HELICOPTER` plus a version compare for heli (131), `_vehicle->sub()` for Lights and
SubFrame (150-164), `parameterExists(MAV_COMP_ID_UDP_BRIDGE, "SW_VER")` for ESP8266 (166). The
list is finally sorted alphabetically (189-191).

`prerequisiteSetup()` (200-236) returns the name of a blocking component — airframe before radio,
power, tuning, sensors — which the UI uses to grey out pages.

### VehicleComponent

`src/AutoPilotPlugins/VehicleComponent.h` is the page descriptor. Pure virtuals: `name()`,
`iconResource()`, `requiresSetup()`, `setupComplete()`, `setupSource()` (the QML URL),
`summaryQmlSource()`, `setupCompleteChangedTriggerList()` (parameter names that re-evaluate
completeness).

It also supports a declarative, JSON-driven page model: `vehicleConfigJson()` returns a
`VehicleConfig.json` resource path from which `sectionIds()` and `sectionKeywords()` are
auto-populated, including repeat-group expansion filtered by `enableParam`/`disabledParamValue`.
Safety gates `allowSetupWhileArmed()` and `allowSetupWhileFlying()` are also present — relevant to
Aerora's supervised-operation requirements.

### Adding Aerora setup pages

Subclass the firmware's AutoPilotPlugin, override `vehicleComponents()`, and return it from
`FirmwarePlugin::autopilotPlugin()`. The worked reference is
`custom-example/src/AutoPilotPlugin/CustomAutoPilotPlugin.{h,cc}`, which rebuilds the list and
hides most pages unless `QGCCorePlugin::showAdvancedUI` is set, clearing `_components` and
re-emitting `vehicleComponentsChanged()` when the flag flips.

## Link management and communication loss

`VehicleLinkManager` is one per Vehicle, created at `Vehicle.cc:271`, with the discovering link
injected at `Vehicle.cc:274`. `Vehicle` is declared a friend class
(`VehicleLinkManager.h:27`).

### Per-vehicle link tracking

```cpp
struct LinkInfo_t {
    SharedLinkInterfacePtr link;
    bool commLost = false;
    QElapsedTimer heartbeatElapsedTimer;
};
QList<LinkInfo_t> _rgLinkInfo;
```

New links are discovered implicitly: `mavlinkMessageReceived()`
(`VehicleLinkManager.cc:34-52`) calls `_addLink()` for any link not already present, otherwise
restarts that link's elapsed timer. `MAVLINK_MSG_ID_RADIO_STATUS` is explicitly skipped
(lines 36-39) because SiK radios emit it locally and it proves nothing about the far end.

`_addLink()` (line 166) resolves a shared pointer from `LinkManager`, calls
`link->addVehicleReference()`, starts the per-link elapsed timer for non-high-latency links, calls
`_updatePrimaryLink()`, connects `LinkInterface::disconnected`, and starts `_commLostCheckTimer`
when the first link arrives (line 196). `_removeLink()` (line 200) mirrors it and stops the timer
when the list empties.

### Timeout constants

`src/Vehicle/VehicleLinkManager.h:84-85`. Note that the heartbeat-timeout identifier contains a
spelling error in upstream source; it is referenced here by location and value rather than
reproduced.

| Purpose | Production value | Location |
| --- | --- | --- |
| Comm-lost check interval | 1000 ms | `VehicleLinkManager.h:84` |
| Heartbeat maximum elapsed before loss | 3500 ms | `VehicleLinkManager.h:85` |

Test-only values at `VehicleLinkManager.h:89-97`: `kTestHeartbeatTimeoutMs = 1500`,
`kTestCommLostCheckTimeoutMs = 250`, and
`kTestCommLostDetectionTimeoutMs = 250 + 1500 + 300 = 2050`. The interval is selected at
`VehicleLinkManager.cc:27` and the timeout at `.cc:105`, both branching on
`QGC::runningUnitTests()`.

In production: poll every 1000 ms; declare a link lost after more than 3500 ms without a
non-RADIO_STATUS message.

### What QGC does on communication loss

`VehicleLinkManager::_commLostCheck()` (`.cc:99-152`). **All of this is GCS-side only.**

| Step | Line | Action |
| --- | --- | --- |
| Guard | 101 | Skipped entirely when `_communicationLostEnabled` is false |
| Exemption | 108 | High-latency links are exempt from the heartbeat test |
| Per link | 109-124 | Set `commLost = true`; if more than one link exists, announce loss on the primary or secondary link via `AudioOutput`; emit `linkStatusesChanged()` so QML reports "Comm Lost" |
| Failover | 126-131 | `_updatePrimaryLink()`, then audio plus an application message about switching to the secondary link |
| Total loss | 140-144 | If `_autoDisconnect` is set, `closeVehicle()` and return — which removes the vehicle |
| Total loss | 146-150 | Otherwise announce loss, set `_communicationLost = true`, emit `communicationLostChanged(true)` |

`communicationLost` is exposed as `Q_PROPERTY` (`VehicleLinkManager.h:24`) and drives the Fly View
"Communication Lost" banner. Regain is handled by `_commRegainedOnLink()` (`.cc:55-98`), which
clears the per-link flag, announces regain, may promote a new primary, and clears total loss as
soon as **any** link is healthy (lines 92-97).

Teardown: `closeVehicle()` removes every link, sets `_allLinksRemovedSignalledByCloseVehicle` to
avoid a double signal, calls `_vehicle->_stopCommandProcessing()`, and emits `allLinksRemoved`.

### Primary link policy

`_bestActivePrimaryLink()` (`.cc:250`), priority order:

1. USB-direct serial, guarded by `#ifndef QGC_NO_SERIAL_LINK`
2. Any normal-latency link
3. Keep the current high-latency link
4. Any high-latency link

Links marked `commLost` are never candidates. `_updatePrimaryLink()` (`.cc:290`) sends
`MAV_CMD_CONTROL_HIGH_LATENCY` with param1 `0` when leaving a high-latency link and `1` when
adopting one.

### Explicit statement on failsafe

Nothing in `VehicleLinkManager` commands or infers vehicle-side failsafe behaviour. The only
vehicle-directed traffic on loss is the `MAV_CMD_CONTROL_HIGH_LATENCY` toggle during primary-link
switching. Vehicle-side link-loss response is entirely autopilot-configured and out of scope of
this code.

This matters for the Aerora programme. Per the v4.1 baseline, Aerora's final failsafe behaviour is
deliberately deferred to a future Simulink implementation. QGC's comm-loss handling is
**observation and annunciation**, not control, and Aerora must preserve that boundary: the GCS
reports the effective state, the flight controller owns the response. Recorded as `OPEN` in
[open-questions.md](open-questions.md).

# Mission Architecture

Plan controllers, the visual item model, the transfer protocol, resume, and how much of this is
reusable for an industrial non-camera payload.

## Headline findings

Two results matter most for Aerora:

- **A new complex mission pattern — such as a surface-coverage painting pattern — requires zero
  core-file edits.** `custom-example/` ships a complete worked example (`PerimeterScanComplexItem`)
  proving it. This is the single best piece of news in this analysis.
- **There is no generic payload-action abstraction.** The `Section` base exists but has only two
  implementations, both camera- or speed-specific. Non-camera payload actions are new work.

## Controller structure

```text
QObject
└── PlanElementController                    src/MissionManager/PlanElementController.h  (abstract)
    ├── MissionController                    src/MissionManager/MissionController.h
    ├── GeoFenceController                   src/MissionManager/GeoFenceController.h
    └── RallyPointController                 src/MissionManager/RallyPointController.h

QObject
└── PlanMasterController                     src/MissionManager/PlanMasterController.h
                                             (owner/orchestrator, NOT a PlanElementController)
```

### PlanElementController

`PlanElementController.h:11-66` is the shared abstract base. Its contract is the full plan-element
lifecycle, all pure virtual:

| Group | Members |
| --- | --- |
| Disk | `save(QJsonObject&)`, `load(const QJsonObject&, QString& errorString)` |
| Vehicle sync | `loadFromVehicle()`, `sendToVehicle()`, `removeAllFromVehicle()` |
| Local | `removeAll()`, `showPlanFromManagerVehicle()` |
| State | `supported()`, `containsItems()`, `syncInProgress()`, `dirty()`, `setDirty(bool)` |
| Signals | `supportedChanged`, `containsItemsChanged`, `syncInProgressChanged`, `dirtyChanged`, `sendComplete`, `removeAllComplete` |
| Protected | `_masterController`, `_flyView` |

### PlanMasterController

Owns the three sub-controllers **by value** and exposes each via `Q_PROPERTY(... CONSTANT)`. This
is what QML binds to. Responsibilities:

- **Vehicle binding.** Holds `_controllerVehicle` (the offline editing vehicle) and
  `_managerVehicle` (the active vehicle, or `_controllerVehicle` if none).
  `_activeVehicleChanged()` re-binds. `startStaticActiveVehicle(Vehicle*, bool
  deleteWhenSendCompleted)` pins to one vehicle.
- **Upload and download sequencing.** A `SyncSequence` enum — `Idle`, `Mission`, `GeoFence`,
  `RallyPoints`, `InitialPlanLoad` — with `_loadSequence` and `_sendSequence`. The documented chain
  is Mission, then GeoFence, then RallyPoints, driven by the `_loadMissionComplete` /
  `_loadGeoFenceComplete` / `_loadRallyPointsComplete` and matching send slots. `Idle` exists
  specifically so a manager `loadComplete`/`sendComplete` signal triggered by *another* controller
  is ignored. `InitialPlanLoad` handles the automatic download at vehicle connect and completes on
  `Vehicle::initialPlanRequestCompleteChanged`, because unsupported sections never emit
  `loadComplete`.
- **Two separate dirty flags.** `_dirtyForSave` and `_dirtyForUpload`, aggregated in
  `_updateOverallDirty()`. This differs from older QGC, which had a single `dirty`.
- **Plan creator templates.** `planCreators()` returns a `QmlObjectListModel*` rebuilt by
  `_updatePlanCreatorsList()` keyed on `_planCreatorsVehicleClass`;
  `showCreateFromTemplate` is `!containsItems() && !_userSelectedManualCreation`.

Each sub-controller owns a protocol manager. `MissionManager : public PlanManager`
(`MissionManager.h:10`) was confirmed directly. Per the class documentation comment in
`PlanManager.h:10-11`, `GeoFenceManager` and `RallyPointManager` are also `PlanManager`
subclasses; those two headers were **not** opened, so that specific inheritance is taken from the
comment rather than confirmed.

## Plan file format

Constants in `PlanMasterController.h`:

```cpp
static constexpr int         kPlanFileVersion          = 1;
static constexpr const char* kPlanFileType             = "Plan";
static constexpr const char* kJsonMissionObjectKey     = "mission";
static constexpr const char* kJsonGeoFenceObjectKey    = "geoFence";
static constexpr const char* kJsonRallyPointsObjectKey = "rallyPoints";
```

### Save

`PlanMasterController::saveToJson()` (`.cc:416-437`):

1. `QGCCorePlugin::instance()->preSaveToJson(this, planJson)`
2. `JsonParsing::saveQGCJsonFileHeader(planJson, kPlanFileType, kPlanFileVersion)`
3. `preSaveToMissionJson` hook, `_missionController.save(missionJson)`, `postSaveToMissionJson` hook
4. `_geoFenceController.save(fenceJson)`, `_rallyPointController.save(rallyJson)`
5. Assign the three sub-objects under their keys
6. `postSaveToJson` hook

`saveToFile(filename)` (`.cc:449-479`) appends the extension if absent, writes, updates
`_currentPlanFile`, clears `_dirtyForSave`.

### Load

`loadFromFile()` (`.cc:343-379`) branches on suffix: `AppSettings::waypointsFileExtension` or
`"txt"` goes to `_missionController.loadTextFile()`; otherwise `_loadPlanJson()`. On success it
rewrites `_currentPlanFile` with the `.plan` extension and calls
`_setDirtyStates(false /*save*/, true /*upload*/)` — a freshly loaded plan is clean for disk but
dirty for upload.

`_loadPlanJson()` (`.cc:380-415`):

1. `JsonParsing::isJsonFile(bytes, jsonDoc, errorString)`
2. `preLoadFromJson` plugin hook
3. `JsonParsing::validateExternalQGCJsonFile(json, kPlanFileType, kPlanFileVersion,
   kPlanFileVersion, version, errorString)` — **minimum and maximum version are both 1**, so the
   top-level envelope accepts exactly version 1. There is no multi-version migration at the
   envelope level.
4. `validateKeys` requiring all three objects present and of type Object
5. Mission, then GeoFence, then RallyPoints load, short-circuiting on first failure
6. `postLoadFromJson` plugin hook

### Versioning mechanism

One fixed envelope version plus **independently versioned nested objects**. For example
`CameraCalc::save()` writes `json[JsonParsing::jsonVersionKey] = 2` (`CameraCalc.cc:165`) and
`CameraCalc::load()` has explicit V0 and V1 back-compatibility paths.

**Aerora consequence.** Aerora plan extensions should use the `preSaveToJson` /
`postSaveToJson` / `preSaveToMissionJson` / `postSaveToMissionJson` / `preLoadFromJson` /
`postLoadFromJson` plugin hooks and carry their own nested `version` key. Incrementing
`kPlanFileVersion` would be a core edit and would break compatibility with stock `.plan` files.

### Fire-and-forget upload

`PlanMasterController::sendPlanToVehicle(Vehicle*, filename)` (`.cc:607-611`) news a controller,
pins it with `startStaticActiveVehicle(vehicle, true)`, loads, sends, and self-deletes.

## Visual item model

All inheritance below was confirmed by reading the `class X : public Y` declarations.

```text
QObject
└── VisualMissionItem                        src/MissionManager/VisualMissionItem.h
    ├── SimpleMissionItem                    src/MissionManager/SimpleMissionItem.h
    │   └── TakeoffMissionItem               src/MissionManager/TakeoffMissionItem.h
    └── ComplexMissionItem                   src/MissionManager/ComplexMissionItem.h
        ├── MissionSettingsItem              src/MissionManager/MissionSettingsItem.h
        ├── StructureScanComplexItem         src/MissionManager/StructureScanComplexItem.h
        ├── TransectStyleComplexItem         src/MissionManager/TransectStyleComplexItem.h
        │   ├── SurveyComplexItem            src/MissionManager/SurveyComplexItem.h
        │   └── CorridorScanComplexItem      src/MissionManager/CorridorScanComplexItem.h
        └── LandingComplexItem               src/MissionManager/LandingComplexItem.h
            ├── FixedWingLandingComplexItem  src/MissionManager/FixedWingLandingComplexItem.h
            └── VTOLLandingComplexItem       src/MissionManager/VTOLLandingComplexItem.h
```

Note: `StructureScanComplexItem` derives **directly** from `ComplexMissionItem`, not from
`TransectStyleComplexItem`. Only Survey and CorridorScan are transect-style.

That is the complete set of complex patterns in core. There is **no** surface-coverage pattern, no
facade or wall pattern, and no generic polygon-fill pattern beyond Survey.

### VisualMissionItem contract

`VisualMissionItem.h:22+`, a `QML_ELEMENT` + `QML_UNCREATABLE`. Pure virtuals every item must
provide: `dirty()`, `isSimpleItem()`, `isStandaloneCoordinate()`, `specifiesCoordinate()`,
`specifiesAltitudeOnly()`, `commandDescription()`, `commandName()`, `abbreviation()`,
`coordinate()`, `entryCoordinate()`, `exitCoordinate()`, `editableAlt()`, `amslEntryAlt()`,
`amslExitAlt()`, `sequenceNumber()`, `specifiedFlightSpeed()`, `specifiedGimbalYaw()`,
`specifiedGimbalPitch()`, `exitCoordinateSameAsEntry()`, `setDirty()`, `setCoordinate()`,
`setSequenceNumber()`, `lastSequenceNumber()`, `save(QJsonArray&)`, `mapVisualQML()`,
**`appendMissionItems(QList<MissionItem*>&, QObject* missionItemParent)`**, `applyNewAltitude()`,
`additionalTimeDelay()`.

Virtuals with defaults worth noting: `isTakeoffItem()`, `isLandCommand()`, `isSurveyItem()`,
`specifiedVehicleYaw()`, `boundingCube()`, `setMissionFlightStatus(MissionFlightStatus_t&)`
(overrides **must** call the base), `readyForSaveState()`, `geoMapVisualQML()` (empty means none),
`terrainAltitudeRequiredInFlyView()`.

It also owns terrain querying (`_updateTerrainTimer`, `TerrainAtCoordinateQuery`), the
`_childItems` model for subsequent non-coordinate items, `_parentItem`, and the recalculation
outputs written by `MissionController`: `altDifference`, `altPercent`, `terrainPercent`,
`terrainCollision`, `azimuth`, `distance`, `distanceFromStart`.

JSON type discrimination:

```cpp
static constexpr const char* jsonTypeKey              = "type";
static constexpr const char* jsonTypeSimpleItemValue  = "SimpleItem";
static constexpr const char* jsonTypeComplexItemValue = "ComplexItem";
// and on ComplexMissionItem:
static constexpr const char* jsonComplexItemTypeKey   = "complexItemType";
```

### ComplexMissionItem additions

`ComplexMissionItem.h:18-130` adds `patternName()` (pure virtual, the UI label),
`complexDistance()`, `minAMSLAltitude()`, `maxAMSLAltitude()`, `greatestDistanceTo()`,
`load(const QJsonObject&, int sequenceNumber, QString& errorString)`, `isSingleItem()`,
`terrainCollision()`, `addKMLVisuals()`, and `applyPreviousAltitudeFrame(AltitudeFrame, double)`
(no-op default, called by `MissionController` after insert when the global frame is
`AltitudeFrameMixed`).

It also owns a **preset system**: `presetsSettingsGroup()` (an empty string means presets
unsupported), `loadPreset`, `savePreset`, `deletePreset`, `_savePresetJson`, `_loadPresetJson`,
with key `_presetSettingsKey = "_presets"`. For a painting pattern with repeatable job
configurations this is directly useful.

Plus `_flightPathSegments` (a `QmlObjectListModel` of `FlightPathSegment`) and
`_appendFlightPathSegment(...)`.

### Expansion into MAVLink items

Expansion happens **entirely** through
`VisualMissionItem::appendMissionItems(QList<MissionItem*>& items, QObject* missionItemParent)`.
The complex item news one `MissionItem` per generated MAVLink command, assigning sequence numbers
from its own `sequenceNumber()`, and declares the range it consumes via `lastSequenceNumber()`.
`MissionController` walks the visual item list and concatenates results to build the flat upload
list.

The custom example shows the shape concretely
(`custom-example/src/MissionManager/PerimeterScanComplexItem.cc:160-195`): iterate polygon
vertices, `items.append(new MissionItem(seqNum++, MAV_CMD_NAV_WAYPOINT, ...))`, then close the
loop by re-appending vertex 0.

`SurveyComplexItem::appendMissionItems` and the `TransectStyleComplexItem` bodies were **not**
read, so Survey's exact emitted command sequence — transect splitting, turnaround points,
`DO_SET_CAM_TRIGG_DIST` placement — is not described here from first-hand reading.

### Reverse direction — scanning a downloaded mission

`MissionController::_scanForAdditionalSettings()` (`.cc:1801+`) first tries
`FixedWingLandingComplexItem::scanForItems(...)`, falling back to
`VTOLLandingComplexItem::scanForItems(...)`, then walks items calling
`MissionSettingsItem::scanForMissionSettings` and `SimpleMissionItem::scanForSections`. The code
comment is explicit: a complex item cannot have sections.

**Only landing patterns are round-trippable from a raw vehicle mission.** Survey, CorridorScan, and
StructureScan are **not** reconstructed from downloaded items. An Aerora painting pattern would
likewise not survive a download-from-vehicle round trip unless Aerora implements its own
`scanForItems`. Recorded as `OPEN` in [open-questions.md](open-questions.md).

## Adding an Aerora pattern — extensible, with caveats

This checkout ships a complete worked example. `custom-example/` contains
`PerimeterScanComplexItem`, a new polygon-perimeter pattern added entirely from the overlay
directory. **A new pattern does not require editing core files** to be created, listed in the
picker, saved and loaded, or given its own editor and map visual.

### Required implementation

Subclass one of:

| Base | Use when |
| --- | --- |
| `ComplexMissionItem` | Generic — what PerimeterScan does |
| `TransectStyleComplexItem` | You want the transect and camera-calc machinery (Survey, CorridorScan) |
| `LandingComplexItem` | Landing patterns only |

Members to override, the verified full list from
`custom-example/src/MissionManager/PerimeterScanComplexItem.h:46-86`:

```text
# ComplexMissionItem overrides
patternName, complexDistance, minAMSLAltitude, maxAMSLAltitude,
lastSequenceNumber, load(QJsonObject, int, QString&), greatestDistanceTo, mapVisualQML

# VisualMissionItem overrides
dirty, isSimpleItem, isStandaloneCoordinate, specifiesCoordinate, specifiesAltitudeOnly,
commandDescription, commandName, abbreviation, coordinate, entryCoordinate, exitCoordinate,
exitCoordinateSameAsEntry, editableAlt, amslEntryAlt, amslExitAlt, sequenceNumber,
specifiedFlightSpeed, specifiedGimbalYaw, specifiedGimbalPitch,
appendMissionItems, setMissionFlightStatus, applyNewAltitude, additionalTimeDelay,
readyForSaveState, setDirty, setCoordinate, setSequenceNumber, save(QJsonArray&)
```

Plus three static identity constants — the naming convention matters because the factory matches
on both of the first two:

```cpp
static constexpr const char* canonicalName            = "Perimeter Scan";   // picker key + UI label
static constexpr const char* jsonComplexItemTypeValue = "perimeterScan";    // .plan complexItemType
static constexpr const char* settingsGroup            = "PerimeterScan";    // Fact settings group
```

And in the constructor (`PerimeterScanComplexItem.cc:17-30`): build `_metaDataMap` from
`FactMetaData::createMapFromJsonFile(":/json/PerimeterScan.SettingsGroup.json", this)`, construct
`SettingsFact` objects against it, and set
`_editorQml = "qrc:/qml/Custom/Plan/PerimeterScanEditor.qml"`.

### Every registration point

| # | What | Where | Core edit? |
| --- | --- | --- | --- |
| 1 | Picker list entry — a `QVariantMap{canonicalName, translatedName}` | Override `QGCCorePlugin::complexMissionItemNames(Vehicle*)`; base at `QGCCorePlugin.cc:399-416`, example override at `custom-example/src/CustomPlugin.cc:299-311` | **No** — call base, append, return |
| 2 | Factory, matching by `canonicalName` **or** `jsonComplexItemTypeValue` | Override `QGCCorePlugin::createComplexMissionItem(...)`; base at `QGCCorePlugin.cc:428-449`, example at `CustomPlugin.cc:312-324` | **No** — handle your type, else delegate to base. This single factory serves interactive insert (`MissionController.cc:411-431`) **and** `.plan` load (`MissionController.cc:717-722`) |
| 3 | Plan template tile, optional | Subclass `PlanCreator` + override `QGCCorePlugin::planCreators(...)`; example `PerimeterScanPlanCreator.{h,cc}` | **No** |
| 4 | Editor QML | `_editorQml` in the constructor; consumed by `src/PlanView/MissionItemEditor.qml` | **No** |
| 5 | Map visual QML | `mapVisualQML()`; optionally `geoMapVisualQML()` | **No** |
| 6 | FactMetaData JSON | `<Name>.SettingsGroup.json` loaded from `:/json/...` | **No** |
| 7 | CMake sources | Append to `CUSTOM_SOURCES` in the overlay `CMakeLists.txt` | **No** — overlay only |
| 8 | CMake QML module | `qt_add_qml_module(... URI Custom.Plan ... NO_PLUGIN)`, add to `CUSTOM_LIBRARIES` | **No** — overlay only |
| 9 | CMake JSON resource | `qt_add_resources(... PREFIX "/json" ...)` | **No** — overlay only |

**There are no `.qrc` files involved.** The build uses CMake-driven Qt resources throughout. For
core items, `src/MissionManager/CMakeLists.txt:87` is just
`qgc_add_json_resources(json_mission_manager)` — an unfiltered glob pulling every `*.json` in the
directory into the `:/json` prefix.

### The QML picker is fully data-driven

`src/PlanView/PlanView.qml:830-855`:

```qml
Repeater {
    model: _missionController.complexMissionItems
    QGCButton {
        text: modelData.translatedName
        onClicked: insertComplexItemAfterCurrent(modelData.canonicalName)
    }
}
```

Lines 506-517 handle the single-pattern degenerate case by turning the toolstrip button into a
direct insert. `MissionController::complexMissionItems()` (`.cc:1849-1852`) simply forwards to the
plugin, and is re-emitted on `Vehicle::vehicleTypeChanged` (`.cc:1662`). **No QML edit is needed to
add a pattern.**

### Caveats — where core edits would still be required

These are real and must inform the Aerora design.

1. **`MissionController::_insertComplexMissionItemWorker()` (`.cc:444-480`) hardcodes pattern
   identity by `qobject_cast`:**

   ```cpp
   bool surveyStyleItem = qobject_cast<SurveyComplexItem*>(complexItem) ||
                          qobject_cast<CorridorScanComplexItem*>(complexItem) ||
                          qobject_cast<StructureScanComplexItem*>(complexItem);
   ```

   `surveyStyleItem` drives special insertion behaviour — setting the camera to photo mode and
   pointing the gimbal straight down. A plugin-supplied pattern deriving from
   `ComplexMissionItem` will **not** get that treatment. Deriving from `SurveyComplexItem` or
   `CorridorScanComplexItem` would opt in via the cast; deriving from
   `TransectStyleComplexItem` alone would **not**. Also at lines 478-481, any item that is not a
   `VTOLLandingComplexItem` gets `setWizardMode(true)` by default.

2. **Removal logic also casts.** `MissionController.cc:513` and `:527` use
   `_visualItems->value<SurveyComplexItem*>(i) || _visualItems->value<CorridorScanComplexItem*>(i)`
   for the survey-style bookkeeping that removes the auto-added camera and gimbal commands.

3. **Landing patterns are hardcoded.** `MissionController.cc:378-385` selects between
   `FixedWingLandingComplexItem::canonicalName` and `VTOLLandingComplexItem::canonicalName` keyed
   off `fixedWing()`/`vtol()`. A custom **landing** pattern would require a core edit; a custom
   **survey or coverage** pattern would not.

4. **The overlay mechanism is build-time, not runtime.** Per `custom-example/README.md`, the
   directory is renamed to `custom` and the project rebuilt. There is no drop-in plugin loading.

**Net assessment for Aerora.** A surface-coverage painting pattern is implementable as a pure
overlay addition with zero core-file edits, provided Aerora accepts the default non-survey-style
insertion behaviour and implements roughly thirty virtuals. If Aerora needs the survey-style
insert path, the `qobject_cast` chains in `MissionController.cc` become core edits. Recorded as
`OPEN` in [open-questions.md](open-questions.md).

## Transfer protocol

`src/MissionManager/PlanManager.h` is the MAVLink mission protocol engine, shared by Mission,
GeoFence, and Rally via a `MAV_MISSION_TYPE _planType` passed to the constructor.

### Public API

```cpp
void loadFromVehicle();                                   // signals newMissionItemsAvailable
void writeMissionItems(const QList<MissionItem*>&);       // signals sendComplete
void removeAll();                                         // signals removeAllComplete
bool inProgress() const;
const QList<MissionItem*>& missionItems();
int currentIndex() const;                                 // from MISSION_CURRENT
int lastCurrentIndex() const;                             // last current index while in Mission mode
```

`writeMissionItems` **takes ownership of the `MissionItem` objects and frees them**.

### State machine

Two orthogonal state variables.

`AckType_t _expectedAck` — what message the machine is waiting for:

| State | Meaning |
| --- | --- |
| `AckNone` | Idle |
| `AckMissionCount` | `MISSION_COUNT` expected |
| `AckMissionItem` | `MISSION_ITEM` expected |
| `AckMissionRequest` | `MISSION_REQUEST` expected, or `MISSION_ACK` to end the sequence |
| `AckMissionClearAll` | `MISSION_CLEAR_ALL` sent, `MISSION_ACK` expected |
| `AckGuidedItem` | `MISSION_ACK` expected for the ArduPilot guided-mode single-item send |

`TransactionType_t _transactionInProgress` — `TransactionNone`, `TransactionRead`,
`TransactionWrite`, `TransactionRemoveAll`. Set via the private `_setTransactionInProgress()`,
which drives `inProgressChanged`.

Message handlers: `_handleMissionCount`, `_handleMissionItem`, `_handleMissionRequest`,
`_handleMissionAck`. Flow helpers: `_requestList()`, `_requestNextMissionItem()`,
`_writeMissionCount()`, `_writeMissionItemsWorker()`, `_removeAllWorker()`,
`_readTransactionComplete()`, `_finishTransaction(bool success, bool apmGuidedItemWrite)`.

Progress bookkeeping: `_itemIndicesToWrite` and `_itemIndicesToRead` work queues,
`_lastMissionRequest`, `_missionItemCountToRead`, `_missionItems` (the on-vehicle set) versus
`_writeMissionItems` (the in-flight write set, freed by `_clearAndDeleteWriteMissionItems()`).

### Timeouts and retry

```cpp
// When passively waiting for a mission process, use a longer timeout.
static constexpr int _ackTimeoutMilliseconds   = 1500;
// When actively retrying to request mission items, use a shorter timeout instead.
static constexpr int _retryTimeoutMilliseconds = 250;
static constexpr int _maxRetryCount            = 5;
static constexpr int kTestAckTimeoutMs         = 50;   // unit tests
```

Driven by `QTimer* _ackTimeoutTimer`, `_startAckTimeout(AckType_t)`, the `_ackTimeout()` slot,
`_checkForExpectedAck(AckType_t)`, and `int _retryCount`.

### Error codes

Reported via `error(int errorCode, const QString& errorMsg)` through `_sendError`:

| Code | Meaning |
| --- | --- |
| `InternalError` | |
| `AckTimeoutError` | Timed out waiting for a response |
| `ProtocolError` | Incorrect protocol sequence from the vehicle |
| `RequestRangeError` | Vehicle requested an item out of range |
| `ItemMismatchError` | Vehicle returned an item with a different sequence number than requested |
| `VehicleAckError` | Vehicle returned an error in the ack |
| `MissingRequestsError` | Vehicle did not request all items during a write |
| `MaxRetryExceeded` | Retry exhausted |
| `MissionTypeMismatch` | `MAV_MISSION_TYPE` does not match `_planType` |

`PlanManager.cc` was **not** read, so the above describes the declared states, constants, and
contract rather than line-by-line transition logic.

`MissionManager` adds `writeArduPilotGuidedMissionItem(const QGeoCoordinate&, bool altChangeOnly)`,
`_handleMissionCurrent`, `_handleHighLatency`, `_handleHighLatency2`, `_handleHeartbeat`,
`_updateMissionIndex`, and `_cachedLastCurrentIndex`.

## Execution, progress, and resume

### Signal wiring

`MissionController.cc:1654-1667`:

```cpp
connect(_missionManager, &MissionManager::progressPctChanged,      this, &MissionController::_progressPctChanged);
connect(_missionManager, &MissionManager::currentIndexChanged,     this, &MissionController::_currentMissionIndexChanged);
connect(_missionManager, &MissionManager::lastCurrentIndexChanged, this, &MissionController::resumeMissionIndexChanged);
connect(_missionManager, &MissionManager::resumeMissionReady,      this, &MissionController::resumeMissionReady);
```

### Current item, and the home-position offset

`MissionController::currentMissionIndex()` (`.cc:1753-1764`) returns `-1` outside the fly view,
otherwise `_missionManager->currentIndex()` **incremented by 1 when
`firmwarePlugin()->sendHomePositionToVehicle()` is false**. This is the PX4-versus-ArduPilot home
item offset, and `_currentMissionIndexChanged()` (`.cc:1766-1779`) applies the same adjustment
before marking the current item.

Aerora must get `sendHomePositionToVehicle()` right for its firmware plugin or every mission index
in the UI will be off by one.

### Distances

`MissionFlightStatus_t` (`src/MissionManager/MissionFlightStatus.h`) carries
`maxTelemetryDistance`, `totalDistance`, `plannedDistance`, `totalTime`, `hoverDistance`,
`hoverTime`, `cruiseDistance`, `cruiseTime`, `mAhBattery`, `hoverAmps`, `cruiseAmps`,
`ampMinutesAvailable`, `hoverAmpsTotal`, `cruiseAmpsTotal`, **`batteryChangePoint`**,
**`batteriesRequired`**, `vehicleYaw`, `gimbalYaw`, `gimbalPitch`, `vtolMode`, `cruiseSpeed`,
`hoverSpeed`, `vehicleSpeed`.

Computed by `MissionFlightStatusCalculator` (`.cc` not read), held as
`MissionController::_flightStatusCalc` and `_missionFlightStatus`, recalculated in
`_recalcMissionFlightStatus()`.

Per-item geometry written by the recalculation passes: `distance` (to previous waypoint),
`distanceFromStart` (cumulative horizontal from home), `azimuth`, `altDifference`, `altPercent`,
`terrainPercent`, `terrainCollision`.

**There is no `distanceToNext` property in `src/MissionManager/`.** A grep of
`MissionController.h` found nothing. Distance-to-next-waypoint in the Fly view comes from
elsewhere — vehicle facts or FlyView QML — and was not traced.

### Battery-swap prediction

`batteryChangePoint` uses `-1` for unsupported and `0` for not needed; `batteriesRequired` uses
`-1` for unsupported. Both are exposed as properties with change signals. QGC therefore both
**predicts** where a battery swap will be needed and provides the resume mechanism to continue
afterwards.

This is directly relevant to the Aerora battery-mode painting configuration, and the existing
machinery may be reusable. However, it models a simple energy budget, not a dual-source
tether-and-battery architecture with a paint supply. Recorded as `OPEN`.

### Mission resume

Implemented by `MissionManager::generateResumeMission(int resumeIndex)`
(`src/MissionManager/MissionManager.cc:71+`). Declared at `MissionManager.h:31-32`.

Entry path: QML calls `MissionController::resumeMission(int resumeIndex)` (`.cc:1854-1860`), which
decrements when `!sendHomePositionToVehicle()` and forwards.

The resume index itself is `MissionController::resumeMissionIndex()` (`.cc:1735-1751`):

```cpp
resumeIndex = _missionManager->lastCurrentIndex() + (sendHomePositionToVehicle() ? 0 : 1);
if (resumeIndex > 1 && resumeIndex != <last item's sequenceNumber>) {
    resumeIndex--;        // Resume at the item previous to the one we were heading towards
} else {
    resumeIndex = 0;      // 0 == resume not available
}
```

`lastCurrentIndex()` is the last current mission item reported **while in Mission flight mode** —
that is what survives a disarm or battery swap.

What `generateResumeMission` does, read from `MissionManager.cc:71-158`:

1. Bails if this is the offline editing vehicle or a transaction is in progress.
2. **Hard refusal on `MAV_CMD_DO_JUMP`.** Scans all items; if any DO_JUMP is present it shows a
   message stating resume cannot be generated and returns. **Resume is unsupported for missions
   containing jumps.**
3. Clamps `resumeIndex` to `[0, count-1]`.
4. **Snaps the resume index back to a fly-through coordinate command.** Queries
   `MissionCommandTree::getUIInfo(...)`; if the item is standalone-coordinate or does not specify a
   coordinate, it walks backwards until it finds one that does specify a coordinate and is not
   standalone.
5. Replays **state-setting DO commands** from the start so the vehicle resumes correctly
   configured. The `includedResumeCommands` allow-list is exactly:
   `MAV_CMD_DO_CONTROL_VIDEO`, `MAV_CMD_DO_SET_ROI`, `MAV_CMD_DO_DIGICAM_CONFIGURE`,
   `MAV_CMD_DO_DIGICAM_CONTROL`, `MAV_CMD_DO_MOUNT_CONFIGURE`, `MAV_CMD_DO_MOUNT_CONTROL`,
   `MAV_CMD_DO_SET_CAM_TRIGG_DIST`, `MAV_CMD_DO_FENCE_ENABLE`, `MAV_CMD_IMAGE_START_CAPTURE`,
   `MAV_CMD_IMAGE_STOP_CAPTURE`, `MAV_CMD_VIDEO_START_CAPTURE`, `MAV_CMD_VIDEO_STOP_CAPTURE`,
   `MAV_CMD_DO_CHANGE_SPEED`, `MAV_CMD_SET_CAMERA_MODE`.
6. An item is copied if it is index 0 with `addHomePosition`, **or** `i >= resumeIndex`, **or** its
   command is in the allow-list, **or** it is a takeoff command.
7. **De-duplicates no-ops in the replayed prefix**, walking backwards with `foundROI`,
   `foundCameraSetMode`, and `foundCameraStartStop` flags so only the last of each is retained.

The function tail past roughly line 158 was not read. Completion is signalled by
`PlanManager::resumeMissionReady()` / `resumeMissionUploadFail()`.

**Critical Aerora finding.** The `includedResumeCommands` allow-list is a **core constant** and
contains only camera, gimbal, ROI, speed, and fence commands. An Aerora painting payload that uses
custom MAV_CMD state-setting commands — pump on, spray rate, tank valve — would have those
commands **silently dropped** from a generated resume mission, so the aircraft would resume
painting in the wrong state. This is a safety-relevant gap, recorded in
[risk-register.md](risk-register.md) and [open-questions.md](open-questions.md).

## Camera and payload in missions

### The Section abstraction

`src/MissionManager/Section.h` defines an abstract `Section` base. `CameraSection : public Section`
(`CameraSection.h:15`) overrides `available()`, `dirty()`, `setAvailable()`, `setDirty()`,
**`scanForSection(QmlObjectListModel* visualItems, int scanIndex)`**,
**`appendSectionItems(QList<MissionItem*>& items, QObject* missionItemParent, int& seqNum)`**,
`itemCount()`, `settingsSpecified()`.

Sections attach to `SimpleMissionItem` objects, **not** to complex items, via
`SimpleMissionItem::scanForSections(...)` called from
`MissionController::_scanForAdditionalSettings()` (`.cc:1806-1830`).

**Only two concrete Sections exist**: `CameraSection` and `SpeedSection`. So there *is* a generic
section abstraction an Aerora payload could implement, but there is **no payload-agnostic
abstraction** — no `PayloadSection`, no vendor-extensible action registry. `Section.h` itself was
not read; the base contract above is inferred from `CameraSection`'s override list.

### CameraSection

The `CameraAction` enum (`CameraSection.h:23-31`, with a comment stating the values must match the
JSON metadata): `CameraActionNone`, `TakePhotosIntervalTime`, `TakePhotoIntervalDistance`,
`StopTakingPhotos`, `TakeVideo`, `StopTakingVideo`, `TakePhoto`.

Facts, with metadata from `CameraSection.FactMetaData.json`: `gimbalPitch`, `gimbalYaw`,
`cameraAction`, `cameraPhotoIntervalTime`, `cameraPhotoIntervalDistance`, `cameraMode`. Toggles:
`specifyGimbal`, `specifyCameraMode`, `cameraModeSupported`.

Round-tripping is per-command, with private scanners `_scanGimbal`, `_scanTakePhoto`,
`_scanTakePhotosIntervalTime`, `_scanTriggerStartDistance`, `_scanTriggerStopDistance`,
`_scanTakeVideo`, `_scanSetCameraMode`, plus statics `scanStopTakingPhotos`,
`scanStopTakingVideo`, `appendStopTakingPhotos`, `appendStopTakingVideo`.

Minor defect noted: `#define VIDEO_CAPTURE_STATUS_INTERVAL 0.2` at `CameraSection.h:7` carries a
comment saying "every 5 seconds". Comment and value disagree; this looks like a stale comment.

### CameraCalc and the spacing mathematics

`CameraCalc : public CameraSpec` (`src/MissionManager/CameraCalc.{h,cc}`). `CameraSpec` holds
`sensorWidth`/`sensorHeight`, `imageWidth`/`imageHeight`, `focalLength`, `landscape`.

Inputs: `distanceToSurface`, `distanceMode` (an `AltitudeFrame`), `imageDensity` (cm per pixel),
`frontalOverlap` (%), `sideOverlap` (%), `valueSetIsDistance`, `cameraName`.

Outputs: `imageFootprintSide`, `imageFootprintFrontal`, `adjustedFootprintSide`,
`adjustedFootprintFrontal`.

The core calculation, `CameraCalc::_recalcTriggerDistance()` (`.cc:119-160`), triggered by
`rawValueChanged` on every input (`.cc:39-48`):

```cpp
if (_disableRecalc || isManualCamera()) { return; }   // manual grid bypasses the math entirely

if (landscape()->rawValue().toBool()) {
    _imageFootprintSide    = (imageWidth  * imageDensity) / 100.0;
    _imageFootprintFrontal = (imageHeight * imageDensity) / 100.0;
} else {
    _imageFootprintSide    = (imageHeight * imageDensity) / 100.0;
    _imageFootprintFrontal = (imageWidth  * imageDensity) / 100.0;
}
_adjustedFootprintSideFact.setRawValue   (_imageFootprintSide    * ((100.0 - sideOverlap)    / 100.0));
_adjustedFootprintFrontalFact.setRawValue(_imageFootprintFrontal * ((100.0 - frontalOverlap) / 100.0));
```

Footprint in metres equals pixels times ground sample distance divided by 100, orientation-swapped
by `landscape`; overlap scales it down. `adjustedFootprintFrontal` **is** the photo trigger
distance and `adjustedFootprintSide` **is** the transect spacing.

### The manual-grid reuse path — directly relevant to Aerora

`CameraCalc` is explicitly overloaded as a generic spacing and trigger-distance carrier. The header
comment at `CameraCalc.h:33-38` states that when creating a manual grid, CameraCalc still stores
the manual grid information, mapping grid altitude to `distanceToSurface`, grid altitude mode to
`distanceMode`, trigger distance to `adjustedFootprintFrontal`, and transect spacing to
`adjustedFootprintSide`. The comment itself concedes this is a repurposing of the class that
greatly simplifies code and persistence.

In manual-camera mode `_recalcTriggerDistance()` early-returns and the user sets spacing and
trigger distance directly.

**This is the sanctioned existing path for driving a transect pattern with non-camera spacing
semantics** — exactly what a paint-spray pattern needs: swath width and trigger interval without
optics mathematics. Reuse is viable. The caveat is cosmetic: the abstraction is still *named*
camera, and the Fact keys persisted to `.plan` are camera-named (`DistanceToSurface`,
`AdjustedFootprintSide`, `AdjustedFootprintFrontal`).

## Mission command metadata

`MissionCommandTree` (`src/MissionManager/MissionCommandTree.{h,cc}`), a `QML_ELEMENT` +
`QML_UNCREATABLE` singleton via `Q_APPLICATION_STATIC` (`.cc:12`).

### Two-dimensional override hierarchy

Documented at `MissionCommandTree.h:16-32`: `FirmwareClass` by `VehicleClass`. The root is
`FirmwareClassGeneric` by `VehicleClassGeneric` — the base set defined by the MAVLink
specification. Each more specific cell **overrides** entries but **cannot add new commands**.

`_collapseHierarchy(cmdList, collapsedTree, baseList)` (`.cc:61+`) calls
`collapsedTree[command]->_overrideInfo(uiInfo)` for existing commands, creates a new
`MissionCommandUIInfo` **only when `baseList` is true**, and otherwise rejects with a warning.

Storage:

```cpp
QMap<FirmwareClass_t, QMap<VehicleClass_t, MissionCommandList*>>                  _staticCommandTree;
QMap<FirmwareClass_t, QMap<VehicleClass_t, QMap<MAV_CMD, MissionCommandUIInfo*>>> _allCommands;
QMap<FirmwareClass_t, QMap<VehicleClass_t, QStringList>>                          _supportedCategories;
```

Note the **VTOL-mode dimension**: `getUIInfo` takes a `vtolMode` so a VTOL vehicle gets fixed-wing
or multirotor command metadata depending on transition state.

### JSON file paths

| Scope | Resource path | Source directory | Resolver |
| --- | --- | --- | --- |
| Generic (base) | `:/json/MavCmdInfo{Common,FixedWing,MultiRotor,VTOL,Sub,Rover}.json` | `src/MissionManager/` | `FirmwarePlugin::missionCommandOverrides()`, `FirmwarePlugin.cc:95-115` |
| PX4 | `:/json/PX4-MavCmdInfo*.json` | `src/FirmwarePlugin/PX4/` | `PX4FirmwarePlugin.cc:251-268` |
| APM | `:/json/APM-MavCmdInfo*.json` | `src/FirmwarePlugin/APM/` (inferred from the resource prefix convention; directory not listed) | `APMFirmwarePlugin.cc:589-606` |
| Unit test | `:/unittest/UT-MavCmdInfo*.json` | test tree | Hardcoded, `MissionCommandTree.cc:22-27` |

Production construction iterates
`FirmwarePluginManager::instance()->supportedFirmwareClasses()` by
`QGCMAVLink::allVehicleClasses()` and asks each plugin for its override file
(`MissionCommandTree.cc:14-41`).

### Schema

`MissionCommandList` file type is `qgcFileType = "MavCmdInfo"`, with a top-level `version` key and
a `mavCmdInfo` **array** (`MissionCommandList.cc:16-40`).

Per-command object schema, documented verbatim at `MissionCommandUIInfo.h:97-119`: `id` (int,
**required**), `comment`, `rawName` (string, required, should only be set in base tree
information), `friendlyName` (defaults to `rawName`), `description`, `specifiesCoordinate`,
`specifiesAltitudeOnly`, `standaloneCoordinate`, `isTakeoffCommand`, `isLandCommand`,
`friendlyEdit`, `category` (defaults to `"Advanced"`), `paramRemove` (e.g. `"1,3"`), and
`param1` through `param7`. An `isLoiterCommand` key also exists as a property though it is not in
the documentation comment.

Per-param schema (`MissionCommandUIInfo.h:19-35`, class `MissionCmdParamInfo`): `label`
(required), `units` (should use FactMetaData unit strings for automatic translation), `default`,
`decimalPlaces` (7), `min`, `max`, `userMin`, `userMax`, `enumStrings`, `enumValues`,
`nanUnchanged`, `advanced`. **NaN is written as unquoted JSON `null`** and converted on read.

Partial overrides specify only the keys to change, but **to override a param you must supply the
entire param object** (`MissionCommandUIInfo.h:91-95`).

### Can Aerora add custom MAV_CMD entries via JSON?

**Partially.** This distinction is important.

- **Overriding existing commands: yes, no core edit.** Supply an override JSON and return its path
  from an Aerora `FirmwarePlugin::missionCommandOverrides(VehicleClass_t)`. That requires a custom
  FirmwarePlugin — a small one-time C++ shim, for which `custom-example` provides the template —
  and is pure JSON thereafter. Relabelling, re-categorising, restricting ranges, hiding params, and
  marking params advanced are all available.
- **Adding a brand-new MAV_CMD: no, not without touching core.** The entry must land in the
  **base** list, because `_collapseHierarchy` only creates new `MissionCommandUIInfo` objects when
  `baseList == true`, and that is true only for the `FirmwareClassGeneric` by
  `VehicleClassGeneric` cell, which resolves to `src/MissionManager/MavCmdInfoCommon.json`. The
  header comment states explicitly that override lists cannot add new commands.

Whether a custom build can **shadow** the existing `:/json/MavCmdInfoCommon.json` resource alias —
plausible given the alias-based resource override mechanism documented in
[plugin-architecture.md](plugin-architecture.md) — was **not verified**. It is recorded as `OPEN`
in [open-questions.md](open-questions.md) because it determines whether Aerora payload commands can
appear in the Plan UI without a core edit.

## Extensibility summary

| Concern | Extensible from an overlay? |
| --- | --- |
| New complex pattern — class, editor, map visual, save/load, picker entry, plan template | **Yes** — `complexMissionItemNames()` + `createComplexMissionItem()` + `planCreators()`; worked example in `custom-example/` |
| Pattern gets survey-style insert behaviour | **No** — hardcoded `qobject_cast` chain, `MissionController.cc:444-480`, `513`, `527` |
| New landing pattern | **No** — hardcoded canonical names, `MissionController.cc:378-385` |
| Extra data in the `.plan` file | **Yes** — six `preSave`/`postSave`/`preLoad`/`postLoad` plugin hooks |
| Non-camera payload action on a simple item | **Partially** — implement the `Section` base; no payload-agnostic abstraction exists |
| Spacing and trigger distance for a non-camera transect payload | **Yes** — the manual-camera `CameraCalc` path, by design |
| Override MAV_CMD UI metadata | **Yes** — custom FirmwarePlugin plus override JSON |
| Add a brand-new MAV_CMD to the Plan UI | **No** — must be in base `MavCmdInfoCommon.json`, or via an unverified resource-alias shadow |
| Commands preserved across mission resume | **No** — `includedResumeCommands` is a core allow-list |
| Mission transfer protocol | Core only — `PlanManager` is not plugin-extensible |
| Round-trip reconstruction from a downloaded mission | **No** for non-landing patterns, unless Aerora implements `scanForItems` |

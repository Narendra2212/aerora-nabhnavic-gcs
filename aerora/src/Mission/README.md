# Aerora Mission System

How Aerora attaches to QGroundControl's mission system, what it deliberately reuses unchanged, what
it deliberately does not do, and what the overlay root must provide for this subtree to build.

Everything in this document was read from this repository at QGC `HEAD 9459a4b5`. Where an earlier
phase document and the current source disagree, the source wins and the difference is recorded under
[Corrections to earlier phases](#corrections-to-earlier-phases).

## Status

| Item | Status |
| --- | --- |
| QGC mission architecture | `SOURCE-INSPECTED` — read at `9459a4b5`, cited below |
| Integration seam selection | `SOURCE-INSPECTED` — no core edit required |
| Source in this subtree | `IMPLEMENTED-UNCOMPILED` |
| Tests in this subtree | `IMPLEMENTED-UNCOMPILED` |
| QGC core modifications by this workstream | **NONE** |
| `includedResumeCommands` | **NOT MODIFIED** |
| GCS-side payload state replay | **NOT IMPLEMENTED** — asserted, not merely absent |
| Payload state persistence across mission interruption | `NOT-EVIDENCED` — unchanged |
| Wiring into `aerora/CMakeLists.txt` | **DONE by M-Plugin** — aggregated and both tests registered; verified by reading that file, not by building |
| `AeroraMissionBridge::attachTo()` caller | **DONE by M-Vehicle** — called from `AeroraFirmwarePlugin::attachDomainServices()`; contract verified by reading, not by building |
| Aerora complex mission items | `CONTRACT-ONLY` — seam exists, no item implemented (Q-021). Integration contract: [coverage-mission-contract.md](coverage-mission-contract.md) |
| Coverage mission integration approach | `DESIGNED` — [coverage-mission-contract.md](coverage-mission-contract.md) |
| Mission validation result model | `IMPLEMENTED-UNCOMPILED` — `AeroraMissionValidationReport`, three outcomes, no boolean |
| Mission validation pass | `IMPLEMENTED-UNCOMPILED` — `AeroraMissionValidator`, 5 checks performable, 4 reported unperformable |
| Operating limits used by validation | **NONE.** No spacing, altitude, geofence or turn-radius threshold exists in this subtree |
| Aerora mission command set | `OPEN / NOT-EVIDENCED` — Q-023 |
| Mission verification depth | `OPEN` — Q-041; depth is reported, not chosen |
| Aerora mission status beyond the standard index | `OPEN / NOT-EVIDENCED` — no dialect |
| Mission pause semantics | `OPEN` — Q-007, untouched here |

**Nothing here has been compiled.** This host has no compiler, no CMake, no Ninja and no Qt, so no
build or test has run. Verification that did happen is listed under
[What was actually verified](#what-was-actually-verified).

## What this layer is

A small, read-only Aerora view of one vehicle's mission situation, plus the seams a product family
uses to contribute mission content. It is an **adapter over QGroundControl's mission system, not a
mission engine**.

It contains no painting, firefighting, agriculture or inspection behaviour, because a product family
is a profile rather than the architecture. There is deliberately no enumeration of families anywhere
in this subtree.

### What it is not

| Not | Why |
| --- | --- |
| A second mission engine | QGC already has a complete one. Building a parallel engine would duplicate the protocol state machine, the item model and the plan file format, and the two would drift |
| A command path | `AeroraMissionObserver` has no upload, download, clear, resume, set-current or start method. Commanding stays in `PlanMasterController` / `MissionController` / `MissionManager` |
| Aircraft safety authority | Mission state reporting is not failsafe authority. Nothing here can decide RTL, LAND, HOLD, DISARM or termination, because nothing here can send anything |
| Autonomy decision authority | The companion proposes, the flight controller decides, the ground station observes. This module is on the observing side |
| A telemetry state model | REPORTED / EFFECTIVE / REQUESTED / PROPOSED / STALE / UNKNOWN / UNAVAILABLE belong to `AeroraTelemetryState`. This module reaches no freshness verdict |
| A payload controller | No pump, nozzle, actuator, payload state or payload replay. See [Mission ↔ payload boundary](#mission--payload-boundary) |
| A mission UI | No QML. The future UI workstream owns presentation; this module provides the model |

## QGroundControl's mission architecture

Source-inspected. Class and member names are exact as they exist now, not as earlier phase documents
describe them.

```text
Operator
   │
   ▼
PlanMasterController                    src/MissionManager/PlanMasterController.{h,cc}
   │  owns MissionController, GeoFenceController, RallyPointController BY VALUE (.h:184-186)
   │  _controllerVehicle (offline editing vehicle, .h:180)  vs  _managerVehicle (.h:181)
   │  sequences transfers: SyncSequence { Idle, Mission, GeoFence, RallyPoints, InitialPlanLoad }
   │
   ▼
MissionController                       src/MissionManager/MissionController.{h,cc}
   │  GCS-LOCAL PLAN = _visualItems (QmlObjectListModel of VisualMissionItem, .h:412)
   │  _convertToMissionItems()  .cc:205-234   flattens plan -> QList<MissionItem*>
   │  sendItemsToVehicle()      .cc:245-256
   │
   ▼
MissionManager : PlanManager            src/MissionManager/MissionManager.{h,cc}
   │                                    src/MissionManager/PlanManager.{h,cc}
   │  VEHICLE MISSION = _missionItems (QList<MissionItem*>, PlanManager.h:139)
   │  upload staging  = _writeMissionItems (PlanManager.h:140)
   │  ACTIVE INDEX    = _currentMissionIndex / _lastCurrentIndex
   │
   ▼
MAVLink mission protocol (v2)
   │  out: MISSION_COUNT          PlanManager.cc:100-124
   │       MISSION_REQUEST_LIST   PlanManager.cc:145-167
   │       MISSION_REQUEST_INT    PlanManager.cc:352-377
   │       MISSION_ITEM_INT       PlanManager.cc:491-559
   │       MISSION_ACK            PlanManager.cc:293-318
   │       MISSION_CLEAR_ALL      PlanManager.cc:874-897
   │  in:  MISSION_COUNT / MISSION_ITEM_INT / MISSION_REQUEST(_INT) / MISSION_ACK
   │                              PlanManager.cc:648-669
   │       MISSION_CURRENT        MissionManager.cc:269-275
   │       HIGH_LATENCY(2).wp_num MissionManager.cc:255-268
   │       HEARTBEAT              MissionManager.cc:276-285
   ▼
Flight controller
```

`PlanManager` is the shared protocol engine for all three plan types; `MissionManager` is the
`MAV_MISSION_TYPE_MISSION` specialisation that additionally tracks the current index.

### Item model

```text
QObject
└── VisualMissionItem                   abstract; appendMissionItems() is the flatten seam (.h:195)
    ├── SimpleMissionItem
    │   └── TakeoffMissionItem
    └── ComplexMissionItem              patternName(), load(), minAMSLAltitude() ...
        ├── MissionSettingsItem         always _visualItems index 0
        ├── StructureScanComplexItem
        ├── TransectStyleComplexItem
        │   ├── SurveyComplexItem
        │   └── CorridorScanComplexItem
        └── LandingComplexItem
            ├── FixedWingLandingComplexItem
            └── VTOLLandingComplexItem
```

`MissionItem` (`src/MissionManager/MissionItem.{h,cc}`) is **not** in this hierarchy. It is the flat
MAVLink record — sequence, command, frame, seven parameters, autoContinue, isCurrentItem — and it is
what `PlanManager` stores and transmits. The plan/vehicle boundary is exactly the boundary between
`VisualMissionItem` and `MissionItem`.

### Command declaration

`MissionCommandTree` (`src/MissionManager/MissionCommandTree.{h,cc}`) collapses a two-axis hierarchy
of firmware class × vehicle class from `MavCmdInfo*.json`. Override precedence is
`_buildAllCommands()` `.cc:84-130` via `_collapseHierarchy()` `.cc:62-83`, and the extension point is
`FirmwarePlugin::missionCommandOverrides()` (`FirmwarePlugin.h:313`, base `.cc:94-113`).

One constraint worth knowing before planning an Aerora dialect: **an override file cannot add a new
command**, only override an existing one. `_collapseHierarchy()` logs
`"Skipping override with no base command info"` and discards it. Only the generic base list may
introduce commands.

## What Aerora reuses, extends, adapts, and writes new

| QGC provides | Aerora posture | Mechanism |
| --- | --- | --- |
| `PlanMasterController` transfer sequencing, dual dirty state, plan file format | **Reused unchanged** | — |
| `MissionController` item model, flattening, plan-view state, load/save | **Reused unchanged** | — |
| `PlanManager` / `MissionManager` protocol state machine, retries, acks | **Reused unchanged** | — |
| Mission resume generation | **Reused unchanged, and not modified** | See [Mission resume](#mission-resume) |
| `MissionItem` wire record | **Reused unchanged** | — |
| `ComplexMissionItem` pattern | **Extended** | `AeroraMissionExtension` + `QGCCorePlugin` factory seam |
| `FirmwarePlugin::supportedMissionCommands()` | **Extended** | `AeroraMissionExtensionRegistry::declaredMissionCommandSet()` — blocked on Q-023 |
| Mission state for an operator interface | **Adapted** | `AeroraMissionObserver` + `AeroraMissionBridge` |
| Plan-versus-vehicle comparison | **New** | `AeroraMissionSnapshot` / `AeroraMissionAgreement` |
| Index provenance | **New** | `AeroraMissionIndex` |
| Statement of resume behaviour | **New** | `AeroraMissionResumeBoundary` |

The four "new" rows are the entire Aerora mission footprint. Each exists because QGC genuinely has
no equivalent, not because Aerora will eventually have several products.

## Files

| File | Purpose |
| --- | --- |
| `AeroraMissionIndex.{h,cc}` | A sequence number plus its origin. Makes index confusion a compile error |
| `AeroraMissionSnapshot.{h,cc}` | Flattened mission fingerprint, and plan-versus-vehicle comparison at a stated depth |
| `AeroraMissionObserver.{h,cc}` | Read-only per-vehicle mission state. No command API at all |
| `AeroraMissionBridge.{h,cc}` | The only file here that touches `Vehicle` / `MissionManager`. Feeds the observer |
| `AeroraMissionResumeBoundary.{h,cc}` | What resume carries and does not carry, as assertable facts |
| `AeroraMissionExtension.{h,cc}` | Product-family mission seam and its registry. No extension implemented |
| `AeroraMissionValidationReport.{h,cc}` | Check and report value types. Three outcomes, and deliberately no `isValid()` boolean |
| `AeroraMissionValidator.{h,cc}` | The validation pass. Nine named checks, five performable, no invented threshold |
| `AeroraMissionLog.{h,cc}` | `Aerora.Mission` logging category |
| `Mission.cmake` | Variable-only source manifest. Defines no target and no `PARENT_SCOPE` |
| [`coverage-mission-contract.md`](coverage-mission-contract.md) | Coverage-mission integration contract, reuse analysis, and the payload boundary |

Under `aerora/test/Mission/`:

| File | Purpose |
| --- | --- |
| `AeroraMissionMock.{h,cc}` | **MOCK / TEST ONLY** deterministic driver |
| `AeroraMissionTest.{h,cc}` | Architectural-property tests, `Unit MissionManager` |
| `AeroraMissionSyncTest.{h,cc}` | Bridge against a real `Vehicle`, `Integration Vehicle MissionManager` |

Only `AeroraMissionBridge` depends on QGroundControl's mission classes. That is what lets everything
else be unit-tested without a vehicle, a link or an event loop, and what lets the mock drive exactly
the surface a real vehicle drives.

## Plan, vehicle mission, active state

The three are distinct in QGC already, but they are all a `QList<MissionItem*>` or a plain `int`, so
nothing in the type system objects to confusing them.

```text
Operator
   ↓
MISSION PLAN              MissionController::_visualItems          AeroraMissionSnapshot::GcsPlan
   ↓  flatten (one-way)   _convertToMissionItems()
REQUESTED MISSION         PlanManager::_writeMissionItems          (upload staging, transient)
   ↓  upload / replace    MISSION_COUNT + MISSION_ITEM_INT
VEHICLE MISSION           PlanManager::_missionItems               AeroraMissionSnapshot::VehicleStore
   ↓
ACTIVE INDEX              PlanManager::_currentMissionIndex        AeroraMissionIndex::VehicleReported
```

`AeroraMissionSnapshot::Origin` tags which side a capture came from, and
`AeroraMissionSnapshot::compare()` requires one `GcsPlan` and one `VehicleStore`. Comparing a plan
against itself returns `NotCompared` with a warning rather than `Matches`, because a false agreement
is the worst available failure on this path.

### The plan file remains authoritative

`AERORA-MISSION-REQ-001`. Four source-inspected reasons the vehicle store cannot be:

| Fact | Consequence |
| --- | --- |
| `MissionItem` has no vendor-extension slot | Aerora metadata cannot round-trip through the vehicle |
| Complex items flatten to primitive items once, on upload (`MissionController.cc:205-234`) | A download is the flattened form; the structure is gone |
| Resume replaces the whole vehicle mission and resets the index | The store after a resume is not the planned mission |
| A failed upload discards the write list with no rollback (`PlanManager.cc:845-848`) | The store can be in a state no plan describes |

The last one is modelled explicitly: `AeroraMissionObserver::isVehicleMissionIndeterminate()` is set
by a failed upload and cleared only by a completed download or a successful upload. An indeterminate
store is not compared against anything.

### Flattening is one-way, and reconstruction is refused

`AERORA-MISSION-REQ-002`. Upstream reconstructs complex items from a download only for landing
patterns, through `LandingComplexItem::_scanForItems()` (`.cc:433-455`) and its function-pointer
bindings. Survey, corridor scan and structure scan round-trip through the plan file alone.

An Aerora scanner is rejected: a flat item sequence that happens to match a pattern is not evidence
that the pattern is what the operator planned. Nothing in this subtree reconstructs anything.

## Mission current index

Three different indices exist, all `int` upstream:

| Index | Upstream | Origin tag | Authoritative? |
| --- | --- | --- | --- |
| Vehicle execution position | `MissionController::currentMissionIndex` (`.h:68`), from `MISSION_CURRENT` | `VehicleReported` | **Yes** |
| Editor selection | `MissionController::currentPlanViewSeqNum` (`.h:70`) | `PlanViewSelection` | No — never transmitted |
| Resume start proposal | `MissionController::resumeMissionIndex` (`.h:69`) | `ResumeCandidate` | No — a ground-station computation |

`AeroraMissionIndex` carries the origin with the number. There is no constructor taking a bare
sequence number, no API that changes an existing value's origin, and no arithmetic, so an editor
selection cannot become a vehicle report by assignment, increment or copy.
`isVehicleAuthoritative()` is true only for `VehicleReported`.

### The -1 sentinel is a real state

A successful mission write sets both indices to -1 and emits `currentIndexChanged(-1)`
(`PlanManager.cc:832-838`) **before** the vehicle has reported anything about the new mission. So
"attributed to the vehicle, carrying no position" is a distinct state from "nothing observed":
`isAttributed()` is true while `isKnown()` is false. A consumer must render neither as item 0.

### What arrives, and what does not

`MISSION_CURRENT` → `MissionManager::_handleMissionCurrent` (`.cc:269-275`) →
`_updateMissionIndex` (`.cc:235-254`) → `currentIndexChanged`. Also fed by
`HIGH_LATENCY`/`HIGH_LATENCY2` `wp_num`.

`_lastCurrentIndex` is **not** updated directly. The candidate is cached and promoted only on the
next `HEARTBEAT` while in mission flight mode (`_handleHeartbeat`, `.cc:276-285`), because an
RTL/`DO_LAND_START` index jump also changes flight mode and `MISSION_CURRENT` can arrive first. This
module does not reproduce that logic; it consumes the signals.

Exposed three ways upstream: `MissionController.currentMissionIndex`, the `missionItemIndex` **Fact**
on `VehicleFactGroup` (fed by `Vehicle::_updateMissionItemIndex`, `Vehicle.cc:2647-2658`), and
`PlanManager::currentIndex()` directly.

**Mission item completion is never inferred** (`AERORA-MISSION-REQ-006`). The observer reports the
index the vehicle reports and nothing else. There is no elapsed-time or position-based completion
anywhere in this subtree — and there could not be, because `MISSION_ITEM_REACHED` is not handled
upstream either (see [Corrections](#corrections-to-earlier-phases)).

## Mission synchronisation

Source-inspected, reused unchanged. No conflict-resolution rule is invented.

| Operation | Entry point | Wire | Completion |
| --- | --- | --- | --- |
| Download | `PlanMasterController::loadFromVehicle` `.cc:203` → `MissionController::loadFromVehicle` `.cc:171` → `PlanManager::loadFromVehicle` `.cc:125` | `MISSION_REQUEST_LIST`, then `MISSION_REQUEST_INT` per item | `newMissionItemsAvailable(bool)` |
| Upload | `PlanMasterController::sendToVehicle` `.cc:319` → `MissionController::sendToVehicle` `.cc:183` → `PlanManager::writeMissionItems` `.cc:59` | `MISSION_COUNT`, then `MISSION_ITEM_INT` per request | `sendComplete(bool error)` |
| Clear | `removeAllFromVehicle` `.cc:524` → `PlanManager::removeAll` `.cc:898` | `MISSION_CLEAR_ALL` | `removeAllComplete(bool error)` |
| Resume | `MissionController::resumeMission` `.cc:1854` → `MissionManager::generateResumeMission` `.cc:71` | `MISSION_COUNT` + items | `sendComplete`, plus `resumeMissionReady` / `resumeMissionUploadFail` |

Acks, retries and timeouts are `PlanManager`'s: `AckType_t` (`.h:84-91`), `_ackTimeout()`
`.cc:168-243`, `_ackTimeoutMilliseconds = 1500`, `_retryTimeoutMilliseconds = 250`,
`_maxRetryCount = 5`. Nine `ErrorCode_t` values reach a single `error(int, QString)` signal.

Local dirty state is `PlanMasterController`'s two independent flags, `_dirtyForSave` and
`_dirtyForUpload` (`.h:191-192`). Save-dirty implies upload-dirty but not the reverse; a file load
sets `(false, true)`; a vehicle download sets `(false, false)`. This module does not duplicate them.

### Two limits on what is observable from outside

Both are properties of upstream's interface, recorded because they shape the observer's state model.

**The in-flight transaction kind is not observable.** `PlanManager` keeps `_transactionInProgress` in
a protected member and exposes only a boolean `inProgress()`. So
`AeroraMissionObserver::TransferActivity` has `InProgress` with no kind. The kind becomes known at
completion, because each operation has its own completion signal.

**A failed download is not distinguishable from an empty one by its completion signal.**
`_finishTransaction()` emits `newMissionItemsAvailable(false)` on both success and failure, with no
error flag, and clears the item list on failure (`PlanManager.cc:822-828`). The only distinguishing
evidence is an `error()` signal during the same transaction, so the bridge latches it and the
observer uses the latch. Where no error was reported, the observer reports success with an empty
mission rather than guessing.

### Mismatch detection

`AeroraMissionSnapshot::compare()` at one of three depths. Verification depth is **Q-041 and is not
decided here**; the depth travels with the verdict so that "matches" is never read as more than it is.

| Depth | Catches | Cost |
| --- | --- | --- |
| `DepthItemCount` | Truncation | Negligible |
| `DepthCommandSequence` | + reordering | Negligible |
| `DepthFullParameters` | + an altered parameter | Negligible |
| *(read-back and compare)* | + a vehicle-side storage fault | A full download per upload — **not implemented**, Q-041 |

`Verdict::NotCompared` is the initial state and the honest answer for an upload nobody checked. It is
never `isVerified()`, which is what `AERORA-MISSION-REQ-007` requires. A verdict is invalidated
whenever either side changes, so a verified claim cannot outlive the mission it described.

#### Comparison happens at wire precision

A plan parameter is a full-precision `double`. The same parameter back from the vehicle has been
through `MISSION_ITEM_INT`, which is lossy in two different ways, both read from the pack and unpack
sites:

- `param1`–`param4` and `param7` travel as `float`.
- `param5`/`param6` travel in `int32` fields — scaled by 1e7 for every frame except
  `MAV_FRAME_MISSION`, where they are placed unscaled and quantise to whole units
  (`PlanManager.cc:545-553`, `_handleMissionItem` `.cc:418-425`).

Comparing raw doubles would report a difference for almost every mission that round-tripped
correctly, which would make verification useless. `compare()` normalises both sides through the same
transform first, including the **truncating** `double`→`int32` conversion upstream actually performs
rather than a rounding one. A difference below wire precision is not observable on the vehicle and is
not reported as a difference.

## Mission replacement

Source-inspected, preserved, not modified.

| Element | Behaviour |
| --- | --- |
| Old mission on the vehicle | No `MISSION_CLEAR_ALL` is sent. The overwrite is implicit in `MISSION_COUNT` |
| Upload | `MISSION_COUNT`, then one `MISSION_ITEM_INT` per `MISSION_REQUEST(_INT)` |
| `current` flag on the wire | Set for sequence 0 only — `missionRequestSeq == 0` (`PlanManager.cc:545`). `MissionItem::isCurrentItem` is never transmitted |
| Acknowledgement | `MISSION_ACK` / `MAV_MISSION_ACCEPTED` with an empty write list ends the write (`.cc:596-608`) |
| Current index after success | Reset to -1, both current and last-current, with both signals emitted (`.cc:832-838`) |
| GCS state after success | `_writeMissionItems` is promoted into `_missionItems` (`.cc:839-843`) |
| GCS state after failure | Write list discarded; `_missionItems` keeps **stale pre-upload content**; no rollback is attempted (`.cc:845-848`) |
| `MISSION_SET_CURRENT` | Not sent as part of a replacement, and not sent by resume |

The failure row is why `isVehicleMissionIndeterminate()` exists. Leaving the previous snapshot
standing after a failed upload would present a mission the vehicle may no longer hold.

`Vehicle::setCurrentMissionSequence()` (`.cc:2111-2143`, `MAV_CMD_DO_SET_MISSION_CURRENT` with a
deprecated `MISSION_SET_CURRENT` fallback) has exactly one caller:
`GuidedActionsController.qml:644`, the operator's "set current waypoint" action. It is not used by
resume and not used after an upload. This module does not call it.

## Mission resume

**The most sensitive area in this workstream, and the one with the least Aerora code in it.**

### What upstream does

`MissionManager::generateResumeMission(int)`, `MissionManager.cc:71-211`.

1. Return if the vehicle is the offline editing vehicle (`:73`).
2. Return **silently** — `qCDebug` only — if a transaction is in progress (`:78-81`).
3. Scan the whole mission; any `MAV_CMD_DO_JUMP` aborts with an application message (`:83-89`).
4. Clamp the resume index into range (`:91`).
5. Snap the index backwards to the last command the vehicle flies through, i.e. the last
   `specifiesCoordinate() && !isStandaloneCoordinate()` (`:93-106`).
6. Keep an item when **any** of four conditions holds (`:130-143`) — there is no `else`:
   - `i == 0 && addHomePosition`
   - `i >= resumeIndex` — everything at or after the resume point, untouched
   - `includedResumeCommands.contains(command)` — the replay prefix
   - the command is a takeoff command
7. De-duplicate the prefix, latest-wins, removing one-shot actions (`:145-195`).
8. Renumber densely from 0 (`:196-200`) and set the current item flag on index
   `addHomePosition ? 1 : 0` (`:201-202`) — **GCS-internal only; never transmitted**.
9. Set `_resumeMission` and call `_writeMissionItemsWorker()` directly (`:204-210`), bypassing
   `writeMissionItems()` and therefore its home-skip and `DO_JUMP` fix-ups.

An unlisted command **before** the resume index is omitted with no log line, no warning, no counter
and no operator notification.

The de-duplication rules are the clearest evidence of intent: latest-wins for camera mode and region
of interest, one shared slot across five camera start/stop commands, and image capture with a
non-zero third parameter deleted as a non-time-triggered one-shot. This is **state restoration**, not
command replay — the mechanism exists because QGC assumes the vehicle retains nothing across a
mission replacement.

### `includedResumeCommands`

| Question | Answer |
| --- | --- |
| Where defined | `src/MissionManager/MissionManager.cc:110`, populated `:112-126` |
| What kind of thing | A plain **function-local automatic** `QList<MAV_CMD>`. Not static, not const, not a member, not reachable from outside the function |
| Where consumed | One site: `.contains()` at `MissionManager.cc:134`. No other reference exists in the tree |
| Which commands | Exactly 14: `DO_CONTROL_VIDEO`, `DO_SET_ROI`, `DO_DIGICAM_CONFIGURE`, `DO_DIGICAM_CONTROL`, `DO_MOUNT_CONFIGURE`, `DO_MOUNT_CONTROL`, `DO_SET_CAM_TRIGG_DIST`, `DO_FENCE_ENABLE`, `IMAGE_START_CAPTURE`, `IMAGE_STOP_CAPTURE`, `VIDEO_START_CAPTURE`, `VIDEO_STOP_CAPTURE`, `DO_CHANGE_SPEED`, `SET_CAMERA_MODE` |
| Effect on resume | A listed command before the resume index is replayed at the front of the generated mission; an unlisted one is omitted |
| Does Aerora need an extension | **Not answerable yet.** It would require an evidenced Aerora payload mission command, which is Q-023 and `NOT-EVIDENCED` |
| **Modified?** | **NO** |

There is no seam: `generateResumeMission` is not virtual, `MissionManager` declares no virtual
members, `Vehicle` constructs the concrete class (`Vehicle.cc:243`), and no core-plugin or
firmware-plugin hook exists on the path. Changing the filter means editing QGC core and carrying it
against upstream indefinitely, on a path with no upstream test coverage for vendor commands because
upstream has no vendor commands. `IF-MIS-016` carries nothing, by decision (ADR-005).

Notably **absent** from the list, and absent upstream: every actuator and payload-actuation command,
every `DO_GIMBAL_MANAGER_*` command, and every region-of-interest variant other than the legacy
`DO_SET_ROI`. The last one is worth knowing on its own: a plan built with `DO_SET_ROI_LOCATION` loses
its region of interest across a resume.

### What Aerora adds, and why it is only a statement

`AeroraMissionResumeBoundary` mirrors the 14 commands read-only and exposes three things: the set,
predicates over it, and an analysis of which items before a resume index would not be carried
forward. It changes nothing.

The mirror exists because there is no other way to answer "what will a resume re-establish" from
outside the function — needed for the operator warning that `AERORA-MISSION-REQ-004` requires — and
because it lets a test assert that no Aerora or payload command has been smuggled into the set.

Two prohibitions are pinned as assertable constants rather than left as inspections, which is the
conversion R3-007 asks for:

```cpp
static constexpr bool kGcsPayloadStateReplayImplemented = false;   // IF-MIS-015 carries nothing
static constexpr bool kPayloadStatePersistenceEvidenced = false;   // IF-MIS-014, evidence item 14
```

`notCarriedForwardBeforeResumeIndex()` is named for what it reports. For a navigation item before the
resume point the omission **is** the purpose of resuming; for a state-setting item it is the exposure
recorded as R-002. The function reports the fact and classifies neither, because choosing what to
show an operator is presentation, and because the payload-specific classification it would need is
blocked on Q-023.

### What this module does not infer

```text
Mission resume   ≠   Payload-state replay
```

- The payload controller is **not** assumed to remember pump, nozzle or actuator state across a
  mission interruption. No baseline document states retention, the firmware payload elements are
  placeholders, and no bench or flight evidence has been produced. Ownership of payload state is not
  retention of it.
- The ground station does **not** replay payload commands. There is no payload code path in this
  module to do it through.
- `resumeMissionReady()` is not consumed here. It has no consumer anywhere in the tree (see
  [Upstream defects](#upstream-defects-on-the-resume-path)), and inventing one would be deciding that
  a successful resume upload should auto-start a mission — an aircraft authority decision this
  module must not make.

### The plan constraint remains blocked

The mitigation recorded in `AERORA-MISSION-REQ-003`'s rationale — constrain plans so a resume cannot
leave payload state ambiguous — **cannot be implemented yet**. It requires knowing which commands set
Aerora payload state, and no Aerora payload mission command is evidenced (Q-023).

What is implementable now, and is implemented, is the payload-agnostic half: reporting which items
before a resume index are not carried forward. A consumer that wants only the state-setting subset
must filter using `MissionCommandTree::getUIInfo()` to tell a coordinate command from an action
command. Marking this `BLOCKED` rather than approximating it is deliberate: a plan check driven by a
guessed payload command set would give operators false assurance.

### Upstream defects on the resume path

Recorded because they affect anyone touching this code. None blocks this module; none is worked
around here.

| Defect | Location |
| --- | --- |
| `_missionItems[resumeIndex]` indexed with no empty-list guard | `MissionManager.cc:94` |
| The current-item write is unguarded | `MissionManager.cc:202` |
| The locally built resume items are never deleted — one leak per resume | `MissionManager.cc:71-211` |
| A resume requested during a transaction is dropped with only a debug log | `MissionManager.cc:78-81` |
| The `DO_JUMP` refusal reports only through an application message, emitting no signal | `MissionManager.cc:83-89` |
| `resumeMissionReady()` has no consumer anywhere in the tree | Searched; none |

The fourth is the one most likely to be seen in the field: an operator who triggers resume during an
in-flight transaction gets no feedback at all.

## Product-profile boundary

```text
Generic mission infrastructure          this subtree — knows no product family
      │
      ├── Painting profile              not implemented
      ├── Firefighting profile          not implemented
      ├── Agriculture profile           not implemented
      └── Inspection profile            not implemented
```

An `AeroraMissionExtension` declares its own id, the complex mission item names it provides, a
factory for them, and its mission command set. The generic layer knows only the interface, so a new
family is a new file rather than a change to shared code. There is no `if (painting)` anywhere in
this subtree, and no enumeration of families — adding one would reintroduce the conditional the
interface exists to avoid.

Registration is explicit rather than by static self-registration, matching
`AeroraProductProfileRegistry`: a set assembled by static-initialisation order would be unordered and
unreviewable. **Ambiguity is refused at registration**, not resolved later — two extensions claiming
one complex item name is rejected when the second registers, so the outcome never depends on link
order.

**No extension is registered in this build**, and that is correct rather than incomplete. Which
complex items Aerora needs is Q-021; their geometry and parameters are product definition and Q-023.

### The empty-list trap

`FirmwarePlugin::supportedMissionCommands()` treats an **empty** returned list as "every command is
supported". `AERORA-MISSION-REQ-005` requires Aerora to declare its set explicitly instead, because
an empty list offers the operator commands the firmware may not implement.

So the registry distinguishes "declares nothing" from "declares an empty set":
`isMissionCommandSetDeclared()` is true only when some extension returned a non-empty set. A caller
must check it before using `declaredMissionCommandSet()`, and must not pass an empty set through.

While it is false — which it is in this build — the Aerora firmware plugin must **leave upstream's
behaviour in place rather than ship a guessed list**. `AeroraMissionTest::_missionCommandSetIsNotDeclared`
pins that restraint so it is checked rather than intended.

## Mission ↔ payload boundary

```text
Mission plan
   ↓  references a payload action
Payload action request
   ↓
Payload controller            owns payload state, interlocks and payload-local safety
```

not

```text
Mission  →  GCS directly controls pump / nozzle
```

This module implements no payload protocol, defines no payload command, stores no payload state and
replays nothing. The generic attachment point for a future payload service is
`AeroraVehicleServices::attachService()`, which already exists and is the vehicle workstream's.

## Mission ↔ vehicle boundary

The vehicle workstream owns `Vehicle` integration. This module consumes it and modifies nothing:

| Consumed | From |
| --- | --- |
| `AeroraVehicleServices::forVehicle()` / `attachService()` | `aerora/src/Vehicle/AeroraVehicleServices.h` |
| `Vehicle::missionManager()` | `src/Vehicle/Vehicle.h:574` |
| `MissionManager` signals | `src/MissionManager/PlanManager.h:67-77` |

`AeroraMissionBridge::attachTo()` requires `AeroraVehicleServices` to already be attached, which the
Aerora firmware plugin does from `FirmwarePlugin::initializeVehicle()`. Creating the services object
from here would take a lifecycle decision owned by the vehicle layer, so `attachTo()` declines and
logs instead. The consequence, stated plainly: **mission observation is present only on vehicles
routed to the Aerora firmware plugin.**

### How the composition root consumes this contract

`AeroraFirmwarePlugin::attachDomainServices()` calls `AeroraMissionBridge::attachTo(vehicle)` once,
last in its dependency-ordered composition. Verified by reading, at `9459a4b5` plus the overlay
working tree:

| Property | Evidence |
| --- | --- |
| Called exactly once per vehicle | One call site. Its caller `initializeVehicle()` is invoked once per `Vehicle` construction (`Vehicle.cc:161`, `:209`) |
| `MissionManager` exists first | `_missionManager` is built in `_commonInit()` (`Vehicle.cc:243`), which completes before `initializeVehicle()` runs. `attachTo()` null-checks it anyway |
| Observer is per vehicle | Created inside `attachTo()`, then `setParent()`ed to the per-vehicle `AeroraVehicleServices` by `attachService()` |
| No shared observer | `AeroraFirmwarePlugin` is a per-firmware-class singleton, but holds **no data members at all** and `attachDomainServices()` is `static`. The Q4-002 sharing hazard therefore cannot reach mission state |
| Lifetime | `Vehicle` → services → observer → bridge. Destroyed with the vehicle; no teardown code |
| Repeated attach is safe | Three independent guards: the composition root's telemetry-service check, `AeroraVehicleServices::attachTo()`, and `AeroraMissionBridge::attachTo()` |

Two consequences of that composition, neither a defect, both worth knowing:

**The offline editing vehicle carries no mission observer.** The composition root returns before
`attachDomainServices()` when `Vehicle::isOfflineEditingVehicle()` is true. That is correct for
mission: `PlanManager::writeMissionItems()` and `loadFromVehicle()` both early-return for that
vehicle, so there would be no transfer to observe and no index to report. But it means
`observerFor()` returns nullptr for `PlanMasterController::_controllerVehicle`, which is the
`_managerVehicle` whenever the Plan view is offline. A plan-side consumer must null-check rather
than assume — see [integration point 5](#5-plan-side-reporting-for-the-ui-workstream).

**Mission attachment is sequenced behind telemetry attachment succeeding.** The composition root
returns early if the telemetry service fails to attach, which would skip the mission bridge.
Mission has no dependency on telemetry — the only mention of `AeroraTelemetryState` anywhere in
this module is a doc comment saying it owns a model this module does not restate — so the coupling
is incidental to composition order rather than real. It is also unreachable in the current code,
because `attachService()` refuses only an empty name, a null service or a duplicate name, and none
of the three can occur at that call site. Recorded as latent ordering fragility, owned by the
vehicle workstream, not as a contract mismatch.

## Mission ↔ telemetry boundary

The telemetry workstream owns the value state model. This module does **not** restate it:

| Telemetry owns | This module |
| --- | --- |
| `Class { Unknown, Unavailable, Reported, Effective, Requested, Proposed, Stale }` | Defines none of these |
| Freshness thresholds and verdicts | Reaches no freshness verdict |
| `AeroraTelemetrySource` attribution | Does not attribute values |

What this module models is a different question: whether a transfer is happening, what the last one
did, and whether two missions agree. No freshness verdict should be inferred from
`TransferActivity`, `LastOutcome` or `Verdict`. A consumer that needs a value's state class must use
`AeroraTelemetryState::resolve()`.

The one anti-collapse guarantee this module makes is its own: index provenance, where an editor
selection can never decay into a vehicle report.

## No mission state machine was invented

A full lifecycle enum — `IDLE / PLANNING / UPLOADING / EXECUTING / PAUSED / RESUMING / FAILED /
COMPLETED` — was considered and **rejected**, because most of its states have no evidence:

| Candidate state | Evidence |
| --- | --- |
| `UPLOADING` / `DOWNLOADING` as distinct in-flight states | **None observable.** `PlanManager` exposes only a boolean `inProgress()`; the kind is a protected member |
| `EXECUTING` | No mission-execution state is reported. The vehicle reports an index, not a state |
| `PAUSED` | `OPEN` — Q-007. Mission pause goes through the flight-mode path and no Aerora pause mode is confirmed |
| `RESUMING` | Not observable. A resume is an ordinary write transaction on the wire |
| `COMPLETED` | Not reported. Completion is never inferred (`AERORA-MISSION-REQ-006`) |

What was implemented instead is three small enumerations over facts that *are* observable —
`TransferActivity`, `LastOperation`, `LastOutcome` — each with an explicit `NotEvidenced` member so
that "nothing has been observed" is a value rather than a default that reads as a state. Everything
else is left `OPEN`.

## Mission validation

`AeroraMissionValidationReport` + `AeroraMissionValidator`. `P7-CHK-002`, `P7-CHK-004`.

### Why upstream's gate was not enough

QGroundControl already has a mission readiness gate, and it is a different shape:
`VisualMissionItem::readyForSaveState()` returns one of three per-item values, and
`MissionController::readyForSaveState()` (`MissionController.cc:876-884`) returns the **first**
non-ready item's state and stops.

That answers "may this be saved". It cannot answer what was checked, what passed, or what could
not be checked — and the third is the requirement. A check that silently did not run is
indistinguishable from a check that passed, which is the false assurance `P7-CHK-002` exists to
prevent.

### Three outcomes, and no boolean

```text
Outcome::NotPerformed = 0     ← the default, so an unstated check is not a passed one
Outcome::Passed
Outcome::Failed
```

```text
Verdict::NoChecksPerformed    nothing ran
Verdict::Failed               at least one check failed
Verdict::PassedWithGaps       everything that ran passed, and something could not run
Verdict::Passed               every named check ran and passed
```

The report exposes **no `isValid()`**. A boolean would have to fold `PassedWithGaps` into `true` or
`false`; `true` hides the gap and `false` reports a failure nothing found. That fold is the defect.

Per `P7-CHK-004`, a check that could not be evaluated blocks nothing on its own.
`kNotPerformedBlocksOperation = false` pins it as an assertable constant rather than a reading of
`verdict()`.

### Nine named checks, every one present in every report

An omitted check is indistinguishable from a passed one, so all nine appear in enum order
regardless of whether they ran.

| Check | Runs | Semantics from |
| --- | --- | --- |
| `MissionPresence` | **yes** | A mission with no items has nothing to fly |
| `SequenceNumbering` | **yes** | QGC renumbers densely from 0 on upload and on resume generation |
| `FrameValidity` | **yes** | `MAV_FRAME_ENUM_END` is the fingerprint's unpopulated default and is not packable |
| `CoordinateRange` | **yes** | `PlanManager` treats every frame except `MAV_FRAME_MISSION` as scaled lat/lon (`PlanManager.cc:404-405`, `:551-552`) |
| `ResumeCompatibility` | **yes** | `generateResumeMission()` refuses outright on any `MAV_CMD_DO_JUMP` (`MissionManager.cc:84-86`) |
| `ResumeStateCarryForward` | no — `Q-023` | Omissions are enumerable; classifying which ones matter needs the payload command set |
| `MissionCommandSupport` | **branch exists** — `Q-023` | Runs the moment a command set is declared |
| `VehicleCapabilityAgreement` | no — `Q3-009` + `Q-023` | Nothing reported, and no command-to-capability mapping defined |
| `GeometryLimits` | no — product | No operating limit is approved |

### No threshold is invented

The validator contains exactly two numeric constants, and both are the domain of the
latitude/longitude coordinate system rather than operating limits:

```cpp
constexpr double kLatitudeLimitDeg  = 90.0;
constexpr double kLongitudeLimitDeg = 180.0;
```

A coordinate outside those cannot be a position on the Earth. There is no spacing minimum, no
altitude floor or ceiling, no geofence margin, no turn radius and no consumable budget anywhere in
the file, and `AeroraMissionTest::_validationPassesWithGapsOnACleanMission` asserts that feeding
the validator every input it accepts still leaves `GeometryLimits` unperformed.

### A vacuous pass is a gap, not a pass

Three degenerate inputs would otherwise produce a clean-looking result from an examination that
never happened, and each gets a named reason instead:

| Input | Reason reported |
| --- | --- |
| No snapshot captured | `MissionNotCaptured` |
| Captured and carrying no items | `MissionEmpty` |
| Items present, none carrying a coordinate (all `MAV_FRAME_MISSION`) | `NoApplicableItems` |

Returning `Passed` for any of them is the same defect as omitting the check. `SequenceNumbering`
over zero items is true and establishes nothing.

### The one verdict that is deliberately withheld

`ResumeStateCarryForward` enumerates the items a resume would not carry forward and then reports
`NotPerformed` anyway. That is not an oversight. Upstream deliberately does not carry forward a
navigation item the aircraft already flew — that is what resuming means — while a state-setting
item whose effect is silently lost is the exposure recorded as R-002. Telling the two apart needs
the Aerora payload mission command set (`Q-023`).

Reporting "0 problems" because the list was short would be false assurance; reporting a failure
because the list was long would be a problem nothing established. So the count is carried in the
check's detail and the verdict is withheld.

## Build and test

Once the overlay root wires this module in:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DQGC_BUILD_TESTING=ON -DQGC_CUSTOM_DIR=aerora
cmake --build build
ctest --test-dir build --output-on-failure -R AeroraMission
```

Both tests also self-register through `UT_REGISTER_TEST`:

```bash
./QGroundControl --unittest:AeroraMissionTest
./QGroundControl --unittest:AeroraMissionSyncTest
```

`AeroraMissionTest` is `Unit MissionManager` and needs no vehicle. `AeroraMissionSyncTest` is
`Integration Vehicle MissionManager` and connects a `MAV_AUTOPILOT_GENERIC` MockLink, because the
bridge attaches through `AeroraVehicleServices`, which only exists on a vehicle routed to the Aerora
firmware plugin.

### Lint scope

These files live under `aerora/`, not `test/`, so the pre-commit hooks that exclude `^test/` —
`clang-tidy`, `clazy`, `check-no-qassert`, `vehicle-null-check` — apply to the tests here as well as
to the production code. That is stricter than the core test tree and is intended.

## What was actually verified

| Check | Result |
| --- | --- |
| Upstream mission architecture, read at `9459a4b5` | **SOURCE-INSPECTED** — every citation in this document |
| `includedResumeCommands` definition, content and sole call site | **SOURCE-INSPECTED** — 14 commands, one `.contains()` |
| `includedResumeCommands` unmodified by this workstream | **SOURCE-INSPECTED** — `git status`/`git diff` report `src/MissionManager/` entirely untouched |
| The resume mirror equals upstream's list, same commands and same order | **SOURCE-INSPECTED** — compared programmatically against `MissionManager.cc` at `9459a4b5`. This is a point-in-time check, not a standing guard; see [drift detection](#6-drift-detection-for-the-resume-mirror) |
| `vehicle_null_check.py` over both mission directories | **PASS**, exit 0, no violations |
| Line endings, trailing whitespace, final newline, tabs, 120-column limit | **PASS** |
| Every `#include "..."` resolves to a real header | **PASS** |
| Brace, paren and bracket balance per file | **PASS** |
| No payload/camera/gimbal state persistence or replay upstream | **SOURCE-INSPECTED** — searched `src/Camera`, `src/Gimbal`, `src/VideoManager`, `src/MissionManager` |
| MAVLink enumerators named in tests exist in this tree | **SOURCE-INSPECTED** — each confirmed at a C++ use site |
| Banned patterns: `Q_ASSERT`, `QTest::ignoreMessage`, fixed `qWait`, uncategorized logging | **PASS** — none present |
| `Mission.cmake` source list matches the files on disk, both directions | **PASS** |
| `UT_REGISTER_TEST` names match `AERORA_MISSION_TEST_NAMES` | **PASS** |
| Every `Class::method` defined in a `.cc` is declared in its `.h` | **PASS** |
| Compile | **NOT RUN** — no compiler on this host |
| Link | **NOT RUN** |
| Test execution | **NOT RUN** |
| `clang-format`, `clang-tidy`, `clazy`, `cmake-format` | **NOT RUN** — not installed |

Re-run for the validation change, on the files it touches:

| Check | Result |
| --- | --- |
| `.kiro/tools/aerora-manifest-check.ps1` | **PASS for this module** — 131 declared sources all resolve, the 4 new files among them. The 2 reported problems are another workstream's in-progress QML, not this module's |
| `python tools/analyzers/vehicle_null_check.py` on the new production files | **PASS**, exit 0 |
| `python tools/analyzers/qt_translate_noop_check.py` on all 6 changed files | **PASS**, exit 0 |
| `Q_ASSERT`, `QTest::ignoreMessage`, `qWait`, uncategorized logging, `qCInfo` | **PASS** — 0 occurrences each |
| 120-column limit, LF endings, no tabs, final newline, no trailing whitespace | **PASS** on all 6 files |
| Every `Class::method` definition has a declaration in its header | **PASS** |
| Every project `#include` resolves | **PASS** |
| Brace balance per file | **PASS** |
| Every `CheckId` / `Outcome` / `Unavailable` / `Verdict` enumerator has a `toString` case | **PASS** — checked programmatically against the header |
| `MISSION_ITEM_REACHED` still handled nowhere in `src/` | **SOURCE-INSPECTED** — re-grepped `*.h`, `*.cc`, `*.qml` this pass, zero hits |
| `includedResumeCommands` still unmodified | **SOURCE-INSPECTED** — `src/MissionManager/` untouched by this change |
| `python tools/check_clang_format.py` | **NOT RUN** — the `clang-format` binary is absent on this host |
| Compile, link, test execution | **NOT RUN** |

The honest status of this subtree is `IMPLEMENTED-UNCOMPILED`. **Every test in this module is
written and unexecuted.** No claim is made that any test passes, that the integration test's
MockLink interaction behaves as expected, or that the module links.

## Corrections to earlier phases

### `MISSION_ITEM_REACHED` is not handled anywhere

`.kiro/specs/02-interface-specification/10-mission-icd.md` records `IF-MIS-004` "Mission item
reached" as `GCS-VERIFIED` and `CONFIRMED`, with the failure behaviour "completion is never inferred
from elapsed time or position".

**Source says the message is not handled at all.** A repository-wide search for
`MISSION_ITEM_REACHED` and `mission_item_reached` across `src/` returns nothing. Nothing subscribes
to it, and no item-completion event reaches the ground station.

The requirement's *intent* is satisfied, and more strongly than the ICD states: completion is not
inferred because there is no completion event to infer from. But the interface itself does not exist,
so `IF-MIS-004` should be `NOT-EVIDENCED` rather than `CONFIRMED`, and any design that assumed a
per-item completion event needs revisiting. Progress is available only as the current index.

### Property names in the resume trace

`.kiro/specs/01-aerora-architecture/mission-architecture.md` is accurate on the resume path,
including line numbers. Two naming corrections for anyone navigating from the phase documents:

- There is no `MissionController::currentPlanViewIndex`. The members are `_currentPlanViewSeqNum`
  and `_currentPlanViewVIIndex` (`MissionController.h:443-444`), exposed as `currentPlanViewSeqNum`
  and `currentPlanViewVIIndex`.
- There is no `MissionController::_buildResumeMission`. `MissionController::resumeMission()`
  (`.cc:1854-1860`) is a thin adapter; the generator is `MissionManager::generateResumeMission()`.

### `Vehicle` exposes no current-index getter

`Vehicle` has `setCurrentMissionSequence(int)` but no `currentMissionSequence()` or
`currentMissionIndex()` property or getter, and no `Vehicle::_handleMissionCurrent`. The index is
read from `MissionManager::currentIndex()`, from `MissionController::currentMissionIndex`, or from
the `missionItemIndex` Fact. Worth recording because the asymmetry invites a wrong assumption.

## Integration points for the orchestrator

Each of these is a change in a file this workstream does not own. **None was made by this
workstream.** Item 1 has since been applied by M-Plugin; the rest are outstanding.

### 1. Overlay wiring — `aerora/CMakeLists.txt` — **APPLIED by M-Plugin**

`aerora/CMakeLists.txt` now includes `src/Mission/Mission.cmake` after `src/Vehicle/Vehicle.cmake`,
appends `AERORA_MISSION_SOURCES` / `_INCLUDE_DIRECTORIES` / `_LIBRARIES` and the test variables, and
registers both tests with the labels this module documented. Read and checked against this module's
contract; **not** verified by configuring or building, which this host cannot do.

The snippet below is retained as the contract that wiring satisfies, so a future re-aggregation has
something to check against.

```cmake
include(${CMAKE_CURRENT_SOURCE_DIR}/src/Vehicle/Vehicle.cmake)   # hard dependency
include(${CMAKE_CURRENT_SOURCE_DIR}/src/Mission/Mission.cmake)

list(APPEND CUSTOM_SOURCES ${AERORA_MISSION_SOURCES})
list(APPEND CUSTOM_INCLUDE_DIRECTORIES ${AERORA_MISSION_INCLUDE_DIRECTORIES})

if(QGC_BUILD_TESTING)
    list(APPEND CUSTOM_SOURCES ${AERORA_MISSION_TEST_SOURCES})
    list(APPEND CUSTOM_INCLUDE_DIRECTORIES ${AERORA_MISSION_TEST_INCLUDE_DIRECTORIES})
    include(QGCTest)
    add_qgc_test(AeroraMissionTest LABELS Unit MissionManager)
    add_qgc_test(AeroraMissionSyncTest LABELS Integration Vehicle MissionManager)
endif()
```

`AERORA_VEHICLE_INCLUDE_DIRECTORIES` must be on the target: `AeroraMissionBridge.cc` includes
`AeroraVehicleServices.h`. This manifest deliberately does not re-export another module's include
directory, so omitting the vehicle module fails the build on the missing header rather than silently
producing a mission module that never attaches.

### 2. Somebody must call `AeroraMissionBridge::attachTo(vehicle)` — **CLOSED**

**Resolved by the vehicle workstream.** `AeroraFirmwarePlugin::attachDomainServices()` calls
`AeroraMissionBridge::attachTo(vehicle)` once, last in its dependency-ordered composition, from
`FirmwarePlugin::initializeVehicle()`. The contract and its verification are recorded under
[How the composition root consumes this contract](#how-the-composition-root-consumes-this-contract).

The lifecycle assertion is also in place and is **not duplicated here**: the vehicle workstream's
`AeroraVehicleLifecycleTest` asserts the caller exists, that the bridge is present as a child of
the observer rather than only the observer, and that a repeated `attachTo()` returns the same
bridge instead of connecting a second one. That is the right home for it, because it needs a real
MockLink-connected `Vehicle`. This module's `AeroraMissionSyncTest` covers the real `PlanManager`
completion path instead, which is the part a vehicle lifecycle test does not reach.

### 3. Two `QGCCorePlugin` overrides, if Aerora complex items are ever added

**Required interface:** `AeroraCorePlugin` overriding `complexMissionItemNames()` and
`createComplexMissionItem()` to delegate to `AeroraMissionExtensionRegistry`.
**Reason:** that is the no-core-edit factory seam for complex items.
**Producer:** `aerora/src/AeroraCorePlugin.{h,cc}` — **owned by the integration workstream**.
**Consumer:** `MissionController::insertComplexMissionItem` / `_loadJsonMissionFileV2`.

**Not needed yet**, and deliberately not done: no Aerora complex item is evidenced (Q-021), so the
override would delegate to an empty registry and be dead code. Filed as `HO-TAB5-01` in
[coverage-mission-contract.md § 9](coverage-mission-contract.md#9-handoff-aeroracoreplugin-must-consult-the-mission-extension-registry)
so that registering the first extension does not silently fail to appear in the Plan view.

### 4. Mission command set declaration, when Q-023 resolves

**Required interface:** `AeroraFirmwarePlugin::supportedMissionCommands()` returning
`AeroraMissionExtensionRegistry::declaredMissionCommandSet()` **only when**
`isMissionCommandSetDeclared()` is true.
**Reason:** `AERORA-MISSION-REQ-005`. An empty list means "all commands" upstream.
**Producer:** `aerora/src/Vehicle/AeroraFirmwarePlugin.cc` — **owned by the vehicle workstream**.
**Blocked on:** Q-023. Until then the plugin must keep upstream's behaviour rather than ship a
guessed list.

### 5. Plan-side reporting, for the UI workstream

`AeroraMissionObserver::reportGcsPlan()` and `reportPlanViewSelection()` are not fed by the bridge,
because a plan belongs to a `PlanMasterController` in a view rather than to a vehicle. Binding them
to a vehicle would invent a relationship QGroundControl does not have.

Whoever owns the plan view should call them, with
`AeroraMissionSnapshot::fromMissionItems(AeroraMissionSnapshot::GcsPlan, ...)` built from the same
flattening `MissionController::_convertToMissionItems()` performs. Until then `gcsPlan()` is
uncaptured and every comparison honestly returns `NotCompared`.

Reach the observer through `AeroraMissionBridge::observerFor(masterController->managerVehicle())`
and **null-check the result**. It is nullptr in two legitimate cases: a vehicle not routed to the
Aerora firmware plugin, and the offline editing vehicle, which the composition root deliberately
skips. Offline there is no vehicle mission to compare against in any case, so nothing is lost by
having nowhere to report the plan.

### 6. Drift detection for the resume mirror

`AeroraMissionResumeBoundary::replayedCommands()` mirrors a function-local variable that no seam
exposes. The mirror can go stale against upstream, and no unit test can prove it has not, because
the upstream list is unreachable from code.

**Required check** — for the upstream-drift workstream, `STATIC` per
`03-implementation-specification/09-mission-implementation.md`:

```text
File:     src/MissionManager/MissionManager.cc
Anchor:   lines 110-126, the includedResumeCommands initialiser
Assert:   the 14 MAV_CMD literals and their order match
          AeroraMissionResumeBoundary::replayedCommands()
On drift: fail, and report both lists
```

This workstream did not add it as a unit test, because a test that reads a repository source file at
runtime would be asserting the state of the checkout rather than the behaviour of the binary. The
spec labels this check `STATIC` for the same reason.

### 7. Authorization, if a mission operation ever needs it

No mission operation in this module sends anything, so none needs authorization today. If a future
mission action does, the required interface is the existing
`QGCCorePlugin::authorizeMavCommand()` gate, which the security workstream owns. This workstream
implemented no authorization and no bypass.

## Unresolved questions

Only genuine blockers. Nothing here is answerable from this repository.

| Question | Subject | Blocks |
| --- | --- | --- |
| **Q-023** | Aerora payload actuator model, which determines the mission command set | The explicit command set declaration, and the payload-state plan constraint |
| **Q-021** | Which Aerora complex mission items exist, and their geometry | Any complex item. The seam is ready; the content is product definition |
| **Q-022** | Whether a painting plan must survive a download from the vehicle | Nothing structural. Reconstruction is refused either way |
| **Q-041** | Mission verification depth, including whether to pay for a read-back | Nothing structural. Depth is reported, not chosen |
| **Q-007** | Aerora flight-mode and pause mapping | Mission pause. Untouched here |
| **IF-MIS-014** | Whether the payload controller retains payload state across a mission interruption | R-002 mitigation. `NOT-EVIDENCED`; needs `BENCH` evidence |
| **IF-MIS-013** | Aerora mission status beyond the standard index | Nothing. Degrades to the index, which is what is implemented |
| **IF-MIS-004** | Whether an item-completion event exists at all | See [Corrections](#corrections-to-earlier-phases). Not handled upstream |
| **Q3-009** | Capability presence reporting | `VehicleCapabilityAgreement`. Nothing is reported, so the check cannot run |
| Operating limits | No spacing, altitude, geofence or turn-radius limit is approved | `GeometryLimits`. Product + airframe decision; **no value is invented meanwhile** |
| Applicator working envelope | Not evidenced | Coverage geometry, spacing semantics, and any coverage-quality claim |
| Command-to-capability mapping | Undefined even if `Q-023` and `Q3-009` both resolved | `VehicleCapabilityAgreement`'s performable branch |
| Coverage editor and map visual QML | Owned by the UI workstream | Only the *display* of a coverage item. See [coverage-mission-contract.md § 3](coverage-mission-contract.md#3-the-two-map-visual-seams) |
| ~~Bridge attachment~~ | **Closed.** `AeroraFirmwarePlugin::attachDomainServices()` calls `attachTo()`, and the vehicle workstream's lifecycle test asserts it | — |
| ~~Overlay wiring~~ | **Closed.** `aerora/CMakeLists.txt` aggregates this manifest and registers both tests | — |
| Build host | No toolchain | Every compile, link and test claim in this document |

## Status vocabulary

`CONTRACT-ONLY`, `IMPLEMENTATION-PLANNED`, `IMPLEMENTED`, `IMPLEMENTED-UNCOMPILED`,
`SOURCE-INSPECTED`, `BLOCKED`, `OPEN`, `NOT-EVIDENCED`.

Defined programme-wide in
[05-product-completion/00-README.md](../../../.kiro/specs/05-product-completion/00-README.md#status-vocabulary).
This document does not redefine any of them.

`SOURCE-INSPECTED` is the status for a fact about existing source established by reading, grepping
or mechanical comparison at a named commit. It says nothing about behaviour.

**`VERIFIED` is deliberately absent.** Programme-wide it means built, executed and observed, and
nothing in this subtree has been built or run. An earlier revision of this document used `VERIFIED`
for source reading and scoped it locally, which collided with the programme definition — a reader
grepping for `VERIFIED` would have found claims that look like execution evidence and are not. The
weaker class now has its own name rather than borrowing the stronger one. Build, link, lint and
test results here are **`NOT RUN`**.

The one remaining occurrence of the word in this document is `GCS-VERIFIED` under
[Corrections](#corrections-to-earlier-phases), which quotes another document's status string rather
than making a claim.

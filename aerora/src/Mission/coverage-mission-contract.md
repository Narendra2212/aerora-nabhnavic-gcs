# Aerora Coverage Mission Contract

The integration contract for Aerora's future industrial coverage missions, and the mission
validation foundation that serves them.

Everything below was read from this repository at `7e41a529e`. Where a Phase 7 planning document
and the current source disagree, the source wins and the difference is recorded under
[Corrections to the Phase 7 plan](#corrections-to-the-phase-7-plan).

## Status

| Item | Status |
| --- | --- |
| QGC complex-item architecture | `SOURCE-INSPECTED` — read at `7e41a529e`, cited throughout |
| Integration approach for a coverage item | `DESIGNED` — this document |
| Aerora coverage complex item | `CONTRACT-ONLY` — `Q-021` selects the items, `Q-023` the commands |
| Coverage geometry, parameters and units | `OPEN / NOT-EVIDENCED` — **nothing chosen here** |
| Mission validation result model | `IMPLEMENTED-UNCOMPILED` — `AeroraMissionValidationReport` |
| Mission validation pass | `IMPLEMENTED-UNCOMPILED` — `AeroraMissionValidator`, 5 performable checks, 4 reported unperformable |
| Operating limits used by validation | **NONE** — no threshold is approved, and none is invented |
| Mission ↔ payload boundary | `DESIGNED` — this document, agreed shape with the payload workstream pending |
| `includedResumeCommands` | **NOT MODIFIED** |
| `MISSION_ITEM_REACHED` | `NOT-EVIDENCED` — handled nowhere in `src/`, re-grepped this pass |

**Nothing here has been compiled, linked, linted or executed, and no test has run.** No build host
exists (`D-006`). Every status above is `SOURCE-INSPECTED` or weaker; `VERIFIED` is not claimed.

## 1. What a coverage mission has to cover

Four classes of mission must coexist under one architecture:

| Class | Status | Vehicle for it |
| --- | --- | --- |
| Standard waypoint missions | **Already works, unchanged** | Upstream `SimpleMissionItem` |
| Area coverage over ground | Reusable upstream today | `SurveyComplexItem` |
| Linear coverage along a path | Reusable upstream today | `CorridorScanComplexItem` |
| **Facade and surface coverage** | The genuinely new class | See [§4](#4-reuse-analysis) |

The first three need **no Aerora code at all**. That is worth stating plainly, because the cheapest
version of this work package is the one where Aerora adds an item only for the class upstream
genuinely lacks.

### What is not decided, and must not be

| Decision | Why it cannot be made here |
| --- | --- |
| Which coverage items Aerora ships | `Q-021`. A product decision |
| The geometry of a surface-coverage pattern | Depends on the applicator's working envelope, which is `NOT-EVIDENCED` |
| Parameter names, units and ranges | Same. No applicator ICD exists |
| The mission commands a coverage item flattens to | `Q-023`. The Aerora mission command set is `OPEN / NOT-EVIDENCED` |
| Spacing, overlap, standoff and pass-count rules | Product + airframe. No approved value exists |

This document therefore specifies the **integration contract** — the seams, the obligations, and
the failure modes — and stops at the boundary where product definition begins.

## 2. The integration approach

One seam, already in place, needing no core edit.

```text
Plan view "pattern" button
   │
   ▼  QGCCorePlugin::complexMissionItemNames(Vehicle*)        src/API/QGCCorePlugin.cc:398-415
   │  returns [{canonicalName, translatedName}, ...]
   ▼
Operator picks a pattern
   │
   ▼  QGCCorePlugin::createComplexMissionItem(type, ...)      src/API/QGCCorePlugin.cc:427-449
   │  dispatches on canonicalName OR jsonComplexItemTypeValue
   ▼
ComplexMissionItem instance
   │
   ├──► appendMissionItems()   flatten to MissionItem, once, on upload
   ├──► save() / load()        plan-file round trip
   ├──► mapVisualQML()         QtLocation map visual        REQUIRED
   └──► geoMapVisualQML()      GeoMap visual                REQUIRED FOR AERORA, see §3
```

Both `QGCCorePlugin` methods are `virtual` and `AeroraCorePlugin` already derives from
`QGCCorePlugin`, so the whole mechanism lives inside the overlay. The plan file loader reaches the
same factory, which is what makes a saved plan reload.

### The overlay already has the registry for this

`AeroraMissionExtension` and `AeroraMissionExtensionRegistry` exist and are `CONTRACT-ONLY`. The
registry already provides exactly what this seam needs:

| Registry member | Feeds |
| --- | --- |
| `complexMissionItemNames()` | `QGCCorePlugin::complexMissionItemNames()` |
| `createComplexMissionItem(name, controller, flyView)` | `QGCCorePlugin::createComplexMissionItem()` |
| `isMissionCommandSetDeclared()` / `declaredMissionCommandSet()` | `FirmwarePlugin::supportedMissionCommands()`, and `AeroraMissionValidator` |

So the remaining work when `Q-021` resolves is: write one `AeroraMissionExtension`, write the
complex item it creates, and have `AeroraCorePlugin` consult the registry. **No new seam is
needed**, and the two `AeroraCorePlugin` overrides are a handoff to Tab 1 rather than work here.

### Plan-file round trip

Two distinct keys, and upstream is inconsistent about them in a way Aerora should not copy:

| Item | `canonicalName` | `jsonComplexItemTypeValue` |
| --- | --- | --- |
| `SurveyComplexItem` | `"Survey"` | `"survey"` |
| `CorridorScanComplexItem` | `"Corridor Scan"` | `"CorridorScan"` |
| `StructureScanComplexItem` | `"Structure Scan"` | `"StructureScan"` |

`canonicalName` is `QT_TR_NOOP` and operator-visible. `jsonComplexItemTypeValue` is the **stable
plan-file key** and must never change once a plan has been saved with it — changing it silently
orphans every saved plan, because `createComplexMissionItem()` returns `nullptr` for an
unrecognised type and logs a warning.

Obligations for an Aerora item:

- Pick one casing convention and keep it. The upstream inconsistency is not a precedent to follow.
- The key must be namespaced enough that it cannot collide with a future upstream pattern.
- **No scanner.** Flattening is one-way. Upstream reconstructs complex items from a download only
  for landing patterns; survey-style items round-trip through the plan file alone. Aerora adds no
  scanner, by decision (`AERORA-MISSION-REQ-002`): a flat item sequence that happens to match a
  pattern is not evidence that the pattern is what the operator planned.

## 3. The two map visual seams

This is the defect class the Phase 7 plan flags, and the source says something more specific than
the plan does.

| Seam | Declaration | Required? | Base returns |
| --- | --- | --- | --- |
| `mapVisualQML()` | `VisualMissionItem.h:182` | **Pure virtual** — cannot be omitted | — |
| `geoMapVisualQML()` | `VisualMissionItem.h:186` | Optional | `QString()` |

Consumers:

| Loader | Location | Guard |
| --- | --- | --- |
| `MissionItemMapVisual.qml:36` | `src/PlanView/` | **None.** `setSource(object.mapVisualQML, properties)` |
| `GeoMapMissionItems.qml:40,44` | `src/GeoMap/` | `active: object.geoMapVisualQML !== ""` |

So an item with no GeoMap visual is **silently invisible** under the GeoMap engine: the guard means
no error is produced, the `Loader` simply never activates.

### Upstream's own coverage items already have this gap

Only three types in the whole tree override `geoMapVisualQML()`:

| Type | Returns |
| --- | --- |
| `SimpleMissionItem.h:126` | `"GeoMapWaypointItem.qml"` |
| `FixedWingLandingComplexItem.h:31` | `"GeoMapFWLandingPatternVisual.qml"` |
| `VTOLLandingComplexItem.h:24` | `"GeoMapVTOLLandingPatternVisual.qml"` |

`TransectStyleComplexItem`, `SurveyComplexItem`, `CorridorScanComplexItem` and
`StructureScanComplexItem` **do not**. They inherit the empty default, so every upstream coverage
pattern is already invisible on GeoMap.

Two consequences, and the second is the one that matters for Aerora:

1. The gap is a pre-existing upstream condition, not an Aerora-specific trap.
2. **Inheriting `TransectStyleComplexItem` does not give you a GeoMap visual.** An Aerora item must
   override `geoMapVisualQML()` itself, and no amount of base-class reuse will supply it.

### A bare filename will not work for an overlay item

Upstream returns bare filenames — `"SurveyMapVisual.qml"`, `"GeoMapWaypointItem.qml"`. That works
because a relative URL passed to `Loader.setSource()` resolves against the document containing the
`Loader`, and upstream's visuals are co-located with their loaders:

| Visual | Lives in | Loader lives in |
| --- | --- | --- |
| `SurveyMapVisual.qml`, `StructureScanMapVisual.qml` | `src/PlanView/` | `src/PlanView/` |
| `GeoMapWaypointItem.qml`, `GeoMapFWLandingPatternVisual.qml` | `src/GeoMap/` | `src/GeoMap/` |

An Aerora visual lives in the overlay's QML module, `Aerora.Controls`, which resolves to
`qrc:/qml/Aerora/Controls/` (`aerora/src/UI/Ui.cmake:133`, alias landed in `a73699796`). A bare
filename returned from an Aerora item would resolve against `src/PlanView/` or `src/GeoMap/` and
find nothing.

**Obligation: an Aerora complex item must return a full `qrc:` URL from both seams**, not a bare
filename. The two failure modes differ and neither is loud:

- `mapVisualQML()` — the loader is unguarded, so a non-resolving URL produces a `Loader` error on
  the console and an item with no visual on the map.
- `geoMapVisualQML()` — an empty string is a silent no-visual; a non-resolving URL is the same
  console error as above.

Both are presentation-layer files and therefore Tab 7's to write. The C++ side's obligation is to
return resolvable URLs for both, and to be tested for it. The existence-gated pattern Tab 3 used
for setup pages and tool indicators is directly applicable.

## 4. Reuse analysis

### `TransectStyleComplexItem` — substantial, with a camera-shaped surface attached

What it genuinely provides, none of which should be rewritten:

| Provided | Where |
| --- | --- |
| Survey-area polygon, with validity tracking | `_surveyAreaPolygon` |
| Transect generation, rebuild and recalculation plumbing | `_rebuildTransects()`, `_rebuildTransectsPhase1()` |
| Terrain following: path height queries, climb/descent rate limiting, tolerance | `_polyPathTerrainData()`, `_adjustForMaxRates()`, `_adjustForTolerance()` |
| Turnaround extensions | `_turnAroundDistanceFact`, `_hasTurnaround()` |
| Flight-path segments and terrain-collision reporting | `_appendFlightPathSegment()` |
| KML export | `addKMLVisuals()` |
| Item flattening with waypoints and condition gates | `_buildAndAppendMissionItems()` |
| `MISSION_ITEM_INT` sequencing, entry/exit coordinates, AMSL min/max | various `final` overrides |
| A transect-count ceiling that raises spacing rather than generating an unbounded mission | `maxTransectCount = 1000` |

What comes with it that a non-camera coverage mission would have to carry:

| Inherited surface | Problem |
| --- | --- |
| `CameraCalc _cameraCalc` is a **by-value member** | Not injectable. Cannot be replaced or omitted |
| `Q_PROPERTY cameraCalc`, `cameraShots`, `timeBetweenShots`, `cameraTriggerInTurnAround`, `hoverAndCapture`, `refly90Degrees` | All QML-visible, all camera-shaped, all meaningless for a non-camera applicator |
| `virtual void _recalcCameraShots() = 0` | **Pure virtual.** A subclass must implement it, even to return nothing |
| `triggerDistance()` reads `_cameraCalc.adjustedFootprintFrontal()` | The spacing source is hard-wired to the camera model |
| `_appendSinglePhotoCapture()`, `_appendCameraTriggerDistance()` | Emit camera commands into the flattened mission |

**Verdict: reuse is correct for a ground-coverage pattern, and the camera-shaped surface is the
price.** It is a real price — those properties would appear in any editor QML bound to the item —
and whether it is acceptable is a `Q-021` decision, not a technical one. It must not be paid
silently.

### `CameraCalc` — repurposing is upstream-sanctioned, within limits

`CameraCalc.h:28-33` documents the manual-grid mapping in upstream's own words: it is "a
bastardization of what CameraCalc is meant for but it greatly simplifies code and persistance of
manual grids".

| Coverage concept | `CameraCalc` field |
| --- | --- |
| Working altitude | `distanceToSurface` |
| Altitude frame | `distanceMode` |
| Trigger / action spacing along a pass | `adjustedFootprintFrontal` |
| Spacing between passes | `adjustedFootprintSide` |

So using `CameraCalc` to carry non-camera spacing is **not** an Aerora invention; it is the
documented manual-grid path, and it brings plan-file persistence for free.

Its limits, which must not be papered over:

- It models a **projected rectangular footprint at a distance**. An applicator's working envelope
  is not evidenced, and there is no reason to assume it is a rectangle, or that overlap composes
  the way image overlap does.
- `frontalOverlap` / `sideOverlap` are percentages of an image footprint. Reusing them for a
  coverage overlap asserts the same geometry.
- Painting coverage is **not** camera coverage. A camera either captured a frame or did not; a
  surface treatment has a deposition profile, a consumable budget and a result that depends on
  standoff and rate. None of that is in `CameraCalc`, and none of it is evidenced.

**Verdict: `CameraCalc` is the right carrier for spacing values and the wrong model for coverage
quality.** Use it to hold and persist the numbers once a product defines them. Do not derive a
coverage claim from it.

### `StructureScanComplexItem` — the closest analogue to a facade mission, and it is overlooked

The Phase 7 plan points at `TransectStyleComplexItem` for reuse. For **facade and surface**
coverage the nearer relative is `StructureScanComplexItem`, which the plan does not mention:

| Property | Models |
| --- | --- |
| `structureHeight` | The vertical extent of the surface |
| `scanBottomAlt`, `entranceAlt` | Where the scan starts and stops vertically |
| `layers` | Horizontal passes stacked vertically — the facade pattern |
| `cameraCalc` | Spacing, via the same by-value member |

It derives from `ComplexMissionItem` **directly**, not from `TransectStyleComplexItem`, and
provides its own `mapVisualQML()` (`StructureScanMapVisual.qml`). That is strong evidence about the
architecture: upstream itself concluded that a vertical, layered, surface-following pattern does
**not** fit the transect machinery, and wrote a sibling instead.

**Recommendation for the `Q-021` decision:** evaluate `StructureScanComplexItem` as the model for
facade coverage before `TransectStyleComplexItem`. It already solves layering over a vertical
surface, it carries no turnaround or refly-90 concepts that make no sense on a facade, and its
polygon-plus-height shape is closer to a building face than a ground survey area is. The transect
base remains the right model for *ground* coverage.

This is a recommendation, not a decision. It does not change what is implemented today, which is
nothing.

## 5. Mission validation foundation

Implemented this pass. `AeroraMissionValidationReport` + `AeroraMissionValidator`.

### Why upstream's gate is not enough

QGC has a readiness gate: `VisualMissionItem::readyForSaveState()` returns one of three per-item
values, and `MissionController::readyForSaveState()` (`MissionController.cc:876-884`) returns the
**first** non-ready item's state and stops.

That answers "may this be saved". It cannot answer what was checked, what passed, or what could not
be checked — and the last one is the requirement (`P7-CHK-002`). A check that silently did not run
looks identical to a check that passed.

### The three-outcome model

```text
Outcome::NotPerformed = 0     ← the default. An unstated check is not a passed check
Outcome::Passed
Outcome::Failed
```

Aggregated by `Verdict`, which has a value a boolean cannot express:

```text
NoChecksPerformed    nothing ran
Failed               at least one check failed
PassedWithGaps       everything that ran passed, and something could not run
Passed               every named check ran and passed
```

There is deliberately **no `isValid()` boolean** on the report. A boolean would have to fold
`PassedWithGaps` into `true` or `false`, and both are wrong. That fold is the defect the
requirement exists to prevent.

Per `P7-CHK-004`, a check that could not be evaluated blocks nothing on its own.
`kNotPerformedBlocksOperation = false` pins that as an assertable constant rather than a
convention.

### The nine named checks

Every check appears in every report, in enum order, whether or not it ran. An omitted check is
indistinguishable from a passed one.

| Check | Performable | Semantics come from |
| --- | --- | --- |
| `MissionPresence` | **Yes** | A mission with no items has nothing to fly |
| `SequenceNumbering` | **Yes** | QGC renumbers densely from 0 on upload and resume generation, so non-monotonic is corrupt |
| `FrameValidity` | **Yes** | `MAV_FRAME_ENUM_END` is the fingerprint's unpopulated default and is not a frame `PlanManager` can pack |
| `CoordinateRange` | **Yes** | `PlanManager` treats every frame except `MAV_FRAME_MISSION` as carrying scaled lat/lon (`PlanManager.cc:404-405`, `:551-552`) |
| `ResumeCompatibility` | **Yes** | `generateResumeMission()` refuses outright on any `MAV_CMD_DO_JUMP` (`MissionManager.cc:84-86`) |
| `ResumeStateCarryForward` | No — `Q-023` | Omissions can be enumerated; classifying which ones matter needs the payload command set |
| `MissionCommandSupport` | **Branch exists** — `Q-023` | Runs as soon as a command set is declared. Unperformable today because none is |
| `VehicleCapabilityAgreement` | No — `Q3-009` + `Q-023` | No capability is reported, and no command-to-capability mapping is defined |
| `GeometryLimits` | No — product | No spacing, altitude, geofence or turn-radius limit is approved |

Five run. Four report why they cannot. `MissionCommandSupport` is the one of the four with a real
performable branch, which is what makes the gate itself testable rather than a stub.

### No threshold is invented

The validator contains exactly two numeric constants, both the domain of the latitude/longitude
coordinate system rather than operating limits:

```cpp
constexpr double kLatitudeLimitDegrees  = 90.0;
constexpr double kLongitudeLimitDegrees = 180.0;
```

A coordinate outside those is not a marginal mission; it is a value that cannot be a position on
the Earth. There is no spacing minimum, no altitude floor or ceiling, no geofence margin, no turn
radius and no consumable budget anywhere in the file.

### What a coverage item will add to validation

When `Q-021` resolves, a coverage item participates without changing the model:

- Its flattened form is already checkable — the validator runs on `AeroraMissionSnapshot`, which is
  the flattened wire shape.
- Its commands become checkable by `MissionCommandSupport` as soon as the extension declares a set.
- Its geometry becomes checkable by `GeometryLimits` as soon as a limit is approved.

No new check id is needed for the first two. That is the point of naming the unperformable checks
now: the report's shape does not change when the inputs arrive.

## 6. Mission ↔ payload boundary

```text
Mission plan
   │  may reference a payload action
   ▼
Payload action request
   │
   ▼
Payload controller      owns payload state, payload interlocks and payload-local safety
```

not

```text
Mission  ──X──►  GCS directly actuates pump / nozzle / applicator
```

### The rules, and how each is enforced rather than stated

| Rule | Enforcement |
| --- | --- |
| The payload controller owns payload state and payload safety | This module defines no payload state and no payload command. There is no type to hold one |
| The flight controller retains flight authority | Nothing in this module sends anything. `AeroraMissionObserver` has no command API at all |
| The GCS may represent payload status | Through the payload workstream's model, reached via `AeroraVehicleServices::attachService()`. Not through mission types |
| No GCS-side actuator path | No mission type reaches a payload controller, a link, or `Vehicle` — except `AeroraMissionBridge`, which only reads `MissionManager` signals |
| No station-side interlock | None exists. An interlock here would be a second safety authority competing with the payload controller's |
| No automatic payload actuation | Nothing in this module can actuate. The validator reports; it does not act |
| No assumed painting commands or payload protocol | `Q-023`. `AeroraMissionExtensionRegistry::isMissionCommandSetDeclared()` is false and `AeroraMissionTest` pins it |
| `includedResumeCommands` not modified | Mirrored read-only by `AeroraMissionResumeBoundary`; a test asserts no payload command entered the mirror |
| No station payload-state replay | `kGcsPayloadStateReplayImplemented = false`, asserted |
| Payload state retention across interruption not assumed | `kPayloadStatePersistenceEvidenced = false`, asserted |

### The consequence the validator makes visible

Mission resume is **not** payload-state replay. Upstream's resume carries forward only 14 camera,
gimbal, ROI, geofence-enable and speed commands; every actuator and payload-actuation command is
absent from that set, and absent upstream too.

So after an interruption, a payload action commanded before the resume point is **not**
re-established by the station, and whether the payload controller retained it is `NOT-EVIDENCED`.
`ResumeStateCarryForward` reports the count of items that would not be carried forward and
explicitly withholds a verdict on whether any of them matters, because that classification needs
`Q-023`. Reporting "0 problems" there would be exactly the false assurance this contract exists to
prevent.

### What the payload workstream still owns

| Open | Owner |
| --- | --- |
| The payload reported-value set | Payload workstream + applicator ICD |
| Whether the payload controller retains state across an interruption | Firmware evidence |
| The payload mission command set, if any | `Q-023` |
| Whether a coverage item may reference a payload action at all | Product + `Q-023` |

The boundary shape above is this module's half. Agreement with the payload workstream is pending
and is recorded as such rather than assumed.

## 7. Corrections to the Phase 7 plan

Read from source this pass. None changes the plan's conclusions; two sharpen them and one
redirects a reuse recommendation.

| Plan statement | Source says |
| --- | --- |
| `geoMapVisualQML()`'s omission "makes the item silently invisible under the GeoMap engine. That is a defect class" — framed as an Aerora trap | Correct about the consequence, and it is **already true of upstream**: `Survey`, `CorridorScan`, `StructureScan` and `TransectStyleComplexItem` all inherit the empty default. Only `SimpleMissionItem` and the two landing patterns override it |
| "Reuse `TransectStyleComplexItem*` scaffolding and `CameraCalc*` for spacing math" | Both reusable, with the limits in [§4](#4-reuse-analysis). But for **facade** coverage the nearer model is `StructureScanComplexItem`, which the plan does not mention and which upstream wrote precisely because a layered vertical pattern does not fit the transect machinery |
| Not stated in the plan | A bare-filename return works only because upstream's visuals are co-located with their loaders. An overlay item must return a full `qrc:` URL from both seams or neither visual loads. See [§3](#3-the-two-map-visual-seams) |

## 8. Open decisions and dependencies

| Item | Blocks | Owner |
| --- | --- | --- |
| `Q-021` — which complex items Aerora needs | The item itself, its editor, both map visuals | Product |
| `Q-023` — the Aerora mission command set | Flattening, `MissionCommandSupport`, `ResumeStateCarryForward` classification, any payload reference | Firmware + product |
| `Q3-009` — capability presence reporting | `VehicleCapabilityAgreement` | Firmware |
| `Q-041` — mission verification depth | Whether a read-back comparison is performed after upload | Product |
| Operating limits | `GeometryLimits` | Product + airframe |
| Applicator working envelope | Coverage geometry, spacing semantics, any coverage-quality claim | Applicator ICD |
| Coverage editor and map visual QML | Both visual seams resolving | UI workstream |
| `AeroraCorePlugin` consulting the registry | The pattern reaching the Plan view at all | Integration workstream — handoff below |

## 9. Handoff: `AeroraCorePlugin` must consult the mission extension registry

Filed rather than implemented, because `aerora/src/AeroraCorePlugin.*` is the integration
workstream's file.

| Field | Detail |
| --- | --- |
| ID | `HO-TAB5-01` |
| Owner | Integration workstream (Tab 1) |
| File | `aerora/src/AeroraCorePlugin.{h,cc}` |
| Change | Override `QGCCorePlugin::complexMissionItemNames(Vehicle*)` and `QGCCorePlugin::createComplexMissionItem(...)`; call the base, then append from / fall through to `AeroraMissionExtensionRegistry::instance()` |
| Invariant at risk | A registered Aerora pattern is unreachable from the Plan view, and a saved plan containing one fails to load with only a `qCWarning` |
| Blocks | Nothing today — the registry is empty while `Q-021` is open. Required before the first extension is registered |
| Tracked by | `AeroraMissionTest::_missionExtensionRegistryRefusesAmbiguity` covers the registry; the plugin override needs its own assertion when written |
| Note | **Not urgent, and must not be written speculatively.** With no extension registered, the override would be dead code. It is filed so that registering the first extension does not silently fail to appear |

## 10. Status vocabulary

`CONTRACT-ONLY`, `DESIGNED`, `IMPLEMENTED`, `IMPLEMENTED-UNCOMPILED`, `SOURCE-INSPECTED`,
`BLOCKED`, `OPEN`, `NOT-EVIDENCED`.

Defined programme-wide in
[05-product-completion/00-README.md](../../../.kiro/specs/05-product-completion/00-README.md#status-vocabulary).
This document does not redefine any of them.

`SOURCE-INSPECTED` is the status for a fact about existing source established at `7e41a529e` by
reading, grepping or mechanical comparison. It says nothing about behaviour.

**`VERIFIED` is deliberately absent from the list above.** Programme-wide it means built, executed
and observed, and nothing in this subtree has been built or run. An earlier revision of this
document used `VERIFIED` for source reading and scoped it locally; that collided with the programme
definition, so the weaker class now has its own name rather than borrowing the stronger one. Build,
link, lint and test results here are **`NOT RUN`**.

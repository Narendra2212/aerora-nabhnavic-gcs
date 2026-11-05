# Mission Command Set and Coverage Items

The content `DG-MIS` settles. Product signed the `mission-command-set` part against this document
and the Gate is recorded `CLOSED`.

## Status

| Item | Status |
| --- | --- |
| This document | `DESIGNED` -- Product signed the `mission-command-set` part and `DG-MIS` is recorded `CLOSED` |
| Aerora MAVLink commands introduced | **Zero.** Every command below is an existing upstream command |
| Aerora complex items introduced | One: surface coverage |
| Operating limits set here | **NONE.** No spacing, standoff, altitude or consumable limit is invented |
| Aerora MAVLink dialect | Still `NOT-EVIDENCED`, and deliberately not required by anything here |

The five elements the Gate_Register records against the `mission-command-set` part are settled in
order below: the command set, the complex-item type name, the item parameter set, the asset and
obstruction geometry, and the plan-file representation.

## 1. The command set reuses upstream entirely

**Decision: the first product ships no Aerora mission command.**

The Gate_Register element asks for each mission command *with its issued MAVLink command identifier
or its existing upstream command*. Every command this mission set needs is an existing upstream
command, so the second branch is taken for all of them and no identifier is issued.

This is not a compromise, it is the cheaper correct answer. The Aerora dialect does not exist:
[03-mavlink-icd.md](../02-interface-specification/03-mavlink-icd.md) records Class B, Aerora
extensions, as containing zero entries. Creating one is its own programme -- a controlled fork of
the MAVLink definitions repository pinned to an immutable commit, a version reported in telemetry,
and a four-axis compatibility matrix. Its own recorded allocation discipline includes *never reuse
an identifier, even for a withdrawn message*, because a field recording is forever. Committing
identifiers into saved plan files before the payload interfaces are known would spend that
permanence on guesses.

| Mission need | Upstream command | Note |
| --- | --- | --- |
| Fly a pass | `MAV_CMD_NAV_WAYPOINT` | Standard |
| Working altitude and its frame | Carried on the item as `MAV_FRAME_*` | Not a command |
| Pass speed | `MAV_CMD_DO_CHANGE_SPEED` | Standard |
| Actuate at a fixed interval along a pass | `MAV_CMD_DO_SET_CAM_TRIGG_DIST` | See below |
| Discrete applicator engage and release | `MAV_CMD_DO_SET_ACTUATOR` | `MAV_CMD_DO_SET_SERVO` where the applicator is a servo channel |
| Hold attention on a point | `MAV_CMD_DO_SET_ROI_LOCATION` | Inspection, structural fire fighting |
| Station-keeping at a point | `MAV_CMD_NAV_LOITER_TIME` | Standard |

### Why distance-triggered actuation is the right carrier

The fourth row is the one that makes this work. `TransectStyleComplexItem` already emits
distance-triggered actions and takes the interval from `CameraCalc::adjustedFootprintFrontal()`.
*Actuate every N centimetres along this pass* is structurally the same statement as *capture a frame
every N metres*: a geometric interval along a planned path, resolved by the flight controller.

The reuse is upstream-sanctioned rather than an Aerora invention. `CameraCalc.h` describes the
manual-grid mapping in upstream words as a bastardization of what `CameraCalc` is meant for that
*greatly simplifies code and persistance of manual grids*. Taking that path brings plan-file
persistence of the interval for free.

Its limit is recorded here so it is not discovered later. `CameraCalc` models a projected
rectangular footprint at a distance, and its overlap percentages are percentages of an image
footprint. It is the right carrier for the interval **value** and the wrong model for a coverage
**quality** claim: a camera either captured a frame or did not, whereas a surface treatment has a
deposition profile, a consumable budget and a result that depends on standoff and rate. No coverage
quality claim is derived from it anywhere.

## 2. One Aerora complex item, and it is not transect-shaped

**Decision: Aerora adds exactly one complex item, for surface coverage: a sibling of
`StructureScanComplexItem` that derives from `ComplexMissionItem` directly, exactly as the upstream
`StructureScan` itself does.**

Three of the four mission classes the product range needs require no Aerora code at all, which is
worth stating plainly because the cheapest correct version of this work is the one that adds an item
only for the class upstream genuinely lacks.

| Class | Products it serves | Vehicle for it |
| --- | --- | --- |
| Waypoint missions | Surveillance, transit, delivery | Upstream `SimpleMissionItem` |
| Ground area coverage | Agriculture, survey, mapping | Upstream `SurveyComplexItem` |
| Linear coverage along a path | Pipeline, road, power line, fence line | Upstream `CorridorScanComplexItem` |
| **Surface and facade coverage** | **Painting, cleaning, facade inspection, structural fire fighting** | The one new item |

### Why `StructureScan` and not `TransectStyle`

The Phase 7 plan points at `TransectStyleComplexItem` for reuse. For a vertical surface the nearer
relative is `StructureScanComplexItem`, and upstream itself is the evidence: `StructureScan` derives
from `ComplexMissionItem` **directly** rather than from the transect machinery, which means upstream
concluded that a layered vertical pattern does not fit transects and wrote a sibling instead.

| `StructureScan` already models | Facade meaning |
| --- | --- |
| `structureHeight` | The vertical extent of the surface |
| `scanBottomAlt`, `entranceAlt` | Where the work starts and stops vertically |
| `layers` | Horizontal passes stacked vertically, which *is* the facade pattern |
| `cameraCalc` | The interval and spacing carrier, per section 1 |

What inheriting `TransectStyleComplexItem` would cost instead: a by-value `CameraCalc` member that
cannot be replaced or omitted; QML-visible `cameraShots`, `timeBetweenShots`,
`cameraTriggerInTurnAround`, `hoverAndCapture` and `refly90Degrees` properties that are meaningless
on a wall and would appear in any editor bound to the item; and a pure-virtual
`_recalcCameraShots()` that a subclass must implement even to return nothing.

### Why a sibling and not a subclass

`StructureScanComplexItem` cannot be subclassed into this item, and the reason is mechanical rather
than stylistic: it declares every override this item needs `final`.

| Override | Declared | Sealed to |
| --- | --- | --- |
| `mapVisualQML()` | `final` | `StructureScanMapVisual.qml` |
| `patternName()` | `final` | `Structure Scan` |
| `commandName()`, `commandDescription()` | `final` | `Structure Scan` |
| `abbreviation()` | `final` | `S` |
| `load()` | `final` | The StructureScan plan format |

So a subclass could not carry its own name, could not return its own map visual, and could not read
its own plan-file representation. Every obligation this document places on the item is sealed by
that base.

`ComplexMissionItem` seals none of it: `patternName()`, `mapVisualQML()`, `load()` and the rest are
pure virtual, and `geoMapVisualQML()` is a plain virtual returning an empty string, which is the
overridable default section 7 depends on. Deriving there costs 35 pure virtuals to implement rather
than inherit -- all mechanical, and all owned here rather than by a base that sealed them.

Ground coverage keeps the transect lineage through upstream `Survey`. The split follows upstream.

### What this means for adding a product

A new product family does not add a complex item. It adds a capability token and an
`AeroraProductProfile`, and the existing surface-coverage item serves it. That is the integration
placeholder, enforced by structure rather than by convention: the item carries geometry and
intervals, and what the payload *does* at those intervals is the payload controller concern.

## 3. The type name and the plan-file key

**Decision: canonical name `Surface Coverage`; plan-file key `AeroraSurfaceCoverage`, permanent from
the first saved plan.**

| Key | Value | Role |
| --- | --- | --- |
| `canonicalName` | `Surface Coverage` | `QT_TR_NOOP`, operator-visible, what `complexMissionItemNames()` reports |
| `jsonComplexItemTypeValue` | `AeroraSurfaceCoverage` | The stable plan-file key `createComplexMissionItem()` dispatches on |

The second must never change once a plan has been saved with it. `createComplexMissionItem()`
returns `nullptr` for an unrecognised type and logs a warning, so changing the key **silently
orphans every saved plan** -- the operator sees a plan that loads without its pattern and no error
that says why.

Upstream is inconsistent about casing between the two keys: `Survey` and `survey`,
`Corridor Scan` and `CorridorScan`, `Structure Scan` and `StructureScan`. That inconsistency is not
a precedent to copy. Aerora uses one convention and namespaces the plan-file key with the `Aerora`
prefix so it cannot collide with a future upstream pattern.

## 4. The item parameter set

**Decision: the parameter set below is settled. No limit on any of them is settled here.**

The distinction matters and is the reason this element can close while operating limits stay open.
The *set* -- which parameters exist, what each means, and its unit -- is a shape decision this
document makes. A parameter *limit* is an operating limit that depends on the applicator working
envelope, which is `NOT-EVIDENCED`. `AeroraMissionValidator` already names a `GeometryLimits` check
and already reports it unperformable for exactly this reason; it starts running when limits arrive,
with no change to the report shape.

| Parameter | Unit | Meaning | Limit source |
| --- | --- | --- | --- |
| `workingDistance` | m | Standoff from the surface along its normal | Applicator ICD |
| `passInterval` | m | Spacing between stacked passes | Applicator ICD |
| `actionInterval` | m | Distance between actuations along a pass | Applicator ICD |
| `surfaceHeight` | m | Vertical extent of the worked surface | Frame and airframe |
| `startAltitude` | m | Altitude of the first pass, in the item frame | Frame and airframe |
| `entryAltitude` | m | Altitude the aircraft arrives and departs at | Frame and airframe |
| `passSpeed` | m/s | Ground speed along a pass | Airframe and applicator |
| `passDirection` | enum | Whether stacked passes alternate direction or all run one way | Product |
| `capabilityRequired` | opaque token | The payload capability a pass needs, resolved per section 6 | `DG-PAY:capability` |

`capabilityRequired` is a token and not a payload command. It carries no actuator identity, no
enumeration member and no MAVLink field, for the reason `mavCommand` is carried as an int.

## 5. Asset and obstruction geometry

**Decision: geometry is carried as item parameters in the two shapes below, and the scene editor
edits those parameters rather than a separate model.**

| Geometry | Representation | Why this shape |
| --- | --- | --- |
| The worked surface | A polygon plus `surfaceHeight` | The `StructureScan` shape: a ground footprint extruded vertically. Closer to a building face than a survey area is |
| An obstruction | Zero or more polygons, each with a height and a clearance | A balcony, a sign, a cable tray or a parked vehicle is an exclusion with vertical extent, not a point |

An obstruction is a first-class list rather than a flag, because the product range needs several per
surface and because clearance is per obstruction: a cable tray and a window ledge do not warrant the
same standoff. The clearance **value** is an operating limit and is not set here.

Both shapes persist through the plan file by the same mechanism the polygon in `StructureScan`
already uses, so the round trip needs no new serialisation.

## 6. Payload actions are referenced by capability, never by command

**Decision: a pass references a payload capability token. The payload controller owns actuation.**

```text
Mission plan  --> payload capability token  --> payload controller --> applicator
```

not

```text
Mission plan  --> station actuates pump, nozzle or applicator
```

This keeps the boundary the mission layer already enforces structurally rather than by convention:
the mission module defines no payload state and no payload command, so there is no type that could
hold one. The station supervises and firmware actuates.

It is also what makes a new product cheap. Fire fighting after painting is a new capability token
and a new `AeroraProductProfile`, not a change to the mission item, the command set or the plan-file
format.

While `DG-PAY:capability` holds no accepted Closure_Record, a capability token resolves as
**unresolvable** rather than as every aircraft, which Requirement 4 criterion 7 of
[aerora-fleet-and-authority](../aerora-fleet-and-authority/requirements.md) requires.

## 7. Both map visual seams, as resolvable URLs

**Decision: the item returns a full `qrc:` URL from `mapVisualQML()` and from
`geoMapVisualQML()`, and is tested for both.**

This is recorded as an obligation because neither failure is loud.

| Seam | Required? | Consumer | Failure when absent |
| --- | --- | --- | --- |
| `mapVisualQML()` | Pure virtual, cannot be omitted | `MissionItemMapVisual.qml`, unguarded | Loader error on the console, no visual |
| `geoMapVisualQML()` | Optional, defaults to empty | `GeoMapMissionItems.qml`, guarded on non-empty | **Silently invisible.** No error at all |

Two map engines exist, selected by `flyViewSettings.useGeoMapEngine` with a reboot to switch, so an
item missing the second visual works for one operator and is invisible to another.

Every upstream coverage pattern already has this gap: `TransectStyleComplexItem`, `Survey`,
`CorridorScan` and `StructureScan` all inherit the empty default, and only `SimpleMissionItem` and
the two landing patterns override it. **No base class supplies a GeoMap visual** and no amount of
reuse will.

The URL must be fully qualified. Upstream returns bare filenames, which resolve relative to the
document containing the `Loader` and work only because upstream visuals are co-located with their
loaders. An Aerora visual lives in `qrc:/qml/Aerora/Controls/`, so a bare filename would resolve
against `src/PlanView/` and find nothing.

## What this document does not settle

| Open | Owner | Why it stays open |
| --- | --- | --- |
| Every operating limit: standoff, intervals, clearance, consumable budget | Product with the applicator ICD | The applicator working envelope is `NOT-EVIDENCED`. `GeometryLimits` reports unperformable until they arrive |
| The payload state and actuator vocabulary | `Q-023`, payload engineering | Section 6 needs no part of it |
| The capability identifier form | `DG-PAY:capability`, Firmware | Section 4 carries an opaque token meanwhile |
| The Aerora MAVLink dialect | ADR-004, Architecture | Section 1 needs none of it. Driven later by payload *telemetry*, which is additive, rather than by commands, which are permanent |
| Mission verification depth | `Q-041`, Product | Whether a read-back comparison follows upload |

## What signing this releases

Seven screens, which is more than any other single Gate signature available:

| Screen | What it becomes |
| --- | --- |
| `SCR-MIS-01` | Scene editor: the surface polygon and the obstruction list of section 5 |
| `SCR-MIS-02` | Industrial surface item editor: the parameter set of section 4 |
| `SCR-MIS-03` | Surface map visual, QtLocation |
| `SCR-MIS-04` | Surface visual, GeoMap engine, per section 7 |
| `SCR-MIS-07` | Mission validation report |
| `SCR-MIS-08` | 3D plan review |
| `SCR-MIS-09` | Mission verification |

`SCR-MIS-05` and `SCR-MIS-06` are panels inside `SCR-MIS-02` and additionally wait on `DG-PAY` for
the consumption model, so they are not released by this signature alone.

It also lights a validation check that is currently dark. `AeroraMissionValidator` carries nine
named checks, five performable today and four reporting why they cannot run.
`MissionCommandSupport` has a real performable branch that switches on as soon as a command set is
declared, so declaring this set starts it checking every mission rather than reporting itself
unperformable.

## Obligations this creates

| ID | Obligation | Owner |
| --- | --- | --- |
| `HO-TAB5-01` | `AeroraCorePlugin` must override `complexMissionItemNames()` and `createComplexMissionItem()` to consult `AeroraMissionExtensionRegistry`. Filed and deliberately unwritten while the registry is empty; **required before the first extension is registered**, or the pattern never appears in the Plan view and a saved plan containing one fails to load with only a warning | Integration |
| -- | `AeroraMissionExtension` declares the command set of section 1, which is what turns `MissionCommandSupport` performable | Mission |
| -- | Both visual QML documents, returning from the two seams of section 7 | UI |
| -- | A test asserting both seams return resolvable `qrc:` URLs | Mission |

## Status vocabulary

Defined programme-wide in [00-README.md](00-README.md#status-vocabulary). This document redefines
none of them, and claims `VERIFIED` for nothing: no part of this has been built or run.

# Phase 7 Work Packages and Tab Ownership Plan

Owner: **Tab 1**. The assignment sheet. Supersedes the "Recommended next parallel work" section of
[29-integration-status.md](../03-implementation-specification/29-integration-status.md) and nothing
else in it.

Status vocabulary: [00-README](00-README.md#status-vocabulary).

## Contents

- [1. Ownership map](#1-ownership-map)
- [2. Tab numbering resolution](#2-tab-numbering-resolution)
- [3. Conflict avoidance](#3-conflict-avoidance)
- [4. Work packages](#4-work-packages)
- [5. Activation state](#5-activation-state)
- [6. Commit and review conventions](#6-commit-and-review-conventions)
- [7. Handoff protocol](#7-handoff-protocol)

## 1. Ownership map

Boundaries are unchanged from Phase 6. Tab 9 is new.

| Tab | Domain | Owns (may edit) | Must not edit |
| --- | --- | --- | --- |
| **1** | Integration, architecture, dependency coordination | `aerora/CMakeLists.txt`, `aerora/cmake/`, `aerora/src/AeroraCorePlugin.*`, `.kiro/specs/05-product-completion/` | Any domain module's sources or manifest |
| **2** | Security | `aerora/src/Security/*`, `aerora/test/Security/*` | Other modules; `src/` beyond the recorded security edits |
| **3** | Vehicle | `aerora/src/Vehicle/*`, `aerora/test/Vehicle/*` | Other modules |
| **4** | Telemetry | `aerora/src/Telemetry/*`, `aerora/test/Telemetry/*`, `aerora/src/Supervisory/*` | Other modules. **Exception: one line in `aerora/test/UI/AeroraUiSafetyTest.cc` for U3** |
| **5** | Mission | `aerora/src/Mission/*`, `aerora/test/Mission/*` | Other modules |
| **6** | Payload / Power / Supervisory | `aerora/src/Payload/*`, `aerora/src/Power/*`, `aerora/src/PayloadPower.cmake`, `aerora/test/Payload/*`, `aerora/test/Power/*`, `aerora/test/Supervisory/*` | Other modules. `aerora/src/Supervisory/*` is **Tab 4's** |
| **7** | GUI | `aerora/src/UI/*`, `aerora/test/UI/*` | Any backend module; any `src/` file |
| **8** | Build / test infrastructure | `.github/`, `tools/`, `CMakePresets.json`, `cmake/presets/Aerora.json`, test-lane configuration | Overlay sources |
| **9** | Fact System / parameter authority (**new**) | `src/FactSystem/*` and the parameter authorization boundary | Overlay modules |

### Changes from Phase 6

| Change | Reason |
| --- | --- |
| `aerora/src/Supervisory/*` moves from Tab 6 to **Tab 4** | It is a telemetry primitive (`AeroraReportedValue`). The directory-absorption question stays `OPEN` and deferred, but the *ownership* is resolved now so U3-class changes have one owner |
| Tab 9 created for Fact System / `PARAM_SET` | [§2](#2-tab-numbering-resolution) |
| Tab 1 additionally owns the persistence architecture (`P7-DAT-002`) | It is cross-cutting and blocks five screens; no domain tab can own it |

## 2. Tab numbering resolution

A collision existed and is resolved here.

[29-integration-status.md](../03-implementation-specification/29-integration-status.md) proposed
"Tab 7 (new) — Fact System / parameter authority". That workstream was instead assigned **UI/QML**,
and Tab 7's own
[UI architecture map](../04-ui-implementation/00-ui-architecture-map.md#tab-numbering-collision)
flagged the collision and declined to claim Fact System, correctly — it is outside UI's declared
ownership.

**Resolution:**

| Number | Domain | Status |
| --- | --- | --- |
| Tab 7 | **GUI / QML** | Confirmed. Matches the current ownership boundaries and Tab 7's existing output |
| Tab 8 | **Build / test infrastructure** | Confirmed |
| Tab 9 | **Fact System / parameter authority** | **New.** Owns `src/FactSystem`, `PARAM_SET`, and conflict 3 |

`P7-AUTH-003` moves from `OPEN — UNASSIGNED` to `OPEN — ASSIGNED (Tab 9)`. The decision it needs is
still open; the **owner** is no longer.

### Why this matters more than numbering

`src/FactSystem/ParameterManager.cc:319` and `:398` carry the broadest reconfiguration path in the
application. Until Tab 9 exists, **no claim about parameter authorization is supportable** — the
command gate covers `MAV_CMD` traffic, and `PARAM_SET` is a different path. Stating "Aerora
authorizes vehicle commands" while that path is unowned would be the kind of overclaim this
programme exists to avoid.

## 3. Conflict avoidance

### The rule

One tab per file. Two tabs may not edit the same file, or work the same root cause, concurrently.

### Known shared-file hazards

| Hazard | Resolution |
| --- | --- |
| `aerora/test/UI/AeroraUiSafetyTest.cc` — Tab 7's file, but U3 requires Tab 4 to remove one entry from `recordedWritablePropertyDeviations()` | **Tab 4 makes the whole U3 change — header + test entry — in one commit. Tab 7 does not touch this file until U3 lands.** Removing the deviation without removing the entry fails the test on the now-absent deviation |
| `aerora/CMakeLists.txt` — every tab wants a test registered | **Only Tab 1 edits it.** A tab exports `_TEST_NAMES` from its own manifest and Tab 1 adds the `add_qgc_test` line |
| `aerora/src/AeroraCorePlugin.*` — Tab 7 needs the interceptor installed, Tab 2 owns the authorization override | **Tab 1 owns the file.** Both tabs request changes through a handoff |
| `.kiro/specs/03-implementation-specification/27-implementation-adrs.md` — contended in Phase 6 (Tab 1 created, Tab 2 extended) | **Accepted as-is.** New ADRs go in a new file with a unique number. Tab 1 reserves odd numbers, Tab 2 even, for ADRs added in Phase 7 |
| Document number 28 used twice in Phase 6 | Phase 7 documents live in `05-product-completion/` with a single owner per file. **Reproduced once at number 09 — see the row below** |
| **Document number 09 was used twice**, both untracked: `09-local-persistence-architecture.md` (Tab 1, WP1-C) and `09-parameter-write-path-and-authorization-boundary.md` (Tab 9, WP9-A) | **Resolved. Tab 9's document is now `10-parameter-write-path-and-authorization-boundary.md`** and its three self-references were updated with it. Decided on reference count rather than seniority: doc 09 had 8 inbound links from 00, 04, 07 and 08, Tab 9's had only self-references, so renumbering it touched nothing else. Tab 1 did not rename another tab's file; Tab 9 performed the move. **Allocation rule from here: Tab 1 assigns the number before a document is created** |

### Root-cause collisions to avoid

| Root cause | Owner | Other tabs must not |
| --- | --- | --- |
| Authorization policy shape | Tab 2 | Implement a parallel permission check in a domain module |
| Freshness budget values | Tab 4 | Hard-code a timeout in a panel or a model |
| Fleet identity format | Tab 3 | Key a record on MAVLink sysid |
| State-class derivation | Tab 4 | Re-derive a state class in QML or in a domain model |
| Payload safety sequencing | Tab 6 | Add a station-side interlock from mission or UI |
| Command path existence | Tab 2 + Tab 1 | Add a `Q_INVOKABLE` that sends anything |

The last row is the one with a mechanical guard: `AeroraUiSafetyTest` fails if the QML-reachable
surface grows.

## 4. Work packages

One package per tab. Each gives prerequisites, the exact editable file set, deliverables, and the
definition of done.

---

### Tab 1 — Integration, architecture, dependency coordination

**WP1-A: U1 — install the QML override interceptor**

| Field | Detail |
| --- | --- |
| Prerequisite | **None** |
| Files | `aerora/src/AeroraCorePlugin.h`, `aerora/src/AeroraCorePlugin.cc` |
| Change | Override `createQmlApplicationEngine` and `destroyQmlApplicationEngine`; call `AeroraQmlOverrides::installOn()` after the base creates the engine and `releaseFrom()` before the base deletes it. Exact code is in [HO-TAB7-02](../04-ui-implementation/01-ui-foundation-closure.md#ho-tab7-02--install-the-override-interceptor) |
| Why order matters | Install **after** creation, release **before** deletion. No `addImportPath` call is needed — the base already adds `qrc:/qml` |
| Unblocks | Every tier-2 and tier-3 QML override path. Without it an overlay override file is compiled into a resource and **never loaded, with no error** |
| Done | `IMPLEMENTED-UNCOMPILED`, static checks pass, `AeroraUiFoundationTest` extended to assert `isInstalled()` |

**WP1-B: U2 — complete the `AeroraControlsModule` block**

| Field | Detail |
| --- | --- |
| Prerequisite | **None** |
| Files | `aerora/CMakeLists.txt` (lines 160-172) |
| Change | Add `IMPORTS QGC / QGroundControl / QGroundControl.Controls` and the `qgc_set_qt_resource_alias(${AERORA_UI_QML_FILES})` call, per [HO-TAB7-03](../04-ui-implementation/01-ui-foundation-closure.md#ho-tab7-03--create-the-aeroracontrols-qml-module-target) |
| Why | Without `IMPORTS QGC`, `AeroraUi` and `AeroraTelemetryState` do not resolve inside the module. Without the alias, controls land at `qrc:/qml/Aerora/Controls/src/UI/Controls/<Name>.qml` instead of `qrc:/qml/Aerora/Controls/<Name>.qml` |
| Note | The current block's comment asserts aliasing would be "inappropriate" because `Aerora.Controls` shadows nothing. **That reasoning is wrong, and Tab 7's own manifest says so**: `aerora/src/UI/Ui.cmake:57-61` documents that the alias strips the directory prefix, without which the resource lands at `qrc:/qml/Aerora/Controls/src/UI/Controls/<Name>.qml`. `cmake/Helpers.cmake:12-22` confirms the function sets `QT_RESOURCE_ALIAS` to `get_filename_component(... NAME)`. `AERORA_UI_QML_FILES` holds absolute paths (`Ui.cmake:135-141`), so the nesting is real. Correct the comment with the change |
| Done | `IMPLEMENTED-UNCOMPILED`, manifest check passes |

**WP1-C: persistence architecture (`P7-DAT-002`)**

| Field | Detail |
| --- | --- |
| Prerequisite | **None — this is Tab 1's own decision** |
| Files | `.kiro/specs/05-product-completion/` (a new design document) |
| Deliverable | Schema, storage location per platform, retention and pruning policy, export format, and an explicit answer on whether the same store serves audit durability (`Q-037`) |
| Why first among Tab 1's design work | Blocks 5 screens, and plausibly collapses `Q-037` into the same mechanism. **Highest-leverage item on Tab 1's plate** ([05 §6](05-implementation-dependency-graph.md#the-one-tab-1-can-shorten-unilaterally)) |
| Constraint | Local-first (`P7-ARCH-010`), no automatic outbound transmission (`P7-DAT-004`), operator-controlled retention (`P7-DAT-005`) |
| Done | `DESIGNED` with a named implementation owner |

**WP1-D: build provenance source (`P7-HELP-004`)**

| Field | Detail |
| --- | --- |
| Prerequisite | None |
| Files | `aerora/CMakeLists.txt`, a new `aerora/src/AeroraBuildInfo.*` |
| Deliverable | Base commit, overlay commit, profile set, enforcement state, and whether `QGC_CUSTOM_BUILD` was actually on — the last one because its absence is a silent failure (ADR-012) |
| Unblocks | `SCR-SHL-04`, `SCR-HLP-01` |
| Done | `IMPLEMENTED-UNCOMPILED` with a test |

**WP1-E: resolve the deferred Supervisory absorption**

| Field | Detail |
| --- | --- |
| Prerequisite | None. Currently `OPEN` and deferred |
| Recommendation | **Keep deferred.** It is cosmetic, it would edit two other tabs' files, and it carries no benefit before a build exists. Phase 7 resolves the *ownership* ([§1](#1-ownership-map)) which is what U3-class changes actually needed |
| Done | Recorded as deliberately deferred with the ownership clarified |

---

### Tab 2 — Security

**WP2-A: audit writer interface against `Q-037`**

| Field | Detail |
| --- | --- |
| Prerequisite | None for the interface. **`Q-037` for the mechanism** |
| Files | `aerora/src/Security/AeroraAuditSink.h`, a new specification document |
| Deliverable | The writer interface: what a record contains, ordering guarantees, what happens when a write fails. **Do not choose a durability mechanism** — coordinate with WP1-C, which may supply the store |
| Constraint | `P7-AUDIT-003` — whether an unauditable *allow* must be denied is a **product** decision, not Tab 2's. Count losses; do not block commands |
| Done | `DESIGNED`, mechanism left `OPEN` |

**WP2-B: `D-UI-01` — a QML-safe enforcement read path**

| Field | Detail |
| --- | --- |
| Prerequisite | None |
| Files | `aerora/src/Security/*` (the accessor), coordinated with Tab 7 for the consumer |
| Deliverable | A minimal, read-only way for `SCR-SHL-03` and `SCR-SEC-01` to report whether authorization is enforced, **without** making any security type QML-visible |
| Constraint | `AeroraUiSafetyTest` must still pass unchanged. If it needs a new entry, the design is wrong |
| Done | `DESIGNED` with the consumer contract agreed with Tab 7 |

**WP2-C: do not do these**

| Prohibited this phase | Why |
| --- | --- |
| Implement `AeroraAuthorizationPolicy` | `Q-029` is open. A policy over an unapproved role set is an invented policy |
| Add a retry-time authorization check | Explicitly excluded in Phase 6. Authorization is bounded to enqueue time |
| Implement a `AeroraPlatformKeyProtector` backend | `D-006`. **An untested crypto backend is worse than none** |
| Convert `Security.cmake` further | Already converted in Phase 6. Done |

---

### Tab 3 — Vehicle

**WP3-A: U4 — `AeroraAutoPilotPlugin`**

| Field | Detail |
| --- | --- |
| Prerequisite | **None** |
| Files | new `aerora/src/Vehicle/AeroraAutoPilotPlugin.{h,cc}`, `aerora/src/Vehicle/AeroraFirmwarePlugin.{h,cc}`, `aerora/src/Vehicle/Vehicle.cmake` |
| Change | Subclass `AutoPilotPlugin`; implement `vehicleComponents()` and `prerequisiteSetup()`; return the instance from `AeroraFirmwarePlugin::autopilotPlugin(Vehicle*)` (`src/FirmwarePlugin/FirmwarePlugin.h:122`) |
| Recommendation | Prefer the JSON-driven `vehicleConfigJson()` path for parameter-driven pages. The base class then derives section ids (with `repeat` expansion probed against live params), a search keyword index, display names and per-section completeness |
| Unblocks | `SCR-VEH-01`..`06` — **6 screens** |
| Done | `IMPLEMENTED-UNCOMPILED`, `_TEST_NAMES` exported, test authored |

**WP3-B: U5 — per-vehicle tool indicators**

| Field | Detail |
| --- | --- |
| Prerequisite | None |
| Files | `aerora/src/Vehicle/AeroraFirmwarePlugin.{h,cc}` |
| Change | Override `FirmwarePlugin::toolIndicators(const Vehicle*)`; call the base, append Aerora entries, return. Each entry is a QML URL whose root exposes a `showIndicator` property |
| Note | This is the **one** indicator seam that is genuinely dynamic — it has a `toolIndicatorsChanged()` signal, unlike the `CONSTANT` `toolBarIndicators` ([03 §3](03-gui-information-architecture-and-screens.md#3-the-constant-registry-constraint)). Capability-driven indicators must go here, not there |
| Unblocks | `SCR-SHL-02` per-vehicle half |
| Done | `IMPLEMENTED-UNCOMPILED` with a test |

**WP3-C: assert the `AeroraMissionBridge::attachTo()` caller**

| Field | Detail |
| --- | --- |
| Prerequisite | None |
| Files | `aerora/test/Vehicle/AeroraVehicleLifecycleTest.cc` |
| Status | **The caller exists.** `AeroraFirmwarePlugin.cc:162` calls `(void) AeroraMissionBridge::attachTo(vehicle);` from `initializeVehicle()` — the recommended seam. Conflict 2 from the Phase 6 handoff is `RESOLVED` |
| Action | Add the assertion, so a future refactor that drops the call fails a test instead of silently producing a vehicle with no mission observation |
| Also assert | `attachTo()` idempotence — it returns the existing bridge rather than connecting a second one, which would double-report every signal into one observer (`AeroraMissionBridge.cc:70-84`) |
| Done | `IMPLEMENTED-UNCOMPILED` assertion in place |

**WP3-D: profile registry tests**

| Field | Detail |
| --- | --- |
| Prerequisite | None |
| Files | `aerora/test/Vehicle/*` |
| Deliverable | `AeroraProfileRegistryTest`, `AeroraProfileDoesNotGrantCapabilityTest` — `P7-FLEET-001`, `P7-FLEET-002`, `P7-PROF-001`, `P7-PROF-004` |
| Done | `IMPLEMENTED-UNCOMPILED` |

**WP3-E: fleet identity — specify, do not choose**

| Field | Detail |
| --- | --- |
| Prerequisite | Product decision pending |
| Deliverable | The *requirements* on a fleet identity: stability across sessions, uniqueness scope, what happens on a collision, and why MAVLink sysid cannot serve. **Do not invent a format** |
| Done | `DESIGNED` requirements, format `OPEN` |

---

### Tab 4 — Telemetry

**WP4-A: U3 — remove `WRITE` from `AeroraStateValue::capability`**

| Field | Detail |
| --- | --- |
| Prerequisite | **None** |
| Files | `aerora/src/Telemetry/AeroraStateValue.h:111` **and** `aerora/test/UI/AeroraUiSafetyTest.cc` |
| Change | Drop `WRITE setCapability` from the `Q_PROPERTY`; keep `setCapability()` as a public C++ method — the capability model needs it, and `AeroraReportedValue` already has exactly this shape |
| **Critical** | **Remove the entry from `recordedWritablePropertyDeviations()` in the same commit**, or `_onlyTheRecordedPropertyDeviationIsWritable` fails on the now-absent deviation |
| Why | `CapabilityAbsent` is the only input producing `UNAVAILABLE`. A QML write could manufacture an `UNAVAILABLE` the vehicle never reported — `AERORA-GCS-REQ-029`, `IF-UIS-005` |
| Unblocks | `SCR-LIV-02` and every screen binding an `AeroraStateValue` |
| Note | This is the **one** sanctioned cross-boundary edit in Phase 7. Tab 7 must not touch that file until this lands |
| Done | `IMPLEMENTED-UNCOMPILED`, both halves in one commit |

**WP4-B: prepare both `Q4-002` attachment variants as specifications**

| Field | Detail |
| --- | --- |
| Prerequisite | `Q4-002` is **DECIDED** (Option B / ADR-016); implementation is gated on `Q-023`/`Q-024` and `D-006` |
| Files | `aerora/src/Telemetry/*` (specification only), module README |
| Deliverable | The implementation ready to apply once the gate clears. **Keep the six fact groups `CONTRACT-ONLY`** — none has a message to parse, and inventing one violates `P7-NOINV-001` |
| Done | `DESIGNED` |

**WP4-C: freshness budget requirements — specify, do not choose**

| Field | Detail |
| --- | --- |
| Prerequisite | `Q-036` pending |
| Deliverable | What a budget must express (per-item, per-kind, or per-source), what happens at the boundary, and how a budget change propagates. **No value invented** |
| Done | `DESIGNED`, values `OPEN` |

**WP4-D: event timeline model**

| Field | Detail |
| --- | --- |
| Prerequisite | **WP1-C** (persistence design) |
| Deliverable | The model joining state transitions, operator actions and authorization outcomes on one axis — `P7-TEL-004`. Must record the **state class** of every value, which no reference product does ([02 §7.1](02-gcs-reference-benchmark.md#71-a-state-quality-vocabulary)) |
| Unblocks | `SCR-TEL-02` |
| Done | `DESIGNED` then `IMPLEMENTED-UNCOMPILED` |

**WP4-E: new ownership — `aerora/src/Supervisory/`**

Tab 4 now owns it ([§1](#1-ownership-map)). No code change required; the directory move stays
deferred.

---

### Tab 5 — Mission

**WP5-A: Aerora complex mission item — specify fully, implement when unblocked**

| Field | Detail |
| --- | --- |
| Prerequisite | **`Q-021` / `Q-023`** for the command set |
| Files | `aerora/src/Mission/*`, later a new complex-item class + editor + **two** map visuals |
| Deliverable now | The full specification: what a work surface is, what parameters it carries, how a coverage pattern is derived, plan-file round-trip keys (`canonicalName`, `jsonComplexItemTypeValue`) |
| **Do not forget** | `geoMapVisualQML()` as well as `mapVisualQML()`. The latter is pure virtual and mandatory; the former is optional and **its omission makes the item silently invisible under the GeoMap engine**. That is a defect class, not a polish item |
| Reuse | `TransectStyleComplexItem*` scaffolding and `CameraCalc*` for spacing math. Do not reinvent |
| Unblocks | `SCR-MIS-01`..`05` — 5 screens, and journeys 6.1 and 6.5 |
| Done | `DESIGNED` now; `IMPLEMENTED-UNCOMPILED` after the decision |

**WP5-B: mission/payload interaction boundary, with Tab 6**

| Field | Detail |
| --- | --- |
| Prerequisite | None |
| Deliverable | Where mission state ends and payload state begins. **The payload controller stays authoritative for payload safety** (`P7-PAY-002`), and the station does not replay payload state on resume (`P7-MIS-006`) |
| Constraint | Do not add a station-side interlock. If the boundary seems to need one, the boundary is wrong |
| Done | `DESIGNED`, agreed with Tab 6 |

**WP5-C: mission validation report (`SCR-MIS-07`)**

| Field | Detail |
| --- | --- |
| Prerequisite | None |
| Deliverable | A validation pass that reports **what it checked, what passed, and what it could not check** (`P7-CHK-002`). The third category is the one that matters and the one reference products omit |
| Done | `DESIGNED` then `IMPLEMENTED-UNCOMPILED` |

---

### Tab 6 — Payload / Power

**WP6-A: payload command boundary against Tab 5's mission work**

| Field | Detail |
| --- | --- |
| Prerequisite | None for the specification |
| Deliverable | The boundary specification. **`AeroraPayloadCommandSink` must stay unimplemented** until a command path is authorized — a sink without authorization is an unauthorized actuator |
| Constraint | Keep `kGroundStationActuatorPathImplemented` and `kAutonomousPayloadActionImplemented` at `false` |
| Done | `DESIGNED` |

**WP6-B: painting reported-value set — specify the requirements**

| Field | Detail |
| --- | --- |
| Prerequisite | **Payload ICD, `Q3-009`** |
| Deliverable | What the station must display about applicator state, coverage and consumption, expressed as *requirements on a reported-value set*. **Invent no item, no unit and no range** |
| Unblocks | `SCR-LIV-03`, and feeds `SCR-MIS-05`/`06` |
| Done | `DESIGNED`, item set `OPEN` |

**WP6-C: tether reported-value set — same discipline**

| Field | Detail |
| --- | --- |
| Prerequisite | Reported tether items undefined |
| Deliverable | Tether as its own power source with its own freshness (`P7-PWR-006`). **No switching, arbitration or transition behaviour** — the four `false` flags stay `false` |
| Unblocks | `SCR-LIV-04` |
| Done | `DESIGNED` |

**WP6-D: consumable model — record the gap, do not fill it**

| Field | Detail |
| --- | --- |
| Prerequisite | Reported-consumable set, **and an approved consumption model** |
| Why it is listed | [Benchmark §7.4](02-gcs-reference-benchmark.md#74-consumable-constrained-multi-pass-work) found that **no reference product** models a job bounded by a payload-carried consumable. It is specific to painting, firefighting and agriculture |
| Constraint | `P7-ARCH-005` prohibits a station-derived consumption projection without an approved method. `kRemainingEnergyEstimationImplemented = false` is the precedent and the same rule applies to consumables |
| Done | Gap recorded as `OPEN` with requirements stated |

**WP6-E: autonomy proposal model, with Tab 1**

| Field | Detail |
| --- | --- |
| Prerequisite | **`P7-AI-004`** — the GPU/autonomy interface |
| Deliverable | The proposal model: source, timestamp, subject, basis, lifecycle (`PROPOSED` → approved/rejected/expired). **No endpoint, schema or response may be invented** |
| Constraint | A proposal never becomes a command without an explicit operator approval event (`P7-AI-002`) |
| Unblocks | `SCR-LIV-05`, `SCR-AIS-01`..`02`, journey 6.4 |
| Done | `DESIGNED` |

---

### Tab 7 — GUI

**WP7-A: design system components**

| Field | Detail |
| --- | --- |
| Prerequisite | **U2** (module block) and **U3** (capability `WRITE`) |
| Files | `aerora/src/UI/Controls/*`, `aerora/src/UI/Ui.cmake` |
| Deliverable | The 8 composite components in [03 §5](03-gui-information-architecture-and-screens.md#composite-components-to-build). `AeroraDerivedBadge` and `AeroraPanelFrame` first — they encode `P7-ARCH-004` and `UI-RL-03` as composition rules rather than as reviewer vigilance |
| Constraint | No colour or pixel literal. `QGCPalette` + `ScreenTools` only |
| Done | `IMPLEMENTED-UNCOMPILED`, static checks pass. **Note: still unlinted — no `qmllint` on this host** |

**WP7-B: `SCR-SHL-01` Aerora Operations Hub**

| Field | Detail |
| --- | --- |
| Prerequisite | **U1** (interceptor install) |
| Files | `aerora/src/UI/*` |
| Deliverable | The navigation root, registered through `analyzePages()`, using `mainWindow.showTool()` to reach the other Aerora screens. **It must also be the overlay's honest self-report**: which services attached, which interfaces are available, whether authorization is enforced |
| Why | [D-NAV-01](03-gui-information-architecture-and-screens.md#d-nav-01--the-top-level-entry-point) — this is the zero-fork resolution of the top-level entry problem |
| Anti-requirement | **Not a dashboard of green tiles.** A tile that is green because nothing was checked is the failure mode |
| Done | `IMPLEMENTED-UNCOMPILED` with a registration test |

**WP7-C: the 24 `DESIGNED` screens, in dependency order**

| Field | Detail |
| --- | --- |
| Prerequisite | Per screen — see [03 §8](03-gui-information-architecture-and-screens.md#8-screen-inventory) |
| Order | `SCR-HLP-02` (state vocabulary, no prerequisites) → `SCR-VEH-01`..`06` (after U4) → `SCR-SHL-02` (after U5) → `SCR-LIV-01`..`02`, `06`..`09` → `SCR-MIS-08`, `SCR-VID-01`, `SCR-SET-01`..`04` |
| Constraint | Every Fly-view panel publishes its occluder rect (`UI-RL-03`). **No action controls** (`P7-FLY-004`) |
| Done | `IMPLEMENTED-UNCOMPILED` per screen |

**WP7-D: `D-UI-05` — the Aerora Qt Quick Test target**

| Field | Detail |
| --- | --- |
| Prerequisite | Tab 1 adds the `add_executable` / `add_test`; Tab 7 writes `AeroraQmlQuickTestMain.cc` |
| When | **At the first Aerora QML behaviour that needs asserting**, not before. An empty test target is a target with nothing in it |
| First candidate | `AeroraStateText` rendering `UNKNOWN` as the word rather than as an empty string — `P7-STATE-002`, which the C++ harness cannot cover |
| Shape | Mirror `test/UnitTestFramework/QmlTesting/CMakeLists.txt`: `QUICK_TEST_SOURCE_DIR` → `aerora/test/UI/qml`, `-platform offscreen`, `LABELS "Unit;QML"`, same `ENVIRONMENT` block |
| Blocks | **16 `QML`-method tests** ([06 §6](06-requirements-to-test-traceability.md#blocked-on-a-qml-test-target--16-tests)) |

**WP7-E: do not do these**

| Prohibited | Why |
| --- | --- |
| Populate `AeroraQmlOverrides::overrideEntries()` with the two action seams | `CustomGuidedActionsController.qml` and `FlyViewAdditionalCustomActionsList.qml` introduce **actions**. No Aerora action has an authorized command path |
| Fork `SelectViewDropdown.qml` | `D-NAV-01` resolves it with the hub. The fork stays deferred and available |
| Override `CustomMapItems.qml` | On `forbiddenPaths()`. Upstream populates it with an `Instantiator` over `customMapItems()` — overriding it forks a populated file **and** discards the C++ seam |
| Override any video seam | The three must be overridden as a set or not at all. Aerora has no reason to |
| Touch `aerora/test/UI/AeroraUiSafetyTest.cc` before U3 lands | Tab 4 owns that edit this phase |

---

### Tab 8 — Build and test infrastructure

**WP8-A: `D-006` — the build host**

| Field | Detail |
| --- | --- |
| Prerequisite | **User decision.** This is the programme's single largest blocker |
| Measured this pass | `cmake`, `ninja`, `qmake`, `ctest`, `qmllint`, `clang-format`, `clang-tidy`, `pre-commit`, `just` all **absent**. `C:\Qt` **absent**. `python`, `git`, `node` present. Disk insufficient and no administrator elevation — two separate gates |
| Deliverable | A host, or a documented decision to build elsewhere |
| Unblocks | **Every** `VERIFIED` claim in the programme |
| Constraint | The build-host prohibition is **unchanged** by Phase 7. No build was attempted |

**WP8-B: `Q-035` — the overlay CI test leg**

| Field | Detail |
| --- | --- |
| Prerequisite | `D-006` |
| Files | `.github/`, `cmake/presets/Aerora.json` |
| Deliverable | A CI leg configuring with `-DQGC_CUSTOM_DIR=aerora -DQGC_BUILD_TESTING=ON` and running the Aerora labels. **Must assert `QGC_CUSTOM_BUILD` is actually on** — its absence is the silent failure of ADR-012 |
| Known gap to fix | Overlay tests miss the sanitizer timeout escalation, because root `CMakeLists.txt` adds the custom directory at `:403` and `test/` at `:445`. Pass an explicit `TIMEOUT` to `add_qgc_test` for any sanitizer lane |
| Unblocks | All denial-path tests |

**WP8-C: first-build sequencing**

| Field | Detail |
| --- | --- |
| Prerequisite | `D-006` |
| Deliverable | Execute [04 §10](04-subsystem-and-interface-completion-matrix.md#build-order-for-the-first-attempt) and record the result honestly |
| Expectation to set | Compile errors are **likely** (105 files, never compiled). `qmllint` errors are **near-certain** (4 files, zero tooling). `AeroraUiSafetyTest` may fail first run on a list error rather than a regression. **None of this is a regression; it is the first measurement** |

**WP8-D: upstream drift detection**

| Field | Detail |
| --- | --- |
| Prerequisite | None |
| Deliverable | Tooling that detects a change to any of the 9 modified core files or any consumed seam signature. Class-X fork count is **0**, which is what makes this cheap — keep it there |

---

### Tab 9 — Fact System and parameter authority (new)

**WP9-A: claim ownership and assess**

| Field | Detail |
| --- | --- |
| Prerequisite | None |
| Files | `src/FactSystem/*` |
| Deliverable | An assessment of the `PARAM_SET` path at `src/FactSystem/ParameterManager.cc:319` and `:398`: who can write a parameter, from where, and what the authorization options are |
| Why it matters | Until this is answered, **no claim about parameter authorization is supportable**. The command gate covers `MAV_CMD`; `PARAM_SET` is a different path |
| Done | `DESIGNED` assessment with options, no mechanism chosen |

**WP9-B: do not do this**

Do not add a parameter authorization gate before ADR-014 settles the general authorization scope. A
second, differently-shaped gate would be worse than one gap.

## 4a. Current assignments, Phase C batch 1

Issued by Tab 1. Non-overlapping: no two tabs touch the same file. Branch for all work is
`aerora/phase7-product-completion`; verify branch and a clean tree before the first edit.

| Tab | Assignment | Prereq | Exclusive file set |
| --- | --- | --- | --- |
| **1** | WP1-D provenance; `AeroraLocalStore` per [09](09-local-persistence-architecture.md) | none | `aerora/CMakeLists.txt`, `aerora/src/AeroraCorePlugin.*`, new `aerora/src/Persistence/`, `05-product-completion/` |
| **2** | WP2-A audit writer interface against `Q-037`, now informed by [09 §7](09-local-persistence-architecture.md#7-q-037-assessment-can-this-carry-audit); WP2-B the `D-UI-01` read path | none | `aerora/src/Security/*`, `aerora/test/Security/*` |
| **3** | **U4 and U5 together**, with tests and `Vehicle.cmake` `_TEST_NAMES`; WP3-C the `attachTo` assertion | none | `aerora/src/Vehicle/*`, `aerora/test/Vehicle/*` |
| **4** | WP4-B `Q4-002` attachment variants as specifications; WP4-C freshness budget requirements; WP4-D event timeline model once `AeroraLocalStore` lands | WP4-D needs Tab 1's store | `aerora/src/Telemetry/*`, `aerora/src/Supervisory/*`, `aerora/test/Telemetry/*` |
| **5** | **`HO-TAB1-01` first — it will not compile.** Then WP5-B mission/payload boundary, WP5-C validation report | none | `aerora/src/Mission/*`, `aerora/test/Mission/*` |
| **6** | WP6-A payload command boundary; WP6-B/C reported-value *requirements* only | none for the specs | `aerora/src/Payload/*`, `aerora/src/Power/*`, `aerora/src/PayloadPower.cmake`, `aerora/test/{Payload,Power,Supervisory}/*` |
| **7** | WP7-A design-system components, then WP7-B `SCR-SHL-01`. **U1, U2 and U3 are closed, so both prerequisites are met.** Also `HO-TAB4-01` and `HO-TAB4-02` | U1/U2/U3 — **met** | `aerora/src/UI/*`, `aerora/test/UI/*` |
| **8** | WP8-D drift tooling only | `D-006` blocks everything else | `.github/`, `tools/`, presets |
| **9** | WP9-A `PARAM_SET` assessment. **Do not add a second authorization gate** | none | `src/FactSystem/*` |

### Sequencing constraints for this batch

| Constraint | Reason |
| --- | --- |
| Tab 7 must not edit `aerora/test/UI/AeroraUiSafetyTest.cc` for the U3 entry | Already done by Tab 4 in `a7b317f89`. The file is Tab 7's again from now on |
| Tab 4's WP4-D waits on `AeroraLocalStore` | The timeline model has nothing to write to until the store exists |
| Only Tab 1 edits `aerora/CMakeLists.txt` | Tabs export `_TEST_NAMES` from their own manifest; Tab 1 adds the `add_qgc_test` line |
| Tab 3's U4 adds a new source pair | Must be declared in `Vehicle.cmake` in the same commit, or the manifest check fails |
| No tab implements an operator action control | `P7-FLY-004` is `FORBIDDEN-UNTIL` an authorized command path exists |

### Reporting contract

Every tab reports, per completed work package:

| Field | Required content |
| --- | --- |
| **Files** | Exact paths added and modified. Any file outside the tab's exclusive set is a protocol violation and must be a handoff instead |
| **Tests** | Test names authored, which requirement IDs each covers, and the `add_qgc_test` label requested |
| **Manifest** | Result of `.kiro/tools/aerora-manifest-check.ps1`. A source added without a manifest entry compiles into nothing, silently |
| **Static checks** | Result of `.kiro/tools/aerora-ui-static-check.ps1` for the touched files |
| **Commit** | Hash and Conventional Commits subject |
| **Status** | Per item. **`IMPLEMENTED-UNCOMPILED` is the ceiling** until `D-006` resolves |
| **Unresolved** | Open questions, new handoffs raised, and any decision the tab declined to invent |

A report claiming `VERIFIED` is rejected. A report that is silent on the manifest is incomplete.

## 5. Activation state

| Tab | Activated | Can start immediately | First blocked item |
| --- | --- | --- | --- |
| **1** | **Yes** | WP1-A, WP1-B, WP1-C, WP1-D | — |
| **2** | **Yes** | WP2-A (interface), WP2-B | WP2-C is prohibited, not blocked |
| **3** | **Yes** | WP3-A, WP3-B, WP3-C, WP3-D | WP3-E needs a product decision |
| **4** | **Yes** | WP4-A, WP4-B, WP4-C | WP4-D needs WP1-C |
| **5** | **Yes** | WP5-B, WP5-C | WP5-A needs `Q-021`/`Q-023` |
| **6** | **Yes** | WP6-A | WP6-B/C/D/E all need external inputs |
| **7** | **Yes** | WP7-A (after U2+U3), WP7-C start on `SCR-HLP-02` | WP7-B needs U1 |
| **8** | **Partially** | WP8-D only | WP8-A/B/C all need `D-006` |
| **9** | **Yes** | WP9-A | WP9-B is prohibited |

Eight of nine tabs have work that is gated on nothing. **Tab 8 is the exception**, and it is the one
whose blocker matters most.

## 6. Commit and review conventions

| Item | Rule |
| --- | --- |
| Branch | `aerora/phase7-product-completion`. **Never** `aerora/phase6-integration-checkpoint`, `master`, or anything pushed to `origin` |
| Commit format | Conventional Commits. Type drives release automation: `feat`, `fix`, `perf`, `revert` release; `docs`, `style`, `chore`, `refactor`, `test`, `build`, `ci` do not |
| Scope | The module: `feat(Vehicle): add AeroraAutoPilotPlugin for Aerora setup pages` |
| Size | Small and reviewable. Combine fixups before publication; avoid a trail of WIP commits |
| Push | **Prohibited.** `D-005` is unresolved and `origin` is upstream |
| Tests | Authored alongside each implementation, `_TEST_NAMES` exported, Tab 1 adds the `add_qgc_test` line. **The export half is currently unenforced** — `aerora-manifest-check.ps1` never asserts that a registered test is declared by a manifest, which is how `HO-TAB1-05` went unnoticed since Phase 6. `HO-TAB1-06` closes the gap |
| Status claims | Never claim `VERIFIED`. The ceiling is `IMPLEMENTED-UNCOMPILED` until `D-006` resolves |
| Static checks | Re-run `.kiro/tools/aerora-manifest-check.ps1` after **every** commit that adds a file. A source added without a manifest entry compiles into nothing, silently |
| Coding style | [CODING_STYLE.md](../../../CODING_STYLE.md). No `Q_ASSERT`, no `QTest::ignoreMessage`, no fixed-delay `qWait`, no uncategorised `qDebug()`, 4-space indent, LF, 120 columns |
| Comments | Only where non-obvious. Comment the *why* |

### Who commits what

All nine tabs share **one working tree on one branch**. There are no per-tab worktrees, so at any
moment the tree holds the in-flight edits of several tabs at once — at this reconciliation, 19
modified and 20 untracked files belonging to Tabs 1, 2, 3 and 9 simultaneously. Everything below
follows from that single fact.

| Rule | Detail |
| --- | --- |
| **The owning tab commits its own files** | The tab that wrote the change writes the commit. Tab 1 reviews and integrates; reviewing is not committing |
| **Never `git add -A`, `git add .`, or `git commit -a`** | In a shared tree these stage other tabs' unfinished work. Stage by explicit path, every time |
| **Verify the staged set before committing** | `git status --porcelain` and `git diff --cached --stat`. If a path you do not own appears, unstage it — do not commit "while you are there" |
| **A commit spans one tab's files** | If a change genuinely needs two tabs' files, it is a handoff ([§7](#7-handoff-protocol)), not a wider commit. U3's one-line exception in `AeroraUiSafetyTest.cc` is the only sanctioned cross-boundary edit in Phase 7, and it is named in [§1](#1-ownership-map) |
| **Commit promptly once a change is validated** | An uncommitted change in a shared tree is exposed to the next tab's staging mistake. Validated and uncommitted is the risky state, not committed |

#### When a tab commits another tab's work

Allowed only when the owning tab cannot commit and the work is already validated. It happened once:
Tab 1 committed Tab 4's U3 change as `a7b317f89` while Tab 4 was still validating its diff. No work
was lost — the commit held exactly Tab 4's three files, and Tab 4 confirmed the hunks were
byte-identical to what it wrote — but it published from an intermediate state, and Tab 4 had to
follow with `54ffb4409` to finish a correction it was still making.

If it is unavoidable:

1. Tell the owning tab **before** staging, not after.
2. Stage only that tab's paths, by name.
3. Show `git diff --cached` to the owner, or verify the hunks against the owner's reported change,
   before committing.
4. Credit the owner in the commit message and say who integrated it.
5. Let the owner confirm the published state matches what they intended.

The failure this avoids is not a lost file; `git` makes that hard. It is **publishing a change its
author had not finished checking**, which is exactly what happened above and cost an extra commit.

#### Correction to `7e41a529e`'s message

`7e41a529e` absorbed Tab 1's staged reconciliation together with Tab 9's doc 10. Its message states
that `HO-TAB1-02` is `PARTIALLY APPLIED` with doc 10's self-references "still read `09-`, so they
are broken links". **That is not the state it committed.** Doc 10's three internal references at
lines 556, 562 and 568 all read `10-parameter-write-path-…`, verified against the file before
staging and again after the commit, and the `HO-TAB1-02` row inside the same commit correctly reads
`CLOSED`. The message was drafted against a mid-move snapshot and the document overtook it.

Recorded here rather than by amending: `7e41a529e` is published history on a shared branch and
rewriting it would invalidate the three tabs working on top of it. **Where a commit message and the
committed document disagree, the document is authoritative** — the message is a snapshot of
intent, the document is the artefact under review. This is also why `R7-21` exists.

## 7. Handoff protocol

A tab needing a change in another tab's file files a handoff rather than making the edit.

### ID allocation

`HO-TAB<n>-<nn>`, where `<n>` is the **raising** tab and `<nn>` is the next number **not already in
the table below**. Grep the table before choosing; do not assume the highest number you remember is
still the highest.

This rule exists because it has now been broken twice. Document number 09 was used twice, and in the
same pass `HO-TAB1-04` and `HO-TAB1-05` were each issued twice — the second time by Tab 1, which
owns this file, because rows were appended without re-reading a table that other passes had grown
concurrently. Both collisions were resolved by renumbering the entry with fewer inbound references:
Tab 1's two new rows became `HO-TAB1-08` and `HO-TAB1-09`, leaving Tab 7's closed `HO-TAB1-04` and
Tab 4's `HO-TAB1-05` untouched, since those are already referenced from module documentation.

The durable fix is the same shape as `D-UI-07`: an identifier space that is checked mechanically
rather than remembered. Until something checks it, re-reading the table is the rule.

| Field | Required |
| --- | --- |
| ID | `HO-TAB<n>-<nn>` — see [ID allocation](#id-allocation) |
| Owner | The tab that owns the file |
| File and line | Exact |
| Change | The exact text where it is short enough to quote |
| Invariant at risk | The requirement ID |
| Blocks | What cannot proceed without it |
| Tracked by | The test that will detect regression, **and any test entry that must be removed with the fix** |

### Outstanding handoffs entering Phase 7

| ID | Owner | Change | Status |
| --- | --- | --- | --- |
| HO-TAB7-01 | Tab 4 | Drop `WRITE setCapability`, remove the test entry | **CLOSED** — U3, `a7b317f89`, reviewed and integrated by Tab 1 |
| HO-TAB7-02 | Tab 1 | Install the interceptor | **CLOSED** — U1, `84bbb3a6e` |
| HO-TAB7-03 | Tab 1 | `IMPORTS QGC` + resource alias | **CLOSED** — U2, `a73699796` |
| HO-TAB7-04 | Tab 1 + Tab 7 | Aerora Qt Quick Test target | `OPEN` → WP7-D, deferred until the first QML behaviour needs asserting |

### Raised this pass

| ID | Owner | File | Change needed | Severity |
| --- | --- | --- | --- | --- |
| `HO-TAB1-01` | **Tab 5** | `aerora/test/Mission/AeroraMissionTest.cc` lines 107, 113, 120, 366, 376, 382, 390, 406, 521, 538, 570 | `UnitTest::expectLogMessage()` takes `const char *category` (`UnitTest.h:472`) and no `QString` overload exists. All 11 sites pass `QString::fromUtf8(kLogCategory)`, which does not convert. Drop the wrapper — `kLogCategory` is already `constexpr const char *`. **This will not compile.** | **CLOSED** — Tab 5, `bd1249aec`. Reviewed by Tab 1: 11 insertions, 11 deletions, one file, exactly the enumerated sites and nothing else. All 11 now pass `kLogCategory` (`AeroraMissionTest.cc:14`, `constexpr const char *`) directly, and zero `QString::fromUtf8` wrappers remain. Was **High** — it blocked `AeroraMissionTest` entirely at first build |
| `HO-TAB1-08` | **Tab 5** | `aerora/src/Mission/README.md` — L14, L15, L755-758, L763, L764, L787, L788, L974, L977 | Replace the 12 status-token uses of `VERIFIED` with `SOURCE-INSPECTED`, and replace the local redefinition at L977 with a reference to the programme table. Every one of those rows is source reading, grepping or mechanical comparison — **none involved a build**. Leave the ordinary-English uses alone (L22, L23, L36, L62, L213, L279, L325, L326, L347, L574, L841) and **do not touch L801**, where `GCS-VERIFIED` quotes an earlier phase's state name. Do not rename the `## What was actually verified` heading — `00-README` and `03` do not link to it, but the sentence at L36 does | **Medium.** Tab 1 could not apply it: the file carried 133 lines of uncommitted Tab 5 work and was open in an active editor, so editing it would have been overwritten and committing it would have swept in another tab's work |
| `HO-TAB1-09` | **Tab 3** | `aerora/src/Vehicle/README.md` — L14, L15, L966, L969 | Same change as `HO-TAB1-08`, same reason. This file independently adopted the identical convention, so the collision is wider than Tab 5 reported: **three** module documents locally redefined `VERIFIED`, two of them resolved this pass | **Medium.** Outside Tab 1's instructed scope for this pass |
| `HO-TAB1-03` | **Tab 7** | `aerora/test/UI/AeroraUiSafetyTest.cc`, `auditedTypes()` | **The overlay now registers 12 QML-reachable types and the safety audit covers 10.** Add `AeroraSecurityStatus` (new, Tab 2 WP2-B, `QML_NAMED_ELEMENT(AeroraSecurity)` + `QML_SINGLETON`) and `AeroraFreshness` (`QML_ELEMENT` + `QML_UNCREATABLE`, **unaudited since Phase 6**) | **Medium.** No live defect — both are read-only today and the independent sweep confirms no writable `Q_PROPERTY` anywhere in `aerora/src/**`. But the ratchet does not cover them, so `P7-AUTH-004`'s guarantee is narrower than it reads |
| `HO-TAB4-01` | **Tab 7** | `aerora/test/UI/AeroraUiSafetyTest.h`, `\section known` | Describes one live recorded writable-property deviation; the set is now empty | Low, documentation only |
| `HO-TAB4-02` | **Tab 7** | `aerora/src/UI/AeroraVehicleUiContext.h:63-66`, and `04-ui-implementation/01-ui-foundation-closure.md` HO-TAB7-01 | "carries a `WRITE setCapability`" is no longer true; the handoff is closed. Tab 1 independently found the same stale text and raised no duplicate | Low, documentation only — but it understates the safety posture, so it should not linger |
| `HO-TAB4-03` | Tab 1 | `05-product-completion/` 00, 03, 04, 05, 07, 08 | Close `D-UI-03` / HO-TAB7-01; `R7-13` no longer applies | **CLOSED** — but in two passes, see below |
| `HO-TAB1-02` | Tab 9 | `10-parameter-write-path-and-authorization-boundary.md` | Document number 09 was already taken by WP1-C, so the file renumbered to `10-` | **CLOSED.** Re-checked at 15:53 against the file itself: all three internal references (lines 556, 562, 568) read `10-parameter-write-path-…`, and `00-README` carries the doc-10 row. An earlier draft of this row called them broken links; that was written against a mid-move state and is withdrawn. Raised and closed inside this reconciliation |
| `HO-TAB1-04` | **Tab 7** | `04-ui-implementation/00-ui-architecture-map.md:533` and `aerora/src/UI/README.md:45` | **Two further stale HO-TAB7-01 rows that `HO-TAB4-01` and `HO-TAB4-02` do not name.** Both still read in the present tense and list the handoff as live: the first as "`AeroraStateValue::capability` is QML-writable, so QML could manufacture an `UNAVAILABLE`", the second as "Tab 4 — make `AeroraStateValue::capability` read-only in QML / Latent; not reachable". U3 (`a7b317f89`) closed both. Owner column should also stop pointing at Tab 4 | Low, documentation only — but it is the `R7-20` failure mode recurring in files outside the six that `HO-TAB4-03` swept |
| `HO-TAB1-05` | **Tab 4** | `aerora/src/Telemetry/Telemetry.cmake` | **The manifest never declares `_TEST_NAMES`.** `AeroraTelemetryStateTest` and `AeroraTelemetryAttachmentTest` have been registered straight into the aggregation since `738462422` instead of being exported by the module, which is why `aerora-manifest-check.ps1` reports 16 declared names against 18 `add_qgc_test` entries. Export both names so the module owns its own test list, per [§3](#3-conflict-avoidance) | **Low — no live defect.** Both tests build and run today; the gap is in the manifest contract, and it makes the checker's 16-vs-18 asymmetry permanent and unexplained. **Pre-existing since Phase 6; not caused by U3 or any Phase C commit** |
| `HO-TAB1-07` | Tab 1 (self) | `00-README.md`, `01-…plan.md`, `02-…benchmark.md`, `05-…dependency-graph.md`, `06-…traceability.md` | **363 double-encoded character sequences.** UTF-8 bytes were decoded as cp1252 and re-encoded, so `—` is stored as `â€”` (`U+00E2 U+20AC U+201D`), `→` as `â†'` and `§` as `Â§`. Docs 03, 04, 07, 08, 09 and 10 are clean. **Functional symptom: exactly one broken anchor** — this document's WP7-D "Blocks" cell links `06#blocked-on-a-qml-test-target--16-tests`, which is correct for a real em dash and cannot resolve against the corrupted heading | **Low.** Cosmetic in rendering, one broken link. **Wants its own commit** per [CODING_STYLE](../../../CODING_STYLE.md#formatting-tools) — a 363-site substitution across five files must not ride along inside a reconciliation change, and a partial repair would leave one document internally inconsistent |
| `HO-TAB1-06` | **Tab 8** | `.kiro/tools/aerora-manifest-check.ps1`, checks 4–6 (lines 122-181) | **The checker cannot detect `HO-TAB1-05`'s class.** Check 4 asserts declared ⊆ registered, check 5 self-registering ⊆ registered, check 6 registered ⊆ self-registering. **Nothing asserts registered ⊆ declared**, so a test may self-register and hold a `add_qgc_test` line while no module manifest ever exports it. The 16-vs-18 counts are printed at line 149 but not compared. Add the fourth direction | **Low**, and it is the durable fix: the `_TEST_NAMES` contract in [§6](#6-commit-and-review-conventions) is currently unenforced in the one direction that matters, which is the same hand-maintained-list failure mode as `D-UI-07` |

`HO-TAB1-01` was the significant one, and it is now closed. It was found by source inspection rather
than by a compiler: `expectLogMessage` has exactly one declaration, the argument type does not
convert, and the call sites were enumerated mechanically. The same scan confirmed the other 47 call
sites across seven test files are correct, and that every test-side category string matches its
`QGC_LOGGING_CATEGORY` definition exactly — a mismatch there would be a silent test failure rather
than a compile error.

It is also the cleanest demonstration of the [§6](#6-commit-and-review-conventions) protocol working:
Tab 5 owned the file, made the change, committed it alone, and the integration review was a diff
stat plus a grep. **The fix is still `IMPLEMENTED-UNCOMPILED`** — the type now matches by
inspection, which is not the same as having compiled.

`HO-TAB4-03` closed in two passes, and the reason is worth recording. The first pass updated every
**status row** — `D-UI-03`, HO-TAB7-01, `R7-13`, the U3 entries in 04 and 05 — and left five
**prose passages** in the same six documents still describing the writable property as a live
defect: `04 §3`'s "The one recorded deviation" section and two of its table rows, `00-README`'s
state-vocabulary note, `03 §6`'s `UI-ST-07` paragraph, and `08 §6`'s mitigation row. A reader
arriving at any of those would have concluded the opposite of the status table two sections away.
**Closing a status row is not closing a handoff.** A handoff against documentation is closed when
the prose agrees with the code, so the grep for the old claim — not the status column — is what
ends it.

`HO-TAB1-04` is the same lesson applied twice over: `HO-TAB4-03`'s sweep was scoped to the six
`05-product-completion/` documents it named, so two equivalent stale rows outside that set survived
both passes. The scope of a documentation handoff has to be the grep, not a file list.

### Test-declaration attribution

`HO-TAB1-05` is **Tab 4's alone**, and the attribution is worth stating because the obvious guess is
wrong. The two tests missing from a manifest are `AeroraTelemetryStateTest` and
`AeroraTelemetryAttachmentTest`, both in `aerora/test/Telemetry/`.

| Module | Manifest declares `_TEST_NAMES` | New tests this phase | Declared? |
| --- | --- | --- | --- |
| Vehicle (**Tab 3**) | Yes — 4 names | `AeroraVehicleSetupTest`, `AeroraProfileRegistryTest` | **Both declared.** Nothing outstanding |
| Security (**Tab 2**) | Yes — 5 names | `AeroraAuditSinkTest`, `AeroraSecurityStatusTest` | **Both declared.** Nothing outstanding |
| Telemetry (**Tab 4**) | **No — the `set(..._TEST_NAMES ...)` block is absent** | none this phase | **Neither declared**, since Phase 6 |
| Mission, UI, Payload/Power | Yes — 2, 2, 3 names | none | Declared |

Both tabs that added tests during Phase C declared them correctly in the same commit that added the
source. **Tab 3's declarations are complete and must not be recorded as in flight or attributed
elsewhere.** The 16-versus-18 gap predates Phase 7 entirely.

HO-TAB7-03's partial application is worth noting: the `qt_add_qml_module` block exists, so the
handoff looks done. The two lines that make it function are absent, and the omission carries a
comment asserting the alias would be "inappropriate". That is the kind of handoff that a status
column marks closed and a build marks broken.

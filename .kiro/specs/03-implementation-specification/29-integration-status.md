# Integration Status

Owner: **Tab 1 (orchestrator / integration authority)**. Authoritative integration state for the
parallel workstreams. Nothing here is compiled; see [Build readiness](#build-readiness).

Verified against `HEAD = 9459a4b57986f40df8bdc193d5453c0596fbb416`, branch `master`.

## Repository state

| Field | Value |
| --- | --- |
| HEAD | `9459a4b57986f40df8bdc193d5453c0596fbb416` |
| `git describe` | `v5.2.0-dev-102-g9459a4b57` |
| Branch | `master`, tracking `origin/master` |
| Remote | `origin` → `https://github.com/mavlink/qgroundcontrol.git`, **fetch and push** |
| Tracked files modified | **9** |
| Untracked trees | `.kiro/`, `aerora/`, `Aerora_Universal_GCS_Product_Baseline_Specification_v4.1/` |
| Destructive git operations performed | **None** |

Tracked modifications — eight from the security workstream's approved core edits, plus
`src/Vehicle/Vehicle.cc` from the calibration authorization seam ([ADR-015](32-integration-adrs.md)):

```text
 M src/API/QGCCorePlugin.h
 M src/MAVLink/Signing/CMakeLists.txt
 M src/MAVLink/Signing/MAVLinkSigningKeys.cc
 M src/MAVLink/Signing/MAVLinkSigningKeys.h
 M src/Vehicle/MavCommandQueue.cc
 M src/Vehicle/Vehicle.cc
 M src/Vehicle/VehicleTypes.h
 M test/MAVLink/Signing/CMakeLists.txt
 M test/Vehicle/CMakeLists.txt
```

`origin` still points at upstream for push, so **no Aerora work may be pushed** (`D-005`).

## Integration matrix

| Domain | Owner | Status | Files | Dependencies | Open decisions | Build/Test |
| --- | --- | --- | --- | --- | --- | --- |
| Security | Tab 2 | `IMPLEMENTED-UNCOMPILED` | 7 in `aerora/src/Security/`, 2 `AeroraCorePlugin.*`, 6 core files modified, 2 core files added | `QGCCorePlugin`, `MavCommandQueue`, `MAVLinkSigningKeys` | `Q-029`, `Q-037`/`Q4-003`, ADR-014, platform key protection, unauditable-allow, authorization lifetime | Tests authored in the core test tree; chain tests need an overlay build (`Q-035`) |
| Vehicle | Tab 3 | `IMPLEMENTED-UNCOMPILED` | 15 sources, 6 test sources, `Vehicle.cmake` | QGC `FirmwarePlugin`, `FirmwarePluginFactory` | `Q4-002`, `Q4-004`, `Q-018`, `Q-007`, fleet identity, capability identifiers | 2 tests registered by Tab 1 |
| Telemetry | Tab 4 | `IMPLEMENTED-UNCOMPILED` | 12 sources, 2 test sources, `Telemetry.cmake` | `QGC::Timing` | `Q-036`, `Q4-002`/`R4-001`, `Q3-009`, `IF-MAV-022`, message rates, Aerora dialect, source timestamps | 1 test registered by Tab 1 |
| Mission | Tab 5 | `IMPLEMENTED-UNCOMPILED` | 14 sources, 6 test sources, `Mission.cmake` | **Hard: Aerora Vehicle module** (`AeroraVehicleServices`), QGC `MissionManager` | `Q-021`, `Q-023`, no caller for `attachTo()` | 2 tests registered by Tab 1 |
| Payload / Power | Tab 6 | `IMPLEMENTED-UNCOMPILED` | 7 sources across `Payload/`, `Power/`, `Supervisory/`; 8 test sources; `PayloadPower.cmake` | **Hard: Telemetry** (state, freshness) and **Vehicle** (capabilities); `QGC::Timing` | Power arbitration, tether/battery transition, thresholds, safety actions, `Q-023`, `Q-024` — all `OPEN` | 2 tests registered by Tab 1 |
| Supervisory | Tab 6 | `IMPLEMENTED-UNCOMPILED` | 2 files, `AeroraReportedValue.*`, covered by `PayloadPower.cmake` | Telemetry | Absorption into M-Telemetry — `OPEN`, assigned to Tab 1 | Covered by the Payload/Power tests |
| Shared CMake | Tab 1 | `IMPLEMENTED-UNCOMPILED` | `aerora/CMakeLists.txt` | All module manifests | `include(QGCTest)` side effects, accepted | Static manifest check passed, 55 entries |
| Root integration | Tab 1 | `IMPLEMENTED-UNCOMPILED` | docs 29, 30; ADR-014 recorded `OPEN` | — | ADR-014, `PARAM_SET` ownership | n/a |

## Parallel workstream status

| Tab | Domain | Manifest | Integrated | Last observed activity |
| --- | --- | --- | --- | --- |
| 2 | Security | none — listed inline, by Tab 2's request | **Yes** | Settled |
| 3 | Vehicle | `Vehicle.cmake` | **Yes** | Settled |
| 4 | Telemetry | `Telemetry.cmake` | **Yes** | Settled |
| 5 | Mission | `Mission.cmake` | **Yes** | Manifest published after its sources; complete |
| 6 | Payload / Power | `PayloadPower.cmake` | **Yes** | Settled; one manifest covers Payload, Power and Supervisory |
| — | Supervisory | covered by `PayloadPower.cmake` | **Yes** | Not a separate module — see below |

Every module was integrated only after confirming its manifest was complete: each declared source
and test source was resolved and checked against disk. **All five domain manifests are now
aggregated.** Nothing is left unintegrated.

`src/Supervisory/` is **not** a sixth module. It holds the single type Payload and Power share,
`AeroraReportedValue`, placed in its own directory so neither module depends on the other for it.
Tab 6 records that absorbing it into M-Telemetry is a directory rename plus one manifest line with
no API change, and assigns the call to Tab 1. Tab 1 has **deferred** it: the move is cosmetic, it
would require editing Tab 6's manifest and Tab 4's file set, and it carries no functional benefit
before a build exists. Recorded `OPEN`.

## CMake integration

Tab 1 modified exactly one build file: `aerora/CMakeLists.txt`. No module manifest was edited.

### Convention — no conflict to arbitrate

Tabs 3, 4 and 5 independently converged on the same manifest convention, and each documented it in
its own file:

- variables only — no targets, no `add_test()`, nothing in `PARENT_SCOPE`;
- exports `AERORA_<MODULE>_SOURCES` / `_INCLUDE_DIRECTORIES` / `_LIBRARIES`;
- exports `AERORA_<MODULE>_TEST_SOURCES` / `_TEST_INCLUDE_DIRECTORIES` / `_TEST_NAMES`;
- aggregation explicitly delegated to `aerora/CMakeLists.txt`.

Tab 1 adopted it unchanged. There was no convention A versus B to decide between — the only
inconsistency is that the **security module has no manifest** and is listed inline, which Tab 2's
integration note explicitly requested. Converting it is a Tab 2 change, not an aggregation change.

### What the aggregation does

| Item | Detail |
| --- | --- |
| Manifests included | `Telemetry.cmake`, `Vehicle.cmake`, `Mission.cmake`, `PayloadPower.cmake` — in that order, honouring Mission's declared dependency on Vehicle and PayloadPower's on both Telemetry and Vehicle |
| Exported | `CUSTOM_SOURCES`, `CUSTOM_INCLUDE_DIRECTORIES`, `CUSTOM_LIBRARIES`, `CUSTOM_DEFINITIONS`, `CUSTOM_QT_COMPONENTS` |
| Consumption verified | `src/CMakeLists.txt:203-222`, each guarded by `if(QGC_CUSTOM_BUILD)` |
| Append-then-set | Every `list(APPEND ...)` precedes a single `PARENT_SCOPE` `set` per variable, as all three manifests warn is required |
| Duplicate protection | `list(REMOVE_DUPLICATES)` on sources, includes and libraries after aggregation |
| Test gating | All test sources and all `add_qgc_test` calls inside `if(QGC_BUILD_TESTING)` |
| Libraries | `QGC::Timing` from Telemetry; Mission exports an empty list |
| Definitions | `QGC_CUSTOM_BUILD`, `CUSTOMHEADER="AeroraCorePlugin.h"`, `CUSTOMCLASS=AeroraCorePlugin` |

Sources are compiled into the application target rather than a static library. That is required:
`AeroraFirmwarePluginFactory` self-registers through a non-local static, and a linker may discard
such an object from an archive when no symbol references it. The failure is silent — the build
succeeds and no Aerora plugin is ever registered.

### Test registrations added

| Test | Labels | Owner |
| --- | --- | --- |
| `AeroraTelemetryStateTest` | `Unit` | Tab 4 |
| `AeroraVehicleIntegrationTest` | `Unit` | Tab 3 |
| `AeroraVehicleLifecycleTest` | `Integration Vehicle` | Tab 3 |
| `AeroraMissionTest` | `Unit MissionManager` | Tab 5 |
| `AeroraMissionSyncTest` | `Integration Vehicle MissionManager` | Tab 5 |
| `AeroraPayloadModelTest` | `Unit` | Tab 6 |
| `AeroraPowerModelTest` | `Unit` | Tab 6 |
| `AeroraCalibrationAuthorizationTest` | `Unit` | Tab 2 |
| `AeroraTelemetryAttachmentTest` | `Unit` | Tab 4 |

**Nine** Aerora tests are registered, all inside the `QGC_BUILD_TESTING` guard. None has been
executed.

Labels are taken from each manifest's documented snippet, not chosen by Tab 1.

### Known, accepted side effects

`include(QGCTest)` is used rather than `cmake_language(DEFER ...)`, for consistency with the snippet
all three manifests document. Two consequences, both flagged by the manifests themselves:

- `QGCTest.cmake` has `include_guard(GLOBAL)` and defines the `check` / `check-unit` targets, so
  including it from the overlay moves those targets into the overlay directory. They are global by
  name, so `ninja check` still resolves.
- The overlay is configured before `test/` (root `CMakeLists.txt:403` adds the custom directory,
  `:445` adds `test/`), so the sanitizer timeout escalation inside `test/CMakeLists.txt` does not
  reach overlay-registered tests. An explicit `TIMEOUT` is needed if a sanitizer lane requires one.

### Static validation performed

CMake cannot be run (`D-006`). As a substitute, every file path declared by the three integrated
manifests was resolved and checked:

| Check | Result |
| --- | --- |
| Manifest file entries resolved | **70** across all five manifests |
| Declared files missing from disk | **0** |
| Production/test sources on disk but absent from their manifest | **0** |

This is a file-existence check. **It is not a configure and must not be read as one.**

## Cross-domain conflicts

No contradiction was found between any two domain implementations. The items below are integration
gaps and process conflicts, each with an owner.

| # | Conflict | Evidence | Affected | Owner | Recommended resolution | Blocking? |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | **Telemetry has no attachment path to a vehicle.** No Vehicle-module file references any telemetry type, and no telemetry type is registered through `FirmwarePlugin::factGroups()` | Searched `aerora/src/Vehicle/**` for `AeroraTelemetrySet`, `AeroraFactGroupBase`, `AeroraTelemetryState` — no matches | Tab 3, Tab 4 | **Architecture** | Resolve `Q4-002`. Both tabs deliberately avoided the shared-plugin `factGroups()` hazard, so this is a missing decision, not a defect | **Yes** for telemetry reaching a vehicle; not for either module's own tests |
| 2 | **`AeroraMissionBridge::attachTo()` has no caller.** Every candidate seam lives in a file owned by another workstream | `Mission.cmake` records it; the candidates are the Aerora firmware plugin's `initializeVehicle`, the Aerora core plugin, or `MultiVehicleManager::vehicleAdded` | Tab 3, Tab 5, Tab 2 | **Tab 3**, as owner of the vehicle lifecycle seam | Call it from `initializeVehicle()` where `AeroraVehicleServices` is already attached | **Yes** for mission observation being live |
| 3a | **`Vehicle::startCalibration()` bypassed command authorization** — **RESOLVED** | `Vehicle.cc:2328`, packs at `:2395-2404` | Tab 1, Tab 3 | Tab 1 | Closed by [ADR-015](32-integration-adrs.md): authorization evaluated after the parameter switch, before the pack | No — resolved |
| 3 | **`PARAM_SET` has no owning workstream.** `src/FactSystem` is assigned to no tab, yet carries the broadest reconfiguration path in the application | `src/FactSystem/ParameterManager.cc:319`, `:398` | — | **Assign explicitly** — candidate Tab 7 | Create a Fact System / parameter authority owner | **Yes** for any claim about parameter authorization |
| 4 | **Document number 28 used twice.** `28-layer-1-security-command-boundary.md` (Tab 1) and `28-security-implementation-note.md` (Tab 2) | Both present in `.kiro/specs/03-implementation-specification/` | Tab 1, Tab 2 | **Tab 1** | Renumber the Tab 1 document; Tab 2's is the domain report and the more detailed as-built record | No |
| 5 | **`27-implementation-adrs.md` is a contended file.** Created by Tab 1, subsequently extended by Tab 2 with the enumerated bypass inventory | File mtime 11:36 with content Tab 1 did not write | Tab 1, Tab 2 | **Tab 1** | Accepted as-is: Tab 2's content is more complete and verified. Tab 1 attempted an edit, detected the change, and did not overwrite | No |
| 6 | **Security module lacked a `.cmake` manifest** — **RESOLVED** | Tab 2 published `aerora/src/Security/Security.cmake` | Tab 2 | Tab 2 | Closed. Tab 1 removed the inline Security source list and now aggregates the manifest; verified 0 duplicate and 0 inline-clash registrations | No — resolved |
| 7 | **Manifest location convention varies.** Telemetry, Vehicle and Mission place their manifest inside the module directory; Payload/Power places one manifest at `aerora/src/` covering three directories | `aerora/src/PayloadPower.cmake` vs `aerora/src/<Module>/<Module>.cmake` | Tab 6, Tab 1 | **Tab 1** | Accepted. The variance is justified in Tab 6's manifest — two modules share one type, and splitting would duplicate the entry or create a dependency between them. The exported-variable contract is identical, which is what the aggregation depends on | No |
| 8 | **Overlay tests miss the sanitizer timeout escalation** | Root `CMakeLists.txt:403` vs `:445` ordering | All overlay tests | Tab 1 | Pass explicit `TIMEOUT` when a sanitizer lane is added | No |

## Open decision register

| ID | Decision | Owner | Blocking? | Current status |
| --- | --- | --- | --- | --- |
| `D-004` | Radio transport / `ADR-002` | Hardware | No | `DEFERRED` — radio not attached |
| `D-005` | Aerora Git origin | User | **Yes** | `BLOCKED` — no Aerora URL; `origin` is upstream |
| `D-006` | Build host | User | **Yes** | `NO-GO` |
| `D-007` | §27 product authorization | External | **Yes** for product features | `EXTERNAL EVIDENCE REQUIRED` |
| `Q-029` | Operator role set | Product + operations | **Yes** | `OPEN` — no authorization decisions are evaluated |
| ADR-014 | Outbound authorization scope, Options A/B/C | **Architecture** | **Yes** for any general authorization claim | `OPEN — ARCHITECTURAL DECISION REQUIRED` |
| — | Authorization lifetime: re-evaluate on retry? | Architecture + Tab 2 | No | `OPEN` — bounded to four non-actuating commands; no second check added |
| — | Must an unauditable *allow* be denied? | Product | No | `OPEN` — losses counted, commands not blocked |
| — | Platform key protection per OS | Tab 2 + architecture | No | `OPEN` — no backend on any of the five target platforms |
| `Q-037` / `Q4-003` | Audit integrity mechanism | Security | No | `OPEN` — sink reports neither durable nor tamper-evident |
| `Q4-002` / `R4-001` | Per-vehicle FactGroup attachment | Architecture → Tab 4 | No longer blocking the decision; blocking the *feature* | **DECIDED** — Option B, ADR-016. Implementation gated on `Q-023`/`Q-024` and `D-006`. `R4-001` stays open upstream |
| ADR-015 | Calibration authorization seam | Tab 1 | No | **ACCEPTED**, `IMPLEMENTED-UNCOMPILED` |
| `Q4-004` | Consequences of disabling vendor factories | Product | No | `OPEN` — neither factory disabled |
| `Q-018` | Aerora `MAV_AUTOPILOT` value | Firmware | No | `OPEN` — Aerora claims Generic; no value invented |
| `Q-007` | Flight-mode / authority mapping | Firmware + product | No | `OPEN` |
| `Q3-009` | Capability presence reporting | Firmware | No | `OPEN` |
| `Q-036` | Freshness thresholds | Product | No | `OPEN` — no value invented |
| `IF-MAV-022` | Telemetry source attribution | Dialect | No | `OPEN` |
| `Q-021` / `Q-023` | Aerora complex item / mission command set | Product | No | `OPEN` — no extension registered |
| `Q-035` / `R-021` | Overlay test configuration | Tab 1 + CI | **Yes** for denial tests | `OPEN` — needs an overlay leg with `QGC_BUILD_TESTING=ON` |
| — | Fleet identity format | Product | No | `OPEN` — MAVLink sysid is explicitly not the fleet identity |
| — | Capability identifiers | Product | No | `OPEN` |
| — | Power arbitration, tether/battery transition, thresholds, safety actions | Product + engineering | No | `OPEN` — none invented |
| — | Absorb `src/Supervisory/` into M-Telemetry? | **Tab 1** | No | `OPEN` — deferred; cosmetic, and would edit two other tabs' files |
| — | `src/FactSystem` / `PARAM_SET` ownership | **Programme** | **Yes** for parameter authorization | `OPEN — UNASSIGNED` |

## Build readiness

| Item | State |
| --- | --- |
| Compiler (MSVC) | **Absent** |
| CMake ≥ 3.25 | **Absent** |
| Ninja | **Absent** |
| Qt 6.11.1 | **Absent** |
| GStreamer 1.28.4 | **Absent** |
| `ctest` / test execution | **Not possible** |
| `clang-format`, `clang-tidy`, `pre-commit` | **Absent** |
| Disk | Insufficient; cache cleanup established as not sufficient (`R4-003`) |
| Elevation | Not administrator — a gate separate from disk |
| Python, git, node | Present |

Status: **`BLOCKED — BUILD ENVIRONMENT`**. No compilation, launch, runtime or integration-test
result is claimed anywhere in this document or in any document Tab 1 owns.

Additional precondition beyond tooling: the overlay must be configured with
`-DQGC_CUSTOM_DIR=aerora`, otherwise `QGCCorePlugin::instance()` does not route to `CUSTOMCLASS` and
the authorization gate is never consulted.

## Recommended next parallel work

### Parallel-safe

| Tab | Work package |
| --- | --- |
| 2 | Convert `aerora/src/Security/` to the `.cmake` manifest convention. Specify the audit writer interface against `Q-037` without choosing a mechanism. Do **not** add a retry-time check |
| 3 | Call `AeroraMissionBridge::attachTo()` from `initializeVehicle()` — conflict 2, and Tab 3 owns the seam. Record the `startCalibration()` authorization decision |
| 4 | Keep the six fact groups `CONTRACT-ONLY`. Prepare both `Q4-002` attachment variants as specifications so the decision can be applied cheaply once taken |
| 5 | Mission module is complete and integrated. Next: specify payload/mission interaction boundaries with Tab 6 **without** replaying payload state |
| 6 | Payload and Power are complete and integrated. Next: define the payload command boundary against Tab 5's mission work, keeping the payload controller authoritative for payload safety. Keep every threshold and transition `OPEN` |
| 7 (new) | **Fact System / parameter authority.** Owns `src/FactSystem`, `PARAM_SET`, and conflict 3 |
| 8 (new) | **Build host and CI.** Owns `D-006`, `Q-035` overlay test leg, and upstream drift tooling |

### Must be sequential

| Work | Blocked by |
| --- | --- |
| Arming the authorization policy | `Q-029`, then ADR-014 |
| Attaching telemetry to vehicles | `Q4-002` |
| Any compile, test or verification claim | `D-006`, then `Q-035` |
| Any Aerora commit | `D-005` |
| Any product feature | `D-007` |
| Platform key protection backends | `D-006` — an untested crypto backend is worse than none |

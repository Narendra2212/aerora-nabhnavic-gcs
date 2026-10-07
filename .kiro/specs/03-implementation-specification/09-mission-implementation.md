# Mission Implementation

Mapping QGroundControl's mission architecture onto Aerora, and the three things Phase 4 is forbidden
to touch.

## Upstream architecture, as inherited

| Element | Role | Aerora posture |
| --- | --- | --- |
| `PlanMasterController` | Owns the plan: mission, geofence, rally points. Drives upload and download | Unchanged |
| `MissionController` | Mission item list, visual items, sequencing | Unchanged |
| `MissionManager` | The protocol transaction with the vehicle | Unchanged |
| Complex items | A pattern that flattens to primitive mission items on upload | **The Aerora extension point** |
| Plan creators | Entry points that build a plan from a template | Aerora adds one per product |
| `FirmwarePlugin::supportedMissionCommands` | The command set offered to the editor | Returns the standard set until a dialect exists |
| Vehicle mission store | The vehicle's own copy | Observed, never assumed |

Verified connection points on `Vehicle`: `MissionManager` is constructed in `_commonInit` and its
`currentIndexChanged` and `newMissionItemsAvailable` signals drive index tracking and trajectory
clearing (`src/Vehicle/Vehicle.cc` `_commonInit` region).

## Plan file authority

| Property | Position | Requirement |
| --- | --- | --- |
| The plan file is the authoritative record of operator intent | Required | `AERORA-MISSION-REQ-001` |
| The vehicle mission store is a **copy**, not the record | Required | `AERORA-MISSION-REQ-001` |
| An uploaded mission not verified against the plan file is presented as **unverified** | Required | `AERORA-MISSION-REQ-007` |
| Post-flight reconstruction uses the plan file and the log, **never** the vehicle store | Required | `AERORA-MISSION-REQ-008` |
| Mission item completion is never inferred; the vehicle-reported index is presented | Required | `AERORA-MISSION-REQ-006` |

The fourth row has a specific reason: resume **replaces** the vehicle mission, so the post-flight
store is not the planned mission. Reconstructing from it would silently reconstruct the resumed
mission and call it the plan.

## Aerora complex items

| Aspect | Decision | Status |
| --- | --- | --- |
| Mechanism | A complex item plus plan creator, following the pattern the reference overlay demonstrates | `IMPLEMENTATION-PLANNED` |
| Layer | **L2 product profile**, never L3 | `IMPLEMENTATION-PLANNED` |
| Registration | `QGCCorePlugin::complexMissionItemNames` and `createComplexMissionItem`, pattern at `custom-example/src/CustomPlugin.cc:305+` | `IMPLEMENTATION-PLANNED` |
| Map visual and editor | Aerora Plan QML module | `IMPLEMENTATION-PLANNED` |
| Flattening to primitive items on upload | Required, like every complex item | `IMPLEMENTATION-PLANNED` |
| Painting pattern geometry and parameters | **Not specified** — Q-023 and product definition | `CONTRACT-ONLY` |
| Survey-style insert behaviour | Default accepted — Q-021, R-018 | `CONTRACT-ONLY` |

### The pattern-survives-download problem

R-012: a complex item does not survive a download from the vehicle, because only the flattened
primitive items come back.

| Position | Detail |
| --- | --- |
| The pattern is a plan-file construct | It exists in the plan, not on the vehicle |
| A downloaded mission is primitive items | The pattern is not recoverable from them |
| Aerora does not attempt reconstruction | That would be inferring operator intent |
| The plan file remains authoritative | Which is why `AERORA-MISSION-REQ-001` is stated the way it is |
| Whether a painting plan must survive download | **Q-022, OPEN** |

This is why the plan-file-authority rule is load-bearing rather than tidy: without it, a downloaded
mission would be mistaken for the plan, and the pattern would appear to have been lost.

## Resume — the prohibited area

Three prohibitions, all carried from Phase 2.1 and ADR-005.

| Prohibited | Reason | Phase 4 compliance |
| --- | --- | --- |
| Implementing ground-station payload state replay | ADR-005; rejected in Phase 2.1. `IF-MIS-015` carries nothing | **Not implemented** |
| Modifying QGC's resume filtering | ADR-005; rejected in Phase 2.1. `IF-MIS-016` carries nothing | **Not modified** |
| Assuming payload state persistence | Evidence matrix item 14 is `NOT-EVIDENCED` | **Not assumed** |

### What upstream resume actually does

Verified in Phase 2 and unchanged: resume rebuilds the mission from the current index and replays a
**fixed upstream set** of state-setting commands. The resume index filter uses a local variable and
an `i >= resumeIndex` clause, with no virtual to override — hence R-002.

| Consequence | Aerora response |
| --- | --- |
| A state-setting command before the resume index is not replayed | Accepted. The vehicle store is replaced; the ground station says so |
| Aerora payload commands would be dropped from a resume | **The plan constraint is the mitigation** |
| Retention might make it moot | `NOT-EVIDENCED`; cannot be relied on |

### The plan-constraint fallback

The mitigation, unconditional and independent of whether retention exists.

| Element | Behaviour | Requirement |
| --- | --- | --- |
| Plan constraint | Plans are constrained so a resume cannot leave payload state ambiguous | `AERORA-MISSION-REQ-003` rationale |
| Resume warning | The operator is warned at resume time that state set before the resume point is not re-established | Same |
| Applied when? | **Always**, not only when retention is unevidenced | Same |
| Requirement identifier | **None of its own** — Phase 3 was prohibited from creating one | R3-008 |
| Where it is carried | `AERORA-MISSION-REQ-003` rationale, the evidence matrix, and criterion 28 of [24-build-readiness.md](24-build-readiness.md) | Same |

This is the clearest instance of R3-008: a real, unconditional engineering obligation that lives in a
rationale rather than a requirement. Phase 4 carries it in the build-readiness gate so it cannot be
lost, and still cannot create the requirement.

## Interface status

| Interface | Subject | Status |
| --- | --- | --- |
| `IF-MIS-001` to `IF-MIS-006` | Plan authority, upload, download, verification, index | `IMPLEMENTATION-PLANNED` |
| `IF-MIS-007` | Mission pause | `CONTRACT-ONLY` — vehicle semantics depend on Q-007 |
| `IF-MIS-008`, `IF-MIS-009` | Plan identity, partial-upload handling | `IMPLEMENTATION-PLANNED` |
| `IF-MIS-010` | Resume | `IMPLEMENTATION-PLANNED` — observation and warning only |
| `IF-MIS-011` | Aerora complex item | `CONTRACT-ONLY` — geometry is product definition |
| `IF-MIS-012` | Aerora mission command set | `CONTRACT-ONLY` — dialect |
| `IF-MIS-013` | Mission verification | `IMPLEMENTATION-PLANNED` |
| `IF-MIS-014` | Payload state interaction on resume | `CONTRACT-ONLY` — `BENCH` evidence needed |
| `IF-MIS-015` | Ground-station payload state replay | `IMPLEMENTATION-PLANNED` as a **prohibition assertion** |
| `IF-MIS-016` | Modification of the resume filter | `IMPLEMENTATION-PLANNED` as a **prohibition assertion** |

The last two are implementable now and the implementation is a test: assert no code path reaches a
payload replay, and assert the upstream resume source is unmodified. R3-007 asks for exactly this
conversion from inspection to assertion.

## Test strategy

| Test | Method | Asserts |
| --- | --- | --- |
| Plan file is authoritative | `INTEGRATION` | An uploaded-then-downloaded mission is compared against the plan, and a mismatch is presented as unverified |
| Partial upload | `INTEGRATION` | The vehicle store is presented as indeterminate, not as the plan |
| Index tracking | `INTEGRATION` | The vehicle-reported index is presented; completion is never inferred |
| Resume warning | `INTEGRATION` | A resume produces an operator warning regardless of retention |
| Plan constraint | `UNIT` | A plan that would leave payload state ambiguous is refused at planning time |
| No replay path exists | `UNIT` | **R3-007 conversion** |
| Resume filter unmodified | `STATIC` | **R3-007 conversion** — the upstream source is unchanged |
| Complex item flattening | `UNIT` | A pattern flattens to primitive items deterministically |
| Pattern does not survive download | `INTEGRATION` | The known behaviour is asserted, so a future change is caught |

The last row is worth noting: asserting a **limitation** stops it silently becoming a regression in
the other direction.

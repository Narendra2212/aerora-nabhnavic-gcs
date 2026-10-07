# Traceability

Every identifier, where it comes from, and what it connects to. This is the navigation document: if
a reviewer wants to know what a decision rests on or what a risk is answered by, it is answerable
from here.

Revised in Phase 2.1 for the R-016 reconciliation, the R-002 reclassification, the three new
documents and the corrected counts.

## Document inventory

Twenty-nine documents, in the order a reviewer should read them.

| # | Document | Purpose |
| --- | --- | --- |
| 1 | [README.md](README.md) | Entry point, nine architectural principles, reading order |
| 2 | [phase-2-risk-gates.md](phase-2-risk-gates.md) | All 20 Phase 1 risks re-gated; R-001 reclassified |
| 3 | [phase-2-open-question-resolution.md](phase-2-open-question-resolution.md) | All 34 Phase 1 questions triaged; 6 resolved |
| 4 | [system-architecture.md](system-architecture.md) | The four-layer stack and what each layer may do |
| 5 | [component-architecture.md](component-architecture.md) | Fifteen Aerora modules, four dependency rules |
| 6 | [runtime-architecture.md](runtime-architecture.md) | Threads, startup, freshness, degraded operation |
| 7 | [deployment-architecture.md](deployment-architecture.md) | Platform scope, signing, packaging |
| 8 | [communication-architecture.md](communication-architecture.md) | Link model, serial-first transport |
| 9 | [mavlink-architecture.md](mavlink-architecture.md) | Dialect strategy, interface separation, seams |
| 10 | [vehicle-integration.md](vehicle-integration.md) | Firmware abstraction, FactGroups, parameters, power |
| 11 | [gpu-integration.md](gpu-integration.md) | Companion observability, read-only by construction |
| 12 | [payload-architecture.md](payload-architecture.md) | Payload state authority; stops at the enumeration |
| 13 | [mission-architecture.md](mission-architecture.md) | R-002 analysis, plan-file authority |
| 14 | [cloud-architecture.md](cloud-architecture.md) | Cloud off the command path, eight named traps |
| 15 | [ui-architecture.md](ui-architecture.md) | Three seam tiers, override inventory |
| 16 | [authority-model.md](authority-model.md) | Authority, eight command states, failsafe boundary |
| 17 | [security-architecture.md](security-architecture.md) | Identity, authorisation, key custody |
| 18 | [logging-architecture.md](logging-architecture.md) | Three records, local-first, upload defaults |
| 19 | [configuration-architecture.md](configuration-architecture.md) | Version control, build, settings, distribution |
| 20 | [data-model.md](data-model.md) | Entities, identity, the value model |
| 21 | [testing-architecture.md](testing-architecture.md) | Architectural-property tests, verification limits |
| 22 | [requirements.md](requirements.md) | 120 requirements with full records |
| 23 | [architecture-decisions.md](architecture-decisions.md) | Eleven ADRs with rejected alternatives |
| 24 | [risk-gates.md](risk-gates.md) | Nine Phase 2 risks with gates |
| 25 | [open-questions.md](open-questions.md) | Nine Phase 2 questions |
| 26 | traceability.md | This document |
| 27 | [phase-2-1-reconciliation.md](phase-2-1-reconciliation.md) | Phase 2.1 — every contradiction, overclaim and count error corrected, with amendment history |
| 28 | [hardware-firmware-evidence-matrix.md](hardware-firmware-evidence-matrix.md) | Phase 2.1 — twenty-two vehicle-side items with evidence status and confidence |
| 29 | [ratification.md](ratification.md) | Phase 2.1 — all 120 requirements classified for ratification, human decisions, Phase 3 entry criteria |

Documents 1 to 26 are the Phase 2 set. Documents 27 to 29 were added in Phase 2.1 and change no
architectural decision; they reconcile the record and prepare it for review.

A reviewer with limited time should read 29, then 28, then 27, then the Phase 2 documents those
three point into.

## Requirements by deriving document

| Deriving document | Requirement range | Count | Risks addressed | Questions depended on |
| --- | --- | --- | --- | --- |
| [communication-architecture.md](communication-architecture.md) | `AERORA-COMM-REQ-006`–`012` | 7 | R-003 | Q-012, Q-014, Q-015, Q-032 |
| [mavlink-architecture.md](mavlink-architecture.md) | `AERORA-MAV-REQ-005`–`010` | 6 | R-011 | Q-003, Q-010, Q-017 |
| [vehicle-integration.md](vehicle-integration.md) | `AERORA-GCS-REQ-007`–`014`, `AERORA-POWER-REQ-007`–`008` | 10 | R-009, R-010, R-017, R-029 | Q-007, Q-017, Q-018, Q-023, Q-024 |
| [gpu-integration.md](gpu-integration.md) | `AERORA-GPU-REQ-001`–`008` | 8 | R-014 | Q-008, Q-031 |
| [payload-architecture.md](payload-architecture.md) | `AERORA-PAYLOAD-REQ-006`–`013` | 8 | R-002 | Q-009, Q-023, Q-043 |
| [mission-architecture.md](mission-architecture.md) | `AERORA-MISSION-REQ-001`–`008` | 8 | R-002, R-012, R-018 | Q-021, Q-023, Q-041 |
| [cloud-architecture.md](cloud-architecture.md) | `AERORA-AWS-REQ-005`–`015` | 11 | R-015, R-016 | Q-016, Q-027, Q-029, Q-030, Q-039, Q-042 |
| [ui-architecture.md](ui-architecture.md) | `AERORA-UI-REQ-004`–`010` | 7 | R-019, R-026 | Q-007, Q-020, Q-029, Q-040 |
| [authority-model.md](authority-model.md) | `AERORA-SAFE-REQ-006`–`011`, `AERORA-GCS-REQ-015`–`019` | 11 | R-006, R-025 | Q-007, Q-008, Q-009, Q-029, Q-036 |
| [security-architecture.md](security-architecture.md) | `AERORA-SEC-REQ-011`–`020` | 10 | R-004, R-005, R-007, R-020, R-023, R-024 | Q-010, Q-016, Q-027, Q-029, Q-032 |
| [logging-architecture.md](logging-architecture.md) | `AERORA-GCS-REQ-020`–`025`, `AERORA-COMPLY-REQ-002`–`005` | 10 | R-022, R-027 | Q-016, Q-029, Q-037, Q-039 |
| [configuration-architecture.md](configuration-architecture.md) | `AERORA-BUILD-REQ-004`–`012` | 9 | R-008, R-011, R-014, R-023 | Q-003, Q-006, Q-010, Q-017, Q-031, Q-038 |
| [data-model.md](data-model.md) | `AERORA-VEH-REQ-001`–`005`, `AERORA-GCS-REQ-026`–`029` | 9 | R-017 | Q-005, Q-007, Q-016, Q-023, Q-029, Q-036, Q-037 |
| [testing-architecture.md](testing-architecture.md) | `AERORA-TEST-REQ-004`–`009` | 6 | R-009, R-019, R-021, R-026 | Q-020, Q-035 |
| Total | | **120** | | |

The count reconciles with [requirements.md](requirements.md): 120 published, one withdrawn
(`AERORA-UI-REQ-009`), 119 active, across 15 identifier domains.

Every one of the 120 is additionally classified for ratification in
[ratification.md](ratification.md), and the classification was cross-checked against this register
by script: no registered requirement is unclassified and no classification refers to an identifier
that does not exist.

## Risks to mitigating requirements

All 29 risks across both registers. A risk with no mitigating requirement is either resolved, or
owned outside GCS engineering, and the table says which.

| Risk | Title | Gate | Mitigated by |
| --- | --- | --- | --- |
| R-001 | Qt licensing decision gates the delivery model | `G-RELEASE` | None — open legal question, ADR-009 |
| R-002 | Payload state commands silently dropped from a resume mission | `G-ARCH` | `AERORA-MISSION-REQ-003` unconditionally; `AERORA-PAYLOAD-REQ-007` and `AERORA-MAV-REQ-008` **conditionally — unevidenced, see evidence matrix item 14** |
| R-003 | Transport extensibility requires core-tree modification | `G-ARCH` | `AERORA-COMM-REQ-006`, ADR-002 |
| R-004 | No application authentication or authorisation exists | `G-ARCH` | `AERORA-SEC-REQ-011`–`015` |
| R-005 | MAVLink signing keys stored unprotected | `G-RELEASE` | `AERORA-SEC-REQ-016`–`018` |
| R-006 | Signing off by default, no link-layer encryption | `G-OPEN` | `AERORA-SAFE-REQ-010` partially; Q-010, Q-011 |
| R-007 | No signature verification on updates or firmware | `G-RELEASE` | `AERORA-SEC-REQ-020` |
| R-008 | Checkout targets upstream, not an Aerora fork | `G-IMPL` | `AERORA-BUILD-REQ-004`, `AERORA-BUILD-REQ-005` |
| R-009 | Host cannot build QGroundControl | `G-IMPL` | `AERORA-TEST-REQ-004`, `AERORA-GCS-REQ-009` |
| R-010 | Firmware plugin factory conflict is silent | `G-IMPL` | `AERORA-GCS-REQ-007`, `AERORA-TEST-REQ-004` |
| R-011 | Reproducibility broken by floating dependency references | `G-IMPL` | `AERORA-BUILD-REQ-008`, `AERORA-SEC-REQ-020` |
| R-012 | Mission patterns do not survive a vehicle download | `G-INTEG` | `AERORA-MISSION-REQ-001`, `AERORA-MISSION-REQ-002` |
| R-013 | MAVLink channel budget is 15 | `G-INTEG` | None — a resource budget recorded in [runtime-architecture.md](runtime-architecture.md); Q-015 |
| R-014 | Overlay maintenance burden against a moving upstream | `G-OPEN` | `AERORA-BUILD-REQ-006`, `AERORA-BUILD-REQ-007`, `AERORA-UI-REQ-004` |
| R-015 | Platform artefacts are not signed by the build | `G-RELEASE` | `AERORA-SEC-REQ-020`; Q-032 |
| R-016 | Qt HTTP server purpose is untraced | `G-OPEN` — amended in Phase 2.1 | `AERORA-AWS-REQ-014`; security premise disproved, hygiene open as Q-027 — see reconciliation below |
| R-017 | Vehicle identity is system-id only | `G-ARCH` | `AERORA-VEH-REQ-001`–`005`, `AERORA-GCS-REQ-014` |
| R-018 | Survey-style insert unavailable to a supplied pattern | `G-OPEN` | None — Q-021; accepting the default keeps it core-edit free |
| R-019 | Silent failure of QML and resource overrides | `G-IMPL` | `AERORA-UI-REQ-007`, `AERORA-TEST-REQ-007` |
| R-020 | Per-dependency licence inventory unverified | `G-RELEASE` | `AERORA-SEC-REQ-020`; Q-005 |
| R-021 | Overlay test reachability unverified | `G-IMPL` | `AERORA-TEST-REQ-009`; Q-035 |
| R-022 | Log auto-upload default targets a third party | `G-IMPL` | `AERORA-GCS-REQ-023`, `AERORA-GCS-REQ-024` |
| R-023 | Settings-version bump destroys signing keys | `G-RELEASE` | `AERORA-SEC-REQ-018`, `AERORA-BUILD-REQ-009` |
| R-024 | Key-derivation salt is fixed across installations | `G-RELEASE` | `AERORA-SEC-REQ-016` |
| R-025 | Acknowledgement correlation cannot distinguish commands | `G-IMPL` | `AERORA-GCS-REQ-018`, `AERORA-COMPLY-REQ-002` |
| R-026 | Override drift is invisible to tooling | `G-IMPL` | `AERORA-UI-REQ-005`, `AERORA-UI-REQ-006`, `AERORA-TEST-REQ-008` |
| R-027 | CSV columns misalign with runtime-created FactGroups | `G-OPEN` | `AERORA-GCS-REQ-022` |
| R-028 | Authority mode and flight mode may be conflated | `G-IMPL` | `AERORA-SAFE-REQ-006`, `AERORA-GCS-REQ-011` |
| R-029 | Single-firmware UI simplification unreachable | `G-OPEN` | None — informational; `AERORA-UI-REQ-004` gives the alternative route |

Six risks have no mitigating requirement. Five of those are deliberate: an open legal question
(R-001), a resource budget (R-013), a product decision (R-018), an informational finding (R-029),
and a partially-mitigated open question (R-006). The sixth is R-016, whose Phase 2.1 reconciliation is below.

### Reconciliation: R-016

Resolved in Phase 2.1. The full record is in
[phase-2-1-reconciliation.md](phase-2-1-reconciliation.md); the short form:

| Phase | Position |
| --- | --- |
| Phase 1 | `MEDIUM`, "purpose is untraced" |
| Phase 2, first pass | `MEDIUM` at `G-IMPL`, explicitly "still untraced; Phase 2 did not investigate it" |
| Phase 2, later pass | Traced in [cloud-architecture.md](cloud-architecture.md) — linked for two type aliases, no server constructed, no listening socket bound |
| Phase 2.1 | **`LOW` at `G-OPEN`.** The security premise is disproved; the residual item is dependency hygiene |

The two Phase 2 positions contradicted each other because they were written in sequence. The earlier
text is **retained verbatim** in [phase-2-risk-gates.md](phase-2-risk-gates.md) under an amendment
heading rather than deleted, so the sequence remains auditable.

R-016 is **not** closed. The security question is answered; the hygiene decision is Q-027, and the
assurance obligation that keeps the answer true is `AERORA-AWS-REQ-014`.

## Open questions to dependent requirements

Only questions with a dependent Phase 2 requirement are listed. Phase 1 questions with no Phase 2
dependency are in [phase-2-open-question-resolution.md](phase-2-open-question-resolution.md).

| Question | Subject | Impact | Requirements depending on it |
| --- | --- | --- | --- |
| Q-003 | Aerora fork strategy | `ARCH-SHAPING` | `AERORA-BUILD-REQ-004`, `AERORA-BUILD-REQ-005`, `AERORA-MAV-REQ-005` |
| Q-005 | Third-party licence audit | `DEFERRABLE` | `AERORA-SEC-REQ-020` |
| Q-006 | Branding and artwork removal | `IMPL-BLOCKING` | None directly; Q-040 |
| Q-007 | Failsafe behaviour and mode mapping | `ARCH-SHAPING` | `AERORA-SAFE-REQ-010`, `AERORA-GCS-REQ-011` |
| Q-008 | Proposed versus effective autonomy command | `ARCH-SHAPING` | `AERORA-GPU-REQ-005` |
| Q-009 | Payload emergency-stop behaviour | `ARCH-SHAPING` | `AERORA-PAYLOAD-REQ-008`, `AERORA-SAFE-REQ-011` |
| Q-010 | Default signing policy | `DEFERRABLE` | `AERORA-SEC-REQ-017` |
| Q-015 | Worst-case concurrent link count | `DEFERRABLE` | None; R-013 |
| Q-016 | Fleet vehicle identity | `ARCH-SHAPING` | `AERORA-VEH-REQ-002`, `AERORA-GCS-REQ-014` |
| Q-017 | Single-stack or multi-stack | `ARCH-SHAPING` | `AERORA-GCS-REQ-007` |
| Q-018 | Distinct autopilot identifier | `IMPL-BLOCKING` | `AERORA-GCS-REQ-007` |
| Q-020 | Resource alias shadowing | `IMPL-BLOCKING` | `AERORA-UI-REQ-007`, `AERORA-TEST-REQ-007` |
| Q-021 | Survey-style insert behaviour | `DEFERRABLE` | None; R-018 |
| Q-023 | Payload actuator interfaces | `ARCH-SHAPING` | `AERORA-MISSION-REQ-005`, `AERORA-PAYLOAD-REQ-013` |
| Q-024 | Frozen power architecture | `DEFERRABLE` | `AERORA-POWER-REQ-007` |
| Q-027 | HTTP server dependency | `IMPL-BLOCKING` | `AERORA-AWS-REQ-014` |
| Q-029 | Operator role model | `ARCH-SHAPING` | `AERORA-SEC-REQ-013`, `AERORA-GCS-REQ-019` |
| Q-030 | Permitted cloud dependencies and degradation | `ARCH-SHAPING` | `AERORA-AWS-REQ-007` |
| Q-031 | Is video required | `DEFERRABLE` | None |
| Q-032 | Platform scope | `IMPL-BLOCKING` | `AERORA-SEC-REQ-020` |
| Q-035 | Aerora test placement and reachability | `IMPL-BLOCKING` | `AERORA-TEST-REQ-009`, `AERORA-BUILD-REQ-006` |
| Q-036 | Per-command corroboration windows | `DEFERRABLE` | `AERORA-GCS-REQ-016` |
| Q-037 | Audit record integrity mechanism | `ARCH-SHAPING` | `AERORA-GCS-REQ-028` |
| Q-038 | Upstream merge cadence and owner | `IMPL-BLOCKING` | `AERORA-BUILD-REQ-007`, `AERORA-UI-REQ-006` |
| Q-039 | Log retention and upload policy | `DEFERRABLE` | `AERORA-COMPLY-REQ-005` |
| Q-040 | Visual design language | `DEFERRABLE` | None |
| Q-041 | Mission verification depth | `DEFERRABLE` | `AERORA-MISSION-REQ-007` |
| Q-042 | Cloud synchronisation conflict policy | `ARCH-SHAPING` | `AERORA-AWS-REQ-007`, `AERORA-BUILD-REQ-012` |
| Q-043 | Payload calibration and diagnostics | `DEFERRABLE` | `AERORA-PAYLOAD-REQ-013` |

## Architecture decisions

| ADR | Requirements | Risks | Questions |
| --- | --- | --- | --- |
| ADR-001 Overlay strategy | `AERORA-BUILD-REQ-006`, `AERORA-GCS-REQ-007`, `AERORA-GCS-REQ-009`, `AERORA-GCS-REQ-010`, `AERORA-UI-REQ-004`, `AERORA-TEST-REQ-009` | R-009, R-010, R-014, R-019 | Q-003, Q-017, Q-035 |
| ADR-002 Transport strategy | `AERORA-COMM-REQ-006` | R-003 | Q-012, Q-014 |
| ADR-003 Serial radio strategy | `AERORA-COMM-REQ-011`, `AERORA-COMM-REQ-012` | R-003 | Q-012, Q-032 |
| ADR-004 MAVLink extension strategy | `AERORA-MAV-REQ-005`–`010`, `AERORA-BUILD-REQ-008` | R-011 | Q-003 |
| ADR-005 Mission and payload state | `AERORA-MISSION-REQ-001`–`008`, `AERORA-PAYLOAD-REQ-006`, `AERORA-PAYLOAD-REQ-007`, `AERORA-MAV-REQ-008` | R-002, R-012 | Q-019, Q-023; evidence matrix item 14 |
| ADR-006 Cloud architecture | `AERORA-AWS-REQ-005`–`015`, `AERORA-TEST-REQ-005` | R-015 | Q-027, Q-030, Q-039, Q-042 |
| ADR-007 Modular monolith | `AERORA-AWS-REQ-012`, `AERORA-GPU-REQ-003` | — | Q-030 |
| ADR-008 Security architecture | `AERORA-SEC-REQ-011`–`020`, `AERORA-BUILD-REQ-009` | R-004, R-005, R-023, R-024 | Q-010, Q-029, Q-037 |
| ADR-009 Qt licensing neutrality | — | R-001 | Q-004 |
| ADR-010 Failsafe boundary | `AERORA-SAFE-REQ-008`–`011` | R-006 | Q-007 |
| ADR-011 Companion authority boundary | `AERORA-GPU-REQ-001`–`008`, `AERORA-MAV-REQ-007` | R-014 | Q-008 |

ADR-009 is the only decision with no requirement. That is correct: a decision to be
licence-**neutral** imposes no obligation on the architecture — it is the absence of a dependency,
and there is nothing to verify beyond the absence itself.

## Phase 1 to Phase 2 continuity

### Requirement identifier continuity

| Domain | Phase 1 | Phase 2 | Combined |
| --- | --- | --- | --- |
| `AERORA-GCS-REQ-` | 001–006 (6) | 007–029 (23) | 29 |
| `AERORA-SEC-REQ-` | 001–010 (10) | 011–020 (10) | 20 |
| `AERORA-AWS-REQ-` | 001–004 (4) | 005–015 (11) | 15 |
| `AERORA-COMM-REQ-` | 001–005 (5) | 006–012 (7) | 12 |
| `AERORA-BUILD-REQ-` | 001–003 (3) | 004–012 (9) | 12 |
| `AERORA-PAYLOAD-REQ-` | 001–005 (5) | 006–013 (8) | 13 |
| `AERORA-SAFE-REQ-` | 001–005 (5) | 006–011 (6) | 11 |
| `AERORA-MAV-REQ-` | 001–004 (4) | 005–010 (6) | 10 |
| `AERORA-UI-REQ-` | 001–003 (3) | 004–010 (7) | 10 |
| `AERORA-TEST-REQ-` | 001–003 (3) | 004–009 (6) | 9 |
| `AERORA-POWER-REQ-` | 001–006 (6) as `PWR` | 007–008 (2) | 8 |
| `AERORA-MISSION-REQ-` | none | 001–008 (8) | 8 |
| `AERORA-GPU-REQ-` | none | 001–008 (8) | 8 |
| `AERORA-COMPLY-REQ-` | 001 (1) | 002–005 (4) | 5 |
| `AERORA-VEH-REQ-` | none | 001–005 (5) | 5 |
| Total | **55** | **120** | **175** |

Phase 1's 55 identifiers were verified by enumerating them from the Phase 1 documents; Phase 2's 120
were verified by enumerating them from [requirements.md](requirements.md). Both counts are machine
derived, not asserted.

### The power domain alias

| Phase 1 identifier | Phase 2 canonical | Status |
| --- | --- | --- |
| `AERORA-PWR-REQ-001` | `AERORA-POWER-REQ-001` | Alias; Phase 1 text unchanged |
| `AERORA-PWR-REQ-002` | `AERORA-POWER-REQ-002` | Alias; Phase 1 text unchanged |
| `AERORA-PWR-REQ-003` | `AERORA-POWER-REQ-003` | Alias; Phase 1 text unchanged |
| `AERORA-PWR-REQ-004` | `AERORA-POWER-REQ-004` | Alias; Phase 1 text unchanged |
| `AERORA-PWR-REQ-005` | `AERORA-POWER-REQ-005` | Alias; Phase 1 text unchanged |
| `AERORA-PWR-REQ-006` | `AERORA-POWER-REQ-006` | Alias; Phase 1 text unchanged |

The Phase 2 directive names the domain `AERORA-POWER-REQ-`, which outranks a Phase 1 analysis
output, so that spelling is canonical from Phase 2 onward. The numbers are preserved rather than
renumbered because identifier stability matters more than tidiness, and the Phase 1 documents are
not edited because they are a frozen, human-reviewed baseline.

Either spelling resolves to the same requirement. A future phase should retire the alias once no
artifact references it.

### Risk and question identifier continuity

| Identifier class | Phase 1 | Phase 2 | Combined |
| --- | --- | --- | --- |
| `R-` risks | R-001–R-020 (20) | R-021–R-029 (9) | 29 |
| `Q-` questions | Q-001–Q-034 (34) | Q-035–Q-043 (9) | 43 |
| `ADR-` decisions | none | ADR-001–ADR-011 (11) | 11 |
| `QGC-BASE-` baseline facts | 43 | none added | 43 |
| Evidence matrix items | none | 1–22, added in Phase 2.1 | 22 |
| Ratification classes | none | 5, applied to all 120 requirements in Phase 2.1 | 5 |

Phase 2 added no `QGC-BASE-` identifiers. New QGroundControl findings in Phase 2 are cited by file
and line in the document that uses them, rather than given a baseline identifier, because they were
discovered in service of a decision rather than as part of a systematic survey. A future phase that
re-surveys should fold them in.

## Gate summary across both phases

| Gate | Risks | Questions blocking |
| --- | --- | --- |
| `G-ARCH` | R-002, R-003, R-004, R-017 | None — zero architecture-blocking questions remain. R-002 and R-017 carry unevidenced vehicle dependencies: evidence matrix items 14 and 22 |
| `G-IMPL` | R-008, R-009, R-010, R-011, R-019, R-021, R-022, R-025, R-026, R-028 | Q-006, Q-018, Q-020, Q-032, Q-035, Q-038 |
| `G-INTEG` | R-012, R-013 | Q-013, Q-025, Q-026 |
| `G-RELEASE` | R-001, R-005, R-007, R-015, R-020, R-023, R-024 | Q-004, Q-005 |
| `G-OPEN` | R-006, R-014, R-016, R-018, R-027, R-029 | Q-027 |

**The architecture can be ratified.** All four `G-ARCH` risks are addressed by Phase 2 decisions,
and no question in either register is architecture-blocking.

Seven risks block release. Four concentrate in two areas — signing key custody (R-005, R-023,
R-024) and the open licensing question (R-001) — and neither area is an architectural problem.

## What a reviewer should check first

If review time is limited, these five items carry the most weight. Each is a place where the
architecture makes a claim that, if wrong, changes the plan.

| Item | Where | Why it matters |
| --- | --- | --- |
| Zero QGC core edits are required by the current architecture | [vehicle-integration.md](vehicle-integration.md), ADR-001 | The whole maintenance argument rests on it; it is verified seam by seam against source |
| The R-002 resolution | [mission-architecture.md](mission-architecture.md), ADR-005 | It moves a GCS problem to the payload controller; the payload team must agree |
| The key custody position | [security-architecture.md](security-architecture.md), ADR-008 | One QGC core edit remains a possible fallback for signing-key storage if upstream extensibility is unavailable; three release-blocking risks |
| The failsafe refusal to specify | [authority-model.md](authority-model.md), ADR-010 | Deliberately incomplete; a reviewer may disagree that it should be |
| The 91 proposed requirements | [requirements.md](requirements.md) | Three quarters of the register is Phase 2 reasoning, not pre-existing mandate |
| The evidence matrix, one item of 22 confirmed | [hardware-firmware-evidence-matrix.md](hardware-firmware-evidence-matrix.md) | The honest state of the vehicle-side inputs this architecture depends on |
| The three `ASSUMED` evidence rows | Same, items 8, 10, 14 | Assumptions with no evidence read exactly like facts in a design document |

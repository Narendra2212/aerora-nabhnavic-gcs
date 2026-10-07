# Ratification

What can be ratified now, what cannot, and what has to happen before Phase 3 starts.

Phase 2 produced an architecture and a requirements register. Phase 2.1 asks a different question of
the same material: **for each item, what must happen before a human can accept it?** That is not the
same as asking where it came from, which is what the requirement `Status` field records.

## Classification

| Class | Meaning | Action required |
| --- | --- | --- |
| `CONFIRMED` | Traceable to a baseline or directive statement, with no gating condition | Ratify |
| `PROPOSED — ACCEPTABLE` | A Phase 2 derivation within ground-station control, implementable as written | Ratify, or reject with a reason |
| `PRODUCT DECISION REQUIRED` | Cannot be implemented until a product, operations or process decision is taken | Assign the decision to an owner |
| `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Depends on vehicle-side behaviour that is not evidenced | Produce the evidence, or adopt the fallback |
| `REJECT/WITHDRAW` | Not to be carried forward | Retire the identifier |

### The classification rule

Each requirement is classified by **the gating condition for implementing and verifying it**, not by
whether its wording is agreeable.

This matters for a specific group. Several requirements state a *shape* that is ratifiable now while
their *content* waits on evidence — `AERORA-SAFE-REQ-010` requires failsafe state, reason, source,
severity and action to be represented separately, which is sound regardless of what the values turn
out to be. Those are classified `HARDWARE/FIRMWARE EVIDENCE REQUIRED` because that is the
decision-useful fact: a reviewer can accept the shape today and still cannot get the requirement
implemented until the vehicle side lands. Where that applies, the row says so.

**No `PROPOSED` requirement has been promoted to `CONFIRMED` in Phase 2.1.** The `Status` field in
[requirements.md](requirements.md) is unchanged except where a factual error was corrected. The
classification below is a separate, additive judgement.

## Summary

| Class | Count | Share of active |
| --- | --- | --- |
| `CONFIRMED` | 29 | 24% |
| `PROPOSED — ACCEPTABLE` | 59 | 50% |
| `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | 21 | 18% |
| `PRODUCT DECISION REQUIRED` | 10 | 8% |
| Active total | 119 | |
| `REJECT/WITHDRAW` | 1 | — |
| Published total | 120 | |

**Eighty-eight of 119 active requirements — 74% — can be ratified in this review.** The remaining 31
split into 21 waiting on vehicle evidence and 10 waiting on a human decision. None is waiting on
further architecture work.

## Ground station core

| ID | Class | Gating condition |
| --- | --- | --- |
| `AERORA-GCS-REQ-007` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-GCS-REQ-008` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-GCS-REQ-009` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-GCS-REQ-010` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-GCS-REQ-011` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Q-007 — the flight-mode mapping. Rule ratifiable; mapping is not |
| `AERORA-GCS-REQ-012` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Firmware must place flight-controller parameters on component 1 |
| `AERORA-GCS-REQ-013` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Matrix item 10 — no capability inventory exists |
| `AERORA-GCS-REQ-014` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Matrix item 22 — no Aerora identifier exists |
| `AERORA-GCS-REQ-015` | `CONFIRMED` | — |
| `AERORA-GCS-REQ-016` | `PRODUCT DECISION REQUIRED` | Q-036 — corroboration window values. Rule ratifiable |
| `AERORA-GCS-REQ-017` | `CONFIRMED` | — |
| `AERORA-GCS-REQ-018` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-GCS-REQ-019` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-GCS-REQ-020` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-GCS-REQ-021` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-GCS-REQ-022` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-GCS-REQ-023` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-GCS-REQ-024` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-GCS-REQ-025` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-GCS-REQ-026` | `CONFIRMED` | — |
| `AERORA-GCS-REQ-027` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Matrix items 2 and 12 — reporting source requires the dialect to identify originators |
| `AERORA-GCS-REQ-028` | `PRODUCT DECISION REQUIRED` | Q-037 — integrity mechanism |
| `AERORA-GCS-REQ-029` | `CONFIRMED` | — |

## MAVLink

| ID | Class | Gating condition |
| --- | --- | --- |
| `AERORA-MAV-REQ-005` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-MAV-REQ-006` | `CONFIRMED` | — |
| `AERORA-MAV-REQ-007` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-MAV-REQ-008` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Matrix items 10 and 14 — the dialect does not exist and resume behaviour is undesigned |
| `AERORA-MAV-REQ-009` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Matrix item 10 — the capability inventory is a vehicle obligation |
| `AERORA-MAV-REQ-010` | `PROPOSED — ACCEPTABLE` | — |

## Communications

| ID | Class | Gating condition |
| --- | --- | --- |
| `AERORA-COMM-REQ-006` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Matrix item 6 — the radio's host-side presentation is unverified. **ADR-002 rests on this** |
| `AERORA-COMM-REQ-007` | `CONFIRMED` | — |
| `AERORA-COMM-REQ-008` | `CONFIRMED` | — |
| `AERORA-COMM-REQ-009` | `CONFIRMED` | — |
| `AERORA-COMM-REQ-010` | `CONFIRMED` | — |
| `AERORA-COMM-REQ-011` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Matrix item 8 — a hardware identifier decision |
| `AERORA-COMM-REQ-012` | `PROPOSED — ACCEPTABLE` | — |

## Mission

| ID | Class | Gating condition |
| --- | --- | --- |
| `AERORA-MISSION-REQ-001` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-MISSION-REQ-002` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-MISSION-REQ-003` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-MISSION-REQ-004` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-MISSION-REQ-005` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Q-023, matrix item 15 — the actuator model |
| `AERORA-MISSION-REQ-006` | `CONFIRMED` | — |
| `AERORA-MISSION-REQ-007` | `PRODUCT DECISION REQUIRED` | Q-041 — verification depth. Rule ratifiable |
| `AERORA-MISSION-REQ-008` | `PROPOSED — ACCEPTABLE` | — |

## Payload

| ID | Class | Gating condition |
| --- | --- | --- |
| `AERORA-PAYLOAD-REQ-006` | `CONFIRMED` | — |
| `AERORA-PAYLOAD-REQ-007` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | **Matrix item 14. The headline Phase 2.1 reclassification.** Fallback adopted |
| `AERORA-PAYLOAD-REQ-008` | `CONFIRMED` | — |
| `AERORA-PAYLOAD-REQ-009` | `CONFIRMED` | — |
| `AERORA-PAYLOAD-REQ-010` | `CONFIRMED` | — |
| `AERORA-PAYLOAD-REQ-011` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-PAYLOAD-REQ-012` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-PAYLOAD-REQ-013` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Q-023, matrix item 15 — the capability set |

## Power

| ID | Class | Gating condition |
| --- | --- | --- |
| `AERORA-POWER-REQ-007` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Matrix item 20 — baseline 13 requires each observed watchdog path to be formalised and verified |
| `AERORA-POWER-REQ-008` | `CONFIRMED` | — |

## Companion computer

| ID | Class | Gating condition |
| --- | --- | --- |
| `AERORA-GPU-REQ-001` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Matrix item 9 — a firmware addressing decision |
| `AERORA-GPU-REQ-002` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Matrix item 9 |
| `AERORA-GPU-REQ-003` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-GPU-REQ-004` | `CONFIRMED` | — |
| `AERORA-GPU-REQ-005` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Q-008, matrix item 21 — acceptance must be reported for the prohibition to be satisfiable |
| `AERORA-GPU-REQ-006` | `CONFIRMED` | — |
| `AERORA-GPU-REQ-007` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-GPU-REQ-008` | `PROPOSED — ACCEPTABLE` | — |

## Cloud

| ID | Class | Gating condition |
| --- | --- | --- |
| `AERORA-AWS-REQ-005` | `CONFIRMED` | — |
| `AERORA-AWS-REQ-006` | `CONFIRMED` | — |
| `AERORA-AWS-REQ-007` | `PRODUCT DECISION REQUIRED` | Q-030 and Q-042 — the permitted dependency set and conflict policy |
| `AERORA-AWS-REQ-008` | `CONFIRMED` | — |
| `AERORA-AWS-REQ-009` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-AWS-REQ-010` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-AWS-REQ-011` | `CONFIRMED` | — |
| `AERORA-AWS-REQ-012` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-AWS-REQ-013` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-AWS-REQ-014` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-AWS-REQ-015` | `PROPOSED — ACCEPTABLE` | — |

## Security

| ID | Class | Gating condition |
| --- | --- | --- |
| `AERORA-SEC-REQ-011` | `CONFIRMED` | — |
| `AERORA-SEC-REQ-012` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-SEC-REQ-013` | `PRODUCT DECISION REQUIRED` | Q-029 — the role set defines the gated operation set |
| `AERORA-SEC-REQ-014` | `CONFIRMED` | — |
| `AERORA-SEC-REQ-015` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-SEC-REQ-016` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-SEC-REQ-017` | `CONFIRMED` | **Release-critical.** See the signing-key section below |
| `AERORA-SEC-REQ-018` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-SEC-REQ-019` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-SEC-REQ-020` | `CONFIRMED` | — |

## Operator interface

| ID | Class | Gating condition |
| --- | --- | --- |
| `AERORA-UI-REQ-004` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-UI-REQ-005` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-UI-REQ-006` | `PRODUCT DECISION REQUIRED` | Q-038 — merge cadence and a named owner |
| `AERORA-UI-REQ-007` | `PROPOSED — ACCEPTABLE` | Q-020 blocks one of four assertions; the other three are implementable |
| `AERORA-UI-REQ-008` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-UI-REQ-009` | `REJECT/WITHDRAW` | Duplicate of `AERORA-SEC-REQ-014`; identifier retired |
| `AERORA-UI-REQ-010` | `PROPOSED — ACCEPTABLE` | — |

## Safety and authority

| ID | Class | Gating condition |
| --- | --- | --- |
| `AERORA-SAFE-REQ-006` | `CONFIRMED` | — |
| `AERORA-SAFE-REQ-007` | `CONFIRMED` | — |
| `AERORA-SAFE-REQ-008` | `CONFIRMED` | — |
| `AERORA-SAFE-REQ-009` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-SAFE-REQ-010` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Q-007, matrix item 19 — the five interfaces are ratifiable as shape; the values are not |
| `AERORA-SAFE-REQ-011` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Matrix items 16 and 20 — baseline 13 requires the observed paths to be formalised and verified |

## Build and configuration

| ID | Class | Gating condition |
| --- | --- | --- |
| `AERORA-BUILD-REQ-004` | `PRODUCT DECISION REQUIRED` | Q-003 — the fork must be created by a decision and an action |
| `AERORA-BUILD-REQ-005` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-BUILD-REQ-006` | `CONFIRMED` | — |
| `AERORA-BUILD-REQ-007` | `PRODUCT DECISION REQUIRED` | Q-038 — cadence and owner |
| `AERORA-BUILD-REQ-008` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-BUILD-REQ-009` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-BUILD-REQ-010` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-BUILD-REQ-011` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-BUILD-REQ-012` | `PRODUCT DECISION REQUIRED` | Q-042 — conflict policy |

## Compliance and records

| ID | Class | Gating condition |
| --- | --- | --- |
| `AERORA-COMPLY-REQ-002` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-COMPLY-REQ-003` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-COMPLY-REQ-004` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Matrix item 12 — reporting source requires the dialect |
| `AERORA-COMPLY-REQ-005` | `PRODUCT DECISION REQUIRED` | Q-039 — retention policy |

## Vehicle identity

| ID | Class | Gating condition |
| --- | --- | --- |
| `AERORA-VEH-REQ-001` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-VEH-REQ-002` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Q-016, matrix item 22 — the identifier source |
| `AERORA-VEH-REQ-003` | `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Matrix item 22 — requires a reporting message |
| `AERORA-VEH-REQ-004` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-VEH-REQ-005` | `PROPOSED — ACCEPTABLE` | — |

## Testing

| ID | Class | Gating condition |
| --- | --- | --- |
| `AERORA-TEST-REQ-004` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-TEST-REQ-005` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-TEST-REQ-006` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-TEST-REQ-007` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-TEST-REQ-008` | `PROPOSED — ACCEPTABLE` | — |
| `AERORA-TEST-REQ-009` | `PROPOSED — ACCEPTABLE` | Q-035 affects where tests live, not whether the requirement is right |

## Items held explicitly OPEN

Per the Phase 2.1 directive, these are **not** closed by any Phase 2.1 statement, and no document in
this set may be read as having settled them.

| Item | Tracked as | Why it stays open |
| --- | --- | --- |
| Exact failsafe actions | Q-007, matrix 19 | Baseline 13 defers them to the evolving flight-controller implementation and forbids the GCS specification from assigning them |
| Power transition behaviour | Q-026, matrix 18 | Baseline 49 states transition is "not yet a claim" and requires separate engineering and verification |
| Payload actuator semantics | Q-023, matrix 15 | Firmware placeholders; no actuator model exists |
| Operator role model | Q-029 | A product and operations decision; no candidate role set recorded |
| Fleet identity source | Q-016, matrix 22 | The secure element is present; provisioning and readability are unknown |
| Aerora autopilot identifier | Q-018, matrix 11 | No firmware decision recorded |
| QML resource precedence | Q-020 | Requires a build to observe what the engine resolves. **Held open until runtime verification** |
| Payload state retention | Matrix 14 | Reclassified in Phase 2.1 as unevidenced |
| Upstream merge cadence and owner | Q-038 | Nobody has been assigned |
| HTTP server dependency hygiene | Q-027 | R-016's security premise is disproved; the hygiene decision is open |
| Aerora test placement | Q-035 | Requires a build |
| Audit store integrity mechanism | Q-037 | A product decision on what the trail must withstand |
| Cloud synchronisation conflict policy | Q-042 | A product decision |
| Log retention policy | Q-039 | A product decision |
| Mission verification depth | Q-041 | A product decision |
| Corroboration windows | Q-036 | Needs actuator response and link latency figures |
| Visual design language | Q-040 | A product and design decision |
| Qt licensing | Q-004, R-001 | An open legal question; no engineering conclusion offered |

Eighteen items. Every implementation-blocking question in either register appears above.

## Signing-key storage remains release-critical

| Field | Value |
| --- | --- |
| Risks | R-005, R-023, R-024 |
| Requirements | `AERORA-SEC-REQ-016`, `AERORA-SEC-REQ-017`, `AERORA-SEC-REQ-018`, `AERORA-BUILD-REQ-009` |
| Decision | ADR-008, status `ACCEPTED WITH AN OPEN REMEDIATION` |
| Gate | **`G-RELEASE`** |
| Phase 2.1 position | **Unchanged and reaffirmed** |

Three findings, with their current state:

| Finding | Remediation | Cost |
| --- | --- | --- |
| Keys persisted unencrypted in the application settings store | Upstream pluggable storage backend, or one Aerora core edit as fallback | Real; no seam exists |
| Fixed derivation salt makes a precomputation reusable across all installations | Provision randomly generated raw keys instead of passphrase-derived keys | **None — procedural** |
| A settings-schema version bump destroys every stored key | Move key custody out of the settings store; interim is a recorded reprovisioning plan | Resolved by the first remediation |

The first remediation resolves all three. It does not block the architecture or implementation, and
it **does** block shipping. A production release that persists key material unprotected is not
acceptable, and Phase 2.1 does not soften that.

## Core edit terminology

Standardised across the document set. These two sentences are the only permitted formulation:

> **Zero QGC core edits are required by the current architecture.**
>
> **One QGC core edit remains a possible fallback for signing-key storage if upstream extensibility
> is unavailable.**

The earlier phrasings — "currently zero are planned", "zero core edits", "one identified candidate"
— are superseded. The two sentences are not redundant: the first is a property of the architecture,
the second is a property of a remediation that sits outside it. Stating only the first would be
misleading; stating only the second would be alarming.

## Proposed ratification list

Presented as a decision sheet. Each line is something a reviewer accepts, rejects, or defers.

### Architecture decisions

| ADR | Subject | Recommendation | Note |
| --- | --- | --- | --- |
| ADR-001 | Overlay strategy and build configuration | **Ratify** | Verified seam by seam against source |
| ADR-002 | Transport strategy | **Ratify with a condition** | Conditional on matrix item 6 — the radio presenting as a serial device |
| ADR-003 | Serial radio strategy | **Ratify** | Both mitigations stand; one is entirely GCS-side |
| ADR-004 | MAVLink extension strategy | **Ratify** | Mechanism verified; the dialect is downstream work |
| ADR-005 | Mission and payload state authority | **Ratify with a condition** | Plan-file authority is unconditional. Payload retention is unevidenced; fallback adopted |
| ADR-006 | Cloud architecture | **Ratify** | — |
| ADR-007 | Modular monolith | **Ratify** | Adopts the baseline without alteration |
| ADR-008 | Security architecture and key custody | **Ratify with an open remediation** | Release-critical; status already says so |
| ADR-009 | Qt licensing neutrality | **Ratify** | Ratifying neutrality commits to nothing legal |
| ADR-010 | Failsafe boundary | **Ratify** | Ratifying a deliberate refusal to specify; a reviewer may disagree that it should refuse |
| ADR-011 | Companion authority boundary | **Ratify** | Read-only module is GCS-decided and structural |

Nine unconditional, two conditional. The two conditions are the two load-bearing unevidenced rows of
the evidence matrix.

### Requirements

| Group | Count | Recommendation |
| --- | --- | --- |
| `CONFIRMED` | 29 | Ratify |
| `PROPOSED — ACCEPTABLE` | 59 | Ratify, or reject individually with a reason |
| `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | 21 | Accept as stated obligations; do not schedule implementation |
| `PRODUCT DECISION REQUIRED` | 10 | Accept as stated obligations; assign the ten decisions |
| `REJECT/WITHDRAW` | 1 | Confirm the retirement |

### Risk gates

| Register | Recommendation |
| --- | --- |
| Four `G-ARCH` risks | Acknowledge that R-002 and R-017 carry unevidenced vehicle dependencies, and ratify on that basis |
| Ten `G-IMPL`, two `G-INTEG`, six `G-OPEN` | Accept the gate assignments |
| Seven `G-RELEASE` | Accept, and note that four concentrate in key custody and licensing |

## Remaining human decisions

Ten product, process and legal decisions. None requires engineering work first.

| # | Decision | Owner type | Blocks | Effort |
| --- | --- | --- | --- | --- |
| 1 | Create the Aerora fork, upstream fetch-only | Engineering management | Everything | Hours |
| 2 | Name an owner and cadence for upstream merges | Engineering management | `AERORA-UI-REQ-006`, `AERORA-BUILD-REQ-007` | Minutes |
| 3 | Decide the HTTP server dependency hygiene question | Engineering | Q-027, R-016 | Hours |
| 4 | Define the operator role model | Product and operations | `AERORA-SEC-REQ-013`, authorisation design | Days |
| 5 | Decide what the audit trail must withstand | Product and security | `AERORA-GCS-REQ-028` | Days |
| 6 | Decide the cloud synchronisation conflict policy | Product | `AERORA-AWS-REQ-007`, `AERORA-BUILD-REQ-012` | Days |
| 7 | Decide log retention and upload policy | Product and operations | `AERORA-COMPLY-REQ-005` | Days |
| 8 | Decide mission verification depth | Product | `AERORA-MISSION-REQ-007` | Hours |
| 9 | Decide platform scope | Product | Build and signing matrix | Days |
| 10 | Obtain a licensing position from counsel | Legal | R-001, release | Weeks |

Decisions 1, 2, 3 and 8 are each under a day and together unblock a disproportionate amount of work.
Decision 10 has the longest lead time and the least engineering dependency, which is an argument for
starting it now rather than when it becomes urgent.

### Separately: hardware and firmware evidence to produce

Not decisions — work. Listed so the two kinds are not confused.

| # | Evidence | Owner | Blocks |
| --- | --- | --- | --- |
| 1 | Does the ground radio present as a USB serial device, and at what parameters | Hardware | ADR-002, `AERORA-COMM-REQ-006` |
| 2 | Will the payload controller retain state across an interruption | Firmware | ADR-005, `AERORA-PAYLOAD-REQ-007`, R-002 |
| 3 | What `MAV_AUTOPILOT` value does the flight controller report | Firmware | `AERORA-GCS-REQ-007` |
| 4 | What is the fleet identity source, and is the secure element provisioned | Hardware and firmware | `AERORA-VEH-REQ-002`, R-017 |
| 5 | Freeze the companion interface parameters in the interface control document | Hardware and firmware | Matrix item 9 |
| 6 | Define the Aerora dialect and the capability inventory | Firmware | Matrix item 10, and most display requirements |
| 7 | Define the payload actuator model | Hardware and firmware | Q-023, `AERORA-PAYLOAD-REQ-013` |
| 8 | Freeze failsafe behaviour and verify each observed path | Firmware and safety | Q-007, `AERORA-SAFE-REQ-010` |

## Phase 3 entry criteria

Phase 3 may not start until all of the following hold. Each is checkable, and the check is stated.

### Mandatory

| # | Criterion | Check |
| --- | --- | --- |
| 1 | This document set is reviewed, and each ADR is accepted, rejected or deferred on the record | A signed or recorded review decision exists for all eleven ADRs |
| 2 | The 88 ratifiable requirements are accepted, or individually rejected with a reason | The register records a ratification outcome for every active requirement |
| 3 | The 21 evidence-gated and 10 decision-gated requirements are acknowledged as not yet implementable | No Phase 3 task schedules work against them |
| 4 | The Aerora fork exists with upstream configured fetch-only, at the recorded fork point | `git remote -v` shows upstream fetch-only; the fork-point commit matches the baseline analysis commit |
| 5 | A build host exists that can configure and build the project | A successful configure and build is recorded |
| 6 | Q-020 is resolved by observing what the QML engine resolves | A recorded observation from a real build |
| 7 | Q-035 is resolved by confirming an Aerora test appears in the registered-test listing | A recorded listing output |
| 8 | An owner and cadence for upstream merges is recorded | A named person and an interval |
| 9 | The evidence matrix has a named owner for each `G-INTEG` item | Eight owners recorded |

Criteria 5, 6 and 7 are the ones Phase 2 could not satisfy and said so: the analysis host had no
build toolchain, so no runtime claim in this set was verified by running anything. Phase 3 should
not begin while that is still true.

### Not entry criteria

Stated to prevent scope creep at the gate.

| Item | Why it does not block Phase 3 |
| --- | --- |
| Qt licensing position | Blocks release, not implementation. Phase 3 may proceed provided nothing is distributed |
| Signing-key remediation | Blocks release. Phase 3 may implement the signing integration as upstream provides it |
| Failsafe action definitions | The interfaces are defined; the values are not needed to build them |
| Payload actuator model | The payload abstraction stops at the state-name boundary by design |
| Payload state retention evidence | The GCS fallback is implementable without it |
| Operator role model | The authorisation mechanism is implementable before the role set is populated |
| Fleet identity source | The identifier is opaque; no consumer parses it |
| Platform scope | Windows and Linux are sufficient to start |

### Recommended Phase 3 scope

Advisory, not a gate. Derived from the build-order chains in
[component-architecture.md](component-architecture.md):

| Order | Work | Why first |
| --- | --- | --- |
| 1 | Overlay scaffold, with the startup self-check and the architectural-property tests | Everything else attaches to it, and the tests fail loudly if attachment is wrong |
| 2 | Aerora dialect integration with a placeholder dialect | Proves the generator path end to end before messages matter |
| 3 | `AeroraFirmwarePlugin` and the six FactGroups | The attachment point for all telemetry |
| 4 | Freshness and the command state model | Every later module depends on them |
| 5 | Settings defaults, including the four corrections to upstream defaults | Cheap, and two of them are governance items |

Nothing in that list depends on an unresolved question. That is deliberate: the first increment
should be the part of the architecture that is fully evidenced, so that the unevidenced parts are
reached with a working build and a working test harness.

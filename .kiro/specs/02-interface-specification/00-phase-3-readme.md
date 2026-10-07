# Phase 3 — Interface Control and System Specification

What must exist between the components, what crosses each boundary, who owns it, what evidence
supports it, and what remains open.

## Status

| Field | Value |
| --- | --- |
| Phase | PHASE 3 — detailed interface control and system specification |
| Authoritative inputs | Phase 2 architecture and Phase 2.1 reconciliation |
| QGC baseline commit | `9459a4b57986f40df8bdc193d5453c0596fbb416` |
| QGC source modified | **No** |
| Implementation started | **No** |
| Review status | **AWAITING HUMAN REVIEW** |

## What Phase 3 is, and is not

Phase 3 converts architecture into contracts. A contract says what crosses a boundary, in which
direction, with what semantics, under whose authority, and how a reader would know whether it is
real yet.

| Phase 3 does | Phase 3 does not |
| --- | --- |
| Define the boundaries and what crosses them | Define message identifiers the architecture has not ratified |
| Record what evidence supports each interface | Supply evidence by reasoning |
| State what remains open and what would resolve it | Close an open question to make a document tidy |
| Name the verification method for each interface | Claim any verification has been performed |
| Preserve the Phase 2.1 evidence discipline | Promote a `PROPOSED` requirement to `CONFIRMED` |

An interface specified here is **not** thereby approved. Several are specified precisely so that the
thing they depend on can be seen to be missing.

## Source hierarchy

Applied as given, strongest first:

| Rank | Source |
| --- | --- |
| 1 | Explicit confirmed Aerora requirements |
| 2 | Approved product baseline |
| 3 | Approved Phase 2 architecture |
| 4 | Hardware and PCB evidence |
| 5 | Simulink and firmware evidence |
| 6 | Approved interface control document evidence |
| 7 | QGroundControl source behaviour |
| 8 | Official regulations |
| 9 | Approved proposals |
| 10 | Inference |

**Nothing in Phase 3 rests on rank 10 without saying so.** Where an interface exists only because
the architecture needs it to, its evidence status says `NOT-EVIDENCED` and its status says `OPEN`
or `PROPOSED`.

### Conflict rule

Where two sources disagree, Phase 3 does not pick one. It records the conflict, preserves the
earlier position, and names the authority needed to settle it. Conflicts found in Phase 3 are
listed in [21-phase-3-risks.md](21-phase-3-risks.md) and, where they affect an interface, in that
interface's row.

The Phase 2.1 precedent applies: superseded text is retained rather than deleted.

## Vocabularies

### Status

Carried unchanged from Phase 1 and Phase 2.

| Status | Meaning |
| --- | --- |
| `CONFIRMED` | Established by direct evidence or an approved Aerora requirement |
| `PROPOSED` | An engineering recommendation, not yet ratified |
| `OPEN` | A decision Aerora must make before implementation |
| `PLACEHOLDER` | Exists in an artifact but is explicitly provisional |
| `UNKNOWN` | Not yet investigated or not determinable |
| `DEPRECATED` | Superseded; retained for traceability |

### Ratification class

Carried from Phase 2.1. Applies to requirements, and is quoted in interface rows where an interface
depends on a requirement that is not yet acceptable.

| Class | Meaning |
| --- | --- |
| `CONFIRMED` | Traceable to a baseline or directive statement, no gating condition |
| `PROPOSED — ACCEPTABLE` | A derivation within ground-station control, implementable as written |
| `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | Depends on unevidenced vehicle behaviour |
| `PRODUCT DECISION REQUIRED` | Blocked on a product, operations or process decision |
| `REJECT/WITHDRAW` | Not carried forward |

### Evidence status

Carried from the Phase 2.1 evidence matrix.

| Code | Meaning |
| --- | --- |
| `ARTIFACT-OBSERVED` | A named hardware or PCB artifact shows it exists, **as recorded in baseline 46** |
| `MODEL` | Present in the Simulink firmware model, **as recorded in baselines 13 and 46** |
| `BASELINE-ASSERTED` | A baseline document states it; no artifact cited |
| `GCS-VERIFIED` | Established by **reading QGroundControl source**. Not an execution result |
| `GCS-DECIDED` | Within Aerora ground-station control, decided in Phase 2 |
| `SPECIFIED-NOT-VERIFIED` | Defined in a document, not bench or flight verified |
| `NOT-EVIDENCED` | Neither artifact nor baseline statement exists |
| `PLACEHOLDER` | Present in the firmware model and explicitly provisional |

Two provenance limits, corrected in Phase 3.1 and binding on every row in this set:

| Limit | Consequence |
| --- | --- |
| **No Simulink model artifact exists in this repository.** Every `MODEL` and `PLACEHOLDER` row rests on baseline 13 and baseline 46 **describing** the uploaded model, not on inspecting it | A `MODEL` row is second-hand evidence. Baseline 13 directs that each observed path "must be converted into a formal requirement and verified against the real FC hardware"; none has been |
| **No hardware artifact exists in this repository.** Every `ARTIFACT-OBSERVED` row rests on baseline 46 recording the schematic and PCB | Presence of a part is evidence it was selected, not that its interface or behaviour is defined |

`GCS-VERIFIED` is the code most easily misread. It means a statement about **upstream
QGroundControl** behaviour was established by reading upstream source. It is **not** a verification
result for any Aerora interface, and it says nothing about whether Aerora code exists.

### Interface state: contract, implemented, verified

Mandatory for Phase 4 and stated here so it is inherited rather than rediscovered.

| State | Meaning | How many interfaces are in it |
| --- | --- | --- |
| **Interface contract** | The behaviour is specified in an interface control document | **All 240** |
| **Implemented interface** | Source code exists that realises the contract | **0** |
| **Verified interface** | The implementation has been executed and evidence confirms the behaviour | **0** |

An interface is **not** implemented because it has an identifier, a status, a ratification class, a
named QGroundControl seam, a planned fact group, an identified C++ class or an identified QML
module. It is not verified because it has an assigned verification method.

Every normative present-tense sentence in this document set — "the ground station marks the value
stale", "a write without readback is not treated as applied" — is a **contract statement** about
what an implementation shall do. None is a statement that code exists or has been run. Where a
sentence describes behaviour that genuinely exists today, it concerns **upstream** QGroundControl and
carries `GCS-VERIFIED`.

### Verification method

| Code | Meaning |
| --- | --- |
| `STATIC` | Static inspection of code, configuration or document |
| `UNIT` | Automated unit test |
| `INTEGRATION` | Automated integration test, typically against the mock link |
| `HIL` | Hardware in the loop |
| `BENCH` | Bench test against real hardware |
| `SIM` | Simulation |
| `DEMO` | Observed operation |
| `FLIGHT` | Flight test |
| `AUDIT` | Process check at a defined point |

Phase 3 assigns methods. It reports no results.

## Identifier schemes

### Interfaces

`IF-<domain>-<nnn>`. Domains:

| Domain | Subject | Document |
| --- | --- | --- |
| `SYS` | System boundary | [01-gcs-system-icd.md](01-gcs-system-icd.md) |
| `FC` | Flight controller to ground station | [02-fc-gcs-icd.md](02-fc-gcs-icd.md) |
| `MAV` | MAVLink protocol contract | [03-mavlink-icd.md](03-mavlink-icd.md) |
| `RAD` | Radio link | [04-radio-link-icd.md](04-radio-link-icd.md) |
| `GPU` | Companion computer | [05-gpu-fc-icd.md](05-gpu-fc-icd.md) |
| `PAY` | Payload | [06-payload-icd.md](06-payload-icd.md) |
| `PWR` | Power telemetry | [07-power-telemetry-icd.md](07-power-telemetry-icd.md) |
| `VID` | Vehicle identity | [08-vehicle-identity-icd.md](08-vehicle-identity-icd.md) |
| `AUT` | Command authority | [09-command-authority-icd.md](09-command-authority-icd.md) |
| `MIS` | Mission | [10-mission-icd.md](10-mission-icd.md) |
| `TLM` | Telemetry model | [11-telemetry-icd.md](11-telemetry-icd.md) |
| `PAR` | Parameters | [12-parameter-icd.md](12-parameter-icd.md) |
| `SEC` | Security | [13-security-icd.md](13-security-icd.md) |
| `LOG` | Audit and logging | [14-audit-logging-icd.md](14-audit-logging-icd.md) |
| `CLD` | Cloud integration | [15-cloud-integration-icd.md](15-cloud-integration-icd.md) |
| `UIS` | User interface state | [16-ui-state-icd.md](16-ui-state-icd.md) |

Each interface is defined in exactly one document and appears in
[19-interface-traceability.md](19-interface-traceability.md). Both properties are checked by script.

### Questions and risks

| Scheme | Range | Meaning |
| --- | --- | --- |
| `Q-nnn` | Q-001 to Q-043 | Existing questions from Phases 1, 2 and 2.1. **Referenced, never redefined** |
| `Q3-nnn` | From Q3-001 | New Phase 3 interface questions only |
| `R-nnn` | R-001 to R-029 | Existing risks. **Referenced, never redefined** |
| `R3-nnn` | From R3-001 | New Phase 3 interface and specification risks only |
| `AERORA-*-REQ-nnn` | 175 existing | **Referenced only.** Phase 3 defines no requirements |

Phase 3 introduces **no new requirement identifiers**. If an interface needs an obligation that no
requirement states, that gap is recorded as a Phase 3 question rather than filled by invention.

## Document set

| # | Document | Subject |
| --- | --- | --- |
| 00 | This document | Scope, vocabularies, identifier schemes |
| 01 | [01-gcs-system-icd.md](01-gcs-system-icd.md) | System boundary, actors, ownership, the five non-authorities |
| 02 | [02-fc-gcs-icd.md](02-fc-gcs-icd.md) | The flight controller contract, all state categories |
| 03 | [03-mavlink-icd.md](03-mavlink-icd.md) | Dialect, signing, channels, identifiers, compatibility |
| 04 | [04-radio-link-icd.md](04-radio-link-icd.md) | Serial-first chain, connection lifecycle, USB identity |
| 05 | [05-gpu-fc-icd.md](05-gpu-fc-icd.md) | Companion proposals and the authority boundary |
| 06 | [06-payload-icd.md](06-payload-icd.md) | Payload ownership, telemetry, command, actuator, emergency stop |
| 07 | [07-power-telemetry-icd.md](07-power-telemetry-icd.md) | Tether and battery reporting; transitions left open |
| 08 | [08-vehicle-identity-icd.md](08-vehicle-identity-icd.md) | System id versus fleet identity; selectable source |
| 09 | [09-command-authority-icd.md](09-command-authority-icd.md) | Authority modes, acceptance, rejection, staleness |
| 10 | [10-mission-icd.md](10-mission-icd.md) | Upload, download, resume, verification, R-002 |
| 11 | [11-telemetry-icd.md](11-telemetry-icd.md) | Canonical telemetry model; reported, effective, proposed |
| 12 | [12-parameter-icd.md](12-parameter-icd.md) | Parameter lifecycle |
| 13 | [13-security-icd.md](13-security-icd.md) | Authentication, authorisation, signing, key custody |
| 14 | [14-audit-logging-icd.md](14-audit-logging-icd.md) | Records, correlation, local-first |
| 15 | [15-cloud-integration-icd.md](15-cloud-integration-icd.md) | Cloud as optional supporting system |
| 16 | [16-ui-state-icd.md](16-ui-state-icd.md) | What the interface shows and where it comes from |
| 17 | [17-capability-model.md](17-capability-model.md) | Universal capability hierarchy; painting as a profile |
| 18 | [18-error-degradation-model.md](18-error-degradation-model.md) | Degradation conditions without invented safety actions |
| 19 | [19-interface-traceability.md](19-interface-traceability.md) | Machine-checkable interface mapping |
| 20 | [20-phase-3-open-questions.md](20-phase-3-open-questions.md) | New Phase 3 questions |
| 21 | [21-phase-3-risks.md](21-phase-3-risks.md) | New Phase 3 risks |
| 22 | [22-phase-3-verification-plan.md](22-phase-3-verification-plan.md) | Verification method per interface |
| 23 | [23-build-readiness.md](23-build-readiness.md) | Build environment specification |
| 24 | [24-phase-3.1-correction-audit.md](24-phase-3.1-correction-audit.md) | **Phase 3.1** — watchdog evidence audit, interface-state terminology audit, `OPEN` interface isolation audit, override and drift audit, question and risk gate audit |

Where a Phase 2 architecture document already carries sufficient detail, Phase 3 references it and
adds only the missing interface contract. The Phase 2 set is not restated.

## The invariants Phase 3 preserves

These come from Phase 2 and Phase 2.1 and are not reopened here.

| Invariant | Source |
| --- | --- |
| The ground station is supervisory | Architectural principle 1 |
| Effective state takes precedence over requested state | Principle 2 |
| Telemetry is untrusted external input | Principle 3 |
| Behaviour that is not confirmed is not implemented | Principle 4 |
| Local before cloud; no cloud service on the command path | Principle 5 |
| All parameters are Facts | Principle 6 |
| Aerora code lives in the overlay | Principle 7 |
| Payload concerns do not leak into flight-control concerns | Principle 8 |
| Licence-neutral by construction | Principle 9 |
| Zero QGC core edits are required by the current architecture | ADR-001 |
| One QGC core edit remains a possible fallback for signing-key storage if upstream extensibility is unavailable | ADR-008 |
| QGC core modification of the resume filter is rejected | ADR-005, Phase 2.1 |
| Ground-station payload state replay is rejected | ADR-005, Phase 2.1 |
| Payload-controller state retention is preferred and `NOT-EVIDENCED` | Phase 2.1 |

## What is intentionally left open

Phase 3 inherits eighteen open items from Phase 2.1 and adds its own. The inherited list is in
[ratification.md](../01-aerora-architecture/ratification.md); the Phase 3 additions are in
[20-phase-3-open-questions.md](20-phase-3-open-questions.md).

Specifically, and repeated here because they are the ones most likely to be filled in by accident:

| Not specified in Phase 3 | Why |
| --- | --- |
| The Aerora `MAV_AUTOPILOT` value | Q-018; no firmware decision exists |
| Aerora message and command identifiers | No ratified dialect; inventing them would make the interface control document fiction |
| Payload actuator, state and command enumerations | Q-023; firmware placeholders only |
| Failsafe action mappings | Q-007; baseline 13 forbids the ground station assigning them |
| Autonomy blending equations and freshness thresholds | Firmware model marks the wiring as future work |
| Radio USB vendor and product identifiers | No hardware decision exists |
| Fleet identity source | Q-016; the secure element is present, not shown to be usable |
| Power transition behaviour and timing | Baseline 49 requires separate engineering and verification |
| Operator role definitions | Q-029 |
| Payload state persistence | Phase 2.1 evidence matrix item 14 |

## How to read an interface row

Every interface carries, at minimum: an identifier, a direction, a producer, a consumer, the data
crossing the boundary, update and freshness semantics, acknowledgement requirement, authority,
failure behaviour, evidence status, status, and links to requirements, risks and questions.

Where a table would be unreadably wide, each document splits this into a **contract table** and a
**failure and traceability table** keyed on the same identifier. A reader needs both rows to have
the whole contract.

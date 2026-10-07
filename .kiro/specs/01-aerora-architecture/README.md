# Aerora Universal GCS Architecture

Phase 2 deliverable. The architecture for the Aerora Universal Ground Control Station, derived from
the QGroundControl baseline established in Phase 1 and the Aerora Product Baseline v4.1.

## Status

| Field | Value |
| --- | --- |
| Phase | PHASE 2.1 — reconciliation and ratification preparation |
| Date | 2026-10-04 |
| Requirements | 120 published, 119 active, 1 withdrawn, 15 domains |
| Ratifiable now | 88 of 119 active |
| QGC baseline | 5.2.0-dev at commit `9459a4b57986f40df8bdc193d5453c0596fbb416` |
| Product baseline | Aerora Universal GCS Product Baseline v4.1 |
| QGC source modified | **No** |
| Implementation started | **No** |
| Review status | **AWAITING HUMAN REVIEW** |

## Scope

This set defines **what will be built and where it attaches**. It does not implement anything, does
not patch QGroundControl, and does not specify code.

In scope: system and component architecture, runtime and deployment structure, the integration
architecture for flight controller, companion computer and payload, the authority model, the
communication and MAVLink architecture, mission and payload architecture, cloud architecture,
security, logging, configuration, data model, UI architecture, testing architecture, requirements
with stable identifiers, architecture decision records, risk gates and traceability.

Not in scope and deliberately not attempted: any behaviour the v4.1 baseline defers. Failsafe
actions, power transition behaviour and payload actuator interfaces are represented as interfaces
and states, never as values.

## Terminology

Phase 1 established that QGroundControl has **no runtime plugin system**. This set therefore uses
the following vocabulary consistently, and the phrase "plugin" is avoided except when naming an
actual QGC class such as `QGCCorePlugin`.

| Term | Meaning |
| --- | --- |
| **QGC Upstream** | The unmodified QGroundControl codebase at the pinned baseline commit |
| **Aerora Overlay** | The single custom-build directory, activated by `QGC_CUSTOM_DIR`, containing all Aerora code. Compile-time, not loadable |
| **Aerora Modules** | Logical units inside the overlay — payload, authority, power, security, cloud, and so on |
| **Aerora Configuration** | Behaviour changed by CMake option, settings, or declarative JSON, with no code written |
| **Seam Attachment** | Aerora behaviour reached through a QGC virtual method, signal, or resource-override mechanism |
| **Core Edit** | A modification to QGC Upstream source. **Zero QGC core edits are required by the current architecture.** See the standard formulation below |

## Documents

### Review of Phase 1

| Document | Contents |
| --- | --- |
| [phase-2-risk-gates.md](phase-2-risk-gates.md) | All 20 Phase 1 risks reclassified with gate decisions. Includes the R-001 reclassification as an open legal question |
| [phase-2-open-question-resolution.md](phase-2-open-question-resolution.md) | All 34 Phase 1 questions triaged by resolution route and architectural impact. Includes the Q-014 and R-002 resolutions |

### Architecture

| Document | Contents |
| --- | --- |
| [system-architecture.md](system-architecture.md) | System context, layering, architectural principles, ownership boundaries |
| [component-architecture.md](component-architecture.md) | Aerora modules, their responsibilities and dependencies |
| [runtime-architecture.md](runtime-architecture.md) | Threading, startup, data flow, degraded operation |
| [deployment-architecture.md](deployment-architecture.md) | Build topology, platforms, packaging, signing |

### Interfaces and integration

| Document | Contents |
| --- | --- |
| [communication-architecture.md](communication-architecture.md) | Link model, transport strategy, link state and arbitration mirroring |
| [mavlink-architecture.md](mavlink-architecture.md) | Dialect strategy, message families, extension seams, interface separation |
| [vehicle-integration.md](vehicle-integration.md) | Firmware abstraction, parameters, telemetry fact groups, power model |
| [gpu-integration.md](gpu-integration.md) | Companion computer observability, proposal path, freshness |

### Domains

| Document | Contents |
| --- | --- |
| [payload-architecture.md](payload-architecture.md) | Payload abstraction, painting payload, configuration states |
| [mission-architecture.md](mission-architecture.md) | Coverage pattern, plan authority, payload state across resume |
| [cloud-architecture.md](cloud-architecture.md) | Modular monolith, module boundaries, degradation, the real-time boundary |
| [ui-architecture.md](ui-architecture.md) | Operator interface structure, extension seams, override discipline |

### Cross-cutting

| Document | Contents |
| --- | --- |
| [authority-model.md](authority-model.md) | Command lifecycle states, authority sources, failsafe interfaces |
| [security-architecture.md](security-architecture.md) | Identity, authorisation, audit, key management, update integrity |
| [logging-architecture.md](logging-architecture.md) | Log domains, retention, evidence, cloud aggregation |
| [configuration-architecture.md](configuration-architecture.md) | Fork strategy, settings, vehicle configuration, versioning |
| [data-model.md](data-model.md) | Entities, identity, telemetry representation, persistence |

### Verification and governance

| Document | Contents |
| --- | --- |
| [testing-architecture.md](testing-architecture.md) | Test strategy, inherited infrastructure, Aerora-specific suites |
| [requirements.md](requirements.md) | Aerora requirements with stable identifiers and verification methods |
| [architecture-decisions.md](architecture-decisions.md) | Eleven architecture decision records |
| [risk-gates.md](risk-gates.md) | Risks arising from Phase 2, with gates |
| [open-questions.md](open-questions.md) | Questions arising from Phase 2 |
| [traceability.md](traceability.md) | Requirement chain, reconciled with the Phase 1 foundation |

### Phase 2.1 — reconciliation and ratification preparation

| Document | Contents |
| --- | --- |
| [ratification.md](ratification.md) | All 120 requirements classified by what must happen before acceptance; the eighteen items held open; the proposed ratification list; ten human decisions; Phase 3 entry criteria |
| [hardware-firmware-evidence-matrix.md](hardware-firmware-evidence-matrix.md) | Twenty-two vehicle-side items with evidence source, evidence status and confidence. One of twenty-two is confirmed |
| [phase-2-1-reconciliation.md](phase-2-1-reconciliation.md) | Every contradiction, overclaim and count error corrected, with the amendment history |

**A reviewer should start with [ratification.md](ratification.md).** It is a decision sheet; the rest
of the set is the reasoning behind it.

## Two decisions that shape everything else

### Serial-first transport

Q-014 resolved: a radio presenting as a standard serial port carries MAVLink through QGroundControl
unmodified. The six transport core edits identified in Phase 1 are **deferred, not incurred**.

Consequence: **no Aerora feature in this architecture requires a QGC core modification.** The
overlay is pure addition, which materially reduces the upstream maintenance burden that Phase 1
flagged.

Recorded as ADR-002 and ADR-003.

### Core edit terminology — the standard formulation

Two sentences, used verbatim wherever a global claim about core edits is made:

> **Zero QGC core edits are required by the current architecture.**
>
> **One QGC core edit remains a possible fallback for signing-key storage if upstream extensibility
> is unavailable.**

Both are needed. The first is a property of the architecture; the second is a property of a
remediation that sits outside it. Stating only the first would be misleading, and stating only the
second would be alarming.

The background: every Aerora module, FactGroup, seam, setting and build option in this set attaches
without touching QGroundControl source. MAVLink signing keys, however, are persisted unprotected,
and the storage class has no virtual members, so the backend cannot be replaced from the overlay.

| Route | Core edits |
| --- | --- |
| Contribute a pluggable key-storage backend upstream — preferred | Zero |
| Replace the storage backend as an Aerora core edit — fallback | One |

This blocks release, not the architecture. Detail in
[security-architecture.md](security-architecture.md) and ADR-008; ratification position in
[ratification.md](ratification.md).

### Payload state authority sits with the payload controller

R-002 analysed in full: QGroundControl replays a prefix of configuration commands on mission resume
*because it assumes the vehicle retains nothing*. The v4.1 baseline assigns actuator-level payload
behaviour and payload-local interlocks to the payload controller, so the proposed resolution is for
the payload controller to retain payload state across an interruption.

Consequence: the resume limitation is addressed by **correct ownership**, not by patching a QGC
allow-list.

**Phase 2.1 caveat.** Ownership is baseline-supported; retention across an interruption is **not**.
No baseline document states it and no firmware evidence exists, so it is carried as a vehicle-side
verification requirement with evidence status `NOT-EVIDENCED`, with a GCS-side fallback if it is not
implemented. R-002 remains gated at `G-ARCH`.

Recorded as ADR-005; evidence item in the
[hardware and firmware evidence matrix](hardware-firmware-evidence-matrix.md).

## Architectural principles

Carried from the v4.1 baseline and sharpened by Phase 1 evidence. These are binding.

1. **The GCS is supervisory.** It never becomes the sole safety mechanism, and never replaces
   flight-controller local safety.
2. **Effective state over requested state.** The GCS shows what the aircraft is actually doing and
   who holds authority, never what was merely transmitted.
3. **Telemetry is untrusted external input.** It is validated, timestamped and attributed before
   it is displayed or acted upon.
4. **Behaviour that is not confirmed is not implemented.** Deferred items are represented as
   interfaces with explicit unknown states.
5. **Local before cloud.** No cloud service is on the command path. Loss of cloud degrades
   synchronisation only.
6. **All parameters are Facts.** No bespoke parameter storage anywhere.
7. **Aerora code lives in the overlay.** Core edits require individual justification. Zero QGC core
   edits are required by the current architecture; one remains a possible fallback for signing-key
   storage.
8. **Payload code does not leak into flight-control modules.**
9. **Licence-neutral by construction.** No architectural decision depends on the unresolved
   licensing question.

## Status vocabulary

Unchanged from Phase 1 and used consistently.

| Status | Meaning |
| --- | --- |
| `CONFIRMED` | Established by direct evidence or an approved Aerora requirement |
| `PROPOSED` | An engineering recommendation from this analysis, not yet ratified |
| `OPEN` | A decision Aerora engineering must make before implementation |
| `PLACEHOLDER` | Exists in an artefact but is explicitly provisional |
| `DEPRECATED` | Superseded; retained for traceability |
| `UNKNOWN` | Not yet investigated or not determinable |

Every architectural recommendation originating in this set is `PROPOSED`. Ratification is a human
act.

## What happens next

Phase 2.1 ends here, pending review.

The nine mandatory Phase 3 entry criteria, the eight items explicitly **not** entry criteria, and
the recommended first increment are in [ratification.md](ratification.md). In outline:

| # | Condition | Where it is tracked |
| --- | --- | --- |
| 1 | This document set is reviewed, and each ADR accepted, rejected or deferred on the record | Human act |
| 2 | The 88 ratifiable requirements are accepted, or individually rejected with a reason | [ratification.md](ratification.md) |
| 3 | The 21 evidence-gated and 10 decision-gated requirements are acknowledged as not yet implementable | [ratification.md](ratification.md) |
| 4 | A build host exists that can configure and build the project | R-009 |
| 5 | Each `G-INTEG` evidence-matrix item has a named owner | [hardware-firmware-evidence-matrix.md](hardware-firmware-evidence-matrix.md) |
| 6 | The Aerora fork exists, with upstream as a fetch-only remote | Q-003, `AERORA-BUILD-REQ-004` |
| 7 | Resource alias shadowing precedence is confirmed by a build | Q-020, R-019 |
| 8 | Aerora test reachability is confirmed by a build | Q-035, R-021 |
| 9 | The upstream merge cadence has a named owner | Q-038 |

Criteria 4, 7 and 8 are the ones Phase 2 could not satisfy and said so: the analysis host had no
build toolchain, so no runtime claim in this set was verified by running anything.

The ten remaining human decisions, with owner types and effort, are in
[ratification.md](ratification.md). The full register of what blocks implementation, integration and
release is in [risk-gates.md](risk-gates.md) and [traceability.md](traceability.md).

No QGroundControl source may be modified, and no implementation may begin, until this set is
reviewed and ratified.

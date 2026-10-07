# Architecture Decisions

Eleven decisions that shape the Aerora GCS, each with the alternatives that were considered and the
reason they were rejected.

A decision record earns its place by recording what was *not* chosen. The chosen option is usually
reconstructible from the code; the rejected options are not, and they are what a future engineer
needs when the decision looks wrong from a new vantage point.

## Index

| ADR | Decision | Status |
| --- | --- | --- |
| [ADR-001](#adr-001-overlay-strategy) | Overlay strategy and build configuration | `ACCEPTED` |
| [ADR-002](#adr-002-transport-strategy) | Transport strategy | `ACCEPTED` |
| [ADR-003](#adr-003-serial-radio-strategy) | Serial radio strategy | `ACCEPTED` |
| [ADR-004](#adr-004-mavlink-extension-strategy) | MAVLink extension strategy | `ACCEPTED` |
| [ADR-005](#adr-005-mission-and-payload-state-authority) | Mission and payload state authority | `ACCEPTED` |
| [ADR-006](#adr-006-cloud-architecture) | Cloud architecture | `ACCEPTED` |
| [ADR-007](#adr-007-modular-monolith) | Modular monolith | `ACCEPTED` |
| [ADR-008](#adr-008-security-architecture) | Security architecture and key custody | `ACCEPTED WITH AN OPEN REMEDIATION` |
| [ADR-009](#adr-009-qt-licensing-neutrality) | Qt licensing neutrality | `ACCEPTED` |
| [ADR-010](#adr-010-failsafe-boundary) | Failsafe boundary | `ACCEPTED` |
| [ADR-011](#adr-011-companion-authority-boundary) | Companion authority boundary | `ACCEPTED` |

`ACCEPTED` here means accepted *by this architecture proposal*. All eleven are subject to the human
review this phase ends at.

## ADR-001: Overlay strategy

### Context

Aerora needs substantial functionality QGroundControl does not have, and needs to track upstream
indefinitely. The cost of divergence is paid at every upstream merge, forever.

QGroundControl provides a custom-build mechanism: an overlay directory with its own CMake
configuration, source set, QML modules, resource overrides and settings metadata, wired in by the
top-level build when configured.

### Decision

All Aerora code, resources and configuration live in a single overlay directory. Aerora attaches
only at existing extension points. **No QGroundControl source file is modified**, with one
identified exception tracked in [ADR-008](#adr-008-security-architecture).

Single-stack versus multi-stack operation is a build configuration using two existing CMake options,
not a code difference.

### Alternatives rejected

| Alternative | Reason rejected |
| --- | --- |
| Fork and modify freely | Every upstream merge becomes a conflict resolution exercise; the divergence compounds |
| Aerora code distributed through `src/` alongside upstream code | Makes the core-edit count unverifiable and the boundary unenforceable |
| Add new CMake options to gate the PX4 and ArduPilot factories | Unnecessary — the options already exist and are honoured, so the directive's premise here was out of date |
| Put Aerora C++ in its own static library | A self-registering factory in a static library can be discarded by the linker with no diagnostic |

### Consequences

| Consequence | Assessment |
| --- | --- |
| Fifteen Aerora modules attach with zero core edits | Verified against source, module by module |
| Some upstream behaviour is reachable only through a resource override | Three tiers of seam, with the expensive tier inventoried — see [ui-architecture.md](ui-architecture.md) |
| A resource override leaves version control clean | So drift is invisible to tooling and needs an explicit inventory and a merge-time review |
| Aerora cannot change upstream behaviour that has no seam | Accepted; the two cases found are the resume filter and key storage, and both have better answers than a core edit |

Related: `AERORA-BUILD-REQ-006`, `AERORA-GCS-REQ-007`, `AERORA-GCS-REQ-009`, `AERORA-UI-REQ-004`,
R-009, R-010, R-019, Q-003, Q-017.

## ADR-002: Transport strategy

### Context

The Aerora aircraft carries three radios on different bands with vehicle-side arbitration between
them. The obvious reading is that the GCS needs three new link types.

Phase 1 recorded this as R-003 with an estimate of six transport core edits: a link type
enumeration, a configuration class, a link class, factory registration, settings UI and
auto-connect handling.

### Decision

**Aerora adds no transport types.** Every Aerora radio presents as a serial device and connects
through the existing serial link. Vehicle-side link arbitration is represented as telemetry, not as
a GCS transport concern.

This followed from resolving Q-014 by code analysis: the serial path applies no device-identity
gate at any point, enumerates ports without filtering, and accepts an arbitrary baud rate.

### Alternatives rejected

| Alternative | Reason rejected |
| --- | --- |
| One new link type per radio | Six core edits per radio, in a part of the code with no extension seam |
| One new link type with a radio sub-type | Still six core edits, and Q-012's granularity question becomes load-bearing |
| A link type that multiplexes the three radios | Duplicates vehicle-side arbitration in the GCS, which would then disagree with the aircraft |

The third is the most instructive rejection. A GCS that arbitrates between links has an opinion
about which link is active, and that opinion will sometimes differ from the aircraft's. The
architecture instead displays what the vehicle reports.

### Consequences

| Consequence | Assessment |
| --- | --- |
| R-003 reduced from `HIGH` to `MEDIUM` | Recorded in [phase-2-risk-gates.md](phase-2-risk-gates.md) |
| Q-012 downgraded to deferrable | The granularity question only mattered if new link types existed |
| Planned transport core edits: zero | The single largest reduction in scope this phase produced |
| Aerora radios need an operator-created link on first use | Unknown boards are not auto-connected; the link persists, so it is a one-time action |

Related: `AERORA-COMM-REQ-006`, R-003, Q-012, Q-014.

## ADR-003: Serial radio strategy

### Context

[ADR-002](#adr-002-transport-strategy) makes the serial path load-bearing. Three frictions were
found in it, and one is a trap rather than a limitation.

QGroundControl identifies USB serial devices from a vendor and product identifier table. That table
maps the stock Silicon Labs CP2102 identifiers — and a common FTDI description — to "SiK Radio".

### Decision

Aerora radio hardware uses vendor and product identifiers that do not collide with the existing
board-identification entries, **and** the Aerora settings defaults disable SiK radio auto-connect.

Two independent mitigations for one trap, deliberately.

### Alternatives rejected

| Alternative | Reason rejected |
| --- | --- |
| Add an Aerora entry to the board-identification table | A core edit, and the board-class vocabulary is a closed set with no Aerora-appropriate value |
| Rely on disabling auto-connect alone | Leaves the hardware identifier colliding, so any future re-enablement reintroduces the fault |
| Rely on a non-colliding identifier alone | Depends on a hardware decision the GCS cannot enforce |
| Accept the misidentification | A radio identified as a SiK is auto-connected at a fixed baud and takes the port reservation, so the operator's own link then fails to open |

The last row is why this is a trap and not a limitation: the symptom is an operator-created link
that will not connect, with the cause three subsystems away.

### Consequences

| Consequence | Assessment |
| --- | --- |
| A hardware requirement falls out of a GCS analysis | `AERORA-COMM-REQ-011` is addressed to the hardware team |
| Android serial enumeration remains conditional on a device probe table | A custom identifier may require a core edit to the prober — recorded in [deployment-architecture.md](deployment-architecture.md) |
| iOS is excluded | Serial links are disabled there by build configuration, making iOS a poor fit for a serial-first product |

Related: `AERORA-COMM-REQ-011`, `AERORA-COMM-REQ-012`, R-003, Q-012, Q-032.

## ADR-004: MAVLink extension strategy

### Context

Aerora needs its own message families. QGroundControl checks in no generated message headers: the
entire C library is generated at configure time from a fetched definitions repository, selected by
build variables.

### Decision

Aerora maintains its own fork of the MAVLink definitions repository, selected through the existing
build variables and **pinned to an immutable commit, never a branch**.

The GCS-to-flight-controller interface and the flight-controller-to-companion interface are
separately specified even though they share one dialect, and the GCS implements only the first.

### Alternatives rejected

| Alternative | Reason rejected |
| --- | --- |
| Contribute Aerora messages to the common dialect | Aerora messages are vendor-specific and have no upstream motivation |
| Hand-maintain generated headers in the Aerora tree | Defeats the generator and guarantees drift between definitions and code |
| Track the dialect fork by branch | Two builds of one Aerora commit could embed different definitions, making the compatibility matrix meaningless |
| One undifferentiated dialect with no interface separation | The two interfaces differ in trust level, change cadence and security posture; a shared specification would let the GCS emit companion-interface messages |

### Consequences

| Consequence | Assessment |
| --- | --- |
| Aerora messages, enumerations and QML bindings all arrive automatically | No core edits |
| Dialect pinning becomes a release-process obligation | Appears in three documents because it serves reproducibility, matrix validity and supply-chain integrity |
| The capability inventory becomes the multi-product mechanism | A GCS that adapts to a reported capability set does not need to know which product it is talking to |

Related: `AERORA-MAV-REQ-005` through `AERORA-MAV-REQ-010`, `AERORA-BUILD-REQ-008`, R-011, Q-003.

## ADR-005: Mission and payload state authority

### Context

QGroundControl's mission resume builds a replay prefix from a fixed list of fourteen camera,
region-of-interest and speed commands, so that a resumed mission re-establishes that state. An
unlisted command before the resume point is dropped with no log line.

Phase 1 recorded this as R-002 at `CRITICAL`. Aerora payload commands are unlisted by construction.

The filter is a local automatic variable inside a non-virtual method of a class with no virtual
members, constructed concretely by the vehicle. There is no seam.

### Decision

Two parts, and they are one decision.

**The payload controller is authoritative for payload state.** This is a baseline-supported
ownership statement.

**Preferred architecture: the payload controller retains that state across a mission
interruption**, so the replay prefix has nothing Aerora-specific to carry. **Phase 2.1 records this
second part as `NOT-EVIDENCED`** — it is a vehicle-side verification requirement, not an
established behaviour. See the evidence caveat below.

**The saved Aerora plan file is authoritative for mission intent.** The vehicle mission store is a
derived artifact. This part depends on no external evidence and stands as written.

### Evidence caveat, added in Phase 2.1

The original decision text read: "The payload controller is authoritative for payload state and
retains it across a mission interruption." **That conflated an ownership statement with a
persistence behaviour.** The first is supported by baseline 13; the second is supported by nothing.
Baseline 10 contains no statement about retention, persistence, non-volatile storage or survival
across resume, and the firmware model's payload elements are placeholders.

| Part of the decision | Evidence basis | Status |
| --- | --- | --- |
| Payload controller owns payload state | Baseline 13 | `CONFIRMED` |
| Payload state is reported as telemetry | Baselines 07 and 11 | `CONFIRMED` |
| Payload state survives a mission interruption | None | **`NOT-EVIDENCED`** |
| Plan file is authoritative for mission intent | QGC source analysis; no vehicle dependency | `CONFIRMED` |

The decision is therefore **accepted as the preferred architecture with a verification gate**, not
as a resolved problem. A named fallback exists if retention is not implemented: constrain Aerora
plans so payload-state commands do not precede a plausible resume point, and warn the operator at
resume time. Both are within Aerora's control and neither needs a core edit.

### Alternatives rejected

| Alternative | Reason rejected |
| --- | --- |
| Patch the command list in QGroundControl core | A core edit on a path with no seam, carried forever, with no upstream test coverage for vendor commands |
| Encode payload state in the parameters of an already-listed command | Makes the wire protocol dishonest and the saved plan unreadable |
| Model payload state in the GCS and replay it from there | Puts authoritative state in the ground station, violating the first principle |
| Implement an Aerora mission scanner to rebuild structure from a downloaded mission | Inference presented as fact; the plan file already holds the answer with certainty |
| Contribute the fix upstream and wait | Upstream has no vendor commands, so the change has no upstream motivation |

### Consequences

| Consequence | Assessment |
| --- | --- |
| R-002 reduced from `CRITICAL` to `HIGH` | The filter's `i >= resumeIndex` clause means commands at or after the resume point already survive untouched |
| A GCS risk is **transferred** to a vehicle-side requirement, not resolved | `AERORA-PAYLOAD-REQ-007` is addressed to the payload controller team and is unproven. R-002 stays at `G-ARCH` with an evidence gate |
| R-012 reduced from `MEDIUM` to `LOW` | A mission item has no vendor-extension slot; that is inherent to MAVLink, and plan-file authority resolves it |
| An operator-interface obligation falls out | Resume reproduces the mission from the resume point, not the full history, and the operator must be told |
| A fallback is required | Because the preferred resolution is unproven, the plan-constraint and resume-warning fallbacks are adopted as part of the decision |

The pattern worth noting, stated correctly: the mechanism was built on an assumption — that the
vehicle retains nothing — and the proposal is to make that assumption untrue for Aerora.
**Phase 2.1 is explicit that it is not yet untrue.** Proposing to remove a premise is better
engineering than patching the mechanism built on it, but a proposal is not a removal, and the
original text claimed the latter.

Related: `AERORA-MISSION-REQ-001` through `AERORA-MISSION-REQ-008`, `AERORA-PAYLOAD-REQ-006`,
`AERORA-PAYLOAD-REQ-007`, `AERORA-MAV-REQ-008`, R-002, R-012, Q-023.

## ADR-006: Cloud architecture

### Context

The baseline requires AWS as the primary cloud platform and states that AWS must not become a
hidden single point of failure for flight safety. It lists six capabilities that must work without
AWS.

A cloud dependency on the flight path is rarely introduced deliberately. It arrives one convenience
at a time, each individually defensible.

### Decision

The cloud is **off the command path as a structural property**, not as a stated outcome. Eight
specific routes by which a cloud dependency would arrive are enumerated and refused by name.

There is **no global offline mode**. Each cloud-backed feature has a defined local behaviour.

### Alternatives rejected

| Alternative | Reason rejected |
| --- | --- |
| A global offline mode | Invites the question "are we offline?" where the real question is always "is this specific thing available?" |
| Cloud-validated authentication with an offline fallback | A fallback is the less-tested path, and this one must work on every flight |
| Blocking version or entitlement checks before flight | Makes a commercial system a safety system — the worst form of the trap |
| Streaming telemetry to the cloud as the primary record | A record that exists only after a successful transfer can be suppressed by interfering with the transfer |
| Treating the stated baseline outcome as sufficient | An outcome must be re-verified continuously; a structure is true by inspection |

### Consequences

| Consequence | Assessment |
| --- | --- |
| A test exists that would catch a regression | Full local operation with all network access blocked |
| Two display obligations follow | Cloud-sourced data shows its age; upload queue depth and drop count are visible |
| Cloud connectivity and link connectivity share no visual language | Conflating them makes an outage look like a link problem |
| One Aerora background thread owns all network I/O | A stalled call cannot reach the main thread |

Related: `AERORA-AWS-REQ-005` through `AERORA-AWS-REQ-015`, R-015, Q-027, Q-030, Q-039, Q-042.

## ADR-007: Modular monolith

### Context

The baseline specifies a modular monolith on AWS with eleven internal modules communicating through
typed application interfaces rather than shared database access, preferring a single deployable
backend until scale justifies extraction.

### Decision

Adopted without alteration. The architecture additionally records the GCS-side counterpart of each
backend module, so that the boundary between the two is explicit rather than assumed.

The same reasoning is applied to the GCS itself: fifteen Aerora modules with four enforced
dependency rules, in one application.

### Alternatives rejected

| Alternative | Reason rejected |
| --- | --- |
| Microservices from the outset | The baseline's reasoning holds: understandability and deployability now, extraction later if justified |
| Shared database access between backend modules | Makes future extraction a rewrite rather than a refactor, and couples modules invisibly |
| A GCS without enforced internal module boundaries | The obvious implementations of several features would couple modules that must stay independent |

The third is the one that needed deciding rather than inheriting. Payload and Mission are mutually
independent by rule, and the obvious implementation of "run the paint pattern" would couple them.
The coupling is unnecessary: a mission carries payload commands as mission items and the Payload
module observes the resulting state.

### Consequences

| Consequence | Assessment |
| --- | --- |
| A backend refactor extracting one service is invisible to the GCS | The GCS talks to defined interfaces |
| The Autonomy module is read-only by construction | A deliberate asymmetry — see [ADR-011](#adr-011-companion-authority-boundary) |
| Cloud depends on Security, never the reverse | Prevents the cloud reaching the authorisation path |
| Aerora domain modules run on the main thread by design | Preserves Fact rate limiting; only two Aerora background threads exist |

Related: `AERORA-AWS-REQ-012`, `AERORA-GPU-REQ-003`, Q-030.

## ADR-008: Security architecture

### Context

QGroundControl's MAVLink signing implementation is well engineered: a per-link state machine,
signature-verifying key auto-detection, timestamp persistence for forward progress across restarts,
explicit unsigned-acceptance policy, and non-elidable memory wipes.

It writes the 32-byte key to the application settings store unencrypted. The class has no virtual
members, so the storage backend cannot be replaced from the overlay.

Two further findings: passphrase derivation uses a fixed application-wide salt with an
eight-character minimum, and the application clears all settings — including the keys — when the
settings-schema version changes.

### Decision

Retain the signing implementation unchanged. Address the three findings separately:

| Finding | Decision |
| --- | --- |
| Keys unprotected at rest | **Open remediation.** Preferred route is an upstream contribution adding a pluggable storage backend; an Aerora core edit is the fallback. Not shippable as-is |
| Fixed derivation salt | Provision randomly generated raw keys instead of passphrase-derived keys. No code change required |
| Settings-version coupling | A schema version bump is a release-process decision with a recorded reprovisioning plan |

Operator authorisation is a separate, GCS-side gate applied before a command reaches a link,
validated offline, with an explicitly declared gated-operation set. The advanced-UI toggle is not
part of it.

### Alternatives rejected

| Alternative | Reason rejected |
| --- | --- |
| Replace the whole signing implementation | It is good; only its persistence is wrong |
| Ship with keys in unprotected settings and mitigate operationally | Full-disk encryption and handling procedures mitigate; they do not remediate |
| Derive the gated operation set from interface visibility | Would make the advanced-UI mistake structural |
| Use the advanced-UI toggle as an authorisation boundary | A security control one click deep |
| Treat the vehicle secure element as closing the key-custody gap | It cannot reach a key in a file on the operator's laptop; conflating the two would hide the gap behind real hardware |

### Consequences

| Consequence | Assessment |
| --- | --- |
| This is the architecture's single core-edit candidate | Zero QGC core edits are required by the current architecture; one remains a possible fallback for signing-key storage if upstream extensibility is unavailable |
| R-005 is gated at `G-RELEASE` | It blocks shipping; it does not block the architecture or the implementation |
| Two of the five procedural requirements disappear once key custody moves | Worth doing for that reason alone |
| "A compromised operator host" is out of threat scope | Which is exactly why R-005 matters: it does not create the exposure, it makes it trivially harvestable |

Status: `ACCEPTED WITH AN OPEN REMEDIATION`. The only ADR here not cleanly accepted, and the status
says so.

Related: `AERORA-SEC-REQ-011` through `AERORA-SEC-REQ-020`, `AERORA-BUILD-REQ-009`, R-004, R-005,
Q-010, Q-029.

## ADR-009: Qt licensing neutrality

### Context

Phase 1 recorded R-001 as a licensing risk and presented a binary engineering conclusion. That
overreached: licence interpretation is not an engineering question, and Phase 1 should not have
answered it.

Phase 2 reclassified R-001 as an open legal question with observable technical facts and six
verification items for counsel.

### Decision

**The architecture is licence-neutral by construction.** No architectural decision depends on which
licensing answer is correct.

This document provides no legal interpretation and takes no position on the licensing question.

### Alternatives rejected

| Alternative | Reason rejected |
| --- | --- |
| Assume one licensing outcome and architect for it | Would have to be unpicked if the assumption were wrong, and the assumption is not ours to make |
| Restate Phase 1's engineering conclusion | It exceeded the authority of an engineering analysis; recording that overreach is part of the correction |
| Defer the question entirely with no record | Counsel needs the technical facts assembled, which is work engineering can legitimately do |

### Consequences

| Consequence | Assessment |
| --- | --- |
| R-001 remains a `BLOCKER` at `G-RELEASE` | It blocks release and nothing earlier |
| Architecture and implementation proceed | Neither depends on the answer |
| The deliverable to counsel is technical facts plus six verification items | Not a recommendation |

Related: R-001, Q-004.

## ADR-010: Failsafe boundary

### Context

The baseline distributes safety across flight-controller-local, payload-local, communications,
GCS-supervisory and operator-procedural layers, and explicitly excludes the GCS from being the
safety controller for high-voltage isolation, converter protection, grounding, connector interlock
and tether electrical fault.

The firmware model's failsafe paths are substantially placeholders: the watchdog path is real and
observable, while geofence, velocity and acceleration proposal-rejection paths are skeletons.

### Decision

The architecture provides **five interfaces defined by shape** — failsafe state, reason, source,
severity and reported action — and **deliberately does not prescribe the failsafe action**, its
values, policy, thresholds, or precedence.

The GCS displays, alerts, logs and offers vehicle-reported actions. It does not decide, initiate,
infer, suppress or locally clear.

### Alternatives rejected

| Alternative | Reason rejected |
| --- | --- |
| Specify failsafe actions in the GCS architecture | Writing policy for a layer the GCS has been explicitly excluded from |
| Specify actions against the firmware skeletons | Would produce a specification that looks authoritative and is not |
| A single boolean failsafe state | Collapses five distinct questions and loses what a debrief needs |
| Treat a failsafe that stops being reported as cleared | The GCS would be concluding a safety condition ended; it marks the value stale instead |
| Present the watchdog disarm and payload stop as cause and consequence | They reach the aircraft independently; linking them would be a GCS-side inference |

### Consequences

| Consequence | Assessment |
| --- | --- |
| Q-007 does not block the architecture | The five interfaces are the same whatever the answer |
| The failsafe display cannot be built to completion yet | Correct: it should not be built against invented values |
| Authority mode and flight mode stay separate concepts | Whether they map one-to-one is itself part of Q-007 |

Related: `AERORA-SAFE-REQ-008` through `AERORA-SAFE-REQ-011`, R-006, Q-007.

## ADR-011: Companion authority boundary

### Context

The baseline is explicit that the companion computer is not authoritative: it proposes, the flight
controller decides. Manual authority is structurally dominant, and semi-automatic supervised
operation requires explicit pilot confirmation.

QGroundControl models a companion computer only as a display label for a component identifier.

### Decision

The Aerora Autonomy module is **read-only by construction**: it has no outbound command path at
all. It is the only Aerora domain module without one.

Proposal acceptance must be **explicitly reported** by the flight controller. Where it is not, the
GCS shows the proposal and the effective state side by side and asserts no causal link between
them.

### Alternatives rejected

| Alternative | Reason rejected |
| --- | --- |
| An Autonomy module with a command path plus a policy rule forbidding its use | A policy rule is one code review away from being violated; a missing function is not |
| Infer proposal acceptance by comparing a proposal against subsequent state | Unfalsifiable: an inferred acceptance is indistinguishable from a coincidence, and the audit trail would record a correlation as a decision |
| Let the GCS accept a proposal on the companion's behalf | Would make the GCS an authority it is not |
| Allow GCS-side perception or inference | Would raise the question of whether a ground-side result could become authoritative |
| Treat an absent companion and a silent companion as one state | Different situations, different operator responses |

### Consequences

| Consequence | Assessment |
| --- | --- |
| Companion observability costs nothing | Messages sharing the vehicle system id reach the Aerora FactGroups with no new transport, no second vehicle and no core edit |
| Three requirements land on the hardware and firmware teams | System id sharing, component id choice, and explicit outcome reporting |
| Model deployment and workload management are out of scope | Declared explicitly so the read-only property cannot be eroded by a later feature arriving through this door |
| One test verifies an architectural property rather than a behaviour | A static check that the module emits no MAVLink — the most valuable test in the set |

Related: `AERORA-GPU-REQ-001` through `AERORA-GPU-REQ-008`, `AERORA-MAV-REQ-007`, R-014, Q-008.

## What these decisions have in common

Reading the eleven together, four patterns recur. They are worth naming because they are the
architecture's actual method, and because a future decision can be checked against them.

| Pattern | Where it appears |
| --- | --- |
| Prefer removing an assumption over fixing the mechanism built on it | ADR-005 — propose that the payload controller retain state, rather than patch the replay. Note Phase 2.1's caveat: the proposal is not yet evidence |
| Prefer a structural guarantee over a policy rule | ADR-011 — no command path beats a rule saying not to use one |
| Prefer two independent mitigations where a failure is silent | ADR-003 — non-colliding identifier and disabled auto-connect |
| Prefer refusing to specify over specifying against placeholders | ADR-010 — interfaces by shape, values left open |

And one anti-pattern, recorded because the architecture committed it once and corrected it:
**presenting a conclusion outside the authority of the analysis that produced it**. That is ADR-009,
and the correction is part of the record.

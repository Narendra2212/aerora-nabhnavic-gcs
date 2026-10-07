# Payload Interface Control Document

Payload state ownership, telemetry, commands, actuators, emergency stop, and the point at which
this document stops.

This is the highest-discipline section in Phase 3. The payload is where the architecture has the
most to say and the least evidence, and the two are easy to confuse.

## The six separations

| Concern | Owner | Evidence | Status |
| --- | --- | --- | --- |
| Payload state ownership | Payload controller | `BASELINE-ASSERTED` — baselines 10 and 13 | `CONFIRMED` |
| Payload telemetry | Payload controller reports; ground station displays | `BASELINE-ASSERTED` | `PROPOSED` |
| Payload command interface | Operator originates; payload controller decides | `BASELINE-ASSERTED` | `PROPOSED` |
| Payload actuator interface | Payload controller only | `PLACEHOLDER` | `OPEN` |
| Payload safety and emergency stop | Payload-local, with an independent watchdog path | `MODEL` | `PROPOSED` |
| Painting-specific interface | Product capability profile, not universal core | `PLACEHOLDER` | `OPEN` |

The ground station appears in exactly two rows, both as an observer or a relay. That is the
contract.

## Payload state retention is NOT-EVIDENCED

Stated before anything else, because Phase 2 asserted the opposite and nine documents repeated it.

| Claim | Evidence |
| --- | --- |
| The payload controller **owns** payload state | `BASELINE-ASSERTED` — baseline 13 assigns payload-local safety and interlocks |
| Payload state is **reported** as telemetry | `BASELINE-ASSERTED` — baselines 07 and 11 |
| Payload state **survives a mission interruption** | **`NOT-EVIDENCED`** |

Baseline document 10 contains no statement about retention, persistence, non-volatile storage or
survival across resume. The firmware model's payload elements are placeholders. No bench or flight
evidence exists.

**Ownership does not imply persistence.** A payload controller can own its state and lose it on a
reset, a power cycle, a mode change or a mission replacement.

This document does **not** claim persistence. Where an interface would depend on it, the row says
so and names the fallback.

## The Phase 2.1 decisions, preserved

| Decision | Status in Phase 3 |
| --- | --- |
| QGC core modification for resume filtering | **Rejected — unchanged** |
| Ground-station payload state replay | **Rejected — unchanged** |
| Payload-controller retained state | **Preferred architecture, evidence required** |
| Fallback: constrain plans so payload-state commands do not precede a plausible resume point | Traceable to ADR-005 and the Phase 2.1 reconciliation. **Not a confirmed requirement** |
| Fallback: warn the operator at resume time | Covered by the existing `AERORA-MISSION-REQ-004` |

The first fallback has **no requirement identifier and does not acquire one here.** It is a
conditional obligation that applies only if the evidence lands badly, and Phase 3 introduces no
requirements. It is recorded in `AERORA-MISSION-REQ-003`'s rationale, in the Phase 2.1 evidence
matrix, and in the Phase 3 entry criteria. If the evidence lands badly it becomes a requirement in
Phase 4 scoping — by a human decision, not by this document.

## Interface contracts

| ID | Interface | Direction | Producer | Consumer | Crossing | Update | Freshness | Ack | Authority | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-PAY-001` | Payload identity and version | Vehicle to GCS | Payload controller via flight controller | Payload module, diagnostics | Identity, firmware version | On connection | Current for the connection | None | Payload controller | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-PAY-002` | Payload capability set | Vehicle to GCS | Payload controller | Capability model | Which payload capabilities exist | On connection, on change | Current | None | Payload controller | `NOT-EVIDENCED` | `OPEN` |
| `IF-PAY-003` | Payload state | Vehicle to GCS | Payload controller | Payload module | An enumerated state, values from metadata | `UNKNOWN` | Stamped at parse; stale visible | None | Payload controller | `PLACEHOLDER` | `OPEN` |
| `IF-PAY-004` | Payload health | Vehicle to GCS | Payload controller | Payload module, alerting | Health indication | `UNKNOWN` | Stamped at parse | None | Payload controller | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-PAY-005` | Payload safety state | Vehicle to GCS | Payload controller | Payload module, operator display | Safety state, reported never inferred | `UNKNOWN` | Stamped at parse | None | Payload controller | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-PAY-006` | Payload emergency stop state | Vehicle to GCS | Payload controller or watchdog path | Payload module, authority module | Stop active, and its initiating source | Event | Stamped at parse | None | Vehicle | `MODEL` | `PROPOSED` |
| `IF-PAY-007` | Payload telemetry | Vehicle to GCS | Payload controller | Payload fact group | Payload measurements with metadata units | `UNKNOWN` | Stamped at parse | None | Payload controller | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-PAY-008` | Payload configuration | Bidirectional | Payload controller owns | Payload module | Configuration values, where parameter-backed | Request, response | Per interface | Readback | Payload controller | `NOT-EVIDENCED` | `OPEN` |
| `IF-PAY-009` | Payload command | GCS to vehicle | Operator via GCS | Payload controller via flight controller | An operator-initiated payload command | Event | Not applicable | **Required; effectiveness needs a reported state change** | Payload controller decides | `NOT-EVIDENCED` | `OPEN` |
| `IF-PAY-010` | Operator emergency stop request | GCS to vehicle | Operator via GCS | Payload controller via flight controller | A stop request | Event | Not applicable | Required; the stop is effective when reported | Payload controller decides | `NOT-EVIDENCED` | `OPEN` |
| `IF-PAY-011` | Payload calibration | Bidirectional | Operator initiates; payload reports | Payload module | Procedure start, progress, outcome | Event | Stamped at parse | Required | Payload controller | `BASELINE-ASSERTED` | `OPEN` |
| `IF-PAY-012` | Payload diagnostics | Vehicle to GCS | Payload controller | Diagnostics module | Diagnostic reporting | `UNKNOWN` | Stamped at parse | None | Payload controller | `BASELINE-ASSERTED` | `OPEN` |
| `IF-PAY-013` | Payload actuator interface | **Internal to the vehicle** | Payload controller | Actuators | Actuator drive | — | — | — | **Payload controller only** | `PLACEHOLDER` | `OPEN` |
| `IF-PAY-014` | Payload state retention across interruption | Vehicle internal | Payload controller | Itself | Retained state across a mission replacement | — | — | — | Payload controller | **`NOT-EVIDENCED`** | `OPEN` |
| `IF-PAY-015` | Ground station to payload actuators | **None — prohibited** | — | — | Nothing | — | — | — | **No authority** | `GCS-DECIDED` | `CONFIRMED` |

| ID | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- |
| `IF-PAY-001` | Unknown payload identity; the compatibility matrix records it as unknown rather than assuming a default | `AERORA-PAYLOAD-REQ-006` | — | — | `INTEGRATION` |
| `IF-PAY-002` | No capability set means no payload controls are presented at all | `AERORA-PAYLOAD-REQ-013` | — | Q-023 | `INTEGRATION` |
| `IF-PAY-003` | State unknown; never inferred from a command the GCS sent | `AERORA-PAYLOAD-REQ-006`, `AERORA-GCS-REQ-029` | R-002 | Q-023 | `INTEGRATION` |
| `IF-PAY-004` | Health unknown; shown as unknown, not as healthy | `AERORA-PAYLOAD-REQ-006` | — | — | `INTEGRATION` |
| `IF-PAY-005` | Safety state unknown; **never inferred**, and no payload condition reaches a flight command | `AERORA-PAYLOAD-REQ-010` | — | Q-009 | `INTEGRATION` |
| `IF-PAY-006` | Shown as a distinct event from payload state and from the watchdog disarm; never presented as a consequence of either | `AERORA-PAYLOAD-REQ-008`, `AERORA-SAFE-REQ-011` | — | Q-009 | `INTEGRATION` |
| `IF-PAY-007` | Values stale; units from metadata, never assumed | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-027` | — | Q-023 | `INTEGRATION` |
| `IF-PAY-008` | Write without readback is not treated as applied | `AERORA-BUILD-REQ-011` | — | Q-023 | `INTEGRATION` |
| `IF-PAY-009` | Accepted but not effective is **surfaced to the operator** | `AERORA-PAYLOAD-REQ-011`, `AERORA-GCS-REQ-016` | R-002 | Q-023 | `INTEGRATION` |
| `IF-PAY-010` | The request is not the stop; the stop is effective only when the vehicle reports it | `AERORA-PAYLOAD-REQ-009` | — | Q-009 | `INTEGRATION` |
| `IF-PAY-011` | Progress is **observed, not simulated**; the GCS models no calibration progress of its own | `AERORA-PAYLOAD-REQ-013` | — | Q-043 | `BENCH` |
| `IF-PAY-012` | Logged; surfaced in diagnostics; not parsed to derive state | `AERORA-GCS-REQ-029` | — | Q-043 | `INTEGRATION` |
| `IF-PAY-013` | Not applicable — the ground station has no visibility and no path | `AERORA-PAYLOAD-REQ-010` | — | Q-023 | `STATIC` |
| `IF-PAY-014` | **If absent, the mission fallback applies**: constrain plans and warn at resume | `AERORA-PAYLOAD-REQ-007`, `AERORA-MISSION-REQ-003` | R-002 | Q-023 | `BENCH` |
| `IF-PAY-015` | Not applicable — no path exists | `AERORA-PAYLOAD-REQ-010` | — | — | `STATIC` |

## Representation in the ground station

### Two fact groups, deliberately

| Group | Scope | Layer |
| --- | --- | --- |
| Product-independent payload abstraction | Identity, capabilities, health, safety state | Extension layer |
| Product-specific application telemetry | Painting, and later other products | Application layer |

The split is the mechanism by which the platform serves more than one Aerora product. Anything that
would have to be rewritten for a different payload belongs in the second group; anything that would
not belongs in the first. If the line blurs, the second product pays for it.

### The generic payload interface

Baseline 10 defines ten elements. All ten are adopted **as shape**:

| Element | Interface | Content status |
| --- | --- | --- |
| Identity | `IF-PAY-001` | `PROPOSED` |
| Version | `IF-PAY-001` | `PROPOSED` |
| Capabilities | `IF-PAY-002` | `OPEN` |
| Health | `IF-PAY-004` | `PROPOSED` |
| Telemetry | `IF-PAY-007` | `OPEN` |
| Configuration | `IF-PAY-008` | `OPEN` |
| Commands | `IF-PAY-009` | `OPEN` |
| Calibration | `IF-PAY-011` | `OPEN` |
| Safety state | `IF-PAY-005` | `PROPOSED` |
| Diagnostics | `IF-PAY-012` | `OPEN` |

Seven of ten are `OPEN`. That is the honest state of the payload interface, and it is why the
capability set in `IF-PAY-002` is the gating mechanism: with no capability set reported, no payload
control is presented, and the ground station is correct rather than broken.

### Payload parameters are preferably not MAVLink parameters

| Reason | Detail |
| --- | --- |
| QGroundControl's parameter subsystem assumes the autopilot is component 1 | Both the parameter manager and the fact panel controller address component 1 directly — `GCS-VERIFIED` |
| Payload configuration changes on a different cadence from flight parameters | Coupling them means a payload change invalidates a flight-parameter cache |
| It keeps the downward boundary clean | A payload configuration error cannot present as a flight-controller parameter error |

Where payload configuration genuinely must be a MAVLink parameter, it is a Fact like any other. The
all-parameters-are-Facts principle has no payload exemption.

## Emergency stop

### An independent path

Corrected in Phase 3.1 to separate what the **aircraft** is described as doing from what the
**ground station** receives. The two are not the same claim and the first does not license the
second.

#### Aircraft side

Baseline 13 records two observed paths in the uploaded firmware model: watchdog fault into disarm
logic, and watchdog fault **directly** into payload emergency stop.

| Property | Position | Evidence | Status |
| --- | --- | --- | --- |
| A watchdog exists as a distinct device | Named nets and a second MCU on the schematic | `ARTIFACT-OBSERVED` via baseline 46 | `CONFIRMED` for presence |
| Watchdog fault reaches disarm logic | An observed model path | `MODEL` via baseline 13 | `PROPOSED` |
| Watchdog fault reaches payload emergency stop, **directly** | An observed model path | `MODEL` via baseline 13 | `PROPOSED` |
| The two paths are independent of each other | Implied by "directly", which bypasses the disarm path | `MODEL` via baseline 13 | `PROPOSED` |
| Timeout, reset and latch behaviour | **Unresolved** — baseline 46 lists it as open | `NOT-EVIDENCED` | `OPEN` |

Baseline 13 treats these paths as **design inputs, not proof of completion**, and directs that each
"must be converted into a formal requirement and verified against the real FC hardware". None has
been. No Simulink artifact exists in this repository, so every `MODEL` row above is the baseline's
description of the model rather than an inspection of it.

#### Ground-station side

What the ground station actually receives is **separate reported items**. No evidenced message
carries the linkage between them.

| Obligation | Reason |
| --- | --- |
| Show the reported watchdog state, with its receive time | It is a reported value like any other |
| Show the reported arming state, with its receive time | Likewise, and from a different report |
| Show the payload emergency-stop state **as a separate event** | Likewise, and from a third report |
| Show the stop's **initiating source** only where the vehicle reports one | `IF-PAY-006` carries that field, and the field itself is `NOT-EVIDENCED` |
| Do **not** present any of the three as caused by another | **No reported field carries causation.** Asserting it would be a ground-station inference, prohibited by `AERORA-GCS-REQ-029` |
| Do **not** present temporal order as causal order | Two events arriving in sequence is not a reported relationship |
| Represent the stop separately from the payload state enumeration | It is an independent cut, not a state in a sequence |

The distinction is narrow and it is the whole point. The aircraft-side relationship is `PROPOSED` on
`MODEL` evidence; the **reported** relationship is `NOT-EVIDENCED`. A ground station that drew an
arrow between a watchdog event and a disarm would be rendering the first as if it were the second.

Folding the stop into the state enumeration would also make it impossible to record "the payload was
in state X when the stop fired", which is exactly what a debrief needs.

### What the ground station may and may not do

| Action | Permitted |
| --- | --- |
| Display payload safety state and emergency-stop state | Yes |
| Offer the operator an emergency-stop command | Yes — an operator-initiated command like any other |
| Treat its own stop command as the stop | **No** — the stop is effective when the vehicle reports it |
| Initiate a payload emergency stop autonomously as a safety response | **No** — the ground station is not the safety controller |
| Infer payload safety state from payload telemetry | **No** |
| Allow a payload fault to trigger any flight-control command | **No** |

Offering the command is right. Believing it worked is not.

### Not specified

The emergency stop's **behaviour** — what it does, how it is cleared, whether it is distinct from a
mode change — is Q-009 and is not specified here. Only its representation is.

## Painting as a product profile

Painting is the first concrete Aerora product. It is **not** the universal architecture.

| Universal | Painting-specific |
| --- | --- |
| Payload identity, capabilities, health, safety state | Paint-specific telemetry |
| Payload command and acknowledgement semantics | Pump, nozzle, spray-rate semantics |
| The capability-gating mechanism | Which capabilities a painting payload declares |
| Emergency-stop representation | What a painting emergency stop does |

The universal half is specified above. The painting half is `OPEN` and depends on Q-023. The
capability model that keeps them separate is in [17-capability-model.md](17-capability-model.md).

## Conflicts recorded

| Conflict | Position A | Position B | Authority needed |
| --- | --- | --- | --- |
| Whether the payload controller is a distinct physical device or a partition of the flight controller | Baselines 10 and 13 describe it as a distinct authority with its own local safety | Baseline 50's schematic evidence names no separate payload MCU; the firmware model contains payload elements inside the flight-controller model | Hardware owner. The difference changes whether `IF-PAY-003`'s producer is a separate component with its own component id, or the flight controller reporting on behalf of an internal subsystem — and therefore what the reporting-source field records |

Both positions are preserved. Every payload interface above names the producer as "payload
controller via flight controller", which is correct under either reading and commits to neither.
Recorded as Q3-006.

## Not specified, and why

| Item | Reason |
| --- | --- |
| Payload state enumeration members | Q-023; firmware placeholders. Inventing them would put unconfirmed values into a dialect, a metadata file, a log schema, an audit record and a display simultaneously |
| Actuator enumerations | Q-023 |
| Pump, gimbal or nozzle command identifiers | Q-023; no dialect |
| Emergency-stop behaviour | Q-009 |
| Payload state persistence | Evidence matrix item 14 |
| Calibration procedures | Q-043 |
| Diagnostic content | Q-043 |
| Reaction-force compensation interface | Firmware placeholder |
| Payload component id | Conflict above; Q3-006 |

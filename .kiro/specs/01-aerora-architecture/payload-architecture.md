# Payload Architecture

How the Aerora GCS represents, observes and commands a payload, where payload authority lives, and
the point at which this document deliberately stops.

## The two boundaries that define this document

Payload architecture is constrained from both sides, and naming both constraints first keeps the
rest of the document short.

| Boundary | Statement |
| --- | --- |
| Downward | Payload concerns must not leak into flight-control concerns. A payload fault must not be able to reach the flight-control path through the GCS |
| Upward | The GCS does not own payload state. The payload controller does |

The first is the eighth architectural principle. The second is the conclusion of the R-002 analysis
and is the single most consequential decision in this document.

## Payload state authority

### Where it lives

The payload controller is authoritative for payload state. The GCS observes that state, displays
it, logs it, and sends operator-initiated commands. It does not maintain a parallel model it
believes to be true.

```text
Operator → GCS command → FC → payload controller → payload state changes
                                       │
                                       └── reported as telemetry → GCS display + log
```

The GCS never closes that loop locally. A command marked `ACCEPTED` does not change any displayed
payload state; only a reported state change does. This is the effective-over-requested principle,
and in the payload case it also happens to be what makes mission resume survivable.

### Why this follows from R-002

QGroundControl's mission resume builds a replay prefix of selected commands so that a resumed
mission re-establishes camera and speed state. The filter is a local automatic variable of fourteen
commands inside `MissionManager::generateResumeMission()` with no `else` branch — an unlisted
command before the resume index is silently dropped.

Two facts are established by reading the source, and one is a proposal:

| Statement | Basis | Consequence |
| --- | --- | --- |
| The filter is a local variable in a non-virtual method of a class with no virtuals | **Verified** in QGC source | There is no seam. Changing it means editing QGC core |
| The filter clause `i >= resumeIndex` passes everything at or after the resume point | **Verified** in QGC source | Aerora commands at or after resume already survive untouched |
| The replay exists because QGroundControl assumes the vehicle retains nothing | **Verified** — inferred from the de-duplication logic | The premise could be removed if the Aerora payload controller retained state |

The third row is where Phase 2 overstepped. It originally read that the premise "is wrong for
Aerora, which has a payload controller". **Phase 2.1 corrects that.** Having a payload controller
establishes ownership of payload state; it does not establish that the state survives a mission
interruption. No baseline document states that it does, and no firmware evidence has been produced.

So the proposed fix — the payload controller retaining payload state across an interruption — is a
**vehicle-side verification requirement with evidence status `NOT-EVIDENCED`**, not a property
Aerora already has. Full treatment, including the fallback if retention is not implemented, in
[mission-architecture.md](mission-architecture.md). Recorded as ADR-005; evidence item in the
[hardware and firmware evidence matrix](hardware-firmware-evidence-matrix.md).

Three alternatives were considered and remain rejected regardless of how the evidence lands:

| Rejected | Reason | Still rejected in Phase 2.1 |
| --- | --- | --- |
| Patch `includedResumeCommands` in QGC core | A core edit on a path with no seam, carried forever against upstream | Yes |
| Hide payload state in the parameters of an already-allow-listed command | Makes the wire protocol dishonest and the mission file unreadable | Yes |
| Model payload state GCS-side and replay it from the GCS | Puts authoritative state in the ground station, violating the first principle | Yes |

## Payload parameters are not MAVLink parameters

Where payload configuration must be persistent and operator-editable, the preferred representation
is payload-controller-owned configuration reported and set through the payload interface — not
MAVLink parameters on a payload component.

| Reason | Detail |
| --- | --- |
| QGroundControl's parameter subsystem assumes the autopilot is component 1 | Both `ParameterManager` and `FactPanelController` address component 1 directly |
| Payload configuration changes on a different cadence from flight parameters | Coupling them means a payload change invalidates a flight-parameter cache |
| It keeps the downward boundary clean | A payload configuration error cannot present as a flight-controller parameter error |

Where payload configuration genuinely must be a MAVLink parameter, it is a Fact like any other —
the all-parameters-are-Facts principle has no payload exemption.

## Representation in the GCS

### Two FactGroups, deliberately

| Group | Scope | Layer |
| --- | --- | --- |
| `aeroraPayload` | Product-independent payload abstraction: identity, capabilities, health, safety state | L3 extension layer |
| `aeroraPaint` | Product-specific application telemetry | L2 application module |

The split is not cosmetic. It is the mechanism by which the platform serves more than one Aerora
product. Anything that would have to be rewritten for a different payload belongs in `aeroraPaint`;
anything that would not belongs in `aeroraPayload`. If that line is blurred, the second product
pays for it.

### The generic payload interface

The baseline defines a generic payload interface with ten elements. The architecture adopts all ten
as the shape of `aeroraPayload`:

| Element | GCS treatment |
| --- | --- |
| Identity | Displayed; recorded in the log header and the compatibility matrix |
| Version | Part of the compatibility matrix |
| Capabilities | Drives which payload controls are presented at all |
| Health | Displayed with freshness; drives operator alerting |
| Telemetry | Facts with metadata-supplied units and bounds |
| Configuration | Displayed; editable where the payload interface permits |
| Commands | Operator-initiated, tracked through the command state model |
| Calibration | Operator-initiated procedure, observed not simulated |
| Safety state | Displayed prominently; never inferred |
| Diagnostics | Logged; surfaced in the diagnostics view |

Capabilities doing the gating is the same technique as in
[vehicle-integration.md](vehicle-integration.md): one GCS build, many payloads, no product
branching in the UI.

### Freshness applies without exception

Every value in both groups carries a receive time and a reporting source, stamped at parse time.
Payload health and payload safety state are the two where a stale value shown as current would be
most misleading, so neither is ever retained past its staleness threshold.

## Payload safety

### The emergency stop is an independent path

The baseline describes a watchdog fault acting directly: disarm, and a payload emergency stop that
does not route through the payload state machine. The architecture preserves that independence in
how it represents the payload:

| Element | Representation |
| --- | --- |
| Payload safety state | A Fact in `aeroraPayload`, reported by the payload controller |
| Emergency stop active | A distinct Fact, not a value of the payload state enum |
| Emergency stop source | Reported, so operator-initiated and watchdog-initiated are distinguishable |

Folding the emergency stop into the payload state enum would imply it is one state among a sequence
the payload moves through. It is not; it is an independent cut. Representing it as an enum value
would also make it impossible to show "payload was in state X when the stop fired", which is
exactly what a debrief needs.

### What the GCS may and may not do

| GCS action | Permitted |
| --- | --- |
| Display payload safety state and emergency-stop state | Yes |
| Offer the operator an emergency-stop command | Yes — it is an operator-initiated command like any other |
| Treat its own emergency-stop command as the stop | No — the stop is effective when the vehicle reports it |
| Initiate a payload emergency stop autonomously as a safety response | No — the GCS is not the safety controller |
| Infer payload safety state from payload telemetry | No |
| Allow a payload fault to trigger any flight-control command | No — the downward boundary |

The second and third rows together are the whole of it. Offering the command is right; believing it
worked is not.

## Command path

Payload commands use the eight-state command model in
[authority-model.md](authority-model.md) — `REQUESTED`, `SENT`, `RECEIVED`, `ACCEPTED`, `REJECTED`,
`EFFECTIVE`, `STALE`, `FAILED` — with no payload-specific additions.

Two payload-specific notes:

| Note | Reason |
| --- | --- |
| `EFFECTIVE` for a payload command requires a reported payload state change, not an acknowledgement | An acknowledgement means the command arrived, not that the actuator moved |
| A payload command that reaches `ACCEPTED` and never reaches `EFFECTIVE` is surfaced, not silently left pending | This is the failure an operator most needs to see and the one most easily hidden |

## Where this document stops

**The architecture defines the shape of payload state. It does not define the state names or
values.** That line is drawn deliberately and here is why.

The baseline's payload elements — gimbal pan and tilt, spray on and off, spray rate, pump control,
payload power and safety interlocks, reaction-force compensation — are present in the firmware
model as placeholders. The actuator model itself is Q-023. Writing a payload state enumeration now
would mean inventing values, and those values would then appear in a dialect, a Fact metadata file,
a log schema and an operator display before anyone had confirmed them.

What is committed:

| Committed | Not committed |
| --- | --- |
| Payload state is an enumerated Fact with metadata-supplied names | The enumeration's members |
| Transitions are reported by the payload controller, never inferred | The transition graph |
| The emergency stop is separate from the state enum | What the stop's reported detail contains |
| Capabilities gate which controls are presented | Which capabilities exist |
| Safety state and health are distinct | Their value sets |

This is the unconfirmed-is-not-implemented principle applied to the place where it is most tempting
to violate. An invented payload state enumeration would look like progress and would have to be
unpicked from five artifacts later.

Q-023 is ranked fifth in
[phase-2-open-question-resolution.md](phase-2-open-question-resolution.md), routed to the hardware
and firmware teams. It is architecture-shaping, not architecture-blocking: the Fact, its metadata
file and its display are the same regardless of the answer.

## Module dependencies

The Payload module and the Mission module are mutually independent, per the third dependency rule
in [component-architecture.md](component-architecture.md).

This is worth stating as a rule rather than an observation, because the obvious implementation of
"run the paint pattern" would couple them. The coupling is unnecessary: a mission carries payload
commands as mission items, and the Payload module observes the resulting payload state. Neither
needs a reference to the other.

| Dependency | Permitted |
| --- | --- |
| Payload → Mission | No |
| Mission → Payload | No |
| Both → Aerora extension layer | Yes |
| UI → both | Yes |

## Core edits required

**None.** Payload representation attaches through `FirmwarePlugin::factGroups()`; payload commands
use existing command sending; payload displays attach at UI seams covered in
[ui-architecture.md](ui-architecture.md).

The one place a core edit was tempting — `includedResumeCommands` — is resolved by putting state
authority where it belongs instead.

## Open questions

| Question | Subject | Impact |
| --- | --- | --- |
| Q-023 | Payload actuator model, which decides the payload state enumeration | `ARCH-SHAPING` |
| Q-009 | Payload emergency-stop behaviour | `ARCH-SHAPING` |
| Q-043 | Payload calibration and diagnostics detail | `DEFERRABLE` |

## Requirements derived

| Identifier | Statement |
| --- | --- |
| `AERORA-PAYLOAD-REQ-006` | The payload controller shall be authoritative for payload state; the GCS shall not maintain an independent payload state model |
| `AERORA-PAYLOAD-REQ-007` | The payload controller shall retain payload state across a mission interruption so that mission resume requires no GCS-side command replay |
| `AERORA-PAYLOAD-REQ-008` | Payload emergency stop shall be represented separately from the payload state enumeration, with its initiating source reported |
| `AERORA-PAYLOAD-REQ-009` | The GCS shall not initiate a payload emergency stop as an autonomous safety response |
| `AERORA-PAYLOAD-REQ-010` | No payload condition shall cause the GCS to issue a flight-control command |
| `AERORA-PAYLOAD-REQ-011` | A payload command that is accepted but never becomes effective shall be surfaced to the operator |
| `AERORA-PAYLOAD-REQ-012` | Product-independent payload state shall be separated from product-specific application telemetry into distinct FactGroups |
| `AERORA-PAYLOAD-REQ-013` | Payload controls shall be presented according to the reported payload capability set |

Full records in [requirements.md](requirements.md).

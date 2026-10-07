# Telemetry Interface Control Document

The canonical telemetry model, and the mandatory separation of reported, effective and proposed
state.

## The three kinds of state

**This distinction is mandatory.** Every telemetry item in the system is exactly one of these, and
conflating them is the error the operator pays for.

| Kind | Meaning | Source | Presentation |
| --- | --- | --- | --- |
| **Reported** | What a component says is true | A vehicle component | Primary, with its age |
| **Effective** | What the aircraft is actually doing, as reported by the deciding authority | The flight controller | **Primary and authoritative** |
| **Proposed** | What something has suggested or requested but which is not in force | Operator, or companion | **Visually subordinate, explicitly labelled** |

### Why it matters

| Conflation | Operator consequence |
| --- | --- |
| Proposed shown as effective | The operator believes the aircraft is doing what was suggested |
| Requested shown as effective | The operator believes a command took effect when it was only sent |
| Reported shown as effective | The operator attributes a relayed value to the deciding authority |
| Stale shown as current | The operator acts on data that was true some time ago |

The second architectural principle — effective over requested — exists because of the second row.
It is not a display preference; it is the reason the eight-state command model has an `EFFECTIVE`
state at all.

### Worked separation

| Concept | Reported | Effective | Proposed |
| --- | --- | --- | --- |
| Authority mode | Mode value in telemetry | The mode the flight controller says is in force | A mode the operator has requested |
| Payload state | State value in telemetry | The reported state, since the payload controller is the authority | A payload command the operator has sent |
| Autonomy | Companion perception output | The resulting vehicle behaviour | A companion proposal |
| Active link | Each link's validity | The link the vehicle says it is using | A GCS-local observation — **never promoted** |
| Power source | Measurements | The source the vehicle says is active | — |
| Mission progress | Current index | The index the vehicle reports | Nothing; progress is never estimated |

The fourth row is the one most easily got wrong. A ground station's local observation of which link
is carrying traffic is a **proposed** reading of the active link, and the vehicle's report is the
effective one. They disagree sometimes, and the vehicle is right.

## The canonical item model

Every Aerora telemetry item carries these fields. Where a value is unknown, the field says so
rather than being omitted.

| Field | Meaning |
| --- | --- |
| Name | Fact name within its group |
| Kind | Reported, effective or proposed |
| Source | The reporting component |
| Destination | The consuming Aerora module |
| Unit | From Fact metadata, never assumed |
| Type | Scalar, enumerated, boolean, text, opaque |
| Update mode | Streamed, event, on-request, on-connection |
| Expected rate | Only where evidenced |
| Freshness | Receive time, stamped at parse |
| Stale behaviour | What happens past the threshold |
| Display priority | Primary, secondary, diagnostic |
| Logging requirement | Telemetry record, audit record, or both |
| Security sensitivity | Whether the value is sensitive |
| Capability dependency | Which reported capability gates it |
| Evidence status | From the Phase 2.1 vocabulary |

## The value triple

Every Aerora value carries three things, and the triple is **never reduced** on its way to a display
or a record.

| Element | Why |
| --- | --- |
| Value | The datum |
| Receive time | Stamped at parse — the only moment freshness is knowable |
| Reporting source | A reading from the flight controller and the same reading relayed by a companion are not equally trustworthy |

Reconstructing freshness after the fact is impossible, which is why the stamp is at parse and not at
display. The reporting-source half depends on messages identifying their originator, which is
`IF-MAV-022` and is `OPEN`.

## Interface contracts

| ID | Interface | Direction | Producer | Consumer | Update | Freshness | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-TLM-001` | Standard vehicle telemetry | Vehicle to GCS | Flight controller | Vehicle model, standard handling | Streamed | Stamped at parse | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-TLM-002` | Aerora system telemetry | Vehicle to GCS | Flight controller | Aerora system fact group | `UNKNOWN` | Stamped at parse | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-TLM-003` | Aerora payload telemetry | Vehicle to GCS | Payload controller via flight controller | Aerora payload fact group | `UNKNOWN` | Stamped at parse | `BASELINE-ASSERTED` | `OPEN` |
| `IF-TLM-004` | Aerora product telemetry | Vehicle to GCS | Payload controller via flight controller | Product-specific fact group | `UNKNOWN` | Stamped at parse | `PLACEHOLDER` | `OPEN` |
| `IF-TLM-005` | Aerora power telemetry | Vehicle to GCS | Power system via flight controller | Aerora power fact group | `UNKNOWN` | Stamped at parse | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-TLM-006` | Aerora link telemetry | Vehicle to GCS | Flight controller arbitration | Aerora link fact group | `UNKNOWN` | Stamped at parse | `MODEL` | `PROPOSED` |
| `IF-TLM-007` | Aerora autonomy telemetry | Companion via flight controller | Companion | Aerora autonomy fact group | `UNKNOWN` | Per-class threshold | `MODEL`, partly future work | `OPEN` |
| `IF-TLM-008` | Fact group registration | GCS internal | Firmware abstraction | Vehicle model | On vehicle construction | Not applicable | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-TLM-009` | Fact metadata | GCS internal | Aerora metadata files | Fact system | Build time | Not applicable | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-TLM-010` | Freshness stamping | GCS internal | Parse point | All consumers | Every message | The stamp itself | `GCS-DECIDED` | `PROPOSED` |
| `IF-TLM-011` | Staleness evaluation | GCS internal | Freshness evaluator | Display, logging | Continuous | Per-class threshold | `GCS-DECIDED` | `PROPOSED` |
| `IF-TLM-012` | Telemetry record | GCS internal | Vehicle fact enumeration | Local storage | Periodic | Carried with each value | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-TLM-013` | Telemetry value selection | GCS internal | Operator | Instrument panel | Operator action | Not applicable | `GCS-VERIFIED` | `CONFIRMED` |

| ID | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- |
| `IF-TLM-001` | Stale values marked; standard handling otherwise unchanged | `AERORA-GCS-REQ-027` | — | — | `INTEGRATION` |
| `IF-TLM-002` | Absent Aerora telemetry degrades the display to the standard set | `AERORA-MAV-REQ-009` | — | — | `INTEGRATION` |
| `IF-TLM-003` | Payload values unknown; no independent model, no inference from commands | `AERORA-PAYLOAD-REQ-006` | R-002 | Q-023 | `INTEGRATION` |
| `IF-TLM-004` | Product telemetry absent means the product panel is not presented | `AERORA-PAYLOAD-REQ-012` | — | Q-023 | `INTEGRATION` |
| `IF-TLM-005` | Power values unknown; shown as unknown, never estimated | `AERORA-POWER-REQ-008` | — | Q-024 | `INTEGRATION` |
| `IF-TLM-006` | Vehicle-reported link state stale; GCS-local observation shown separately | `AERORA-COMM-REQ-007`, `AERORA-COMM-REQ-008` | — | Q-015 | `INTEGRATION` |
| `IF-TLM-007` | Proposals marked stale then absent; acceptance never inferred | `AERORA-GPU-REQ-005`, `AERORA-GPU-REQ-006` | R-014 | Q-008 | `INTEGRATION` |
| `IF-TLM-008` | Registration happens at vehicle construction; **no fact group is created dynamically** | `AERORA-GCS-REQ-010`, `AERORA-GCS-REQ-022` | R-027 | — | `UNIT` |
| `IF-TLM-009` | A missing metadata entry means the value is not presented, rather than presented without units | `AERORA-GCS-REQ-026` | — | — | `UNIT` |
| `IF-TLM-010` | A value that reaches a consumer without a stamp is a defect, detectable by test | `AERORA-GCS-REQ-027` | — | — | `UNIT` |
| `IF-TLM-011` | **A stale value is never presented as current**; thresholds are per class | `AERORA-GCS-REQ-027`, `AERORA-GPU-REQ-006` | — | Q-036 | `UNIT` |
| `IF-TLM-012` | Write failure surfaced; the writer drops with an accounted counter rather than blocking | `AERORA-GCS-REQ-025`, `AERORA-COMPLY-REQ-004` | — | Q-039 | `UNIT` |
| `IF-TLM-013` | Aerora values appear in the selector because the groups are registered on the vehicle | `AERORA-GCS-REQ-010` | — | — | `INTEGRATION` |

## Fact group structure

Six Aerora fact groups, all created at vehicle construction.

| Group | Carries | Kind | Primary consumer |
| --- | --- | --- | --- |
| Aerora system | System status, dialect version, capability inventory, authority mode, identity | Reported and effective | Diagnostics, Authority |
| Aerora payload | Payload state, health, safety state | Reported | Payload |
| Aerora product | Product-specific application telemetry | Reported | Product interface |
| Aerora power | Tether and power-chain state | Reported | Power |
| Aerora link | Vehicle-reported link arbitration and per-radio status | Reported and effective | LinkState |
| Aerora autonomy | Companion proposals, perception status, outcome | **Proposed** and reported | Autonomy |

### Three properties this attachment buys

All `GCS-VERIFIED`, from registering through the firmware abstraction rather than beside it.

| Property | Mechanism |
| --- | --- |
| Automatic MAVLink delivery | Every inbound message is offered to every fact group |
| Appearance in the operator's telemetry value selector | The selector enumerates the vehicle's groups |
| Inclusion in the telemetry record | The record writer enumerates group facts |

### One constraint it imposes

**No fact group may be created dynamically after vehicle construction.**

The telemetry record's column header is written once at file open from the groups that exist then;
each row is written from the groups that exist when the row is written. A group appearing later
shifts every subsequent column. Aerora's six are constructor-time, so Aerora is safe — and must stay
that way. This is R-027 and `AERORA-GCS-REQ-022`.

## Rates, and why none are specified

| Property | Status |
| --- | --- |
| Expected message rates | **Not specified** — not evidenced anywhere |
| Per-class staleness thresholds | Shape specified; **values not** |
| Main-thread Fact update budget | A real constraint; the architecture keeps domain modules on the main thread to preserve rate limiting |
| Channel budget | 15 channels, one per active link |

Rates are needed for two separate purposes and neither can be served yet:

| Purpose | Blocked by |
| --- | --- |
| Sizing staleness thresholds | No rates |
| Checking the main-thread Fact budget against R-013 | No rates, and no worst-case link count (Q-015) |

Specifying a rate here would be inventing a number on a path where a wrong number produces either
false staleness on a slow link, or an operator waiting for a value that will never update.

## Display priority

| Priority | Content | Rule |
| --- | --- | --- |
| Primary | Effective state: authority mode, arming, position, altitude, active link, power source, payload safety state | Always visible when the capability is reported |
| Secondary | Reported detail: per-link status, individual sensor health, payload telemetry | Available on request |
| Subordinate | **Proposed** state: companion proposals, requested modes, pending commands | Visually subordinate and explicitly labelled |
| Diagnostic | Versions, counters, drop counts, queue depths | Diagnostics view |

The subordinate tier is a presentation rule with a safety rationale, not an aesthetic choice. Equal
visual weight invites the reading that the aircraft is following the proposal.

## Logging requirement

| Item class | Telemetry record | Audit record |
| --- | --- | --- |
| Vehicle state | Yes | No |
| Authority mode, effective | Yes | **Yes** — and separately from the request |
| Authority mode request | No | **Yes** |
| Payload state | Yes | No |
| Payload command | No | **Yes** |
| Companion proposal | Yes | Only where it resulted in an operator action |
| Power state and faults | Yes | Faults only |
| Command state transitions | No | **Yes**, all of them |
| Authorisation denial | No | **Yes** |

The audit record is **unconditional** — not gated on armed state, telemetry settings or vehicle
connection — because the telemetry record's own writer only opens once the vehicle is armed, which
would exclude every pre-arm operator action.

## Security sensitivity

| Class | Items | Treatment |
| --- | --- | --- |
| Not sensitive | Vehicle state, power measurements, mission progress | Logged and uploadable |
| Operationally sensitive | Position history, mission plans, fleet identity | Logged locally; upload governed by policy |
| Security sensitive | Credentials, signing keys | **Never telemetry, never logged** |

No telemetry item carries credentials or key material. That is a contract, and the signing key
store's exported-hex accessor is the reason it is worth stating.

## Capability dependency

Every Aerora telemetry group is gated on a reported capability. With no capability inventory, no
Aerora group is presented and the interface degrades to the standard QGroundControl set.

That is what makes the first implementation increment buildable before the dialect exists: the
degraded path is the **initial** path, not an error path.

## Conflicts recorded

| Conflict | Position A | Position B | Authority needed |
| --- | --- | --- | --- |
| Whether a relayed value's reporting source is the originator or the relay | `AERORA-GCS-REQ-027` requires the reporting source; the architecture intends the originator | MAVLink identifies the sending component, which for a relayed payload or power value is the flight controller, not the originating subsystem | Firmware owner, via `IF-MAV-022` |

Both preserved. Until a message identifies the originator, the ground station records the **sending
component** and must not present it as the originating subsystem. That distinction is recorded here
because the field name invites the wrong reading. Related to Q3-004.

## Not specified, and why

| Item | Reason |
| --- | --- |
| Message rates | Not evidenced |
| Staleness threshold values | Depend on rates; Q-036 for commands |
| Aerora Fact names | Follow from the dialect, which does not exist |
| Enumeration members for any Aerora item | Q-007, Q-023, Q3-003 |
| Units for Aerora items | Follow from the fields |

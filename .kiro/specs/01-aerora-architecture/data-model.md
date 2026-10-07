# Data Model

The entities the Aerora GCS holds, how they are identified, and the one identity decision the rest
of the architecture rests on.

## Vehicle identity

### The decision

**The Aerora vehicle identifier is not the MAVLink system id.** Every Aerora artifact that outlives
a connection keys on the Aerora identifier. The system id is a transport-level addressing detail.

This is R-017, promoted to the `G-ARCH` gate in
[phase-2-risk-gates.md](phase-2-risk-gates.md) because three subsystems depend on it: the cloud
vehicle registry, the audit trail, and configuration distribution.

### Why the system id cannot be the identity

| Property of the MAVLink system id | Consequence |
| --- | --- |
| It is an 8-bit value with 255 usable addresses | A fleet of any size exhausts or reuses it |
| It is set by firmware configuration | Two aircraft can be configured identically, by mistake or by cloning a configuration |
| The ground station's own system id is an operator-editable setting (`gcsMavlinkSystemID`) | Even the GCS end of the address space is not fixed |
| It can change when firmware is reconfigured | A record keyed on it would silently re-attribute to a different aircraft |
| `Vehicle::id()` returns it directly (`Vehicle.h:432`) | It is pervasive in QGroundControl and therefore tempting to reuse |

The last row is the hazard. The system id is right there, it is an integer, it is unique among
connected vehicles, and it works perfectly for an afternoon of testing. It fails at the point where
a record has to mean something a year later.

### The mapping

```text
MAVLink system id   ──  observed per connection  ──→   Aerora vehicle identifier
   (addressing)                                            (identity)
                              ▲
                              │
              reported in the Aerora system status message
```

| Property | Statement |
| --- | --- |
| The binding is observed, never assumed | It comes from vehicle-reported telemetry |
| It is established per connection | A reconnection re-establishes it rather than trusting the last one |
| It is not inferred from the system id | There is no table mapping system ids to identifiers |
| Until it is reported, the vehicle has no Aerora identity | It is addressable and displayable, but not recordable |

That last row has a real consequence worth stating plainly: **records that require an Aerora
identity cannot be written before the identifier is reported.** The architecture handles this by
buffering such records until the identity arrives, and by marking the buffered span rather than
discarding it. A connection that never reports an identity produces a record explicitly attributed
to an unidentified vehicle, which is honest and searchable.

### What the identifier is

`OPEN`. Q-016 asks what the Aerora fleet identifier is sourced from — a serial number, a secure
element identity, a registry-assigned value, or a combination.

The architecture does not depend on the answer, which is why R-017 is gated at `G-ARCH` and still
resolvable: the identifier is an opaque value reported by the vehicle, and every consumer treats it
as opaque. Requirements on it:

| Requirement | Reason |
| --- | --- |
| Stable for the life of the aircraft | Records must remain attributable |
| Unique across the fleet | Two aircraft must not merge in the registry |
| Reportable over MAVLink | The GCS has no other source |
| Not operator-editable | An editable identity is not an identity |
| Independent of the system id | Otherwise it inherits the system id's problems |

The secure element on the flight controller is a plausible source and would satisfy all five. That
is a hardware-team question, not a GCS decision.

## Entities

| Entity | Identified by | Lifetime |
| --- | --- | --- |
| Vehicle | Aerora vehicle identifier | The aircraft's life |
| Operator | Operator identity from the Security module | The person's account |
| Command | GCS-assigned request identity | One command |
| Mission plan | Plan file identity, with the Aerora vehicle identifier it was planned for | Indefinite |
| Payload | Payload identity reported through the payload interface | The payload's life |
| Capability inventory | Scoped to the vehicle identifier and a version | Per firmware version |
| Configuration | Configuration version, scoped to the vehicle identifier | Until superseded |
| Link | Link configuration name, local to the station | Station-local |
| Telemetry record | Session identity, plus the vehicle identifier | Per retention policy |
| Audit record | Record identity, plus operator and vehicle identifiers | Indefinite |

### Two notes on this table

The **link** is the only entity identified by something station-local. That is correct: a link is a
station's route to a vehicle, not a property of the vehicle. The vehicle's own view of its links is
separate telemetry in `aeroraLink`, as described in
[communication-architecture.md](communication-architecture.md). Conflating the two would make a
GCS-local serial port name appear to be a vehicle attribute.

The **command** is identified by a GCS-assigned value rather than anything from the wire, because
MAVLink acknowledgements are correlated by component and command, not by a unique request id. The
reasoning is in [authority-model.md](authority-model.md); the consequence for the data model is
that a command record's identity is generated, not observed.

## The value model

### The unit is a Fact

Every vehicle-sourced value and every GCS setting is a Fact. There is no Aerora value store, no
Aerora parameter cache, and no Aerora settings store.

| Property | Comes from |
| --- | --- |
| Name, type, units | Fact metadata |
| Bounds and enumerations | Fact metadata |
| Translation | Fact metadata |
| QML binding | The Fact system |
| Rate limiting | The Fact system, on the main thread |

Enumerated values are enumerated **in metadata**, not in code. That is what lets a payload state
enumeration be filled in when Q-023 resolves without touching the display, the log schema or the
audit record — the point [payload-architecture.md](payload-architecture.md) stops at.

### Every Aerora value carries three things

| Field | Why |
| --- | --- |
| Value | The datum |
| Receive time | Stamped at parse time; the only moment freshness is knowable |
| Reporting source | Which component reported it |

The triple is carried through memory, display and record without being reduced. Freshness cannot be
reconstructed after the fact, and a value whose source is unknown cannot be assessed — a reading
from the flight controller and the same reading relayed by a companion computer are not equally
trustworthy.

### What the GCS never derives

| Not derived | Instead |
| --- | --- |
| A vehicle state the vehicle has not reported | Shown as unknown |
| A value's freshness from its arrival in a display | Stamped at parse |
| An Aerora identity from a system id | Reported |
| Proposal acceptance from subsequent state | Reported, per [gpu-integration.md](gpu-integration.md) |
| Mission structure from a downloaded flat mission | From the plan file, per [mission-architecture.md](mission-architecture.md) |
| Failsafe clearance from a value going stale | Marked stale, per [authority-model.md](authority-model.md) |

Six prohibitions, all the same prohibition: the GCS does not manufacture state. They are listed
separately because each one is tempting in a different context, and the general rule is easy to
agree with and easy to violate locally.

## References between entities

| From | To | Via |
| --- | --- | --- |
| Audit record | Operator | Operator identity |
| Audit record | Vehicle | Aerora vehicle identifier |
| Audit record | Command | Request identity |
| Command | Vehicle | Aerora vehicle identifier |
| Mission plan | Vehicle | Aerora vehicle identifier |
| Telemetry record | Vehicle | Aerora vehicle identifier |
| Configuration | Vehicle | Aerora vehicle identifier |
| Capability inventory | Vehicle | Aerora vehicle identifier |
| Link | Vehicle | None — the association is observed per connection and not persisted |

Eight of nine references pass through the Aerora vehicle identifier. That concentration is why
R-017 is load-bearing, and why the identifier being opaque is what keeps Q-016 from blocking:
nothing in this table interprets the value.

## Persistence

| Data | Stored where | Protected |
| --- | --- | --- |
| GCS settings | Application settings store | No |
| Signing keys | Application settings store | **No — R-005, see [security-architecture.md](security-architecture.md)** |
| Operator credentials | Local credential store | Required |
| Telemetry records | Telemetry directory | Filesystem only |
| Audit records | Local record store | Integrity-protected |
| Mission plans | Missions directory | Filesystem only |
| Cached configuration | Local configuration cache | Filesystem only |
| Parameter cache | QGroundControl's existing cache | Filesystem only |

Two rows are deliberate flags rather than descriptions. The signing-key row is the architecture's
single known unremediated gap. The audit-record row states a requirement — integrity protection —
that the local record store must provide, because an audit record that can be edited is not
evidence.

Note also that settings and signing keys sharing one store is what creates the key-destruction
coupling in [configuration-architecture.md](configuration-architecture.md). Separating them fixes
two problems at once.

## What is deliberately not modelled

| Not modelled | Reason |
| --- | --- |
| Payload state enumeration members | Q-023; [payload-architecture.md](payload-architecture.md) stops at the shape |
| Failsafe action values | Q-007; [authority-model.md](authority-model.md) provides interfaces only |
| Authority mode value set beyond the baseline sentinels | The safe-mode identity is flagged in the baseline as requiring confirmation |
| Operator role set | Q-029 |
| Aerora complex-item geometry | Q-021 and product definition |
| Per-command corroboration windows | Q-036 |
| The Aerora identifier's internal structure | Q-016; opaque by design, and opaque is also the correct final answer |

The last row differs from the others. The rest are gaps awaiting an answer. Opacity of the
identifier is a decision: even once Q-016 resolves, no consumer in this data model should parse the
identifier, because a parsed identifier becomes a dependency on its format.

## Core edits required

**None.** Every entity above is Aerora data in Aerora modules, or an existing QGroundControl entity
used unchanged.

## Open questions

| Question | Subject | Impact |
| --- | --- | --- |
| Q-016 | Aerora fleet identifier source | `ARCH-SHAPING` |
| Q-023 | Payload state enumeration | `ARCH-SHAPING` |
| Q-029 | Operator role set | `ARCH-SHAPING` |
| Q-007 | Failsafe values and authority mode mapping | `ARCH-SHAPING` |
| Q-036 | Per-command corroboration windows | `DEFERRABLE` |
| Q-037 | Integrity protection mechanism for the local audit record store | `ARCH-SHAPING` |

## Requirements derived

| Identifier | Statement |
| --- | --- |
| `AERORA-VEH-REQ-001` | The Aerora vehicle identifier shall be distinct from the MAVLink system id |
| `AERORA-VEH-REQ-002` | The Aerora vehicle identifier shall be stable for the life of the aircraft, unique across the fleet, reportable over MAVLink, and not operator-editable |
| `AERORA-VEH-REQ-003` | The binding between MAVLink system id and Aerora vehicle identifier shall be established from vehicle-reported telemetry on each connection |
| `AERORA-VEH-REQ-004` | No consumer shall parse the internal structure of the Aerora vehicle identifier |
| `AERORA-VEH-REQ-005` | Records requiring an Aerora identity shall be buffered until the identity is reported, and a connection that reports none shall produce records explicitly attributed to an unidentified vehicle |
| `AERORA-GCS-REQ-026` | Every Aerora value shall be represented as a Fact, with enumerations and bounds supplied by metadata rather than code |
| `AERORA-GCS-REQ-027` | Every Aerora value shall carry its value, receive time and reporting source through memory, display and record without reduction |
| `AERORA-GCS-REQ-028` | The local audit record store shall be integrity-protected |
| `AERORA-GCS-REQ-029` | The GCS shall not derive vehicle state, identity, freshness, proposal acceptance, mission structure or failsafe clearance that the vehicle has not reported |

Full records in [requirements.md](requirements.md).

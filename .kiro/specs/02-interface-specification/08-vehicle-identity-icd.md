# Vehicle Identity Interface Control Document

The separation between MAVLink addressing and Aerora fleet identity, and an abstraction that lets
the authoritative source be chosen later.

## The separation

```text
MAVLink system id                    Aerora fleet identity
─────────────────                    ────────────────────
addressing                           identity
8-bit, 255 usable values             scope not yet defined
operator-configurable at both ends   not operator-editable
reassignable                         stable for the aircraft's life
unique among connected vehicles      unique across the fleet
present from the first heartbeat     reported, when a message exists for it
```

**They are not the same value and neither derives from the other.** The binding between them is
observed per connection.

### Why the system id cannot be the identity

| Property | Evidence | Consequence |
| --- | --- | --- |
| 8-bit, 255 usable addresses | MAVLink standard | A fleet of any size exhausts or reuses it |
| Set by firmware configuration | `GCS-VERIFIED` | Two aircraft can be configured identically, by mistake or by cloning |
| The ground station's own system id is an operator-editable setting | `GCS-VERIFIED` | Even the station end of the address space is not fixed |
| Can change when firmware is reconfigured | `GCS-VERIFIED` | A record keyed on it would silently re-attribute to a different aircraft |
| The vehicle accessor returns it directly, and it is pervasive in QGroundControl | `GCS-VERIFIED` | It is right there, it is an integer, and it works perfectly for an afternoon of testing |

The last row is the hazard. It fails at the point where a record has to mean something a year later.

This is R-017, gated at `G-ARCH`.

## The identity source is OPEN

| Candidate | Evidence | Assessment |
| --- | --- | --- |
| Secure element on the flight controller | `ARTIFACT-OBSERVED` — the part is present in the schematic | Presence says nothing about provisioning, readability, or whether firmware will expose an identity from it |
| Flight-controller serial number | `NOT-EVIDENCED` | No statement anywhere |
| Registry-assigned value, provisioned to the vehicle | `NOT-EVIDENCED` | Would satisfy the properties; requires a provisioning process |
| A composite of several sources | `NOT-EVIDENCED` | Possible; no decision |

**The secure element does not automatically become authoritative.** A part on a schematic is
evidence that the part was selected, not that it holds a readable, provisioned, reportable identity.
Treating it as the answer would be exactly the error Phase 2.1 corrected for payload retention:
hardware presence standing in for a designed behaviour.

This is Q-016 and it **remains open**.

### Required properties, whatever the source

| Property | Reason |
| --- | --- |
| Stable for the life of the aircraft | Records must remain attributable |
| Unique across the fleet | Two aircraft must not merge in the registry |
| Reportable over MAVLink | The ground station has no other source |
| Not operator-editable | An editable identity is not an identity |
| Independent of the system id | Otherwise it inherits every problem above |

The secure element would satisfy all five if it were provisioned and exposed. So would a
registry-assigned value. The architecture does not need to know which.

## The abstraction

This is the part that lets the question stay open without blocking anything.

```text
         ┌─────────────────────────────────────────────┐
         │  identity consumers                         │
         │  logs · audit · plans · registry · config   │
         └───────────────────┬─────────────────────────┘
                             │  opaque value only
                             ▼
         ┌─────────────────────────────────────────────┐
         │  IDENTITY BINDING                           │
         │  system id  ←observed per connection→  id   │
         └───────────────────┬─────────────────────────┘
                             │  reported
                             ▼
         ┌─────────────────────────────────────────────┐
         │  identity source — SELECTABLE, UNDECIDED    │
         │  secure element? serial? registry? composite?│
         └─────────────────────────────────────────────┘
```

| Property of the abstraction | Consequence |
| --- | --- |
| Consumers receive an **opaque** value | No consumer parses it, so no consumer depends on its format |
| The binding is established from telemetry, per connection | A cached binding would survive a hardware swap and silently mis-attribute |
| The source sits below the binding | Changing the source changes one reporting message, not the ground station |
| Opacity remains correct after Q-016 resolves | A parsed identifier becomes a dependency on its format, permanently |

Eight of the nine inter-entity references in the Phase 2 data model pass through this identifier,
and **none interprets it**. That concentration is why R-017 is load-bearing, and the opacity is why
Q-016 does not block.

## Interface contracts

| ID | Interface | Direction | Producer | Consumer | Crossing | Update | Freshness | Authority | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-VID-001` | MAVLink system id | Vehicle to GCS | Flight controller | Transport addressing | An 8-bit address in every message | Every message | Not applicable | Vehicle | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-VID-002` | MAVLink component id | Vehicle to GCS | Any component | Component-scoped handling | A component address | Every message | Not applicable | Component | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-VID-003` | Aerora vehicle identity report | Vehicle to GCS | Flight controller | Identity binding | An opaque identity value | On connection | Required before identity-bearing records are written | Vehicle | `NOT-EVIDENCED` | `OPEN` |
| `IF-VID-004` | Identity binding | GCS internal | Identity binding | All identity consumers | The association of a connection to an identity | Per connection | Re-established on reconnection | GCS | `GCS-DECIDED` | `PROPOSED` |
| `IF-VID-005` | Identity source selection | Vehicle internal | Firmware | Its own identity report | Which source the reported value derives from | Build or provisioning time | Not applicable | Firmware | `NOT-EVIDENCED` | `OPEN` |
| `IF-VID-006` | Unidentified-vehicle attribution | GCS internal | Identity binding | Record writers | An explicit unidentified marker | When no identity is reported | Not applicable | GCS | `GCS-DECIDED` | `PROPOSED` |
| `IF-VID-007` | Registry reconciliation | GCS to backend | Cloud module | Vehicle registry | Identity and observed attributes | Opportunistic | Not applicable | Backend reconciles, does not resolve | `BASELINE-ASSERTED` | `PROPOSED` |

| ID | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- |
| `IF-VID-001` | Collision between two aircraft on one station is a transport-level condition the station surfaces; **it is never treated as the same aircraft** | `AERORA-VEH-REQ-001` | R-017 | — | `INTEGRATION` |
| `IF-VID-002` | A non-autopilot component is not promoted to a vehicle | `AERORA-GPU-REQ-002` | — | — | `INTEGRATION` |
| `IF-VID-003` | **If never reported, the vehicle has no Aerora identity**; it is addressable and displayable but not recordable. The value is opaque: **no consumer parses its internal structure**, so opacity stays correct even after Q-016 resolves | `AERORA-VEH-REQ-002`, `AERORA-VEH-REQ-003`, `AERORA-VEH-REQ-004` | R-017 | Q-016 | `INTEGRATION` |
| `IF-VID-004` | A stale or absent binding blocks identity-bearing records, which are buffered rather than discarded | `AERORA-VEH-REQ-003`, `AERORA-VEH-REQ-005` | R-017 | Q-016 | `INTEGRATION` |
| `IF-VID-005` | Not visible to the ground station; a source change is invisible provided the reported value is stable | `AERORA-VEH-REQ-002` | R-017 | Q-016 | `BENCH` |
| `IF-VID-006` | Records are written with an explicit unidentified attribution, **never discarded and never attributed by guess** | `AERORA-VEH-REQ-005` | R-017 | — | `UNIT` |
| `IF-VID-007` | Unreachable registry defers reconciliation; local records remain complete and authoritative | `AERORA-AWS-REQ-005`, `AERORA-AWS-REQ-009` | R-015 | Q-030, Q-042 | `INTEGRATION` |

## Record-writing contract

The consequence most likely to be missed: **records that require an Aerora identity cannot be
written before the identity is reported.**

| Situation | Behaviour |
| --- | --- |
| Identity reported at connection | Records flow normally |
| Identity not yet reported | Identity-bearing records are **buffered**, and the buffered span is marked |
| Identity never reported for a connection | Records are written with an explicit unidentified-vehicle attribution |
| Identity reported late | Buffered records are written with the identity; the delay is recorded |
| Identity changes within a connection | Treated as a fault condition and surfaced; **not** silently re-attributed |

Buffering rather than discarding matters because the pre-identity span contains the connection and
parameter-load events, which are exactly what an investigation of a connection problem needs.

The last row is a condition no evidence addresses. An identity that changes mid-connection is either
a firmware fault or an addressing collision, and both are worth surfacing rather than absorbing.

## What keys on the identity

| Artifact | Keys on | Why not the system id |
| --- | --- | --- |
| Flight log | Aerora identity | A log outlives the connection |
| Audit record | Aerora identity, plus operator and request identity | Attribution must survive |
| Mission plan | Aerora identity of the vehicle it was planned for | Plans are reused across sessions |
| Cloud vehicle registry | Aerora identity | The registry is the fleet's record |
| Configuration binding | Aerora identity | Configuration is distributed per aircraft |
| Capability inventory | Aerora identity, plus a version | Capabilities change with firmware |
| Compatibility matrix entry | Aerora identity | The matrix spans versions over time |
| Link configuration | **Station-local name** | A link is a station's route to a vehicle, not a vehicle attribute |

The last row is the exception and it is deliberate. Conflating the two would make a station-local
serial port name appear to be a property of the aircraft.

## Conflicts recorded

None between sources. Identity's problem is that no source addresses it at all: the secure element
is observed, and nothing states what the identity is or how it is reported.

That absence is itself worth recording as a conflict of a different kind — between the
architecture's need for an identity and the complete silence of every input document on the
subject. It is captured as Q-016 and as evidence matrix item 22, not as a new question.

## Not specified, and why

| Item | Reason |
| --- | --- |
| The identity source | Q-016; a hardware and firmware decision |
| The identity's format, length or structure | Opaque by design, and opacity stays correct after Q-016 resolves |
| Whether the secure element is provisioned | `UNKNOWN`; presence of the part proves nothing |
| The message carrying the identity | No dialect exists |
| Registry reconciliation conflict policy | Q-042 |
| Fleet scope — per customer, per organisation, global | Q-016 |

# GCS System Interface Control Document

The external and internal boundaries of the Aerora ground control station, and who owns what at
each one.

## System purpose

The Aerora ground control station is the operator's supervisory interface to an Aerora aircraft. It
presents vehicle-reported state, issues operator-initiated commands, records what happened, and
synchronises with a fleet backend when one is reachable.

It is a QGroundControl custom build: all Aerora behaviour is additive, in an overlay directory, and
**zero QGC core edits are required by the current architecture**.

## The five non-authorities

Stated first because every other boundary in this document depends on them.

| The ground station is **not** | Who is | Source |
| --- | --- | --- |
| Flight stabilisation authority | Flight controller | Baseline 13; architectural principle 1 |
| Local failsafe authority | Flight controller, with payload-local and communications layers | Baseline 13 |
| Payload actuator authority | Payload controller | Baselines 10 and 13 |
| Companion and autonomy authority | Flight controller decides; companion proposes | Baseline 48; ADR-011 |
| Electrical power-switching authority | Power-system design and its local protection | Baseline 13 |

Each of these is a **structural** boundary in the architecture, not a policy. The autonomy module
has no outbound command path; the ground station constructs no failsafe action; no payload condition
reaches a flight-control command.

The baseline also records a project non-goal: making the ground station the sole safety mechanism.

## System boundary

```text
                        ┌──────────────────────────┐
     operator  ────────▶│                          │
                        │   AERORA GCS             │
  fleet backend ◀──────▶│   (QGC custom build)     │
     (optional)         │                          │
                        └───────────┬──────────────┘
                                    │ serial link, MAVLink
                                    ▼
                        ┌──────────────────────────┐
                        │   AERORA RADIO (ground)  │
                        └───────────┬──────────────┘
                                    │ RF
                                    ▼
                        ┌──────────────────────────┐
                        │   AERORA RADIO (air)     │
                        └───────────┬──────────────┘
                                    │
                                    ▼
                        ┌──────────────────────────┐
                        │   FLIGHT CONTROLLER      │◀──── safety MCU / watchdog
                        │   (authority)            │
                        └─────┬──────────────┬─────┘
                              │              │
                      companion UART    payload interface
                              │              │
                              ▼              ▼
                        ┌──────────┐   ┌──────────────────┐
                        │ COMPANION│   │ PAYLOAD CONTROLLER│
                        │ (proposes)│   │ (owns payload state)│
                        └──────────┘   └──────────────────┘
```

Two properties of this topology are load-bearing:

| Property | Consequence |
| --- | --- |
| The companion is reached **through** the flight controller | The ground station has no path to it and cannot command it, whatever a later change might want |
| The payload controller is reached **through** the flight controller | The same, and payload-local interlocks stay out of the ground station's reach |

Both are `ARTIFACT-OBSERVED` — companion UART and payload interface appear in the schematic
evidence. The interface *parameters* are not frozen; the *topology* is.

## Actors

| Actor | Role | Authority over the GCS |
| --- | --- | --- |
| Operator | Issues commands, observes state, acknowledges prompts | Full, subject to authorisation |
| Pilot in command | May hold manual authority at the aircraft | None over the GCS; manual authority is structurally dominant at the vehicle |
| Maintainer | Configures links, parameters, settings | Full, subject to authorisation |
| Fleet manager | Manages vehicle records and configuration distribution | Indirect, via the backend |
| Engineer | Reads logs and diagnostics | Read, subject to authorisation |

The operator role set itself is **`OPEN` as Q-029**. The actors above are functional descriptions,
not role definitions, and must not be read as a role model.

## Component inventory

The fifteen Aerora modules are defined in
[component-architecture.md](../01-aerora-architecture/component-architecture.md). Phase 3 does not
restate them. What Phase 3 adds is ownership at each boundary.

## Ownership

### Data ownership

| Data | Owner | Ground station role |
| --- | --- | --- |
| Vehicle state | Flight controller | Display, log |
| Authority mode | Flight controller | Display reported mode; request changes |
| Payload state | Payload controller | Display, log; **no independent model** |
| Companion proposals | Companion | Display as subordinate; no inference of acceptance |
| Power state | Power system, reported by the flight controller | Display, log |
| Vehicle parameters | Vehicle | Read and write via the parameter interface; no shadow copy |
| Mission intent | **Ground station**, in the saved plan file | Authoritative |
| Vehicle mission store | Vehicle | Derived artifact |
| Operator identity | Ground station security module | Authoritative locally |
| Audit record | Ground station | Authoritative; written locally first |
| Fleet records | Backend | Reconciled, not resolved |
| Aerora vehicle identity | Vehicle | Observed per connection; never inferred |

Two rows invert the usual direction and are worth noting. **Mission intent** is the ground
station's, because a mission item has no vendor-extension slot and the vehicle store can be left
indeterminate by a partial upload. The **audit record** is the ground station's, because it records
what the operator did, which only the ground station observes.

### Command ownership

| Command class | Originator | Decided by | Ground station role |
| --- | --- | --- | --- |
| Flight commands | Operator | Flight controller | Send, track, display outcome |
| Authority mode change | Operator | Flight controller | Request only |
| Payload commands | Operator | Payload controller, via flight controller | Send, track, display outcome |
| Mission upload and resume | Operator | Vehicle accepts or rejects | Construct and send |
| Parameter writes | Operator | Vehicle | Send, confirm |
| Autonomy proposals | Companion | Flight controller | **Observe only** |
| Failsafe actions | Vehicle | Vehicle | **Observe only** |
| Power transitions | Power system | Power system | **Observe only** |

### Telemetry ownership

All vehicle telemetry is owned by its reporting component and is untrusted input. The ground station
stamps receive time at parse, records the reporting source, and never reduces the triple of value,
time and source on its way to a display or a record.

## Boundary contracts

The system-level contracts. Detail for each is in the referenced document.

| ID | Boundary | Direction | Crossing | Evidence | Status |
| --- | --- | --- | --- | --- | --- |
| `IF-SYS-001` | Operator to ground station | Bidirectional | Commands, acknowledgements, displayed state, prompts | `GCS-DECIDED` | `PROPOSED` |
| `IF-SYS-002` | Ground station to vehicle | Bidirectional | MAVLink over a serial link | `GCS-VERIFIED` transport; `NOT-EVIDENCED` dialect | `PROPOSED` |
| `IF-SYS-003` | Ground station to fleet backend | Bidirectional, optional | Records, fleet metadata, configuration | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-SYS-004` | Ground station to local storage | Bidirectional | Plans, logs, audit records, settings, caches | `GCS-VERIFIED` | `PROPOSED` |
| `IF-SYS-005` | Ground station to companion | **None — prohibited** | Nothing | `GCS-DECIDED` | `CONFIRMED` |
| `IF-SYS-006` | Ground station to payload actuators | **None — prohibited** | Nothing | `GCS-DECIDED` | `CONFIRMED` |
| `IF-SYS-007` | Ground station to power switching | **None — prohibited** | Nothing | `GCS-DECIDED` | `CONFIRMED` |
| `IF-SYS-008` | Ground station to inbound network | **None — no listener** | Nothing | `GCS-VERIFIED` | `PROPOSED` |

Four of eight boundary contracts specify that **nothing crosses**. Those are the useful ones: a
prohibition stated as an interface is checkable, and three of the four are **assigned** a test
rather than a policy. No such test has been written.

| ID | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- |
| `IF-SYS-001` | Operator input is never assumed effective; every command is tracked through the eight-state model | `AERORA-GCS-REQ-015`, `AERORA-GCS-REQ-016` | — | Q-029 | `INTEGRATION` |
| `IF-SYS-002` | Link loss degrades display to stale; no vehicle-directed action beyond existing link failover | `AERORA-COMM-REQ-006`, `AERORA-COMM-REQ-010` | R-003 | Q-013, Q-015 | `INTEGRATION` |
| `IF-SYS-003` | Unreachable backend degrades synchronisation only; no command path effect | `AERORA-AWS-REQ-005`, `AERORA-AWS-REQ-006` | R-015 | Q-030 | `INTEGRATION` |
| `IF-SYS-004` | Write failure is surfaced; the log writer drops with an accounted counter rather than blocking | `AERORA-GCS-REQ-025` | — | Q-039 | `UNIT` |
| `IF-SYS-005` | Not applicable — the autonomy module has no outbound command path | `AERORA-GPU-REQ-003`, `AERORA-MAV-REQ-007` | R-014 | — | `UNIT` |
| `IF-SYS-006` | Not applicable — no payload condition reaches a flight-control command | `AERORA-PAYLOAD-REQ-009`, `AERORA-PAYLOAD-REQ-010` | — | Q-009 | `STATIC` |
| `IF-SYS-007` | Not applicable — the ground station initiates no power transition | `AERORA-POWER-REQ-008` | — | Q-026 | `STATIC` |
| `IF-SYS-008` | Not applicable — asserted by startup self-check and test | `AERORA-AWS-REQ-014` | R-016 | Q-027 | `UNIT` |

## Cloud boundary

| Property | Position |
| --- | --- |
| Classification | Optional supporting system |
| On the command path | **Never** |
| Required for local authorisation | **Never** |
| Required for local logging | **Never** |
| Degradation | Per feature; there is no global offline mode |

The eight named routes by which a cloud dependency would arrive, and the refusal of each, are in
[cloud-architecture.md](../01-aerora-architecture/cloud-architecture.md). Phase 3 adds the interface
contracts in [15-cloud-integration-icd.md](15-cloud-integration-icd.md).

## Local and offline boundary

The ground station performs its full local function with all network access blocked: connect, load
parameters, plan, upload, execute, and produce a complete local flight record. That is
`AERORA-AWS-REQ-006`, and a single integration test is **assigned** to it — which is what will make
the property enforceable rather than aspirational once the test exists.

## Vehicle boundary

| Property | Position | Evidence |
| --- | --- | --- |
| Transport | Serial, carrying MAVLink | `GCS-VERIFIED` for the ground-station path |
| Radio presentation as a serial device | Required | **`NOT-EVIDENCED`** — see [04-radio-link-icd.md](04-radio-link-icd.md) |
| Protocol | MAVLink 2 with an Aerora dialect | `BASELINE-ASSERTED`; the dialect does not exist |
| Vehicle addressing | One MAVLink system id per aircraft; components distinguished by component id | `GCS-VERIFIED` |
| Flight-controller component | Component 1 | `GCS-VERIFIED` as a QGroundControl assumption; a firmware obligation |

## Companion boundary

The companion shares the aircraft's MAVLink system id and uses a distinct component id. Messages
from it reach the vehicle model, because delivery filters on system id only, and its heartbeat does
not create a second vehicle, because vehicle creation requires the autopilot component id. Both are
`GCS-VERIFIED`.

What that buys: companion observability at no transport cost. What it requires: a firmware
addressing decision that has not been recorded. Detail in
[05-gpu-fc-icd.md](05-gpu-fc-icd.md).

## Payload boundary

| Property | Position |
| --- | --- |
| Payload state authority | Payload controller |
| Ground station model of payload state | None independent |
| Payload actuator commands from the ground station | Operator-initiated only, through the flight controller |
| Payload state retention across a mission interruption | **Preferred architecture, `NOT-EVIDENCED`** |
| Payload emergency stop | Observed as an independent event, never inferred |

## Radio boundary

Serial-first. The ground radio presents a serial device; QGroundControl's existing serial link
carries MAVLink over it; no new transport type and no core edit. The chain, its lifecycle and the
misidentification trap are in [04-radio-link-icd.md](04-radio-link-icd.md).

## Security boundary

| Property | Position |
| --- | --- |
| Inbound network connections | None accepted |
| Operator authentication | Validated against local credentials as the primary path |
| Authorisation gate | Applied before a command reaches a link |
| MAVLink signing | Retained from QGroundControl, unchanged |
| Signing key custody | **Unresolved; release-critical** |
| Advanced-interface toggle | Disclosure only; never an authorisation boundary |

## Internal boundaries

The four dependency rules from
[component-architecture.md](../01-aerora-architecture/component-architecture.md) are the internal
boundary contract: presentation depends on domain only; domain reaches QGroundControl only through
the extension layer; payload and mission are mutually independent; cloud depends on security and
never the reverse.

Phase 3 adds one observation. Of the four, **only the fourth is currently verified by a test** —
the static check that the security module has no reference to the cloud module. The other three are
review disciplines. That asymmetry is recorded as a Phase 3 risk.

## Traceability

Every interface above appears in [19-interface-traceability.md](19-interface-traceability.md) with
its verification method.

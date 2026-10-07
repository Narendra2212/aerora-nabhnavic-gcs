# Flight Controller to Ground Station Interface Control Document

The conceptual contract between the Aerora flight controller and the ground station: what state
crosses the boundary, with what semantics, under whose authority.

## Scope and discipline

This document specifies the contract at the level of **state categories**, not message fields.
That is deliberate and it is a limit, not a style choice: the Aerora dialect does not exist, so
specifying fields would produce a document that reads like a frozen interface and is not one.

| This document specifies | This document does not specify |
| --- | --- |
| Which state categories cross the boundary | Message or command identifiers the architecture has not ratified |
| Direction, producer, consumer, authority | Field layouts of Aerora messages |
| Update, freshness and acknowledgement semantics | Rates, where no rate is evidenced |
| Failure behaviour on the ground-station side | Vehicle-side failsafe behaviour |
| Which standard MAVLink families already carry a category | New enumerations of any kind |

### Where the standard already suffices

Several categories are carried by standard MAVLink families that QGroundControl already parses.
Those rows cite the standard family by name, with evidence `GCS-VERIFIED`, because the parsing was
read in Phase 1 and Phase 2. **No Aerora extension is proposed where the standard is adequate** —
that is the baseline's own instruction.

### Where Aerora extension is needed

Baseline document 07 names nine candidate Aerora message families. Phase 3 cites them **as the
baseline's candidates**, with status `PROPOSED` and evidence `BASELINE-ASSERTED`. Their fields,
identifiers and enumerations are not specified here and must not be inferred from the names.

## Contract

| ID | Category | Direction | Producer | Consumer | Carried by | Units | Range | Update | Freshness | Ack | Authority |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-FC-001` | Vehicle state: attitude, position, velocity, altitude | Vehicle to GCS | Flight controller | GCS vehicle model | Standard MAVLink attitude, global position and VFR families | SI, per MAVLink standard | Per MAVLink standard | Streamed | Stamped at parse; stale past class threshold | None | Vehicle |
| `IF-FC-002` | Flight mode | Vehicle to GCS | Flight controller | GCS, operator display | Standard heartbeat base and custom mode | Enumerated | `UNKNOWN` — Aerora custom modes undefined | Streamed with heartbeat | Stamped at parse | None | Vehicle |
| `IF-FC-003` | Authority mode, effective | Vehicle to GCS | Flight controller | Authority module | Aerora system status family, candidate | Enumerated | Three sentinels evidenced; set not closed | `UNKNOWN` | Stamped at parse; stale is visible | None | Vehicle |
| `IF-FC-004` | Arming state | Vehicle to GCS | Flight controller | GCS, operator display | Standard heartbeat base mode | Boolean | — | Streamed with heartbeat | Stamped at parse | None | Vehicle |
| `IF-FC-005` | System health and sensor status | Vehicle to GCS | Flight controller | GCS, diagnostics | Standard system status family | Bitmask, per standard | Per standard | Streamed | Stamped at parse | None | Vehicle |
| `IF-FC-006` | Battery and power telemetry | Vehicle to GCS | Flight controller, from power monitoring | Power module | Standard battery family; Aerora power reporting for tether state | Volts, amps, percent | Per standard; Aerora range `UNKNOWN` | Streamed | Stamped at parse | None | Vehicle |
| `IF-FC-007` | Link state, vehicle-reported | Vehicle to GCS | Flight controller comms arbitration | LinkState module | Aerora system status family, candidate | Enumerated link id plus validity | Four values evidenced in the model | `UNKNOWN` | Stamped at parse; all-stale is a distinct state | None | Vehicle |
| `IF-FC-008` | GPS state | Vehicle to GCS | Flight controller | GCS vehicle model | Standard GPS families | Per standard | Per standard | Streamed | Stamped at parse | None | Vehicle |
| `IF-FC-009` | Estimator state | Vehicle to GCS | Flight controller | GCS vehicle model | Standard estimator status family | Per standard | Per standard | Streamed | Stamped at parse | None | Vehicle |
| `IF-FC-010` | Autonomy state and proposals | Vehicle to GCS | Flight controller, relaying or summarising companion output | Autonomy module | Aerora autonomy status family, candidate | `UNKNOWN` | `UNKNOWN` | `UNKNOWN` | Stamped at parse; per-class staleness required | None | Vehicle |
| `IF-FC-011` | Companion health and presence | Companion to GCS, via shared system id | Companion | Autonomy module | Aerora autonomy status family, candidate | `UNKNOWN` | `UNKNOWN` | `UNKNOWN` | Stamped at parse; absent and silent distinguished | None | Companion, non-authoritative |
| `IF-FC-012` | Payload state, health, safety state | Vehicle to GCS | Payload controller, via flight controller | Payload module | Aerora payload status family, candidate | `UNKNOWN` | `UNKNOWN` | `UNKNOWN` | Stamped at parse | None | Payload controller |
| `IF-FC-013` | Command acknowledgement | Vehicle to GCS | Flight controller or addressed component | Authority module, command tracker | Standard command acknowledgement family | Result enumeration, per standard | Per standard | Event, in response | Ack timeout governs retry | **This is the ack** | Vehicle |
| `IF-FC-014` | Mission state and current index | Vehicle to GCS | Flight controller | Mission module | Standard mission current and mission item reached families | Index | Zero to item count | Event and streamed | Stamped at parse | None | Vehicle |
| `IF-FC-015` | Parameters | Bidirectional | Vehicle owns values | Parameter manager, Facts | Standard parameter protocol, with file transfer where supported | Per parameter metadata | Per parameter metadata | Request and response; event on change | Cache validity governed by the protocol | Write is acknowledged by value readback | Vehicle |
| `IF-FC-016` | Diagnostics and status text | Vehicle to GCS | Any component | Diagnostics module | Standard status text and event families | Text, severity | Severity per standard | Event | Stamped at parse | None | Reporting component |
| `IF-FC-017` | Capability inventory | Vehicle to GCS | Flight controller | Capability model | Aerora vehicle capabilities family, candidate | `UNKNOWN` | `UNKNOWN` | On connection, and on change | Must be current for the connection | None | Vehicle |
| `IF-FC-018` | Aerora vehicle identity | Vehicle to GCS | Flight controller | Data model, identity binding | Aerora system status family, candidate | Opaque | Not parsed by any consumer | On connection | Required before identity-bearing records are written | None | Vehicle |

### Commands in the other direction

| ID | Category | Direction | Producer | Consumer | Carried by | Ack | Authority |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-FC-019` | Operator flight commands | GCS to vehicle | Operator, via GCS | Flight controller | Standard command protocol | Required; tracked through the eight-state model | Vehicle decides |
| `IF-FC-020` | Authority mode change request | GCS to vehicle | Operator, via GCS | Flight controller | Aerora command, candidate | Required; effective mode must corroborate | Vehicle decides |
| `IF-FC-021` | Payload commands | GCS to vehicle | Operator, via GCS | Payload controller, via flight controller | Aerora commands, candidate | Required; **effectiveness requires a reported payload state change, not an acknowledgement** | Payload controller decides |
| `IF-FC-022` | Mission transfer | GCS to vehicle | GCS mission module | Flight controller | Standard mission protocol | Per protocol transaction | Vehicle accepts or rejects |
| `IF-FC-023` | Parameter write | GCS to vehicle | Operator, via GCS | Flight controller | Standard parameter protocol | Value readback | Vehicle |
| `IF-FC-024` | Ground station heartbeat | GCS to vehicle | GCS | Flight controller | Standard heartbeat | None | — |

## Failure behaviour, evidence and traceability

| ID | Failure behaviour, ground-station side | Evidence | Status | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-FC-001` | Marked stale; last value retained with its age shown, never as current | `GCS-VERIFIED` | `CONFIRMED` | `AERORA-GCS-REQ-027` | — | — | `INTEGRATION` |
| `IF-FC-002` | Mode unknown; dependent guided actions hidden, not disabled | `GCS-VERIFIED` transport; mapping `NOT-EVIDENCED` | `OPEN` | `AERORA-GCS-REQ-011` | R-028 | Q-007 | `INTEGRATION` |
| `IF-FC-003` | Authority mode shown unknown; no requested mode displayed as effective | `MODEL` for the sentinels | `PROPOSED` | `AERORA-SAFE-REQ-006`, `AERORA-SAFE-REQ-007` | R-028 | Q-007 | `INTEGRATION` |
| `IF-FC-004` | Arming state unknown; arming-dependent presentation suppressed | `GCS-VERIFIED` | `CONFIRMED` | — | — | — | `INTEGRATION` |
| `IF-FC-005` | Health unknown; not inferred from other telemetry | `GCS-VERIFIED` | `CONFIRMED` | `AERORA-GCS-REQ-029` | — | — | `INTEGRATION` |
| `IF-FC-006` | Power state stale; no transition inferred, no power action initiated | `ARTIFACT-OBSERVED` hardware; Aerora reporting `NOT-EVIDENCED` | `PROPOSED` | `AERORA-POWER-REQ-007`, `AERORA-POWER-REQ-008` | — | Q-024, Q-025, Q-026 | `BENCH` |
| `IF-FC-007` | Vehicle-reported link state stale; GCS-local observation shown separately and never promoted | `MODEL` | `PROPOSED` | `AERORA-COMM-REQ-007`, `AERORA-COMM-REQ-008`, `AERORA-COMM-REQ-009` | R-003 | Q-013, Q-015 | `BENCH` |
| `IF-FC-008` | Standard QGroundControl handling, unchanged | `GCS-VERIFIED` | `CONFIRMED` | — | — | — | `INTEGRATION` |
| `IF-FC-009` | Standard handling, unchanged | `GCS-VERIFIED` | `CONFIRMED` | — | — | — | `INTEGRATION` |
| `IF-FC-010` | Proposals marked stale then absent; no acceptance inferred from subsequent state | `MODEL`, partially future work | `OPEN` | `AERORA-GPU-REQ-005`, `AERORA-GPU-REQ-006` | R-014 | Q-008 | `HIL` |
| `IF-FC-011` | Companion presence lost; absent distinguished from reporting-nothing | `ARTIFACT-OBSERVED` topology; addressing `NOT-EVIDENCED` | `PROPOSED` | `AERORA-GPU-REQ-001`, `AERORA-GPU-REQ-007` | R-014 | Q-008 | `HIL` |
| `IF-FC-012` | Payload state stale; no independent model, no inference from commands sent | `BASELINE-ASSERTED` ownership; content `PLACEHOLDER` | `OPEN` | `AERORA-PAYLOAD-REQ-006`, `AERORA-PAYLOAD-REQ-008` | R-002 | Q-009, Q-023 | `BENCH` |
| `IF-FC-013` | No ack within the timeout retries up to the limit, then fails; failure is surfaced | `GCS-VERIFIED` | `CONFIRMED` | `AERORA-GCS-REQ-015`, `AERORA-GCS-REQ-018` | R-025 | Q-036 | `INTEGRATION` |
| `IF-FC-014` | Index unknown; completion never inferred; plan file remains authoritative | `GCS-VERIFIED` | `CONFIRMED` | `AERORA-MISSION-REQ-006` | R-002 | — | `INTEGRATION` |
| `IF-FC-015` | Load failure surfaced; no partial parameter set presented as complete | `GCS-VERIFIED` | `CONFIRMED` | `AERORA-GCS-REQ-012`, `AERORA-GCS-REQ-026` | — | — | `INTEGRATION` |
| `IF-FC-016` | Text displayed and logged; never parsed to derive state | `GCS-VERIFIED` | `CONFIRMED` | `AERORA-GCS-REQ-029` | — | — | `INTEGRATION` |
| `IF-FC-017` | Absent inventory means no Aerora capability is presented; the interface degrades to the standard set | `NOT-EVIDENCED` | `OPEN` | `AERORA-MAV-REQ-009`, `AERORA-GCS-REQ-013` | — | — | `BENCH` |
| `IF-FC-018` | Identity-bearing records buffered; a connection reporting none yields records attributed to an unidentified vehicle | `ARTIFACT-OBSERVED` secure element; reporting `NOT-EVIDENCED` | `OPEN` | `AERORA-VEH-REQ-002`, `AERORA-VEH-REQ-003`, `AERORA-VEH-REQ-005` | R-017 | Q-016 | `BENCH` |
| `IF-FC-019` | Tracked through the eight-state model; accepted-not-effective surfaced | `GCS-VERIFIED` | `CONFIRMED` | `AERORA-GCS-REQ-015`, `AERORA-GCS-REQ-016`, `AERORA-GCS-REQ-017` | R-025 | Q-036 | `INTEGRATION` |
| `IF-FC-020` | Request state shown separately from effective mode; never conflated | `MODEL` | `PROPOSED` | `AERORA-SAFE-REQ-006`, `AERORA-COMPLY-REQ-003` | R-028 | Q-007 | `HIL` |
| `IF-FC-021` | Accepted-not-effective surfaced; no payload condition reaches a flight command | `BASELINE-ASSERTED`; command set `PLACEHOLDER` | `OPEN` | `AERORA-PAYLOAD-REQ-010`, `AERORA-PAYLOAD-REQ-011` | R-002 | Q-023 | `BENCH` |
| `IF-FC-022` | Partial upload leaves the vehicle store indeterminate; presented as unverified until compared against the plan file | `GCS-VERIFIED` | `CONFIRMED` | `AERORA-MISSION-REQ-001`, `AERORA-MISSION-REQ-007` | R-002, R-012 | Q-041 | `INTEGRATION` |
| `IF-FC-023` | Write without readback is not treated as applied | `GCS-VERIFIED` | `CONFIRMED` | `AERORA-BUILD-REQ-011` | — | — | `INTEGRATION` |
| `IF-FC-024` | Suppressible by setting; absence has no ground-station consequence | `GCS-VERIFIED` | `CONFIRMED` | — | — | — | `UNIT` |

## Semantics that apply to every row

### Freshness

Every value is stamped with its receive time **at parse**, carries its reporting source, and is
marked stale past a per-class threshold. The triple of value, time and source is never reduced on
its way to a display or a record.

The **threshold values are not specified here.** Per-class staleness thresholds depend on message
rates that are not evidenced, and the corroboration windows for commands are Q-036. What is
specified is that thresholds are per class rather than global, and that stale is visible.

### Acknowledgement and correlation

Standard command acknowledgements are correlated by component identifier and command, not by a
unique request identifier. Two sequential identical commands produce acknowledgements the mechanism
cannot distinguish — `GCS-VERIFIED`, and recorded as R-025.

The ground station therefore assigns its own request identity for the record. Where the Aerora
dialect carries a request identifier, both are recorded. This needs no protocol change, because the
identity is required for the audit record rather than for the wire.

### Authority

For every row above, the **vehicle decides**. The ground station's authority is limited to issuing
operator-initiated requests and to deciding which operator may issue them. No row grants the ground
station authority over vehicle behaviour, and the four prohibited boundaries in
[01-gcs-system-icd.md](01-gcs-system-icd.md) make three of those prohibitions structural rather
than procedural.

### Effectiveness

An acknowledgement means a command arrived and was accepted. It does not mean the result occurred.
A command becomes `EFFECTIVE` only when vehicle state corroborates it:

| Command class | Corroboration |
| --- | --- |
| Authority mode change | A reported effective mode matching the request |
| Payload command | A reported payload state change |
| Mission transfer | A completed protocol transaction, then verification against the plan file |
| Parameter write | Value readback |
| Flight command | The relevant reported vehicle state |

A command that reaches accepted and never reaches effective is surfaced to the operator. That gap
is where a command is agreed to and quietly does nothing, and it is the most valuable thing the
state model detects.

## Conflicts recorded

One conflict between sources was found while writing this document.

| Conflict | Position A | Position B | Authority needed |
| --- | --- | --- | --- |
| Whether the watchdog reports independently of the primary flight-controller MCU | Baseline 50 places the watchdog interface on the secondary MCU, implying an independent reporter | The firmware model's observed paths run watchdog fault into disarm and payload stop within the primary model | Firmware and safety owner, before `AERORA-GCS-REQ-027`'s reporting-source field can be meaningful |

Neither position is adopted. If the watchdog reports through the primary MCU, the source the ground
station records is the relay rather than the originator, and an operator reading the audit trail
would draw the wrong conclusion about what was still alive. Recorded as Q3-004.

## Not specified, and why

| Item | Reason |
| --- | --- |
| Message and command identifiers | No ratified dialect; see [03-mavlink-icd.md](03-mavlink-icd.md) |
| Aerora custom flight-mode values | Q-007 |
| Authority mode value set beyond the three evidenced sentinels | Baseline flags the safe-mode identity as requiring confirmation |
| Payload state, actuator and command enumerations | Q-023; firmware placeholders |
| Message rates | Not evidenced; needed to size the main-thread Fact budget against R-013 |
| Staleness thresholds | Depend on rates |
| Failsafe reporting content | Q-007; see [09-command-authority-icd.md](09-command-authority-icd.md) |

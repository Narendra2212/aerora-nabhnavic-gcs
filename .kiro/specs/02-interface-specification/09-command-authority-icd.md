# Command Authority Interface Control Document

Who may command the aircraft, how a command's progress is represented, and where the failsafe
boundary sits.

## Three distinct concepts

Collapsing these into one word is the most common error in this area. They are kept separate
throughout.

| Concept | Question it answers | Decided by |
| --- | --- | --- |
| Vehicle authority mode | Which source may command the aircraft right now | Flight controller |
| Command state | What happened to one specific command | Ground station, observing |
| Operator authorisation | Which human may use which ground-station function | Ground station, deciding |

The first is the aircraft's. The third is the ground station's. The second is the ground station's
honest account of the first.

## Authority modes

### The evidenced sentinels

Three values appear in the firmware model and are preserved exactly.

| Mode | Sentinel | Evidence | Status |
| --- | --- | --- | --- |
| Manual | **100** | `MODEL` | `CONFIRMED` as observed |
| Automatic coverage | **101** | `MODEL` | `CONFIRMED` as observed |
| Semi-automatic supervised | **102** | `MODEL` | `CONFIRMED` as observed |

### What the model also shows

| Property | Evidence | Status |
| --- | --- | --- |
| Manual is **structurally dominant** when selected | `MODEL` | `CONFIRMED` |
| Semi-automatic supervised requires **explicit pilot confirmation** before following a proposal | `MODEL` | `CONFIRMED` |
| Proposals may be **rejected**, routing to a safe or default mode | `MODEL` | `CONFIRMED` |
| The safe or default mode's identity | **Flagged in the baseline as requiring confirmation** | `OPEN` |
| The authority mode value set is closed at these three | **Not established** | `UNKNOWN` |

The fourth and fifth rows are why this document does not publish an enumeration. Three values are
observed; whether they are the whole set is not stated anywhere, and the safe-mode code is
explicitly flagged as needing confirmation.

### Authority mode is not flight mode

| | Flight mode | Authority mode |
| --- | --- | --- |
| Answers | What is the aircraft doing | Who may command it |
| Carried by | Standard heartbeat base and custom mode | Aerora reporting, candidate |
| Ground-station representation | A Fact | A **separate** Fact |
| Mapping between them | **`UNKNOWN`** — part of Q-007 | |

They are different axes and are kept in different Facts. Whether they map one-to-one is unresolved,
and treating them as one concept would put a conflated value into the display, the log schema and
the audit trail simultaneously. That hazard is R-028.

## The eight-state command model

Mandated by the Phase 2 directive and preserved.

| State | Meaning | Determined by |
| --- | --- | --- |
| `REQUESTED` | An operator or module has asked; nothing is on the wire | Ground station |
| `SENT` | Written to a link | Ground station |
| `RECEIVED` | The vehicle has indicated receipt | Vehicle |
| `ACCEPTED` | The vehicle reports it will act | Vehicle |
| `REJECTED` | The vehicle reports it will not act, with a reason | Vehicle |
| `EFFECTIVE` | Vehicle state corroborates that it took effect | Vehicle, by corroboration |
| `STALE` | No longer current enough to be meaningful | Ground station, by elapsed time |
| `FAILED` | Did not complete and will not be retried | Ground station |

```text
[*] → REQUESTED → SENT → RECEIVED → ACCEPTED → EFFECTIVE → [*]
          │          │        │          │
          │          │        │          └→ STALE → [*]
          │          │        └→ REJECTED → [*]
          │          └→ (retry) → SENT
          │          └→ FAILED → [*]
          └→ FAILED (no link) → [*]
```

| State | Reachability |
| --- | --- |
| `RECEIVED` | Reachable **only** where the Aerora interface reports receipt separately from acceptance. Where it does not, a command moves from `SENT` directly to `ACCEPTED` or `REJECTED`, and the model records that `RECEIVED` was not observable rather than pretending it was |
| `EFFECTIVE` | What Aerora adds over upstream |
| `STALE` | What Aerora adds over upstream |

## Mapping onto the existing mechanism

QGroundControl's command pipeline supplies these observables, all `GCS-VERIFIED`.

| Aerora state | Observable |
| --- | --- |
| `REQUESTED` | Entry queued, zero attempts |
| `SENT` | One or more attempts made |
| `RECEIVED` | Not distinguished by QGroundControl; Aerora-interface specific |
| `ACCEPTED` | Terminal acknowledgement with an accepting result |
| `REJECTED` | Terminal acknowledgement with a rejecting result |
| `EFFECTIVE` | **Not available** — Aerora corroborates against telemetry |
| `STALE` | **Not available** — Aerora elapsed-time rule |
| `FAILED` | No-response or duplicate-command failure |

Mechanism parameters, `GCS-VERIFIED`:

| Parameter | Value |
| --- | --- |
| Maximum retries | 3 |
| Acknowledgement timeout, default | 1200 ms |
| Acknowledgement timeout, high-latency links | 120000 ms |
| In-progress result | Refreshes the timer rather than completing |
| Failure codes | Three: result-only, no response, duplicate |

### The correlation gap

Acknowledgements are matched by the pair of **component id and command**. There is no unique request
identifier.

| Consequence | Assessment |
| --- | --- |
| Two concurrent identical commands to one component | Guarded upstream by refusing duplicates |
| Two **sequential** identical commands | Produce acknowledgements the mechanism cannot distinguish |
| For display | Tolerable |
| For an audit trail | **Not tolerable** — a record stating a command was accepted must be attributable to that command and no other |

The ground station therefore assigns an Aerora request identity for the record, carried alongside
any MAVLink correlation the dialect provides. No protocol change is needed, because the identity is
required for the record rather than for the wire. This is R-025, and the dialect-side option is
`IF-MAV-024`.

## Interface contracts

| ID | Interface | Direction | Producer | Consumer | Crossing | Ack | Authority | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-AUT-001` | Effective authority mode | Vehicle to GCS | Flight controller | Authority module | The mode currently in force | None | Vehicle | `MODEL` | `PROPOSED` |
| `IF-AUT-002` | Authority mode change request | GCS to vehicle | Operator via GCS | Flight controller | A requested mode | Required | Vehicle decides | `NOT-EVIDENCED` | `OPEN` |
| `IF-AUT-003` | Authority source | Vehicle to GCS | Flight controller | Authority module | Which source is currently commanding | None | Vehicle | `MODEL` | `PROPOSED` |
| `IF-AUT-004` | Confirmation pending | Vehicle to GCS | Flight controller | Authority module, operator display | That a proposal awaits pilot confirmation | None | Vehicle | `MODEL` | `PROPOSED` |
| `IF-AUT-005` | Operator confirmation | GCS to vehicle | Operator via GCS | Flight controller | An explicit confirmation act | Required | Vehicle decides | `NOT-EVIDENCED` | `OPEN` |
| `IF-AUT-006` | Command submission | GCS to vehicle | Operator or module via GCS | Addressed component | A command with parameters | Required | Vehicle decides | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-AUT-007` | Command acknowledgement | Vehicle to GCS | Addressed component | Command tracker | Result, with a reason on rejection | This **is** the ack | Vehicle | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-AUT-008` | Command state | GCS internal | Command tracker | Operator display, audit | One of the eight states, with transition times | — | GCS | `GCS-DECIDED` | `PROPOSED` |
| `IF-AUT-009` | Effectiveness corroboration | GCS internal | Command tracker, against telemetry | Command state | Whether reported state confirms the command | — | GCS observes | `GCS-DECIDED` | `PROPOSED` |
| `IF-AUT-010` | Failsafe state | Vehicle to GCS | Vehicle | Authority module | State, reason, source, severity, reported action — **five separate items** | None | Vehicle | `PLACEHOLDER` | `OPEN` |
| `IF-AUT-011` | Operator authorisation decision | GCS internal | Security module | Command path | Whether this operator may issue this command | — | **GCS decides** | `GCS-DECIDED` | `PROPOSED` |
| `IF-AUT-012` | GCS-initiated failsafe action | **None — prohibited** | — | — | Nothing | — | **No authority** | `GCS-DECIDED` | `CONFIRMED` |

| ID | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- |
| `IF-AUT-001` | Mode unknown; **no requested mode displayed as effective** | `AERORA-SAFE-REQ-006` | R-028 | Q-007 | `INTEGRATION` |
| `IF-AUT-002` | Request tracked through the eight states; the old mode remains displayed until a change is reported | `AERORA-SAFE-REQ-006`, `AERORA-COMPLY-REQ-003` | R-028 | Q-007 | `INTEGRATION` |
| `IF-AUT-003` | Source unknown; not inferred from mode | `AERORA-GCS-REQ-029` | — | Q-007 | `INTEGRATION` |
| `IF-AUT-004` | **A pending confirmation the operator cannot see is the failure that makes supervised autonomy unsafe**; absence of this item is surfaced | `AERORA-SAFE-REQ-007` | — | Q-007 | `INTEGRATION` |
| `IF-AUT-005` | A confirmation not acknowledged is not assumed delivered | `AERORA-SAFE-REQ-007` | — | Q-007 | `INTEGRATION` |
| `IF-AUT-006` | Unauthorised commands never reach a link; the gate is before the wire | `AERORA-SEC-REQ-012`, `AERORA-GCS-REQ-019` | — | Q-029 | `UNIT` |
| `IF-AUT-007` | No ack within the timeout retries to the limit, then fails; failure is surfaced | `AERORA-GCS-REQ-015` | R-025 | — | `INTEGRATION` |
| `IF-AUT-008` | Every transition recorded with its time; the record carries the GCS request identity | `AERORA-GCS-REQ-015`, `AERORA-GCS-REQ-018`, `AERORA-COMPLY-REQ-002` | R-025 | — | `UNIT` |
| `IF-AUT-009` | Accepted but not corroborated within the window becomes `STALE` and is **surfaced**, not retried | `AERORA-GCS-REQ-016`, `AERORA-GCS-REQ-017` | — | Q-036 | `UNIT` |
| `IF-AUT-010` | Each of the five items shown separately; **a failsafe reported once and then absent goes stale, not cleared** | `AERORA-SAFE-REQ-008`, `AERORA-SAFE-REQ-009`, `AERORA-SAFE-REQ-010` | R-006 | Q-007 | `INTEGRATION` |
| `IF-AUT-011` | Denial is an audit event with operator identity and reason | `AERORA-SEC-REQ-015` | R-004 | Q-029 | `UNIT` |
| `IF-AUT-012` | Not applicable — no path exists | `AERORA-SAFE-REQ-008` | — | Q-007 | `STATIC` |

## Staleness

| Property | Position | Status |
| --- | --- | --- |
| Every Aerora command class declares a corroboration window | Required | `PROPOSED` |
| A command past its window without corroboration becomes `STALE` | Required | `PROPOSED` |
| `STALE` is visible, not silent | Required | `CONFIRMED` |
| A stale command is retried automatically | **Prohibited** | `CONFIRMED` |
| The window values | **Not specified** | `OPEN` — Q-036 |

Retrying an operator action whose moment has passed would be the ground station deciding to command
the aircraft. It does not do that.

## The failsafe boundary

### Five interfaces, defined by shape only

`IF-AUT-010` carries five separate items. Their **shapes** are specified; their **values** are not.

| Item | Shape | What it is not |
| --- | --- | --- |
| Failsafe state | An enumerated Fact, values from metadata | Not a boolean |
| Failsafe reason | An enumerated Fact identifying the triggering condition | Not a free-text string |
| Failsafe source | Which subsystem raised it | Not assumed to be the flight controller |
| Failsafe severity | An ordered scale | Not conflated with the reason |
| Failsafe action | What the vehicle reports it is doing or will do | **Not what the ground station thinks should happen** |

### What is deliberately not provided

**No failsafe action mappings. No final safety transitions. No thresholds. No precedence rule for
coincident conditions.**

Two reasons, both from the baseline:

| Reason | Source |
| --- | --- |
| Safety is distributed, and the ground station is explicitly excluded from being the safety controller for high-voltage isolation, converter protection, grounding, connector interlock and tether electrical fault | Baseline 13 |
| Final vehicle failsafe behaviour is **intentionally deferred**, and the ground-station specification "shall not assign final RTL, LAND, HOLD, termination, mode-change or other emergency responses" | Baseline 13, in those words |

The firmware model's failsafe paths are substantially placeholders: geofence, velocity and
acceleration proposal-rejection paths are skeletons, as are parameter validation and boot state.
Specifying actions against skeletons would produce a document that looks authoritative and is not.

This is ADR-010 and Q-007. Both stand.

### What the ground station does with a failsafe

| Action | Permitted |
| --- | --- |
| Display state, reason, source, severity, reported action | Yes |
| Alert the operator, scaled by severity | Yes |
| Log the full record with freshness and source | Yes |
| Offer operator actions the vehicle reports as available | Yes |
| Decide a failsafe action | **No** |
| Initiate a failsafe response autonomously | **No** |
| Infer a failsafe condition the vehicle has not flagged | **No** |
| Suppress or de-escalate a reported failsafe | **No** |
| Clear a failsafe state locally | **No** |

The last row covers the tempting case: a failsafe reported once and then not reported again. The
ground station marks the value **stale**. It does not conclude the condition cleared.

### The watchdog path

Baseline 13 records two observed model paths from a watchdog fault: into disarm, and **directly**
into payload emergency stop. Both are `MODEL` evidence via the baseline, `PROPOSED`, and explicitly
"design inputs, not proof of completion". Each is presented independently and none as a consequence
of another.

**The watchdog is not a failsafe interface.** No source relates a watchdog fault to the failsafe
state of `IF-AUT-010`, and baseline 13 defers failsafe behaviour entirely pending the flight
controller contract. The relationship is `NOT-EVIDENCED`, `DEG-07` and `DEG-08` remain separate
degradation conditions, and the ground station relates them in neither direction.

The watchdog's **reporting** path is contested between sources — see Q3-004 and R3-003. The full
relationship audit is in [18-error-degradation-model.md](18-error-degradation-model.md).

## Operator authorisation

The one place in this document where the ground station decides. It decides what the person in front
of it may do; it never decides what the aircraft does.

| Property | Position |
| --- | --- |
| Authorisation gates ground-station functions | Not vehicle behaviour |
| An authorised operator's command is still subject to vehicle authority | Authorisation is necessary, not sufficient |
| An unauthorised operator's command is never sent | The gate is before the wire |
| The advanced-interface toggle is not authorisation | Disclosure only |

### Roles are not hard-coded

**No role definitions appear in this document.** The role set is Q-029 and remains open.

| Specified | Not specified |
| --- | --- |
| That an authorisation gate exists, before the wire | Which roles exist |
| That the gated operation set is declared explicitly, not derived from interface visibility | Which operations each role may perform |
| That denials are audit events | Any default role assignment |
| That authorisation is validated offline as the primary path | Any role hierarchy |

The mechanism is implementable before the role set is populated, which is why Q-029 is
architecture-shaping rather than blocking.

## Conflicts recorded

| Conflict | Position A | Position B | Authority needed |
| --- | --- | --- | --- |
| Whether the authority mode value set is closed at the three observed sentinels | The firmware model shows exactly three | The baseline flags a safe or default mode code as requiring confirmation, implying a fourth value exists | Firmware and safety owner, before any authority enumeration is written |

Both are preserved. This document publishes the three observed sentinels as **observed**, not as a
closed set, and the metadata-driven enumeration in
[11-telemetry-icd.md](11-telemetry-icd.md) accommodates additional values without code change.
Recorded as Q3-003.

## Not specified, and why

| Item | Reason |
| --- | --- |
| The complete authority mode value set | Q3-003 conflict above |
| The safe or default mode identity | Baseline flags it as requiring confirmation |
| Failsafe state, reason, severity or action enumerations | Q-007; baseline 13 forbids the ground station assigning them |
| Failsafe precedence when conditions coincide | Q-007 |
| Corroboration window values | Q-036 |
| Operator roles and permissions | Q-029 |
| The authority mode change command | No dialect |

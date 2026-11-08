# Autonomy Source Path and Companion Topology

The owner's answer to `Q3-005`, recorded against the interfaces it shapes. This document resolves a
topology question and closes no Gate.

## Status

| Item | Status |
| --- | --- |
| This document | `PROPOSED` — owner-answered, unsigned |
| `Q3-005` | **Answered: the summarised reading.** Companion traffic is terminated at the flight controller |
| `DG-AUT:source-interface` | Still `OPEN`. The transport element is settled; the schema element is not |
| Interfaces this answer shapes | `IF-FC-010`, `IF-FC-011`, `IF-GPU-001` through `IF-GPU-005`, `IF-TLM-007`, `IF-UIS-003` |
| New Contract_Values introduced | **None.** No field, identifier, threshold or message name is authored here |
| Cascade through the interface rows | **Applied.** See section 5 for what changed and what deliberately did not |
| Which monotonic clock the ordering key resolves to | **Still open.** Named in section 3 and chosen nowhere |
| Document number | 13, allocated here rather than by Tab 1. [00-README.md](00-README.md) reserves that to Tab 1, so the number needs confirming |

## 1. The answer

The companion computer is carried on the aircraft and runs AI model inference for the painting
workload together with control and navigation intelligence. Its entire communication path is with
the flight controller. The flight controller is the command centre: it receives the companion's
output, decides, and is the party the ground station hears from.

So companion traffic is **terminated at the flight controller, not bridged onto the radio link.**
That is the second of the two readings `Q3-005` holds open.

This is consistent with everything the GPU-to-flight-controller interface control document already
records, and corroborates rather than contradicts it:

| Already recorded | Where | Evidence |
| --- | --- | --- |
| Companion connects to the flight controller, not the ground station | [05-gpu-fc-icd.md](../02-interface-specification/05-gpu-fc-icd.md) § Transport | `ARTIFACT-OBSERVED`, `CONFIRMED` |
| The ground station has no path to the companion | § The authority boundary | `CONFIRMED` |
| `IF-GPU-008`, ground station to companion | § Interface contracts | **None — prohibited**, `GCS-DECIDED`, `CONFIRMED` |
| The companion proposes; the flight controller decides; the ground station observes both | § The authority boundary | `CONFIRMED` |

The workload scope does not disturb the authority boundary. Control and navigation intelligence
arrives as a **proposal** like any other companion output, never as a command, because the companion
is non-authoritative by topology rather than by policy.

## 2. What the answer resolves

`Q3-005` records what turns on it: which of `IF-FC-010` and `IF-GPU-002` exists, and against which
clock companion staleness is measured.

| Consequence | Resolution |
| --- | --- |
| Which interface exists | `IF-FC-010`. The flight controller is the producer the station receives from |
| `IF-GPU-002` as a direct companion-to-station interface | Does not exist. The companion's output is an input to the flight controller |
| What the reporting-source field records | The flight controller, as the sending and summarising party |
| Companion staleness visible to the station | Only the staleness of the flight controller's summary — see section 3 |

### Three firmware obligations dissolve

The interface control document records three obligations under `IF-GPU-001`, all `NOT-EVIDENCED`, and
calls them "the first thing to confirm with the firmware team": that the companion shares the flight
controller's system id, that it uses a non-autopilot component id, and that it never impersonates the
autopilot.

All three exist only under the direct reading, where the companion is a MAVLink component the station
receives from. Under this answer it is not, so there is no system-id filter to satisfy, no phantom
second vehicle to avoid, and no impersonation surface. The zero-cost integration the document
describes — four `GCS-VERIFIED` upstream behaviours letting companion telemetry reach the vehicle
model — is not needed for proposals.

One obligation replaces them, and it is section 3.

## 3. The clock, which is the consequence that matters

`Q3-005` states it directly: `AERORA-GPU-REQ-006` requires every companion-sourced value to carry a
receive time and to be marked stale past its class threshold, and **under the summarised reading that
requirement is satisfied against the wrong clock unless the summary carries the companion's own
timestamp.**

For this product that is not an abstraction. The companion's output is AI inference driving a
painting operation. If the flight controller re-emits a summary stamped at the moment of
re-emission, a perception result computed seconds earlier — or one the companion stopped updating
altogether — reads as current on the station. The station would be marking fresh a proposal whose
basis is stale, which is the one failure the freshness vocabulary exists to prevent.

The accepted `DG-AUT:proposal-model` Closure_Record already anticipates the shape and now depends on
this. It settles the proposal timestamp as two values carried together, `occurredUtcMs` for wall time
and `monotonicUs` as the ordering source of truth, and it chose a monotonic ordering key precisely so
a queue would not reorder itself when the clock is corrected mid-flight. Under the summarised path
the companion's monotonic clock is not the flight controller's and neither is the station's, so:

| Requirement created by this answer | Owner |
| --- | --- |
| The flight controller's summary carries the companion's own timestamp, not only the re-emission time | Firmware |
| The pair the accepted proposal model settles resolves to the companion's clock for `monotonicUs`, or the ordering guarantee applies to the summary rather than to the inference and the record says which | Architecture, with Firmware |

Neither is authored here. Both are named because this answer is what creates them.

## 4. What this does not settle

`DG-AUT:source-interface` has three Decision_Content elements. This answer settles one.

| Element | State after this answer |
| --- | --- |
| The autonomy source transport | **Settled.** Terminated at the flight controller, summarised and re-emitted; the station participates in interface A only, and `IF-GPU-008` prohibits the reverse path |
| The request and response schema | **Not settled.** The request half is settled as empty by `IF-GPU-008`. The response half has no field-level definition: `IF-FC-010` is `MODEL`, partially future work, `OPEN`, and carries no update rate |
| The station's behaviour on a response it cannot evaluate | **Defined, not closed.** The interface control document defines the fallback — the proposal and the effective state shown side by side with no causal link asserted — and ties it to `Q-008`, which remains open |

### The carrier question this sharpens

The schema element is now the whole of what the part waits on, and there is a constraint on it that
two closed Gates already set.

The accepted `DG-MIS` record settles that Aerora introduces **zero** MAVLink command identifiers. The
accepted `DG-TLM` dialect record settles that every Aerora fact arrives as `NAMED_VALUE_INT` or
`NAMED_VALUE_FLOAT`, which are common-dialect and already parsed, so no identifier is allocated and
the Aerora dialect stays `NOT-EVIDENCED`.

But a proposal, per the accepted `DG-AUT:proposal-model` record, carries an opaque source identifier
stored verbatim, two timestamps, a subject named as a telemetry item is named, and a basis carried
verbatim and uninterpreted. `NAMED_VALUE_INT` and `NAMED_VALUE_FLOAT` carry one scalar against a
short name. They can carry none of those four.

So the mechanism that unblocked `DG-PAY` cannot carry a proposal, and the carrier is an open
question rather than a solved one. It is named here and decided nowhere: choosing it is an
Architecture decision with the external interface authority, and `DG-AUT-SOC-01` requires an
**issued** interface control document cited for every schema element before the part can close.

## What signing this would and would not do

It would resolve `Q3-005` for the summarised reading, retire the three `IF-GPU-001` firmware
obligations, and create the one timestamp obligation in section 3. It would settle the transport
element of `DG-AUT:source-interface`.

It would **not** close `DG-AUT`. The schema element stays uncited, and criterion 13 holds the part
`OPEN` and names the uncited element until an issued interface control document covers it. Four
screens stay unreleased: `SCR-AIS-01`, `SCR-AIS-02`, `SCR-AIS-03` and the proposal half of
`SCR-LIV-05`.

It introduces nothing into the overlay. No autonomy-source type, enumerator or accessor is declared,
which is what the `autonomy-source` check holds absent while the Gate is open.

## Status vocabulary

Defined programme-wide in [00-README.md](00-README.md#status-vocabulary). This document redefines
none of them and claims `VERIFIED` for nothing.

## 5. The cascade, as applied

Every row `Q3-005` names as affected, and what the answer did to it. Each changed row reads
`PROPOSED` and not `CONFIRMED` where it depends on this answer, because this document is unsigned.

| Row | Change |
| --- | --- |
| `IF-GPU-001` companion addressing | Recorded as **no path** — nothing reaches the station under a companion component id. Follows the `IF-GPU-008` pattern of retaining the row and recording non-existence |
| `IF-GPU-002` proposal | No direct path; carried by `IF-FC-010`, producer the flight controller. Its failure row now records that staleness is measured on the summary |
| `IF-GPU-004` companion health | No direct path; carried by `IF-FC-011`, producer the flight controller |
| `IF-GPU-005` companion presence | No direct path; carried by `IF-FC-011`, producer the flight controller |
| `IF-FC-010` autonomy state and proposals | Producer hedge "relaying or summarising" resolved to **summarising**; freshness cell records that the summary carries the companion's own timestamp |
| `IF-FC-011` companion health and presence | Direction "Companion to GCS, via shared system id" becomes **Vehicle to GCS**; producer becomes the flight controller; the `NOT-EVIDENCED` addressing clause is gone because no companion addressing is required. `AERORA-GPU-REQ-001` dropped from its requirement list |
| `IF-TLM-007` autonomy telemetry | Producer "Companion" becomes the flight controller summarising; direction already read "Companion via flight controller" and was already correct |
| `IF-UIS-003` proposed state presentation | Authority records companion content **as reported by the flight controller and never verified by the station as originating there**, which is the same principle `Q3-004` settled for the watchdog: the reporting source is the sender, never presented as the originator |
| `IF-GPU-003`, `IF-GPU-006`, `IF-GPU-007`, `IF-GPU-008` | Unchanged. All four were already flight-controller-sourced or already prohibited |

### Not changed, and why

| Left alone | Reason |
| --- | --- |
| `AERORA-GPU-REQ-001` and `AERORA-GPU-REQ-002` | Both are `PROPOSED` requirements in the ratified architecture register, and both lose their subject under this answer: the first requires the companion to share the flight controller's system id, the second forbids it the autopilot component id, and the companion now addresses the station with nothing. Retiring a ratified requirement is the requirement owner's act, not this document's. `IF-GPU-001`'s failure row records that they have no subject and names the owner |
| The interface and question counts in [19-interface-traceability.md](../02-interface-specification/19-interface-traceability.md) | No row was added or removed, so the counts hold. `Q3-005` is now cited in Evidence columns rather than Questions columns, and that document records the distinction so the unchanged count does not read as an error |
| `IF-UIS-004` staleness presentation | It presents a stale marking and an age, and that is unchanged. What changed is *whose* age, which is the clock obligation in section 3 and belongs to the producer rather than to the presentation |

## 6. What is still open after this

| Open | Owner | Why it is still open |
| --- | --- | --- |
| Which monotonic clock `monotonicUs` resolves to | Architecture, with Firmware | Three candidates exist under the summarised path and the accepted `DG-AUT:proposal-model` record names none. This is the one question this answer created and did not settle |
| The proposal schema: fields, update rate, and the message that carries it | Architecture, with the external interface authority | `IF-FC-010` carries `UNKNOWN` for crossing, update and freshness. This is the whole of what `DG-AUT:source-interface` now waits on |
| The MAVLink carrier for a proposal | Architecture, with the external interface authority | Section 4. The `NAMED_VALUE_*` mechanism that unblocked `DG-PAY` cannot carry a proposal's four fields, and both closed Gates record zero new Aerora identifiers |
| `Q-008`: whether the flight controller reports proposal outcomes explicitly | Firmware | Unaffected by this answer. The interface control document's fallback stands: proposal and effective state side by side, no causal link asserted |
| Retiring `AERORA-GPU-REQ-001` and `AERORA-GPU-REQ-002` | Requirement owner | Recorded above |

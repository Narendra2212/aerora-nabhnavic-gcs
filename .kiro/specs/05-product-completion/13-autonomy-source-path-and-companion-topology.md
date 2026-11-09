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
| Which monotonic clock the ordering key resolves to | **Decided: the companion's**, with the flight controller's relay stamp carried alongside it. Section 3.1 |
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

### What "terminates at the flight controller" is scoped to

It is scoped to **MAVLink, and to the proposal traffic this question is about.** Stated because the
phrase is easy to over-read into "no companion data reaches the station", which is not what `Q3-005`
settles and not what the owner describes.

| Companion output | Path to the station | Status |
| --- | --- | --- |
| Proposals, health, presence — MAVLink on the UART | Terminates at the flight controller, which summarises and re-emits | `Q3-005`, answered |
| **Video from the perception cameras** | A separate companion-to-station stream on its own transport, not MAVLink and not a flight-control path | Owner-stated. Recorded here, not settled |

The video path does not disturb what is already recorded, and the distinction is worth being exact
about. `IF-GPU-008` prohibits **station to companion**, and video flows the other way, so it is
unviolated. The authority boundary row reads "the ground station has no path **to** the companion",
which still holds — no command, no configuration, no model deployment, and the ICD already declares
companion software update and model deployment out of scope.

What changes is only that the companion is a source of data the station receives directly, while
remaining not a MAVLink peer of the station and not a party the station can address. A perception
overlay needs a frame and a proposal, and they arrive by different routes: the frame directly, the
proposal through the flight controller. The ICD already anticipated exactly that overlap when it
declared video out of its own scope.

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

## 3.1 The clock, decided

`monotonicUs` is **the companion's** production clock, and the flight controller's summary carries
**both** it and the flight controller's own relay stamp.

### Why not either of the other two

| Candidate | Why it fails |
| --- | --- |
| The **station's** clock, stamped at parse | It stamps arrival, not production. Two proposals computed five seconds apart and relayed in one burst get near-identical stamps, so `AERORA-GPU-REQ-006` is satisfied against a clock on which **nothing is ever stale** — arrival is always now |
| The **flight controller's** clock | Better, but it stamps relay rather than production. If the companion stops producing and the flight controller keeps re-emitting its last value, the stamp advances while the content ages: the same failure with a shorter fuse |
| The **companion's** clock | The only one that answers what the field exists to answer — which proposal came first, and how old is this one |

### Why both stamps rather than only the companion's

A companion monotonic value alone is useless to the station, because it shares no timebase with
anything the station holds and cannot be compared against station or flight-controller time.
Carrying both is what makes the pair usable:

| The station can | Because |
| --- | --- |
| Order the proposal queue stably | The companion's `monotonicUs` is the production order, and it does not reorder when a wall clock is corrected mid-flight, which is the reason the accepted record chose a monotonic key |
| Report a true end-to-end age | Production to relay from the two carried stamps, plus relay to parse from its own stamp at the parse point, which `IF-TLM-010` already applies to every message |
| **Detect a stalled companion** | A relay stamp that advances while the companion's `monotonicUs` stays frozen is a companion that has stopped producing. With one stamp that state is invisible; with both it is arithmetic |

The last row is why this is a decision and not a preference. The obligation above is that a summary
stamped at re-emission is indistinguishable from a fresh one. Carrying both stamps is what makes it
distinguishable, and it turns the obligation from something the station must trust firmware to
honour into something the station can check.

### What this does not make the station do

Subtracting two reported timestamps is observation, not projection. The station already stamps every
message at the parse point under `IF-TLM-010`, and already evaluates and presents staleness under
`IF-UIS-004`, whose own text records that a stale value is marked and aged rather than hidden or
shown plain. Nothing here computes a value the aircraft did not report, which is the line
`kRemainingEnergyEstimationImplemented` and `P7-ARCH-005` both draw elsewhere.

No numeric budget is set. How old is too old stays a freshness threshold, and `DG-FRS` holds those.

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
| `AERORA-GPU-REQ-001` and `AERORA-GPU-REQ-002` | Both are active `PROPOSED` requirements in the ratified architecture register, and the answer in section 7 is that they are **re-scoped rather than withdrawn**: the companion speaks MAVLink, so both statements keep a subject on the internal link. Only their ground-station rationales are void. Changing a ratified rationale is the requirement owner's act |
| The interface and question counts in [19-interface-traceability.md](../02-interface-specification/19-interface-traceability.md) | No row was added or removed, so the counts hold. `Q3-005` is now cited in Evidence columns rather than Questions columns, and that document records the distinction so the unchanged count does not read as an error |
| `IF-UIS-004` staleness presentation | It presents a stale marking and an age, and that is unchanged. What changed is *whose* age, which is the clock obligation in section 3 and belongs to the producer rather than to the presentation |

## 6. What is still open after this

| Open | Owner | Why it is still open |
| --- | --- | --- |
| That the flight controller's summary carries **two** stamps, the companion's production clock and its own relay clock | Firmware | Section 3.1 decides which clock the ordering key is; carrying both is what makes a stalled companion detectable, and only firmware can emit them |
| The proposal schema: fields, update rate, and the message that carries it | Architecture, with the external interface authority | `IF-FC-010` carries `UNKNOWN` for crossing, update and freshness. This is the whole of what `DG-AUT:source-interface` now waits on |
| The MAVLink carrier for a proposal | Architecture, with the external interface authority | Section 4. The `NAMED_VALUE_*` mechanism that unblocked `DG-PAY` cannot carry a proposal's four fields, and both closed Gates record zero new Aerora identifiers |
| `Q-008`: whether the flight controller reports proposal outcomes explicitly | Firmware | Unaffected by this answer. The interface control document's fallback stands: proposal and effective state side by side, no causal link asserted |
| Whether `AERORA-GPU-REQ-001` and `AERORA-GPU-REQ-002` are withdrawn or re-scoped | Requirement owner, with Firmware | Section 7. The answer turns on whether the companion speaks MAVLink to the flight controller, which this answer did not cover |

## 7. Answered: re-scope, not withdraw

**The companion speaks MAVLink to the flight controller.** Owner-stated, and it decides the question
this section was written to hold open. The analysis below is kept because it is the reasoning the
answer resolves, and because the alternative it rejected would have been irreversible.

| Consequence | |
| --- | --- |
| `AERORA-GPU-REQ-001` and `AERORA-GPU-REQ-002` | **Re-scoped, not withdrawn.** The companion has a MAVLink identity, so both statements have a subject. What is void is their *rationales*, which cite the ground station's message-delivery filter and its vehicle list — and `Q3-005` settled that companion traffic never reaches either |
| Their re-scoped subject | The flight controller's own routing. A system id and component id are what let the flight controller's MAVLink subsystem distinguish companion traffic from the ground station's |
| Evidence matrix item 9 | **Vindicated as written.** Its `ARTIFACT-OBSERVED` row — "the model receives autonomy commands through a MAVLink subsystem" — reads correctly on the plain reading. An earlier draft of this document reconciled that row by supposing the commands came from the ground station; that reconciliation was wrong and the row was right |
| `Q3-005`'s resolution | **Unaffected.** MAVLink on the internal link does not mean bridged to the radio. The owner states the flight controller is the command centre and the station hears only it, so companion traffic still terminates rather than bridging, and Position A stands |

### Why re-scoping leaves them worth keeping

Not merely a formality. Under the summarised topology both requirements become **defence in depth
against an accidental bridge**, which is the one failure that would break `Q3-005`'s resolution.

If the companion shares the flight controller's system id and the flight controller ever bridges a
companion message onto the radio link — by deliberate change or by a routing mistake — that message
reaches the ground station. `AERORA-GPU-REQ-002` is then what stops it being mistaken for the
autopilot and creating a phantom second vehicle. The requirement protects against the mistake that
the architecture is relying on nobody making.

So a withdrawal would have retired, permanently and unreusably, the two requirements that guard the
assumption the autonomy topology rests on.

### What this does not change

The three `IF-GPU-001` obligations stay retired **as ground-station-facing interface obligations**,
because the station receives nothing from the companion and no companion message is exposed to its
filters. They survive as constraints on the internal link, which is a different document's concern:
the flight-controller-to-companion interface, which baseline 07 requires to be specified separately
from the ground-station-to-flight-controller one even though they share a dialect.

## 8. The analysis this answer resolved

The two companion-addressing requirements look like the clean casualty of this answer. They are not,
and recording why is the point of this section, because the register has a legal withdrawal path and
taking it wrongly retires an identifier that is never reused.

| | `AERORA-GPU-REQ-001` | `AERORA-GPU-REQ-002` |
| --- | --- | --- |
| Statement | The companion shares the flight controller's MAVLink system id and uses a distinct component id | The companion does not emit messages using the autopilot component id |
| Recorded rationale | The ground station's message delivery filters on system id only | An autopilot component id would create a phantom second vehicle in the ground station |
| Does the rationale survive this answer? | **No.** No companion message reaches that filter | **No.** No companion heartbeat reaches vehicle creation |
| Does the statement survive? | **Possibly.** It constrains the companion's MAVLink identity, not the ground station's view of it | **Possibly**, on the same ground |

The reason the statements may survive is in this document's own transport diagram: the flight
controller's "MAVLink subsystem receives autonomy commands", evidence `MODEL`. If the companion
speaks MAVLink to the flight controller over the UART, then its system id and component id still
matter — to the flight controller's routing rather than to the station's filters. This answer settled
where companion traffic *terminates*; it did not say whether the companion speaks MAVLink on the
link that terminates there.

So there are two defensible actions and they are not interchangeable:

| Action | When it is right | Cost of getting it wrong |
| --- | --- | --- |
| **Withdraw**, status `WITHDRAWN` and ratification class `REJECT/WITHDRAW` | The companion does not speak MAVLink to the flight controller at all | A real constraint on the internal link is retired, and the identifier cannot be reused to restate it |
| **Re-scope**, keeping the identifiers and replacing the ground-station rationale with the flight-controller one | The companion speaks MAVLink over the UART | Two requirements keep a rationale that names the wrong consumer, which is the weaker failure and the reversible one |

`AERORA-GPU-REQ-002` has a second reason to be careful with it: `IF-VID-002` cites it for the rule
that a non-autopilot component is not promoted to a vehicle, and that rule is `GCS-VERIFIED` upstream
behaviour which holds whatever the companion does. That citation stands on its own and does not
depend on this answer.

Nothing is withdrawn here. The question needs one fact — does the companion emit MAVLink to the
flight controller — and that is Firmware's to state.

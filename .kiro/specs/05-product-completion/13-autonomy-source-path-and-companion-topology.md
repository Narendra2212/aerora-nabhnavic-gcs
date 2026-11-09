# Autonomy Source Path and Companion Topology

The owner's answer to `Q3-005`, recorded against the interfaces it shapes. This document resolves a
topology question and closes no Gate.

## Status

| Item | Status |
| --- | --- |
| This document | `PROPOSED` — owner-answered, unsigned |
| `Q3-005` | **Answered: the summarised reading.** Companion traffic is terminated at the flight controller |
| `DG-AUT:source-interface` | Still `OPEN`. The transport element is settled; the schema element is not, and section 14 finds it needs firmware work before it can be specified at all |
| Interfaces this answer shapes | `IF-FC-010`, `IF-FC-011`, `IF-GPU-001` through `IF-GPU-005`, `IF-TLM-007`, `IF-UIS-003` |
| New Contract_Values introduced | **None.** No field, identifier, threshold or message name is authored here |
| Owner-relayed firmware-model reading | Recorded in section 14, at a provenance class **below `MODEL`**. No row of it is citable for a Gate or for an interface status |
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
| The proposal schema: fields, update rate, and the message that carries it | **Firmware first**, then Architecture with the external interface authority | `IF-FC-010` carries `UNKNOWN` for crossing, update and freshness. This is the whole of what `DG-AUT:source-interface` now waits on, and section 14 records a relayed message set that carries no proposal at all, so there is nothing yet for a document to specify |
| The MAVLink carrier for a proposal | Architecture, with the external interface authority | Section 4. The `NAMED_VALUE_*` mechanism that unblocked `DG-PAY` cannot carry a proposal's four fields, and both closed Gates record zero new Aerora identifiers. Section 14 leaves the section 9 recommendation standing and unconfirmed rather than superseded |
| Whether ground-station MAVLink traverses the companion computer | Firmware | Section 14. A relayed sentence describes the companion forwarding the flight controller's telemetry to the station, which neither `IF-GPU-008`'s topology basis nor document 14's link axes record |
| Which axis each of the two mode encodings belongs to | Firmware | Section 14. The command-authority document keeps flight mode and authority mode apart and records the mapping between them as `UNKNOWN`; `R-028` is the hazard of collapsing them |
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

## 9. The setpoint stream and the proposal are two different objects

`PROPOSED` analysis, unsigned, and it closes nothing. Written because asking which message carries
the companion's instruction turned out to be the wrong question, and the right one is narrower.

### The accepted proposal model does not fit a setpoint message

The companion sends where the aircraft should be and when. In the common dialect that is the
`SET_POSITION_TARGET_*` family, and `SET_POSITION_TARGET_LOCAL_NED` is **already in this tree** —
`src/FirmwarePlugin/APM/APMFirmwarePlugin.cc:878` builds one for guided motion, with
`coordinate_frame`, a `type_mask` selecting which fields are active, and target system and component.
Zero new identifiers, already in the build.

Now compare it against what the accepted `DG-AUT:proposal-model` record settles a proposal as
carrying:

| Proposal field, accepted | Present in a setpoint message |
| --- | --- |
| `source`, an opaque identifier of the autonomy source, stored verbatim | **No** |
| `timestamp`, as `occurredUtcMs` **and** `monotonicUs` | Partially — a boot-time field, not the pair |
| `subject`, named the way a telemetry item is named | **No** — a setpoint carries coordinates, not a subject |
| `basis`, the evidence the source offers, carried verbatim | **No** |

Nothing is wrong with either object. They are **different objects**, and the question conflated them:

| | The setpoint stream | The proposal |
| --- | --- | --- |
| What it is | Control. Go to this position at this time | A decision artifact an operator reviews |
| Rate | Continuous, high | One per decision, low |
| Lifecycle | None. The next one supersedes it | Absent, Proposed, Approved, Expired — the accepted record |
| Carries a basis | No | Yes, and `SCR-AIS-01` displays it |
| Interface | **B**, companion to flight controller, which the station does not participate in | `IF-FC-010`, flight controller to station |

### Why that matters for the Gate

`DG-AUT:source-interface` is a ground-station Gate part. Its schema element is about what reaches
the **station**, which under `Q3-005` is the flight controller's summary — so it is about the
**proposal**, not the setpoint stream. The setpoint stream is interface B, which baseline 07 requires
to be specified separately and in which the station is recorded as not participating at all.

Read that way the mode behaviour the owner describes falls out without anything new:

| Mode | What flows | What the station needs |
| --- | --- | --- |
| Autonomous, 101 | Setpoints, companion to flight controller | Autonomy state to display. **No proposal to approve** — there is no decision pending |
| Hybrid, 102 | The same setpoints, but the firmware model records that 102 "requires explicit pilot confirmation before following a proposal" | A **proposal**, with its basis, and an approval surface |
| Manual, 100 | Operator command | The same autonomy signals, presented as guidance rather than as a pending decision |

So the proposal object earns its four fields exactly where the operator has something to decide,
which is mode 102, and the setpoint stream needs none of them because nobody approves a setpoint
individually.

### What `DG-AUT:source-interface` still needs, narrowed

| Needed | State |
| --- | --- |
| The autonomy source transport | **Settled.** Section 1 |
| The request half of the schema | **Settled as empty.** `IF-GPU-008` |
| The response half: the message and fields by which the flight controller reports a **proposal** to the station, and the autonomy state in modes where no proposal is pending | **Open.** This is the whole of it |
| An issued interface control document to cite for that element | **Open**, and `DG-AUT-SOC-01` requires it |

Two candidate shapes for the response half, both built on the common dialect and neither adding a
message to it. Named as candidates for Firmware and the external interface authority to confirm or
reject, and not decided by this document, because `P7-AI-004` records that no endpoint, schema or
response may be invented:

| Candidate | Fits | Does not fit |
| --- | --- | --- |
| `TUNNEL` (385), with a vendor-defined payload type | Carries an opaque byte payload up to 128 bytes, so a `basis` of bounded length survives and one message holds the whole proposal atomically | The station parses a payload the dialect does not describe, so the schema lives in the interface control document rather than in the message definition, and its `payload_type` must be chosen — see below |
| A set of `NAMED_VALUE_*` keys plus `STATUSTEXT` for the basis | Reuses the mechanism `DG-TLM` already settled, and the ten-byte key discipline is already recorded | A proposal is split across several messages with no atomicity, so a half-arrived proposal is representable. `STATUSTEXT` truncates a basis |

### The owner's recommended carrier, and what it does not close

The product owner has stated a preference for `TUNNEL` and, with full authority, delegated the
engineering call. It is recorded here as an **input to the interface control document, not as a
closure** of `DG-AUT:source-interface`: the carrier and its schema are the external interface
authority's to issue, and `DG-AUT-SOC-01` is met only by citing that issued document, which does not
yet exist.

Section 14 examines a relayed firmware reading that arrived with the conclusion that it supersedes
this recommendation. It does not. The reading describes interface B and this recommends a carrier on
interface A, so the recommendation is left standing and unconfirmed.

The recommendation rests on one point. `TUNNEL`'s only disadvantage against the split-message
candidate is that its schema sits in the interface control document rather than in a message
definition — and that is not a cost here, because `DG-AUT-SOC-01` requires an issued interface
control document whichever carrier is chosen. The split-message candidate pays that same
documentation cost and additionally makes a half-arrived proposal representable, which section 12
shows is the dangerous direction for a proposal that carries its own class. So `TUNNEL` is better on
the axis that matters and no worse on the one that looked like a cost.

One correction to the framing above. Calling both candidates free of any new identifier is too
strong for `TUNNEL`. It adds no *message* to the common dialect, but its `payload_type` is a value
that must be chosen, and the choice carries a consequence
([MAVLink TUNNEL, message 385](https://mavlink.io/en/messages/common.html#TUNNEL)): a code below
32768 is a *registered* type that belongs in the common `MAV_TUNNEL_PAYLOAD_TYPE` enum — a new
common-dialect identifier, which the zero-new-identifier position of `DG-MIS` and `DG-TLM` does not
permit — while a code above 32767 is reserved by the specification for local experiments and is
explicitly not to be checked into a widely distributed codebase. `TUNNEL` therefore stays free of a
new common-dialect identifier **only** by using a local-experiment `payload_type`, which then lives
in the issued interface control document beside the payload schema. The specification reserving a
range for exactly this use is a point in `TUNNEL`'s favour; it is written down so the issued document
chooses a local-experiment code deliberately rather than minting a registered one by habit.

Two questions the carrier choice leaves for that issued document. The **basis size**: the 128-byte
payload bounds a `basis` to roughly that, and whether a longer one is needed — which would force
fragmentation across several `TUNNEL` messages and reintroduce the atomicity problem the
single-message fit was chosen to avoid — is unanswered. And the **payload schema** itself, the field
layout the station parses, which is the whole of what `DG-AUT-SOC-01` waits on.

## 10. Approval delay: why timeout must not approve, and what that requires instead

The owner raises the case that matters: if a human is slow to approve or deny, something must act
rather than let a delay become a crash. The concern is correct and the answer is not a timeout.

### What the accepted record already forbids, by name

`DG-AUT:proposal-model` is accepted and signed, and its expiry rule says this:

> "Expiry never approves. An expired proposal transitions only to Absent, so no path runs from
> Expired to Approved and the rule cannot become a timeout that acts by itself. `P7-AI-002` requires
> an explicit operator approval event, and **a proposal that approved itself by ageing would be the
> hardest version of that violation to notice**."

The accepted transition map carries no `Expired → Approved` edge. So auto-approval on delay is
excluded, and the reason is sound: a plan nobody reviewed would execute, and the audit trail would
record an approval that never happened.

### Why the concern is still satisfied

Because an avoidance action **is not a proposal**. Conflating the two is what makes the delay look
dangerous.

| | Proposal | Flight-controller safety function |
| --- | --- | --- |
| What it is | A discretionary suggestion about mission execution — paint this pass next, approach from here | Obstacle avoidance, geofence, plausibility, failsafe |
| Who decides | The operator, explicitly, under `P7-AI-002` | The flight controller, immediately |
| Does it wait for approval | Yes, and expires unapproved | **No. It is never routed through approval at all** |
| Already recorded | The accepted lifecycle and expiry rule | "Proposals may be **rejected**, routing to a safe or default mode", `MODEL`, `CONFIRMED`; and baseline 46 records geofence, velocity and acceleration rejection skeletons and GPS and flow plausibility checking in the model |

The flight controller protecting the aircraft is not the station deciding a failsafe. The
command-authority ICD is explicit about which of those is prohibited, and it prohibits the station's
half: decide a failsafe action, **No**; initiate a failsafe response autonomously, **No**; infer a
failsafe condition the vehicle has not flagged, **No**. None of those constrains the flight
controller, which is the party that must act inside human reaction time and the only one that can.

### The rule this gives

**A pending proposal must never be the thing standing between the aircraft and safety.**

The aircraft is always already executing a safe behaviour. A proposal is an *improvement* awaiting
approval, not a *rescue* awaiting approval. Operator delay is then benign by construction: the
proposal ages, expires, goes Absent, and the aircraft carries on doing the safe thing it was already
doing.

The corollary is the test to apply to every autonomy output: **if it must take effect within human
reaction time, it cannot be a proposal.** It has to be an input to a flight-controller safety
function instead, and it must not appear on an approval surface at all — because putting it there
would be exactly the delay the owner is right to worry about.

### The gap this exposes, and it is safety-critical

The partition above has to be written down per autonomy output, and it is not.

`IF-GPU-002` already records that a proposal carries a **class**, and `IF-GPU-007` carries "which
autonomy modes and **proposal classes** exist" at `NOT-EVIDENCED` and `OPEN`. That class set is
where the partition belongs: for each class, whether it is **approvable** — routed through the
lifecycle, able to expire unapproved — or **immediate** — an input to a flight-controller safety
function, never approved and never shown as pending.

Getting it wrong is severe in both directions:

| Misclassification | Consequence |
| --- | --- |
| An immediate output classed as approvable | A collision-avoidance manoeuvre waits for a human, which is the owner's concern made real |
| A discretionary plan classed as immediate | An unreviewed AI plan executes with no operator decision, which is what `P7-AI-002` exists to prevent |

This has to be settled **before** the approval surface is built, not after, because the surface is
what makes the first failure reachable.

### One premise worth naming, because it is also open

The no-auto-approve rule is safe only if the behaviour the aircraft falls back to is safe. The
command-authority ICD records "Proposals may be rejected, routing to a safe or default mode" as
`CONFIRMED` — and on the next row, **"The safe or default mode's identity | Flagged in the baseline
as requiring confirmation | `OPEN`"**.

So the fallback exists and what it *is* does not. That is load-bearing here rather than incidental:
expiry routes a proposal to Absent and the aircraft continues under the mode in force, and if that
mode is ever one that holds the aircraft in an unsafe state awaiting instruction, the accepted
expiry rule stops being conservative. Settling the safe-mode identity is what makes the rule sound,
and it is Firmware's with the baseline owner.

Section 14 records a relayed answer for that mode's identity and why the row stays `OPEN` anyway.
The short version is that the mode offered is a pilot-on-the-sticks one, which is the case this
paragraph names rather than an exception to it.

Nothing in this section changes the accepted record. The expiry rule and the transition map stand as
signed.

## 11. Paint failure breaks the previous section's premise, and fixes it

The owner adds paint failure to what approval delay must not cause. That is not a safety matter, and
adding it exposes an error in section 10.

### What section 10 got wrong

Section 10 concluded that operator delay is "benign by construction: the proposal ages, expires,
goes Absent, and the aircraft carries on doing the safe thing it was already doing."

That reasons on the safety axis only, and it is **false on the quality axis**. Hovering is safe and
ruins the coating. A sprayed surface has a window in which the adjacent pass must be applied while
the previous edge is still wet; hold position past it and the result is a visible lap mark. So a
pause is not benign merely because it is safe, and "carry on hovering" is not a neutral fallback for
a painting aircraft.

### The resolution: approve the plan, not the step

A proposal in hybrid mode is a **deviation request**, not a step instruction. With that one change
the owner's requirement is met and nothing signed has to move.

| | Who approves, and when | What happens on delay |
| --- | --- | --- |
| **Immediate safety** — obstacle, geofence, plausibility, failsafe | Nobody. It is not a proposal and never reaches an approval surface | The flight controller acts. Section 10 |
| **In-plan execution** — the next pass of an approved coverage plan | Pre-approved **once**, with the plan, at mission upload and start. This is `P7-AI-002`'s explicit operator approval event | **Nothing to delay.** No proposal exists, so no approval sits in the painting loop and latency cannot reach the wet edge |
| **Deviation from the plan** — the autonomy wants to go outside what was approved | Explicitly, per deviation | It expires and goes Absent, and the aircraft **continues the approved plan** rather than hovering |

The third row is what repairs section 10. The fallback for an unapproved deviation is the approved
plan, not a hover — so delay is benign on both axes at once: the flight controller still handles
hazards, and the aircraft keeps painting the pattern it was already authorised to paint.

The mechanism is already built and signed. `DG-MIS` is closed over the mission command set, the
Aerora complex-item type, its parameter set, the asset and obstruction geometry, and the plan-file
representation. A coverage plan is a real, reviewable, pre-approvable artifact, and approving it is
an explicit operator event with an audit record. Nothing here needs a timeout that approves, and the
accepted expiry rule stands exactly as signed: expiry never approves, it drops a deviation.

### What this sharpens about the class partition

The `unpartitioned-proposal-classes` hazard asked for each class to be marked approvable or
immediate. The answer is three-way rather than two-way, and the middle row is the one that was
missing: content that is **neither** immediate **nor** individually approvable, because it was
approved in advance as part of a plan. Most of what the autonomy does in a painting run is that
middle row, which is why per-step approval was never the right shape.

### The new gap: the coating window is recorded nowhere

Searched: no document in this tree records a wet-edge window, a recoat interval, a lap-mark
constraint or any coating timing at all. The overlap parameters that exist are upstream
camera-survey ones, `frontalOverlap` and `sideOverlap` for photo coverage, which are geometric and
not temporal.

So the constraint that makes "paint failure" real is unrecorded, and two things follow from it:

| Consequence | Owner |
| --- | --- |
| A coverage plan must be **flyable within the coating window**, which makes the window a planning constraint on the Aerora complex item rather than a display value | Product, with the applicator ICD |
| **A safety pause can cause a paint failure.** The flight controller stopping to avoid an obstacle may hold the aircraft past the wet edge and ruin the pass | Recorded consequence, not a conflict to resolve |

The second deserves saying plainly rather than discovering: safety wins, every time, and the pass is
lost. What the station owes the operator is that the cost is **reported** rather than hidden — an
avoidance action that ruined a pass should say so, because an operator who finds the lap mark later
and cannot explain it will start distrusting the avoidance that caused it. That is the same
reasoning as the unachievable-freshness-threshold check: a protection whose cost is invisible gets
switched off.

Neither consequence is settled here. The window is a Contract_Value with an applicator ICD behind
it, and no number is authored anywhere in this document.

## 12. A proposed class mapping for the named models

`PROPOSED`, unsigned, and it settles nothing. `IF-GPU-007` carries the class set as
**vehicle-reported** — "which autonomy modes and proposal classes exist" — so the set is firmware's
to report and not this document's to declare. What is offered here is a mapping of the models the
owner named onto the three-way partition of section 11, derived by applying one test, so that
firmware has something concrete to confirm or correct rather than a blank.

**The test.** If an output must take effect within human reaction time, it cannot be a proposal. If
it is the next step of a plan already approved, it needs no approval of its own. Everything else is
a deviation and needs one.

| Named model | Proposed class | Why |
| --- | --- | --- |
| Obstacle detection | **Immediate** | Its whole value is acting faster than a human can. Routing it through approval would be the owner's original concern made real, and it must not appear on an approval surface at all |
| Surface detection | **In-plan** | It keeps the applicator at the correct standoff and attitude while painting the approved pattern. Continuous, and already authorised by the plan; an approval here would sit directly in the wet-edge window |
| Path planning | **Depends, and the split is the point** — in-plan while it is sequencing passes inside the approved coverage pattern, deviation when it proposes leaving it | The same model produces both, so the class belongs to the **output** rather than to the model that produced it. That is the finding worth carrying: a per-model partition would be wrong |
| Object detection | **Neither — perception, not a command** | It reports what is seen. It becomes an input to one of the three above and is never itself approved. Displayed under `IF-UIS-003` and the autonomy fact group, read-only |

### The consequence for `IF-GPU-002`

`IF-GPU-002` already records that a proposal carries a **class**, which the partition needs. But the
path-planning row above means the class cannot be derived from the producing model — it has to be
**carried per proposal**, decided by the companion at the moment it emits one. A station that
inferred class from source would misclassify exactly the model that spans two classes, and the
misclassification is the dangerous direction: an in-plan continuation shown as a pending deviation
pauses the paint, and a deviation shown as in-plan executes unreviewed.

So `IF-GPU-002`'s class field is load-bearing rather than descriptive, and that is an argument for
the `TUNNEL` candidate of section 9 over the split-across-messages one: a class and its content
arriving in separate messages can arrive apart, and a proposal whose class has not yet arrived is
not safely displayable in either direction.

Nothing here is settled. The set is `IF-GPU-007`'s, the mapping is firmware's to confirm, and the
safe-or-default mode that an expired deviation falls back to is still `OPEN` in the
command-authority ICD.

## 13. Immediate-class actions run in all three modes, including manual

Owner-stated: collision avoidance and the operations a human cannot usefully control are AI-driven
in all three modes. That confirms the immediate class of section 11 and extends it — immediate means
immediate in **manual** too, not only under autonomy.

Two things follow, and the first looks like a contradiction and is not.

### Manual dominance and envelope protection are different axes

The firmware model's recorded claims, both `CONFIRMED`:

| Claim | Where |
| --- | --- |
| "Manual is **structurally dominant** when selected" | Command-authority ICD |
| "Manual \| Structurally dominant; **proposals do not override it**" | Same, autonomy-modes table |

Read the second row carefully: the dominance is recorded **over proposals**. It does not say nothing
overrides manual. That distinction is the reconciliation, and it is already in the wording:

| Axis | What it decides | Manual's position |
| --- | --- | --- |
| **Command source** — which party the flight controller obeys | Manual input, companion setpoints, station commands | **Manual wins.** This is what "structurally dominant" records, and the autonomy-modes row says so explicitly: proposals do not override it |
| **Envelope** — the bounds every command source is held inside | Obstacle proximity, geofence, altitude floor, attitude and rate limits | **Manual is bounded like everything else.** Protection is not a competing command source; it is a constraint on all of them |

So the pilot is dominant over the autopilot's navigation and over the companion's proposals, and is
**not** dominant over the envelope. Nothing recorded says otherwise, but nothing recorded says this
either, and a reader could easily take "structurally dominant" to mean the pilot can fly into a
wall. The owner has now said they cannot, so it is worth writing down.

### The companion perceives; the flight controller decides

"Controlled by AI only" needs splitting, and the split is not pedantic.

Baseline 48 records, `CONFIRMED`, that "the companion does not become flight authority". If the
companion computed and commanded the avoidance manoeuvre it would be flight authority for a safety
function, and that `CONFIRMED` row would be false.

The engineering reason is the same argument section 5 of
[product-completion 14](14-ground-link-classes-and-control-authority.md) makes about the ground
station, one wire shorter. The companion is a general-purpose computer running model inference: CUDA
synchronisation, ROS scheduling, allocator pauses and memory pressure all put a tail on its worst
case. Giving it the avoidance *decision* gives avoidance that tail. **The flight controller is the
only deterministic element in the system**, and a reflex belongs in the deterministic element.

| Function | Owner | Why |
| --- | --- | --- |
| Detecting the obstacle, classifying the surface, estimating range | **Companion.** This is what the models are for and nothing else can do it | Perception is compute-bound and tolerates a tail, because the flight controller can know how old a perception is |
| Deciding to stop, slow or deviate, and commanding it | **Flight controller** | A reflex needs a bounded worst case, and only the flight controller has one |

That is also why the clock decision of section 3.1 is load-bearing rather than bookkeeping: the
flight controller can only treat perception responsibly if it knows the perception's age, which is
what the companion's own timestamp is for.

### Two consequences that have to be designed, not discovered

**The pilot will be surprised.** If the envelope bounds manual input, there are moments when the
pilot commands a direction and the aircraft refuses or deviates. Unannounced, that reads as the
aircraft feeling sluggish or broken, and a pilot who cannot tell protection from malfunction fights
it. So the station must **report that an input is being bounded, and by what** — the same reasoning
as the unachievable-freshness-threshold check and the ruined-pass report of section 11: a protection
whose action is invisible gets distrusted, then disabled.

**Stale perception must degrade, not fail silently.** If the companion's perception ages past
usefulness, the flight controller must neither act on it as though fresh nor quietly stop protecting.
Both are wrong and the second is worse, because nothing announces it. That needs a third state —
avoidance **degraded**, reported to the station and visible to the pilot — alongside active and
inactive. It follows directly from the clock decision and is not settled here.

Neither consequence introduces a Contract_Value. No proximity distance, no timeout and no envelope
limit is authored anywhere in this document; those are the applicator and airframe limits that
`kDefinedThresholdCount` is pinned at zero to keep the station out of.

## 14. The firmware model, relayed: what it supersedes and what it does not

`PROPOSED`, unsigned, and it closes nothing. The owner has relayed a reading of the flight
controller's model-based firmware that bears directly on sections 9 and 10. The first thing to get
right about it is its provenance, because the content is load-bearing and the act behind it is thin.

### Provenance, before content

The artifact is a Simulink model of the flight-controller firmware. **It is not in this repository,
it was not opened in this pass, and what follows is a relay of an earlier reading rather than an
inspection.** Phase 3.1 already had to correct this tree's `MODEL` and `ARTIFACT-OBSERVED` classes
to mean a baseline description rather than an inspected artifact, recorded as finding `F3.1-06` in
[24-phase-3.1-correction-audit.md](../02-interface-specification/24-phase-3.1-correction-audit.md).
A relayed reading sits below even that. Nothing in this section earns `MODEL`, and no row of it is
citable as evidence for a Gate or for an interface status.

No number is transcribed. The relay carries mode codes, a companion-loss budget, a cell count, and a
connector and pin assignment. This document's status table records that no identifier, threshold or
message name is authored here, and a relayed number is the weakest possible basis on which to break
that. Where a value matters below, what it is *about* is named and the value stays with the issued
document.

### The `TUNNEL` supersession claim, corrected

The relay arrived with the conclusion that it supersedes section 9's carrier recommendation, on the
ground that the firmware uses standard commands and standard telemetry rather than a vendor `TUNNEL`
payload. That comparison puts the two interfaces back together, which is the one thing section 9
exists to keep apart.

| | What the relay describes | What section 9 recommends |
| --- | --- | --- |
| Interface | **B**, companion to flight controller, which the station does not participate in | **A**, flight controller to station |
| Object | The setpoint and mode-request stream | The **proposal** an operator reviews |
| Standing | What the firmware does today | A candidate carrier for a message that does not exist yet |

A command the companion sends to the flight controller cannot supersede a candidate carrier for a
message the flight controller would send to the station. Section 9's recommendation is therefore not
contradicted, and it is not confirmed either. It is untouched.

What the relay does change is narrower than a carrier choice and more consequential.

### The finding that matters: nothing carries a proposal today

The relayed flight-controller-to-station set is common-dialect telemetry that QGroundControl already
parses — heartbeat, system status, attitude, global position, GPS, battery, head-up values and status
text — and **it contains no proposal-carrying message at all.** The station is described as observing
the mode, an autonomy-active indication and status text.

Read against section 4, that moves the schema element rather than settling it:

| Before the relay | After it |
| --- | --- |
| The response half of the schema is open, and `DG-AUT-SOC-01` wants an issued interface control document cited for it | The response half is open **and the producer emits nothing to specify.** A schema cannot be issued for a message the firmware does not send |
| The element waits on Architecture with the external interface authority, which is a documentation act | It waits on **firmware work first**, and on that documentation act second |

This is worse for the Gate and better for the record. `DG-AUT:source-interface` was readable as one
document away from closing; on the relay it is a firmware change away from being specifiable at all.
The four screens the Gate holds — `SCR-AIS-01`, `SCR-AIS-02`, `SCR-AIS-03` and the proposal half of
`SCR-LIV-05` — stay unreleased, for a stronger reason than before.

It also sharpens section 12. `IF-GPU-002`'s class field is load-bearing rather than descriptive, and
a station that had to recover a proposal's class from a mode value and a status string would be
inferring it, which is the direction section 12 names as the dangerous one.

### Two mode encodings, and the axis each belongs to

The relay gives single-digit mode values. [09-command-authority-icd.md](../02-interface-specification/09-command-authority-icd.md)
records three mode sentinels as three-digit values, evidence `MODEL`, status `CONFIRMED` as observed.
Handed both lists, a reader reconciles them by assuming one is wrong.

Neither has to be. That document already separates two axes and records the mapping between them as
unresolved:

| | Flight mode | Authority mode |
| --- | --- | --- |
| Answers | What is the aircraft doing | Who may command it |
| Carried by | The standard heartbeat base and custom mode | Aerora reporting, candidate |
| Mapping between them | **`UNKNOWN`**, part of `Q-007` | |

The relay's values attach to a standard mode-setting command, which is the flight-mode mechanism.
The sentinels sit in the authority-mode table. So the likely reading is that the two lists describe
the two axes rather than disagreeing about one — and collapsing them is exactly `R-028`, the hazard
that document raises for a conflated value reaching the display, the log schema and the audit trail
together.

Likely is not established. Which axis each list belongs to is Firmware's to state, and until it
does, **neither list may be cited for the other axis.**

### The safe-or-default mode: an answer offered, and why it is not taken

Section 10 names this as load-bearing. The expiry rule is conservative only if the mode an expired
deviation falls back to is one the aircraft holds safely with nobody instructing it, and the
command-authority document carries that mode's identity as `OPEN`, flagged in the baseline as
requiring confirmation.

The relay offers a value for it. The row stays `OPEN`, on three reasons that stack:

| Reason | |
| --- | --- |
| Provenance | A relayed reading does not meet the bar for a row the baseline flags as requiring confirmation |
| Axis | The offered value is a flight mode, and the `OPEN` row sits in the authority-mode table. The previous subsection is why that is not a detail |
| Content | The mode named is an attitude-stabilised one, which is a **pilot-on-the-sticks** mode |

The third reason is the one to act on. Section 10's soundness argument needs a fallback the aircraft
holds without instruction, and it already names the failure: if that mode is ever one that holds the
aircraft in an unsafe state awaiting instruction, the accepted expiry rule stops being conservative.
A fallback that requires a human on the sticks is that case, not a counter-example to it.

So the relay does not close section 10's premise. It raises the priority of confirming it, and it
narrows the question to one Firmware can answer directly: does the fallback mode fly the aircraft,
or does it wait for a pilot?

### Three more relayed items, and where each belongs

None is this document's to settle. Each is named so it is not lost.

| Relayed item | Where it belongs | What it does here |
| --- | --- | --- |
| A companion-loss budget past which the aircraft lands by itself, in the autonomous and supervised modes | Firmware, and the failsafe rows of the command-authority document | Gives section 13's "stale perception must degrade, not fail silently" a concrete shape. There is a point past which the aircraft acts, and the station owes the operator the reason it acted |
| The companion-setpoint validation is marked placeholder and rejects by default | Firmware | The interface shape is described and the deciding logic is future work. `IF-GPU-006` corroboration cannot be observed against a gateway that rejects everything, so the station's side of the autonomy path is not integration-testable yet |
| A generic autopilot identifier, and the airframe type | `Q-018`, and [hardware-firmware-evidence-matrix.md](../01-aerora-architecture/hardware-firmware-evidence-matrix.md) | `Q-018` asks whether the flight controller reports a **distinct** Aerora autopilot identifier, and a generic value would answer it in the negative, which decides firmware-plugin registration. Referred and not answered: `Q-018` is classed `HARDWARE`, and a relay is not a hardware observation |

### The relay's one architectural complication

Recorded last because it has the widest consequence and the least support.

The relay describes the companion as forwarding the flight controller's telemetry onward to the
ground station. If that is what happens, the station's link partner is the companion's network stack
carrying the flight controller's messages, and three recorded positions need re-reading:

| Recorded | What a forwarding companion does to it |
| --- | --- |
| Section 1: companion traffic terminates at the flight controller and is not bridged onto the radio link, with video named as the only companion-to-station stream | **Survives in substance; its scoping table is incomplete.** Forwarding carries the flight controller's messages under the flight controller's identity, so no proposal is bridged and the companion becomes no one's MAVLink peer. But the station would receive flight-controller telemetry *through* the companion, and the table does not cover that |
| `IF-GPU-008`, ground station to companion, **None — prohibited**, `GCS-DECIDED`, `CONFIRMED` | **Harder to hold structurally.** A forwarding node is addressable at the transport layer by definition, and a station-to-flight-controller command would transit it. The prohibition is on MAVLink addressing rather than on packet transit, so the row is probably still true as written, and "probably" is not the status it carries |
| [Document 14](14-ground-link-classes-and-control-authority.md)'s link axes | The operator link mode and the Link_Class the station's MAVLink traverses would both be describing a path with a companion in it, which neither axis records |

Each of those is a `CONFIRMED` or applied position being questioned by a single relayed sentence.
That is not enough to move any of them and it is more than enough to ask. **The question for
Firmware is one sentence: does ground-station MAVLink traverse the companion computer?** The answer
either moves `IF-GPU-008`'s basis from topology to policy or leaves it on topology, and those are
not the same row.

### What this section does to the Gate

Nothing, by design.

| | |
| --- | --- |
| `DG-AUT:source-interface` | Still `OPEN`. The transport element stays settled by section 1. The schema element is now understood to need firmware work before it can be specified, which is a weaker position than before rather than a stronger one |
| `DG-AUT-SOC-01` | Unmet. An issued interface control document is still required and this section is not one. A Simulink model is not an interface control document, and a relayed reading of one is not evidence |
| The act of issuance | The owner's, and not this document's. [The Closure_Record directory](../aerora-decision-gate-closure/closures/README.md) records why: writing the content of a signature is the engineering forging the thing the signature exists to demand |
| Contract_Values | Still none authored here |
| The overlay | Still nothing. No autonomy-source type, enumerator or accessor is declared, so the `autonomy-source` check stays in force |

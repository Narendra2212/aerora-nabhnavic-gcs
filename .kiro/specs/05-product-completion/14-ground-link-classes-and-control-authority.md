# Ground Link Classes and Control Authority

The owner's answer on the ground links: `lora` and `wifi` are one class, and traffic balances across
the available links according to what each is capable of carrying. This records what that settles,
what it does not, and one mismatch it exposes.

## Status

| Item | Status |
| --- | --- |
| This document | `PROPOSED` — owner-answered, unsigned |
| `lora-wifi` as one Link_Class | **Applied** to the register's `linkProfiles`, revision `r1-lora-wifi-merged` |
| `rc` as a Link_Class | **Deliberately not added.** Section 2 |
| Routing principle | **Capability-based**, owner-stated. Section 3 |
| Link_Class parameter values | Still held by `Q-015`, unchanged. Both profiles carry empty parameters |
| Whether the RC path traverses the ground station | **Decided: it does not.** Section 5 gives the engineering reasoning |
| Manual authority on ground-station link loss | **A contradiction in signed content.** Section 6, and it is the serious finding here |
| New Contract_Values introduced | **None.** No throughput, latency, rate or numeric link identifier is authored here |
| Document number | 14, allocated here rather than by Tab 1, which [00-README.md](00-README.md) reserves to Tab 1 |

## 1. The collapse

The register recorded `lora`, `wifi` and `tether`. It now records `lora-wifi` and `tether`.

Two classes where there were three, because the owner states the LoRa and WiFi paths are one module
and one class rather than two. `linkClass` is a non-empty string and not an enumeration precisely so
the set can move without a schema change, and nothing outside the `linkProfiles` block references a
class, so the collapse is self-contained. `link-profile` confirms one profile per class in both
directions.

The parameter values are untouched. Both profiles carry `{}` and name `Q-015`, which is the recorded
state of a Link_Class whose figures are unsettled; merging two classes settles the set, not the
numbers.

## 2. Why `rc` is not a Link_Class

A Link_Profile exists to answer one question: how much telemetry a link can sustain. The fleet spec
derives which Telemetry_Tiers a Link_Class can carry from the profile, so that a tier the link
cannot sustain is never offered.

The RC path carries manual control, not telemetry tiers. A profile for it would hold throughput
parameters that answer a question the path is not asked, and the derivation over it would be
meaningless rather than merely empty. So `rc` is recorded here as a **control path** and not as a
Link_Class, and the register's profile set stays at two.

This is a distinction about what the block is for, not a judgement that the RC path matters less.

## 3. Capability-based routing

The owner's principle: traffic balances across the links in both directions according to the
capability of each.

That is the principle the overlay already implements for telemetry, which is worth recording because
it means nothing new is being invented — the Link_Profile to Telemetry_Tier derivation *is*
capability-based routing, and a tier a class cannot sustain is never offered.

Applied to the five command classes the accepted `DG-ROL:authorization-scope` record enumerates, the
routing resolves to exactly one carrier per class today:

| Class | Carrier | Why it is the only one |
| --- | --- | --- |
| manual-control | RC path | The `manual-control-latency` hazard forbids sticks traversing the station, and `DG-ROL` records no manual-control seam, so the station cannot originate the class at all |
| command | `lora-wifi` | An RC transmitter sends channels, not `COMMAND_LONG` |
| parameter | `lora-wifi` | Same |
| mission | `lora-wifi` | Same |
| payload | `lora-wifi` | Same |

**No tie-break is required, and that is the useful result.** Capability-based routing only needs an
arbitration rule where two links are capable of the same class, and no class here has two permitted
carriers. The co-equal authority problem does not arise from this routing — it would arise only from
giving the station a manual-control seam, which two separate recorded decisions already forbid.

Capability here means *permitted* capability rather than physical capability. A MAVLink
`MANUAL_CONTROL` message is physically carriable over `lora-wifi`; what makes `lora-wifi` not a
carrier for the class is the hazard and the absent seam, not the radio.

## 4. Three taxonomies, which are not the same set

Named because conflating them would surface as a wrong indicator in flight rather than as a
documentation defect.

| Axis | Values | Question it answers | Owner |
| --- | --- | --- | --- |
| Link_Class, register | `lora-wifi`, `tether` | How much telemetry can this class sustain | `Q-015` for the values |
| Firmware link state | LoRa, 2.4 GHz frequency-hopping, 433 MHz, all-stale — "four values evidenced in the model" | Which radio is the **vehicle** hearing | Firmware |
| Operator link mode | `lora-wifi`, `rc`, `lora-wifi + rc` | Which ground link is the **operator** using | Product, stated here |

None of the three matches the others, and the architecture already warns about the asymmetry between
the first two: the station may hold a link to one radio while the vehicle reports a different radio
as active, because the vehicle arbitrates over its own receivers independently of what the station
is connected to. The presentation must not imply they are the same thing.

The operator link mode set is recorded here as content for the vehicle-to-Aerora link identifier
mapping, which `communication-architecture.md` records as part of the interface control document and
`OPEN` until the message set is frozen. **No numeric encoding is frozen here**, for the reason the
enumeration integers are exceptions: a link identifier that travels becomes permanent in every
flight log.

The three modes are states the **vehicle reports**, not modes the station selects. A station-side
link selector is the alternative `ADR` rejected — "duplicates vehicle-side arbitration in the GCS,
which would then disagree with the aircraft" — and `AERORA-COMM-REQ-008` is `CONFIRMED` that the
station displays the vehicle-reported active link and never presents its own observation as that.

## 5. Decision: the stick stream does not traverse the ground station

The owner asked for the ground station to be the main source of authority with the RC signal under
its control, and delegated the engineering decision. **The stick stream reaches the flight
controller directly. The station authorises manual control and never carries it.**

### Why, from the control loop rather than from the existing record

An operator flying manually closes a visual feedback loop: observe attitude or position error, move
the stick, see the aircraft respond. What breaks that loop is not mean latency but **variance**. A
path with 20 ms mean and a 400 ms tail is worse than one with 60 ms mean and a 70 ms tail, because a
human adapts to constant delay and cannot adapt to jitter — the adaptation is what pilot-induced
oscillation is made of.

| Path | Frame period | Worst case |
| --- | --- | --- |
| Dedicated RC link, microcontroller to microcontroller | 4 to 8 ms | Bounded by design, with a failsafe defined in the receiver |
| Stick sample through a ground station on a general-purpose operating system | Nominally similar | **Unbounded.** Page faults, disk stalls, driver interrupt storms, a Qt event loop blocked by a telemetry burst or a QML render hitch, and on Windows a GPU driver timeout-detection reset which is a two-second budget by default |

That is roughly two orders of magnitude on the tail, and the tail is the part that matters. The
hazard record already states the operational consequence precisely — a host that stalls "delivers
stick positions late or not at all, and an operator holding the sticks has no way to tell which of
those is happening."

For this aircraft the consequence is concrete rather than theoretical. A hexacopter at hover close
to a vertical surface, carrying an applicator whose contact with that surface is the failure the
mission exists to avoid, has a few hundred milliseconds of margin. A stale attitude command inside
that window is a contact, and the recovery makes it worse: the stall clears and a burst of
out-of-date stick positions arrives.

### Why not the hybrid

Carrying sticks through the station for non-critical phases only was considered and rejected. It
introduces a handover, and a handover is a new failure mode located at exactly the moment authority
is ambiguous. It also requires classifying phases as critical or not, and in a surface-painting
mission flown at standoff distance almost every phase is critical. The cost is a doubled test
surface and a transition that can coincide with a stall; the benefit over a direct link is nothing
the operator can perceive.

### What the owner asked for, delivered without the latency path

The request was to integrate both controllers into one control system using the ground station. That
is achievable, and the layer to do it at is **authority and display, not carriage**:

| Concern | Where it lives |
| --- | --- |
| Which operator may fly manually, and when | The station, through `AeroraManualAuthorityGrant` — issue, refuse, refresh, expire, all audited |
| The stick signal itself | Transmitter to receiver, direct, bounded |
| The operator's unified picture | The station, which displays stick positions and grant state as mirrored telemetry |
| Presence of the transmitter | The station, over the transmitter's own USB connection |

A modern transmitter is simultaneously an input device a ground station can read over USB **and** a
direct radio carrier to the aircraft. So the station sees every stick movement, knows the
transmitter is present, and holds the authority to permit or withdraw manual flight — one control
system, one authority — while no stick sample ever crosses a non-deterministic scheduler. The
station becomes a single point of authority without becoming a single point of control, which is
the distinction `AeroraManualAuthorityTest` already records as the reason the grant exists instead
of passthrough.

This decision reverses nothing and reopens no Gate. It is the architecture already recorded, and
section 6 is the part of that architecture which does not hold together.

## 6. The finding: criteria 4 and 5 contradict the hazard they implement

Reaching the decision above surfaced an inconsistency in signed content, and it is more serious
than the question that surfaced it.

The `manual-control-latency` hazard, accepted by Architecture, records this mitigation:

> "That makes the station a single point of authority rather than a single point of control: a
> stalled host withdraws permission to fly manually and **never withdraws the ability to**."

Requirement 2 of the fleet-and-authority spec says:

| Criterion | Text |
| --- | --- |
| 4 | WHILE no unexpired Manual_Authority_Grant is in force, THE Flight_Controller SHALL accept **no** manual control input |
| 5 | WHEN the Aerora_Station loses its link to a Flight_Controller, THE Manual_Authority_Grant SHALL expire without refresh |

Chained: the station's link drops, the grant expires, and the flight controller accepts no manual
input. **The ability is withdrawn.** The mitigation's final clause is false given the criteria of
the requirement that the hazard exists to motivate, and Requirement 2's own user story states the
goal as ensuring "command authority does not become a single point of failure for flight control" —
which criteria 4 and 5 together make it.

Criterion 6 is where this was meant to be resolved: "THE Gate_Register SHALL record the behaviour a
Flight_Controller adopts when a Manual_Authority_Grant expires in flight, which `DG-ROL` settles."

**`DG-ROL` is `CLOSED` and settles nothing about it.** Its four Decision_Content elements are the
role names, the command classes each role may authorise, the selected authorization scope and the
evaluation point. Neither accepted record contains the word grant or expiry.

### Why it is latent rather than live

Nothing can rely on a grant today. `DG-ROL:authorization-scope` records the manual-control class as
"No seam is built", the grant exists as a type with no issuing path, and no manual control passes
through the station at all. So the contradiction is a defect in the record rather than a behaviour
in the aircraft — and it becomes live the moment a grant is wired to real enforcement, which is
exactly when it would be hardest to notice.

### The recommended resolution: the grant gates entry, not continuation

Both obvious readings are wrong, and seeing why points at a third that is better than either.

| Reading | Where it fails |
| --- | --- |
| Expiry reverts the aircraft to an **autonomous failsafe** | Consider the case that matters: the station's link drops while a pilot is actively flying close to a wall. The aircraft stops answering the sticks and initiates a recovery the pilot did not ask for, possibly into the surface being painted or into the tether. Taking control away from a pilot mid-manoeuvre is the worst available moment to do it |
| Manual authority is **dominant regardless of grant** | Then the grant gates nothing, and anyone with a transmitter on the right binding can fly an aircraft the station never authorised. The station's authority becomes a record of a decision it does not actually make |

The first is unsafe and the second is insecure, and the reason both fail is that they treat one
question as if it were one: *may manual control begin* and *may manual control continue* are
different questions with different answers.

**So: an unexpired Manual_Authority_Grant is required to *enter* manual control authority, and
expiry does not *end* it.**

| Case | Behaviour |
| --- | --- |
| No grant has been issued, someone moves the sticks | The flight controller does not enter manual authority. This is the security case, and it holds |
| A grant is in force, the pilot takes manual control, the station's link then drops | The grant expires and is recorded as expired. Manual authority **continues**. The pilot keeps the aircraft |
| A grant has expired and the pilot wants manual control again | Entry requires a grant, so it requires the station. The station's authority is real |
| The RC link itself is lost | Untouched by any of this. RC loss has its own firmware failsafe and always did; conflating the two is what produced the contradiction |

### Why this is the right answer and not a compromise

It makes the signed mitigation **true again, as written**. "A stalled host withdraws permission to
fly manually and never withdraws the ability to" is exactly this behaviour: permission is the grant,
which a stalled host stops being able to issue or refresh, and the ability is manual authority
already in force, which expiry leaves alone. The mitigation was right. Criterion 4 is what is wrong,
and it is wrong in one word: it says the flight controller shall accept no manual control *input*
where it should say shall not *enter* manual control authority.

It also matches how flight controllers actually behave. Manual mode is entered by the pilot and is
dominant once entered; a ground station link is a separate failsafe axis with its own action. The
firmware model's `CONFIRMED` position that "manual authority is structurally dominant" is about
continuation, and Requirement 2 criterion 4 is about entry — read that way the two were never in
conflict, and the register recorded a contradiction that exists only in the wording.

### What this needs, and from whom

| Action | Owner |
| --- | --- |
| Rewrite Requirement 2 criterion 4 from "shall accept no manual control input" to "shall not enter manual control authority" | The spec owner for `aerora-fleet-and-authority`. **Not done here** — editing an acceptance criterion of a completed spec should be a visible request rather than a quiet edit |
| Settle criterion 6 as: on in-flight expiry with manual authority in force, the flight controller continues to accept manual input and reports the grant expired; it does not revert to autonomous. RC link loss retains its own independent failsafe | Firmware with Architecture, and it carries the safety case |
| Retire the `manual-authority-on-station-loss` hazard once both land | Architecture |

The register carries this as the `manual-authority-on-station-loss` hazard with this resolution
recorded as the proposed repair. It stays open until signed, because a recommendation on a safety
case is not a safety case.

## What signing this would and would not do

It would settle the Link_Class set at two, record `rc` as a control path outside the profile block,
adopt capability-based routing with the per-class carriers of section 3, and confirm that the stick
stream does not traverse the ground station.

It would **not** close any Gate. `DG-AUT:source-interface` is the only open part and this touches
none of its content. It would not settle any Link_Class parameter value, which `Q-015` holds, and it
would not freeze a numeric link identifier.

It would not resolve section 6. That decision is Firmware's with Architecture, and signing this
document records the contradiction rather than answering it — which is the honest order, because the
answer changes what a flight controller does when a ground station dies in flight.

## Status vocabulary

Defined programme-wide in [00-README.md](00-README.md#status-vocabulary). This document redefines
none of them and claims `VERIFIED` for nothing.

## 7. The three operating modes, and the control chain under them

Owner-stated, and the useful finding is that the three modes are not new: they map exactly onto the
three authority sentinels the firmware model already evidences, which
[09-command-authority-icd.md](../02-interface-specification/09-command-authority-icd.md) publishes as
`CONFIRMED as observed`.

| Owner's mode | Sentinel | What the model already records about it |
| --- | --- | --- |
| Autonomous | **101**, automatic coverage | — |
| Hybrid | **102**, semi-automatic supervised | "requires **explicit pilot confirmation** before following a proposal", which is the owner's approve step |
| Manual | **100**, manual | "Manual is **structurally dominant** when selected" |

So this records a mapping rather than an enumeration. The ICD deliberately does not publish the set
as closed — whether these three are all of them is `UNKNOWN` and the safe-or-default mode's identity
is `OPEN` — and nothing here closes either.

### The control chain

The companion sends instructions describing where the aircraft should be and when. The flight
controller computes motor demand and flies there. The same split applies to the painting module.

That is the standard companion-computer arrangement and it is what makes `IF-GPU-002`'s recorded
authority of **"Proposal only"** true in operation rather than by assertion: the companion emits
setpoints, and the flight controller is free to refuse them. The firmware model already records
"Proposals may be **rejected**, routing to a safe or default mode" as `CONFIRMED`, and baseline 46
records geofence, velocity and acceleration proposal-rejection skeletons in the model. The companion
proposes; the flight controller disposes; neither claim is new.

The companion reaches the flight controller over **UART/USART carrying MAVLink**, which is section 7
of [product-completion 13](13-autonomy-source-path-and-companion-topology.md).

## 8. Correcting section 5: two latency classes, not one

Section 5 decided that the stick stream does not traverse the ground station. That decision is
correct and stands, **but it was written in a way that reads wider than it should.** It is a
statement about rate and attitude command. It is not a statement about every manual command, and
read as one it would forbid the manual mode the owner has just described.

| Class | What it commands | Tolerance | May the station carry it |
| --- | --- | --- | --- |
| **Rate or attitude** | Roll, pitch and yaw rate — adjacent to the inner loop | Tens of milliseconds, and the *tail* matters more than the mean | **No.** Section 5, unchanged |
| **Position or velocity setpoint** | Where to be, and when | Hundreds of milliseconds, because the flight controller holds position between setpoints | **Yes** |

The distinction is not a matter of degree, it is a matter of what the failure looks like:

| Stream | What a stall does |
| --- | --- |
| Rate command | Attitude authority is lost while the operator holds the sticks and cannot tell. The `manual-control-latency` hazard |
| Setpoint command | The aircraft holds its last commanded position. Benign, observable, and self-announcing — the station stops getting acknowledgements |

A setpoint stream degrades into a hover. A rate stream degrades into a loss of control. That is why
one may cross a general-purpose operating system and the other may not, and it is why the owner's
manual mode is not in conflict with section 5.

### The modes against the two classes

| Mode | Command source | Class | Carriage |
| --- | --- | --- | --- |
| Autonomous, 101 | Companion | Setpoint | Companion to flight controller, direct over UART |
| Hybrid, 102 | Companion, with the operator approving or overriding at the station | Setpoint | Station may carry the operator's half |
| Manual, 100, from the station | Operator | Setpoint | Station may carry it |
| Manual, 100, from a transmitter | Operator | **Rate** | **Direct, never the station.** Structurally dominant, and the path that remains when everything else is unavailable |

The last two rows are both "manual" and they are not the same thing. One is an operator commanding
positions from a workstation; the other is a pilot flying. Keeping them apart is what lets the
station be the normal operating surface without the station becoming load-bearing for flight
control — and it is why section 5's decision costs the owner nothing.

### What stays true in all three modes

The companion's output is visible to the station in every mode, read-only, and that is unaffected by
which mode is in force. `IF-UIS-003` presents a proposal as subordinate and labelled; the autonomy
fact group carries the values; `IF-GPU-008` keeps the reverse path prohibited. In manual mode the
same signals are guidance to the pilot rather than instructions to the aircraft, which is a
presentation difference and not an interface one — the station displays what is reported and
originates nothing.

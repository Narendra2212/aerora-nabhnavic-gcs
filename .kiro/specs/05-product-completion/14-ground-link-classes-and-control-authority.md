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
| Whether the RC path traverses the ground station | **OPEN, and blocking.** Section 5 |
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

## 5. What is still open, and why it blocks

One question, and it decides the authority model rather than decorating it.

**Does the RC path reach the flight controller directly, or through the ground station?**

| Reading | Consequence |
| --- | --- |
| RC transmitter to the aircraft's receiver, **direct**. The station connects over `lora-wifi` and sees the sticks as mirrored telemetry | The recorded architecture, and safe. The station grants manual authority through `AeroraManualAuthorityGrant`, the flight controller enforces it, and a stalled station withdraws permission to fly manually without ever withdrawing the ability to |
| Sticks routed **through the station** and forwarded over `lora-wifi` | Puts manual control on a path with no bounded worst-case latency, which is the `manual-control-latency` hazard exactly. The hazard's severity is recorded as loss of manual control authority in flight |

The phrase that raises it is "communicate with FC using RC controller and LoRa WiFi module", which
reads as two station transports; the safe architecture has the RC path as a **separate ground-to-
aircraft path that the station observes and authorises but never carries.**

Nothing in this document assumes either reading. The routing table in section 3 holds under both,
because it assigns manual-control to the RC path without saying what that path is wired to.

## What signing this would and would not do

It would settle the Link_Class set at two, record `rc` as a control path outside the profile block,
and adopt capability-based routing with the per-class carriers of section 3.

It would **not** close any Gate. `DG-AUT:source-interface` is the only open part and this touches
none of its content. It would not settle any Link_Class parameter value, which `Q-015` holds, and it
would not freeze a numeric link identifier.

## Status vocabulary

Defined programme-wide in [00-README.md](00-README.md#status-vocabulary). This document redefines
none of them and claims `VERIFIED` for nothing.

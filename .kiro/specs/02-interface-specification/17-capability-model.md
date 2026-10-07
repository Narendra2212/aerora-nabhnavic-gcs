# Capability Model

How the ground station learns what an aircraft can do, what it does with each of the three possible
answers, and why painting is a profile over this model rather than the model itself.

## This document defines no new interface identifiers

Capability **reporting** is already specified: `IF-FC-017` for the vehicle capability inventory and
`IF-PAY-002` for the payload capability set. Capability **consumption** is already specified:
`IF-UIS-005` for unknown versus unavailable and `IF-UIS-006` for hide versus disable.

What was missing is the model in between — the rules those four interfaces are bound by. That is
what this document supplies. Adding a fifth identifier for a rule set would be identifier sprawl,
not precision.

## Why the model exists

A ground station can learn what an aircraft can do in three ways:

| Route | Assessment |
| --- | --- |
| Assume, from the product it was built for | Fails the multi-product goal on the second product |
| Infer, from which telemetry arrives | Conflates "not reported yet" with "not present", and `AERORA-GCS-REQ-029` forbids it |
| **Be told** | The only route that survives a second payload |

So the aircraft reports its capabilities and the ground station adapts. This is the stated purpose of
the capability inventory in [mavlink-architecture.md](../01-aerora-architecture/mavlink-architecture.md):
the GCS adapts rather than assumes.

The test of the model is the one from
[system-architecture.md](../01-aerora-architecture/system-architecture.md): **can a second payload be
added without touching the common platform?** Every rule below exists to keep that answer yes.

## The hierarchy

```text
Vehicle                         one aircraft, one identity
  │
  ├── Product family            painting · firefighting · agriculture · inspection
  │
  └── Capability categories
        ├── Vehicle capability     what the airframe and flight controller can do
        ├── Payload capability     what the installed payload can do
        ├── Mission capability     which mission structures the vehicle will accept
        ├── Telemetry capability   which reporting the vehicle produces
        └── Command capability     which commands the vehicle will act on
```

Five categories, because they are answered by different authorities and they fail independently.

| Category | Reported by | Consumed by | Reporting interface |
| --- | --- | --- | --- |
| Vehicle capability | Flight controller | Flight UI, authority module, setup pages | `IF-FC-017` |
| Payload capability | Payload controller, via the flight controller | Payload module, product panels | `IF-PAY-002` |
| Mission capability | Flight controller | Mission module, plan validation | `IF-FC-017`, and the mission command set at `IF-MIS` |
| Telemetry capability | Flight controller and payload controller | Fact groups, instrument panel, records | `IF-FC-017`, `IF-PAY-002` |
| Command capability | Flight controller and payload controller | Command path, guided actions | `IF-FC-017`, `IF-PAY-002` |

### Why five and not one

A single capability bitmask would be simpler and would be wrong, because the categories have
different owners and different failure modes.

| If this is absent | Consequence |
| --- | --- |
| Vehicle capability | The flight interface degrades to the standard QGroundControl set |
| Payload capability | **No payload controls are presented at all** — `IF-PAY-002` |
| Mission capability | Plan validation falls back to the standard mission command set |
| Telemetry capability | Fact groups exist but report unknown; the panels are present and empty-marked |
| Command capability | Aerora guided actions are hidden, not disabled |

Four different degradations. A single mask would force one of them on all five.

## The three questions a capability answer must distinguish

This is the substantive gap Phase 3 found, and it is unresolved.

| Question | Example | Answered by |
| --- | --- | --- |
| **Presence** — does this aircraft have the capability at all? | This airframe has no spray system | Vehicle, per configuration |
| **Availability** — can it be used right now? | The spray system is present but the tank is empty | Vehicle, per state |
| **Permission** — may this operator use it? | The operator's role excludes payload commands | Ground station, `IF-SEC-004` |

Permission is cleanly separated: it is a ground-station decision made at `IF-SEC-004`, and it never
reaches the vehicle. The problem is the first two.

**Nothing evidenced shows that the vehicle reports presence separately from availability.** The
capability inventory is `NOT-EVIDENCED` for its content, so whether it carries one bit or two per
capability is unknown. That is Q3-009.

It matters because `AERORA-UI-REQ-008` requires exactly this distinction to choose between hiding a
control and disabling it with a reason:

| Presence | Availability | Presentation | Operator reads |
| --- | --- | --- | --- |
| Absent | — | **Hidden** | The aircraft does not do this |
| Present | Unavailable | **Disabled, with the reason** | It does this, not right now, and here is why |
| Present | Available | Presented | Usable |
| **Unknown** | **Unknown** | **Shown as unknown** | The aircraft has not said |

The fourth row is the honest default and it is not the same as the first. An aircraft that has not
yet reported its capabilities is not an aircraft without capabilities, and a station that renders
the two identically has made an inference it was told not to make.

## Resolution rules

These are the rules the four reporting and consuming interfaces are bound by.

| Rule | Statement | Requirement |
| --- | --- | --- |
| 1 | A capability not reported is **unknown**, not absent | `AERORA-GCS-REQ-029` |
| 2 | A capability reported absent is absent, and the dependent interface is **hidden** | `AERORA-UI-REQ-008` |
| 3 | Capability is **never** inferred from the arrival of telemetry, the success of a command, or the presence of a component id | `AERORA-GCS-REQ-029` |
| 4 | Capability is never inferred from the product family. The family is a **label**, not a source | `AERORA-PAYLOAD-REQ-013` |
| 5 | A capability set is current **for a connection**; it is re-established on reconnection, never carried across | `AERORA-VEH-REQ-003` |
| 6 | An unrecognised capability identifier is retained, recorded and ignored — **not** treated as absent, and not treated as an error | `AERORA-GCS-REQ-026` |
| 7 | The default for an unreported Aerora capability is **not presented**, which is distinct from reported-absent | `AERORA-UI-REQ-008` |
| 8 | Capability drives presentation and command gating. It does **not** drive telemetry parsing, which is metadata-driven | `AERORA-GCS-REQ-026` |

Rule 3 is the one that will be violated first. "The payload status message arrived, so there is a
payload" is a correct inference nine times in ten, and the tenth time it presents controls for an
actuator that is not fitted.

Rule 6 is what lets the firmware add a capability without a ground-station release. The station does
not need to understand a capability to record that the aircraft claimed it, and a recorded unknown
claim is the evidence that a compatibility matrix is out of date.

### Rule 5 and the identity dependency

A capability set belongs to a connection, and a connection belongs to a vehicle identity through
`IF-VID-004`. Where identity is unknown — the common case today, because `IF-VID-003` is `OPEN` —
the capability set is still usable for that connection and is simply not retained for the next one.

This is deliberately conservative. Retaining a capability set against a MAVLink system id would
restore one aircraft's capabilities onto another, which is the same class of failure
`AERORA-GCS-REQ-014` prevents for every other artifact that outlives a connection.

## Capability sources, ranked

Where the ground station may learn a capability from, strongest first. This is the Phase 3 source
hierarchy applied to one subject.

| Rank | Source | Evidence status today |
| --- | --- | --- |
| 1 | The vehicle capability inventory, `IF-FC-017` | `NOT-EVIDENCED` for content; the carrying family is a candidate |
| 2 | The payload capability set, `IF-PAY-002` | `NOT-EVIDENCED`, `OPEN` |
| 3 | The standard MAVLink autopilot capability bitmask | `GCS-VERIFIED`; covers standard features only |
| 4 | The vehicle-supplied mission command set | `GCS-VERIFIED` mechanism |
| 5 | Ground-station configuration, for ground-station-only capabilities | `GCS-DECIDED` |
| — | Inference from observed traffic | **Prohibited** — rule 3 |

Ranks 1 and 2 are the two that matter for Aerora, and both are `NOT-EVIDENCED`. The model is
therefore specified and **not yet populated**: the structure is ready, the content is a firmware
deliverable, and no part of this document invents it.

## Painting: the first product capability profile

Painting is the **first product**, not the model. This section is a profile over the model above, and
the model does not change when a second product arrives.

| Property | Value |
| --- | --- |
| Product family | Painting |
| Status as a product | First product, per the baseline |
| Status in this model | **A profile.** Nothing in the hierarchy above mentions paint |
| Where it lives in the code | Entirely inside a payload module in layer 2 |
| What it may touch in layer 3 | **Nothing.** Layer 3 stays product-independent |

### What the profile declares

| Element | Content | Evidence | Status |
| --- | --- | --- | --- |
| Payload fact group | `aeroraPayload` — identity, capabilities, health, safety state | `BASELINE-ASSERTED` | `PROPOSED` |
| Product fact group | `aeroraPaint` — tank level, pump, pressure, flow, nozzle, spray state | `BASELINE-ASSERTED`, carried from Phase 2 | `PROPOSED` |
| Product telemetry interface | `IF-TLM-004` | `PLACEHOLDER` | `OPEN` |
| Product mission structure | A coverage pattern as a complex item in layer 2 | `GCS-VERIFIED` mechanism; geometry undefined | `OPEN` |
| Product map visuals | Through the tier-2 map-items seam, `IF-UIS-011` | `GCS-VERIFIED` | `CONFIRMED` |
| Product guided actions | Through the tier-2 guided-actions seam, `IF-UIS-011` | `GCS-VERIFIED` | `CONFIRMED` |
| Product panel | Through the tier-2 fly-view overlay seam, `IF-UIS-011` | `GCS-VERIFIED` | `CONFIRMED` |

The six value names in the product fact group are carried forward from Phase 2 and are **names
only**. Units, ranges, enumerations, actuator identities and command sets are not specified:

| Not specified | Reason |
| --- | --- |
| Units and ranges for tank, pressure and flow | Not evidenced; a firmware and product deliverable |
| Nozzle and spray state enumerations | Q-023; firmware placeholders only |
| Painting command set | Q-023 |
| Coverage pattern geometry and parameters | Q-023 and product definition |
| Calibration procedures | Q-043 |
| Whether a painting plan must survive download from the vehicle | Q-022 |

The firmware model contains `Payload_Interface`, `Painting_System` and `Payload_Actuator_Drivers`
with the actuator behaviours as placeholders. That is enough to confirm the structure exists and
not enough to specify a single value, which is why every row above that could have been filled in
by plausible guesswork is left `OPEN`.

### The profile boundary, stated as a prohibition

| Painting may | Painting may not |
| --- | --- |
| Define its own fact group and metadata | Add a field to a layer-3 structure |
| Define its own complex mission item | Modify the mission engine |
| Add guided actions through the tier-2 seam | Add a branch to upstream action dispatch |
| Add map visuals through the tier-2 seam | Override a populated upstream map file |
| Declare payload capabilities it consumes | Assume a capability the vehicle has not reported |
| Report a paint condition as payload state | Let a paint condition reach a flight command |

The last row is architectural principle 8, and it is the one a product profile is most likely to
breach, because "stop the aircraft when the tank is empty" is an obviously sensible feature and it
is a payload concern reaching a flight concern. If that behaviour is wanted, it belongs in the
aircraft, not in the ground station.

## The second-product test

The model earns its keep only if a second payload costs nothing in the common platform. Phase 3
states the test concretely so it can be run rather than asserted.

| Adding a second product requires | Does it touch the common platform? |
| --- | --- |
| A new payload module in layer 2 | No |
| A new product fact group and metadata file | No |
| New complex mission items | No |
| New entries at the four tier-2 QML seams | No |
| New capability identifiers in the vehicle's inventory | No — rule 6 means the station records what it does not recognise |
| A new product panel | No |
| A change to the capability hierarchy | **If yes, the model is wrong** |
| A change to layer 3 | **If yes, the boundary has been breached** |

The last two rows are the test. Everything above them is additive by construction; everything below
them is the failure signal.

Painting is the only profile that currently exists, so the test cannot be run yet. That is worth
saying plainly: a structure designed for extension has not been extended, and until it has, the
claim that it extends cleanly is a `SPECIFIED-NOT-VERIFIED` claim rather than a demonstrated one.

## Verification

| Property | Method | Where |
| --- | --- | --- |
| Capability absent hides the dependent control | `INTEGRATION` | `IF-UIS-006` |
| Capability unknown is distinguishable from capability absent | `INTEGRATION` | `IF-UIS-005` |
| No capability is inferred from telemetry arrival | `UNIT` | Rule 3 |
| A capability set is not carried across a reconnection | `INTEGRATION` | Rule 5 |
| An unrecognised capability identifier is recorded and ignored | `UNIT` | Rule 6 |
| Painting touches no layer-3 structure | `STATIC` | Profile boundary |
| A second product adds no common-platform change | `STATIC`, when a second product exists | Second-product test |

Method assignments per interface are in
[22-phase-3-verification-plan.md](22-phase-3-verification-plan.md). No result is claimed here.

## Conflicts recorded

| Conflict | Position A | Position B | Resolution authority |
| --- | --- | --- | --- |
| Capability presence versus availability | `AERORA-UI-REQ-008` requires the distinction | No evidenced interface reports both | **Unresolved.** Q3-009; firmware evidence required. Until then `IF-UIS-006` is correct for standard capabilities and unpopulated for Aerora ones |
| Product independence versus product usefulness | Layer 3 must stay product-independent | A painting-aware mission engine would plan better coverage | **Resolved by rule.** Phase 2 dependency rule 3 keeps Payload and Mission mutually independent; they meet only through emitted mission items and reported payload state |

The second is recorded rather than omitted because the coupling is the obvious implementation. The
Phase 2 decision stands, and the reason is worth keeping visible: a mission engine that knows about
paint cannot be reused for firefighting without being taught about water.

## Not specified, and why

| Item | Reason |
| --- | --- |
| Capability identifier values and encoding | `NOT-EVIDENCED`; a firmware and dialect deliverable |
| Whether presence and availability are reported separately | Q3-009 |
| The painting capability set | Q-023 |
| Firefighting, agriculture and inspection profiles | No product definition exists. Writing them would be the universal-architecture mistake the directive names |
| Capability versioning across firmware revisions | Depends on the capability encoding, which is undefined |
| Ground-station-only capability declarations | `GCS-DECIDED` in principle; no content needed until a feature requires one |

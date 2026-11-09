# Companion Computer Interface Control Document

How the companion computer's output reaches the ground station, and the authority boundary that
makes it safe to display.

"GPU" in the baseline means the Aerora companion computer running perception and autonomy
workloads. It does not mean graphics acceleration on the ground-station host, which is a rendering
concern and outside this document.

## The authority boundary

The baseline is explicit: the companion is **not authoritative**. It proposes; the flight controller
decides; the ground station observes both.

| Statement | Status |
| --- | --- |
| The companion may propose intelligence | `CONFIRMED` — baseline 48 |
| The companion does not become flight authority | `CONFIRMED` — baseline 48 |
| Manual authority is structurally dominant | `CONFIRMED` — firmware model |
| Supervised autonomy requires explicit pilot confirmation | `CONFIRMED` — firmware model |
| Autonomy proposals may be rejected | `CONFIRMED` — firmware model |
| The ground station has no path to the companion | `CONFIRMED` — topology, and the autonomy module has no outbound command path |

The last row is structural rather than procedural. A module with a command path and a policy
forbidding its use is one code review away from violation; a module with no command path is not.

## Transport

```text
COMPANION
     │  companion UART — ARTIFACT-OBSERVED in the schematic
     ▼
FLIGHT CONTROLLER
     │  MAVLink subsystem receives autonomy commands — MODEL
     ▼
radio link
     ▼
GROUND STATION
```

| Property | Position | Evidence | Status |
| --- | --- | --- | --- |
| Companion connects to the flight controller, not the ground station | Required | `ARTIFACT-OBSERVED` | `CONFIRMED` |
| Transport between them | UART | `ARTIFACT-OBSERVED` | `CONFIRMED` |
| Protocol configuration, baud, connector assignment | **`UNKNOWN`** — baseline 50 requires these frozen in the interface control document before implementation | — | `OPEN` |
| Companion module identity | **`UNKNOWN`** | — | `OPEN` |
| Whether companion traffic is bridged onto the radio link or terminated at the flight controller | **Terminated.** The flight controller summarises and re-emits; it is the command centre | Owner-answered, `Q3-005` | `PROPOSED` |

The architecture depends on the **topology**, not the parameters. The topology is observed; the
parameters are open and do not affect the ground station.

The message path was the one thing the topology did not settle, and it is now answered: companion
traffic terminates at the flight controller. That is `Q3-005`, and
[product-completion 13](../05-product-completion/13-autonomy-source-path-and-companion-topology.md)
carries the answer with its consequences. The rows below are written against it. Every row that
depends on the answer reads `PROPOSED` rather than `CONFIRMED`, because the document carrying the
answer is itself unsigned.

## The two interfaces, kept separate

Baseline 07 requires the ground-station-to-flight-controller interface and the
flight-controller-to-companion interface to be separately specified **even though they share one
dialect**. This is a real constraint.

| | Interface A | Interface B |
| --- | --- | --- |
| Between | Ground station and flight controller | Flight controller and companion |
| Carries | Operator commands, telemetry, mission, parameters | Autonomy proposals, perception status |
| Trust level | Operator-originated | Non-authoritative proposals |
| Change cadence | Ground protocol, slower | Companion stack, faster |
| Security posture | Crosses a radio | Internal wired link |
| Ground station participates in it | **Yes** | **No** |

The ground station is specified to participate in interface A only. That structurally prevents it
from impersonating the companion computer, which is the point. Neither interface is implemented;
both are contracts.

## How companion data reaches the ground station

**Not by this path, now that `Q3-005` is answered.** The four behaviours below are what would carry
*companion-addressed* traffic to the vehicle model, and under the summarised reading the companion
addresses the ground station with nothing. They are retained because they are verified upstream
behaviour and they still describe how any non-autopilot component would be handled — but the
proposal path is `IF-FC-010`, and it needs none of them.

Verified in QGroundControl source. Two existing filters pull in opposite directions, and the
combination is exactly what is wanted.

| Step | Behaviour | Consequence |
| --- | --- | --- |
| Vehicle creation on heartbeat | Heartbeats from a non-autopilot component id are ignored | No phantom second vehicle |
| Message delivery to the vehicle | Filters on system id only; component id is not filtered | Companion telemetry reaches the vehicle model |
| Fact group dispatch | Every inbound message is offered to every fact group | The Aerora autonomy fact group receives it |
| Packet-loss accounting | Scoped to the first-heard component id | Companion traffic does not distort link statistics |

All four are `GCS-VERIFIED`. Companion observability therefore costs **no new transport, no second
vehicle and no core edit**.

### What that requires of the vehicle side

These were the three obligations, and they were the first thing to confirm with the firmware team.
`Q3-005`'s answer **retires all three**: each exists only where the companion is a MAVLink component
the ground station receives from, and under the summarised reading it is not. There is no system-id
filter to satisfy and no phantom second vehicle to avoid.

| Retired obligation | Why it no longer applies |
| --- | --- |
| The companion shares the flight controller's system id | No companion message is addressed to the ground station, so no message is exposed to the system-id filter |
| The companion uses a non-autopilot component id | No companion heartbeat reaches vehicle creation, so no phantom vehicle can appear |
| The companion does not impersonate the autopilot in any message | The companion emits nothing onto the radio link, so the impersonation surface does not exist on this interface |

**Retired here, not everywhere.** The owner states the companion speaks MAVLink to the flight
controller, so it does hold a MAVLink identity and all three constraints survive on the internal
link — which baseline 07 requires to be specified separately from this one. What is retired is their
standing as *ground-station-facing* obligations, because the station receives nothing from the
companion and no companion message is exposed to its filters. `AERORA-GPU-REQ-001` and
`AERORA-GPU-REQ-002` are therefore re-scoped rather than withdrawn, and under the summarised
topology they also become defence in depth: if the flight controller ever bridges a companion
message onto the radio link by mistake, they are what stops it being read as the autopilot.

### The obligation that replaces them

One obligation takes their place, and it is heavier than the three it replaces because getting it
wrong is silent.

| Obligation | Owner | Status | Failure if violated |
| --- | --- | --- | --- |
| The flight controller's summary carries the **companion's own timestamp**, not only the time of re-emission | Firmware | `OPEN` — created by `Q3-005`'s answer | `AERORA-GPU-REQ-006` is satisfied against the wrong clock: a perception result computed seconds earlier, or one the companion stopped producing, reads as current |

`Q3-005` states this directly — under the summarised reading the requirement "is satisfied against
the wrong clock unless the summary carries the companion's own timestamp". The ground station cannot
detect the violation, because a summary stamped at re-emission is indistinguishable from a fresh
one. That is why it is recorded as an interface obligation on the producer rather than as something
the station can check.

**Which clock the ordering key resolves to is not settled here.** The accepted
`DG-AUT:proposal-model` Closure_Record settles the proposal timestamp as two values carried
together, `occurredUtcMs` for wall time and `monotonicUs` as the ordering source of truth. Under the
summarised path there are three candidate monotonic clocks — the companion's, the flight
controller's and the ground station's — and the record does not say which. It needs Architecture
with Firmware, and nothing here chooses it.

## Interface contracts

| ID | Interface | Direction | Producer | Consumer | Crossing | Update | Freshness | Authority | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-GPU-001` | Companion addressing | **None — companion traffic terminates at the flight controller** | — | — | Nothing reaches the ground station under a companion component id | — | Not applicable | **No authority** | `ARTIFACT-OBSERVED` topology; message path answered by `Q3-005` | `PROPOSED` |
| `IF-GPU-002` | Proposal | **None direct — carried by `IF-FC-010`** | Flight controller, summarising companion output | Autonomy module, via `IF-FC-010` | A proposal: class, content, and its own identity, re-emitted by the flight controller | `UNKNOWN` | Per-class threshold on the summary; see the clock obligation below | **Proposal only** | `MODEL`, partly future work; producer answered by `Q3-005` | `OPEN` |
| `IF-GPU-003` | Proposal outcome | Vehicle to GCS | Flight controller | Autonomy module | Accepted, rejected, with a reason | `UNKNOWN` | Stamped at parse | Flight controller | `NOT-EVIDENCED` | `OPEN` |
| `IF-GPU-004` | Companion health | **None direct — carried by `IF-FC-011`** | Flight controller, summarising companion health | Autonomy module, diagnostics | Health, reported load, perception status, re-emitted by the flight controller | `UNKNOWN` | Per-class threshold on the summary | Non-authoritative | `NOT-EVIDENCED`; producer answered by `Q3-005` | `OPEN` |
| `IF-GPU-005` | Companion presence | **None direct — carried by `IF-FC-011`** | Flight controller, summarising companion presence | Autonomy module | Presence, as distinct from reporting nothing, re-emitted by the flight controller | `UNKNOWN` | Absence detected by threshold on the summary | Non-authoritative | `NOT-EVIDENCED`; producer answered by `Q3-005` | `OPEN` |
| `IF-GPU-006` | Effective state for corroboration | Vehicle to GCS | Flight controller | Authority module | The effective authority mode and relevant vehicle state | Streamed | Stamped at parse | Flight controller | `MODEL` | `PROPOSED` |
| `IF-GPU-007` | Autonomy capability | Vehicle to GCS | Flight controller | Capability model | Which autonomy modes and proposal classes exist | On connection | Current for the connection | Vehicle | `NOT-EVIDENCED` | `OPEN` |
| `IF-GPU-008` | Ground station to companion | **None — prohibited** | — | — | Nothing | — | — | **No authority** | `GCS-DECIDED` | `CONFIRMED` |

| ID | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- |
| `IF-GPU-001` | Not applicable — no companion-addressed path exists on this interface. `AERORA-GPU-REQ-001` and `AERORA-GPU-REQ-002` lose the ground-station rationale they were written against but keep their statements, **re-scoped to the companion-to-flight-controller link**, which the owner states is MAVLink. Not withdrawn | — | R-014 | `Q3-005`, answered | `INTEGRATION` |
| `IF-GPU-002` | Stale proposals marked stale, then absent; never retained as current. **Staleness is measured on the summary**, so it detects a stale proposal only where the summary carries the companion's own timestamp | `AERORA-GPU-REQ-004`, `AERORA-GPU-REQ-006` | R-014 | Q-008 | `INTEGRATION` |
| `IF-GPU-003` | Absent outcome means the proposal and the effective state are shown side by side with **no causal link asserted** | `AERORA-GPU-REQ-005` | R-014 | Q-008 | `INTEGRATION` |
| `IF-GPU-004` | Health unknown; not inferred from proposal activity | `AERORA-GPU-REQ-006` | — | — | `INTEGRATION` |
| `IF-GPU-005` | Absent companion distinguished from a companion reporting no proposal | `AERORA-GPU-REQ-007` | — | — | `INTEGRATION` |
| `IF-GPU-006` | Effective state stale; proposals cannot be corroborated and are shown unresolved | `AERORA-SAFE-REQ-006` | R-028 | Q-007 | `INTEGRATION` |
| `IF-GPU-007` | No autonomy capability reported means no autonomy presentation at all | `AERORA-MAV-REQ-009` | — | — | `INTEGRATION` |
| `IF-GPU-008` | Not applicable — no path exists. Asserted by a static check that the module emits no MAVLink. **No perception or autonomy inference runs on the ground-station host either**, so there is no ground-side result that could become authoritative | `AERORA-GPU-REQ-003`, `AERORA-GPU-REQ-008`, `AERORA-MAV-REQ-007` | R-014 | — | `STATIC`, `UNIT` |

## Proposal lifecycle

Every transition happens on the aircraft. The ground station contributes nothing to it, and its job
is to make the distinction legible.

```text
companion observes  →  proposal published  →  flight controller evaluates
                                                      │
                                       ┌──────────────┼──────────────┐
                                       ▼              ▼              ▼
                                   accepted        rejected       expired
                                   → effective     → reason       → stale
```

| State | Where it is decided | Ground station treatment |
| --- | --- | --- |
| Proposal published | Companion | Shown, visually subordinate, labelled as proposed |
| Accepted | Flight controller | Shown as the outcome, alongside the effective state |
| Rejected | Flight controller | Shown with its reason |
| Expired or stale | Elapsed time | Marked stale; never silently retained as current |

### Acceptance must be reported, not inferred

| Option | Assessment |
| --- | --- |
| The flight controller reports acceptance explicitly | **Required** |
| The ground station infers acceptance by comparing a proposal against subsequent state | **Rejected** |

Inference is rejected not because it is difficult but because it is **unfalsifiable**. An inferred
acceptance cannot be distinguished from a coincidence, and the audit trail would then record a
correlation as a decision.

If the flight controller does not report the outcome, the ground station shows the proposal and the
effective state side by side and **asserts no causal link between them**. That is the defined
fallback and it is honest.

This is Q-008 and it remains open.

## Freshness and timeout

| Property | Position | Status |
| --- | --- | --- |
| Receive time stamped at parse | Required | `CONFIRMED` |
| Staleness thresholds are per proposal class | Required | `PROPOSED` |
| Threshold values | **Not specified** | `OPEN` |
| Source of threshold values | The vehicle-reported interface definition, when it exists | `OPEN` |
| A stale proposal is retained as current | **Prohibited** | `CONFIRMED` |

The baseline rule the ground station must satisfy: it shall not transform a stale companion command
into a current one. Thresholds are **not invented here**. The firmware model marks the companion
freshness wiring as future work, so any number Phase 3 chose would be fiction with the authority of
a specification.

## Autonomy modes

The three evidenced authority sentinels are in
[09-command-authority-icd.md](09-command-authority-icd.md). Their relationship to companion
proposals:

| Mode | Companion proposal effect | Evidence |
| --- | --- | --- |
| Manual | Structurally dominant; proposals do not override it | `MODEL` |
| Supervised autonomy | A proposal is followed only after explicit pilot confirmation | `MODEL` |
| Autonomous coverage | `UNKNOWN` — the blending wiring is marked future work | `MODEL`, incomplete |

### Not specified

| Item | Reason |
| --- | --- |
| Autonomy blending equations | The firmware model marks the throttle, roll and pitch blending as future wiring |
| Proposal authority bounds | Not evidenced; the model notes bounds as open |
| Freshness timeout values | Not evidenced |
| Yaw handling | Explicitly open in the baseline analysis |
| Final authority arbitration | Vehicle-side, and not frozen |

Inventing any of these would place a number on a safety-adjacent path with no basis. The ground
station does not need them: it displays what is reported and corroborates against effective state.

## Out of scope, declared

Recorded so the read-only property cannot be eroded by a later feature arriving through this door.

| Out of scope | Reason |
| --- | --- |
| Model deployment to the companion | Not a ground-station function; if it becomes a product requirement it is a separate subsystem with its own authority analysis |
| Companion workload management | Same |
| Companion software update | Same |
| Ground-side perception or inference | Would raise the question of whether a ground-side result could become authoritative |
| Video | QGroundControl's video pipeline; the only overlap is that a perception overlay needs both a frame and a proposal |

## Conflicts recorded, and the one that is answered

| Conflict | Position A | Position B | Authority |
| --- | --- | --- | --- |
| Whether autonomy proposals reach the ground station directly from the companion or summarised by the flight controller | The topology implies everything passes through the flight controller | The firmware model's MAVLink subsystem receives companion commands, which suggests the companion speaks MAVLink and could address the ground station directly on a shared system id | **Answered by the Firmware owner: Position A.** Companion traffic terminates at the flight controller, which is the command centre |

Recorded as `Q3-005`. Both positions were preserved until it was answered, which is why `IF-FC-010`
and `IF-GPU-002` were written as two interfaces rather than one.

The answer collapses them. `IF-FC-010` is the interface that exists; `IF-GPU-002` carries no direct
path and its producer is the flight controller. The rows above are written against that, the three
`IF-GPU-001` addressing obligations are retired, and the summary's timestamp obligation replaces
them. The answer and its full consequence list are in
[product-completion 13](../05-product-completion/13-autonomy-source-path-and-companion-topology.md).

What the answer does **not** settle: the proposal schema itself. `IF-FC-010` carries `UNKNOWN` for
its crossing, update rate and freshness, and no message carries a proposal yet — so
`DG-AUT:source-interface` stays `OPEN` on the schema element, and `DG-AUT-SOC-01` still wants an
issued interface control document cited for it. Resolving `Q3-005` narrowed that part from three
open elements to one.

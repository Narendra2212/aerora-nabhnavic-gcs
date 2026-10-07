# Power Telemetry Interface Control Document

What the ground station is told about power, and what it deliberately does not define.

## The confirmed power architecture

Two **operating configurations**, from baseline 49.

```text
TETHER CONFIGURATION
  480 V DC from the ground  →  tether  →  onboard DC-DC stage  →  48 V aircraft domain

BATTERY CONFIGURATION
  onboard battery  →  aircraft power domain
```

| Element | Evidence | Status |
| --- | --- | --- |
| Dual-power requirement, two physical configurations | `BASELINE-ASSERTED` — baseline 49 | `CONFIRMED` |
| 480 V DC tether input, specified at product level | `BASELINE-ASSERTED` — baselines 49 and 50 | `CONFIRMED` |
| Onboard conversion to a 48 V domain | `BASELINE-ASSERTED` | `CONFIRMED` |
| Dual tether and battery input connectors | `ARTIFACT-OBSERVED` — schematic | `CONFIRMED` |
| Current and power monitoring hardware | `ARTIFACT-OBSERVED` — schematic | `CONFIRMED` |
| Converter topology, source arbitration, diode-OR behaviour, tether-loss detection, transition transients | **Explicitly not proven by the schematic** — baseline 49 says so | `OPEN` |

The last row is the baseline's own words, not a Phase 3 caveat: the schematic "does not, by itself,
prove" those five things, and "these must be specified and verified".

### Configurations, not switching

Baseline 49 states that the two configurations are "not yet a claim of automatic in-flight source
switching", and that any future source transition or hybrid operation "requires separate engineering
and verification".

So the ground station's power model is a **configuration and state model**, not a transition model.

## Required ground-station power states

Baseline 49 enumerates nine. All nine are adopted as interface items; their content and range are
mostly unevidenced.

| ID | State | Type | Units | Range | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- |
| `IF-PWR-001` | Tether connected or disconnected | Boolean | — | — | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-PWR-002` | Battery present or absent | Boolean | — | — | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-PWR-003` | Active source | Enumerated | — | **`UNKNOWN`** — value set not defined | `BASELINE-ASSERTED` | `OPEN` |
| `IF-PWR-004` | Source transition | Enumerated or boolean | — | **`UNKNOWN`** | `BASELINE-ASSERTED` | `OPEN` |
| `IF-PWR-005` | Battery state of charge | Scalar | Percent | 0 to 100 | `GCS-VERIFIED` standard family | `CONFIRMED` |
| `IF-PWR-006` | Voltage, current, power | Scalar | Volts, amperes, watts | **`UNKNOWN`** for the 480 V domain | `ARTIFACT-OBSERVED` hardware | `PROPOSED` |
| `IF-PWR-007` | Power-system fault | Enumerated | — | **`UNKNOWN`** | `BASELINE-ASSERTED` | `OPEN` |
| `IF-PWR-008` | Estimated remaining energy, where validated | Scalar | **`UNKNOWN`** | **`UNKNOWN`** | `BASELINE-ASSERTED`, conditional | `OPEN` |
| `IF-PWR-009` | Degraded or limited-power mode | Enumerated | — | **`UNKNOWN`** | `BASELINE-ASSERTED` | `OPEN` |

Note `IF-PWR-008`'s baseline wording: "estimated remaining energy **where validated**". The baseline
itself conditions the item on validation, so the ground station must be able to present it as
unavailable rather than computing an estimate of its own.

### Additional items the architecture needs

| ID | State | Rationale | Evidence | Status |
| --- | --- | --- | --- | --- |
| `IF-PWR-010` | Converter state | The tether configuration has an onboard conversion stage whose health is operationally relevant | `ARTIFACT-OBSERVED` hardware; reporting `NOT-EVIDENCED` | `OPEN` |
| `IF-PWR-011` | Watchdog fault | Represented **separately** from power state, because the watchdog acts independently | `MODEL` | `PROPOSED` |
| `IF-PWR-012` | Power capability | Which power configuration this aircraft supports, so the display adapts | `NOT-EVIDENCED` | `OPEN` |

## Contract details

| ID | Direction | Producer | Consumer | Update | Freshness | Ack | Authority |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-PWR-001` to `IF-PWR-004` | Vehicle to GCS | Power system, reported via the flight controller | Power module | `UNKNOWN` | Stamped at parse; stale visible | None | Power system |
| `IF-PWR-005` | Vehicle to GCS | Flight controller | Power module, standard handling | Streamed | Stamped at parse | None | Vehicle |
| `IF-PWR-006` | Vehicle to GCS | Power monitoring via the flight controller | Power module | `UNKNOWN` | Stamped at parse | None | Power system |
| `IF-PWR-007`, `IF-PWR-009`, `IF-PWR-010` | Vehicle to GCS | Power system via the flight controller | Power module, alerting | Event | Stamped at parse | None | Power system |
| `IF-PWR-008` | Vehicle to GCS | Vehicle, where validated | Power module | `UNKNOWN` | Stamped at parse | None | Vehicle |
| `IF-PWR-011` | Vehicle to GCS | Watchdog, reporting path contested | Power module, authority module | Event | Stamped at parse | None | Safety path |
| `IF-PWR-012` | Vehicle to GCS | Flight controller | Capability model | On connection | Current | None | Vehicle |

| ID | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- |
| `IF-PWR-001` | Tether state unknown; not inferred from voltage | `AERORA-GCS-REQ-029` | — | Q-025 | `INTEGRATION` |
| `IF-PWR-002` | Battery presence unknown; not inferred | `AERORA-GCS-REQ-029` | — | Q-025 | `INTEGRATION` |
| `IF-PWR-003` | Active source unknown; **both configurations shown as possible rather than one assumed** | `AERORA-GCS-REQ-029` | — | Q-024, Q-025 | `INTEGRATION` |
| `IF-PWR-004` | Transition state unknown; the GCS initiates no transition and infers none | `AERORA-POWER-REQ-008` | — | Q-026 | `INTEGRATION` |
| `IF-PWR-005` | Standard handling; stale marked | — | — | — | `INTEGRATION` |
| `IF-PWR-006` | Values stale; units from metadata | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-027` | — | Q-024 | `INTEGRATION` |
| `IF-PWR-007` | Fault shown with its reported detail; **never cleared locally** | `AERORA-SAFE-REQ-009` | — | Q-024 | `INTEGRATION` |
| `IF-PWR-008` | Presented as unavailable when not validated; **the GCS computes no estimate of its own** | `AERORA-GCS-REQ-029` | — | Q-024 | `INTEGRATION` |
| `IF-PWR-009` | Degraded mode shown as reported; no limitation inferred | `AERORA-GCS-REQ-029` | — | Q-024 | `INTEGRATION` |
| `IF-PWR-010` | Converter state unknown; shown as unknown | `AERORA-GCS-REQ-029` | — | Q-024 | `INTEGRATION` |
| `IF-PWR-011` | Shown as a distinct event, not as a power state value, and not as the cause of the payload stop | `AERORA-POWER-REQ-007`, `AERORA-SAFE-REQ-011` | — | Q3-004 | `INTEGRATION` |
| `IF-PWR-012` | No power capability reported means the display offers the generic battery model only | `AERORA-MAV-REQ-009` | — | Q-024 | `INTEGRATION` |

## The watchdog fault is separate from power state

`IF-PWR-011` is not a value of `IF-PWR-007` or `IF-PWR-009`. The firmware model shows the watchdog
acting directly and independently — into disarm, and separately into payload emergency stop.

Folding it into a power-state enumeration would imply it is one state in a sequence the system moves
through. It is not. It is an independent cut, and representing it as an enumeration value would also
lose the ability to record what the power state was when the watchdog fired.

The reporting path for the watchdog is **contested between sources** — see
[02-fc-gcs-icd.md](02-fc-gcs-icd.md) and Q3-004.

### No watchdog-to-power relationship is evidenced

Stated explicitly in Phase 3.1, because the absence was previously left to be inferred from the
separation above.

| Candidate relationship | Evidence | Position |
| --- | --- | --- |
| A watchdog fault causes a power source transition | **`NOT-EVIDENCED`** — no baseline document, model description or schematic record states it | The ground station asserts nothing |
| A watchdog fault causes a converter state change | **`NOT-EVIDENCED`** | The ground station asserts nothing |
| A power fault causes a watchdog fault | **`NOT-EVIDENCED`** | The ground station asserts nothing |
| The two may be displayed side by side with their own receive times | `GCS-DECIDED` | Required |

`IF-PWR-011` is carried in this document because the watchdog is reported alongside power state, not
because it acts on power. Baseline 13 places high-voltage isolation, converter protection and tether
fault protection outside the ground station's remit entirely; the ground station "may display
validated power-system status and faults" and nothing more.

The full set of watchdog relationships, each with its own evidence, is in
[18-error-degradation-model.md](18-error-degradation-model.md) under `DEG-08`.

## What the ground station does with power telemetry

| Action | Permitted | Basis |
| --- | --- | --- |
| Display power state, source, voltage, current, faults | Yes | Baseline 49 requires it |
| Log all of it with freshness and source | Yes | `AERORA-COMPLY-REQ-004` |
| Raise operator-facing alerts | Yes | Supervisory role |
| Present a configuration the vehicle has not reported | **No** | Principle 3 |
| Compute a remaining-energy estimate the vehicle has not validated | **No** | Baseline 49 conditions the item on validation |
| Initiate a power transition | **No** | `AERORA-POWER-REQ-008` |
| Act as safety controller for high-voltage isolation, converter protection, grounding, connector interlock or tether electrical fault | **No** | Baseline 13, explicitly |

The last row is the baseline's own exclusion list, reproduced because it is the clearest statement
anywhere of what the ground station is not.

## Deliberately not defined

The directive names four. All four remain undefined, and the reason is that baseline 49 and baseline
13 both require separate engineering and verification before they can be.

| Not defined | Status | Would be resolved by |
| --- | --- | --- |
| Automatic power switching | `OPEN` — Q-026 | A power-system design decision, then bench verification |
| Failover timing | `OPEN` | The same |
| Transition behaviour | `OPEN` — Q-026 | The same |
| Emergency electrical behaviour | `OPEN` | Power-system design; the ground station is excluded from this by baseline 13 |

Phase 3 defines no value, no threshold and no sequence for any of them.

### What the architecture does instead

The power interface is built so that **either answer fits**:

| Property | Consequence |
| --- | --- |
| Active source is a reported enumerated Fact, not a derived one | If switching exists, the vehicle reports the change; if not, the value simply never changes |
| Source transition is its own item | If transitions do not exist, the item is never populated and the display omits it |
| Power capability gates the display | An aircraft that supports only one configuration shows only that one |
| The ground station initiates nothing | True whether or not transitions exist |

That is why Q-026 is not architecture-blocking: the interface accommodates a yes or a no without
redesign.

## Conflicts recorded

| Conflict | Position A | Position B | Authority needed |
| --- | --- | --- | --- |
| Whether the 480 V domain's voltage and current are reported to the ground station at all | Baseline 49 requires voltage, current and power among the ground-station power states | Baseline 13 excludes the ground station from high-voltage safety functions, which could be read as excluding high-voltage telemetry | Power-system and safety owner |

Both positions are preserved. They are reconcilable — displaying a value is not controlling it, and
baseline 13 explicitly permits the ground station to "display validated power-system status and
faults" — but the **scope** of what is reported from the high-voltage side is not settled, and
`IF-PWR-006`'s range is `UNKNOWN` partly for this reason. Recorded as Q3-007.

## Not specified, and why

| Item | Reason |
| --- | --- |
| Power state enumeration members | Q-024; the power architecture is not frozen |
| Fault enumeration members | Q-024 |
| Voltage and current ranges for the high-voltage domain | Q3-007 conflict above |
| Remaining-energy algorithm | The vehicle validates it or it is not presented |
| Transition timing and sequencing | Q-026; baseline 49 requires separate engineering |
| How the ground station learns which configuration is fitted | Q-025 — sensed or operator-declared is itself undecided |

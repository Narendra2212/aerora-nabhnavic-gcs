# Power Implementation

A state abstraction, and a list of behaviours Phase 4 refuses to implement because no evidence
supports them.

## What is implemented

| Artefact | Content | Status |
| --- | --- | --- |
| `AeroraPowerFactGroup` | Fact group class, metadata loading, `handleMessage` override with an empty body | `IMPLEMENTATION-PLANNED` |
| `AeroraPowerModel` | Five separate values: configuration, effective state, reported state, fault, watchdog event | `IMPLEMENTATION-PLANNED` |
| Capability gating | No power capability reported → the generic battery model only | `CONTRACT-ONLY` |
| Standard battery handling | Inherited unchanged | Inherited |

## The five values, kept separate

| Value | Meaning | Source | Status |
| --- | --- | --- | --- |
| **Tether configuration** | Which configuration this aircraft is in | Vehicle-reported capability or state | `CONTRACT-ONLY` |
| **Battery configuration** | Likewise | Vehicle-reported | `CONTRACT-ONLY` |
| **Effective power state** | What the vehicle reports is in force now | Vehicle-reported | `CONTRACT-ONLY` |
| **Reported power state** | The last reported value with its receive time | Vehicle-reported | `IMPLEMENTATION-PLANNED` representation |
| **Power fault** | A reported fault condition | Vehicle-reported | `CONTRACT-ONLY` |

Enumeration members for every one of these are `UNKNOWN`. The power architecture is not frozen —
Q-024 — so the **structure** is implementable and the **content** is not.

### Why effective and reported are separate values

They answer different questions. "What is in force" and "what was last said" diverge the moment a
report goes stale, and the second architectural principle requires the first to be primary. A single
value would force the implementation to choose, and it would choose wrongly under exactly the
conditions that matter.

## What is not implemented, explicitly

The directive's prohibition list, each confirmed.

| Prohibited | Confirmed not done | Why it would be invention |
| --- | --- | --- |
| Automatic source switching | **Not implemented** | The ground station initiates no power transition — `IF-SYS-007` carries nothing |
| Source arbitration | **Not implemented** | Arbitration is a vehicle-side electrical function |
| Diode-OR behaviour | **Not implemented** | Baseline 49 says the schematic "does not, by itself" prove the topology |
| Tether-loss response | **Not implemented** | No evidenced behaviour, and the station is not the safety controller |
| Transition behaviour | **Not implemented** | Q-026; baseline 49 requires separate engineering and verification |
| Power failover | **Not implemented** | Same |
| Remaining-energy estimation | **Not implemented** | Presented only where the **vehicle** validates it — `IF-PWR-008` |

Seven refusals. The absence of an estimation algorithm is the design, not a gap: an energy figure the
ground station computed would be a ground-station opinion presented next to vehicle facts.

## Evidence position

| Item | Evidence | Confidence |
| --- | --- | --- |
| Dual tether and battery input connectors | `ARTIFACT-OBSERVED` via baseline 46 | Present |
| Current and power monitoring hardware | `ARTIFACT-OBSERVED` via baseline 46 | Present |
| Converter topology, source arbitration, diode-OR, tether-loss detection, transition transients | **Explicitly not proven by the schematic** — baseline 49's own words | `OPEN` |
| Whether the high-voltage domain is reported at all | **`NOT-EVIDENCED`** — Q3-007 | `OPEN` |
| Power state and fault enumerations | `NOT-EVIDENCED` — Q-024 | `OPEN` |

Baseline 13 places high-voltage isolation, converter protection, grounding, connector interlock and
tether electrical fault protection **outside** the ground station entirely. The station "may display
validated power-system status and faults" and nothing more.

## The watchdog, and the relationship that does not exist

| Candidate relationship | Evidence | Position |
| --- | --- | --- |
| A watchdog fault causes a power source transition | **`NOT-EVIDENCED`** | The station asserts nothing |
| A watchdog fault causes a converter state change | **`NOT-EVIDENCED`** | The station asserts nothing |
| A power fault causes a watchdog fault | **`NOT-EVIDENCED`** | The station asserts nothing |
| The two may be displayed side by side with their own receive times | `GCS-DECIDED` | Required |

`IF-PWR-011` carries the watchdog event because it is **reported alongside** power state, not because
it acts on power. The implementation consequence is that `AeroraPowerModel` has no field relating the
two — there is nothing to populate it from.

Phase 3.1 made this explicit after finding the absence had been left to inference. Phase 4 carries
the correction into the data model.

## Interface status

| Interface | Subject | Status |
| --- | --- | --- |
| `IF-PWR-001`, `IF-PWR-002` | Battery voltage, current | `IMPLEMENTATION-PLANNED` — standard families |
| `IF-PWR-003` | Active source | `CONTRACT-ONLY` — enumeration unknown |
| `IF-PWR-004` | Source transition | `CONTRACT-ONLY` — **no transition inferred** |
| `IF-PWR-005` | Battery state of charge | `IMPLEMENTATION-PLANNED` — standard |
| `IF-PWR-006` | High-voltage domain | `CONTRACT-ONLY` — Q3-007; range `UNKNOWN` |
| `IF-PWR-007` | Power-system fault | `CONTRACT-ONLY` |
| `IF-PWR-008` | Estimated remaining energy, where validated | `CONTRACT-ONLY` |
| `IF-PWR-009`, `IF-PWR-010` | Degraded mode, converter state | `CONTRACT-ONLY` |
| `IF-PWR-011` | Watchdog event | `IMPLEMENTATION-PLANNED` representation |
| `IF-PWR-012` | Power capability | `CONTRACT-ONLY` |

Three of twelve are implementable now, all of them standard battery handling. That ratio is an honest
measure of how much of the power domain depends on firmware deliverables.

## Test strategy

| Test | Method | Asserts |
| --- | --- | --- |
| Unknown shown as unknown | `UNIT` | Never zero, never a default |
| No transition inferred | `UNIT` | A configuration change in telemetry produces no transition event |
| No station-initiated power action | `UNIT` | `IF-SYS-007` — **R3-007 candidate conversion** |
| No energy estimation | `UNIT` | No code path computes remaining energy |
| Watchdog separate from power state | `UNIT` | Separate values; no causal field |
| Effective and reported diverge correctly | `UNIT` | A stale report does not become the effective value |
| Standard battery families | `INTEGRATION` | Inherited behaviour unbroken |
| Aerora power family parse | `BENCH` | **Deferred** — no dialect |
| Transition behaviour | `BENCH` | **Deferred** — Q-026 requires separate engineering |

# Payload Implementation

The abstraction boundary, and nothing else. **No payload behaviour is implemented in Phase 4.**

## The boundary

```text
Payload controller (aircraft)        owns actuator behaviour, local interlocks,
        │                            emergency behaviour  —  NOT the ground station
        ▼  reports
MAVLink telemetry, via the flight controller
        │
        ▼  parsed, stamped at parse
AeroraPayloadFactGroup               state representation only
        │
        ▼  gated by capability
M-Capability                         no capability set → no controls at all
        │
        ▼
GCS UI                               display and operator-originated requests
```

The arrow that does not exist: nothing from the ground station reaches an actuator. `IF-PAY-015` and
`IF-SYS-006` both specify that boundary as empty, and Phase 4 implements them as assertions.

## What is implemented

| Artefact | Content | Status |
| --- | --- | --- |
| `AeroraPayloadFactGroup` | A fact group class with metadata loading and a `handleMessage` override | `IMPLEMENTATION-PLANNED` — body empty, no dialect |
| `AeroraPayloadModel` | State representation: identity, capabilities, health, safety state, emergency-stop state as **separate** values | `IMPLEMENTATION-PLANNED` |
| Capability gating | No capability set reported → no payload surface presented | `CONTRACT-ONLY` — Q-023 |
| Command submission path | An operator-originated payload command, tracked through the eight-state model | `CONTRACT-ONLY` — no command set |
| Prohibition assertions | No ground-station path to actuators; no autonomous payload action | `IMPLEMENTATION-PLANNED` |

## What is not implemented, explicitly

| Prohibited | Confirmed not done |
| --- | --- |
| Payload state enumeration members | **None defined** |
| Actuator commands | **None defined** |
| Painting pump control | **Not implemented** |
| Painting nozzle control | **Not implemented** |
| Gimbal control | **Not implemented** |
| Payload safety behaviour | **Not implemented** — the ground station is not the safety controller |
| Emergency-stop semantics | **Not implemented** — the request is not the stop |
| Payload state retention or replay | **Not implemented** — `NOT-EVIDENCED` |
| Calibration procedures | **Not implemented** — Q-043 |

## State representation rules

Each is a rule about what the ground station may say, not about what the payload does.

| Rule | Consequence | Requirement |
| --- | --- | --- |
| Payload state is **reported**, never inferred | No ground-station model of payload state exists | `AERORA-PAYLOAD-REQ-006`, `AERORA-GCS-REQ-029` |
| Payload state is never inferred from a command the station sent | A sent command changes the **command** state, not the payload state | `IF-PAY-003` |
| Effectiveness requires a reported state change | An acknowledgement is not an effect | `AERORA-PAYLOAD-REQ-011` |
| Health unknown is shown as unknown, **not as healthy** | The dangerous default | `IF-PAY-004` |
| Safety state is never inferred | And no payload condition reaches a flight command | `AERORA-PAYLOAD-REQ-010` |
| Emergency-stop state is separate from payload state | Not an enumeration member of it | `AERORA-SAFE-REQ-011` |
| The emergency stop's initiating source is shown **only where reported** | And that field is itself `NOT-EVIDENCED` | `IF-PAY-006` |
| Diagnostics are logged and surfaced, never parsed to derive state | A diagnostic string is not a state source | `IF-PAY-012` |

### Why the stop is not a state member

Folding the emergency stop into the payload state enumeration would imply it is one state in a
sequence the payload moves through. It is an **independent cut**. It would also make it impossible
to record "the payload was in state X when the stop fired", which is exactly what a debrief needs.

## The watchdog relationship, as corrected in Phase 3.1

| Relationship | Aircraft side | Reported to the station | Station may display it |
| --- | --- | --- | --- |
| Watchdog → payload emergency stop | `MODEL` via baseline 13, `PROPOSED` | **`NOT-EVIDENCED`** | **No** — show both, assert no link |
| Watchdog → disarm | `MODEL` via baseline 13, `PROPOSED` | **`NOT-EVIDENCED`** | **No** |
| The initiating-source field | — | `NOT-EVIDENCED` | Only if it ever exists |

Baseline 13 records the paths as **design inputs, not proof of completion**, and directs that each
be converted into a formal requirement and verified against real hardware. None has been. The
implementation consequence is that `AeroraPayloadModel` has **no causal field** linking the stop to
anything — there is nothing to populate it from.

## Payload configuration

| Route | Preference | Reason |
| --- | --- | --- |
| Payload-controller-owned, reported and set through the payload interface | **Preferred** | Keeps the downward boundary clean |
| A MAVLink parameter on a non-autopilot component | Possible, second-class | Parameter handling assumes the autopilot is component 1 |

If configuration must be parameter-backed, it is a Fact like any other — the all-parameters-are-Facts
principle has no payload exemption. Whether the payload controller is a distinct component at all is
Q3-006.

## Interface status

| Interface | Subject | Status |
| --- | --- | --- |
| `IF-PAY-001` | Payload identity and version | `IMPLEMENTATION-PLANNED` |
| `IF-PAY-002` | Payload capability set | `CONTRACT-ONLY` |
| `IF-PAY-003` | Payload state | `CONTRACT-ONLY` |
| `IF-PAY-004`, `IF-PAY-005` | Health, safety state | `IMPLEMENTATION-PLANNED` representation; content `CONTRACT-ONLY` |
| `IF-PAY-006` | Emergency-stop state | `IMPLEMENTATION-PLANNED` representation |
| `IF-PAY-007` | Payload telemetry | `IMPLEMENTATION-PLANNED` class; body `CONTRACT-ONLY` |
| `IF-PAY-008` to `IF-PAY-012` | Configuration, command, stop request, calibration, diagnostics | `CONTRACT-ONLY` |
| `IF-PAY-013` | Actuator interface | `IMPLEMENTATION-PLANNED` as a prohibition assertion |
| `IF-PAY-014` | State retention across interruption | `CONTRACT-ONLY` — `BENCH` |
| `IF-PAY-015` | Ground station to payload actuators | `IMPLEMENTATION-PLANNED` as a prohibition assertion |

Two of fifteen are implementable as **assertions that nothing exists**. R3-007 named `IF-PAY-015` as
one of the two highest-priority conversions from inspection to test, and Phase 4 adopts that.

## Test strategy

| Test | Method | Asserts |
| --- | --- | --- |
| No actuator path | `UNIT` | **R3-007 conversion.** No code path reaches a payload command from a flight-control context |
| No autonomous payload action | `UNIT` | The station initiates no payload command without an operator act |
| State never inferred from a sent command | `UNIT` | Sending a command changes command state only |
| Health unknown is not healthy | `UNIT` | — |
| Stop is distinct from state | `UNIT` | Separate values, separate signals |
| Stop is not presented as a watchdog consequence | `UNIT` | No causal field exists to populate |
| Capability absent → no controls | `INTEGRATION` | — |
| Payload message parse | `BENCH` | **Deferred** — no dialect |
| Retention across resume | `BENCH` | **Deferred** — needs a payload rig |

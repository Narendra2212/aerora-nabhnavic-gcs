# 16. Dual power: the tether and battery contract, as the firmware actually states it

**Status:** evidence record, raised 2026-11-27. Nothing here is a decision. Every value is quoted
from the flight-controller project with a file and line, and every value the **firmware itself**
marks `PLACEHOLDER` is carried through as a placeholder rather than promoted.

**Why this document exists.** [08 §2](08-risk-register-and-open-decisions.md#2-open-decisions)
records the battery-reserve and `Tether_Interlock` set as *orphaned* — "recorded as held by
`DG-PAY`, never actually its Decision_Content, and that gate has closed", needing "a fresh decision
thread, not a gate". The reason it could not be opened was that no evidence source existed. One
exists: the `Aerora_FC_RevB_H743` project, whose `PARAM_TABLE.md` is **generated** from the model by
`harness_scripts/gen_param_table_md.m`, which makes it the aircraft's own statement rather than a
description of it.

**Scope.** Power only. The paint/spray payload is deliberately excluded.

## 1. Source of evidence

| Artifact | Role |
| --- | --- |
| `PARAM_TABLE.md` | Generated parameter table — names, defaults, units, min, max, owning subsystem |
| `PCB_COMPATIBILITY_20261114.md` | The Rev B battery-sensing rework (INA228), newest power document |
| `FC_Connectors.md` | Physical power inputs |
| `BUILD_READINESS.md` | Flight preconditions and what cannot be done in software |
| `CONTROL_ARCHITECTURE.md` | Failsafe table |
| `REQUEST5_REPORT.md`, `VALIDATION_REPORT.md` | What has been exercised in SIL |
| `MAVLINK_TELEMETRY.md` | What actually reaches a ground station |

The model file of record is **`motor_test_1__CURRENT.slx.BACKUP_BATTINA228_20261114`**, not
`motor_test_1__CURRENT.slx`. The latter is byte-identical to `motor_test_1__CURRENT.slx.r2026a`
(SHA-256 `58F142DD…`), is an R2026a downgrade export, and contains **none** of
`MAVLink_Telemetry`, `mav_link`, `Flash_Manager`, `aerora_param`, `Event_Encoder` or
`Vertical_Controller`. Any reading taken from the file named `CURRENT` would be a reading of a
gutted model.

## 2. The two tether parameters, and why there are two

There is no single "tethered" setting. There are two independent flash parameters with unrelated
effects, each set by hand over MAVLink while disarmed.

| Parameter | Default | Range | Owner | Stated effect |
| --- | --- | --- | --- | --- |
| `BAT_TETHERED` | — (params 273-277 batch) | bool | `Battery_Pack_Monitor` | "1 = tether power: no battery sensor needed, **no battery failsafe**, SoC unknown" (`PCB_COMPATIBILITY_20261114.md:18`) |
| `TETHER_MODE` (205) | `false` | 0..1 | `Offboard_Gateway` | "tethered operation: fence radius = `FENCE_TETHER_RADIUS_M`" (`PARAM_TABLE.md:203`) |
| `FENCE_TETHER_RADIUS_M` (204) | `30` m | 2..200 | `Offboard_Gateway` | "tethered-mode fence radius = tether length **(PLACEHOLDER)**" (`PARAM_TABLE.md:202`) |

`BAT_TETHERED` is a **power** statement. `TETHER_MODE` is a **navigation** statement. Nothing in the
firmware or the hardware ties them together.

### 2.1 This is the `Tether_Interlock` the register was missing

The interlock is not a threshold and not a transition. It is a **consistency requirement between two
operator-set parameters that no component enforces**:

- `BAT_TETHERED = 1` with `TETHER_MODE = 0` — the aircraft is on tether power with **no battery
  failsafe**, while the fence still uses the untethered radius. Flight beyond tether length is not
  prevented by the fence, and the energy failsafe that would otherwise end the flight is disabled.
- `BAT_TETHERED = 0` with `TETHER_MODE = 1` — the fence is clamped to tether length while the
  battery failsafe is live and SoC is being estimated. Less dangerous, and still a misconfiguration.

Both states are reachable, neither is rejected, and **both are invisible today** because no ground
station reads either parameter. This is the first dual-power item that is a real safety surface
rather than a display.

## 3. What the aircraft can and cannot tell a ground station

This is the constraint that shapes the whole feature, and it is the opposite of what a dual-power
display would assume.

`MAVLINK_TELEMETRY.md`'s outbound set is `HEARTBEAT`, `SYS_STATUS`, `ATTITUDE`,
`GLOBAL_POSITION_INT`, `GPS_RAW_INT`, `BATTERY_STATUS`, `VFR_HUD`, `STATUSTEXT`, `PARAM_VALUE` and
`COMMAND_ACK`. **There is no tether message, and no tether field in any of them.**

| Question | Answerable? | How |
| --- | --- | --- |
| Is the aircraft *configured* for tether power? | **Yes** | `BAT_TETHERED` via `PARAM_VALUE` |
| Is the aircraft *configured* for a tether fence? | **Yes** | `TETHER_MODE`, `FENCE_TETHER_RADIUS_M` |
| Is a tether physically attached? | **No** | No telemetry, and no sense channel — see §4 |
| Is an attached tether supplying power? | **No** | Same |
| Has the aircraft switched source in flight? | **No** | Nothing reports a transition |

So `AeroraPowerModel::tetherConfiguration()` (`IF-PWR-001`, attachment) and `tetherSupply()`
(`IF-PWR-013`) have **no producer on this aircraft** and must stay never-observed. Their existing
documentation already forbids inferring them from a voltage reading or from traffic stopping, which
is exactly right and is now load-bearing rather than cautionary.

What *is* newly answerable is **configuration**, which is a different item from either of those and
has no slot in the model today.

## 4. Three discrepancies between the GCS baseline and Rev B

Each needs an owner. None is mine to resolve.

### 4.1 RESOLVED — the high-voltage domain is upstream of the flight controller

Settled by the programme owner on 2026-11-27: the supply is **800 V** DC from the ground, through
the tether, into an **onboard DC-DC converter** that outputs 48 V, with the battery carried for
**safe landing**. Baseline 49's "480 V" is a stale figure and should read 800 V.

This removes the apparent contradiction rather than choosing a side. The 800 V never reaches the
flight controller:

```text
ground 800 V DC ──tether──► onboard DC-DC converter ──48 V──► J10  "pack input B (tether)"
                                                                    │ F2 ─ D4 ─┐
battery (safe-landing reserve) ─────────────────────────────────────► J9       ├─► VIN_RAW
                                                                    │ F1 ─ D3 ─┘
```

`FC_Connectors.md:332` describes the converter's **output**, not the tether: `J10 | 1 GND, 2 → F2 →
D4 → VIN_RAW | pack input B (tether)`. Both sources arrive as ~48 V pack inputs. So the board is
consistent with the 800 V architecture, and the correction needed is to baseline 49's number.

**The consequence is the finding.** The flight controller has **no sensing whatsoever on the 800 V
domain** — no voltage, no current, no converter state, no tether continuity. `IF-PWR-006`'s
high-voltage scalars and `IF-PWR-010`'s converter state therefore have no producer *in principle*
on this aircraft, not merely *in practice*. `Q3-007` — whether the high-voltage domain is reported
to the ground station — is answerable for this airframe: **not through the flight controller.** It
could only come from the DC-DC converter's own telemetry or from ground-side equipment, neither of
which has an evidenced path to the station today.

### 4.2 The sources are diode-OR'd, so failover is passive, automatic and silent

Two pack inputs, each fused and diode-isolated onto one rail, is a diode-OR. `AeroraPowerModel`'s
seven refusals include "no diode-OR behaviour" and `kPowerArbitrationImplemented = false`. Those
refusals stay **correct** — arbitration is an electrical function and the station must not model it
— but the justification changes, and so does what the station can expect to observe.

Failover needs no logic: if the tether or converter stops supplying, the battery takes the rail
through its diode, with no firmware involvement. That is the right design for a safe-landing
reserve. It is also **silent**. Nothing reports a transition (`IF-PWR-004` has no producer, §3), so
the station cannot tell that the tether has failed and the aircraft is now flying on its reserve.

The one quantity that moves is the `VIN_RAW` bus voltage the INA228 measures after the diodes. It
is **not** a reliable discriminator: a 48 V converter output sits inside a 14S pack's own
46.2–58.8 V range, so the two sources are not separable by rail voltage alone. This is very likely
why `BAT_TETHERED` is a manually set parameter rather than something the firmware detects. The
station must not attempt the inference either.

### 4.4 The safe-landing reserve is unmonitored exactly when it is the reserve

This follows from §4.1 and §2 together, and it is the most consequential item in this document.

The battery exists to land the aircraft safely if the tether fails. But `BAT_TETHERED = 1` tells the
firmware "tether power: no battery sensor needed, **no battery failsafe**, SoC unknown"
(`PCB_COMPATIBILITY_20261114.md:18`). So during tethered flight — the whole time the battery is
serving as a reserve rather than as the supply — its state of charge is **not** being tracked and
no energy failsafe is armed.

Combined with §4.2, the failure sequence is:

1. Tether or converter stops supplying. Nothing is reported.
2. The battery silently takes the rail through its diode.
3. The aircraft is now flying on a reserve whose charge nobody has been monitoring.

The station cannot detect step 1, cannot report step 2, and cannot say in step 3 how much reserve
remains. Whether that is acceptable is a product and safety-engineering judgement, not a ground
station one — but the station's obligation is clear either way: **while `BAT_TETHERED = 1` it must
state that reserve capacity is unknown**, rather than showing a stale, zero or last-known SoC that
would read as a live reserve figure. That is §6 item 4, and this section is why it is not cosmetic.

### 4.3 Two firmware documents disagree on whether battery sensing works

- `BUILD_READINESS.md:103`: battery sensing "**cannot be done in software**" — Rev B has no
  pack-voltage or pack-current ADC channel — and therefore "`BENCH_MODE` must stay 1 and **there is
  no battery failsafe**: do not fly untethered without an external battery alarm".
- `PCB_COMPATIBILITY_20261114.md:18` (later): the board has an INA228 at `0x40`,
  `FC_Power_Monitor` now reads it, `Battery_Pack_Monitor` derives SoC and remaining mAh, and the
  capacity failsafe is live at SoC ≤ 20 %.

The later document appears to supersede the earlier one, but `BUILD_READINESS.md` is the flight
preconditions document and still carries the prohibition. **A ground station must not present a
battery failsafe as live while the aircraft's own readiness document says it is not.** Which
document governs is a firmware-side call.

## 5. Battery values, as stated

Carried for traceability. The station defines none of these and enforces none of them.

| Quantity | Value | Source |
| --- | --- | --- |
| Pack | 14S, 58.8 V max | `PCB_COMPATIBILITY_20261114.md:18` |
| Critical voltage | ≤ 3.30 V/cell (46.2 V) for 3 s → `Bat_Crit` → **LAND**, held until disarm | `CONTROL_ARCHITECTURE.md:231`, `:77` |
| Overcurrent | > `PLACEHOLDER_OVERCURRENT_A` **250 A** for 1 s | `CONTROL_ARCHITECTURE.md:232` |
| Capacity failsafe | used mAh ≥ 0.8 × `PLACEHOLDER_PACK_MAH` (**30 000 mAh, to be confirmed**) | `CONTROL_ARCHITECTURE.md:233` |
| Capacity failsafe (restated) | SoC ≤ 100 × (1 − `BAT_CAPACITY_FS_FRAC`) = 20 % | `PCB_COMPATIBILITY_20261114.md:18` |
| `BAT_DIODE_DROP_V` | 0.4 V | same |
| `BAT_I_FULL_A` | 250 A | same |
| `BAT_I_EXP` | 2.5 | same |
| `BAT_SOC_GAIN` | 0.002 /s | same |
| `BATTERY_STATUS.type` | 1 LIPO **(PLACEHOLDER)** | `MAVLINK_TELEMETRY.md` |

### 5.1 Pack current is estimated, not measured

`Battery_Pack_Monitor` "uses the INA228 bus voltage + `BAT_DIODE_DROP_V` and **ESTIMATES** pack
current from the six motor commands (R8 carries only the FC electronics, about 0.6 A, not the
motors)" (`PCB_COMPATIBILITY_20261114.md:18`).

So `BATTERY_STATUS.current_battery`, `current_consumed` and `battery_remaining` on this aircraft are
**derived from commanded motor output**, not from a current sensor. QGroundControl's
`BatteryFactGroupListModel` will present them as measurements, because that is what the message
means everywhere else. The distinction is exactly what `AeroraReportedValue`'s `basis` field exists
for, and it is the honest-reporting item of this whole document: a number that looks measured and is
modelled.

## 6. What is implementable now, and what is not

Implementable, with no value authored by the station:

1. **Configured power source** — read `BAT_TETHERED` through the Fact System and report it as
   configuration, not as attachment or supply.
2. **Configured fence mode** — read `TETHER_MODE` and `FENCE_TETHER_RADIUS_M`, carrying the
   firmware's own `PLACEHOLDER` annotation on the radius.
3. **The §2.1 interlock** — report when `BAT_TETHERED` and `TETHER_MODE` disagree. Report only; the
   station commands nothing.
4. **The battery-failsafe consequence** — when `BAT_TETHERED = 1`, state that SoC is unknown and no
   battery failsafe is armed, rather than showing a stale or zero SoC.
5. **Basis text for battery values** — record that current and SoC are estimated from motor
   commands, per §5.1.

Not implementable, and must stay never-observed:

- Tether attachment (`IF-PWR-001`) and tether supply (`IF-PWR-013`) — no producer, §3.
- Source transition (`IF-PWR-004`) — nothing reports one, and §4.2 explains why: failover is a
  passive diode hand-off with no firmware involvement to report.
- High-voltage scalars (`IF-PWR-006`) and converter state (`IF-PWR-010`) — §4.1 establishes these
  are beyond the flight controller's sensing *in principle*, not merely unreported.
- Degraded mode (`IF-PWR-009`) and remaining energy (`IF-PWR-008`) — unchanged; the station
  computes no energy estimate, and under §4.4 it has no reserve figure to estimate from.

## 7. Owners

| Item | Needs | Owner |
| --- | --- | --- |
| ~~§4.1 topology~~ | **RESOLVED 2026-11-27**: 800 V ground supply, onboard DC-DC to 48 V, battery for safe landing. Baseline 49's "480 V" needs correcting to 800 V | Architecture |
| **§4.4 unmonitored reserve** | Whether tethered flight may proceed with the safe-landing reserve's charge untracked and its failsafe disarmed. The station will report the unknown either way | **Product + safety** |
| §4.3 battery sensing — which document governs | Whether a battery failsafe may be presented as live | Firmware |
| §2.1 interlock — report only, or also refuse to arm | Product behaviour; the station has no arming authority anyway (`COMMAND_LONG` 400 `param1=1` → ACK 2 DENIED) | Product |
| `FENCE_TETHER_RADIUS_M`, `PLACEHOLDER_OVERCURRENT_A`, `PLACEHOLDER_PACK_MAH`, `BATTERY_STATUS.type` | Confirmation; all four are the firmware's own placeholders | Firmware + product |
| 800 V domain telemetry | Whether the DC-DC converter or ground equipment reports anything, given §4.1 puts it beyond the flight controller's sensing | Architecture + firmware |

None of these blocks §6's implementable list, because every item there reports a parameter the
aircraft already publishes or states a consequence the firmware already documents. §4.4 in
particular is **more** buildable for being unresolved: reporting that reserve capacity is unknown is
correct under either product answer.

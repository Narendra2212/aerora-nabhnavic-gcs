# Payload, Tether and Gimbal Item Sets

The content `DG-PAY` waits on, derived from the firmware model rather than composed. This document
is what Product and engineering would sign; writing it does not close the Gate.

## Status

| Item | Status |
| --- | --- |
| This document | `PROPOSED` -- `DG-PAY` remains `OPEN` on all four parts |
| Item names and units | Derived from the firmware model, named below with the signal each came from |
| Operating limits set here | **NONE.** Every numeric value is recorded as configurable and unset |
| Reporting messages | **NOT DERIVABLE from this model.** See section 2, and it is why the Gate cannot close on this document alone |
| Aerora MAVLink messages introduced | **Zero.** The model references common-dialect messages only |

## 1. Where this comes from

`motor_test_1__CURRENT.slx`, read directly. An SLX file is a ZIP of XML and JSON, so the contents
are quotable rather than summarised: 366 entries, including `simulink/blockDiagram.json`,
`simulink/systems/system_*.xml` and 40-odd `simulink/stateflow/chart_*.xml`.

What was searched for and found, with counts, so the basis of each item below is checkable:

| Term | Occurrences |
| --- | --- |
| `spray` | 131 |
| `payload` | 81 |
| `gimbal` | 46 |
| `paint` | 28 |
| `pump` | 22 |
| `tether` | 13 |
| `nozzle` | 11 |

Terms searched for and **absent**: `consumable`, `applicator`, `sprayer`, `winch`. The consumable
part of `DG-PAY` therefore gets nothing from this model, and no consumable item is proposed below.

## 2. What the model does not supply, and why that matters

Two gaps, and both are recorded rather than filled.

### The values are placeholders in the model itself

`PLACEHOLDER` appears **238 times across 67 distinct tokens**. The model declares parameter names
and leaves them unbound: there is no data dictionary, no `InitFcn` and no `.m`, `.mat` or `.sldd`
reference anywhere in the archive, so values are supplied from the MATLAB base workspace at
simulation time and are not in the file.

Among them, directly over the ground `DG-PAY` covers:

| Placeholder | Would have supplied |
| --- | --- |
| `PLACEHOLDER_NOZZLE_POS_M` | Nozzle geometry |
| `PLACEHOLDER_CAPACITY_FS_MAH`, `PLACEHOLDER_PACK_MAH` | Battery capacity |
| `PLACEHOLDER_OVERCURRENT_A`, `PLACEHOLDER_LOW_V_CELL`, `PLACEHOLDER_CRIT_V_CELL` | Power value ranges |
| `PLACEHOLDER_SERVO_PWM_RANGE`, `PLACEHOLDER_THR_MIN_US` | Actuator ranges |
| `PLACEHOLDER_UNTUNED` (12 occurrences) | Controller gains |
| `PLACEHOLDER_EXT_BINDING` (7 occurrences) | External bindings |

This is the firmware authors recording what they have not yet determined. Transcribing those into
a Closure_Record as settled values would launder somebody elses open question into a closed Gate,
which is worse than inventing a number, because the invention would at least be visible as mine.
Requirement 4 criterion 10 is explicit that an unsettled value is left **absent** rather than
carrying a placeholder in its place.

### The reporting message is not in the model

`reportingMessage` is a **required** attribute of the payload and power parts. The model cannot
supply it. Its payload signals -- `Spray_Rate`, `Spray_On_Off`, `Payload_EStop` and the rest -- are
internal Simulink signals inside the plant and control loops, not wire messages. Every MAVLink
reference in the archive is common-dialect: `HEARTBEAT`, `SYS_STATUS`, `BATTERY_STATUS`, `ATTITUDE`,
`GLOBAL_POSITION_INT`, `COMMAND_LONG`, `COMMAND_ACK`, `PARAM_SET`, `PARAM_VALUE`,
`PARAM_REQUEST_LIST`, `PARAM_REQUEST_READ` and `DO_SET_ACTUATOR`.

So this model answers *what the payload has* and not *how the aircraft reports it*. One attribute
per item is the remaining gap, and it is the whole reason the Gate stays open on this document.

A side effect worth recording: this is independent corroboration of the accepted `DG-MIS` record,
which settles that Aerora introduces zero MAVLink commands. A firmware model built for this
airframe references no Aerora message either.

## 3. Reported items proposed for the payload part

These are the candidates for `DG-PAY:payload`, which asks for the **reported** item set. Each row
names the model signal it came from, so the proposal is traceable rather than asserted.

`SOC-01` requires every item to be vehicle-reported. Each of these is a signal the flight code
computes or drives, so each is reportable in principle; whether the aircraft actually puts it on
the wire is the `reportingMessage` question section 2 leaves open.

| Proposed item | Model signal | Unit | Value range | Enumeration members | Reporting message |
| --- | --- | --- | --- | --- | --- |
| `sprayEnabled` | `Spray_On_Off`, `Spray_On_Off_In` | none, it is an enumeration | n/a | `Off`, `On` | **not evidenced** |
| `sprayRate` | `Spray_Rate`, `Spray_Rate_In` | **not evidenced** -- the signal name carries no unit suffix | configurable, unset | n/a | **not evidenced** |
| `sprayPumpCommand` | `Spray_Pump_Control` | **not evidenced** | configurable, unset | n/a | **not evidenced** |
| `sprayGateState` | `Spray_Gate` | none, it is an enumeration | n/a | **not evidenced** -- the chart states were not enumerated | **not evidenced** |
| `payloadEmergencyStop` | `Payload_EStop` | none, it is an enumeration | n/a | `Inactive`, `Active` | **not evidenced** |

## 4. Reported items proposed for the gimbal

Recorded here because the model carries them, and flagged because they may not belong to `DG-PAY`
at all: a gimbal is a camera concern and upstream already models one. Whether these become Aerora
items or are left to upstream `CAMERA_*` handling is a decision this document raises and does not
take.

| Proposed item | Model signal | Unit | Value range | Reporting message |
| --- | --- | --- | --- | --- |
| `gimbalPanTarget` | `Gimbal_Pan_Target` | **not evidenced** | configurable, unset | **not evidenced** |
| `gimbalTiltTarget` | `Gimbal_Tilt_Target` | **not evidenced** | configurable, unset | **not evidenced** |

The model also carries an annotation on `Gimbal_Range_Min` reading *was miswired (command on up)*.
It is quoted because it is a fixed defect in the firmware and not a specification, and because it
is the kind of note that should not be mistaken for an item.

## 5. The tether part

The model carries one tether signal, `TETHER_MODE`, and no tether measurement of any kind.

`DG-PAY:power` asks for the reported **tether item set** against `P7-PWR-006` -- the quantities a
tether reports, such as supply voltage, current or temperature. A mode selector is not one of those.
So this model supplies **nothing** for the power part, and `IF-PWR-013`, the tether supply interface
already recorded in the power ICD, remains the only shape there and is still `PROPOSED`.

No consumable item is proposed. Section 1 records that the term appears nowhere in the model.

## 6. Operating limits are configurable and are not set here

The model names a set of limits whose **units are derivable from the names themselves**, which is
the one place it supplies more than signal identity. These are configuration rather than reported
items, so they belong to the applicator ICD and to the mission item parameter set rather than to
the payload item set, and they are listed so the unit evidence is not lost.

| Model parameter | Unit, from the name | Value |
| --- | --- | --- |
| `SPRAY_DIST_MIN_M`, `SPRAY_DIST_MAX_M` | m | configurable, unset |
| `SPRAY_MAX_TILT_DEG` | deg | configurable, unset |
| `SPRAY_REARM_S` | s | configurable, unset |
| `WALL_MIN_M`, `AUTO_ALT_MIN_M`, `AUTO_MAX_JUMP_M` | m | configurable, unset |
| `AUTO_SP_TIMEOUT_S` | s | configurable, unset |
| `AUTO_SP_MIN_HZ` | Hz | configurable, unset |
| `SPRAY_MAX_VEL`, `AUTO_MAX_VEL`, `AUTO_MAX_ACC` | **not evidenced** -- no unit suffix | configurable, unset |

Every one is carried the way the surface-coverage parameters are carried: a `FactMetaData` entry
with a unit and no `min`, `max` or `default`, so the value is operator-configurable and starts
unset. `SPRAY_DIST_MIN_M` and `SPRAY_DIST_MAX_M` are the standoff bounds the mission item calls
`workingDistance`, and `SPRAY_MAX_TILT_DEG` is a surface-inclination limit, so two of the limit
sources the mission record left open are at least *named* by the firmware now even though they
carry no numbers.

## What this document does not settle

| Open | Owner | Why |
| --- | --- | --- |
| The reporting message for every item above | Firmware | Section 2. The required attribute the model cannot supply, and the reason the Gate stays open |
| Every unit marked not evidenced | Firmware | The signal names carry no suffix and the model annotates none |
| Every value | Product with the applicator ICD | 67 placeholders, unbound in the model |
| The consumable item set and consumption model | Product, engineering | Absent from the model entirely |
| The tether item set | Product, engineering | The model carries a mode selector and no measurement |
| Whether the gimbal belongs to Aerora at all | Product | Section 4 |

## What signing this would and would not do

It would not close `DG-PAY`. Three of the four parts get nothing or almost nothing from this model,
and the payload part is missing a required attribute on every row. Signing it would record the item
*names* and the derived units as settled and leave the Gate open on the message bindings, which is
a smaller open question than the one that exists today and an honest one.

What it would unblock immediately is nothing in code. The payload item set is exactly what `DG-PAY`
holds, so the items reach `aerora/src/` when the Gate closes and not before.

## Status vocabulary

Defined programme-wide in [00-README.md](00-README.md#status-vocabulary). This document redefines
none of them and claims `VERIFIED` for nothing: no part of this has been built, compiled or run.

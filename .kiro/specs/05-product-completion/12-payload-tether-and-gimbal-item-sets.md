# Payload, Tether and Gimbal Item Sets

The content `DG-PAY` waits on, derived from the firmware model rather than composed. This document
is what Product and engineering would sign; writing it does not close the Gate.

## Status

| Item | Status |
| --- | --- |
| This document | `PROPOSED` -- `DG-PAY` remains `OPEN` on all four parts |
| Item names and units | Derived from the firmware model, named below with the signal each came from |
| Operating limits set here | **NONE.** Every numeric value is recorded as configurable and unset |
| Reporting messages | `NAMED_VALUE_FLOAT` and `NAMED_VALUE_INT`, decided by Product and Firmware on 2026-11-05. See section 7 |
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

## 7. The reporting mechanism

**Decision: Aerora values are reported as `NAMED_VALUE_FLOAT` and `NAMED_VALUE_INT`. No Aerora
dialect is created.** Product and Firmware, 2026-11-05.

Section 2 recorded that the model reports no payload value at all: it references only `STATUSTEXT`
and `SET_ACTUATOR` beyond the basics, with no `NAMED_VALUE_FLOAT`, `DEBUG_VECT`, `ESC_TELEMETRY`,
`SERVO_OUTPUT_RAW` or `GIMBAL_DEVICE_*`. So the firmware commands the payload and speaks in text,
and its payload signals never leave the aircraft. There was no message to look up, only a mechanism
to choose.

Both messages are common-dialect: a string name and one scalar. QGroundControl already parses them,
so no identifier is allocated and nothing is forked.

### What this costs, recorded now rather than discovered

The name is the contract. These messages are untyped and unversioned, so renaming a value breaks
every log already recorded under the old name, and nothing in the protocol will say so. Each
carries one scalar, so a unit or an enumeration is convention rather than schema: `sprayEnabled`
travels as a `NAMED_VALUE_INT` whose members are agreed here and asserted nowhere on the wire.

This is the trade that was taken deliberately. A typed Aerora dialect was the alternative and is
what Class B of the telemetry ICD was reserved for, but that ICD records it as a programme rather
than a decision: a controlled fork pinned to an immutable commit, a version reported in telemetry,
a four-axis compatibility matrix, and an allocation discipline in which an identifier is never
reused even for a withdrawn message, because a field recording is forever. The permanence that
discipline protects is the reason it was declined for now: committing identifiers before the item
set has settled would spend it on guesses.

### Why this unblocks the value range as well

`valueRange` is required for every item, and section 2 recorded the model marking its limits as
placeholders. The mechanism resolves that without inventing anything, because the question changes:
the range a `NAMED_VALUE_FLOAT` carries is the IEEE 754 single-precision range, which is a fact
about the message rather than a judgement about the payload.

So each item records the range the message carries and asserts no narrower one. The *operating*
limits -- standoff, clearance, tilt, rate -- stay where they already are, held by the applicator ICD
and reported unperformable by the `GeometryLimits` check until they arrive. A transport range and an
operating limit are different things, and conflating them is what would have turned a placeholder
into a signed value.

## 8. Fact-set scope and enumeration members

Two decisions by Product and Firmware, 2026-11-05, completing what this document can settle.

### Scope: only the groups the firmware model evidences

`DG-TLM` declares facts for the **payload** and **power** groups and records the other four as
carrying no fact yet. The telemetry ICD names six, and declaring all six was rejected for a reason
the ICD itself supplies: the product group is `PLACEHOLDER` and the autonomy group waits on
`DG-AUT`, which has no issued interface document, so a third of the set would have been names with
no firmware behind them. Groups fill in by amendment as firmware adds reporting, which requirement
8 criterion 5 already provides for.

This keeps the rule the ICD is strictest about intact: all six groups are still created at vehicle
construction, because a group appearing later shifts every column of the telemetry record. Carrying
no fact is not the same as not existing.

### Members: appended, never renumbered

With `NAMED_VALUE_INT` the integer is what travels, so a value already written to a flight log keeps
whatever meaning is assigned now. Appending a member is safe; reordering or renumbering silently
rewrites the meaning of records already on disk. That asymmetry is why only the member set that is
actually known gets written.

| Fact | Members | Basis |
| --- | --- | --- |
| `sprayEnabled` | `Off` = 0, `On` = 1 | Binary in the model: `Spray_On_Off` |
| `sprayGateState` | **Recorded exception** | The interlock is a Stateflow chart and its states are not enumerated in the archive |
| `tetherMode` | **Recorded exception** | The model carries `TETHER_MODE` beside `AUTO_*` parameters, which implies an AUTO mode and does not enumerate the set |

Both exceptions are declared facts with unsettled member sets, not omitted facts. The distinction
matters: an operator reading the telemetry explorer sees the item exists and that its vocabulary is
not yet known, rather than not seeing it at all. Guessing the sets was the alternative and was
declined, because a guessed integer becomes permanent the moment firmware sends it once.

# Parameter Write Path and Authorization Boundary Assessment

Owner: **Tab 9 (Fact System / parameter authority)**. Deliverable for
[WP9-A](07-phase-7-work-packages-and-ownership.md#tab-9--fact-system-and-parameter-authority-new):
who can write a vehicle parameter, from where, and where a gate could go.

Status vocabulary: [00-README](00-README.md#status-vocabulary). **Nothing here was compiled, linted
or executed.** `VERIFIED` is not claimed. No gate was added, no write semantics changed, no retry
behaviour altered, and no role, permission, allow-list or parameter classification is proposed.

## Contents

- [1. Executive summary](#1-executive-summary)
- [2. Evidence basis](#2-evidence-basis)
- [3. Source-verified call graph](#3-source-verified-call-graph)
- [4. Write-entry-point inventory](#4-write-entry-point-inventory)
- [5. Bypass and retry analysis](#5-bypass-and-retry-analysis)
- [6. Current coverage versus uncovered paths](#6-current-coverage-versus-uncovered-paths)
- [7. Candidate enforcement locations](#7-candidate-enforcement-locations)
- [8. Recommended future tests](#8-recommended-future-tests)
- [9. Open questions and dependencies](#9-open-questions-and-dependencies)
- [10. Proposed traceability updates](#10-proposed-traceability-updates)
- [11. Handoffs](#11-handoffs)
- [12. Reference index](#12-reference-index)

## 1. Executive summary

Seven findings. Each is traced to a file and line in [§12](#12-reference-index).

1. **There is one in-process funnel for `PARAM_SET`, and it is narrower than expected.**
   `ParameterManager::_mavlinkParamSet()` (`src/FactSystem/ParameterManager.cc:306`) is the only
   production function that builds a `PARAM_SET` carrying a real parameter value. It has exactly two
   callers. A gate placed there would cover every C++ and QML parameter write in
   `src/AutoPilotPlugins/**`, `src/Vehicle/**`, `src/FactSystem/FactControls/**` and the parameter
   editor — the whole of [§4.1](#41-the-fact-funnel-and-its-callers) and
   [§4.2](#42-reachable-from-qml).

2. **The line numbers in the existing documents are still correct, but they name the wrong thing.**
   [30-outbound-command-security-audit.md](../03-implementation-specification/30-outbound-command-security-audit.md)
   records the parameter path as `ParameterManager.cc:319`, `:398`. Both lines are accurate:
   `:319` is `mavlink_msg_param_set_pack_chan()` inside the encoder lambda, `:398` is the
   `SendMavlinkMessageState` that transmits and retries. Neither is a callable entry point. The
   entry point is `_mavlinkParamSet()` at `:306`, and the wire send is
   `SendMavlinkMessageState.cc:46`.

3. **`PARAM_SET` is ungated; "reset every parameter to factory defaults" is gated.** The existing
   `MavCommandQueue::sendWorker()` gate (`src/Vehicle/MavCommandQueue.cc:294`) covers queued
   `MAV_CMD` traffic, so `ParameterManager::resetAllParametersToDefaults()` — which sends
   `MAV_CMD_PREFLIGHT_STORAGE` — passes through it. Setting one parameter does not. This is the same
   shape as the `startCalibration()`/`stopCalibration()` asymmetry already recorded in doc 30, and it
   points the other way: the coarse destructive operation is evaluated, the fine-grained one is not.

4. **Two parameter-write channels are absent from the doc-30 inventory.** A second `PARAM_SET`
   emission at `ParameterManager.cc:1240` (the `_HASH_CHECK` handshake, no retry state machine, not
   routed through `_mavlinkParamSet`), and the camera `PARAM_EXT_SET` path at
   `src/Camera/QGCCameraIO.cc:202`, which has its own fact set, its own ack handling and its own
   retry budget and never touches `ParameterManager`.

5. **Two indirect paths can change parameter values without emitting any parameter message.**
   `MAVLinkConsoleController` forwards unparsed operator text to the vehicle shell
   (`src/AnalyzeView/MAVLinkConsole/MAVLinkConsoleController.cc:161`), where `param set <NAME>
   <VALUE>` is a valid command; and `Vehicle::sendParamMapRC()` (`src/Vehicle/Vehicle.cc:2922`,
   `Q_INVOKABLE` at `Vehicle.h:398`) binds a named parameter to an RC channel, handing continuous
   change authority to a stick. Both are reachable from QML. A gate on parameter messages sees
   neither.

6. **The retry path can transmit after the authorization state changes.** `_mavlinkParamSet()`
   builds a self-contained `QGCStateMachine` with `kParamSetRetryCount = 2` and a 1000 ms ack wait
   (`ParameterManager.h:111`, `:113`). The encoder lambda captures the value by copy at construction
   time and the machine re-enters the send state on timeout
   (`ParameterManager.cc:447-448`). An enqueue-time-only decision would therefore authorize once and
   transmit up to three times, which is the already-`OPEN` authorization-lifetime question applied to
   a path that, unlike the command queue, re-sends by design.

7. **Offline editing and parameter-file download are write-safe by construction.** Facts created by
   `_loadOfflineEditingParams()` (`ParameterManager.cc:1570`) are never connected to
   `_factRawValueUpdated`, so mutating them emits nothing. Inbound values arrive via
   `Fact::containerSetRawValue()` (`src/FactSystem/Fact.cc:198`), which deliberately does not emit
   `containerRawValueChanged` (`Fact.h:179-180`), so a received `PARAM_VALUE` cannot echo back as a
   write. A loaded `.plan` file cannot write a parameter: nothing in `src/MissionManager/**`
   references `parameterManager()`.

**The claim that remains supportable is unchanged.** Per doc 30: *"D-001/Q-029 enforcement is
implemented for queued MAV_CMD traffic."* Parameter writes are outside it.
[`R7-15`](08-risk-register-and-open-decisions.md#4-risk-register) is now substantiated rather than
suspected, and the ownership half of `P7-AUTH-003` is discharged. The policy half stays `OPEN`
pending `Q-029` and ADR-014.

## 2. Evidence basis

| Item | Value |
| --- | --- |
| Branch | `aerora/phase7-product-completion` |
| `HEAD` when the trace was taken | `cbe944bb0` |
| `HEAD` when it was re-confirmed | `2d3107a75`. All six intervening commits (U1, U2, U3, Tab 2's audit/status work, Tab 3's setup pages, one docs fix) touch only `aerora/` and the Phase 7 documents — `git diff --name-only cbe944bb0..HEAD -- src/ test/` is **empty**, so every line reference below holds at both commits |
| Working tree | Concurrent uncommitted work by Tabs 1, 2 and 3 under `aerora/`. **All read only; none edited.** Nothing under `src/` or `test/` is modified |
| Method | Source reading plus `Select-String` enumeration, then a second pass re-deriving every cited line number. No compiler, no linter, no test run |
| Host tooling | Unchanged from [00-README § Host tooling](00-README.md#host-tooling). `D-006` still `NO-GO` |
| Document number | This file is **10**. Number 09 was taken mid-pass by Tab 1's [Local Persistence Architecture](09-local-persistence-architecture.md) (WP1-C); renumbering here rather than duplicating a number is the [07 § 3](07-phase-7-work-packages-and-ownership.md#3-conflict-avoidance) rule that the Phase 6 "document 28 twice" collision is not to be reproduced |

Three evidence markers are used below. They sit *under* `VERIFIED`, which requires execution and is
claimed nowhere in Phase 7.

| Marker | Means |
| --- | --- |
| `SOURCE-CONFIRMED` | Read in this checkout at the cited file and line |
| `INFERRED` | Follows from cited source but no execution observed it |
| `NOT-TRACED` | Searched for and not established; stated as such rather than assumed absent |

Line numbers were re-derived this pass rather than carried forward from any earlier document, then
re-confirmed against `2d3107a75`. Every line reference in doc 30's parameter row was re-checked; see
[finding 2](#1-executive-summary).

## 3. Source-verified call graph

`SOURCE-CONFIRMED` throughout. Function names are as they exist, not as earlier documents guessed:
there is no `_writeParameterRaw`, no `_sendParamSetToVehicle`, no `ParameterManager::loadFromJson`
and no `ParameterManager::setDefaultComponentId` in this tree.

```text
                    QML                                   C++
   FactTextField / FactComboBox / FactCheckBox      RadioComponentController
   LabelledFactIncrementer / FactValueSlider        AirframeComponentController
   QGCOptionsComboBox / LabelledFactBrowse          APMSensorsComponentController
   (src/FactSystem/FactControls/*.qml)              ESP8266 / Syslink / Actuators
            │  fact.value = / fact.rawValue =               VehicleObjectAvoidance
            │                                               APMFirmwarePlugin
            ▼                                                     │
   Fact::setCookedValue  Fact.cc:163 ──┐                          │
   Fact::setRawValue     Fact.cc:134 ──┼──────────────────────◄────┘
   Fact::forceSetRawValue Fact.cc:111 ─┘   (no change-check: always emits)
            │
            │  FactMetaData::convertAndValidateRaw(..., convertOnly = true)
            │  FactMetaData.cc:356  — type conversion only; min/max NOT applied
            ▼
   emit Fact::containerRawValueChanged      Fact.cc:126 / :154
            │
            │  connect() at ParameterManager.cc:257  (param discovered via PARAM_VALUE)
            │  connect() at ParameterManager.cc:1897 (param from FTP param.pck)
            ▼
   ParameterManager::_factRawValueUpdated   ParameterManager.cc:536
            │
            │                      ParameterEditorController::sendDiff
            │                      ParameterEditorController.cc:418  (friend access)
            ▼                               │
   ParameterManager::_mavlinkParamSet  ◄────┘      ParameterManager.cc:306
            │
            ├─ paramSetEncoder lambda  :308 ─ mavlink_msg_param_set_pack_chan :319
            ├─ checkForCorrectParamValue lambda (PARAM_VALUE echo match)  :331
            ├─ checkForParamError lambda        (PARAM_ERROR match)       :368
            │
            └─ QGCStateMachine "ParameterManager PARAM_SET"               :397
                 ├─ SendMavlinkMessageState(encoder, kParamSetRetryCount) :398
                 │     └─ SendMavlinkMessageState.cc:45  _encoder(...)
                 │        SendMavlinkMessageState.cc:46  Vehicle::sendMessageOnLinkThreadSafe
                 │                                        → Vehicle.cc:1392 → link
                 ├─ _incrementPendingWriteCount                           :400
                 ├─ WaitForParamResponseState(_waitForParamValueAckMs)    :411
                 │     └─ WaitForParamResponseState.cc:29 on PARAM_VALUE → waitComplete
                 │        WaitForParamResponseState.cc:41 on PARAM_ERROR  → waitFailed
                 ├─ success → _decrementPendingWriteCount → log → final   :439-444
                 ├─ timeout → _decrement → back to SendMavlinkMessageState:447-448
                 └─ error   → _decrement → log → showAppMessage → refresh :451-460
```

### What each stage does and does not do

| Stage | File:line | Behaviour | `SOURCE-CONFIRMED` note |
| --- | --- | --- | --- |
| Fact mutation | `Fact.cc:134` | `setRawValue` converts, compares, emits only on change | Silently does nothing when `_metaData` is null, logging a warning |
| Forced mutation | `Fact.cc:111` | `forceSetRawValue` skips the change check and always emits | Used by `AirframeComponentController.cc:91-92` so the write goes out even when the value matches |
| Validation | `FactMetaData.cc:356` | `convertAndValidateRaw` is called with `convertOnly = true` from both setters | **Every min/max check is gated behind `!convertOnly`.** Range enforcement on the write path is therefore *not* applied here; it lives in the QML controls (`Fact::validate`, `Fact.h:82`) and in the vehicle's `PARAM_ERROR` response |
| Funnel | `ParameterManager.cc:536` | `_factRawValueUpdated` recovers the Fact from `sender()`, warns and returns if the cast fails, then calls `_mavlinkParamSet` | The only argument it has is the Fact — component id, name, type, raw value |
| Transmission | `SendMavlinkMessageState.cc:46` | Encodes on the primary link's channel and calls `Vehicle::sendMessageOnLinkThreadSafe` | Emits `error()` when no link, when no encoder, or when retries are exhausted (`:21`) |
| Acknowledgement | `WaitForParamResponseState.cc:29` | A write is confirmed only by a `PARAM_VALUE` whose component, name and value all match, float compared fuzzily | `ParameterManager.cc:331-366` holds the predicate; a type mismatch logs `qCWarning` and is treated as not-for-us |
| Rejection | `WaitForParamResponseState.cc:41` | `PARAM_ERROR` ends the attempt with no retry; nine `MAV_PARAM_ERROR_*` codes are mapped to strings (`:63-91`) | Includes `MAV_PARAM_ERROR_PERMISSION_DENIED` — the *vehicle* can refuse a write; the station cannot |
| Operator feedback | `ParameterManager.cc:421-426` | `QGC::showAppMessage` on failure, with the `PARAM_ERROR` detail appended when present | |
| Audit | — | **`NOT-TRACED`, because it does not exist.** The path emits `qCDebug(ParameterManagerLog)` only (`:429`, `:433`, `:462`) and two internal signals, `_paramSetSuccess` (`:430`) / `_paramSetFailure` (`:434`) | No `AeroraAuditSink` record, no operator identity, no outcome cause |

## 4. Write-entry-point inventory

### 4.1 The Fact funnel and its callers

Every entry below reaches the wire through `_mavlinkParamSet` at `ParameterManager.cc:306`. All
`SOURCE-CONFIRMED`.

| # | Entry point | File:line | Call path to the wire | Preconditions / validation | QML-reachable | Bypasses the funnel |
| --- | --- | --- | --- | --- | --- | --- |
| E1 | `Fact::setRawValue` | `Fact.cc:134` | → `containerRawValueChanged` → `_factRawValueUpdated:536` → `_mavlinkParamSet:306` | Metadata must exist; type conversion only; value must differ | **Yes** — `Fact.h:54` `rawValue` is a writable `Q_PROPERTY` | No |
| E2 | `Fact::setCookedValue` | `Fact.cc:163` | cooked→raw, then E1 | As E1, plus the cooked translator | **Yes** — `Fact.h:53` `value` | No |
| E3 | `Fact::forceSetRawValue` | `Fact.cc:111` | → `containerRawValueChanged` → E1's funnel | As E1 **minus the change check** | No — not a property or `Q_INVOKABLE` | No |
| E4 | `Fact::setEnumIndex` / `setEnumStringValue` | `Fact.cc:189`, `:181` | → `setCookedValue` → E2 | Index must map to an enum value | **Yes** — `Fact.h:30`, `:32` | No |
| E5 | `ParameterManager::resetAllToVehicleConfiguration` | `ParameterManager.cc:1648` | `getParameter(...,"SYS_AUTOCONFIG")->setRawValue(2)` → E1 | Null-checks the Fact before writing | **Yes**, via `ParameterEditorController.h:162` → `ParameterEditor.qml:59` | No |
| E6 | `ParameterEditorController::sendDiff` | `ParameterEditorController.cc:400` | `:418` calls `_mavlinkParamSet` **directly**; `:423` uses `setRawValue` | `cannotSend` and `load` flags from the parsed file; `noVehicleValue` selects the direct call | **Yes** — `Q_INVOKABLE` `.h:159` → `ParameterDiffDialog.qml:19` | **Yes — skips the Fact layer entirely** |

E6 is the one caller that does not pass through a `Fact`. It is possible because
`ParameterEditorController` is declared `friend class` in `ParameterManager.h:30`, which is how it
reaches a private member. Its purpose is legitimate — a parameter present in a file but absent on the
vehicle has no Fact to write — and it means a boundary placed on `Fact` rather than on
`_mavlinkParamSet` would miss it.

### 4.2 Reachable from QML

`SOURCE-CONFIRMED`. QML never names `setRawValue`; it assigns the `Q_PROPERTY`.

| Surface | File:line | Reaches |
| --- | --- | --- |
| `FactTextField.qml:27` | `fact.value = text` | E2 |
| `FactComboBox.qml:39`, `:41` | `fact.value = …` | E2 |
| `FactCheckBox.qml:21`, `FactCheckBoxSlider.qml:21` | `fact.value = …` | E2 |
| `FactValueSlider.qml:109`, `:119` | `fact.value = …` | E2 |
| `FactTextFieldSlider.qml:118`, `FactTextFieldSlider2.qml:86`, `:95`, `:124` | `fact.value = …` | E2 |
| `LabelledFactIncrementer.qml:39`, `:56` | `fact.value = …` | E2 |
| `LabelledFactBrowse.qml:59` | `fact.rawValue = file` | E1 |
| `QGCOptionsComboBox.qml:87` | `fact.value = …` | E2 |

The first seven live under `src/FactSystem/FactControls/`; `QGCOptionsComboBox.qml` is in
`src/QmlControls/`. Any of them bound to a parameter Fact is a write control. The accessor that hands
a parameter Fact to QML is
`FactPanelController::getParameterFact(int, QString, bool)` —
`src/FactSystem/FactControls/FactPanelController.h:25` — used by every setup page in
`src/AutoPilotPlugins/**/*.qml`.

The higher-privilege QML surface is `ParameterEditorController`
(`src/QmlControls/ParameterEditorController.h:156-165`): `sendDiff` (E6), `resetAllToDefaults`
(`.cc:639` → gated `MAV_CMD`, see [§6](#6-current-coverage-versus-uncovered-paths)) and
`resetAllToVehicleConfiguration` (`.cc:645` → E5).

`AeroraUiSafetyTest` asserts over Aerora's own QML-reachable types. **It does not cover these
upstream surfaces**, and it is not a defect that it does not — `P7-AUTH-004` is scoped to the Aerora
UI. The consequence is that the overlay's QML safety assertion says nothing about parameter writes.

### 4.3 C++ subsystems that write parameters

`SOURCE-CONFIRMED`. All funnel through E1/E3, so all are covered by a boundary at
`_mavlinkParamSet`. Listed because [§2 of doc 30](../03-implementation-specification/30-outbound-command-security-audit.md#path-inventory)
records the path without recording who uses it.

| Subsystem | File:line | What it writes |
| --- | --- | --- |
| Radio calibration | `src/AutoPilotPlugins/Common/RadioComponentController.cc:97`, `:99`, `:128`, `:132`, `:136`, `:170`, `:179` | `RCn_REV`, `RCn_TRIM/MIN/MAX`, `RC_MAP_*`, `RC_CHAN_CNT` |
| Airframe selection | `src/AutoPilotPlugins/PX4/AirframeComponentController.cc:91-92` | `SYS_AUTOSTART`, `SYS_AUTOCONFIG`, both via `forceSetRawValue` |
| Compass calibration | `src/AutoPilotPlugins/APM/APMSensorsComponentController.cc:134`, `:254`, `:709` | `COMPASS_LEARN`; temporarily relaxes then restores the cal-fitness parameter |
| WiFi bridge | `src/AutoPilotPlugins/Common/ESP8266ComponentController.cc:97`, `:99`, `:101`, `:103`, `:131-137`, `:173-179`, `:215-221`, `:262` | SSID/password/baud, packed four bytes per `uint32` parameter |
| Crazyflie radio | `src/AutoPilotPlugins/Common/SyslinkComponentController.cc:34`, `:52-53`, `:64`, `:70-73` | Channel, address, rate; `resetDefaults()` writes all four |
| Actuator/mixer config | `src/Vehicle/Actuators/MotorAssignment.cc:125`, `:132`, `:160`; `Mixer.cc:165`, `:207-209`, `:296`; `Common.cc:54`, `:56`, `:91`, `:93` | Motor-function and bitmask parameters; `Common.cc` uses `forceSetRawValue` |
| Obstacle avoidance | `src/Vehicle/VehicleObjectAvoidance.cc:73`, `:84` | Collision-prevention distance, from the fly view |
| ArduPilot firmware plugin | `src/FirmwarePlugin/APM/APMFirmwarePlugin.cc:1315`, `:1329` | Ground temperature, baro-alt offset |
| Camera mode | `src/Camera/VehicleCameraControl.cc:364` | Camera mode — **on the `PARAM_EXT_SET` path**, see [§4.4](#44-outside-parametermanager) |

### 4.4 Outside `ParameterManager`

These do not pass through `_mavlinkParamSet`. The first two are `PARAM_SET`-adjacent wire traffic; the
last three change parameter values without emitting a parameter message at all.

| # | Path | File:line | MAVLink | In doc 30? | Note |
| --- | --- | --- | --- | --- | --- |
| X1 | `_HASH_CHECK` handshake | `ParameterManager.cc:1240`, inside `_tryCacheHashLoad` (`:1166`) | `PARAM_SET` (#23) | **No** | Writes the pseudo-parameter `_HASH_CHECK` to stop the update stream. Packs with `mavlink_msg_param_set_encode_chan` and sends directly — no state machine, no retry, no pending-write accounting. Not an operator-initiated reconfiguration, but it *is* a `PARAM_SET` on the wire |
| X2 | Camera parameter write | `src/Camera/QGCCameraIO.cc:202` (`sendParameter`, `:371`) | `PARAM_EXT_SET` (#323) | **No** | `QGCCameraParamIO` owns one Fact per camera parameter, sends straight to the camera component, and runs its own ack (`handleParamAck`, `:215`), retry (`kMaxRetries = 3`, `:14`, `:237`) and timer. `ParameterManager` is not involved |
| X3 | MAVLink shell | `src/AnalyzeView/MAVLinkConsole/MAVLinkConsoleController.cc:161` (`sendCommand`, `:38`; `_sendSerialData`, `:135`) | `SERIAL_CONTROL` (#126) | **No** | Unparsed operator text to `SERIAL_CONTROL_DEV_SHELL`. `param set <NAME> <VALUE>` is a valid shell command. `Q_INVOKABLE`, instantiated at `MAVLinkConsolePage.qml:20` and invoked at `:202`, `:242` |
| X4 | Parameter-to-RC binding | `src/Vehicle/Vehicle.cc:2922` (`sendParamMapRC`, packs at `:2940`); `:2956` (`clearAllParamMapRC`, packs at `:2968`) | `PARAM_MAP_RC` (#50) | Yes, but **misattributed** — doc 30 names `setPIDTuningTelemetryMode` (`Vehicle.cc:2754`), which does not pack this message, and gives `:2926-2962`, which excludes the second pack site at `:2968` | `Q_INVOKABLE` (`Vehicle.h:398`, `:401`), called from `RCToParamDialog.qml:16` and `ParameterEditor.qml:85`. Grants an RC channel continuous authority over a named parameter. **Tab 3's file** |
| X5 | `MAV_CMD_DO_SET_PARAMETER` as mission content | `src/MissionManager/MavCmdInfoCommon.json` | inside `MISSION_ITEM_INT` (#73) | Yes, as the mission-item row | `NOT-TRACED` as a live command: searched `src/**`, **no sender exists**. It is selectable as a mission item, so it reaches the vehicle as mission content. That is doc 30's mission row, not a parameter row |

`MockLink` handles inbound `PARAM_SET` (`src/Comms/MockLink/MockLink.cc:1856`) and `PARAM_MAP_RC`
(`:1301`). It is the test link and sends no parameter traffic of its own.

### 4.5 Searched and not found

Recorded so the next pass does not re-derive it. All `NOT-TRACED` in the sense of "searched for,
absent".

| Searched | Result |
| --- | --- |
| `_writeParameterRaw`, `_sendParamSetToVehicle` | Do not exist in this tree |
| `ParameterManager::loadFromJson`, `setDefaultComponentId` | Do not exist. `defaultComponentId` is a read-only sentinel resolved by `_actualComponentId()` |
| `MAV_CMD_DO_SET_PARAMETER` sender in `src/**` | None. Mission metadata only |
| `parameterManager()` in `src/MissionManager/**` | **Zero references.** A loaded `.plan` cannot write a parameter |
| Parameter writes in `src/Joystick/**`, `src/ADSB/**` | None. Joystick writes are `JoystickSettings` app settings |
| Raw parameter sends in `src/Comms/**` | None outside `MockLink` |
| Parameter writes in `aerora/**` production code | **None.** Every hit is documentation prose or a test on an Aerora-local telemetry Fact |

## 5. Bypass and retry analysis

### 5.1 Would a UI-layer check be bypassable?

**Yes. `SOURCE-CONFIRMED`.** Three independent reasons:

1. The QML controls in [§4.2](#42-reachable-from-qml) are generic. A check added to
   `FactTextField.qml` does not apply to `FactComboBox.qml`, and a new control added later inherits
   nothing.
2. Nine C++ subsystems ([§4.3](#43-c-subsystems-that-write-parameters)) write parameters without
   traversing any QML control. Calibration is the clearest case: `RadioComponentController.cc`
   performs seven distinct writes driven by a state machine, not by a bound control.
3. `forceSetRawValue` (E3) exists specifically to defeat a guard — the change check — and is in
   active use at `AirframeComponentController.cc:91-92`. A UI-level predicate has the same
   structural weakness.

### 5.2 Would a check at a public parameter-write API cover all writers?

**Partially, and the gap is specific. `SOURCE-CONFIRMED`.**

| Candidate API | Covers | Misses |
| --- | --- | --- |
| `Fact::setRawValue` + `setCookedValue` + `forceSetRawValue` | E1, E2, E3, E4, E5 and all of §4.2 and §4.3 | **E6** (`sendDiff` calls `_mavlinkParamSet` directly), X1–X4. Also fires for every non-parameter Fact in the application — app settings, telemetry fact groups, mission-item facts — none of which is a vehicle write |
| `ParameterManager::_mavlinkParamSet` | E1–E6, hence §4.2 and §4.3 in full | X1 (`_HASH_CHECK`), X2 (`PARAM_EXT_SET`), X3 (shell), X4 (`PARAM_MAP_RC`) |

`Fact` is the wrong altitude: it is shared with app settings and telemetry, so a check there would
have to re-derive "is this a vehicle parameter" — which is exactly the distinction
`_mavlinkParamSet` already embodies by construction. `_mavlinkParamSet` is the narrowest point that
covers all six entry points.

### 5.3 Can the retry path transmit after the authorization state changes?

**Yes, by design, and more readily than the command queue. `SOURCE-CONFIRMED` for the mechanism,
`INFERRED` for the consequence.**

| Element | File:line | Value |
| --- | --- | --- |
| Retry count | `ParameterManager.h:111` | `kParamSetRetryCount = 2` — so up to **three** transmissions per write |
| Retry enforcement | `SendMavlinkMessageState.cc:20` | `++_runCount > _retryCount + 1` → `error()`. `_runCount` is deliberately not reset on exit |
| Ack wait | `ParameterManager.h:113`, `.cc:43` | 1000 ms in production, 50 ms under unit tests |
| Timeout transition | `ParameterManager.cc:447-448` | `WaitStateBase::timeout` → decrement → **back to `sendParamSetState`** |
| Captured state | `ParameterManager.cc:308` | The encoder lambda captures `componentId`, `paramName`, `valueType`, `rawValue` **by copy** at construction |
| Retry suppression | `ParameterManager.cc:42` | `_disableAllRetries` is set from `_logReplay` only — not from any authorization state |

The window is therefore up to roughly two seconds of wall time after the originating call, during
which the value to be written is already fixed in a lambda and the state machine owns the decision to
re-send. This is the `OPEN` "authorization lifetime: re-evaluate on retry?" question from
[08 § 2](08-risk-register-and-open-decisions.md#2-open-decisions), and it lands differently here than
it does for `MAV_CMD`: the command queue's retry set is deliberately narrow
(`MavCommandQueue::_shouldRetry`, `MavCommandQueue.cc:220`, cases at `:240-243` — four non-actuating
commands), whereas **every** `PARAM_SET` retries.

Three consequences worth recording for whoever closes ADR-014, without choosing between them:

- A decision taken before `_mavlinkParamSet` authorizes an *intent* and the machine executes it up to
  three times.
- A decision taken inside `SendMavlinkMessageState` would be evaluated per transmission, but that
  class is generic — it also carries `PARAM_REQUEST_READ` (`ParameterManager.cc:1086-1088`) and is
  outside `src/FactSystem`, so it is not Tab 9's file.
- `_pendingWritesCount` (`:1928`, `:1936`) and the `pendingWrites` `Q_PROPERTY` (`.h:28`) already
  expose "a write is in flight" to QML. A denial that bypassed the increment would leave that
  property truthful; a denial inserted between increment and send would need to decrement, or the
  counter leaks. The warning at `:1939` makes an unbalanced decrement visible rather than silent.

### 5.4 Can direct MAVLink sends bypass `ParameterManager`?

**Yes. `SOURCE-CONFIRMED`.** X1–X4 in [§4.4](#44-outside-parametermanager). Ranked by how much
authority each confers:

1. **X3, the shell**, is unbounded: it is not a parameter channel at all, it is arbitrary text to a
   firmware console. No parameter-shaped gate can constrain it, and parsing operator text to find
   `param set` would be a guess about firmware grammar.
2. **X2, `PARAM_EXT_SET`**, is a complete parallel implementation of the same concept for camera
   components, with its own facts, acks and retries.
3. **X4, `PARAM_MAP_RC`**, is the subtlest: one message delegates ongoing parameter authority to an
   RC channel. Nothing further is sent by the station.
4. **X1, `_HASH_CHECK`**, is a protocol handshake rather than a reconfiguration, but it is a
   `PARAM_SET` message and a message-type-based boundary will intercept it. Any such boundary has to
   decide whether to let it through, or parameter download breaks.

### 5.5 Which subsystems originate parameter writes?

| Subsystem | Originates a parameter write | Evidence |
| --- | --- | --- |
| Vehicle setup / `AutoPilotPlugins` | **Yes**, heavily | §4.3, six controllers |
| Calibration | **Yes** | `RadioComponentController.cc`, `APMSensorsComponentController.cc` |
| Firmware plugin | **Yes** | `APMFirmwarePlugin.cc:1315`, `:1329`. Also *mutates* outbound `PARAM_SET` in flight: decode `:201`, re-encode `:237`, rewriting `param_type` for ArduPilot. **Any inspection of `PARAM_SET` must account for this happening after `_mavlinkParamSet`** |
| Fly view | **Yes**, narrowly | `VehicleObjectAvoidance.cc:73`, `:84` |
| Camera / payload | **Yes**, on the separate `PARAM_EXT_SET` path | X2 |
| Mission / plan file | **No** | Zero `parameterManager()` references in `src/MissionManager/**` |
| Aerora overlay | **No** | Zero production parameter writes in `aerora/**` |
| Joystick | **No** | App settings only |

## 6. Current coverage versus uncovered paths

The existing gate is `MavCommandQueue::sendWorker()`, `src/Vehicle/MavCommandQueue.cc:281`, with the
authorization call at `:294` and the deny branch at `:297-312`. It is placed before `_list.append()`
so a refusal cannot be retried or later misreported — the rationale is in the source comment at
`:291-294` and in ADR-012. The hook is
`QGCCorePlugin::authorizeMavCommand(...)`, `src/API/QGCCorePlugin.h:148`. There is a second call site
at `src/Vehicle/Vehicle.cc:2400` (`startCalibration`, ADR-015).

**This gate does not protect `PARAM_SET`, and no claim in this document says otherwise.** It is a
`MAV_CMD` queue; `PARAM_SET` is a separate MAVLink message that never enters it. Likewise
`includedResumeCommands` (declared `src/MissionManager/MissionManager.cc:110`, populated from `:113`,
consumed `:134`) is a mission-resume replay list of camera/gimbal/ROI/fence/speed commands, contains
no parameter command, and is **unmodified by this investigation**.

| Operation | Wire | Gated today | Where |
| --- | --- | --- | --- |
| `resetAllParametersToDefaults()` — erase all parameters | `MAV_CMD_PREFLIGHT_STORAGE` | **Yes** | `ParameterManager.cc:1641` → `Vehicle::sendMavCommand` → `MavCommandQueue.cc:294` |
| PX4 sensors factory reset | `MAV_CMD_PREFLIGHT_STORAGE` | **Yes** | `src/AutoPilotPlugins/PX4/SensorsComponentController.cc:424` |
| ESP8266 restore defaults | `MAV_CMD_PREFLIGHT_STORAGE` | **Yes** | `src/AutoPilotPlugins/Common/ESP8266ComponentController.cc:290` |
| Write one parameter | `PARAM_SET` | **No** | `ParameterManager.cc:319` |
| `resetAllToVehicleConfiguration()` | `PARAM_SET` (`SYS_AUTOCONFIG = 2`) | **No** | `ParameterManager.cc:1648` |
| Send a parameter file diff | `PARAM_SET` ×N | **No** | `ParameterEditorController.cc:418`, `:423` |
| Write a camera parameter | `PARAM_EXT_SET` | **No** | `QGCCameraIO.cc:202` |
| Bind a parameter to an RC channel | `PARAM_MAP_RC` | **No** | `Vehicle.cc:2940` |
| `param set` via the firmware shell | `SERIAL_CONTROL` | **No** | `MAVLinkConsoleController.cc:161` |
| Read a parameter | `PARAM_REQUEST_READ` / `_LIST` | **No** | `ParameterManager.cc:1018`, `:719` |

The first and fourth rows together are the finding worth escalating: **the station evaluates
authority to erase every parameter, and does not evaluate authority to change one.** It is not an
ordering mistake, it is the mechanical consequence of gating a message type rather than an operation
class, and it is the same shape as the calibration asymmetry doc 30 already asks the owner to weigh.

## 7. Candidate enforcement locations

**No location is selected and no policy is proposed.** `Q-029` and ADR-014 are `OPEN`; selecting
here would be the invention `P7-NOINV-001` prohibits, and WP9-B prohibits a second differently-shaped
gate. What follows maps each candidate onto ADR-014's existing Options A/B/C vocabulary so the
decision can be taken with the parameter path in view instead of without it.

| # | Candidate | File:line | Covers | Misses | ADR-014 fit |
| --- | --- | --- | --- | --- | --- |
| C1 | `ParameterManager::_mavlinkParamSet` entry | `ParameterManager.cc:306` | E1–E6: all C++ and QML parameter writes | X1–X4. Authorizes once per write, not per transmission | **Option C** — a parameter-class seam at its natural convergence point |
| C2 | Inside the encoder lambda | `ParameterManager.cc:308-329` | Every transmission including retries | X1–X4. Runs on the send path, so a denial surfaces as an encode failure rather than a decision | Option C, per-transmission variant |
| C3 | `Fact::setRawValue` / `setCookedValue` / `forceSetRawValue` | `Fact.cc:134`, `:163`, `:111` | All Fact-mediated writes, including non-parameter Facts | **E6**, X1–X4. Fires for app settings and telemetry too | Option C, but at the wrong altitude |
| C4 | `SendMavlinkMessageState::_sendMessage` | `SendMavlinkMessageState.cc:18` | Every retry of every state-machine message, parameter reads included | X1–X4. **Outside `src/FactSystem`** — not Tab 9's file | Between Options B and C |
| C5 | `Vehicle::sendMessageOnLinkThreadSafe` | `src/Vehicle/Vehicle.cc:1392` | All per-vehicle outbound traffic: E1–E6, X1, X2, X3, X4 | Raw-byte forwarding in `MAVLinkProtocol` | **Option B** — already named in doc 30's "Deeper boundary candidates" |
| C6 | Parameter-class seam **plus** the camera path | C1 + `QGCCameraIO.cc:202` (`sendParameter`, `:371`) | E1–E6 and X2 — the whole "named parameter write" concept | X1, X3, X4 | Option C with the class drawn around the concept rather than the message id |

### Trade-offs, stated without choosing

| Dimension | C1 / C6 (parameter-class seam) | C5 (message boundary) | C3 (Fact layer) |
| --- | --- | --- | --- |
| Coverage of §4 | Complete for E1–E6; C6 adds X2 | Complete for E1–E6 and X1–X4 | Incomplete — E6 escapes |
| Core edits | One file (C1), two (C6). Both already Tab 9's or adjacent | `Vehicle.cc` is **Tab 3's**, plus `LinkInterface` and `MAVLinkProtocol` for raw bytes | `Fact.cc` is in Tab 9's directory, but the blast radius is every Fact in the application |
| Decision inputs needed | Parameter identity and value — see below | Must decide on **arbitrary message types**, including heartbeats and telemetry requests | Must re-derive "is this Fact a vehicle parameter" |
| Retry interaction | Authorizes intent once; retries re-send under that decision unless C2 is chosen | Evaluated per transmission for free | Authorizes intent once |
| Testability | Good — one seam, one class, deterministic through `MockLink`'s `PARAM_ERROR` support | Every existing test traverses it | Poor — would need per-Fact-kind fixtures |
| `_HASH_CHECK` | Unaffected, so parameter download cannot break | **Must be explicitly allowed** or download breaks | Unaffected |
| Shell (X3) | Not covered. Would need a separate decision | Covered as `SERIAL_CONTROL` | Not covered |
| Reversibility | Each increment independently revertible | Hot-path edits in three files | Wide and hard to scope |

### Inputs an eventual gate would need, and what the existing structures carry

`SOURCE-CONFIRMED` comparison. This is a gap statement, not a proposed schema.

| Input | Available at `_mavlinkParamSet:306`? | Present in `AeroraGatedOperation`? |
| --- | --- | --- |
| Operation class (write versus read) | Implicit — the function only writes | No. The struct has `mavCommand`, an int `MAV_CMD` |
| Parameter identity (`paramName`) | **Yes** | **No field exists** |
| Parameter value and type (`rawValue`, `valueType`) | **Yes** | Only `param1`..`param7`, which are `MAV_CMD` arguments |
| Target component (`componentId`) | **Yes** | `targetCompId` |
| Vehicle identity | `_vehicle->id()` — MAVLink sysid | `AeroraVehicleContext.mavlinkSystemId`, explicitly documented as **not** the Aerora fleet identity |
| Operator identity and role | Not present on this path | `AeroraOperatorIdentity`, `AeroraRoleAssignment` — both `CONTRACT-ONLY`, gated on `Q-029` |
| Audit outcome cause | Not present | `AeroraAuthorizationDecision.outcome`, `reasonCode` |

Reference: `aerora/src/Security/AeroraAuthorization.h`, structs at the `AeroraGatedOperation` and
`AeroraAuthorizationDecision` definitions; the existing gate consumes them via
`AeroraCommandAuthorizer::authorize()` (`aerora/src/Security/AeroraCommandAuthorizer.h:35`).

**The load-bearing consequence:** `AeroraGatedOperation` cannot express a parameter write. It has no
parameter-name and no parameter-value field, and its `mavCommand` is a `MAV_CMD` enumerant that a
`PARAM_SET` does not have. Extending it, or adding a sibling operation type, is an ADR-014 decision
about the *shape* of the authorization model — which is Tab 2's root cause per
[07 § 3](07-phase-7-work-packages-and-ownership.md#3-conflict-avoidance) — and is **not** proposed
here. It is recorded so the decision is not taken in ignorance of the requirement.

## 8. Recommended future tests

None of the following is written. Writing an authorization test before `Q-029` and ADR-014 settle
would encode a policy that does not exist, and registering a target with nothing in it is the empty
test target [07 WP7-D](07-phase-7-work-packages-and-ownership.md#tab-7--gui) rules out. Each entry
below names the regression it would catch, per [test/README](../../../test/README.md#test-quality).

### 8.1 Available now — no policy needed

| # | Test | Asserts | Base class | Catches |
| --- | --- | --- | --- | --- |
| T1 | `_mavlinkParamSet` caller count is exactly two | A static source check over `src/FactSystem` and `src/QmlControls`, in the manner of `.kiro/tools/aerora-ui-static-check.ps1` | n/a — source check | A third caller appearing, which would silently widen the surface a future seam has to cover |
| T2 | `PARAM_SET` pack sites are exactly `ParameterManager.cc:319` and `:1240` | Same mechanism, over `src/**` | n/a | A new `PARAM_SET` emitter outside the funnel |
| T3 | Offline-editing facts emit no `PARAM_SET` | Mutate a Fact from `_loadOfflineEditingParams()`; assert `pendingWrites` stays false and no message is sent | `ParameterTest` | Someone "fixing" the missing `connect` at `ParameterManager.cc:1626` and turning offline editing into live reconfiguration |
| T4 | `containerSetRawValue` does not echo | Inbound `PARAM_VALUE` must not produce an outbound `PARAM_SET` | `ParameterTest` | A regression in the `Fact.h:179-180` contract, which would create a write loop |

T1 and T2 are the two with real leverage and no policy dependency. Both are source checks, which is
the only class of check this host can run (`D-006`). **Neither is implemented in this pass** — adding
tooling under `.kiro/tools/` overlaps Tab 1/Tab 8 and is filed as
[HO-TAB9-02](#11-handoffs) rather than taken unilaterally.

### 8.2 Blocked on ADR-014 — the boundary itself

| # | Test | Asserts | Blocked on |
| --- | --- | --- | --- |
| T5 | A denied write does not reach the wire | Drive E1 with a denying policy; assert no `PARAM_SET` observed at `MockLink` | ADR-014 + `Q-029` |
| T6 | A denied write is distinguishable from a transport failure | Per ADR-012, a denial must not surface as a timeout or `MavCmdResultFailureNoResponseToCommand` analogue | ADR-014 |
| T7 | E6 is covered | `sendDiff` with a `noVehicleValue` entry — the direct `_mavlinkParamSet` call at `:418` — is denied too | ADR-014 |
| T8 | Retry does not escape the decision | Deny after the first transmission; assert the retry at `ParameterManager.cc:447-448` does not send | ADR-014 + the authorization-lifetime decision |
| T9 | Each uncovered path's status is asserted, not assumed | One case per X1–X4 asserting the **documented** outcome, whatever ADR-014 chooses — including "deliberately out of scope" | ADR-014's required "enumerated list of paths out of scope" |
| T10 | An allowed write is unaffected | Full success path with an allowing policy; `pendingWrites` returns to false | ADR-014 |

### 8.3 Regression tests that already exist and must keep passing

`SOURCE-CONFIRMED` present in the tree, `0` executed (`D-006`). Any future boundary must leave all of
these green; they are the definition of "existing validation and acknowledgement behaviour
preserved".

| Test | File | Covers |
| --- | --- | --- |
| `_paramWriteNoAckRetry`, `_paramWriteNoAckPermanent` | `test/FactSystem/ParameterManagerTest.h:23-24` | The retry budget and its exhaustion |
| `_paramWriteUInt8`, `_paramWriteUInt16` | `:25-26` | Type round-trip through `_fillMavlinkParamUnion` and the ack predicate |
| `_paramWriteParamError` | `:29` | `PARAM_ERROR` ends the attempt without retry |
| `_paramReadParamError`, `_paramReadNoResponse`, `_paramReadFirstAttemptNoResponseRetry` | `:27-28`, `:30` | The read path, which shares `WaitForParamResponseState` |
| `_setParamWithFailureMode` | `:48` | The `MockLink::ParamSetFailureMode_t` matrix |
| `_sendDiffUnknownParamStopsAfterWriteFailure` | `test/FactSystem/ParameterEditorControllerTest.h:20` | **E6 specifically** — the direct `_mavlinkParamSet` call |
| `FactTest`, `FactMetaDataTest` | `test/FactSystem/` | `setRawValue` / `convertAndValidateRaw` semantics |
| `MavCommandAuthorizationTest` | `test/Vehicle/MavCommandAuthorizationTest.cc` | The existing `MAV_CMD` gate. Shows the shape a parameter equivalent would take |

`_sendDiffUnknownParamStopsAfterWriteFailure` is the useful one to read first: it already exercises
the single path that bypasses the Fact layer.

## 9. Open questions and dependencies

### Unchanged by this investigation

| ID | Status | Why Tab 9 does not touch it |
| --- | --- | --- |
| `Q-029` operator role set | `OPEN` — [08 § 1](08-risk-register-and-open-decisions.md#1-blockers) | Product + operations. No role, permission or allow-list is proposed here |
| `ADR-014` outbound scope | `OPEN — ARCHITECTURAL DECISION REQUIRED` — [doc 30](../03-implementation-specification/30-outbound-command-security-audit.md#adr-014-outbound-authorization-scope) | Architecture. §7 informs Options A/B/C; it does not select |
| Authorization lifetime on retry | `OPEN` — [08 § 2](08-risk-register-and-open-decisions.md#2-open-decisions) | §5.3 shows it bites harder on `PARAM_SET` than on `MAV_CMD`. Still Architecture + Tab 2 |
| `Q-037` audit mechanism | `OPEN` | §3 records that the parameter path has no audit record at all. The mechanism is Tab 2's, gated on WP1-C |
| `D-006` build host | `NO-GO` | Nothing here was compiled or run |

### Resolved by this investigation

| ID | Before | After |
| --- | --- | --- |
| `P7-AUTH-003` | `OPEN — ASSIGNED (Tab 9)`, no assessment | **Ownership and assessment discharged.** The path is traced, the funnel is named, six entry points and four bypasses are enumerated. The *policy* half stays `OPEN` on `Q-029`/ADR-014 |
| `R7-15` | "`PARAM_SET` remains unowned"; evidence `ParameterManager.cc:319`, `:398` | Owned and assessed. Evidence corrected to `_mavlinkParamSet:306` with `:319`/`:398` as its pack and retry sites. The **risk** persists: the gap is now documented rather than merely suspected |

### New, raised here

| # | Question | Owner | Blocks |
| --- | --- | --- | --- |
| Q9-01 | Does ADR-014's parameter class mean "`PARAM_SET`" or "named parameter write"? The first excludes the camera `PARAM_EXT_SET` path (X2); the second includes it | Architecture, with Tab 6 for the camera path | The scope of any C1-versus-C6 seam |
| Q9-02 | Is the firmware shell (X3) in scope for authorization at all, or explicitly out? It is arbitrary text, so no parameter-shaped check constrains it | Architecture + product | ADR-014's required enumerated out-of-scope list |
| Q9-03 | Should `PARAM_MAP_RC` (X4) be treated as a parameter operation or a manual-control one? It delegates ongoing parameter authority to a stick | Architecture, with **Tab 3** who owns `Vehicle.cc` | Which ADR-014 class it belongs to |
| Q9-04 | Must `_HASH_CHECK` (X1) be exempted explicitly? Under Option B it is intercepted, and blocking it breaks parameter download | Architecture | Option B feasibility |

None of these is a decision Tab 9 may take. Each is recorded with an owner so it closes rather than
drifts, which is the [07 § 3](07-phase-7-work-packages-and-ownership.md#3-conflict-avoidance)
root-cause rule.

## 10. Proposed traceability updates

[06](06-requirements-to-test-traceability.md) is **Tab 1's file.** These are proposed replacement rows
filed as [HO-TAB9-01](#11-handoffs), not applied here. No new requirement ID is invented.

Replacing the `P7-AUTH-003` row in
[06 § 2](06-requirements-to-test-traceability.md#2-traceability-by-requirement-block):

| Requirement | Component | Test | Method | Status |
| --- | --- | --- | --- | --- |
| `P7-AUTH-003` `PARAM_SET` has an owner | `ParameterManager::_mavlinkParamSet` `src/FactSystem/ParameterManager.cc:306` — sole funnel, 2 callers | `ParameterManagerTest` (existing write regressions) + `TBW` T1/T2 source checks | UT + STATIC | **`OPEN — ASSIGNED (Tab 9)`**; assessed in [10](10-parameter-write-path-and-authorization-boundary.md), policy gated on `Q-029`/ADR-014 |

Two scope clarifications to existing rows, so neither reads wider than it is:

| Requirement | Current | Proposed clarification |
| --- | --- | --- |
| `P7-AUTH-001` every command passes the decision point | `MavCommandQueue.cc` + `authorizeMavCommand()` / `MavCommandAuthorizationTest` | Append: *scope is queued `MAV_CMD` plus `startCalibration` (ADR-015). `PARAM_SET`, `PARAM_EXT_SET`, `PARAM_MAP_RC` and `SERIAL_CONTROL` are outside it — [10 § 6](10-parameter-write-path-and-authorization-boundary.md#6-current-coverage-versus-uncovered-paths)* |
| `P7-AUTH-004` UI exposes no bypassing actuator | `AeroraVehicleUiContext` / `AeroraUiSafetyTest` | Append: *scoped to Aerora's QML-reachable surface. Upstream parameter controls (`FactPanelController::getParameterFact`, `ParameterEditorController`) are out of scope and unasserted by this test* |

And a risk-register note for [08 § 4](08-risk-register-and-open-decisions.md#4-risk-register):
`R7-15`'s mitigation column reads "No parameter-authorization claim is supportable until WP9-A
completes". WP9-A is complete as a `DESIGNED` assessment; the **gap** is not closed. Suggested
rewording: *"Assessed in [10](10-parameter-write-path-and-authorization-boundary.md). Owner and path
known; enforcement still `OPEN` on `Q-029` → ADR-014."*

## 11. Handoffs

Filed per [07 § 7](07-phase-7-work-packages-and-ownership.md#7-handoff-protocol).

| Field | HO-TAB9-01 | HO-TAB9-02 | HO-TAB9-03 |
| --- | --- | --- | --- |
| Owner | **Tab 1** | **Tab 1** (with Tab 8 for the lane) | **Tab 1** |
| File | `06-requirements-to-test-traceability.md`, `08-risk-register-and-open-decisions.md`, `00-README.md` document-set table | `.kiro/tools/` — a new source check | `../03-implementation-specification/30-outbound-command-security-audit.md` |
| Change | The rows in [§10](#10-proposed-traceability-updates), plus a document-set row for **document 10**, alongside the row Tab 1 already added for 09 | T1/T2 from [§8.1](#81-available-now--no-policy-needed): assert the `_mavlinkParamSet` caller count and the `PARAM_SET` pack-site set | Four corrections in [§12.2](#122-corrections-to-existing-records) |
| Invariant at risk | `P7-AUTH-003`, `P7-AUTH-001`, `P7-AUTH-004` accuracy | `P7-ARCH-021`, `P7-AUTH-003` — surface growth going unnoticed | `P7-NOINV-002`; doc 30 is the authority for the path inventory |
| Blocks | Nothing. Documentation accuracy | Nothing. Pure addition | Nothing. Doc 30 is not wrong, it is incomplete and one attribution is off |
| Tracked by | This document | T1, T2 | This document |

Tab 9 edited none of those files, and no file under `src/` or `test/`. Concurrent uncommitted work by
Tabs 1, 2 and 3 under `aerora/` was read only and left untouched.

## 12. Reference index

### 12.1 Primary references

Every significant finding, with its file and function. All `SOURCE-CONFIRMED` at `cbe944bb0`.

| Finding | File | Line | Symbol |
| --- | --- | --- | --- |
| Sole `PARAM_SET` funnel | `src/FactSystem/ParameterManager.cc` | 306 | `ParameterManager::_mavlinkParamSet` |
| `PARAM_SET` pack | `src/FactSystem/ParameterManager.cc` | 319 | `mavlink_msg_param_set_pack_chan` |
| Encoder lambda | `src/FactSystem/ParameterManager.cc` | 308 | `paramSetEncoder` |
| Ack predicate | `src/FactSystem/ParameterManager.cc` | 331 | `checkForCorrectParamValue` lambda |
| Rejection predicate | `src/FactSystem/ParameterManager.cc` | 368 | `checkForParamError` lambda |
| Write state machine | `src/FactSystem/ParameterManager.cc` | 397 | `QGCStateMachine("ParameterManager PARAM_SET")` |
| Retry state | `src/FactSystem/ParameterManager.cc` | 398 | `SendMavlinkMessageState(..., kParamSetRetryCount)` |
| Ack wait state | `src/FactSystem/ParameterManager.cc` | 411 | `WaitForParamResponseState` |
| Timeout → re-send | `src/FactSystem/ParameterManager.cc` | 447-448 | `WaitStateBase::timeout` transition |
| Operator failure notice | `src/FactSystem/ParameterManager.cc` | 421-426 | `userNotifyState`, `QGC::showAppMessage` |
| Fact → manager funnel | `src/FactSystem/ParameterManager.cc` | 536 | `ParameterManager::_factRawValueUpdated` |
| Funnel connect (discovery) | `src/FactSystem/ParameterManager.cc` | 257 | `connect(fact, &Fact::containerRawValueChanged, …)` |
| Funnel connect (FTP file) | `src/FactSystem/ParameterManager.cc` | 1897 | same, inside `_parseParamFile` (`:1709`) |
| Offline facts **not** connected | `src/FactSystem/ParameterManager.cc` | 1570, 1626 | `_loadOfflineEditingParams` |
| Second `PARAM_SET` emitter | `src/FactSystem/ParameterManager.cc` | 1240 | `_HASH_CHECK`, in `_tryCacheHashLoad` (`:1166`) |
| Gated reset | `src/FactSystem/ParameterManager.cc` | 1639, 1641 | `resetAllParametersToDefaults` |
| Ungated reset | `src/FactSystem/ParameterManager.cc` | 1648 | `resetAllToVehicleConfiguration` |
| Pending-write accounting | `src/FactSystem/ParameterManager.cc` | 1928, 1936 | `_increment/_decrementPendingWriteCount` |
| Retry/ack constants | `src/FactSystem/ParameterManager.h` | 111, 113 | `kParamSetRetryCount = 2`, `kWaitForParamValueAckMs = 1000` |
| `friend` that reaches the private API | `src/FactSystem/ParameterManager.h` | 30 | `friend class ParameterEditorController` |
| Wire transmission | `src/Utilities/StateMachine/States/SendMavlinkMessageState.cc` | 45-46 | `_encoder(...)`, `sendMessageOnLinkThreadSafe` |
| Retry exhaustion | `src/Utilities/StateMachine/States/SendMavlinkMessageState.cc` | 20-22 | `_runCount > _retryCount + 1` → `error()` |
| Ack / rejection handling | `src/Utilities/StateMachine/States/WaitForParamResponseState.cc` | 29, 41 | `_messageReceived` |
| Fact write, change-checked | `src/FactSystem/Fact.cc` | 134 | `Fact::setRawValue` |
| Fact write, unconditional | `src/FactSystem/Fact.cc` | 111 | `Fact::forceSetRawValue` |
| Cooked write | `src/FactSystem/Fact.cc` | 163 | `Fact::setCookedValue` |
| Inbound, no echo | `src/FactSystem/Fact.cc` | 198 | `Fact::containerSetRawValue` |
| Writable QML properties | `src/FactSystem/Fact.h` | 53-54, 30, 32 | `value`, `rawValue`, `enumIndex`, `enumStringValue` |
| Range checks skipped on write | `src/FactSystem/FactMetaData.cc` | 356 | `convertAndValidateRaw`, `!convertOnly` guards |
| QML parameter accessor | `src/FactSystem/FactControls/FactPanelController.h` | 25 | `getParameterFact` |
| Fact-layer bypass | `src/QmlControls/ParameterEditorController.cc` | 418 | `sendDiff` → `_mavlinkParamSet` |
| Editor `Q_INVOKABLE` surface | `src/QmlControls/ParameterEditorController.h` | 156-165 | 10 methods |
| Existing `MAV_CMD` gate | `src/Vehicle/MavCommandQueue.cc` | 281, 294 | `sendWorker`, `authorizeMavCommand` |
| Authorization hook | `src/API/QGCCorePlugin.h` | 148 | `authorizeMavCommand` |
| Second gate call site | `src/Vehicle/Vehicle.cc` | 2400 | `startCalibration`, ADR-015 |
| Per-vehicle send boundary | `src/Vehicle/Vehicle.cc` | 1392 | `sendMessageOnLinkThreadSafe` |
| `PARAM_MAP_RC` senders | `src/Vehicle/Vehicle.cc` | 2922, 2940, 2956, 2968 | `sendParamMapRC`, `clearAllParamMapRC` |
| `PARAM_MAP_RC` QML reach | `src/Vehicle/Vehicle.h` | 398, 401 | `Q_INVOKABLE` |
| Camera `PARAM_EXT_SET` | `src/Camera/QGCCameraIO.cc` | 202, 371 | `mavlink_msg_param_ext_set_encode_chan`, `sendParameter` |
| Firmware shell | `src/AnalyzeView/MAVLinkConsole/MAVLinkConsoleController.cc` | 38, 135, 161 | `sendCommand`, `_sendSerialData` |
| In-flight `PARAM_SET` mutation | `src/FirmwarePlugin/APM/APMFirmwarePlugin.cc` | 201, 237 | decode then re-encode `param_type` |
| Resume list, **unmodified** | `src/MissionManager/MissionManager.cc` | 110-127 | `includedResumeCommands` |
| Authorization structs | `aerora/src/Security/AeroraAuthorization.h` | — | `AeroraGatedOperation`, `AeroraAuthorizationDecision` |
| Overlay gate consumer | `aerora/src/Security/AeroraCommandAuthorizer.h` | 35, 38 | `authorize`, `isAuthorized` |

### 12.2 Corrections to existing records

Filed as [HO-TAB9-03](#11-handoffs). Doc 30's parameter rows are not wrong; three are incomplete and
one attribution is off.

| # | Record | Says | Should say |
| --- | --- | --- | --- |
| 1 | doc 30, parameter-write row, entry point | `ParameterManager.cc:319`, `:398` | Both lines are correct but neither is an entry point. Entry point is `_mavlinkParamSet` at `:306`; `:319` packs, `:398` retries |
| 2 | doc 30, parameter-write row, owner | `UNASSIGNED` | **Tab 9**, per [07 § 2](07-phase-7-work-packages-and-ownership.md#2-tab-numbering-resolution) |
| 3 | doc 30, parameter-to-RC row | `Vehicle::setPIDTuningTelemetryMode`, `clearAllParamMapRC` `Vehicle.cc:2926-2962` | `Vehicle::sendParamMapRC` (`:2922`, packs `:2940`) and `clearAllParamMapRC` (`:2956`, packs `:2968`). `setPIDTuningTelemetryMode` (`:2754`) does not pack this message, and `:2968` falls outside the quoted range |
| 4 | doc 30, path inventory | 13-plus paths, no camera or shell entry | Two parameter-authority paths are missing: `PARAM_EXT_SET` (`QGCCameraIO.cc:202`) and `SERIAL_CONTROL` (`MAVLinkConsoleController.cc:161`) |

Also worth recording: `ParameterManager.cc:319` and `:398` were quoted in
[07](07-phase-7-work-packages-and-ownership.md#2-tab-numbering-resolution),
[08](08-risk-register-and-open-decisions.md#4-risk-register) and
[04](04-subsystem-and-interface-completion-matrix.md) and are **still accurate at `cbe944bb0`**. The
instruction to re-verify them rather than trust them was the right one, and the result was that the
numbers held while the description did not.

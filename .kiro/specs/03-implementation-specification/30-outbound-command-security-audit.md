# Outbound Command Security Audit

Owner: **Tab 1 (integration)**. The path evidence is Tab 2's, verified against
`HEAD = 9459a4b57986f40df8bdc193d5453c0596fbb416` and recorded in
[27-implementation-adrs.md](27-implementation-adrs.md) and
[28-security-implementation-note.md](28-security-implementation-note.md). This document does not
re-derive it. What it adds is the **ownership and decision assignment** for each path, and the
framing of the scope decision that follows.

No gate is implemented or moved here. No MAVLink identifier, component id, role or permission is
invented.

## Current enforcement scope, stated exactly

```text
D-001/Q-029 enforcement is implemented for queued MAV_CMD traffic.
```

That phrasing is deliberate and must be used verbatim in status reporting. The following claim is
**false** and must not appear anywhere:

> "all unauthorized GCS commands are blocked"

The gate sits at `MavCommandQueue::sendWorker()`. It evaluates commands that enter the command
queue. It does not evaluate any other outbound path.

## Path inventory

Columns follow the required audit format. "Current gate" is the gate as it exists today, not as it
might exist later.

### Gated

| Path | Entry point | Owner | MAVLink type | Current gate | Security scope | Decision |
| --- | --- | --- | --- | --- | --- | --- |
| Queued commands, all 7 public variants | `Vehicle::sendMavCommand*` → `MavCommandQueue::sendWorker()` | Tab 3 / Tab 2 | `COMMAND_LONG` (#76), `COMMAND_INT` (#75) | **`sendWorker()`** | In scope | None. `D-001` settled |
| Coordinator direct sends | `MavCommandQueue::sendWorker()` | Tab 2 | same | **`sendWorker()`** | In scope | None |
| Delayed commands | `sendCommandDelayed` → `QTimer::singleShot` → `sendCommand` | Tab 2 | same | **`sendWorker()`** at fire time | In scope | None |
| `stopCalibration()` | `Vehicle::stopCalibration()` `Vehicle.cc:2408` | Tab 3 | `COMMAND_LONG` | **`sendWorker()`** | In scope | None |

### Ungated — actuating

| Path | Entry point | Owner | MAVLink type | Current gate | Security scope | Decision |
| --- | --- | --- | --- | --- | --- | --- |
| **Calibration start** | `Vehicle::startCalibration()` `Vehicle.cc:2328`; packs at `:2395-2404`, sends at `:2405` | **Tab 3** | `COMMAND_LONG` carrying `MAV_CMD_PREFLIGHT_CALIBRATION` | **None** | **Out of scope** | **`OPEN` — assigned to Tab 3 with Tab 2 review.** See the note below |
| Joystick / manual control | `Vehicle::sendJoystickDataThreadSafe` `Vehicle.cc:3002-3024` | **Tab 3** | `MANUAL_CONTROL` (#69) | **None** | Out of scope | `OPEN` — high-rate path; a per-message gate has timing consequences |
| RC override | `Vehicle::sendJoystickAuxRcOverrideThreadSafe` `Vehicle.cc:3056-3108` | **Tab 3** | `RC_CHANNELS_OVERRIDE` (#70) | **None** | Out of scope | `OPEN` |
| Mode change | `Vehicle.cc:1511` | **Tab 3** | `SET_MODE` (#11) | **None** | Out of scope | `OPEN` — firmware-dependent routing |
| Guided altitude change | `APMFirmwarePlugin::guidedModeChangeAltitude` `APMFirmwarePlugin.cc:890-898` | **Tab 3** | `SET_POSITION_TARGET_LOCAL_NED` (#84) | **None** | Out of scope | `OPEN` |
| Follow-target motion report | `FirmwarePlugin::sendGCSMotionReport` `FirmwarePlugin.cc:466-474` | **Tab 3** | `FOLLOW_TARGET` (#144) | **None** | Out of scope | `OPEN` |
| Gimbal rate | `GimbalController::sendRate` `GimbalController.cc:749-764` | **Tab 6** | `GIMBAL_MANAGER_SET_ATTITUDE` (#282) | **None** | Out of scope | `OPEN` — payload-adjacent actuation |

### Ungated — reconfiguration and indirect authority

| Path | Entry point | Owner | MAVLink type | Current gate | Security scope | Decision |
| --- | --- | --- | --- | --- | --- | --- |
| **Parameter write** | `ParameterManager::_mavlinkParamSet` `ParameterManager.cc:319`, `:398`; `SendMavlinkMessageState.cc:45` | **UNASSIGNED** | `PARAM_SET` (#23) | **None** | Out of scope | **`OPEN` — no tab owns `src/FactSystem`.** Broadest reconfiguration channel in the application |
| Parameter-to-RC mapping | `Vehicle::setPIDTuningTelemetryMode`, `clearAllParamMapRC` `Vehicle.cc:2926-2962` | Tab 3 | `PARAM_MAP_RC` (#50) | **None** | Out of scope | `OPEN` |
| Mission upload, count | `PlanManager::_writeMissionCount` `PlanManager.cc:108-120` | **Tab 5** | `MISSION_COUNT` (#44) | **None** | Out of scope | `OPEN` |
| **Mission item upload** | `PlanManager::_handleMissionRequest` `PlanManager.cc:536-555` | **Tab 5** | `MISSION_ITEM_INT` (#73) | **None** | Out of scope | **`OPEN`** — carries `MAV_CMD` values *inside items*, so commands reach the vehicle as mission content without being evaluated as commands |
| Mission clear | `PlanManager::_removeAllWorker` `PlanManager.cc:886-893` | **Tab 5** | `MISSION_CLEAR_ALL` (#45) | **None** | Out of scope | `OPEN` |
| Signing setup | `VehicleSigningController::_sendSetupSigning` `VehicleSigningController.cc:194` | **Tab 2** | `SETUP_SIGNING` (#256) | **None** | Out of scope | `OPEN` — configures the authentication mechanism itself |

### Deeper boundary candidates

If the scope decision widens, these are the convergence points, already located:

| Candidate | Location | Covers |
| --- | --- | --- |
| `Vehicle::sendMessageOnLinkThreadSafe()` | `src/Vehicle/Vehicle.cc:1392` | All per-vehicle outbound traffic |
| `LinkInterface::sendMessageThreadSafe()` | `src/Comms/LinkInterface.cc:151` | All link traffic |
| `MAVLinkProtocol::_forward` / `_forwardSupport` | `src/Comms/MAVLinkProtocol.cc:206`, `:225` | Raw-byte forwarding, which the two above do not cover |

## Note on calibration — not a coding slip

`Vehicle.cc:2392` carries an upstream comment explaining why that path does not use
`sendMavCommand`: the command's completion time is unbounded, which breaks the retry logic. The
bypass is therefore **deliberate upstream design**.

Two consequences for the decision:

- It cannot be closed by switching call sites. Routing it through `sendWorker()` would reintroduce
  the ack-timeout behaviour upstream deliberately avoided.
- It produces an asymmetry inside one feature: `startCalibration()` is ungated, `stopCalibration()`
  is gated. An operator could begin a motor-actuating calibration without evaluation but be refused
  permission to stop it. **That asymmetry is itself a safety argument and should be weighed by the
  owner**, independent of the broader scope decision.

Tab 3 owns `src/Vehicle/Vehicle.cc`. Tab 1 has inspected and recorded it and has **not** modified it.

## Ownership gap found

`src/FactSystem/ParameterManager.cc` carries `PARAM_SET`, the broadest reconfiguration path in the
application, and **no tab owns it**. Tabs 2–6 cover security, vehicle, telemetry, mission and
payload/power; the Fact System is assigned to none of them.

Recommendation: assign Fact System / parameter authority explicitly before the parameter path is
treated as decided. Until then it is `OPEN — UNASSIGNED`, which is worse than `OPEN`, because no one
is accountable for closing it.

---

## ADR-014: Outbound authorization scope

**Status: `OPEN — ARCHITECTURAL DECISION REQUIRED`.** No option is selected. Tab 1 has no authority
to select one, and no prior approved decision covers it.

### Context

`D-001` approved an enforcing command-authorization gate and it is implemented at
`MavCommandQueue::sendWorker()`. The audit above shows the gate covers queued `MAV_CMD` traffic and
that at least thirteen other paths can change vehicle or payload behaviour without evaluation.

### Problem

Whether `D-001` remains a `MAV_CMD` authorization boundary or becomes a broader outbound
authorization architecture. Every status claim about `AERORA-SEC-REQ-012` depends on the answer.

### Evidence

The path inventory above, all verified against pinned source with file and line references.

### Options

#### Option A — keep the scope at queued `MAV_CMD`

Keep the gate exactly as built; document every other path as out of scope.

| Dimension | Assessment |
| --- | --- |
| Security completeness | **Low.** Thirteen-plus authority-bearing paths remain unevaluated, two of them actuating |
| Architecture | Minimal and already built |
| Maintainability | Highest. One gate, one call site |
| QGC compatibility | Best. One behavioural core edit |
| Core-edit cost | Already paid |
| Testability | The gate is testable; the uncovered paths are untestable because there is nothing to test |
| Bypass risk | **High and permanent**, including `startCalibration()` which can spin motors |
| Future scaling | Poor. Each new path needs its own decision |

#### Option B — authorize all outbound control operations

Move or add the boundary at `Vehicle::sendMessageOnLinkThreadSafe()`, with `LinkInterface` and the
`MAVLinkProtocol` forwarding sites to cover raw bytes.

| Dimension | Assessment |
| --- | --- |
| Security completeness | **Highest.** A single boundary nothing routes around |
| Architecture | One concept, uniformly applied |
| Maintainability | One gate, but it must decide on arbitrary message types |
| QGC compatibility | **Worst.** Multiple core edits across `Vehicle`, `LinkInterface` and `MAVLinkProtocol` |
| Core-edit cost | High, and in hot paths |
| Testability | Good, but every existing test traverses it |
| Bypass risk | Lowest |
| Future scaling | Best |
| Blocking issue | Requires a policy able to decide on arbitrary messages, which cannot be specified while `Q-029` is open. Also evaluates heartbeats and telemetry requests, with signing and timing consequences **not yet assessed** |

#### Option C — layered model, different seams per outbound class

Distinct seams for command, parameter, mission, manual-control and payload classes, each gated where
that class converges.

| Dimension | Assessment |
| --- | --- |
| Security completeness | **High**, and incremental: classes can be closed one at a time |
| Architecture | More concepts, but each seam sits at a natural convergence point already identified |
| Maintainability | Moderate. Several seams to keep aligned |
| QGC compatibility | Moderate, and each edit is independently justifiable and revertible |
| Core-edit cost | Spread, and each increment is small |
| Testability | **Best.** Each class is testable in isolation |
| Bypass risk | Decreases per increment; non-zero until all classes are covered |
| Future scaling | Good. A new class adds a seam rather than reopening the decision |
| Side benefit | Lets high-rate paths (`MANUAL_CONTROL`, `RC_CHANNELS_OVERRIDE`) use a different mechanism from low-rate ones, which Option B cannot |

### Alternatives considered and rejected by Tab 1

Selecting an option. Rejected: §28 of the orchestration directive forbids it absent an approved
decision, and the choice turns on security policy that does not exist while `Q-029` is open.

### Consequences

Unresolved, this blocks any claim that Aerora enforces operator authorization in general. The
currently defensible claim is the one sentence at the top of this document and nothing stronger.

### Verification requirements

Whichever option is chosen:

- a test proving a denied operation of each covered class does not reach the wire;
- a test proving a denial is distinguishable from a transport failure, per `ADR-012`;
- an explicit, enumerated list of paths that remain out of scope;
- evidence that no high-rate path regressed in timing, if Option B or C covers one.

None of these can run today: `D-006` build host, and `Q-035` overlay test configuration.

# Phase 4 — Implementation ADRs

Architecture decisions taken during Layer 1 implementation, following reviewer approval of `D-001`
and `D-002`. Those involving approved QGC core modifications are recorded here in full rather than
concealed behind the overlay.

| ADR | Subject | Reviewer decision | Core edit |
| --- | --- | --- | --- |
| ADR-012 | Command authorization enforcement point | `D-001` APPROVED — enforcing core gate | **Yes**, 3 files |
| ADR-013 | Signing-key storage backend | `D-002` APPROVED — storage backend abstraction | **Yes**, 2 files + 2 additive |
| ADR-017 | Calibration authorization **policy** | Companion to Tab 1's ADR-015 seam | None of its own |

ADR numbering is shared across tabs and is **not** owned here. ADR-014 (outbound authorization
scope) lives in [30-outbound-command-security-audit.md](30-outbound-command-security-audit.md) and
ADR-015 / ADR-016 in [32-integration-adrs.md](32-integration-adrs.md), both Tab 1's. This document
originally numbered the calibration policy decision ADR-014, which collided; it is **ADR-017** and
the next free number should be checked against both of those documents before another is claimed.

---

## ADR-012: Command authorization enforcement point

### Context

`D-001` approved an enforcing command-authorization boundary and explicitly warned against assuming
`_sendFromList()` is correct merely because review suggested it. The call graph was therefore traced
in full before any edit.

### Traced call graph

Public surface on `Vehicle`, all seven variants, `src/Vehicle/Vehicle.cc:2146-2193`:

```text
Vehicle::sendMavCommand                     -> MavCommandQueue::sendCommand
Vehicle::sendMavCommandDelayed              -> MavCommandQueue::sendCommandDelayed
Vehicle::sendMavCommandInt                  -> MavCommandQueue::sendCommandInt
Vehicle::sendMavCommandWithHandler          -> MavCommandQueue::sendCommandWithHandler
Vehicle::sendMavCommandIntWithHandler       -> MavCommandQueue::sendCommandIntWithHandler
Vehicle::sendMavCommandWithLambdaFallback   -> MavCommandQueue::sendCommandWithLambdaFallback
Vehicle::sendMavCommandIntWithLambdaFallback-> MavCommandQueue::sendCommandIntWithLambdaFallback
```

Convergence inside the queue, `src/Vehicle/MavCommandQueue.cc`:

```text
sendCommand                      -> sendWorker
sendCommandInt                   -> sendWorker
sendCommandWithHandler           -> sendWorker
sendCommandIntWithHandler        -> sendWorker
sendCommandDelayed               -> QTimer::singleShot -> sendCommand -> sendWorker
sendCommandWithLambdaFallback    -> sendCommandWithLambdaFallbackWorker -> one of the above
sendCommandIntWithLambdaFallback -> sendCommandWithLambdaFallbackWorker -> one of the above
RequestMessageCoordinator et al. -> sendWorker directly (documented full-control entry point)
```

Transmission stage:

```text
sendWorker
   |  validates, builds entry, _list.append(entry)
   v
_sendFromList(index)
   |  encodes COMMAND_INT or COMMAND_LONG
   v
Vehicle::sendMessageOnLinkThreadSafe   <- the wire
```

`_sendFromList()` has exactly two callers: `sendWorker()` for the first attempt, and
`_responseTimeoutCheck()` for retries.

### Decision

**Enforce at `MavCommandQueue::sendWorker()`, at enqueue time, before `_list.append()`.**

### Why not `_sendFromList()`

The directive requires that a denial must not be treated as a transport timeout and must not
silently retry. Gating at `_sendFromList()` violates both, structurally:

| Requirement | Outcome if gated at `_sendFromList()` |
| --- | --- |
| Must not be treated as a transport timeout | **Violated.** The entry stays queued, exhausts `maxTries`, and the give-up path reports `MavCmdResultFailureNoResponseToCommand` — a denial presented to the operator as an unresponsive vehicle |
| Must not silently retry | **Violated.** `_responseTimeoutCheck()` re-enters `_sendFromList()` on every expired ack window, re-evaluating and re-denying a command the operator was never told was refused |

Gating at `sendWorker()` avoids both because the command never becomes a queue entry, so neither the
retry timer nor the give-up path can ever observe it.

### Why enqueue time is sufficient to prevent transmission

`_sendFromList()` only ever operates on entries already in `_list`, and `_responseTimeoutCheck()`
only iterates `_list`. Returning from `sendWorker()` before `_list.append()` therefore makes
transmission unreachable for that command. No third path reaches `_sendFromList()`.

### Residual risk, bounded and stated

A command authorized at enqueue time is **not** re-authorized before a retry. If authorization were
revoked inside the retry window, an already-queued command could still be re-transmitted.

The exposure is bounded by `_shouldRetry()`, which returns `true` for only four production commands —
`MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES`, `MAV_CMD_REQUEST_MESSAGE`, `MAV_CMD_PREFLIGHT_STORAGE`,
`MAV_CMD_RUN_PREARM_CHECKS` — plus MockLink commands under `QT_DEBUG`. Every other command receives
`maxTries = 1`. All four retryable commands are status or request operations, not actuation.

A transmit-time re-check is the mitigation if this is ever judged insufficient. It is **not**
implemented, because it is beyond the approved minimum and reintroduces the two failure modes above
unless the denial path is duplicated at the transmit stage. Recorded for a future decision.

### Scope limit, stated plainly

This gate authorizes **`MAV_CMD` commands that pass through `MavCommandQueue`**. That is narrower
than "`MAV_CMD` commands", and narrower again than "vehicle-affecting traffic". The earlier wording
of this section claimed the first of those three; re-verification showed it is the third that is
true, so the claim is corrected here rather than left to be discovered later.

#### One verified `MAV_CMD` bypass

`Vehicle::startCalibration()` hand-packs a `COMMAND_LONG` carrying `MAV_CMD_PREFLIGHT_CALIBRATION`
and sends it directly, never entering the queue:

| Item | Value |
| --- | --- |
| Location | `src/Vehicle/Vehicle.cc:2396-2404` |
| Mechanism | `mavlink_msg_command_long_pack_chan(...)` then `sendMessageOnLinkThreadSafe(...)` |
| Upstream rationale | Comment at `:2393-2394` — the command's completion time is unbounded, which breaks the queue's retry logic |
| Authority | **Actuating.** Starts gyro/accel/mag/airspeed calibration; the ArduPilot `CalibrationAPMCompassMot` variant spins motors |
| Counterpart | `Vehicle::stopCalibration()` at `:2407` **does** use `sendMavCommand`, so the pair is asymmetric |

Enumeration method, so this can be re-checked rather than trusted: every
`mavlink_msg_command_long_*` / `mavlink_msg_command_int_*` pack-or-encode site in `src/` was listed.
There are exactly four. Two are the queue's own encoders
(`src/Vehicle/MavCommandQueue.cc:449`, `:469`), one is the bypass above, and the remainder are in
`src/Comms/MockLink/MockLink.cc` — simulated **vehicle-side** traffic, not a ground-station send
path, therefore not a bypass.

So the honest form of the guarantee is: **of the two ground-station `MAV_CMD` emitters, the gate
covers one and does not cover the other.**

#### Three conditional bypasses

These prefer a `MAV_CMD` — which the gate sees — and fall back to a raw message on firmware that
does not support it. Coverage of the underlying operation is therefore **firmware-dependent and
cannot be stated unconditionally**:

| Operation | Gated path | Fallback that bypasses | Fallback location |
| --- | --- | --- | --- |
| Flight-mode change | `MAV_CMD_DO_SET_MODE` | `SET_MODE` (#11), carrying `base_mode` including the armed flag | `src/Vehicle/Vehicle.cc:1504-1511` |
| Current mission item | `MAV_CMD_DO_SET_MISSION_CURRENT` | `MISSION_SET_CURRENT` (#41) | `src/Vehicle/Vehicle.cc:2129-2137` |
| EKF origin | `MAV_CMD_DO_SET_GLOBAL_ORIGIN` | `SET_GPS_GLOBAL_ORIGIN` (#48) | `src/Vehicle/Vehicle.cc:3161-3171` |

#### Authority-bearing traffic the gate never sees

Not an exhaustive census of outbound traffic; these are the paths verified to be able to change what
the aircraft or its payload does.

| Path | Location | Message |
| --- | --- | --- |
| `ParameterManager::_mavlinkParamSet` → `SendMavlinkMessageState` | `src/FactSystem/ParameterManager.cc:319`, `:398`; `src/Utilities/StateMachine/States/SendMavlinkMessageState.cc:45` | `PARAM_SET` (#23) |
| `PlanManager::_writeMissionCount` | `src/MissionManager/PlanManager.cc:108-120` | `MISSION_COUNT` (#44) |
| `PlanManager::_handleMissionRequest` | `src/MissionManager/PlanManager.cc:536-555` | `MISSION_ITEM_INT` (#73) |
| `PlanManager::_removeAllWorker` | `src/MissionManager/PlanManager.cc:886-893` | `MISSION_CLEAR_ALL` (#45) |
| `Vehicle::sendJoystickDataThreadSafe` | `src/Vehicle/Vehicle.cc:3002-3024` | `MANUAL_CONTROL` (#69) |
| `Vehicle::sendJoystickAuxRcOverrideThreadSafe` | `src/Vehicle/Vehicle.cc:3056-3108` | `RC_CHANNELS_OVERRIDE` (#70) |
| `Vehicle::setPIDTuningTelemetryMode`, `clearAllParamMapRC` | `src/Vehicle/Vehicle.cc:2926-2962` | `PARAM_MAP_RC` (#50) |
| `APMFirmwarePlugin::guidedModeChangeAltitude` | `src/FirmwarePlugin/APM/APMFirmwarePlugin.cc:890-898` | `SET_POSITION_TARGET_LOCAL_NED` (#84) |
| `FirmwarePlugin::sendGCSMotionReport` | `src/FirmwarePlugin/FirmwarePlugin.cc:466-474` | `FOLLOW_TARGET` (#144) |
| `VehicleSigningController::_sendSetupSigning` | `src/Vehicle/VehicleSigningController.cc:194` | `SETUP_SIGNING` (#256) |
| `GimbalController::sendRate` | `src/Gimbal/GimbalController.cc:749-764` | `GIMBAL_MANAGER_SET_ATTITUDE` (#282) |

Two of these deserve naming individually. `PARAM_SET` is a parallel authority channel with its own
retry state machine that can reconfigure anything a `MAV_CMD` could. `MISSION_ITEM_INT` carries
`MAV_CMD` values *inside mission items*, so commands can reach the vehicle as mission content
without ever being evaluated as commands.

#### Why the boundary is not moved

A genuinely complete boundary would sit at `Vehicle::sendMessageOnLinkThreadSafe()`
(`src/Vehicle/Vehicle.cc:1392`) or `LinkInterface::sendMessageThreadSafe()`
(`src/Comms/LinkInterface.cc:151`), plus the two raw-byte forwarding sites in
`MAVLinkProtocol::_forward` / `_forwardSupport` (`src/Comms/MAVLinkProtocol.cc:206`, `:225`).

That is **not** implemented, for three reasons, in order of weight:

1. `D-001` approved a *command*-authorization gate. A message-level gate is a different and much
   larger decision, and taking it unilaterally would be scope expansion without approval.
2. It requires a policy able to decide on arbitrary message types, which does not exist and cannot
   be specified while the role set (`Q-029`) is open.
3. It would place an evaluation on every outbound message including heartbeats and telemetry
   requests, with signing and timing consequences that have not been assessed.

Recorded as a known, enumerated gap. **No requirement anywhere in this programme should be read as
met on the strength of this gate alone**, and in particular `AERORA-SEC-REQ-012` is met only for
queued `MAV_CMD` traffic.

### Mechanism

A new virtual on `QGCCorePlugin`, mirroring the existing `mavlinkMessage()` hook exactly in spirit
and return convention — `true` allows, `false` refuses. This was chosen over a bespoke function
pointer or a static registry because `QGCCorePlugin` is already QGC's documented application-level
extension point, which makes the edit idiomatic and upstreamable, and keeps all Aerora policy out of
the core tree.

```text
Operator
   |
Aerora authorization policy
   |
QGCCorePlugin::authorizeMavCommand   <- the core seam, defaults to allow
   |
MavCommandQueue::sendWorker          <- the enforcement point
   |
MavCommandQueue::_sendFromList
   |
MAVLink transmission
```

Default behaviour is unchanged for stock QGC: the base implementation returns `true`.

### Denial semantics, as required

| Requirement | Implementation |
| --- | --- |
| Prevent transmission | Return before `_list.append()` |
| Produce an audit event | The Aerora authorizer emits it to an injected `AeroraAuditSink`; the core seam does not depend on Aerora. **No durable sink exists** — see the status table below |
| Not a transport timeout | New distinct failure code, reported through the existing handler and signal path |
| No silent retry | The command is never queued, so no retry mechanism can observe it |
| Not alter vehicle authority state | The gate reads only; it touches no vehicle state and sends nothing |

The denial path also deliberately does **not** trigger any aircraft behaviour. A refusal does not
command RTL, land, hold or disarm, does not terminate a mission, and does not change payload or power
state. Those are other workstreams' decisions and a security refusal must not manufacture them.

### Verification status

Recorded per the evidence categories so nothing here reads as tested when it is not.

| Item | Status | Evidence |
| --- | --- | --- |
| Call graph and chokepoint selection | `VERIFIED` | Source read at `9459a4b`; all four `MAV_CMD` encode sites enumerated |
| Core gate in `sendWorker()` | `IMPLEMENTED-UNCOMPILED` | Code present; no compiler on the host (`D-006`) |
| `QGCCorePlugin::authorizeMavCommand` seam | `IMPLEMENTED-UNCOMPILED` | Same |
| `MavCmdResultFailureAuthorizationDenied` | `IMPLEMENTED-UNCOMPILED` | Same |
| Aerora authorization chain, fail-closed | `IMPLEMENTED-UNCOMPILED` | Tests written for every stage, including that "resolved to no roles" is denied by the *policy* rather than short-circuited by the chain; none executed |
| Denial is auditable | `IMPLEMENTED-UNCOMPILED` | Sink interface + authorizer emission + vehicle context; test written, not run |
| Durable / tamper-evident audit store | `BLOCKED` | `IF-LOG-020`, `Q-037`. Both sink properties default to false |
| Operator role policy | `OPEN` | `Q-029`. With no policy installed the gate allows and warns — see below |
| End-to-end "denied command is never queued" | `IMPLEMENTATION-PLANNED` | Needs an overlay build with `QGC_BUILD_TESTING`; `Q-035` / `R-021` |

The last two rows are the ones that matter for honest reporting. **While `Q-029` is open, no
authorization decisions are made at all**: `AeroraCorePlugin::authorizeMavCommand` returns `true`
and warns once. The enforcement *point* is real and tested-by-construction; the enforcement *policy*
does not exist. A reader must not conclude that commands are being authorized today.

### Core edit

| File | Change |
| --- | --- |
| `src/API/QGCCorePlugin.h` | Add `virtual bool authorizeMavCommand(...)`, defaulting to `true` |
| `src/Vehicle/VehicleTypes.h` | Add `MavCmdResultFailureAuthorizationDenied` to `MavCmdResultFailureCode_t` |
| `src/Vehicle/MavCommandQueue.cc` | Consult the hook in `sendWorker()`; on refusal report the new code and return |

The added enum value is a QGC-internal failure code. **No MAVLink enum is modified or invented.**

### Rollback

Revert the three files. The enum value is additive and the hook defaults to `true`, so reverting
restores stock behaviour exactly, with no data migration and no persisted state involved.

### Upstream drift

Three files to track. The `QGCCorePlugin` virtual and the enum value are additive and
upstream-contributable. The only behavioural edit is the call site in `sendWorker()`, which is a
single guarded block at the top of one function.

### Related

`D-001`, `AERORA-SEC-REQ-013`, `IF-SEC-003`, `Q-029` (role set still `OPEN`), `ADR-008`, `ADR-001`
(core-edit position now amended — see the note below).

### Consequence for ADR-001

`ADR-001`'s standing position was zero core edits. That position no longer holds. It is **amended, not
reinterpreted**: Aerora carries approved, enumerated core edits, tracked for drift, justified
individually. The governing objective is minimum necessary core modification.

---

## ADR-013: Signing-key storage backend

### Context

`D-002` approved a storage backend abstraction. The source was re-verified before editing; findings
match the investigation exactly.

| Item | Verified |
| --- | --- |
| Storage coupling | `QSettings`, default-constructed inside `_save()` and `_load()` |
| Write path | `_save()`, `src/MAVLink/Signing/MAVLinkSigningKeys.cc:248-278` |
| Read path | `_load()`, `src/MAVLink/Signing/MAVLinkSigningKeys.cc:399` |
| Serialization | Raw 32 bytes, no encryption, no platform keystore |
| In-memory hygiene | `QGC::secureZero` on passphrase, derived buffer, serialized copy, load buffer |
| Virtual members | None except the destructor |

### Decision

Introduce an abstract key-store backend, route only `_save()` and `_load()` through it, and leave
derivation, trial-matching, the QML model and every other behaviour untouched.

```text
MAVLinkSigningKeys
        |
MAVLinkSigningKeyStore        <- abstract backend, additive core file
        |
   +----+----------------------------+
   |                                 |
QSettings backend              AeroraSigningKeyStore
(default, preserves stock      (overlay-side, platform backends
 behaviour exactly)             are NOT implemented in Layer 1)
```

### Why a default backend is shipped here, unlike the audit store

The audit-integrity seam deliberately ships **no** default, so the gap fails loudly at first build.
Key storage is the opposite case: a missing default would break stock QGC signing, which is working
today. The default backend therefore reproduces current `QSettings` behaviour byte-for-byte, and the
security improvement arrives by installing an Aerora backend. Stock QGC is not regressed and is not
silently "fixed" either.

### Cross-platform posture

`D-002` requires that this not be a Windows-only answer. The abstraction is platform-neutral and
**no platform backend is implemented in Layer 1**, because `D-006` leaves the host unable to compile
or test one, and an untested crypto backend is worse than none.

No platform API is named or assumed. Each backend, when implemented, must document storage mechanism,
protection, availability, failure mode, export implications, backup implications, deletion behaviour
and test strategy. Where a platform cannot provide equivalent protection, that posture is to be
recorded explicitly rather than silently downgraded. The error model carries an explicit
"unavailable" outcome so a platform without protection cannot be mistaken for one with it.

### Denial of silent downgrade

The backend interface reports failure explicitly. A backend that cannot protect material must say so
through the error model; it must not fall back to plaintext persistence on its own initiative. The
fallback decision belongs to the installing policy, not the backend.

### Core edit

| File | Change |
| --- | --- |
| `src/MAVLink/Signing/MAVLinkSigningKeyStore.h` | **New, additive.** Backend interface, record type, error model |
| `src/MAVLink/Signing/MAVLinkSigningKeyStore.cc` | **New, additive.** Default `QSettings` backend, extracted verbatim |
| `src/MAVLink/Signing/MAVLinkSigningKeys.h` | Backend injection point; no change to the existing public API |
| `src/MAVLink/Signing/MAVLinkSigningKeys.cc` | `_save()` and `_load()` delegate to the backend |

`secureZero` behaviour is preserved on every path, including inside the default backend.

### Rollback

Revert the two modified files and delete the two additive ones. The on-disk settings layout is
unchanged — same group, same `manifest`, same `keys/<name>` and `timestamps/<name>` paths, same raw
byte encoding — so keys written before or after the change remain readable either way. **No data
migration is required in either direction.**

### Upstream drift

Two modified files, two additive. The abstraction is a natural upstream contribution, which would
reduce drift on the modified pair to zero.

### Verification status

| Item | Status | Evidence |
| --- | --- | --- |
| Pre-change behaviour | `VERIFIED` | `git show 9459a4b:src/MAVLink/Signing/MAVLinkSigningKeys.cc` — `_save()` wrote raw bytes via `settings.setValue("keys/<name>", ...)` |
| Storage path unchanged | `VERIFIED` | `kSettingsGroup` is `"MAVLinkSigningKeys"` and `kKeySubgroup` is `"keys"` both before and after; the default backend writes the identical path |
| Migration required | `VERIFIED` — **none** | Follows from the row above: keys written either side of the change are readable by the other |
| Backend seam, `_save()` / `_load()` delegation | `IMPLEMENTED-UNCOMPILED` | Code present; no compiler on the host (`D-006`) |
| Default `QSettings` backend | `IMPLEMENTED-UNCOMPILED` | Round-trip, not-found, empty-name, remove, corrupt-material tests written; none executed |
| `AeroraSigningKeyStore` refusal when unprotected | `IMPLEMENTED-UNCOMPILED` | Returns `ProtectionUnavailable`; never falls back to plaintext. Now covered by `AeroraSigningKeyStoreTest` — refusal leaves **no** stored material at either the Aerora or the stock path |
| `AeroraSigningKeyStore` wraps before writing | `IMPLEMENTED-UNCOMPILED` | `_testStoredBlobIsNotThePlaintextKey` asserts the persisted bytes are neither equal to nor contain the key. Uses a reversible test double, **not** cryptography |
| Protected and stock paths cannot be confused | `IMPLEMENTED-UNCOMPILED` | `_testAeroraPathIsSeparateFromStockPath` — neither backend reads the other's entries |
| Corrupt protected blob degrades to unusable, not weak | `IMPLEMENTED-UNCOMPILED` | `_testUnprotectFailureReportsCorruptData` — `CorruptData` returned and the output buffer cleared |
| Any platform protection backend | `NOT-EVIDENCED` | `AeroraPlatformKeyProtector::createForCurrentPlatform()` returns `nullptr` on every platform, deliberately. `_testNoPlatformProtectorIsShipped` pins this, so adding a backend forces a deliberate update of this ADR rather than a silent posture change |
| Protection at rest, in practice | **`BLOCKED`** | Follows from the row above. Keys remain stored unprotected, exactly as stock QGC. `F3.1-01`, `R-005` |

The last two rows are the substantive posture: **D-002's abstraction is in place and its security
benefit is not**. `AeroraCorePlugin::_installSigningKeyStore()` therefore leaves the stock backend
installed and warns, rather than installing a store that would refuse every write and break signing
outright. Cross-platform coverage for Windows, Linux, macOS, Android and iOS is `OPEN`; no platform
is the implicit reference, because none is implemented.

### Related

`D-002`, `F3.1-01`, `R-005`, `AERORA-SEC-REQ-017`, `IF-SEC-013`, `ADR-008`, `Q4-003` and `R4-002`
(audit integrity may depend on this key storage).

---

## ADR-017: Calibration authorization policy

The security-policy half of the calibration decision. The **seam** — the core edit in
`Vehicle::startCalibration()` — is Tab 1's
[ADR-015](32-integration-adrs.md), which is accepted and has landed. ADR-015 closes with "it
installs the decision point; the policy that makes it meaningful is Tab 2's and is still `OPEN`".
This ADR is that policy, and it records precisely how much of it is deliverable while `Q-029` is
open.

Division of responsibility, so neither ADR is read as the whole answer:

| Concern | Owner | Where | Status |
| --- | --- | --- | --- |
| Where the decision is taken, and the core edit | Tab 1 | ADR-015 | **Landed**, `IMPLEMENTED-UNCOMPILED` |
| What the decision is keyed on, and what it may not break | Tab 2 | **here** | Delivered |
| Who may calibrate | Product + operations | `Q-029` | **`OPEN`** |
| Whether non-`MAV_CMD` outbound traffic is gated at all | Architecture | Tab 1's ADR-014 | **`OPEN`** |

### Context

`ADR-012` placed the command-authorization gate at `MavCommandQueue::sendWorker()` and recorded that
`Vehicle::startCalibration()` reaches the wire without passing through it. The Vehicle workstream
independently re-verified the bypass and produced an options analysis
(`aerora/src/Vehicle/README.md`) recommending its Option A. Tab 1 then implemented that option.

The evidence below was gathered independently of both, before ADR-015 existed. It is retained
because three workstreams converging on the same reading of the same source is stronger evidence
than one, and because two findings here are not recorded in ADR-015 — the start/cancel wire
ambiguity, and the two-parameter `CalibrationAPMPreFlight` case.

### Evidence, re-verified at `9459a4b5`

```text
stopCalibration(showError)                  startCalibration(calType)
      │  Vehicle.cc:2408-2420                     │  Vehicle.cc:2328-2406
      ▼                                           │  builds COMMAND_LONG in place
sendMavCommand()                                  │  mavlink_msg_command_long_pack_chan
      ▼                                           │                   Vehicle.cc:2396
MavCommandQueue::sendWorker()                     │
      ▼                                           │
QGCCorePlugin::authorizeMavCommand()  ◄─ GATE     │   ✗ never reached
                            MavCommandQueue.cc:294│
      ▼                                           ▼
Vehicle::sendMessageOnLinkThreadSafe()  ◄──────────┘              Vehicle.cc:2405
      ▼
LinkInterface::sendMessageThreadSafe()  — signs, serializes
      ▼
wire
```

Both functions send `MAV_CMD_PREFLIGHT_CALIBRATION`. They converge only *below* the gate.

| Fact | Evidence |
| --- | --- |
| It is the only ungated `MAV_CMD` emitter in the tree | Four `mavlink_msg_command_(long\|int)_(pack\|encode)` sites in `src/`: `MavCommandQueue.cc:449`, `:469`, `Vehicle.cc:2396`, and `src/Comms/MockLink/` (simulated vehicle side, not a send path) |
| `stopCalibration()` sends all seven params zero | `Vehicle.cc:2411-2419` |
| `startCalibration()` sets params per calibration type | `Vehicle.cc:2344-2391` |
| ESC calibration is carried in `param7` alone | `Vehicle.cc:2363-2365`, reached from `PowerComponentController::calibrateEsc()` (`:13`) |
| Compass motor-interference calibration is `param6` | `Vehicle.cc:2372-2374`, reached from `APMSensorsComponentController::calibrateMotorInterference()` (`:342`), whose own operator text reads "the props will spin!" (`:346`) |
| `CalibrationAPMPreFlight` sets **two** params | `param3`, plus `param1` on multirotor or rover — `Vehicle.cc:2378-2385` |
| `Vehicle::motorTest()` is **already** gated | Uses `sendMavCommand(MAV_CMD_DO_MOTOR_TEST, …)`, `Vehicle.cc:2432` |
| Queuing it would block cancel | `_canBeDuplicated()` is false for this command, so a pending start makes `stopCalibration()` fail as `MavCmdResultFailureDuplicateCommand` |
| Queuing it would report a false failure | `PREFLIGHT_CALIBRATION` is absent from `_shouldRetry()`, so `maxTries = 1` and the give-up branch fires at the 1200 ms ack timeout, reporting "Vehicle did not respond" for a calibration that is running normally |

The last two rows are why the direct send is kept. The upstream comment at `Vehicle.cc:2393-2394`
blames retry logic; mechanically that is wrong — the real consequences are cancel-blocking and
spurious failure reporting. The conclusion is the same: **do not route calibration through the
queue.** Option C from the Vehicle analysis is rejected on that basis.

### Decision 1 — policy

**Calibration is a declared member of the gated-operation set. Who may calibrate is `OPEN`.**

What is decided here, because it is decidable without the role model:

| # | Decision |
| --- | --- |
| P1 | `MAV_CMD_PREFLIGHT_CALIBRATION` is in the gated set, declared explicitly per `IF-SEC-005` — never derived from whether a UI control is reachable |
| P2 | The decision is keyed on the **command and its parameter vector**, never on the command id alone. Start and cancel share the command id and differ only in parameters |
| P3 | The all-zero parameter vector is one case, not two. `stopCalibration()`, `startCalibration(CalibrationNone)` and any enumerator the switch does not handle emit identical bytes and must not be distinguished, because they cannot be |
| P4 | A request with any parameter non-zero — including NaN — classifies as a start and stays inside the gated set. A malformed request must not fall through as a cancel |
| P5 | No allow-list, no deny-list and no calibration-specific carve-out is created |
| P6 | A policy must key on parameter **values**, not on which parameter is set. The index does not identify motor relevance — constraint C4 |

What is **not** decided, and must not be inferred:

| Item | Status | Why |
| --- | --- | --- |
| Which operators may start a calibration | **`OPEN`** — `Q-029` | Requires the role model. Naming a role here would be fabricating one |
| Whether motor-relevant calibrations need a higher privilege than others | **`OPEN`** — `Q-029` | Whether they are *privileged* is policy. Note that making them reliably *identifiable* is itself not fully solved — see constraint C4 |
| Whether the firmware type belongs in the authorization context | **`OPEN`** | Needed to disambiguate `param6 = 1`. See C4 — narrower than `Q-029` and answerable independently |
| Whether a policy may deny the all-zero form | **`OPEN`** | See the constraint below. It is a policy choice with a stated consequence, not a thing this ADR settles |

**Calibration policy content cannot be completed before `Q-029` resolves.** Stated plainly, as
required: the gated-set membership, the keying rule and the classification are deliverable now and
are delivered; the permission mapping is not, and no placeholder is shipped in its place.

#### Constraint C4 — the parameter index does not identify motor relevance

Found by walking all fourteen `QGCMAVLink::CalibrationType` enumerators against the switch at
`src/Vehicle/Vehicle.cc:2344-2391`, after this ADR had already been written. **It contradicts an
earlier statement in this workstream's own documentation**, which said a policy needing subset
distinctions could key on the parameter index. That was wrong, and the correction matters.

The index-to-type mapping is not one-to-one, and the collisions are badly placed:

| Parameter | Value | Calibration | Spins motors? |
| --- | --- | --- | --- |
| `param6` | **1** | APM motor-interference (`CalibrationAPMCompassMot`) | **Yes** |
| `param6` | **2** | PX4 airspeed (`CalibrationPX4Airspeed`) | No |
| `param7` | 1 | ESC (`CalibrationEsc`) | **Yes** |
| `param5` | 1 / 2 / 4 | Accel / Level / APM accel-simple | No |
| `param4` | 1 / 2 | Radio / Copy-trims | No |
| `param3` | 1 | PX4 pressure **and** APM pressure-airspeed **and** APM pre-flight — three types, byte-identical | No |
| `param1` | 1 | Gyro, **and** APM pre-flight on multirotor or rover | No |

Three consequences for any future policy:

1. **A policy gated on the parameter index would treat APM motor-interference calibration and PX4
   airspeed calibration as the same operation.** Those are a prop-spinning procedure and a
   pitot-tube procedure. This is the most consequential mistake available on this path.
2. **The two motor-relevant calibrations live in different parameters** — `param6` and `param7` —
   so there is no single "motor parameter" to test.
3. `param3 = 1` is emitted by three enumerators with an identical payload, so those three are
   mutually indistinguishable on the wire, in the same way start and cancel are under C3.

The policy must therefore read the parameter **values** from `AeroraGatedOperation`, which it
receives in full. `AeroraCalibrationRequest` exposes structure only and is deliberately **not**
sufficient for a motor-relevance decision.

There is a residue that no amount of parameter inspection resolves. The upstream comment at
`src/Vehicle/Vehicle.cc:2366` records that `param6 = 1` is a deprecated-but-still-accepted PX4
airspeed value, while the same value is APM motor-interference calibration. **So `param6 = 1` is
ambiguous without knowing the firmware**, and the firmware type is not part of the authorization
context today. Options, none taken here:

| Option | Note |
| --- | --- |
| Add the firmware type to `AeroraGatedOperation` | The hook already receives `Vehicle*`, so `firmwareType()` is reachable. Smallest change, but it widens the authorization context and that is a decision, not a detail |
| Treat `param6 = 1` as motor-relevant on every firmware | Fails safe. Over-restricts PX4 stations whose operators would be refused a harmless airspeed calibration |
| Leave it to the policy | Only works if the policy is given the firmware, i.e. option one |

Recorded `OPEN`. It is a narrower question than `Q-029` and can be answered independently of it,
which makes it the most tractable open item in this ADR.

#### Constraint C1 — denying cancel disables stopping

Because P3 holds, any policy that denies the all-zero parameter vector also removes the operator's
ability to stop a calibration that is already running. `AeroraCalibrationAuthorization::denialWouldBlockCancel()`
reports this condition so a policy author cannot miss it.

It is a **warning, not an override.** Nothing in this workstream overrides a policy verdict;
hard-coding "cancel is always allowed" would be inventing the policy that `Q-029` reserves. The
condition is surfaced, tested, and recorded.

### Decision 2 — the authorization hook

**Reuse `QGCCorePlugin::authorizeMavCommand()` unchanged. No new hook.** Concurred with ADR-015,
reached independently; the security-side reasons are recorded here because they are the ones that
constrain the policy.

| Criterion | Assessment |
| --- | --- |
| Does the signature carry what a calibration decision needs? | Yes — `Vehicle*`, target component, command, command-int flag and all seven parameters. P2 requires the parameters and the hook already passes them |
| Is it documented as safe to call from here? | Yes — "a decision hook, not an action hook"; implementations must not send or mutate vehicle state |
| Is the Aerora side already wired? | Yes — `AeroraCorePlugin::authorizeMavCommand()` overrides it and delegates to `AeroraCommandAuthorizer` |
| Does calling it from a second site change stock QGC? | No — the base implementation returns `true` |
| Would a new virtual add anything? | No. It would duplicate an identical signature and create two authorization vocabularies for one decision |

One hook, two enforcement sites, one policy. That is the property worth protecting, and it is why a
calibration-specific hook was not requested.

### Decision 3 — the enforcement site

**Owner: Tab 1. Status: landed as ADR-015, `IMPLEMENTED-UNCOMPILED`.** Specified here first as a
handoff; the table below is retained as the contract the landed edit is checked against, and the
conformance check follows it.

| Field | Value |
| --- | --- |
| File | `src/Vehicle/Vehicle.cc` |
| Function | `Vehicle::startCalibration(QGCMAVLink::CalibrationType calType)` |
| Region | After the `switch (calType)` block ends (`~:2391`), immediately before `mavlink_msg_command_long_pack_chan` (`:2396`) |
| Current behaviour | Locks the primary link, maps `calType` to a parameter vector, packs a `COMMAND_LONG` and sends it. No authorization evaluation on any path |
| Problem | A motor-relevant `MAV_CMD` reaches the wire without authorization. ESC and compass motor-interference calibration both spin propellers |
| Required change | One guarded block consulting `QGCCorePlugin::instance()->authorizeMavCommand(...)`; on refusal report and `return` before the pack |
| Why this location | **After** the switch, because the parameter vector is the operation identity (P2) — a check before the switch cannot tell start from cancel. **Before** the pack, because that is the last point at which no packet exists. **After** the existing link check, so a missing link is still reported as a missing link and not as a refusal |
| Affected interfaces | None. `startCalibration()` keeps its signature and `void` return; no caller changes |
| Security impact | Closes the only ungated `MAV_CMD` path. Does not alter the direct-send behaviour, add retry semantics, or touch signing or firmware-plugin adjustment |
| Upstream drift impact | One guarded block in one function. The hook it calls is already an additive upstream-contributable virtual |
| Test strategy | See "Tests" below. The wire-level assertions become reachable only once this edit exists |
| Rollback | Delete the block. Behaviour returns to the current bypass exactly |

Invocation contract, exact:

```text
authorizeMavCommand(
    vehicle       = this
    targetCompId  = defaultComponentId()        // same value the pack_chan call uses
    command       = MAV_CMD_PREFLIGHT_CALIBRATION
    commandInt    = false                       // this is a COMMAND_LONG
    param1..7     = the post-switch values, unmodified
)
```

- Call exactly once per `startCalibration()` invocation. It must not be called inside a retry or
  resend loop; there is none on this path and none may be introduced.
- `true` → proceed to pack and send, unchanged.
- `false` → do not pack, do not send, report per the denial semantics below, `return`.
- The hook must not be consulted for `stopCalibration()`; that path is already gated by
  `MavCommandQueue::sendWorker()` and a second check would double-evaluate one command.

#### Conformance of the landed edit

Read at `src/Vehicle/Vehicle.cc:2392-2405` after Tab 1 applied it. Checked against the contract
above rather than assumed correct.

| Contract item | Landed | Note |
| --- | --- | --- |
| Placement after the `switch` | ✅ | Immediately after the `CalibrationNone`/`default` break |
| Placement before the pack | ✅ | `return` precedes `mavlink_message_t msg;` — no packet is constructed |
| Placement after the link check | ✅ | A missing link is still reported as a missing link, not as a refusal |
| `vehicle = this` | ✅ | |
| `targetCompId = defaultComponentId()` | ✅ | Same value the `pack_chan` call uses |
| `command = MAV_CMD_PREFLIGHT_CALIBRATION` | ✅ | No MAVLink identifier invented |
| `commandInt = false` | ✅ | Correct; this is a `COMMAND_LONG` |
| `param1`-`param7` passed unmodified | ✅ | Satisfies P2 — the decision sees the operation identity |
| Called exactly once, no retry loop | ✅ | |
| Not also applied to `stopCalibration()` | ✅ | No double evaluation |
| Operator-visible refusal | ✅ | `QGC::showAppMessage(tr("Calibration not authorized."))` plus a `qCWarning` |
| **Machine-readable refusal** | ❌ | **`mavCommandResult` is not emitted.** See below |

Eleven of twelve. The outstanding item is not cosmetic, so it is stated rather than rounded up.

#### Divergence — no `mavCommandResult` on refusal

ADR-015 records the reason as: `startCalibration()` "returns `void` and has no result handler or
`commandResult` signal". The first half is right; the second is not. `Vehicle::mavCommandResult`
exists (`src/Vehicle/Vehicle.h:822`) and is relayed from `MavCommandQueue::commandResult`
(`src/Vehicle/Vehicle.cc:263`). More to the point, **the queued gate already emits it for a command
that was never queued** — `MavCommandQueue::sendWorker()` emits `commandResult` with
`MavCmdResultFailureAuthorizationDenied` on the authorization-denied path, before `_list.append()`.
So emitting from this site is precedented, not novel.

Two consequences of the omission:

| Consequence | Severity |
| --- | --- |
| A calibration refusal is distinguishable from a vehicle timeout **only by reading the message text**. No consumer can tell them apart programmatically, and the two sites now report the same decision in two different ways | Moderate — inconsistent denial contract |
| The partial-state rollback described below has **no signal to hang off**, so it needs either another core edit or polling | **This is the one that matters** |

The one-line addition, for Tab 1 to accept or reject on the record:

```cpp
        emit mavCommandResult(id(), defaultComponentId(), MAV_CMD_PREFLIGHT_CALIBRATION,
                              MAV_RESULT_DENIED, MavCmdResultFailureAuthorizationDenied);
```

Not applied here: `src/Vehicle/Vehicle.cc` is Tab 1's and was being actively edited. Raised as a
coordination item, not a defect — ADR-015's choice is defensible on its own terms, and this records
the cost so the decision is explicit.

### Decision 4 — denial semantics

| Requirement | How it is met |
| --- | --- |
| No MAVLink packet reaches the send path | Return before `mavlink_msg_command_long_pack_chan`. No message object is ever constructed |
| No retry storm | This path has no retry mechanism and the command never enters `MavCommandQueue`, so neither `_responseTimeoutCheck()` nor `_shouldRetry()` can observe it |
| No silent allow | The base hook returns `true`, which is the documented stock-QGC default, not a silent allow. When Aerora policy is installed, a refusal is reported and audited |
| Distinguishable from a vehicle timeout | `emit mavCommandResult(id(), defaultComponentId(), MAV_CMD_PREFLIGHT_CALIBRATION, MAV_RESULT_DENIED, MavCmdResultFailureAuthorizationDenied)` — the same signal and the same failure code the queue gate uses. A timeout reports `MavCmdResultFailureNoResponseToCommand`; the two codes are asserted distinct |
| Operator-visible | `QGC::showAppMessage(...)`, the same mechanism the queue's denial path uses |
| Auditable | `AeroraCommandAuthorizer` emits `CommandRequested`, `AuthorizationDecision` and `CommandDenied`, now carrying all seven parameters so the record identifies *which* calibration |
| Causes no aircraft behaviour | The gate reads only. No RTL, land, hold, disarm, mission, payload or power effect |

`startCalibration()` has no `showError` parameter and gains none — adding one would change the
signature and every call site. A security refusal is therefore always reported. That is deliberate:
there is no legitimate case for silently refusing a safety-relevant action.

#### Fail closed, fail open, or policy-dependent?

**Fail closed. Already contracted, not newly decided here.** Doc 14 fixes it: an unreachable
decision denies, an empty role set denies everything, and "a default that grants everything is not a
placeholder; it is a security decision taken by omission." Calibration inherits that contract and
receives no exception.

One asymmetry must be stated rather than glossed. `AeroraCommandAuthorizer` is fail-closed and
tested as such. `AeroraCorePlugin::authorizeMavCommand()` currently returns `true` when the chain is
unconfigured, so an **unconfigured station allows** — including, once the core edit lands, allowing
calibration. That deviation is pre-existing, deliberate and recorded (`ADR-012` verification status);
it is a property of the plugin, applied uniformly to every command. This ADR does not extend it,
narrow it, or create a calibration-specific variant of it. Whether it stands is a `Q-029`-adjacent
decision for the reviewer.

#### Partial calibration state — not fully closed by this edit

The requirement "no partial calibration state should be created" **cannot be met by a core edit at
this site alone**, and saying otherwise would be false.

Callers create state *before* calling `startCalibration()`:

| Caller | State established first | Evidence |
| --- | --- | --- |
| `APMSensorsComponentController::calibrateMotorInterference()` | Sets `_calTypeInProgress`, **disables communication-lost detection**, starts log capture, tells the operator to raise the throttle | `APMSensorsComponentController.cc:342-349` |
| `PowerComponentController::calibrateEsc()` | Connects `textMessageReceived` | `PowerComponentController.cc:11-13` |
| `SensorsComponentController` (gyro, mag, accel, level, airspeed) | Calls `_startLogCalibration()` | `SensorsComponentController.cc:155-182` |

A refusal inside `startCalibration()` leaves that state in place. For the motor-interference case
that means communication-lost detection stays disabled.

This is bounded deliberately: those callers live in `src/AutoPilotPlugins/**`, which is assigned to
no workstream, and gating per caller is the shape that produces a missed one.

The cost of the fix depends on the divergence above. **With** a `mavCommandResult` emission, each
controller can roll back its own state from a signal it already has access to — a controller-local
change with no further core edit. **Without** one, the only observable is an app message, so a
rollback needs either a second core edit or polling. That is the concrete reason the one-line
addition is worth accepting, and it is why the omission is recorded as moderate rather than
cosmetic.

Recorded as required follow-up. Owner unassigned. Not claimed as solved, and note that the landed
edit makes this **newly reachable**: before ADR-015 the send always succeeded, so the stale-state
path did not exist in practice. Closing the security gap opened a small operational one.

### Audit change made by this ADR

`AeroraCommandAuthorizer::_audit()` recorded `param1` only. For calibration that is unusable: ESC
calibration sets `param7` and nothing else, so the record would have said "a calibration was
refused" without saying which. All seven parameters are now recorded.

This is a correction, not an expansion: for several commands the parameter vector *is* the
operation, and `AERORA-COMPLY-REQ-002` requires the record to carry the terminal reason for a
command — which presumes the command is identifiable.

### Scope boundary

Unchanged by this ADR, and restated so the closure of calibration is not misread as closure of the
class. The following remain **outside** any authorization gate, each a separate scope decision:

| Path | Message |
| --- | --- |
| `ParameterManager::_mavlinkParamSet` | `PARAM_SET` |
| `PlanManager` write transactions | `MISSION_COUNT`, `MISSION_ITEM_INT`, `MISSION_CLEAR_ALL` |
| `Vehicle::sendJoystickDataThreadSafe` | `MANUAL_CONTROL` |
| `Vehicle::sendJoystickAuxRcOverrideThreadSafe` | `RC_CHANNELS_OVERRIDE` |
| `Vehicle::setPIDTuningTelemetryMode`, `clearAllParamMapRC` | `PARAM_MAP_RC` |
| `APMFirmwarePlugin::guidedModeChangeAltitude` | `SET_POSITION_TARGET_LOCAL_NED` |
| `Vehicle::setFlightMode` firmware fallback | `SET_MODE` |
| `Vehicle::setCurrentMissionSequence` fallback | `MISSION_SET_CURRENT` |
| `Vehicle::setEstimatorOrigin` fallback | `SET_GPS_GLOBAL_ORIGIN` |
| `GimbalController::sendRate` | `GIMBAL_MANAGER_SET_ATTITUDE` |
| `VehicleSigningController::_sendSetupSigning` | `SETUP_SIGNING` |

With ADR-015 landed, **every `MAV_CMD` emitter in the tree now reaches an authorization decision
point.** No claim is made beyond that, and in particular it is not a claim that any decision is
being *evaluated* — while `Q-029` is open the hook allows and warns at both sites.

### Tests

New: `aerora/test/Security/AeroraCalibrationAuthorizationTest`, registered through
`aerora/src/Security/Security.cmake`.

| # | Directive requirement | Test | Status |
| --- | --- | --- | --- |
| 1 | Authorized calibration | `_testAuthorizedCalibrationIsAllowed` — start and cancel forms both allowed under a configured allowing policy | `IMPLEMENTED-UNCOMPILED` |
| 2 | Denied calibration | `_testDeniedCalibrationIsDeniedWithReason` — outcome and `reasonCode` asserted | `IMPLEMENTED-UNCOMPILED` |
| 3 | Denied calibration produces no wire message | **Written.** `AeroraCalibrationEnforcementTest::_testDeniedCalibrationStartReachesNoWire` — asserts `MockLink::receivedMavCommandCount(MAV_CMD_PREFLIGHT_CALIBRATION) == 0`, with the three denial log/message expectations claimed so a zero cannot be a missed send | `IMPLEMENTED-UNCOMPILED` |
| 4 | Denied calibration produces correct failure | Code-level half asserted by `ADR-012`'s `_testDenialCodeIsDistinctFromTransportFailures`. At the site, the strongest available assertion is the app-message match in test 3, because no failure code is emitted there. See the divergence above | Partly `IMPLEMENTED-UNCOMPILED`, failure-code half `NOT-EVIDENCED` |
| 5 | Audit behaviour | `_testAuditIdentifiesWhichCalibrationWasRefused` (asserts `param7 == 1`, `param1 == 0` — this is the test the old param1-only record would have failed), `_testAuditDistinguishesCancelFromStart` | `IMPLEMENTED-UNCOMPILED` |
| 6 | Policy unavailable | `_testUnconfiguredPolicyDeniesCalibration` | `IMPLEMENTED-UNCOMPILED` |
| 7 | Fail-closed behaviour | Same test; asserts the chain denies with `DeniedNotConfigured`, and documents the plugin-level deviation rather than asserting it | `IMPLEMENTED-UNCOMPILED` |
| 8 | `stopCalibration()` regression | `_testCancelStillReachesWireWhenStartIsDenied` — cancel reaches the wire while starts are refused, which is constraint C1 asserted as behaviour and exercises the **other** enforcement site (`sendWorker()`) reaching the same policy. Plus `_testCancelIsFlaggedAsBlockingCancel`, `_testAuditDistinguishesCancelFromStart`, and the `all zero is cancel-or-noop` classification row | `IMPLEMENTED-UNCOMPILED` |
| — | Positive control for the wire counter | `_testAuthorizedCalibrationStartReachesWire` — without it, test 3 could pass because the command is never counted rather than because it was refused | `IMPLEMENTED-UNCOMPILED` |
| — | The unconfigured-station deviation is pinned | `_testUnconfiguredChainAllowsCalibration` — asserts that an unconfigured chain **allows** a motor-relevant calibration. Deliberately asserting the deviation, so changing it to fail closed breaks a test and forces this ADR to be updated with it | `IMPLEMENTED-UNCOMPILED` |
| — | Vehicle identity reaches the audit record on the real path | `_testDeniedCalibrationAuditIdentifiesVehicleAndCalibration` — the only test covering `Vehicle::startCalibration` → `AeroraCorePlugin::authorizeMavCommand` → `AeroraCommandAuthorizer` → `AeroraAuditSink`. The others build an `AeroraGatedOperation` directly and so cannot catch a fault in the plugin's `Vehicle*` translation, which is where the identity is attached | `IMPLEMENTED-UNCOMPILED` |
| — | Classification correctness | `_testClassification` — 7 rows covering cancel, `param1`, `param7` (ESC), `param6` (compass-mot), the two-parameter APM pre-flight case, a non-unit parameter value, and NaN | `IMPLEMENTED-UNCOMPILED` |
| — | Gated-set declaration is explicit | `_testCalibrationIsInTheDeclaredGatedSet`, `_testNonCalibrationCommandIsNotClassified` | `IMPLEMENTED-UNCOMPILED` |

Test 3, as implemented in `AeroraCalibrationEnforcementTest`:

```text
Given  an overlay build, a connected MockLink vehicle, and a denying policy installed in
       AeroraCommandAuthorizer
When   vehicle->startCalibration(QGCMAVLink::CalibrationEsc) is called
Then   MockLink receives no COMMAND_LONG with command == MAV_CMD_PREFLIGHT_CALIBRATION
And    expectAppMessage(/not authorized/) is satisfied          -- the only observable today
And    the same call under an allowing policy DOES reach MockLink  (positive control)
And    vehicle->stopCalibration(false) still reaches MockLink under an allowing policy  (regression)

Add once the mavCommandResult emission is accepted:
And    mavCommandResult fires with failureCode == MavCmdResultFailureAuthorizationDenied
```

It is `VehicleTest`-derived and runs only in an overlay build with `QGC_BUILD_TESTING=ON`, i.e. it
still needs `Q-035` / `R-021` to be **executed** — but it is written, not deferred. Note that the
strongest available assertion is an app-message match, which is weaker than a failure code and is
the practical cost of the divergence above.

Three implementation notes worth keeping, because each was a decision:

| Choice | Reason |
| --- | --- |
| The test policy refuses calibration **starts** only, not everything | A deny-all policy would refuse the background command traffic a connected vehicle generates, producing an unpredictable number of denial warnings that strict log checking cannot account for. Selective refusal is also how a real policy must behave, so the stub is realistic rather than merely convenient |
| `CalibrationEsc` is used throughout | It is the motor-relevant variant carried in `param7` alone, and MockLink acknowledges it **without** starting a calibration simulation or emitting status text (`MockLink.cc:2875-2893` handles `param1`, `param2` and `param5` only). A refusal and an acceptance therefore differ only in the thing under test |
| `AeroraCorePlugin`'s once-per-process unconfigured warning is suppressed in `init()` | It fires on the first command of the first connected vehicle, inside `VehicleTest::init()`, before any test body can claim it — and only for whichever test runs first. `expectLogMessage` would assert it fires, which is true for one test per run and false for the rest. Strict checking evaluates at `cleanup()` (`UnitTest.cc:905`), so a suppression registered after the base `init()` still covers it. See the cross-cutting hazard below |

### Cross-cutting hazard this surfaced

`AeroraCorePlugin::authorizeMavCommand()` emits a once-per-process `qCWarning` when no policy is
installed ("allowing commands without authorization evaluation"). It fires on the **first command of
the first connected vehicle in the test process**, which lands inside `VehicleTest::init()`.

Under strict log checking an unclaimed warning fails the test, so this will fail **whichever overlay
vehicle test happens to run first** — and the order is not fixed. That affects every overlay test
that connects a vehicle, not only this workstream's: Tab 3's `AeroraVehicleLifecycleTest` carries
`Integration Vehicle` labels and has the same exposure.

This is a known framework pattern, not a novel problem. `UnitTest::init()` already carries global
suppressions for exactly this shape, with the matching comment — "the warning lands in a random test
each full run, causing flaky failures across many vehicle/UI tests. Ignore it globally"
(`test/UnitTestFramework/UnitTest.cc:830-833`, for `Vehicle.RemoteIDManager`).

| Option | Owner | Assessment |
| --- | --- | --- |
| Global `ignoreLogMessage` in `UnitTest::init()` | Core test framework — unassigned | Matches the established precedent exactly. One line, fixes it for every tab at once |
| Downgrade to `qCDebug` when `QGC::runningUnitTests()` | Tab 1 — `AeroraCorePlugin.cc` | Keeps the operational warning where it matters and removes it where it only causes flakiness |
| Suppress locally in each overlay vehicle test | Each tab | **What this workstream did**, because it needs no other tab. Duplicated per suite, so it is the worst of the three if more suites appear |

Recorded for whoever owns the first two. Not escalated further, because the local suppression means
no test is currently broken by it.

### Verification status

| Item | Status | Evidence |
| --- | --- | --- |
| The bypass existed and was the only `MAV_CMD` one | `VERIFIED` | Four encode sites enumerated at `9459a4b5`; independently reproduced by the Vehicle workstream and by Tab 1 |
| Motor relevance of ESC and compass-mot variants | `VERIFIED` | Parameter assignments and call sites cited above, including the "props will spin" operator text |
| Start and cancel are wire-indistinguishable | `VERIFIED` | `stopCalibration()` all-zero vector vs the switch's unhandled-enumerator fallthrough. **Not recorded in ADR-015** |
| `CalibrationAPMPreFlight` sets two parameters | `VERIFIED` | `Vehicle.cc:2378-2385`. **Not recorded in ADR-015** |
| Parameter index does not identify motor relevance (C4) | `VERIFIED` | All fourteen enumerators walked against `Vehicle.cc:2344-2391`. `param6 = 1` spins props, `param6 = 2` does not. **Corrects an earlier claim in this document** |
| Firmware-dependent ambiguity of `param6 = 1` | `VERIFIED` | Upstream comment at `Vehicle.cc:2366`. Resolution `OPEN` |
| Callers create state before the send | `VERIFIED` | Three call sites cited |
| **The enforcement site** | **`IMPLEMENTED-UNCOMPILED`** | **Landed.** `Vehicle.cc:2392-2405`, conformance checked above — 11 of 12 contract items |
| Classification, gated-set declaration, constraint reporting | `IMPLEMENTED-UNCOMPILED` | `AeroraCalibrationAuthorization.{h,cc}`; no compiler on the host (`D-006`) |
| Audit records all seven parameters | `IMPLEMENTED-UNCOMPILED` | `AeroraCommandAuthorizer::_audit()` |
| Machine-readable refusal at the calibration site | **`NOT-EVIDENCED`** | `mavCommandResult` not emitted; one-line addition recorded above |
| Calibration permission mapping | **`OPEN`** | `Q-029`. No role or permission name exists |
| Wire-level denial, as a written test | `IMPLEMENTED-UNCOMPILED` | `AeroraCalibrationEnforcementTest`, 4 cases against `MockLink`'s received-command counters |
| Wire-level denial, as an **executed** result | **`BLOCKED`** | `Q-035` — the test exists but no lane runs an overlay build with `QGC_BUILD_TESTING=ON`, and `D-006` blocks compiling it here |
| Partial-state rollback in the calling controllers | **`OPEN`** | Unowned; `src/AutoPilotPlugins/**` is assigned to no workstream |

**The calibration path now reaches a decision point; no decision is evaluated.** That is a real
advance over the bypass and is not the same as being authorized. Two things still stand between this
and "calibration is authorized": `Q-029`, and the fact that an unconfigured station allows.

### Related

[ADR-015](32-integration-adrs.md) (the seam that implements this policy's enforcement point — read
the two together), `ADR-012` (the scope gap), [ADR-014](30-outbound-command-security-audit.md)
(outbound scope, `OPEN`, Tab 1), `D-001`, `Q-029` (role set, `OPEN`), `Q-035` / `R-021` (overlay test
lane), `D-006` (build host), `AERORA-SEC-REQ-012`, `IF-SEC-005` (gated-operation set),
`AERORA-COMPLY-REQ-002`, and the Vehicle workstream's options analysis in
`aerora/src/Vehicle/README.md`.

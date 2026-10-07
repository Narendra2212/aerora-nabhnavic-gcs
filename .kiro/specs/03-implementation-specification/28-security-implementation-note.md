# Security Implementation Note — Layer 1 as-built

Records what the security foundation **actually is** in the working tree, as distinct from what the
Phase 4 specification says it should become. Docs 12, 13 and 14 are the specification; `ADR-012` and
`ADR-013` are the decisions; this note is the as-built state and its gaps.

Verified against `HEAD = 9459a4b57986f40df8bdc193d5453c0596fbb416`, branch `master`.

## One-paragraph summary

Enforcement **points** for command authorization exist at both `MAV_CMD` emitters in the tree — the
queue (`ADR-012`) and the calibration direct send (`ADR-015`, Tab 1) — and are correct for their
scope. The enforcement **policy** does not exist, because the operator role set (`Q-029`) is open,
so today every command is allowed and a warning is logged. The signing-key storage **abstraction**
exists and is byte-compatible with stock QGC; no platform protection backend exists, so key material
remains unprotected at rest exactly as in stock QGC. Nothing in this workstream has been compiled or
executed: the host has no Qt, no CMake and 0.2 GB of free disk (`D-006`).

The distinction that matters for reading any status below: **reaching a decision point is not being
authorized.** Both gates are real and both currently allow everything.

## Current architecture

```text
Operator
   │                        identity: AeroraIdentityProvider          OPEN format
   ▼
Role Resolution             AeroraRoleResolver                        Q-029 OPEN
   ▼
Authorization Policy        AeroraAuthorizationPolicy                 Q-029 OPEN
   ▼
AeroraCommandAuthorizer     sequences the chain, fails closed         IMPLEMENTED-UNCOMPILED
   ▼
AeroraCorePlugin            ::authorizeMavCommand override            IMPLEMENTED-UNCOMPILED
   ▼
QGCCorePlugin               ::authorizeMavCommand virtual, CORE EDIT  defaults to allow
   ▼
MavCommandQueue::sendWorker ENFORCEMENT POINT, CORE EDIT              IMPLEMENTED-UNCOMPILED
   ▼
MavCommandQueue::_sendFromList
   ▼
MAVLink
   ▼
Flight controller           independent second gate, NOT ours
```

Two properties of this shape are deliberate and should survive review:

- The core seam has **no dependency on Aerora**. `QGCCorePlugin::authorizeMavCommand` defaults to
  `true`, so stock QGC behaviour is unchanged and the edit is upstream-contributable.
- Fail-closed lives in **one** place, `AeroraCommandAuthorizer::authorize()`. No caller can
  reconstruct a permissive path by assembling the collaborators differently.

## Command call graph, verified

```text
Vehicle::sendMavCommand                       ─┐
Vehicle::sendMavCommandDelayed                 │
Vehicle::sendMavCommandInt                     │  7 public variants
Vehicle::sendMavCommandWithHandler             ├─ src/Vehicle/Vehicle.cc
Vehicle::sendMavCommandIntWithHandler          │
Vehicle::sendMavCommandWithLambdaFallback      │
Vehicle::sendMavCommandIntWithLambdaFallback  ─┘
          ▼
MavCommandQueue::sendCommand / sendCommandDelayed / sendCommandInt
                / sendCommandWithHandler / sendCommandIntWithHandler
          ▼
MavCommandQueue::sendWorker()          ← ALL paths converge here
          │  [1] _stopped check
          │  [2] AUTHORIZATION GATE
          │  [3] MAV_COMP_ID_ALL / duplicate checks
          │  [4] primary-link check
          │  [5] _list.append(entry)
          ▼
MavCommandQueue::_sendFromList(index)  ← exactly two callers
          │                              sendWorker (first attempt)
          │                              _responseTimeoutCheck (retries)
          ▼
Vehicle::sendMessageOnLinkThreadSafe → the wire
```

`sendCommandDelayed` schedules a `QTimer::singleShot` that calls `sendCommand`, so a delayed command
is evaluated **when it fires**, not when it is scheduled. `sendCommandWithLambdaFallback` routes
through one of the gated entry points in every branch. `RequestMessageCoordinator` and similar
coordinators call `sendWorker` directly, which is the documented full-control entry point and is
still gated.

## Selected enforcement chokepoint

`MavCommandQueue::sendWorker()`, at enqueue time, immediately after the `_stopped` check and before
`_list.append()`.

| Question from the directive | Answer |
| --- | --- |
| Earliest reliable enforcement point | `sendWorker()`. Everything earlier is a per-caller wrapper, so an earlier gate would have to be replicated seven times and could be bypassed by the direct `sendWorker` entry point |
| Latest reliable enforcement point | `_sendFromList()`, immediately before the encode |
| Which point guarantees no transmission | `sendWorker()` before `_list.append()`. `_sendFromList()` only ever indexes `_list`, and `_responseTimeoutCheck()` only ever iterates `_list`, so a command that never becomes an entry is unreachable from the transmit stage |
| Retries | Cannot carry a denied command: it was never queued. Retry exposure is instead the *post-enqueue revocation* case below |
| Delayed commands | Gated at fire time via `sendCommand` → `sendWorker` |
| Queued commands | A denied command is never queued, so there is no queued-denied state to bypass |
| Authorization changes after enqueue | **Residual risk, bounded.** Entries are not re-authorized before a retry |
| Bypass through another path | **Yes, one, verified** — see `ADR-012`'s scope section |

### Why not `_sendFromList()`

Gating at the transmit stage would structurally violate two of the directive's denial requirements:
the entry would remain queued, exhaust `maxTries`, and surface as
`MavCmdResultFailureNoResponseToCommand` — a refusal presented to the operator as an unresponsive
vehicle — and `_responseTimeoutCheck()` would re-evaluate and re-deny it on every expired ack window.

### Post-enqueue revocation, quantified

`_shouldRetry()` returns `true` for exactly four production commands —
`MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES`, `MAV_CMD_REQUEST_MESSAGE`, `MAV_CMD_PREFLIGHT_STORAGE`,
`MAV_CMD_RUN_PREARM_CHECKS` — plus MockLink commands under `QT_DEBUG`. Everything else gets
`maxTries = 1`, and `sendWorker()` calls `_sendFromList()` synchronously, so for every actuating
command there is **no window at all** between the decision and the transmission.

The exposure is therefore confined to four status/request commands, none of which actuate. A
transmit-time re-check is the mitigation if that is ever judged insufficient; it is not implemented
because it is beyond the approved minimum and reintroduces the two failure modes above.

## Authorization interface

In `aerora/src/Security/AeroraAuthorization.h`. **No role name, permission name or policy rule
appears anywhere in it**, and nothing in it may grow a hard-coded role comparison.

| Type | Responsibility |
| --- | --- |
| `AeroraOperatorIdentity` | Opaque id plus an authenticated flag. Not a credential. Valid only when authenticated and non-empty |
| `AeroraRoleAssignment` | Opaque role tokens plus a `resolved` flag. "Resolved to no roles" is a valid authority-free answer, distinct from "unresolved" |
| `AeroraVehicleContext` | MAVLink system id plus a `resolved` flag. **Not** the Aerora fleet identity, which is `OPEN` |
| `AeroraGatedOperation` | The `MAV_CMD` as `int`, target component, command-int flag, all seven params, and the vehicle context |
| `AeroraAuthorizationOutcome` | `Allowed` plus five distinct denial causes, so audit records a cause rather than a bare boolean |
| `AeroraAuthorizationDecision` | Outcome plus a machine-readable `reasonCode` |
| `AeroraIdentityProvider` | Injected. Supplies the current identity |
| `AeroraRoleResolver` | Injected. Maps identity to opaque roles. Must return unresolved rather than guess |
| `AeroraAuthorizationPolicy` | Injected. The only place a role matrix will ever live. `isConfigured()` exists so absence of policy is not permission |

Params are included deliberately: arm and disarm differ only in `param1`, so a policy that cannot
see parameters cannot decide correctly.

### Existing QGC role vs Aerora role model

These are **not** equated anywhere. QGC's advanced-UI toggle (`showAdvancedUI`) is a settings-backed
*visibility preference* that anyone at the keyboard can turn on. It is not authentication, not
authorization and not access control. The Aerora role model is `Q-029` and `OPEN`. No gated
operation derives a permission from the toggle.

## Denial path

| Property | How it is achieved |
| --- | --- |
| Deterministic | A single `if` in `sendWorker()`; same inputs give the same outcome |
| Prevents transmission | Returns before `_list.append()` |
| Observable | `MavCmdResultFailureAuthorizationDenied` via the caller's `resultHandler`, or the `commandResult` signal when there is none; `MAV_RESULT_DENIED` as the ack result |
| Not a transport timeout | A distinct failure code, asserted distinct from `NoResponseToCommand` and `DuplicateCommand` by test |
| Non-retrying | Never queued, so no retry mechanism can observe it |
| Local | No network participates. Cloud is not in the decision path |
| Auditable | `AeroraCommandAuthorizer` emits `CommandRequested`, `AuthorizationDecision` and `CommandDenied` |
| Causes no aircraft behaviour | No RTL, land, hold, disarm, mission termination, payload change or power change |

Operator-facing wording is the existing QGC `showAppMessage` mechanism with a minimal string; no new
UI vocabulary is invented.

## Audit interface

In `aerora/src/Security/AeroraAuditSink.h`. Interface only — there is no writer and no store.

The six questions the directive requires, and the field that answers each:

| Question | Field | If unavailable |
| --- | --- | --- |
| Who/what requested it | `operatorId` (opaque) | Empty. Recorded as a gap, never backfilled |
| What was requested | `detail` — `mavCommand`, `targetCompId`, `commandInt`, `param1` | — |
| When | `utc` | — |
| Allowed or denied | `type` — `CommandRequested` / `AuthorizationDecision` / `CommandDenied` kept distinct | — |
| Why | `reasonCode` from the policy | Empty on allow |
| Which vehicle | `vehicleMavlinkSystemId` + `vehicleResolved` | `vehicleResolved == false`. **Never substituted with a plausible id** |

Two honesty mechanisms are built into the interface:

- `isDurable()` and `isTamperEvident()` both default to **false**, so an in-memory or log-only sink
  can never be mistaken for an evidential record. Tamper-evidence stays false until `Q-037` /
  `Q4-003` resolves and a mechanism is implemented and tested.
- `AeroraCommandAuthorizer::droppedAuditRecordCount()` is a monotonic count of records lost because
  no sink was installed. A gap with a count is usable evidence; a silent gap is not.

### Deliberately not decided

**Must an unauditable *allow* be denied?** The specification fixes fail-closed behaviour for the
*authorizer* but says nothing about the *sink*. Making an uninstalled sink deny every command would
be inventing a security policy, so the authorizer counts the loss and does not block. Recorded
`OPEN`; it needs a product decision, not an engineering one.

`vehicleMavlinkSystemId` is the only vehicle identifier available at the gate. It is explicitly
**not** a durable fleet identifier and no consumer may derive one from it — Aerora fleet identity is
`OPEN`.

## Signing-key storage abstraction

| Aspect | Detail |
| --- | --- |
| Existing behaviour | `_save()` wrote raw 32-byte key material to `QSettings` at `MAVLinkSigningKeys/keys/<name>`; `_load()` read it back. No encryption, no platform keystore |
| Abstraction boundary | `MAVLinkSigningKeyStore` — key **secrets only**. The name manifest and per-key timestamps stay in `QSettings` because neither is a secret, which keeps the core change minimal |
| Backend contract | `storeKey` / `loadKey` / `removeKey` / `flush`, plus `protectsAtRest()` and `backendName()`. Seven explicit result values including `ProtectionUnavailable` |
| Default backend | `MAVLinkSigningKeyStoreQSettings` — reproduces prior behaviour at the identical settings path, and reports `protectsAtRest() == false` |
| Installation point | `MAVLinkSigningKeys::setKeyStore()`, called from `AeroraCorePlugin::init()` |
| Migration | **None required, in either direction.** Paths and encoding are unchanged, verified against `HEAD` |
| Failure behaviour | A key the backend refuses is **not listed in the manifest**, so the manifest never claims material that cannot be read back |
| Compatibility | Stock QGC is neither regressed nor silently "fixed" |
| Cross-platform | The abstraction names no platform API. `AeroraPlatformKeyProtector::createForCurrentPlatform()` returns `nullptr` on Windows, Linux, macOS, Android and iOS alike — no platform is the implicit reference because none is implemented |
| Security implication | **Key material is still unprotected at rest.** The abstraction is in place; its benefit is not |

`AeroraSigningKeyStore` refuses to write when no protector is available, returning
`ProtectionUnavailable` rather than falling back to plaintext — the decision to accept unprotected
storage belongs to the installing policy, not the backend. Because no protector exists,
`AeroraCorePlugin::_installSigningKeyStore()` leaves the stock backend in place and warns, rather
than installing a store that would refuse every write and break signing outright.

`QGC::secureZero` hygiene is preserved on every path, including inside both backends.

## Core modifications

Five core files. Per the directive, each is recorded in full.

### 1. `src/API/QGCCorePlugin.h` — `authorizeMavCommand`

| Field | Value |
| --- | --- |
| Function | New `virtual bool authorizeMavCommand(...)`, defaults to `true` |
| Current behaviour | The class had an **incoming** hook (`mavlinkMessage`) and no outgoing authorization point |
| Problem | Without a seam, policy would have to live in the core tree |
| Required change | One virtual, mirroring `mavlinkMessage()`'s `true`-allows convention |
| Why this location | `QGCCorePlugin` is QGC's documented application-level extension point, so the edit is idiomatic and upstreamable |
| Affected interfaces | `QGCCorePlugin` public API, additively |
| Security impact | None by itself — it is a decision hook that must not send or mutate state |
| Upstream drift | Additive; contributable upstream |
| Test strategy | Default-allow asserted for takeoff, arm and mode-change; null-vehicle safety asserted |
| Rollback | Delete the virtual |

### 2. `src/Vehicle/VehicleTypes.h` — denial failure code

| Field | Value |
| --- | --- |
| Change | Add `MavCmdResultFailureAuthorizationDenied` to `MavCmdResultFailureCode_t` |
| Problem | Reusing `NoResponseToCommand` would present a refusal as an unresponsive vehicle |
| Why this location | The enum is the existing vocabulary for command failure causes |
| Security impact | Makes a refusal distinguishable from a transport failure. **No MAVLink enum is modified or invented** |
| Upstream drift | Additive enumerator |
| Test strategy | Asserted distinct from all three existing codes, and asserted to render |
| Rollback | Delete the enumerator |

### 3. `src/Vehicle/MavCommandQueue.cc` — the gate

| Field | Value |
| --- | --- |
| Function | `sendWorker()` |
| Current behaviour | Validated, built an entry, appended it, transmitted |
| Problem | No authorization evaluation existed on any outgoing command path |
| Required change | One guarded block: consult the hook; on refusal report the new code and return before `_list.append()` |
| Why this location | The sole convergence point of every queue entry point, and before the only mutation that makes a command reachable by the transmit stage |
| Affected interfaces | None. Behaviour is identical while the hook returns `true` |
| Security impact | **This is the enforcement boundary.** Its scope limits are enumerated in `ADR-012` |
| Upstream drift | The only behavioural core edit; a single block at the top of one function |
| Test strategy | Reporting contract tested in a stock build; "denied is never queued" needs an overlay build (`Q-035`) |
| Rollback | Delete the block |

### 4 & 5. `src/MAVLink/Signing/MAVLinkSigningKeys.{h,cc}` — backend delegation

| Field | Value |
| --- | --- |
| Functions | `_save()`, `_load()`, plus `setKeyStore()` / `keyStore()` |
| Current behaviour | `QSettings` default-constructed inline inside both functions |
| Problem | Storage mechanism hard-coded; no seam for platform protection |
| Required change | Route key **bytes** through the backend; leave manifest, timestamps, derivation, trial-matching and the QML model untouched |
| Affected interfaces | Additive statics only; no existing public API changed |
| Security impact | Enables protection without claiming it. Default backend declares `protectsAtRest() == false` |
| Upstream drift | Natural upstream contribution, which would reduce drift to zero |
| Test strategy | Round-trip, not-found, empty-name, remove, corrupt-material, injected-backend and refusal-not-in-manifest |
| Rollback | Revert both files, delete the two additive ones. No data migration either way |

Additive core files, no existing behaviour touched:
`src/MAVLink/Signing/MAVLinkSigningKeyStore.{h,cc}` and the two lines registering them in
`src/MAVLink/Signing/CMakeLists.txt`.

## Security implications

| Implication | Assessment |
| --- | --- |
| A real enforcement point now exists for queued `MAV_CMD` traffic | Correct, and bounded as stated |
| `Vehicle::startCalibration()` could emit a `MAV_CMD` without evaluation | **Closed as a bypass.** Tab 1 landed the enforcement site (`ADR-015`); the policy is `ADR-017`. Every `MAV_CMD` emitter now reaches a decision point. One divergence: the site emits no `mavCommandResult`, so the refusal is not machine-readable |
| Mode change coverage is firmware-dependent | **Known gap**, `ADR-012` |
| `PARAM_SET` is an ungated parallel authority channel | **Known gap**, `ADR-012`. Broadest reconfiguration path in the application |
| Mission items carry `MAV_CMD` values the gate never sees | **Known gap**, `ADR-012` |
| No authorization decisions are made today | `Q-029` open. The gate allows and warns |
| Signing keys remain unprotected at rest | `F3.1-01`, `R-005`. Unchanged from stock QGC |
| No durable or tamper-evident audit record exists | `IF-LOG-020` `BLOCKED`, `Q-037` |
| An unauditable allow is not blocked | `OPEN` by deliberate refusal to invent policy |
| Stock QGC security posture | Unchanged. Every core edit defaults to prior behaviour |

## Unresolved decisions

| Id | Subject | Effect while open |
| --- | --- | --- |
| `Q-029` | Operator role set | No authorization is evaluated; both gates allow and warn. Also blocks the calibration permission mapping — `ADR-017` |
| `ADR-014` (Tab 1) | Outbound authorization scope beyond `MAV_CMD` | `PARAM_SET`, mission writes, `MANUAL_CONTROL`, RC override and the firmware fallbacks remain ungated |
| — | Calibration refusal is not machine-readable | `mavCommandResult` not emitted at the calibration site; blocks the controller-local partial-state rollback — `ADR-017` |
| — | Calibration partial-state rollback | Callers in `src/AutoPilotPlugins/**` leave state behind on refusal. **Newly reachable** now that refusal is possible. No owning workstream |
| — | Firmware type in the authorization context | `param6 = 1` means motor-interference calibration on APM and deprecated airspeed on PX4, so motor relevance is not decidable from the parameters alone — `ADR-017` constraint C4. Narrower than `Q-029` and answerable independently |
| `Q-037` / `Q4-003` | Audit integrity mechanism | No tamper-evident record; both sink flags stay false |
| `Q-035` / `R-021` | Overlay test configuration in CI | The end-to-end denial test cannot run anywhere |
| `D-006` | Build host | Nothing can be compiled or executed |
| — | Aerora fleet identity | Audit records the MAVLink system id and labels it as not-the-fleet-identity |
| — | Must an unauditable allow deny? | Records are counted as dropped; commands are not blocked |
| — | Platform key protection per OS | No backend on any of the five target platforms |
| — | Message-level gate for non-`MAV_CMD` authority traffic | Requires a decision beyond `D-001` |

## Testing plan and status

Nothing below is `VERIFIED`. The host has no Qt, no CMake and 0.2 GB free disk.

### Authorization

| # | Test | Where | Status |
| --- | --- | --- | --- |
| 1 | Authorized command is transmitted | `_testConfiguredChainAllows` (chain), `_testDefaultHookAuthorizesCommands` (core) | `IMPLEMENTED-UNCOMPILED` |
| 2 | Unauthorized command is not transmitted | End-to-end form needs an overlay build | `IMPLEMENTATION-PLANNED` — `Q-035` |
| 3 | Decision is evaluated at the intended point | Code-structure verified; asserted indirectly by 1 and 7 | `IMPLEMENTED-UNCOMPILED` |
| 4 | Queued unauthorized command cannot bypass | Reduces to 2: a denial never becomes an entry | `IMPLEMENTATION-PLANNED` — `Q-035` |
| 5 | Retried command cannot bypass | Same; plus the bounded-revocation analysis above | `IMPLEMENTATION-PLANNED` — `Q-035` |
| 6 | Delayed command cannot bypass | Gate runs at fire time, not schedule time | `IMPLEMENTATION-PLANNED` — `Q-035` |
| 7 | Authorization failure is auditable | `_testDenialIsAuditableWithReasonAndVehicle`, `_testUnresolvedVehicleIsNotBackfilled` | `IMPLEMENTED-UNCOMPILED` |
| 8 | Mechanism failure fails safely | `_testUnconfiguredChainDenies`, `_testMissingCollaboratorDenies` (3 rows), `_testInvalidIdentityDenies`, `_testUnresolvedRoleDenies`, `_testMissingAuditSinkIsAccountedNotSilent` | `IMPLEMENTED-UNCOMPILED` |
| — | "Resolved to no roles" is not collapsed into "unresolved" | `_testResolvedButEmptyRoleSetReachesPolicy` — an empty-but-resolved set must reach the policy and be denied *by policy with a policy reason*, not short-circuited by the plumbing. Doc 14 requires such an identity to be denied; this asserts **who** denies it | `IMPLEMENTED-UNCOMPILED` |

Tests 2, 4, 5 and 6 all reduce to one observable: after a denied send, `MavCommandQueue::isPending()`
is false. That requires `QGCCorePlugin::instance()` to resolve to a denying subclass, which is fixed
at compile time, so an overlay build with `QGC_BUILD_TESTING=ON` is a hard precondition.

### Signing storage

| # | Test | Where | Status |
| --- | --- | --- | --- |
| 1 | Save key | `_testDefaultBackendRoundTrip`, `_testInjectedBackendReceivesKeyMaterial` | `IMPLEMENTED-UNCOMPILED` |
| 2 | Load key | `_testDefaultBackendRoundTrip` | `IMPLEMENTED-UNCOMPILED` |
| 3 | Missing key | `_testDefaultBackendReportsNotFound`, `_testDefaultBackendRemoveKey` | `IMPLEMENTED-UNCOMPILED` |
| 4 | Storage failure | `_testFailingBackendIsNotListedInManifest` | `IMPLEMENTED-UNCOMPILED` |
| 5 | Backend unavailable | Same test, via `ProtectionUnavailable` | `IMPLEMENTED-UNCOMPILED` |
| 6 | Invalid / corrupt stored data | `_testCorruptStoredMaterialCannotBecomeAKey` — 5 data rows | `IMPLEMENTED-UNCOMPILED` |
| 7 | Cross-platform abstraction behaviour | `_testKeyStoreDefaultsToQSettings`, `_testDefaultBackendDoesNotClaimProtection`, `_testResultToStringIsNeverUnknown` | `IMPLEMENTED-UNCOMPILED` |

The Aerora backend itself is covered separately by `aerora/test/Security/AeroraSigningKeyStoreTest`
(10 cases): refusal with no or unavailable protector leaving **no** stored material, removal still
permitted without a protector, wrap-before-write proven by asserting the persisted bytes neither
equal nor contain the key, `CorruptData` on unwrap failure with the output buffer cleared, failed
wrap writing nothing, path separation from the stock backend, empty-name rejection, and a test that
pins `createForCurrentPlatform()` at `nullptr` so adding a platform backend cannot silently change
the protection-at-rest claims. The protector used is a reversible test double, **not** cryptography.

### Calibration authorization — `ADR-017`

| Area | Test | Status |
| --- | --- | --- |
| Classification | `_testClassification` (7 rows: cancel, `param1`, `param7` ESC, `param6` compass-mot, two-param APM pre-flight, non-unit value, NaN) | `IMPLEMENTED-UNCOMPILED` |
| Cancel-blocking constraint | `_testCancelIsFlaggedAsBlockingCancel` | `IMPLEMENTED-UNCOMPILED` |
| Gated-set is declared, not derived | `_testCalibrationIsInTheDeclaredGatedSet`, `_testNonCalibrationCommandIsNotClassified` | `IMPLEMENTED-UNCOMPILED` |
| **Parameter index does not identify motor relevance** (constraint C4) | `_testParamIndexDoesNotIdentifyMotorRelevance` — pins that `param6 = 1` (props spin) and `param6 = 2` (airspeed) are structurally identical, that the two motor-relevant calibrations sit in different parameters, and that `param5` carries three types by value alone | `IMPLEMENTED-UNCOMPILED` |
| Authorized / denied / fail-closed | `_testAuthorizedCalibrationIsAllowed`, `_testDeniedCalibrationIsDeniedWithReason`, `_testUnconfiguredPolicyDeniesCalibration` | `IMPLEMENTED-UNCOMPILED` |
| Audit identifies *which* calibration | `_testAuditIdentifiesWhichCalibrationWasRefused`, `_testAuditDistinguishesCancelFromStart` | `IMPLEMENTED-UNCOMPILED` |

End-to-end enforcement, now that the seam exists —
`aerora/test/Security/AeroraCalibrationEnforcementTest` (`VehicleTest` + MockLink, 4 cases):

| Area | Test | Status |
| --- | --- | --- |
| **Denied calibration puts nothing on the wire** | `_testDeniedCalibrationStartReachesNoWire` — `receivedMavCommandCount(MAV_CMD_PREFLIGHT_CALIBRATION) == 0`, with all three denial messages claimed so a zero cannot be a missed send | `IMPLEMENTED-UNCOMPILED` |
| Positive control for the counter | `_testAuthorizedCalibrationStartReachesWire` | `IMPLEMENTED-UNCOMPILED` |
| Cancel survives a start refusal (constraint C1 + `stopCalibration()` regression, and exercises the **queue** enforcement site) | `_testCancelStillReachesWireWhenStartIsDenied` | `IMPLEMENTED-UNCOMPILED` |
| Audit record carries **which vehicle** and **which calibration**, on the real path | `_testDeniedCalibrationAuditIdentifiesVehicleAndCalibration` | `IMPLEMENTED-UNCOMPILED` |
| The unconfigured-station deviation is pinned | `_testUnconfiguredChainAllowsCalibration` | `IMPLEMENTED-UNCOMPILED` |

This is the test that two earlier sessions specified but could not write: before the seam landed
there was nothing to deny, so the assertion would have passed for the wrong reason.

The vehicle-identity case is the only test that exercises the full production path
`Vehicle::startCalibration` → `AeroraCorePlugin::authorizeMavCommand` → `AeroraCommandAuthorizer` →
`AeroraAuditSink`. Every other test constructs an `AeroraGatedOperation` directly, so none of them
could catch a mistake in the plugin's translation from `Vehicle*` into the operation — which is
precisely where the vehicle identity is attached, and was previously asserted nowhere.

### What was actually executed

| Check | Result |
| --- | --- |
| Compile | **Not possible.** No CMake, no Ninja, no Qt, no `just`, no `pre-commit`, no `clang-format`; 0.2 GB free disk |
| `just lint` / `pre-commit` | **Not possible.** Tooling absent |
| Test execution | **Not possible.** Follows from the above |
| Structural check, 18 files | **Passed.** Balanced brackets, paired `#if`/`#endif`, ≤120 columns, LF endings, final newline, no tabs, no `Q_ASSERT` in production, no `QTest::ignoreMessage`, no fixed-delay `qWait`, test slot declarations match definitions, `_data()` rows paired with their test, `UT_REGISTER_TEST` present, `expect*Message` balanced against `verifyExpectedLogMessage`, `init()`/`cleanup()` overrides chain to their base, and the CMake manifest agrees with both the files on disk and the `add_qgc_test` entries. **This is not a compile and must not be read as one** |

The structural check is a substitute, not a stand-in. It caught one real defect across these sessions
— a 121-column line in `AeroraCommandAuthorizer.cc` that would have failed `clang-format` — and its
remaining rules have so far only confirmed clean files. It cannot detect a type error, a missing
symbol, or a wrong Qt API, so **no file here should be assumed to compile.**

## Integration requirements for the orchestrator

| # | Requirement | Why |
| --- | --- | --- |
| 1 | ~~Apply the calibration core edit~~ — **DONE.** Tab 1 landed it as `ADR-015` | Conformance checked in `ADR-017`: 11 of 12 contract items |
| 2 | **Accept or reject the one-line `mavCommandResult` emission** at the calibration site | The remaining conformance item. Without it the refusal is not machine-readable and the partial-state rollback has no signal to use. **Owner: Tab 1**, since `src/Vehicle/Vehicle.cc` is theirs |
| 3 | Register **two** security tests in `aerora/CMakeLists.txt` | `add_qgc_test(AeroraSigningKeyStoreTest LABELS Unit)` and `add_qgc_test(AeroraCalibrationEnforcementTest LABELS Integration Vehicle)`. Both are listed in `Security.cmake` so they compile and self-register, and are reachable with `--unittest:<name>`, but CTest will not run them. Two lines. **Owner: Tab 1** — that file is forbidden to this workstream |
| 3a | Fix the once-per-process unconfigured warning that will fail whichever overlay vehicle test runs first | `AeroraCorePlugin` warns inside `VehicleTest::init()`, before any test body can claim it. Affects Tab 3's vehicle tests too. Precedent and three options are in `ADR-017`; this workstream suppressed it locally so nothing is broken today |
| 4 | Configure with `-DQGC_CUSTOM_DIR=aerora` to activate either gate at all | `QGCCorePlugin::instance()` routes to `CUSTOMCLASS` only under `QGC_CUSTOM_BUILD` |
| 5 | Resolve `Q-035`: an overlay build leg with `QGC_BUILD_TESTING=ON` | Required for the wire-level denial test, and for any overlay test to run |
| 6 | Treat `AERORA-SEC-REQ-012` as "every `MAV_CMD` emitter reaches a decision point" — **not** as "commands are authorized" | While `Q-029` is open no decision is evaluated, and an unconfigured station allows |
| 7 | Assign the calibration partial-state rollback | `src/AutoPilotPlugins/**` is assigned to no workstream. Newly reachable now that refusal is possible |
| 8 | ADR numbering is shared and collided once | ADR-014/015/016 are Tab 1's (docs 30, 32); this workstream's calibration policy is **ADR-017** in doc 27. Check both documents before claiming the next number |
| 9 | Document number 28 is used twice | Tab 1's `28-layer-1-security-command-boundary.md` and this file. Tab 1 has accepted the renumbering |

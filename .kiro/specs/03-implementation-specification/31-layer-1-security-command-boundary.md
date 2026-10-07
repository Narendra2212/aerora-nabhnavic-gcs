# Phase 4 — Layer 1: Security and Command Boundary Foundation

First implementation layer. Two approved QGC core edits, the Aerora authorization and
key-storage abstractions, and two test targets.

**Nothing here has been compiled.** The build host is `NO-GO` under `D-006`. No compilation,
runtime, integration or execution result is claimed anywhere in this document.

| Field | Value |
| --- | --- |
| QGC HEAD | `9459a4b57986f40df8bdc193d5453c0596fbb416` |
| Tracked core files modified | **8** |
| Core files added | **2** |
| Status | **`IMPLEMENTED-UNCOMPILED`** |
| Tests authored | **2 targets, 14 test functions** |
| Tests executed | **0** — no build host |
| ADRs | ADR-012, ADR-013 |

---

## Changed

### QGC core — modified

| File | Change | ADR |
| --- | --- | --- |
| `src/API/QGCCorePlugin.h` | Added `virtual bool authorizeMavCommand(...)`, defaults to `true` | ADR-012 |
| `src/Vehicle/VehicleTypes.h` | Added `MavCmdResultFailureAuthorizationDenied` | ADR-012 |
| `src/Vehicle/MavCommandQueue.cc` | Authorization gate in `sendWorker()`; new code in `failureCodeToString()`; one include | ADR-012 |
| `src/MAVLink/Signing/MAVLinkSigningKeys.h` | Backend injection API; `kSettingsGroup` now aliases the store's constant; `kKeySubgroup` removed | ADR-013 |
| `src/MAVLink/Signing/MAVLinkSigningKeys.cc` | `_save()` and `_load()` delegate key bytes to the backend | ADR-013 |
| `src/MAVLink/Signing/CMakeLists.txt` | Registers the new store source | ADR-013 |
| `test/Vehicle/CMakeLists.txt` | Registers `MavCommandAuthorizationTest` | — |
| `test/MAVLink/Signing/CMakeLists.txt` | Registers `MAVLinkSigningKeyStoreTest` | — |

Measured diff against `HEAD`: **6 source files, 106 insertions, 5 deletions** before the test
CMake registrations. Line endings verified LF, no CRLF introduced.

### QGC core — added

| File | Purpose |
| --- | --- |
| `src/MAVLink/Signing/MAVLinkSigningKeyStore.h` | Backend interface, result enum, shared settings group |
| `src/MAVLink/Signing/MAVLinkSigningKeyStore.cc` | Default `QSettings` backend, behaviour-preserving |

### Aerora overlay — added

| File | Purpose |
| --- | --- |
| `aerora/CMakeLists.txt` | Overlay contract: `CUSTOM_SOURCES`, includes, `CUSTOMHEADER`/`CUSTOMCLASS` |
| `aerora/cmake/CustomOverrides.cmake` | Required by the contract; deliberately overrides nothing |
| `aerora/src/AeroraCorePlugin.h`, `.cc` | Binds the authorizer and key store to QGC extension points |
| `aerora/src/Security/AeroraAuthorization.h` | Identity, role assignment, gated operation, policy interfaces |
| `aerora/src/Security/AeroraAuditSink.h` | Audit event types and sink interface |
| `aerora/src/Security/AeroraCommandAuthorizer.h`, `.cc` | Fail-closed chain, audit emission |
| `aerora/src/Security/AeroraPlatformKeyProtector.h` | Platform protection seam; no backend implemented |
| `aerora/src/Security/AeroraSigningKeyStore.h`, `.cc` | Aerora key store; refuses rather than storing plaintext |

### Tests — added

| File | Target | Label |
| --- | --- | --- |
| `test/Vehicle/MavCommandAuthorizationTest.h`, `.cc` | `MavCommandAuthorizationTest` | `Unit Vehicle` |
| `test/MAVLink/Signing/MAVLinkSigningKeyStoreTest.h`, `.cc` | `MAVLinkSigningKeyStoreTest` | `Unit MAVLink` |

---

## Command call graph

Traced before editing, as `D-001` required. All seven `Vehicle::sendMavCommand*` variants
(`Vehicle.cc:2146-2193`) delegate to `MavCommandQueue`. Inside the queue every typed wrapper, the
delayed send via `QTimer::singleShot`, and both lambda-fallback paths converge on `sendWorker()`.
Higher-level coordinators call `sendWorker()` directly. `sendWorker()` appends to `_list` then calls
`_sendFromList()`, which encodes and calls `Vehicle::sendMessageOnLinkThreadSafe()`.

`_sendFromList()` has exactly two callers: `sendWorker()` and `_responseTimeoutCheck()` (retries).

## Enforcement point

**`MavCommandQueue::sendWorker()`, at enqueue time, before `_list.append()`.**

`_sendFromList()` was evaluated and rejected. Gating there would have violated two of the five
denial requirements: the queued entry would exhaust `maxTries` and report
`MavCmdResultFailureNoResponseToCommand`, presenting a refusal as an unresponsive vehicle, and
`_responseTimeoutCheck()` would silently re-attempt the denied command on every expired ack window.
Full reasoning in [ADR-012](27-implementation-adrs.md).

| Denial requirement | How it is met |
| --- | --- |
| Prevent transmission | Returns before `_list.append()`; `_sendFromList()` only runs on appended entries |
| Produce an audit event | Emitted by `AeroraCommandAuthorizer`, which owns the decision context |
| Not a transport timeout | Distinct `MavCmdResultFailureAuthorizationDenied`, reported with `MAV_RESULT_DENIED` |
| No silent retry | Never queued, so no retry mechanism can observe it |
| Not alter vehicle authority state | The gate reads only and sends nothing |

### Known scope limit

The gate authorizes **`MAV_CMD` commands only**. Mission upload, parameter writes, mode changes,
RemoteID and log operations reach the wire via `Vehicle::sendMessageOnLinkThreadSafe()` without
passing through `MavCommandQueue`. A message-level gate is the candidate for a broader boundary and
is **not** implemented, because `D-001` approved a command gate and widening scope needs its own
decision. Recorded as a gap, not solved.

### Residual retry window

A command authorized at enqueue is not re-authorized before a retry. Bounded by `_shouldRetry()`,
which returns true for only four production commands — `REQUEST_AUTOPILOT_CAPABILITIES`,
`REQUEST_MESSAGE`, `PREFLIGHT_STORAGE`, `RUN_PREARM_CHECKS` — all status or request operations, never
actuation. Everything else gets `maxTries = 1`.

---

## Signing-key abstraction

Only secret key bytes pass through the backend. The key-name manifest and per-key timestamps hold no
secrets and stay in their existing `QSettings` locations, which keeps the change surgical and avoids
any migration: **the on-disk layout is unchanged in both directions.**

```text
MAVLinkSigningKeys
        |
MAVLinkSigningKeyStore
        |
   +----+------------------------+
   |                             |
QSettings backend          AeroraSigningKeyStore
(default, unprotected,     (requires a platform protector;
 reports protectsAtRest     refuses to write without one)
 == false)
```

A default backend **is** shipped here, unlike the audit-integrity seam which deliberately ships none.
The asymmetry is intentional: stock QGC signing works today and must not break, whereas audit
integrity has never existed and should fail loudly.

### Cross-platform posture

**No platform backend is implemented.** `AeroraPlatformKeyProtector::createForCurrentPlatform()`
returns `nullptr` on every platform. `D-002` approved the abstraction, but `D-006` leaves the host
unable to compile or test a cryptographic backend, and an untested key-protection backend is worse
than none. No platform API is named or assumed.

Consequence, stated plainly: **MAVLink signing keys remain stored without protection at rest, exactly
as in stock QGC.** `AeroraCorePlugin::_installSigningKeyStore()` logs this at startup rather than
installing a store that would refuse every write and break signing. The security posture is unchanged
from the state recorded in `R-005` and `F3.1-01`; what changed is that the seam now exists and the
posture is reported instead of implicit.

---

## Unresolved role dependency

`Q-029` is `OPEN`. No role name, permission name or default role appears in any file. The authorizer
is fail-closed at every stage: missing policy, missing provider, missing resolver, invalid identity
and unresolved role all deny, and only the injected policy can allow.

**The boundary is wired but not armed.** With no policy installed,
`AeroraCorePlugin::authorizeMavCommand()` returns `true` and warns once. That is a recorded gap, not
a design position. The two alternatives were considered and rejected:

| Alternative | Why rejected |
| --- | --- |
| Deny every command | The station becomes unusable with no approved policy in existence |
| Claim a boundary anyway | Dishonest: no evaluation occurs, so there is no boundary to claim |

`AeroraCorePlugin::isAuthorizationEnforced()` exposes the real state so nothing downstream can assume
enforcement that is not happening. Arming it requires `Q-029`.

---

## Tests

| Target | Functions | Asserts |
| --- | --- | --- |
| `MavCommandAuthorizationTest` | 5 | Denial code is distinct from both transport failure codes; all four codes render; the default hook allows commands and never dereferences its `Vehicle*` |
| `MAVLinkSigningKeyStoreTest` | 9 | Default backend round-trip, `NotFound`, empty-name rejection, removal idempotence, never claims protection; every result string renders; the store defaults to `QSettings`; **an injected backend actually receives the key material**; **a failing backend's key is not listed in the manifest** |

### Tests executed: none

`ctest`, CMake, Ninja, a compiler and Qt are all absent. **No test has been run, and no test is
reported as passing.**

### Not testable in a stock build

The end-to-end denial path requires `QGCCorePlugin::instance()` to resolve to an overriding subclass,
which is fixed at compile time via `CUSTOMCLASS`. In a stock build the refusal path is unreachable, so
it is covered only from a custom-build test target — which is `Q-035` / `R-021`, still unresolved.
This is stated in the test header rather than papered over.

---

## Pre-existing content found in `aerora/`

While verifying my own files I found that `aerora/` already contained **39 files I did not author**:

| Area | Contents |
| --- | --- |
| `aerora/src/Telemetry/` | `AeroraTelemetryState`, `AeroraStateValue`, `AeroraFreshness`, `AeroraTelemetrySet`, `AeroraTelemetrySource`, `AeroraFactGroupBase`, `Telemetry.cmake`, `README.md` |
| `aerora/src/Vehicle/` | `AeroraFirmwarePlugin`, `AeroraFirmwarePluginFactory`, `AeroraProductProfile`, `AeroraVehicleCapabilities`, `AeroraVehicleIdentity`, `AeroraVehicleServices`, `AeroraVehicleLog`, `AeroraFirmwareClaim`, `CMakeLists.txt`, `README.md`, plus `Mock/` and `Test/` |
| `aerora/test/Telemetry/` | `AeroraTelemetryStateTest` |

Actions taken: **none.** I did not read them for correctness, adopt them, modify them, wire them or
delete them. They are outside Layer 1's scope, and several cover material that is explicitly deferred
— firmware plugin factory routing (section 12 of the directive, and `Q4-004`), product profiles, and
the capability model.

Two things the reviewer needs to decide:

| # | Issue |
| --- | --- |
| 1 | **CMake integration conflict.** My `aerora/CMakeLists.txt` declares a flat `CUSTOM_SOURCES` list containing only the Layer 1 security files. The pre-existing `aerora/src/Vehicle/CMakeLists.txt` and `aerora/src/Telemetry/Telemetry.cmake` imply a subdirectory-based wiring style. These need reconciling before the overlay can build, and the reconciliation is a design decision, not a mechanical merge |
| 2 | **No recovery point.** `aerora/` is untracked and has never been committed, so if a root `aerora/CMakeLists.txt` existed before mine there is no way to recover it. I cannot rule this out from the evidence available. This is a direct consequence of `D-005` — without an Aerora repository there is no history to fall back on |

---

## Core edit register

Per the core-edit policy. Two edits, both approved, both enumerated.

### Core edit 1 — command authorization enforcement

| Field | Value |
| --- | --- |
| Files | `src/API/QGCCorePlugin.h`, `src/Vehicle/VehicleTypes.h`, `src/Vehicle/MavCommandQueue.cc` |
| Functions | `QGCCorePlugin::authorizeMavCommand()` (new), `MavCommandQueue::sendWorker()`, `MavCommandQueue::failureCodeToString()` |
| Reason | `D-001`: an advisory overlay gate cannot enforce, because stock UI paths bypass it |
| Affected interfaces | `IF-SEC-003`, and `AERORA-SEC-REQ-013` |
| Security impact | Creates the only enforceable command-authorization point. Currently wired, not armed — see the role dependency above |
| Upstream drift | 3 files. The virtual and the enum value are additive and contributable; the only behavioural change is one guarded block at the top of `sendWorker()` |
| Test | `MavCommandAuthorizationTest`, authored, not executed |
| Rollback | Revert 3 files. The hook defaults to `true` and the enum value is additive, so stock behaviour returns exactly. No persisted state, no migration |
| ADR | ADR-012 |

### Core edit 2 — signing-key storage backend

| Field | Value |
| --- | --- |
| Files | `src/MAVLink/Signing/MAVLinkSigningKeys.h`, `.cc` modified; `MAVLinkSigningKeyStore.h`, `.cc` added; `CMakeLists.txt` updated |
| Functions | `MAVLinkSigningKeys::_save()`, `_load()`, plus new `setKeyStore()` / `keyStore()` |
| Reason | `D-002` / `F3.1-01`: the class has no virtual members, so no overlay-side seam existed |
| Affected interfaces | `IF-SEC-013`, and `AERORA-SEC-REQ-017` |
| Security impact | Makes protection at rest possible and confined to the key store. Does not by itself protect anything: no platform backend exists |
| Upstream drift | 2 modified, 2 added. The abstraction is contributable, which would reduce drift on the modified pair to zero |
| Test | `MAVLinkSigningKeyStoreTest`, authored, not executed |
| Rollback | Revert 2 files, delete 2. Settings layout unchanged, so keys stay readable either way. No migration |
| ADR | ADR-013 |

No other core edit was made. The six radio transport edits remain `DEFERRED`.

---

## Verification actually performed

| Check | Result |
| --- | --- |
| `git diff --stat HEAD` reviewed | 6 source files, 106 insertions, 5 deletions |
| Line endings on all new and edited files | **LF, zero CRLF** |
| `Q_ASSERT` in new code | **0** |
| `QTest::ignoreMessage` in new tests | **0** |
| Fixed-delay `qWait(<n>)` in new tests | **0** |
| Trailing whitespace | **0** |
| QGC HEAD | Unchanged |
| Compilation | **NOT PERFORMED** — no compiler, no CMake, no Ninja, no Qt |
| `clang-format` / `clang-tidy` / `pre-commit` | **NOT RUN** — absent from the host |
| Tests | **NOT RUN** — no `ctest` |

---

## Remaining blockers

| # | Blocker | Effect |
| --- | --- | --- |
| 1 | **`D-006` build host `NO-GO`** | Nothing can be compiled or tested. Every status here is `IMPLEMENTED-UNCOMPILED` |
| 2 | **`D-005` Aerora Git origin `BLOCKED`** | No commit can be made. `origin` still pushes to `mavlink/qgroundcontrol`, and `aerora/` has no history |
| 3 | **`Q-029` role set `OPEN`** | The authorization boundary is wired but performs no evaluation |
| 4 | **`D-002` platform backends absent** | Signing keys remain unprotected at rest, as in stock QGC |
| 5 | **`Q-037` / `Q4-003` `OPEN`** | Audit sink reports neither durable nor tamper-evident, correctly |
| 6 | **`Q-035` / `R-021` `OPEN`** | The denial path is not reachable from a stock test target |
| 7 | **`D-007` §27 `EXTERNAL EVIDENCE REQUIRED`** | No product feature work may proceed |
| 8 | **`D-004` radio `DEFERRED`** | Unchanged; no transport work attempted |
| 9 | **Pre-existing `aerora/` content** | CMake wiring conflict needs a reviewer decision before the overlay builds |

---

PHASE 4 IMPLEMENTATION — SECURITY AND COMMAND BOUNDARY FOUNDATION COMPLETE — READY FOR HUMAN REVIEW

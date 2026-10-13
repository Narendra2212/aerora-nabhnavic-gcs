# Requirements-to-Test Traceability Matrix

Owner: **Tab 1**. Requirement â†’ component â†’ test, for every requirement in
[01 Â§5](01-product-requirements-and-completion-plan.md#5-requirement-blocks).

**No test in this matrix has been executed.** Test columns record what *would* verify the
requirement, not what has. Status vocabulary: [00-README](00-README.md#status-vocabulary).

## Contents

- [1. How to read this](#1-how-to-read-this)
- [2. Traceability by requirement block](#2-traceability-by-requirement-block)
- [3. Coverage analysis](#3-coverage-analysis)
- [4. Requirements no test can verify](#4-requirements-no-test-can-verify)
- [5. Tests with no requirement](#5-tests-with-no-requirement)
- [6. Tests to be written](#6-tests-to-be-written)
- [7. Verification method by requirement class](#7-verification-method-by-requirement-class)

## 1. How to read this

| Column | Meaning |
| --- | --- |
| **Requirement** | The ID from [01 Â§5](01-product-requirements-and-completion-plan.md#5-requirement-blocks) |
| **Component** | The file or type that implements it, or `â€”` if none exists |
| **Test** | The test that would verify it. `TBW` = to be written. `â€”` = no test can verify it, see [Â§4](#4-requirements-no-test-can-verify) |
| **Method** | `UT` unit Â· `IT` integration Â· `QML` Qt Quick Test Â· `STATIC` source check Â· `DEMO` human observation Â· `HW` hardware |
| **Status** | Of the requirement, not of the test |

A requirement whose component is `â€”` and whose test is `TBW` is not a gap in the matrix; it is a gap
in the product, and it is the matrix's job to show it.

## 2. Traceability by requirement block

### Operating modes

| Requirement | Component | Test | Method | Status |
| --- | --- | --- | --- | --- |
| `P7-MOD-001` manual input, no interposition | upstream QGC manual control | â€” (upstream) | IT | `IMPLEMENTED` |
| `P7-MOD-002` guided actions gated on reported capability | `AeroraVehicleCapabilities` | `AeroraVehicleIntegrationTest` (partial) + `TBW` | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-MOD-003` no GCS-side failsafe substitution | *absence of* any failsafe code | `AeroraUiSafetyTest` (no actuating reach) + `STATIC` | STATIC | `IMPLEMENTED-UNCOMPILED` |
| `P7-MOD-010` `PROPOSED` â‰  `REQUESTED` | `AeroraTelemetryState` | `AeroraTelemetryStateTest` | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-MOD-011` proposal carries source + freshness + basis | â€” | `TBW` | UT | `SPECIFIED-NOT-DESIGNED` |
| `P7-MOD-012` explicit per-proposal approval, no standing approval | â€” | `TBW` | UT + QML | `SPECIFIED-NOT-DESIGNED` |
| `P7-MOD-013` stale proposal not approvable | `AeroraFreshness` (mechanism) | `TBW` | UT | `OPEN` â€” `Q-036` has no values |
| `P7-MOD-014` four distinct recorded events | `AeroraAuditSink` | `TBW` | UT | `CONTRACT-ONLY` |
| `P7-MOD-020` autonomy needs capability **and** authorization | `AeroraVehicleCapabilities` + policy | `TBW` | IT | `BLOCKED` â€” `Q-029` |
| `P7-MOD-021` absent capability = `UNKNOWN`, not permissive | `AeroraTelemetryState` | `AeroraTelemetryStateTest` | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-MOD-022` abort path always present, availability displayed | â€” | `TBW` | IT + DEMO | `SPECIFIED-NOT-DESIGNED` |
| `P7-MOD-023` no self-initiated autonomous entry | *absence of* | `TBW` | STATIC | `DESIGNED` |
| `P7-MOD-030` displayed mode is `EFFECTIVE` | `AeroraTelemetryState` + `AeroraStateText` | `AeroraTelemetryStateTest` + `TBW` QML | UT + QML | `IMPLEMENTED-UNCOMPILED` / `DESIGNED` |

### Architecture boundaries

| Requirement | Component | Test | Method | Status |
| --- | --- | --- | --- | --- |
| `P7-ARCH-001` no control loop or failsafe in the station | *absence of* | `STATIC` + `AeroraUiSafetyTest` | STATIC | `IMPLEMENTED-UNCOMPILED` |
| `P7-ARCH-002` GPU proposes, FC decides | â€” | `TBW` | IT | `SPECIFIED-NOT-DESIGNED` |
| `P7-ARCH-003` refusal displayed, no station retry or translation | â€” | `TBW` | UT | `SPECIFIED-NOT-DESIGNED` |
| `P7-ARCH-004` station-derived values labelled | `AeroraDerivedBadge` | `TBW` QML | QML + DEMO | `DESIGNED` |
| `P7-ARCH-005` no remaining-energy estimate | `kRemainingEnergyEstimationImplemented = false` | `AeroraPowerModelTest` | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-ARCH-010` full function with no network | â€” | `TBW` | IT | `DESIGNED` |
| `P7-ARCH-011` no cloud input to authorization | `AeroraCommandAuthorizer` (no network dependency) | `STATIC` + `TBW` | STATIC | `IMPLEMENTED-UNCOMPILED` |
| `P7-ARCH-012` cloud absence is a state, not an error | â€” | `TBW` | QML | `DESIGNED` |
| `P7-ARCH-013` no outbound transmission without operator action | *absence of* | `STATIC` | STATIC | `DESIGNED` |
| `P7-ARCH-020` QGC behaviour preserved | the 9 core files | `ctest -L Unit` (full upstream suite) | IT | `IMPLEMENTED-UNCOMPILED` |
| `P7-ARCH-021` core changes minimal and enumerated | [04 Â§6](04-subsystem-and-interface-completion-matrix.md#6-qgroundcontrol-core-modifications) | `git diff --stat` review | STATIC | `IMPLEMENTED-UNCOMPILED` |
| `P7-ARCH-022` no invented MAVLink identifier | â€” | `aerora-ui-static-check.ps1` (hard-coded MAVLink id check) | STATIC | `IMPLEMENTED-UNCOMPILED` |
| `P7-ARCH-023` rebase stays mechanical; class-X forks registered | tier-3 count = 0 | `AeroraUiFoundationTest` (allow-list + forbidden paths) | UT | `IMPLEMENTED-UNCOMPILED` |

### Safety, authorization, auditability, no-invention

| Requirement | Component | Test | Method | Status |
| --- | --- | --- | --- | --- |
| `P7-SAFE-001` traceable or labelled, no third category | `AeroraStateValue` + `AeroraDerivedBadge` | `AeroraTelemetryStateTest` + `TBW` QML | UT + QML | `IMPLEMENTED-UNCOMPILED` / `DESIGNED` |
| `P7-SAFE-002` no inferred failsafe action | *absence of* | `STATIC` | STATIC | `IMPLEMENTED-UNCOMPILED` |
| `P7-SAFE-003` no unapproved threshold | `kDefinedThresholdCount == 0` | `AeroraPowerModelTest` | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-AUTH-001` every command passes the decision point | `MavCommandQueue.cc` + `authorizeMavCommand()` | `MavCommandAuthorizationTest` (core tree) | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-AUTH-002` station reports when not enforced | `isAuthorizationEnforced()` | `TBW` + `SCR-SHL-03` QML | UT + QML | `IMPLEMENTED-UNCOMPILED` / `DESIGNED` |
| `P7-AUTH-003` `PARAM_SET` has an owner | â€” | â€” | â€” | **`OPEN â€” UNASSIGNED`** |
| `P7-AUTH-004` UI exposes no bypassing actuator | `AeroraVehicleUiContext` | **`AeroraUiSafetyTest`** | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-AUDIT-001` decisions recorded with time/actor/subject/outcome | `AeroraAuditSink` | `TBW` | UT | `CONTRACT-ONLY` |
| `P7-AUDIT-002` durability and tamper-evidence | â€” | `TBW` | UT | `OPEN` â€” `Q-037` |
| `P7-AUDIT-003` unauditable events counted | `AeroraAuditSink` | `TBW` | UT | `CONTRACT-ONLY` |
| `P7-HW-001` hardware behind an interface | the 8 interfaces | `STATIC` | STATIC | `IMPLEMENTED-UNCOMPILED` |
| `P7-HW-002` designed â‰  supported | this matrix + the docs | documentation review | STATIC | `DESIGNED` |
| `P7-STATE-001` seven states distinguishable through every layer | `AeroraTelemetryState` | **`AeroraTelemetryStateTest`** | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-STATE-002` three non-value states mutually distinguishable, no blank | `AeroraStateText` | `TBW` QML + `DEMO` | QML + DEMO | `DESIGNED` |
| `P7-NOINV-001` nothing invented | the 10 `false` flags | all module tests read the flags | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-NOINV-002` unknown â‡’ emit the requirement ID, not a number | this document set | documentation review | STATIC | `DESIGNED` |

### Shell, live operations, mission

| Requirement | Component | Test | Method | Status |
| --- | --- | --- | --- | --- |
| `P7-SHELL-001` one nav model, â‰¤1 fork | hub via `analyzePages()` | `TBW` | STATIC | `DESIGNED` |
| `P7-SHELL-002` no `MainWindow.qml` fork | `showTool()` + `analyzePages()` | `AeroraUiFoundationTest` (`MainWindow.qml` never intercepted) | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-SHELL-003` no hard-coded size or colour | design system | `aerora-ui-static-check.ps1` | STATIC | `DESIGNED` |
| `P7-SHELL-004` responsive, no safety-value truncation | breakpoints `UI-RL-01` | `TBW` QML + `DEMO` | QML + DEMO | `DESIGNED` |
| `P7-SHELL-005` original design system | design system | documentation + legal review | â€” | `DESIGNED` |
| `P7-SHELL-006` interceptor installed | `AeroraCorePlugin` | `AeroraUiFoundationTest` (`isInstalled()`) | UT | **`OPEN` â€” U1** |
| `P7-FLY-001` one live screen, all state | `SCR-LIV-01`..`07` | `TBW` QML | QML | `DESIGNED` |
| `P7-FLY-002` every value with state class + age | `AeroraValueDisplay` | `TBW` QML | QML | `DESIGNED` |
| `P7-FLY-003` occluder rects published | `AeroraPanelFrame` | `TBW` QML | QML | `DESIGNED` |
| `P7-FLY-004` actions only where authorized | *absence of* actions | `AeroraUiSafetyTest` | UT | **`FORBIDDEN-UNTIL`** |
| `P7-FLY-005` critical change surfaced out-of-panel | `SCR-LIV-08` | `TBW` QML | QML + DEMO | `DESIGNED` |
| `P7-MIS-001` waypoint/pattern planning reused | upstream | `ctest -L Unit` | IT | `IMPLEMENTED` |
| `P7-MIS-002` industrial surface item | â€” | `TBW` | UT + QML | `BLOCKED` â€” `Q-021`/`Q-023` |
| `P7-MIS-003` 3D via existing `Viewer3D` | upstream | â€” | DEMO | `DESIGNED` |
| `P7-MIS-004` both map visuals or documented | â€” | `TBW` | UT | `BLOCKED` |
| `P7-MIS-005` verification compares, never asserts | `AeroraMissionObserver::verify()` | **`AeroraMissionTest`, `AeroraMissionSyncTest`** | UT + IT | `IMPLEMENTED-UNCOMPILED` |
| `P7-MIS-006` no station payload replay | `kGcsPayloadStateReplayImplemented = false` | `AeroraMissionTest` | UT | `IMPLEMENTED-UNCOMPILED` |

### Payload, vehicle, telemetry

| Requirement | Component | Test | Method | Status |
| --- | --- | --- | --- | --- |
| `P7-PAY-001` payload state as reported | `AeroraPayloadModel` | **`AeroraPayloadModelTest`** | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-PAY-002` payload controller authoritative for safety | `kGroundStationActuatorPathImplemented = false` | `AeroraPayloadModelTest`, `AeroraUiSafetyTest` | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-PAY-003` request path is an interface with no sink | `AeroraPayloadCommandSink` | `AeroraPayloadModelTest` | UT | `CONTRACT-ONLY` |
| `P7-PAY-004` painting values reported-only | â€” | `TBW` | UT | `BLOCKED` â€” payload ICD |
| `P7-PAY-005` no station-initiated autonomous payload action | `kAutonomousPayloadActionImplemented = false` | `AeroraPayloadModelTest` | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-VEH-001` setup via `AeroraAutoPilotPlugin` | â€” | `TBW` | UT | **ABSENT â€” U4** |
| `P7-VEH-002` firmware behaviour via `firmwarePlugin()` | `AeroraFirmwarePlugin` | **`AeroraVehicleIntegrationTest`** | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-VEH-003` calibration authorization-gated | ADR-015 seam | **`AeroraCalibrationAuthorizationTest`, `AeroraCalibrationEnforcementTest`** | UT + IT | `IMPLEMENTED-UNCOMPILED` |
| `P7-VEH-004` health as reported, `UNKNOWN` otherwise | â€” | `TBW` | UT | `DESIGNED` |
| `P7-VEH-005` identity/capability reported, sysid â‰  fleet id | `AeroraVehicleIdentity`, `AeroraVehicleCapabilities` | `AeroraVehicleIntegrationTest` | UT | `IMPLEMENTED-UNCOMPILED` + `OPEN` |
| `P7-VEH-006` missing service visible | `AeroraVehicleServices` | `AeroraVehicleLifecycleTest` | IT | `IMPLEMENTED-UNCOMPILED` / `DESIGNED` |
| `P7-TEL-001` kind + observation + derived class | `AeroraTelemetryState` | **`AeroraTelemetryStateTest`** | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-TEL-002` per-item freshness budget | `AeroraFreshness` | `AeroraTelemetryStateTest` (mechanism) | UT | `IMPLEMENTED-UNCOMPILED` + `OPEN` values |
| `P7-TEL-003` per-vehicle fact group attachment | `AeroraVehicleFactGroupRegistrar` | **`AeroraTelemetryAttachmentTest`** | UT | `CONTRACT-ONLY` |
| `P7-TEL-004` event timeline | â€” | `TBW` | UT | `SPECIFIED-NOT-DESIGNED` |
| `P7-TEL-005` source attribution where known | `AeroraTelemetrySource` | `TBW` | UT | `OPEN` â€” `IF-MAV-022` |

### Power, fleet, video, logs, checks

| Requirement | Component | Test | Method | Status |
| --- | --- | --- | --- | --- |
| `P7-PWR-001` power state as reported per source | `AeroraPowerModel` | **`AeroraPowerModelTest`** | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-PWR-002`..`005` four prohibitions | the four `false` flags | **`AeroraPowerModelTest`** | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-PWR-006` tether as its own source | â€” | `TBW` | UT | `BLOCKED` â€” tether item set |
| `P7-FLEET-001` profile selects UI/content/expectations | `AeroraProductProfileRegistry` | `TBW` | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-FLEET-002` profile never grants capability | `AeroraVehicleCapabilities` | `AeroraVehicleIntegrationTest` + `TBW` | UT | `DESIGNED` |
| `P7-FLEET-003` fleet list with identity/profile/last-seen | â€” | `TBW` | UT | `BLOCKED` â€” fleet identity |
| `P7-FLEET-004` no profile-keyed `CONSTANT` registry | static registration, runtime content | `TBW` | STATIC | `DESIGNED` |
| `P7-VID-001` three video seams as a set or not at all | not overridden | `STATIC` | STATIC | `DESIGNED` |
| `P7-VID-002` overlay follows existing precedent | `SCR-VID-01` | `TBW` QML | QML | `DESIGNED` |
| `P7-VID-003` Aerora camera definitions | `getOfflineCameraDefinitionFile()` | `TBW` | UT | `DESIGNED` |
| `P7-VID-004` no video safety value without state class | `AeroraValueDisplay` | `TBW` QML | QML | `DESIGNED` |
| `P7-LOG-001` QGC log tooling reused | upstream | `ctest -L Unit` | IT | `IMPLEMENTED` |
| `P7-LOG-002` Aerora analysis via `analyzePages()` | `SCR-LOG-01` | `TBW` | UT | `DESIGNED` |
| `P7-LOG-003` export records reported vs derived | â€” | `TBW` | UT | `SPECIFIED-NOT-DESIGNED` |
| `P7-LOG-004` replay reconstructs state classes | â€” | `TBW` | UT | `SPECIFIED-NOT-DESIGNED` |
| `P7-CHK-001` geofence planned and uploaded, not enforced | upstream | `ctest -L Unit` | IT | `IMPLEMENTED` |
| `P7-CHK-002` validation reports what it could not check | `SCR-MIS-07` | `TBW` | UT | `DESIGNED` |
| `P7-CHK-003` checklist replaced via `preFlightChecklistUrl()` | `SCR-LIV-09` | `TBW` | UT | `DESIGNED` |
| `P7-CHK-004` unevaluable check = `UNKNOWN`, blocks nothing alone | `SCR-LIV-09` | `TBW` | UT | `DESIGNED` |
| `P7-CHK-005` checklist completion as audit evidence | `AeroraAuditSink` | `TBW` | UT | `BLOCKED` â€” `Q-037` |

### Security, radio, data, simulation, settings, help, profiles, packaging

| Requirement | Component | Test | Method | Status |
| --- | --- | --- | --- | --- |
| `P7-SEC-001` single enforcement point | `MavCommandQueue.cc` | **`MavCommandAuthorizationTest`** | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-SEC-002` identity/role/policy interfaces | 3 interfaces | `TBW` | UT | `CONTRACT-ONLY` |
| `P7-SEC-003` platform key protection | `AeroraPlatformKeyProtector` | `TBW` | UT + HW | `CONTRACT-ONLY` â€” gated on `D-006` |
| `P7-SEC-004` security not QML-visible | no QML macros in `Security/` | **`AeroraUiSafetyTest`** | UT | `IMPLEMENTED-UNCOMPILED` |
| `P7-SEC-005` security screen can say "not enforced" | `SCR-SEC-01` | `TBW` QML | QML | `SPECIFIED-NOT-DESIGNED` |
| `P7-RAD-001` QGC link layer reused | upstream | `ctest -L Unit` | IT | `IMPLEMENTED` |
| `P7-RAD-002` per-link state, not aggregated | `SCR-LIV-07` | `TBW` QML | QML | `DESIGNED` |
| `P7-RAD-003` radio is `DESIGNED`, not supported | this matrix | documentation review | STATIC | `DESIGNED` |
| `P7-RAD-004` designed-unverified labelled in the UI | `SCR-LIV-07` | `TBW` QML | QML + DEMO | `DESIGNED` |
| `P7-RAD-005` no redundancy/failover displayed as available | *absence of* | `STATIC` | STATIC | `OPEN` |
| `P7-DAT-001` tile cache reused | upstream | `ctest -L Unit` | IT | `IMPLEMENTED` |
| `P7-DAT-002` documented local schema and location | â€” | `TBW` | UT | `SPECIFIED-NOT-DESIGNED` |
| `P7-DAT-003` local-first | â€” | `TBW` | IT | `DESIGNED` |
| `P7-DAT-004` no automatic outbound transmission | *absence of* | `STATIC` | STATIC | `DESIGNED` |
| `P7-DAT-005` operator-controlled retention/export | â€” | `TBW` | UT | `SPECIFIED-NOT-DESIGNED` |
| `P7-SIM-001` MockLink/SITL reused | upstream | `ctest -L Unit` | IT | `IMPLEMENTED` |
| `P7-SIM-002` simulated source labelled everywhere | â€” | `TBW` QML | QML | `DESIGNED` |
| `P7-SIM-003` simulated vehicle cannot satisfy authorization | â€” | `TBW` | UT | `DESIGNED` |
| `P7-SIM-004` mocks gated on both build flags | `AeroraVehicleMock` `#error` guard | `STATIC` + a build with the flags off | STATIC | `IMPLEMENTED-UNCOMPILED` |
| `P7-SET-001` settings via JSON overlay, no upstream edit | custom-dir seam | `TBW` | STATIC | `DESIGNED` |
| `P7-SET-002` all strings `qsTr()`-wrapped | â€” | `TBW` `STATIC` | STATIC | `DESIGNED` |
| `P7-SET-003` no hard-coded size/colour | â€” | `aerora-ui-static-check.ps1` | STATIC | `DESIGNED` |
| `P7-SET-004` keyboard, focus, contrast, non-colour encoding | design system | `TBW` QML + `DEMO` | QML + DEMO | `DESIGNED` |
| `P7-SET-005` WCAG **not claimed** | â€” | â€” | â€” | `OPEN` |
| `P7-HELP-001` diagnostics reports overlay state | `SCR-HLP-01` | `TBW` | UT | `SPECIFIED-NOT-DESIGNED` |
| `P7-HELP-002` error report sent only on operator action | `SCR-HLP-03` | `TBW` | UT | `DESIGNED` |
| `P7-HELP-003` operator docs cover the state vocabulary | `SCR-HLP-02` + docs | documentation review | â€” | `SPECIFIED-NOT-DESIGNED` |
| `P7-HELP-004` station states its own provenance | â€” | `TBW` | UT | `SPECIFIED-NOT-DESIGNED` |
| `P7-PROF-001` profiles over one shell | `AeroraProductProfile` | `TBW` | UT | `DESIGNED` |
| `P7-PROF-002` new profile changes no shell/telemetry/auth code | â€” | `TBW` | STATIC | `DESIGNED` |
| `P7-PROF-003` no profile behaviour before requirements + evidence | *absence of* | `STATIC` | STATIC | `FORBIDDEN-UNTIL` â€” `D-007` |
| `P7-PROF-004` expectation-vs-report mismatch displayed | `AeroraProductProfile` + capabilities | `TBW` | UT | `DESIGNED` |
| `P7-PKG-001` builds with `-DQGC_CUSTOM_DIR=aerora` | `aerora/CMakeLists.txt` | a configure + build | IT | `IMPLEMENTED-UNCOMPILED` |
| `P7-PKG-002` CI leg with `QGC_BUILD_TESTING=ON` | â€” | the CI leg itself | IT | `OPEN` â€” `Q-035` |
| `P7-PKG-003` installers with provenance | â€” | `TBW` | IT | `SPECIFIED-NOT-DESIGNED` |
| `P7-PKG-004` update does not silently change enforcement | â€” | `TBW` | IT | `DESIGNED` |
| `P7-PKG-005` deployment docs state verified vs designed | this document set | documentation review | STATIC | `DESIGNED` |

## 3. Coverage analysis

### By requirement count

| Category | Count | Share |
| --- | --- | --- |
| Total requirements traced | 118 | 100% |
| Have an existing component | 48 | 41% |
| Have an existing test | 31 | 26% |
| Test is `TBW` | 71 | 60% |
| No test possible (`â€”`) | 9 | 8% |
| Covered by an upstream test | 7 | 6% |

### By existing Aerora test

| Test | Requirements it would verify |
| --- | --- |
| `AeroraTelemetryStateTest` | `P7-MOD-010`, `P7-MOD-021`, `P7-MOD-030`, `P7-SAFE-001`, `P7-STATE-001`, `P7-TEL-001`, `P7-TEL-002` â€” **7** |
| `AeroraUiSafetyTest` | `P7-MOD-003`, `P7-ARCH-001`, `P7-AUTH-004`, `P7-FLY-004`, `P7-PAY-002`, `P7-SEC-004` â€” **6** |
| `AeroraPowerModelTest` | `P7-ARCH-005`, `P7-SAFE-003`, `P7-PWR-001`, `P7-PWR-002`..`005` â€” **6** |
| `AeroraPayloadModelTest` | `P7-PAY-001`, `P7-PAY-002`, `P7-PAY-003`, `P7-PAY-005` â€” **4** |
| `AeroraVehicleIntegrationTest` | `P7-MOD-002`, `P7-VEH-002`, `P7-VEH-005`, `P7-FLEET-002` â€” **4** |
| `AeroraMissionTest` | `P7-MIS-005`, `P7-MIS-006` â€” **2** |
| `AeroraUiFoundationTest` | `P7-ARCH-023`, `P7-SHELL-002`, `P7-SHELL-006` â€” **3** |
| `AeroraCalibrationAuthorizationTest` + `EnforcementTest` | `P7-VEH-003` â€” **1** |
| `MavCommandAuthorizationTest` (core) | `P7-AUTH-001`, `P7-SEC-001` â€” **2** |
| `AeroraTelemetryAttachmentTest` | `P7-TEL-003` â€” **1** |
| `AeroraMissionSyncTest` | `P7-MIS-005` â€” **1** |
| `AeroraVehicleLifecycleTest` | `P7-VEH-006` â€” **1** |
| `AeroraReportedValueTest` | `P7-STATE-001` (primitive half) â€” **1** |
| `AeroraSigningKeyStoreTest` | supports `P7-SEC-003` â€” **0 direct** |

`AeroraUiSafetyTest` and `AeroraTelemetryStateTest` carry the most requirement weight. They are also
the two most likely to fail on a first run â€” the first because its reviewed-method lists were derived
by reading headers, the second because it is the largest behavioural surface. Worth knowing before
the first build, so the result is read correctly.

### Coverage by requirement block

| Block | Requirements | With a test | Gap |
| --- | --- | --- | --- |
| Operating modes | 13 | 5 | 8 |
| Architecture boundaries | 13 | 8 | 5 |
| Safety / authorization / audit / no-invention | 16 | 9 | 7 |
| Shell + live operations | 11 | 4 | 7 |
| Mission | 6 | 3 | 3 |
| Payload | 5 | 4 | 1 |
| Vehicle | 6 | 4 | 2 |
| Telemetry | 5 | 3 | 2 |
| Power | 6 | 5 | 1 |
| Fleet / profiles | 8 | 1 | 7 |
| Video / logs / checks | 13 | 3 | 10 |
| Security / radio | 10 | 3 | 7 |
| Data / simulation / settings | 14 | 4 | 10 |
| Help / packaging | 9 | 0 | 9 |

**The best-covered blocks are the prohibitions** â€” payload, power, safety. That is the right shape:
a prohibition is cheap to test (assert the flag, assert the absence) and expensive to get wrong. The
worst-covered are the screens, which is expected because 19 of them are blocked on a decision.

## 4. Requirements no test can verify

Nine. Each needs human judgement, a legal review, or hardware.

| Requirement | Why no test | Method |
| --- | --- | --- |
| `P7-AUTH-003` `PARAM_SET` has an owner | An assignment, not a behaviour | Programme decision |
| `P7-SHELL-005` original design system | Originality is a judgement | Design + legal review |
| `P7-HW-002` designed â‰  supported | A documentation discipline | Documentation review |
| `P7-NOINV-002` emit the requirement ID, not a number | A writing discipline | Documentation review |
| `P7-RAD-003` radio is designed, not supported | A claim about claims | Documentation review |
| `P7-SET-005` WCAG not claimed | Conformance needs assistive-technology testing and expert review | Expert accessibility review |
| `P7-HELP-003` docs cover the state vocabulary | Comprehension, not behaviour | Documentation + operator trial |
| `P7-PKG-005` docs state verified vs designed | Documentation discipline | Documentation review |
| Visual distinguishability of two state indicators | No automated test can assert perceptual difference | `DEMO`, gated on `Q-040` |

These are **not** untested requirements to be fixed. They are requirements whose verification method
is not a test, and recording that is the point.

## 5. Tests with no requirement

Checked for the reverse direction: a test asserting something no requirement asks for. That would
indicate either an undocumented requirement or over-testing.

| Test assertion | Requirement | Verdict |
| --- | --- | --- |
| `AeroraUiFoundationTest::_customMapItemsIsForbiddenAndUndeclared` | `P7-ARCH-023` | Covered. The specific file is a design decision inside that requirement |
| `AeroraUiFoundationTest::_undeclaredResourceUrlIsNeverIntercepted` (6 URL shapes) | `P7-ARCH-023`, `IF-UIS-015` | Covered |
| `AeroraUiSafetyTest::_uiContextDoesNotExposeTheVehicleOrTheServiceMap` | `P7-AUTH-004` | Covered |
| `AeroraUiSafetyTest::_onlyTheRecordedPropertyDeviationIsWritable` | `P7-STATE-001`, `UI-ST-07` | Covered â€” and is the tracking mechanism for `D-UI-03` |
| `AeroraVehicleIntegrationTest` asserting the 4 non-overridden base defaults | `P7-VEH-002` | Covered. Asserting a property rather than intending it is the right call |

**No orphan test found.** Every assertion traces to a requirement.

## 6. Tests to be written

71 `TBW` entries. Grouped by prerequisite, because most cannot be written yet.

### Writable now â€” 14 tests

| Test | Requirements | Owner | Prerequisite |
| --- | --- | --- | --- |
| `AeroraAutoPilotPluginTest` | `P7-VEH-001` | 3 | U4 |
| `AeroraToolIndicatorTest` | `P7-SHELL-002` (indicator half) | 3 | U5 |
| `AeroraQmlOverrideInstallTest` (extend the foundation test) | `P7-SHELL-006` | 1 | U1 |
| `AeroraProfileRegistryTest` | `P7-FLEET-001`, `P7-PROF-001`, `P7-PROF-004` | 3 | â€” |
| `AeroraProfileDoesNotGrantCapabilityTest` | `P7-FLEET-002` | 3 | â€” |
| `AeroraConstantRegistryTest` | `P7-FLEET-004` | 1 | â€” |
| `AeroraNoOutboundTransmissionTest` (static) | `P7-ARCH-013`, `P7-DAT-004` | 1 | â€” |
| `AeroraCameraDefinitionTest` | `P7-VID-003` | 7 | â€” |
| `AeroraChecklistReplacementTest` | `P7-CHK-003`, `P7-CHK-004` | 7 | â€” |
| `AeroraAnalyzePageRegistrationTest` | `P7-LOG-002` | 7 | â€” |
| `AeroraSettingsRegistrationTest` | `P7-SET-001` | 7 | â€” |
| `AeroraTranslationCoverageTest` (static) | `P7-SET-002` | 7 | â€” |
| `AeroraMockGatingTest` (build with flags off) | `P7-SIM-004` | 8 | `D-006` |
| `AeroraSimulatedSourceCannotAuthorizeTest` | `P7-SIM-003` | 8 | â€” |

### Blocked on a decision â€” 41 tests

Cannot be written without inventing the thing under test. Grouped by blocker:

| Blocker | Tests blocked |
| --- | --- |
| `Q-029` â†’ ADR-014 | 11 â€” all authorization, action and approval tests |
| `Q-021` / `Q-023` | 8 â€” complex item, scene, surface, coverage |
| `P7-DAT-002` | 7 â€” persistence, timeline, fleet records, export |
| `Q-036` | 3 â€” freshness with real budgets |
| `Q-037` | 4 â€” audit durability and tamper-evidence |
| Payload ICD / `Q3-009` | 5 â€” painting, tether, consumable values |
| `P7-AI-004` | 3 â€” proposal model and lifecycle |

### Blocked on a QML test target â€” 16 tests

Every `QML`-method test needs HO-TAB7-04 / `D-UI-05`: a second Qt Quick Test executable registered
from `aerora/CMakeLists.txt`, with `QUICK_TEST_SOURCE_DIR` pointing at `aerora/test/UI/qml`, the
same `-platform offscreen` command, `LABELS "Unit;QML"` and the same `ENVIRONMENT` block as
`test/UnitTestFramework/QmlTesting/CMakeLists.txt`.

Recommendation stands: create it at the **first** Aerora QML behaviour that needs asserting, not
before. An empty test target is a target with nothing in it. The first candidate is
`AeroraStateText` rendering `UNKNOWN` as the word rather than as an empty string â€” `P7-STATE-002`,
which the C++ harness cannot cover.

## 7. Verification method by requirement class

| Class | Method | Count | Note |
| --- | --- | --- | --- |
| Behaviour | `UT` | 54 | The bulk |
| Composition / lifecycle | `IT` | 17 | Needs a vehicle, real or mock |
| Presentation | `QML` | 16 | **All blocked on `D-UI-05`** |
| Absence / prohibition | `STATIC` | 18 | Cheapest and most reliable class. A prohibition is verified by showing the code does not exist |
| Perceptual | `DEMO` | 7 | Human observation. No test substitutes |
| Hardware | `HW` | 1 | `P7-SEC-003` platform key protection |
| Judgement | â€” | 9 | [Â§4](#4-requirements-no-test-can-verify) |

### A note on the STATIC class

18 requirements are verified by demonstrating that code **does not exist** â€” no failsafe, no
actuator path, no outbound transmission, no invented identifier. This is the overlay's most
distinctive verification property and the reason the ten `false` flags exist: a flag that a test
reads converts "we did not build this" from a claim into an assertion. It is also why
`aerora-ui-static-check.ps1` has real value on a host with no compiler â€” text-level prohibition
checks are the one form of verification that does not need a toolchain.

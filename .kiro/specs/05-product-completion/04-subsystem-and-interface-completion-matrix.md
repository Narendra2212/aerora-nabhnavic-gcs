# Subsystem and Interface Completion Matrix

Owner: **Tab 1**. As-built inventory of the Aerora overlay and the interfaces it needs, verified
against the checkout this pass.

Status vocabulary: [00-README](00-README.md#status-vocabulary). `VERIFIED` is not claimed anywhere.

## Contents

- [1. Overlay inventory](#1-overlay-inventory)
- [2. Subsystem matrix](#2-subsystem-matrix)
- [3. QML-reachable surface](#3-qml-reachable-surface)
- [4. The deliberate-false flags](#4-the-deliberate-false-flags)
- [5. Interface completion](#5-interface-completion)
- [6. QGroundControl core modifications](#6-qgroundcontrol-core-modifications)
- [7. Missing functionality](#7-missing-functionality)
- [8. Test inventory](#8-test-inventory)
- [9. Static checks available](#9-static-checks-available)
- [10. First-build expectations](#10-first-build-expectations)
- [11. Completion scorecard](#11-completion-scorecard)

## 1. Overlay inventory

Counted this pass under `aerora/`:

| Category | Count |
| --- | --- |
| Production C++ sources (`.cc` + `.h`) | 69 |
| QML files | 4 |
| Test sources (`.cc` + `.h`) | 36 |
| CMake files (`CMakeLists.txt` + `.cmake` manifests) | 8 |
| **Total declared overlay files** | **109** |
| Registered CTest entries | 14 |

All 109 are declared in a manifest and resolve on disk — last verified by
`.kiro/tools/aerora-manifest-check.ps1` at 0 problems. **None has been compiled.**

### Module layout

```text
aerora/
├── CMakeLists.txt                  Tab 1 — the only aggregation point
├── cmake/CustomOverrides.cmake     Tab 1
└── src/
    ├── AeroraCorePlugin.{h,cc}     Tab 1 (M-Plugin)
    ├── Security/      + Security.cmake        Tab 2
    ├── Telemetry/     + Telemetry.cmake       Tab 4
    ├── Vehicle/       + Vehicle.cmake         Tab 3
    ├── Mission/       + Mission.cmake         Tab 5
    ├── Payload/   ─┐
    ├── Power/      ├─ PayloadPower.cmake      Tab 6
    ├── Supervisory/┘
    └── UI/            + Ui.cmake              Tab 7
```

Manifest convention (converged on independently by Tabs 3, 4 and 5, adopted unchanged): a manifest
sets variables only — no targets, no `add_test()`, nothing in `PARENT_SCOPE`. It exports
`AERORA_<MODULE>_SOURCES` / `_INCLUDE_DIRECTORIES` / `_LIBRARIES` and the `_TEST_*` equivalents.
`aerora/CMakeLists.txt` appends to local lists and performs one `PARENT_SCOPE` set per variable at
the end.

**Sources compile into the application target, not a static library.** Required, not incidental:
`AeroraFirmwarePluginFactory` self-registers through a non-local static, which a linker may discard
from an archive when no symbol references it. The failure is silent — the build succeeds, the
application starts, and no Aerora plugin is registered.

One structural variance, accepted: `PayloadPower.cmake` sits at `aerora/src/` and covers three
directories, because Payload and Power share `AeroraReportedValue` and splitting would duplicate it.
The exported-variable contract is identical, which is all the aggregation depends on.

## 2. Subsystem matrix

### M-Plugin (Tab 1)

| Item | File | Status |
| --- | --- | --- |
| `AeroraCorePlugin` | `src/AeroraCorePlugin.{h,cc}` | `IMPLEMENTED-UNCOMPILED` |
| `init()` override | — | `IMPLEMENTED-UNCOMPILED` |
| `authorizeMavCommand()` override | — | `IMPLEMENTED-UNCOMPILED` — **returns `true` and warns once**. No policy installed |
| `isAuthorizationEnforced()` | — | `IMPLEMENTED-UNCOMPILED` — reports `false` |
| `_installSigningKeyStore()` | — | `IMPLEMENTED-UNCOMPILED` |
| `createQmlApplicationEngine` / `destroyQmlApplicationEngine` overrides | — | **ABSENT — HO-TAB7-02** |
| Overlay aggregation | `CMakeLists.txt` | `IMPLEMENTED-UNCOMPILED` |
| `AeroraControlsModule` QML module target | `CMakeLists.txt:160-172` | `IMPLEMENTED-UNCOMPILED` — **missing `IMPORTS QGC` + `qgc_set_qt_resource_alias`, D-UI-04** |

`AeroraCorePlugin` is deliberately minimal: Layer 1 scope is the security and command boundary. It
overrides no UI, branding, firmware-factory or resource hook. Two of the three outstanding Tab 1
items are in this file.

### M-Security (Tab 2)

| Item | File | QML | Status |
| --- | --- | --- | --- |
| `AeroraCommandAuthorizer` | `Security/AeroraCommandAuthorizer.{h,cc}` | none | `IMPLEMENTED-UNCOMPILED` |
| `AeroraCalibrationAuthorization` | `Security/AeroraCalibrationAuthorization.{h,cc}` | none | `IMPLEMENTED-UNCOMPILED` — ADR-015 |
| `AeroraSigningKeyStore` | `Security/AeroraSigningKeyStore.{h,cc}` | none | `IMPLEMENTED-UNCOMPILED` |
| `AeroraIdentityProvider` | `Security/AeroraAuthorization.h:113` | none | **`CONTRACT-ONLY`** — no implementation |
| `AeroraRoleResolver` | `Security/AeroraAuthorization.h:125` | none | **`CONTRACT-ONLY`** — no implementation |
| `AeroraAuthorizationPolicy` | `Security/AeroraAuthorization.h:138` | none | **`CONTRACT-ONLY`** — no implementation |
| `AeroraAuditSink` | `Security/AeroraAuditSink.h:72` | none | **`CONTRACT-ONLY`** — neither durable nor tamper-evident |
| `AeroraPlatformKeyProtector` | `Security/AeroraPlatformKeyProtector.h:38` | none | **`CONTRACT-ONLY`** — no backend on any of the five target platforms |

**No security type carries `Q_OBJECT`, `Q_GADGET` or any QML macro.** That is the design: security
state is unreachable from QML, asserted by `AeroraUiSafetyTest`. The consequence is that
[`SCR-SEC-01`..`04`](03-gui-information-architecture-and-screens.md#security--4-screens) need a
deliberate minimal accessor (`D-UI-01`) rather than a type exposure.

**The honest summary**: the enforcement *point* is implemented; the enforcement *logic* is five
interfaces with nothing behind them. `Q-029` is why.

### M-Telemetry (Tab 4)

| Item | File | QML | Status |
| --- | --- | --- | --- |
| `AeroraTelemetryState` | `Telemetry/AeroraTelemetryState.{h,cc}` | `QML_ELEMENT` + `QML_SINGLETON` | `IMPLEMENTED-UNCOMPILED` — 7 static `Q_INVOKABLE` classifiers |
| `AeroraStateValue` | `Telemetry/AeroraStateValue.{h,cc}` | `QML_ELEMENT` + `QML_UNCREATABLE` | `IMPLEMENTED-UNCOMPILED` — no writable property; HO-TAB7-01 closed by U3 |
| `AeroraFreshness` | `Telemetry/AeroraFreshness.{h,cc}` | `QML_ELEMENT` + `QML_UNCREATABLE` | `IMPLEMENTED-UNCOMPILED` — mechanism only; **no budget values (`Q-036`)** |
| `AeroraFactGroupBase` | `Telemetry/AeroraFactGroupBase.{h,cc}` | none (derives `FactGroup`) | `IMPLEMENTED-UNCOMPILED` |
| The six Aerora fact groups | — | — | **`CONTRACT-ONLY`** — none has a message to parse |
| `AeroraFactGroupRegistrar` | `Telemetry/AeroraFactGroupRegistrar.{h,cc}` | none | `IMPLEMENTED-UNCOMPILED` (interface) |
| `AeroraVehicleFactGroupRegistrar` | `Telemetry/AeroraVehicleFactGroupRegistrar.{h,cc}` | none | `IMPLEMENTED-UNCOMPILED` — **`isRegistrationAvailable()` is false** |
| `AeroraTelemetrySet` | `Telemetry/AeroraTelemetrySet.{h,cc}` | **none** | `IMPLEMENTED-UNCOMPILED` — deliberately not QML-registered |
| `AeroraTelemetrySource` | `Telemetry/AeroraTelemetrySource.{h,cc}` | none | `IMPLEMENTED-UNCOMPILED` — attribution is `IF-MAV-022`, `OPEN` |

The state classifier is the most complete subsystem in the overlay and the product's core claim. The
telemetry *data* behind it is empty: six fact groups with no messages, no freshness budgets, and no
registration path (`Q4-002` is **DECIDED** as Option B / ADR-016, but the implementation is gated on
`Q-023`/`Q-024` and `D-006`).

`AeroraTelemetrySet` carrying no QML macro is correct and worth noting: there is nothing to show
while every group is `CONTRACT-ONLY`, so exposing it would be exposing an empty surface.

### M-Vehicle (Tab 3)

| Item | File | QML | Status |
| --- | --- | --- | --- |
| `AeroraFirmwarePlugin` | `Vehicle/AeroraFirmwarePlugin.{h,cc}` | none | `IMPLEMENTED-UNCOMPILED` — the per-vehicle composition root |
| `AeroraFirmwarePluginFactory` | `Vehicle/AeroraFirmwarePluginFactory.{h,cc}` | none | `IMPLEMENTED-UNCOMPILED` — non-local static self-registration |
| `AeroraFirmwareClaim` | `Vehicle/AeroraFirmwareClaim.h` | none | `IMPLEMENTED-UNCOMPILED` |
| `AeroraVehicleServices` | `Vehicle/AeroraVehicleServices.{h,cc}` | `QML_ELEMENT` + `QML_UNCREATABLE` | `IMPLEMENTED-UNCOMPILED` — service registry; `service()`/`serviceNames()` stay out of QML |
| `AeroraVehicleIdentity` | `Vehicle/AeroraVehicleIdentity.{h,cc}` | `QML_ELEMENT` + `QML_UNCREATABLE` | `IMPLEMENTED-UNCOMPILED` — **fleet identity format `OPEN`** |
| `AeroraVehicleCapabilities` | `Vehicle/AeroraVehicleCapabilities.{h,cc}` | `QML_ELEMENT` + `QML_UNCREATABLE` | `IMPLEMENTED-UNCOMPILED` — **no capability is reported (`Q3-009`)** |
| `AeroraProductProfile` / `Registry` | `Vehicle/AeroraProductProfile.{h,cc}` | none | `IMPLEMENTED-UNCOMPILED` |
| `AeroraAutoPilotPlugin` | — | — | **ABSENT.** Required for all 6 `SCR-VEH-*` screens |
| `toolIndicators()` override | — | — | **ABSENT.** Required for per-vehicle Aerora indicators |

`AeroraFirmwarePlugin` deliberately derives from `FirmwarePlugin` rather than a vendor plugin, so
every firmware-specific behaviour is an explicit reviewable override and the base-class safe defaults
apply where Aerora has no evidence. Four such defaults are *not* overridden and
`AeroraVehicleIntegrationTest` asserts each one instead — so the property is checked rather than
intended.

Two absences gate real screens. Neither is blocked by a decision; both are straightforward work
([05](05-implementation-dependency-graph.md)).

### M-Mission (Tab 5)

| Item | File | QML | Status |
| --- | --- | --- | --- |
| `AeroraMissionObserver` | `Mission/AeroraMissionObserver.{h,cc}` | `QML_ELEMENT` + `QML_UNCREATABLE` | `IMPLEMENTED-UNCOMPILED` — one `Q_INVOKABLE`: `verify()` |
| `AeroraMissionSnapshot` / `AeroraMissionAgreement` | `Mission/AeroraMissionSnapshot.{h,cc}` | none / `Q_INVOKABLE isVerified()` | `IMPLEMENTED-UNCOMPILED` |
| `AeroraMissionIndex` | `Mission/AeroraMissionIndex.{h,cc}` | `Q_INVOKABLE toString()` | `IMPLEMENTED-UNCOMPILED` |
| `AeroraMissionBridge` | `Mission/AeroraMissionBridge.{h,cc}` | none | `IMPLEMENTED-UNCOMPILED` — `attachTo()` **has** a caller; see below |
| `AeroraMissionResumeBoundary` | `Mission/AeroraMissionResumeBoundary.{h,cc}` | none | `IMPLEMENTED-UNCOMPILED` — `kGcsPayloadStateReplayImplemented = false` |
| `AeroraMissionExtension` / `Registry` | `Mission/AeroraMissionExtension.{h,cc}` | none | `IMPLEMENTED-UNCOMPILED` — **no extension registered (`Q-021`/`Q-023`)** |
| Aerora complex mission item | — | — | **ABSENT.** Gates `SCR-MIS-01`..`06` |

`AeroraMissionBridge::attachTo()` having no caller was conflict 2 in
[29-integration-status.md](../03-implementation-specification/29-integration-status.md#cross-domain-conflicts),
assigned to Tab 3 as owner of the vehicle lifecycle seam. **Verified closed this pass:**
`AeroraFirmwarePlugin.cc:162` calls `(void) AeroraMissionBridge::attachTo(vehicle);` inside
`initializeVehicle()`, which is the recommended seam — `AeroraVehicleServices` is already attached at
that point. `attachTo()` is also idempotent: it returns the existing bridge rather than connecting a
second one, which would double-report every signal into one observer
(`AeroraMissionBridge.cc:70-84`).

Conflict 2 is therefore `RESOLVED`, and [07 WP3-C](07-phase-7-work-packages-and-ownership.md#tab-3--vehicle)
is reduced from "confirm or add the caller" to a test assertion.

The mission module's design posture is the strongest in the overlay: it *compares* two snapshots and
reports agreement, and it refuses to replay payload state. Both are the right shape.

### M-Payload / M-Power / M-Supervisory (Tab 6)

| Item | File | QML | Status |
| --- | --- | --- | --- |
| `AeroraPayloadModel` | `Payload/AeroraPayloadModel.{h,cc}` | `QML_ELEMENT` + `QML_UNCREATABLE` | `IMPLEMENTED-UNCOMPILED` |
| `AeroraPayloadRequest` | `Payload/AeroraPayloadCommandSink.h:31` | none | `IMPLEMENTED-UNCOMPILED` (value type) |
| `AeroraPayloadCommandSink` | `Payload/AeroraPayloadCommandSink.h:114` | none | **`CONTRACT-ONLY` — no implementation anywhere in the overlay** |
| `AeroraPowerModel` | `Power/AeroraPowerModel.{h,cc}` | `QML_ELEMENT` + `QML_UNCREATABLE` | `IMPLEMENTED-UNCOMPILED` — **four `false` flags, zero thresholds** |
| `AeroraReportedValue` | `Supervisory/AeroraReportedValue.{h,cc}` | `QML_ELEMENT` + `QML_UNCREATABLE` | `IMPLEMENTED-UNCOMPILED` — `capability` correctly read-only |
| Painting reported-value set | — | — | **ABSENT.** `P7-PAY-004` |
| Tether reported-value set | — | — | **ABSENT.** `P7-PWR-006` |

`AeroraPowerModel` declares **no outbound method of any kind**. That is the implementation of
`P7-PWR-002`..`005`, not an omission.

### M-Ui (Tab 7)

| Item | File | QML | Status |
| --- | --- | --- | --- |
| `AeroraVehicleUiContext` | `UI/AeroraVehicleUiContext.{h,cc}` | `QML_NAMED_ELEMENT(AeroraUi)` + `QML_SINGLETON` | `IMPLEMENTED-UNCOMPILED` — 5 typed read-only properties |
| `AeroraQmlOverrides` | `UI/AeroraQmlOverrides.{h,cc}` | none | `IMPLEMENTED-UNCOMPILED` — **not installed (HO-TAB7-02)**; allow-list currently empty |
| `AeroraValueDisplay` | `UI/Controls/AeroraValueDisplay.qml` | — | `IMPLEMENTED-UNCOMPILED` — **never linted** |
| `AeroraStateText` | `UI/Controls/AeroraStateText.qml` | — | `IMPLEMENTED-UNCOMPILED` — never linted |
| `AeroraAgeLabel` | `UI/Controls/AeroraAgeLabel.qml` | — | `IMPLEMENTED-UNCOMPILED` — never linted |
| `AeroraTokenDisplay` | `UI/Controls/AeroraTokenDisplay.qml` | — | `IMPLEMENTED-UNCOMPILED` — never linted |
| The other 41 screens | — | — | `DESIGNED` (24) / `SPECIFIED-NOT-DESIGNED` (19) — see [03 §8](03-gui-information-architecture-and-screens.md#8-screen-inventory) |

`AeroraVehicleUiContext` exposes one typed `Q_PROPERTY` per service (`payload`, `power`, `mission`,
`identity`, `capabilities`), all `READ`-only, each null-safe through `QPointer`. It deliberately
exposes **no** `vehicle` property (reachable already as
`QGroundControl.multiVehicleManager.activeVehicle`; a second route to the same writable object widens
the surface for no gain) and **no** `telemetry` property (nothing to show). Both absences are
asserted by `AeroraUiSafetyTest::_uiContextDoesNotExposeTheVehicleOrTheServiceMap`.

**No QML file in the overlay has had any tooling applied to it.** There is no `qmllint` or
`qmlformat` on this host. This is the single largest first-build risk — see
[§10](#10-first-build-expectations).

## 3. QML-reachable surface

The surface is enumerated because a method becomes QML-callable by acquiring `Q_INVOKABLE` or by
being a slot, and a property becomes writable by acquiring a `WRITE` accessor — each a one-line change
in another workstream's header with no compiler warning and no UI diff.

> **The overlay registers 12 QML-reachable types; `AeroraUiSafetyTest::auditedTypes()` covers 10.**
> `AeroraSecurityStatus` (new with Tab 2's WP2-B) and `AeroraFreshness` (unaudited since Phase 6) are
> outside the ratchet. Neither is a live defect — an independent sweep confirms no `Q_PROPERTY`
> carrying `WRITE` or `MEMBER` exists anywhere in `aerora/src/**` — but `P7-AUTH-004`'s guarantee
> currently covers ten of twelve types rather than all of them. Filed as `HO-TAB1-03` to Tab 7.

The ten audited types, reachable from `AeroraUi`:

| Type | QML macro | `Q_INVOKABLE` | Writable properties |
| --- | --- | --- | --- |
| `AeroraVehicleUiContext` | `QML_NAMED_ELEMENT(AeroraUi)` + `QML_SINGLETON` | none | none |
| `AeroraTelemetryState` | `QML_ELEMENT` + `QML_SINGLETON` | 7 static classifiers | none |
| `AeroraStateValue` | `QML_ELEMENT` + `QML_UNCREATABLE` | `ageMs()` | none — `capability` lost its `WRITE` in U3 |
| `AeroraReportedValue` | `QML_ELEMENT` + `QML_UNCREATABLE` | `ageMs()` | none |
| `AeroraFreshness` | `QML_ELEMENT` + `QML_UNCREATABLE` | none | none |
| `AeroraPayloadModel` | `QML_ELEMENT` + `QML_UNCREATABLE` | `reportedValue(id)` | none |
| `AeroraPowerModel` | `QML_ELEMENT` + `QML_UNCREATABLE` | `configuration()`, `reportedValue(id)` | none |
| `AeroraMissionObserver` | `QML_ELEMENT` + `QML_UNCREATABLE` | `verify(depth)` | none |
| `AeroraVehicleCapabilities` | `QML_ELEMENT` + `QML_UNCREATABLE` | `state()`, `isAvailable()`, `reportedKeys()` | none |
| `AeroraVehicleIdentity` | `QML_ELEMENT` + `QML_UNCREATABLE` | none | none |
| `AeroraVehicleServices` | `QML_ELEMENT` + `QML_UNCREATABLE` | none | none |

### Forbidden reaches, and why each is unreachable

| Forbidden | Status | Evidence |
| --- | --- | --- |
| Payload command submission | **Unreachable** | `submitRequest()` / `setCommandSink()` are neither `Q_INVOKABLE` nor slots; no sink implementation exists |
| Power switching | **Unreachable** | `AeroraPowerModel` declares no outbound method; four `false` flags; `kDefinedThresholdCount == 0` |
| MAVLink command authorization | **Unreachable** | Nothing in `Security/` carries a QML macro; `isAuthorizationEnforced()` is plain static C++ |
| Security policy, roles, operator identity | **Unreachable** | Same |
| Mission commanding | **Unreachable** | `AeroraMissionObserver`'s only `Q_INVOKABLE` is a local two-snapshot comparison |
| Flight stabilization, failsafe, radio | **Unreachable** | No Aerora type exposes any of them |
| Fact-group registration | **Unreachable** | `registerGroup` / `registerFactGroup` are plain C++ |
| The service map | **Unreachable** | `service()` / `serviceNames()` stay private to `AeroraVehicleUiContext` |

Signals are excluded from this audit: emitting one from QML re-evaluates bindings and actuates
nothing.

### The one recorded deviation, now closed

`AeroraStateValue::capability` carried `WRITE setCapability` until U3 (`a7b317f89`).
`CapabilityAbsent` is the only input producing `UNAVAILABLE`, so a QML write could have presented a
reported-absent capability as available, or manufactured an `UNAVAILABLE` the vehicle never
reported — exactly what `AERORA-GCS-REQ-029` and `IF-UIS-005` prohibit. It was never reachable from
`AeroraUi`, because no property returns an `AeroraStateValue`; it would have become reachable the
moment `AeroraValueDisplay` was bound to one.

The `WRITE` is gone, the property stays readable, and `setCapability()` remains public C++ for the
capability model. `recordedWritablePropertyDeviations()` returns an empty list, and the assertion is
set equality, so `AeroraUiSafetyTest::_onlyTheRecordedPropertyDeviationIsWritable` now requires
**zero** writable properties across all ten audited types — stricter than before, not relaxed.

Re-verified at this reconciliation, independently of Tab 4's report: every `aerora/**/*.h` grepped
for `Q_PROPERTY` carrying `WRITE` or `MEMBER` returns no match. Both forms were checked because
`QMetaProperty::isWritable()` is true for `MEMBER` properties as well.

That grep is repo-wide rather than scoped to `auditedTypes()`, which matters because the audit list
covers 10 of the 12 QML-registered overlay types (`R7-19`). So "no writable property exists" holds
for all twelve, including the two the test does not yet reach — the conclusion does not depend on
the audit list being complete. What the **test** asserts is still limited to its ten.

Status `IMPLEMENTED-UNCOMPILED`. The change has not been compiled and that test has not been run.

### A noted non-issue

`AeroraStateValue::fact` returns a `Fact *`, and `Fact` carries writable `value` / `rawValue`. That is
upstream QGroundControl and is how parameter editing works. Every Fact in the application has had
that surface since before Aerora existed, no Aerora Fact exists, and no Aerora control writes one.
The authority question for `PARAM_SET` is `src/FactSystem` ownership — `OPEN — UNASSIGNED`, and
`P7-AUTH-003`.

## 4. The deliberate-false flags

Ten compile-time flags and counters encode "not implemented, and not accidentally". They are the
mechanism that keeps `P7-NOINV-001` enforced rather than remembered, and **changing one to `true`
without an approved specification is a requirement violation, not a feature.**

| Flag | File | Value | Keeps open |
| --- | --- | --- | --- |
| `kGroundStationActuatorPathImplemented` | `Payload/AeroraPayloadModel.h:128` | `false` | `P7-PAY-002` |
| `kAutonomousPayloadActionImplemented` | `Payload/AeroraPayloadModel.h:135` | `false` | `P7-PAY-005` |
| `kStationInitiatedPowerActionImplemented` | `Power/AeroraPowerModel.h:140` | `false` | `P7-PWR-002` |
| `kPowerArbitrationImplemented` | `Power/AeroraPowerModel.h:144` | `false` | `P7-PWR-003` |
| `kAutomaticSourceTransitionImplemented` | `Power/AeroraPowerModel.h:148` | `false` | `P7-PWR-004` |
| `kRemainingEnergyEstimationImplemented` | `Power/AeroraPowerModel.h:153` | `false` | `P7-PWR-005`, `P7-ARCH-005` |
| `kGcsPayloadStateReplayImplemented` | `Mission/AeroraMissionResumeBoundary.h:75` | `false` | `P7-MIS-006` |
| `kDefinedThresholdCount` | `Power/AeroraPowerModel.h` | `0` | `P7-SAFE-003` |
| `isRegistrationAvailable()` | `Telemetry/AeroraVehicleFactGroupRegistrar` | `false` | `Q4-002` implementation gate |
| `isAuthorizationEnforced()` | `src/AeroraCorePlugin.h` | `false` | `P7-AUTH-002` |

`AeroraUiSafetyTest` and the per-module tests read these flags, so flipping one without the
corresponding implementation fails a test rather than silently shipping. That is the point.

## 5. Interface completion

### Interfaces that exist with an implementation

| Interface | Implementation | Status |
| --- | --- | --- |
| `AeroraFactGroupRegistrar` | `AeroraVehicleFactGroupRegistrar` | `IMPLEMENTED-UNCOMPILED`, gated false |
| `MAVLinkSigningKeyStore` | `AeroraSigningKeyStore` | `IMPLEMENTED-UNCOMPILED` |
| `FirmwarePlugin` | `AeroraFirmwarePlugin` | `IMPLEMENTED-UNCOMPILED` |
| `FirmwarePluginFactory` | `AeroraFirmwarePluginFactory` | `IMPLEMENTED-UNCOMPILED` |
| `QGCCorePlugin` | `AeroraCorePlugin` | `IMPLEMENTED-UNCOMPILED`, partial |

### Interfaces that exist with no implementation

| Interface | Declaration | Blocks | Owner |
| --- | --- | --- | --- |
| `AeroraPayloadCommandSink` | `Payload/AeroraPayloadCommandSink.h:114` | Any payload actuation | Tab 6 — **and must stay unimplemented until a command path is authorized** |
| `AeroraIdentityProvider` | `Security/AeroraAuthorization.h:113` | All authorization | Tab 2, gated on `Q-029` |
| `AeroraRoleResolver` | `Security/AeroraAuthorization.h:125` | All authorization | Tab 2, gated on `Q-029` |
| `AeroraAuthorizationPolicy` | `Security/AeroraAuthorization.h:138` | All authorization | Tab 2, gated on `Q-029` + ADR-014 |
| `AeroraAuditSink` | `Security/AeroraAuditSink.h:72` | Durable audit, `SCR-SEC-03`, journey 6.6 | Tab 2, gated on `Q-037` |
| `AeroraPlatformKeyProtector` | `Security/AeroraPlatformKeyProtector.h:38` | Key protection on all 5 platforms | Tab 2, gated on `D-006` — an untested crypto backend is worse than none |
| `AeroraMissionExtension` | `Mission/AeroraMissionExtension.h:45` | Aerora complex item, `SCR-MIS-01`..`06` | Tab 5, gated on `Q-021`/`Q-023` |
| `AeroraTelemetrySource` | `Telemetry/AeroraTelemetrySource.h:33` | Source attribution | Tab 4, gated on `IF-MAV-022` |

Eight interfaces, no implementations. **Six of the eight are gated on a decision, not on
engineering.** The two that are not (`AeroraPayloadCommandSink`, `AeroraPlatformKeyProtector`) are
deliberately unimplemented for safety reasons.

### Interfaces that do not exist yet

| Needed | For | Status |
| --- | --- | --- |
| `AeroraAutoPilotPlugin` | `SCR-VEH-01`..`06` | **ABSENT** — seam confirmed at `FirmwarePlugin.h:122` |
| Autonomy proposal model + source interface | `SCR-LIV-05`, `SCR-AIS-01`..`03`, journey 6.4 | **ABSENT** — `P7-AI-004`, nothing may be invented |
| Local persistence (missions, profiles, fleet records, events, audit) | `SCR-FLT-01`..`02`, `SCR-TEL-02`, `SCR-LOG-01`..`02`, `SCR-SEC-03` | **ABSENT** — `P7-DAT-002` |
| Build provenance source | `SCR-SHL-04` | **ABSENT** — `P7-HELP-004` |
| Event timeline model | `SCR-TEL-02` | **ABSENT** — `P7-TEL-004` |
| Painting reported-value set | `SCR-LIV-03`, `SCR-MIS-05`..`06` | **ABSENT** — payload ICD + `Q3-009` |
| Tether reported-value set | `SCR-LIV-04` | **ABSENT** — `P7-PWR-006` |
| Consumable model | `SCR-MIS-06` | **ABSENT** — benchmark gap §7.4; consumption model prohibited by `P7-ARCH-005` without approval |

The local persistence interface is the highest-leverage absent item: **six screens depend on it**,
and it plausibly also serves audit durability (`Q-037`), which would turn two problems into one.

## 6. QGroundControl core modifications

Nine tracked files, all from Phase 6, all enumerated here because `P7-ARCH-021` requires it.

| File | Change | Justification |
| --- | --- | --- |
| `src/API/QGCCorePlugin.h` | `+20` — added the `authorizeMavCommand()` virtual and the `destroyQmlApplicationEngine()` counterpart | The outbound authorization decision point. No alternative seam exists |
| `src/Vehicle/MavCommandQueue.cc` | `+27` — consults `authorizeMavCommand()` at enqueue time | Refusing at enqueue means the command is never queued, so it cannot be retried by the response timer or mis-reported as an unresponsive vehicle |
| `src/Vehicle/Vehicle.{h,cc}` | `+8` / `+14` — `friend class AeroraVehicleFactGroupRegistrar` (ADR-016) and the `startCalibration()` authorization seam (ADR-015) | `FactGroup::_addFactGroup()` is `protected`; upstream already grants friendship to two classes for the same reason. Calibration bypassed the gate |
| `src/Vehicle/VehicleTypes.h` | `+1` | Supporting the above |
| `src/MAVLink/Signing/MAVLinkSigningKeys.{h,cc}` | `+17` / `±44` | Extracting a replaceable key store |
| `src/MAVLink/Signing/CMakeLists.txt` | `+2` | New files |
| `src/MAVLink/Signing/MAVLinkSigningKeyStore.{h,cc}` | **new**, `+84` / `+83` | The store interface `AeroraSigningKeyStore` implements |
| `test/MAVLink/Signing/CMakeLists.txt`, `test/Vehicle/CMakeLists.txt` | `+3` each | Registering two core tests |

### Assessment

| Property | Value |
| --- | --- |
| Files touched | 9 tracked, 2 added |
| Lines added to core | ~220, of which ~167 are the two new signing-store files |
| Changes that alter default behaviour with no overlay | **None.** `authorizeMavCommand()` base returns `true`; the friend declaration grants nothing without `AERORA_VEHICLE_FACTGROUP_FRIENDSHIP` |
| Changes that could break an upstream rebase | The `MavCommandQueue.cc` insertion point and the `Vehicle.cc` calibration seam. Both are localised and would surface as merge conflicts, not silent behaviour changes |

This is a defensible core footprint. `P7-ARCH-021` is satisfied, and no Phase 7 work should enlarge
it without the same per-file justification.

### The build-identification requirement

`QGC_CUSTOM_BUILD` routes `QGCCorePlugin::instance()` through `CUSTOMCLASS::instance()`, which is how
the authorization gate is bound to the core send path. **Without it the overlay compiles, the
application starts, and the gate is never consulted** (ADR-012). The overlay must be configured with
`-DQGC_CUSTOM_DIR=aerora`. This is a silent-failure mode and belongs in the first-build checklist.

## 7. Missing functionality

Named rather than mocked, per the Phase B rule. Each row is a real gap with an owner.

| # | Missing | Impact | Owner | Gated on |
| --- | --- | --- | --- | --- |
| 1 | Authorization policy, identity provider, role resolver | **Every command is permitted.** No operator action surface can exist | Tab 2 | `Q-029` → ADR-014 |
| 2 | `AeroraAutoPilotPlugin` | 6 vehicle screens unreachable | Tab 3 | Nothing — ready to implement |
| 3 | `FirmwarePlugin::toolIndicators()` override | No per-vehicle Aerora indicator | Tab 3 | Nothing — ready to implement |
| 4 | ~~Interceptor install (HO-TAB7-02)~~ | — | Tab 1 | **DONE** — U1, `84bbb3a6e`, `IMPLEMENTED-UNCOMPILED` |
| 5 | ~~`AeroraControlsModule` `IMPORTS QGC` + resource alias (`D-UI-04`)~~ | — | Tab 1 | **DONE** — U2, `a73699796`, `IMPLEMENTED-UNCOMPILED` |
| 6 | ~~`AeroraStateValue::capability` `WRITE` removal (`D-UI-03`)~~ | — | Tab 4 | **DONE** — U3, `a7b317f89`, `IMPLEMENTED-UNCOMPILED` |
| 7 | Fact group messages + freshness budgets | Telemetry explorer and quality dashboard have no data | Tab 4 | `Q-036`, `Q-023`/`Q-024` |
| 8 | Aerora complex mission item | Painting and mission-planning journeys cannot start | Tab 5 | `Q-021`/`Q-023` |
| 9 | Local persistence layer | 7 screens; carries audit durability but **not** tamper-evidence | **Tab 1** (store), domain tabs (writers) | **`DESIGNED`** — [09](09-local-persistence-architecture.md). Implementation ready to start |
| 10 | Autonomy proposal model + source interface | Supervised-autonomy journey | Tab 6 + Tab 1 | `P7-AI-004`, `D-007` |
| 11 | Painting / tether / consumable reported-value sets | Payload and power panels have nothing to show | Tab 6 | Payload ICD, `Q3-009` |
| 12 | Durable, tamper-evident audit sink | No post-flight Aerora record exists | Tab 2 | `Q-037` |
| 13 | Fleet identity format | Fleet records cannot be keyed across sessions | Tab 3 | Product decision |
| 14 | `src/FactSystem` / `PARAM_SET` authority owner | No claim about parameter authorization is supportable | **Programme** | Assignment — see [07 Tab 9](07-phase-7-work-packages-and-ownership.md#tab-9--fact-system-and-parameter-authority-new) |
| 15 | Overlay CI test leg | No test has ever run | Tab 8 | `Q-035`, `D-006` |

**Items 2, 3, 4, 5 and 6 are gated on nothing.** They are five small, independent, high-value changes
that unblock twelve screens between them. That is the Phase C starting set
([05 §5](05-implementation-dependency-graph.md#5-the-unblocked-five)).

## 8. Test inventory

14 tests registered in `aerora/CMakeLists.txt`, all inside `if(QGC_BUILD_TESTING)`. Two further tests
live in the core test tree. **None has been executed.**

| Test | Labels | Module | Covers |
| --- | --- | --- | --- |
| `AeroraCalibrationAuthorizationTest` | `Unit` | Security | ADR-015 calibration gate |
| `AeroraCalibrationEnforcementTest` | `Integration Vehicle` | Security | Gate reached from the vehicle path |
| `AeroraSigningKeyStoreTest` | `Unit` | Security | Key store behaviour |
| `AeroraTelemetryStateTest` | `Unit` | Telemetry | The 7-state classifier |
| `AeroraTelemetryAttachmentTest` | `Unit` | Telemetry | Fact-group attachment contract |
| `AeroraReportedValueTest` | `Unit` | Supervisory | Reported-value primitive, read-only capability |
| `AeroraVehicleIntegrationTest` | `Unit` | Vehicle | Composition root; asserts the 4 non-overridden base defaults |
| `AeroraVehicleLifecycleTest` | `Integration Vehicle` | Vehicle | Attach/detach across vehicle lifetime |
| `AeroraMissionTest` | `Unit MissionManager` | Mission | Snapshot, index, agreement |
| `AeroraMissionSyncTest` | `Integration Vehicle MissionManager` | Mission | Upload/download agreement |
| `AeroraPayloadModelTest` | `Unit` | Payload | Reported values, no actuator path |
| `AeroraPowerModelTest` | `Unit` | Power | Reported values, four `false` flags, zero thresholds |
| `AeroraUiFoundationTest` | `Unit` | UI | Interceptor allow-list, forbidden paths, URL non-interception |
| `AeroraUiSafetyTest` | `Unit` | UI | **Meta-object surface audit — the QML safety boundary** |
| `MAVLinkSigningKeyStoreTest` | core tree | Core | Store seam |
| `MavCommandAuthorizationTest` | core tree | Core | Authorization at enqueue |

### Coverage gaps

| Not covered | Needs |
| --- | --- |
| Any QML rendering behaviour | A Qt Quick Test target (HO-TAB7-04 / `D-UI-05`) |
| QML syntax, unqualified access, import resolution | `qmllint` |
| C++ declaration/definition parity | A compiler |
| Whether `Aerora.Controls` resolves at runtime | A build and a launch |
| Whether the safety test's reviewed-method lists match the real meta-objects | **A run.** The lists were derived by reading headers; a first run may fail on a list error rather than a real regression |
| Authorization denial paths | An overlay build with `QGC_BUILD_TESTING=ON` (`Q-035`) |
| Visual distinguishability of two indicators | No test can assert this. `DEMO` verification, gated on `Q-040` |

The fifth row matters for expectation-setting: a first run of `AeroraUiSafetyTest` failing is the
**likely** outcome and is not evidence of a regression.

## 9. Static checks available

The only verification possible on this host. All four are PowerShell, which is present.

| Tool | Checks | Last result |
| --- | --- | --- |
| `.kiro/tools/aerora-ui-static-check.ps1` | LF endings, no tabs, newline at EOF, no trailing whitespace, ≤120 columns, no `Q_ASSERT`, no uncategorised `qDebug()`, no `QTest::ignoreMessage`, no fixed-delay `qWait`, balanced delimiters, `#pragma once`, no hard-coded MAVLink identifier | **15/15 pass** at the UI foundation pass |
| `.kiro/tools/aerora-manifest-check.ps1` | Every declared manifest entry resolves; every source on disk is declared; every `_TEST_NAMES` entry has an `add_qgc_test` and conversely | **0 problems**, 109 entries |
| `.kiro/tools/aerora-preflight.ps1` | Overlay preflight | — |
| `.kiro/tools/aerora-ui-parity-check.ps1` | UI parity | — |

**These are file-existence and text-hygiene checks. They are not a configure, a compile or a lint.**
They cannot detect a missing `IMPORTS QGC`, a type mismatch, an unresolved QML import, or a
declaration/definition mismatch.

### What must be re-run in Phase D

All four, plus a recorded result. The manifest check in particular must be re-run after every Phase C
commit that adds a file, because a source added without a manifest entry compiles into nothing and
fails silently.

## 10. First-build expectations

Stated so the first build is read as a measurement rather than as a regression.

| Expectation | Why |
| --- | --- |
| **Compile errors are likely** | 105 C++ files, never compiled. Declaration/definition parity was checked by reading only |
| **`qmllint` errors are near-certain** | 4 QML files, zero tooling applied. Unqualified access and import resolution are exactly what reading cannot catch |
| **`AeroraUiSafetyTest` may fail first run** | Its reviewed-method lists were derived from headers. A divergence fails the test, which is the intent, but the first failure is more likely a list error than a regression |
| **`Aerora.Controls` may not resolve** | `D-UI-04`: the module block omits `IMPORTS QGC` |
| **Overlay QML overrides will not load** | HO-TAB7-02: the interceptor is not installed. Allow-list is empty, so nothing is lost today — but it would fail silently once populated |
| **Without `-DQGC_CUSTOM_DIR=aerora` everything "works" and the gate is never consulted** | ADR-012. The worst failure mode in the build, because it is invisible |
| **`AeroraMissionTest.cc` will not compile — 11 known call sites** | Found by source inspection, not by a compiler. `UnitTest::expectLogMessage()` takes `const char *category` (`test/UnitTestFramework/UnitTest.h:472`) and there is no `QString` overload; the file passes `QString::fromUtf8(kLogCategory)` at lines 107, 113, 120, 366, 376, 382, 390, 406, 521, 538, 570. Fix is to drop the wrapper — `kLogCategory` is already `constexpr const char *`. Tab 5's file, filed as `HO-TAB1-01` |

### A confirmed latent compile error, and what it implies

The `AeroraMissionTest.cc` row above is the first defect found in the overlay by reading rather than
by building, and it is worth recording how: `expectLogMessage` has exactly one declaration, the
argument type does not convert, and the call sites were enumerated mechanically. The same scan
confirmed the other 47 call sites across seven test files are correct and that every test-side
category string matches its `QGC_LOGGING_CATEGORY` definition exactly — a mismatch there would be a
*silent* test failure rather than a compile error, which is worse.

It also sets the expectation for `R7-01` concretely: 105 never-compiled C++ files produced at least
one type error that a careful reviewer missed eleven times in one file. The first build will find
more.

### Build order for the first attempt

1. Configure with `-DQGC_CUSTOM_DIR=aerora -DQGC_BUILD_TESTING=ON`. **Verify `QGC_CUSTOM_BUILD` is actually on** before interpreting any result
2. Build. Fix compile errors module by module; do not batch
3. Run `qmllint` over `aerora/src/UI/Controls/*.qml`
4. `ctest -R AeroraTelemetryStateTest` first — the most self-contained test with the fewest dependencies
5. Then the remaining `Unit`-labelled Aerora tests individually
6. Then `ctest -L Unit` for the whole suite, to confirm no upstream regression
7. Only then the `Integration` labels

## 11. Completion scorecard

| Dimension | Count | Note |
| --- | --- | --- |
| Overlay files declared and resolving | 109 | Static check only |
| Production C++ sources | 69 | `IMPLEMENTED-UNCOMPILED` |
| QML files | 4 | `IMPLEMENTED-UNCOMPILED`, **never linted** |
| Tests registered | 14 + 2 core | **Never executed** |
| Subsystems with a working implementation | 6 of 7 | Security is the exception — the point exists, the logic does not |
| Interfaces with an implementation | 5 | — |
| Interfaces without an implementation | 8 | 6 gated on a decision, 2 deliberately unimplemented |
| Interfaces not yet existing | 8 | — |
| Deliberate-`false` flags | 10 | The no-invention enforcement mechanism |
| Core files modified | 9 + 2 added | Each individually justified |
| Class-X forks | **0** | Target is 0 |
| Screens designed | 24 of 45 | — |
| Screens specified but not designed | 19 of 45 | **16 blocked on a decision, not on capacity** |
| Features `VERIFIED` | **0** | No build exists |

### The one-sentence summary

The Aerora overlay is a **structurally complete supervisory foundation with no data in it and no
authorization behind it**: the state vocabulary, the service composition, the command decision point
and the UI safety boundary are all implemented in source and unexecuted, while the telemetry items,
the authorization policy, the mission content and the persistence layer are either gated on named
product decisions or not yet written.

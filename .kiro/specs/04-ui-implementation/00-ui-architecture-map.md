# Aerora GCS UI Architecture Map

Owner: **Tab 7 (`M-Ui`)**. Inventory and classification. **No UI was implemented by this
document.**

Originally verified by reading the checkout, with no build, launch, lint or test executed. As of
2026-10-18 the overlay compiles and its tests pass in Windows CI; **no launch and no `qmllint` run
has happened**, so nothing about rendering or QML lint is evidenced. See
[Verification status](#verification-status) for what each claim rests on.

> **Superseded in part.** The UI foundation pass that followed closed Q7-001, Q7-002 and Q7-003 and
> added the first `aerora/src/UI/` module. Where this document and
> [01-ui-foundation-closure.md](01-ui-foundation-closure.md) disagree, **the closure document is
> authoritative**. Specifically: Q7-002 needs no Tab 3 change, and the Q7-001 ordering question is
> informed by in-tree precedent. Read that narrowly: precedent indicates the **expected**
> configuration order, and it does not by itself prove that registration and type-name resolution
> actually occur in the running Aerora application. That measurement stays `NOT-EVIDENCED` — see
> *Open: where do Aerora C++ QML types register?* below. The area map, the classification scheme
> and the seam inventory below are unchanged and remain current.

## Contents

- [Scope and ownership](#scope-and-ownership)
- [Classification scheme](#classification-scheme)
- [Seam inventory, as verified](#seam-inventory-as-verified)
- [Corrections to prior Aerora UI specifications](#corrections-to-prior-aerora-ui-specifications)
- [The binding contract: what the backend actually exposes to QML](#the-binding-contract-what-the-backend-actually-exposes-to-qml)
- [State-class rendering](#state-class-rendering)
- [UI area map](#ui-area-map)
- [Product profile consumption](#product-profile-consumption)
- [Deliberately disabled surfaces](#deliberately-disabled-surfaces)
- [Planned Aerora QML module layout](#planned-aerora-qml-module-layout)
- [Test strategy](#test-strategy)
- [Blockers](#blockers)
- [Verification status](#verification-status)

## Scope and ownership

| May modify | May not modify |
| --- | --- |
| Aerora QML / UI files (none exist yet) | Security, Vehicle, Telemetry, Mission, Payload, Power backends |
| Aerora UI tests | Any `src/` upstream file |
| UI-specific documentation (this file) | Root `CMakeLists.txt`, `src/CMakeLists.txt` |

### Tab numbering collision

[29-integration-status.md](../03-implementation-specification/29-integration-status.md) proposes
"Tab 7 (new) — **Fact System / parameter authority**", owning `src/FactSystem`, `PARAM_SET` and its
conflict 3. This workstream was instead assigned **UI / QML**. Two consequences, both for Tab 1:

| Item | State |
| --- | --- |
| Tab number 7 used for two different work packages | Needs renumbering by the orchestrator |
| `src/FactSystem` / `PARAM_SET` ownership | Still **`OPEN — UNASSIGNED`**. Not claimed here, and outside this workstream's declared ownership |

## Classification scheme

Every UI surface falls into exactly one of five classes. The class determines the maintenance cost
and the review obligation, so it is recorded per surface rather than decided per change.

| Class | Meaning | Mechanism | Cost |
| --- | --- | --- | --- |
| **R — Reusable QGC UI** | Upstream UI used unchanged | None | None |
| **C — Configuration-only** | Upstream UI whose behaviour changes through data or a C++ virtual, with no new QML | `QGCOptions` / `QGCCorePlugin` virtual, settings JSON, palette override, CMake cache override | None |
| **O — Aerora overlay UI** | New Aerora content added at a seam upstream provides for the purpose | Tier-1 C++ list, tier-2 QML seam, new Aerora QML module | Low |
| **X — Aerora replacement UI** | A populated upstream QML file replaced wholesale | URL interceptor over a mirrored resource | **High — a silent fork** |
| **P — Product-specific UI** | Content belonging to one product family, loaded by profile id | Profile-keyed module slot | Low, isolated |

Rule, inherited from [`AERORA-UI-REQ-004`](../01-aerora-architecture/ui-architecture.md): use the
lowest-cost class that works. `X` requires a recorded fork point, a named owner and a drift review.

## Seam inventory, as verified

### Tier 1 — C++ virtuals

| Surface | Declaration | Aerora relevance |
| --- | --- | --- |
| `QGCOptions` — 30 virtuals | `src/API/QGCOptions.h:45-175` | `multiVehicleEnabled()`, `preFlightChecklistUrl()`, `combineSettingsAndSetup()`, `showSensorCalibration*()`, `toolbarBackgroundLight/Dark()`, `toolbarHeightMultiplier()`, `missionWaypointsOnly()`, `showMissionAbsoluteAltitude()` |
| `QGCFlyViewOptions` — 6 virtuals | `src/API/QGCOptions.h:11-41` | `showInstrumentPanel()`, `showMultiVehicleList()`, `showMapScale()`, `guidedBarShowEmergencyStop/Orbit/ROI()` |
| `QGCCorePlugin::toolBarIndicators()` | `src/API/QGCCorePlugin.h:240`, base `QGCCorePlugin.cc:333-343` | **Vehicle-independent indicator list.** Base returns one entry (`RTKGPSIndicator.qml`) |
| `FirmwarePlugin::toolIndicators(const Vehicle*)` | `src/FirmwarePlugin/FirmwarePlugin.h:344`, base `.cc:237-266` (10 entries) | **Per-vehicle indicator list**, reached through `Vehicle::toolIndicators()` (`Vehicle.cc:2595-2602`, null-guarded). `AeroraFirmwarePlugin` does **not** override it today |
| `QGCCorePlugin::customMapItems()` | `src/API/QGCCorePlugin.h:158` | **The map-item seam.** Returns a `QmlObjectListModel` of `QmlComponentInfo` with `url` set |
| `QGCCorePlugin::paletteOverride()` | `src/API/QGCCorePlugin.h:111` | Every colour role in all four theme/group combinations routes through it |
| `QGCCorePlugin::overrideSettingsGroupVisibility()` | `src/API/QGCCorePlugin.h:84`, consumed `src/Settings/SettingsGroup.cc:8` | Read once at group construction |
| `QGCCorePlugin::registerCustomSettings()` | `src/API/QGCCorePlugin.h:95` | Aerora settings group registration |
| `QGCCorePlugin::factValueGridCreateDefaultSettings()` | `src/API/QGCCorePlugin.h:113` | Default value-grid cells |
| `QGCCorePlugin::analyzePages()` | `src/API/QGCCorePlugin.h:71` | Aerora analyze pages |
| `QGCCorePlugin::createQmlApplicationEngine()` / `destroyQmlApplicationEngine()` | `src/API/QGCCorePlugin.h:117`, `:122` | Import paths, URL interceptor install/release |
| `QGCCorePlugin::complexMissionItemNames()` / `createComplexMissionItem()` / `planCreators()` | `src/API/QGCCorePlugin.h:205`, `:216`, `:225` | Product mission content |
| `QGCCorePlugin::showAdvancedUIMessage()`, `firstRunPromptCustomIds()`, `firstRunPromptResource()` | `src/API/QGCCorePlugin.h:98`, `:235`, `:238` | Disclosure text, first-run prompts |

**`CONSTANT` properties, read once.** `toolBarIndicators`, `analyzePages`, `options`,
`customMapItems`, `showInstrumentPanel`, `showMultiVehicleList`, `showMapScale` are declared
`CONSTANT` (`src/API/QGCCorePlugin.h:48-55`, `src/API/QGCOptions.h:16-18`). A value that becomes
known after QML binds will not be picked up. Anything Aerora wants in these lists must be decided
before the first QML read, which rules out populating them from vehicle-reported capability.

### Tier 2 — purpose-built QML seams

| Seam file | Instantiated at | Upstream content | Aerora use |
| --- | --- | --- | --- |
| `src/FlyView/FlyViewCustomLayer.qml` | `FlyView.qml:145-152` | Empty `Item` with `occluders`, `customOccluders`, `mapControl` | The Aerora fly-view overlay surface |
| `src/FlyView/CustomGuidedActionsController.qml` | `GuidedActionsController.qml:187-190`, dispatched at `:550` and `:695` | `QtObject` with `customConfirmAction` / `customExecuteAction` returning `false` | Aerora guided actions — **none, see [Deliberately disabled surfaces](#deliberately-disabled-surfaces)** |
| `src/FlyView/FlyViewAdditionalCustomActionsList.qml` | `FlyViewAdditionalActionsButton.qml:28-31` | Invisible `Item`, empty `model` array, `anyActionAvailable` | Aerora additional-actions entries — **none today** |

`customOccluders` on the first seam feeds back into map recentering (`FlyView.qml:49`), so an
Aerora overlay panel that covers the map must publish its rectangle or the vehicle will be hidden
underneath it.

The guided-actions seam's `false` return means "not handled here". It is a clean delegation
protocol and it is the only outbound-action seam available to Aerora UI. It stays unused while no
Aerora action has a command path.

### Tier 3 — populated-file replacement

| File | Reason | Upstream content | Fork point | Owner |
| --- | --- | --- | --- | --- |
| `src/FlyView/FlyViewToolStripActionList.qml` | The tool strip is a flat `model: [...]` literal with no insertion seam | 8 entries: `Viewer3DShowAction`, `PreFlightCheckListShowAction`, `GuidedActionTakeoff`, `GuidedActionLand`, `GuidedActionRTL`, `GuidedActionPause`, `FlyViewAdditionalActionsButton`, `FlyViewGripperButton` | To be recorded at the commit the replacement is authored against | **`OPEN` — Q-038** |

Composition chain: `FlyViewToolStripActionList.qml` → `FlyViewToolStrip.qml:12-18` →
`FlyViewWidgetLayer.qml:143`.

**This replacement is not needed yet.** Aerora has no tool-strip action to add, because every
candidate action is a command with no path. The entry stays at zero until an Aerora action exists,
which keeps the tier-3 count at **0** rather than 1.

### The override mechanism

Verified at `custom-example/CMakeLists.txt:79-97` and `custom-example/src/CustomPlugin.cc:240-296`.

```text
engine requests  qrc:/qml/QGroundControl/FlyView/FlyViewCustomLayer.qml
      │
      ▼
interceptor tests  :/Custom/qml/QGroundControl/FlyView/FlyViewCustomLayer.qml
      │
      ├── QFile::exists  ──►  overlay file loads
      └── absent         ──►  upstream file loads, unchanged
```

Per-file, opt-in by existence. No two resources share a URL, so there is no precedence question for
QML. The interceptor acts on `DataType::QmlFile` and `UrlString` with scheme `qrc` only — a C++
`QFile(":/json/...")` read never passes through it, which is **Q4-001** and applies to fact
metadata and settings-group JSON.

#### The mechanism is not wired in the Aerora overlay

`AeroraCorePlugin` overrides **only** `init()` and `authorizeMavCommand()`. It does not override
`createQmlApplicationEngine()`, so **no URL interceptor is installed and no Aerora QML import path
is added.** Grepping `aerora/**` for `createQmlApplicationEngine`, `toolBarIndicators`,
`toolIndicators`, `customMapItems`, `paletteOverride`, `analyzePages`, `registerCustomSettings`,
`planCreators`, `QGCOptions` and `QGCFlyViewOptions` returns **no override of any of them**.

The consequence is concrete: a tier-2 or tier-3 QML file added to the overlay today would be
compiled into a resource and **never loaded**, with no error. That is exactly the silent failure
`IF-UIS-014` and R-019 describe, and it is the first thing the Aerora UI workstream must build.
`AeroraCorePlugin`'s own header records resource overrides as belonging to "the overlay foundation
work", so this is UI-owned rather than a security-workstream omission — but `AeroraCorePlugin.cc`
is a Tab 2 file, so the override must be coordinated rather than taken unilaterally.

### Seams the prior specifications under-counted

Four extensible surfaces are data-driven and need neither a QML override nor a C++ list:

| Surface | Mechanism | Class |
| --- | --- | --- |
| Instrument panel widget set | `src/FlyView/FlyViewInstrumentPanel.qml` → `src/QmlControls/SelectableControl.qml` → `enumValues` of the `instrumentQmlFile2` fact in `src/Settings/FlyView.SettingsGroup.json`. Each enum value **is a QML file path** | **C** |
| Settings pages | Generated at configure time from `src/AppSettings/pages/SettingsPages.json` + `*.SettingsUI.json`. Overlay keys `insertAfter` / `insertBefore` / `remove`, plus replace-by-name. CMake auto-detects `${QGC_CUSTOM_DIR}/src/AppSettings/pages` and `/src/Settings` (`src/AppSettings/CMakeLists.txt:26-57`) | **C** |
| Map items | `QGCCorePlugin::customMapItems()` returning `url`-bearing objects; `CustomMapItems.qml` is the consumer, not the seam | **O** |
| Branding | `aerora/cmake/CustomOverrides.cmake` cache overrides + resource alias at the stock name | **C** |

`MainWindow.qml` additionally publishes facilities any Aerora page may use without a core edit:
`globals.activeVehicle`, `allowViewSwitch()`, `globals.navigationBlockedReason`,
`showIndicatorDrawer()`, `showMessageDialog()`, `showCriticalVehicleMessage()`. The
`navigationBlockedReason` mechanism is the sanctioned way to block operator navigation during a
safety-critical operation.

## Corrections to prior Aerora UI specifications

Four statements in [ui-architecture.md](../01-aerora-architecture/ui-architecture.md),
[16-ui-state-icd.md](../02-interface-specification/16-ui-state-icd.md) and
[05-qml-implementation-map.md](../03-implementation-specification/05-qml-implementation-map.md) do
not match the checkout. Recorded rather than silently worked around.

| Prior statement | What the source says | Consequence |
| --- | --- | --- |
| "Four purpose-built empty QML seams", including `src/FlightMap/MapItems/CustomMapItems.qml` | `CustomMapItems.qml` is **not empty**. It is an `Instantiator` over `QGroundControl.corePlugin.customMapItems` that calls `Qt.createComponent(object.url)` and `map.addMapItem()` | **There are three tier-2 QML seams, not four.** Map items are a **tier-1 C++** seam. Overriding `CustomMapItems.qml` would replace a populated upstream file — a tier-3 fork taken by mistake |
| "A C++ virtual on … `QGCFlyViewOptions`" implying `src/API/QGCFlyViewOptions.h` | No such file. `QGCFlyViewOptions` is declared inside `src/API/QGCOptions.h:11-41` | Documentation only |
| Tier-3 override count is **1** | Nothing in the overlay overrides anything. The count is **0**, and the tool-strip entry is not yet needed | The architecture is currently cheaper than specified. Keep it there |
| "Aerora QML modules … registered through `qt_add_qml_module` and exported as libraries" (`AERORA-UI-REQ-010`) | **True as of `602b38a18`** — `AeroraControlsModule`, URI `Aerora.Controls`, seven QML files, exported through `CUSTOM_LIBRARIES` (`aerora/CMakeLists.txt:195-208`). It was *not* true when this row was written, which is what the row records. The C++ half is unchanged and separate: Aerora's `QML_ELEMENT` types reach the application target through `CUSTOM_SOURCES` (`src/CMakeLists.txt:202-223`, `target_sources` at `:208`), and that target is itself a QML module with **`URI QGC`** (root `CMakeLists.txt:422-428`) | QML half **closed**. C++ half: see the registration question below |

### Open: where do Aerora C++ QML types register?

Eleven Aerora types carry `QML_ELEMENT` and are compiled into the application target, not into a
QML module of their own:

`AeroraTelemetryState` (also `QML_SINGLETON`), `AeroraStateValue`, `AeroraFreshness`,
`AeroraReportedValue`, `AeroraVehicleServices`, `AeroraVehicleIdentity`,
`AeroraVehicleCapabilities`, `AeroraPayloadModel`, `AeroraPowerModel`, `AeroraMissionObserver`,
and the gadgets `AeroraTelemetrySource`, `AeroraMissionIndex`, `AeroraMissionSnapshot`,
`AeroraMissionAgreement`, `AeroraMissionItemFingerprint`, `AeroraPayloadRequest`.

Two facts were recorded as making the registration outcome uncertain. **One is now closed and the
other is narrowed but still `NOT-EVIDENCED`.**

**1. Configure ordering — still open, but weaker than first stated.** Root `CMakeLists.txt` calls
`add_subdirectory("${QGC_CUSTOM_DIR}")` at `:403`, then
`qt_add_qml_module(${CMAKE_PROJECT_NAME} URI QGC ...)` at `:422-428`, then `add_subdirectory(src)`
at `:442` — and `CUSTOM_SOURCES` are attached at `src/CMakeLists.txt:208`, **after**
`qt_add_qml_module` has run on that target, from a different directory scope.

What narrows it: Qt documents this registration as a **build-time** scan, not a configure-time one
— the backing target's C++ sources are scanned at build time to generate the typeinfo file and the
type-registration translation unit, which is then added back to the target as a source, and this
requires `AUTOMOC`
([qt_add_qml_module](https://doc.qt.io/qt-6/qt-add-qml-module.html)). `AUTOMOC` is on, via
`qt_standard_project_setup` at root `:289`, which precedes both the target and the module call. A
target's source list is finalised at generate time, after every `add_subdirectory`, so a build-time
scan would see the `:208` additions. That is an argument from documented behaviour, not a
measurement, so the question stays open — it is `SOURCE-INSPECTED` reasoning, nothing more.

The overlay has since been built: Windows CI run 39146146397, commit `915dd09ae`, 2026-10-18. That
removes the `D-006` precondition for the `Q7-001` experiment below, but it does **not** answer this
question, and the status stays `NOT-EVIDENCED`. Two reasons. The experiment was never performed —
nobody inspected the generated type-registration translation unit, `qmldir` or `QGC.qmltypes`, and
the run uploaded only test results. And a green build is not evidence either way: a registration
that did not happen is silent at build time and surfaces only when QML resolves a type **by name**,
which no Aerora test does.

**2. The `QGC` URI — closed, and it is not a contradiction.** Two separate points, both settled by
reading source:

- `AERORA-UI-REQ-010` is satisfied by the **QML** module, not by the C++ types. Aerora QML *files*
  go into a named module, `Aerora.Controls`. Aerora C++ types sharing `QGC` with upstream's is
  upstream's own convention — see
  [01 § Where Aerora types land](01-ui-foundation-closure.md#where-aerora-types-land-and-how-qml-reaches-them).
  The "contradicts `AERORA-UI-REQ-010`" claim this section used to carry is withdrawn.
- Registering under `QGC` does **not** put the types out of reach of Aerora QML. Every Aerora QML
  file carries a file-level `import QGroundControl`, and `QGroundControlModule` declares
  `IMPORTS QGC` (`src/CMakeLists.txt:22-23`). Qt documents `IMPORTS` as propagating to a module's
  *importers* — if a file imports the module, it also imports everything under that module's
  `IMPORTS`. So `AeroraSecurity`, `AeroraTelemetryState`, `AeroraStateValue` and `AeroraUi` are in
  scope through a documented, consumer-direction chain, with no change to any file. This is the
  evidence that collapsed the integration review's F1 from seven files to one; see `HO-TAB1-13`.

### `Q7-001` — the experiment that resolves it

Status **`NOT-EVIDENCED`**, no longer `BLOCKED` on `D-006`: CI now builds the overlay, so steps 1-4
are runnable. Not `CONTRACT-ONLY` and not `VERIFIED`: the contract is readable, the outcome is not.
Owner Tab 8. In order, stopping at the first failure:

1. Configure only — `just configure`. A `qt_add_qml_module` warning about the backing target's
   directory structure is expected and is not the failure being looked for.
2. Grep the generated type-registration translation unit in the app target's binary directory for
   `AeroraSecurityStatus` and `AeroraVehicleUiContext`. Present ⇒ the `:208` sources were scanned
   and ordering is a non-issue.
3. Grep the generated `QGC` `qmldir` and `QGC.qmltypes` for `AeroraSecurity`, `AeroraUi`,
   `AeroraTelemetryState`, `AeroraStateValue`. Present ⇒ registered under the URI as expected.
4. If 2 or 3 is empty, the ordering hazard is real. The fix is to register the Aerora QML-exposed
   C++ through `qt_target_qml_sources()` on the app target from the overlay, rather than relying on
   the already-executed `qt_add_qml_module` call to pick them up.
5. Build, launch, open the Operations Hub from the Analyze menu and read the Fly View toolbar.
   **Only this step licenses `VERIFIED`**, and only for what it actually exercised.

Steps 1-4 are configuration and build evidence. None of them has been run. The reason has changed:
it is no longer that no build is possible — the Windows CI leg configures and builds the overlay —
but that the steps inspect generated files on the build host and nothing collects or publishes
them. Step 5 still requires a launch, which has not happened. The authoring host itself remains
without a compiler or Qt; see `D-006`.

## The binding contract: what the backend actually exposes to QML

This is the authoritative list of what Aerora UI may bind to. Anything absent here does not exist
for QML and must not be displayed.

### `M-Telemetry`

| Type | QML exposure | Readable members |
| --- | --- | --- |
| `AeroraTelemetryState` | `QML_ELEMENT` + `QML_SINGLETON` | Enums `Class` (7), `Kind` (4), `Observation` (5). `Q_INVOKABLE` `resolve`, `isAuthoritative`, `isCurrent`, `isSubordinate`, `carriesValue`, `isNonValue`, `classToString`, `kindToString` |
| `AeroraStateValue` | `QML_ELEMENT` + `QML_UNCREATABLE` | `fact`, `name`, `declaredKind`, `freshnessClass`, `stateClass`, `freshnessEvaluated`, `observed`, `receiveTimeUs`, `source`, `capability`; `Q_INVOKABLE ageMs()`; `stateChanged` |
| `AeroraFactGroupBase` | `Q_INVOKABLE stateValue(factName)` | Per-fact state value lookup |
| `AeroraTelemetrySource` | `Q_GADGET` | `systemId`, `componentId`, attribution |

**No instance of `AeroraStateValue` exists in the build.** All six Aerora fact groups are
`CONTRACT-ONLY` — no Aerora dialect, so no `handleMessage` body, so no Aerora `Fact`. And
`factGroups()` is never called (Q4-002), so nothing reaches a vehicle's fact-group map.

| Constraint | Effect on UI |
| --- | --- |
| No freshness threshold is configured (**Q-036**) | Every item resolves `ObservedFreshnessUnevaluated`; `freshnessEvaluated` is **false**. The UI must surface **age**, never a freshness verdict. `STALE` is unreachable |
| Nothing sets `capability` (**Q3-009 / F3.1-05**) | `UNAVAILABLE` is unreachable. An unreported item is `UNKNOWN` |
| `ageMs()` is a function, not a notifying property | Age must be re-read on a timer by the consumer; it cannot be bound |

### `M-Vehicle`

| Type | QML exposure | Readable members |
| --- | --- | --- |
| `AeroraVehicleServices` | `QML_ELEMENT` + `QML_UNCREATABLE` | `vehicle`, `identity`, `capabilities`, `classification`, `productProfileId`; signals `classificationChanged`, `productProfileChanged`, `serviceAttached` |
| `AeroraVehicleIdentity` | `QML_ELEMENT` + `QML_UNCREATABLE` | `mavlinkSystemId`, `mavlinkComponentId` (both `CONSTANT`), `fleetIdentityKnown`, `fleetIdentity` |
| `AeroraVehicleCapabilities` | `QML_ELEMENT` + `QML_UNCREATABLE` | Enums `Category` (5), `State` (3); `Q_INVOKABLE` `state`, `isAvailable`, `reportedKeys` |
| `AeroraProductProfile`, `AeroraProductProfileRegistry` | **None** | Not QML-visible at all |

### `M-Payload`

| Member | Exposure |
| --- | --- |
| `identity`, `payloadState`, `health`, `safetyState`, `emergencyStop` | `Q_PROPERTY`, each an `AeroraReportedValue *`, `CONSTANT` |
| `controlsPermitted`, `commandPathInstalled`, `requestCount` | `Q_PROPERTY` with notify |
| `reportedValue(itemId)` | `Q_INVOKABLE` |
| `submitRequest()` | **Not `Q_INVOKABLE`. QML cannot call it.** |
| `reportedValues()` | Not `Q_INVOKABLE`, not a property. QML cannot enumerate items; the five named properties are the whole set |

### `M-Power`

| Member | Exposure |
| --- | --- |
| `tetherConfiguration`, `batteryConfiguration`, `effectiveState`, `reportedState`, `sourceTransition`, `fault`, `watchdogEvent` | `Q_PROPERTY`, each an `AeroraReportedValue *`, `CONSTANT` |
| `extendedSurfacePermitted` | `Q_PROPERTY` with notify |
| `configuration(Configuration)`, `reportedValue(itemId)` | `Q_INVOKABLE` |
| Any outbound path | **None exists.** No command interface, no sink, no outbound method |

### `Supervisory`

`AeroraReportedValue` — `QML_ELEMENT` + `QML_UNCREATABLE`: `itemId`, `declaredKind`,
`freshnessClass`, `reportedToken`, `stateClass`, `freshnessEvaluated`, `observed`, `receiveTimeUs`,
`source`, `capability`; `Q_INVOKABLE ageMs()`; `reportChanged`.

`reportedToken` is an **opaque string**, verbatim from the vehicle. The UI must render it raw and
must not parse it, map it to a label, range it, or default it. An empty token with
`stateClass == Unknown` is "never reported"; an empty token with an observation is "reported with
nothing in it". These are different displays.

### `M-Mission`

| Member | Exposure |
| --- | --- |
| `vehicleCurrentIndex`, `resumeCandidate`, `planViewSelection` | `Q_PROPERTY` of `AeroraMissionIndex` (`Q_GADGET`: `sequenceNumber`, `origin`, `Q_INVOKABLE toString()`) |
| `vehicleMission`, `gcsPlan` | `Q_PROPERTY` of `AeroraMissionSnapshot` (`Q_GADGET`: `origin`, `itemCount`) |
| `agreement` | `Q_PROPERTY` of `AeroraMissionAgreement` (`Q_GADGET`: `verdict`, `depth`, `Q_INVOKABLE isVerified()`) |
| `transferActivity`, `lastOperation`, `lastOutcome`, `vehicleMissionIndeterminate` | `Q_PROPERTY` with notify |
| `verify(depth)` | `Q_INVOKABLE` — the one Aerora UI action that exists and is safe: a local comparison with no outbound traffic |
| Any command API | **None.** No upload, download, clear, resume, set-current, start or mode change |
| `AeroraMissionExtension`, registry | **None.** Not QML-visible; no extension registered |

### `M-Security`

| Type | QML exposure |
| --- | --- |
| `AeroraAuthorization.h` — outcomes, identity, roles, policy interfaces | **None.** No `Q_OBJECT`, no `Q_GADGET` |
| `AeroraCommandAuthorizer` | **None.** Not a `QObject` |
| `AeroraCorePlugin::isAuthorizationEnforced()` | **None.** Static C++, not a `Q_PROPERTY` or `Q_INVOKABLE` |

**The UI cannot read authorization state.** This is the hard constraint on the security surface and
it is the correct outcome while **Q-029** (operator role set) and **ADR-014** are open: there is no
authorization state to read, because no policy is installed and no decision is evaluated.

### The missing route: QML cannot reach the Aerora per-vehicle services

This is the single largest structural blocker and it affects payload, power, mission, identity,
capability and profile simultaneously.

```text
QML:  QGroundControl.multiVehicleManager.activeVehicle   ──►  Vehicle
                                                                 │
                                                                 ✗  no Q_PROPERTY, no Q_INVOKABLE
                                                                 ▼
                                                        AeroraVehicleServices
                                                                 │  service("payload") — not Q_INVOKABLE
                                                                 ▼
                                   AeroraPayloadModel / AeroraPowerModel / AeroraMissionObserver
```

`AeroraVehicleServices::attachTo()` parents the services object to the `Vehicle`, and
`forVehicle()` is the documented C++ entry point — but both are plain static C++ functions, and
`service()` / `serviceNames()` are plain members. `Vehicle` carries no property pointing at the
services object, and adding one would be a core edit.

Three candidate resolutions, none of which is Tab 7's to choose alone:

| Option | Owner | Cost |
| --- | --- | --- |
| Make `service()` / `serviceNames()` `Q_INVOKABLE` and add a QML-reachable lookup for `forVehicle()` | **Tab 3** (Vehicle backend) | Smallest; two macro additions plus one accessor |
| An Aerora UI-owned `QML_SINGLETON` facade that wraps `forVehicle(activeVehicle)` | Tab 7, if explicitly assigned the C++ file | Adds a type, but keeps the vehicle backend untouched |
| Per-domain `Q_PROPERTY` on `AeroraVehicleServices` | Tab 3 | Rejected by the Vehicle module's own design note — it would make the vehicle layer depend on every domain |

Recorded as **`Q7-002`**. Until it resolves, **every Aerora data panel is unbindable** and the only
implementable UI work is the presentation layer plus its tests against injected mocks.

## State-class rendering

The rule, inherited unchanged:

> The state class is a property of the **binding**, not a decision made per screen.

A screen author never chooses a treatment. One control reads `stateClass` and decides.

| Class | Treatment | Never |
| --- | --- | --- |
| `Effective` | Primary reading | Shown when none has been reported |
| `Reported` | Primary, with freshness available | — |
| `Requested` | Subordinate, labelled as requested | Promoted to effective |
| `Proposed` | Subordinate, labelled as proposed, visually distinct from vehicle state | Rendered as authoritative |
| `Stale` | Marked, **with its age** | Shown plain; hidden |
| `Unknown` | Explicit unknown marking | Rendered as zero, dash-as-zero, or an empty string that reads as a value |
| `Unavailable` | Element **absent** | Rendered as unknown; shown disabled |

Two additional obligations that follow from the backend's current state and are easy to get wrong:

| Obligation | Reason |
| --- | --- |
| When `freshnessEvaluated` is **false**, a `Reported` or `Effective` value must surface its age | No freshness verdict exists (Q-036). Presenting it plain would imply a check that never ran |
| Age must be re-read, not bound | `ageMs()` is a function; age advances with no signal |

### Planned controls

| Control | Responsibility | Status |
| --- | --- | --- |
| `AeroraValueDisplay` | Renders an `AeroraStateValue` by class | **Implementable** — presentation and tests can be built against a mock before any Aerora Fact exists |
| `AeroraTokenDisplay` | Renders an `AeroraReportedValue`'s opaque token by class | **Implementable** on the same basis |
| `AeroraAgeLabel` | Age, re-read on a timer, with an explicit "no verdict" state | **Implementable** |
| `AeroraActionButton` | Hide versus disable-with-reason, by capability | **`BLOCKED` — F3.1-05.** Capability presence is not reported separately from capability state (Q3-009), so the control cannot decide which of the two an absence is |

`AeroraActionButton` is blocked on data, not on design. The rule it would implement — *hide what
the aircraft cannot do, disable with a stated reason what it cannot do right now* — needs an input
that does not exist. It is not built, and nothing substitutes a guess for it.

### Indicator separation

`IF-UIS-009` enumerates six pairs that must share no visual language. Verification is `DEMO` for
all six — no unit test can assert that two indicators look different — and **Q-040** gates it,
because "shares no visual language" cannot be assessed before a visual language exists.

| Pair | Implementation consequence | Data available? |
| --- | --- | --- |
| Cloud connectivity vs vehicle link | Two distinct indicator types, not one type twice | No cloud surface exists |
| Vehicle-reported active link vs GCS-local observation | Two differently-shaped rows | No — `aeroraLink` is `CONTRACT-ONLY` |
| Payload emergency stop vs payload state | A dedicated stop indicator, not an enumeration member | **Yes** — separate `AeroraReportedValue`s |
| Watchdog event vs payload emergency stop | Two independent rows, never nested | **Yes** — separate items on separate models |
| Authority mode vs flight mode | Two selectors in different regions | No authority-mode surface exists |
| Requested vs effective authority mode | One primary reading plus a subordinate labelled row | No |

Two of six are implementable today, and both are the ones the backend deliberately kept as separate
items.

## UI area map

`R`/`C`/`O`/`X`/`P` per [Classification scheme](#classification-scheme). "Data" states whether any
Aerora value can actually populate the surface now.

| # | Area | Reused (R / C) | Aerora (O / X / P) | Data | Blocker |
| --- | --- | --- | --- | --- | --- |
| 1 | **Vehicle connection / status** | `MultiVehicleManager`, `activeVehicle`, `ConnectionStatusRow.qml`, `VehicleSummaryRow.qml`, `MultiVehicleSelector.qml`, `FlyViewToolBarIndicators.qml` — **R**. `multiVehicleEnabled()`, `showMultiVehicleList()` — **C** | Classification indicator (`SelectedBySeam` vs `CorroboratedAerora`) via `FirmwarePlugin::toolIndicators()` — **O** | Classification reaches `SelectedBySeam` only; no corroboration source exists | `Q7-002` |
| 2 | **Telemetry / state** | `Fact`, `FactMetaData`, `FactGroup`, `FactValueGrid`, `InstrumentValueData`, `SelectableControl`, telemetry CSV — **R**. Aerora instrument widget added via `instrumentQmlFile2` `enumValues` — **C** | `AeroraValueDisplay`, `AeroraAgeLabel`, Aerora instrument widget — **O** | **None.** Six fact groups `CONTRACT-ONLY`; zero Aerora Facts; zero `AeroraStateValue` instances | Aerora dialect; Q-036; Q4-002 |
| 3 | **Mission** | `PlanView`, `PlanMasterController`, `MissionController`, `MissionManager`, `MissionCommandTree`, `PlanCreator`, complex-item infrastructure — **R**. `missionWaypointsOnly()`, `showMissionStatus()`, `showMissionAbsoluteAltitude()` — **C** | Mission-agreement / transfer / indeterminate panel bound to `AeroraMissionObserver`; `verify(depth)` as the one safe action — **O**. Complex items via `complexMissionItemNames()` / `createComplexMissionItem()` / `planCreators()` — **P** | **Partial.** Observer populates once `attachTo()` has a caller and `reportGcsPlan()` has an owner | `Q7-002`; conflict 2 (no `attachTo()` caller); Q-021 / Q-023 |
| 4 | **Payload** | `PhotoVideoControl.qml`, `GimbalIndicator.qml`, camera stack — **R**, and unrelated to the Aerora payload | Read-only payload panel: identity, state, health, safety state, **emergency stop as its own indicator** — **O** | `controlsPermitted` is **false** (no capability report). `IF-PAY-002`: present **no payload control at all** | Q-023; Q3-009; `Q7-002` |
| 5 | **Power** | `BatteryIndicator.qml`, `BatteryFactGroupListModel` — **R, unchanged** | Tether / battery configuration, effective vs reported state, fault, watchdog event — all read-only, gated on `extendedSurfacePermitted` — **O** | `extendedSurfacePermitted` is **false**. Standard QGC battery model only, which is correct, not degraded | Q-024; Q3-007; `Q7-002` |
| 6 | **Autonomy / supervision** | `GuidedActionsController` infrastructure — **R** | The `Proposed` rendering path in `AeroraValueDisplay` — **O, placeholder** | **None.** `aeroraAutonomy` `CONTRACT-ONLY`; no companion presence, health or proposal reported | Aerora dialect; Q-008 |
| 7 | **Security / operator state** | `showAdvancedUIMessage()`, advanced-UI toggle — **C**, as **disclosure only** | **Architecture placeholder only** — no role surface, no permission surface, no operator identity surface | **None.** Zero QML exposure in `aerora/src/Security/`; `isAuthorizationEnforced()` unreadable from QML | **Q-029**, ADR-014, and a missing QML-readable enforcement property owned by Tab 2 |
| 8 | **Link status** | `TelemetryRSSIIndicator.qml`, `RCRSSIIndicator.qml`, `LinkManager`, link settings pages — **R** | Two differently-shaped rows separating vehicle-reported active link (`Effective`) from GCS-local observation (`Proposed`) — **O** | **None.** `aeroraLink` `CONTRACT-ONLY` | Aerora dialect; `IF-MAV-022` |
| 9 | **Video / camera** | `VideoManager`, `FlyViewVideo.qml`, `PhotoVideoControl.qml`, `PipView`, `createVideoReceiver` / `createVideoSink` / `releaseVideoSink` — **R / C** | **None.** No Aerora video or camera contract exists in any interface artefact | n/a | None. **Reuse entirely** |
| 10 | **Alarms / warnings** | `VehicleWarnings.qml`, `showCriticalVehicleMessage()`, `QGCPopupDialog`, `alertBackground` / `alertBorder` / `alertText` palette roles — **R** | Reported payload and power faults surfaced as display rows — **O** | Payload `health` / `safetyState` / `emergencyStop` and power `fault` / `watchdogEvent` exist as items | `Q7-002`. **No Aerora condition reaches an action** — `18-error-degradation-model.md` records "none" in the authority column for all twelve |
| 11 | **Vehicle identity** | Vehicle name, sysid display — **R** | Fleet identity presented **separately** from MAVLink sysid, with `fleetIdentityKnown` false shown as unknown and never falling back to sysid — **O** | `fleetIdentityKnown` is always false; no evidence source reports one | Fleet identity format `OPEN`; `Q7-002` |
| 12 | **Product profile** | `SelectableControl` and `customMapItems` as the data-driven precedents — **R** | Profile-id-keyed module slot. `AeroraVehicleServices::productProfileId` is the **only** QML-readable profile hook — **O** | Empty string: no profile is registered | `Q7-002`; no profile implemented |
| 13 | **Painting workflow** | — | **P, slot only.** No painting content is implementable | **None.** No payload state enum member, pump, valve, nozzle, pressure, flow, tank level, spray rate or colour exists anywhere; no painting complex item; no dialect | Q-021, Q-023, `D-007` |
| 14 | **Firefighting / agriculture / inspection** | — | **P, same slot** | None | Same |

### Area 9 is the honest outlier

Video is the one area where the correct Aerora answer is **no Aerora UI at all**. Recording it as
reuse rather than inventing an Aerora video surface keeps the overlay honest about where it adds
nothing.

## Product profile consumption

The Vehicle workstream already defines the architecture: `AeroraProductProfile` is an interface,
`AeroraProductProfileRegistry` resolves exactly one profile from the reported capability inventory,
and resolution returns `nullptr` both when none matches **and when more than one matches**. There
is deliberately no enumeration of product families in the header.

The UI must mirror that discipline:

| Rule | Why |
| --- | --- |
| The UI reads `productProfileId` as an **opaque string** | A `switch` on profile id reintroduces the conditional the profile architecture exists to remove |
| Profile UI loads through a `profileId → QML URL` registry consumed by a `Loader` | Same shape as `customMapItems` and `SelectableControl`. Adding a product is a new file plus a registry line |
| No core Aerora UI file names a product family | The second-product test: adding firefighting must touch no painting file and no shared file |
| An empty or unresolved `productProfileId` shows **no product surface**, not a default one | An unresolved profile is a build-configuration defect or an un-reported aircraft; a default would hide it |

```text
AeroraVehicleCapabilities  ──►  Registry::resolve()  ──►  profileId (opaque)
                                                              │
                                                              ▼
                                            profile UI registry  ──►  Loader  ──►  product QML module
                                                              │
                                              empty / unresolved  ──►  nothing
```

Painting occupies one slot in this registry. It is not the core and acquires no privileged path.

## Deliberately disabled surfaces

Each row is a UI capability that is **not** built, with the structural reason it cannot be built by
accident.

| Surface | Why disabled | Structural enforcement |
| --- | --- | --- |
| **Payload actuator controls** | `IF-PAY-015` is `CONFIRMED` with direction "None — prohibited"; the actuator interface is inside the vehicle | `AeroraPayloadModel::submitRequest()` is **not `Q_INVOKABLE`** — QML cannot call it. No implementation of `AeroraPayloadCommandSink` exists. `commandPathInstalled` is false, so `submitRequest` would return `RefusedNoCommandPath`. `kGroundStationActuatorPathImplemented == false` |
| **Any payload control at all** | `IF-PAY-002`: with no payload capability reported, no payload control is presented | `controlsPermitted` is false |
| **Power source switching, failover, thresholds, emergency power action** | Power arbitration and transition behaviour are `OPEN`; baseline 49 requires separate engineering and verification | `AeroraPowerModel` has **no outbound path of any kind** — no sink, no command method. `kStationInitiatedPowerActionImplemented`, `kPowerArbitrationImplemented`, `kAutomaticSourceTransitionImplemented`, `kRemainingEnergyEstimationImplemented` all false; `kDefinedThresholdCount == 0` |
| **Aerora guided actions** | Every candidate action is a command with no path | `CustomGuidedActionsController.qml` is not overridden; both stubs keep returning `false` |
| **Aerora tool-strip entries** | Same | `FlyViewToolStripActionList.qml` is not replaced; tier-3 count stays 0 |
| **Mission commanding from Aerora UI** | Mission authority stays with `PlanMasterController` / `MissionController` / `MissionManager` | `AeroraMissionObserver` has **no command API**. The only `Q_INVOKABLE` is `verify(depth)`, a local comparison |
| **Role / permission / operator-identity display** | Q-029 open; no policy installed; no decision evaluated | Zero QML exposure in `aerora/src/Security/` |
| **Advanced-UI toggle as a permission gate** | `IF-UIS-017`: it is disclosure, not authorisation, and is one click deep | Documented prohibition. It may reduce clutter; it never gates an operation requiring an authorised operator |
| **Fault-driven automatic action** | `18-error-degradation-model.md` records "none" in the authority column for all twelve aircraft-reported conditions | No Aerora UI path from a reported condition to a command |
| **Local fault acknowledgement / clearing** | Clearing a fault the vehicle still holds misreports the aircraft | `AeroraPowerModel` has no clear, acknowledge or reset; `clearReports()` is connection teardown only |
| **`includedResumeCommands` modification** | A core edit on a seamless path | `aerora/src/Mission/AeroraMissionResumeBoundary` is a read-only mirror. The UI may **display** which commands replay across a resume; it changes nothing |

## Planned Aerora QML module layout

Not created. Recorded so the shape is reviewable before any file exists, and because `Q7-001` may
change the registration mechanics.

| Module URI | Content | Depends on |
| --- | --- | --- |
| `Aerora.Controls` | `AeroraValueDisplay`, `AeroraTokenDisplay`, `AeroraAgeLabel`, state-class visual vocabulary | `QGroundControl.Controls`, `ScreenTools`, `QGCPalette` |
| `Aerora.FlyView` | The `FlyViewCustomLayer.qml` replacement content and its panels | `Aerora.Controls` |
| `Aerora.Toolbar` | Aerora indicators listed by `toolBarIndicators()` / `toolIndicators()` | `Aerora.Controls` |
| `Aerora.Plan` | Product complex-item visuals and editors | `Aerora.Controls`, `QGroundControl.PlanView` |
| `Aerora.Profiles.<Product>` | One module per product family, loaded by profile id | `Aerora.Controls` |

Division of labour, which the reference overlay demonstrates and which is easy to invert:

| Artefact | Goes into |
| --- | --- |
| Self-registering C++ | The **application target**, via `CUSTOM_SOURCES` |
| QML modules | Aerora **libraries**, via `qt_add_qml_module`, exported through `CUSTOM_LIBRARIES` |
| Import paths | Added in `createQmlApplicationEngine()` override, as `CustomPlugin.cc:243-244` does |

A module URI is a namespace, so an Aerora control may share a name with an upstream control without
colliding. `Aerora.Controls` must not be merged into `QGroundControl.Controls`.

### Constraint on moving QML singletons

`set_source_files_properties(... QT_QML_SINGLETON_TYPE TRUE)` has **directory scope**, and
cross-directory propagation fails on Windows — documented at `src/QmlControls/CMakeLists.txt:189-194`
for `ScreenTools.qml`. Any Aerora QML singleton must be declared in the same `CMakeLists.txt` as
its `qt_add_qml_module()` call.

## Test strategy

No test is written and none is executed. What is proposed, and what each kind can and cannot prove:

| Kind | Mechanism | Proves | Cannot prove |
| --- | --- | --- | --- |
| **Static / QML validation** | `qmllint` via `python3 ./tools/analyze.py --tool qmllint` (pre-commit hook `.pre-commit-config.yaml:228`, `files: \.qml$`); stricter `just lint-qml-build` promotes `ImportFailure` and missing-property to errors | Syntax, unqualified access, anchor/layout conflicts, stale property reads, import resolution | Any behaviour |
| **Model/view contract tests** | `UnitTest`-derived C++ with `UT_REGISTER_TEST(…, TestLabel::Unit)`, `MultiSignalSpy`, and an injected `ManualScheduler` | That each property the UI binds exists, notifies, and carries the documented value; that `declaredKind` has no setter; that `submitRequest` is absent from the meta-object's invokable set | Rendering |
| **Component / state-class tests** | Qt Quick Test `tst_*.qml` run by `QGCQmlQuickTests` (`test/UnitTestFramework/QmlTesting/`), out of process with `-platform offscreen` | That a given `stateClass` produces the required treatment; that `Unknown` renders as unknown and not zero; that `Unavailable` is absent; that a `Proposed` item is labelled and subordinate | That two indicators are visually distinguishable (`DEMO`, Q-040) |
| **Visibility / integration tests** | `test/QmlUITests/` harness (`QmlUITestBase`: `startUI()`, `findVisibleItem()`, `verifyVisibility()`, `verifyEnabled()`, `runWithMockLink()`) | That an Aerora panel appears, survives a null vehicle, and gates on `controlsPermitted` / `extendedSurfacePermitted` | Pixel appearance |
| **Disabled-state tests** | Both harnesses plus assertions on the backend prohibition constants | That no actuator control is reachable; that `commandPathInstalled` is false; that `kDefinedThresholdCount == 0` | — |
| **Resolution self-check** | `IF-UIS-015` startup assertions: each overridden stock URL intercepts to the overlay URL; no unintended URL is intercepted; each Aerora module URI imports; the selected firmware plugin is Aerora's | That an override silently did not take effect, or silently took effect where it should not | The `:/json` C++-read case — **Q4-001**, partial |

### Two harness constraints that shape where Aerora UI tests can live

| Constraint | Consequence |
| --- | --- |
| `QGCQmlQuickTests` hard-codes `QUICK_TEST_SOURCE_DIR` to `test/UnitTestFramework/QmlTesting/tests` (`test/UnitTestFramework/QmlTesting/CMakeLists.txt`) | An Aerora `tst_*.qml` cannot be discovered from `aerora/test/`. Either it goes in the core test directory — **outside this workstream's ownership** — or a second Qt Quick Test executable is registered for the overlay. Recorded as **`Q7-004`** |
| The Aerora manifest convention forbids targets and `add_test()` in a `.cmake` manifest; registration happens in `aerora/CMakeLists.txt`, which Tab 1 owns | A second Quick Test executable is a Tab 1 change. `add_qgc_test(AeroraUiTest LABELS Unit)` for the C++ harness follows the existing precedent and needs only a manifest plus one registration line |

`test/QmlUITests/ToolbarIndicatorUITest.{cc,h}` is the closest precedent for an indicator
visibility test and should be followed rather than replaced with a new harness.

### Rules every Aerora UI test must follow

| Rule | Source |
| --- | --- |
| No `QTest::ignoreMessage` — use `expectLogMessage()` + `verifyExpectedLogMessage()` | `check-no-qtest-ignore-message` |
| No fixed-delay `QTest::qWait(n)` — use `QVERIFY_TRUE_WAIT`, `QVERIFY_SIGNAL_WAIT` or `waitForCondition` | `check-no-fixed-qwait` |
| Timeouts come from `TestTimeout::shortMs()` / `mediumMs()` / `longMs()`, not literals | `test/README.md` |
| Disconnect the MockLink **before** `closeUIWindow()` | `QmlUITestBase`. QML then binds against a null vehicle, which exposes null-dereference bugs rather than avoiding them. Every Aerora panel must survive it — this is the QML half of the multi-vehicle golden rule |
| Strict log mode: any unexpected log message fails the test | `UnitTest`. An Aerora panel that warns on a null vehicle will fail its own test, which is the intended pressure |

## Blockers

| ID | Blocker | Owner | Blocks |
| --- | --- | --- | --- |
| ~~`Q7-002`~~ | **`CLOSED`.** `AeroraVehicleUiContext` reaches the services through public C++ and exposes typed read-only properties. No vehicle-layer change needed; the Tab 3 handoff is withdrawn | — | — |
| ~~`Q7-001`~~ | **`CLOSED`.** The ordering is the same one `src/QmlControls/CMakeLists.txt:6-75` uses for 38 upstream types including the `QGroundControl` singleton. Aerora types register under URI `QGC`, reached via `IMPORTS QGC` | — | — |
| ~~`Q7-003`~~ | **`CLOSED`.** Both hooks are public virtuals and the allow-listed interceptor is installed on the engine — U1, `84bbb3a6e`. The allow-list is still empty, so nothing is overridden yet; the seam works | — | — |
| ~~HO-TAB7-03~~ | **`CLOSED`.** The `Aerora.Controls` module target exists with `IMPORTS QGC` and the resource alias — U2, `a73699796`. Six QML files were registered at that commit, not four; the manifest now lists eight (`aerora/src/UI/Ui.cmake`) | — | — |
| ~~HO-TAB7-01~~ | **`CLOSED`.** `AeroraStateValue::capability` is read-only in QML — U3, `a7b317f89`. `setCapability()` remains public C++ for the capability model | — | — |
| ~~HO-TAB7-05~~ | **`CLOSED`.** `AeroraCorePlugin::analyzePages()` overrides the seam — `39406ff5f` — and registers `SCR-SHL-01` and `SCR-LIV-06` at `qrc:/qml/Aerora/Controls/`. `toolBarIndicators()` is overridden too, for the enforcement strip — `b77bc4e8e`, HO-TAB7-06. That the menu entries *appear* still needs a launch; see `D-006` | — | — |
| `D-006` | **Narrowed, not closed.** CI builds and tests the overlay (Windows leg, Qt 6.11.1 + MSVC 2022), so compile and test claims are no longer blocked. The **authoring host** still has no C++ compiler and no Qt — `cl`, `g++`, `clang++`, `qmake`, `qmllint`, `qmlformat` and `just` are absent and `C:\Qt` does not exist — while `cmake`, `ninja`, `ctest` and `pre-commit` are available from the repository's own `tools/.venv/Scripts` rather than the default `PATH`. As first written this row said all of them were absent; that was true of the authoring host at the time and `tools/.venv` has since been populated | User / Tab 8 | Local build, lint and test; `qmllint` anywhere; every runtime and launch claim |
| `Q-029` | Operator role set | Product + operations | Area 7 beyond placeholders |
| ADR-014 | Outbound authorization scope | Architecture | Any claim that an operator action is authorised |
| Aerora dialect | No fact name, unit, range or enumeration member for any Aerora item | Firmware + product | Areas 2, 4, 5, 6, 8, 13 — **no Aerora value can populate any panel** |
| `Q-036` | Freshness threshold values | Product | `STALE` is unreachable; UI must show age with no verdict |
| `Q3-009` / F3.1-05 | Capability presence reported separately from capability state | Firmware | `UNAVAILABLE` is unreachable; `AeroraActionButton` stays blocked |
| `Q4-002` / R4-001 | Per-vehicle fact-group attachment | Architecture | Aerora Facts reaching a vehicle, the value selector and the CSV record |
| Conflict 2 | `AeroraMissionBridge::attachTo()` has no caller | Tab 3 | Area 3 data |
| `Q-021` / `Q-023` | Aerora complex item and mission command set | Product | Areas 13, 14 |
| `Q-040` | Aerora visual design language | Product + design | All six `IF-UIS-009` indicator pairs (`DEMO` verification) |
| `Q-038` | Tier-3 override owner | Process | Only if a tier-3 override becomes necessary. Currently none is |
| `Q4-001` | Precedence of two resources registered at one `:/json` path | Tab 8 | The `:/json` half of the startup self-check |
| `Q7-004` | `QGCQmlQuickTests` discovers `tst_*.qml` only from `test/UnitTestFramework/QmlTesting/tests`, which this workstream does not own | Tab 1 + Tab 7 | Qt Quick Test coverage of Aerora QML components. The C++ harness is unaffected |
| `D-007` | §27 product authorization | External | Any product feature, including every painting surface |

### The shape of the blocker set

After the foundation pass, one blocker dominates and it is not Tab 7's.

| Blocker | Kind | Reach |
| --- | --- | --- |
| Absent Aerora dialect | Neither small nor Tab 7's | Areas 2, 4, 5, 6, 8, 13 have **no value to display**. No fact name, unit, range or enumeration member exists for any Aerora item |
| ~~HO-TAB7-02 and HO-TAB7-03~~ | **Both applied.** The interceptor is installed on the engine (`84bbb3a6e`) and the `Aerora.Controls` module target exists (`a73699796`) | Closed. Recorded here because the row below was written when they were the second-largest blocker |
| `D-006` | Environment, **narrowed** | Local build, lint and test; `qmllint` anywhere; every runtime and launch claim. No longer blocks compile and test claims, which CI now supplies |

Stated plainly: **Aerora UI can be built and tested against the state model before any Aerora
telemetry exists, and that is the correct order.** The state-class contract is what is expensive to
retrofit; the panels are not.

## Verification status

| Check | Status | Evidence |
| --- | --- | --- |
| Compiles | **`VERIFIED`** (CI only) | Windows CI run 39146146397, commit `915dd09ae`, 2026-10-18: Qt 6.11.1 + MSVC 2022 Debug, overlay active (the run's own `Assert the Aerora overlay is active` step passed), `aerora/test/UI/AeroraUiFoundationTest.cc` among the compiled translation units. Not reproducible on the authoring host — see `D-006` |
| Tests executed | **`VERIFIED`** (CI only) | Same run: 15/15 reported Aerora unit tests passed, 3/3 reported Aerora integration tests passed, 0 failures and 0 skips. Includes `AeroraUiFoundationTest` and `AeroraUiSafetyTest`. The leg is scoped to tests matching `^Aerora`; it is not the full upstream suite |
| `Q_GADGET` access from a QML engine | **`VERIFIED`** (CI only) | Same run, via `AeroraUiFoundationTest`. `_missionGadgetValuesAreReadableFromQml` reads `AeroraMissionIndex` and `AeroraMissionSnapshot` properties, and `_missionAgreementVerdictIsReadableFromQml` calls `AeroraMissionAgreement::isVerified()`, both through a live `QQmlEngine`. **Scope:** the observer is reached as a `QQmlContext` **context property**, so this evidences gadget property and invokable reachability only — not that any Aerora type resolves by QML type name under URI `QGC` (`Q7-001`), and not anything about the pages |
| `qmllint` on Aerora QML | **`BLOCKED`** | `qmllint` ships with Qt and is absent from the authoring host. No CI leg runs it on an `aerora/**` push either: `aerora-windows.yml` has no `qmllint` step, and the `pre-commit` workflow's push trigger is restricted to `master` / `Stable*`. A pull request would run it |
| `just lint` / full `pre-commit` sweep | **`BLOCKED`** | `just` is absent. `pre-commit` is available from `tools/.venv/Scripts`, but the hooks that matter for this workstream shell out to `qmllint`, `clang-tidy` and `clazy`, none of which is installed, and several invoke `python3`, which this host does not provide |
| Runtime / override resolution | **`BLOCKED`** | Requires a launch, which has not happened. Nothing is evidenced about whether any Aerora screen renders, whether its menu entry appears, how it presents missing data, or whether it stays read-only in a live scene. No test loads a `.qml` file: `AeroraUiFoundationTest` contains no `QQmlComponent`, `setSource` or `loadFromModule` call, and `Q7-004` still leaves the overlay without a Qt Quick Test target |
| Upstream seam inventory | **Source-verified** | `QGCCorePlugin.h`, `QGCOptions.h`, `FlyViewCustomLayer.qml`, `CustomGuidedActionsController.qml`, `FlyViewAdditionalCustomActionsList.qml`, `CustomMapItems.qml`, `FlyViewToolStripActionList.qml`, `FlyViewToolBarIndicators.qml`, `FirmwarePlugin.{h,cc}`, `Vehicle.{h,cc}`, `custom-example/CMakeLists.txt`, `custom-example/src/CustomPlugin.cc`, root and `src/CMakeLists.txt` |
| Aerora QML-facing surface | **Source-verified** | Every `aerora/src/**/*.h` grepped for `Q_GADGET`, `QML_ELEMENT`, `QML_SINGLETON`, `QML_UNCREATABLE`, `Q_INVOKABLE`, `Q_DECLARE_METATYPE` |
| Aerora QML file count | **Verified eight** | Eight `.qml` files exist under `aerora/`, and all eight are listed in `AERORA_UI_QML_FILES` (`aerora/src/UI/Ui.cmake`). As first written this row read "verified zero", which was true before the UI foundation pass |
| Aerora overrides of a `QGCCorePlugin` seam | **Verified four** | `AeroraCorePlugin` overrides `createQmlApplicationEngine()` and `destroyQmlApplicationEngine()` (`84bbb3a6e`), `analyzePages()` (`39406ff5f`) and `toolBarIndicators()` (`b77bc4e8e`). `toolIndicators`, `customMapItems`, `paletteOverride`, `QGCOptions`, `registerCustomSettings`, `complexMissionItemNames` and `planCreators` remain un-overridden. As first written this row read "verified zero", which was true before the UI foundation pass |
| Tier-3 populated-file overrides | **Verified zero** | The interceptor's allow-list is empty, so no upstream populated file is replaced. `AeroraUiFoundationTest::_noOverrideIsDeclared` holds this at zero |
| Test harness surface | **Source-verified** | `test/README.md`, `test/UnitTestFramework/QmlTesting/CMakeLists.txt`, `tests/tst_Example.qml`, `test/QmlUITests/`, `.pre-commit-config.yaml`, `justfile` |
| Build tooling on the authoring host | **Verified, re-checked 2026-10-18** | Absent from the default `PATH` and from the host entirely: `cl`, `g++`, `clang++` (no MSVC install, no `vswhere`), Qt (`qmake` absent, `C:\Qt` does not exist), `qmllint`, `qmlformat`, `just`. Present in the repository's `tools/.venv/Scripts`, not on the default `PATH`: `cmake` 3.31.10, `ninja` 1.13.0, `ctest` 3.31.10, `pre-commit` 4.6.0, `gcovr`. `clang-format` 23.1.0 exists only inside the `pre-commit` hook environment cache. As first written this row listed `cmake`, `ninja` and `pre-commit` as absent, which was true of the host at the time |
| `includedResumeCommands` untouched | **Verified** | Sole definition and call site remain `src/MissionManager/MissionManager.cc:110-134`; no Aerora file modifies it |

**Status as of 2026-10-26, baseline commit `eafbb77a1`** (Aerora Linux run 40356071361, Aerora
Windows run 40356071332). The overlay **compiles** on both platforms and its tests **pass** — 16/16
unit on each, 9/9 integration on Linux, 6/6 on Windows.

The "no Aerora UI renders" caveat this paragraph used to carry is **withdrawn**, and the three
reasons it gave are each now false. Tests do load `.qml` files; a booted application window does
exist; and the screens are reached by clicking, not by registry inspection.
`AeroraSceneAttachmentUITest`, `AeroraVehicleConfigUITest`, `AeroraFlyViewOverlayUITest` and
`AeroraMissionProgressUITest` navigate to the Aerora screens through the Analyze list, the Configure
sidebar and the Fly View overlay seam, and `verifyPanelContentVisible()` requires a laid-out item
with a real size rather than an instantiated object. What is still **not** claimed is pixel
appearance, and the Aerora visual language remains unresolved (`Q-040`).

### `qmllint` has now been run on Aerora QML, and what it says needs reading carefully

First run: 40363963190, 15 files, `--qml-build` against the generated types. It is **not** clean, and
the previous rounds of "qmllint: success" said nothing about the overlay at all —
`tools/analyze.py` collected whole-tree files from `repo_root / "src"` only, so `aerora/src` was
never opened. Fixed by `FileCollector.QML_ROOTS`; a regression test covers it. The C++ roots are
deliberately still `src`-only, because widening them changes what clazy, clang-tidy and cppcheck
report and that cannot be assessed in the same run.

The findings fall into two groups and only one of them is a defect:

| Finding | Count | Reading |
| --- | --- | --- |
| `X was not found. Did you add all imports and dependencies?` for `AeroraSelfReportRow`, `AeroraStateText`, `AeroraMissionProgressPanel`, plus `[unqualified]` on `AeroraTelemetryState` / `AeroraSecurity` / `AeroraMissionObserver` | most of the output | **Artifact of linting the source tree.** `Aerora.Controls` members live in three sibling source directories (`Controls/`, `Indicators/`, `Screens/`) and the build flattens them into one module directory via `qgc_set_qt_resource_alias()`. A file in `Screens/` has no sibling `AeroraSelfReportRow.qml`, so the implicit same-directory import does not contain it — a layout the running application never sees. The same cause explains the singleton warnings |
| `colorGroupEnabled: enabled` reads `enabled` unqualified from the parent element | 8 controls | **Real, and unfixed.** It is also upstream QGC's idiom, repeated across `src/`, so the overlay is no stricter than the code it mirrors here. Left alone deliberately rather than silently: polishing this before the module resolves would be tidying output that cannot yet be trusted |

So the honest status is *`qmllint` now covers the overlay and reports findings that are mostly its
own view of the source layout.* Making the run meaningful means pointing it at the module as built
rather than at the source tree, which is the next step and is not taken here.

Screens exist, are registered with `analyzePages()` and with
`AeroraAutoPilotPlugin::vehicleComponents()`, and are reached by test-driven clicks. Every status
gated on `Q-029`, `ADR-014`, `D-007`, the absent Aerora dialect, `Q-036`, `Q3-009`/F3.1-05,
`Q4-002`, `Q-021`/`Q-023`, `Q-040` or `Q7-004` is unchanged by this run.

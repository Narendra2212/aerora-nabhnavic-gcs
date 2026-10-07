# UI Foundation Closure

Owner: **Tab 7 (`M-Ui`)**. Closure records for Q7-001 through Q7-004, the UI safety audit, and the
exact handoffs other workstreams must apply.

**Build status: `IMPLEMENTED-UNCOMPILED`.** No compiler, CMake, Qt, `qmllint`, `qmlformat` or
`pre-commit` exists in this environment (`D-006`). Nothing here has been built, linted, loaded or
executed. See [Verification status](#verification-status).

## Contents

- [Q7-001 — QML type registration](#q7-001--qml-type-registration)
- [Q7-003 — QML override interception](#q7-003--qml-override-interception)
- [Q7-002 — vehicle service exposure](#q7-002--vehicle-service-exposure)
- [Q7-004 — QML test discovery](#q7-004--qml-test-discovery)
- [UI safety audit](#ui-safety-audit)
- [Tier discipline](#tier-discipline)
- [Handoffs](#handoffs)
- [Verification status](#verification-status)

## Q7-001 — QML type registration

**`CLOSED`.** The ordering is correct, and it is correct for a reason stronger than analysis: it is
the load-bearing path QGroundControl's own QML types already use.

### The question

Aerora's `QML_ELEMENT` types reach the application target through `CUSTOM_SOURCES`
(`src/CMakeLists.txt:208`), which executes inside `add_subdirectory(src)` at root
`CMakeLists.txt:442` — **after** `qt_add_qml_module(${CMAKE_PROJECT_NAME} URI QGC ...)` at root
`:422`, and from a different directory scope. Does a type added that late still register?

### The answer, from in-tree precedent

Yes. `src/QmlControls/CMakeLists.txt:6-75` adds 38 C++ source pairs to `${CMAKE_PROJECT_NAME}` with
`target_sources`, from a subdirectory reached by the same `add_subdirectory(src)` at `:442`. Among
them:

| Type | Macro | File |
| --- | --- | --- |
| `QGroundControlQmlGlobal` | `QML_NAMED_ELEMENT(QGroundControl)` + `QML_SINGLETON` | `src/QmlControls/QGroundControlQmlGlobal.h:46-47` |
| `ScreenToolsController` | `QML_ELEMENT` + `QML_SINGLETON` | `src/QmlControls/ScreenToolsController.h:12-13` |
| `QGCPalette` | `QML_ELEMENT` | `src/QmlControls/QGCPalette.h` |
| `FactValueGrid`, `InstrumentValueData`, `QmlObjectListModel`, `ToolStripActionList`, … | `QML_ELEMENT` | same directory |

`QGroundControlQmlGlobal` is the `QGroundControl` singleton every QML file in the application uses.
If types added to the application target after `qt_add_qml_module` failed to register, QGC would not
start. **The ordering question is answered by the application booting.**

### Where Aerora types land, and how QML reaches them

They register into the application target's QML module, whose URI is **`QGC`** — not an Aerora URI.
That is the same module QGC's own app-target types land in, and it is reached transitively:

```text
QML file:  import QGroundControl
               │
               ▼  src/CMakeLists.txt:15-26 — qt_add_qml_module(QGroundControlModule
               │       URI QGroundControl ... IMPORTS QGC)
               ▼
           URI QGC  =  the application target's module  (root CMakeLists.txt:422-428)
               │
               ▼
           AeroraTelemetryState, AeroraUi, AeroraStateValue, AeroraReportedValue,
           AeroraPayloadModel, AeroraPowerModel, AeroraMissionObserver, …
```

Two consequences recorded rather than assumed:

| Consequence | Detail |
| --- | --- |
| `AERORA-UI-REQ-010` is satisfied by the **QML** module, not the C++ types | Aerora QML *files* go into a named module (`Aerora.Controls`). Aerora C++ types share `QGC` with upstream's, which is upstream's own convention and needs no change |
| An Aerora QML module must declare `IMPORTS QGC` | Otherwise `AeroraTelemetryState` does not resolve inside it. This mirrors `QGroundControlModule` exactly, and is why `Ui.cmake` specifies it in the module block |

### What is still untested

That the types *resolve at runtime* needs a build. The ordering question — the actual content of
Q7-001 — is closed by precedent, not by a run.

## Q7-003 — QML override interception

**`IMPLEMENTED-UNCOMPILED`.** The seam is established from source, it requires **no QGroundControl
core edit**, and the implementation lives entirely in the overlay. One handoff remains to install
it (HO-TAB7-02).

### The seam

| Step | Source | Fact |
| --- | --- | --- |
| 1 | `src/QGCApplication.cc:268` | `QGCApplication::_initQmlRootWindow()` is the boot path |
| 2 | `src/QGCApplication.cc:275` | The engine is obtained **through the plugin**: `QGCCorePlugin::instance()->createQmlApplicationEngine(this)` |
| 3 | `src/API/QGCCorePlugin.h:117` | `createQmlApplicationEngine` is **`virtual`** and in the `public:` section (which opens at `:57`, `signals:` at `:263`) |
| 4 | `src/API/QGCCorePlugin.cc:300-307` | Base implementation: `new QQmlApplicationEngine(parent)`, `addImportPath("qrc:/qml")`, one context property |
| 5 | `src/API/QGCCorePlugin.h:122` | `destroyQmlApplicationEngine` is the symmetric public virtual, documented as the interceptor release point |
| 6 | `src/QGCApplication.cc:705-710` | Teardown routes through the destroy hook, with a comment stating parent-based teardown would bypass it |

`AeroraCorePlugin` already derives from `QGCCorePlugin`. Overriding two public virtuals is an
overlay change.

### Compatibility with the current source

Checked against this checkout rather than against the Phase 3 description. The reference overlay's
mechanism (`custom-example/src/CustomPlugin.cc:240-296`) is intact and the hooks it uses are
unchanged. One correction to the Phase 2 model, already recorded in the architecture map: there is
no alias collision and no undetermined precedence — the overlay copy lives under a different
resource prefix and resolution is a single existence check.

### Where Aerora deliberately differs from the reference overlay

| | `custom-example` | Aerora |
| --- | --- | --- |
| Interception key | `QFile::exists(":/Custom" + path)` for **any** `qrc` path | An **explicit compile-time allow-list**, then existence |
| Set of overridden files | Implicit and unbounded | Declared, enumerable, and currently **empty** |
| A resource mirrored by accident | Silently wins | Never intercepted |
| `IF-UIS-015` "no unintended interception" assertion | Not writable — no declaration of intent to compare against | Writable, and is `AeroraQmlOverrides::overridePaths()` |

The security constraint on this pass — *the interceptor must only affect Aerora-owned UI resources
and must not intercept arbitrary URLs* — is satisfied structurally. A path outside the allow-list is
returned identically, and that is asserted by
`AeroraUiFoundationTest::_undeclaredResourceUrlIsNeverIntercepted` over six URL shapes including
`MainWindow.qml`, a core `:/res` icon and a `:/json` resource.

### Blast radius

The interceptor rewrites QML resource URLs and nothing else. It holds no vehicle, reads no Fact,
sends no message, and has no reference to MAVLink, `MissionManager`, `ParameterManager`, the
authorization gate or any command path. A defect in it can lose Aerora UI; it cannot alter aircraft
behaviour.

### Lifecycle

```text
createQmlApplicationEngine()   →  AeroraQmlOverrides::installOn(engine)
                                    new interceptor, engine->addUrlInterceptor()
destroyQmlApplicationEngine()  →  AeroraQmlOverrides::releaseFrom(engine)
                                    removeUrlInterceptor(), delete, null out
                                  then the base deletes the engine
```

Install is idempotent and refuses a second interceptor with a warning. Release is a no-op for an
engine it did not install on, so an out-of-order teardown cannot delete another engine's
interceptor. State is file-local in `AeroraQmlOverrides.cc` and main-thread only, which is where
`QQmlApplicationEngine` is created and destroyed.

### Rollback

Remove the two overrides from `AeroraCorePlugin` (HO-TAB7-02) and the behaviour is stock
QGroundControl: every URL resolves upstream. Removing `AERORA_UI_SOURCES` from the aggregation
additionally removes the code. Neither leaves residue, because nothing else in the overlay calls
`AeroraQmlOverrides`.

### Expected upstream drift

| Risk | Likelihood | Detection |
| --- | --- | --- |
| `createQmlApplicationEngine` / `destroyQmlApplicationEngine` signature change | Low — both are documented custom-build hooks | **Compile error.** The safe direction |
| Semantic change, e.g. the engine being created elsewhere | Low | Not a compile error. Caught by the `IF-UIS-015` self-check reporting `isInstalled() == false` |
| A stock QML path Aerora declared being renamed upstream | Moderate once the list is non-empty | `verify()` reports it as a missing mirrored resource |
| The `/Aerora` prefix colliding with an upstream prefix | None | No upstream resource uses it; `:/json` is the genuinely shared prefix and is Q4-001, not this |

While the allow-list is empty, the third and fourth rows cannot fire.

## Q7-002 — vehicle service exposure

**`CLOSED`, and the handoff is withdrawn.** The smallest safe route needs **no change outside
M-Ui**, because everything required from the vehicle layer is already public C++.

### What the first pass got wrong

The architecture map filed `Q7-002` as a Tab 3 handoff: make `service()` / `serviceNames()` /
`forVehicle()` `Q_INVOKABLE`. That was the wrong shape. `Q_INVOKABLE` on a `static` member of a
`QML_UNCREATABLE` type is not usefully callable from QML, and exposing a string-keyed `QObject *`
lookup is precisely what this pass was told not to do.

### The route actually taken

```text
QML:  AeroraUi.payload / .power / .mission / .identity / .capabilities
         │
         ▼  AeroraVehicleUiContext, in C++
      MultiVehicleManager::instance()->activeVehicle()        public
      MultiVehicleManager::activeVehicleChanged               public signal  (MultiVehicleManager.h:50)
         │
         ▼  AeroraVehicleServices::forVehicle(vehicle)        public static  (AeroraVehicleServices.h)
      AeroraVehicleServices
         │
         ▼  service(QString) + qobject_cast                   stays private to M-Ui
      AeroraPayloadModel / AeroraPowerModel / AeroraMissionObserver
```

`AeroraVehicleUiContext` is `QML_NAMED_ELEMENT(AeroraUi)` + `QML_SINGLETON`, matching
`QGroundControlQmlGlobal`'s shape.

| Design rule | Why |
| --- | --- |
| One typed `Q_PROPERTY` per service | A typed property fails to compile when its service changes shape. A string lookup returns null |
| The string-keyed lookup stays in C++ | The service registry is generic so the *vehicle layer* need not depend on every domain. A consumer has no such constraint, and the UI legitimately depends on the domains it displays |
| No `vehicle` property | `Vehicle` is already reachable as `QGroundControl.multiVehicleManager.activeVehicle`. A second route to the same writable object widens the surface for no gain. Asserted by `_uiContextDoesNotExposeTheVehicleOrTheServiceMap` |
| No `telemetry` property | `AeroraTelemetrySet` carries no QML registration macro, and there is nothing to show: every fact group is `CONTRACT-ONLY` and `AeroraVehicleFactGroupRegistrar::isRegistrationAvailable()` is false without the Q4-002 core edit |
| Every accessor null-safe | No vehicle, a vehicle with no Aerora composition, and the offline-editing vehicle — which is deliberately not composed — are all normal states. Services are held through `QPointer` |

### Read-orientation

Every property is `READ`-only. The services reached through them expose no actuating path to QML,
which was verified against each type's meta-object rather than asserted — see
[UI safety audit](#ui-safety-audit).

## Q7-004 — QML test discovery

**`OPEN`**, with the mechanism established and the smallest change identified. No Aerora QML test
is written, so nothing is blocked by it today.

### What the CMake actually does

`test/UnitTestFramework/QmlTesting/CMakeLists.txt`:

| Fact | Consequence |
| --- | --- |
| `add_executable(QGCQmlQuickTests ...)` with `QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"` | Discovery is hard-coded to `test/UnitTestFramework/QmlTesting/tests`. An Aerora `tst_*.qml` is not discoverable from `aerora/test/` |
| `add_test(NAME QmlQuickTests COMMAND ... -platform offscreen)` with `LABELS "Unit;QML"`, `ENVIRONMENT QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML2_IMPORT_PATH=${CMAKE_BINARY_DIR}/qml` | The environment an Aerora equivalent must reproduce |
| `QmlTestFileValidator` registered with `add_qgc_test(... LABELS Unit)`, reading `QGC_QML_TEST_SOURCE_DIR` | An in-process structure check, not the Qt Quick execution path |

### The four options, assessed

| Option | Core edit? | Verdict |
| --- | --- | --- |
| Add Aerora `tst_*.qml` to the upstream test directory | **Yes** — writes into `test/`, which M-Ui does not own | Rejected |
| A second Qt Quick Test executable registered from `aerora/CMakeLists.txt` | No | **Recommended.** Needs an `AeroraQmlQuickTestMain.cc` (M-Ui owns it) plus `add_executable` / `add_test` in Tab 1's file |
| `aerora/test/` participating directly in `QGCQmlQuickTests` | **Yes** — `QUICK_TEST_SOURCE_DIR` is a single hard-coded path | Rejected |
| C++ harness only, via `add_qgc_test` | No | **Done this pass.** Covers both seams deterministically; covers no QML rendering |

The recommendation is option 2 **when the first Aerora QML behaviour needs asserting**, not before.
Creating an empty test target now would add a build target with nothing in it. Filed as HO-TAB7-04.

### What the C++ harness cannot cover

Whether `AeroraStateText` renders `UNKNOWN` as the word rather than as an empty string, and whether
two indicators are visually distinguishable. The first needs option 2; the second is `DEMO`
verification gated on Q-040 and no test can assert it.

## UI safety audit

The interface is **observation and supervision**. It is not flight authority. That is asserted
mechanically rather than in prose, because a method becomes QML-callable by acquiring `Q_INVOKABLE`
or by being a slot, and a property becomes QML-writable by acquiring a `WRITE` accessor — each a
one-line change in another workstream's header, with no compiler warning and no UI diff.

`AeroraUiSafetyTest` reads the meta-objects of the ten types reachable from `AeroraUi` and fails
when the surface grows. Signals are excluded: emitting one from QML re-evaluates bindings and
actuates nothing.

### Result

| Forbidden reach | Status | Evidence |
| --- | --- | --- |
| Payload command submission | **Unreachable** | `AeroraPayloadModel::submitRequest()` and `setCommandSink()` are not `Q_INVOKABLE` and not slots. No `AeroraPayloadCommandSink` implementation exists anywhere in the overlay |
| Power switching | **Unreachable** | `AeroraPowerModel` declares no outbound method of any kind. `kStationInitiatedPowerActionImplemented`, `kPowerArbitrationImplemented`, `kAutomaticSourceTransitionImplemented` all false; `kDefinedThresholdCount == 0` |
| MAVLink command authorization | **Unreachable** | Nothing in `aerora/src/Security/` carries `Q_OBJECT`, `Q_GADGET` or any QML macro. `AeroraCorePlugin::isAuthorizationEnforced()` is a static C++ function |
| Security policy | **Unreachable** | Same. No role, permission or operator-identity type is QML-visible |
| Mission commanding | **Unreachable** | `AeroraMissionObserver` has no command API. Its only `Q_INVOKABLE` is `verify()`, a local comparison of two snapshots |
| Flight stabilization, failsafe, radio | **Unreachable** | No Aerora type exposes any of them; M-Ui references none |
| Fact-group registration | **Unreachable** | `registerGroup` / `registerFactGroup` are plain C++ |
| The service map | **Unreachable** | `service()` / `serviceNames()` stay private to `AeroraVehicleUiContext` |

### The one deviation found

| | |
| --- | --- |
| **Property** | `AeroraStateValue::capability` |
| **Declaration** | `Q_PROPERTY(Capability capability READ capability WRITE setCapability NOTIFY stateChanged)` (`AeroraStateValue.h:111`) |
| **Why it matters** | `CapabilityAbsent` is the only input that produces `UNAVAILABLE`. A QML write could manufacture an `UNAVAILABLE` the vehicle never reported — the derivation `AERORA-GCS-REQ-029` prohibits, and the one `IF-UIS-005` exists to prevent |
| **Reachability today** | Not reachable from `AeroraUi`: no property returns an `AeroraStateValue`. It becomes reachable when `AeroraValueDisplay` is bound to one |
| **Disposition** | Filed as HO-TAB7-01. Not worked around. `AeroraUiSafetyTest::_onlyTheRecordedPropertyDeviationIsWritable` asserts the writable set is **exactly** this one entry, so a second cannot appear while the first is outstanding |

`AeroraReportedValue`, the primitive payload and power actually use, declares `capability` correctly
as `READ`-only with `setCapability` as plain C++. The deviation is in the Fact-backed type only.

### A noted non-issue

`AeroraStateValue::fact` returns a `Fact *`, and `Fact` carries writable `value` / `rawValue`
properties. That is upstream QGroundControl and is how parameter editing works. It is not an Aerora
widening: every Fact in the application has had that surface since before Aerora existed, no Aerora
Fact exists, and no Aerora UI control writes one. The authority question for `PARAM_SET` is
`src/FactSystem` ownership, which remains **`OPEN — UNASSIGNED`** and is not M-Ui's.

## Tier discipline

| Tier | Mechanism | Aerora count |
| --- | --- | --- |
| 1 | C++ virtual on `QGCOptions` / `QGCFlyViewOptions` / `QGCCorePlugin` | **1** — `createQmlApplicationEngine` + its destroy counterpart, pending HO-TAB7-02 |
| 2 | Purpose-built empty QML seam | **0** |
| 3 | Override of a populated upstream file | **0** |

Three pre-approved tier-2 candidates are listed, commented out, in
`AeroraQmlOverrides::overrideEntries()`. Two of them —
`CustomGuidedActionsController.qml` and `FlyViewAdditionalCustomActionsList.qml` — exist to
introduce operator **actions**, and must stay unused until an Aerora action has an authorized
command path. None exists.

### `CustomMapItems.qml` is forbidden, mechanically

It reads as a fourth purpose-built seam and is not one: upstream populates it with an `Instantiator`
over `QGCCorePlugin::customMapItems()` (`src/API/QGCCorePlugin.h:158`) and instantiates it at
`src/FlyView/FlyViewMap.qml:247`. Overriding it would fork a populated file **and** discard the C++
seam that is the supported way to add a map item.

It is on `AeroraQmlOverrides::forbiddenPaths()`, and
`AeroraUiFoundationTest::_customMapItemsIsForbiddenAndUndeclared` asserts it is both forbidden and
undeclared. The mistake is unrepresentable rather than merely reviewable.

## Handoffs

### HO-TAB7-01 — `AeroraStateValue::capability` is QML-writable

| Field | Detail |
| --- | --- |
| **Owner** | **Tab 4 (M-Telemetry)**, `aerora/src/Telemetry/AeroraStateValue.h:111` |
| **Severity** | Medium, **latent** — not reachable from the current Aerora UI surface |
| **Invariant at risk** | `AERORA-GCS-REQ-029`, `IF-UIS-005`: `UNAVAILABLE` is produced only by a vehicle capability report |
| **Change requested** | Drop `WRITE setCapability` from the `Q_PROPERTY`. Keep `setCapability()` as a public C++ method — the capability model needs it, and `AeroraReportedValue` already has exactly this shape |
| **Impact** | None known. No Aerora code sets capability yet (Q3-009 / F3.1-05), and no QML reads or writes it |
| **Tracked by** | `AeroraUiSafetyTest::_onlyTheRecordedPropertyDeviationIsWritable`. **Remove the entry from `recordedWritablePropertyDeviations()` when the fix lands**, or the test will fail on the now-absent deviation |

### HO-TAB7-02 — install the override interceptor

| Field | Detail |
| --- | --- |
| **Owner** | **Tab 1 (M-Plugin)**, `aerora/src/AeroraCorePlugin.{h,cc}` |
| **Why not M-Ui** | `AeroraCorePlugin.*` is M-Plugin's per `aerora/CMakeLists.txt`. The interceptor itself is M-Ui's and is complete |
| **Blocks** | Every tier-2 and tier-3 QML override. Until it is applied, an overlay QML override file would be compiled into a resource and **never loaded, with no error** |
| **Does not block** | The `Aerora.Controls` QML module. A module import does not pass through the interceptor |

Header, in the `public:` section:

```cpp
    /// Installs the Aerora QML override interceptor. See aerora/src/UI/AeroraQmlOverrides.h.
    QQmlApplicationEngine *createQmlApplicationEngine(QObject *parent) override;

    /// Releases the interceptor before the base implementation destroys the engine.
    void destroyQmlApplicationEngine(QQmlApplicationEngine *qmlEngine) override;
```

Implementation:

```cpp
#include "AeroraQmlOverrides.h"
#include <QtQml/QQmlApplicationEngine>

QQmlApplicationEngine *AeroraCorePlugin::createQmlApplicationEngine(QObject *parent)
{
    QQmlApplicationEngine *const qmlEngine = QGCCorePlugin::createQmlApplicationEngine(parent);
    AeroraQmlOverrides::installOn(qmlEngine);
    return qmlEngine;
}

void AeroraCorePlugin::destroyQmlApplicationEngine(QQmlApplicationEngine *qmlEngine)
{
    AeroraQmlOverrides::releaseFrom(qmlEngine);
    QGCCorePlugin::destroyQmlApplicationEngine(qmlEngine);
}
```

Order matters in both directions: install **after** the base creates the engine, release
**before** the base deletes it. No `addImportPath` call is needed — the base already adds
`qrc:/qml` (`src/API/QGCCorePlugin.cc:303`) and `Aerora.Controls` resolves from it.

### HO-TAB7-03 — create the `Aerora.Controls` QML module target

| Field | Detail |
| --- | --- |
| **Owner** | **Tab 1 (M-Plugin)**, `aerora/CMakeLists.txt` |
| **Why not M-Ui** | `qt_add_library` and `qt_add_qml_module` are targets, which the manifest convention excludes from a `.cmake` manifest |
| **Blocks** | Registration of the four Aerora QML controls. Tab 1's note at `aerora/CMakeLists.txt:148-152` records this as outstanding pending verification of the pattern |
| **Status of that verification** | **Done.** The pattern below is `custom-example/CMakeLists.txt:157-183`'s `CustomModule` with the URI and file list substituted; `qgc_set_qt_resource_alias` is `cmake/Helpers.cmake:12`, reached because root `CMakeLists.txt:21` does `include(Helpers)` |

The exact block, also carried in `aerora/src/UI/Ui.cmake`:

```cmake
qt_add_library(AeroraControlsModule STATIC)
qgc_configure_target(AeroraControlsModule)

qgc_set_qt_resource_alias(${AERORA_UI_QML_FILES})

qt_add_qml_module(AeroraControlsModule
    URI ${AERORA_UI_QML_MODULE_URI}
    VERSION 1.0
    RESOURCE_PREFIX /qml
    QML_FILES ${AERORA_UI_QML_FILES}
    IMPORTS
        QGC
        QGroundControl
        QGroundControl.Controls
    NO_PLUGIN
)

list(APPEND _aerora_libraries AeroraControlsModule)
```

`IMPORTS QGC` is the load-bearing line: it is how `AeroraTelemetryState` and `AeroraUi` resolve
inside the module, and it mirrors `src/CMakeLists.txt:15-26`. `qgc_set_qt_resource_alias` flattens
each path to its bare filename; without it the resource lands at
`qrc:/qml/Aerora/Controls/src/UI/Controls/<Name>.qml` instead of
`qrc:/qml/Aerora/Controls/<Name>.qml`.

### HO-TAB7-04 — an Aerora Qt Quick Test target, when needed

| Field | Detail |
| --- | --- |
| **Owner** | **Tab 1** (the `add_executable` / `add_test`) and **Tab 7** (`AeroraQmlQuickTestMain.cc`) |
| **Blocks** | QML rendering tests only. The C++ foundation tests are unaffected |
| **When** | At the first Aerora QML behaviour that needs asserting. Not now — an empty test target is a target with nothing in it |
| **Shape** | Mirror `test/UnitTestFramework/QmlTesting/CMakeLists.txt` with `QUICK_TEST_SOURCE_DIR` pointing at `aerora/test/UI/qml`, the same `-platform offscreen` command, `LABELS "Unit;QML"`, and the same `ENVIRONMENT` block |

### Withdrawn

**Q7-002 / Tab 3.** The architecture map proposed making `service()`, `serviceNames()` and
`forVehicle()` `Q_INVOKABLE` on `AeroraVehicleServices`. Withdrawn: the route needs no vehicle-layer
change, and the proposal conflicted both with Tab 3's rationale for keeping the registry generic and
with this pass's instruction not to expose arbitrary service lookup to QML.

## Verification status

| Check | Status | Evidence |
| --- | --- | --- |
| Compiles | **`BLOCKED`** | No `cmake`, `ninja`, `cl`, `g++` or Qt on this host; `C:\Qt` does not exist |
| `just lint` / `pre-commit` / `clang-format` / `clang-tidy` | **`BLOCKED`** | None present |
| `qmllint` / `qmlformat` | **`BLOCKED`** | Neither present. **No QML file here has been linted** |
| Tests executed | **`BLOCKED`** | Requires a build. The two suites are written and registered; they have never run |
| QML loaded / rendered | **`BLOCKED`** | Requires a launch |
| Override interception exercised at runtime | **`BLOCKED`** | Requires a launch, and HO-TAB7-02 |
| File hygiene | **Checked, 15/15 pass** | `.kiro/tools/aerora-ui-static-check.ps1`: LF endings, no tabs, newline at EOF, no trailing whitespace, ≤120 columns, no `Q_ASSERT`, no uncategorised `qDebug()`, no `QTest::ignoreMessage`, no fixed-delay `qWait`, balanced delimiters, `#pragma once`, no hard-coded MAVLink identifier |
| Overlay manifest consistency | **Checked, 0 problems** | `.kiro/tools/aerora-manifest-check.ps1`: 109 declared entries all resolve; 109 sources on disk all declared; every `_TEST_NAMES` entry and every `UT_REGISTER_TEST` has an `add_qgc_test` entry, and conversely |
| Upstream seam signatures | **Source-verified** | `QGCCorePlugin.{h,cc}`, `QGCOptions.h`, `QGCApplication.cc`, `MultiVehicleManager.h`, `QmlControls/CMakeLists.txt`, root and `src/CMakeLists.txt`, `custom-example/{CMakeLists.txt,src/CustomPlugin.cc}`, `test/UnitTestFramework/QmlTesting/CMakeLists.txt` |
| Aerora contract signatures | **Source-verified** | `AeroraVehicleServices.h`, `AeroraPayloadModel.h`, `AeroraPowerModel.h`, `AeroraMissionObserver.h`, `AeroraReportedValue.h`, `AeroraStateValue.h`, `AeroraTelemetryState.h`, `AeroraVehicleCapabilities.h`, `AeroraVehicleIdentity.h` |
| QML-reachable surface | **Source-enumerated** | Every `aerora/src/**/*.h` grepped for `Q_PROPERTY … WRITE`, `Q_INVOKABLE` and `slots:`. One writable property found, recorded as HO-TAB7-01 |

**No claim is made that any of this compiles, lints, loads or passes a test.** The QML in particular
has had no tooling applied to it of any kind.

### What the static checks cannot cover

| Not covered | Needs |
| --- | --- |
| QML syntax, unqualified access, property typos, import resolution | `qmllint` |
| C++ declaration / definition parity beyond manual reading | A compiler |
| Whether `Aerora.Controls` resolves at runtime | A build and a launch |
| Whether the safety test's reviewed-method lists match the real meta-objects | A run. The lists were derived by reading the headers; a divergence fails the test, which is the intent, but a first run may fail on an error in the list rather than a real regression |

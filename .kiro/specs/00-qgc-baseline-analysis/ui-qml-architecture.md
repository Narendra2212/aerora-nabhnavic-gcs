# UI and QML Architecture

QML module structure, the application shell, the C++/QML boundary, and — most importantly — every
seam through which Aerora can change the UI without editing core QML.

## Headline finding

**The UI is extensively and deliberately extensible.** A QML URL interceptor allows per-file
replacement of *any* core QML file or resource, opt-in by mere presence. Every extensible surface
is a flat list of QML file URLs resolved by a `Loader`. Aerora can re-skin and re-shape the
application without forking core QML.

One correction to a common assumption: **`src/UI/` does not exist.** The application shell lives
in `src/MainWindow/`, which contains only `MainWindow.qml` and `MainWindowSavedState.qml`.

## QML module structure

It is `qt_add_qml_module` everywhere — **no legacy `.qrc` for QML**. `qt_add_resources` is used
only for non-QML assets. Every module uses `RESOURCE_PREFIX /qml` and `NO_PLUGIN`, meaning static
linkage with no runtime QML plugins.

The root `CMakeLists.txt:422-429` declares the application target itself as a module:

```cmake
qt_add_qml_module(${CMAKE_PROJECT_NAME} URI QGC VERSION 1.0
    RESOURCE_PREFIX /qml
    IMPORTS QtQuick QtQuick.Controls QtQuick.Dialogs QtQuick.Layouts)
```

This `QGC` module carries no QML files of its own; it exists to aggregate the `IMPORTS` and host
the resource prefix.

### URI to target map

| URI | Target | Declared in |
| --- | --- | --- |
| `QGC` | `${CMAKE_PROJECT_NAME}` | `CMakeLists.txt:422` |
| `QGroundControl` | `QGroundControlModule` | `src/CMakeLists.txt:15` |
| `QGroundControl.Controls` | `QGroundControlControlsModule` | `src/QmlControls/CMakeLists.txt:88` |
| `QGroundControl.FactControls` | `FactControlsModule` | `src/FactSystem/FactControls/CMakeLists.txt:11` |
| `QGroundControl.FlyView` | `FlyViewModule` | `src/FlyView/CMakeLists.txt:8` |
| `QGroundControl.PlanView` | `PlanViewModule` | `src/PlanView/CMakeLists.txt:12` |
| `QGroundControl.Toolbar` | `ToolbarModule` | `src/Toolbar/CMakeLists.txt:3` |
| `QGroundControl.AppSettings` | `AppSettingsModule` | `src/AppSettings/CMakeLists.txt:112` |
| `QGroundControl.FlightMap` | `FlightMapModule` | `src/FlightMap/CMakeLists.txt:8` |
| `QGroundControl.GeoMap` | `GeoMapModule` | `src/GeoMap/CMakeLists.txt:34` |
| `QGroundControl.AnalyzeView` | `AnalyzeViewModule` | `src/AnalyzeView/CMakeLists.txt:19` |
| `QGroundControl.AnalyzeView.GeoTag` | `GeoTagModule` | `src/AnalyzeView/GeoTag/CMakeLists.txt:28` |
| `QGroundControl.LogViewer` | `LogViewerModule` | `src/AnalyzeView/LogViewer/CMakeLists.txt:37` |
| `QGroundControl.VehicleSetup` | `VehicleSetupModule` | `src/Vehicle/VehicleSetup/CMakeLists.txt:32` |
| `QGroundControl.AutoPilotPlugins.Common` / `.PX4` / `.APM` | matching `*Module` | `src/AutoPilotPlugins/*/CMakeLists.txt` |
| `QGroundControl.FirmwarePlugin.PX4` / `.APM` | `PX4FirmwareModule` / `APMFirmwareModule` | `src/FirmwarePlugin/*/CMakeLists.txt` |
| `QGroundControl.Viewer3D` | `Viewer3DModule` | `src/Viewer3D/CMakeLists.txt:40` |
| `QGroundControl.FirstRunPromptDialogs` | `FirstRunPromptDialogsModule` | `src/FirstRunPromptDialogs/CMakeLists.txt:3` |
| `QGroundControl.Logging` / `.LogManager` / `.Compression` / `.Geo` / `.GPS.NTRIP` | matching targets | `src/Utilities/...`, `src/LogManager/`, `src/GPS/NTRIP/` |
| `Custom.Widgets` | `CustomModule` | `custom-example/CMakeLists.txt:157` |
| `Custom.Plan` | `CustomPlanModule` | `custom-example/CMakeLists.txt:185` |

Two modules are commented out and therefore dead configuration:
`QGroundControl.MissionManager` (`src/MissionManager/CMakeLists.txt:110`) and
`QGroundControl.Test.MissionManager` (`test/MissionManager/CMakeLists.txt:69`).

### How QML reaches C++

Declarative registration via `QtQmlIntegration` macros on the C++ class, then a plain `import`.
Three idioms in use:

| Idiom | Example |
| --- | --- |
| `QML_NAMED_ELEMENT(QGroundControl)` + `QML_SINGLETON` | `src/QmlControls/QGroundControlQmlGlobal.h:46-47` |
| `QML_ELEMENT` + `QML_SINGLETON` | `src/QmlControls/ScreenToolsController.h:12-13` |
| `QML_ELEMENT` / `QML_UNCREATABLE("")` for C++-owned objects exposed as properties | `QGCOptions`, `QGCFlyViewOptions`, `QGCCorePlugin.h:40-41`, `QGCPalette` |

Where C++ registration lives in a different module from the QML that uses it, CMake wires it with
`DEPENDENCIES` — for example `AppSettingsModule` declares
`DEPENDENCIES QGroundControl.Controls QGroundControl.LogManager`.

## Application shell

### Boot path

1. `QGCCorePlugin::createQmlApplicationEngine()` creates the `QQmlApplicationEngine`, calls
   `addImportPath("qrc:/qml")`, and puts `joystickManager` into the root context as a context
   property.
2. `QGCCorePlugin::createRootWindow()` loads `qrc:/qml/QGroundControl/MainWindow.qml`.

The root file is `src/MainWindow/MainWindow.qml`, an `ApplicationWindow`. The CMake alias flattens
the path (`src/CMakeLists.txt:11-14` sets `QT_RESOURCE_ALIAS MainWindow.qml`), which is why the
runtime URL is `.../QGroundControl/MainWindow.qml` rather than
`.../QGroundControl/MainWindow/MainWindow.qml`.

### What the root provides to every page

All QML in the application can see these because they are properties or functions on the root
window.

| Facility | Detail |
| --- | --- |
| `QtObject { id: globals }` | `activeVehicle`, `defaultTextHeight`, `defaultTextWidth`, `planMasterControllerFlyView`, `guidedControllerFlyView`, `validationErrorCount`, `navigationBlockedReason`, `commingFromRIDIndicator` |
| `QGCPalette { id: qgcPal; colorGroupEnabled: true }` | The application-wide palette instance |
| Navigation | `showFlyView()`, `showPlanView()`, `showTool(title, source, icon)`, `showAnalyzeTool()`, `showVehicleConfig()`, `showVehicleConfigParametersPage()`, `showKnownVehicleComponentConfigPage()`, `showSettingsTool(page)`, `showToolSelectDialog()` |
| Dialogs and drawers | `_showMessageDialogWorker()`, `showCriticalVehicleMessage()`, `showIndicatorDrawer(component, item)`, `closeIndicatorDrawer()` |
| Validation gate | `allowViewSwitch(previousValidationErrorCount, showErrorOnDisallow)` — also honours `globals.navigationBlockedReason`, which is how calibration blocks navigation |
| Close guard | `onClosing` to `performCloseChecks()` to `checkForUnsavedMission()` / `checkForPendingParameterWrites()` / `checkForActiveConnections()` to `finishCloseProcess()` |
| First-run prompts | `firstRunPromptManager` iterates `QGroundControl.corePlugin.firstRunPromptsToShow()` and creates each via `corePlugin.firstRunPromptResource(id)` |
| Signals | `armVehicleRequest`, `forceArmVehicleRequest`, `disarmVehicleRequest`, `vtolTransitionToFwdFlightRequest`, `vtolTransitionToMRFlightRequest`, `showPreFlightChecklistIfNeeded` |

The `allowViewSwitch` and `navigationBlockedReason` mechanism is directly relevant to Aerora: it is
the existing, sanctioned way to block operator navigation during a safety-critical operation.

Android gets special treatment at the window level: `flags` adds
`Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint`, and all four padding values are zeroed
because Qt 6.9+ auto-sets `ApplicationWindow` padding to display safe-area insets, which would
inset QGC's edge-to-edge content.

### View switching

FlyView and PlanView are both instantiated eagerly and are **always alive**. Switching is pure
visibility, not a `StackView` or `Loader`:

```qml
FlyView  { id: flyView;  objectName: "mainView_fly";  anchors.fill: parent; enabled: !toolDrawer.visible }
PlanView { id: planView; objectName: "mainView_plan"; anchors.fill: parent; visible: false; enabled: !toolDrawer.visible }
```

The `enabled: !toolDrawer.visible` binding carries a comment explaining that pointer handlers —
for example the GeoMap `DragHandler` — would otherwise receive presses through the drawer and steal
drags.

**Consequence worth knowing:** PlanView state and its `PlanMasterController` persist across view
switches, and FlyView has its own separate `PlanMasterController { flyView: true }`. Aerora must
not assume a view is destroyed when hidden.

### Secondary views

Analyze, Vehicle Config, and App Settings are **not** siblings of FlyView and PlanView. They load
into a full-screen `Rectangle { id: toolDrawer }` containing its own mini toolbar — a QGC logo
button plus a title label — and a `Loader { id: toolDrawerLoader }`.

| Call | Loads |
| --- | --- |
| `showAnalyzeTool()` | `qrc:/qml/QGroundControl/AnalyzeView/AnalyzeView.qml` |
| `showVehicleConfig()` | `qrc:/qml/QGroundControl/VehicleSetup/VehicleConfigView.qml` |
| `showSettingsTool()` | `qrc:/qml/QGroundControl/Controls/AppSettings.qml` |

Setting `toolDrawer.visible = false` clears `toolDrawerLoader.source`, destroying the page.

### Toolbar composition

There is **no single global toolbar** in the root window. Each view owns its own:

| File | Role |
| --- | --- |
| `src/Toolbar/FlyViewToolBar.qml` | Instantiated inside `src/FlyView/FlyView.qml` |
| `src/Toolbar/PlanViewToolBar.qml` | The PlanView equivalent |
| `src/Toolbar/SelectViewDropdown.qml` | The view switcher, opened via `showIndicatorDrawer()` from the logo button |
| Inline `toolDrawerToolbar` | The tool drawer builds its own |

The root window does own `footer: LogReplayStatusBar` and the global `indicatorDrawer` popup that
all toolbar indicators drop into.

## Fly View composition

`src/FlyView/FlyView.qml` is a flat stack of z-ordered siblings inside `Item { id: mapHolder }`.

| Element | File | Note |
| --- | --- | --- |
| Map engine A | `src/FlyView/FlyViewMap.qml` | QtLocation engine |
| Map engine B | `src/FlyView/FlyViewGeoMapAdapter.qml` | Experimental GeoMap engine |
| Video | `src/FlyView/FlyViewVideo.qml` | |
| Picture-in-picture swap | `PipView` (QmlControls) | `item1IsFullSettingsKey: "MainFlyWindowIsMap"` |
| Widget overlay | `src/FlyView/FlyViewWidgetLayer.qml` | All upstream widgets |
| **Custom overlay** | `src/FlyView/FlyViewCustomLayer.qml` | **Vendor seam, empty by default** |
| Occluder debug | `src/FlyView/FlyViewOccluderViewer.qml` | `visible: false` |
| Guided logic | `src/FlyView/GuidedActionsController.qml` | Non-visual controller |
| Guided slider | `src/FlyView/GuidedValueSlider.qml` | |
| 3D viewer | `Loader` to `Viewer3DModel.qml` | |
| Toolbar | `src/Toolbar/FlyViewToolBar.qml` | |

Map engine choice is snapshotted at `Component.onCompleted` from
`flyViewSettings.useGeoMapEngine`, a reboot-required setting; a live swap is explicitly avoided.

`FlyViewWidgetLayer.qml` contains `FlyViewOccluders` (rectangle bookkeeping so the map keeps the
vehicle out from under widgets), `FlyViewTopRightPanel`, `FlyViewTopRightColumnLayout`,
`FlyViewBottomRightRowLayout`, `FlyViewToolStrip`, `VehicleWarnings`, `MapScale`,
`FlyViewGeoMapControls`, `Viewer3DScaleBar`, the virtual joystick `Loader`, and the
preflight-checklist `Loader`.

### Toolbar indicators — two extensible lists

`src/Toolbar/FlyViewToolBarIndicators.qml` is just two `Repeater` elements over `QVariantList`
objects of QML file URLs:

```qml
Repeater { id: appRepeater;            model: QGroundControl.corePlugin.toolBarIndicators
           Loader { source: modelData; visible: item.showIndicator } }
Repeater { id: toolIndicatorsRepeater; model: _activeVehicle ? _activeVehicle.toolIndicators : []
           Loader { source: modelData; visible: item.showIndicator } }
```

| List | Source | Scope |
| --- | --- | --- |
| `QGCCorePlugin::toolBarIndicators()` | `src/API/QGCCorePlugin.h`, base implementation returns a one-element list containing `RTKGPSIndicator.qml` | **Vehicle-independent** |
| `Vehicle::toolIndicators()` | `src/Vehicle/Vehicle.h:195`, implemented at `Vehicle.cc:2581` as pure delegation to `_firmwarePlugin->toolIndicators(this)` | **Per-firmware** |

The per-firmware default is `FirmwarePlugin::toolIndicators()` (`FirmwarePlugin.cc:237`).
`APMFirmwarePlugin::toolIndicators()` (`APMFirmwarePlugin.cc:641`) is the canonical
extend-the-base pattern: call the base, then append. `toolIndicatorsChanged` propagates
FirmwarePlugin to Vehicle to QML.

Each indicator QML must expose a `showIndicator` property, because the `Loader`'s `visible` binds
to `item.showIndicator`. Indicator detail pages are separate files — `GPSIndicatorPage.qml`,
`EscIndicatorPage.qml`, `RemoteIDIndicatorPage.qml` — opened via `showIndicatorDrawer()`.

**Aerora use.** Tether state, paint supply, and GPU/autonomy status indicators belong in
`QGCCorePlugin::toolBarIndicators()` if vehicle-independent, or in an Aerora
`FirmwarePlugin::toolIndicators()` if per-vehicle. Neither requires a core edit.

### Instrument panel — a Fact enum, not a C++ list

`src/FlyView/FlyViewInstrumentPanel.qml` is a thin wrapper:

```qml
SelectableControl {
    selectionUIRightAnchor: true
    selectedControl: QGroundControl.settingsManager.flyViewSettings.instrumentQmlFile2
}
```

`src/QmlControls/SelectableControl.qml` is the generic mechanism: a `property Fact selectedControl`
whose metadata carries `enumStrings` and `enumValues` where **each value is the QML file path of a
candidate widget**. A `Loader { source: selectedControl.rawValue }` instantiates it, and
right-click on desktop or press-and-hold on mobile reveals a `FactComboBox` to switch.

So the instrument-panel registry is the `enumValues` array of the `instrumentQmlFile2` fact,
defined in `src/Settings/FlyView.SettingsGroup.json` around line 124 and surfaced in the settings
UI through `src/AppSettings/pages/FlyView.SettingsUI.json:109`. **Adding a widget means adding an
enum entry, not registering in C++.**

A custom build can shadow `FlyView.SettingsGroup.json` through the custom settings directory, or
hide the stock panel entirely with `QGCFlyViewOptions::showInstrumentPanel()` returning `false`.

### Guided and tool strip surfaces

`FlyViewAdditionalActionsList.qml`, **`FlyViewAdditionalCustomActionsList.qml`**, and
**`FlyViewToolStripActionList.qml`** (all in `src/FlyView/`). The latter two are explicit override
targets in `custom-example`. Visibility of individual guided buttons is gated by
`QGCFlyViewOptions`: `guidedBarShowEmergencyStop`, `guidedBarShowOrbit`, `guidedBarShowROI`
(`src/API/QGCOptions.h:19-21`).

The bodies of `FlyViewToolStrip.qml`, `FlyViewToolStripActionList.qml`,
`FlyViewAdditionalCustomActionsList.qml`, and `FlyViewTopRightColumnLayout.qml` were **not** read,
so whether those lists are arrays of `ToolStripAction` objects or hand-laid items is inferred from
file names and the custom-example override list rather than confirmed.

## C++/QML boundary

### Main singleton

`QGroundControlQmlGlobal` (`src/QmlControls/QGroundControlQmlGlobal.h`), registered as
`QML_NAMED_ELEMENT(QGroundControl)` plus `QML_SINGLETON`, so QML writes `QGroundControl.foo` after
`import QGroundControl`.

Manager handles, all `CONSTANT`: `linkManager`, `serialPortManager`, `multiVehicleManager`,
`mapEngineManager`, `qgcPositionManger`, `videoManager`, `settingsManager`, `adsbVehicleManager`,
`ntripManager`, **`corePlugin`**, `missionCommandTree`, `mavlinkSigningKeys`, `gpsRtk`,
`gpsManager`, `globalPalette`, `unitsConversion`.

Feature flags: `singleFirmwareSupport`, `singleVehicleSupport`, `px4ProFirmwareSupported`,
`apmFirmwareSupported`. Version info: `qgcVersion`, `qgcAppDate`, `qgcDailyBuild`,
`newStableVersion`. Map view state: `flightMapPosition`, `flightMapZoom`, `flightMapInitialZoom`.

Z-order constants used throughout QML: `zOrderTopMost` 1000, `zOrderWidgets` 100,
`zOrderMapItems` 50, `zOrderWaypointIndicators` 50, `zOrderVehicles` 49, `zOrderTrajectoryLines`
48, `zOrderWaypointLines` 47.

Invokables include `showMessageDialog(owner, title, text, buttons, acceptFn, closeFn)` — which
emits `showMessageDialogRequested`, handled by the `Connections` block in `MainWindow.qml` —
plus `copyToClipboard`, `testAudioOutput`, the `start*MockLink` family, and
`altitudeFrameExtraUnits`/`altitudeFrameShortDescription`.

### ScreenTools — dual-natured

`src/QmlControls/ScreenToolsController.h` is the C++ `QML_ELEMENT` + `QML_SINGLETON` side, holding
platform flags `isAndroid`, `isiOS`, `isDebug`, `isMobile`. Separately,
**`src/QmlControls/ScreenTools.qml`** is a QML singleton declared via
`set_source_files_properties(ScreenTools.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)` in
`src/QmlControls/CMakeLists.txt:84-87`.

The CMake comment there documents why that file must stay in that module:
`set_source_files_properties()` has directory scope, and cross-directory propagation of
`QT_QML_SINGLETON_TYPE` fails on Windows. Worth knowing before any Aerora file reorganisation.

`ScreenTools` is the source of `defaultFontPixelWidth`/`defaultFontPixelHeight`, `toolbarHeight`,
`smallFontPointSize`/`largeFontPointSize`, `isTinyScreen`, `isMobile`. `CODING_STYLE.md` forbids
hardcoded QML sizes; everything must derive from these.

### Reaching the active vehicle

`QGroundControl.multiVehicleManager.activeVehicle`, conventionally cached at the top of each QML
file as `property var _activeVehicle`. The root window also publishes `globals.activeVehicle`.

It is legitimately null, so QML guards with ternaries — for example
`model: _activeVehicle ? _activeVehicle.toolIndicators : []` in `FlyViewToolBarIndicators.qml`, and
the null-check before `rebootVehicle()` in `MainWindow.qml`. This is the QML half of the
multi-vehicle golden rule.

### Binding to a Fact

Controls come from `QGroundControl.FactControls` (target `FactControlsModule`,
`src/FactSystem/FactControls/`). The pattern is a `property Fact` plus the control reading
`fact.rawValue` or `fact.value`.

Controls in use: `FactTextField` (`MainWindow.qml` calls `activeFocusControl._onEditingFinished()`
on it and uses `instanceof FactTextField` for validation gating), `FactComboBox` (with
`indexModel: false` to bind by value rather than index, as in `SelectableControl.qml`), plus
`FactCheckBox`, `FactSlider`, `FactValueSlider`, `FactLabel`, `LabelledFactTextField`.

Facts are reached by path: `QGroundControl.settingsManager.<group>.<factName>`, or
`vehicle.parameterManager` and the fact groups for vehicle parameters.

## Settings UI — generated and extensible

The settings page list is **data-driven and generated at CMake configure time**. Three layers.

### Layer A — the page list

`src/AppSettings/pages/SettingsPages.json`, an ordered `pages[]` array. Each entry has `name`,
`qml` (output file name), `icon`, and exactly one of `pageDefinition` (generated) or `url`
(hand-written). Optional `visible` is a **QML expression string** evaluated at runtime, for
example:

```json
"visible": "QGroundControl.corePlugin.options.showPX4LogTransferOptions && QGroundControl.px4ProFirmwareSupported"
```

`{"divider": true}` entries insert separators.

### Layer B — per-page layout

`src/AppSettings/pages/<Page>.SettingsUI.json`, fourteen of them: `General`, `FlyView`, `PlanView`,
`ADSBVehicleManager`, `CommLinks`, `Logging`, `Maps`, `NTRIP`, `PX4LogTransfer`, `RemoteID`,
`Telemetry`, `Video`, `Viewer3D`.

Schema per `tools/generators/settings_qml/README.md`: top-level `fileType: "SettingsUI"`,
`version`, optional `bindings` (accessor aliases such as
`"_appSettings": "QGroundControl.settingsManager.appSettings"`), and `groups[]` of controls
referencing settings by `"setting": "flyViewSettings.instrumentQmlFile2"`.

### Layer C — the generator

`tools/generators/settings_qml/` — `generate_pages.py`, `model.py`, `emit.py`,
`templates/*.j2` including `pages_model.qml.j2`, plus `README.md`.

Driven from `src/AppSettings/CMakeLists.txt:1-105` via:

```cmake
qgc_add_qml_codegen(GenerateSettingsQml
    GENERATE_AT_CONFIGURE
    GENERATOR_MODULE tools.generators.settings_qml.generate_pages
    OUTPUT_DIR "${SETTINGS_QML_GEN_DIR}"
    QML_NAMES ${_generated_qml_names}
    ...)
```

Inputs are `SettingsPages.json`, the `*.SettingsUI.json` files, and
`src/Settings/*.SettingsGroup.json` fact metadata. Outputs are one `.qml` per page **plus
`SettingsPagesModel.qml`**, the `ListModel` that drives the sidebar.

### Consumer

`src/QmlControls/AppSettings.qml` instantiates `SettingsPagesModel { id: settingsPagesModel }` and
renders a `Repeater` over it. Each model row carries `name`, `nameKey` (untranslated, used by
`showSettingsPage()`), `url`, `iconUrl`, `pageVisible` (a function compiled from the JSON `visible`
expression), and `sections()`. The right side is a `Loader` set to `entry.url`. It also implements
search, collapsible sections, and divider-collapsing logic.

### SettingsGroup and SettingsFact

`src/Settings/SettingsGroup.{h,cc}` with the `DECLARE_SETTINGGROUP` and `DECLARE_SETTINGSFACT`
macros. Note that `SettingsGroup`'s constructor seeds `_userVisible` from
`QGCCorePlugin::instance()->overrideSettingsGroupVisibility(name)` (`SettingsGroup.cc:9`), so a
core plugin can hide an entire group.

`SettingsManager` (`src/Settings/SettingsManager.{h,cc}`) owns all groups and provides
`registerCustomSettingsGroup(accessorName, group)` (`SettingsManager.h:113`, implementation
`.cc:91`), which validates the accessor against `^[a-z_][A-Za-z0-9_]*$`, rejects duplicates,
rejects collisions with built-in `Q_PROPERTY` accessors via `staticMetaObject.indexOfProperty`,
takes ownership, and deletes a rejected group.

### Adding an Aerora settings group — exact steps

1. Subclass `SettingsGroup` in `<custom>/src/Settings/`. Reference:
   `custom-example/src/Settings/CustomSettings.{h,cc}`.
2. Add `<custom>/src/Settings/<Name>.SettingsGroup.json` for fact metadata. The accessor name is
   the camelCase JSON stem plus `Settings`, so `Custom.SettingsGroup.json` maps to
   `customSettings`.
3. Override `QGCCorePlugin::registerCustomSettings(SettingsManager*)` and call
   `settingsManager->registerCustomSettingsGroup("aeroraSettings", new AeroraSettings())`.
4. Register the JSON as a resource under `/json`. The CMake comment at
   `custom-example/CMakeLists.txt:127-134` is explicit about why **both** paths are needed: the
   generator reads it from `src/Settings` at configure time, and `SettingsGroup` loads it from
   `:/json` at runtime.
5. Add `<custom>/src/AppSettings/pages/SettingsPages.json` to place the page.

### Page overlay semantics

Documented in `tools/generators/settings_qml/README.md`. Overlay keys are `insertAfter`,
`insertBefore`, and `remove`. An entry whose `name` matches a stock page **replaces it in place**,
and positioning keys are not allowed on a replace. A `*.SettingsUI.json` in the custom pages
directory **shadows** a stock file of the same name.

CMake auto-detects this: `src/AppSettings/CMakeLists.txt:26-38` checks
`${QGC_CUSTOM_DIR}/src/AppSettings/pages` and `${QGC_CUSTOM_DIR}/src/Settings` and appends
`--custom-pages-dir` and `--custom-settings-dir`. Because an overlay can add, remove, or replace
pages, the output file list cannot be hardcoded, so CMake runs the generator with `--list-outputs`
first and uses the result (lines 40-57).

## Theming and branding

### QGCPalette

`src/QmlControls/QGCPalette.h`, a `QML_ELEMENT` and therefore instantiable. `MainWindow.qml`
creates `QGCPalette { id: qgcPal; colorGroupEnabled: true }`, and `QGroundControl.globalPalette` is
an always-enabled global instance.

Structure: a two-by-two matrix of `Theme` (`Light` meaning outdoor, `Dark` meaning indoor) by
`ColorGroup` (`ColorGroupEnabled`, `ColorGroupDisabled`), stored in a static
`QMap<int, QMap<int, QMap<QString, QColor>>> _colorInfoMap`.

`_theme` is **static** — one global theme for all palette objects. `setGlobalTheme()` triggers
`_signalPaletteChangeToAll()`, repainting every live `QGCPalette` tracked in a static
`_paletteObjects` list.

Roughly fifty colour roles are declared with `DEFINE_QGC_COLOR(name, setName)`, which generates a
`Q_PROPERTY QColor <name>`, a `Q_PROPERTY QStringList <name>Colors` (all four variants, used by the
Palette Test page), and a setter.

Roles include `window`, `windowShade`, `windowShadeDark`, `text`, `warningText`, `button`,
`buttonText`, `buttonHighlight`, `primaryButton`, `textField`, `mapButton`, `mapIndicator`,
`colorGreen`, `colorYellow`, `colorOrange`, `colorRed`, `colorGrey`, `colorBlue`,
`alertBackground`, `alertBorder`, `alertText`, `missionItemEditor`, `toolbarBackground`,
`toolStripFGColor`, `toolStripHoverColor`, and — notably for a vendor — **`brandPrimary` and
`brandAccent`**.

### The palette override hook

Actual colour values live in `_buildMap()` in `src/QmlControls/QGCPalette.cc`, declared through
the `DECLARE_QGC_COLOR(name, lightDisabled, lightEnabled, darkDisabled, darkEnabled)` macro. That
macro (`QGCPalette.h:9-21`) does this **before** storing anything:

```cpp
QGCCorePlugin::instance()->paletteOverride(#name, colorInfo);
```

**Every single colour, in all four theme and group combinations, passes through
`QGCCorePlugin::paletteOverride()` and can be rewritten by a custom plugin.**
`DECLARE_QGC_NONTHEMED_COLOR` and `DECLARE_QGC_SINGLE_COLOR` route through the same hook.

`custom-example/src/CustomPlugin.cc` overrides roughly thirty roles including `brandPrimary` and
`brandAccent` — it is the reference implementation. The body of `_buildMap()` was not read, so the
stock default values per role are unverified; the override mechanism itself is confirmed from the
header macros.

`QGCOptions` adds two theming knobs outside the palette: `toolbarBackgroundLight()` and
`toolbarBackgroundDark()` (`QGCOptions.h:102-106`), plus `toolbarHeightMultiplier()`.

### Branding

Split between CMake at build time and resources at runtime, neither requiring core QML edits.

Build-time, via `<custom>/cmake/CustomOverrides.cmake`, which is included before `project()` so it
can `FORCE`-override any cache variable: `QGC_APP_NAME`, `QGC_ANDROID_PACKAGE_NAME`,
`QGC_MACOS_ICON_PATH`, `QGC_APPIMAGE_ICON_SCALABLE_PATH`, `QGC_WINDOWS_ICON_PATH`,
`QGC_WINDOWS_INSTALL_HEADER_PATH`. Also available: `QGC_APP_COPYRIGHT`, `QGC_APP_DESCRIPTION`,
`QGC_ORG_NAME`, `QGC_ORG_DOMAIN`, `QGC_PACKAGE_NAME`, `QGC_SETTINGS_VERSION`,
`QGC_MACOS_PLIST_PATH`, `QGC_MACOS_BUNDLE_ID`, `QGC_APPIMAGE_DESKTOP_ENTRY_PATH`,
`QGC_APPIMAGE_METADATA_PATH`, `QGC_WINDOWS_RESOURCE_FILE_PATH`.

Runtime, by replacing a resource at the same alias. `custom-example/CMakeLists.txt:97-110`
re-registers `QGCLogoFull.svg` and `qgroundcontrol.ico` under prefix `/Custom/res` with
`QT_RESOURCE_ALIAS` set to the stock names; the URL interceptor then redirects core QML's
`/res/QGCLogoFull.svg` reference. The same trick swaps the vehicle icon:
`CustomVehicleIcon.svg` aliased to `PaperPlane.svg` under `/Custom/qmlimages`.

The application name reaches QML as `QGroundControl.appName`.

**No splash-screen mechanism was found.** No splash resource or splash QML exists anywhere in the
tree. Aerora splash branding would have to be the platform launch image — Android package overlay
or macOS bundle — not a QGC feature. Recorded in [open-questions.md](open-questions.md).

## UI extension seams

### Activating a custom build

`cmake/CustomOptions.cmake:11-14` defines `QGC_CUSTOM_DIR`, default `"custom"`. Root
`CMakeLists.txt:47-77` then:

1. **Validates it.** Must be non-empty, **not absolute**, and contain no `..` segment; otherwise
   `FATAL_ERROR`. There is a CMake test for this escape attempt at
   `cmake/tests/CMakeLists.txt:46-50`.
2. If the directory exists, requires **both** `CMakeLists.txt` and `cmake/CustomOverrides.cmake`
   inside it, else `FATAL_ERROR`.
3. Sets `QGC_CUSTOM_BUILD ON`, appends the overlay `cmake/` to `CMAKE_MODULE_PATH`, and includes
   `CustomOverrides.cmake` — early enough to `FORCE`-override cache variables.

Later, `add_subdirectory("${QGC_CUSTOM_DIR}")` at `CMakeLists.txt:402-405`, before platform
deployment so Android overlays exist in time, and `qgc_configure_directory_targets` on the custom
tree at lines 483-486. `src/CMakeLists.txt:188-215` consumes the `PARENT_SCOPE` variables the
overlay exports. `cmake/install/Install.cmake:44-47` adds `-qmldir=<custom>` for deployment.

Activation per `custom-example/README.md`: **rename `custom-example` to `custom`**, clean the build
directory, rebuild.

### Replacing individual core QML files — the central seam

This is the most powerful mechanism available to Aerora. Precisely:

1. The overlay registers its replacement QML under resource prefix **`/Custom/qml`**, with
   `QT_RESOURCE_ALIAS` set to the *core* module-relative path
   (`custom-example/CMakeLists.txt:76-95`):

   ```cmake
   set_source_files_properties(".../src/FlyViewCustomLayer.qml"
       PROPERTIES QT_RESOURCE_ALIAS "QGroundControl/FlyView/FlyViewCustomLayer.qml")
   qt_add_resources(${CMAKE_PROJECT_NAME} custom_qml_overrides
       PREFIX "/Custom/qml" FILES ${CUSTOM_QML_OVERRIDES})
   ```

2. `CustomPlugin::createQmlApplicationEngine()` installs a `QQmlAbstractUrlInterceptor`:

   ```cpp
   _qmlEngine = QGCCorePlugin::createQmlApplicationEngine(parent);
   _qmlEngine->addImportPath("qrc:/qml/Custom/Widgets");
   _qmlEngine->addImportPath("qrc:/qml/Custom/Plan");
   _urlInterceptor = new CustomOverrideInterceptor();
   _qmlEngine->addUrlInterceptor(_urlInterceptor);
   ```

3. `CustomOverrideInterceptor::intercept()` implements the precedence rule. For `QmlFile` and
   `UrlString` data types with scheme `qrc`, it prepends `Custom` to the resource path and uses the
   override **if and only if the file exists**:

   ```cpp
   const QString overrideRes = QStringLiteral(":/Custom%1").arg(url.path());
   if (QFile::exists(overrideRes)) { /* return qrc:/Custom<path> */ }
   return url;  // otherwise fall through to core
   ```

So `qrc:/qml/QGroundControl/FlyView/FlyViewCustomLayer.qml` becomes
`qrc:/Custom/qml/QGroundControl/FlyView/FlyViewCustomLayer.qml`.

**Resolution is per-file and opt-in: dropping in a file overrides it, omitting it keeps the core
version.** This works for *any* core QML or resource asset, not a fixed allow-list. Teardown is
symmetric — `destroyQmlApplicationEngine()` removes and deletes the interceptor before the engine
is destroyed.

Three files are overridden in the example: `FlyViewCustomLayer.qml`,
`FlyViewToolStripActionList.qml`, `CustomGuidedActionsController.qml`. The first is the
designed-for seam — `src/FlyView/FlyViewCustomLayer.qml` is deliberately an empty `Item` whose own
comment directs a vendor to copy the code into their own control and override the resource. Its
contract is `occluders` in, where core widgets are, and `customOccluders` out, the rectangles of
your controls, so the map keeps the vehicle clear of them.

**Caveat.** The interceptor only rewrites `qrc:` URLs. QML `import` statements resolving through
module URIs are **not** intercepted, which is why the example also calls `addImportPath()` for its
own modules and appends to `QML_IMPORT_PATH` in CMake.

### Injecting new QML modules

`qt_add_qml_module(CustomModule URI Custom.Widgets ...)` and
`qt_add_qml_module(CustomPlanModule URI Custom.Plan ...)`
(`custom-example/CMakeLists.txt:157-205`), both `NO_PLUGIN` with `RESOURCE_PREFIX /qml`, exported
through `CUSTOM_LIBRARIES` and linked by `src/CMakeLists.txt:197-199`.

Example widgets: `CustomArtificialHorizon.qml`, `CustomAttitudeWidget.qml`,
`CustomIconButton.qml`, `CustomOnOffSwitch.qml`, `CustomQuickButton.qml`,
`CustomSignalStrength.qml`, `CustomToolBarButton.qml`, `CustomVehicleButton.qml`.

### Capability summary

| Can Aerora... | Answer | Mechanism |
| --- | --- | --- |
| Replace individual core QML files | **Yes** | `/Custom/qml` prefix + `QT_RESOURCE_ALIAS` + URL interceptor; per-file, existence-checked |
| Replace core non-QML resources | **Yes** | Same interceptor; `/Custom/res`, `/Custom/qmlimages` prefixes |
| Inject brand-new QML | **Yes** | Own `qt_add_qml_module` plus `addImportPath` |
| Override the palette | **Yes** | `QGCCorePlugin::paletteOverride()` — every colour, all four variants |
| Change branding (name, icons, installer art, Android package) | **Yes** | `CustomOverrides.cmake` cache `FORCE` plus resource aliasing |
| Change the splash screen | **No such QGC mechanism** | Platform launch image only |
| Add Analyze pages | **Yes** | `QGCCorePlugin::analyzePages()` |
| Add a new top-level view peer to FlyView/PlanView | **Partly** | Requires overriding `MainWindow.qml` via the interceptor, or replacing `createRootWindow()` |
| Add, remove, reorder settings pages | **Yes** | `SettingsPages.json` overlay with `insertAfter`/`insertBefore`/`remove`/name-match replace |
| Add settings groups | **Yes** | `registerCustomSettingsGroup()` plus metadata JSON plus `/json` resource |
| Hide or alter stock settings | **Yes** | `overrideSettingsGroupVisibility()`, `adjustSettingMetaData()` |
| Add toolbar indicators | **Yes** | `toolBarIndicators()` globally, or `FirmwarePlugin::toolIndicators()` per vehicle |
| Add FlightMap items | **Yes** | `QGCCorePlugin::customMapItems()` |
| Overlay the Fly View | **Yes** | Override `FlyViewCustomLayer.qml` — the purpose-built seam |
| Change tool strip and guided actions | **Yes** | Override the action list QML; gate buttons via `QGCFlyViewOptions` |
| Swap the instrument panel | **Yes** | `showInstrumentPanel()` false plus an Aerora widget, or add an `instrumentQmlFile2` enum value |
| Ship multiple customisations in one binary | **No** | `CUSTOMCLASS`/`CUSTOMHEADER` are compile-time; one custom plugin per build |

## Architectural observations for Aerora

- **Everything extensible is a flat list of QML file URLs resolved by a `Loader`**: toolbar
  indicators, analyze pages, settings pages, custom map items, instrument panel. A uniform,
  easy-to-extend pattern. It also means a bad URL fails silently at the `Loader` level — Aerora
  should add explicit verification, since there is no compile-time check.
- **No runtime plugin system.** All extension is compile-time (`CUSTOMCLASS`) plus resource-time
  (URL interceptor). An Aerora build is a fork-by-overlay-directory, not a loadable add-on. This is
  the governing delivery constraint.
- **FlyView and PlanView are never destroyed**, only hidden. State and both
  `PlanMasterController` objects persist across switches.
- `src/Settings/` and `src/AppSettings/` are deliberately separate: metadata defines *what a
  setting is*, `SettingsUI.json` defines *how it is laid out*, and the generator joins them at
  configure time. Aerora should preserve that separation.

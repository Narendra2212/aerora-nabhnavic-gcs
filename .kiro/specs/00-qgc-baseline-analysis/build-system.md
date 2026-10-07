# Build System

CMake-based, Qt6, C++20. No git submodules; third-party sources arrive via CPM at configure time.

## Toolchain requirements

Versions are centralised in `.github/build-config.json`, which is the single source of truth and
is schema-validated by `.github/build-config.schema.json`. `cmake/BuildConfig.cmake` reads it at
configure time. Query it with `python tools/setup/read_config.py --get qt.version`.

| Component | Required |
| --- | --- |
| Qt | 6.11.1 (minimum 6.11.0) |
| CMake | 3.25 minimum |
| C++ standard | C++20 |
| GStreamer | 1.28.4 (minimum 1.20.0), optional via `QGC_ENABLE_GST_VIDEOSTREAMING` |
| Android | platform 36, min SDK 28, NDK r27c, Java 21 |
| Apple | Xcode 26.x, macOS target 13.0, iOS target 17.0 |
| LLVM (analysis only) | 18 |

Required Qt components, from the root `find_package(Qt6 ...)` call: Graphs, Concurrent, Core, Gui,
HttpServer, LinguistTools, Location (+Private), Multimedia, Network, Positioning, Qml,
QmlIntegration, Quick, QuickControls2, QuickVectorImage, QuickWidgets, Sensors, Sql, Svg,
TextToSpeech, Xml, Quick3D, StateMachine. Optional: Bluetooth, MultimediaQuickPrivate, OpenGL,
QuickTest, SerialPort, Test; WaylandClient on Linux.

The `Qt6::HttpServer` dependency is worth noting for security review — see
[security-architecture.md](security-architecture.md).

## Root CMakeLists.txt order

Sequence matters, particularly for the vendor overlay. Line references are to the root
`CMakeLists.txt`.

```text
module paths (cmake, find-modules, install, install/CPack, modules, platform)
include(Helpers)
default CMAKE_BUILD_TYPE = Release
include(CustomOptions)                        ← defines every QGC_* option
QGC_CUSTOM_DIR validation                     ← lines 47-54, FATAL_ERROR on abuse
overlay detection + include(CustomOverrides)  ← lines 64-78, BEFORE project()
testing gate
Apple pre-project settings
include(Git)                                  ← version extraction
compiler caching (ccache/sccache)
project(${QGC_APP_NAME} VERSION ${QGC_APP_VERSION} ...)
CPM setup                                     ← CPM_SOURCE_CACHE = ${CMAKE_SOURCE_DIR}/.cache/CPM
Toolchain, PythonVenv, Python3, QGCQmlCodegen
find_package(Qt6 ...) + Qt policies QTP0001-0005
include(Hardening) + qgc_apply_global_hardening()
qt_add_executable(...)
Sanitizers, Coverage, warning flags
add_subdirectory(${QGC_CUSTOM_DIR})           ← line 402, before platform deployment
platform include (Windows / Apple / Android / Linux)
qt_add_qml_module(... URI QGC)                ← line 422
add_subdirectory(src)                         ← line 442
add_subdirectory(test), cmake/GStreamer/tests, cmake/tests   [when testing]
qgc-analysis-headers aggregate target
qt_add_translations(...)
qt_import_plugins(... EXCLUDE_BY_TYPE geoservices)
include(Install), include(PrintSummary)
```

The critical ordering fact: `CustomOverrides.cmake` is included **before** `project()`, which is
why a vendor overlay can change the project name and version, and before `add_subdirectory(src)`,
which is why it can override `QGC_MAVLINK_*` and any other cache variable the source tree reads.

## Target topology

Hybrid, not fully modular. There is one monolithic application target created by
`qt_add_executable`, and many `src/` subdirectories call `target_sources(${CMAKE_PROJECT_NAME}
PRIVATE ...)` directly onto it. Alongside that, numerous static libraries exist, each paired with
a QML module.

Static library + QML module targets observed: `QGroundControlModule`, `AnalyzeViewModule`,
`AppSettingsModule`, `AutoPilotPluginsCommonModule`, `AutoPilotPluginsAPMModule`,
`AutoPilotPluginsPX4Module`, `FactControlsModule`, `FirstRunPromptDialogsModule`, `FlyViewModule`,
`FlightMapModule`, `GeoMapModule`, `PlanViewModule`, `QGroundControlControlsModule`,
`ToolbarModule`, `VehicleSetupModule`, `Viewer3DModule`, `GeoTagModule`, `LogViewerModule`,
`PX4FirmwareModule`, `APMFirmwareModule`, `GPSNTRIPModule`, `QGCLogging`, `QGCLogManager`,
`QGCCompression`, `QGCGeoFormats`, `QGCFileSystem`, `QGCSensors`, `QGCAudio`, `QGCLocation`.

Resources are attached to the executable rather than to a library, deliberately, to avoid needing
`Q_INIT_RESOURCE()`.

Three targets are marked `OPTIMIZE_DEPENDENCIES ON` because they are free of QML modules,
resources, and static-initialisation registration: `QGCFileSystem`, `QGCSensors`, `QGCAudio`
(`src/CMakeLists.txt`).

## QGC_* options

From `cmake/CustomOptions.cmake`. Any of these can be forced by a vendor overlay.

### Overlay and metadata

| Option | Default |
| --- | --- |
| `QGC_CUSTOM_DIR` | `custom` |
| `QGC_APP_NAME` | `QGroundControl` |
| `QGC_APP_COPYRIGHT` | `Copyright (c) <year> QGroundControl. All rights reserved.` |
| `QGC_APP_DESCRIPTION` | `Open Source Ground Control App` |
| `QGC_ORG_NAME` | `QGroundControl` |
| `QGC_ORG_DOMAIN` | `qgroundcontrol.com` |
| `QGC_PACKAGE_NAME` | `org.mavlink.qgroundcontrol` |
| `QGC_SETTINGS_VERSION` | `9` — increment to invalidate stored settings |

### Build behaviour

| Option | Default | Controls |
| --- | --- | --- |
| `BUILD_SHARED_LIBS` | OFF | Shared vs static libraries |
| `QGC_STABLE_BUILD` | OFF | OFF implies a `QGC_DAILY_BUILD` define |
| `QGC_USE_CACHE` | ON | ccache/sccache compiler caching |
| `QGC_USE_MOCCACHE` | ON | Content-addressed moc cache |
| `QGC_UNITY_BUILD` | OFF | Unity builds |
| `QGC_BUILD_INSTALLER` | ON | Platform installers |
| `QGC_ENABLE_WERROR` | ON | Warnings as errors for QGC sources |
| `QGC_BUILD_TESTING` | ON for Debug/multi-config | Unit tests; also sets `QGC_UNITTEST_BUILD` |
| `QGC_BUILD_PORTABLE_TESTS` | OFF | Standalone portable utility test executables |
| `QGC_DEBUG_QML` | Debug default | QML debugging/profiling |
| `QT_QML_NO_CACHEGEN` | Debug default | Skip qmlcachegen |
| `QGC_ENABLE_COVERAGE` | OFF | Coverage instrumentation |
| `QGC_ENABLE_CLANG_TIDY` | OFF | clang-tidy during build |
| `QGC_TIME_TRACE` | OFF | Per-TU Clang `-ftime-trace` JSON |
| `QGC_SPLIT_DWARF` | OFF | Split DWARF (Linux/Android ELF) |
| `QGC_LINK_PARALLEL_LEVEL` | 2 | Parallel link jobs, limits LTO memory use |
| `QGC_VERBOSE_CONFIGURE` | OFF | Context-labelled configure messages |
| `GIT_SUBMODULE` | OFF | Submodule update (moot — none exist) |

### Dependency policy

| Option | Default | Effect |
| --- | --- | --- |
| `QGC_USE_SYSTEM_LIBS` | OFF | Sets `CPM_USE_LOCAL_PACKAGES` — `find_package` first, CPM fallback |
| `QGC_SYSTEM_LIBS_ONLY` | OFF | Sets `CPM_LOCAL_PACKAGES_ONLY` — misses are hard configure errors |

Per-package `FIND_PACKAGE_ARGUMENTS` wiring is incremental; `geographiclib` is the reference
conversion. Unconverted packages error under `QGC_SYSTEM_LIBS_ONLY`, and packages that require
the CPM source tree (the `qgc_require_cpm_added` call sites) fail either way.

### Protocol, comms, media

| Option | Default |
| --- | --- |
| `QGC_MAVLINK_GIT_REPO` | `https://github.com/mavlink/mavlink.git` |
| `QGC_MAVLINK_GIT_TAG` | `c409cf690454db6d3e004bd14173bc6c7ff1e0ff` |
| `QGC_MAVLINK_DIALECT` | `all` |
| `QGC_MAVLINK_VERSION` | `2.0` |
| `QGC_NO_SERIAL_LINK` | OFF — forced ON for iOS in `src/CMakeLists.txt` |
| `QGC_ENABLE_GST_VIDEOSTREAMING` | ON |
| `QGC_ENABLE_BZIP2` | OFF |
| `QGC_ENABLE_LZ4` | OFF |
| `GStreamer_REQUIRE_CHECKSUM` | ON — fails closed if a download checksum cannot be verified |
| `GStreamer_DEBUG` | OFF |
| `QGC_GST_DOWNLOAD_TIMEOUT` | 1200 s |
| `QGC_GST_DOWNLOAD_INACTIVITY_TIMEOUT` | 60 s |

The four `QGC_MAVLINK_*` variables are the mechanism by which Aerora would introduce a custom
dialect. See [mavlink-architecture.md](mavlink-architecture.md).

### Autopilot factories

| Option | Default |
| --- | --- |
| `QGC_DISABLE_APM_PLUGIN_FACTORY` | OFF |
| `QGC_DISABLE_PX4_PLUGIN_FACTORY` | OFF |

`custom-example/cmake/CustomOverrides.cmake` forces both ON and registers its own PX4-derived
factory. All firmware plugin code still compiles; the UI adapts at runtime via
`FirmwarePluginManager::supportedFirmwareClasses()`.

### Quality thresholds

| Option | Default |
| --- | --- |
| `QGC_COVERAGE_LINE_THRESHOLD` | 30 |
| `QGC_COVERAGE_BRANCH_THRESHOLD` | 20 |
| `QGC_VALGRIND_TIMEOUT_MULTIPLIER` | 20 |

### Test harness

From `cmake/QGCTest.cmake`.

| Option | Default |
| --- | --- |
| `QGC_TEST_PARALLEL_LEVEL` | logical core count |
| `QGC_TEST_TIMEOUT_UNIT` | 60 |
| `QGC_TEST_TIMEOUT_INTEGRATION` | 120 |
| `QGC_TEST_TIMEOUT_SLOW` | 180 |
| `QGC_TEST_TIMEOUT_DEFAULT` | 90 |
| `QGC_TEST_ONSCREEN` | OFF |
| `QGC_TEST_DETECT_LEAKS` | OFF |
| `QGC_TEST_FLAKY_REPEAT` | 3 |

## cmake/ modules

| Module | Purpose |
| --- | --- |
| `BuildConfig.cmake` | Reads `.github/build-config.json` |
| `CompilerWarnings.cmake` | `qgc_apply_warning_flags()` |
| `CustomOptions.cmake` | All `QGC_*` option definitions |
| `Helpers.cmake` | `qgc_set_qt_resource_alias`, `qgc_configure_target`, `qgc_require_cpm_added` (line 506) |
| `QGCTest.cmake` | `add_qgc_test()` and the `check-*` targets |
| `Toolchain.cmake` | Compiler/toolchain setup |
| `PrintSummary.cmake` | Configure summary |
| `modules/CPM.cmake` | Vendored CPM |
| `modules/CPMPatchCache.cmake` | `qgc_cpm_patch_cache_key()` — hashes patch contents into the cache key |
| `modules/CPMTrackHead.cmake` | Floating-branch package support |
| `modules/Hardening.cmake` | `qgc_apply_global_hardening()` |
| `modules/Sanitizers.cmake` | ASan/UBSan/TSan/MSan |
| `modules/Coverage.cmake`, `CleanCoverage.cmake` | Coverage targets |
| `modules/Git.cmake` | Version extraction from git describe |
| `modules/PythonVenv.cmake` | Locked Python environment integration |
| `modules/QGCQmlCodegen.cmake` | `qgc_add_qml_codegen()` — drives the settings page generator |
| `modules/AndroidOpenSSL.cmake` | Checksum-verified OpenSSL archive |
| `modules/Download.cmake`, `IWYU.cmake`, `LinuxDistro.cmake`, `TestSupport.cmake` | Supporting |
| `GStreamer/` | Dedicated subsystem: Orchestrator, Probe, PkgConfig, Download, Components, Link, Layout, Install, PluginPolicy, Json, Helpers, `platform/`, `tests/` |
| `install/` | `Install.cmake`, `SignMacBundle.cmake`, `CPack/` |
| `platform/` | Windows, Apple, Android, Linux specifics |
| `presets/` | Preset fragments included by `CMakePresets.json` |
| `tests/` | CMake-level tests, including the `QGC_CUSTOM_DIR` path-escape test at `cmake/tests/CMakeLists.txt:46-50` |

## Presets

`CMakePresets.json` is a thin aggregator, version 6, requiring CMake 3.25+. It includes six
fragments from `cmake/presets/`: `common.json`, `Android.json`, `iOS.json`, `Linux.json`,
`macOS.json`, `Windows.json`.

Visible configure presets from `common.json`: `default` (Debug, Ninja, `build/`),
`default-release`, `default-relwithdebinfo`, `default-minsizerel`. Hidden building blocks:
`debug`, `debug-no-tests`, `release`, `relwithdebinfo`, `minsizerel`, `coverage`, `ccache`,
`sccache`.

Platform presets: `Linux`, `Linux-debug`, `Linux-relwithdebinfo`, `Linux-coverage`,
`Linux-arm64`, `Linux-arm64-debug`, `Linux-deb`, `Linux-rpm`, `Windows`, `Windows-debug`,
`Windows-arm64`, `Windows-arm64-debug`, `macOS`, `macOS-debug`, `Android`, `Android-debug`,
`iOS`, `iOS-debug`, `iOS-appstore`.

The single test preset `default` inherits `test-defaults`: include labels `Unit|Integration`,
exclude `Flaky|Network`, `noTestsAction: error`, output on failure.

## Developer workflow

The `justfile` is canonical. Recipes relevant to any later Aerora phase:

```bash
just configure     # CMake configure through the matching default* preset
just build         # incremental build, all cores (override with JOBS=N)
just test          # ctest, LABELS="Unit|Integration" EXCLUDE="Flaky|Network"
just lint          # pre-commit run --all-files
just check         # lint + test — the definition-of-done gate
just format-fix    # apply clang-format / ruff-format
just info          # print resolved Qt / CMake / GStreamer versions
```

CI invocation conventions are documented in `.github/ci-overview.md`. Match CI rather than
guessing: CI runs `ctest --output-on-failure -L Unit`, and integration tests with
`--parallel 2`.

## Code generation

Four generators run at build or configure time. All are Python, executed through the locked
`tools/.venv` environment.

| Generator | Output | Driven from |
| --- | --- | --- |
| `tools/generators/mavlink_enums.py` | `MAVLinkEnums.h`, `MAVLinkEnumsQml.{h,cc}` | `src/MAVLink/CMakeLists.txt:65-97` |
| `tools/generators/mavlink_instance_fields.py` | `MAVLinkInstanceFields.h` | `src/MAVLink/CMakeLists.txt:99-118` |
| `tools/generators/settings_qml/generate_pages.py` | Settings page QML + `SettingsPagesModel.qml` | `src/AppSettings/CMakeLists.txt` |
| mavgen (upstream, inside the CPM package) | All MAVLink message headers | `src/MAVLink/CMakeLists.txt:21-29` |

The settings page generator is notable because it accepts `--custom-pages-dir` and
`--custom-settings-dir`, giving a vendor overlay declarative control over the settings UI. See
[ui-qml-architecture.md](ui-qml-architecture.md).

## Aerora implications

- Aerora does **not** need to fork the build system. `QGC_CUSTOM_DIR` plus a
  `cmake/CustomOverrides.cmake` is the sanctioned entry point, and it is exercised by CI
  (`.github/workflows/custom-build.yml` configures with `-DQGC_CUSTOM_DIR=custom-example`).
- `QGC_SETTINGS_VERSION` must be considered whenever Aerora changes persisted settings shape.
- Build reproducibility has two gaps, raised in [risk-register.md](risk-register.md): the
  `ArduPilotParams` CPM package tracks branch `main` rather than a pinned commit, and
  `qtandroidextensions` uses the moving tag `qt6-beta1`.

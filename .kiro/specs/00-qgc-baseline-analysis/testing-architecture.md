# Testing Architecture

The test framework, label scheme, simulation infrastructure, and what Aerora inherits.

## Headline finding

The test infrastructure is mature and directly reusable. Roughly 325 to 330 tests are registered.
MockLink provides a full synthetic autopilot, so Aerora can test vehicle, mission, parameter, and
link behaviour with no hardware. The one structural quirk Aerora must understand: **tests are
compiled into the application binary**, not into a separate test executable.

## Layout

`test/` mirrors `src/`.

```text
test/
├── UnitTestFramework/
│   ├── UnitTest.{h,cc}           base class, labels, registration macros, wait helpers
│   ├── MultiSignalSpy.{h,cc}     multi-signal assertion helper
│   ├── AllocationTracker         allocation tracking
│   ├── PropertyTestHelper.h      Q_PROPERTY round-trip helper
│   ├── UnitTestCoords.h          coordinate comparison helpers
│   ├── UnitTestMath.h            numeric comparison helpers
│   ├── UnitTestTileGenerator     map tile fixtures
│   ├── BaseClasses/              VehicleTest, CommsTest, MissionTest, ...
│   ├── Fixtures/
│   ├── Benchmarking/
│   ├── PropertyTesting/
│   ├── QmlTesting/
│   └── Tests/
├── UnitTestList.{h,cc}           runner and dispatcher
├── LibraryBoundaryChecks.cmake
├── README.md
├── ADSB/  AnalyzeView/  AutoPilotPlugins/  Camera/  Comms/  FactSystem/  FlyView/
├── FollowMe/  Fuzz/  GeoMap/  Gimbal/  GPS/  Joystick/  MAVLink/  MissionManager/
├── Portable/  QmlControls/  QmlUITests/  QtLocationPlugin/  Settings/  Terrain/
└── Utilities/  Vehicle/  VideoManager/  Viewer3D/
```

`UnitTestList.{h,cc}` is the runner: name validation, label filtering, iteration, and a
"lightweight" mode that runs on a bare `QCoreApplication`.

## Tests live in the application binary

`test/CMakeLists.txt` does `target_sources(${CMAKE_PROJECT_NAME} PRIVATE UnitTestList.cc ...)`,
links `Qt6::Test`, and precompiles `<QtTest/QTest>`, `<UnitTest.h>`, `<MultiSignalSpy.h>`.

Consequently `add_qgc_test()` registers:

```cmake
add_test(NAME <t> COMMAND $<TARGET_FILE:${CMAKE_PROJECT_NAME}> --unittest:<t> --allow-multiple)
```

There is no central list of tests to maintain. `main.cc`, `QGCApplication`,
`QGCCommandLineParser`, and `UnitTestList` are all generic and enumerate tests at runtime via
`UnitTest::registeredTests()`.

**Aerora consequence.** Aerora tests added through the overlay will be compiled into the Aerora
binary and discovered automatically, provided they are registered with the macro and added to
CMake. The CI custom-build job already runs the custom-plugin unit and integration suites.

## UnitTest base class

`test/UnitTestFramework/UnitTest.h`, `class UnitTest : public QObject` at roughly line 262.
Provides application lifecycle, temporary-file management, and signal-wait helpers
(`waitForSignal`, `waitForNoSignal`, `waitForSignalCount`).

### Base class hierarchy

Pick the narrowest class that provides the fixtures needed; each builds on the last.

| Class | Extends | Use for |
| --- | --- | --- |
| `UnitTest` | — | Basic tests, no special setup |
| `JsonTest` | `UnitTest` | Parsing and validating JSON documents |
| `StateMachineTest` | `UnitTest` | Driving a `QStateMachine`-based controller |
| `CommsTest` | `UnitTest` | Requires LinkManager or MockLink |
| `TerrainTest` | `UnitTest` | Requires TerrainQuery |
| `VehicleTest` | `UnitTest` | Requires a connected Vehicle |
| `VehicleTestManualConnect` | `VehicleTest` | Connects explicitly mid-test |
| `VehicleTestNoInitialConnect` | `VehicleTest` | Skips the default auto-connect |
| `VehicleTestAPM` | `VehicleTest` | ArduPilot-flavoured MockLink |
| `ParameterTest` | `VehicleTest` | Involves ParameterManager |
| `MissionTest` | `VehicleTest` | MissionController on a live Vehicle |
| `OfflineMissionTest` | `UnitTest` | Mission tests without a connected Vehicle |
| `PortableTest` | — | Standalone Qt Test executables, no QGCApplication |

### Registration macros

`UnitTest.h:73-91`:

```cpp
#define UT_REGISTER_TEST(className, ...) \
    static UnitTestWrapper<className> s_##className##_registration(#className, false, {__VA_ARGS__});

#define UT_REGISTER_TEST_STANDALONE(className, ...)   // only runs when explicitly requested

#define UT_REGISTER_TEST_LIGHTWEIGHT(className, ...)  // opts into the bare-QCoreApplication harness
```

The lightweight variant is for pure-logic tests needing no QML engine, vehicle, or plugin scan.
In the default full-application run it behaves like any other test, so it is always safe to use.

### TestLabel enum

`UnitTest.h`, a `Q_DECLARE_FLAGS` bitfield:

```text
None, Unit, Integration, Vehicle, MissionManager, Comms, Utilities,
Slow, Network, Serial, Joystick, AnalyzeView, Terrain
```

With conversion helpers `labelFromString`, `labelToString`, `parseLabels`, `labelsToString`,
`availableLabelNames`.

Note from `test/README.md`: the CTest `LABELS` set in CMakeLists files is a **superset**. Some
tests carry CTest-only names such as `MAVLink` and `Camera` that have no corresponding `TestLabel`
enum value. The two lists are independent.

### Assertion and wait macros

```cpp
QVERIFY_SIGNAL_WAIT(spy, timeoutMs);               // spy.count() >= 1
QVERIFY_SIGNAL_COUNT_WAIT(spy, expectedCount, timeoutMs);
QVERIFY_NO_SIGNAL_WAIT(spy, timeoutMs);
QVERIFY_TRUE_WAIT(condition, timeoutMs);           // QTRY_VERIFY_WITH_TIMEOUT wrapper
QCOMPARE_TRUE_WAIT(actual, expected, timeoutMs);   // QTRY_COMPARE_WITH_TIMEOUT wrapper
UT_PARAMETERIZED_TEST(testName);                   // declares testName() and testName##_data()
```

These wrappers exist because they emit readable failure messages including the signal or condition
name and elapsed time.

### Timeout helpers

`TestTimeout` namespace. These auto-scale roughly two-fold on CI so the same test works on a
loaded build agent without slowing local runs:

| Helper | Local | CI | Use for |
| --- | --- | --- | --- |
| `TestTimeout::shortMs()` | 1 s | 2 s | Quick signals, in-memory operations |
| `TestTimeout::mediumMs()` | 5 s | 10 s | Normal async work, file I/O |
| `TestTimeout::longMs()` | 30 s | 60 s | Vehicle connect, FTP, metadata |

Chrono-typed variants also exist: `shortDuration()`, `mediumDuration()`, `longDuration()`.

**`QVERIFY_NO_SIGNAL_WAIT` always burns the full timeout**, because proving absence requires
waiting the entire window. Keep it tight and place it after a `QVERIFY_TRUE_WAIT` that already
proves state has settled.

## add_qgc_test

`cmake/QGCTest.cmake`, function at roughly line 160.

```cmake
add_qgc_test(<name>
    [SERIAL]
    [TIMEOUT <seconds>]
    [LABELS <label>...]
    [RESOURCE_LOCK <resource>...]
    [SKIP_REGEX <regex>...]
    [ENV_MODIFICATION <mod>...])
```

Validation: name charset `^[A-Za-z0-9_.+-]+$`, rejects unknown arguments, rejects missing values,
rejects `SERIAL` combined with `RESOURCE_LOCK`, requires `TIMEOUT` to be a non-negative integer.

### Timeout selection

Explicit `TIMEOUT` wins. Otherwise, in order:

| Condition | Timeout |
| --- | --- |
| `Slow` in `LABELS` | `QGC_TEST_TIMEOUT_SLOW` = 180 s |
| `Integration` in `LABELS` | `QGC_TEST_TIMEOUT_INTEGRATION` = 120 s |
| `Unit` in `LABELS` | `QGC_TEST_TIMEOUT_UNIT` = 60 s |
| None of the above | `QGC_TEST_TIMEOUT_DEFAULT` = 90 s |

Sanitizer builds scale these up substantially — `test/CMakeLists.txt` raises them to 180, 360,
540, and 270 respectively — to absorb instrumentation overhead.

### Environment

| Variable | Value | Condition |
| --- | --- | --- |
| `QT_LOGGING_RULES` | `*.debug=false` | Always |
| `QT_QPA_PLATFORM` | `offscreen` | Unless `QGC_TEST_ONSCREEN` |
| `QT_QUICK_BACKEND` | `software` | Unless `QGC_TEST_ONSCREEN` |
| `LIBGL_ALWAYS_SOFTWARE` | `1` | Unless `QGC_TEST_ONSCREEN` — forces the GLX path onto Mesa llvmpipe, no GPU needed |
| `ASAN_OPTIONS` | `detect_leaks=0` | Under ASan unless `QGC_TEST_DETECT_LEAKS`. The comment explains that the LeakSanitizer tracer needs ptrace, which Yama blocks on most hosts |
| `TMPDIR` (plus `TMP`/`TEMP` on Windows) | `${CMAKE_BINARY_DIR}/test-tmp/<name>` | Always — per-test isolated temp directory |

The per-test temp directory replaced a shared `TempFiles` resource lock and needs no C++ change,
because `QTemporaryDir` and `QStandardPaths` honour it. `add_qgc_test` strips `TempFiles` from any
supplied `RESOURCE_LOCK` so those tests parallelise; genuinely serial locks — `MockLink`,
`Joystick`, `Settings`, fixed ports, `PlanViewSettings` — are preserved.

Other properties set: `FAIL_REGULAR_EXPRESSION "FAIL!;Segmentation fault"`, `LABELS`, optional
`SKIP_REGULAR_EXPRESSION`, and either `RUN_SERIAL` or `RESOURCE_LOCK`.

## Label reference

Only three labels set a timeout. Everything else is a filterable category that combines with one of
them.

| Label | Meaning |
| --- | --- |
| `Unit` | Fast, isolated — 60 s |
| `Integration` | Multiple components — 120 s |
| `Slow` | Long-running — 180 s, overrides the other two |
| `Vehicle` | Requires a MockLink-connected Vehicle |
| `MissionManager` | Mission planning |
| `Comms` | Communication and link |
| `Utilities` | Utility classes |
| `Network` | Needs network; excluded from pull-request suites, covered by manual Linux coverage runs |
| `Flaky` | Intermittent; excluded from pull-request suites, covered by manual Linux coverage runs |
| `Serial` | Must run alone — set automatically by `SERIAL` |
| `Joystick` | Joystick and controller |
| `AnalyzeView` | Log analysis and geo-tagging |
| `Terrain` | Terrain query and tiles |
| `NoSanitizer` | Incompatible with ASan/UBSan; excluded from sanitizer CI |
| `Portable` | Standalone portable utility executables |

## Convenience targets

```bash
ninja check              # all tests
ninja check-unit         # -L Unit
ninja check-integration  # -L Integration, --parallel 1
ninja check-fast         # -LE Slow
ninja check-ci           # -LE "Flaky|Network"
ninja check-flaky        # --repeat until-fail:3, -LE Network
```

Category targets, generated in a loop over `MissionManager Vehicle Utilities MAVLink Comms`:
`check-missionmanager`, `check-vehicle`, `check-utilities`, `check-mavlink`, `check-comms`.

All `check-*` targets depend on the main executable.

## Running tests

```bash
ctest --parallel $(nproc)                # all, parallel
ctest --output-on-failure -L Unit        # what CI runs
ctest -L Integration                     # integration
ctest -LE "Flaky|Network"                # exclude
ctest -R "Camera|Mission"                # name regex
ctest --rerun-failed
```

Via `just`, which matches the CI label filters by default:

```bash
just test                              # LABELS="Unit|Integration" EXCLUDE="Flaky|Network"
LABELS=Unit just test
EXCLUDE=Network just test
```

Via the binary directly:

```bash
./QGroundControl --list-tests
./QGroundControl --list-tests --label=Unit,Utilities
./QGroundControl --unittest:MyTest
./QGroundControl --unittest --label=Unit
./QGroundControl --unittest:MyTest --unittest-output:results.xml   # JUnit XML
./QGroundControl --unittest:MyTest --unittest-stress:50
```

To match CI for integration tests locally:

```bash
ctest --test-dir build -L Integration -LE 'Flaky|Network' --output-on-failure --parallel 2
```

CI runs integration tests with two processes. Settings and temporary directories are isolated per
test, and local network fixtures allocate ephemeral ports. **Aerora must preserve this isolation
when adding integration tests.**

## Test counts

Measured in this checkout:

| Metric | Count |
| --- | --- |
| `UT_REGISTER_TEST*` occurrences across `test/` and `src/` | 331 |
| `add_qgc_test(` call sites in CMakeLists files | 327 |

The 331 figure includes the `_STANDALONE` and `_LIGHTWEIGHT` variants because the search matched
the common prefix. So roughly 325 to 330 CTest-registered tests.

## MockLink as test infrastructure

MockLink is **production code**, not test-only — `src/Comms/MockLink/` — but it is Debug-build
gated. Full architecture in [communication-architecture.md](communication-architecture.md).

Components: `MockLink`, `MockLinkWorker`, `MockLinkFTP`, `MockLinkCamera`, `MockLinkGimbal`,
`MockLinkMissionItemHandler`, `MockLinkPX4Calibration`, plus `MockVideoStreamServer`.

It simulates parameter download, the mission protocol, MAVLink FTP, camera protocol v2, the gimbal
manager protocol, and PX4 calibration flows. Vehicle, mission, and camera tests therefore run
against a synthetic autopilot with no hardware.

### The shared harness

`test/UnitTestFramework/BaseClasses/VehicleTest.cc:132-169`, `VehicleTest::_connectMockLink`:

1. Assert no existing `_mockLink`.
2. Create a `QSignalSpy` on `MultiVehicleManager::activeVehicleChanged`.
3. Switch on `MAV_AUTOPILOT` to pick the factory; `MAV_AUTOPILOT_INVALID` selects
   `startNoInitialConnectMockLink`.
4. Connect `QObject::destroyed` to null the pointer.
5. Wait for `activeVehicleChanged` with `TestTimeout::longMs()`.
6. Grab `MultiVehicleManager::instance()->activeVehicle()`.
7. Call `waitForInitialConnect()`.

`_disconnectMockLink()` spies the same signal, calls `_mockLink->disconnect()`, and asserts the
active vehicle becomes null.

Usage is broad: `test/Vehicle/InitialConnectTest.cc`, `test/Comms/MAVLinkV1TrafficTest.cc`,
`test/AnalyzeView/OnboardLogDownloadTest.cc`,
`test/MissionManager/PlanMasterControllerTest.cc`,
`test/Gimbal/GimbalControllerTestBase.cc`, `test/FollowMe/FollowMeTest.cc`, and the QML UI tests,
which use a `runWithMockLink([]{ return MockLink::startPX4MockLink(); }, ...)` lambda form.

### Fault injection

The reason MockLink matters to Aerora. Available controls include `setCommLost(bool)`,
`simulateConnectionRemoved()`, `setRequestMessageFailureMode()`,
`setRequestMessageNoResponse(msgId)`, `setParamSetFailureMode()` including
`FailParamSetFirstAttemptNoAck`, `setParamRequestReadFailureMode()`, `setHashCheckNoResponse()`,
`setMissionItemFailureMode()`, `sendUnexpectedMissionAck/Item/Request()`,
`setDuplicateResponses(bool)` which sends every vehicle-to-QGC message twice as when two links
receive the same traffic, `_stayMavlinkV1`, and APM calibration failure modes.

Verification counters: `receivedMavCommandCount()`, `receivedRequestMessageCount()`,
`receivedMavlinkMessageCount()`, `lastReceivedMavlinkMessage()`, `paramRequestReadIndexLog()`,
`paramValue()`.

Custom test commands are mapped onto `MAV_CMD_USER_1` through `MAV_CMD_USER_5`, for example
`MAV_CMD_MOCKLINK_ALWAYS_RESULT_ACCEPTED` and `MAV_CMD_MOCKLINK_NO_RESPONSE`. **Aerora can use the
same `MAV_CMD_USER_*` approach for its own mock commands.**

## Strict log checking

Every test runs with strict log checking: **any log message not explicitly expected or suppressed
causes `QFAIL`**. `QTest::ignoreMessage` is banned by the `check-no-qtest-ignore-message`
pre-commit hook. The base-class helpers are:

```cpp
// Preferred: one-shot expectation that also asserts the message was actually emitted
expectLogMessage("category", QtWarningMsg, QRegularExpression(QStringLiteral("pattern")));
somethingThatLogs();
verifyExpectedLogMessage();

// Last resort: persistent suppression that does NOT assert the message fires
ignoreLogMessage("category", QtWarningMsg, QRegularExpression(QStringLiteral("pattern")));
```

`expectLogMessage(...)` **must** be followed by `verifyExpectedLogMessage()`; omitting it fails the
test in `cleanup()`. Always prefer the pairing, because it both silences the strict check and
asserts the message was emitted, so a regression that removes the log breaks the test.

Also relevant: `qCCritical` fails unit tests when hit, per `CODING_STYLE.md`. It is reserved for
coding errors.

## Test quality standards

From `test/README.md`, and binding on Aerora contributions:

- One behaviour per test, one reason to fail. If two tests fail for the same cause, merge them.
- Consolidate permutations into data-driven tests using `_data()` with `QTest::addColumn` and
  `QFETCH`, rather than copy-pasted methods. Canonical shape:
  `Utilities/Compression/QGCCompressionTest.cc`.
- Test behaviour, not implementation. Assert observable outputs, not private state or call order.
- Prioritise edge cases and failure paths over repeated happy-path variations.
- Do not test Qt or the standard library.
- A test that cannot fail is noise. No tautologies, no asserting the mock.

### Forbidden patterns

| Pattern | Why | Enforced by |
| --- | --- | --- |
| `QTest::qWait(<non-zero>)` | Unconditional sleep, no early exit | `check-no-fixed-qwait` pre-commit hook |
| `while (!done) { QTest::qWait(20); }` | Use `QTRY_VERIFY_WITH_TIMEOUT` or `waitForCondition` | Convention |
| `QTest::ignoreMessage` | Does not assert the message fires | `check-no-qtest-ignore-message` hook |
| Literal millisecond timeouts | Does not scale on CI | Convention; add a comment if unavoidable |

## MultiSignalSpy

```cpp
MultiSignalSpy spy;
spy.init(myObject);

QVERIFY(spy.emittedOnce("valueChanged"));
QVERIFY(spy.onlyEmittedOnce("valueChanged"));
QVERIFY(spy.waitForSignal("valueChanged", TestTimeout::mediumMs()));
QVERIFY(spy.notEmitted("errorOccurred"));
spy.clearAllSignals();

// Fluent expectation API
QVERIFY(spy.expect("signal").once());
QVERIFY(spy.expect("signal").atLeastOnce());
QVERIFY(spy.expect("signal").never());
QVERIFY(spy.expect("signal").times(3));

int value = spy.argument<int>("valueChanged");
QVERIFY(spy.emittedOnce("signal1", "signal2"));   // each exactly once
```

Note from `test/README.md`: prefer `MultiSignalSpy::waitForSignal()` over raw
`QSignalSpy::wait()` for flaky signals.

## QML tests

Two separate entry points with different purposes:

| Entry point | Purpose |
| --- | --- |
| `QmlQuickTests` | **Recommended for real QML testing.** Runs `tst_*.qml` via a dedicated Qt Quick Test executable `QGCQmlQuickTests`, out of process |
| `QmlTestFileValidator` | Sanity and structure check. Runs inside the regular `UnitTest` harness and validates test discovery and setup; not the primary Qt Quick execution path |

```bash
ctest -R QmlQuickTests --output-on-failure
ctest -R QmlTestFileValidator --output-on-failure
```

`test/QmlUITests/` boots the real QML UI against MockLink vehicles.

## Portable utility tests

Enabled by `QGC_BUILD_PORTABLE_TESTS`. Six `Portable.*` CTest entries use small Qt Test
executables with the same test bodies and production utility libraries as the Linux application
tests, requiring no QGCApplication, vehicles, or QML engine.

```bash
cmake -S . -B build -DQGC_BUILD_PORTABLE_TESTS=ON
cmake --build build --target portable-tests --parallel 8
ctest --test-dir build --build-config Debug --output-on-failure -L Portable
```

Use `PortableTest` and `QGC_REGISTER_PORTABLE_TEST` for these suites. Per
`.github/ci-overview.md`, Windows x64 and the macOS Release leg enable them alongside the normal
application build; the macOS Debug leg runs the Unit and Integration suites instead.

## Coverage

```bash
just coverage
```

Wraps `tools/coverage.py`, which uses a dedicated `build-coverage/` directory by default.
Requires `gcovr`.

Direct CMake targets:

```bash
cmake -B build -DQGC_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target coverage          # run tests + XML + HTML
cmake --build build --target coverage-report   # from existing data
cmake --build build --target coverage-check    # enforce the floor
cmake --build build --target coverage-clean
```

Thresholds: `QGC_COVERAGE_LINE_THRESHOLD` 30, `QGC_COVERAGE_BRANCH_THRESHOLD` 20. Verified in CI by
`.github/scripts/verify_coverage_thresholds.py`.

Coverage retains the compiler launcher — ccache stores `.gcno` files alongside objects. GCC
coverage uses atomic counter updates for worker threads, and the known GCC negative-counter
workaround remains enabled and reports any discarded counters.

## Sanitizers

```bash
# AddressSanitizer
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DQGC_BUILD_TESTING=ON -DQGC_ENABLE_ASAN=ON

# UndefinedBehaviorSanitizer
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DQGC_BUILD_TESTING=ON -DQGC_ENABLE_UBSAN=ON

# Combined — the recommended default sanitizer lane
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DQGC_BUILD_TESTING=ON -DQGC_ENABLE_ASAN=ON -DQGC_ENABLE_UBSAN=ON

# ThreadSanitizer — run separately
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DQGC_BUILD_TESTING=ON -DQGC_ENABLE_TSAN=ON
```

Given the per-link worker-thread architecture documented in
[communication-architecture.md](communication-architecture.md), **ThreadSanitizer is the
appropriate gate for any Aerora transport work.**

## Additional test infrastructure

| Item | Location |
| --- | --- |
| Fuzzing | `.clusterfuzzlite/` plus `test/Fuzz/`. PR runs use the bundled seed corpus with `NO_CLUSTERFUZZ_DEPLOYMENT=true`, which also disables previous-build crash comparison, so reproducible crashes fail the PR regardless of whether they predate it |
| CMake-level tests | `cmake/tests/`, including the `QGC_CUSTOM_DIR` path-escape test and `CMake.ApplicationAutogenGraph` (Ninja, non-custom builds) |
| GStreamer CMake tests | `cmake/GStreamer/tests/` |
| Property testing | `rapidcheck` via `cmake/modules/TestSupport.cmake` |
| Benchmarking | `nanobench` via the same module |
| Test-only dependencies | `fixposition-sdk`, `eigen`, `minmea` for GPS protocol comparison |
| Library boundary checks | `test/LibraryBoundaryChecks.cmake` |

## Debugging failures

```bash
QGC_TEST_VERBOSE=1 ./QGroundControl --unittest:MyTest
./QGroundControl --unittest:MyTest -- -v2 _specificMethod
```

Common issues from `test/README.md`:

1. Test timeout — check for blocking operations, raise with `TIMEOUT`.
2. Resource conflicts — use `RESOURCE_LOCK` or `SERIAL`.
3. Flaky signals — use `MultiSignalSpy::waitForSignal()`.
4. Stale singletons — ensure proper cleanup in `cleanup()`.
5. Unexpected log messages — use `expectLogMessage` plus `verifyExpectedLogMessage`.
6. Expensive skip paths — cache a backend probe result in a test-class member so subsequent
   skipping tests do not each burn a full timeout. Pattern:
   `JoystickManagerTest::_multiJoysticksSupported()`.

## What Aerora inherits and what it must add

### Inherited directly

- The whole `UnitTest` hierarchy and registration mechanism.
- MockLink as a synthetic autopilot, including fault injection.
- The label and timeout scheme, CI-adaptive timeouts, strict log checking.
- Coverage, sanitizer, and fuzzing infrastructure.
- Automatic test discovery — no central list to maintain.

### Must be added by Aerora

`PROPOSED`, and expanded in the eventual `10-testing` specification:

| Area | Requirement |
| --- | --- |
| Aerora transport | A test double following the MockLink pattern, plus ThreadSanitizer coverage for the worker threads |
| Aerora dialect | Tests that Aerora messages parse, route, and reach the correct FactGroup |
| Painting pattern | Pattern-expansion tests asserting the exact emitted MAVLink sequence, plus `.plan` save and load round-trip |
| Mission resume | A test asserting Aerora payload state commands survive resume generation — this currently **would fail**, see [risk-register.md](risk-register.md) |
| Power and tether | State-machine tests for configuration reporting, once the power architecture is frozen |
| Authority model | Tests asserting the GCS never presents a proposed GPU or autonomy command as an effective aircraft command |

The last item deserves emphasis. It is a core Aerora principle from the product baseline, it is
testable, and no equivalent test exists in QGC because QGC has no such concept.

# PHASE 6 — FIRST BUILD HANDOFF

Owner: **Tab 1 (integration authority)**. Recipient: **Tab 8 (build host / CI)**.

This document is documentation only. It introduces no source change. The frozen source baseline is
unmodified and unamended.

## Frozen revision

| Field | Value |
| --- | --- |
| Commit | `738462422e7579fd5102cffac53e625707a72340` |
| Branch | `aerora/phase6-integration-checkpoint` |
| Parent | `9459a4b57986f40df8bdc193d5453c0596fbb416` (unmodified upstream QGC 5.2.0-dev) |
| Origin | `https://github.com/mavlink/qgroundcontrol.git` — unchanged, **nothing pushed** |
| Working-tree state | **clean** (`git status --porcelain` → 0 lines) |

The frozen baseline must not be amended, reset, cleaned, rebased or force-pushed. Any defect found
during compilation is fixed in a **new** commit, after the compiler evidence is recorded.

## Required Windows host

| Requirement | Value |
| --- | --- |
| OS | Windows 10/11 x64 |
| Compiler | MSVC 2022 (`Microsoft.VisualStudio.Workload.VCTools --includeRecommended`) |
| SDK | Windows SDK |
| CMake | ≥ 3.25 (`CMakeLists.txt:5`) |
| Generator | Ninja |
| Qt | **6.11.1**, `win64_msvc2022_64` |
| GStreamer | **1.28.4** MSVC x86_64, to `C:\gstreamer\1.0\msvc_x86_64` |
| Git | any recent |
| Python | 3.12 |
| Free disk | **≥ 50 GB** |

Qt modules, exactly as pinned in `.github/build-config.json`:

```text
qtgraphs qtlocation qtpositioning qtspeech qtmultimedia qtserialport qtimageformats
qtshadertools qtconnectivity qtquick3d qtsensors qtscxml qtwebsockets qthttpserver
```

Minimum Qt is 6.11.0; GStreamer minimum is 1.20.0, default 1.28.4. No dependency beyond this list
is required, and none has been added here.

Host sizing note: `QGC_ENABLE_WERROR` defaults **ON**, so warnings fail the build. Cap parallelism at
`--parallel 4` on 4-core / 8 GB hosts.

## Experiment A — upstream baseline

Proves stock QGC builds before any Aerora code is involved. Run first, and change no Aerora code
while it runs.

```powershell
git checkout 9459a4b57986f40df8bdc193d5453c0596fbb416

cmake --preset Windows-debug
cmake --build --preset Windows-debug
ctest --preset Windows-debug
```

Record:

```text
Configure:
Build:
CTest:
Tests discovered:
Tests passed:
Tests failed:
Warnings:
Errors:
Build time:
Disk consumption:
```

## Experiment B — Aerora checkpoint

Only after Experiment A succeeds.

```powershell
git checkout aerora/phase6-integration-checkpoint

cmake --preset Aerora-Windows-debug
cmake --build --preset Aerora-Windows-debug
ctest --preset Aerora-Windows-debug
```

`Aerora-Windows-debug` sets `QGC_CUSTOM_DIR=aerora` through the hidden `aerora-overlay` fragment and
inherits `Windows-debug`, so `QGC_BUILD_TESTING=ON`. Its binary directory is
`build/Aerora-Windows-debug`, separate from the stock `build/Windows-debug`, so the two cannot
collide. `Aerora-Linux-debug` is the equivalent Linux leg.

**Capture the complete first failure surface before patching anything.** Do not fix the first
compiler error on sight.

Classify every failure:

```text
A — environment/toolchain
B — upstream QGC
C — Aerora CMake
D — Aerora compilation
E — linker
F — runtime
G — unit/integration test
H — QML
```

Then group by root cause and fix causes, not symptoms. After each logical fix: configure, build,
test, record evidence.

## What is expected to be exercised for the first time

16 test targets have never been compiled: 14 Aerora overlay tests plus `MavCommandAuthorizationTest`
and `MAVLinkSigningKeyStoreTest` in the core test tree. Also first-time: the `qt_add_qml_module`
call for `AeroraControlsModule`, resolution of the `Aerora.Controls` import, and the
`friend class AeroraVehicleFactGroupRegistrar` / `AERORA_VEHICLE_FACTGROUP_FRIENDSHIP` pair
compiling together.

## Ownership matrix

| Area | Owner | State |
| --- | --- | --- |
| Build host, CI, toolchain provisioning | **Tab 8** | **GO** |
| Build-failure triage, assignment | **Tab 1** | active |
| CMake aggregation, presets, QML module target | **Tab 1** | `VERIFIED` (static) |
| `HO-TAB7-02` engine-override installation | **Tab 1** | `IMPLEMENTATION-MISSING`, runtime-only |
| Security policy — `Q-029`, `Q-037`/`Q4-003`, ADR-014, platform key protection | **Tab 2** | `OPEN` |
| Vehicle — lifecycle assertion inversion, `Q-018`, `Q4-004` | **Tab 3** | `OPEN` |
| Telemetry — `Q-036`, `IF-MAV-022`, `README.md` MD060 | **Tab 4** | `OPEN` / doc-only |
| Mission — `Q-021`, `Q-023` | **Tab 5** | `OPEN` |
| Payload/Power — arbitration, transitions, thresholds, `Q-024` | **Tab 6** | `OPEN` |
| UI foundation — `Q7-001`..`Q7-004`, `HO-TAB7-01` | **Tab 7** | `OPEN` |
| `PARAM_SET` / `src/FactSystem` ownership | **Programme** | `OPEN — UNASSIGNED` |
| `D-005` Aerora remote, `D-007` §27 | **Programme / external** | `BLOCKED` |

Tab 1 is no longer the primary implementation owner for the remaining architecture questions.

## Issues intentionally deferred

Tracked, not to be implemented because the first build is pending:

```text
Q-029   Q-037 / Q4-003   ADR-014 outbound scope   Q4-004   Q-018   Q-007
Q3-009  Q-036            Q-021 / Q-023 / Q-024    fleet identity
capability identifiers   PARAM_SET ownership      HO-TAB7-01   HO-TAB7-02
D-005   D-007
```

Also deferred, and explicitly not to be touched during first-build debugging: Aerora MAVLink
dialect, payload FactGroups, power FactGroups, radio, product UI, autonomy, FC/GPU protocol, power
arbitration, payload actuator commands, role model, signing security architecture.

`aerora/src/Telemetry/README.md` MD060 (lines 358, 443) remains **DOC-ONLY, NON-BLOCKING**.

### Stale lifecycle assertions — do not silently fix

`aerora/test/Vehicle/AeroraVehicleLifecycleTest.cc` carries two assertions with
**MUST-BE-INVERTED** markers, asserting that no Aerora fact group reaches the vehicle map. Telemetry
publication is now wired (`AeroraFirmwarePlugin.cc:166`), so these are stale in intent — though they
may still pass, because no concrete Aerora fact group exists while `Q-023`/`Q-024` are open.

**Tab 3 must confirm the expected post-publication behaviour before they are changed.** If
confirmed, the correction is test-only, minimal, and goes in a new commit with its rationale
recorded. They are retained meanwhile because they are the only guard against
`FirmwarePlugin::factGroups()` being wired by accident, which would reintroduce `R4-001`.

## Build-failure triage protocol

Tab 1 is the single triage authority. No two tabs may independently patch one root cause.

For each failure, Tab 1 produces:

```text
Failure ID
Exact error
Root cause
Affected subsystem
Owning tab
Minimal fix
Regression risk
Required verification
```

Rules:

- group by root cause before assigning — one cause may produce dozens of errors;
- assign the smallest possible fix to the one owning tab;
- no architectural optimisation during first-build debugging;
- fix only defects demonstrated by CMake configure, the compiler, the linker, MOC/UIC/RCC, QML
  compilation, CTest or runtime;
- every fix lands in a new commit, never an amendment of the frozen baseline.

## Current status

```text
Architecture/design       ~90%
Source implementation     ~70%
Static integration        VERIFIED
Checkpoint                FROZEN
Configure                 NOT EXECUTED
Compilation               BLOCKED BY HOST
CTest                     BLOCKED
Runtime                   NOT-EVIDENCED
Execution verification    0%
```

Milestone sequence, none of which may be reported as achieved without execution evidence:

```text
FIRST CLEAN CONFIGURE -> FIRST SUCCESSFUL COMPILE -> FIRST CTEST RUN -> FIRST RUNTIME/QML RUN
```

Local environment remains ~3.28 GB free with no complete MSVC/CMake/Ninja/Qt/GStreamer toolchain:
**LOCAL BUILD = BLOCKED**, **PROVISIONED BUILD HOST = GO**. Nothing is to be installed on, or
deleted from, the current machine.

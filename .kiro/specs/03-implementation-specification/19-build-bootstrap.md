# Build Bootstrap

The measured host, the exact dependency set, the scripted sequence, and **F3.1-02** answered for the
half that source evidence can answer.

## Status: BLOCKED

| Step | State |
| --- | --- |
| Host inventory | **Complete** |
| Dependency identification | **Complete** |
| Installation | **Not attempted** |
| Baseline QGC build | **Not performed** |
| Repository bootstrap | **Specified, not created** |
| Overlay structural proof | **Specified, not created** |
| Build evidence | **None. Nothing is claimed** |

## Host inventory

Measured, not assumed.

| Property | Value |
| --- | --- |
| Operating system | Windows 10 Pro, build 19045 |
| Shell | PowerShell 5.1 |
| Architecture | AMD64 |
| Logical cores | **4** |
| Physical memory | **7.9 GB** |
| Fixed drives | **One** — `C:`, 111.1 GB total |
| **Free space** | **1.00 GB** |
| **Administrator** | **No** |

### Tool inventory

| Tool | State |
| --- | --- |
| `cmake` | **Absent** |
| `ninja` | **Absent** |
| `cl` — MSVC | **Absent** |
| `gcc`, `clang`, `clang-cl` | **Absent** |
| `msbuild` | **Absent** |
| Visual Studio / Build Tools | **Absent** — no `vswhere.exe`, no `Microsoft Visual Studio` directory |
| Windows SDK | **Absent** — no `Windows Kits\10\Include` |
| `qmake`, `qmake6`, any Qt | **Absent** — nothing at `C:\Qt`, `C:\Qt6` or `%USERPROFILE%\Qt` |
| GStreamer | **Absent** — no `gst-launch-1.0` |
| `just` | **Absent** |
| `pre-commit` | **Absent** |
| `choco` | **Absent** |
| `python` | Present — 3.12 |
| `uv` | Present |
| `pip` | Present |
| `node` | Present |
| `git` | Present |
| `java` | Present — JDK 25. **Not required for a Windows desktop build**; Android only |
| `winget` | Present |
| `build/` directory | **Does not exist** |

## Required dependency set

From `.github/build-config.json` and `tools/setup/install_dependencies/_windows.py` — the
repository's own authority, not a guess.

| Component | Required | Source | Admin? | Approx. footprint |
| --- | --- | --- | --- | --- |
| MSVC 2022 Build Tools, `Microsoft.VisualStudio.Workload.VCTools --includeRecommended` | Yes | `https://aka.ms/vs/17/release/vs_BuildTools.exe` | **Yes** | 6–8 GB |
| Windows SDK | Yes, via `--includeRecommended` | Same installer | **Yes** | Included above |
| Qt **6.11.1**, `win64_msvc2022_64`, 14 modules | Yes | `tools/setup/install_qt.py` via `aqtinstall` | No | 4–6 GB |
| CMake **≥ 3.25** | Yes | User-local archive or `winget` | No | ~0.1 GB |
| Ninja | Yes | User-local archive | No | <0.01 GB |
| GStreamer **1.28.4** devel | Yes for video | Repository installer, to `C:\gstreamer\1.0\msvc_x86_64` | **Yes** | 1–2 GB |
| CPM dependency source cache | Yes | Fetched at configure time to `.cache/CPM` | No | 1–2 GB |
| Debug build tree | Yes | Generated | No | 10–20 GB |
| NSIS | Installer builds only | Optional | **Yes** | ~0.05 GB |
| Vulkan SDK | Optional | Optional | **Yes** | ~1 GB |
| Java / Android SDK / NDK | **Not required** | — | — | — |
| **Total minimum** | — | — | — | **~25–40 GB** |

### Qt module set

```text
qtgraphs qtlocation qtpositioning qtspeech qtmultimedia qtserialport
qtimageformats qtshadertools qtconnectivity qtquick3d qtsensors
qtscxml qtwebsockets qthttpserver
```

`qtserialport` is the module ADR-002's transport strategy depends on. `qthttpserver` is linked for
two type aliases with no listener — Q-027, R-016 as amended.

### Version constraints

| Constraint | Value | Evidence |
| --- | --- | --- |
| CMake minimum | 3.25 | `CMakeLists.txt:5`; `build.cmake_minimum_version` |
| C++ standard | **C++20, required** | `cmake/Toolchain.cmake:17-18` |
| Qt supported range | 6.11.0 to **6.11.1 inclusive** | `QGC_QT_MAXIMUM_VERSION` is set from `qt.version` |
| GStreamer minimum | 1.20.0, default 1.28.4 | `gstreamer.version.*` |

The Qt range is narrow by construction: the maximum equals the pinned version, so a newer Qt is
**rejected rather than tried**. Qt upgrades are a deliberate act.

## The two blockers

| # | Blocker | Measured | Needed | Can Phase 4 resolve it? |
| --- | --- | --- | --- | --- |
| 1 | **Disk** | 1.00 GB free, one drive | ~40 GB | **No.** Requires the user to free space or attach a drive |
| 2 | **Elevation** | Not administrator | Admin for MSVC and GStreamer | **No.** Requires an elevated session |

No installation was attempted. Two reasons, both deliberate:

| Reason | Detail |
| --- | --- |
| Filling the last gigabyte of a system drive risks destabilising Windows | The paging file and Windows Update both need headroom on `C:` |
| A part-completed MSVC installation is worse than none | It leaves a registered-but-broken toolchain that `vswhere` reports as present |

## The preflight gate, executable

`.kiro/tools/aerora-preflight.ps1` turns the 28-criterion gate into something runnable.
Non-product tooling, authorised under directive §27. **Read-only by default — it installs nothing.**

```text
powershell -NoProfile -ExecutionPolicy Bypass -File .kiro\tools\aerora-preflight.ps1
```

| Property | Value |
| --- | --- |
| Checks | Free disk, elevation, cores, memory, CMake ≥ 3.25, Ninja, MSVC via `vswhere`, Qt exactly 6.11.1, GStreamer, python, git, node, HEAD against the pinned commit, `git diff` cleanliness, and whether `origin` pushes to upstream |
| Output | A per-criterion table plus `VERDICT: GO` or `VERDICT: NO-GO` |
| Exit code | `0` go, `1` no-go, `2` usage error |
| `-InstallUserTools` | Installs **only** the admin-free Qt step, via the repository's own `tools/setup/install_qt.py`. **Refuses if less than 10 GB is free** |
| Never does | Install MSVC or GStreamer, create the overlay, or modify any QGroundControl file |

### Result on this host

Run at the time of writing:

```text
pass 6   fail 8   warn 1
VERDICT: NO-GO  (8 blocking criteria failed)
```

| Passing | Failing |
| --- | --- |
| CPU cores, python, git, node, HEAD at the pinned commit, QGC source unmodified | Free disk, elevation, CMake, Ninja, MSVC, Qt, GStreamer, `origin` push target |
| Warning | Memory at 7.9 GB — cap build parallelism, R4-004 |

The script is the first executable artefact in the programme and the only one Phase 4 produced. It
is deliberately a **gate**, not an installer: it refuses rather than guesses, and it reports the
`origin`-points-at-upstream hazard (R-008) alongside the toolchain gaps.

## The scripted bootstrap

Written so it runs unattended once the two blockers clear. **Not executed.**

### Phase B1 — admin-free, user-local

```text
1  Qt 6.11.1
   python tools/setup/install_qt.py --version 6.11.1 --host windows
          --arch win64_msvc2022_64 --modules "<the 14 modules above>"

2  CMake >= 3.25 and Ninja
   user-local archives on PATH, or winget user-scope

3  Verify
   cmake --version      expect >= 3.25
   ninja --version
   qmake -query QT_VERSION   expect 6.11.1
```

### Phase B2 — requires elevation

```text
4  MSVC Build Tools
   python -m tools.setup.install_dependencies --msvc

5  GStreamer 1.28.4 devel
   python -m tools.setup.install_dependencies --gstreamer-version 1.28.4

6  Verify
   vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64
   gst-launch-1.0 --version
```

The repository installer verifies MSVC itself via `vswhere` after installing, and sets
`GSTREAMER_1_0_ROOT_MSVC_X86_64` plus `PATH` for GStreamer.

### Phase B3 — baseline build, unmodified upstream

```text
7  Confirm the tree is unmodified
   git rev-parse HEAD        expect 9459a4b57986f40df8bdc193d5453c0596fbb416
   git diff --stat HEAD      expect empty

8  Configure
   just configure            → tools/configure.py --preset default -B build -t Debug

9  Build
   just build                → cmake --build --preset default --parallel 4

10 Run the test suite
   just test                 → ctest, labels Unit|Integration, excluding Flaky|Network
```

`--parallel 4`, not more: 4 logical cores and 7.9 GB of memory. A higher job count on this host will
thrash rather than build faster, and MSVC with a Qt translation unit is memory-hungry.

### Required build record

To be filled with measurements, **not** estimates. Currently empty because nothing ran.

| Field | Value |
| --- | --- |
| Compiler version | **Not measured** |
| Qt version | **Not measured** |
| CMake version | **Not measured** |
| Ninja version | **Not measured** |
| GStreamer version | **Not measured** |
| Configure command | Specified above |
| Build command | Specified above |
| Output executable | **Not produced** |
| Build duration | **Not measured** |
| Warning count | **Not measured** |
| Error count | **Not measured** |
| Test result | **Not measured** |

Note `QGC_ENABLE_WERROR` defaults **ON** (`cmake/CustomOptions.cmake`), so a warning in Aerora code
is an error. That is the correct default and worth knowing before the first Aerora commit.

### Phase B4 — repository bootstrap

| Step | Action | Requirement |
| --- | --- | --- |
| 1 | Create the Aerora repository; push the pinned tree | `AERORA-BUILD-REQ-004` |
| 2 | Reconfigure `origin` to the Aerora repository | R-008 — **`origin` is currently upstream for both fetch and push** |
| 3 | Add `upstream` as a **fetch-only** remote | `AERORA-BUILD-REQ-004` |
| 4 | Record the fork point as `9459a4b5...` | `AERORA-BUILD-REQ-005` |
| 5 | Verify no submodules were introduced | No `.gitmodules` exists today |

Step 2 is the live hazard. The first implementation commit against the current configuration would
target upstream QGroundControl.

### Phase B5 — overlay structural proof

Eight minimal artefacts, each labelled `BOOTSTRAP`, proving the seven properties the directive lists.

| # | Property to prove | Artefact | How it is proven |
| --- | --- | --- | --- |
| 1 | `QGC_CUSTOM_DIR` is recognised | `aerora/CMakeLists.txt` + `aerora/cmake/CustomOverrides.cmake` | Configure log reports "Custom build directory detected" |
| 2 | The Aerora target is included | `CUSTOM_SOURCES` export | A deliberate compile error in a bootstrap file fails the build |
| 3 | A custom QML module loads | One Aerora QML module, one control | Startup self-check imports it |
| 4 | Plugin registration links | `AeroraFirmwarePluginFactory` as a non-local static | Self-check asserts the selected plugin is Aerora's |
| 5 | The application starts | — | Launch, offscreen, and exit cleanly |
| 6 | Upstream remains functional | — | `ctest -L Unit` passes identically to the baseline run |
| 7 | The overlay disables cleanly | `-DQGC_CUSTOM_DIR=` pointing at a non-existent directory | Configure succeeds and produces stock QGC |

Property 7 matters more than it sounds: an overlay that cannot be switched off cannot be bisected
against upstream when something breaks.

## F3.1-02 — Radio host presentation and transport extensibility

### The question, split in two

```text
QGC transport capability          ≠          Aerora radio hardware behaviour
   answerable from source                        requires a bench observation
```

### Part 1 — QGC transport capability: ANSWERED

All `GCS-VERIFIED`, read at the baseline commit.

| Inspection | Finding | Location |
| --- | --- | --- |
| `SerialLink` port open | `setPortName(_settings.portName)` then `open(QIODevice::ReadWrite)` | `src/Comms/Serial/SerialLink.cc:177-189` |
| **Any device whitelist?** | **No.** The only refusal is the bootloader flag | `.cc:180-183` |
| Baud handling | `setBaudRate(_settings.baud)` with full parameter set | `.cc:336-338` |
| Configurable parameters | baud, data bits, parity, stop bits, flow control, DTR | `.cc:36-119` |
| Modem-control lines | `UnsupportedOperationError` **tolerated** — the code notes "PTYs and some USB adapters have no modem-control lines" | `.cc:342-358` |
| Port selection | Any port name; discovery via `SerialPortManager::availablePorts()` | `.cc:116-119` |
| Persistence | Plain `QSettings` round-trip | `.cc:77-107` |
| SiK / radio detection | Board type from a JSON table, consumed by auto-connect | `QGCSerialPortInfo.cc:208`; `SerialAutoConnect.cc:91-99` |

> **Answer: yes. A plain USB-to-serial radio can be used by QGroundControl today with zero core
> edits, by configuring a serial link with a port name and a baud rate.**

Therefore **ADR-002's serial-first strategy requires no transport work at all**, provided the Aerora
radio enumerates as a serial device. `IF-RAD-002`, `IF-RAD-004` to `IF-RAD-009` are
`IMPLEMENTATION-PLANNED` as inherited behaviour.

### The misidentification risk, located precisely

| Element | Behaviour | Location |
| --- | --- | --- |
| Board type source | **A JSON resource**, not a C++ table: `":/json/USBBoardInfo.json"` | `QGCSerialPortInfo.cc:47` |
| Resource registration | `PREFIX "/json" FILES USBBoardInfo.json` | `src/Comms/Serial/CMakeLists.txt:4-7` |
| Match rule | `vendorId` equal **and** (`productId` equal **or** `productId == 0`) | `.cc:208` |
| Regular-expression fallbacks | Two further lists keyed on description and manufacturer | `.cc:127-168` |
| `boardClass` vocabulary | **Five fixed strings** — not extensible without a core edit | `.cc:179-185` |
| Consequence of a SiK match | `setSikRadio(true)` and **baud 57600** instead of 115200 | `SerialAutoConnect.cc:79-80` |
| Auto-connect gate | Per board class, from settings | `SerialAutoConnect.cc:91-99` |

A generic USB-serial bridge matching a SiK entry is auto-connected at the wrong baud and flagged as a
SiK radio.

### Mitigation, chosen

| Option | Assessment | Decision |
| --- | --- | --- |
| **Disable SiK auto-connect in the Aerora settings defaults** | Entirely ground-station-side, no identifier needed, no core edit | **Primary.** `IMPLEMENTATION-PLANNED` |
| A non-colliding VID/PID | Depends on hardware Aerora may not control | Secondary, not relied upon |
| Override `USBBoardInfo.json` from the overlay | The file is a **resource**, so this looks cheap — but it is a C++-read `:/json/` path that the URL interceptor does **not** cover, so it is a genuine alias collision | **Rejected** — Q4-001 |
| Add an Aerora `boardClass` | A core edit; the vocabulary is closed | **Rejected** |

The third option is a new Phase 4 finding and a near-miss: discovering the table is JSON suggested a
cheap override, and tracing the read path showed it would land in the one resource case Phase 3.1
could **not** settle.

### Part 2 — Aerora radio hardware behaviour: UNANSWERED

| Question | State |
| --- | --- |
| Does the Aerora radio enumerate as a standard serial device? | **`NOT-EVIDENCED`** — Q3-001 |
| What USB vendor and product identifiers does it present? | **`NOT-EVIDENCED`**. No identifier is invented here |
| Evidence required | **One bench observation**: attach the radio, record the enumerated device class and the reported VID/PID |
| Owner | Hardware |
| Gate | **`G-ARCH`** — the answer can invalidate ADR-002 |

`IF-RAD-001` and `IF-RAD-003` stay `CONTRACT-ONLY`, and `IF-RAD-001` stays **`BLOCKED`** for
implementation because implementing either branch would commit to an unverified hardware fact.

This remains the cheapest high-value evidence item in the programme: one connection, no firmware, no
dialect, no payload.

### If a non-serial transport were ever required

Quantified exactly, so the cost is known rather than feared.

| Change | Location | Core edit? |
| --- | --- | --- |
| Add a `LinkType` enumerator | `src/Comms/LinkConfiguration.h:104-117` — a closed enum ending in `TypeLast` | **Yes** |
| Add a `case` to `createSettings` | `src/Comms/LinkConfiguration.cc:56-68` | **Yes** |
| Add a `case` to `duplicateSettings` | `src/Comms/LinkConfiguration.cc:92-103` | **Yes** |
| Update `LinkManager::linkTypeStrings()` | Required by the header comment at `.h:103` | **Yes** |

**Minimum four core edits, and there is no registration or factory hook for a custom link type.**
That is R-003 quantified. It is also why the usual answer — "abstract the transport" — is not an
isolation strategy: the abstraction is the thing that needs the core edits.

## Interface status

| Interface | Subject | Status |
| --- | --- | --- |
| `IF-RAD-001` | Radio host presentation | **`BLOCKED`** — Q3-001, F3.1-02 |
| `IF-RAD-002` | Serial parameters | `IMPLEMENTATION-PLANNED` — inherited |
| `IF-RAD-003` | USB device identity | `CONTRACT-ONLY` — no identifier invented |
| `IF-RAD-004` | Byte stream | `IMPLEMENTATION-PLANNED` — inherited |
| `IF-RAD-005` to `IF-RAD-009` | Link lifecycle, loss, quality, arbitration mirror | `IMPLEMENTATION-PLANNED` |

## What the user must provide

| # | Item | Why |
| --- | --- | --- |
| 1 | **~40 GB free on `C:`**, or an additional drive | 1.00 GB is available. Nothing else can start |
| 2 | **An elevated session** | MSVC Build Tools and GStreamer both require it |
| 3 | An Aerora repository to push to | So `origin` stops pointing at upstream |
| 4 | *Optional:* the radio bench observation | Settles Q3-001 and ADR-002 independently of the build |

Item 4 is independent of items 1 to 3 and can be done in parallel by whoever holds the hardware. It
needs no ground station at all — any host that enumerates USB devices will do.

# Dependency Map

Every third-party dependency, how it is acquired, and the licensing exposure it creates.

## Acquisition model

Two mechanisms only.

| Mechanism | Used for |
| --- | --- |
| `find_package` | Qt6 — a system or separately installed SDK, version range from `.github/build-config.json` |
| CPM | Everything else, fetched at configure time |

There are **no git submodules** — no `.gitmodules` file exists. CPM is vendored at
`cmake/modules/CPM.cmake`, with a source cache at `${CMAKE_SOURCE_DIR}/.cache/CPM` or
`$CPM_SOURCE_CACHE`.

Two policy switches let packagers build against system libraries:

| Option | Effect |
| --- | --- |
| `QGC_USE_SYSTEM_LIBS` | Sets `CPM_USE_LOCAL_PACKAGES` — `find_package` first, CPM fallback |
| `QGC_SYSTEM_LIBS_ONLY` | Sets `CPM_LOCAL_PACKAGES_ONLY` — misses are hard configure errors |

Per-package `FIND_PACKAGE_ARGUMENTS` wiring is incremental; `geographiclib` is the reference
conversion. Unconverted packages error under `QGC_SYSTEM_LIBS_ONLY`, and packages needing the CPM
source tree — the `qgc_require_cpm_added()` call sites — fail either way.

Supporting modules: `cmake/modules/CPMPatchCache.cmake` provides `qgc_cpm_patch_cache_key()`,
which hashes patch file contents into the cache key because CPM's default key does not;
`cmake/modules/CPMTrackHead.cmake` supports floating-branch packages.

## Core dependencies — always built

| Package | Version / tag | Declared in |
| --- | --- | --- |
| mavlink | `${QGC_MAVLINK_GIT_REPO}` @ `c409cf690454db6d3e004bd14173bc6c7ff1e0ff`, dialect `all`, v2.0 | `src/MAVLink/CMakeLists.txt:46` |
| libevents (mavlink/libevents) | `840a88ea226d4eb0fd4c391ce860317422756435` | `src/MAVLink/LibEvents/CMakeLists.txt:24` |
| ZLIB (madler/zlib) | 1.3.2 / `v1.3.2` | `src/Utilities/Compression/CMakeLists.txt:48` |
| LibLZMA (tukaani-project/xz) | 5.8.3 / `v5.8.3` | `src/Utilities/Compression/CMakeLists.txt:79` |
| zstd (facebook/zstd) | 1.5.7 / `v1.5.7` | `src/Utilities/Compression/CMakeLists.txt:134` |
| LibArchive | 3.8.8 / `v3.8.8` | `src/Utilities/Compression/CMakeLists.txt:278` |
| Shapelib (OSGeo/shapelib) | 1.6.3 / `v1.6.3` | `src/Utilities/Geo/Formats/CMakeLists.txt:28` |
| geographiclib | 2.7 / `r2.7`, patched, `FIND_PACKAGE_ARGUMENTS "NAMES GeographicLib"` | `src/Utilities/Geo/GeographicLib.cmake:22` |
| libexif | 0.6.26 / `v0.6.26`, `DOWNLOAD_ONLY` | `src/Utilities/Parsing/CMakeLists.txt:29` |
| ulog_cpp (PX4) | `v1.0.1` | `src/Utilities/Parsing/CMakeLists.txt:114` |
| valijson | `v1.1.0` — **pinned deliberately**; 1.1.1 breaks AppleClang | `src/Utilities/Parsing/CMakeLists.txt:126` |
| SDL3 | 3.4.2 / `release-3.4.2` — **pinned deliberately**; 3.4.3+ breaks the vendored Java glue | `src/Utilities/SDL/CMakeLists.txt:46` |
| sdl_gamecontrollerdb | `513c72e34569e0f471dde7aa26eecb23946c3ef7`, `DOWNLOAD_ONLY` | `src/Utilities/SDL/CMakeLists.txt:131` |
| EXPAT (libexpat) | `R_2_7_5` | `src/Viewer3D/Providers/Osm/CMakeLists.txt:39` |
| protozero (mapbox) | `v1.8.1`, `DOWNLOAD_ONLY` | `src/Viewer3D/Providers/Osm/CMakeLists.txt:70` |
| libosmium | `v2.23.0`, `DOWNLOAD_ONLY` | `src/Viewer3D/Providers/Osm/CMakeLists.txt:81` |
| earcut.hpp (mapbox) | `f36ced7e50254738c4e5af1a239f5fb7b1094007` | `src/Viewer3D/Providers/Osm/CMakeLists.txt:117` |
| ArduPilotParams (ArduPilot/ParameterRepository) | **branch `main` — floating**, `GIT_SHALLOW`, `DOWNLOAD_ONLY` | `src/FirmwarePlugin/APM/CMakeLists.txt:41` |

## Optional dependencies

Gated by CMake options, both OFF by default.

| Package | Version | Option | Declared in |
| --- | --- | --- | --- |
| lz4 | 1.10.0 / `v1.10.0` | `QGC_ENABLE_LZ4` | `src/Utilities/Compression/CMakeLists.txt:191` |
| BZip2 | 1.0.8 / commit `66c46b8c9436613fd81bc5d03f63a61933a4dcc3` from the GitLab bzip2 repository | `QGC_ENABLE_BZIP2` | `src/Utilities/Compression/CMakeLists.txt:229` |

## Platform-specific dependencies

| Package | Version / reference | Platform | Declared in |
| --- | --- | --- | --- |
| VulkanHeaders (KhronosGroup) | `vulkan-sdk-1.4.341.0` | Windows only, when the Vulkan SDK is absent | `CMakeLists.txt:218` |
| qtandroidextensions (2gis) | **tag `qt6-beta1` — mutable**, `DOWNLOAD_ONLY` | Android | `src/Android/CMakeLists.txt:36` |
| QtAndroidTools (FalsinSoft) | `47ba0fcf75c91a4e3b01ee34f718bf59014892db`, `DOWNLOAD_ONLY` | Android | `src/Android/CMakeLists.txt:101` |
| android_openssl | Archive `URL` with `URL_HASH` | Android | `cmake/modules/AndroidOpenSSL.cmake:23` |
| gstreamer (SDK archive) | `VERSION ${GStreamer_FIND_VERSION}`, local file URL | Android | `cmake/GStreamer/platform/Android.cmake:42` |
| create-dmg | `v1.2.3`, `DOWNLOAD_ONLY` | macOS packaging | `cmake/install/Install.cmake:262` |

## Test-only dependencies

Built when `QGC_BUILD_TESTING` is set.

| Package | Version | Declared in |
| --- | --- | --- |
| nanobench (martinus) | 4.3.11 | `cmake/modules/TestSupport.cmake:8` |
| rapidcheck (emil-e) | `b96a4e626ef4c7348dcd16c500353c2f997a9f3f` | `cmake/modules/TestSupport.cmake:29` |
| fixposition-sdk | `200d20167551da4c1bb57b7e0eb2331e8f070f36`, `DOWNLOAD_ONLY` | `test/GPS/Driver/Protocols/Comparisons/Fixposition/CMakeLists.txt:10` |
| eigen (GitLab libeigen) | `3147391d946bb4b6c68edd901f2add6ac1f31f8c`, `DOWNLOAD_ONLY` | same file, line 19 |
| minmea (kosma) | `${_minmea_revision}`, `DOWNLOAD_ONLY` | `test/GPS/.../Minmea/CMakeLists.txt:7` |

## GStreamer

Not CPM-from-source. `cmake/GStreamer/` is a dedicated subsystem:

```text
cmake/GStreamer/
├── Orchestrator.cmake   Probe.cmake      PkgConfig.cmake
├── Download.cmake       Components.cmake Link.cmake
├── Layout.cmake         Install.cmake    PluginPolicy.cmake
├── Json.cmake           Helpers.cmake
├── platform/            (incl. Android.cmake)
└── tests/
```

Acquisition differs by platform. Per the comment in the GStreamer section of
`cmake/CustomOptions.cmake`, the SDK-download platforms are Android, macOS, iOS, and Windows, while
**Linux resolves GStreamer through system pkg-config and never downloads**.

Downloads are checksum-verified by default — `GStreamer_REQUIRE_CHECKSUM` is ON and the comment
states it can be set OFF to bypass — with configurable wall-clock and inactivity timeouts
(`QGC_GST_DOWNLOAD_TIMEOUT` 1200 s, `QGC_GST_DOWNLOAD_INACTIVITY_TIMEOUT` 60 s).

Enabled by `QGC_ENABLE_GST_VIDEOSTREAMING`, ON by default. **Aerora can disable the entire video
backend** with a single option if the painting UAV does not need GStreamer video, which would
remove a large LGPL surface and simplify packaging. `PROPOSED`, recorded in
[open-questions.md](open-questions.md).

## Qt

Acquired via `find_package`, not CPM. Version 6.11.1 required, minimum 6.11.0, with the range from
`.github/build-config.json`.

Required components: Graphs, Concurrent, Core, Gui, HttpServer, LinguistTools, Location
(including Private), Multimedia, Network, Positioning, Qml, QmlIntegration, Quick, QuickControls2,
QuickVectorImage, QuickWidgets, Sensors, Sql, Svg, TextToSpeech, Xml, Quick3D, StateMachine.

Optional: Bluetooth, MultimediaQuickPrivate, OpenGL, QuickTest, SerialPort, Test; WaylandClient on
Linux.

Qt's own geoservices plugins are deliberately excluded —
`qt_import_plugins(... EXCLUDE_BY_TYPE geoservices)` — because QGC ships its own QtLocation plugin
in `src/QtLocationPlugin/`.

## Vendored source with its own licensing

Two places where third-party code lives inside the tree rather than being fetched:

| Path | Licence | Note |
| --- | --- | --- |
| `src/Android/qtandroidserialport/` | `LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only` per SPDX header | Vendored Qt code |
| `src/VideoManager/.../GstDmaFourcc.h:14` | Notes the code is modelled on Qt's LGPL-3 `qgstvideobuffer.cpp` | Derived work |

These are the only `SPDX-License-Identifier` hits in `src/**`. QGC's own sources carry **no
per-file SPDX header convention**, so licence identification relies on the root licence files.

## Licensing

### QGroundControl itself

Files in this checkout: `LICENSE-APACHE` (Apache License 2.0 full text), `LICENSE-GPL`
(GNU GPL v3, 29 June 2007, full text), `.github/COPYING.md`,
`docs/en/qgc-dev-guide/contribute/licences.md`.

`.github/COPYING.md` states that QGroundControl is dual-licensed under Apache 2.0 and GPL v3 and
that either may be chosen. It describes Apache 2.0 as the permissive path allowing use in
proprietary applications and mobile app stores, and states that path **requires a commercial Qt
licence**. GPL v3 is described as strong copyleft, compatible with the open-source build of Qt.

Contributions must be compatible with **both**. Acceptable inbound licences are original work,
BSD-2-Clause, BSD-3-Clause, MIT, or Apache 2.0. GPL-only code cannot be accepted. The licensing
contact is named in that file.

`docs/en/qgc-dev-guide/contribute/licences.md` repeats the dual-licence statement, notes that users
of the codebase are free to use it under either licence, observes that the GPL grant permits a
later version of the licence while contributions must be under 3.0, and has a separate section
covering documentation, artwork, and images.

### Obligations for Aerora

Stated strictly as what those files say. **This is not legal advice, and Aerora must obtain its
own.**

| Path | Obligation per the licence files |
| --- | --- |
| **GPL v3** | Convey the complete corresponding source of the modified work under GPL v3 with the same licence and notices. Requires an LGPL or GPL-compatible Qt build |
| **Apache 2.0** | Modifications may remain proprietary. `COPYING.md` states this path **requires a commercial Qt licence** |

Either way, third-party components remain governed by their own licences. Qt itself is LGPLv3,
GPL, or commercial. GStreamer is LGPL, which is directly relevant to the dynamic-plugin layout
under `cmake/GStreamer/`. Every CPM dependency listed above carries its own terms.

**The upstream licence texts of the individual CPM dependencies were not read in this checkout.**
Each must be verified from its own source tree before any Aerora distribution. A representative
indication, not a verified claim: zlib under the zlib licence, xz, zstd under BSD or GPLv2 dual,
libarchive under BSD, libexif under LGPL, SDL3 under zlib, EXPAT under MIT, libosmium and
protozero under BSD, GeographicLib under MIT, shapelib under MIT or LGPL. **Treat that list as
`UNKNOWN` until each is confirmed.**

### Branding and trademark

`QGC_APP_NAME`, `QGC_ORG_NAME`, and the icon paths are explicitly designed to be overridden by an
overlay, so re-branding is a supported build operation. However, trademark and artwork terms are
covered separately in `licences.md` under its documentation, artwork, and images section — the code
licence does **not** automatically grant the QGroundControl marks or artwork.

Aerora must remove or replace QGC branding assets rather than relying on the code licence. The
overlay mechanism makes this straightforward; the obligation is to actually do it.

## Risks arising from dependencies

Escalated to [risk-register.md](risk-register.md).

| Risk | Detail |
| --- | --- |
| Floating `ArduPilotParams` reference | Branch `main`, so two builds of the same QGC commit can embed different content. Defeats build reproducibility and makes any SBOM a point-in-time claim |
| Mutable `qtandroidextensions` tag | `qt6-beta1` can be moved upstream |
| Licence inventory incomplete | Per-dependency licence texts not verified in this analysis |
| Qt licensing decision outstanding | The Apache path requires a commercial Qt licence; the GPL path requires releasing Aerora source. This is a **commercial and legal decision that gates the entire delivery model** and is the single highest-impact open item |
| Configure-time network dependency | CPM fetches at configure time, so a clean build requires network access unless the CPM source cache is pre-populated. Relevant to any air-gapped or reproducible build requirement |

## Aerora dependency additions

`PROPOSED`. Aerora will add its own dependencies, and they inherit the same obligations.

| Likely area | Consideration |
| --- | --- |
| AWS SDK or API client | Licence, size, and the constraint that it must never sit on the flight-control path |
| Aerora MAVLink dialect fork | An Aerora-controlled mavlink fork, pinned to a commit, not a branch |
| Any cryptographic library beyond Qt | Must be justified; Qt already provides PBKDF2 via `QPasswordDigestor` |
| Any new transport library (LoRa, radio) | Licence and platform availability; prefer Qt-native where possible |

Aerora should adopt the same discipline observed here: pin to immutable references, prefer
checksum-verified archives, and record every addition in a maintained inventory. The two floating
references found in upstream are the counter-example to avoid.

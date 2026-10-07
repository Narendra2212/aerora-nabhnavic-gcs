# QGroundControl Extension Map

Every planned integration point, with the file, class, method and signature as read in the checkout
at `9459a4b57986f40df8bdc193d5453c0596fbb416`. **Nothing here was modified.**

All rows are `GCS-VERIFIED`. Line numbers are 1-indexed and were read, not recalled.

## Overlay activation

| Element | Location | Behaviour |
| --- | --- | --- |
| `QGC_CUSTOM_DIR` cache variable | `cmake/CustomOptions.cmake:11-14` | Default `"custom"`, a path relative to the source root |
| Path validation | `CMakeLists.txt:47-54` | `FATAL_ERROR` on empty, absolute, or `..`-containing values |
| Contract enforcement | `CMakeLists.txt:65-72` | Requires **both** `<overlay>/CMakeLists.txt` and `<overlay>/cmake/CustomOverrides.cmake`, else `FATAL_ERROR` |
| Activation flags | `CMakeLists.txt:73-75` | Sets `QGC_CUSTOM_BUILD ON`, appends `<overlay>/cmake` to `CMAKE_MODULE_PATH`, includes `CustomOverrides.cmake` **before** `project()` |
| Overlay subdirectory | `CMakeLists.txt:402-404` | `add_subdirectory("${QGC_CUSTOM_DIR}")` under `if(QGC_CUSTOM_BUILD)` |
| Overlay sources into the app target | `src/CMakeLists.txt:207-208` | `if(CUSTOM_SOURCES)` / `target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${CUSTOM_SOURCES})` |
| Overlay export side | `custom-example/CMakeLists.txt:199-215` | Exports `CUSTOM_SOURCES ... PARENT_SCOPE` |
| Compile-policy pass | `CMakeLists.txt:483-486` | `qgc_configure_directory_targets` over the overlay tree |

`CustomOverrides.cmake` being included **before** `project()` is what lets it `FORCE`-override cache
variables. That is the only place Aerora can change a build default without editing core CMake.

## Firmware plugin factory

### What a subclass must implement

`src/FirmwarePlugin/FirmwarePluginFactory.h:9-28`. **Two pure virtuals, not more:**

```cpp
virtual FirmwarePlugin *firmwarePluginForAutopilot(MAV_AUTOPILOT autopilotType,
                                                   MAV_TYPE vehicleType) = 0;   // :21
virtual QList<QGCMAVLink::FirmwareClass_t> supportedFirmwareClasses() const = 0; // :24
virtual QList<QGCMAVLink::VehicleClass_t> supportedVehicleClasses() const        // :27  optional
    { return QGCMAVLink::allVehicleClasses(); }
```

There is no `factGroups()` hook on the factory, and no `name()`.

### Registration is a static constructor

| Element | Location |
| --- | --- |
| Self-registration | `src/FirmwarePlugin/FirmwarePluginFactory.cc:16` — the base constructor calls `FirmwarePluginFactoryRegister::instance()->registerPluginFactory(this)` |
| Registry | `FirmwarePluginFactory.h:32-46`, singleton via `Q_GLOBAL_STATIC` at `.cc:26` |
| Storage | `QList<FirmwarePluginFactory*> _factoryList`, append-only, `.h:45` |
| Overlay pattern | `custom-example/src/FirmwarePlugin/CustomFirmwarePluginFactory.cc:4` defines a non-local static; `.h:27` declares it `extern` so the translation unit is not dropped |

There is **no** deregistration, no priority, and no duplicate-class detection.

### Selection is first-match

`src/FirmwarePlugin/FirmwarePluginManager.cc:91-102`:

```cpp
for (FirmwarePluginFactory *factory: factoryList) {
    if (factory->supportedFirmwareClasses().contains(firmwareClass)) {
        return factory;          // first match wins
    }
}
```

| Consequence | Detail |
| --- | --- |
| Two factories claiming one class | **First registered wins, silently** |
| Which registers first | Static-initialisation order across translation units — unspecified |
| No warning is emitted | This is R-010's exact mechanism |
| Fallback | `.cc:84-87` lazily creates a shared generic `FirmwarePlugin` when no factory matches |

### The factory-disable options

| Option | Defined | Consumed |
| --- | --- | --- |
| `QGC_DISABLE_APM_PLUGIN_FACTORY` | `cmake/CustomOptions.cmake:187` | `src/FirmwarePlugin/APM/CMakeLists.txt:1` |
| `QGC_DISABLE_PX4_PLUGIN_FACTORY` | `cmake/CustomOptions.cmake:190` | `src/FirmwarePlugin/PX4/CMakeLists.txt:1` |

Each guard wraps **only the factory translation unit**; the plugin implementations are always
compiled. The effect is that the static object never exists, so the factory never registers. The
reference overlay forces both at `custom-example/cmake/CustomOverrides.cmake:43,46`.

### Where Vehicle selects a plugin

| Call site | Location |
| --- | --- |
| Primary | `src/Vehicle/Vehicle.cc:229`, inside `Vehicle::_commonInit(LinkInterface*)` |
| Offline editing | `src/Vehicle/Vehicle.cc:483`, after `_firmwareType` is set |

`MultiVehicleManager` plays no part in plugin selection.

## Fact groups

### The attachment hook

`src/FirmwarePlugin/FirmwarePlugin.h:353`:

```cpp
virtual QMap<QString, FactGroup*> *factGroups() { return nullptr; }
```

Consumed at `src/Vehicle/Vehicle.cc:364-370`, immediately after the fixed `_addFactGroup` block at
`:344-362`.

The only in-tree override is `src/FirmwarePlugin/APM/ArduSubFirmwarePlugin.h:98` /
`.cc:281-284`, returning `&_nameToFactGroupMap` — **a member of the plugin**.

> **Hazard found in Phase 4.** The plugin is a shared singleton; the map it returns is a plugin
> member; the groups are inserted into **each** `Vehicle`'s map. Two vehicles of the same firmware
> class therefore share the same `FactGroup` objects, and both dispatch loops write into them.
> Detail and mitigation in [06-factgroup-implementation.md](06-factgroup-implementation.md).

### FactGroup API

`src/FactSystem/FactGroup.h:25-27`:

```cpp
explicit FactGroup(int updateRateMsecs, const QString &metaDataFile,
                   QObject *parent = nullptr, bool ignoreCamelCase = false);
explicit FactGroup(int updateRateMsecs, QObject *parent = nullptr, bool ignoreCamelCase = false);
```

| Element | Location | Behaviour |
| --- | --- | --- |
| Metadata load | `FactGroup.cc`, first constructor | `FactMetaData::createMapFromJsonFile(metaDataFile, this)` |
| Metadata path convention | e.g. `src/Vehicle/FactGroups/VehicleGPSFactGroup.cc:13` | `":/json/Vehicle/GPSFact.json"` |
| JSON into the resource prefix | e.g. `src/Comms/Serial/CMakeLists.txt:4-7` | `qgc_add_json_resources(... PREFIX "/json" FILES ...)` |
| Fact registration | `FactGroup.h:59-62`, `.cc:_addFact` | Rejects duplicates with a warning; binds metadata if the name is present |
| Dispatch entry point | `FactGroup.h:48-49` | `virtual void handleMessage(Vehicle*, const mavlink_message_t&) {}` |
| Update coalescing | `.cc:_setupTimer` | `updateRateMsecs > 0` starts a coalescing timer; `0` means immediate signals |
| Name handling | `.cc:_camelCase` | Camel-cased unless `ignoreCamelCase`; **only one level** of `"group.fact"` hierarchy |
| Telemetry availability | `.h:65`, `.cc:_setTelemetryAvailable` | Drives the `telemetryAvailable` property |

## Vehicle message path

`Vehicle::_mavlinkMessageReceived(LinkInterface*, mavlink_message_t)` — `src/Vehicle/Vehicle.cc:525`,
connected to `MAVLinkProtocol::messageReceived` at `:123`.

| Order | Step | Location | Can it swallow the message? |
| --- | --- | --- | --- |
| 1 | sysid gate | `:527-532` | **Yes** — returns unless `sysid` matches or is 0. One exception: `RADIO_STATUS` on a link this vehicle uses |
| 2 | Link manager | `:535` | No |
| 3 | Loss accounting by compid | `:543-561` | No. **compid does not gate dispatch** |
| 4 | `FirmwarePlugin::adjustIncomingMavlinkMessage` | `:564-566` | **Yes** |
| 5 | `QGCCorePlugin::mavlinkMessage` | `:569-571` | **Yes** |
| 6 | Terrain protocol handler | `:573-575` | **Yes** |
| 7 | FTP, parameter, image, remote-ID, request-message handlers | `:576-581` | No |
| 8 | Dynamic list-model group creation | `:584-585` | No |
| 9 | **Fact group dispatch loop** | `:588-590` | No |
| 10 | `Vehicle`'s own `handleMessage` | `:592` | No |
| 11 | The `switch (message.msgid)` | `:594` onward | — |

Steps 4 and 5 are the two Aerora extension seams that can veto a message, and `IF-MAV-027` /
`IF-MAV-028` require a vetoed message to be logged. Aerora's intended use of both is **observation,
not interception**.

## CSV telemetry writer

| Element | Location | Behaviour |
| --- | --- | --- |
| Timer | `Vehicle.cc:172-173` | `_csvLogTimer` → `_writeCsvLine`, 1000 ms |
| Header enumeration | `Vehicle.cc:2796-2821`, written at `:2820` | `factNames()` then each group's `factNames()` as `"group.fact"` |
| Row enumeration | `Vehicle.cc:2823-2852`, written at `:2851` | **Re-enumerates** the same structures |
| Open gate | `:2826-2829` | Lazy open on `_armed \|\| telemetrySaveNotArmed()` |

Header and row are two independent enumerations. A fact group added after `_initializeCsv()` runs
shifts every subsequent column. `AERORA-GCS-REQ-022` forbids dynamic fact-group creation for exactly
this reason — R-027.

## Core plugin and QML engine

| Element | Location | Use |
| --- | --- | --- |
| `createQmlApplicationEngine(QObject*)` | `src/API/QGCCorePlugin.h:117` | **Virtual.** Where the URL interceptor is installed |
| `destroyQmlApplicationEngine(...)` | `src/API/QGCCorePlugin.h:119-122` | Symmetric release point, documented for interceptors |
| Application call site | `src/QGCApplication.cc:275` | Obtains the engine **through the plugin** |
| Reference implementation | `custom-example/src/CustomPlugin.cc:240-248` | Calls the base, adds import paths, installs the interceptor |
| Interceptor logic | `custom-example/src/CustomPlugin.cc:273-295` | Forms `:/Custom<origPath>`; returns the rewritten URL **only if `QFile::exists()`** |
| Explicit non-QML interception | `src/QGCApplication.cc:311` | `interceptUrl()` called directly for the Linux window icon |
| Override resource declaration | `custom-example/CMakeLists.txt:84-97` | `PREFIX "/Custom/qml"` with `QT_RESOURCE_ALIAS` set to the stock path |

This confirms the Phase 3.1 correction: resolution is a deterministic existence check, not an alias
contest. **But see [05-qml-implementation-map.md](05-qml-implementation-map.md)** — the interceptor
covers `qrc:` URLs resolved through the QML engine, and does **not** cover `:/json/...` resources
read directly by C++. That distinction is new in Phase 4.

## Serial transport

| Element | Location | Behaviour |
| --- | --- | --- |
| Port open | `src/Comms/Serial/SerialLink.cc:177-189` | `setPortName(_settings.portName)` then `open(QIODevice::ReadWrite)` |
| The only refusal | `:180-183` | `if (_settings.bootloader)` — "Not connecting to a bootloader" |
| Parameters applied | `:336-338` | baud, data bits, flow control, stop bits, parity |
| DTR and RTS | `:342-358` | `UnsupportedOperationError` is tolerated — the code explicitly notes "PTYs and some USB adapters have no modem-control lines" |
| Configuration fields | `:36-119` | `portName`, `baud`, `dataBits`, `flowControl`, `stopBits`, `parity`, `dtrForceLow` |
| Persistence | `:77-107` | Plain `QSettings` round-trip of those fields |

**There is no device whitelist.** Any port name plus a baud rate opens a link. This is the decisive
finding for F3.1-02 — see [19-build-bootstrap.md](19-build-bootstrap.md).

### Board identification and auto-connect

| Element | Location | Behaviour |
| --- | --- | --- |
| Board type enumeration | `src/Comms/Serial/QGCSerialPortInfo.h:25-32` | `BoardTypePixhawk`, `BoardTypeSiKRadio`, `BoardTypeOpenPilot`, `BoardTypeRTKGPS`, `BoardTypeUnknown` — **closed** |
| VID/PID table source | `QGCSerialPortInfo.cc:47` | `":/json/USBBoardInfo.json"` — **a JSON resource, not a C++ table** |
| Resource registration | `src/Comms/Serial/CMakeLists.txt:4-7` | `PREFIX "/json" FILES USBBoardInfo.json` |
| Match rule | `QGCSerialPortInfo.cc:208` | `vendorIdentifier() == boardInfo.vendorId && (productIdentifier() == boardInfo.productId \|\| boardInfo.productId == 0)` |
| Regular-expression fallbacks | `.cc:127-168` | Two further fallback lists keyed on description/manufacturer |
| `boardClass` vocabulary | `.cc:179-185` | Five fixed strings. **Not extensible without a core edit** |
| Auto-connect gate | `src/Comms/Serial/SerialAutoConnect.cc:91-99` | `_allowed(boardType, options)` against per-class settings |
| SiK consequence | `SerialAutoConnect.cc:79-80` | `setSikRadio(boardType == BoardTypeSiKRadio)` and **baud 57600** instead of 115200 |

A device matched as a SiK radio is auto-connected at 57600 and flagged as a SiK radio. That is the
misidentification path Phase 1 observed, now located precisely.

## Link type extensibility

| Element | Location | Consequence |
| --- | --- | --- |
| `LinkConfiguration::LinkType` | `src/Comms/LinkConfiguration.h:104-117` | A closed enum terminated by `TypeLast` |
| Header warning | `.h:103` | "Any changes here MUST be reflected in `LinkManager::linkTypeStrings()`" |
| Factory switch 1 | `src/Comms/LinkConfiguration.cc:56-68` | `createSettings` — needs a new `case` |
| Factory switch 2 | `src/Comms/LinkConfiguration.cc:92-103` | `duplicateSettings` — needs a new `case` |

**A non-serial transport therefore costs a minimum of three core edits**: the enum, and both switch
statements, plus `LinkManager::linkTypeStrings()`. There is no registration or factory hook for a
custom link type. This quantifies R-003 exactly, and it is why F3.1-02's answer matters.

## MAVLink signing

| Element | Location | Behaviour |
| --- | --- | --- |
| Class | `src/MAVLink/Signing/MAVLinkSigningKeys.h:48` | `QObject`, `QML_ANONYMOUS`, singleton via `instance()` |
| Virtual members | — | **None**, apart from the destructor |
| Friend declarations | `.h:60` | `friend class SigningTest` only |
| Settings group | `.h:131-134` | `"MAVLinkSigningKeys"`, with `manifest`, `keys/<name>`, `timestamps/<name>` |
| Persistence | `.cc:250-277` | `settings.setValue(QString("%1/%2").arg(kKeySubgroup, key->name()), serialized)` — **raw 32 bytes, unencrypted** — then `sync()` |
| Load | `.cc:404-414` | `settings.value(...).toByteArray()` → `MAVLinkSigning::makeSigningKey` |
| Export path | `.h:86` | `Q_INVOKABLE QString keyHexByName(const QString&) const` — reachable from QML |
| Derivation | `.cc:158-161` | `QPasswordDigestor::deriveKeyPbkdf2(Sha256, utf8, salt, iterations, 32)` |
| Salt | `.h:139` | `kPbkdf2Salt = "QGroundControl-MAVLink-Signing-v1"` — **a compile-time constant** |
| Iterations | `.h:141` | `kPbkdf2Iterations = 600'000` |
| Minimum passphrase | `.h:146` | `kMinPassphraseLength = 8` |
| Capacity | `.h:145` | `kMaxKeys = 64` |
| Random raw key | `.h:75`, `.cc:202` | `generateRandomHexKey()` using `QRandomGenerator::system()` |
| Test-only hook | `.h:150` | `setPbkdf2IterationsForTesting(int)` |

**No seam exists.** F3.1-01's full treatment is in
[12-security-implementation.md](12-security-implementation.md).

## Test framework

| Element | Location | Use |
| --- | --- | --- |
| `UnitTest` | `test/UnitTestFramework/UnitTest.h:262` | Base for tests with no vehicle |
| `VehicleTest` | `test/UnitTestFramework/BaseClasses/VehicleTest.h:60` | MockLink vehicle per test; `vehicle()`, `mockLink()` |
| `ParameterTest` | `BaseClasses/ParameterTest.h:26` | Derives from `VehicleTest` |
| `MissionTest` / `OfflineMissionTest` | `BaseClasses/MissionTest.h:32` / `:93` | Mission suites |
| `MockLink` | `src/Comms/MockLink/MockLink.h:30` | Ships in `src/`, not `test/` |
| Registration helper | `cmake/QGCTest.cmake:156` | `add_qgc_test(name [LABELS ...] [TIMEOUT n] [RESOURCE_LOCK r] [SERIAL])` |
| Invocation model | `QGCTest.cmake:179` | `$<TARGET_FILE:${CMAKE_PROJECT_NAME}> --unittest:<name> --allow-multiple` |
| Labels and timeouts | `QGCTest.cmake:140-154`, `:190-201` | `Unit` 60 s, `Integration` 120 s, `Slow` 180 s; `Flaky` and `Network` excluded from CI |
| Per-test isolation | `QGCTest.cmake:203-229` | Offscreen QPA, software Quick backend, per-test `TMPDIR`/`TMP`/`TEMP` |

Tests are **not separate binaries** — they run inside the application target. An Aerora test is
therefore reachable only if its source is compiled into that target, which is the same linkage rule
as the firmware plugin factory. That is the mechanism behind Q-035.

## Summary of seams Aerora will use

| Seam | Tier | Verified | Aerora use |
| --- | --- | --- | --- |
| `QGC_CUSTOM_DIR` + `CustomOverrides.cmake` | Build | Yes | Overlay activation, forced cache overrides |
| `CUSTOMCLASS` substitution of `QGCCorePlugin` | 1 | Yes | `AeroraPlugin` |
| `FirmwarePluginFactory` self-registration | 1 | Yes | `AeroraFirmwarePluginFactory` |
| `FirmwarePlugin::factGroups()` | 1 | Yes | Aerora telemetry — **with the singleton caveat** |
| `FirmwarePlugin::adjustIncomingMavlinkMessage` | 1 | Yes | Observation only |
| `QGCCorePlugin::mavlinkMessage` | 1 | Yes | Available, unused |
| `createQmlApplicationEngine` + URL interceptor | 1 | Yes | Resource override, Aerora import paths |
| `QGCOptions` / `QGCFlyViewOptions` virtuals | 1 | Yes | 36 accessors |
| Four purpose-built QML seams | 2 | Yes | Fly-view overlay, guided actions, actions list, map items |
| Tool-strip action list override | 3 | Yes | One silent fork, recorded |
| Settings metadata generator | Build | Yes | Aerora settings groups and defaults |
| `add_qgc_test` | Build | Yes | Aerora tests, existing labels |

Twelve seams, all verified present. **No planned seam was found to be missing** — the §28 condition
"a planned extension seam does not actually exist" did not trigger.

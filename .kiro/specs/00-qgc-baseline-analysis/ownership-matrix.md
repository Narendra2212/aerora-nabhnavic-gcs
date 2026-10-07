# Ownership Matrix

Component-by-component classification of what Aerora reuses, extends, replaces, or must not touch.

## Classification vocabulary

| Classification | Meaning |
| --- | --- |
| `QGC CORE` | Reuse unchanged. Aerora consumes the behaviour as-is |
| `AERORA EXTENSION` | Reuse the QGC component, attach Aerora behaviour through a sanctioned seam. No core edit |
| `AERORA NEW MODULE` | No QGC equivalent exists. Aerora builds it, living in the overlay |
| `CONFIGURATION` | Behaviour changed by CMake option, settings, or JSON — no code written |
| `REPLACEMENT` | An Aerora implementation substituted for the QGC one via a seam |
| `CORE EDIT` | Requires modifying QGC source. Each instance is enumerated and justified |
| `DO NOT MODIFY` | Aerora must not change or take over this concern |

All classifications are `PROPOSED` pending review.

## Protocol layer

| Component | Classification | Mechanism / note |
| --- | --- | --- |
| MAVLink C library acquisition | `CONFIGURATION` | `QGC_MAVLINK_GIT_REPO` / `_TAG` / `_DIALECT` from `CustomOverrides.cmake` |
| MAVLink dialect content | `AERORA NEW MODULE` | An Aerora fork of the mavlink repository, pinned to a commit |
| `MAVLinkProtocol` parse loop | `QGC CORE` | Per-channel parsing, loss statistics, v1 filtering — reuse unchanged |
| MAVLink channel allocation | `QGC CORE` | 15 usable channels. A ceiling, not a thing to change |
| Message routing (fan-out-and-filter) | `QGC CORE` | Reuse. Do not introduce a competing router |
| Message forwarding | `QGC CORE` | Signature-stripping forwarding already correct |
| MAVLink 2 signing stack | `QGC CORE` | Well engineered. Reuse the mechanism |
| Signing **key storage** | `REPLACEMENT` | Plaintext `QSettings` is inadequate. Replace the storage backend, keep the signing logic |
| Aerora message handling | `AERORA EXTENSION` | `QGCCorePlugin::mavlinkMessage()` |
| Inbound message adjustment | `AERORA EXTENSION` | `FirmwarePlugin::adjustIncomingMavlinkMessage()` |
| Outbound message adjustment | `AERORA EXTENSION` | `FirmwarePlugin::adjustOutgoingMavlinkMessageThreadSafe()` |
| `MavCommandQueue` send/retry/ack | `QGC CORE` | Reuse. Retry and timeout constants are sound |
| MAV_CMD metadata overrides | `AERORA EXTENSION` | `FirmwarePlugin::missionCommandOverrides()` + JSON |
| **New MAV_CMD entries in the Plan UI** | `CORE EDIT` or `CONFIGURATION` | Base `MavCmdInfoCommon.json` edit, unless the resource-alias shadow works. `OPEN` |

## Transport layer

| Component | Classification | Mechanism / note |
| --- | --- | --- |
| `LinkInterface` contract | `QGC CORE` | Four pure virtuals. Subclass, do not change |
| `LinkConfiguration` contract | `QGC CORE` | Five pure virtuals. Subclass, do not change |
| `LinkManager` lifecycle | `QGC CORE` | Reuse the whole lifecycle, mutex discipline, and shutdown logic |
| Worker-thread pattern | `QGC CORE` | Copy the pattern from `UDPLink`. Do not invent a new threading model |
| Serial, UDP, TCP, Bluetooth, LogReplay | `QGC CORE` | Reuse. Aerora may well need UDP and serial regardless |
| `MockLink` | `QGC CORE` | Reuse as the test-double pattern. Debug-only, cannot ship |
| Auto-connect and reconnect backoff | `QGC CORE` | Reuse. Exponential backoff with a stability window is correct |
| **`LinkType` enum** | `CORE EDIT` | One value per Aerora transport, immediately before `TypeLast` |
| **`LinkConfiguration::createSettings` switch** | `CORE EDIT` | Unavoidable |
| **`LinkConfiguration::duplicateSettings` switch** | `CORE EDIT` | Unavoidable |
| **`LinkManager::createConnectedLink` switch** | `CORE EDIT` | Unavoidable |
| **`LinkManager::loadLinkConfigurationList` switch** | `CORE EDIT` | Unavoidable |
| **`LinkManager::linkTypeStrings()`** | `CORE EDIT` | Order-coupled to the enum. Self-checks the count |
| Aerora LoRa transport | `AERORA NEW MODULE` | `LinkInterface` + `LinkConfiguration` subclasses |
| Aerora FHSS24 transport | `AERORA NEW MODULE` | Hardware existence unconfirmed by the baseline |
| Aerora RF433 transport | `AERORA NEW MODULE` | Hardware existence unconfirmed by the baseline |
| Aerora transport test double | `AERORA NEW MODULE` | Follow the MockLink pattern |
| Link settings QML pages | `AERORA NEW MODULE` | Data-driven; no QML switch edit needed |
| Link loss annunciation | `QGC CORE` | `DO NOT MODIFY` into control. See the safety row below |

**Six core edits total for the transport layer**, paid once per distinct `LinkType`. The
granularity decision — one `LinkType` per radio versus one with a sub-type discriminator — is
`OPEN`.

## Vehicle layer

| Component | Classification | Mechanism / note |
| --- | --- | --- |
| `Vehicle` class | `QGC CORE` | **Do not extend.** 3573 lines already. Attach via FactGroups and seams |
| `MultiVehicleManager` | `QGC CORE` | Reuse, including the two-phase delete and active-vehicle switch |
| Vehicle identity by system id | `QGC CORE` | Note: no UID-based matching exists. Fleet identity is `OPEN` |
| `VehicleLinkManager` | `QGC CORE` | Reuse. Multi-link tracking and primary-link policy are sound |
| Comm-loss detection (1000 ms poll, 3500 ms timeout) | `QGC CORE` | Reuse the detection. `DO NOT MODIFY` into a control action |
| `Fact`, `FactMetaData`, `FactGroup` | `QGC CORE` | Mandatory. Golden rule — never create custom parameter storage |
| `ParameterManager` | `QGC CORE` | Reuse all four download branches, retry logic, and cache |
| Parameter metadata | `AERORA EXTENSION` | `FirmwarePlugin::_internalParameterMetaDataFile()` + `adjustMetaData()` |
| Aerora payload FactGroup | `AERORA NEW MODULE` | Via `FirmwarePlugin::factGroups()` |
| Aerora power / tether FactGroup | `AERORA NEW MODULE` | Same mechanism |
| Aerora autonomy / GPU FactGroup | `AERORA NEW MODULE` | Same mechanism |
| `FirmwarePlugin` base | `QGC CORE` | Subclass. Note it is a shared singleton per firmware class |
| `FirmwarePluginFactory` registration | `QGC CORE` | Self-registration works. No core edit |
| Aerora firmware plugin | `AERORA EXTENSION` | `AeroraFirmwarePlugin : FirmwarePlugin` |
| PX4 / APM firmware plugins | `CONFIGURATION` | Disable via `QGC_DISABLE_*_PLUGIN_FACTORY` if Aerora is single-stack |
| `AutoPilotPlugin` / `VehicleComponent` | `AERORA EXTENSION` | Subclass and override `vehicleComponents()` |
| Firmware upgrade / bootloader | `QGC CORE` | Reuse, or hide via `QGCOptions::showFirmwareUpgrade()` |
| Named flight-mode accessors | `DO NOT MODIFY` **yet** | Must not be populated with guessed Aerora mode names. `OPEN` |

## Mission layer

| Component | Classification | Mechanism / note |
| --- | --- | --- |
| `PlanMasterController` | `QGC CORE` | Reuse the orchestration and dual dirty flags |
| `MissionController` | `QGC CORE` | Reuse |
| `GeoFenceController`, `RallyPointController` | `QGC CORE` | Reuse |
| `PlanManager` transfer protocol | `QGC CORE` | Reuse. Not plugin-extensible by design |
| `VisualMissionItem` / `SimpleMissionItem` | `QGC CORE` | Reuse |
| `ComplexMissionItem` | `QGC CORE` | Subclass |
| Survey, CorridorScan, StructureScan | `QGC CORE` | Reuse. May be useful for inspection missions |
| Landing patterns | `QGC CORE` | Reuse |
| **Aerora coverage / painting pattern** | `AERORA NEW MODULE` | `ComplexMissionItem` subclass. **Zero core edits** — proven by `custom-example` |
| Pattern registration | `AERORA EXTENSION` | `complexMissionItemNames()` + `createComplexMissionItem()` |
| Plan creator tile | `AERORA EXTENSION` | `planCreators()` |
| Pattern preset system | `QGC CORE` | `presetsSettingsGroup()` — reuse for repeatable painting jobs |
| `CameraCalc` manual mode for swath and trigger | `QGC CORE` | Sanctioned reuse path. Fact keys remain camera-named |
| `CameraSection` | `QGC CORE` | Reuse for actual cameras |
| `Section` base | `AERORA EXTENSION` | Implement for an Aerora payload action. Only two implementations exist today |
| Plan file extra data | `AERORA EXTENSION` | Six plugin hooks, with a nested `version` key |
| `kPlanFileVersion` | `DO NOT MODIFY` | Incrementing breaks stock `.plan` compatibility |
| Mission progress and flight status | `QGC CORE` | Reuse, including `batteryChangePoint` and `batteriesRequired` |
| **`includedResumeCommands` allow-list** | `CORE EDIT` | Aerora payload state commands would be silently dropped from a resume mission. Safety-relevant. `OPEN` |
| Survey-style insert behaviour | `CORE EDIT` if needed | Hardcoded `qobject_cast` chain. `OPEN` |
| Round-trip scan from a downloaded mission | `AERORA NEW MODULE` if needed | Only landing patterns round-trip today |
| `MissionCommandTree` | `QGC CORE` | Reuse the hierarchy and collapse semantics |

## Presentation layer

| Component | Classification | Mechanism / note |
| --- | --- | --- |
| `MainWindow.qml` | `QGC CORE` | Prefer not to override. A new top-level view would require it |
| Navigation and close-guard chain | `QGC CORE` | Reuse, including `navigationBlockedReason` for safety-critical blocking |
| `FlyView` composition | `QGC CORE` | Reuse the structure |
| **`FlyViewCustomLayer.qml`** | `REPLACEMENT` | The purpose-built vendor seam. Aerora's primary UI surface |
| `FlyViewToolStripActionList.qml` | `REPLACEMENT` | Per-file override via the interceptor |
| `CustomGuidedActionsController.qml` | `REPLACEMENT` | Per-file override |
| Guided action visibility | `CONFIGURATION` | `QGCFlyViewOptions::guidedBarShow*` |
| Toolbar indicators | `AERORA EXTENSION` | `toolBarIndicators()` or `toolIndicators()` |
| Instrument panel | `REPLACEMENT` or `CONFIGURATION` | `showInstrumentPanel()` false plus an Aerora widget, or an `instrumentQmlFile2` enum entry |
| `PlanView` | `QGC CORE` | Reuse. The pattern picker is data-driven |
| Pattern editor and map visual QML | `AERORA NEW MODULE` | Own QML module |
| `QGCPalette` mechanism | `QGC CORE` | Reuse |
| Palette colour values | `REPLACEMENT` | `paletteOverride()` — all roles, all four variants |
| `ScreenTools` | `QGC CORE` | Mandatory for sizing. No hardcoded QML sizes |
| `QGroundControlQmlGlobal` | `QGC CORE` | Reuse as the QML entry point |
| `FactControls` | `QGC CORE` | Reuse. Do not write bespoke Fact-bound controls |
| Branding — name, icons, installer art | `CONFIGURATION` | `CustomOverrides.cmake` plus resource aliasing |
| Splash screen | `AERORA NEW MODULE` | No QGC mechanism exists. Platform launch image only |
| Settings page generator | `QGC CORE` | Reuse |
| Settings page set | `CONFIGURATION` | `SettingsPages.json` overlay with insert, remove, replace |
| Aerora settings groups | `AERORA EXTENSION` | `registerCustomSettingsGroup()` |
| Stock settings visibility | `CONFIGURATION` | `overrideSettingsGroupVisibility()`, `adjustSettingMetaData()` |
| Analyze pages | `AERORA EXTENSION` | `analyzePages()` |
| Pre-flight checklist | `REPLACEMENT` | `QGCOptions::preFlightChecklistUrl()` |
| Video pipeline | `QGC CORE` or `CONFIGURATION` | Reuse, or disable entirely via `QGC_ENABLE_GST_VIDEOSTREAMING=OFF` |
| Map tile engine | `QGC CORE` | Reuse. QGC ships its own QtLocation plugin |

## Safety and authority

The most important section. Classifications here are architectural commitments, not conveniences.

| Concern | Classification | Rationale |
| --- | --- | --- |
| **Flight controller local safety** | `DO NOT MODIFY` | The baseline requires the aircraft retain local control and safety capability. Never move this into the GCS |
| **Failsafe behaviour definition** | `DO NOT MODIFY` | Deferred to a future approved Simulink implementation. Status `OPEN`. Do not infer RTL, LAND, LOITER, termination, or comm-loss response |
| **GPU / autonomy decision-making** | `DO NOT MODIFY` | An FC and GPU subsystem concern. The GCS observes and supervises |
| GCS comm-loss annunciation | `QGC CORE` | Reuse the detection and display. Do not convert it into a control action |
| Operator control authority | `AERORA EXTENSION` | Extend the existing `requestOperatorControl` and `gcsControlStatusFlags` mechanism |
| **Proposal versus effective command** | `AERORA NEW MODULE` | QGC has no concept of a proposal. Core baseline principle, so this must be built explicitly |
| Command acceptance and rejection display | `AERORA EXTENSION` | `MavCommandQueue::commandResult` already carries result and failure code |
| Command freshness | `AERORA NEW MODULE` | No QGC equivalent |
| `HealthAndArmingCheckReport` | `QGC CORE` | Reuse vehicle-reported readiness |
| Pre-arm error display | `QGC CORE` | Reuse |
| Payload emergency stop | `AERORA NEW MODULE` | Behaviour `OPEN`; the baseline does not define it |
| Power fault handling | `DO NOT MODIFY` into the GCS | Belongs to the power system and firmware. GCS observes |
| Tether detection | `AERORA NEW MODULE` | Observation only. Mechanism `OPEN` |

## Infrastructure

| Component | Classification | Mechanism / note |
| --- | --- | --- |
| CMake build system | `QGC CORE` | Reuse. The overlay is the sanctioned entry point |
| `CustomOverrides.cmake` | `AERORA NEW MODULE` | Required by the overlay contract |
| `QGC_*` options | `CONFIGURATION` | Force from the overlay |
| CPM dependency management | `QGC CORE` | Reuse the discipline. Pin to immutable references |
| Test framework | `QGC CORE` | Reuse the entire hierarchy |
| Aerora tests | `AERORA NEW MODULE` | Overlay tests compile into the Aerora binary and are auto-discovered |
| Coverage, sanitizers, fuzzing | `QGC CORE` | Reuse. TSan is the gate for transport work |
| Pre-commit hooks | `QGC CORE` | Binding on Aerora code |
| CI workflows | `AERORA EXTENSION` | The custom-build workflow already exercises an overlay |
| Translations | `QGC CORE` | Reuse. Wrap Aerora strings in `qsTr()` |
| Platform packaging | `CONFIGURATION` + `AERORA NEW MODULE` | Overlay `deploy/`; Windows and Android signing is new work |

## Security and cloud

Every row here is new, because [security-architecture.md](security-architecture.md) found no
foundation to extend.

| Capability | Classification | Note |
| --- | --- | --- |
| Operator authentication | `AERORA NEW MODULE` | Nothing exists |
| Role-based authorisation | `AERORA NEW MODULE` | `showAdvancedUI` is a UI toggle, not authorisation |
| Session management | `AERORA NEW MODULE` | Nothing exists |
| Audit logging | `AERORA EXTENSION` | Extend `LogManager`, which has rate limiting |
| Secure key storage | `REPLACEMENT` | Replace the `QSettings` backend behind `MAVLinkSigningKeys` |
| Signed software update | `AERORA NEW MODULE` | `setExpectedHash` exists but is unused on that path |
| Configuration integrity | `AERORA NEW MODULE` | Nothing exists |
| Mission integrity | `AERORA NEW MODULE` | Plan file is unsigned JSON |
| Link-layer encryption | `AERORA NEW MODULE` | Transport-layer decision, `OPEN` |
| Windows / Android code signing | `AERORA NEW MODULE` | Build work |
| AWS modular monolith | `AERORA NEW MODULE` | Strictly additive, never on the command path |
| Fleet management | `AERORA NEW MODULE` | Depends on fleet vehicle identity, which is `OPEN` |
| Cloud telemetry ingestion | `AERORA NEW MODULE` | Must degrade gracefully |

## Totals

136 components classified across the tables above.

| Classification | Count |
| --- | --- |
| `QGC CORE` — reuse unchanged | 57 |
| `AERORA NEW MODULE` | 31 |
| `AERORA EXTENSION` — seam attachment | 18 |
| `CONFIGURATION` | 11 |
| `CORE EDIT` | 9 — six of them the transport registration, paid once |
| `REPLACEMENT` | 8 |
| `DO NOT MODIFY` | 6 |

The counts sum to 140 rather than 136 because four rows carry two classifications, where the
choice depends on an `OPEN` decision — for example the video pipeline, which is either `QGC CORE`
reuse or `CONFIGURATION` disablement depending on Q-031.

The shape of this distribution is the headline: **the overwhelming majority of QGC is reusable
unchanged, and the core-edit surface is small, enumerated, and concentrated in one place.** The
risk is not in the code — it is in the `OPEN` decisions listed in
[open-questions.md](open-questions.md).

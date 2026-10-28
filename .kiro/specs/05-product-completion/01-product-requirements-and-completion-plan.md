# Aerora Universal GCS â€” Product Requirements and Completion Plan

Owner: **Tab 1**. Master product requirements document for the Phase 7 completion programme.

Status: `DESIGNED`. No requirement in this document has been implemented by this document. Status
vocabulary: [00-README](00-README.md#status-vocabulary).

## Contents

- [1. Product definition](#1-product-definition)
- [2. Operating modes](#2-operating-modes)
- [3. Architecture boundaries](#3-architecture-boundaries)
- [4. Non-negotiable product constraints](#4-non-negotiable-product-constraints)
- [5. Requirement blocks](#5-requirement-blocks)
- [6. User journeys](#6-user-journeys)
- [7. Completion plan](#7-completion-plan)
- [8. Definition of done, per status](#8-definition-of-done-per-status)
- [9. What this document does not decide](#9-what-this-document-does-not-decide)

## 1. Product definition

Aerora Universal GCS is a cross-product industrial drone ground control station built as a
QGroundControl overlay. The first target product is an AI-enabled painting hexacopter. Firefighting,
agriculture, inspection and other compatible aircraft follow as **product profiles** over the same
shell, services and interfaces.

### Product identity

| Property | Value |
| --- | --- |
| Category | Industrial, multi-product ground control station |
| Base | QGroundControl (fork at `9459a4b57`, `v5.2.0-dev-102`), customised through the supported overlay mechanism (`QGC_CUSTOM_DIR=aerora`) |
| Protocol | MAVLink. No invented dialect. See [Â§4.6](#46-no-invention) |
| Primary deployment | Local-first desktop/tablet operator station |
| Cloud | Optional, additive, never authorising |
| First product profile | AI-enabled painting hexacopter |
| Planned profiles | Firefighting, agriculture, inspection |

### What Aerora is

A **supervisory** station. It plans work, uploads it, observes what the aircraft reports, surfaces
state honestly including the absence of state, mediates operator approval of autonomy proposals,
and records what happened for audit and analysis.

### What Aerora is not

| Not | Because |
| --- | --- |
| A flight controller | The flight controller owns stabilization and local failsafes. See [Â§3](#3-architecture-boundaries) |
| An autonomy authority | The GPU proposes; the flight controller decides. See [Â§3.2](#32-gpu-proposes-the-flight-controller-decides) |
| A cloud client that needs the cloud | Local-first operation is a requirement, not a fallback. `P7-DAT-003` |
| A payload safety controller | The payload controller stays authoritative for payload safety. `P7-PAY-002` |

### Target users

| Role | Needs | Authority |
| --- | --- | --- |
| Operator / pilot in command | Live state, manual control, approval of proposals, abort | Highest in-flight authority the station can exercise |
| Mission planner | Surface and route definition, validation, coverage estimation | No in-flight authority |
| Maintenance / setup technician | Calibration, sensors, parameters, diagnostics, firmware | Configuration authority; gated when armed |
| Fleet / operations manager | Vehicle inventory, profiles, logs, utilisation, incident review | No vehicle authority |
| Safety / compliance reviewer | Audit trail, event timelines, pre-flight evidence, export | Read-only |

Role definitions are the **requirement**. The authorised role *set* is `Q-029`, still `OPEN` â€” see
[08](08-risk-register-and-open-decisions.md#1-blockers). Until it closes, no
authorization decision is evaluated and the station behaves as permissive, which
`AeroraCorePlugin::isAuthorizationEnforced()` reports as `false` rather than hiding.

## 2. Operating modes

Three modes. The mode is a property of the **operation**, not a UI theme, and the station never
upgrades its own authority.

### 2.1 Manual

| Aspect | Requirement |
| --- | --- |
| `P7-MOD-001` | Operator provides continuous control input. Station displays state and does not interpose |
| `P7-MOD-002` | Station-originated guided actions are available only where the vehicle reports the corresponding capability |
| `P7-MOD-003` | Loss of operator input is handled by the flight controller's configured behaviour. The station reports what the vehicle reports and **does not** substitute a GCS-side failsafe |

`P7-MOD-003` is the first place invention is tempting and prohibited. The station has no authority to
decide what happens on link loss; it has an obligation to display what the vehicle says will happen,
or `UNKNOWN` when the vehicle has not said.

### 2.2 Semi-autonomous / operator-supervised

| Aspect | Requirement |
| --- | --- |
| `P7-MOD-010` | An autonomy source may emit a `PROPOSED` action. It is not `REQUESTED` until the operator approves and the station transmits |
| `P7-MOD-011` | Every proposal carries its source, its freshness, and the evidence behind it. A proposal with no displayable basis is shown as such, not hidden |
| `P7-MOD-012` | Approval is explicit, per-proposal or per-bounded-batch. There is no standing approval that survives a session |
| `P7-MOD-013` | A stale proposal is not approvable. Freshness budget is `Q-036`, `OPEN` â€” no value invented |
| `P7-MOD-014` | Approval, transmission, acceptance and rejection are four distinct recorded events |

### 2.3 Fully autonomous

| Aspect | Requirement |
| --- | --- |
| `P7-MOD-020` | Available only when the vehicle **reports** the capability and the operator holds the authorization. Both conditions, both verified at the time of use |
| `P7-MOD-021` | Absence of a capability report is `UNKNOWN`, which does not satisfy `P7-MOD-020`. Optimistic defaulting is prohibited |
| `P7-MOD-022` | The operator retains an abort path at all times, and the station displays whether that path is currently believed available |
| `P7-MOD-023` | The station does not enter autonomous mode on its own initiative, including on reconnection |

### Mode display rule

`P7-MOD-030`: the displayed mode is the `EFFECTIVE` mode as reported by the vehicle. A station-side
`REQUESTED` mode change that the vehicle has not confirmed is displayed as `REQUESTED`, visually
distinct, and never as the current mode. This is the single most consequential UI requirement in the
document, and it is why `AeroraTelemetryState` exists as a separate classifier rather than as a
boolean "fresh" flag.

## 3. Architecture boundaries

Five boundaries. Each is a prohibition on the station, not a feature of it.

### 3.1 The flight controller owns stabilization and local failsafes

`P7-ARCH-001`. The station does not implement a control loop, does not implement a failsafe, and does
not implement a geofence enforcement action. It **uploads** a geofence and **displays** the
vehicle's reported fence state and reported breach action.

### 3.2 GPU proposes, the flight controller decides

`P7-ARCH-002`. The autonomy compute element produces proposals. The flight controller determines
whether it can execute one. The station's role is to present the proposal, carry the operator's
approval, and display the flight controller's answer â€” including a refusal.

`P7-ARCH-003`. A proposal the flight controller refuses is displayed as refused with the reported
reason, or with `UNKNOWN` when no reason was reported. The station does not retry on the operator's
behalf and does not translate a refusal into a different command.

### 3.3 The GCS supervises and displays

`P7-ARCH-004`. The station does not silently become the flight controller. Any station-side
computation that could be mistaken for flight authority â€” a predicted trajectory, an estimated
remaining energy, a derived coverage figure â€” is labelled as station-derived and is visually distinct
from a `REPORTED` value.

`P7-ARCH-005`. `AeroraPowerModel::kRemainingEnergyEstimationImplemented` is `false` and stays `false`
until a method is specified and approved. The station shows reported energy state, not an invented
projection. (`aerora/src/Power/AeroraPowerModel.h:153`)

### 3.4 Cloud is optional and never authorises

| Requirement | Statement |
| --- | --- |
| `P7-ARCH-010` | Every operator-facing function except explicitly cloud-labelled ones works with no network |
| `P7-ARCH-011` | No cloud response is an input to a command authorization decision |
| `P7-ARCH-012` | Cloud absence is a displayed state, not an error dialog, and not a blocked workflow |
| `P7-ARCH-013` | No project code, telemetry, log or credential leaves the host without an explicit operator action |

### 3.5 MAVLink and QGroundControl behaviour are preserved

| Requirement | Statement |
| --- | --- |
| `P7-ARCH-020` | Existing QGC behaviour is preserved unless a requirement explicitly changes it, and every such change is recorded with its fork point |
| `P7-ARCH-021` | Core (`src/`) changes stay minimal, enumerated and individually justified. The Phase 6 set is 9 files; see [04 Â§ Core modifications](04-subsystem-and-interface-completion-matrix.md#6-qgroundcontrol-core-modifications) |
| `P7-ARCH-022` | No invented MAVLink dialect, message, enum value or identifier. `Q-018` (Aerora `MAV_AUTOPILOT`) stays `OPEN`; Aerora claims `Generic` |
| `P7-ARCH-023` | Upstream rebase must remain mechanically possible. Every class-X replacement (populated-file fork) is registered with an owner and a drift review |

## 4. Non-negotiable product constraints

### 4.1 Safety

`P7-SAFE-001`: a safety-relevant display is either traceable to a vehicle report or labelled as not
being one. There is no third category.

`P7-SAFE-002`: the station does not define, infer or display a failsafe action it did not receive.
Where the vehicle has not reported its configured action, the display is `UNKNOWN`.

`P7-SAFE-003`: no station-side threshold acquires safety meaning without an approved value.
`AeroraPowerModel::kDefinedThresholdCount` is `0`. It stays `0` until thresholds are approved.

### 4.2 Authorization

`P7-AUTH-001`: every outbound command passes the authorization decision point before it is queued.
The enforcement point exists (`QGCCorePlugin::authorizeMavCommand`, consumed in
`MavCommandQueue::sendWorker()`).

`P7-AUTH-002`: when no policy is installed the station reports that fact rather than implying a
boundary. This is the current state and it is a **recorded gap**, not a design position.

`P7-AUTH-003`: `PARAM_SET` is the broadest reconfiguration path in the application and currently has
no owning workstream. Resolving this is a Phase 7 gate â€” see
[07 Â§ Tab 9](07-phase-7-work-packages-and-ownership.md#tab-9--fact-system-and-parameter-authority-new).

`P7-AUTH-004`: the UI exposes no actuating path that bypasses the authorization point. Asserted
mechanically by `AeroraUiSafetyTest` over the meta-objects of the QML-reachable types.

### 4.3 Auditability

`P7-AUDIT-001`: operator decisions, authorization outcomes, mode changes, mission uploads and payload
requests are recorded with their time, actor, subject and outcome.

`P7-AUDIT-002`: the audit sink's durability and tamper-evidence are `Q-037`/`Q4-003`, `OPEN`. The
current sink reports itself as neither durable nor tamper-evident, which is honest and insufficient.

`P7-AUDIT-003`: an unauditable event is counted. Whether an unauditable **allow** must be denied is
`OPEN` and belongs to product, not engineering.

### 4.4 Hardware-independent interfaces

`P7-HW-001`: every hardware-facing capability is expressed as an interface with the hardware behind
it, so a profile change is a configuration change.

`P7-HW-002`: an interface designed but not hardware-verified is labelled `DESIGNED`, never
"supported". This applies in full to radio/connection management â€” see
[Â§5.14](#514-radioconnection-management).

### 4.5 State distinctness

`P7-STATE-001`: `REPORTED`, `EFFECTIVE`, `REQUESTED`, `PROPOSED`, `STALE`, `UNKNOWN` and
`UNAVAILABLE` remain distinguishable in the service layer, in the view-model and on screen.

`P7-STATE-002`: the three non-value states (`UNKNOWN`, `UNAVAILABLE`, `STALE`) are visually
distinguishable from each other and from a zero value. A blank field is prohibited for all three.

### 4.6 No invention

`P7-NOINV-001`: the following are never invented â€” failsafe actions, hardware capabilities, telemetry
values, actuator behaviour, MAVLink dialects or identifiers, firmware responses, API endpoints,
threshold values, and regulatory compliance claims.

`P7-NOINV-002`: where a value is required and unknown, the correct output is the requirement ID of
the open decision, not a plausible number.

## 5. Requirement blocks

Twenty blocks, matching the programme's module list. Each block gives its requirements, its current
status, and its owner. Component and seam mapping is in
[03](03-gui-information-architecture-and-screens.md) and
[04](04-subsystem-and-interface-completion-matrix.md).

### 5.1 Application shell, navigation, design system, responsive layout

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-SHELL-001` | A single navigation model covers every Aerora screen, with at most one recorded fork of an upstream populated file | `DESIGNED` |
| `P7-SHELL-002` | Aerora screens are reachable without forking `MainWindow.qml` | `DESIGNED` â€” `mainWindow.showTool()` and `analyzePages()` are both open registries |
| `P7-SHELL-003` | An Aerora design system defines colour roles, spacing, type scale, state iconography and control variants, derived from `QGCPalette` + `ScreenTools`, with no hard-coded size or colour | `DESIGNED` |
| `P7-SHELL-004` | Layout adapts across desktop, tablet and the narrow-window case without horizontal truncation of safety-relevant values | `DESIGNED` |
| `P7-SHELL-005` | The design system is original. No reference product's branding, visual assets or distinctive look is reproduced | `DESIGNED` |
| `P7-SHELL-006` | `AeroraQmlOverrides` is installed on the QML engine | `IMPLEMENTED-UNCOMPILED` (interceptor) + `OPEN` (install â€” HO-TAB7-02) |

Owner: Tab 7. Install step is Tab 1 (`AeroraCorePlugin` is M-Plugin's).

### 5.2 Flight / live operations

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-FLY-001` | A live operations view shows vehicle state, mode (per `P7-MOD-030`), position, mission progress, payload state and link state in one screen | `DESIGNED` |
| `P7-FLY-002` | Every value carries its state class and its age. Non-value states render per `P7-STATE-002` | `DESIGNED`; primitives `IMPLEMENTED-UNCOMPILED` (`AeroraValueDisplay`, `AeroraStateText`, `AeroraAgeLabel`) |
| `P7-FLY-003` | Aerora live content occupies the `FlyViewCustomLayer` seam and publishes its occluder rectangles so the map does not hide the vehicle beneath a panel | `DESIGNED` |
| `P7-FLY-004` | Operator actions are presented only where a command path exists and is authorized. Zero today | `FORBIDDEN-UNTIL` â€” gated on `Q-029` and ADR-014 |
| `P7-FLY-005` | A critical-state change is surfaced without requiring the operator to be looking at the relevant panel | `DESIGNED` |

Owner: Tab 7 (presentation), Tab 4 (state), Tab 6 (payload/power content).

### 5.3 Mission Studio â€” 2D, 3D, waypoint and industrial-surface planning

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-MIS-001` | Waypoint and standard-pattern planning is QGC's, reused unchanged | `IMPLEMENTED` (upstream) |
| `P7-MIS-002` | An industrial-surface planning item defines a work surface, an approach, a standoff and a coverage pattern, and is registered through `complexMissionItemNames()` / `createComplexMissionItem()` | `SPECIFIED-NOT-DESIGNED` â€” blocked on `Q-021`/`Q-023` |
| `P7-MIS-003` | 3D planning uses the existing `src/Viewer3D/` Qt Quick 3D viewport. No new 3D engine is introduced | `DESIGNED` |
| `P7-MIS-004` | A complex item supplies **both** `mapVisualQML()` and `geoMapVisualQML()`, or is documented as invisible under the GeoMap engine | `DESIGNED` |
| `P7-MIS-005` | Mission upload, download and verification compare two snapshots and report agreement, never assert it | `IMPLEMENTED-UNCOMPILED` (`AeroraMissionObserver`, `AeroraMissionSnapshot`) |
| `P7-MIS-006` | Resume after interruption does not replay payload state from the station | `IMPLEMENTED-UNCOMPILED` â€” `kGcsPayloadStateReplayImplemented = false` is the enforcement |

Owner: Tab 5. `P7-MIS-002` is blocked on a product decision, not on engineering.

### 5.4 Painting operations and payload supervision

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-PAY-001` | Payload state is displayed as reported, with state class and freshness | `IMPLEMENTED-UNCOMPILED` (`AeroraPayloadModel`) |
| `P7-PAY-002` | The payload controller remains authoritative for payload safety. The station does not interlock, inhibit or sequence payload safety | `IMPLEMENTED-UNCOMPILED` â€” `kGroundStationActuatorPathImplemented = false` |
| `P7-PAY-003` | A payload request path exists as an interface with no concrete sink until a command path is authorized | `CONTRACT-ONLY` â€” `AeroraPayloadCommandSink` has no implementation anywhere in the overlay |
| `P7-PAY-004` | Painting-specific supervision (coverage, consumption, nozzle/applicator state) is displayed from reported values only | `SPECIFIED-NOT-DESIGNED` â€” the reported value set is not defined; depends on the payload ICD and `Q3-009` |
| `P7-PAY-005` | Autonomous payload action is not initiated by the station | `IMPLEMENTED-UNCOMPILED` â€” `kAutonomousPayloadActionImplemented = false` |

Owner: Tab 6.

### 5.5 Vehicle setup, health, sensors and diagnostics

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-VEH-001` | Aerora setup pages are supplied through an `AeroraAutoPilotPlugin` returned from `FirmwarePlugin::autopilotPlugin()`, not by forking `VehicleConfigView.qml` | `DESIGNED` â€” seam confirmed at `src/FirmwarePlugin/FirmwarePlugin.h:122` |
| `P7-VEH-002` | Firmware-specific behaviour routes through `vehicle->firmwarePlugin()`. No `if (px4)` branch | `IMPLEMENTED-UNCOMPILED` (`AeroraFirmwarePlugin`) |
| `P7-VEH-003` | Calibration is authorization-gated, evaluated after the parameter switch and before the pack | `IMPLEMENTED-UNCOMPILED` â€” ADR-015 |
| `P7-VEH-004` | Health and sensor status is reported state only, with `UNKNOWN` where unreported | `VERIFIED` — `SCR-VEH-04`. The page withholds every verdict until a `SYS_STATUS` has arrived, which `AeroraVehicleConfigUITest` asserts against `Vehicle::allSensorsHealthy()`'s default of true |
| `P7-VEH-005` | Vehicle identity and capability are reported, never assumed. Fleet identity format is `OPEN`; MAVLink sysid is explicitly **not** the fleet identity | `IMPLEMENTED-UNCOMPILED` + `OPEN` |
| `P7-VEH-006` | A diagnostics screen shows service attachment, capability reports and interface availability, so a missing service is visible rather than silent | `DESIGNED` |

Owner: Tab 3.

### 5.6 Telemetry, status quality, freshness, event timelines

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-TEL-001` | Every telemetry item carries a declared kind, an observation and a derived state class | `IMPLEMENTED-UNCOMPILED` (`AeroraTelemetryState`) |
| `P7-TEL-002` | Freshness is evaluated against a per-item budget. Budgets are `Q-036`, `OPEN` â€” none invented | `IMPLEMENTED-UNCOMPILED` (mechanism) + `OPEN` (values) |
| `P7-TEL-003` | Aerora fact groups attach per-vehicle through the ADR-016 registrar | `CONTRACT-ONLY` â€” `isRegistrationAvailable()` is false; six groups are contract-only |
| `P7-TEL-004` | An event timeline records state transitions, operator actions and authorization outcomes on one time axis | `SPECIFIED-NOT-DESIGNED` |
| `P7-TEL-005` | Telemetry source attribution is carried where the source is known. `IF-MAV-022` is `OPEN`; no attribution is invented | `OPEN` |

Owner: Tab 4.

### 5.7 AI/GPU supervision and operator approval

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-AI-001` | A proposal has a source, a timestamp, a subject, a basis and a lifecycle (`PROPOSED` â†’ approved/rejected/expired) | `SPECIFIED-NOT-DESIGNED` |
| `P7-AI-002` | A proposal never becomes a command without an explicit operator approval event | `DESIGNED` |
| `P7-AI-003` | The approval surface is disabled while no authorized command path exists, and says why | `DESIGNED` |
| `P7-AI-004` | The GPU/autonomy interface is specified as an interface. No endpoint, schema or response is invented | `OPEN` â€” depends on the GPU-FC ICD and `D-007` |
| `P7-AI-005` | A proposal the station cannot evaluate is displayed as unevaluable, not suppressed | `DESIGNED` |

Owner: Tab 6 (supervisory), Tab 7 (approval surface), Tab 1 (interface boundary).

### 5.8 Power, tether and battery monitoring

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-PWR-001` | Power state is displayed as reported, per source, with state class and freshness | `IMPLEMENTED-UNCOMPILED` (`AeroraPowerModel`) |
| `P7-PWR-002` | No station-initiated power action | `IMPLEMENTED-UNCOMPILED` â€” `kStationInitiatedPowerActionImplemented = false` |
| `P7-PWR-003` | No station-side arbitration between power sources | `IMPLEMENTED-UNCOMPILED` â€” `kPowerArbitrationImplemented = false` |
| `P7-PWR-004` | No automatic source transition, and no display implying one | `IMPLEMENTED-UNCOMPILED` â€” `kAutomaticSourceTransitionImplemented = false` |
| `P7-PWR-005` | No remaining-energy estimate | `IMPLEMENTED-UNCOMPILED` â€” `kRemainingEnergyEstimationImplemented = false` |
| `P7-PWR-006` | Tether state, where reported, is displayed as its own source with its own freshness | `SPECIFIED-NOT-DESIGNED` â€” the reported tether item set is undefined |

Owner: Tab 6. Every switching, threshold and transition behaviour is `OPEN` by design: the four
`false` flags are the mechanism that keeps them open.

### 5.9 Fleet and product-profile management

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-FLEET-001` | A profile selects product-specific UI, mission content and capability expectations | `IMPLEMENTED-UNCOMPILED` (`AeroraProductProfile`, `AeroraProductProfileRegistry`) |
| `P7-FLEET-002` | A profile never grants a capability. Capability comes from the vehicle | `DESIGNED` |
| `P7-FLEET-003` | A fleet screen lists known vehicles with identity, profile, last-seen and capability summary | `SPECIFIED-NOT-DESIGNED` â€” blocked on fleet identity format (`OPEN`) |
| `P7-FLEET-004` | Profile-keyed UI cannot be delivered through `CONSTANT` registries. See [03 Â§ The CONSTANT constraint](03-gui-information-architecture-and-screens.md#3-the-constant-registry-constraint) | `DESIGNED` |

Owner: Tab 3 (profile model), Tab 7 (profile-keyed UI), Tab 1 (the `CONSTANT` resolution).

### 5.10 Video and camera integration

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-VID-001` | QGC's video pipeline is reused. The three `createVideoReceiver`/`createVideoSink`/`releaseVideoSink` seams are overridden only as a set, or not at all | `DESIGNED` â€” not overridden today |
| `P7-VID-002` | Aerora overlays on video reuse the existing precedent (`ObstacleDistanceOverlayVideo`, `ProximityRadarVideoView`) | `DESIGNED` |
| `P7-VID-003` | Camera definitions may ship from Aerora resources via `getOfflineCameraDefinitionFile()` | `DESIGNED` |
| `P7-VID-004` | A video overlay never displays a safety value without its state class | `DESIGNED` |

Owner: Tab 7.

### 5.11 Flight logs, plots, playback and export

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-LOG-001` | QGC's log download, ULog/DataFlash plotting and replay are reused unchanged | `IMPLEMENTED` (upstream) |
| `P7-LOG-002` | Aerora-specific analysis screens register through `analyzePages()` | `DESIGNED` |
| `P7-LOG-003` | Export records which values were `REPORTED` and which were station-derived | `SPECIFIED-NOT-DESIGNED` |
| `P7-LOG-004` | Replay of an Aerora session reconstructs state classes, not just values | `SPECIFIED-NOT-DESIGNED` |

Owner: Tab 7 (screens), Tab 4 (state reconstruction).

### 5.12 Geofence, mission validation and pre-flight checks

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-CHK-001` | Geofence and rally points use QGC's planning and upload. The station does not enforce | `IMPLEMENTED` (upstream) |
| `P7-CHK-002` | Mission validation reports what it checked, what passed, and what it could not check | `DESIGNED` |
| `P7-CHK-003` | The pre-flight checklist is replaced wholesale via `QGCOptions::preFlightChecklistUrl()` â€” a clean seam, no fork | `DESIGNED` |
| `P7-CHK-004` | A check that cannot be evaluated is `UNKNOWN` and blocks nothing on its own; an operator decision is recorded instead | `DESIGNED` |
| `P7-CHK-005` | Checklist completion is recorded as audit evidence | `DESIGNED` â€” depends on `P7-AUDIT-002` |

Owner: Tab 7 (checklist), Tab 5 (mission validation).

### 5.13 Security, roles, permissions and audit

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-SEC-001` | Command authorization is evaluated at a single enforcement point in the core send path | `IMPLEMENTED-UNCOMPILED` |
| `P7-SEC-002` | An identity provider, role resolver and authorization policy exist as interfaces | `CONTRACT-ONLY` â€” `AeroraIdentityProvider`, `AeroraRoleResolver`, `AeroraAuthorizationPolicy` have no concrete implementation |
| `P7-SEC-003` | MAVLink signing key storage is protected per platform | `CONTRACT-ONLY` â€” `AeroraPlatformKeyProtector` has no backend on any of the five target platforms |
| `P7-SEC-004` | Security state is not QML-visible or QML-writable | `IMPLEMENTED-UNCOMPILED` â€” asserted by `AeroraUiSafetyTest` |
| `P7-SEC-005` | A security screen displays whether enforcement is active. It must be able to say "not enforced" | `VERIFIED` — `SCR-SEC-01`. `AeroraSceneAttachmentUITest` requires the screen to report *not enforced* in this build, and the derivation is written against the enforced enumerator and negated so a resolution failure lands the same way |

Owner: Tab 2.

### 5.14 Radio/connection management

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-RAD-001` | QGC's link layer (serial, UDP, TCP, Bluetooth, MockLink) is reused unchanged | `IMPLEMENTED` (upstream) |
| `P7-RAD-002` | Link state, quality and freshness are displayed per link, not aggregated into one bar | `VERIFIED` (state) / `OPEN` (quality) — `SCR-LIV-07` renders one row per link and `AeroraFlyViewOverlayUITest` holds the row list against the vehicle's own `linkNames`. Quality is **not** per link and is not presented as such: upstream counts message loss per vehicle, so the panel labels that figure as covering all links together rather than dividing an aggregate |
| `P7-RAD-003` | **Every Aerora radio capability is `DESIGNED`, not supported.** No radio hardware has been attached, so no claim of hardware support is made | `DESIGNED` â€” `D-004` deferred |
| `P7-RAD-004` | A designed-but-unverified interface is labelled as such in the UI as well as in the documents | `DESIGNED` |
| `P7-RAD-005` | Link redundancy and failover behaviour are not implemented and not displayed as available | `OPEN` |

Owner: Tab 1 (boundary), Tab 7 (display). `P7-RAD-003` and `P7-RAD-004` exist because this is the
area where "designed" most easily drifts into "supported" in a sales conversation.

### 5.15 Offline maps, local storage, optional cloud

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-DAT-001` | QGC's SQLite tile cache and offline map editor are reused unchanged | `IMPLEMENTED` (upstream) |
| `P7-DAT-002` | Aerora mission, profile and audit data persists locally with a documented schema and location | `SPECIFIED-NOT-DESIGNED` |
| `P7-DAT-003` | Full local-first operation. Cloud is additive | `DESIGNED` |
| `P7-DAT-004` | No automatic outbound transmission of logs, telemetry or code | `DESIGNED` |
| `P7-DAT-005` | Local data retention, pruning and export are operator-controlled | `SPECIFIED-NOT-DESIGNED` |

Owner: Tab 1 (storage architecture), Tab 7 (surfaces).

### 5.16 Simulation and test interfaces

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-SIM-001` | QGC's MockLink and SITL support are reused for station-side development | `IMPLEMENTED` (upstream) |
| `P7-SIM-002` | A simulated source is labelled as simulated in every display that shows its values | `DESIGNED` |
| `P7-SIM-003` | A simulated vehicle cannot satisfy a capability requirement for an authorization decision | `DESIGNED` |
| `P7-SIM-004` | Test mocks are compiled only under `QGC_BUILD_TESTING`, and `AeroraVehicleMock` additionally requires `QGC_UNITTEST_BUILD` | `IMPLEMENTED-UNCOMPILED` |

Owner: Tab 8.

### 5.17 Settings, localization, accessibility, operator preferences

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-SET-001` | Aerora settings are delivered through the documented custom-dir JSON overlay plus `registerCustomSettings()`. No upstream settings edit | `DESIGNED` â€” seam at `src/AppSettings/CMakeLists.txt:25-40` |
| `P7-SET-002` | All user-visible strings are `qsTr()`-wrapped and reach the existing translation pipeline | `DESIGNED` |
| `P7-SET-003` | No hard-coded size or colour; `ScreenTools` and `QGCPalette` only | `DESIGNED` â€” enforced by `.kiro/tools/aerora-ui-static-check.ps1` for size/colour literals |
| `P7-SET-004` | Accessibility: keyboard reachability, focus order, contrast of state indicators, and non-colour-only encoding of the three non-value states | `DESIGNED` |
| `P7-SET-005` | Full WCAG conformance is **not claimed**. It requires manual assistive-technology testing and expert review, neither of which has occurred | `OPEN` |

Owner: Tab 7.

### 5.18 Help, diagnostics, error reporting, documentation

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-HELP-001` | A diagnostics screen reports overlay state: which services attached, which interfaces are available, whether authorization is enforced | `VERIFIED` — answered by `SCR-SHL-01` (attachment and posture) and `SCR-SEC-01` (enforcement scope). `SCR-HLP-01` is superseded rather than deferred; a second screen carrying the same readings would drift from the first |
| `P7-HELP-002` | An error report is assembled locally and transmitted only on explicit operator action | `VERIFIED` — `SCR-HLP-03`. Satisfied structurally: the screen has no network or file path at all, and its one action copies to the station clipboard |
| `P7-HELP-003` | Operator documentation covers the state vocabulary, because the vocabulary is the product's main claim | `SPECIFIED-NOT-DESIGNED` |
| `P7-HELP-004` | The station can state its own build provenance: base commit, overlay commit, profile set, enforcement state | `VERIFIED` (3 of 4 fields) — `SCR-SHL-04`. Commit, profile set and enforcement state are reported; base and overlay commit are the **same** commit because the overlay shares the git tree. The **upstream** commit the fork diverged from is genuinely unrecorded — no remote, merge-base or vendored revision is captured — so `AeroraProvenance.upstreamBaseCommitKnown` is a compile-time `false` and the screen renders *not recorded* rather than substituting the fork's own hash |

Owner: Tab 1 (provenance), Tab 7 (screens).

### 5.19 Future product profiles

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-PROF-001` | Firefighting, agriculture and inspection are profiles over the same shell, services and interfaces | `DESIGNED` |
| `P7-PROF-002` | A new profile requires no change to the shell, the telemetry model or the authorization point | `DESIGNED` |
| `P7-PROF-003` | No profile-specific behaviour is implemented before its product requirements and hardware evidence exist | `FORBIDDEN-UNTIL` â€” `D-007` |
| `P7-PROF-004` | A profile declares the capabilities it *expects*; mismatch against what the vehicle reports is displayed, not reconciled silently | `DESIGNED` |

Owner: Tab 3 (model), Tab 1 (profile boundary).

### 5.20 Packaging, installation, update, deployment

| ID | Requirement | Status |
| --- | --- | --- |
| `P7-PKG-001` | The overlay builds through the documented custom-build path with `-DQGC_CUSTOM_DIR=aerora` | `IMPLEMENTED-UNCOMPILED` â€” never configured |
| `P7-PKG-002` | A CI leg builds the overlay with `QGC_BUILD_TESTING=ON` and runs the Aerora test labels. `Q-035` | `OPEN` |
| `P7-PKG-003` | Installers for the target platforms, with provenance recorded in the artefact | `SPECIFIED-NOT-DESIGNED` |
| `P7-PKG-004` | Update strategy does not silently change authorization behaviour or enforcement state | `DESIGNED` |
| `P7-PKG-005` | Deployment documentation states which features are hardware-verified and which are designed | `DESIGNED` |

Owner: Tab 8.

## 6. User journeys

Seven journeys. Each names the screens it crosses, the services it needs, and where it currently
stops. "Stops at" is the honest end of the journey today, not a plan.

### 6.1 Painting operation

```text
Fleet â†’ select vehicle â†’ profile resolves â†’ Vehicle Diagnostics (capability check)
  â†’ Mission Studio (surface definition, standoff, coverage pattern)
  â†’ Validation (what was checked / what could not be)
  â†’ Pre-flight checklist â†’ upload â†’ Mission Verify (two-snapshot agreement)
  â†’ Live Operations (state, payload supervision, power, AI proposals)
  â†’ post-flight: Log + Event Timeline + Audit export
```

| Needs | Status |
| --- | --- |
| Industrial-surface complex item | `SPECIFIED-NOT-DESIGNED`, blocked on `Q-021`/`Q-023` |
| Painting reported-value set | `SPECIFIED-NOT-DESIGNED`, blocked on the payload ICD |
| Payload supervision display | `IMPLEMENTED-UNCOMPILED` (model), `DESIGNED` (screen) |
| Mission verify | `IMPLEMENTED-UNCOMPILED` |

**Stops at**: surface definition. There is no Aerora complex mission item, so the journey cannot
start from a surface today.

### 6.2 Vehicle setup and calibration

```text
Fleet â†’ Vehicle Config â†’ Aerora setup pages (via AeroraAutoPilotPlugin)
  â†’ sensor/calibration page â†’ authorization decision â†’ calibration
  â†’ Diagnostics (what attached, what reported)
```

| Needs | Status |
| --- | --- |
| `AeroraAutoPilotPlugin` | `DESIGNED` â€” seam confirmed, no implementation |
| Calibration authorization | `IMPLEMENTED-UNCOMPILED` (ADR-015) |
| Authorization policy | `OPEN` â€” `Q-029`. The gate exists and permits everything |

**Stops at**: the authorization decision is made by a gate with no policy. The seam is right; the
answer is unconditional.

### 6.3 Manual flight

```text
Link â†’ Live Operations â†’ mode display (EFFECTIVE) â†’ manual input
  â†’ vehicle-reported state only â†’ land â†’ log
```

| Needs | Status |
| --- | --- |
| QGC manual control, joystick, virtual joystick | `IMPLEMENTED` (upstream) |
| `EFFECTIVE`-vs-`REQUESTED` mode display | `DESIGNED`; `AeroraTelemetryState` is `IMPLEMENTED-UNCOMPILED` |
| Aerora live overlay | `DESIGNED` |

**Stops at**: nothing structural. This is the journey closest to working, because it is mostly
upstream QGC with an Aerora display layer on top.

### 6.4 Supervised autonomy

```text
Live Operations â†’ proposal arrives (PROPOSED, with source + basis + freshness)
  â†’ operator reviews â†’ approve â†’ authorization decision â†’ transmit (REQUESTED)
  â†’ vehicle accepts or refuses â†’ display EFFECTIVE or refusal
  â†’ every step recorded
```

| Needs | Status |
| --- | --- |
| Proposal model and lifecycle | `SPECIFIED-NOT-DESIGNED` |
| GPU/autonomy interface | `OPEN` â€” no endpoint or schema may be invented |
| Approval surface | `DESIGNED`, deliberately disabled |
| Authorized command path | **None exists.** `Q-029` then ADR-014 |
| Audit of the four events | `CONTRACT-ONLY` |

**Stops at**: the approval button. It is deliberately absent rather than present-and-inert â€” see
[03 Â§ Why there are no action buttons yet](03-gui-information-architecture-and-screens.md#9-why-there-are-no-action-buttons-yet).

### 6.5 Mission planning

```text
Mission Studio â†’ 2D plan or 3D (Viewer3D) â†’ pattern or surface
  â†’ terrain follow â†’ validation â†’ save â†’ upload â†’ verify
```

| Needs | Status |
| --- | --- |
| QGC waypoint/pattern/geofence/terrain planning | `IMPLEMENTED` (upstream) |
| `Viewer3D` reuse | `DESIGNED` |
| Aerora surface item | `SPECIFIED-NOT-DESIGNED` |
| `geoMapVisualQML()` for the second map engine | `DESIGNED` â€” a known omission trap |

**Stops at**: the Aerora-specific item. Standard planning works today because it is unmodified QGC.

### 6.6 Post-flight analysis

```text
Analyze â†’ Log Viewer (upstream) â†’ Aerora Session Review
  â†’ event timeline (state transitions + operator actions + authorization outcomes)
  â†’ export with provenance labelling
```

| Needs | Status |
| --- | --- |
| QGC log download, plotting, replay | `IMPLEMENTED` (upstream) |
| Event timeline | `SPECIFIED-NOT-DESIGNED` |
| State-class reconstruction on replay | `SPECIFIED-NOT-DESIGNED` |
| Provenance-labelled export | `SPECIFIED-NOT-DESIGNED` |
| Durable audit record to review | `OPEN` â€” `Q-037` |

**Stops at**: there is no durable Aerora record to analyse. The audit sink is an interface.

### 6.7 Fleet management

```text
Fleet â†’ vehicle list (identity, profile, last-seen, capability summary)
  â†’ profile assignment â†’ per-vehicle diagnostics â†’ utilisation
```

| Needs | Status |
| --- | --- |
| Profile registry | `IMPLEMENTED-UNCOMPILED` |
| Fleet identity format | `OPEN` â€” and MAVLink sysid is explicitly not it |
| Persistence of fleet records | `SPECIFIED-NOT-DESIGNED` â€” `P7-DAT-002` |
| Multi-vehicle support | `IMPLEMENTED` (upstream `MultiVehicleManager`) |

**Stops at**: identity. Without an agreed fleet identity there is nothing stable to key a fleet
record on across sessions.

### Journey coverage summary

| Journey | Reachable end-to-end today | Primary blocker |
| --- | --- | --- |
| Manual flight | Closest â€” upstream QGC plus display layer | None structural |
| Mission planning (standard) | Yes, as upstream QGC | None |
| Mission planning (industrial surface) | No | `Q-021` / `Q-023` |
| Vehicle setup | Partially | `AeroraAutoPilotPlugin` absent; `Q-029` |
| Painting operation | No | Surface item + payload reported-value set |
| Supervised autonomy | No | No authorized command path; GPU interface `OPEN` |
| Post-flight analysis | Partially, as upstream QGC | No durable Aerora record |
| Fleet management | No | Fleet identity `OPEN` |

## 7. Completion plan

### Phase A â€” Product definition (this pass)

| Deliverable | State |
| --- | --- |
| Master PRD | This document |
| Reference benchmark | [02](02-gcs-reference-benchmark.md) |
| Screen and navigation inventory | [03](03-gui-information-architecture-and-screens.md) |
| User journeys | [Â§6](#6-user-journeys) |
| Requirements â†’ component â†’ test traceability | [06](06-requirements-to-test-traceability.md) |
| Dependencies, blockers, OPEN decisions | [05](05-implementation-dependency-graph.md), [08](08-risk-register-and-open-decisions.md) |

### Phase B â€” Architecture freeze

Freeze, in this order, because each constrains the next:

1. **Navigation model** â€” the registry set, and whether `SelectViewDropdown.qml` is forked. [03 Â§ D-NAV-01](03-gui-information-architecture-and-screens.md#d-nav-01--the-top-level-entry-point)
2. **Service boundaries** â€” which service owns which value, and the `src/Supervisory/` absorption call
3. **View-model contracts** â€” one typed property per service; no string-keyed lookup in QML
4. **Product profiles** â€” profile resolution timing against the `CONSTANT` registry constraint
5. **Command/authorization boundary** â€” ADR-014 scope, then the role set

Phase B exit criterion: every screen in [03](03-gui-information-architecture-and-screens.md) maps to
a named seam or a named future interface, with no screen in an undecided state.

### Phase C â€” Implementation

Dependency order is [05](05-implementation-dependency-graph.md). Rules:

- Small reviewable commits on `aerora/phase7-product-completion`
- Conventional Commits; type drives release automation
- Tests authored alongside each implementation, registered in `aerora/CMakeLists.txt`
- Core (`src/`) changes stay minimal and each one is individually documented
- No generated dialect, no invented hardware response, no unverified API
- One tab per file; no two tabs editing the same file or root cause concurrently

### Phase D â€” Pre-build completion audit

| Step | Content |
| --- | --- |
| 1 | Inventory every screen, service, requirement, test and unresolved interface |
| 2 | Run the available static checks (`.kiro/tools/*.ps1`) and record results |
| 3 | Classify every feature: `IMPLEMENTED-UNCOMPILED`, `CONTRACT-ONLY`, `DESIGNED`, `BLOCKED`, `OPEN` |
| 4 | Produce the exact future build, CTest, QML and runtime verification steps |
| 5 | State explicitly that production readiness is **not** claimed |

## 8. Definition of done, per status

A requirement is only "done" against the status it claims. These are the evidence bars.

| Claim | Evidence bar |
| --- | --- |
| `DESIGNED` | A named seam, a named owner, acceptance criteria, and a test that would detect regression |
| `IMPLEMENTED-UNCOMPILED` | Source complete, declared in a manifest, passes the static checks, test authored and registered |
| `VERIFIED` | `just build` succeeds, `just lint` passes, `ctest -R <name>` passes, and for hardware claims, observed on hardware |

The third row is unreachable on this host. Phase 7 therefore ends at Phase D, and **production
readiness is not claimed**.

## 9. What this document does not decide

Recorded so the absence is deliberate rather than overlooked. Each is in
[08](08-risk-register-and-open-decisions.md) with an owner.

| Not decided | Owner | Blocks |
| --- | --- | --- |
| The authorised operator role set (`Q-029`) | Product + operations | All authorization, `P7-FLY-004`, journey 6.4 |
| Outbound authorization scope (ADR-014) | Architecture | Any general authorization claim |
| Freshness budgets (`Q-036`) | Product | `P7-MOD-013`, `P7-TEL-002` |
| Aerora mission command / complex item set (`Q-021`, `Q-023`) | Product | `P7-MIS-002`, journeys 6.1 and 6.5 |
| Power thresholds, arbitration, transitions | Product + engineering | `P7-PWR-003..006` |
| Fleet identity format | Product | `P7-FLEET-003`, journey 6.7 |
| Audit durability and tamper-evidence (`Q-037`) | Security | `P7-AUDIT-002`, journey 6.6 |
| `src/FactSystem` / `PARAM_SET` ownership | Programme | `P7-AUTH-003` |
| GPU/autonomy interface specification | Architecture + external | `P7-AI-004`, journey 6.4 |
| Build host (`D-006`) | User | Every `VERIFIED` claim |
| Aerora git origin (`D-005`) | User | Any push |
| Product authorization (`D-007`) | External | Every product feature |

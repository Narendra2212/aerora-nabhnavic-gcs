# Open Questions

Every item that must be answered before implementation can proceed. Nothing here has been resolved
by assumption.

Each question records who must answer it, what it blocks, and what evidence exists. Questions are
grouped by domain and carry stable identifiers for traceability.

## Status legend

| Status | Meaning |
| --- | --- |
| `OPEN` | Requires an Aerora engineering decision |
| `UNKNOWN` | Requires investigation; may resolve without a decision |
| `BLOCKED` | Cannot be answered until another item resolves |

## Process and governance

### Q-001 Which output structure governs this analysis?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora engineering lead |
| Blocks | Ratification of this document set |

The directive that produced this set nominates sixteen documents under
`.kiro/specs/00-qgc-baseline-analysis/`. The v4.1 baseline's own
`.kiro/specs/00-qgc-baseline-analysis/design.md` nominates five documents under `docs/`:
`QGC_BASELINE_REPORT.md`, `QGC_ARCHITECTURE_MAP.md`, `QGC_DEPENDENCY_INVENTORY.md`,
`QGC_EXTENSION_POINT_MAP.md`, `QGC_BUILD_BASELINE.md`. The baseline also already contains
`docs/05_QGC_BASELINE_ANALYSIS.md`.

This set followed the directive. Three resolutions are available: ratify this structure and update
spec 00's `design.md` in the same change; relocate this content into the five nominated documents;
or maintain both, which would create the contradictory documentation that change control forbids.

**Recommendation:** ratify this structure and update `design.md`, so the two stop disagreeing.

### Q-002 Should the baseline's `.kiro/` scaffold be promoted to the workspace root?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora engineering lead |
| Blocks | Aerora steering rules taking effect |

The v4.1 baseline ships a `.kiro/` scaffold containing `AGENTS.md`, seven steering files
(`licensing`, `project`, `qgc`, `safety`, `security`, `testing`, `workflow`), and two specs. It is
**not** installed at the workspace root, so those rules are currently inert — the active
`AGENTS.md` is QGroundControl's own.

Promoting it is a deliberate act with consequences: Aerora steering would then govern all agent
work in this repository, and would need reconciling with QGC's own `AGENTS.md`, which remains
binding for anything touching QGC source.

### Q-003 What is the fork and upstream-tracking strategy?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora configuration management |
| Blocks | Any commit; Phase 5 entirely |
| Evidence | R-008, R-014 |

`origin` currently points at upstream `mavlink/qgroundcontrol`. Required decisions: the fork
location, the Aerora branch naming scheme, whether Aerora rebases or merges from upstream, the
cadence, and who owns conflict resolution for the enumerated core edits.

Record `9459a4b57986f40df8bdc193d5453c0596fbb416` as the fork point.

## Legal and commercial

### Q-004 Apache 2.0 with commercial Qt, or GPL v3 with open-source Qt?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora commercial and legal |
| Blocks | **The entire delivery model.** Phase 5 should not start without this |
| Evidence | R-001, [dependency-map.md](dependency-map.md) |

`.github/COPYING.md` states the Apache path requires a commercial Qt licence and permits
proprietary derivatives; the GPL path is compatible with open-source Qt and requires conveying
complete corresponding source under GPL v3.

This is the highest-impact open item in the programme. It determines whether the Aerora overlay can
remain proprietary.

### Q-005 Have all third-party dependency licences been audited?

| Field | Value |
| --- | --- |
| Status | `UNKNOWN` |
| Owner | Aerora legal |
| Blocks | Distribution |
| Evidence | R-020 |

Roughly twenty-five CPM packages plus Qt and GStreamer. Their upstream licence texts were **not**
read during this analysis. GStreamer's LGPL status is particularly relevant to the dynamic-plugin
layout under `cmake/GStreamer/`.

### Q-006 Will Aerora remove all QGroundControl branding and artwork?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora legal and product |
| Blocks | Distribution |

`docs/en/qgc-dev-guide/contribute/licences.md` covers documentation, artwork, and images
separately from code. **The code licence does not grant the QGroundControl marks or artwork.** The
overlay mechanism makes replacement straightforward; the obligation is to actually do it and to
confirm completeness.

## Safety and authority

### Q-007 What is the Aerora failsafe behaviour?

| Field | Value |
| --- | --- |
| Status | `OPEN` — **deliberately deferred by the v4.1 baseline** |
| Owner | Aerora flight-controller engineering, via the approved Simulink implementation |
| Blocks | `FirmwarePlugin` named flight-mode accessors; any GCS failsafe annunciation; Phase 8 |
| Evidence | Baseline `PROJECT_STATUS.md` and `README.md` both state final failsafe behaviour is not frozen |

Specifically undefined, and **must not be inferred**: RTL behaviour, LAND behaviour, LOITER
behaviour, termination behaviour, emergency transition behaviour, and communication-loss response.

Consequence for implementation: the Aerora `FirmwarePlugin` named flight-mode accessors
(`rtlFlightMode`, `landFlightMode`, `pauseFlightMode`, `missionFlightMode`, and the rest) **must
not be populated with guessed Aerora mode names**. QGC uses them to decide what to command, so a
guess becomes a command.

Note the boundary this analysis established: QGC's comm-loss handling is observation and
annunciation only, not control. Aerora must preserve that boundary regardless of how failsafe is
eventually defined.

### Q-008 How is a proposed autonomy command distinguished from an effective command?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora systems architecture |
| Blocks | The authority model; UI design |
| Evidence | [aerora-extension-map.md](aerora-extension-map.md) |

The baseline requires the GCS distinguish operator command, autonomous proposal, GPU command,
FC-accepted command, effective command, and failsafe command, and never falsely represent a
proposal as effective.

QGC has partial raw material: `MavCommandQueue` acknowledgement tracking distinguishes sent from
accepted; `requestOperatorControl` and the `gcsControlStatusFlags` properties provide a genuine
multi-GCS authority handshake. **QGC has no concept of a proposal at all.**

Required decisions: what MAVLink messages carry an Aerora autonomy proposal; whether a proposal
requires operator acceptance, flight-controller acceptance, or both; how freshness is expressed and
what staleness threshold applies; and what the UI shows when a proposal and an effective command
disagree.

### Q-009 What is the payload emergency-stop behaviour?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora systems and payload engineering |
| Blocks | Payload module design; operator UI |

The baseline lists payload emergency stop among the items whose behaviour must not be inferred.
Required: what the operator action does, whether it is GCS-originated or a hardware interlock, what
the aircraft does in response, and whether it is distinct from a flight-mode change.

### Q-010 Should Aerora mandate MAVLink signing by default?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora security architecture |
| Blocks | Security baseline; transport design |
| Evidence | R-006 |

Signing is opt-in in QGC and the unsigned-acceptance policy is configurable. An unsigned stream is
accepted by default.

Required: whether Aerora sets `UnsignedAcceptancePolicy::Strict` as the default; how keys are
provisioned and distributed across a fleet; and whether unsigned operation is permitted at all in
an Aerora configuration.

### Q-011 Do the Aerora transports provide link-layer security?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora communications and security architecture |
| Blocks | Transport design; security baseline |
| Evidence | R-006 |

No QGC transport provides encryption. Whether Aerora's LoRa, FHSS24, and RF433 links provide
confidentiality or integrity at the link layer — in hardware or in the Aerora transport
implementation — determines whether MAVLink signing is the only protection.

## Communications

### Q-012 What is the Aerora transport granularity?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora communications architecture |
| Blocks | The transport core edits; settings UI design |
| Evidence | R-003, [communication-architecture.md](communication-architecture.md) |

Two options with a real trade-off:

| Option | Core edits | Settings UI |
| --- | --- | --- |
| One `LinkType` per Aerora radio | Paid once per radio | Idiomatic — each radio gets its own settings page and type entry |
| One Aerora `LinkType` with a sub-type discriminator in the configuration | Paid exactly once in total | Less idiomatic — one page with a radio selector |

The second minimises the permanent upstream merge burden, which R-003 identifies as the lasting
cost. The first matches QGC conventions.

### Q-013 Does FHSS24 and RF433 hardware actually exist?

| Field | Value |
| --- | --- |
| Status | `UNKNOWN` |
| Owner | Aerora hardware engineering |
| Blocks | Transport scope |
| Evidence | The baseline's own `docs/46_PRODUCT_BASELINE_ANALYSIS.md:38-40` states FHSS24 and RF433 must be confirmed separately before being called onboard hardware |

This analysis confirmed that **none of LoRa, FHSS24, or RF433 has any implementation in QGC**, and
that all references are in Aerora specification documents. The baseline itself flags the hardware
as unconfirmed. Building three transports for hardware that may not exist would be wasted work.

Required: schematic, PCB, and bench-verification evidence per radio, distinguishing schematic
present, PCB implemented, firmware implemented, bench verified, and flight verified.

### Q-014 Can an Aerora radio present as a serial device?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora hardware and communications |
| Blocks | Potentially eliminates Q-012 and R-003 entirely |

This is the highest-value question in the communications domain. QGC carries SiK and RFD telemetry
radios over the **existing** `SerialLink` transport, because they present as USB-serial. The
SiK-specific code is only VID/PID identification, auto-connect baud selection, a radio-link frame
hint, and a firmware-flash tool. **There is no `TypeSiKRadio` link type.**

If an Aerora radio presents as a serial or USB-serial device to the GCS, Aerora may need only
identification and configuration work rather than a new transport — eliminating all six core edits.

### Q-015 What is the worst-case concurrent link count?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora systems architecture |
| Blocks | Channel budget validation |
| Evidence | R-013 |

Fifteen MAVLink channels are allocatable per QGC instance. One per link, plus one consumed by
ArduPilot v1 re-encoding if the APM plugin is active. Required: links per vehicle times vehicles
per GCS instance, worst case.

## Vehicle and fleet

### Q-016 What is the Aerora fleet vehicle identity?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora systems and cloud architecture |
| Blocks | Fleet module; cloud vehicle records; audit trail |
| Evidence | R-017 |

QGC identifies a vehicle by MAVLink system id only — a small, locally assigned, reusable integer.
`vehicleUID` properties exist but are not used for matching.

Required: the durable identifier, where it originates (the baseline mentions an ATECC608A secure
element, which may be relevant), how it is read, and how system id maps to it.

### Q-017 Is Aerora single-stack or multi-stack?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora product |
| Blocks | Firmware plugin factory configuration |

If Aerora vehicles only ever run Aerora firmware, setting `QGC_DISABLE_PX4_PLUGIN_FACTORY` and
`QGC_DISABLE_APM_PLUGIN_FACTORY` avoids the silent factory-conflict hazard in R-010 and simplifies
the UI. If Aerora must also support PX4 or ArduPilot vehicles — for development, SITL, or mixed
fleets — both factories must remain and the conflict must be managed.

### Q-018 Does the Aerora flight controller report a distinct MAV_AUTOPILOT value?

| Field | Value |
| --- | --- |
| Status | `UNKNOWN` |
| Owner | Aerora flight-controller engineering |
| Blocks | Firmware plugin selection |

`FirmwarePluginManager::_findPluginFactory()` selects on firmware class derived from
`MAV_AUTOPILOT`. If the Aerora FC reports `MAV_AUTOPILOT_PX4` or `MAV_AUTOPILOT_GENERIC`, the
Aerora factory must claim that class — which is exactly where R-010's conflict arises.

## Mission and payload

### Q-019 How is Aerora payload state preserved across mission resume?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora systems and payload engineering |
| Blocks | Resume support; a safety-relevant gap |
| Evidence | R-002 |

`includedResumeCommands` is a hardcoded core allow-list containing only camera, gimbal, ROI, speed,
and fence commands. Aerora payload state commands would be silently dropped.

Options: extend the core allow-list as an enumerated core edit; design the pattern to re-establish
payload state itself at resume; or declare resume unsupported for painting missions and document
it. The third is honest but operationally poor.

Note the second hazard in the same function: **resume is refused entirely if the mission contains
any `MAV_CMD_DO_JUMP`.**

### Q-020 Can a custom build shadow an existing `:/json` resource alias?

| Field | Value |
| --- | --- |
| Status | `UNKNOWN` — **investigable without a decision** |
| Owner | Aerora software engineering |
| Blocks | Whether Aerora payload MAV_CMDs can appear in the Plan UI without a core edit |

`MissionCommandTree::_collapseHierarchy` only creates new command entries when processing the
**base** list, which resolves to `src/MissionManager/MavCmdInfoCommon.json`. Override files cannot
add commands.

If the overlay's resource-alias mechanism can shadow `:/json/MavCmdInfoCommon.json` — plausible
given the URL interceptor and alias precedence documented in
[plugin-architecture.md](plugin-architecture.md), but **not verified** — Aerora could add payload
commands with zero core edits. This is cheap to test and worth doing early.

### Q-021 Does the Aerora pattern need survey-style insert behaviour?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora software engineering |
| Blocks | Whether a core edit is needed for the mission pattern |
| Evidence | R-018 |

`MissionController::_insertComplexMissionItemWorker()` hardcodes pattern identity by `qobject_cast`
against the three stock survey-style classes, which triggers automatic camera-mode and
gimbal-pointing behaviour.

For a painting payload that behaviour may be irrelevant. Accepting the default keeps the pattern a
zero-core-edit addition.

### Q-022 Must a painting plan survive download from the vehicle?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora operations and software engineering |
| Blocks | Whether an Aerora `scanForItems` is required |
| Evidence | R-012 |

Only landing patterns are reconstructed from a downloaded mission. An Aerora pattern would appear
as a flat waypoint list.

If the authoritative plan is always the saved `.plan` file, no work is needed — but that is a
workflow constraint to document and accept.

### Q-023 What are the Aerora payload actuator interfaces?

| Field | Value |
| --- | --- |
| Status | `OPEN` — the baseline explicitly defers this |
| Owner | Aerora payload engineering |
| Blocks | Payload module implementation |

The baseline states final actuator interfaces must not be assumed. The payload abstraction in
[aerora-extension-map.md](aerora-extension-map.md) deliberately stops at the state-name boundary:
paint supply, tank, pump, spray, hose, payload power, gimbal, painting mission state, payload
safety state.

Required per state: the MAVLink representation, whether it is commandable or observation-only, and
units and bounds for the Fact metadata.

## Power and tether

### Q-024 What is the frozen power architecture?

| Field | Value |
| --- | --- |
| Status | `OPEN` — the baseline lists these as controlled items |
| Owner | Aerora electrical engineering |
| Blocks | Power and tether FactGroup definition; any configuration-state UI |

The baseline explicitly records as not frozen: the 480 V source type (AC versus DC), isolation
topology, converter rating, cable protection, connector system, grounding, interlocks, and
source-transition behaviour.

The GCS cannot report a configuration it cannot define. This analysis treated the power domains as
boundaries only, with no transition behaviour implied.

### Q-025 How does the GCS learn the tether versus battery configuration?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora systems engineering |
| Blocks | Configuration display; pre-flight checklist |

The baseline defines two configurations — tethered with ground paint and the onboard tank detached,
and battery with the onboard tank attached — but not how the aircraft reports which is active.

Required: whether configuration is sensed or operator-declared, which MAVLink message carries it,
and what the GCS shows when sensed and declared values disagree. The last case matters: a mismatch
is exactly the kind of condition a supervisory GCS exists to surface.

### Q-026 Is a tether-to-battery transition in flight supported?

| Field | Value |
| --- | --- |
| Status | `OPEN` — the baseline says it must not be assumed |
| Owner | Aerora electrical and systems engineering |
| Blocks | Power state machine; operator UI |

The baseline states the aircraft must not be assumed able to transition seamlessly between tether
and battery power. If a transition is supported, the GCS needs to represent the states and the
transition. If not, the GCS should make the constraint visible.

## Security and cloud

### Q-027 What consumes Qt6::HttpServer, and is it authenticated?

| Field | Value |
| --- | --- |
| Status | `UNKNOWN` — **investigable without a decision** |
| Owner | Aerora software engineering |
| Blocks | Security baseline claims |
| Evidence | R-016 |

`Qt6::HttpServer` is a required Qt component and is linked, but the consuming feature was not
identified. An unauthenticated network-exposed endpoint would be a security finding. Cheap to
resolve and should be done before any security claim.

### Q-028 Does QGC apply any custom TLS policy?

| Field | Value |
| --- | --- |
| Status | `UNKNOWN` |
| Owner | Aerora software engineering |
| Blocks | Security baseline completeness |

A search found no `QSslConfiguration`, `sslErrors`, or `ignoreSslErrors` usage in `src/`, implying
Qt defaults with no pinning and no explicit minimum TLS version. **The search was truncated by
unrelated matches**, so this is strongly indicated rather than proven.

### Q-029 What is the Aerora operator role model?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora product and security architecture |
| Blocks | The security module; UI gating |
| Evidence | R-004 |

No authentication or authorisation exists in QGC. Required: the role set, what each role may do,
whether roles are local or cloud-managed, whether the GCS must function with roles enforced while
offline, and how role state interacts with the existing `showAdvancedUI` toggle — which is a UI
convenience, not authorisation, and must not be mistaken for one.

### Q-030 Which cloud dependencies are permitted, and how does each degrade?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora cloud architecture |
| Blocks | The cloud module |

The baseline requires that loss of internet, AWS, cloud API, cloud storage, or cloud telemetry must
not imply loss of aircraft control, and that all cloud dependencies be documented explicitly.

Required per cloud-touching feature: what it needs, what happens when the cloud is unreachable,
whether it blocks or queues, and whether any flight-relevant information is cloud-only. The last
one is the trap — a feature that is technically optional but operationally required becomes a
hidden dependency.

### Q-031 Is video required at all?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora product |
| Blocks | GStreamer dependency scope; packaging; licence surface |

`QGC_ENABLE_GST_VIDEOSTREAMING` is ON by default. If the painting UAV does not need video, turning
it OFF removes a large LGPL surface and simplifies packaging substantially.

## Platform and build

### Q-032 Which platforms must Aerora support?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora product |
| Blocks | Build matrix; signing work; test matrix |
| Evidence | R-015 |

QGC supports Windows, macOS, Linux, Android, and iOS. Signing is implemented only for macOS and
iOS. Each additional platform adds build, signing, and test cost.

### Q-033 Is an air-gapped or fully reproducible build required?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora configuration management |
| Blocks | Build infrastructure design |
| Evidence | R-011 |

CPM fetches dependencies at configure time, so a clean build needs network access unless the CPM
source cache is pre-populated. Two dependencies use floating references, which defeats
reproducibility outright.

If reproducibility is a compliance requirement, both the cache strategy and the pinning fixes
become mandatory rather than advisable.

### Q-034 Is there a splash-screen requirement?

| Field | Value |
| --- | --- |
| Status | `OPEN` |
| Owner | Aerora product |
| Blocks | Branding scope |

**No splash mechanism exists in QGC** — searched, not found. Aerora splash branding would be a
platform launch image, or new work.

## Summary

| Domain | Count | Identifiers |
| --- | --- | --- |
| Process and governance | 3 | Q-001 to Q-003 |
| Legal and commercial | 3 | Q-004 to Q-006 |
| Safety and authority | 5 | Q-007 to Q-011 |
| Communications | 4 | Q-012 to Q-015 |
| Vehicle and fleet | 3 | Q-016 to Q-018 |
| Mission and payload | 5 | Q-019 to Q-023 |
| Power and tether | 3 | Q-024 to Q-026 |
| Security and cloud | 5 | Q-027 to Q-031 |
| Platform and build | 3 | Q-032 to Q-034 |
| **Total** | **34** | |

### Cheap to resolve, high value

These four need investigation rather than a decision, and each could materially reduce scope:

| Item | Why it matters |
| --- | --- |
| Q-014 | Could eliminate all six transport core edits and R-003 entirely |
| Q-020 | Could allow Aerora MAV_CMDs in the Plan UI with zero core edits |
| Q-027 | Closes a security unknown before any claim is made |
| Q-013 | Could remove two of three transports from scope |

### Blocking Phase 5

| Item | Reason |
| --- | --- |
| Q-004 | Determines whether the Aerora overlay can be proprietary |
| Q-003 | No commit should be made to the current `origin` |
| Q-012 | Determines the transport core-edit footprint |
| Q-007 | Flight-mode accessors cannot be written without it |

### Blocking Phase 8

Q-007, Q-024, Q-025, and Q-026 all require engineering decisions that do not yet exist, and all
bear on flight and operational validation.

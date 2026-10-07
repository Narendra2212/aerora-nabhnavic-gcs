# Traceability Foundation

The requirement identifier scheme and the traceability chain structure for the Aerora programme.

This document establishes the **foundation** — the scheme, the rules, and the seeded entries that
follow from this analysis. It does not attempt to enumerate Aerora requirements, because most do
not yet exist and inventing them would violate the governing rule of this programme.

## The chain

Every significant Aerora requirement must eventually be traceable along the full chain.

```text
REQ-ID
   ↓
Specification          which spec document states it
   ↓
Architecture component which architectural element realises it
   ↓
Implementation         which source file or module implements it
   ↓
Test                   which test verifies it
   ↓
Evidence               the artefact proving verification passed
```

A requirement with a gap in this chain is incomplete, not merely undocumented. A requirement with
no test is unverifiable. A requirement with no evidence is unverified.

## Identifier scheme

```text
AERORA-<DOMAIN>-REQ-<NNN>
```

| Element | Rule |
| --- | --- |
| `AERORA` | Fixed prefix |
| `<DOMAIN>` | A domain code from the table below. Fixed set — extending it is a change-control action |
| `REQ` | Fixed. Other artefact types use their own codes, below |
| `<NNN>` | Three-digit, zero-padded, allocated sequentially within a domain, **never reused** |

### Domain codes

| Code | Domain | Governing specification |
| --- | --- | --- |
| `GCS` | GCS application, general | `02-system-architecture` |
| `MAV` | MAVLink protocol, dialect, messaging | `03-mavlink` |
| `COMM` | Communications, transports, link management | `04-communications` |
| `SAFE` | Safety, authority, failsafe boundaries | `05-safety` |
| `PAYLOAD` | Payload abstraction and the painting payload | `06-payload` |
| `PWR` | Power, tether, battery | `06-payload` or a dedicated power spec |
| `AWS` | Cloud, fleet, telemetry ingestion | `07-cloud-aws` |
| `UI` | Operator interface and UX | `08-ui-ux` |
| `SEC` | Security, authentication, authorisation, audit | `09-security` |
| `TEST` | Verification and validation | `10-testing` |
| `MISSION` | Mission planning and execution | `02-system-architecture` or a dedicated mission spec |
| `VEH` | Vehicle model, parameters, firmware abstraction | `02-system-architecture` |
| `BUILD` | Build, packaging, configuration management | `02-system-architecture` |
| `COMPLY` | Regulatory and DGCA compliance readiness | A dedicated compliance spec |

### Other artefact identifiers

| Prefix | Artefact | Allocated in |
| --- | --- | --- |
| `AERORA-<DOMAIN>-REQ-<NNN>` | Requirement | The governing specification |
| `R-<NNN>` | Risk | [risk-register.md](risk-register.md) |
| `Q-<NNN>` | Open question | [open-questions.md](open-questions.md) |
| `AERORA-<DOMAIN>-TEST-<NNN>` | Test case | `10-testing` |
| `AERORA-<DOMAIN>-ADR-<NNN>` | Architecture decision record | The governing specification |
| `QGC-BASE-<NNN>` | A confirmed QGroundControl baseline fact from this analysis | This document |

### Allocation rules

1. Identifiers are **never reused**, even after a requirement is deprecated.
2. A deprecated requirement keeps its identifier and gains status `DEPRECATED` plus a pointer to
   whatever superseded it.
3. Identifiers are allocated when a requirement is **written**, not when it is implemented.
4. A requirement must carry a status from the vocabulary in [README.md](README.md) at all times.
5. Splitting a requirement retires the original as `DEPRECATED` and allocates new identifiers.
   Renumbering is forbidden.

## Required fields per requirement

Every Aerora requirement record carries at minimum:

| Field | Purpose |
| --- | --- |
| Identifier | `AERORA-<DOMAIN>-REQ-<NNN>` |
| Statement | What is required, in testable terms |
| Status | `CONFIRMED` / `PROPOSED` / `OPEN` / `PLACEHOLDER` / `DEPRECATED` / `UNKNOWN` |
| Source | Where it came from, ranked by the source-of-truth hierarchy |
| Rationale | Why it exists |
| Architecture | Which component realises it |
| Implementation | Which files implement it; empty until implemented |
| Test | Which test verifies it; empty until written |
| Evidence | Verification artefact; empty until verified |
| Affected by | Risk and question identifiers that bear on it |
| Supersedes / superseded by | For deprecation chains |

A requirement whose `Source` is this analysis must be marked `PROPOSED`, never `CONFIRMED`. **An
AI-derived recommendation is not a confirmed requirement.**

## QGC baseline facts

These are the confirmed, evidence-backed facts from this analysis that Aerora requirements will
need to reference. They are **observations about QGroundControl**, not Aerora requirements, and are
identified separately so a requirement can cite one without implying Aerora endorses it.

| Identifier | Fact | Status | Evidence |
| --- | --- | --- | --- |
| `QGC-BASE-001` | QGC version is 5.2.0-dev at commit `9459a4b57986f40df8bdc193d5453c0596fbb416`, branch `master`, working tree clean | `CONFIRMED` | [repository-architecture.md](repository-architecture.md) |
| `QGC-BASE-002` | No git submodules exist; third-party sources arrive via CPM at configure time | `CONFIRMED` | [dependency-map.md](dependency-map.md) |
| `QGC-BASE-003` | Qt 6.11.1 required, CMake 3.25 minimum, C++20 | `CONFIRMED` | [build-system.md](build-system.md) |
| `QGC-BASE-004` | QGC is dual-licensed Apache 2.0 or GPL v3; the Apache path requires a commercial Qt licence | `CONFIRMED` | [dependency-map.md](dependency-map.md) |
| `QGC-BASE-005` | MAVLink headers are generated at configure time by mavgen; dialect is `all`; controlled by four `QGC_MAVLINK_*` variables | `CONFIRMED` | [mavlink-architecture.md](mavlink-architecture.md) |
| `QGC-BASE-006` | Inbound MAVLink uses fan-out-and-filter; every Vehicle receives every message and filters on system id | `CONFIRMED` | [mavlink-architecture.md](mavlink-architecture.md) |
| `QGC-BASE-007` | All outbound sends route through the single `LinkInterface::sendMessageThreadSafe` chokepoint so signing cannot be bypassed | `CONFIRMED` | [mavlink-architecture.md](mavlink-architecture.md) |
| `QGC-BASE-008` | 15 of 16 MAVLink channels are allocatable; channel 0 is permanently reserved | `CONFIRMED` | [mavlink-architecture.md](mavlink-architecture.md) |
| `QGC-BASE-009` | MAVLink 2 signing is fully implemented with PBKDF2-HMAC-SHA256 at 600000 iterations and a fixed app-wide salt | `CONFIRMED` | [security-architecture.md](security-architecture.md) |
| `QGC-BASE-010` | Signing keys are persisted in plaintext in `QSettings` | `CONFIRMED` | [security-architecture.md](security-architecture.md) |
| `QGC-BASE-011` | Link types are **not** pluggable; adding a transport requires 1 enum + 4 switches + 1 ordered string list across 3 core files | `CONFIRMED` | [communication-architecture.md](communication-architecture.md) |
| `QGC-BASE-012` | Six transports exist: Serial, UDP, TCP, Bluetooth, LogReplay, Mock. AirLink has been removed | `CONFIRMED` | [communication-architecture.md](communication-architecture.md) |
| `QGC-BASE-013` | No LoRa, FHSS24, or RF433 support exists anywhere in QGC source | `CONFIRMED` | [communication-architecture.md](communication-architecture.md) |
| `QGC-BASE-014` | SiK radios are carried over `SerialLink`; there is no dedicated SiK link type | `CONFIRMED` | [communication-architecture.md](communication-architecture.md) |
| `QGC-BASE-015` | Each transport runs I/O on a dedicated worker thread; parsing happens on the main thread | `CONFIRMED` | [communication-architecture.md](communication-architecture.md) |
| `QGC-BASE-016` | Comm loss is detected by a 1000 ms poll against a 3500 ms heartbeat timeout; QGC annunciates, it does not control | `CONFIRMED` | [vehicle-architecture.md](vehicle-architecture.md) |
| `QGC-BASE-017` | A vehicle is identified by MAVLink system id only; no UID-based matching exists | `CONFIRMED` | [vehicle-architecture.md](vehicle-architecture.md) |
| `QGC-BASE-018` | All vehicle parameters are Facts with metadata, units, bounds, and raw/cooked translation | `CONFIRMED` | [vehicle-architecture.md](vehicle-architecture.md) |
| `QGC-BASE-019` | Firmware plugins self-register via a constructor into a global list; no core edit is needed to add one | `CONFIRMED` | [vehicle-architecture.md](vehicle-architecture.md) |
| `QGC-BASE-020` | Firmware plugins are shared singletons per firmware class; per-vehicle state belongs in `FirmwarePluginInstanceData` | `CONFIRMED` | [vehicle-architecture.md](vehicle-architecture.md) |
| `QGC-BASE-021` | A new complex mission pattern requires zero core edits; `custom-example` proves it | `CONFIRMED` | [mission-architecture.md](mission-architecture.md) |
| `QGC-BASE-022` | `CameraCalc` in manual mode is the sanctioned carrier for non-camera swath and trigger distance | `CONFIRMED` | [mission-architecture.md](mission-architecture.md) |
| `QGC-BASE-023` | `includedResumeCommands` is a hardcoded core allow-list; non-listed state commands are dropped from a resume mission | `CONFIRMED` | [mission-architecture.md](mission-architecture.md) |
| `QGC-BASE-024` | Mission resume is refused outright if the mission contains any `MAV_CMD_DO_JUMP` | `CONFIRMED` | [mission-architecture.md](mission-architecture.md) |
| `QGC-BASE-025` | Override JSON cannot add new MAV_CMD entries; only the base list can | `CONFIRMED` | [mission-architecture.md](mission-architecture.md) |
| `QGC-BASE-026` | Only landing patterns are reconstructed from a mission downloaded from a vehicle | `CONFIRMED` | [mission-architecture.md](mission-architecture.md) |
| `QGC-BASE-027` | Any core QML file or resource can be replaced per-file via a URL interceptor, opt-in by presence | `CONFIRMED` | [ui-qml-architecture.md](ui-qml-architecture.md) |
| `QGC-BASE-028` | Every palette colour routes through `QGCCorePlugin::paletteOverride()` | `CONFIRMED` | [ui-qml-architecture.md](ui-qml-architecture.md) |
| `QGC-BASE-029` | Settings pages are generated at configure time from JSON and are overlay-extensible | `CONFIRMED` | [ui-qml-architecture.md](ui-qml-architecture.md) |
| `QGC-BASE-030` | No splash-screen mechanism exists | `CONFIRMED` | [ui-qml-architecture.md](ui-qml-architecture.md) |
| `QGC-BASE-031` | There is no runtime plugin system; all extension is compile-time plus resource override | `CONFIRMED` | [plugin-architecture.md](plugin-architecture.md) |
| `QGC-BASE-032` | The overlay is activated by `QGC_CUSTOM_DIR` and requires `CMakeLists.txt` plus `cmake/CustomOverrides.cmake` | `CONFIRMED` | [plugin-architecture.md](plugin-architecture.md) |
| `QGC-BASE-033` | CI proof-builds the overlay without renaming the directory | `CONFIRMED` | [plugin-architecture.md](plugin-architecture.md) |
| `QGC-BASE-034` | No application authentication or authorisation exists anywhere in QGC | `CONFIRMED` | [security-architecture.md](security-architecture.md) |
| `QGC-BASE-035` | No link-layer encryption exists on any transport | `CONFIRMED` | [security-architecture.md](security-architecture.md) |
| `QGC-BASE-036` | No signature verification exists on updates or firmware downloads | `CONFIRMED` | [security-architecture.md](security-architecture.md) |
| `QGC-BASE-037` | Code signing is implemented for macOS and iOS only | `CONFIRMED` | [security-architecture.md](security-architecture.md) |
| `QGC-BASE-038` | QGC has no cloud dependency on the flight-control path | `CONFIRMED` | [security-architecture.md](security-architecture.md) |
| `QGC-BASE-039` | An operator-control authority mechanism exists: `requestOperatorControl` plus `gcsControlStatusFlags` | `CONFIRMED` | [aerora-extension-map.md](aerora-extension-map.md) |
| `QGC-BASE-040` | QGC has no concept of a proposed command awaiting acceptance | `CONFIRMED` | [aerora-extension-map.md](aerora-extension-map.md) |
| `QGC-BASE-041` | Roughly 325 to 330 tests are registered; tests compile into the application binary | `CONFIRMED` | [testing-architecture.md](testing-architecture.md) |
| `QGC-BASE-042` | MockLink provides a full synthetic autopilot with extensive fault injection, Debug builds only | `CONFIRMED` | [testing-architecture.md](testing-architecture.md) |
| `QGC-BASE-043` | Two CPM dependencies use floating references, defeating build reproducibility | `CONFIRMED` | [dependency-map.md](dependency-map.md) |

## Seeded Aerora requirement entries

These follow directly from statements the v4.1 baseline already makes, so their `Source` is the
baseline rather than this analysis. They are seeded here to establish the chain structure; the
governing specifications must adopt, refine, and own them.

Status reflects the baseline's own position. Where the baseline defers a decision, the status is
`OPEN` and the statement records the boundary rather than a behaviour.

| Identifier | Statement | Status | Source | Architecture | Affected by |
| --- | --- | --- | --- | --- | --- |
| `AERORA-GCS-REQ-001` | The GCS shall be derived from QGroundControl, reusing its vehicle, parameter, mission, and protocol subsystems | `CONFIRMED` | Baseline README | [ownership-matrix.md](ownership-matrix.md) | R-001, R-014 |
| `AERORA-GCS-REQ-002` | All Aerora code shall reside in a single overlay directory activated by `QGC_CUSTOM_DIR`, except the enumerated transport registration edits | `PROPOSED` | This analysis | [aerora-extension-map.md](aerora-extension-map.md) | R-003, R-014 |
| `AERORA-GCS-REQ-003` | The GCS shall support current and future Aerora industrial UAV platforms, not only the painting hexacopter | `CONFIRMED` | Baseline README | Payload abstraction | Q-017 |
| `AERORA-GCS-REQ-004` | Flight-critical command and control shall function without internet, AWS, or any cloud service | `CONFIRMED` | Baseline README and PROJECT_STATUS | `QGC-BASE-038` | Q-030 |
| `AERORA-GCS-REQ-005` | The GCS shall present the effective aircraft state and authority, not merely the requested command | `CONFIRMED` | Directive and baseline | `QGC-BASE-039`, `QGC-BASE-040` | Q-008 |
| `AERORA-GCS-REQ-006` | The GCS shall not duplicate or replace flight-controller local safety capability | `CONFIRMED` | Directive and baseline | `QGC-BASE-016` | Q-007 |
| `AERORA-MAV-REQ-001` | Aerora shall define its MAVLink dialect in an Aerora-controlled fork pinned to an immutable commit | `PROPOSED` | This analysis | `QGC-BASE-005` | R-011 |
| `AERORA-MAV-REQ-002` | Aerora message handling shall attach via `QGCCorePlugin::mavlinkMessage()` without core edits | `PROPOSED` | This analysis | `QGC-BASE-031` | — |
| `AERORA-MAV-REQ-003` | Aerora telemetry shall be exposed as Facts in Aerora FactGroups, never as bespoke storage | `PROPOSED` | Directive Fact System rule | `QGC-BASE-018` | Q-023 |
| `AERORA-MAV-REQ-004` | The MAVLink interface between flight controller and GPU shall be documented in an interface-control document | `CONFIRMED` | Baseline README | — | Q-008 |
| `AERORA-COMM-REQ-001` | The GCS shall support a transport abstraction capable of handling multiple communication transports | `CONFIRMED` | Directive | `QGC-BASE-011` | R-003, Q-012, Q-014 |
| `AERORA-COMM-REQ-002` | LoRa transport support | `OPEN` | Baseline — hardware present per schematic analysis | New Aerora module | Q-013, Q-014 |
| `AERORA-COMM-REQ-003` | FHSS24 transport support | `OPEN` | Baseline — **hardware existence explicitly unconfirmed** | New Aerora module | Q-013 |
| `AERORA-COMM-REQ-004` | RF433 transport support | `OPEN` | Baseline — **hardware existence explicitly unconfirmed** | New Aerora module | Q-013 |
| `AERORA-COMM-REQ-005` | Worst-case concurrent link count shall not exceed the 15-channel MAVLink budget | `PROPOSED` | This analysis | `QGC-BASE-008` | R-013, Q-015 |
| `AERORA-SAFE-REQ-001` | Final failsafe behaviour | `OPEN` | **Deliberately deferred to a future approved Simulink implementation** | Flight controller — **not the GCS** | Q-007 |
| `AERORA-SAFE-REQ-002` | The GCS shall distinguish operator command, autonomous proposal, GPU command, FC-accepted command, effective command, and failsafe command | `CONFIRMED` | Directive | `QGC-BASE-039`, `QGC-BASE-040` | Q-008 |
| `AERORA-SAFE-REQ-003` | The GCS shall never represent a proposed command as an effective aircraft command | `CONFIRMED` | Directive | New Aerora authority module | Q-008 |
| `AERORA-SAFE-REQ-004` | Command freshness, authority, acceptance, and rejection shall remain explicit | `CONFIRMED` | Directive | `MavCommandQueue` acknowledgement tracking | Q-008 |
| `AERORA-SAFE-REQ-005` | Payload emergency-stop behaviour | `OPEN` | Baseline defers it | — | Q-009 |
| `AERORA-PAYLOAD-REQ-001` | The GCS shall use a product-independent payload abstraction | `CONFIRMED` | Directive | Payload manager | Q-023 |
| `AERORA-PAYLOAD-REQ-002` | The painting payload shall expose paint supply, tank, pump, spray, hose, payload power, gimbal, painting mission state, and payload safety state | `CONFIRMED` | Directive | Aerora FactGroups | Q-023 |
| `AERORA-PAYLOAD-REQ-003` | Final payload actuator interfaces | `OPEN` | Baseline states they must not be assumed | — | Q-023 |
| `AERORA-PAYLOAD-REQ-004` | A surface-coverage mission pattern shall be provided for painting operations | `PROPOSED` | This analysis, from the baseline product description | `QGC-BASE-021`, `QGC-BASE-022` | R-018, Q-021 |
| `AERORA-PAYLOAD-REQ-005` | Payload state shall be correctly re-established when a mission is resumed | `PROPOSED` | This analysis — safety-relevant gap | `QGC-BASE-023` | R-002, Q-019 |
| `AERORA-PWR-REQ-001` | Tethered operation shall use 480 V supplied from the ground, converted onboard to the 48 V aircraft domain | `CONFIRMED` | Baseline README and PROJECT_STATUS | Power FactGroup | Q-024 |
| `AERORA-PWR-REQ-002` | Tethered painting shall use ground paint supply via the tether hose, with the onboard tank detached | `CONFIRMED` | Baseline README and PROJECT_STATUS | Payload FactGroup | Q-025 |
| `AERORA-PWR-REQ-003` | Battery operation shall use onboard battery power with the onboard paint tank attached | `CONFIRMED` | Baseline README and PROJECT_STATUS | Power + payload FactGroups | Q-025 |
| `AERORA-PWR-REQ-004` | 480 V source type, isolation topology, converter rating, cable protection, connector system, grounding, and interlocks | `OPEN` | Baseline lists all as controlled items | — | Q-024 |
| `AERORA-PWR-REQ-005` | Power source transition behaviour | `OPEN` | Baseline states seamless transition must not be assumed | — | Q-026 |
| `AERORA-PWR-REQ-006` | The GCS shall report the active power and paint configuration | `PROPOSED` | This analysis | Power FactGroup | Q-025 |
| `AERORA-AWS-REQ-001` | AWS shall be the primary cloud platform | `CONFIRMED` | Baseline README | Cloud module | — |
| `AERORA-AWS-REQ-002` | The cloud application shall be a modular monolith, not a microservice architecture | `CONFIRMED` | Baseline README | Cloud module | — |
| `AERORA-AWS-REQ-003` | AWS shall never be a mandatory dependency for immediate flight safety | `CONFIRMED` | Directive and baseline | `QGC-BASE-038` | Q-030 |
| `AERORA-AWS-REQ-004` | All cloud dependencies shall be documented explicitly, with defined degradation behaviour | `CONFIRMED` | Directive | Cloud module | Q-030 |
| `AERORA-UI-REQ-001` | The operator interface shall be an industrial GCS appropriate to supervised and autonomous operation | `CONFIRMED` | Baseline | `QGC-BASE-027` | — |
| `AERORA-UI-REQ-002` | UI extension shall use QGC seams without wholesale forking of core QML | `PROPOSED` | This analysis | `QGC-BASE-027` | R-014, R-019 |
| `AERORA-UI-REQ-003` | Aerora branding shall fully replace QGroundControl branding and artwork | `PROPOSED` | This analysis, from licensing | `QGC-BASE-028` | Q-006, Q-034 |
| `AERORA-SEC-REQ-001` | The GCS shall provide operator authentication | `PROPOSED` | Directive high-assurance scalability | New Aerora module | R-004, Q-029 |
| `AERORA-SEC-REQ-002` | The GCS shall provide role-based authorisation with role separation | `PROPOSED` | Directive | New Aerora module | R-004, Q-029 |
| `AERORA-SEC-REQ-003` | The GCS shall provide audit logging | `PROPOSED` | Directive | Extend `LogManager` | R-004 |
| `AERORA-SEC-REQ-004` | Signing key material shall not be stored in plaintext | `PROPOSED` | This analysis | `QGC-BASE-010` | R-005 |
| `AERORA-SEC-REQ-005` | Software updates shall be integrity-verified before installation | `PROPOSED` | Directive | New Aerora module | R-007 |
| `AERORA-SEC-REQ-006` | Distributed artefacts shall be signed on every supported platform | `PROPOSED` | Directive | New build work | R-015, Q-032 |
| `AERORA-SEC-REQ-007` | Configuration integrity shall be verifiable | `PROPOSED` | Directive | New Aerora module | — |
| `AERORA-SEC-REQ-008` | Mission integrity shall be verifiable | `PROPOSED` | Directive | New Aerora module | — |
| `AERORA-SEC-REQ-009` | MAVLink signing default policy | `OPEN` | — | `QGC-BASE-009` | R-006, Q-010 |
| `AERORA-SEC-REQ-010` | Link-layer security for Aerora transports | `OPEN` | — | `QGC-BASE-035` | R-006, Q-011 |
| `AERORA-TEST-REQ-001` | Aerora code shall be covered by tests in the inherited QGC test framework | `PROPOSED` | This analysis | `QGC-BASE-041` | — |
| `AERORA-TEST-REQ-002` | Aerora transports shall have a test double and ThreadSanitizer coverage | `PROPOSED` | This analysis | `QGC-BASE-042` | R-003 |
| `AERORA-TEST-REQ-003` | A test shall assert that the GCS never presents a proposed command as effective | `PROPOSED` | This analysis | `AERORA-SAFE-REQ-003` | Q-008 |
| `AERORA-BUILD-REQ-001` | The Aerora fork shall record commit `9459a4b57986f40df8bdc193d5453c0596fbb416` as its baseline | `PROPOSED` | This analysis | `QGC-BASE-001` | R-008, Q-003 |
| `AERORA-BUILD-REQ-002` | All Aerora dependencies shall be pinned to immutable references | `PROPOSED` | This analysis | `QGC-BASE-043` | R-011, Q-033 |
| `AERORA-BUILD-REQ-003` | Qt licensing path selection | `OPEN` | — | `QGC-BASE-004` | R-001, Q-004 |
| `AERORA-COMPLY-REQ-001` | The system shall be designed with DGCA compliance readiness in mind, without claiming current compliance | `CONFIRMED` | Baseline README and directive | Compliance framework | — |

**Count: 55 seeded entries** — 21 `CONFIRMED`, 23 `PROPOSED`, 11 `OPEN`. All identifiers are
unique and each domain is contiguously numbered with no gaps.

Every `CONFIRMED` entry traces to an explicit statement in the v4.1 baseline or the governing
directive. Every `PROPOSED` entry traces to this analysis and requires ratification. Every `OPEN`
entry records a boundary rather than a behaviour.

## Coverage gaps in the seeded set

Stated so the gaps are visible rather than implied.

| Chain stage | Coverage |
| --- | --- |
| Identifier | 55 of 55 |
| Statement | 55 of 55 |
| Status | 55 of 55 |
| Source | 55 of 55 |
| Architecture | 50 of 55 — the five unpopulated entries are all `OPEN` items with no architecture by definition |
| Implementation | **0 of 55** — no implementation exists |
| Test | 3 of 55 — only the three `AERORA-TEST-REQ-*` entries name a test |
| Evidence | **0 of 55** — nothing has been verified |

This is the expected shape at the end of Phase 1. The chain structure exists; the chain is not yet
populated. Phase 3 populates `Architecture`, Phase 5 populates `Implementation`, Phase 6 populates
`Test`, and Phases 6 to 8 populate `Evidence`.

## Change-control procedure

When a new requirement is introduced, or an existing one changes, execute all ten steps. Partial
execution produces the contradictory documentation that this procedure exists to prevent.

1. Record the requirement with every required field.
2. Assign status from the controlled vocabulary.
3. Identify the source, ranked by the source-of-truth hierarchy.
4. Identify affected components.
5. Perform impact analysis across specification, architecture, interface-control document,
   implementation, and tests.
6. Update affected specifications.
7. Update architecture documents if affected.
8. Update tests if affected.
9. Update traceability — this document and the governing specification.
10. Record the baseline revision.

### Source-of-truth hierarchy

Repeated here because every traceability decision depends on it.

```text
1. Explicitly confirmed Aerora engineering requirements
2. Approved Aerora Product Baseline
3. Approved Aerora architecture specifications
4. Actual Aerora hardware / schematic / PCB evidence
5. Actual Aerora Simulink / firmware implementation
6. Approved interface-control documents
7. QGroundControl source-code behaviour
8. Official regulatory documentation
9. Engineering proposals
10. AI assumptions
```

Level 10 is never equivalent to level 1. Where two sources conflict, detect it, document it,
identify affected requirements, and request confirmation — do not silently choose.

### Evidence maturity

For hardware-dependent requirements, evidence must state which maturity level it represents. The
baseline's own distinction, which this analysis adopted:

```text
SCHEMATIC PRESENT  →  PCB IMPLEMENTED  →  FIRMWARE IMPLEMENTED
                   →  BENCH VERIFIED   →  FLIGHT VERIFIED
```

A requirement supported only by `SCHEMATIC PRESENT` evidence must not be described as implemented.
`AERORA-COMM-REQ-003` and `AERORA-COMM-REQ-004` are the live examples — the baseline itself states
FHSS24 and RF433 must be confirmed before being called onboard hardware.

### Regulatory distinction

Where regulatory requirements apply, keep these five separate and individually traceable:

```text
Regulatory requirement  →  Aerora requirement  →  Implementation
                        →  Verification evidence  →  Certification status
```

Conflating an Aerora requirement with a regulatory one, or implementation with certification, is
the failure mode this separation prevents. `AERORA-COMPLY-REQ-001` is deliberately worded as
readiness, not compliance.

## Maintenance

| Aspect | Rule |
| --- | --- |
| Ownership | This document is owned by the Aerora engineering lead; domain sections by the governing specification owner |
| Update trigger | Any requirement addition, change, or deprecation; any risk or question resolution |
| Review cadence | At each baseline revision |
| Consistency check | Every `R-NNN` and `Q-NNN` referenced here must exist in its register, and the registers must not reference retired requirement identifiers |

Identifier allocation is sequential within a domain. The next free identifier per domain as of this
revision:

| Domain | Next free |
| --- | --- |
| `GCS` | 007 |
| `MAV` | 005 |
| `COMM` | 006 |
| `SAFE` | 006 |
| `PAYLOAD` | 006 |
| `PWR` | 007 |
| `AWS` | 005 |
| `UI` | 004 |
| `SEC` | 011 |
| `TEST` | 004 |
| `BUILD` | 004 |
| `COMPLY` | 002 |
| `MISSION` | 001 |
| `VEH` | 001 |

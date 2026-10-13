# Implementation Dependency Graph

Owner: **Tab 1**. Build order for Phase C, derived from the real dependencies in the checkout rather
than from the module list.

Status vocabulary: [00-README](00-README.md#status-vocabulary).

## Contents

- [1. Dependency layers](#1-dependency-layers)
- [2. Build-graph dependencies](#2-build-graph-dependencies)
- [3. Runtime composition dependencies](#3-runtime-composition-dependencies)
- [4. What is blocked and by what](#4-what-is-blocked-and-by-what)
- [5. The unblocked five](#5-the-unblocked-five)
- [6. The critical path](#6-the-critical-path)
- [7. Parallel-safe work](#7-parallel-safe-work)
- [8. Sequential-only work](#8-sequential-only-work)
- [9. Decision-to-work mapping](#9-decision-to-work-mapping)
- [10. Phase C ordering](#10-phase-c-ordering)

## 1. Dependency layers

Seven layers. A layer may depend only on layers above it. Violations are listed in
[Â§2](#2-build-graph-dependencies) â€” there are none today, and keeping it that way is the point of
the layering.

```text
L0  Upstream QGroundControl            src/           â€” not modified except the 9 recorded files
     â”‚
L1  Primitives                         Supervisory/, Telemetry/ (state, freshness, value)
     â”‚                                 no Aerora dependency; depends only on QGC::Timing + FactSystem
     â”‚
L2  Domain models                      Payload/, Power/      â†’ L1
     â”‚                                 Security/             â†’ QGC core seams only
     â”‚
L3  Vehicle composition                Vehicle/              â†’ L1, L2
     â”‚                                 AeroraFirmwarePlugin::initializeVehicle() is the composition root
     â”‚
L4  Cross-vehicle services             Mission/              â†’ L3 (hard: AeroraVehicleServices)
     â”‚                                 Persistence           â†’ (absent)
     â”‚
L5  View-model                         UI/AeroraVehicleUiContext  â†’ L2, L3, L4
     â”‚
L6  Presentation                       UI/Controls/, screens      â†’ L5
     â”‚
L7  Shell integration                  AeroraCorePlugin, aerora/CMakeLists.txt â†’ everything
```

### Why L1 contains two modules

`Supervisory/` holds exactly one type, `AeroraReportedValue`, which Payload and Power both need. It
sits in its own directory so neither module depends on the other for it. Absorbing it into
`Telemetry/` is a directory rename plus one manifest line with no API change, and is recorded `OPEN`
and assigned to Tab 1 â€” **deferred**, because it is cosmetic, it would edit two other tabs' files,
and it carries no benefit before a build exists.

## 2. Build-graph dependencies

Encoded in CMake and asserted at configure time, not merely documented.

### Manifest inclusion order

From `aerora/CMakeLists.txt`, with the reason each position is required:

| Order | Manifest | Must come after | Reason |
| --- | --- | --- | --- |
| 1 | `Security/Security.cmake` | â€” | No Aerora dependency |
| 2 | `Telemetry/Telemetry.cmake` | â€” | No Aerora dependency |
| 3 | `Vehicle/Vehicle.cmake` | Telemetry | `initializeVehicle()` composes telemetry into every vehicle |
| 4 | `Mission/Mission.cmake` | Vehicle | `AeroraMissionBridge.cc` includes `AeroraVehicleServices.h` |
| 5 | `src/PayloadPower.cmake` | Telemetry, Vehicle | Consumes `AeroraTelemetryState`/`StateValue`/`Source`/`Freshness` and `AeroraVehicleCapabilities` |
| 6 | `UI/Ui.cmake` | all | View-model binds every domain |

Manifests deliberately **do not** re-export their dependencies' include directories. Dropping the
vehicle module therefore fails the build at a missing header rather than silently producing a mission
module that never attaches. That is a good property and Phase C must preserve it.

### The configure-time assertion

`Vehicle.cmake` declares `AERORA_VEHICLE_REQUIRED_MODULES`, and `aerora/CMakeLists.txt` maps each
name to a sentinel variable and fails with `FATAL_ERROR` if the manifest was not included or exported
no sources. The reason this exists: `initializeVehicle()` composes four sibling modules by header and
by symbol, so an omission would otherwise be discovered from a link error â€” or worse, from a vehicle
that silently has an incomplete service set.

Module names do not map one-to-one onto manifest prefixes â€” Payload, Power and Supervisory are all
carried by `PayloadPower.cmake` â€” so the mapping is explicit rather than derived from the name. Any
Phase C module addition must add its sentinel mapping.

### Required compile definitions

| Definition | Without it |
| --- | --- |
| `QGC_CUSTOM_BUILD` | `QGCCorePlugin::instance()` does not route to `AeroraCorePlugin`. **The overlay compiles and the authorization gate is never consulted.** ADR-012 |
| `CUSTOMHEADER="AeroraCorePlugin.h"` / `CUSTOMCLASS=AeroraCorePlugin` | Same |
| `AERORA_VEHICLE_FACTGROUP_FRIENDSHIP` | `AeroraVehicleFactGroupRegistrar`'s single privileged statement does not compile. Pairs with `friend class AeroraVehicleFactGroupRegistrar;` in `src/Vehicle/Vehicle.h`; **defining it without the friend declaration is a compile error**, which is the safe direction. ADR-016 |

## 3. Runtime composition dependencies

The build graph and the runtime graph differ, and the runtime one is where the real gaps are.

```text
QGCApplication boot
  â”‚
  â”œâ”€ QGCCorePlugin::instance() â”€â”€â–º AeroraCorePlugin        [needs QGC_CUSTOM_BUILD]
  â”‚     â”œâ”€ init() â”€â”€â–º _installSigningKeyStore()
  â”‚     â”œâ”€ authorizeMavCommand() â—„â”€â”€ MavCommandQueue::sendWorker()
  â”‚     â”‚     â””â”€ AeroraCommandAuthorizer â”€â”€â–º AeroraAuthorizationPolicy   âœ— ABSENT
  â”‚     â””â”€ createQmlApplicationEngine() â”€â”€â–º AeroraQmlOverrides::installOn()   âœ— NOT WIRED
  â”‚
  â”œâ”€ FirmwarePluginFactory registration â”€â”€â–º AeroraFirmwarePluginFactory
  â”‚     â””â”€ (non-local static; requires direct-to-target compilation)
  â”‚
  â”œâ”€ Vehicle created
  â”‚     â””â”€ AeroraFirmwarePlugin::initializeVehicle(vehicle)
  â”‚           â”œâ”€ AeroraVehicleServices::attachTo(vehicle)
  â”‚           â”œâ”€ AeroraTelemetrySet            (6 groups, all CONTRACT-ONLY)
  â”‚           â”œâ”€ AeroraVehicleIdentity         (fleet id format OPEN)
  â”‚           â”œâ”€ AeroraVehicleCapabilities     (nothing reported â€” Q3-009)
  â”‚           â”œâ”€ AeroraPayloadModel            (no reported-value set)
  â”‚           â”œâ”€ AeroraPowerModel              (no reported-value set, 0 thresholds)
  â”‚           â””â”€ AeroraMissionBridge::attachTo(vehicle)   â† AeroraFirmwarePlugin.cc:162
  â”‚
  â”œâ”€ autopilotPlugin(vehicle) â”€â”€â–º AeroraAutoPilotPlugin    âœ— ABSENT
  â”œâ”€ toolIndicators(vehicle)  â”€â”€â–º Aerora indicators        âœ— NOT OVERRIDDEN
  â”‚
  â””â”€ QML engine
        â”œâ”€ Aerora C++ types register into URI QGC          (precedent: QmlControls does the same)
        â”œâ”€ Aerora.Controls module                          âœ— missing IMPORTS QGC (D-UI-04)
        â””â”€ AeroraUi singleton â”€â”€â–º the 5 typed services
```

Four runtime breaks, marked âœ—. Three of them (`AeroraQmlOverrides` install, `AeroraAutoPilotPlugin`,
`toolIndicators`, plus `D-UI-04`) are gated on nothing.

### Conflict 2 is resolved

`AeroraMissionBridge::attachTo()` had no caller at the Phase 6 handoff â€” conflict 2, assigned to
Tab 3 as owner of the vehicle lifecycle seam, with the recommendation to call it from
`initializeVehicle()` where `AeroraVehicleServices` is already attached.

**Verified closed this pass.** `AeroraFirmwarePlugin.cc:162` calls
`(void) AeroraMissionBridge::attachTo(vehicle);` from exactly that seam, and `attachTo()` is
idempotent — it returns the existing bridge rather than connecting a second one, which would
double-report every signal into one observer (`AeroraMissionBridge.cc:70-84`). The bridge parents
itself to the observer, which `attachService()` reparents to the services object, so it is destroyed
with the vehicle.

Tab 3's remaining action is a test assertion, not a code change.

## 4. What is blocked and by what

19 screens are `SPECIFIED-NOT-DESIGNED`. Grouped by what actually blocks them:

### Blocked on a product decision â€” 11 screens

| Blocker | Owner | Screens |
| --- | --- | --- |
| `Q-029` operator role set | Product + operations | `SCR-LIV-05`, `SCR-SEC-01`, `SCR-SEC-02` |
| `Q-021` / `Q-023` Aerora mission command set | Product | `SCR-MIS-01`, `SCR-MIS-02`, `SCR-MIS-03`, `SCR-MIS-04`, `SCR-MIS-05` |
| Fleet identity format | Product | `SCR-FLT-01`, `SCR-FLT-02` |
| Payload ICD / reported-consumable set | Product + engineering | `SCR-MIS-06` |

### Blocked on an architecture decision â€” 5 screens

| Blocker | Owner | Screens |
| --- | --- | --- |
| `P7-DAT-002` local persistence design | **Tab 1** | `SCR-TEL-02`, `SCR-LOG-01`, `SCR-LOG-02` |
| `P7-AI-004` autonomy interface | Architecture + external | `SCR-AIS-01`, `SCR-AIS-02` |

### Blocked on an engineering prerequisite â€” 3 screens

| Blocker | Owner | Screens |
| --- | --- | --- |
| `Q-037` audit durability | Tab 2 | `SCR-SEC-03`, `SCR-AIS-03` |
| `P7-HELP-001` diagnostics source | Tab 1 + Tab 7 | `SCR-HLP-01` |
| `P7-HELP-004` provenance source | Tab 1 | `SCR-SHL-04` |

**16 of 19 are blocked on a decision.** Designing them before the decision would mean inventing the
decision, which `P7-NOINV-002` prohibits. The honest position is that this is not an engineering
backlog.

## 5. The unblocked five

Five changes gated on nothing, independent of each other, that unblock twelve screens between them.
**This is the Phase C starting set.**

| # | Change | File | Owner | Unblocks | Size |
| --- | --- | --- | --- | --- | --- |
| **U1** | Install `AeroraQmlOverrides` â€” override `createQmlApplicationEngine` / `destroyQmlApplicationEngine` | `aerora/src/AeroraCorePlugin.{h,cc}` | **Tab 1** | Every tier-2/3 QML override path | ~15 lines, exact code in HO-TAB7-02 |
| **U2** | Add `IMPORTS QGC` + `qgc_set_qt_resource_alias` to the module block | `aerora/CMakeLists.txt:160-172` | **Tab 1** | `Aerora.Controls` resolving `AeroraUi` / `AeroraTelemetryState`; correct resource paths | ~6 lines, exact block in HO-TAB7-03 |
| **U3** | Drop `WRITE setCapability` from the `Q_PROPERTY`; keep `setCapability()` as public C++; remove the entry from `recordedWritablePropertyDeviations()` | `aerora/src/Telemetry/AeroraStateValue.h:111` + `aerora/test/UI/AeroraUiSafetyTest.cc` | **Tab 4** | `SCR-LIV-02` and every screen binding an `AeroraStateValue` | 2 lines + 1 test entry |
| **U4** | Create `AeroraAutoPilotPlugin`; return it from `AeroraFirmwarePlugin::autopilotPlugin()` | new `aerora/src/Vehicle/AeroraAutoPilotPlugin.{h,cc}` + `Vehicle.cmake` | **Tab 3** | `SCR-VEH-01`..`06` (6 screens) | new class + `vehicleComponents()` |
| **U5** | Override `FirmwarePlugin::toolIndicators(const Vehicle*)` | `aerora/src/Vehicle/AeroraFirmwarePlugin.{h,cc}` | **Tab 3** | `SCR-SHL-02` per-vehicle half | ~10 lines |

### Why these five first

| Property | Value |
| --- | --- |
| Blocked on a decision | None |
| File conflicts between them | **None.** U1+U2 are Tab 1, U3 is Tab 4, U4+U5 are Tab 3 â€” three tabs, five files, no overlap |
| Screens unblocked | 12 |
| Combined size | Under 200 lines including the new class |
| Risk | U3 changes a public property shape; the test entry must be removed in the same commit or the suite fails |

U3's coupling is the only sharp edge: `AeroraUiSafetyTest::_onlyTheRecordedPropertyDeviationIsWritable`
asserts the writable set is **exactly** that one entry, so removing the deviation without removing
the test entry fails on the now-absent deviation. One commit, both halves.

## 6. The critical path

The longest chain from today to a complete product. Each arrow is a hard dependency.

```text
D-006 (build host)
  â””â”€â–º first configure + build
        â””â”€â–º first test run
              â””â”€â–º any VERIFIED claim                          â† the whole programme gates here

Q-029 (role set)
  â””â”€â–º ADR-014 (authorization scope)
        â””â”€â–º AeroraAuthorizationPolicy implementation
              â””â”€â–º an authorized command path
                    â”œâ”€â–º operator action controls      (P7-FLY-004)
                    â”œâ”€â–º payload command sink          (P7-PAY-003)
                    â””â”€â–º autonomy approval             (SCR-LIV-05, journey 6.4)

P7-DAT-002 (persistence design)
  â””â”€â–º local store
        â”œâ”€â–º event timeline            (SCR-TEL-02)
        â”œâ”€â–º fleet records             (SCR-FLT-01, SCR-FLT-02)
        â”œâ”€â–º session review            (SCR-LOG-01)
        â””â”€â–º durable audit (Q-037)     (SCR-SEC-03, SCR-AIS-03)

Q-021/Q-023 (mission command set)
  â””â”€â–º AeroraMissionExtension registration
        â””â”€â–º Aerora complex item
              â””â”€â–º scene + surface + coverage screens   (SCR-MIS-01..06)
                    â””â”€â–º painting journey (6.1)
```

### The two that dominate

| Path | Length | Blocks |
| --- | --- | --- |
| `D-006` â†’ build â†’ test â†’ `VERIFIED` | 3 steps, **step 1 is a user decision** | **Every** verification claim in the programme |
| `Q-029` â†’ ADR-014 â†’ policy â†’ command path â†’ actions | 5 steps, **step 1 is a product decision** | Every operator action, the payload sink, autonomy approval, journeys 6.1 and 6.4 |

Neither is engineering work. Both are decisions. That is the single most important fact in this
document: **the critical path is not resource-limited, it is decision-limited.**

### The one Tab 1 can shorten unilaterally

`P7-DAT-002`. The persistence design is an architecture decision Tab 1 owns, it blocks five screens,
and it plausibly also resolves `Q-037` audit durability â€” turning two blockers into one. It is the
highest-leverage item on Tab 1's own plate.

## 7. Parallel-safe work

Safe to run concurrently: no shared file, no shared root cause.

| Tab | Work | Files it owns |
| --- | --- | --- |
| 1 | U1 (interceptor install), U2 (module block), persistence architecture, provenance source | `aerora/src/AeroraCorePlugin.*`, `aerora/CMakeLists.txt`, `.kiro/specs/05-product-completion/*` |
| 2 | Audit writer interface against `Q-037` **without choosing a mechanism**; convert nothing else | `aerora/src/Security/*` |
| 3 | U4 (`AeroraAutoPilotPlugin`), U5 (`toolIndicators`), confirm `attachTo()` caller | `aerora/src/Vehicle/*` |
| 4 | U3 (capability `WRITE`), fact-group attachment variants as specifications | `aerora/src/Telemetry/*`, `aerora/test/UI/AeroraUiSafetyTest.cc` (U3 only) |
| 5 | Mission/payload interaction boundary with Tab 6, **without** replaying payload state | `aerora/src/Mission/*` |
| 6 | Payload command boundary against Tab 5; keep every threshold and transition `OPEN` | `aerora/src/Payload/*`, `aerora/src/Power/*`, `aerora/src/Supervisory/*` |
| 7 | Design system components, `SCR-SHL-01`, `SCR-HLP-02`, the 24 `DESIGNED` screens | `aerora/src/UI/*`, `aerora/test/UI/*` (except the U3 line) |
| 8 | Build host, overlay CI leg, upstream drift tooling | `.github/`, `tools/`, build config |
| 9 | `src/FactSystem` / `PARAM_SET` authority | `src/FactSystem/*` |

### The one shared-file hazard

`aerora/test/UI/AeroraUiSafetyTest.cc` is Tab 7's file, but U3 requires Tab 4 to remove one entry
from it. **Resolution: Tab 4 makes the U3 change including the test entry, in one commit, and Tab 7
does not touch that file until U3 lands.** Recorded in
[07 Â§Conflict avoidance](07-phase-7-work-packages-and-ownership.md#3-conflict-avoidance).

## 8. Sequential-only work

| Work | Blocked by | Cannot start because |
| --- | --- | --- |
| Arming the authorization policy | `Q-029`, then ADR-014 | The role set defines the policy's domain |
| Any operator action control | an authorized command path | `P7-FLY-004` â€” an inert button is worse than none |
| Payload command sink implementation | an authorized command path | `P7-PAY-003` â€” a sink without authorization is an unauthorized actuator |
| Attaching telemetry data to vehicles | `Q-023`/`Q-024`, `Q-036` | No message to parse, no budget to evaluate |
| Aerora complex mission item | `Q-021`/`Q-023` | The command set defines the item |
| Painting supervision content | payload ICD, `Q3-009` | No reported-value set exists |
| Consumable segmentation | reported-consumable set + an approved consumption model | `P7-ARCH-005` prohibits inventing the model |
| Durable audit | `Q-037` | Mechanism undecided |
| Platform key protection | `D-006` | **An untested crypto backend is worse than none** |
| Any compile, lint or test claim | `D-006`, then `Q-035` | No toolchain |
| Any push | `D-005` | `origin` is upstream |
| Any product feature | `D-007` | External authorization |

## 9. Decision-to-work mapping

What each open decision unlocks, so the cost of leaving it open is visible.

| Decision | Owner | Unlocks | Screens | Journeys |
| --- | --- | --- | --- | --- |
| `D-006` build host | **User** | All verification | â€” | all (as `VERIFIED`) |
| `D-005` git origin | **User** | Any push | â€” | â€” |
| `D-007` product authorization | **External** | All product features | all P-class | 6.1 |
| `Q-029` role set | Product + ops | Authorization policy â†’ command path â†’ actions | 3 directly, all action surfaces indirectly | 6.2, 6.4 |
| ADR-014 scope | Architecture | General authorization claims | â€” | 6.4 |
| `Q-021` / `Q-023` mission set | Product | Aerora complex item | 5 | 6.1, 6.5 |
| `Q-036` freshness budgets | Product | Freshness evaluation with real values | `SCR-TEL-03` content | 6.3, 6.4 |
| `Q-037` audit durability | Security | Durable audit | 2 | 6.6 |
| Fleet identity format | Product | Fleet records | 2 | 6.7 |
| `P7-DAT-002` persistence | **Tab 1** | Local store | 5 | 6.6, 6.7 |
| `P7-AI-004` autonomy interface | Architecture | Proposal model | 2 | 6.4 |
| `Q3-009` capability reporting | Firmware | Capability display, autonomy preconditions | `SCR-VEH-03` content | 6.2, 6.4 |
| Payload ICD | Product + eng | Painting supervision | `SCR-LIV-03`, `SCR-MIS-05/06` | 6.1 |
| `src/FactSystem` ownership | **Programme** | Parameter authorization claims | â€” | 6.2 |
| `Q-035` overlay test leg | Tab 8 + CI | Denial-path tests | â€” | â€” |
| `D-UI-01` auth read path | Tab 2 + Tab 7 | Enforcement display | `SCR-SHL-03`, `SCR-SEC-01` | â€” |
| `D-UI-06` multi-station authority | Architecture | Authority display | `SCR-SHL-03`, `SCR-LIV-07` | â€” |

### Decisions ranked by what they unblock

| Rank | Decision | Screens unblocked | Note |
| --- | --- | --- | --- |
| 1 | `Q-029` â†’ ADR-014 | 3 direct + **every** action surface | The product's largest gap |
| 2 | `P7-DAT-002` | 5 | **Tab 1 owns it. Nothing external blocks it** |
| 3 | `Q-021` / `Q-023` | 5 | Gates the flagship painting journey |
| 4 | `D-006` | 0 screens, **all** verification | Not a screen blocker; a truth blocker |
| 5 | Fleet identity | 2 | Small decision, clean unblock |
| 6 | `Q-037` | 2 | May be subsumed by `P7-DAT-002` |
| 7 | `P7-AI-004` | 2 | Depends on external GPU work |

## 10. Phase C ordering

Six stages. A stage starts when its predecessor's prerequisites are met, not when its predecessor is
complete â€” most of the work parallelises within a stage.

### Stage C1 â€” the unblocked five

[Â§5](#5-the-unblocked-five). Three tabs, five files, no conflicts, twelve screens unblocked. Nothing
else in Phase C should start first, because U1 and U2 are prerequisites for any QML work landing
correctly and U3 is a prerequisite for binding a state value.

### Stage C2 â€” design system and the self-reporting shell

| Work | Owner | Prerequisite |
| --- | --- | --- |
| Composite components ([03 Â§5](03-gui-information-architecture-and-screens.md#composite-components-to-build)) | 7 | U2, U3 |
| `SCR-SHL-01` Operations Hub | 7 | U1 |
| `SCR-HLP-02` State vocabulary reference | 7 | â€” |
| `SCR-VEH-01`..`06` setup pages | 3 + 7 | U4 |
| `SCR-SHL-02` indicator cluster | 7 + 3 | U5 |

Stage exit: an operator can open the application, see the Aerora hub, and read an honest report of
what attached and whether authorization is enforced. **That is the first point at which the overlay
is observable.**

### Stage C3 â€” persistence and provenance

| Work | Owner | Prerequisite |
| --- | --- | --- |
| `P7-DAT-002` persistence design, then implementation | **1** | Tab 1 decision |
| Build provenance source | 1 | â€” |
| `SCR-HLP-01` diagnostics report | 1 + 7 | provenance |
| `SCR-SHL-04` about/provenance | 1 | provenance |
| Event timeline model | 4 | persistence |
| `SCR-TEL-02` event timeline | 4 + 7 | timeline model |

Highest-leverage stage Tab 1 can drive alone.

### Stage C4 â€” telemetry content

| Work | Owner | Prerequisite |
| --- | --- | --- |
| Fact group messages | 4 | `Q-023` / `Q-024` |
| Freshness budgets | 4 | `Q-036` |
| Fact group registration (ADR-016 Option B) | 4 | the above + `D-006` |
| `SCR-TEL-01` telemetry explorer | 4 + 7 | registration |
| `SCR-TEL-03` quality dashboard | 4 + 7 | budgets |

Blocked on product decisions. Tab 4's parallel-safe work until then is preparing both attachment
variants as specifications so the decision applies cheaply.

### Stage C5 â€” mission and payload content

| Work | Owner | Prerequisite |
| --- | --- | --- |
| `AeroraMissionExtension` registration | 5 | `Q-021` / `Q-023` |
| Aerora complex item (C++ + editor + **two** map visuals) | 5 | registration |
| `SCR-MIS-01`..`05` | 5 + 7 | complex item |
| Painting reported-value set | 6 | payload ICD |
| `SCR-LIV-03` payload panel | 6 + 7 | value set |
| Tether reported-value set, `SCR-LIV-04` | 6 + 7 | reported tether items |
| `SCR-MIS-06` consumable segmentation | 5 + 6 | consumable set + approved consumption model |

### Stage C6 â€” authorization, autonomy, actions

| Work | Owner | Prerequisite |
| --- | --- | --- |
| `AeroraIdentityProvider`, `AeroraRoleResolver`, `AeroraAuthorizationPolicy` | 2 | `Q-029` â†’ ADR-014 |
| Durable audit sink | 2 | `Q-037`, likely C3's store |
| `SCR-SEC-01`..`03` | 2 + 7 | the above + `D-UI-01` |
| Autonomy proposal model + source interface | 6 + 1 | `P7-AI-004` |
| `SCR-LIV-05`, `SCR-AIS-01`..`03` | 6 + 7 | proposal model |
| **Operator action controls** | 7 | an authorized command path |
| Payload command sink | 6 | an authorized command path |

Last on purpose. Every item needs the authorization chain, and nothing in it may be built
speculatively â€” a half-built command path is an unauthorized actuator.

### Stage gate rule

No stage may be reported complete while any item in it is `IMPLEMENTED-UNCOMPILED` **and** the build
host exists. Once `D-006` resolves, the stage gate becomes: builds, lints, and its tests pass. Until
then every stage ends at `IMPLEMENTED-UNCOMPILED`, which is the honest ceiling.

# Aerora Payload Abstraction (`M-Payload`)

The product-independent payload boundary: what the ground station may say about a payload, and the
line it does not cross.

Owns `IF-PAY-001` through `IF-PAY-015`. Depends on `M-Telemetry` for the state model and
`M-Vehicle` for the capability inventory and the attachment point. Depends on no UI, no mission, no
security and no radio code.

**Build status: `IMPLEMENTED-UNCOMPILED`.** No compiler, CMake, Qt, `clang-format` or `pre-commit`
exists in the development environment this was written in, and the MAVLink headers are fetched at
configure time rather than present in the tree. Nothing here has been built, linted or executed.
See [Verification status](#verification-status).

## Ownership

```text
Payload controller (aircraft)   owns payload state, local interlocks, actuator behaviour,
        │                       emergency behaviour  —  NOT the ground station
        ▼  reports
Flight controller               relays
        │
        ▼  parsed, stamped at parse
AeroraPayloadModel              representation only
        │
        ▼  gated by reported capability
GCS UI                          display, and operator-originated requests
```

06-payload-icd.md's ownership table has six rows. The ground station appears in exactly two, both
as an observer or a relay. That is the whole contract, and this module is the observer.

| Concern | Owner |
| --- | --- |
| Payload state | Payload controller |
| Payload safety and interlocks | Payload controller, with an independent watchdog path |
| Actuator behaviour | Payload controller — `IF-PAY-013`, internal to the vehicle |
| Payload telemetry | Payload controller reports; the ground station displays |
| Payload command | The operator originates; the **payload controller decides** |
| Ground station to actuators | **Nobody. The path does not exist** — `IF-PAY-015` |

### The GCS boundary, as compiled facts

Two prohibitions are pinned as constants rather than left as document claims, so reversing either
is a build-breaking change with a `static_assert` message naming the interface:

| Constant | Interface | Value |
| --- | --- | --- |
| `AeroraPayloadModel::kGroundStationActuatorPathImplemented` | `IF-PAY-015`, `IF-PAY-013` | `false` |
| `AeroraPayloadModel::kAutonomousPayloadActionImplemented` | `AERORA-PAYLOAD-REQ-010` | `false` |

The behavioural half is a unit test: with every gate open and a recording command sink installed,
reporting a health fault, a safety-state change and an emergency stop produces **zero** outbound
requests. A future change that wired "fault → close valve" would fail there, which is the point.

## What is implemented, and what is not

| Artefact | Content | Status |
| --- | --- | --- |
| `AeroraReportedValue` | The reported-item primitive: state class, receive time, reporting source, capability tri-state, opaque token | `IMPLEMENTED-UNCOMPILED` |
| `AeroraPayloadModel` | Five separate reported items, capability gating, the request boundary | `IMPLEMENTED-UNCOMPILED` |
| `AeroraPayloadCommandSink` | The one outbound seam, declared and **unimplemented** | `CONTRACT-ONLY` by design |
| Prohibition assertions | No actuator path, no autonomous action | `IMPLEMENTED-UNCOMPILED` |
| `AeroraPayloadFactGroup` | — | `CONTRACT-ONLY` |
| Payload command set | — | `CONTRACT-ONLY` — Q-023 |
| Capability key → item mapping | — | `OPEN` — Q-023, Q3-009 |

Nothing in this module declares a payload state enumeration member, an actuator identity, a
command identifier, a component id, a pump, valve, nozzle, pressure, flow, tank level, spray rate,
colour, gimbal axis, calibration procedure, diagnostic schema, threshold or safety action.

### Why there is no fact group

Two independent reasons, both source-verified:

- **No dialect.** No `handleMessage` body can be written, and no Aerora fact name, unit, range or
  enumeration member is specified anywhere. Declaring facts would mean inventing them.
- **A `Fact` is unusable without metadata.** `Fact::setRawValue` is a no-op that warns when
  `_metaData` is null (`src/FactSystem/Fact.cc:135-161`), and the metadata resource would have to
  carry the fact names that do not exist yet.

So the five items are carried by `AeroraReportedValue` instead — the state class, the receive time
and the reporting source around an **opaque token**. When a dialect lands, each item migrates to a
`Fact` plus an `AeroraStateValue` inside the fact group, and consumers change from reading
`reportedToken()` to reading `fact()`. The state semantics do not change, because this module never
owned them.

## The five items, kept separate

| Item id | Interface | Subject |
| --- | --- | --- |
| `payloadIdentity` | `IF-PAY-001` | Identity and firmware version |
| `payloadState` | `IF-PAY-003` | The payload's own reported state |
| `payloadHealth` | `IF-PAY-004` | Reported health |
| `payloadSafetyState` | `IF-PAY-005` | Reported safety state |
| `payloadEmergencyStop` | `IF-PAY-006` | Emergency-stop state |

Each arrives in its own report and none implies another, so each carries its own class, receive
time and source. A unit test reports each one in turn and asserts the other four stay `UNKNOWN`.

Item ids are stable machine-readable identifiers for logs, records and lookup. None is a protocol
value, a fact name or an operator-facing label, and none is ever sent, parsed or matched against a
MAVLink field.

### Why the emergency stop is not a state member

Folding it into the payload state enumeration would imply it is one state in a sequence the payload
moves through. It is an **independent cut**. It would also make it impossible to record "the
payload was in state X when the stop fired", which is exactly what a debrief needs
(`AERORA-SAFE-REQ-011`).

### Why there is no causal field

Baseline 13's firmware model shows a watchdog fault reaching disarm logic, and separately reaching
payload emergency stop directly. Those are aircraft-side paths on `MODEL` evidence. What is
**reported** to the ground station is separate items, and **no evidenced message carries the linkage
between them** — Phase 3.1 made that explicit after finding the absence had been left to inference.

So `AeroraPayloadModel` has no field relating the stop to a watchdog event, an arming change, or
anything else. There is nothing to populate one from, and asserting causation the vehicle never
reported is the derivation `AERORA-GCS-REQ-029` prohibits. Temporal order is not causal order
either. A unit test asserts the item set is exactly five, so a sixth "cause" item cannot be added
quietly.

## Vehicle integration

`M-Vehicle` composes this model per vehicle from
`AeroraFirmwarePlugin::attachDomainServices()`, in dependency order — telemetry set first, because
it owns the freshness evaluator this model takes as a constructor argument:

```text
Vehicle
 └── AeroraVehicleServices                     attachTo(vehicle), parented to the Vehicle
      ├── AeroraVehicleIdentity
      ├── AeroraVehicleCapabilities            created in the ctor — never null
      ├── AeroraTelemetrySet        "telemetry"
      │    └── AeroraFreshness                 owned here, borrowed by the two models
      ├── AeroraPayloadModel        "payload"   ← this module
      └── AeroraPowerModel          "power"
```

Verified against the composition source, not assumed:

| Contract | Status |
| --- | --- |
| Constructor arguments | `new AeroraPayloadModel(freshness, capabilities)` — matches the published signature; `parent` defaults to null and `attachService()` reparents on success |
| Freshness ownership | Borrowed. `AeroraFreshness` is a QObject child of the telemetry set; this model holds it as a `QPointer` and never adopts it |
| Capability dependency | `AeroraVehicleServices::capabilities()` is **never null** — created in the services constructor. The *inventory* starts empty, which is what gates controls off, and a later report un-gates them through the live signal connections |
| QObject parenting | `attachService()` calls `setParent(this)`, so the model dies with the vehicle. The five items are children of the model |
| Per-vehicle isolation | One model per `Vehicle`; the plugin singleton holds no model member |
| Offline-editing vehicle | Deliberately **not** composed, so `service("payload")` returns null there. A consumer must null-check |

Destruction order is safe and was checked rather than hoped for. Qt deletes children in attachment
order, so the telemetry set — and the freshness evaluator and scheduler it owns — is destroyed
*before* this model. Every access is guarded twice: `QPointer` in `AeroraReportedValue`, and
`QPointer` again inside `ScheduledTask::cancel()`. After the evaluator dies the items keep their
tokens, report their declared class, and return `ageMs() == -1` with `freshnessEvaluated() == false`
— the honest answer, since the observation is real and its freshness is no longer knowable.

## State model dependency

This module creates no state classes. It consumes `M-Telemetry`:

| Consumed | Used for |
| --- | --- |
| `AeroraTelemetryState::Class` / `Kind` / `Observation` | The seven classes and the four declarable kinds |
| `AeroraTelemetryState::resolve()` | The single class-resolution rule in the build |
| `AeroraFreshness::evaluate()` / `ageMs()` / `nowUs()` | Freshness, and the clock domain stamps come from |
| `AeroraTelemetrySource` | The reporting source, attributed to the sending component |
| `AeroraStateValue::Capability` | The capability tri-state — not a second one |

All five payload items are declared `KindReported`: the payload controller is the reporting
authority for every one, and nothing evidenced reports a payload value "in force" distinctly from
reporting it. No payload item is `KindEffective`, so `isAuthoritative()` is false for all of them.

The declared kind is `const` with no setter, so a request or a proposal cannot become effective
payload state by any call sequence — the API that would do it does not exist.

**Freshness thresholds: none.** The payload freshness class is `aeroraPayload` and no threshold is
registered for it anywhere in this build (Q-036, and no payload message rate is evidenced). Every
observed item therefore reports `freshnessEvaluated() == false`, and a consumer presenting it must
surface the age rather than imply a currency verdict.

## Capability model

`IF-PAY-002` makes the capability set the gating mechanism: with no payload capability reported,
**no payload control is presented at all**, and the ground station is correct rather than broken.
`controlsPermitted()` is that gate, and `submitRequest()` enforces it rather than trusting a caller
to check.

Payload capabilities live in `AeroraVehicleCapabilities::Category::Payload` — one of the five
categories 17-capability-model.md defines. This module adds no category, no identifier and no enum.

| Capability evidence | `controlsPermitted()` |
| --- | --- |
| Nothing reported | `false` — "we have not been told" is not permission |
| Reported absent | `false` |
| At least one reported available | `true` |

### Why no capability key is mapped onto an item

No Aerora capability identifier is evidenced (Q-023), and whether the vehicle reports capability
*presence* separately from capability *availability* is itself unresolved (Q3-009, finding
F3.1-05). Deciding here that some key governs the health item would be inventing the mapping, and
would manufacture an `UNAVAILABLE` the vehicle never reported.

So the model never sets an item's capability from a capability report. Whoever eventually knows the
mapping calls `AeroraReportedValue::setCapability()` directly. Until then every item resolves
`UNKNOWN` rather than `UNAVAILABLE`, which is the honest answer and the one `IF-UIS-005` requires
stay distinguishable.

## The request boundary

```text
Operator act
    │
    ▼
AeroraPayloadRequest        opaque token, no state field
    │
    ▼  gated: valid? capability reported? command path installed?
AeroraPayloadModel::submitRequest()
    │
    ▼  (no implementation exists)
AeroraPayloadCommandSink
```

`submitRequest()` returns a `RequestOutcome` so the cause is recordable rather than a bare boolean,
and **changes no reported value under any outcome**. `Submitted` means the request reached the
command path. It never means the payload acted: `IF-PAY-003` requires payload state never be
inferred from a command the station sent, and `AERORA-PAYLOAD-REQ-011` requires effectiveness come
from a reported state change.

In this build every request receives `RefusedNoCommandPath`, because nothing implements the sink.

### Dependencies an implementation inherits

Recorded here so they are not rediscovered later. This module implements none of them and must not.

| Dependency | Owner | Obligation |
| --- | --- | --- |
| Command authorization | `M-Authz` (Tab 2) | Every request passes the command gate before MAVLink. The policy, the role set (Q-029) and the audit record are Tab 2's — see `aerora/src/Security/AeroraAuthorization.h` |
| Command tracking | `M-Authority` | The eight-state model — `REQUESTED`, `SENT`, `RECEIVED`, `ACCEPTED`, `REJECTED`, `EFFECTIVE`, `STALE`, `FAILED` (`AERORA-GCS-REQ-015`), including the accepted-but-not-effective case `AERORA-GCS-REQ-016` requires be surfaced, and no automatic retry of a stale request (`AERORA-GCS-REQ-017`) |
| Protocol mapping | Aerora dialect | A token has nothing to map onto until the dialect exists |
| Vehicle attachment | `M-Vehicle` (Tab 3) | `AeroraVehicleServices::attachService("payload", model)` |

`AeroraPayloadRequest` deliberately carries no state field, so this module cannot acquire a second,
divergent command tracker.

## Mission and resume boundary

This module has **no mission dependency and no persistence**. There is no save, restore, serialise
or replay API anywhere in it.

| Position | Status |
| --- | --- |
| Payload state retention across a mission interruption | **`NOT-EVIDENCED`** — `IF-PAY-014` |
| Ground-station payload state replay | **Rejected by decision** — `IF-MIS-015` carries nothing |
| Patching `includedResumeCommands` | **Rejected by decision** — unchanged, and untouched by this workstream |

Ownership does not imply persistence. A payload controller can own its state and lose it on a
reset, a power cycle, a mode change or a mission replacement. Nothing here claims otherwise.

The mission side pins the same position independently, in `M-Mission`'s
`AeroraMissionResumeBoundary` (`kGcsPayloadStateReplayImplemented`,
`kPayloadStatePersistenceEvidenced`). That is the authoritative statement about what a resume
carries; this module simply offers nothing to replay from. `clearReports()` is the only reset path
and it is teardown, not a checkpoint.

A unit test reports every item, disconnects, and asserts that re-reporting the capability restores
**nothing** — the regression a future "remember the last payload state" change would introduce.

## Product-profile boundary

Painting is the first product, not the architecture. Nothing in this module mentions paint, and
there is no product branch and no place to add one.

| Universal — here | Product-specific — `M-ProfilePainting`, layer 2 |
| --- | --- |
| Identity, state, health, safety state, emergency stop | Paint-specific telemetry |
| The request boundary and its outcomes | Pump, nozzle, spray-rate semantics |
| The capability gating mechanism | Which capabilities a painting payload declares |
| Emergency-stop representation | What a painting emergency stop does |

A product profile attaches through `AeroraProductProfile::attachToVehicle()` and
`AeroraVehicleServices::attachService()`. It may declare its own fact group, metadata and mission
items; it may not add a field to anything here. The row that will be breached first is
"report a paint condition as payload state" versus "let a paint condition reach a flight command" —
"stop the aircraft when the tank is empty" is an obviously sensible feature and it belongs in the
**aircraft**, not in the ground station.

Historical product information (a 6 L tank, two colour options, a solenoid valve, a peristaltic
pump, an airless nozzle, a 2-axis nozzle gimbal) is hardware design detail. It is **not** a GCS
protocol definition, and no fact, command or enumeration here derives from it.

## Verification status

| Check | Status | Evidence |
| --- | --- | --- |
| Compiles | **`BLOCKED`** | No `cmake`, `ninja`, compiler or Qt in the environment; no MAVLink headers in the tree |
| `just lint` / `pre-commit` | **`BLOCKED`** | No `pre-commit`, `clang-format` or `clang-tidy` available |
| Unit tests executed | **`BLOCKED`** | Requires a build |
| Formatting and banned constructs | Checked statically | 120-column limit, LF endings, no tabs, no trailing whitespace, newline at EOF, no `Q_ASSERT`, no uncategorised `qDebug()`, no `QTest::ignoreMessage`, no fixed-delay `QTest::qWait(n)`, `#pragma once` present |
| Declaration / definition agreement | Checked statically | Every method defined in a `.cc` has a matching declaration, and every non-inline declaration has a definition |
| Upstream API signatures | Source-verified | `Fact.cc` metadata behaviour, `FactGroup.h`, `FirmwarePlugin.h:353`, `Vehicle.cc:365-370` and `:588-592`, `ScheduledTask.h`, `ManualScheduler.h`, `UnitTest.h` log helpers |
| Overlay API signatures | Source-verified | `AeroraTelemetryState.h`, `AeroraStateValue.{h,cc}`, `AeroraFreshness.h`, `AeroraTelemetrySource.h`, `AeroraVehicleCapabilities.h`, `AeroraVehicleServices.h` |

**No claim is made that this code compiles, runs, or passes its tests.** The tests are written; they
have never been executed. They are deterministic by construction: every one injects
`ManualScheduler`, and none sleeps or waits on a wall clock.

One defect was found and fixed by inspection rather than by a run. `AeroraFreshness::evaluate()`
emits a `qCWarning` on the `aerora.telemetry` category the first time it evaluates a class with no
configured threshold — the expected state of every Aerora class while Q-036 is open. Under strict
log checking that fails any test which reads a state class on an *observed* item, which was four
tests here. Each now forces and consumes that warning explicitly with
`expectLogMessage` / `verifyExpectedLogMessage`, matching how `M-Telemetry`'s own suite handles it.
This is not a defect in `AeroraFreshness`: the warning is how an unconfigured threshold stays
visible instead of becoming a silent default.

## The staleness-verdict latch

`AeroraReportedValue` carries a one-directional staleness latch, and the invariant it exists for is:

> Once a report has been judged `STALE` against a configured threshold, destruction or loss of the
> freshness evaluator must not cause the report to become current again.

Without it, an item whose evaluator `QPointer` had gone null returned `ObservedFreshnessUnevaluated`,
`AeroraTelemetryState::resolve()` mapped that to the item's declared kind, and
`isCurrent()` answered true for a reading known to be old — the failure `DEG-04` and `IF-TLM-011`
forbid. This was recorded as HO-TAB6-01 and is now **fixed**.

| Transition | Latch |
| --- | --- |
| Evaluation reaches `ObservedStale` | set |
| Evaluation reaches `ObservedFresh` | cleared — a newer receive time put the value back inside the window |
| Threshold removed while still observed | **verdict preserved** — configuration change is not evidence the value became current |
| Evaluator gone, latch set | `ObservedStale` — stays `STALE` |
| Evaluator gone, latch clear | `ObservedFreshnessUnevaluated` — evaluator absence alone is **never** stale |
| New report | cleared **before** the report becomes observable — a new freshness epoch |
| `clearReport()` | cleared — nothing observed, so `UNKNOWN`, not `STALE` |

The latch introduces no threshold and cannot make a value stale that was never judged stale. It
mirrors the latch `M-Telemetry` implements in `AeroraStateValue` deliberately: the two types carry
the same observation semantics over different value stores, and a divergence would be a defect
visible in only one domain.

It also makes destruction order irrelevant to correctness, which is the more valuable outcome than
the fix itself — no consumer now depends on the composition root attaching services in a particular
sequence.

## External handoffs

Items this module found but does not own. Nothing here is worked around locally.

### HO-TAB6-03 — `AeroraStateValue`'s lifetime note states the destruction order backwards

| Field | Detail |
| --- | --- |
| **Owner** | `M-Telemetry` — `aerora/src/Telemetry/AeroraStateValue.h`, `\section lifetime` |
| **Severity** | Low — documentation only. The implementation is correct |

The note says the telemetry set "is that object's first child and therefore its **last**
destroyed", and that "the groups and models holding values attach later and are destroyed earlier".
Both halves appear inverted: `QObjectPrivate::deleteChildren()` iterates the children list forward
from index 0, so the **first** child attached is the **first** destroyed. On that reading the
evaluator is destroyed *before* the payload and power models, not after.

Caveat on confidence: Qt is not vendored in this tree, so this rests on knowledge of
`deleteChildren()` rather than on source I could read here. Worth a maintainer confirming before
editing.

Why it is only a documentation issue: the same note correctly says the ordering "is the expectation,
not an assumption this type relies on", and the latch handles either order. So nothing is broken.
The risk is that a future reader concludes the evaluator-loss window cannot occur in the composed
graph and removes the latch as dead code — when in fact the window occurs on every vehicle
teardown.

### HO-TAB6-02 — Tab 3 comment misdescribes the capability pointer

`AeroraFirmwarePlugin::attachDomainServices()` comments the capability inventory as "Null until a
capability is reported". The pointer is **never** null — `AeroraVehicleServices` creates it in its
constructor initialiser list. The *inventory* starts empty, which is what gates controls off. The
behaviour is correct and in fact better than the comment implies, because a later report can
un-gate controls through this model's live signal connections; only the prose is wrong. Reported to
Tab 3; not edited here.

## Open questions

Only genuinely unresolved items. None is a placeholder for a decision this module could have made.

| Question | Subject | Effect here |
| --- | --- | --- |
| **Q-023** | Payload state, actuator and command enumerations; capability identifiers | Values are opaque tokens; no command set; no key → item mapping. The content of 7 of 10 generic elements |
| **Q3-006** | Whether the payload controller is a distinct component or a flight-controller partition | No payload component id is assumed. Decides what the reporting-source field will mean |
| **Q3-009 / F3.1-05** | Whether capability presence is reported separately from availability | The capability tri-state exists and nothing sets it. `UNAVAILABLE` is unreachable in practice |
| **Q-009** | Emergency-stop behaviour — what it does, how it is cleared | Only its representation exists |
| **Q-043** | Calibration procedures and diagnostic content | Neither is represented |
| **`IF-PAY-014`** | Payload state retention across an interruption | `NOT-EVIDENCED`. Needs a payload rig (`BENCH`) |
| **`IF-MAV-022`** | Whether a relayed value identifies its originator | Source attribution is permanently `SendingComponent` |
| **Q-036** | Per-class staleness thresholds | None configured; every item is freshness-unevaluated |
| **Q4-002 / R4-001** | The `FirmwarePlugin::factGroups()` multi-vehicle hazard | Blocks the fact group, not this model |
| **Q-029** | The operator role set | Blocks the authorization binding on the command sink |

### What would close the biggest one

Q-023 is the gate. A payload interface artefact — a dialect definition, a message set, or a
capability identifier list — converts seven `OPEN` rows to implementable in one step, and turns the
five opaque tokens into Facts with metadata. Until one exists, more code here would be invention.

## Files

| File | Purpose |
| --- | --- |
| `../Supervisory/AeroraReportedValue.{h,cc}` | The reported-item primitive, shared with `M-Power`. Not a new module |
| `AeroraPayloadCommandSink.h` | `AeroraPayloadRequest` and the one outbound seam, deliberately unimplemented |
| `AeroraPayloadModel.{h,cc}` | The five items, capability gating, the request boundary, the pinned prohibitions |
| `../PayloadPower.cmake` | Source manifest and the integration instructions for `aerora/CMakeLists.txt` |
| `../../test/Payload/AeroraPayloadMock.{h,cc}` | Deterministic driver, `MOCK / TEST ONLY` |
| `../../test/Payload/AeroraPayloadModelTest.{h,cc}` | Eleven tests over separation, gating, the request boundary, the prohibitions and the composition contract |

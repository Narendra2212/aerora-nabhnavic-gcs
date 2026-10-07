# User Interface State Interface Control Document

What the operator interface shows, which of five state classes each value belongs to, and the three
tiers of seam through which Aerora content reaches the screen.

Architecture in [ui-architecture.md](../01-aerora-architecture/ui-architecture.md). This document is
where the rest of the specification either holds or quietly fails, because every honesty rule in
Phase 3 is ultimately a rule about a pixel.

## Five state classes

The directive's mandatory distinction, extended to the two cases that make it usable. **Every value
the Aerora interface presents belongs to exactly one of these classes, and the class is visible.**

| Class | Means | Authority | Presentation |
| --- | --- | --- | --- |
| **Effective** | The vehicle reports this is in force now | Vehicle | **Primary.** The default thing the operator reads |
| **Reported** | The vehicle reported this, and it is current enough to use | Vehicle | Primary, with its freshness available |
| **Requested** | The ground station asked for this; the vehicle has not reported it in force | Ground station | Subordinate and labelled as requested |
| **Proposed** | A companion computer suggests this; nobody has accepted it | Companion | Subordinate to vehicle state and labelled as proposed |
| **Stale** | Reported once, now past its freshness threshold | Vehicle, aged | **Never shown as current.** Shown with its age |

And two non-values, which are not a sixth class but are the two ways a class can be empty:

| Non-value | Means | Presentation |
| --- | --- | --- |
| Unknown | The vehicle has not reported it | Explicitly unknown |
| Unavailable | This aircraft does not have the capability | Absent, not blank |

The distinction between "unknown" and "unavailable" is the one most often collapsed, and collapsing
it produces the specific failure this whole set exists to prevent: an operator who cannot tell
"the payload has nothing to report" from "the payload panel is missing".

```text
Effective  ──►  shown primary
Reported   ──►  shown primary, freshness available
Requested  ──►  shown subordinate, labelled            never promoted to Effective
Proposed   ──►  shown subordinate, labelled            never promoted to Effective
Stale      ──►  shown with age                         never presented as current
Unknown    ──►  shown as unknown                       never shown as zero
Unavailable ─►  absent                                 never shown as unknown
```

The arrows that matter are the ones that do not exist. Nothing promotes Requested or Proposed to
Effective; that promotion is the second architectural principle violated, and it is the easiest
thing in the world to do by accident in a binding expression.

## Interface contracts — presentation

| ID | Interface | Direction | Producer | Consumer | Crossing | Update semantics | Authority | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-UIS-001` | Effective state presentation | GCS internal | Fact system | Operator display | The vehicle-reported in-force value, as the primary reading | On change | Vehicle | `GCS-DECIDED` | `PROPOSED` |
| `IF-UIS-002` | Requested state presentation | GCS internal | Command tracker | Operator display | The requested value, **subordinate and labelled** | On request and on each command state transition | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-UIS-003` | Proposed state presentation | GCS internal | Autonomy fact group | Operator display | The companion proposal, **subordinate to vehicle state and labelled as proposed** | On proposal | Companion | `GCS-DECIDED` | `PROPOSED` |
| `IF-UIS-004` | Staleness presentation | GCS internal | Freshness evaluator | Operator display | A stale marking and the value's age | On crossing the class threshold | Ground station observes | `GCS-DECIDED` | `PROPOSED` |
| `IF-UIS-005` | Unknown and unavailable presentation | GCS internal | Fact system, capability model | Operator display | Either an explicit unknown marking **or** absence of the element | On change in reporting or capability | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-UIS-006` | Action availability | GCS internal | Capability model, vehicle state | Operator display | Whether an action is **hidden**, **disabled with a reason**, or available | On capability or state change | Ground station | `GCS-VERIFIED` mechanism | `PROPOSED` |
| `IF-UIS-007` | Command progress presentation | GCS internal | Command tracker | Operator display | The command's current state of the eight, with **accepted-but-not-effective surfaced** | Per transition | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-UIS-008` | Confirmation presentation | GCS internal | Authority module | Operator display | That a confirmation is pending, as a **distinct visible state** | On pending and on resolution | Vehicle requests; operator acts | `MODEL` | `PROPOSED` |
| `IF-UIS-009` | Independent-indicator separation | GCS internal | Several producers | Operator display | The constraint that named indicator pairs **share no visual language** | — | Ground station | `BASELINE-ASSERTED` | `PROPOSED` |

| ID | Freshness semantics | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `IF-UIS-001` | Carries the receive time of the reporting message | If no effective value has been reported, the field is **unknown** — a requested value is never shown in its place | `AERORA-SAFE-REQ-006`, `AERORA-GCS-REQ-029` | R-028 | — | `INTEGRATION` |
| `IF-UIS-002` | Carries the request time and the elapsed time since | A request that goes `STALE` or `FAILED` is shown as such and **not retried**. The previous effective value remains the primary reading | `AERORA-GCS-REQ-016`, `AERORA-GCS-REQ-017` | R-028 | Q-036 | `INTEGRATION` |
| `IF-UIS-003` | Per-class threshold; a proposal past it is stale, then absent | **An absent companion and a companion reporting no proposal are different displays.** Acceptance is never inferred from a proposal disappearing | `AERORA-GPU-REQ-005`, `AERORA-GPU-REQ-006`, `AERORA-GPU-REQ-007` | — | Q-008 | `INTEGRATION` |
| `IF-UIS-004` | **This interface is the freshness presentation** | A stale value is marked and aged. It is not hidden, because hiding it loses the last known reading, and it is not shown plain, because that is a lie | `AERORA-GCS-REQ-027` | — | Q-036 | `INTEGRATION` |
| `IF-UIS-005` | Unknown carries no value and no age | **Unknown is never rendered as zero, dash-as-zero, or an empty string that reads as a value.** Unavailable is absent | `AERORA-GCS-REQ-026`, `AERORA-GPU-REQ-007` | — | Q3-009 | `INTEGRATION` |
| `IF-UIS-006` | Capability presence is not time-varying; state availability is | A capability the aircraft lacks is **hidden**. A capability it has but cannot use right now is **disabled with a stated reason**. Defaulting to disabled for an absent capability invites the operator to wait | `AERORA-UI-REQ-008` | — | Q3-009 | `DEMO` |
| `IF-UIS-007` | Each state carries its transition time | **A command accepted and never effective is surfaced.** That gap is the single most valuable thing the command model detects, and it is worthless if the interface does not show it | `AERORA-GCS-REQ-015`, `AERORA-GCS-REQ-016` | R-025 | Q-036 | `INTEGRATION` |
| `IF-UIS-008` | Pending state carries its age | **A pending confirmation the operator cannot see is the failure that makes supervised autonomy unsafe.** Absence of the pending report is itself surfaced | `AERORA-SAFE-REQ-007` | — | Q-007 | `INTEGRATION` |
| `IF-UIS-009` | — | Each named pair is distinguishable without reading a label. Verified by observation, because no unit test can assert that two indicators look different | `AERORA-AWS-REQ-008`, `AERORA-SAFE-REQ-011` | — | Q-040 | `DEMO` |

### The indicator pairs that must not share visual language

`IF-UIS-009` is a constraint rather than a data flow, and it has a specific enumerated scope. Each
pair exists because conflating it produces a wrong operator response, not merely a confusing one.

| Pair | Wrong response if conflated | Source |
| --- | --- | --- |
| Cloud connectivity and vehicle link | A cloud outage reads as a link problem; the operator stops flying for no reason | `AERORA-AWS-REQ-008`, `IF-CLD-001` |
| Vehicle-reported active link and ground-station-local link observation | The operator trusts one station's view of a link the vehicle has already abandoned | `IF-TLM-006` |
| Payload emergency-stop state and payload state | An emergency stop reads as a payload fault, or a fault reads as a stop | `IF-PAY` set |
| Watchdog-initiated disarm and payload emergency stop | One is read as a consequence of the other, which is a ground-station inference | `AERORA-SAFE-REQ-011` |
| Authority mode and flight mode | The operator believes they changed what commands the aircraft when they changed how it flies | R-028, `IF-AUT-001` |
| Requested authority mode and effective authority mode | The operator believes a request was honoured | `AERORA-SAFE-REQ-006` |

The last two are the same mistake at two levels, and R-028 exists because the conflation is natural:
both are modes, both are selected from a list, and only one of them the ground station can change.

### Hide or disable

The conventional interface advice is the opposite of this rule, so the reasoning is worth stating.

| Situation | Presentation | Why |
| --- | --- | --- |
| The aircraft does not have the capability | **Hidden** | A disabled control says "this exists and is momentarily unavailable", which is false. It invites the operator to wait for something that will never arrive |
| The aircraft has it but cannot use it in this state | **Disabled, with the reason stated** | Here the unavailability genuinely is temporal, and the reason is actionable |
| The operator is not authorised | **Disabled, with the reason stated** — or hidden, by product decision | Either is defensible; what is not defensible is relying on visibility as the gate. See `IF-SEC-011` |

The rule in one line: **hide what the aircraft cannot do, disable with a reason what it cannot do
right now.**

This depends on knowing which of the two a given absence is, and the vehicle must report capability
presence separately from capability state for the distinction to be available at all. Nothing
evidenced says it does. That is Q3-009, and until it is answered `IF-UIS-006` can be built but not
populated correctly for Aerora-specific capabilities.

## Interface contracts — composition seams

Three tiers, and the preference order is the most consequential rule in this document because the
tiers differ in maintenance cost by an order of magnitude and the most expensive one fails silently.

| ID | Interface | Direction | Producer | Consumer | Crossing | Update semantics | Authority | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-UIS-010` | Tier-1 C++ option seam | GCS internal | Aerora core plugin override | Upstream UI composition | Return values of `QGCOptions`, `QGCFlyViewOptions` and `QGCCorePlugin` virtuals | Compile time | Ground station | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-UIS-011` | Tier-2 purpose-built QML seam | GCS internal | Aerora QML | Upstream composing QML | Content at four upstream-provided empty seam files | Build time, resolved at QML load | Ground station | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-UIS-012` | Tier-3 populated-file override | GCS internal | Aerora QML | Upstream resource URL | **A silent fork** of a populated upstream file, by resource alias | Build time, resolved at QML load | Ground station | `GCS-VERIFIED` | `PROPOSED` |
| `IF-UIS-013` | Aerora QML module registration | GCS internal | Aerora QML modules | QML engine | Named module URIs exported as libraries | Build time | Ground station | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-UIS-014` | Override resolution | GCS internal | Aerora plugin's URL interceptor | QML engine | Each `qrc:` URL request, rewritten to the overlay's copy **where one exists** | Per URL request, at load | Ground station | `GCS-VERIFIED` mechanism; Aerora wiring `NOT-EVIDENCED` | `OPEN` |
| `IF-UIS-015` | Startup self-check | GCS internal | Aerora plugin | Operator, visibly | Assertions on override resolution, module importability, plugin selection and absence of a listener | Once, before the interface is shown | Ground station | `GCS-DECIDED` | `PROPOSED` |

| ID | Freshness semantics | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `IF-UIS-010` | Fixed for a build | A changed virtual **signature** is a compile error. A changed virtual **semantic** is not, which is what the drift review is for | `AERORA-UI-REQ-004` | R-014 | Q-038 | `STATIC` |
| `IF-UIS-011` | Fixed for a build | The four seams are upstream-documented as override points, so upstream has little reason to change them. Absence of Aerora content leaves the upstream empty item | `AERORA-UI-REQ-004` | R-014 | Q-020 | `INTEGRATION` |
| `IF-UIS-012` | Fixed for a build; the fork point is recorded | **When upstream fixes a bug in an overridden file, the Aerora build keeps the bug and nothing reports it.** No compiler warning, no lint rule, no merge conflict | `AERORA-UI-REQ-004`, `AERORA-UI-REQ-005`, `AERORA-UI-REQ-006` | R-014, R-019, R-026 | Q-038 | `AUDIT` |
| `IF-UIS-013` | Fixed for a build | A module URI is a namespace, so an Aerora control cannot collide with an upstream control of the same name. A missing module is an import error at load | `AERORA-UI-REQ-010` | R-019 | — | `INTEGRATION` |
| `IF-UIS-014` | Not applicable | **The failure is silent in both directions.** A missing or misaliased overlay resource leaves the upstream file in place with no error; an unintended overlay resource replaces upstream behaviour with no error. The build succeeds either way. The resolution **rule** is deterministic and verified in source; what is unevidenced is whether Aerora's own resources are declared correctly | `AERORA-UI-REQ-007`, `AERORA-TEST-REQ-007` | R-019 | Q-020 | `INTEGRATION` |
| `IF-UIS-015` | Once per launch | **A failed self-check is a visible startup error, not a log line.** An interface quietly missing its Aerora layer is worse than one that refuses to start | `AERORA-UI-REQ-007`, `AERORA-TEST-REQ-007` | R-019, R-010 | Q-020 | `DEMO` |

### How override resolution actually works

Corrected in Phase 3.1. Phase 2 and the first Phase 3 pass framed this as **alias precedence** —
"two resources aliased to one URL, and which one the engine resolves". That premise is wrong, and
the correction matters because it changes what has to be verified and when.

The mechanism is **explicit interception keyed on existence**, not shadowing. All five steps are
`GCS-VERIFIED` by reading the checkout:

| Step | Where | What it does |
| --- | --- | --- |
| 1 | `src/API/QGCCorePlugin.h:117` | `createQmlApplicationEngine(QObject*)` is **virtual**, with a symmetric `destroyQmlApplicationEngine` hook documented as the release point for URL interceptors |
| 2 | `src/QGCApplication.cc:275` | The application obtains its engine **through the plugin**, so a custom override is on the real boot path |
| 3 | `custom-example/CMakeLists.txt:93-97` | Override resources are declared under `PREFIX "/Custom/qml"` with `QT_RESOURCE_ALIAS` set to the **stock** resource path |
| 4 | `custom-example/src/CustomPlugin.cc:240-248` | The plugin override installs a `QQmlAbstractUrlInterceptor` on the engine |
| 5 | `custom-example/src/CustomPlugin.cc:273-295` | `intercept()` forms `:/Custom<origPath>` and returns the rewritten `qrc:` URL **if and only if `QFile::exists()` is true**; otherwise it returns the original unchanged |

```text
QML engine requests  qrc:/qml/QGroundControl/FlyView/FlyViewCustomLayer.qml
      │
      ▼
interceptor forms   :/Custom/qml/QGroundControl/FlyView/FlyViewCustomLayer.qml
      │
      ├── exists  ──►  returns the /Custom URL        the overlay file loads
      └── absent  ──►  returns the original URL       the upstream file loads
```

Three consequences for Phase 4:

| Consequence | Detail |
| --- | --- |
| **No two resources ever share a URL** | The overlay copy lives under a different prefix. There is no collision and no undetermined precedence |
| **The rule is deterministic** | Overlay wins if and only if the overlay resource exists at the mirrored path. One existence check, no ordering, no link-order dependency |
| **The override is a tier-1 virtual, not a build trick** | It needs no core edit, and the engine-destruction hook exists specifically so the interceptor can be detached |

**The startup self-check assertion is therefore writable today.** The first Phase 3 pass recorded
that the "resolved content is the Aerora content" assertion could not be specified until Q-020 was
answered. It can: for each overridden stock URL, assert that the engine's interception returns a
`/Custom`-prefixed URL, which is equivalent to asserting the mirrored resource exists. That removes
a circular dependency the first pass believed it had.

#### What remains open, and why `IF-UIS-014` stays `OPEN`

| Settled by source evidence | Still requires a build |
| --- | --- |
| The resolution rule | That Aerora's own resources are declared under the mirrored prefix with the right aliases |
| That the hook is a supported virtual on the boot path | That the Aerora plugin actually installs the interceptor |
| That no alias collision is possible | That no resource is mirrored **unintentionally**, silently replacing upstream behaviour |

The residual is a wiring question about Aerora's own code, and no amount of source reading answers
it. Q-020 is therefore **partially resolved**: the mechanism is determined, the overlay wiring is
not. Phase 3.1 records the evidence and leaves the question register to the reviewer.

### Tier inventory, carried forward

Phase 2 took the inventory. Phase 3 does not retake it; it records the counts as the contract
surface and the single tier-3 entry as the thing to keep at one.

| Tier | Surface | Count |
| --- | --- | --- |
| 1 | `QGCOptions` virtuals | 30 |
| 1 | `QGCFlyViewOptions` virtuals | 6 |
| 1 | `QGCCorePlugin` override points | Several, including analyze pages, settings-group visibility, palette, advanced-UI message, fact value grid defaults |
| 2 | Upstream-provided empty QML seams | 4 |
| 3 | Populated upstream files overridden | **1** |

The four tier-2 seams cover the whole Aerora fly-view surface: overlay content, guided actions,
additional-actions entries and map visuals. That finding is what keeps this architecture cheap, and
it was not obvious before the inventory.

The one tier-3 entry is the fly-view tool strip, which upstream composes from a literal array with
no insertion seam. There is no way to add an Aerora tool-strip action without replacing the list.

| Obligation on each tier-3 entry | Purpose |
| --- | --- |
| The fork-point commit recorded | Makes "what has upstream changed since?" answerable by one command |
| A named owner | Someone is accountable for reviewing upstream changes to that file |
| A drift review at each upstream merge | The only way the silent-fork failure gets caught |
| A recorded reason | So the override can be removed if upstream later adds a seam |

The owner for the tool-strip entry is still `OPEN` in the Phase 2 inventory, and Phase 3 does not
assign it — that is Q-038's territory. The drift review is a procedure, and procedures decay; the
mitigation is to keep the list at one entry and prefer proposing a seam upstream over adding a
second.

### Why `IF-UIS-012` is specified at all

A tier-3 override leaves the upstream file untouched in the working tree. `git diff` stays empty.
That is precisely what makes the drift silent, and it is why the override is specified as an
interface with an owner rather than left to version control to surface.

It is also why the core-edit count stays at zero while this exists: a resource override is a
build-configuration action in the overlay, not a modification of an upstream file. The distinction
is real and it is not a loophole — but it does mean "zero core edits" and "zero silent forks" are
different claims, and only the first one is currently true.

Stated as a rule Phase 4 must inherit:

> **"No QGroundControl source modification" does not mean "no behavioural divergence from
> upstream."**
>
> A resource override avoids modifying upstream source and still replaces upstream behaviour at
> runtime. **No claim of zero behavioural divergence may be made without drift evidence.**

The drift-detection obligation is therefore a Phase 4 engineering and continuous-integration
deliverable, not a documentation claim. `AERORA-TEST-REQ-008` requires it to be mechanical rather
than procedural, `IF-UIS-012` carries the per-override obligations, and R-026 is the risk it closes.
Phase 3.1 implements no checker.

## Interface contracts — operator input

| ID | Interface | Direction | Producer | Consumer | Crossing | Update semantics | Authority | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-UIS-016` | Operator action origination | Operator to GCS | Operator | Authorisation gate, then command path | The intent to perform an operation | Event | Operator proposes; the gate decides | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-UIS-017` | Advanced-UI disclosure | Operator to GCS | Operator | UI visibility | A visibility preference. **Not a permission** | Event | Ground station | `GCS-VERIFIED` | `CONFIRMED` |

| ID | Freshness semantics | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `IF-UIS-016` | Action time is the request time | Every action passes `IF-SEC-004` before anything reaches a link. A denial is shown with its reason and recorded | `AERORA-SEC-REQ-012`, `AERORA-GCS-REQ-019` | R-004 | Q-029 | `UNIT` |
| `IF-UIS-017` | Not applicable | **The toggle grants no permission to any operation.** It may be combined with a role; it never substitutes for one | `AERORA-SEC-REQ-014` | — | Q-029 | `STATIC` |

Aerora may use the toggle to reduce routine visual clutter. What it may not do is let the toggle be
the mechanism protecting an operation that requires an authorised operator — a security control one
click deep. The full position is in [13-security-icd.md](13-security-icd.md) at `IF-SEC-011`.

## Presentation rules traced to their source

Each rule below comes from another document. The interface is where it holds or fails, which is why
they are collected here rather than restated in each ICD.

| Rule | Interface | Source |
| --- | --- | --- |
| Effective state primary; requested subordinate and labelled | `IF-UIS-001`, `IF-UIS-002` | Architectural principle 2 |
| Every value shows its age when stale; stale never shown as current | `IF-UIS-004` | `AERORA-GCS-REQ-027` |
| Cloud and vehicle-link connectivity share no visual language | `IF-UIS-009` | `AERORA-AWS-REQ-008` |
| Companion proposals subordinate and labelled | `IF-UIS-003` | `AERORA-GPU-REQ-005` |
| An unavailable action is hidden, not shown disabled | `IF-UIS-006` | `AERORA-UI-REQ-008` |
| Vehicle-reported active link distinguished from local observation | `IF-UIS-009` | `AERORA-COMM-REQ-007` |
| Payload emergency-stop state shown distinctly from payload state | `IF-UIS-009` | `AERORA-SAFE-REQ-011` |
| A command accepted but not effective is surfaced | `IF-UIS-007` | `AERORA-GCS-REQ-016` |
| An enumeration value with no metadata entry is not presented | `IF-UIS-005` | `AERORA-GCS-REQ-026` |
| An unknown value is never rendered as zero | `IF-UIS-005` | `AERORA-GCS-REQ-029` |

## Conflicts recorded

| Conflict | Position A | Position B | Resolution authority |
| --- | --- | --- | --- |
| Hide versus disable for an unauthorised operation | `AERORA-UI-REQ-008` governs capability, not authorisation, so it does not decide this case | Hiding an operation from an unauthorised operator conceals the system's shape; disabling it discloses what exists | **Unresolved and deliberately so.** A product and operations decision under Q-029. `IF-UIS-006` permits either, and prohibits relying on visibility as the gate |
| Zero core edits versus zero silent forks | ADR-001 states zero core edits are required, and a resource override is not a core edit | `IF-UIS-012` is a silent fork of a populated upstream file by any reasonable reading | **Recorded, not resolved.** Both claims are true of different things. Phase 3 keeps them as two separate claims rather than letting the first imply the second |

The second is a terminology conflict rather than a technical one, and flattening it would let a
precise claim ("no upstream file is modified in the working tree") be read as a broader one ("no
upstream behaviour is diverged from").

## Not specified, and why

| Item | Reason |
| --- | --- |
| Aerora visual design language, colours, typography, iconography | Q-040; a product and design decision |
| Screen layouts and panel arrangement | Design deliverable; the state-class contract constrains it without specifying it |
| Which guided actions appear | Depends on Q-007 flight-mode mapping and Q-023 payload actuators |
| Whether Aerora's own overlay resources are declared and resolved correctly | Q-020 residual. The resolution **rule** is `GCS-VERIFIED` and documented above; the overlay wiring needs a build and is confirmed by `IF-UIS-015` |
| The tier-3 override owner | Q-038 |
| Whether an unauthorised operation is hidden or disabled | Q-029 |
| How capability presence is distinguished from capability state | Q3-009, and [17-capability-model.md](17-capability-model.md) |
| Freshness threshold values per class | Q-036; the per-class structure is specified, the numbers are not |

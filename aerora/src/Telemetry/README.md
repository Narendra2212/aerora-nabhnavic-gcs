# Aerora Telemetry and State (`M-Telemetry`)

The state representation every other Aerora module and the operator interface read from, and the
reasons it is shaped the way it is.

Owns `IF-TLM-001` through `IF-TLM-013`. Depends on layer 1 only (QGroundControl's Fact system and the
timing utilities) — no Aerora module, no vehicle plugin, no security, no UI.

**Build status: `IMPLEMENTED-UNCOMPILED`.** No compiler, CMake, `clang-format` or `pre-commit` exists
in the development environment this was written in, so nothing here has been built, linted or
executed. See [Verification status](#verification-status) for exactly what was and was not checked.

## What is reused from QGroundControl, and what is added

The starting position was that QGroundControl already has a substantial telemetry model and that
replacing it would be a mistake. Reading the source confirmed that, and also showed precisely where
it stops.

| Mechanism | Source | Aerora's use |
| --- | --- | --- |
| `Fact` as the single value store | `src/FactSystem/Fact.h` | **Reused unchanged.** Every Aerora value is a Fact (`AERORA-GCS-REQ-026`). No parallel value store exists in this module |
| `FactMetaData` from JSON | `src/FactSystem/FactMetaData.h` | **Reused unchanged.** Units, bounds and enumerations come from metadata, never from code |
| `FactGroup` as the grouping and rate-limiting unit | `src/FactSystem/FactGroup.h` | **Extended by composition.** `AeroraFactGroupBase` derives from it |
| Attachment to a vehicle | `FirmwarePlugin::factGroups()`, consumed at `src/Vehicle/Vehicle.cc:365-371` | **Reused as the only seam.** No core edit |
| Message fan-out to every group | `src/Vehicle/Vehicle.cc:588-592` | **Reused unchanged.** Each group filters itself; there is no routing table |
| Appearance in the operator's value selector | `src/QmlControls/InstrumentValueData.cc` | **Free consequence** of attaching to the vehicle |
| Inclusion in the telemetry CSV record | `Vehicle::_initializeCsv` / `_writeCsvLine` | **Free consequence**, with one constraint — see [FactGroup strategy](#factgroup-strategy) |
| Monotonic clock and injectable scheduler | `src/Utilities/Timing/` (`RuntimeScheduler`, `QtRuntimeScheduler`, `MonotonicClock`) | **Reused unchanged.** This is why freshness is deterministically testable |
| Deterministic test clock | `test/Utilities/Timing/Support/ManualScheduler.h` | **Reused unchanged.** No second fake clock was written |

What QGroundControl does not have, verified by reading `Fact.h`, `FactMetaData.h` and `FactGroup.h`
in full:

| Absent upstream | Consequence |
| --- | --- |
| Any timestamp on a `Fact` | Freshness cannot be expressed at all |
| Any freshness, staleness or max-age field in `Fact` or `FactMetaData` | Staleness has nowhere to live |
| Any validity or "unknown" flag | Unknown is encoded as `NaN` for floating-point facts, and as magic numbers (`255`, `0`, `-1`) for integer facts, which have no unknown representation |
| Any reporting-source provenance | `Fact::_componentId` exists but is `0` for every telemetry fact in the tree |
| Any reset when telemetry stops | `FactGroup::_setTelemetryAvailable` is used as a one-way latch at all but one of its ~40 call sites |
| A `FactGroupManager` | `Vehicle` *is* a `FactGroup`; its group map is the registry |

The single upstream precedent for real staleness is `VehicleGPSAggregateFactGroup`, which expires
values on a deadline, exposes staleness as a Fact and is the only place `_setTelemetryAvailable(false)`
is ever reached. This module generalises that shape rather than inventing one.

### Why a normalisation layer was not built

A `QGC → Aerora adapter → normalised state` pipeline was considered and rejected. It would solve a
problem Aerora does not have: there is one protocol, one Fact system and one value representation,
and a translation layer between them would add a copy of every value with no cross-product benefit.

What *is* added is strictly the metadata `Fact` has no room for — the receive time, the reporting
source and the state class. The value itself is never copied out of its Fact. That is the smallest
change that satisfies `AERORA-GCS-REQ-027`, and it keeps the Fact system as the single source of
truth for values.

## The state model

Seven classes, from the five in `16-ui-state-icd.md` plus its two non-values. `AeroraTelemetryState`
holds them, and nothing collapses them.

```text
EFFECTIVE    the deciding authority reports this in force      primary, authoritative
REPORTED     a component reports this                          primary, with its age
REQUESTED    the ground station asked for this                 subordinate, labelled
PROPOSED     a non-authoritative source suggests this          subordinate, labelled
STALE        observed once, now past its threshold             shown with its age, never as current
UNKNOWN      nothing has been observed                         explicitly unknown, never zero
UNAVAILABLE  the capability is reported absent                 the element is absent
```

The arrows that matter are the ones that do not exist. Nothing promotes `REQUESTED` or `PROPOSED` to
`EFFECTIVE`.

### How a class is produced

Two inputs, one pure function, `AeroraTelemetryState::resolve`:

| Input | Where it comes from | Settable later? |
| --- | --- | --- |
| **Declared kind** — `REPORTED`, `EFFECTIVE`, `REQUESTED` or `PROPOSED` | The item's row in its interface contract | **No.** `const`, set at construction, no setter exists |
| **Observation** — unavailable, not observed, fresh, stale, or freshness-unevaluated | Capability model plus the freshness evaluator | Yes |

```text
capability absent           ──────────────────────►  UNAVAILABLE
nothing observed            ──────────────────────►  UNKNOWN
observed, past threshold    ──────────────────────►  STALE
observed, within threshold  ──────────────────────►  the declared kind
observed, no threshold set  ──────────────────────►  the declared kind, freshnessEvaluated() == false
```

Observation outranks kind for the first three rows, because an absent capability, an absent
observation and an aged-out observation are conditions, not things a producer may declare itself to
be. Kind governs the last two, and only a kind of `EFFECTIVE` can ever resolve to `EFFECTIVE`.

### Why the declared kind is `const`

This is the load-bearing decision in the module. The failure to prevent — a proposal or a request
read as what the aircraft is doing — is described in `11-telemetry-icd.md` as the error the operator
pays for, and it is trivially easy to introduce in a binding expression.

A runtime check would catch it in the cases someone remembered to check. Making the kind a `const`
member with no setter means **the API that would do it does not exist**: there is no call sequence
that turns an `AeroraStateValue` declared `PROPOSED` into an `EFFECTIVE` one. That is also why
`05-qml-implementation-map.md`'s rule is expressible at all:

> The state class is a property of the binding, not a decision made per screen.

## Freshness

| Property | Value |
| --- | --- |
| Freshness source | Receive time, stamped at parse, in the monotonic clock domain |
| Timestamp source | `RuntimeScheduler::nowUs()` — `MonotonicClock` in production, `ManualScheduler` in tests |
| Threshold | **`OPEN`** — none is configured and none is invented. Q-036 |
| Threshold granularity | Per class, where a class is an opaque producer-declared name |

### The threshold is open, and stays open

`11-telemetry-icd.md` states the per-class thresholds have their "shape specified; values not", and
no message rate is evidenced anywhere. Choosing a number here would produce, in the ICD's own words,
"either false staleness on a slow link, or an operator waiting for a value that will never update".

So with no threshold registered, `AeroraFreshness::evaluate` returns a third outcome,
`ObservedFreshnessUnevaluated`. It is not a lenient reading of "fresh":

- it never claims the value passed a freshness check, because no check ran;
- it never claims the value is stale, because nothing says it is;
- the age remains available from `ageMs()`, so a consumer can show how old the value is, which is
  what `DEG-04` and `IF-UIS-004` require of a value whose currency is in question;
- `freshnessEvaluated()` is `false`, so a consumer can tell that no verdict exists;
- the condition is warned about once per class, so an unconfigured threshold is visible rather than
  silently benign.

`hasThreshold()` and `configuredClasses()` exist so the startup self-check (`IF-UIS-015`) can assert
which classes a given build has configured.

### Freshness class granularity is deliberately opaque

The ICD specifies that thresholds are per class and does not enumerate the classes. Rather than
invent a category set, a class is a caller-supplied `QString`: producers declare the name, and
configuration registers thresholds against the same names. When Q-036 resolves, the names it uses
become the configuration keys with no code change here.

## Timestamps

Three timestamps are distinguishable in principle. Only one is implemented, and the other two are
recorded as open rather than approximated.

| Timestamp | Status | Semantics |
| --- | --- | --- |
| **Receipt** | `IMPLEMENTED-UNCOMPILED` | Stamped at parse, before the Fact's value becomes visible. Monotonic microseconds. The only timestamp freshness is computed from |
| **Source** (MAVLink `time_boot_ms` / `time_usec`) | `OPEN` | Not retained. No Aerora message exists to carry one, no Aerora item requires one, and no fact anywhere in the upstream tree retains one. Adding it before a dialect exists would be speculative |
| **Derived / local** | `NOT-EVIDENCED` | None. The ground station computes no timestamps of its own |

### A staleness verdict is not withdrawn by losing the ability to evaluate

Found during the lifetime review and fixed. `AeroraStateValue` holds its evaluator through a
`QPointer`, so destroying the evaluator does not dangle — but the original code mapped the resulting
"cannot evaluate" condition to `ObservedFreshnessUnevaluated`, which resolves to the item's declared
kind. **A value already judged `STALE` therefore returned to `REPORTED` or `EFFECTIVE`** — a stale
value silently presented as current, which is the one failure the state model exists to prevent.

The fix is a latch. Once an observation has been judged `STALE` against a configured threshold, the
verdict is recorded and is withdrawn only by a new observation or by clearing the observation. It is
armed inside the expiry callback, at the instant staleness becomes true, so it does not depend on a
consumer having read the value first.

It introduces no threshold and changes no threshold semantics: it cannot make a value stale that was
never judged stale against a configured threshold. Two conditions now behave correctly that did not:

| Condition | Before | After |
| --- | --- | --- |
| Evaluator destroyed while a stale value lives | Value returned to its declared kind | Stays `STALE`; `ageMs()` returns -1 |
| `clearThreshold()` called on a class with stale values | Values returned to their declared kind | Stay `STALE` |

The same defect exists in `AeroraReportedValue` (`aerora/src/Supervisory/`), which is not this
module's file — reported, with the fix, in the final report.

Two invariants, both inherited from `src/Utilities/IO/ReadTimestamp.h` and `MonotonicClock`:

- **A zero stamp means "never observed" and never becomes a fresh receipt.** `recordObservation`
  refuses a zero receive time rather than storing it, because storing it would silently un-observe an
  item that had a real reading.
- **A stamp ahead of the clock has no valid age.** It indicates the stamp and the evaluator disagree
  about their clock domain, which is a defect. The evaluator warns and declines the verdict rather
  than reporting a fresh or stale value derived from an age it cannot compute. `ageMs()` returns `-1`.

Why the stamp is at parse rather than at display: freshness cannot be reconstructed after the fact.
This is also why Aerora fact groups use `FactGroup` update rate **zero**. A non-zero rate coalesces
`Fact::valueChanged` onto a timer, which moves the apparent receive time — acceptable for an upstream
display convenience, not acceptable for a value that feeds the audit record.

The ordering obligation that follows is explicit, and the unit test asserts it: a group stamps the
observation **first** and sets the Fact **second**, so a consumer reacting to `valueChanged`
synchronously already sees a stamped, classified value.

## Reporting source

`AeroraTelemetrySource` carries the observed MAVLink system and component identifiers plus an
**attribution** stating which of two things those identifiers mean.

| Attribution | Produced today? |
| --- | --- |
| `Unidentified` | Yes — the initial state. A value with no source is not a value from an anonymous source; it is a value that has not arrived |
| `SendingComponent` | Yes — the only attribution any Aerora code produces |
| `OriginatingComponent` | **No.** Reserved for `IF-MAV-022`, which is `OPEN` |

### Why there is no source-role enumeration

Modelling the source as a role — flight controller, companion, payload controller, power controller —
would be inventing a mapping from component identifiers to roles. MAVLink identifies the component
that *sent* a message; for a payload or power value relayed through the flight controller, the sender
is the flight controller, not the originating subsystem.

`11-telemetry-icd.md` records this as an open conflict and resolves it in favour of honesty: the
ground station records the sending component and must not present it as the originating subsystem.
This module implements exactly that and no more. No component identifier is invented, and no role is
inferred.

The consequence worth stating plainly: **the ICD's own rationale for carrying the source — that a
flight-controller reading and a companion-relayed reading are not equally trustworthy — cannot be
acted on yet.** The provenance is recorded; the trust assessment it would enable waits on
`IF-MAV-022`.

## Per-class rules

### Effective state

Produced only by an item whose declared kind is `EFFECTIVE`, from the deciding authority's report.
`isAuthoritative()` is true for `EFFECTIVE` and nothing else.

Never derived from an operator request, a companion proposal, stale telemetry, or a ground-station
prediction. Where a specific field's effective-state rule is not given by a source contract, it stays
`OPEN`; this module supplies the mechanism, not the per-field mapping, which is the
`IF-FC` / `IF-AUT` sets' territory.

### Requested state

`REQUESTED` exists in the model so the command tracker has a class to publish into, and so the
interface can show a request subordinate and labelled. The eight-state command tracker itself is
`M-Authority`, not this module.

A successful local enqueue is not a state change. Nothing here converts a request into `REPORTED` or
`EFFECTIVE`; that transition requires an authoritative vehicle report, which arrives as a separate
observation on a separate item.

### Proposed state

`PROPOSED` is for non-authoritative sources — companion proposals, and the ground station's own local
observation of which link is carrying traffic (`11-telemetry-icd.md` classes that as proposed, with
the vehicle's report as effective, "never promoted").

Acceptance is never inferred. A proposal that stops arriving goes `STALE` and then, if the item is
cleared, `UNKNOWN` — it does not become accepted, and `DEG-11` requires that an absent companion and
a companion reporting no proposal remain two different displays. Nothing in this module reads a
proposal disappearing as the aircraft adopting it.

### Unknown versus unavailable

Kept apart by a tri-state capability input on `AeroraStateValue`:

| Capability | Resolves to |
| --- | --- |
| `CapabilityUnknown` (the initial state) | Falls through to the observation — so an unreported item is `UNKNOWN` |
| `CapabilityPresent` | Falls through to the observation |
| `CapabilityAbsent` | `UNAVAILABLE`, outranking any observation |

The tri-state is the point. Collapsing "we have not been told about this capability" into "the
aircraft does not have it" would manufacture an `UNAVAILABLE` the vehicle never reported — the
derivation `AERORA-GCS-REQ-029` prohibits. `IF-UIS-005` and `IF-UIS-006` are `BLOCKED` under finding
F3.1-05 precisely because nothing evidenced reports capability presence separately from capability
state (Q3-009), so the capability input exists and **no Aerora code sets it yet**.

This module never renders anything. It guarantees the distinction survives to the consumer; the
"unknown is never shown as zero, unavailable is absent" presentation rule is `M-Ui`'s to honour.

## FactGroup strategy

| Group | Class | Protocol binding | Status |
| --- | --- | --- | --- |
| — | `AeroraFactGroupBase` | None needed | `IMPLEMENTED-UNCOMPILED` |
| `aeroraSystem` | `AeroraSystemFactGroup` | `IF-MAV-012` candidate | `CONTRACT-ONLY` |
| `aeroraPayload` | `AeroraPayloadFactGroup` | `IF-MAV-014` candidate | `CONTRACT-ONLY` |
| product group | per profile | `IF-MAV-015` candidate | `CONTRACT-ONLY` |
| `aeroraPower` | `AeroraPowerFactGroup` | `IF-MAV-012` candidate | `CONTRACT-ONLY` |
| `aeroraLink` | `AeroraLinkFactGroup` | `IF-MAV-012` candidate | `CONTRACT-ONLY` |
| `aeroraAutonomy` | `AeroraAutonomyFactGroup` | `IF-MAV-018` candidate | `CONTRACT-ONLY` |

**No concrete group is implemented here, and that is the correct state.** All six are `CONTRACT-ONLY`
for one reason: none has a message to parse. No Aerora dialect exists, so no `handleMessage` body can
be written, and no Aerora fact name, unit, range or enumeration member is specified anywhere. Writing
them would mean inventing them.

Nothing in this module declares a fact, a threshold, a rate, an enumeration member, a component
identifier, a pump or nozzle state, a pressure, a tank level, a voltage or current limit, a tether
transition, an autonomy confidence, a thermal limit, a radio or link-quality threshold, or a GPS
accuracy limit.

### The one constraint groups must respect

Groups are created during **vehicle construction only**. `Vehicle::_initializeCsv` enumerates the
vehicle's fact groups once to write the telemetry CSV header, and `_writeCsvLine` re-enumerates them
for every row; a group appearing after the header is written shifts every later column. That is R-027
and `AERORA-GCS-REQ-022`.

`AeroraTelemetrySet` enforces it mechanically: handing out `factGroups()` seals the set, and a later
`registerGroup` is refused with a warning rather than accepted and silently corrupting the record.

### What attaching to the vehicle buys for free

Because the groups hang off the vehicle through the standard seam, every Aerora Fact appears in the
operator's telemetry value selector and in the telemetry CSV record with **no Aerora logging or UI
code at all**.

## Vehicle integration boundary

```text
Tab 3 / M-Vehicle, M-Firmware                      M-Telemetry (this module)
──────────────────────────────                     ─────────────────────────
AeroraFirmwarePlugin::initializeVehicle(Vehicle*)
  └── AeroraVehicleServices::attachService(
          AeroraTelemetrySet::kServiceName,  ─────► AeroraTelemetrySet
          new AeroraTelemetrySet())                   ├── AeroraFreshness (one per vehicle)
                                                      └── AeroraFactGroupBase subclasses
  └── telemetrySet->publishTo(registrar)  ──────────► publishTo() → seals, hands each group over
         ▲
         └── the one authorised core edit lives here (see ADR below)
```

Ownership is settled and needs no core edit: one `AeroraTelemetrySet` per vehicle, reparented into
that vehicle's service graph, owning that vehicle's `AeroraFreshness`. Nothing in this module touches
the vehicle plugin, the vehicle factory, vehicle classification, vehicle lifecycle or vehicle
identity.

What needed deciding was the separate question of whether those groups also enter QGroundControl's
own per-vehicle fact-group map. That is Q4-002, resolved below.

## ADR — Q4-002: Aerora fact groups enter the vehicle's fact-group map

**Status:** DECIDED by M-Telemetry. **Implementation:** telemetry side complete; blocked on one
authorised QGroundControl core edit.

### Decision

**Option B.** Aerora fact groups are registered into each vehicle's own fact-group map, per vehicle,
by `AeroraTelemetrySet::publishTo()` calling through an `AeroraFactGroupRegistrar` whose single
concrete implementation holds `friend`ship with `Vehicle`.

`FirmwarePlugin::factGroups()` is **not** used and must never be wired to `AeroraTelemetrySet`.

### Evidence that decided it

Five findings, all source-verified at `9459a4b5`:

| # | Finding | Location |
|---|---|---|
| 1 | Upstream grants `friend class` to two types **specifically** so they can call `_addFactGroup` per vehicle, with the comment `// Allow call _addFactGroup` | `src/Vehicle/Vehicle.h:108, 115` |
| 2 | Both friends register groups **dynamically, at message-handling time** — long after construction | `FactGroupListModel.cc:44`, `GimbalController.cc:475` |
| 3 | `_addFactGroup()` stores a raw pointer, does **not** reparent, and emits `factGroupNamesChanged()` | `FactGroup.cc:133-146` |
| 4 | `initializeVehicle()` is called **inside the `Vehicle` constructor**, at `:161`, and the CSV log timer only starts afterwards at `:170` | `Vehicle.cc:161, 170` |
| 5 | The CSV header is written lazily on the first `_writeCsvLine()` after arming — strictly later than (4) | `Vehicle.cc:2823-2830` |

Findings 1 and 2 are the ones that changed the answer. Per-vehicle fact-group registration is not a
mechanism Aerora would be inventing; it is an **established upstream pattern used twice**, and what
Aerora lacks is the privilege, not a design. Finding 2 also shows upstream itself adds groups
dynamically for every multi-battery, multi-ESC and gimbal-equipped vehicle, so the CSV column-shift
hazard R-027 describes already exists upstream independently of Aerora.

Findings 4 and 5 remove the only real objection to Option B: registering from `initializeVehicle()`
is constructor-time, so Aerora groups are present before the CSV header is written. **No column
shifts, and R-027 holds** — Aerora still adds nothing dynamically, which `publishTo()` enforces by
sealing.

### Why the shared-plugin hazard does not apply

The hazard is specific to `FirmwarePlugin::factGroups()`: it takes no `Vehicle *`, and
`FirmwarePluginManager` caches one plugin per firmware class, so groups returned through it are
shared by every vehicle of that class and the aircraft overwrite each other's telemetry.
`ArduSubFirmwarePlugin` has that defect upstream today.

A registrar is constructed by a caller that already holds the `Vehicle *`. The sharing hazard is
therefore **structurally absent**, not avoided by discipline. `AeroraFactGroupBase`'s runtime
detector remains as a regression check, not as the mitigation.

### Rejected alternatives

**Option A — service-only telemetry; accept the losses.** Rejected.

The dispatch loss is recoverable; the other two are not. Tab 3 verified that neither the value picker
(`InstrumentValueData::factGroupNames()`) nor the CSV writer (`Vehicle::_initializeCsv`) can be
reached from outside the vehicle's fact-group map. Accepting Option A means re-implementing two
upstream features inside Aerora and maintaining them for four product profiles — a permanent cost
taken on to avoid a one-line core edit. It also silently diverges Aerora telemetry from upstream
telemetry in the operator's own tooling: an Aerora value would be absent from the picker that shows
every other value, with no explanation visible to the operator.

**Option C — Aerora-specific dispatch via `FirmwarePlugin::adjustIncomingMavlinkMessage()`.**
Rejected, and it is the most tempting option because it needs no core edit at all.

It buys only dispatch. The picker and CSV remain unreachable, so it does not actually close the gap —
it closes a third of it and adds a second dispatch path that must be kept consistent with upstream's
fan-out ordering forever. Worse, the hook is a message *adjustment* seam whose `false` return
**drops the message** for `ParameterManager`, FTP, terrain and the remote-ID manager
(`Vehicle.cc:564-567`). Doing dispatch work inside it puts Aerora code on the path where a mistake
breaks parameter loading for every vehicle of the class. Tab 3 identified this seam as mechanically
possible and declined to implement it; that judgement is correct.

**Option B′ — per-call handover through `factGroups()`.** Rejected. `factGroups()` has exactly one
caller, so a plugin could in principle build a fresh set per call. But it makes a seam whose header
documents a singleton return carry hidden per-call state, and it depends on undocumented ordering
inside core. If that ordering changes the failure is silent.

### The core edit required

Not performed. Reported for Tab 1's authorisation. The telemetry side is complete, so the
integration is **two coordinated lines** and no new code.

```text
File:            src/Vehicle/Vehicle.h
Location:        the existing friend-declaration block, immediately after
                     friend class GimbalController;   // Allow GimbalController to call _addFactGroup
                 which is line 115 at 9459a4b5. Inside class Vehicle, before `public:` at :118
Edit 1:          friend class AeroraVehicleFactGroupRegistrar;   // Allow call _addFactGroup

File:            aerora/CMakeLists.txt   (overlay, Tab 1 / M-Plugin)
Edit 2:          add AERORA_VEHICLE_FACTGROUP_FRIENDSHIP to CUSTOM_DEFINITIONS

Problem:         FactGroup::_addFactGroup() is protected; no Aerora type may call it
Why the existing
seam is          FirmwarePlugin::factGroups() is the only no-core-edit route and it shares fact
insufficient:    group objects across every vehicle of a firmware class
Body changes:    none. No Vehicle function is modified, no member added, no signature changed.
                 A friend declaration changes access only; it generates no code
Alternative:     propose upstream that _addFactGroup gain a public per-vehicle counterpart, or that
                 factGroups() take a Vehicle*. Either fixes ArduSubFirmwarePlugin's defect too and
                 removes the need for the friend line entirely. Preferred if there is appetite
Upstream drift:  minimal. One entry in a friend list that already exists for this exact purpose and
                 carries this exact comment, twice. A merge conflict here is textual and obvious
Rollback:        delete both lines. The module keeps compiling and every registration refuses with a
                 reason naming the missing edit. No data migration, no persisted state
Security:        none. Friendship widens compile-time access to one Aerora type, not runtime
                 capability. The registrar sends nothing, authorises nothing and cannot reach a link;
                 Aerora's command authorisation gate is a separate, unrelated seam
```

**The two edits must be applied together.** Friend declaration without the define leaves the
registrar inert — it validates and refuses, so Aerora telemetry simply never appears in the picker or
the CSV. Define without the friend declaration is a compile error, which is the safe direction.
`AeroraVehicleFactGroupRegistrar::isRegistrationAvailable()` exists so the startup self-check can
detect the inert case rather than discovering it as missing telemetry mid-flight.

### Test requirement that comes with the core edit

Everything testable without a `Vehicle` is already covered (see the test inventory). Two assertions
need a real vehicle fixture and therefore belong with whoever owns the `VehicleTest`-based suites:

1. After `initializeVehicle()`, `vehicle->factGroupNames()` contains each published Aerora group name
   and `isRegistrationAvailable()` is true.
2. Two MockLink vehicles of the same firmware class hold **different** group objects under the same
   name — the regression test for the hazard this design avoids.

Until (1) exists, `AeroraVehicleIntegrationTest`'s current assertion that no `aerora`-prefixed group
reaches the vehicle map is correct and should be inverted as part of the same change.

### Consequences

| Area | Consequence |
|---|---|
| **Multi-vehicle** | Correct by construction. Each vehicle has its own set, evaluator and group objects; the registrar is per vehicle. `_twoSetsShareNoGroupsOrEvaluator` asserts it |
| **Lifetime** | Unchanged. Groups stay owned by the `AeroraTelemetrySet`, which is a child of the vehicle's service graph. `_addFactGroup` does not reparent, so no ownership moves to `Vehicle` |
| **Shared state** | None. No static, no cache, no plugin member |
| **QGC compatibility** | Follows the existing pattern exactly. Aerora groups behave like battery, ESC and gimbal groups |
| **UI** | Aerora values appear in the operator's telemetry value picker with no Aerora UI code. `InstrumentValueData` listens to `factGroupNamesChanged()` and re-resolves, so late arrivals are handled |
| **CSV** | Aerora facts appear in the telemetry record with no Aerora logging code, and in the header, because registration precedes it |
| **Testing** | The publication contract is fully testable today against a test registrar, with no core edit. Only the ten-line concrete registrar remains untested until authorised |
| **Maintenance** | One friend line to re-apply on upstream merge. No parallel dispatch, picker or CSV implementation to maintain |
| **Future products** | A new product profile's fact group is registered by the same `registerGroup()` + `publishTo()` path. Nothing per-product is added to the core edit |

### Until the core edit lands

`publishTo()` is callable and correct, and publishes nothing, because no concrete Aerora fact group
exists — no dialect. So the practical state is unchanged and nothing is blocked: the decision is
recorded, the telemetry half is written and tested, and the core edit can be authorised whenever Tab 1
chooses. Aerora telemetry remains `IMPLEMENTATION-PLANNED`, now for multi-vehicle as well as
single-vehicle.

## UI boundary

The interface consumes `AeroraStateValue` and reads only. Everything needed to render by class is a
`Q_PROPERTY` or a `Q_INVOKABLE`:

| Member | Purpose |
| --- | --- |
| `stateClass` | Which of the seven classes to render. Resolved, not chosen per screen |
| `fact` | The value, with its metadata-supplied units, bounds and enumerations |
| `declaredKind` | The contract's kind, for records and for diagnostics |
| `freshnessEvaluated` | False when no freshness verdict exists, so the age must be surfaced |
| `ageMs()` | Age in milliseconds, or `-1` when there is no valid age |
| `observed`, `receiveTimeUs` | Whether and when anything arrived |
| `source` | The reporting source and its attribution |
| `capability` | The capability tri-state |
| `stateChanged` | Fires on class, observation, source and capability changes, and at threshold expiry |

`ageMs()` is deliberately a function rather than a notifying property: age advances continuously with
no event to signal, so a consumer displaying it must re-read it.

This module chooses no colour, icon, label or layout. `classToString()` and `kindToString()` return
stable machine-readable identifiers for logs and records, not operator-facing text — visual design is
Q-040 and `AeroraValueDisplay` is `M-Ui`'s artefact.

## Companion / GPU boundary

```text
Companion / GPU  ──proposes──►  Flight controller  ──decides──►  Aircraft
                                                                    │
                                        GCS observes and supervises ┘
```

Companion output reaches this module as items declared `PROPOSED`, and there is no code path that
makes a proposal effective. `aeroraAutonomy` is read-only and has no outbound path. Companion
presence, health and proposal content are all `CONTRACT-ONLY` pending the dialect.

## What this module must never do

It reports conditions and decides nothing about the aircraft.

There is no path from a value going stale, from telemetry going absent, or from a link degrading, to a
mode change, a return-to-launch, a land, a disarm, or any other aircraft action.
`18-error-degradation-model.md` records "none" in the authority column of all twelve aircraft-reported
conditions. It also authorises no command — that is `M-Authz`, reached through the core plugin's
command hook, and this module has no dependency on it.

## Verification status

| Check | Status | Evidence |
| --- | --- | --- |
| Compiles | **`BLOCKED`** | No `cmake`, `ninja`, `cl`, `g++` or Qt in the environment |
| `just lint` / `pre-commit` | **`BLOCKED`** | No `pre-commit`, `clang-format` or `clang-tidy` available |
| Unit tests executed | **`BLOCKED`** | Requires a build |
| Formatting and banned constructs | Checked statically | 120-column limit, LF endings, no tabs, no trailing whitespace, newline at EOF, no `Q_ASSERT`, no uncategorised `qDebug()`, no `QTest::ignoreMessage`, no fixed-delay `QTest::qWait(n)`, `#pragma once` present, balanced delimiters |
| Declaration / definition agreement | Checked statically | Every method defined in a `.cc` has a matching declaration, and every non-inline declaration has a definition |
| Upstream API signatures | Source-verified | `Fact.h`, `FactMetaData.h`, `FactGroup.h`, `FactGroupListModel.cc`, `Vehicle.h` friend list, `Vehicle.cc:161/170/365`, `GimbalController.cc:475`, `RuntimeScheduler.h`, `MonotonicClock.h`, `ScheduledTask.h`, `ManualScheduler.h`, `QGCTest.cmake`, overlay `CMakeLists.txt` contract |

**No claim is made that this code compiles, runs, or passes its tests.** The tests are written and
registered; they have never been executed.

Tests are deterministic by construction: every one injects `ManualScheduler` and advances time
explicitly. None sleeps, waits on a wall clock, or depends on a real delay.

## Open questions

Only genuinely unresolved items. None is a placeholder for a decision this module could have made.

| Question | Subject | Effect here |
| --- | --- | --- |
| **Q-036** | Per-class staleness threshold values | No threshold is configured; every item resolves `ObservedFreshnessUnevaluated`. The mechanism is complete and unparameterised |
| **Q3-009 / F3.1-05** | Whether the vehicle reports capability presence separately from capability state | The capability tri-state exists and nothing sets it. `UNAVAILABLE` is unreachable in practice until this resolves |
| **IF-MAV-022** | Whether a relayed value identifies its originator | Source attribution is permanently `SendingComponent`. Trust assessment by source role is not possible |
| **Message rates** | Not evidenced anywhere | Blocks sizing thresholds (Q-036) and checking the main-thread Fact budget against R-013 |
| **Aerora dialect** | Fact names, units, ranges, enumeration members, message definitions | All six fact groups stay `CONTRACT-ONLY` |
| **Source timestamps** | Whether any Aerora item needs `time_boot_ms` / `time_usec` | Not retained. Revisit with the dialect |
| **Test registration point** | Whether the overlay registers its own CTest entries or Tab 1 centralises them | Source list and the required `add_qgc_test` call are documented in `Telemetry.cmake`; the call is not made from this module |

## Files

| File | Purpose |
| --- | --- |
| `AeroraTelemetryState.{h,cc}` | The seven classes, the four declarable kinds, the resolution rule, the class predicates, the module's logging category |
| `AeroraTelemetrySource.{h,cc}` | The reporting source: observed identifiers plus attribution |
| `AeroraFreshness.{h,cc}` | Per-class thresholds, the evaluator, the age and expiry primitives |
| `AeroraStateValue.{h,cc}` | The Fact-plus-class binding the interface consumes |
| `AeroraFactGroupBase.{h,cc}` | Fact group base: rate zero, stamping helpers, the cross-vehicle regression check |
| `AeroraFactGroupRegistrar.{h,cc}` | The registration contract. Abstract, so the publication path is testable without the privilege |
| `AeroraVehicleFactGroupRegistrar.{h,cc}` | The one concrete registrar. Per-vehicle, validates and refuses observably; its single privileged statement is gated on `AERORA_VEHICLE_FACTGROUP_FRIENDSHIP` |
| `AeroraTelemetrySet.{h,cc}` | One vehicle's groups and evaluator; `kServiceName`, `publishTo()`, sealing |
| `Telemetry.cmake` | Source manifest and the integration instructions for `aerora/CMakeLists.txt` |
| `../../test/Telemetry/AeroraTelemetryStateTest.{h,cc}` | Ten deterministic tests over state separation, freshness, authority and source |
| `../../test/Telemetry/AeroraTelemetryAttachmentTest.{h,cc}` | Nine deterministic tests over the service name, publication, sealing, two-vehicle isolation and the staleness latch |

## Service name

`AeroraTelemetrySet::kServiceName` is `"telemetry"`, and this module owns it — matching
`AeroraMissionObserver`, `AeroraPayloadModel` and `AeroraPowerModel`, each of which publishes its own
`kServiceName`. It is a service-registry key for `AeroraVehicleServices::attachService()`, never a
protocol identifier: it is not sent, parsed or matched against any MAVLink field.

`AeroraFirmwarePlugin::kTelemetryServiceName` was a provisional stand-in declared at the composition
root because this type published nothing. It should now be deleted so the constant is not duplicated
— see the handoff note in the final report for the three call sites.

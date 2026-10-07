# Aerora Power Abstraction (`M-Power`)

A configuration and state model for a dual-source aircraft, and a list of behaviours this module
refuses to implement because no evidence supports them.

Owns `IF-PWR-001` through `IF-PWR-012`. Depends on `M-Telemetry` for the state model and
`M-Vehicle` for the capability inventory and the attachment point.

**Build status: `IMPLEMENTED-UNCOMPILED`.** No compiler, CMake, Qt, `clang-format` or `pre-commit`
exists in the development environment this was written in. Nothing here has been built, linted or
executed. See [Verification status](#verification-status).

## The confirmed architecture

Two physical **operating configurations**, from baseline 49:

```text
TETHER CONFIGURATION
  480 V DC from the ground  →  tether  →  onboard DC-DC stage  →  48 V aircraft domain

BATTERY CONFIGURATION
  onboard battery  →  aircraft power domain
```

| Element | Evidence | Status |
| --- | --- | --- |
| Dual-power requirement, two physical configurations | `BASELINE-ASSERTED` | `CONFIRMED` |
| 480 V DC tether input, specified at product level | `BASELINE-ASSERTED` | `CONFIRMED` |
| Onboard conversion to a 48 V domain | `BASELINE-ASSERTED` | `CONFIRMED` |
| Dual tether and battery input connectors | `ARTIFACT-OBSERVED` — schematic | `CONFIRMED` |
| Current and power monitoring hardware | `ARTIFACT-OBSERVED` — schematic | `CONFIRMED` |
| Converter topology, source arbitration, diode-OR behaviour, tether-loss detection, transition transients | **Explicitly not proven by the schematic** — baseline 49's own words | `OPEN` |

### What the confirmed rows do not establish

The 480 V figure is an architectural fact about the aircraft. It establishes **nothing** about
minimum tether voltage, maximum current, DC-DC efficiency, thermal limits, allowable voltage drop,
a transition threshold, or emergency behaviour. None of those exists in this module, and
`AeroraPowerModel::kDefinedThresholdCount` is pinned at zero so none can be added quietly.

## Source representation

```text
Battery ──┐
          ├──  AeroraPowerModel  ──►  display, logging
Tether  ──┘
```

The model represents what the vehicle reports about each configuration. It does not arbitrate
between them, rank them, or move between them.

`enum class Configuration { Tether, Battery }` exists only as an addressing key for
`configuration()`. It is **not** a protocol value, **not** the value set of `IF-PWR-003`'s active
source (whose enumeration is `UNKNOWN`), and **not** an arbitration priority. Adding a member would
require a third confirmed physical configuration, not a software decision.

### The seven items

| Item id | Interface | Declared kind | Subject |
| --- | --- | --- | --- |
| `powerTetherConfiguration` | `IF-PWR-001` | `Reported` | Tether connected or disconnected |
| `powerBatteryConfiguration` | `IF-PWR-002` | `Reported` | Battery present or absent |
| `powerEffectiveState` | `IF-PWR-003` | **`Effective`** | What the vehicle reports is in force now |
| `powerReportedState` | `IF-PWR-003`, `IF-PWR-009` | `Reported` | The last reported value, with its receive time |
| `powerSourceTransition` | `IF-PWR-004` | `Reported` | A transition **the vehicle reported** |
| `powerFault` | `IF-PWR-007` | `Reported` | A reported power-system fault |
| `powerWatchdogEvent` | `IF-PWR-011` | `Reported` | A reported watchdog event |

Exactly one item is `Effective`, so exactly one can ever be read as authoritative. A unit test
asserts the item set is exactly these seven and that the other six are `Reported`.

Item ids are stable machine-readable identifiers for logs, records and lookup. None is a protocol
value, a fact name or an operator-facing label.

### Why effective and reported are separate

They answer different questions — "what is in force" and "what was last said" — and they diverge
the moment a report goes stale. A single value would force a choice, and it would choose wrongly
under exactly the conditions that matter.

The unit test drives this directly: with a threshold configured, the effective item ages to
`STALE`, and a fresh ordinary report arriving afterwards stays `REPORTED`. It is **not** promoted
into the gap the stale effective value left. Only the vehicle reporting the effective state again
restores it.

### Why a reported transition item exists but no inferred one

`IF-PWR-004` is a real interface item, so the item exists to carry it — but only when the vehicle
reports one. Nothing derives a transition from a configuration report changing, from a report going
stale, from a fault, or from a watchdog event. A tether configuration becoming absent while a
battery configuration becomes present is **two reports, not a transition**.

The unit test reports the tether, then the battery, then the tether again, and asserts zero
transition signals and an unobserved transition item.

Q-026 is therefore not blocking. The interface accommodates a yes or a no without redesign:

| Property | Consequence |
| --- | --- |
| Active source is a reported item, not a derived one | If switching exists the vehicle reports the change; if not, the value never changes |
| Source transition is its own item | If transitions do not exist, it is never populated and the display omits it |
| Power capability gates the surface | An aircraft supporting one configuration shows only that one |
| The ground station initiates nothing | True whether or not transitions exist |

## Telemetry boundary

This module reuses QGroundControl's battery telemetry **unchanged** and duplicates none of it.

| Mechanism | Source | Aerora's use |
| --- | --- | --- |
| Per-battery Facts with metadata units | `src/Vehicle/FactGroups/BatteryFactGroupListModel.{h,cc}` | **Reused unchanged.** Parses `BATTERY_STATUS`, `HIGH_LATENCY`, `HIGH_LATENCY2` into voltage, current, consumption, percentage, temperature, charge state and instantaneous power, with unknown encoded as `NaN` |
| Generator, EFI, RPM and ESC fact groups | `src/Vehicle/FactGroups/` | Reused unchanged where the aircraft reports them |
| Aerora state model | `M-Telemetry` | Consumed — see below |

No voltage, current, consumption, percentage, temperature, charge state or instantaneous power
appears in this module. `batteryConfiguration()` is `IF-PWR-002` — battery present or absent — which
is a different statement from any battery measurement, and it is **reported** rather than inferred
from a battery message arriving.

There is no tether equivalent to reuse: QGroundControl has no power tether representation. The only
occurrences of "tether" in the tree are a RemoteID aircraft-type string and ArduSub's `tetherTurns`
turn counter, neither of which is a power concept. Confirmed by source search.

### Vehicle integration

`M-Vehicle` composes this model per vehicle from `AeroraFirmwarePlugin::attachDomainServices()`,
attached third, after the telemetry set and the payload model. Verified against the composition
source:

| Contract | Status |
| --- | --- |
| Constructor arguments | `new AeroraPowerModel(freshness, capabilities)` — matches the published signature |
| Freshness ownership | Borrowed. **The same evaluator instance the payload model gets**, owned by the telemetry set and adopted by neither. That is intended: thresholds are per class, not per model, so one evaluator per vehicle keeps a class's threshold identical wherever it is read |
| Capability dependency | `capabilities()` is never null; the inventory starts empty, which gates the extended surface off |
| QObject parenting | Reparented to `AeroraVehicleServices`; the seven items are children of the model |
| Per-vehicle isolation | One model and one evaluator per `Vehicle` |
| Offline-editing vehicle | Not composed, so `service("power")` returns null there |
| Thresholds at composition | None registered. `kDefinedThresholdCount == 0` holds at compile time and `hasThreshold("aeroraPower")` is false at runtime on the composed path |

The shared evaluator is the one place a double-ownership defect could hide: if either model adopted
it, destroying that model would take the evaluator out from under the other and the survivor's
`QPointer` would go null mid-flight. `_twoModelsShareOneEvaluatorAndNeitherOwnsIt` pins that.

### State model dependency

This module creates no state classes. It consumes `M-Telemetry`'s
`AeroraTelemetryState::resolve()`, `AeroraFreshness`, `AeroraTelemetrySource` and
`AeroraStateValue::Capability`. The declared kind of each item is `const` with no setter, which is
what makes "a stale report does not become the effective value" structural rather than a review
rule.

**Freshness thresholds: none.** The power freshness class is `aeroraPower` and no threshold is
registered for it anywhere in this build (Q-036, and no power message rate is evidenced). A unit
test asserts that at runtime. Every observed item therefore reports
`freshnessEvaluated() == false`, and a consumer must surface the age rather than imply a currency
verdict.

### Why there is no fact group

`AeroraPowerFactGroup` is `CONTRACT-ONLY` for the same two source-verified reasons as the payload
group: no dialect exists to parse, and `Fact::setRawValue` is a no-op that warns when `_metaData` is
null (`src/FactSystem/Fact.cc:135-161`), so a Fact without a metadata resource cannot hold a value.
The metadata would have to carry fact names, units and enumeration members that are `UNKNOWN`
(Q-024).

## Arbitration and transition status

| Behaviour | Status | Pinned as |
| --- | --- | --- |
| Automatic source switching | **Not implemented** | `kAutomaticSourceTransitionImplemented = false` |
| Source arbitration | **Not implemented** | `kPowerArbitrationImplemented = false` |
| Diode-OR behaviour | **Not implemented** | Covered by the arbitration constant |
| Tether-loss response | **Not implemented** | No outbound path exists to respond through |
| Transition behaviour and failover timing | **`OPEN`** — Q-026 | `kAutomaticSourceTransitionImplemented = false` |
| Power failover | **Not implemented** | Same |
| Remaining-energy estimation | **Not implemented** | `kRemainingEnergyEstimationImplemented = false` |

Baseline 49 states the two configurations are "not yet a claim of automatic in-flight source
switching", and that any future transition or hybrid operation "requires separate engineering and
verification". The absence of an estimation algorithm is likewise the design, not a gap: an energy
figure the ground station computed would be a ground-station opinion displayed next to vehicle
facts. `IF-PWR-008` presents the figure only where the **vehicle** validates it.

The mock enforces the same discipline. It has no method that changes one configuration as a
consequence of another, and only an explicit `reportSourceTransition()` populates the transition
item — so the unevidenced behaviour cannot enter the codebase through a test fixture either.

## Safety boundary

Power state reporting is not power-control authority. The ground station remains supervisory.

| Action | Permitted | Basis |
| --- | --- | --- |
| Display power state, source, configuration, faults | Yes | Baseline 49 requires it |
| Log all of it with freshness and source | Yes | `AERORA-COMPLY-REQ-004` |
| Raise operator-facing alerts | Yes | Supervisory role |
| Present a configuration the vehicle has not reported | **No** | Architectural principle 3 |
| Compute an energy estimate the vehicle has not validated | **No** | `IF-PWR-008` |
| Initiate a power transition | **No** | `AERORA-POWER-REQ-008`, `IF-SYS-007` |
| Act as safety controller for high-voltage isolation, converter protection, grounding, connector interlock or tether electrical fault | **No** | Baseline 13, explicitly |

There is **no outbound path of any kind** in this module — not even an empty command interface of
the sort the payload side declares for `IF-PAY-015`. The payload side needs one because an operator
may legitimately request a payload operation; nobody may request a power transition, so there is
nothing to declare. `kStationInitiatedPowerActionImplemented` is pinned at `false` with a
`static_assert` naming the requirement.

A reported fault is displayed and logged and reaches no command: no land, no return-to-launch, no
disarm, no mode change, no source change. 18-error-degradation-model.md records "none" in the
authority column of every aircraft-reported condition. There is also no fault clear, acknowledge or
reset method — clearing a fault the vehicle still holds would misreport the aircraft.
`clearReports()` is connection teardown only.

### The watchdog relationship that does not exist

`IF-PWR-011` is carried here because the watchdog is reported *alongside* power state, not because
it acts on power.

| Candidate relationship | Evidence | Position |
| --- | --- | --- |
| A watchdog fault causes a source transition | **`NOT-EVIDENCED`** | The ground station asserts nothing |
| A watchdog fault causes a converter state change | **`NOT-EVIDENCED`** | The ground station asserts nothing |
| A power fault causes a watchdog fault | **`NOT-EVIDENCED`** | The ground station asserts nothing |
| The two may be displayed side by side with their own receive times | `GCS-DECIDED` | Required |

So the watchdog event is a separate item with no field relating it to anything, and
`kWatchdogToPowerRelationshipEvidenced` is pinned at `false`. Folding it into a power-state
enumeration would also lose the ability to record what the power state was when the watchdog fired.

## Capability gating

`IF-PWR-012`: with no power capability reported, a consumer offers the generic QGroundControl
battery model only. That is correct behaviour for an aircraft that has said nothing about its power
configuration, not degraded behaviour. `isExtendedSurfacePermitted()` is that gate.

**It reads `AeroraVehicleCapabilities::Category::Vehicle`, and that is not a guess.**
17-capability-model.md defines exactly five categories — Vehicle, Payload, Mission, Telemetry,
Command — and there is no Power category. `IF-PWR-012`'s producer is the flight controller
reporting on connection, which the capability model's source table routes through `IF-FC-017`, the
vehicle capability inventory. The documented consequence of an absent vehicle capability — "the
flight interface degrades to the standard QGroundControl set" — is exactly `IF-PWR-012`'s failure
behaviour. Inventing a sixth category would change the capability hierarchy and fail that
document's own second-product test.

The gate is coarse — any vehicle capability reported available opens it — and deliberately so: no
capability identifier is evidenced, so there is no key this module could match on without inventing
one. It errs towards showing the standard battery model, which is the safe direction. Keys are
returned unfiltered, because rule 6 requires an unrecognised identifier be retained and recorded
rather than treated as absent.

## Unresolved thresholds and ranges

Nothing in this module defines any of these, and nothing should until the named authority supplies
them.

| Item | Status | Resolved by |
| --- | --- | --- |
| Minimum tether voltage, maximum current, allowable voltage drop | `NOT-EVIDENCED` | Power-system design |
| DC-DC efficiency, thermal limits | `NOT-EVIDENCED` | Power-system design |
| Transition threshold, hysteresis, timeout, failover timing | `OPEN` — Q-026 | Power-system design, then bench verification |
| Low / critical / empty battery thresholds | Not Aerora's to define | QGroundControl's existing battery handling and the FC protocol. No Aerora threshold layer exists or is wanted |
| Power state, fault, degraded-mode and converter-state enumerations | `OPEN` — Q-024 | The power architecture being frozen, then the dialect |
| High-voltage domain voltage and current ranges | `OPEN` — Q3-007 | Whether the 480 V domain is reported to the ground station at all is an unresolved conflict between baselines 49 and 13 |
| Per-class staleness thresholds | `OPEN` — Q-036 | Message rates, which are not evidenced anywhere |
| Emergency electrical behaviour | `OPEN` | Power-system design; baseline 13 excludes the ground station from this entirely |

### Items with no slot, and why

| Interface | Subject | Why absent |
| --- | --- | --- |
| `IF-PWR-006` | Voltage, current, power | Needs units and ranges from metadata, and whether the high-voltage domain is reported at all is Q3-007 |
| `IF-PWR-008` | Estimated remaining energy | Presented only where the vehicle validates it; the station computes none |
| `IF-PWR-009` | Degraded or limited-power mode | No evidenced reporting path; folded into `powerReportedState` if it ever arrives as a state token |
| `IF-PWR-010` | Converter state | No evidenced reporting path |

Adding slots for these without any evidenced content would be shape with no substance behind it.
`IF-PWR-005` (battery state of charge) is `CONFIRMED` and already handled by QGroundControl's
battery fact groups, so it correctly has no Aerora slot.

## Hardware and protocol dependencies

| Dependency | Needed for | Owner |
| --- | --- | --- |
| Aerora dialect, power message family | `AeroraPowerFactGroup`, typed values, units | Firmware |
| Power state and fault enumerations | Replacing opaque tokens with metadata enumerations | Firmware, product |
| Power capability identifiers | A precise gate instead of a coarse one | Firmware |
| Q3-007 resolution | Whether high-voltage telemetry reaches the ground station | Power-system and safety owner |
| Q-026 resolution | Whether transitions exist, and their behaviour | Power-system design, then bench verification |
| Q-036 message rates | Sizing staleness thresholds | Firmware |
| `AeroraVehicleServices::attachService("power", model)` | Per-vehicle attachment | `M-Vehicle` (Tab 3) |
| Q4-002 / R4-001 resolution | The fact group's multi-vehicle isolation | Reviewer / Tab 1 |

Three of twelve interfaces are implementable today, all of them standard battery handling. That
ratio is an honest measure of how much of the power domain depends on firmware deliverables.

## The staleness-verdict latch

HO-TAB6-01 is **fixed**. `AeroraReportedValue` now latches a staleness verdict, so losing the
freshness evaluator cannot return a known-`STALE` item to a current class. Full description of the
latch and its transition table is in
[the payload module's README](../Payload/README.md#the-staleness-verdict-latch); the two modules
share the primitive.

It mattered most here. `powerEffectiveState` is the only `KindEffective` item in the overlay, so it
is the value a consumer is most likely to read through `isCurrent()` before treating it as what the
aircraft is doing now — and it was the one that would silently have come back as `EFFECTIVE` after a
vehicle teardown. `_destroyedEvaluatorMustNotRefreshAStaleObservation` is now a permanent regression
guard with no expected-failure marker, and the primitive is covered directly by
`AeroraReportedValueTest`.

## External handoffs

| ID | Owner | Severity | Summary |
| --- | --- | --- | --- |
| **HO-TAB6-02** | `M-Vehicle` | Low, documentation | `attachDomainServices()` describes the capability inventory pointer as null until reported. It is never null; the inventory is empty. Behaviour correct, prose wrong |
| **HO-TAB6-03** | `M-Telemetry` | Low, documentation | `AeroraStateValue.h`'s `\section lifetime` appears to state the Qt child destruction order backwards. Implementation correct; the risk is a future reader removing the latch as dead code. Detail in [the payload README](../Payload/README.md#external-handoffs) |

HO-TAB6-01 is closed and no longer listed.

## Verification status

| Check | Status | Evidence |
| --- | --- | --- |
| Compiles | **`BLOCKED`** | No `cmake`, `ninja`, compiler or Qt in the environment; no MAVLink headers in the tree |
| `just lint` / `pre-commit` | **`BLOCKED`** | No `pre-commit`, `clang-format` or `clang-tidy` available |
| Unit tests executed | **`BLOCKED`** | Requires a build |
| Formatting and banned constructs | Checked statically | 120-column limit, LF endings, no tabs, no trailing whitespace, newline at EOF, no `Q_ASSERT`, no uncategorised `qDebug()`, no `QTest::ignoreMessage`, no fixed-delay `QTest::qWait(n)`, `#pragma once` present |
| Declaration / definition agreement | Checked statically | Every method defined in a `.cc` has a matching declaration, and every non-inline declaration has a definition |
| Upstream API signatures | Source-verified | `BatteryFactGroupListModel.{h,cc}`, `Fact.cc` metadata behaviour, `FactGroup.h`, `FirmwarePlugin.h:353`, `Vehicle.cc:365-370`, `ScheduledTask.h`, `ManualScheduler.h` |
| Overlay API signatures | Source-verified | `AeroraTelemetryState.h`, `AeroraStateValue.{h,cc}`, `AeroraFreshness.h`, `AeroraTelemetrySource.h`, `AeroraVehicleCapabilities.h` |
| Absence of a QGC tether power concept | Source-verified | Repository-wide search for `tether` |

**No claim is made that this code compiles, runs, or passes its tests.** The tests are written; they
have never been executed. They are deterministic by construction: every one injects
`ManualScheduler` and advances time explicitly.

One defect was found and fixed by inspection rather than by a run. `AeroraFreshness::evaluate()`
emits a `qCWarning` on the `aerora.telemetry` category the first time it evaluates a class with no
configured threshold — the expected state of every Aerora class while Q-036 is open. Under strict
log checking that fails any test which reads a state class on an *observed* item, which was three
tests here. Each now forces and consumes that warning explicitly with
`expectLogMessage` / `verifyExpectedLogMessage`. Tests that register a threshold before reporting
never reach the path at all. This is not a defect in `AeroraFreshness`: the warning is how an
unconfigured threshold stays visible instead of becoming a silent default.

## Files

| File | Purpose |
| --- | --- |
| `../Supervisory/AeroraReportedValue.{h,cc}` | The reported-item primitive, shared with `M-Payload`. Not a new module |
| `AeroraPowerModel.{h,cc}` | The seven items, the configuration key, capability gating, the six pinned refusals |
| `../PayloadPower.cmake` | Source manifest and the integration instructions for `aerora/CMakeLists.txt` |
| `../../test/Power/AeroraPowerMock.{h,cc}` | Deterministic driver, `MOCK / TEST ONLY`, with no source switching |
| `../../test/Power/AeroraPowerModelTest.{h,cc}` | Ten tests over separation, no-inference, effective/reported divergence, the pinned refusals, the shared-evaluator contract and the HO-TAB6-01 invariant |

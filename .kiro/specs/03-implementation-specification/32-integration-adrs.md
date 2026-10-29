# Integration ADRs

Owner: **Tab 1 (integration)**. Held separately from
[27-implementation-adrs.md](27-implementation-adrs.md), which is a contended file that Tab 2 also
edits. ADR-012 and ADR-013 live there; ADR-014 lives in
[30-outbound-command-security-audit.md](30-outbound-command-security-audit.md).

Verified against `HEAD = 9459a4b57986f40df8bdc193d5453c0596fbb416`.

| ADR | Subject | Status | Core edit |
| --- | --- | --- | --- |
| ADR-015 | Calibration authorization seam | **ACCEPTED, IMPLEMENTED-UNCOMPILED** | Yes, 1 file |
| ADR-016 | Q4-002 telemetry FactGroup membership | **ACCEPTED, IMPLEMENTED-UNCOMPILED** — both halves applied; authorisation unresolved, owned by Tab 1 | Yes, 1 file — the friend declaration in `src/Vehicle/Vehicle.h` |

---

## ADR-015: Calibration authorization seam

### Context

ADR-012 placed command authorization at `MavCommandQueue::sendWorker()`. Tab 3's investigation
established that `Vehicle::startCalibration()` is the **only** production `MAV_CMD` sender outside
`MavCommandQueue` after searching `src/**`, and that it can actuate: ESC calibration spins motors,
and APM CompassMot spins props.

### Problem

The queued gate cannot see this path, so the most physically dangerous `MAV_CMD` the station can
send was the one command not subject to authorization. Within one feature the behaviour was
asymmetric: `startCalibration()` ungated, `stopCalibration()` gated.

### Evidence

| Fact | Location |
| --- | --- |
| Entry point | `src/Vehicle/Vehicle.cc:2328` |
| Primary link locked first | `:2331-2335` |
| Calibration type resolved entirely into `param1`-`param7` by a switch | `:2336-2390` |
| ESC calibration sets `param7 = 1` | `:2363` |
| APM CompassMot sets `param6 = 1` | `:2372` |
| Upstream rationale for bypassing the queue | `:2392-2393` — result timing is unbounded, which breaks retry logic |
| Direct pack | `:2395-2404`, `mavlink_msg_command_long_pack_chan` |
| Direct send | `:2405`, `sendMessageOnLinkThreadSafe` |
| Gated sibling | `stopCalibration()` at `:2408`, uses `sendMavCommand` |

The bypass is **deliberate upstream design**, not a defect. Routing it through `sendMavCommand()`
would reintroduce the ack-timeout and duplicate-suppression behaviour upstream deliberately avoided.

### Decision

Add an authorization decision inside `startCalibration()`, **after** the parameter switch and
**before** the pack. Tab 3's Option A, with the placement made precise.

Placement is load-bearing in both directions:

- **After the switch**, because the calibration type exists only as `param1`-`param7`. A decision
  taken before them could not distinguish an ESC calibration from a gyro calibration, which is the
  single most important distinction on this path.
- **Before the pack**, so a refusal constructs no message and transmits nothing.

The existing `QGCCorePlugin::authorizeMavCommand()` hook is reused unchanged. No new seam, no new
signature, no second mechanism.

### Exact core edit

| Field | Value |
| --- | --- |
| File | `src/Vehicle/Vehicle.cc` |
| Function | `Vehicle::startCalibration(QGCMAVLink::CalibrationType)` |
| Change | One guarded block: consult `authorizeMavCommand()`, and on refusal log, notify, and return |
| New includes | **None.** `QGCCorePlugin.h` and `AppMessages.h` are already included |
| Hook arguments | `this`, `defaultComponentId()`, `MAV_CMD_PREFLIGHT_CALIBRATION`, `commandInt = false`, `param1`-`param7` |
| MAVLink identifiers added | **None.** `MAV_CMD_PREFLIGHT_CALIBRATION` is the value this path already sent |

### Effects

| Effect | Assessment |
| --- | --- |
| **Security** | Closes the only `MAV_CMD` path outside the queue. The calibration asymmetry is removed: start and stop are now both subject to the same decision |
| **Compatibility** | None in stock QGC. The base hook returns `true`, so behaviour is byte-identical. The edit is additive and upstream-contributable |
| **Retry** | **None.** This path never used the queue, so no retry, duplicate-suppression or ack-timeout behaviour is involved. The upstream reason for the direct send is preserved exactly |
| **Operator-visible** | On refusal: a `showAppMessage` notice and a warning log. Chosen because `startCalibration()` returns `void` and has no result handler or `commandResult` signal — a silent return would leave the calibration UI waiting with no explanation. The wording reuses the "not authorized" vocabulary the queued path already established; no new UI concept is introduced |
| **Scope** | Still `MAV_CMD` authorization. ADR-014's broader outbound question is unaffected and remains `OPEN` |

### Alternatives rejected

| Alternative | Why rejected |
| --- | --- |
| Route through `sendMavCommand()` | Reintroduces timeout and duplicate semantics upstream deliberately avoided. Tab 3 advised against it and the source comment explains why |
| Gate at `sendMessageOnLinkThreadSafe()` | A message-level boundary. That is ADR-014's decision, not this one, and taking it here would be scope expansion |
| Add a calibration-specific authorization hook | A second mechanism for one call site. The existing hook already carries everything needed |
| Place the check before the switch | Cheaper, but the decision would not see the parameters and so could not tell a motor-spinning calibration from a gyro calibration |

### Consequences

`AERORA-SEC-REQ-012` coverage extends from "queued `MAV_CMD` traffic" to "queued `MAV_CMD` traffic
plus the calibration direct-send path". That is still **not** general outbound authorization, and no
status anywhere should be upgraded beyond this wording.

While `Q-029` is open the hook allows and warns, so this edit changes no runtime behaviour today. It
installs the decision point; the policy that makes it meaningful is Tab 2's and is still `OPEN`.

### Status

**ACCEPTED, `IMPLEMENTED-UNCOMPILED`.** Not compiled, not executed — `D-006`.

### Verification requirements

| # | Requirement | Where it can run | Status |
| --- | --- | --- | --- |
| 1 | Default hook allows calibration, including ESC parameters, so stock behaviour is unchanged | Stock build | `IMPLEMENTED-UNCOMPILED` |
| 2 | A denied calibration transmits nothing | Overlay build with a denying plugin | `IMPLEMENTED-UNCOMPILED` — `Q-035` to run |
| 3 | A denied calibration leaves no queue entry and produces no retry | Overlay build | `IMPLEMENTED-UNCOMPILED` — `Q-035` to run |
| 4 | Refusal is operator-visible rather than a silent no-op | Overlay build | `IMPLEMENTATION-PLANNED` — not covered by the four enforcement functions |

Tab 1 authored no test for this. Requirements 1–4 belong in the authorization test harness that
Tab 2 owns, and requirements 2–4 additionally need the overlay test leg (`Q-035`). Adding a
parallel harness in Tab 1's name would duplicate Tab 2's fixtures and edit files Tab 2 owns.
**Assigned to Tab 2.**

**Update — Tab 2 has delivered the policy half.**
`aerora/test/Security/AeroraCalibrationAuthorizationTest` now carries 11 test functions covering
calibration **classification** (start versus cancel, non-calibration commands, membership of the
declared gated set) and the **authorization chain** applied to a calibration operation (allowed,
denied with reason, unconfigured policy denies, and audit identifying which calibration was
refused). Status `IMPLEMENTED-UNCOMPILED`; registered by Tab 1 as `AeroraCalibrationAuthorizationTest`.

**Update — Tab 2 has now delivered the core-seam half as well.**
`aerora/test/Security/AeroraCalibrationEnforcementTest`, labelled `Integration Vehicle`, carries
four functions that map directly onto the requirements above:

| Test function | Covers |
| --- | --- |
| `_testDeniedCalibrationStartReachesNoWire()` | Requirement 2 — a refusal transmits nothing |
| `_testAuthorizedCalibrationStartReachesWire()` | Requirement 1 — an allowed calibration is unaffected |
| `_testCancelStillReachesWireWhenStartIsDenied()` | The start/cancel asymmetry this ADR raised |
| `_testUnconfiguredChainAllowsCalibration()` | Current `Q-029`-open behaviour: allow and warn |

All four are `IMPLEMENTED-UNCOMPILED` and registered by Tab 1. The third is worth calling out: it
pins the asymmetry in the **safe** direction — if starting a calibration is refused, cancelling one
must still work. A gate that could deny the stop while permitting the start would be worse than no
gate, and that is now asserted rather than assumed.

Requirements 1–4 therefore move from `IMPLEMENTATION-PLANNED` to `IMPLEMENTED-UNCOMPILED`. **None
has been executed**: the enforcement test needs a live `Vehicle`, a primary link and a denying
plugin, so it still requires the overlay test leg (`Q-035`) and a build host (`D-006`).

### Rollback

Delete the guarded block in `Vehicle::startCalibration()`. No other file, no persisted state, no
settings schema, no data migration. The function returns to its prior behaviour exactly.

---

## ADR-016: Q4-002 — Aerora telemetry FactGroup membership

### Context

Per-vehicle telemetry and freshness ownership is **closed**: Tab 3 composes one `AeroraTelemetrySet`
per `Vehicle` from `AeroraFirmwarePlugin::initializeVehicle()`, without calling
`FirmwarePlugin::factGroups()`. What remains open is whether Aerora fact groups should also become
members of the QGC `Vehicle` FactGroup map.

### Problem

Membership is what grants native behaviour: automatic `handleMessage` dispatch, value-picker
integration and CSV Fact logging. Non-membership keeps isolation but forfeits all three.

### Evidence

| Fact | Location | Consequence |
| --- | --- | --- |
| `_addFactGroup()` is **protected** | `src/FactSystem/FactGroup.h`, `protected:` section | An external Aerora object **cannot** register a group into a `Vehicle`. Only `Vehicle`, a subclass, or a declared friend can |
| **Upstream already grants `friend` access for exactly this purpose, twice** | `src/Vehicle/Vehicle.h:108` `friend class FactGroupListModel; // Allow call _addFactGroup`, and `:115` `friend class GimbalController; // Allow GimbalController to call _addFactGroup` | Evidence contributed by Tab 4. There is established upstream precedent for the pattern, so the Option B edit is **one friend declaration plus a concrete registrar**, not a new mechanism |
| `factGroups()` is the only extension path, consumed once in the constructor | `src/Vehicle/Vehicle.cc:363-370` | Iterates the plugin's map and calls `_addFactGroup` |
| The plugin is a cached singleton | `FirmwarePluginManager.cc:75-89`, obtained per vehicle at `Vehicle.cc:229` | Every vehicle of a firmware class receives **the same FactGroup pointers** |
| The only existing override returns a plugin member | `ArduSubFirmwarePlugin.cc:281-284`, `&_nameToFactGroupMap` | Confirms the hazard is real in upstream today, not hypothetical — this is `R4-001` |
| Public `factGroups()` on `FactGroup` is read-only | `FactGroup.h` | An accessor, not a registration path |
| A `Fact` without metadata is inert | `src/FactSystem/Fact.cc:135-161` — `setRawValue` warns and no-ops | A registered group with no metadata would carry no usable value |
| No Aerora dialect exists | `Q-023`, `Q-024`; all six Aerora groups `CONTRACT-ONLY` | There is currently **nothing to dispatch and no fact to display or log** |

### Decision

**Option B is adopted as the target architecture, and its implementation is deferred.**

Concretely:

1. The architectural direction is settled: Aerora fact groups **will** become per-vehicle members of
   the QGC FactGroup map, via a core seam that constructs them **per vehicle** rather than handing
   out shared pointers from a plugin singleton.
2. Implementation is **gated** on two preconditions, both currently unmet: an Aerora dialect with
   defined facts and metadata (`Q-023`, `Q-024`), and a build host (`D-006`).
3. Until both are met, telemetry stays service-only — which is exactly today's state, so nothing
   regresses and nothing is thrown away.

### Why B rather than A or C

| Criterion | Option A — service only | **Option B — per-vehicle core registration** | Option C — Aerora-specific dispatch |
| --- | --- | --- | --- |
| Per-vehicle isolation | Correct | **Correct, and fixes `R4-001` for upstream too** | Correct |
| Correctness | Loses native semantics permanently | **Native semantics, as QGC defines them** | Partial — dispatch only, no picker or CSV |
| QGC compatibility | Best, zero edits | One additive core seam, **repairing an existing upstream defect** | One core edit **plus** a parallel mechanism duplicating one QGC already has |
| Maintenance burden | Lowest | Low — one virtual, one call site | **Highest** — a second dispatch path to keep aligned with QGC's forever |
| Multi-vehicle behaviour | Correct | **Correct** | Correct |
| Future UI requirements | Value picker and CSV unavailable, and would need re-litigating later | **Both arrive free** | Still unavailable |
| Testability | Good | Good | Needs its own harness |

Option A was rejected because it is not a decision so much as a permanent forfeit: the moment a
fact needs to appear in the value picker or a CSV log, the question reopens with code already built
around the wrong answer. Option C was rejected because it costs a core edit *and* a parallel
mechanism, which is strictly worse than Option B's single additive seam.

Option B is also the only option that improves upstream: `ArduSubFirmwarePlugin` shares FactGroup
objects across vehicles today, so a per-vehicle seam is a genuine upstream contribution rather than
an Aerora-only accommodation.

### Alternatives considered for the eventual seam, not selected

The specific signature is deliberately left to the implementing workstream. Candidates recorded so
the decision is not re-derived later:

| Candidate | Note |
| --- | --- |
| `virtual void FirmwarePlugin::addFactGroupsToVehicle(Vehicle*)`, called from the `Vehicle` constructor | Lets the plugin construct groups parented to that vehicle. Would let `ArduSub` migrate off the shared map |
| Deprecate `factGroups()` in favour of the above | Larger upstream change; would fix `R4-001` outright |
| A `friend` declaration for a concrete Aerora registrar | **No longer rejected.** The earlier objection — that a production path through a friend declaration repeats the pattern rejected for signing keys — does not hold here, and the distinction matters. The signing-key objection was to routing production code through a **test** seam (`friend class SigningTest`). `Vehicle.h:108` and `:115` show upstream using `friend` as its ordinary, intended mechanism for granting `_addFactGroup` access to a collaborator. Following an established upstream pattern is not the same as abusing a test hook |

`Vehicle::adjustIncomingMavlinkMessage()` was evaluated as a dispatch seam and is **not** adopted. It
would deliver Option C's partial result — dispatch without picker or CSV — at the cost of a core
edit and a parallel mechanism.

### Consequences

- No core edit is made now. `Q4-002` moves from "undecided" to "decided, implementation gated".
- `R4-001` stays open as an upstream defect, now with a recorded intended fix.
- Tab 4 should keep all six fact groups `CONTRACT-ONLY` and should **not** build around Option A's
  constraints, because Option A is rejected.
- Tab 3 should not call `FirmwarePlugin::factGroups()`. Its decision to avoid it is confirmed correct.

### Status

**ACCEPTED. Both halves of the core edit are applied in the tree. The authorisation of the applied
pair is unresolved and is Tab 1's to record.**

Tab 4 built both halves of the telemetry side: `AeroraFactGroupRegistrar` as the interface and
`AeroraVehicleFactGroupRegistrar` as the one concrete registrar, whose single privileged call is
compiled only when `AERORA_VEHICLE_FACTGROUP_FRIENDSHIP` is defined. Granting the friendship is the
only part Tab 4 cannot do for itself, so it was handed to Tab 1.

Both halves are present at `e7b38eef1`:

| Half | State | Detail |
| --- | --- | --- |
| `friend class AeroraVehicleFactGroupRegistrar;` on `Vehicle` | **APPLIED** | `src/Vehicle/Vehicle.h:123`, alongside the two friends upstream grants for the same purpose |
| `AERORA_VEHICLE_FACTGROUP_FRIENDSHIP` compile definition | **DEFINED** | `aerora/CMakeLists.txt` `CUSTOM_DEFINITIONS`; both `aerora-linux.yml` and `aerora-windows.yml` fail the job if the define is absent from the generated build |

So the privileged call is compiled and `isRegistrationAvailable()` returns `true` in an Aerora build
(`aerora/src/Telemetry/AeroraVehicleFactGroupRegistrar.cc:22-28`). Whether the applied pair carries
the authorisation this ADR hands to Tab 1 is **unresolved** — that answer is Tab 1's, and this record
states what the tree holds rather than standing in for it.

The asymmetry is Tab 4's design, not an accident, and it is why applying the declaration first was
safe: a friend declaration without the flag leaves the registrar **inert** — it validates, refuses,
and reports why — whereas the flag without the friend declaration is a compile error. `D-006` means
no build has verified either half here. In a stock build the declaration names a class that is never
defined, which is legal and has no effect.

The original precondition is unchanged by the define: until Aerora facts and metadata exist
(`Q-023`, `Q-024`) there is no concrete fact group to register, so registration adds nothing.
`isRegistrationAvailable()` exists so a startup self-check can detect the inert case rather than
discovering it as absent telemetry in flight.

Remaining work: the facts to justify registration, plus Tab 1's record of the authorisation the
applied pair still needs.

### Verification requirements

Deferred with the implementation. When it is built:

| # | Requirement |
| --- | --- |
| 1 | Two vehicles of the Aerora firmware class hold **distinct** FactGroup instances — the direct `R4-001` regression test |
| 2 | A fact value set on one vehicle is not observable on another |
| 3 | A registered Aerora group appears in the value picker |
| 4 | A registered Aerora fact is written to the CSV log |
| 5 | Destroying a vehicle destroys its groups, with no leak and no dangling pointer in the map |

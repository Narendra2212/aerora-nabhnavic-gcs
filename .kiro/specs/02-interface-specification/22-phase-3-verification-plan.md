# Phase 3 Verification Plan

A verification method for every one of the 240 interfaces, what each method can and cannot prove,
and what must exist before it can be run.

## Nothing has been run

| Fact | Value |
| --- | --- |
| Interfaces with an assigned method | **240 of 240** |
| Verification results recorded in this document | **0** |
| Tests written | **0** |
| Builds performed | **0** |

The development host cannot build QGroundControl. No CMake, Ninja, compiler, Qt installation or
`just` runner is present, and there is no build directory. This was established in Phase 1, is
recorded as R-009, and is unchanged. **Every method below is an assignment, not a result.**

Reporting this plainly is the point. A verification plan that implied its own execution would be the
first thing in the set that could not be checked.

## Methods

Carried from [00-phase-3-readme.md](00-phase-3-readme.md), with the environment each requires.

| Code | Environment | Count |
| --- | --- | --- |
| `INTEGRATION` | Mock link, in the existing test harness | 145 |
| `UNIT` | No vehicle, no link | 36 |
| `BENCH` | Real flight-controller or radio hardware on a bench | 26 |
| `STATIC` | Inspection of code, configuration or document | 18 |
| `DEMO` | Observed operation by a person | 6 |
| `AUDIT` | A process check at a defined point | 6 |
| `HIL` | Hardware in the loop, with the companion present | 4 |
| Total assignments | — | 241 |

241 assignments for 240 interfaces: `IF-GPU-008` carries both `STATIC` and `UNIT`, because the
prohibition is both a code-structure property and a runtime property.

### Framework

Aerora adds no test framework. `AERORA-TEST-REQ-009` requires using the upstream framework, base
classes and labels unmodified, and Phase 1 assessed that framework as being in good shape.

| Element | Use |
| --- | --- |
| Self-registering test macro | No central list to update |
| Eleven base classes, from the plain unit base up through vehicle, parameter and mission bases | Aerora tests derive from the nearest fitting base |
| Twelve test labels; CTest labels a superset | Aerora tests carry existing labels, not new ones |
| `MockLink` vehicle simulation, with CTest resource locking | The `INTEGRATION` environment for 145 interfaces |
| Wait helpers and timeout wrappers | No fixed-delay waits |
| Out-of-process Qt Quick test executable | The `IF-UIS` presentation contracts |
| JUnit XML, coverage, four sanitizers | Reporting, unchanged |

Two placement properties are unverified and gate the plan rather than being part of it: whether
Aerora tests in the overlay are reachable by the runner at all (R-021, Q-035), and whether the
`MockLink` resource lock is sufficient when Aerora integration tests are added alongside the
existing ones. Both are `G-IMPL` items.

---

## `INTEGRATION` — 145 interfaces

The dominant method, because the mock link can exercise the ground station's side of almost every
contract without an aircraft.

**What it proves.** That the ground station parses, represents, displays, records and gates
correctly given a defined input sequence. Freshness stamping, staleness marking, command state
transitions, record production, degradation on absence.

**What it cannot prove.** That the vehicle produces the input sequence. For a standard MAVLink
family that is a safe assumption. For an Aerora family it is not, which is why no class C or class D
`IF-MAV` interface is assigned `INTEGRATION`.

**The distinction that matters.** A mock the Aerora team writes, exercising a dialect the Aerora
team writes, verifies that the two agree with each other. That is worth having and it is not
evidence about the aircraft.

**Prerequisites.** A building overlay; Aerora test reachability resolved (Q-035); the Aerora dialect
present for any interface carrying an Aerora family.

**Named tests this method includes.** The all-network-blocked test of `AERORA-TEST-REQ-005`, which
covers `IF-CLD-004` and `IF-LOG-018` and is the only test that can catch an accidental re-enablement
of log auto-upload by observing the blocked connection attempt.

---

## `UNIT` — 36 interfaces

**What it proves.** Properties with no vehicle in them: queue behaviour and drop accounting,
timestamp monotonicity across a clock adjustment, metadata bounds checking, record correlation,
authorisation gating, and the architectural properties.

**What it cannot prove.** Anything about a message arriving.

**The architectural-property tests.** `AERORA-TEST-REQ-004` names four: that the autonomy module
emits no MAVLink, that the Aerora firmware plugin is selected, that no fact group is created after
vehicle construction, and that no listening socket is open. These cover `IF-GPU-008`, `IF-SYS-005`,
`IF-SYS-008`, `IF-CLD-003` and `IF-TLM-008`.

They carry the `Unit` label deliberately, so they run in the default gate rather than an optional
job — `AERORA-TEST-REQ-006`. A property test in an optional job stops running the first time that
job is slow.

### Two extensions this plan adds to the property-test set

Both come from Phase 3 risks and both reuse the existing mechanism.

| Extension | Covers | Risk |
| --- | --- | --- |
| Assert all four module dependency rules, not only the cloud-to-security rule | `IF-SYS-001` to `IF-SYS-008` | [R3-001](21-phase-3-risks.md) |
| Convert the inspection-only prohibitions to assertions where the mechanism allows | `IF-AUT-012`, `IF-PAY-015` first | [R3-007](21-phase-3-risks.md) |

Of the sixteen prohibition interfaces, four are already automated and nine rest on inspection. The
two to convert first are `IF-AUT-012` — no ground-station-initiated failsafe action — and
`IF-PAY-015` — no ground-station path to payload actuators. Both are writable as reachability
assertions, and both are the ones where a violation would matter most.

**Prerequisites.** A building overlay; test reachability resolved.

---

## `BENCH` — 26 interfaces

**What it proves.** That the vehicle produces what the interface says it produces. This is the only
method that converts a `NOT-EVIDENCED` or `BASELINE-ASSERTED` row into evidence.

**What it cannot prove.** Timing and behaviour under flight conditions, and anything involving the
companion in the loop.

**Where it is concentrated.** Every class C and class D `IF-MAV` interface, the Aerora-family
`IF-FC` rows, the capability inventory, the vehicle identity report, payload calibration, payload
state retention, and the two radio interfaces behind Q3-001.

That concentration is the intended use of the evidence column. The 26 `BENCH` assignments sit almost
exactly on top of the 37 `NOT-EVIDENCED` rows, which is what makes this plan a prioritisation rather
than a list.

### The four bench observations worth doing first

Ordered by cost-to-value, not by document order.

| Observation | Settles | Cost |
| --- | --- | --- |
| Attach the radio to a host; record device class, vendor and product identifiers | Q3-001, ADR-002's central claim, `IF-RAD-001`, `IF-RAD-003` | **One connection** |
| Read the heartbeat's autopilot identifier | Q-018, `IF-MAV-021`, firmware-plugin selection | One connection |
| Read the capability inventory's field definition | Q3-009, `IF-MAV-026`, `IF-FC-017`, and whether the capability model can be populated | One message |
| Set payload state, interrupt a mission, resume, and read the payload state | Evidence matrix item 14, R-002, `IF-PAY-014` | A bench rig with a payload |

The first is the cheapest high-value item in the programme. One observation either confirms ADR-002
or returns six deferred transport core edits, and it needs no firmware, no dialect and no payload.

**Prerequisites.** Flight-controller hardware with Aerora firmware; the Aerora dialect; for the
fourth, a payload controller.

---

## `HIL` — 4 interfaces

| Interface | Subject |
| --- | --- |
| `IF-FC-010` | Autonomy state and proposals |
| `IF-FC-011` | Companion health and presence |
| `IF-FC-020` | Authority mode change request |
| `IF-MAV-023` | Reporting proposal acceptance or rejection explicitly |

**What it proves.** The proposal-to-outcome path end to end, with a real companion producing
proposals and a real flight controller accepting or rejecting them. This is the only environment in
which the prohibition on inferring acceptance can be tested, because it needs a case where a
proposal is made and **not** followed.

**What it cannot prove.** Flight behaviour.

**Why only four.** The companion's autonomy wiring is marked as future work in the firmware model,
so there is little to put in the loop yet. These four are the interfaces that cannot be verified any
other way, not the full autonomy surface.

**Prerequisites.** Companion hardware with autonomy software; the flight-controller to companion
interface operating; Q3-005 answered, because the correct test differs depending on whether
proposals reach the ground station directly or summarised.

---

## `STATIC` — 18 interfaces

**What it proves.** That a code path does not exist, that a declaration matches its documentation,
or that a configuration value is what the specification says.

**What it cannot prove.** That it will still be true next month. This is the method's defining
weakness and the substance of [R3-007](21-phase-3-risks.md).

**Where it is unavoidable.** `IF-SEC-005` (the gated-operation set is declared, not derived),
`IF-UIS-010` (the tier-1 seam return values), `IF-CLD-009` (a notification is informational, never
flight-directive), `IF-CLD-013` (no global offline mode), `IF-MIS-015` and `IF-MIS-016` (rejected
approaches — there is nothing to execute).

A rejected approach is the honest case for `STATIC`: the verification is that a thing is absent from
the tree, and no test can observe the absence of something nobody wrote.

**Prerequisites.** None beyond the source.

---

## `DEMO` — 6 interfaces

| Interface | Subject | Why no test can assert it |
| --- | --- | --- |
| `IF-CLD-001` | Cloud connectivity state | Whether two indicators are **distinguishable** is not a property code can check |
| `IF-CLD-011` | Map and terrain tile cache | A gap shown as a gap, rather than drawn blank, is a visual judgement |
| `IF-LOG-024` | Record export | End-to-end operator action with a file destination |
| `IF-UIS-006` | Action availability | Whether hidden-versus-disabled reads correctly to an operator |
| `IF-UIS-009` | Independent-indicator separation | Six named pairs, each distinguishable without reading a label |
| `IF-UIS-015` | Startup self-check | That the failure is **visible**, not merely logged |

**What it proves.** That a person operating the station sees what the specification says they
should.

**What it cannot prove.** Anything repeatably. A `DEMO` result is a dated observation by a named
person against a named build, and it expires.

**Prerequisites.** A running build; for `IF-UIS-009`, the visual design language (Q-040), because
"share no visual language" cannot be assessed before a visual language exists.

---

## `AUDIT` — 6 interfaces

Five of the six are security or supply-chain process checks; the sixth is the tier-3 override
discipline.

| Interface | Check | When |
| --- | --- | --- |
| `IF-SEC-012` | Signing keys were provisioned as random raw keys, not passphrase-derived | At provisioning, and at release |
| `IF-SEC-013` | Key storage at rest — **currently unmet**; the check is that the gap is recorded and gated | At release |
| `IF-SEC-018` | Key rotation followed the fleet procedure | At each rotation |
| `IF-SEC-019` | A settings-schema version change has a recorded reprovisioning plan | At each schema change |
| `IF-SEC-021` | Dialect and every third-party dependency pinned to an immutable reference | At release |
| `IF-UIS-012` | Each tier-3 override has a reason, a fork-point commit, a named owner and a completed drift review | At each upstream merge |

**What it proves.** That a procedure was followed.

**What it cannot prove.** That it will be followed next time. `AERORA-TEST-REQ-008` exists precisely
because of this: overridden-file drift should be checked mechanically against recorded fork-point
commits rather than by procedure alone, which would convert `IF-UIS-012` from `AUDIT` to `UNIT`.
That conversion is the single highest-value change available to this plan, because it removes the
one `AUDIT` item whose failure is silent.

`IF-SEC-013` is the unusual row. The verification is not that key storage is protected — it is not —
but that the gap is recorded, gated at `G-RELEASE`, and has not been quietly reclassified. ADR-008
is the record.

**Prerequisites.** A release process with defined checkpoints; recorded fork-point commits.

---

## Gates

Which methods must have passed at each gate.

| Gate | Required | Not required |
| --- | --- | --- |
| `G-ARCH` | Nothing. Architecture is ratified by review | — |
| `G-IMPL` | `STATIC` for the prohibitions; test reachability resolved | `BENCH`, `HIL` |
| `G-INTEG` | `UNIT`, `INTEGRATION`, the first three `BENCH` observations | `HIL`, `DEMO` |
| `G-RELEASE` | All `AUDIT`, all `DEMO`, all `BENCH`, `HIL` where the companion exists | — |

`G-INTEG` not requiring `HIL` is deliberate: the companion's autonomy wiring is future work, so
requiring it would gate integration on something outside the programme's current scope. The four
`HIL` interfaces remain `OPEN` until it exists, and the autonomy presentation degrades to absent.

## What this plan does not do

| Does not | Reason |
| --- | --- |
| Record any result | Nothing has been run |
| Specify test names, file paths or fixtures | Implementation detail; the method and environment are the contract |
| Assign coverage targets | Coverage of a specification is not coverage of a risk |
| Verify requirements | This plan verifies **interfaces**. Requirement verification methods are in [requirements.md](../01-aerora-architecture/requirements.md) |
| Claim the architectural-property tests exist | They are specified by `AERORA-TEST-REQ-004` and unwritten |

The fourth row is a real distinction. A requirement's verification method answers "has the
obligation been met?"; an interface's answers "does the boundary behave as specified?". They overlap
and they are not the same question, and conflating them would let a passed requirement stand in for
an unverified interface.

## Method index

Every interface, its method, its current status, and its defining document number. Mechanically
extracted.

| ID | Interface | Method | Status | Doc |
| --- | --- | --- | --- | --- |
| `IF-AUT-001` | Effective authority mode | `INTEGRATION` | `PROPOSED` | 09 |
| `IF-AUT-002` | Authority mode change request | `INTEGRATION` | `OPEN` | 09 |
| `IF-AUT-003` | Authority source | `INTEGRATION` | `PROPOSED` | 09 |
| `IF-AUT-004` | Confirmation pending | `INTEGRATION` | `PROPOSED` | 09 |
| `IF-AUT-005` | Operator confirmation | `INTEGRATION` | `OPEN` | 09 |
| `IF-AUT-006` | Command submission | `UNIT` | `CONFIRMED` | 09 |
| `IF-AUT-007` | Command acknowledgement | `INTEGRATION` | `CONFIRMED` | 09 |
| `IF-AUT-008` | Command state | `UNIT` | `PROPOSED` | 09 |
| `IF-AUT-009` | Effectiveness corroboration | `UNIT` | `PROPOSED` | 09 |
| `IF-AUT-010` | Failsafe state | `INTEGRATION` | `OPEN` | 09 |
| `IF-AUT-011` | Operator authorisation decision | `UNIT` | `PROPOSED` | 09 |
| `IF-AUT-012` | GCS-initiated failsafe action | `STATIC` | `CONFIRMED` | 09 |
| `IF-CLD-001` | Cloud connectivity state | `DEMO` | `CONFIRMED` | 15 |
| `IF-CLD-002` | Cloud threading boundary | `INTEGRATION` | `PROPOSED` | 15 |
| `IF-CLD-003` | Inbound network connection | `UNIT` | `PROPOSED` | 15 |
| `IF-CLD-004` | Cloud participation in the command path | `INTEGRATION` | `CONFIRMED` | 15 |
| `IF-CLD-005` | Mission library synchronisation | `INTEGRATION` | `PROPOSED` | 15 |
| `IF-CLD-006` | Fleet view data | `INTEGRATION` | `PROPOSED` | 15 |
| `IF-CLD-007` | Configuration distribution | `INTEGRATION` | `PROPOSED` | 15 |
| `IF-CLD-008` | Release and compatibility metadata | `INTEGRATION` | `PROPOSED` | 15 |
| `IF-CLD-009` | Operator notification | `STATIC` | `PROPOSED` | 15 |
| `IF-CLD-010` | Reporting and dashboards | `STATIC` | `CONFIRMED` | 15 |
| `IF-CLD-011` | Map and terrain tile cache | `DEMO` | `PROPOSED` | 15 |
| `IF-CLD-012` | Cloud-sourced data age | `UNIT` | `CONFIRMED` | 15 |
| `IF-CLD-013` | Per-feature availability state | `STATIC` | `PROPOSED` | 15 |
| `IF-CLD-014` | Upload queue state | `UNIT` | `PROPOSED` | 15 |
| `IF-CLD-015` | Synchronisation conflict resolution | `INTEGRATION` | `OPEN` | 15 |
| `IF-CLD-016` | Backend module interface boundary | `STATIC` | `CONFIRMED` | 15 |
| `IF-FC-001` | Vehicle state: attitude, position, velocity, altitude | `INTEGRATION` | `CONFIRMED` | 02 |
| `IF-FC-002` | Flight mode | `INTEGRATION` | `OPEN` | 02 |
| `IF-FC-003` | Authority mode, effective | `INTEGRATION` | `PROPOSED` | 02 |
| `IF-FC-004` | Arming state | `INTEGRATION` | `CONFIRMED` | 02 |
| `IF-FC-005` | System health and sensor status | `INTEGRATION` | `CONFIRMED` | 02 |
| `IF-FC-006` | Battery and power telemetry | `BENCH` | `PROPOSED` | 02 |
| `IF-FC-007` | Link state, vehicle-reported | `BENCH` | `PROPOSED` | 02 |
| `IF-FC-008` | GPS state | `INTEGRATION` | `CONFIRMED` | 02 |
| `IF-FC-009` | Estimator state | `INTEGRATION` | `CONFIRMED` | 02 |
| `IF-FC-010` | Autonomy state and proposals | `HIL` | `OPEN` | 02 |
| `IF-FC-011` | Companion health and presence | `HIL` | `PROPOSED` | 02 |
| `IF-FC-012` | Payload state, health, safety state | `BENCH` | `OPEN` | 02 |
| `IF-FC-013` | Command acknowledgement | `INTEGRATION` | `CONFIRMED` | 02 |
| `IF-FC-014` | Mission state and current index | `INTEGRATION` | `CONFIRMED` | 02 |
| `IF-FC-015` | Parameters | `INTEGRATION` | `CONFIRMED` | 02 |
| `IF-FC-016` | Diagnostics and status text | `INTEGRATION` | `CONFIRMED` | 02 |
| `IF-FC-017` | Capability inventory | `BENCH` | `OPEN` | 02 |
| `IF-FC-018` | Aerora vehicle identity | `BENCH` | `OPEN` | 02 |
| `IF-FC-019` | Operator flight commands | `INTEGRATION` | `CONFIRMED` | 02 |
| `IF-FC-020` | Authority mode change request | `HIL` | `PROPOSED` | 02 |
| `IF-FC-021` | Payload commands | `BENCH` | `OPEN` | 02 |
| `IF-FC-022` | Mission transfer | `INTEGRATION` | `CONFIRMED` | 02 |
| `IF-FC-023` | Parameter write | `INTEGRATION` | `CONFIRMED` | 02 |
| `IF-FC-024` | Ground station heartbeat | `UNIT` | `CONFIRMED` | 02 |
| `IF-GPU-001` | Companion addressing | `INTEGRATION` | `OPEN` | 05 |
| `IF-GPU-002` | Proposal | `INTEGRATION` | `OPEN` | 05 |
| `IF-GPU-003` | Proposal outcome | `INTEGRATION` | `OPEN` | 05 |
| `IF-GPU-004` | Companion health | `INTEGRATION` | `OPEN` | 05 |
| `IF-GPU-005` | Companion presence | `INTEGRATION` | `OPEN` | 05 |
| `IF-GPU-006` | Effective state for corroboration | `INTEGRATION` | `PROPOSED` | 05 |
| `IF-GPU-007` | Autonomy capability | `INTEGRATION` | `OPEN` | 05 |
| `IF-GPU-008` | Ground station to companion | `STATIC`, `UNIT` | `CONFIRMED` | 05 |
| `IF-LOG-001` | Application diagnostic log | `STATIC` | `CONFIRMED` | 14 |
| `IF-LOG-002` | MAVLink stream log | `INTEGRATION` | `CONFIRMED` | 14 |
| `IF-LOG-003` | Audit record store | `INTEGRATION` | `PROPOSED` | 14 |
| `IF-LOG-004` | Operator action record | `INTEGRATION` | `PROPOSED` | 14 |
| `IF-LOG-005` | Command lifecycle record | `UNIT` | `PROPOSED` | 14 |
| `IF-LOG-006` | Authority mode request record | `INTEGRATION` | `PROPOSED` | 14 |
| `IF-LOG-007` | Effective authority change record | `INTEGRATION` | `PROPOSED` | 14 |
| `IF-LOG-008` | Confirmation record | `INTEGRATION` | `PROPOSED` | 14 |
| `IF-LOG-009` | Security event record | `INTEGRATION` | `PROPOSED` | 14 |
| `IF-LOG-010` | Configuration change record | `INTEGRATION` | `PROPOSED` | 14 |
| `IF-LOG-011` | Mission event record | `INTEGRATION` | `PROPOSED` | 14 |
| `IF-LOG-012` | Failsafe event record | `INTEGRATION` | `OPEN` | 14 |
| `IF-LOG-013` | Value provenance in records | `UNIT` | `PROPOSED` | 14 |
| `IF-LOG-014` | Record writer queue | `UNIT` | `PROPOSED` | 14 |
| `IF-LOG-015` | Drop accounting | `UNIT` | `PROPOSED` | 14 |
| `IF-LOG-016` | Local-first write ordering | `INTEGRATION` | `CONFIRMED` | 14 |
| `IF-LOG-017` | Upload submission | `INTEGRATION` | `PROPOSED` | 14 |
| `IF-LOG-018` | Upload to a non-Aerora endpoint | `INTEGRATION` | `PROPOSED` | 14 |
| `IF-LOG-019` | Local record deletion on upload | `STATIC` | `PROPOSED` | 14 |
| `IF-LOG-020` | Audit store integrity protection | `INTEGRATION` | `OPEN` | 14 |
| `IF-LOG-021` | Record timestamp basis | `UNIT` | `PROPOSED` | 14 |
| `IF-LOG-022` | Correlation identity | `UNIT` | `PROPOSED` | 14 |
| `IF-LOG-023` | Retention deletion record | `INTEGRATION` | `PROPOSED` | 14 |
| `IF-LOG-024` | Record export | `DEMO` | `PROPOSED` | 14 |
| `IF-MAV-001` | Heartbeat, base and custom mode | `INTEGRATION` | `CONFIRMED` | 03 |
| `IF-MAV-002` | Attitude, position, velocity, altitude | `INTEGRATION` | `CONFIRMED` | 03 |
| `IF-MAV-003` | System and sensor health | `INTEGRATION` | `CONFIRMED` | 03 |
| `IF-MAV-004` | Battery state | `INTEGRATION` | `CONFIRMED` | 03 |
| `IF-MAV-005` | GPS and estimator state | `INTEGRATION` | `CONFIRMED` | 03 |
| `IF-MAV-006` | Command send and acknowledgement | `INTEGRATION` | `CONFIRMED` | 03 |
| `IF-MAV-007` | Mission transfer and progress | `INTEGRATION` | `CONFIRMED` | 03 |
| `IF-MAV-008` | Parameter protocol | `INTEGRATION` | `CONFIRMED` | 03 |
| `IF-MAV-009` | Status text and events | `INTEGRATION` | `CONFIRMED` | 03 |
| `IF-MAV-010` | Radio status | `INTEGRATION` | `CONFIRMED` | 03 |
| `IF-MAV-011` | Message signing | `INTEGRATION` | `CONFIRMED` | 03 |
| `IF-MAV-012` | Aerora system status | `BENCH` | `PROPOSED` | 03 |
| `IF-MAV-013` | Aerora vehicle capabilities | `BENCH` | `PROPOSED` | 03 |
| `IF-MAV-014` | Aerora payload status | `BENCH` | `PROPOSED` | 03 |
| `IF-MAV-015` | Aerora paint status | `BENCH` | `PROPOSED` | 03 |
| `IF-MAV-016` | Aerora pump status | `BENCH` | `PROPOSED` | 03 |
| `IF-MAV-017` | Aerora gimbal status | `BENCH` | `PROPOSED` | 03 |
| `IF-MAV-018` | Aerora autonomy status | `BENCH` | `PROPOSED` | 03 |
| `IF-MAV-019` | Aerora mission status | `BENCH` | `PROPOSED` | 03 |
| `IF-MAV-020` | Aerora diagnostics | `BENCH` | `PROPOSED` | 03 |
| `IF-MAV-021` | Reporting the Aerora autopilot identifier in the heartbeat | `BENCH` | `OPEN` | 03 |
| `IF-MAV-022` | Reporting which component originated a relayed value | `BENCH` | `OPEN` | 03 |
| `IF-MAV-023` | Reporting proposal acceptance or rejection explicitly | `HIL` | `OPEN` | 03 |
| `IF-MAV-024` | A per-command request identifier for audit correlation | `BENCH` | `OPEN` | 03 |
| `IF-MAV-025` | Declaring the resume behaviour of a state-setting command | `BENCH` | `OPEN` | 03 |
| `IF-MAV-026` | The vehicle reports a capability inventory on connection and on cha... | `BENCH` | `OPEN` | 03 |
| `IF-MAV-027` | Inbound message adjustment seam | `UNIT` | `CONFIRMED` | 03 |
| `IF-MAV-028` | Core-plugin message seam | `UNIT` | `CONFIRMED` | 03 |
| `IF-MIS-001` | Mission upload | `INTEGRATION` | `CONFIRMED` | 10 |
| `IF-MIS-002` | Mission download | `INTEGRATION` | `CONFIRMED` | 10 |
| `IF-MIS-003` | Mission current index | `INTEGRATION` | `CONFIRMED` | 10 |
| `IF-MIS-004` | Mission item reached | `INTEGRATION` | `CONFIRMED` | 10 |
| `IF-MIS-005` | Mission replacement | `INTEGRATION` | `CONFIRMED` | 10 |
| `IF-MIS-006` | Mission resume | `INTEGRATION` | `CONFIRMED` | 10 |
| `IF-MIS-007` | Mission pause | `INTEGRATION` | `OPEN` | 10 |
| `IF-MIS-008` | Plan file | `UNIT` | `CONFIRMED` | 10 |
| `IF-MIS-009` | Plan version and identity | `UNIT` | `PROPOSED` | 10 |
| `IF-MIS-010` | Mission verification | `INTEGRATION` | `PROPOSED` | 10 |
| `IF-MIS-011` | Aerora complex item | `UNIT` | `OPEN` | 10 |
| `IF-MIS-012` | Aerora mission command set | `INTEGRATION` | `OPEN` | 10 |
| `IF-MIS-013` | Aerora mission status | `INTEGRATION` | `PROPOSED` | 10 |
| `IF-MIS-014` | Payload state interaction on resume | `BENCH` | `OPEN` | 10 |
| `IF-MIS-015` | GCS-side payload state replay | `STATIC` | `CONFIRMED` | 10 |
| `IF-MIS-016` | QGC resume filter modification | `STATIC` | `CONFIRMED` | 10 |
| `IF-PAR-001` | Parameter discovery | `INTEGRATION` | `CONFIRMED` | 12 |
| `IF-PAR-002` | Parameter download | `INTEGRATION` | `CONFIRMED` | 12 |
| `IF-PAR-003` | Parameter write | `INTEGRATION` | `CONFIRMED` | 12 |
| `IF-PAR-004` | Parameter change notification | `INTEGRATION` | `CONFIRMED` | 12 |
| `IF-PAR-005` | Parameter metadata | `UNIT` | `PROPOSED` | 12 |
| `IF-PAR-006` | Parameter validation | `UNIT` | `CONFIRMED` | 12 |
| `IF-PAR-007` | Parameter cache | `INTEGRATION` | `CONFIRMED` | 12 |
| `IF-PAR-008` | Parameter synchronisation state | `INTEGRATION` | `CONFIRMED` | 12 |
| `IF-PAR-009` | Payload configuration as parameters | `INTEGRATION` | `OPEN` | 12 |
| `IF-PAR-010` | Parameter reset to defaults | `INTEGRATION` | `CONFIRMED` | 12 |
| `IF-PAY-001` | Payload identity and version | `INTEGRATION` | `PROPOSED` | 06 |
| `IF-PAY-002` | Payload capability set | `INTEGRATION` | `OPEN` | 06 |
| `IF-PAY-003` | Payload state | `INTEGRATION` | `OPEN` | 06 |
| `IF-PAY-004` | Payload health | `INTEGRATION` | `PROPOSED` | 06 |
| `IF-PAY-005` | Payload safety state | `INTEGRATION` | `PROPOSED` | 06 |
| `IF-PAY-006` | Payload emergency stop state | `INTEGRATION` | `PROPOSED` | 06 |
| `IF-PAY-007` | Payload telemetry | `INTEGRATION` | `PROPOSED` | 06 |
| `IF-PAY-008` | Payload configuration | `INTEGRATION` | `OPEN` | 06 |
| `IF-PAY-009` | Payload command | `INTEGRATION` | `OPEN` | 06 |
| `IF-PAY-010` | Operator emergency stop request | `INTEGRATION` | `OPEN` | 06 |
| `IF-PAY-011` | Payload calibration | `BENCH` | `OPEN` | 06 |
| `IF-PAY-012` | Payload diagnostics | `INTEGRATION` | `OPEN` | 06 |
| `IF-PAY-013` | Payload actuator interface | `STATIC` | `OPEN` | 06 |
| `IF-PAY-014` | Payload state retention across interruption | `BENCH` | `OPEN` | 06 |
| `IF-PAY-015` | Ground station to payload actuators | `STATIC` | `CONFIRMED` | 06 |
| `IF-PWR-001` | Tether connected or disconnected | `INTEGRATION` | `PROPOSED` | 07 |
| `IF-PWR-002` | Battery present or absent | `INTEGRATION` | `PROPOSED` | 07 |
| `IF-PWR-003` | Active source | `INTEGRATION` | `OPEN` | 07 |
| `IF-PWR-004` | Source transition | `INTEGRATION` | `OPEN` | 07 |
| `IF-PWR-005` | Battery state of charge | `INTEGRATION` | `CONFIRMED` | 07 |
| `IF-PWR-006` | Voltage, current, power | `INTEGRATION` | `PROPOSED` | 07 |
| `IF-PWR-007` | Power-system fault | `INTEGRATION` | `OPEN` | 07 |
| `IF-PWR-008` | Estimated remaining energy, where validated | `INTEGRATION` | `OPEN` | 07 |
| `IF-PWR-009` | Degraded or limited-power mode | `INTEGRATION` | `OPEN` | 07 |
| `IF-PWR-010` | Converter state | `INTEGRATION` | `OPEN` | 07 |
| `IF-PWR-011` | Watchdog fault | `INTEGRATION` | `PROPOSED` | 07 |
| `IF-PWR-012` | Power capability | `INTEGRATION` | `OPEN` | 07 |
| `IF-RAD-001` | Radio host presentation | `BENCH` | `OPEN` | 04 |
| `IF-RAD-002` | Serial port configuration | `INTEGRATION` | `CONFIRMED` | 04 |
| `IF-RAD-003` | USB device identity | `BENCH` | `OPEN` | 04 |
| `IF-RAD-004` | Byte stream | `UNIT` | `CONFIRMED` | 04 |
| `IF-RAD-005` | Link lifecycle | `INTEGRATION` | `CONFIRMED` | 04 |
| `IF-RAD-006` | Auto-connect decision | `INTEGRATION` | `CONFIRMED` | 04 |
| `IF-RAD-007` | GCS-local link observation | `INTEGRATION` | `CONFIRMED` | 04 |
| `IF-RAD-008` | Vehicle-reported link state | `INTEGRATION` | `PROPOSED` | 04 |
| `IF-RAD-009` | Reconnection | `INTEGRATION` | `PROPOSED` | 04 |
| `IF-SEC-001` | Operator sign-in | `INTEGRATION` | `PROPOSED` | 13 |
| `IF-SEC-002` | Local credential validation | `INTEGRATION` | `CONFIRMED` | 13 |
| `IF-SEC-003` | Role resolution | `INTEGRATION` | `OPEN` | 13 |
| `IF-SEC-004` | Authorisation decision | `INTEGRATION` | `PROPOSED` | 13 |
| `IF-SEC-005` | Gated-operation set | `STATIC` | `PROPOSED` | 13 |
| `IF-SEC-006` | Authorisation denial event | `INTEGRATION` | `PROPOSED` | 13 |
| `IF-SEC-007` | Audit identity | `INTEGRATION` | `PROPOSED` | 13 |
| `IF-SEC-008` | Credential lifetime state | `INTEGRATION` | `PROPOSED` | 13 |
| `IF-SEC-009` | Credential reconciliation | `INTEGRATION` | `PROPOSED` | 13 |
| `IF-SEC-010` | Security-to-cloud dependency | `UNIT` | `PROPOSED` | 13 |
| `IF-SEC-011` | Advanced-UI toggle as an authorisation boundary | `STATIC` | `PROPOSED` | 13 |
| `IF-SEC-012` | Signing key provisioning | `AUDIT` | `PROPOSED` | 13 |
| `IF-SEC-013` | Signing key storage at rest | `AUDIT` | `OPEN` | 13 |
| `IF-SEC-014` | Signing key selection | `INTEGRATION` | `CONFIRMED` | 13 |
| `IF-SEC-015` | Per-link signing state change | `INTEGRATION` | `CONFIRMED` | 13 |
| `IF-SEC-016` | Signing timestamp persistence | `UNIT` | `CONFIRMED` | 13 |
| `IF-SEC-017` | Unsigned-message acceptance policy | `INTEGRATION` | `OPEN` | 13 |
| `IF-SEC-018` | Signing key rotation | `AUDIT` | `PROPOSED` | 13 |
| `IF-SEC-019` | Settings-schema version change | `AUDIT` | `CONFIRMED` | 13 |
| `IF-SEC-020` | Telemetry field validation | `UNIT` | `PROPOSED` | 13 |
| `IF-SEC-021` | Dialect and dependency pin | `AUDIT` | `PROPOSED` | 13 |
| `IF-SYS-001` | Operator to ground station | `INTEGRATION` | `PROPOSED` | 01 |
| `IF-SYS-002` | Ground station to vehicle | `INTEGRATION` | `PROPOSED` | 01 |
| `IF-SYS-003` | Ground station to fleet backend | `INTEGRATION` | `PROPOSED` | 01 |
| `IF-SYS-004` | Ground station to local storage | `UNIT` | `PROPOSED` | 01 |
| `IF-SYS-005` | Ground station to companion | `UNIT` | `CONFIRMED` | 01 |
| `IF-SYS-006` | Ground station to payload actuators | `STATIC` | `CONFIRMED` | 01 |
| `IF-SYS-007` | Ground station to power switching | `STATIC` | `CONFIRMED` | 01 |
| `IF-SYS-008` | Ground station to inbound network | `UNIT` | `PROPOSED` | 01 |
| `IF-TLM-001` | Standard vehicle telemetry | `INTEGRATION` | `CONFIRMED` | 11 |
| `IF-TLM-002` | Aerora system telemetry | `INTEGRATION` | `PROPOSED` | 11 |
| `IF-TLM-003` | Aerora payload telemetry | `INTEGRATION` | `OPEN` | 11 |
| `IF-TLM-004` | Aerora product telemetry | `INTEGRATION` | `OPEN` | 11 |
| `IF-TLM-005` | Aerora power telemetry | `INTEGRATION` | `PROPOSED` | 11 |
| `IF-TLM-006` | Aerora link telemetry | `INTEGRATION` | `PROPOSED` | 11 |
| `IF-TLM-007` | Aerora autonomy telemetry | `INTEGRATION` | `OPEN` | 11 |
| `IF-TLM-008` | Fact group registration | `UNIT` | `CONFIRMED` | 11 |
| `IF-TLM-009` | Fact metadata | `UNIT` | `CONFIRMED` | 11 |
| `IF-TLM-010` | Freshness stamping | `UNIT` | `PROPOSED` | 11 |
| `IF-TLM-011` | Staleness evaluation | `UNIT` | `PROPOSED` | 11 |
| `IF-TLM-012` | Telemetry record | `UNIT` | `CONFIRMED` | 11 |
| `IF-TLM-013` | Telemetry value selection | `INTEGRATION` | `CONFIRMED` | 11 |
| `IF-UIS-001` | Effective state presentation | `INTEGRATION` | `PROPOSED` | 16 |
| `IF-UIS-002` | Requested state presentation | `INTEGRATION` | `PROPOSED` | 16 |
| `IF-UIS-003` | Proposed state presentation | `INTEGRATION` | `PROPOSED` | 16 |
| `IF-UIS-004` | Staleness presentation | `INTEGRATION` | `PROPOSED` | 16 |
| `IF-UIS-005` | Unknown and unavailable presentation | `INTEGRATION` | `PROPOSED` | 16 |
| `IF-UIS-006` | Action availability | `DEMO` | `PROPOSED` | 16 |
| `IF-UIS-007` | Command progress presentation | `INTEGRATION` | `PROPOSED` | 16 |
| `IF-UIS-008` | Confirmation presentation | `INTEGRATION` | `PROPOSED` | 16 |
| `IF-UIS-009` | Independent-indicator separation | `DEMO` | `PROPOSED` | 16 |
| `IF-UIS-010` | Tier-1 C++ option seam | `STATIC` | `CONFIRMED` | 16 |
| `IF-UIS-011` | Tier-2 purpose-built QML seam | `INTEGRATION` | `CONFIRMED` | 16 |
| `IF-UIS-012` | Tier-3 populated-file override | `AUDIT` | `PROPOSED` | 16 |
| `IF-UIS-013` | Aerora QML module registration | `INTEGRATION` | `CONFIRMED` | 16 |
| `IF-UIS-014` | Resource alias precedence | `INTEGRATION` | `OPEN` | 16 |
| `IF-UIS-015` | Startup self-check | `DEMO` | `PROPOSED` | 16 |
| `IF-UIS-016` | Operator action origination | `UNIT` | `CONFIRMED` | 16 |
| `IF-UIS-017` | Advanced-UI disclosure | `STATIC` | `CONFIRMED` | 16 |
| `IF-VID-001` | MAVLink system id | `INTEGRATION` | `CONFIRMED` | 08 |
| `IF-VID-002` | MAVLink component id | `INTEGRATION` | `CONFIRMED` | 08 |
| `IF-VID-003` | Aerora vehicle identity report | `INTEGRATION` | `OPEN` | 08 |
| `IF-VID-004` | Identity binding | `INTEGRATION` | `PROPOSED` | 08 |
| `IF-VID-005` | Identity source selection | `BENCH` | `OPEN` | 08 |
| `IF-VID-006` | Unidentified-vehicle attribution | `UNIT` | `PROPOSED` | 08 |
| `IF-VID-007` | Registry reconciliation | `INTEGRATION` | `PROPOSED` | 08 |

## Coverage of the test requirements

| Requirement | Where this plan carries it |
| --- | --- |
| `AERORA-TEST-REQ-004` | The `UNIT` section; four named properties, extended to eight by [R3-001](21-phase-3-risks.md) and [R3-007](21-phase-3-risks.md) |
| `AERORA-TEST-REQ-005` | The `INTEGRATION` section; `IF-CLD-004`, `IF-LOG-018` |
| `AERORA-TEST-REQ-006` | The `UNIT` section; the `Unit` label keeps the property tests in the default gate |
| `AERORA-TEST-REQ-007` | `IF-UIS-015`, `DEMO`, because the requirement is that the failure is **visible** |
| `AERORA-TEST-REQ-008` | The `AUDIT` section; the named conversion of `IF-UIS-012` from `AUDIT` to `UNIT` |
| `AERORA-TEST-REQ-009` | The framework table; Aerora adds no base class, no label and no runner |

Six test requirements, six carried. R-021 and Q-035 — whether Aerora tests in the overlay are
reachable at all — gate the whole plan and are not resolved by it.

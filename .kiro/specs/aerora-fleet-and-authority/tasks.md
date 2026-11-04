<!-- markdownlint-disable-file MD049 -->
<!-- The `_Requirements: X.Y_` trailer is the spec-tooling format and uses underscore emphasis,
     which MD049 rejects repo-wide. Scoped to this file; every other rule is enforced. -->

# Implementation Plan: aerora-fleet-and-authority

## Overview

This plan builds what `DG-ROL` permits. **It originates no operator command.** Four of the eleven
requirements describe command origination, the read-only posture of the gate-closure spec forbids it
while `DG-ROL` is `OPEN`, and the `actionable-control` rule would reject the user interface for it
before the change entered the repository. For those four the plan builds the types and the
evaluation, and stops at the point of transmission.

The order follows what this host can prove. Requirement 5 is the only requirement buildable end to
end, because criterion 12 lets the register carry a Link_Profile with its parameter values absent, so
nothing in it waits on a signature. It is also pure Python over JSON and source text, which means it
runs here with no compiler and no Qt. It therefore comes first, together with the two other register
additions and the checker that enforces overlay isolation.

The C++ follows, and every C++ task is CI-only by construction: this host has no compiler, no Qt and
3.9 GB of free disk, which is less than the toolchain alone requires. The two Aerora CI legs are the
only evidence available for it, and they are green today.

`ADR-014` is left open, and this plan is arranged so that it stays open without blocking. The
addressing axis of `AeroraGatedOperation` is built in task 5; the subject axis stays at one form.
When `ADR-014` closes it adds a subject form and touches no task here.

## Verification and standing constraints

Each sub-task is marked **Local** with the command that proves it on this host, or **CI-only** with
the leg that proves it.

Commands use the ephemeral `uv run --with` form, which needs no `tools/.venv` sync.

| Purpose | Command |
| --- | --- |
| Register against its schema | `uv run --quiet --with check-jsonschema check-jsonschema --schemafile .kiro/specs/aerora-decision-gate-closure/gate-register.schema.json .kiro/specs/aerora-decision-gate-closure/gate-register.json` |
| Data half | `uv run --quiet --with jsonschema python tools/validate_gate_register.py` |
| Source half | `uv run --quiet --with jsonschema python tools/analyzers/gate_guard_check.py aerora/ src/Vehicle/Vehicle.h` |
| Python tests | `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/<file>` |
| Python lint | `uv run --quiet --with "ruff==0.16.5" ruff check <files>` |
| Python types | `uv run --quiet --with "pyright==1.1.411" --with pytest --with hypothesis --with jsonschema pyright <files>` |
| Markdown | `npx --yes markdownlint-cli2 "<glob>"` |
| C++ build and test | Push to `aerora/**`; the **Aerora Linux** and **Aerora Windows** legs |
| Overlay lint | Push to `aerora/**`; the **Aerora Lint** leg |

Three standing constraints apply to every task:

1. **No Contract_Value.** No fact name, threshold, role name, capability identifier, interlock
   identifier, tier membership or MAVLink identifier is authored by any task here. A task needing a
   value it does not have records the shape and names the Gate.
2. **No new core edit.** Nothing outside `aerora/` is modified. Task 2 adds the checker that makes
   this measurable rather than assumed.
3. **No origination.** No task transmits a command to an aircraft or a Tether_Unit.

## Tasks

- [ ] 1. The Link_Profile, end to end

  - [x] 1.1 Add `linkProfiles` to the register schema
    - One entry per Link_Class, each carrying `linkClass`, `parameters`, `heldBy` and `revision`
    - `parameters` is an object and **may be empty**, which is the recorded state of a profile whose
      values are unsettled. `heldBy` names what settles them
    - The schema **rejects** a `throughput` or `aircraftCount` key anywhere in a profile, because a
      recorded figure is a second number to keep in step with the radio it describes
    - `linkClass` is a non-empty string rather than an enum: enumerating the link classes is a
      decision, and the data-half check establishes the set instead
    - _Requirements: 5.4, 5.12_
    - **Local**: `uv run --quiet --with check-jsonschema check-jsonschema --check-metaschema .kiro/specs/aerora-decision-gate-closure/gate-register.schema.json`

  - [x] 1.2 Add the model and the parser
    - `LinkProfile` in `tools/common/gate_register.py`, read and dumped like every other block, so
      the round-trip property covers it without a new test
    - _Requirements: 5.4_
    - **Local**: `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/test_gate_register.py -k profile`

  - [~] 1.3 Implement the derivation interface and its absent-value semantics
    - Every figure returns an optional and **none is stored**, so a profile change cannot leave a
      stale copy behind
    - A profile carrying no values yields no figure, and the caller reports unknown rather than a
      default. This is the half that is easy to get wrong and it is the half buildable now
    - Each figure carries the profile revision it came from
    - The arithmetic itself is **not** in scope here: it reads parameters that `Q-015` has not
      settled. See [Held by a Gate](#held-by-a-gate)
    - _Requirements: 5.5, 5.7, 5.8, 5.9_
    - **Local**: `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/test_gate_register.py -k derivation`

  - [~] 1.4 Implement the `link-profile` data-half check
    - Every Link_Class the register references carries exactly one profile, in both directions
    - A profile with empty `parameters` names a `heldBy`; a profile with parameters does not
    - No profile carries a throughput or aircraft-count figure, which is the schema rule read again
      where the rule is applied
    - _Requirements: 5.4, 5.9, 5.12_
    - **Local**: `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/test_validate_gate_register.py -k link_profile`

  - [~] 1.5 Record one profile per Link_Class, with values absent
    - Values absent and `heldBy` naming `Q-015`, so the shape is reviewable before the numbers exist
    - _Requirements: 5.12_
    - **Local**: `uv run --quiet --with jsonschema python tools/validate_gate_register.py`

  - [ ]* 1.6 Write property tests for the derivation
    - **Property 3: Every derived link figure follows from its profile**
    - **Property 4: A profile carrying no values yields no figure**
    - **Validates: Requirements 5.5, 5.7, 5.9**

- [x] 2. The overlay-isolation ratchet

  - [x] 2.1 Add `overlayEdits` to the register schema
    - One entry per file outside `aerora/` that carries an Aerora identifier, with its path, the
      reason and the owner who authorised it
    - Same ratchet shape as `mavlinkIdentifiers.allowList`, for the same reason: an
      over-approximating rule needs a recorded baseline or it is a cliff rather than a ratchet
    - _Requirements: 10.10_
    - **Local**: `uv run --quiet --with check-jsonschema check-jsonschema --check-metaschema .kiro/specs/aerora-decision-gate-closure/gate-register.schema.json`

  - [x] 2.2 Add the model, the parser and the data-half check
    - Every recorded entry names a path, a reason and an owner from `OWNER_LABELS`
    - _Requirements: 10.10_
    - **Local**: `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/test_validate_gate_register.py -k overlay`

  - [x] 2.3 Implement the `overlay-isolation` source-half rule
    - Walk every tracked file outside `aerora/` and match `Aerora[A-Z]` and `AERORA_`
    - Set equality in both directions: an unrecorded site is unassigned Decision_Content naming the
      file, and a recorded site the tree no longer carries is a removal, so the surface can grow and
      shrink only deliberately. The same shape `contract-surface` already uses
    - The finding names no Gate and renders `UNASSIGNED`: overlay isolation is unconditional rather
      than held by a Gate, and `contract-surface` already sets that precedent
    - A tree this rule could not walk is a failure rather than a pass
    - _Requirements: 10.10, 10.11_
    - **Local**: `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/test_gate_guard_check.py -k overlay_isolation`

  - [x] 2.4 Record the five sites the tree carries today
    - `src/Vehicle/Vehicle.h`, `test/Vehicle/MavCommandAuthorizationTest.cc`, `cmake/QGCTest.cmake`,
      and the two Aerora CI workflows
    - Each with its reason. The `Vehicle.h` entry notes that `adr-016-pair` already gates its content
    - _Requirements: 10.10_
    - **Local**: `uv run --quiet --with jsonschema python tools/analyzers/gate_guard_check.py aerora/ src/Vehicle/Vehicle.h`

  - [ ]* 2.5 Write the property test for overlay isolation
    - **Property 9: Aerora logic stays inside the overlay**
    - **Validates: Requirements 10.10, 10.11**

- [x] 3. The hazard log

  - [x] 3.1 Add `hazards` to the register schema
    - An identifier, the statement, the severity, the mitigation and the owner who accepted it
    - _Requirements: 10.9_
    - **Local**: `uv run --quiet --with check-jsonschema check-jsonschema --check-metaschema .kiro/specs/aerora-decision-gate-closure/gate-register.schema.json`

  - [x] 3.2 Add the model, the parser and the data-half check
    - Every hazard names an owner from `OWNER_LABELS` and a non-empty mitigation
    - Every rejection that cites a hazard record resolves to a recorded hazard, so a rule cannot
      cite a hazard that does not exist
    - _Requirements: 10.9, 2.9, 11.3_
    - **Local**: `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/test_validate_gate_register.py -k hazard`

  - [x] 3.3 Record the two hazards this design carries
    - Manual control input through a general-purpose operating system has unbounded worst-case
      latency; mitigated by sticks reaching the aircraft directly under a station-granted mode
    - A tether supply interruption long enough to brown out an ESC stops the motors; mitigated by the
      transfer being electrical with the station forbidden from participating
    - Both record a decision to keep software **out** of a path rather than to put it in one
    - _Requirements: 2.10, 11.2, 11.3_
    - **Local**: `uv run --quiet --with jsonschema python tools/validate_gate_register.py`

  - [ ]* 3.4 Write tests for the hazard-citation resolution
    - **Validates: Requirements 10.9**

- [~] 4. Checkpoint — the Python half proves itself on this host
  - Run all four `tools/tests/` files, both entry points, `ruff`, `pyright`, `markdownlint` and both
    schema validations
  - This is the last point before a hook gates a commit on the new blocks, and the only evidence
    available without CI
  - Ensure all tests pass, ask the user if questions arise
- [ ] 5. The addressing axis

  - [~] 5.1 Separate `AeroraGatedOperation` into an address and a subject
    - Today's flat command fields move into `AeroraCommandSubject`; today's single `vehicle` becomes
      an `AeroraOperationAddress` carrying one member
    - `AeroraCorePlugin::authorizeMavCommand()` assembles the new shape from the core hook's
      arguments, so **`QGCCorePlugin::authorizeMavCommand()` keeps its signature and no core edit is
      added**. Task 2's checker fails if this is got wrong
    - Existing behaviour is unchanged: one vehicle, one command, same decision
    - _Requirements: 3.1_
    - **CI-only**: Aerora Linux and Aerora Windows

  - [~] 5.2 Implement `AeroraOperationAddress`
    - A vehicle list, an all-or-nothing flag, and an opaque required-capability string carried for
      the reason `mavCommand` is carried as an int: no capability identifier is invented here
    - _Requirements: 3.1, 3.2, 4.1_
    - **CI-only**: Aerora Linux and Aerora Windows

  - [~] 5.3 Implement `AeroraFleetDecision` and the all-or-nothing evaluation
    - Authorization applies to every addressed member or to none, and a refusal names every member
      refused
    - **This evaluates and does not transmit.** Evaluation is not origination, which is why it is
      buildable while `DG-ROL` is open. The evaluation is a pure function over an address, a subject
      and the recorded policy, and is unit-tested as one
    - The per-member transmission outcome is a separate value from the single decision, because
      authorization is atomic and transmission cannot be
    - _Requirements: 3.3, 3.4, 3.5, 3.6, 3.7_
    - **CI-only**: Aerora Linux and Aerora Windows

  - [~] 5.4 Implement capability resolution, with exclusion distinct from refusal
    - An aircraft reporting no satisfying capability is removed from the addressed set **before**
      authorization and reported as excluded, never as refused
    - No capability is inferred from an airframe type, a firmware identifier or a product family
    - While the capability part of `DG-PAY` is unaccepted, a Capability_Address resolves to
      unresolvable rather than to every aircraft
    - _Requirements: 4.2, 4.3, 4.4, 4.7_
    - **CI-only**: Aerora Linux and Aerora Windows

  - [ ]* 5.5 Write property tests for the addressing axis
    - **Property 1: A Fleet_Operation is authorized atomically**
    - **Property 2: Addressing and subject vary independently**
    - **Property 5: Capability exclusion is not refusal**
    - **Validates: Requirements 3.3, 3.4, 4.3**

- [ ] 6. The manual authority grant

  - [~] 6.1 Implement `AeroraManualAuthorityGrant` and its expiry
    - A grant carries an opaque operator identifier, an issue time and an expiry, and reports
      whether it is in force at a given monotonic time
    - **The type carries no stick value.** Requirement 2 criterion 1 forbids the station from being
      on the manual control path, and a type that could hold an input would make that a review
      matter rather than a structural one
    - _Requirements: 2.2, 2.3_
    - **CI-only**: Aerora Linux and Aerora Windows

  - [~] 6.2 Record every grant transition in the audit trail
    - Issued, refused, refreshed and expired, each with the operator identity and the time
    - _Requirements: 2.8_
    - **CI-only**: Aerora Linux and Aerora Windows

  - [~] 6.3 Implement the mirrored-input read path
    - The flight controller reports stick positions and grant state as telemetry, and the station
      displays them. This is how the station observes manual control without carrying it
    - _Requirements: 2.7_
    - **CI-only**: Aerora Linux and Aerora Windows

- [ ] 7. The tether endpoint, read side

  - [~] 7.1 Report attachment and supply as distinct observations
    - An attached tether that has stopped supplying is neither absent nor healthy, so neither value
      is derived from the other and no combination collapses into one
    - _Requirements: 8.11_
    - **CI-only**: Aerora Linux and Aerora Windows

  - [~] 7.2 Derive Power_Mode from the capability inventory
    - Never from the presence or absence of a tether link. A station that inferred mode from link
      presence would report battery mode for a tethered aircraft whose link had merely dropped
    - _Requirements: 8.7, 8.8_
    - **CI-only**: Aerora Linux and Aerora Windows

  - [~] 7.3 Report tether values `UNAVAILABLE` where no unit is attached
    - Never `UNKNOWN`, and never a numeric default. `UNAVAILABLE` is producible only from a
      capability report, which is the State_Vocabulary rule unchanged
    - _Requirements: 8.9_
    - **CI-only**: Aerora Linux and Aerora Windows

  - [ ]* 7.4 Write the property test for the tether observations
    - **Property 6: Tether attachment and supply are independent**
    - **Validates: Requirements 8.11**

- [ ] 8. Tier and freshness mechanism

  - [~] 8.1 Report the live Telemetry_Tier and the not-carried distinction
    - A value absent because the live tier omits it is reported as not carried by that tier, and
      never as `UNKNOWN`. A value retained from a tier no longer live carries its age
    - _Requirements: 6.3, 6.4, 6.5, 6.7_
    - **CI-only**: Aerora Linux and Aerora Windows

  - [~] 8.2 Derive which tiers a Link_Class can carry
    - From the Link_Profile and the recorded tier memberships, so a tier the link cannot sustain is
      never offered
    - _Requirements: 6.9, 5.7_
    - **CI-only**: Aerora Linux and Aerora Windows

  - [~] 8.3 Implement the unachievable-threshold check
    - A recorded freshness threshold shorter than the interval the profile yields is reported
      unachievable on that class, rather than marking every value it carries `STALE`
    - This is the check that stops a low-rate link under thresholds written for a high-rate one from
      flooding the operator with false staleness and teaching them to ignore the indicator
    - _Requirements: 7.2, 7.3, 7.8_
    - **CI-only**: Aerora Linux and Aerora Windows

  - [ ]* 8.4 Write the property test for threshold achievability
    - **Property 7: An unachievable threshold is named, not applied**
    - **Validates: Requirements 7.8**

- [~] 9. Checkpoint — CI proves the C++ half
  - Push and confirm the **Aerora Lint**, **Aerora Linux** and **Aerora Windows** legs are green
  - Confirm the new suites ran rather than being silently unregistered, by reading the test counts
    in the leg output rather than only its conclusion
  - Ensure all tests pass, ask the user if questions arise

## Held by a Gate

Deliberately absent, with what holds each. Nothing here is an oversight and no task above depends
on one of them.

| Not built | Held by |
| --- | --- |
| The throughput arithmetic over the profile parameters | `Q-015`: the parameter set is unsettled, so there is nothing to compute over |
| Fleet transmission, and any user-interface control for it | `DG-ROL`: origination, and `actionable-control` would reject the control |
| Issuing a Manual_Authority_Grant | `DG-ROL`: a grant is an operator command |
| Any command to a Tether_Unit | `DG-ROL`: origination |
| The capability vocabulary a Capability_Address resolves against | `DG-PAY`, capability part |
| The Tether_Interlock set | `DG-PAY`, power part |
| The battery reserve, and refusing tethered-flight readiness below it | `DG-PAY`, power part |
| Which values each Telemetry_Tier carries | `DG-TLM` |
| The per-Link_Class freshness thresholds | `DG-FRS`; achievability is derivable now, the values are not |
| The mission command namespace and its allocation rule | `DG-MIS` |
| The registration identifier format and its issuing authority | `DG-FLT` |
| Flight-log retention and the tamper-evidence mechanism | `DG-STO` |
| A second subject form for a parameter write | `ADR-014`, left open; by construction it blocks nothing above |
| Where the backend runs, and the licensing boundary | Product, held at the owner's request |
| Raising the fifteen-channel ceiling | Architecture: a core edit with its own signature |
| The power failover itself | Electrical, and not establishable by any software test |

## Notes

- Tasks marked `*` are optional and can be skipped for a faster first pass.
- **No task originates an operator command.** `DG-ROL` is `OPEN`, so fleet transmission, grant issue
  and tether commands are all absent by construction rather than by omission. Task 5.3 evaluates and
  stops; evaluation is not origination, which is what makes it buildable now.
- No task introduces a Contract_Value. Where a task needs a value it does not have, it records the
  shape and names the Gate.
- No task modifies anything outside `aerora/`. Task 2 is what makes that measurable rather than
  assumed, and it is sequenced early for that reason.
- Local evidence stops at task 4. Everything from task 5 on is CI-only, because this host has no
  compiler, no Qt, and less free disk than the toolchain requires.
- `ADR-014` blocks nothing in this plan. Task 5.1's separation of the addressing axis from the
  subject axis is what guarantees that, and Property 2 is the test of the guarantee rather than a
  promise about it.
- Each group authors its schema before its model, because the model's round-trip property asserts
  that the serialised form validates against the schema, so the schema has to exist first.
- Requirement 5 leads because it is the only requirement buildable end to end: criterion 12 lets the
  register carry a Link_Profile with its values absent, so nothing in it waits on a signature.
- Task 1.3 stops short of the arithmetic. The derivation interface, the absent-value semantics and
  the revision reporting are buildable; the formula reads parameters `Q-015` has not settled. The
  half that is easy to get wrong is the half that is buildable.

## Task Dependency Graph

```json
{
  "waves": [
    { "id": 0, "tasks": ["1.1", "2.1", "3.1"] },
    { "id": 1, "tasks": ["1.2", "2.2", "3.2"] },
    { "id": 2, "tasks": ["1.3", "1.4", "2.3", "3.3"] },
    { "id": 3, "tasks": ["1.5", "1.6", "2.4", "2.5", "3.4"] },
    { "id": 4, "tasks": ["4"] },
    { "id": 5, "tasks": ["5.1", "6.1", "7.1", "8.1"] },
    { "id": 6, "tasks": ["5.2", "6.2", "6.3", "7.2", "7.3", "7.4", "8.2"] },
    { "id": 7, "tasks": ["5.3", "5.4", "8.3"] },
    { "id": 8, "tasks": ["5.5", "8.4"] },
    { "id": 9, "tasks": ["9"] }
  ],
  "edges": [
    { "from": "1.1", "to": "1.2" },
    { "from": "1.2", "to": "1.3" },
    { "from": "1.2", "to": "1.4" },
    { "from": "1.4", "to": "1.5" },
    { "from": "1.3", "to": "1.6" },
    { "from": "2.1", "to": "2.2" },
    { "from": "2.2", "to": "2.3" },
    { "from": "2.3", "to": "2.4" },
    { "from": "2.3", "to": "2.5" },
    { "from": "3.1", "to": "3.2" },
    { "from": "3.2", "to": "3.3" },
    { "from": "3.2", "to": "3.4" },
    { "from": "1.5", "to": "4" },
    { "from": "2.4", "to": "4" },
    { "from": "3.3", "to": "4" },
    { "from": "4", "to": "5.1" },
    { "from": "4", "to": "6.1" },
    { "from": "4", "to": "7.1" },
    { "from": "4", "to": "8.1" },
    { "from": "5.1", "to": "5.2" },
    { "from": "5.2", "to": "5.3" },
    { "from": "5.2", "to": "5.4" },
    { "from": "5.3", "to": "5.5" },
    { "from": "5.4", "to": "5.5" },
    { "from": "6.1", "to": "6.2" },
    { "from": "6.1", "to": "6.3" },
    { "from": "7.1", "to": "7.2" },
    { "from": "7.1", "to": "7.3" },
    { "from": "7.1", "to": "7.4" },
    { "from": "1.3", "to": "8.2" },
    { "from": "8.1", "to": "8.2" },
    { "from": "8.2", "to": "8.3" },
    { "from": "8.3", "to": "8.4" },
    { "from": "5.5", "to": "9" },
    { "from": "6.3", "to": "9" },
    { "from": "7.4", "to": "9" },
    { "from": "8.4", "to": "9" }
  ]
}
```

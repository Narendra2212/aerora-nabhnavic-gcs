<!-- markdownlint-disable-file MD049 -->
<!-- The `_Requirements: X.Y_` trailer is the spec-tooling format and uses underscore emphasis,
     which MD049 rejects repo-wide. Scoped to this file; every other rule is enforced. -->

# Implementation Plan: aerora-decision-gate-closure

## Overview

This plan builds the machinery of the Gate_Review and the empty Gate_Register. It does not close a
Gate. Writing the *content* of a Closure_Record is an owner's signature, not an engineering task, so
every task here builds the shape an owner signs into and the checks that read it back.

The two verified findings come first. Both were confirmed by direct inspection at branch tip
`e7b38eef1` and both correct factual errors in documents this spec depends on, so a task list built
on them without correcting them would encode the error. One lint exemption rides with them, because
the spell checker rejects the spec's own gate identifiers and would fail every commit that touches
these documents.

The order after that follows what this host can prove. The shared model and the two Python checkers
are pure functions over source text and JSON, so they run here with no compiler and no Qt — they are
the only part of this spec verifiable outside CI, and they are sequenced to prove themselves locally
before any hook gates a commit. The register comes next, then the hooks that gate on it, then the
three C++ ratchets whose only evidence is CI.

## Verification and standing constraints

Each sub-task is marked **Local** with the command that proves it on this host, or **CI-only** with
the leg that proves it. The authoring host has no compiler and no Qt, so every C++ task is CI-only by
construction.

Commands below use the ephemeral `uv run --with` form, which needs no `tools/.venv` sync. The
`--project tools` form also works but rebuilds `tools/.venv`.

| Purpose | Command |
| --- | --- |
| Data-half tests | `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/test_gate_register.py tools/tests/test_validate_gate_register.py` |
| Source-half tests | `uv run --quiet --with pytest --with jsonschema python -m pytest -q tools/tests/test_gate_guard_check.py` |
| Register check, direct | `uv run --quiet --with jsonschema python tools/validate_gate_register.py` |
| Guard check, direct | `uv run --quiet --with jsonschema python tools/analyzers/gate_guard_check.py aerora/ src/Vehicle/Vehicle.h` |
| Lint on new Python | `uv run --quiet --with "ruff==0.16.5" ruff check <paths>` and `ruff format --check <paths>` |
| Type check on new Python | `uv run --quiet --with "pyright==1.1.411" pyright <paths>` (downloads a node runtime on first run) |
| Schema check | `uv run --quiet --with check-jsonschema check-jsonschema --schemafile .kiro/specs/aerora-decision-gate-closure/gate-register.schema.json .kiro/specs/aerora-decision-gate-closure/gate-register.json` |

Constraints every task inherits:

- **No Contract_Value.** No task may introduce a fact name, unit, value range, enumeration member,
  freshness threshold, role name, permission, payload, tether or consumable item, fleet identifier
  format, proposal schema element, or MAVLink message, command or enumeration identifier. The
  register records *element names* and *owner labels*, both taken verbatim from requirements.md.
- **No action path.** No task may add an operator action control, an outbound command path, a mission
  upload, start or resume path, or a call to `AeroraMissionObserver::verify()`. The read-only ratchet
  walks *for* controls; it adds none.
- **No decision.** No task may require a decision this spec exists to elicit. Where a task needs a
  judgement that belongs to an owner, it records the question and names the owner.
- **Repo conventions.** Analyzers subclass `AnalyzerBase` and live in `tools/analyzers/`; entry points
  live at `tools/` root following `tools/validate_configs.py`; tests live in `tools/tests/`; `ruff`,
  `ruff format` and `pyright` must pass on new Python; `tools/common/` may not import `analyzers`,
  `generators`, `qgc_tools` or `setup` (the import-linter contract in `tools/pyproject.toml`); new
  pre-commit hooks are repo-local and gating, with no `stages: [manual]`.

## Tasks

- [x] 1. Reconcile the two verified findings against the tree at `e7b38eef1`

  - [x] 1.1 Correct the five records of the `ADR-016` pair's applied state
    - Both halves are applied: `friend class AeroraVehicleFactGroupRegistrar;` at
      `src/Vehicle/Vehicle.h:123`, and `AERORA_VEHICLE_FACTGROUP_FRIENDSHIP` in
      `aerora/CMakeLists.txt` `CUSTOM_DEFINITIONS`, so
      `AeroraVehicleFactGroupRegistrar::isRegistrationAvailable()` returns `true`
      (`AeroraVehicleFactGroupRegistrar.cc:22-28`)
    - Edit the five records that say otherwise: `32-integration-adrs.md:254`
      (`**NOT DEFINED** | Deliberate`); `aerora/src/Telemetry/AeroraVehicleFactGroupRegistrar.h:25`
      ("Aerora does not hold it yet"); the Aerora-added comment block at `src/Vehicle/Vehicle.h:115-123`
      ("deliberately NOT defined yet"); `04-subsystem-and-interface-completion-matrix.md:396` ("the
      friend declaration grants nothing without `AERORA_VEHICLE_FACTGROUP_FRIENDSHIP`"); and the
      `SCR-TEL-01` "Blocked by" cell at `03-gui-information-architecture-and-screens.md:543`
      ("`isRegistrationAvailable()` is false")
    - Record what the tree holds **and** that the authorisation is unresolved and owned by Tab 1.
      Record no Gate, part or Closure_Record as accepted, and change no screen's `Status` cell —
      whether the applied define carries Tab 1's authorisation is Tab 1's answer, and the register's
      recorded disagreement (task 9.3) is what keeps it visible until one lands
    - The `Vehicle.h` edit is comment-only and sits inside the block the overlay already added, so no
      code changes and the fork surface is unchanged. It is the one edit in this plan under `src/`
    - _Requirements: 4.5, 7.2, 7.17, 7.18_
    - **Local**: `git grep -n "NOT DEFINED\|does not hold it yet\|grants nothing without\|isRegistrationAvailable() is false"`
      returns nothing, and `git grep -n "AERORA_VEHICLE_FACTGROUP_FRIENDSHIP" -- src/ aerora/CMakeLists.txt`
      still shows both halves

  - [x] 1.2 Correct the screen inventory's declared totals to its own rows
    - The thirteen area headings state 4+9+9+6+3+3+3+1+2+4+4+1+3 = **52**, and each heading agrees
      with its own table's row count. With 14 `VERIFIED` and 1 `SUPERSEDED`, undelivered is **37**
    - Edit the four declared-total sites in
      `.kiro/specs/05-product-completion/03-gui-information-architecture-and-screens.md`: the §8 lead
      line (line 311, "45 screens across 12 areas"), the `### Inventory totals` table rows (`Not
      delivered` 30, `Total Aerora screens` 45), the paragraph that opens "The 30 undelivered
      screens" (line ~708), and the §9 lead line (line ~737, "45 entries")
    - Leave every area heading and every screen row untouched. The rows are the evidence
    - The durable guard against this recurring is the `inventory-totals` check (task 4.3), which
      derives the counts from the rows and reports the self-declared totals as a separate finding
    - _Requirements: 1.6, 1.7_
    - **Local**: `git grep -n -E "^### .+ [0-9]+ screens?$" -- .kiro/specs/05-product-completion/03-gui-information-architecture-and-screens.md`
      lists 13 headings summing to 52. Note that a whole-file row grep returns 57, because `SCR-`
      IDs also appear in the misread-blocker table and the totals table — which is why the parser in
      task 2.4 scopes to the area tables

  - [x] 1.3 Exempt the Aerora gate and screen identifiers from the spell checker
    - The `typos` hook splits `DG-MIS` and `SCR-MIS-05` on the hyphen and reports the three-letter
      fragment that follows the first hyphen as a misspelling of `MISS`. It fires 79 times across
      this spec's existing requirements and design documents, so every task that touches them fails
      `just lint` until this is settled
    - Add an `extend-ignore-re` entry to `.typos.toml` matching the identifier shape rather than the
      bare three-letter token, so a genuine misspelling elsewhere is still caught. Follow the
      discipline the file already states for its hex patterns: anchored, and narrow enough not to
      exempt ordinary words
    - _Requirements: 1.1_
    - **Local**: `uvx --from typos typos .kiro/specs/aerora-decision-gate-closure/` reports nothing

  - [x] 1.4 Correct the stale 45 and 30 figures in `requirements.md`
    - Four sites: the Introduction's first sentence (line 5), Requirement 1 criterion 6 ("each of the
      30 undelivered screens"), criterion 7 ("each of the 45 screens"), and the Source Material row
      describing the inventory as "The 45-screen inventory"
    - The design is already immune: it records no count and derives the set from the rows
    - _Requirements: 1.6, 1.7_
    - **Local**: `git grep -n -E "\b(45|30)\b" -- .kiro/specs/aerora-decision-gate-closure/requirements.md`
      returns no screen-count use

- [ ] 2. Register schema, shared model and the pure derivations

  - [x] 2.1 Add `hypothesis` to the `test` dependency group and regenerate the lock
    - `tools/pyproject.toml`, `test` group only. This is the one new dependency the design asks for,
      and it is test-only
    - _Requirements: 1.1_
    - **Local**: `uv lock --project tools`, then
      `uv run --quiet --with pytest --with hypothesis python -c "import hypothesis"`

  - [x] 2.2 Author `gate-register.schema.json` and `closure-record.schema.json`
    - Shape only, no Gate content beyond the ten identifiers. The three rules that become
      unrepresentable: `gates` exactly 10 items with `id` an enum of the ten identifiers and
      `uniqueItems`; `blockingEntries[].classification` a required enum of `Gate` and
      `Acceptance_Criterion`; `readOnly.exceptions` with `minItems: 1`, `maxItems: 1` and
      `items.screen` a `const`
    - `disagreement` is permitted only on a guard, never on a Release_Set, a classification, a
      signature or a `released` flag, and requires `observedValue`, `against`, `owner`, a non-empty
      `remedies` and a non-empty `handOnRow`
    - `entryCommit` is `null` while a part is `submitted` and a 40-character hex string once
      `accepted`; a Gate with `"guards": []` requires `restsOnReviewAlone: true`
    - _Requirements: 1.1, 1.2, 2.1, 3.1, 3.6, 4.8, 5.1, 5.11_
    - **Local**: `uv run --quiet --with check-jsonschema check-jsonschema --check-metaschema .kiro/specs/aerora-decision-gate-closure/*.schema.json`

  - [x] 2.3 Create `tools/common/gate_register.py` with the register and Closure_Record models
    - Dataclasses mirroring the schema, plus JSON load and dump. Pure model and derivation only — no
      file discovery, no git, no subprocess, so both entry points and the property tests import it
    - Imports nothing from `analyzers`, `generators`, `qgc_tools` or `setup`
    - _Requirements: 1.1, 1.2, 2.1, 3.1, 3.6_
    - **Local**: `uv run --quiet --with "ruff==0.16.5" ruff check tools/common/gate_register.py`

  - [x] 2.4 Parse the screen inventory's area tables into rows
    - Build on `tools/common/markdown.py`. Each row yields the screen ID, the delivery status and the
      blocking-entry token set of its "Blocked by" cell. Scope to the thirteen area tables, so the
      misread-blocker table and the totals table are not read as screen rows
    - Zero rows parsed is a failure, not an empty result, and a table that cannot be parsed fails
      naming the line
    - _Requirements: 1.7, 2.1, 2.10_
    - **Local**: `uv run --quiet --with pytest python -m pytest -q tools/tests/test_gate_register.py -k inventory_rows`

  - [ ]* 2.5 Write the Hypothesis strategies for register and inventory models
    - A composite strategy producing an inventory of screens with statuses from the status
      vocabulary, a register of ten Gates with one to four parts each, and blocking lists mixing both
      classifications
    - The strategies must deliberately produce invalid registers: these properties are two-sided, and
      a check that accepts everything passes a one-sided test
    - Edge cases the generators must reach: an empty owner set, an empty blocking list, a one-part
      Gate, a screen with two Gate-classified entries pointing at the same Gate, a keyed element with
      a duplicated key, a `mapping` relation total in one direction only, an accepted part with an
      unanswered Sign_Off_Condition, an accepted part carrying no permit verdict for a guard mapped to
      it, and an inventory row with no blocking entry
    - `@settings(max_examples=100)` on every property test
    - _Requirements: 1.1, 1.2, 2.1_
    - **Local**: `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/test_gate_register.py`

  - [ ]* 2.6 Write property test for the register model's JSON round trip
    - **Property 28: The register model round-trips through its JSON form**
    - **Validates: Requirements 1.1, 1.2, 2.1**

  - [ ]* 2.7 Write property test for the inventory row parser's round trip
    - **Property 29: The inventory row parser round-trips through the markdown table form**
    - **Validates: Requirements 1.7, 2.10**

  - [~] 2.8 Implement the five derivation equations
    - `gateOpen`, `releaseSet`, `released`, `twoGateScreens`, `undelivered`, exactly as the design
      states them. The Two_Gate_Screen rule falls out of `released` rather than being a special case
    - Each derivation returns its findings with the screen and the Gate named, because every caller
      has to report which decision is being protected
    - _Requirements: 1.3, 1.4, 1.10, 1.12, 2.5, 3.7, 3.8, 3.10, 5.7_
    - **Local**: `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/test_gate_register.py`

  - [ ]* 2.9 Write property test for the release derivation
    - **Property 1: Released is exactly the absence of an open Gate-classified entry**
    - **Validates: Requirements 1.3, 1.4, 2.5, 3.8, 3.10**

  - [ ]* 2.10 Write property test for per-Gate independence
    - **Property 2: Releasing one Gate's screens is independent of the other nine**
    - **Validates: Requirements 1.3, 5.9, 7.10, 9.8, 11.18**

  - [ ]* 2.11 Write property test for Two_Gate_Screen closure order
    - **Property 3: A Two_Gate_Screen's release does not depend on the order its Gates close**
    - **Validates: Requirements 1.4, 10.19, 11.16, 12.18**

  - [ ]* 2.12 Write property test for the Gate-open derivation
    - **Property 7: A Gate is open exactly when some part of it is unaccepted**
    - **Validates: Requirements 3.6, 3.7, 7.15, 9.14, 10.15, 11.15, 12.17, 15.13**

  - [ ]* 2.13 Write property test for Acceptance_Criterion neutrality
    - **Property 8: Acceptance_Criterion entries never affect Released**
    - **Validates: Requirements 2.4, 2.5**

  - [ ]* 2.14 Write property test for the Two_Gate_Screen set
    - **Property 9: The Two_Gate_Screen set is exactly the screens with two Gate-classified entries**
    - **Validates: Requirements 1.5**

  - [ ]* 2.15 Write property test for the delivery-status frame condition
    - **Property 11: A Gate closure leaves every recorded delivery status unchanged**
    - **Validates: Requirements 3.8, 9.18**

  - [ ]* 2.16 Write property test for the read-only posture's single input
    - **Property 25: The read-only posture reads `DG-ROL` and nothing else**
    - **Validates: Requirements 5.7, 9.18, 11.18**

- [~] 3. Checkpoint — the model proves itself on this host
  - Run `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/test_gate_register.py`
    and the lint and type-check commands on the new Python
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 4. `tools/validate_gate_register.py` — the cross-document half

  - [~] 4.1 Entry point, loading, schema validation and the error contract
    - Entry point at `tools/` root following `tools/validate_configs.py`, `pass_filenames: false`,
      non-zero exit on any finding, and it never rewrites the register
    - Implement the error contract in full: a register that is missing or cannot be parsed fails
      naming the path and the parse error; a schema failure reports the JSON pointer and runs no
      derived check; an inventory that cannot be parsed fails naming the line; zero screen rows
      fails. An unresolved premise is a failure, never a pass
    - _Requirements: 1.1, 2.1_
    - **Local**: `uv run --quiet --with jsonschema python tools/validate_gate_register.py` fails with
      a named reason while the register does not yet exist

  - [~] 4.2 Implement the set checks over the register and the inventory
    - `partition`: the ten recorded Release_Sets plus the Released-with-no-Gate set partition the
      derived undelivered set exactly
    - `release-set-derivation`: each recorded Release_Set equals the set derived from the
      classifications, with disagreements naming the screen and the Gate
    - `two-gate`: exactly `SCR-LIV-05`, `SCR-AIS-03`, `SCR-MIS-05` and `SCR-MIS-06` carry two
      Gate-classified entries, and no screen carries three
    - `released-derivation`: recorded `released` equals derived `released`, naming the open Gate for
      each disagreement
    - `delivered-outside` and `inventory-coverage`: no Release_Set holds a `VERIFIED` or `SUPERSEDED`
      row, and the register's screen list and the inventory's rows agree in both directions
    - _Requirements: 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.10, 1.11, 1.12, 2.5, 2.8, 3.8, 3.10, 5.7, 6.6, 7.8, 8.6, 9.7, 10.8, 10.9, 11.7, 11.8, 12.7, 12.8, 13.6, 13.14, 14.7, 15.7_
    - **Local**: `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/test_validate_gate_register.py`

  - [~] 4.3 Implement the classification and totals checks
    - `entry-classification`: the ID tokens in each inventory "Blocked by" cell equal the register's
      classified-entry set for that screen, with the symmetric difference named. Entries carrying no
      ID match by their register key plus an `inventoryText` substring that must appear in the cell
    - `reclassifications`: the five recorded misreads are present with their reclassification, and
      the retained-Gate statement holds — `SCR-VEH-03`, `SCR-VEH-04`, `SCR-SEC-01` and `SCR-HLP-03`
      retain no Gate-classified entry, `SCR-VID-01` retains one
    - `inventory-totals`: the inventory's declared totals agree with its own rows, reported as a
      finding for the inventory's owner either way
    - _Requirements: 2.1, 2.7, 2.10, 2.11_
    - **Local**: `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/test_validate_gate_register.py -k classification`

  - [~] 4.4 Implement `verified-observation`
    - Every inventory `VERIFIED` row has a register observation naming a suite and a slot that exist
      under `aerora/test/**` and are registered through `add_qgc_test`, and every recorded delivery
      status is a member of the inventory's status vocabulary
    - Text inspection of the test sources and the `*_TEST_NAMES` manifests only; no build, no Qt
    - _Requirements: 3.9_
    - **Local**: `uv run --quiet --with pytest --with jsonschema python -m pytest -q tools/tests/test_validate_gate_register.py -k observation`

  - [ ]* 4.5 Write property test for recorded against derived Release_Sets
    - **Property 4: A recorded Release_Set equals the set derived from the classifications**
    - **Validates: Requirements 1.2, 1.12, 6.6, 7.8, 8.6, 9.7, 10.8, 11.7, 12.7, 13.6, 14.7, 15.7**

  - [ ]* 4.6 Write property test for the undelivered partition
    - **Property 5: The Release_Sets and the Released-with-no-Gate set partition the undelivered screens**
    - **Validates: Requirements 1.6, 2.8**

  - [ ]* 4.7 Write property test for delivered-screen exclusion
    - **Property 6: No Release_Set contains a Delivered_Screen**
    - **Validates: Requirements 1.7, 1.11, 10.9, 13.14**

  - [ ]* 4.8 Write property test for inventory and register entry agreement
    - **Property 10: The inventory's blocking-entry tokens and the register's classified entries agree**
    - **Validates: Requirements 2.1, 2.10**

  - [ ]* 4.9 Write property test for the `VERIFIED` observation requirement
    - **Property 27: A `VERIFIED` status requires a recorded observation of a registered test**
    - **Validates: Requirements 3.9**

  - [ ]* 4.10 Write schema-rejection fixture tests
    - Nine Gates, eleven Gates, a duplicate Gate identifier, two read-only exceptions, and an
      exception naming a screen other than `SCR-HLP-03`
    - Each asserts the schema rejects the document, which is what makes "it cannot silently become
      two" a property of the format rather than of the reviewer
    - _Requirements: 1.1, 2.1, 5.1, 5.11_

- [ ] 5. `tools/validate_gate_register.py` — the Closure_Record half

  - [~] 5.1 Implement the three settlement shapes and `closure-completeness`
    - `single`: exactly one settled entry carrying every name in `requiredAttributes`, with each
      omitted attribute named
    - `keyed`: exactly one entry per key or an explicit exception per key, with a missing key and a
      duplicated key reported as distinct findings; the key set is either enumerated in the record or
      named by `keySource`
    - `mapping`: a relation total in both directions, naming the unmapped member and the set it came
      from
    - An element recording an excluded source rejects a settled entry declaring that source
    - Three generic rules, no per-Gate schema
    - _Requirements: 3.3, 3.11, 6.11, 6.13, 7.12, 8.3, 8.10, 8.11, 9.4, 9.5, 9.11, 10.12, 11.11, 11.12, 12.5, 12.12, 12.13, 12.14, 13.4, 13.9, 13.10, 15.10_
    - **Local**: `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/test_validate_gate_register.py -k settlement`

  - [~] 5.2 Implement `closure-signatures`
    - Set equality in both directions: every owner the Gate_Dossier records for the part has signed,
      and no signature comes from a party the dossier does not record as an owner of that part, with
      missing owners and unrecognised signatories reported separately
    - A part whose recorded owner set is empty is a named failure, not a vacuous pass
    - `signOffDate` present, and the `Gate-Signed-By:` trailer set on the adding commit matches the
      recorded owner set. The check compares recorded identity, not cryptographic authorship, and
      says so in its own output
    - _Requirements: 3.1, 3.4, 14.11, 15.17, 15.18_
    - **Local**: `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/test_validate_gate_register.py -k signature`

  - [~] 5.3 Implement `closure-entry-commit` against git history
    - Resolve the add-commit with `git log --diff-filter=A --format=%H -- <record>` through
      `tools/common/git.py`, and compare with the recorded `entryCommit`
    - A record whose state is `submitted` must carry no commit. If git history cannot answer — no
      `.git`, or a shallow clone — the check fails naming that reason rather than skipping
    - _Requirements: 3.1_
    - **Local**: `uv run --quiet --with pytest --with jsonschema python -m pytest -q tools/tests/test_validate_gate_register.py -k entry_commit`

  - [~] 5.4 Implement `gate-status-derivation`, `signoff-verdicts` and amendments
    - A Gate is `CLOSED` exactly when every part is accepted, with every unaccepted part named
    - Every Sign_Off_Condition the dossier records for an accepted part carries a verdict and a named
      reviewer. The check establishes totality; it never evaluates the verdict, because the verdict is
      the reviewer's
    - An amendment's signatures and sign-off date attach to its own key alone, and every other key
      keeps its recorded acceptance
    - _Requirements: 3.2, 3.5, 3.7, 3.11, 7.15, 8.5, 8.14, 9.14, 10.15, 11.15, 12.17, 15.13_
    - **Local**: `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/test_validate_gate_register.py -k "status or verdict or amendment"`

  - [ ]* 5.5 Write property test for the entry commit round trip
    - **Property 12: A Closure_Record's entry commit round-trips against git history**
    - **Validates: Requirements 3.1**
    - The pure half — a commit recorded exactly when the state is accepted — runs at the full
      iteration count. The half that resolves an add-commit needs a temporary repository per example;
      declare its reduced count on that test rather than lowering the property silently

  - [ ]* 5.6 Write property test for signature set equality
    - **Property 13: A part is accepted only when its signature set equals its recorded owner set**
    - **Validates: Requirements 3.4, 14.11**

  - [ ]* 5.7 Write property test for the empty owner set
    - **Property 14: A part recording no owner is a named failure, not a vacuous pass**
    - **Validates: Requirements 15.17, 15.18**

  - [ ]* 5.8 Write property test for Sign_Off_Condition totality
    - **Property 15: Every Sign_Off_Condition of an accepted part carries a verdict and a reviewer**
    - **Validates: Requirements 3.2, 3.5**

  - [ ]* 5.9 Write property test for the `single` settlement shape
    - **Property 16: A `single`-shaped element is settled by exactly one entry carrying every required attribute**
    - **Validates: Requirements 3.3, 3.11, 6.11, 6.13, 6.18, 7.12, 10.12, 11.12, 11.13, 12.14, 13.10, 15.10**

  - [ ]* 5.10 Write property test for the `keyed` settlement shape
    - **Property 17: A `keyed`-shaped element is settled by exactly one entry per key**
    - **Validates: Requirements 8.3, 8.10, 8.11, 12.5, 12.12, 12.13**

  - [ ]* 5.11 Write property test for the `mapping` settlement shape
    - **Property 18: A `mapping`-shaped element is settled only by a relation total in both directions**
    - **Validates: Requirements 9.4, 9.5, 9.11**

  - [ ]* 5.12 Write property test for the excluded source
    - **Property 19: An element whose declared source is the excluded source is rejected**
    - **Validates: Requirements 13.4, 13.9, 15.10**

  - [ ]* 5.13 Write property test for lifecycle state and transition resolution
    - **Property 20: A lifecycle state or transition resolves only against defined members**
    - **Validates: Requirements 11.5, 11.11, 11.12**

  - [ ]* 5.14 Write property test for amendment scope
    - **Property 21: An amendment settles its own key and retains every acceptance already recorded**
    - **Validates: Requirements 3.11, 8.5, 8.14**

  - [ ]* 5.15 Write property test for Decision_Content and excluded Acceptance_Criterion disjointness
    - **Property 22: A Decision_Content element and an excluded Acceptance_Criterion are never the same item**
    - **Validates: Requirements 1.9, 2.6, 6.16, 13.15**

- [ ] 6. The guard registry and the read-only records in the data half

  - [~] 6.1 Implement `guard-registry`
    - The registry covers at least the nine guards Requirement 4 criterion 5 names; every Gate records
      its guards or records `restsOnReviewAlone`; every `permittedToChangeBy` entry resolves to a part
      that exists
    - A guard is free only when every part naming it is accepted **and** each of those parts records a
      permitting verdict for it. A part accepted with no verdict for a guard mapped to it is rejected;
      a part accepted with a non-permitting verdict leaves the guard pinned, which is the case
      `kGcsPayloadStateReplayImplemented` needs after `DG-MIS` closes
    - _Requirements: 4.5, 4.6, 4.8, 6.17, 7.18, 8.15, 9.17, 10.17, 10.18, 11.19, 12.20, 13.16, 14.16, 15.16_
    - **Local**: `uv run --quiet --with pytest --with hypothesis --with jsonschema python -m pytest -q tools/tests/test_validate_gate_register.py -k guard`

  - [~] 6.2 Implement `forbidden-until`
    - `P7-FLY-004` is recorded `FORBIDDEN-UNTIL` `DG-ROL`, and both pre-approved undeclared seams are
      recorded as waiting on `DG-ROL`
    - _Requirements: 5.8, 9.16_
    - **Local**: `uv run --quiet --with pytest --with jsonschema python -m pytest -q tools/tests/test_validate_gate_register.py -k forbidden`

  - [ ]* 6.3 Write property test for the guard mapping and the permission rule
    - **Property 26: Every guard carries a Gate mapping, and a guard is free exactly when every part permitting it has permitted it**
    - **Validates: Requirements 4.5, 4.6, 4.8, 6.17, 9.17, 10.18**

- [ ] 7. `tools/analyzers/gate_guard_check.py` — the source half

  - [~] 7.1 Create the analyzer shell
    - `AnalyzerBase` subclass in `tools/analyzers/`, following `tools/analyzers/vehicle_null_check.py`:
      `name`, `run(files, fix=False) -> AnalysisResult`, a `main()` taking paths or stdin, exit 1 on
      findings, and `find_cpp_files` from `common.file_traversal` for directory arguments
    - `fix` is accepted and discarded with an explicit `del fix`, so the discard is visible rather
      than implied. There is no auto-fix: the only thing to "fix" would be a recorded value no owner
      signed
    - Every finding names the Gate being protected
    - _Requirements: 4.2, 4.7_
    - **Local**: `uv run --quiet --with jsonschema python tools/analyzers/gate_guard_check.py aerora/`

  - [~] 7.2 Implement `guard-value`, `guard-assertion` and the recorded disagreement
    - Each recorded guard's declaration in its recorded `declaredIn` file carries its recorded
      literal. A declaration not found fails naming the symbol and the file — "not found" is not
      "unchanged"
    - Each guard still appears in at least one recorded assertion site under `aerora/test/**`;
      a site that no longer names the symbol fails
    - A value matching a `disagreement.observedValue` prints the finding, the owner and the remedies
      on every run and exits zero. A third value fails naming all three. A `disagreement` with an
      absent or empty `handOnRow` fails
    - _Requirements: 4.6, 4.7, 4.9_
    - **Local**: `uv run --quiet --with pytest --with jsonschema python -m pytest -q tools/tests/test_gate_guard_check.py -k guard`

  - [~] 7.3 Implement `adr-016-pair`
    - The friend declaration in `src/Vehicle/Vehicle.h` and the
      `AERORA_VEHICLE_FACTGROUP_FRIENDSHIP` definition in `aerora/CMakeLists.txt` must match the
      register's recorded authorisation state. Read-only: the check reads `src/Vehicle/Vehicle.h` and
      never writes it
    - _Requirements: 7.17_
    - **Local**: `uv run --quiet --with jsonschema python tools/analyzers/gate_guard_check.py src/Vehicle/Vehicle.h aerora/CMakeLists.txt`

  - [~] 7.4 Implement `mavlink-identifier`
    - No `MAV_CMD_`, `MAVLINK_MSG_ID_`, `MAV_COMP_ID_` or `MAV_FRAME_` token appears under `aerora/**`
      outside the allow-list, which is recorded **by site and reason, never by identifier**
    - The check reads the allow-listed files, takes what it finds there as the current baseline, and
      fails on a token in any file the register does not allow-list, naming the file and `DG-MIS`.
      That keeps it a ratchet without the register holding a single identifier of its own
    - This is the one rule moved rather than duplicated: `aerora-ui-static-check.ps1` keeps working,
      and the authoritative copy becomes this hook, which sees `aerora/src/**` rather than only
      `aerora/src/UI`
    - _Requirements: 4.2, 6.14_
    - **Local**: `uv run --quiet --with pytest --with jsonschema python -m pytest -q tools/tests/test_gate_guard_check.py -k mavlink`

  - [~] 7.5 Implement the four absence checks
    - `populated-file-override`: no `Tier::PopulatedFile` entry in `AeroraQmlOverrides::overrideEntries()`
      while `DG-VID` is `OPEN`
    - `action-seam`: neither pre-approved seam appears in the override allow-list while `DG-ROL` is
      `OPEN`
    - `freshness-threshold`: `kDefinedThresholdCount` holds its recorded literal and no
      `setThreshold(` call site exists under `aerora/src/**`
    - `payload-command-sink`: no implementation of `AeroraPayloadCommandSink` exists under
      `aerora/src/**`
    - _Requirements: 5.4, 8.13, 8.16, 10.18, 14.13_
    - **Local**: `uv run --quiet --with pytest --with jsonschema python -m pytest -q tools/tests/test_gate_guard_check.py -k "override or seam or threshold or sink"`

  - [~] 7.6 Implement `actionable-control` over Aerora QML
    - No `QGCButton`, `MouseArea`, `TapHandler` or other actionable type is written into an Aerora QML
      document outside the single recorded exception, which must carry exactly one and whose
      `objectName` must equal the recorded one
    - Source inspection is the gate; it fires on this host before any build. It sees only types
      spelled out in a document, which is why the C++ walk in task 11 exists alongside it
    - The check walks for controls. It adds none
    - _Requirements: 5.1, 5.2, 5.11_
    - **Local**: `uv run --quiet --with pytest --with jsonschema python -m pytest -q tools/tests/test_gate_guard_check.py -k qml`

  - [~] 7.7 Implement `contract-surface`
    - Compare the enumerators, declared constants and registry string-literal keys under
      `aerora/src/**` against the recorded inventory, in both directions: present in source and absent
      from the inventory fails as unassigned Decision_Content naming the symbol; mapped to an `OPEN`
      Gate fails naming the element and the Gate; present in the inventory and absent from source
      fails as a removal
    - Record the limitation in the module's own docstring: this establishes that the surface did not
      change, not that no value was invented, and it cannot classify. A reviewer classifies once, in
      the register
    - _Requirements: 3.12, 4.2, 6.14, 7.16, 8.13, 9.15, 10.16, 11.17, 12.19, 13.12, 15.14_
    - **Local**: `uv run --quiet --with pytest --with jsonschema python -m pytest -q tools/tests/test_gate_guard_check.py -k contract_surface`

  - [ ]* 7.8 Write property test for the contract surface in both directions
    - **Property 23: The contract surface in source and in the register agree in both directions**
    - **Validates: Requirements 3.12, 4.2, 6.14, 7.16, 8.13, 9.15, 10.16, 11.17, 12.19, 13.12, 15.14**

  - [ ]* 7.9 Write source-fixture tests for every guard-check rule
    - A header flipping each recorded guard; a test file with an assertion site removed; a `MAV_CMD_`
      token added outside the allow-list; a `Tier::PopulatedFile` entry; an action seam added to the
      override allow-list; a `setThreshold(` call site; an implementation of
      `AeroraPayloadCommandSink`; and QML adding a `QGCButton`, a `MouseArea` and a `TapHandler`
    - Each asserts both the rejection and the Gate named in the finding, because a check that rejects
      without naming the Gate leaves the reviewer to work out which decision is being protected
    - Add the three-value guard case and the matched-disagreement case: matched prints and exits zero,
      a third value fails naming all three
    - _Requirements: 4.6, 4.7, 4.9, 5.1, 5.2, 5.4, 6.14, 8.13, 8.16, 10.18, 14.13_

- [~] 8. Checkpoint — both checkers prove themselves on this host
  - Run both test files, both direct script invocations, and the lint and type-check commands. This
    is the last point before a hook can gate a commit, and the only evidence available without CI
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 9. Populate the Gate_Register and scaffold the closures directory

  - [~] 9.1 Write the ten Gate_Dossiers
    - One dossier per Gate from Requirements 6 through 15: status, `implementingOwner`, a `rationale`
      reference (document plus anchor) rather than a copy of the narrative, `parts` with signing
      owners and Sign_Off_Conditions, `elements` with `settlementShape` and `requiredAttributes`,
      `releaseSet` with the `part` field where a screen is released in halves,
      `excludedAcceptanceCriteria`, and `openStateReading`
    - Decision_Content is recorded as **element names and owner labels, taken verbatim from
      requirements.md**. No settled entry, no example and no placeholder
    - `parts` is always a list, so a one-part Gate and a four-part Gate read the same way to the
      checker
    - _Requirements: 1.1, 1.2, 1.8, 3.6, 4.1, 4.3, 4.10, 6.1, 6.2, 6.3, 6.4, 6.5, 6.9, 6.10, 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.9, 7.10, 7.11, 8.1, 8.2, 8.3, 8.4, 8.7, 8.8, 8.9, 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.10, 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.10, 10.11, 10.20, 11.1, 11.2, 11.3, 11.4, 11.5, 11.6, 11.9, 11.10, 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.9, 12.10, 12.11, 13.1, 13.2, 13.3, 13.4, 13.5, 13.7, 13.8, 13.13, 14.1, 14.2, 14.3, 14.4, 14.5, 14.6, 14.9, 15.1, 15.2, 15.3, 15.4, 15.5, 15.6, 15.8, 15.9, 15.15, 15.17_
    - **Local**: `uv run --quiet --with check-jsonschema check-jsonschema --schemafile .kiro/specs/aerora-decision-gate-closure/gate-register.schema.json .kiro/specs/aerora-decision-gate-closure/gate-register.json`

  - [~] 9.2 Write the per-screen records and the blocking-entry classifications
    - One record per inventory row, with `released`, `observation` and `blockingEntries`. The register
      restates no screen name, class, seam, owner or delivery status — it references screen IDs and
      adds the axes the inventory has no column for
    - Classify every entry as exactly one of `Gate` or `Acceptance_Criterion`. Record the five
      reclassifications with the misread entry, and the three entries carrying no identifier with a
      `seam:` or `note:` key plus an `inventoryText` substring that must appear in the cell
    - _Requirements: 1.5, 1.6, 2.1, 2.2, 2.3, 2.4, 2.7, 2.8, 2.11, 3.9, 14.15, 15.16_
    - **Local**: `uv run --quiet --with jsonschema python tools/validate_gate_register.py`

  - [~] 9.3 Write the guard registry, including the one recorded disagreement
    - The nine guards Requirement 4 criterion 5 names, plus each Gate's own named guard, with
      `declaredIn`, `recordedValue`, `assertedIn`, `holdsOpen` and `permittedToChangeBy`. This is the
      only place the guard-to-Gate mapping is recorded — the C++ suites assert values, not mappings
    - One `disagreement`, for `AeroraVehicleFactGroupRegistrar::isRegistrationAvailable()`: both values
      pinned, the criteria the gap is against, Tab 1 as owner, the two remedies, and
      `handOnRow: "finding-1"`. Zero is the target
    - _Requirements: 4.5, 4.6, 4.8, 6.17, 7.18, 8.15, 9.16, 9.17, 10.17, 11.19, 12.20, 13.16, 14.16, 15.16_
    - **Local**: `uv run --quiet --with jsonschema python tools/analyzers/gate_guard_check.py aerora/ src/Vehicle/Vehicle.h`
      reports the one disagreement with its owner and remedies, and exits zero

  - [~] 9.4 Write the `readOnly` block and the contract-surface inventory
    - `readOnly`: `DG-ROL`, the single `SCR-HLP-03` clipboard exception with its file, control
      `objectName` and reason, the `forbiddenUntil` rows, and both pre-approved undeclared seams
    - `contractSurface`: the current enumerators, declared constants and registry string keys under
      `aerora/src/**`, each mapped to the Gate whose Decision_Content would record it or to `none`
      with a reason
    - _Requirements: 3.12, 4.2, 5.1, 5.8, 5.11, 9.16_
    - **Local**: both direct script invocations pass

  - [~] 9.5 Scaffold `closures/` with its contract and no content
    - A `closures/README.md` stating the one-file-per-part rule, the `<GATE>.<part>.json` naming, the
      `Gate-Signed-By:` trailer convention, and that `entryCommit` is derived from git and must be
      absent while a part is `submitted`
    - No Closure_Record is authored. Its content is an owner's signature, and this spec exists to
      elicit it rather than to supply it
    - _Requirements: 3.1, 3.6_

  - [ ]* 9.6 Write the named-fact tests against the real register
    - One data-driven test each for the five recorded groups: the reclassifications of Requirement 2
      criterion 7, the Released-with-no-Gate screens of criterion 8, the retained-entry counts of
      criterion 11, the nine guards of Requirement 4 criterion 5, and the `FORBIDDEN-UNTIL` rows
    - _Requirements: 2.7, 2.8, 2.11, 4.5, 5.8, 9.16_

- [ ] 10. Wire the three gating hooks into `.pre-commit-config.yaml`

  - [~] 10.1 Add `gate-register-schema` to the `check-jsonschema` block
    - Follows the `.github/build-config.json` precedent in the same block, with `pass_filenames: false`
      and a `files` pattern covering the register, its schema and the closures directory
    - _Requirements: 1.1, 2.1, 5.11_
    - **Local**: `uv run --quiet --with check-jsonschema check-jsonschema --schemafile <schema> <register>`

  - [~] 10.2 Add the `gate-register` local hook
    - In the `repo: local` block beside `validate-configs`, same invocation shape, `language: system`,
      `pass_filenames: false`, and a `files` pattern covering the register JSON, the closures
      directory, the screen inventory, `tools/validate_gate_register.py` and
      `tools/common/gate_register.py`
    - **Gating**: non-zero exit, no `stages: [manual]`, no `--advisory`. "Reject the change before the
      change enters the repository" is the criterion, and an advisory check rejects nothing
    - It lands in the `pre-commit.yml` leg only — the leg that runs on every pull request, checks out
      with full history so the entry-commit check can answer, and needs no compiler, no Qt and no
      build directory. Not in `aerora-linux.yml` or `aerora-windows.yml`
    - _Requirements: 1.6, 2.10, 3.1, 3.2, 4.2_
    - **Local**: `uv run --quiet --with jsonschema python tools/validate_gate_register.py`

  - [~] 10.3 Add the `gate-guards` local hook
    - Beside the other repo-local ratchets, `pass_filenames: true`, with a `files` pattern covering
      `aerora/src/**`, `aerora/test/**`, `aerora/CMakeLists.txt`, `src/Vehicle/Vehicle.h` and the
      register
    - Gating, on the same reasoning as 10.2
    - _Requirements: 3.12, 4.2, 4.7, 4.9, 5.2, 5.4, 6.14, 14.13_
    - **Local**: `uv run --quiet --with jsonschema python tools/analyzers/gate_guard_check.py aerora/ src/Vehicle/Vehicle.h`

  - [ ]* 10.4 Write a hook-wiring test
    - Assert the three hook IDs exist, that none carries `stages: [manual]`, and that each `files`
      pattern matches the register path, the inventory path and the guard source paths — a hook whose
      pattern misses the register is a hook that never runs
    - Place it in `tools/tests/test_pre_commit.py` beside the existing pre-commit machinery tests
    - _Requirements: 4.2, 4.7_
    - **Local**: `uv run --quiet --with pytest --with pyyaml python -m pytest -q tools/tests/test_pre_commit.py`

- [ ] 11. The three C++ ratchet assertions

  - [~] 11.1 Generalise the actionable-control predicate in `AeroraFlyViewOverlayUITest`
    - Replace `hasActionableControl()`'s `childItems()` recursion with a walk over the union of the
      visual tree and the object tree, deduplicated by pointer, returning the controls found by
      `objectName` where one is set and by class name otherwise. Keep the `QObject::inherits()`
      approach, and add the `QQuickPointerHandler` branch
    - This closes a real gap: a `TapHandler` is a `QObject` child of the item it handles, not a
      `QQuickItem`, so it is invisible to the current walk and would pass the existing assertion while
      being exactly what Requirement 5 criterion 1 forbids. Deduplication is load-bearing, not tidy,
      because the exception assertion counts
    - Put the walk in a header-only helper under `aerora/test/UI/` so task 11.2 consumes it rather
      than duplicating it, and declare that header in `AERORA_UI_TEST_SOURCES` in
      `aerora/src/UI/Ui.cmake`. No `add_qgc_test` line and no `*_TEST_NAMES` entry changes, so
      `aerora-manifest-check.ps1` keeps passing for the reason it passes now
    - The existing call site keeps its boolean by asking whether the returned list is empty
    - _Requirements: 5.1, 5.2_
    - **CI-only**: `aerora-linux.yml` / `aerora-windows.yml`, `^Aerora` with label `Integration`

  - [~] 11.2 Add the data-driven read-only walk to `AeroraUiFoundationTest`
    - **Property 24: No Aerora screen document carries an actionable control except the one recorded exception**
    - **Validates: Requirements 5.1, 5.2, 5.11**
    - `_noAeroraScreenCarriesAnActionableControl()` plus `_data()`. The screen set is **discovered**
      from the Aerora QML documents `AERORA_UI_QML_FILES` declares, not listed — a hand-maintained
      target list silently skipped a document for an entire pass once already, and the manifest check
      already proves that set complete in both directions. A new screen joins the walk by existing
    - Instantiate each document with `QQmlComponent` against the engine
      `QGCCorePlugin::instance()->createQmlApplicationEngine()` returns, which is also the only way
      the import path the `QGroundControl.Controls` module resolves from gets added. A document that
      does not instantiate is a failure naming the component error, not a skip
    - Resolve the content root before walking: if the root declares a `pageComponent` property holding
      a `QQmlComponent *`, create it in the root's own `QQmlContext` and walk that tree; otherwise walk
      the root. A declared `pageComponent` that does not instantiate is a failure. Without this the
      walk is vacuous for every page, and walking the page root instead would fail on upstream's own
      popout control and say nothing about Aerora's content
    - Assert the exception in both directions and by identity: the recorded document's content carries
      **exactly one** actionable control whose `objectName` equals the recorded one, and every other
      document carries none. That is the set-spelled-out shape
      `_exactlyOnePurposeBuiltSeamIsDeclared` already uses, because a count is satisfied by swapping
      one member for another
    - No `Q_ASSERT`, no `QTest::ignoreMessage`, no fixed-delay `QTest::qWait`. The walk needs no wait:
      `QQmlComponent::create()` returns a constructed tree
    - **CI-only**: `aerora-linux.yml` / `aerora-windows.yml`, `^Aerora` with label `Unit`

  - [~] 11.3 Pin the remaining guards at the composition root in `AeroraVehicleIntegrationTest`
    - Extend the existing guard slot with the recorded guards it does not yet assert
    - `AeroraVehicleFactGroupRegistrar::isRegistrationAvailable()` gets **no** assertion here, and the
      omission is deliberate: asserting `false` would fail CI because the `ADR-016` pair is applied,
      and asserting `true` would record an unauthorised core edit as the expected state. The Python
      check carries the finding (task 9.3) until Tab 1 signs the core-edit part or the definition is
      withdrawn, and the assertion is added in the same change as whichever remedy lands. State that
      reason in the slot's comment so the gap reads as a decision rather than an oversight
    - _Requirements: 4.7, 4.9_
    - **CI-only**: `aerora-linux.yml` / `aerora-windows.yml`, `^Aerora` with label `Integration`

- [~] 12. Final checkpoint
  - Local: both Python test files, both direct script invocations, the lint and type-check commands,
    and `uv run --quiet --with jsonschema python tools/validate_gate_register.py` against the
    populated register
  - CI: the `pre-commit.yml` leg for the three hooks, the `ci-scripts.yml` pytest jobs for the tools
    tests, and the `^Aerora` selections with labels `Unit` and `Integration` for the three suites
  - Ensure all tests pass, ask the user if questions arise.

## Out of scope for this spec

Five screens are recorded as Released with no Gate by Requirement 2 criterion 8: `SCR-LIV-08`,
`SCR-LIV-09`, `SCR-SET-01`, `SCR-SET-03` and `SCR-SET-04`. Every entry in each of their blocking
lists classifies as an Acceptance_Criterion, so nothing holds their work shut.

They are **not** tasks here. This spec builds the closure machinery; those five are buildable product
work for their screen owners, tracked through the screen inventory. They are listed so the next reader
can tell that they were excluded deliberately rather than missed.

## Notes

- Tasks marked `*` are optional and can be skipped for a faster first pass. The three C++ ratchets in
  task 11 are **not** marked optional even though they are test code: they are the deliverable of
  Requirement 4 criterion 7 and Requirement 5 criteria 1, 2 and 11, not elective coverage.
- No task writes a Closure_Record's content. Owners sign; these tasks build the shape and the checks.
- No task introduces a Contract_Value, an operator action control, an outbound command path, a mission
  upload, start or resume path, or a call to `AeroraMissionObserver::verify()`.
- Local evidence stops at task 10. Everything in task 11 is CI-only, because this host has no compiler
  and no Qt.
- The design names two test modules; this plan uses three, splitting the data half into
  `tools/tests/test_gate_register.py` (model, parser, derivations, schema fixtures) and
  `tools/tests/test_validate_gate_register.py` (the checker's cross-document, Closure_Record and
  guard-registry tests). That mirrors the existing `test_validate_configs.py` naming and keeps 30-odd
  property tests from landing in one file where none of them could be written in parallel.
- Task 2.2 authors the schemas before the checkers, ahead of the register's content in task 9. The
  schema is the model's shape and Property 28 asserts the serialised form validates against it, so the
  schema has to exist before the model's round-trip test can.
- The one recorded disagreement in task 9.3 is the register's only optional escape and it is confined
  to a guard. The derivations have no escape hatch, and zero recorded disagreements is the target.

## Task Dependency Graph

```json
{
  "waves": [
    { "id": 0, "tasks": ["1.1", "1.3", "1.4", "2.1"] },
    { "id": 1, "tasks": ["1.2", "2.2"] },
    { "id": 2, "tasks": ["2.3"] },
    { "id": 3, "tasks": ["2.4"] },
    { "id": 4, "tasks": ["2.5", "2.8"] },
    { "id": 5, "tasks": ["2.6", "4.1"] },
    { "id": 6, "tasks": ["2.7", "4.2"] },
    { "id": 7, "tasks": ["2.9", "4.3"] },
    { "id": 8, "tasks": ["2.10", "4.4"] },
    { "id": 9, "tasks": ["2.11", "4.5", "5.1"] },
    { "id": 10, "tasks": ["2.12", "4.6", "5.2"] },
    { "id": 11, "tasks": ["2.13", "4.7", "5.3"] },
    { "id": 12, "tasks": ["2.14", "4.8", "5.4"] },
    { "id": 13, "tasks": ["2.15", "4.9", "6.1"] },
    { "id": 14, "tasks": ["2.16", "5.5", "6.2"] },
    { "id": 15, "tasks": ["4.10", "5.6", "7.1"] },
    { "id": 16, "tasks": ["5.7", "7.2"] },
    { "id": 17, "tasks": ["5.8", "7.3"] },
    { "id": 18, "tasks": ["5.9", "7.4"] },
    { "id": 19, "tasks": ["5.10", "7.5"] },
    { "id": 20, "tasks": ["5.11", "7.6"] },
    { "id": 21, "tasks": ["5.12", "7.7"] },
    { "id": 22, "tasks": ["5.13", "7.8"] },
    { "id": 23, "tasks": ["5.14", "7.9"] },
    { "id": 24, "tasks": ["5.15", "9.1"] },
    { "id": 25, "tasks": ["6.3", "9.2"] },
    { "id": 26, "tasks": ["9.3"] },
    { "id": 27, "tasks": ["9.4", "9.5"] },
    { "id": 28, "tasks": ["9.6"] },
    { "id": 29, "tasks": ["10.1"] },
    { "id": 30, "tasks": ["10.2"] },
    { "id": 31, "tasks": ["10.3"] },
    { "id": 32, "tasks": ["10.4", "11.1", "11.3"] },
    { "id": 33, "tasks": ["11.2"] }
  ]
}
```

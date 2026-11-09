# Phase 7 â€” Aerora Universal GCS Product Completion

Owner: **Tab 1 (integration, architecture and dependency coordination)**.

This directory is the Phase A product-definition set for the Aerora Universal GCS completion
programme. It continues beyond the Phase 6 foundation and covers the full product: GUI, workflows,
vehicle and mission management, AI supervision, telemetry, security, diagnostics, data management
and future product profiles.

**Nothing is built, linted, loaded or executed on this host.** No compiler, CMake, Qt, `qmllint`,
`ctest` or `pre-commit` exists here, so every claim made from this host is source-level. See
[Host tooling](#host-tooling).

**That is no longer the same as nothing being built.** The overlay has its own CI legs, and
`Aerora Lint`, `Aerora Linux` and `Aerora Windows` all pass: configure, build, an assertion that
`QGC_CUSTOM_BUILD` actually attached the overlay, and `ctest -R '^Aerora'` over the `Unit` and
`Integration` labels with `--no-tests=error` so a vacuous pass fails the job. The Linux leg boots a
real QML window under `xvfb`. `VERIFIED` is therefore claimed, and earned, for the screens
[03](03-gui-information-architecture-and-screens.md#inventory-totals) records under it. `D-006` is a
statement about this host, not about the programme.

## Document set

| # | Document | Content |
| --- | --- | --- |
| 01 | [Product Requirements and Completion Plan](01-product-requirements-and-completion-plan.md) | Master PRD: product definition, operating modes, architecture boundaries, the 20 module requirement blocks, user journeys |
| 02 | [GCS Reference Benchmark](02-gcs-reference-benchmark.md) | Identity-verified benchmark of the 18 reference products, with the three misattributions corrected |
| 03 | [GUI Information Architecture and Screen Specification](03-gui-information-architecture-and-screens.md) | Navigation model, design system, complete screen inventory, every screen mapped to a real seam |
| 04 | [Subsystem and Interface Completion Matrix](04-subsystem-and-interface-completion-matrix.md) | Per-subsystem as-built status, the service/interface inventory, what is missing versus what is contract-only |
| 05 | [Implementation Dependency Graph](05-implementation-dependency-graph.md) | Build order, hard and soft dependencies, the critical path, what can proceed in parallel |
| 06 | [Requirements-to-Test Traceability Matrix](06-requirements-to-test-traceability.md) | Requirement â†’ component â†’ test, with coverage gaps named |
| 07 | [Phase 7 Work Packages and Tab Ownership](07-phase-7-work-packages-and-ownership.md) | Per-tab assignment, prerequisites, exact editable file set, conflict avoidance |
| 08 | [Risk Register and Unresolved Decisions](08-risk-register-and-open-decisions.md) | Risks, all OPEN decisions, blockers, and who must resolve each |
| 09 | [Local Persistence Architecture](09-local-persistence-architecture.md) | WP1-C: schema, platform paths, durability, retention, export, failure behaviour, and the Q-037 assessment |
| 10 | [Parameter Write Path and Authorization Boundary](10-parameter-write-path-and-authorization-boundary.md) | WP9-A: who can write a vehicle parameter, from where, and what the current gate does and does not cover |
| 11 | [Mission Command Set and Coverage Items](11-mission-command-set-and-coverage-items.md) | The content `DG-MIS` settles: the command set, the one Aerora complex item, its parameters, the geometry and the plan-file key |
| 12 | [Payload, Tether and Gimbal Item Sets](12-payload-tether-and-gimbal-item-sets.md) | The content `DG-PAY` waits on, derived from the firmware model: the reported item names and the units the model evidences |
| 13 | [Autonomy Source Path and Companion Topology](13-autonomy-source-path-and-companion-topology.md) | The owner's answer to `Q3-005`: companion traffic terminated at the flight controller, what that settles for `DG-AUT`, and the clock obligation it creates |
| 14 | [Ground Link Classes and Control Authority](14-ground-link-classes-and-control-authority.md) | The owner's answer on the ground links: `lora-wifi` as one Link_Class, capability-based routing, the three taxonomies that are not the same set, and the RC wiring question that blocks the authority model |
| 15 | [Operator Identity and the Single-Role Deployment](15-operator-identity-and-the-single-role-deployment.md) | The owner's login statement, why "one role" is a deployment rather than a change to the signed `Q-029` role set, and the three installables that are all that hold enforcement open |

Document numbers are allocated by Tab 1 **before** a document is created. 09 was briefly used twice
and Tab 9's was renumbered to 10; see
[07 § Known shared-file hazards](07-phase-7-work-packages-and-ownership.md#known-shared-file-hazards).

Read 01 and 03 first. 04 and 05 are the implementation gate. 07 is the assignment sheet. 09 is the
first Phase C design document.

## Phase C progress

| Work package | Status | Commit |
| --- | --- | --- |
| U1 — install the QML override interceptor (HO-TAB7-02) | `IMPLEMENTED-UNCOMPILED` | `84bbb3a6e` |
| U2 — complete the `AeroraControlsModule` block (HO-TAB7-03) | `IMPLEMENTED-UNCOMPILED` | `a73699796` |
| U3 — drop `WRITE` from `AeroraStateValue::capability` (HO-TAB7-01) | `IMPLEMENTED-UNCOMPILED` | `a7b317f89` |
| HO-TAB1-03 — audit all twelve QML-registered types; derive the closure | `IMPLEMENTED-UNCOMPILED` | `67f7137d0` |
| `SCR-SHL-01` — Aerora Operations Hub, the first Aerora screen | `IMPLEMENTED-UNCOMPILED` | `3b19d112a`, `d2ffd7c93` |
| HO-TAB7-05 — register the hub in `analyzePages()` | `IMPLEMENTED-UNCOMPILED` | `39406ff5f`, `fddb83459` |
| HO-TAB1-10 — the `SCR-SHL-03` enforcement-state indicator QML | `IMPLEMENTED-UNCOMPILED` | `602b38a18` |
| HO-TAB7-06 — register the indicator in `toolBarIndicators()` | `IMPLEMENTED-UNCOMPILED` | this pass |
| WP1-C — local persistence architecture | `DESIGNED` | doc 09 |
| U4 — `AeroraAutoPilotPlugin` | assigned, Tab 3 | — |
| U5 — per-vehicle tool indicators | assigned, Tab 3 | — |

### Screen inventory, stated precisely

52 screens. **The counts are not restated here.**
[03 § Inventory totals](03-gui-information-architecture-and-screens.md#inventory-totals) holds them
and `tools/validate_gate_register.py` checks that table against the register's screen records, so
repeating a figure here would make this the second place it can be wrong — which is the mistake that
table's own derivation was written to stop.

What is worth stating is where the remaining work sits, because it has moved:

| | |
| --- | --- |
| Blocked by a Gate still open | **4 screens.** `SCR-AIS-01`, `SCR-AIS-02`, `SCR-AIS-03` and the proposal half of `SCR-LIV-05`, all on `DG-AUT:source-interface` |
| Gate-released and not yet built | **33 screens.** Nine of the ten Gates have closed, so these wait on implementation rather than on a decision |
| Delivered | The rows 03's totals table records `VERIFIED`, each resolving to a suite and slot the register names and CI runs |

That is a reversal worth noticing. This section previously recorded the undelivered screens as
"overwhelmingly **not** blocked on UI work" but on absent data contracts — no dialect, no freshness
budgets, no durable store, no role set, no fleet identity format. That was true when written.
`DG-TLM`, `DG-FRS`, `DG-STO`, `DG-ROL` and `DG-FLT` have since closed, so for 33 of the 37 the
constraint `P7-NOINV-001` imposed is lifted and what remains is building them.

### Next integration blockers

In the order they gate the most work. None is a UI screen.

| # | Blocker | Owner | What it gates |
| --- | --- | --- | --- |
| 1 | **`D-006` build host, narrowed.** No local build, so nothing can be compiled or tested *here* and every claim made from this host stays source-level. The overlay's three CI legs do build and test it, on Linux and Windows, so this no longer gates a `VERIFIED` claim | User / Tab 8 | Local iteration speed. A CI round trip is the only way to see a compile error, which is minutes rather than seconds per attempt |
| 2 | **HO-TAB7-04 cannot be built as specified**, and the specified shape would not have helped. A standalone Qt Quick Test executable can import only pure-Qt modules, because every QGC and Aerora QML module is `NO_PLUGIN` and the `QGC` module that holds the C++ singletons is declared on the **application executable** — so a second binary cannot import it. See [04-ui-implementation 01 § HO-TAB7-04](../04-ui-implementation/01-ui-foundation-closure.md#ho-tab7-04--an-aerora-qt-quick-test-target-when-needed) | Tab 1 + Tab 7 | The 16 `QML`-method tests, which need redirecting onto the in-process harness rather than a new target. The `P7-AUTH-002` fail-safe derivation is **no longer review-only**: `AeroraUiFoundationTest` now reads every Aerora document out of the resource and asserts the comparison direction |
| 3 | **`Q4-002` core edit** — one `friend` line in `src/Vehicle/Vehicle.h` plus a define | Tab 1 authorises | Aerora telemetry reaching the value picker and the CSV record; `SCR-TEL-01` |
| 4 | ~~**`Q-029` / ADR-014** authorization policy~~ | Tab 2 | **DECIDED.** `DG-ROL` is `CLOSED`: the role set is signed and ADR-014 is selected as Option C. What is left is the identity provider, the role resolver and a configured policy, which is implementation. Doc 15 |
| 5 | **`Q-037`** audit durability and tamper evidence | Tab 2 | `SCR-SEC-03`, `SCR-LOG-01`, and the audit rows the hub currently reports as false |
| 6 | **`HO-TAB1-11`** `AERORA_TELEMETRY_TEST_NAMES` absent | Tab 4 | Two telemetry test targets are unreachable from CTest; the manifest check fails on it |

`D-006` is first by a wide margin and is the only one whose resolution is not an engineering task.

## Status vocabulary

Used uniformly across all eight documents. A status is a claim about evidence, not about intent.

| Status | Means | Evidence required |
| --- | --- | --- |
| `VERIFIED` | Built, executed and observed to behave as specified | A build, a test run, or hardware observation. **Not claimed anywhere in Phase 7** |
| `SOURCE-INSPECTED` | A fact about existing source, established without building or executing anything | Reading, grepping, or mechanical comparison of source at a named commit. Says nothing about behaviour |
| `IMPLEMENTED-UNCOMPILED` | Code is written and complete in source; never compiled | Source exists, is declared in a manifest, and reads as complete |
| `CONTRACT-ONLY` | Types, signatures and semantics exist; no behaviour behind them | Header exists; implementation is absent, empty, or an explicit `kâ€¦Implemented = false` |
| `DESIGNED` | Specified to the point of being implementable; no code | A specification naming the seam, the owner and the acceptance criteria |
| `SPECIFIED-NOT-DESIGNED` | Requirement captured; mechanism not yet chosen | A requirement ID with no component mapping |
| `BLOCKED` | Cannot proceed until a named dependency resolves | The blocking item's ID |
| `OPEN` | A decision is required from a named owner | The decision, the owner, and what it blocks |
| `FORBIDDEN` | Deliberately not built, with the reason recorded | The prohibiting requirement |

### Why `SOURCE-INSPECTED` exists, and what it is not

Most of what this programme knows, it knows by reading the checkout. That is real evidence and it
deserves a name — but it is not `VERIFIED`, because nothing was built and nothing ran.

Two module documents had independently adopted `VERIFIED` for exactly this, each scoping it locally
to "facts read directly from source". Internally consistent, and a collision with the programme
definition above: a reader grepping for `VERIFIED` would find claims that look like execution
evidence and are not. `SOURCE-INSPECTED` resolves it by naming the weaker class instead of
overloading the stronger one. **The definition of `VERIFIED` is unchanged.**

`SOURCE-INSPECTED` covers reading a declaration, grepping for the absence of a symbol, and
comparing two source constructs mechanically. It does **not** cover compiling, linking, linting,
loading or executing — those remain `NOT RUN` until a build host exists (`D-006`).

### Why `IMPLEMENTED-UNCOMPILED` is kept distinct

It is the dominant status of the Phase 6 foundation: roughly 109 overlay sources and 14 registered
tests that have never been through a compiler. Collapsing it into "implemented" would mean the first
build is reported as a regression rather than as the first measurement. Collapsing it into
"not implemented" would discard real work. See
[04 Â§ First-build expectations](04-subsystem-and-interface-completion-matrix.md#10-first-build-expectations).

## Telemetry state vocabulary

Carried forward from Phase 2â€“4 and **not** redefined here. These seven states stay distinct through
every layer including the UI; collapsing any pair is a defect.

| State | Means |
| --- | --- |
| `REPORTED` | The vehicle said this. Authoritative |
| `EFFECTIVE` | What is actually in force now, as reported |
| `REQUESTED` | The station asked for this; not confirmed in force |
| `PROPOSED` | An autonomy source suggests this; no authority behind it |
| `STALE` | Was reported, is now older than its freshness budget |
| `UNKNOWN` | Never reported, and the vehicle has not said whether it can |
| `UNAVAILABLE` | The vehicle reported it cannot provide this |

`UNAVAILABLE` is producible **only** from a vehicle capability report
(`AERORA-GCS-REQ-029`, `IF-UIS-005`). A UI or service that manufactures it is a defect, which is why
[HO-TAB7-01](../04-ui-implementation/01-ui-foundation-closure.md#ho-tab7-01--aerorastatevaluecapability-was-qml-writable--closed)
was tracked rather than tolerated, and is now closed by U3 (`a7b317f89`): QML reads the capability
classification and cannot write it.

## Baseline and branch protection

| Item | Value |
| --- | --- |
| Frozen Phase 6 source checkpoint | `738462422e7579fd5102cffac53e625707a72340` |
| Checkpoint branch | `aerora/phase6-integration-checkpoint` â†’ `1cc92e61e` (docs-only commit on top) |
| Phase 7 development branch | `aerora/phase7-product-completion`, created at `1cc92e61e` |
| `origin` | `https://github.com/mavlink/qgroundcontrol.git` â€” **upstream, fetch and push** |
| `aerora` | `https://github.com/Narendra2212/aerora-nabhnavic-gcs.git`, fetch and push. The Aerora remote |
| Pushes performed | `aerora/phase7-product-completion` is published on the `aerora` remote, so **`D-005` is resolved**. Nothing has ever been pushed to `origin` |
| Destructive git operations | **None.** No amend, reset, rebase or force |

Returning to the frozen checkpoint is `git checkout aerora/phase6-integration-checkpoint`. The
branch is untouched by Phase 7 and points at the same commit it did at handoff.

## Host tooling

Re-measured at the start of this pass, not carried forward from `D-006`.

| Tool | State |
| --- | --- |
| `cmake`, `ninja`, `qmake`, `ctest` | **Absent** |
| `C:\Qt` | **Absent** |
| `qmllint`, `clang-format`, `clang-tidy`, `pre-commit`, `just` | **Absent** |
| `python`, `git`, `node` | Present |

`D-006` (build host) remains `NO-GO` and the build-host prohibition is **unchanged**. The available
substitutes are the source-level checks in `.kiro/tools/` â€” see
[04 Â§ Static checks available](04-subsystem-and-interface-completion-matrix.md#9-static-checks-available).

## Relationship to earlier phases

| Phase | Directory | Status in Phase 7 |
| --- | --- | --- |
| 0 â€” QGC baseline analysis | `.kiro/specs/00-qgc-baseline-analysis/` | Current. Re-verified where Phase 7 depends on it |
| 1 â€” Aerora architecture | `.kiro/specs/01-aerora-architecture/` | Current. Phase 7 extends, does not supersede |
| 2 â€” Interface specification | `.kiro/specs/02-interface-specification/` | Current. The ICD set is the contract Phase 7 implements against |
| 3 â€” Implementation specification | `.kiro/specs/03-implementation-specification/` | Current. `29-integration-status.md` is superseded **only** in its "recommended next parallel work" section, by [07](07-phase-7-work-packages-and-ownership.md) |
| 4 â€” UI implementation | `.kiro/specs/04-ui-implementation/` | Current and authoritative for the UI foundation. [03](03-gui-information-architecture-and-screens.md) extends it to the full screen set |

Where this directory and an earlier phase disagree on a **fact about the checkout**, this directory
is authoritative â€” it was verified against the tree this pass. Where they disagree on a **decision**,
the earlier ratified decision stands until explicitly superseded, and the supersession is recorded in
[08](08-risk-register-and-open-decisions.md).

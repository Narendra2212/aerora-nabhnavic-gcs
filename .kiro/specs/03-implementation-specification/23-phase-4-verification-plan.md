# Phase 4 Verification Plan

Per-module verification, the gates each must pass, and the evidence that would satisfy them.
**Nothing has been executed. No result is recorded.**

## Nothing has been run

| Fact | Value |
| --- | --- |
| Tests written | **0** |
| Tests executed | **0** |
| Builds performed | **0** |
| Interfaces marked `VERIFIED` | **0** |
| Modules marked `IMPLEMENTED` | **0** |

The host cannot build: 1.00 GB free disk, no administrator rights, no compiler, no Qt. R4-003.
Every entry below is an assignment.

## Methods

Carried from Phase 3 unchanged.

| Code | Environment |
| --- | --- |
| `STATIC` | Inspection of code, configuration or document |
| `UNIT` | No vehicle, no link |
| `INTEGRATION` | MockLink, in the existing harness |
| `BENCH` | Real flight-controller or radio hardware |
| `HIL` | Hardware in the loop, companion present |
| `DEMO` | Observed operation by a person |
| `AUDIT` | A process check at a defined point |

## Per-module verification

| Module | Primary method | Gate | Verification objective | Blocked by |
| --- | --- | --- | --- | --- |
| `M-Plugin` | `UNIT` + `DEMO` | `G-IMPL` | The overlay is detected, sources reach the target, the plugin substitutes, the engine hook installs, the self-check fails visibly | Build host |
| `M-Config` | `UNIT` | `G-IMPL` | Aerora settings defaults apply; auto-upload is off; delete-after-upload is locked off | Build host |
| `M-Capability` | `UNIT` + `INTEGRATION` | `G-INTEG` | The eight resolution rules hold; unknown is not absent; an unrecognised identifier is retained and ignored | Q3-009 for content |
| `M-Vehicle` | `INTEGRATION` | `G-INTEG` | Standard families handled; parameters are Facts only; no shadowing setting exists | Build host |
| `M-Firmware` | `UNIT` | `G-IMPL` | The factory registers, is selected, and is asserted in a deployed build | Build host |
| `M-Telemetry` | `UNIT` | `G-IMPL` | Freshness stamped at parse; staleness per class; unknown never zero; **two vehicles do not share groups** | **Q4-002** for multi-vehicle |
| `M-Authority` | `UNIT` + `INTEGRATION` | `G-INTEG` | Eight states tracked; accepted-not-effective surfaced; stale never retried; no station-initiated failsafe path | Q-007 for content |
| `M-Mission` | `INTEGRATION` | `G-INTEG` | Plan file authoritative; unverified upload presented as unverified; resume warns unconditionally; no replay path | Build host |
| `M-Payload` | `UNIT` + `BENCH` | `G-INTEG` | State never inferred; stop separate from state; **no actuator path exists** | Q-023 for content |
| `M-Power` | `UNIT` + `BENCH` | `G-INTEG` | No transition inferred; no energy estimation; watchdog separate from power | Q-024 for content |
| `M-LinkState` | `INTEGRATION` + `BENCH` | `G-INTEG` | All-stale is a distinct state; local observation never promoted over the vehicle's | **Q3-001** for the radio |
| `M-Autonomy` | `UNIT` + `HIL` | `G-INTEG` | **No outbound path**; absent and silent distinguished; acceptance never inferred | Q3-005 |
| `M-Identity` | `UNIT` + `BENCH` | `G-INTEG` | Identity opaque and unparsed; unidentified attribution explicit; no binding carried across a reconnection | Q-016 |
| `M-Security` | `UNIT` + `AUDIT` | `G-RELEASE` | Offline validation primary; gate before the wire; random raw keys; **key storage unresolved** | **F3.1-01** |
| `M-Authz` | `UNIT` | `G-IMPL` | Empty role set denies everything; unreachable gate denies; gated set declared not derived | **F3.1-04** for a deployable default |
| `M-Audit` | `UNIT` + `INTEGRATION` | `G-RELEASE` | Unconditional record; local before upload; drops accounted; **integrity unresolved** | **F3.1-03** |
| `M-Cloud` | `INTEGRATION` | `G-INTEG` | All-network-blocked operation; no listener; no main-thread network call; per-feature degradation | Build host |
| `M-Ui` | `DEMO` + `INTEGRATION` | `G-RELEASE` | State classes render by class not by screen; six indicator pairs distinguishable; hide versus disable | **F3.1-05**, Q-040 |
| `M-Mavlink` | `UNIT` + `BENCH` | `G-INTEG` | Standard handling unbroken; a vetoed message is logged; unknown enumeration presented raw | Dialect for Aerora families |

## The twelve architectural properties

All `UNIT`, all `Unit`-labelled so they run in the default gate.

| # | Property | New in Phase 4 | Closes |
| --- | --- | --- | --- |
| 1 | The Autonomy module emits no MAVLink | No | `AERORA-TEST-REQ-004` |
| 2 | The Aerora firmware plugin is selected | No | R-010 |
| 3 | No fact group is created after vehicle construction | No | R-027 |
| 4 | No listening socket is open | No | R-016, Q-027 |
| 5 | Presentation depends on domain only | **Yes** | R3-001 |
| 6 | Domain reaches QGC only through the extension layer | **Yes** | R3-001 |
| 7 | Payload and mission are mutually independent | **Yes** | R3-001 |
| 8 | Cloud depends on security, never the reverse | No | `AERORA-AWS-REQ-012` |
| 9 | No L3 module references a product profile | **Yes** | Dependency rule 5 |
| 10 | No ground-station path reaches a payload actuator | **Yes** | R3-007, `IF-PAY-015` |
| 11 | No ground-station failsafe-initiation entry point exists | **Yes** | R3-007, `IF-AUT-012` |
| 12 | Two vehicles do not share fact group objects | **Yes** | **R4-001** |

Seven of twelve are new. Properties 5 to 7 close R3-001, properties 10 and 11 are the two R3-007
conversions, and property 12 is the test that would have caught R4-001.

## Build-bootstrap verification

Eleven items, none reached.

| # | Objective | Method | Evidence required | State |
| --- | --- | --- | --- | --- |
| 1 | Tool versions recorded | `STATIC` | Compiler, Qt, CMake, Ninja, GStreamer versions | **Not reached** |
| 2 | Baseline configures | `DEMO` | Configure log, exit code 0 | **Not reached** |
| 3 | Baseline compiles and links | `DEMO` | Build log, warning and error counts, duration | **Not reached** |
| 4 | Baseline starts | `DEMO` | Clean launch and exit | **Not reached** |
| 5 | Baseline test suite passes | `INTEGRATION` | `ctest -L "Unit\|Integration"` output | **Not reached** |
| 6 | Overlay detected | `DEMO` | Configure log reports the custom directory | **Not reached** |
| 7 | Overlay sources reach the target | `STATIC` | A deliberate error in a bootstrap file fails the build | **Not reached** |
| 8 | Aerora QML module imports | `DEMO` | Self-check passes | **Not reached** |
| 9 | Aerora plugin registers and is selected | `UNIT` + `DEMO` | Property test 2 and the self-check | **Not reached** |
| 10 | Upstream behaviour unregressed | `INTEGRATION` | `ctest -L Unit` identical to item 5 | **Not reached** |
| 11 | Overlay disables cleanly | `DEMO` | Configure succeeds with the overlay absent, producing stock QGC | **Not reached** |

Item 11 is the one most often skipped and the one that makes bisecting against upstream possible.

## Gates

| Gate | Requires | Currently |
| --- | --- | --- |
| `G-ARCH` | **Q4-002** decided; Q3-001 answered | **Not met** |
| `G-IMPL` | Build host; baseline build; overlay proof; Q-035; Q-020 residual; Q4-001 | **Not met** |
| `G-INTEG` | `UNIT` and `INTEGRATION` suites passing; the first three bench observations | **Not met** |
| `G-RELEASE` | All `AUDIT` and `DEMO` items; F3.1-01 and F3.1-03 resolved; dependency pins immutable; platform signing | **Not met** |

`G-ARCH` is listed as not met because Q4-002 was found in this phase and may change the standing
core-edit position. That is a new entry on an otherwise ratified gate, and it is the most consequential
single line in this document.

## Sequencing: what to verify first

Ordered by cost-to-value once a build host exists.

| # | Action | Settles | Cost |
| --- | --- | --- | --- |
| 1 | Baseline build and `ctest -L Unit` | That the toolchain and the upstream tree are sound | One build |
| 2 | One trivial Aerora test in the overlay | Q-035, R-021 | One file |
| 3 | A probe resource at an existing core `:/json` path | **Q4-001**, R4-005 | One file |
| 4 | Overlay structural proof, seven properties | The whole extension strategy | Eight small artefacts |
| 5 | Two-vehicle fact-group isolation test | **R4-001** — confirms the hazard concretely | One test |
| 6 | `M-Audit` skeleton with the twelve property tests | R3-001, R3-007 | The first real module |
| 7 | Radio bench observation | **Q3-001**, ADR-002 | One connection, **independent of 1 to 6** |

Item 7 needs no build and no ground station. It should be done by whoever holds the hardware, in
parallel, starting now.

## What this plan does not do

| Does not | Reason |
| --- | --- |
| Record any result | Nothing has been built or run |
| Name test file paths or fixtures | Implementation detail; the method and objective are the contract |
| Set coverage targets | Coverage of code is not coverage of risk |
| Claim any test exists | **Zero tests written** |
| Estimate durations | An estimate on an unbuilt project is a guess with a number attached |

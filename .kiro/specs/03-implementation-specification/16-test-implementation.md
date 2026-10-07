# Test Implementation

Per-module test architecture, the requirement-to-evidence chain, and the tests that are writable
before the dialect exists. **No test was written and none was run.**

## Framework: inherited unmodified

`AERORA-TEST-REQ-009`. Aerora adds no base class, no label and no runner.

| Element | Location | Aerora use |
| --- | --- | --- |
| `UnitTest` | `test/UnitTestFramework/UnitTest.h:262` | Tests with no vehicle |
| `VehicleTest` | `BaseClasses/VehicleTest.h:60` | MockLink vehicle per test; `vehicle()`, `mockLink()` |
| `ParameterTest` | `BaseClasses/ParameterTest.h:26` | Parameter suites |
| `MissionTest`, `OfflineMissionTest` | `BaseClasses/MissionTest.h:32`, `:93` | Mission suites |
| `MockLink` | `src/Comms/MockLink/MockLink.h:30` | Vehicle simulation. Note: ships in `src/`, not `test/` |
| Registration | `cmake/QGCTest.cmake:156` | `add_qgc_test(name LABELS ...)` |
| Labels and timeouts | `QGCTest.cmake:140-154`, `:190-201` | `Unit` 60 s, `Integration` 120 s, `Slow` 180 s |
| Isolation | `QGCTest.cmake:203-229` | Offscreen QPA, software Quick, per-test `TMPDIR` |

### The linkage consequence

Tests are **not separate binaries**. `QGCTest.cmake:179` invokes
`$<TARGET_FILE:${CMAKE_PROJECT_NAME}> --unittest:<name> --allow-multiple`.

An Aerora test is therefore reachable only if its source is compiled **into the application target** —
the same linkage rule as the firmware plugin factory. That is the mechanism behind Q-035, and it is
the first thing the overlay structural proof must demonstrate.

| Gate | Question | Risk |
| --- | --- | --- |
| Q-035 | Are Aerora tests in the overlay enumerated by `ctest` at all? | R-021 |
| — | Does the `MockLink` resource lock hold with Aerora integration tests added? | Noted in the Phase 3 plan |

Both are `G-IMPL`, and Q-035 is cheap: place one trivial Aerora test in the overlay, build, and see
whether `ctest` lists it.

### Label discipline

The default filter is the contract: `-L "Unit|Integration" -LE "Flaky|Network"`.

| Consequence | Detail |
| --- | --- |
| An Aerora test with any other label **does not run by default** | — |
| An Aerora test labelled `Flaky` or `Network` is excluded from the gate | — |
| Architectural-property tests carry `Unit` | `AERORA-TEST-REQ-006` — so they run in the default gate |

A property test in an optional job stops running the first time that job is slow.

## Architectural-property tests

`AERORA-TEST-REQ-004` names four. Phase 4 extends the set to twelve, reusing the same mechanism
rather than adding a new one.

| # | Property | Source | Status |
| --- | --- | --- | --- |
| 1 | The Autonomy module emits no MAVLink | `AERORA-TEST-REQ-004` | `IMPLEMENTATION-PLANNED` |
| 2 | The Aerora firmware plugin is the selected one | `AERORA-TEST-REQ-004` | `IMPLEMENTATION-PLANNED` |
| 3 | No fact group is created after vehicle construction | `AERORA-TEST-REQ-004` | `IMPLEMENTATION-PLANNED` |
| 4 | No listening socket is open | `AERORA-TEST-REQ-004` | `IMPLEMENTATION-PLANNED` |
| 5 | Presentation depends on domain only | **New** — R3-001 | `IMPLEMENTATION-PLANNED` |
| 6 | Domain reaches QGC only through the extension layer | **New** — R3-001 | `IMPLEMENTATION-PLANNED` |
| 7 | Payload and mission are mutually independent | **New** — R3-001 | `IMPLEMENTATION-PLANNED` |
| 8 | Cloud depends on security, never the reverse | Existing, `AERORA-AWS-REQ-012` | `IMPLEMENTATION-PLANNED` |
| 9 | No L3 platform module references a product profile | **New** — rule 5 | `IMPLEMENTATION-PLANNED` |
| 10 | No ground-station path reaches a payload actuator | **New** — R3-007, `IF-PAY-015` | `IMPLEMENTATION-PLANNED` |
| 11 | No ground-station failsafe-initiation entry point exists | **New** — R3-007, `IF-AUT-012` | `IMPLEMENTATION-PLANNED` |
| 12 | Two vehicles do not share fact group objects | **New** — the Phase 4 fact-group finding | `IMPLEMENTATION-PLANNED` |

Properties 5 to 7 close R3-001. Properties 10 and 11 are the two R3-007 conversions from inspection
to assertion — chosen because they are the two prohibitions whose violation would matter most.
Property 12 is the test that would have caught the fact-group hazard.

## Per-module test matrix

| Module | Unit | MockLink integration | QML | Network-blocked | Authorisation | Freshness | Capability | Regression |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `M-Plugin` | ✓ | ✓ | — | — | — | — | — | ✓ |
| `M-Config` | ✓ | — | — | ✓ | — | — | — | ✓ |
| `M-Capability` | ✓ | ✓ | — | — | — | — | ✓ | ✓ |
| `M-Vehicle` | ✓ | ✓ | — | ✓ | — | ✓ | ✓ | ✓ |
| `M-Firmware` | ✓ | ✓ | — | — | — | — | — | ✓ |
| `M-Telemetry` | ✓ | ✓ | ✓ | — | — | ✓ | ✓ | ✓ |
| `M-Authority` | ✓ | ✓ | ✓ | — | ✓ | ✓ | ✓ | ✓ |
| `M-Mission` | ✓ | ✓ | ✓ | ✓ | ✓ | — | ✓ | ✓ |
| `M-Payload` | ✓ | ✓ | ✓ | — | ✓ | ✓ | ✓ | ✓ |
| `M-Power` | ✓ | ✓ | ✓ | — | — | ✓ | ✓ | ✓ |
| `M-LinkState` | ✓ | ✓ | ✓ | — | — | ✓ | — | ✓ |
| `M-Autonomy` | ✓ | ✓ | ✓ | — | — | ✓ | ✓ | ✓ |
| `M-Identity` | ✓ | ✓ | — | — | — | ✓ | — | ✓ |
| `M-Security` | ✓ | ✓ | ✓ | ✓ | ✓ | — | — | ✓ |
| `M-Authz` | ✓ | ✓ | — | ✓ | ✓ | — | — | ✓ |
| `M-Audit` | ✓ | ✓ | — | ✓ | ✓ | ✓ | — | ✓ |
| `M-Cloud` | ✓ | ✓ | — | ✓ | — | ✓ | — | ✓ |
| `M-Ui` | ✓ | ✓ | ✓ | — | ✓ | ✓ | ✓ | ✓ |
| `M-Mavlink` | ✓ | ✓ | — | — | — | — | — | ✓ |

### Degraded and network-blocked test

`AERORA-TEST-REQ-005`. One test, `Integration`-labelled, MockLink-backed, with all network access
blocked. It must complete: connect, parameter load, mission plan, upload, execution, and a complete
local flight record.

| What it defends | Why a policy document would not |
| --- | --- |
| A new cloud dependency on the flight path | It fails the test rather than passing review |
| An accidental re-enablement of log auto-upload | The blocked connection attempt is the signal |
| The eight named hidden-dependency traps | Each would manifest as a blocked call |

This is the single highest-value test in the set, and it is cheap to run.

## Tests writable before the dialect exists

The useful question for sequencing.

| Category | Writable now | Needs a dialect | Needs hardware |
| --- | --- | --- | --- |
| Architectural properties | **12** | 0 | 0 |
| Audit record production, ordering, correlation, drops | **~20** | 0 | 0 |
| Authorisation abstraction | **10** | 0 | 0 |
| State-class rendering | **7** | 0 | 0 |
| Freshness and staleness | **6** | 0 | 0 |
| Capability model rules | **6** | 0 | 0 |
| Command tracker, eight states | **8** | 0 | 0 |
| Standard MAVLink handling | **~11** | 0 | 0 |
| Mission plan authority and constraint | **9** | 0 | 0 |
| Aerora family parsing | 0 | **~20** | 0 |
| Vehicle-side contract confirmation | 0 | 0 | **~26** |
| Autonomy proposal path end to end | 0 | 0 | **4** |

Roughly 90 tests are writable with no dialect and no aircraft. That is the answer to "what does the
team do first": build `M-Audit`, `M-Authz` and `M-Telemetry`, each of which is fully testable against
the mock link.

## Traceability chain

Required form, maintained in [20-phase-4-traceability.md](20-phase-4-traceability.md):

```text
Requirement
   ↓
Interface
   ↓
Implementation module
   ↓
QGC seam
   ↓
Test
   ↓
Verification evidence
```

| Link | Source | Complete? |
| --- | --- | --- |
| Requirement → Interface | Phase 3 traceability, 120 of 120 referenced | **Yes** |
| Interface → Module | [04-cpp-implementation-map.md](04-cpp-implementation-map.md), 240 of 240 | **Yes** |
| Module → Seam | [03-qgc-extension-map.md](03-qgc-extension-map.md), 12 seams, all verified | **Yes** |
| Seam → Test | This document, per module | **Yes** |
| Test → Evidence | — | **No. Nothing has been run** |

The chain is complete to the fifth link and empty at the sixth. That is the correct state for a
phase with no build.

## What must not be claimed

| Rule | Compliance |
| --- | --- |
| No test is marked passing without execution | **0 tests executed** |
| No interface is `VERIFIED` | **0 verified** |
| Coverage targets | **None set.** Coverage of code is not coverage of risk |
| Test counts above are estimates of scope | Not of effort, and not of completion |

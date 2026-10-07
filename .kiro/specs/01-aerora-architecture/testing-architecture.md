# Testing Architecture

What Aerora tests, how it attaches to QGroundControl's test framework, and the small set of tests
that verify architectural properties rather than behaviours.

## What upstream provides

QGroundControl's test framework is in good shape and Aerora uses it unchanged.

| Element | Detail |
| --- | --- |
| Registration | Tests self-register through a macro; there is no central list to update, and the runner enumerates at runtime |
| Base classes | Eleven, from `UnitTest` up through `VehicleTest`, `ParameterTest`, `MissionTest` and `OfflineMissionTest` |
| Labels | Twelve `TestLabel` values; CTest labels are a superset with a few filter-only names |
| Registration in CMake | `add_qgc_test(Name LABELS ... [TIMEOUT n] [RESOURCE_LOCK r] [SERIAL])` |
| Timeouts | 60 s for `Unit`, 120 s for `Integration`, 180 s for `Slow`, 90 s default |
| Wait helpers | A `TestTimeout` namespace, `QTRY_*_WITH_TIMEOUT` wrappers, and a condition-polling helper |
| Vehicle simulation | `MockLink`, with CTest resource locking so integration tests do not collide |
| QML | A dedicated out-of-process Qt Quick test executable, plus an in-process structure validator |
| Output | JUnit XML, coverage instrumentation, and four sanitizers |
| Stress | A repeat-count option on the binary runner |

Three prohibitions are enforced by the repository's own hooks and apply to Aerora tests equally: no
`Q_ASSERT` in production code, no `QTest::ignoreMessage` in tests, and no fixed-delay waits.

The third is worth understanding rather than merely obeying. A fixed delay encodes an assumption
about timing that passes locally and fails on a loaded continuous-integration machine. The
condition-polling helpers express the actual intent — wait until this is true — and fail with a
useful message when it never becomes true.

## Where Aerora tests live

`OPEN`, as **Q-035**, and the uncertainty is specific.

Aerora code lives in the overlay, per `AERORA-BUILD-REQ-006`. The test tree is a separate
subdirectory added after the overlay, and the preprocessor definition that marks a test-capable
build is applied between the two. Whether an Aerora test registered from overlay sources is
reachable by the harness therefore depends on build-ordering details this architecture has not been
able to verify, because the host it was written on cannot build the project.

| Candidate | Consequence |
| --- | --- |
| Aerora tests in the overlay's source set | Keeps all Aerora code in the overlay; reachability needs confirming |
| Aerora tests in an Aerora-owned subdirectory of the test tree | Certainly reachable; puts a small amount of Aerora code outside the overlay |

How to resolve it: configure with testing enabled, build, and check that an Aerora test appears in
the registered-test listing. That is a one-hour task once a build host exists, and it should be the
first thing the scaffold proves.

The architecture does not choose between the candidates, because choosing without the build result
would be a guess presented as a decision. It does record that the second candidate requires a
documented exception to `AERORA-BUILD-REQ-006` if taken.

## Test categories

### Architectural-property tests

These are the tests worth writing first. Each verifies something the architecture asserts about
itself, which no behavioural test would catch.

| Test | Asserts | Requirement |
| --- | --- | --- |
| The Autonomy module emits no MAVLink | The read-only companion boundary is structural | `AERORA-GPU-REQ-003` |
| The Aerora firmware plugin is the one selected for the Aerora firmware class | No silent factory conflict | `AERORA-GCS-REQ-007` |
| The Aerora factory is registered at all | The self-registering global was not discarded by the linker | `AERORA-GCS-REQ-009` |
| No FactGroup is created after vehicle construction | The CSV column alignment constraint holds | `AERORA-GCS-REQ-022` |
| No listening socket is open | No inbound network surface | `AERORA-AWS-REQ-014` |
| Automatic log upload is disabled in the Aerora build | No third-party transmission by default | `AERORA-GCS-REQ-023` |
| The Security module has no reference to the Cloud module | The dependency rule holds | `AERORA-AWS-REQ-012` |
| Every Aerora QML module URI imports | No missing or misnamed module | `AERORA-UI-REQ-010` |
| Every Aerora override resource resolves to Aerora content | No silent override failure | `AERORA-UI-REQ-007` |

The first is the most valuable test in the whole set. It is the only one that verifies a property
rather than a behaviour, and it is what keeps the read-only companion guarantee true twelve months
and forty commits from now. A behavioural test would check that the module does not send a command
today; this one checks that it cannot.

Two of the nine — the override-resolution and module-import checks — are the same assertions the
startup self-check performs. Running them in both places is deliberate: the test catches it in
continuous integration, and the self-check catches a packaging failure that only manifests in a
deployed build.

### The all-network-blocked test

One test carries disproportionate weight, because it defends the cloud architecture's central
property against the eight named hidden-dependency traps.

With all network access blocked, the GCS must complete: connect to a vehicle, load parameters, plan
a mission, upload it, run it, and produce a complete local flight record.

| Property | Why the test is the right mechanism |
| --- | --- |
| A new cloud dependency on the flight path fails this test | A policy document would not notice |
| An accidental re-enablement of log auto-upload is observable | The blocked connection attempt is the signal |
| It is cheap to run | Integration-labelled, `MockLink`-backed |

This is the test that makes `AERORA-AWS-REQ-005` and `AERORA-AWS-REQ-006` enforceable rather than
aspirational.

### Behavioural tests

| Area | Tests |
| --- | --- |
| Companion integration | A non-autopilot component heartbeat creates no second vehicle; companion messages reach the Aerora FactGroup; companion traffic does not distort the link loss counter |
| Freshness | Values go stale at their threshold; a stale value is never presented as current; absent and silent are distinguishable |
| Command states | Each of the eight states is reachable; accepted-without-effective is surfaced; a stale command is not retried |
| Authority | A requested mode is never displayed as effective; a pending confirmation is visible; a reported failsafe is not locally cleared |
| Payload | Emergency stop is distinct from payload state; a payload command accepted but not effective is surfaced |
| Mission | Plan-file authority: a downloaded mission is not reconstructed as Aerora structure; an unverified upload is presented as unverified |
| Audit | An audit record is written when not armed; a denial is recorded with operator and reason; every command transition is recorded |
| Identity | Records before identity arrives are buffered; a connection reporting no identity yields unidentified-vehicle records |
| Link state | Vehicle-reported active link is distinguished from GCS-local observation; all-stale is distinct from GCS-observed loss |
| Settings defaults | Each of the five Aerora default changes is in effect |

### Tests that cannot be written yet

| Blocked test | Blocked by |
| --- | --- |
| Payload state transition coverage | Q-023 — the enumeration does not exist |
| Failsafe action coverage | Q-007 — the actions are not defined |
| Authorisation matrix coverage | Q-029 — the role set is not defined |
| Override-resolution assertion detail | Q-020 — what "resolved content" is observable as is unknown |
| Corroboration window boundaries | Q-036 — the windows are not defined |

Listing these matters. A test plan that omits them looks complete; a test plan that names them
shows exactly what five answers would unlock.

## The startup self-check

The self-check is a deployed-build verification mechanism, not a test, and it exists because the
failures it catches are invisible rather than loud.

| Assertion | Catches |
| --- | --- |
| Every Aerora override resource exists at its aliased location | A build-system aliasing mistake |
| Every Aerora QML module URI is importable | A missing or misnamed module |
| The resolved content at each overridden location is the Aerora content | The resource-shadowing precedence failure, Q-020 |
| The selected firmware plugin is the Aerora one | The static-initialisation-order factory conflict, R-010 |
| No listening socket is open | An inadvertent inbound surface |

A failed self-check is a visible startup error. An interface quietly missing its Aerora layer is
worse than one that refuses to start: an operator cannot distinguish "the payload panel is absent"
from "the payload has nothing to report".

## A tool worth building early

Three of the five `AUDIT`-verified requirements are merge-time procedures: a drift review of
overridden files, a seam signature review, and the override inventory's accuracy.

All three are partially automatable by a small tool that compares each overridden file against its
recorded fork-point commit and reports what upstream has changed since. That converts a procedure
people forget into a check that fails.

| Requirement | Converted from | To |
| --- | --- | --- |
| `AERORA-UI-REQ-005` | `INSPECT` | `TEST` — the inventory is complete and every fork point is a real commit |
| `AERORA-UI-REQ-006` | `AUDIT` | `TEST` — no overridden file has upstream changes since its fork point, or the change is acknowledged |
| `AERORA-BUILD-REQ-007` | `AUDIT` | Partially — drift becomes mechanical; seam semantics stay human |

This is the highest-leverage tooling investment identified in this phase, and it is small.

## Continuous integration

Aerora inherits the existing workflow. Two additions:

| Addition | Reason |
| --- | --- |
| The all-network-blocked test runs on every change | It defends a property that erodes by accretion |
| The architectural-property tests carry the `Unit` label | So they run in the default gate, not an optional job |

The second matters more than it sounds. An architectural-property test in an optional job is a test
that stops running the first time the optional job is slow.

## What could not be verified in this phase

Stated plainly, because a testing document that omits it would be misleading.

The host this architecture was written on has no build toolchain: no CMake, no Ninja, no compiler,
no Qt build, and no build directory. **No test in this document has been executed, and none of the
architecture's runtime claims have been verified by running anything.**

| Claim type | Verified how |
| --- | --- |
| Source structure, seams, signatures, line references | Direct reading of the checkout |
| Build configuration and option wiring | Direct reading of the CMake files |
| Runtime behaviour | **Not verified** — reasoned from the source |
| Resource alias precedence | **Not verified** — Q-020 |
| Overlay test reachability | **Not verified** — Q-035 |

The first two categories are solid: they are facts about files, with locations. The last three are
the honest limit of a read-only analysis, and the first three things a build host should be pointed
at.

## Requirements derived

| ID | Statement |
| --- | --- |
| `AERORA-TEST-REQ-004` | Architectural-property tests shall verify that the Autonomy module emits no MAVLink, that the Aerora firmware plugin is selected, that no FactGroup is created after vehicle construction, and that no listening socket is open |
| `AERORA-TEST-REQ-005` | A test shall verify full local operation — connect, parameter load, mission plan, upload, execution and local flight record — with all network access blocked |
| `AERORA-TEST-REQ-006` | Architectural-property tests shall run in the default continuous-integration gate and shall not be placed in an optional job |
| `AERORA-TEST-REQ-007` | A startup self-check shall assert override resource resolution, Aerora QML module importability, firmware plugin selection and the absence of a listening socket, and shall fail visibly |
| `AERORA-TEST-REQ-008` | Overridden-file drift shall be checked mechanically against recorded fork-point commits rather than by procedure alone |
| `AERORA-TEST-REQ-009` | Aerora tests shall use the upstream test framework, base classes and labels without modification |

Full records in [requirements.md](requirements.md), which carries these six alongside the 114
derived in the other architecture documents.

# CI and Build Architecture

How the Aerora overlay reaches continuous integration, which gates it must pass, and what the
repository already provides. **Nothing was configured; no workflow file was created.**

## What upstream already provides

| Element | Evidence | Aerora use |
| --- | --- | --- |
| A custom-build workflow that proof-builds an overlay | Configures with an explicit `QGC_CUSTOM_DIR`, asserted by a repository test | **Point it at `aerora`** |
| The overlay is exercised **without renaming** the directory | Same | Aerora keeps its own directory name; `custom-example` stays intact as a reference |
| Composite actions for Qt install, cmake configure, cmake build, unit tests, caching | `.github/actions/` — 40+ actions | Reused unchanged |
| `build-config.json` as the single version source | `cmake/BuildConfig.cmake:41-46` reads it | Never duplicate a version |
| Label-filtered test runs | `.github/actions/run-unit-tests/action.yml` | Reused unchanged |
| Pre-commit hook set | `.pre-commit-config.yaml` | Reused unchanged |

Aerora adds **no new CI mechanism**. It adds one overlay target to an existing matrix and three
checks.

## The Aerora CI gates

| # | Gate | Mechanism | New? | Status |
| --- | --- | --- | --- | --- |
| 1 | Baseline upstream still builds | The existing platform workflows | No | `IMPLEMENTATION-PLANNED` |
| 2 | The Aerora overlay builds | The existing custom-build workflow, `QGC_CUSTOM_DIR=aerora` | No | `IMPLEMENTATION-PLANNED` |
| 3 | The overlay can be disabled cleanly | A configure run with the overlay absent | **Yes** | `IMPLEMENTATION-PLANNED` |
| 4 | Default-gate tests pass | `ctest -L "Unit\|Integration" -LE "Flaky\|Network"` | No | `IMPLEMENTATION-PLANNED` |
| 5 | Architectural-property tests pass | Twelve properties, `Unit`-labelled | Partly | `IMPLEMENTATION-PLANNED` |
| 6 | All-network-blocked test passes | `AERORA-TEST-REQ-005` | **Yes** | `IMPLEMENTATION-PLANNED` |
| 7 | Upstream drift check passes | The manifest check in [18-upstream-drift-detection.md](18-upstream-drift-detection.md) | **Yes** | `IMPLEMENTATION-PLANNED` |
| 8 | Lint and format | `pre-commit run --all-files` | No | `IMPLEMENTATION-PLANNED` |
| 9 | Dialect and dependency pins are immutable | A release-gate check | **Yes** | `CONTRACT-ONLY` |

Gates 3, 6 and 7 are the Aerora additions. Gate 3 is the cheapest and the most often forgotten: an
overlay that cannot be switched off cannot be bisected against upstream.

### Gate placement

| Gate | Runs on | Blocking |
| --- | --- | --- |
| 1, 2, 4, 5, 8 | Every change | **Yes** |
| 3, 6, 7 | Every change | **Yes** — `AERORA-TEST-REQ-006`'s reasoning applies to all three |
| 9 | Release candidates | **Yes** at `G-RELEASE` |

Putting gates 6 and 7 in the default gate rather than a nightly job is deliberate. Both defend
properties that erode by accretion, and an erosion check that runs weekly finds the erosion a week
late.

## Werror

`QGC_ENABLE_WERROR` defaults **ON** — `cmake/CustomOptions.cmake`. A warning in Aerora code is an
error.

| Consequence | Assessment |
| --- | --- |
| Aerora code must be warning-clean from the first commit | Correct, and cheaper than retrofitting |
| Qt and third-party headers are excluded by the existing configuration | Inherited |
| Do not disable it for the overlay | A warning-tolerant overlay inside a warning-free tree is how the tree stops being warning-free |

## Caching

| Cache | Purpose | Aerora consideration |
| --- | --- | --- |
| Qt installation | Avoids a 4–6 GB download per run | Keyed on version plus module set, which the repository's installer already computes |
| CPM source cache | Pinned dependency sources | **Must be keyed on the pinned commits.** A cache hit on a moving reference is how R-011 becomes invisible |
| Compiler cache | `QGC_USE_CACHE` defaults ON | Inherited |
| moc cache | `QGC_USE_MOCCACHE` defaults ON | Inherited |

The CPM row is the one with a correctness consequence rather than a speed one.

## Platform scope

| Platform | In the repository's workflow set | Aerora position |
| --- | --- | --- |
| Windows | Yes | **The current host.** Primary |
| Linux | Yes | Likely, product decision |
| macOS | Yes | Product decision |
| Android | Yes | Product decision; adds the Java and NDK dependency chain |
| iOS | Yes | Unlikely |

Q-032 governs the platform set, and it is `IMPL-BLOCKING` because platform code signing differs per
platform and the Windows signing gap is the known case — R-015.

Phase 4 does not choose the platform set. It records that **each additional platform multiplies the
dependency footprint**, and that the Android chain alone adds the SDK, NDK and a Java requirement
that the desktop build does not need.

## Release artefacts

| Obligation | Requirement | Status |
| --- | --- | --- |
| Release artefacts are signed and versioned | `AERORA-SEC-REQ-020` | `CONTRACT-ONLY` — Q-032, R-015 |
| The dialect is pinned to an immutable commit | `AERORA-BUILD-REQ-008` | `CONTRACT-ONLY` — no dialect yet |
| Every third-party dependency is pinned | `AERORA-SEC-REQ-020` | **Two currently track moving references** — R-011 |
| No signature verification on updates or firmware | — | R-007, release-gated |
| Per-dependency licence position | — | **Unverified** — R-020, Q-005 |
| A deployed station reports six version values | `AERORA-BUILD-REQ-010` | `IMPLEMENTATION-PLANNED` |

### The six version values

Build version, upstream fork point, dialect commit, settings schema version, applied configuration
version, dependency versions. All six are implementable today and none needs the dialect. A
compatibility matrix is useless if a station cannot identify itself.

## The settings-schema version as a release obligation

| Property | Value |
| --- | --- |
| Current value | `QGC_SETTINGS_VERSION` `"9"` |
| Behaviour | A mismatch clears the **entire** settings store before any subsystem starts |
| Coupling | Signing keys share that store |
| Release obligation | A bump requires a recorded fleet reprovisioning plan — `AERORA-BUILD-REQ-009` |
| Verification | `AUDIT`. There is nothing to test |

This belongs in the CI document as well as the security one, because the value lives in the build
configuration and a routine build edit is how it gets incremented.

## Repository configuration

| Item | Current state | Required | Risk |
| --- | --- | --- | --- |
| `origin` fetch | Upstream QGroundControl | The Aerora repository | — |
| `origin` **push** | **Upstream QGroundControl** | The Aerora repository | **R-008** |
| `upstream` remote | Does not exist | Fetch-only | — |
| Fork point recorded | No | `9459a4b5...` | — |
| Submodules | None — no `.gitmodules` | Unchanged | — |
| Working tree | **Clean.** `git diff --stat HEAD` empty | Unchanged until implementation | — |

The push URL is the live hazard and is item 2 of the build-readiness gate.

## Merge cadence

| Item | State |
| --- | --- |
| Upstream merge cadence and owner | **Q-038, OPEN.** A process decision |
| Drift review at each merge | `AERORA-UI-REQ-006`, `AERORA-BUILD-REQ-007` |
| Seam signature review at each merge | `AERORA-BUILD-REQ-007`'s second half — **no mechanism**, R-014 |
| Mechanical drift check | `AERORA-TEST-REQ-008` — gate 7 above |

`AERORA-BUILD-REQ-007` has two halves and the second is the easy one to drop. A changed virtual
**signature** is a compile error. A changed virtual **semantic** is not, and only the seam review
catches it.

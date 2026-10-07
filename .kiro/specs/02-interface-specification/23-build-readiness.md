# Build Readiness

What must be present to configure, build, test and smoke-test the Aerora ground station, what the
current host actually has, and the repository and process obligations that must be settled before
implementation starts.

Every version and command in this document was read from the checkout at commit
`9459a4b57986f40df8bdc193d5453c0596fbb416`. Nothing is recalled.

## The current host cannot build

Measured, not assumed.

| Tool | Status |
| --- | --- |
| `cmake` | **Absent** |
| `ninja` | **Absent** |
| `cl` — MSVC compiler | **Absent** |
| `qmake` — any Qt installation | **Absent** |
| `just` — the canonical task runner | **Absent** |
| `pre-commit` | **Absent** |
| `uv` | Present |
| `python` | Present, 3.12 |
| `node` | Present |
| `git` | Present |
| `build/` directory | **Does not exist** |

**No build, configure, test, lint or static-analysis step has been executed in any phase of this
programme.** This is R-009, established in Phase 1 and unchanged. It is the reason every claim in
Phases 1 through 3 is a source-reading claim, and the reason
[22-phase-3-verification-plan.md](22-phase-3-verification-plan.md) records 240 method assignments and
zero results.

Qt alone is the material gap: the toolchain is a download, and Qt 6.11.1 with the required module
set is a large one.

## Required toolchain

`.github/build-config.json` is the declared source of truth for versions, and the root
`CMakeLists.txt` carries a comment requiring `cmake_minimum_required` to stay in sync with it.

| Component | Required | Where declared |
| --- | --- | --- |
| CMake | **3.25** minimum | `CMakeLists.txt:5`; `build.cmake_minimum_version` in `build-config.json` |
| C++ standard | **C++20**, required — not a preference | `cmake/Toolchain.cmake:17-18` |
| Qt | **6.11.1**, minimum **6.11.0** | `qt.version`, `qt.minimum_version` |
| Qt supported range | 6.11.0 to 6.11.1 inclusive | `QGC_QT_MAXIMUM_VERSION` is set from `qt.version` |
| Generator | Ninja, via the CMake presets | `justfile` presets |
| GStreamer | **1.28.4** default, **1.20.0** minimum | `gstreamer.version.default`, `gstreamer.version.minimum` |
| Python tooling | `uv`-managed, pinned by lockfile | `justfile` `build_tool` |

The Qt range is narrow by construction: maximum equals the pinned version, so a newer Qt is
rejected rather than tried. That is the correct behaviour for a product with a compatibility matrix
and it means **Qt upgrades are a deliberate act**, not a side effect of a developer's installation.

### Qt modules

Declared in `qt.modules`:

```text
qtgraphs qtlocation qtpositioning qtspeech qtmultimedia qtserialport
qtimageformats qtshadertools qtconnectivity qtquick3d qtsensors
qtscxml qtwebsockets qthttpserver
```

Two observations Aerora inherits rather than creates:

| Module | Note |
| --- | --- |
| `qtserialport` | The module ADR-002's serial-first transport strategy depends on. Required, present |
| `qthttpserver` | Linked for two type aliases; **no listener is ever created**. Q-027, R-016 as amended in Phase 2.1 |

`qthttpserver` being in the required module list is the practical cost of Q-027: a full HTTP server
implementation is a mandatory build dependency for two enumerations. `IF-CLD-003` specifies that
nothing crosses the inbound boundary, and `AERORA-TEST-REQ-004` plus `AERORA-TEST-REQ-007` assert it
at build time and at startup respectively, because the module's presence makes the assertion worth
making.

## Repository topology, and the problem with the current one

| Property | Current state | Required |
| --- | --- | --- |
| `origin` fetch | `https://github.com/mavlink/qgroundcontrol.git` | An Aerora repository |
| `origin` **push** | **The same upstream URL** | An Aerora repository |
| Upstream remote | None distinct | Upstream, **fetch-only** |
| Fork point recorded | No | `9459a4b57986f40df8bdc193d5453c0596fbb416` |
| Submodules | None — no `.gitmodules` | Unchanged |
| Working tree modifications | **None.** `git diff --stat HEAD` is empty | Unchanged until implementation |

The push URL is the live hazard. This is R-008, and the first implementation commit against the
current configuration would target upstream QGroundControl.

| Obligation | Requirement |
| --- | --- |
| Aerora maintains its own repository, upstream configured fetch-only | `AERORA-BUILD-REQ-004` |
| The fork-point commit is recorded and matches the analysed commit | `AERORA-BUILD-REQ-005` |

Both are `INSPECT` requirements and both are entry criteria for implementation rather than release
gates. The fork point matters beyond hygiene: if it drifts, the Phase 1 and Phase 2 documents stop
describing the code they were written against, and 89 `GCS-VERIFIED` evidence statements quietly
become `UNKNOWN`.

## Overlay structure

The overlay is activated by the build option `QGC_CUSTOM_DIR` — a path relative to the source root,
default `custom`, validated with `FATAL_ERROR` against empty, absolute and `..`-containing values.
The overlay contract requires **both** a `CMakeLists.txt` and a `cmake/CustomOverrides.cmake` in that
directory. All `GCS-VERIFIED`; detail in
[plugin-architecture.md](../00-qgc-baseline-analysis/plugin-architecture.md) and
[build-system.md](../00-qgc-baseline-analysis/build-system.md).

Continuous integration proof-builds an overlay without renaming the directory, so Aerora can keep
its own overlay directory name and leave the upstream reference overlay intact.

| Rule | Requirement | Failure mode if broken |
| --- | --- | --- |
| All Aerora code and resources in a single overlay directory, pointed at by `QGC_CUSTOM_DIR` | `AERORA-BUILD-REQ-006` | The core-edit count stops being verifiable |
| C++ relying on static self-registration compiled into the **main application target**, not an Aerora static library | `AERORA-GCS-REQ-009` | A self-registering global in a static library **can be discarded by the linker, with no diagnostic** |
| Aerora QML delivered as named QML modules, exported as libraries | `AERORA-UI-REQ-010` | Name collisions with upstream controls |

The second and third rules point in opposite directions and both are correct, which is why they are
stated together. Self-registering C++ goes into the **target**; QML modules go into Aerora
**libraries**. Putting either in the other's place fails — quietly in one direction and loudly in the
other, and the quiet direction is the firmware plugin factory not registering at all.

That quiet failure is R-010, and `AERORA-TEST-REQ-004` asserts the Aerora firmware plugin is the
selected one precisely because the linker gives no warning.

## Configure, build, test

The `just` recipes are the canonical workflow and are what CI invokes. Reproduced from the
`justfile` as read.

| Step | Command |
| --- | --- |
| Configure | `just configure` — runs `tools/configure.py --preset default -B build -t Debug`, after submodule setup |
| Build | `just build` — `cmake --build --preset default --parallel <jobs>` |
| Release configure and build | `just release` |
| Test | `just test` — `ctest` with the preset, `--no-tests=error`, and the label filters below |
| Lint | `just lint` — `pre-commit run --all-files` |
| Python tooling tests | `just test-python` |
| Lint plus all tests | `just check` |
| Formatting, apply | `just format-fix` |
| Static analysis | `just analyze` |
| Coverage | `just coverage` |
| Resolved versions | `just info` |

The test recipe in full, as read from the `justfile`:

```text
ctest --preset default --build-config <type> --parallel <jobs> --no-tests=error
      -L "Unit|Integration" -LE "Flaky|Network"
```

### Test label contract

The default label filter is the contract, not a convenience:

| Filter | Value | Consequence for Aerora |
| --- | --- | --- |
| Include | `Unit\|Integration` | An Aerora test with any other label **does not run by default** |
| Exclude | `Flaky\|Network` | An Aerora test labelled either is excluded from the default gate |

This is the mechanism behind `AERORA-TEST-REQ-006`. The architectural-property tests carry the
`Unit` label so they run in the default gate; an architectural-property test labelled anything else
stops running the first time someone narrows the filter. `AERORA-TEST-REQ-009` requires Aerora to use
the existing labels unmodified, which is what keeps this true.

### Test categories Aerora must exercise

| Category | Mechanism | Covers |
| --- | --- | --- |
| Unit tests | Self-registering macro, eleven base classes, `Unit` label | 36 interfaces assigned `UNIT` |
| Integration tests | `MockLink` with CTest resource locking, `Integration` label | 145 interfaces assigned `INTEGRATION` |
| QML tests | The out-of-process Qt Quick test executable, plus the in-process structure validator | The `IF-UIS` presentation contracts |
| `MockLink` vehicle simulation | Existing, unmodified | Every `INTEGRATION` interface and the all-network-blocked test |
| Architectural-property tests | `Unit`-labelled, in the default gate | `AERORA-TEST-REQ-004`; the prohibition interfaces |

Two unresolved items gate all of this and neither is resolved by this document:

| Item | Question | Risk |
| --- | --- | --- |
| Whether Aerora tests placed in the overlay are reachable by the runner at all | Q-035 | R-021 |
| Whether `MockLink` resource locking holds when Aerora integration tests are added alongside existing ones | — | Noted in [22-phase-3-verification-plan.md](22-phase-3-verification-plan.md) |

Q-035 is `IMPL-BLOCKING` and cheap: place one trivial Aerora test in the overlay, build, and see
whether `ctest` enumerates it.

## The Aerora overlay smoke test

The first thing to run after the first successful build, and the only verification that works in a
deployed build where no test harness exists.

`AERORA-TEST-REQ-007` and `AERORA-UI-REQ-007` require a startup self-check that **fails visibly**.
Four assertions:

| Assertion | Detects | Interface |
| --- | --- | --- |
| Every Aerora override resource exists at its mirrored `/Custom`-prefixed path | A build-system mistake in the resource alias | `IF-UIS-012` |
| Every Aerora QML module URI is importable | A missing or misnamed QML module | `IF-UIS-013` |
| Interception of each overridden stock URL returns the `/Custom`-prefixed URL | An override that silently did not take effect | `IF-UIS-014` |
| **No** stock URL is intercepted that Aerora did not intend to override | An override that silently replaced upstream behaviour | `IF-UIS-014` |
| The selected firmware plugin is the Aerora one | The R-010 factory-conflict failure | — |
| No listening socket is open | The `qthttpserver` exposure | `IF-CLD-003` |

The third and fourth assertions are **writable today**, which is a Phase 3.1 correction. The first
Phase 3 pass recorded that they could not be specified until Q-020 was answered, on the belief that
override resolution was an undetermined contest between two resources aliased to one URL. It is not:
resolution is an existence check performed by a URL interceptor installed through a supported
virtual, and [16-ui-state-icd.md](16-ui-state-icd.md) traces all five steps in source. Both
assertions reduce to "does the mirrored resource exist, and did interception return it".

Q-020's residual remains `IMPL-BLOCKING` and is a different question: whether **Aerora's own**
overlay resources are declared at the mirrored prefix with the right aliases, and whether any
resource is mirrored unintentionally. That needs a build. Its failure is silent in both directions
and the build succeeds either way, which is why the self-check exists at all.

A failed self-check must be a visible startup error and not a log line. An interface quietly missing
its Aerora layer is worse than one that refuses to start, because an operator cannot distinguish "the
payload panel is absent" from "the payload has nothing to report".

## Continuous integration requirements

| Requirement | Obligation |
| --- | --- |
| `AERORA-TEST-REQ-006` | Architectural-property tests run in the **default** gate, never an optional job |
| `AERORA-TEST-REQ-008` | Overridden-file drift checked **mechanically** against recorded fork-point commits, not by procedure |
| `AERORA-TEST-REQ-009` | Upstream test framework, base classes and labels used without modification |
| `AERORA-BUILD-REQ-007` | Each upstream integration includes a drift review of every overridden file **and a seam signature review** |
| `AERORA-BUILD-REQ-010` | A deployed station can report its build version, fork point, dialect commit, settings schema version, applied configuration version and dependency versions |

`AERORA-BUILD-REQ-007` has two halves and the second is the easy one to drop. A changed virtual
**signature** is a compile error and needs no process. A changed virtual **semantic** is not, and
only the seam review catches it. That asymmetry is R-014's mechanism.

`AERORA-TEST-REQ-008` is the highest-value CI addition available, because it converts the one `AUDIT`
item whose failure is silent — the tier-3 drift review at `IF-UIS-012` — into a check that fails.
R-026 is the risk it closes.

### Version self-reporting

`AERORA-BUILD-REQ-010` is a build obligation with a diagnostics consequence. Six values, and the
compatibility matrix is useless without them:

| Value | Source |
| --- | --- |
| Ground-station build version | Version-control description at build time |
| Upstream fork point | Recorded per `AERORA-BUILD-REQ-005` |
| Dialect commit | The pinned immutable reference |
| Settings schema version | The build configuration value — see below |
| Applied configuration version | `IF-CLD-007` |
| Dependency versions | Build manifest |

## Dependency pinning

| Obligation | Requirement | Current state |
| --- | --- | --- |
| MAVLink dialect pinned to an immutable commit, **never a branch** | `AERORA-BUILD-REQ-008` | Not applicable yet; no Aerora dialect exists |
| Dialect and all third-party dependencies pinned before release | `AERORA-SEC-REQ-020` | **Two dependencies track moving references** — R-011 |
| Per-dependency licence position established | — | **Unverified** — R-020, Q-005 |
| Release artifacts signed and versioned | `AERORA-SEC-REQ-020` | Platform signing gap — R-015, Q-032 |
| No signature verification on updates or firmware | — | R-007, release-gated |

Dialect pinning appears in three documents because it serves three purposes — reproducibility,
compatibility-matrix validity, and supply-chain integrity. It is one mechanism answering all three,
and `IF-SEC-021` is its interface contract.

A floating dependency reference means two builds of one Aerora commit can embed different message
definitions, at which point the compatibility matrix is a document about nothing.

## The settings-schema version coupling

A build-configuration value that destroys key material, and therefore a build-readiness item rather
than only a security one.

| Property | Detail |
| --- | --- |
| Behaviour | A stored settings-version mismatch clears the **entire** settings store before any subsystem starts |
| Coupling | Signing keys live in that store |
| Current build value | 9 |
| Consequence | **Incrementing it destroys every stored signing key**, with only a generic settings-upgraded notice |
| Aerora exposure | Aerora will change persisted settings shape, so Aerora will bump it |

All `GCS-VERIFIED`. The obligations are `AERORA-SEC-REQ-018` and `AERORA-BUILD-REQ-009`: treat a
schema bump as a key-destroying event, and require a recorded fleet reprovisioning plan.

Both are **assigned** `AUDIT` rather than a test, because there is nothing to test. The behaviour is
correct for settings and wrong for keys, and only the release process can tell the difference. No
audit has been performed. `IF-SEC-019` is the interface; R-023 is the risk.

## Licensing

ADR-009 maps to no interface and constrains the delivery model, so it is carried here.

| Item | Position |
| --- | --- |
| Qt licensing decision | **Open** — R-001, `BLOCKER` class, gated at `G-RELEASE` |
| Architectural stance | Licence-neutral by construction, the ninth architectural principle |
| What that buys | The architecture does not depend on which Qt licence is chosen |
| What it does not buy | The decision still has to be made before shipping |
| Third-party licence inventory | Unverified — R-020, Q-005 |

Licence neutrality is an engineering mitigation for a commercial question. It removes the risk that
the answer forces a rewrite; it does not remove the risk that there is no answer.

## Entry criteria for implementation

The `G-IMPL` gate, as it applies to the build. Each row is checkable rather than a judgement.

| # | Criterion | Requirement or risk |
| --- | --- | --- |
| 1 | An Aerora repository exists; upstream is a distinct fetch-only remote | `AERORA-BUILD-REQ-004`, R-008 |
| 2 | The fork-point commit is recorded and equals `9459a4b5` | `AERORA-BUILD-REQ-005` |
| 3 | A host can configure and build: CMake 3.25+, Qt 6.11.1 with the declared modules, Ninja, a C++20 compiler | R-009 |
| 4 | `just test` runs and enumerates the existing test suite | R-009 |
| 5 | One trivial Aerora test in the overlay is enumerated by `ctest` | Q-035, R-021 |
| 6 | Alias precedence is observed and recorded | Q-020, R-019 |
| 7 | The Aerora firmware plugin is the selected one, asserted by test | R-010 |
| 8 | The property tests carry the `Unit` label and run in the default gate | `AERORA-TEST-REQ-006` |
| 9 | Overlay directory structure in place; self-registering C++ in the main target | `AERORA-BUILD-REQ-006`, `AERORA-GCS-REQ-009` |
| 10 | The plan-constraint fallback for payload state is implemented | R-002, evidence matrix item 14 |

Criterion 10 is the one with no requirement identifier of its own. The mission fallback — constrain
plans and warn at resume when payload state retention is unevidenced — is carried in
`AERORA-MISSION-REQ-003`'s rationale and the Phase 2.1 evidence matrix rather than in a requirement,
because Phase 3 is prohibited from creating requirement identifiers. That gap is recorded as
[R3-008](21-phase-3-risks.md), and criterion 10 exists so it cannot be lost.

Criteria 5 and 6 are both cheap and both blocking, and neither can be done before criterion 3. That
ordering is the critical path: **a build host is the single prerequisite for everything else in this
table.**

## Not specified, and why

| Item | Reason |
| --- | --- |
| Target platform set | Q-032; Windows signing gap is the known case |
| Qt licence selection | R-001; a legal and commercial decision |
| Installer and packaging format | Depends on the platform set |
| Overlay directory name and internal layout | Implementation choice within `AERORA-BUILD-REQ-006` |
| Branch and merge strategy beyond fetch-only upstream | Q-038; merge cadence and owner |
| Coverage thresholds | Coverage of code is not coverage of risk |
| Build timings and parallelism | Host-dependent; `JOBS` is already parameterised |
| Aerora dialect repository location | Q-003; fork strategy |

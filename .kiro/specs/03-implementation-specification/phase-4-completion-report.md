# Phase 4 Completion Report

Implementation specification and build bootstrap for the Aerora Universal Ground Control Station.

| Field | Value |
| --- | --- |
| Phase | 4 — Track A implementation specification, Track B build bootstrap |
| QGC baseline commit | `9459a4b57986f40df8bdc193d5453c0596fbb416` |
| QGC source files modified | **0** — `git diff --stat HEAD` empty |
| Track A | **Complete** |
| Track B | **BLOCKED** — environment, not engineering |
| Interfaces implemented | **0** |
| Interfaces verified | **0** |
| Documents produced | 26, including this report |
| Review status | **AWAITING HUMAN REVIEW** |

Instruction files read before editing: `AGENTS.md`, `CODING_STYLE.md` in full,
`.pre-commit-config.yaml`, `justfile`, `.github/build-config.json`,
`tools/setup/install_dependencies/_windows.py`, `tools/setup/install_qt.py`, `CMakeLists.txt`,
`cmake/CustomOptions.cmake`, `cmake/QGCTest.cmake`, `cmake/Toolchain.cmake`.

---

## A. Implementation architecture

All 240 interface contracts mapped to an owning module, a verified QGroundControl seam, a class, a
state-model position, a MAVLink boundary, a test target and an implementation status. Full table in
[04-cpp-implementation-map.md](04-cpp-implementation-map.md), mechanically generated and reviewed.

| Status | Count | Share |
| --- | --- | --- |
| `IMPLEMENTATION-PLANNED` | **182** | 76% |
| `CONTRACT-ONLY` | **51** | 21% |
| `BLOCKED` | **7** | 3% |
| `IMPLEMENTED` | **0** | — |
| `VERIFIED` | **0** | — |

### Layering

| Layer | Content | Modified? |
| --- | --- | --- |
| L1 Upstream | QGroundControl at the pinned commit | **No** |
| L2 Product profiles | Painting first; three others undefined | Specified |
| L3 Aerora platform | 19 product-independent modules | Specified |
| L4 Extension surface | 12 verified seams | Specified |

Dependency flow is strictly downward. No L3 module references a product profile, and the property
test that enforces it is specified as architectural property 9.

### The seven blocked interfaces

| Interface | Finding | Why code must not be written yet |
| --- | --- | --- |
| `IF-SEC-013` | F3.1-01 | No seam. Any overlay implementation is a core edit in disguise |
| `IF-LOG-020` | F3.1-03 | Fixed contract shape, no mechanism. A stub would read as compliant |
| `IF-RAD-001` | F3.1-02 | Implementing either branch commits to an unverified hardware fact |
| `IF-SEC-003` | F3.1-04 | Code-isolated but not deployable without Q-029 |
| `IF-FC-017`, `IF-UIS-005`, `IF-UIS-006` | F3.1-05 | Capability presence and availability are indistinguishable |

---

## B. QGC extension map

Twelve seams, every one read in the checkout with file and line. Full map in
[03-qgc-extension-map.md](03-qgc-extension-map.md).

| Seam | Location | Verified |
| --- | --- | --- |
| `QGC_CUSTOM_DIR` + overlay contract | `cmake/CustomOptions.cmake:11-14`; `CMakeLists.txt:47-54`, `:65-76`, `:402-404` | Yes |
| Overlay sources into the app target | `src/CMakeLists.txt:207-208` via `CUSTOM_SOURCES` | Yes |
| Core plugin substitution | `src/API/QGCCorePlugin.cc:64` | Yes |
| QML engine hook + URL interceptor | `QGCCorePlugin.h:117`; `QGCApplication.cc:275`, `:311` | Yes |
| Firmware plugin factory self-registration | `FirmwarePluginFactory.h:9-46`, `.cc:11-31` | Yes |
| Plugin selection | `FirmwarePluginManager.cc:75-102` | Yes |
| Fact group attachment | `FirmwarePlugin.h:353`; `Vehicle.cc:364-370` | Yes |
| Fact group API | `FactGroup.h:25-27`, `:48-49`, `:59-65` | Yes |
| Message dispatch and two veto seams | `Vehicle.cc:525-592` | Yes |
| Serial transport | `SerialLink.cc:36-358` | Yes |
| Factory-disable options | `cmake/CustomOptions.cmake:187`, `:190` | Yes |
| Test registration | `cmake/QGCTest.cmake:140-239` | Yes |

**No planned seam was found to be missing.** The §28 condition "a planned extension seam does not
actually exist" did not trigger.

---

## C. Module structure

Nineteen platform modules plus product profiles. Ownership is disjoint: no interface has two owners.
Full register in [02-module-architecture.md](02-module-architecture.md).

| Module | Interfaces | Module | Interfaces |
| --- | --- | --- | --- |
| `M-Vehicle` | 34 | `M-Identity` | 7 |
| `M-Mavlink` | 28 | `M-Plugin` | 8 |
| `M-Audit` | 24 | `M-Autonomy` | 8 |
| `M-Security` + `M-Authz` | 21 | `M-Capability` | 2 |
| `M-Ui` | 17 | `M-Firmware` | — |
| `M-Mission` | 16 | `M-Config` | — |
| `M-Cloud` | 16 | — | — |
| `M-Payload` | 15 | — | — |
| `M-Telemetry` | 13 | — | — |
| `M-Authority` | 12 | — | — |
| `M-Power` | 12 | — | — |
| `M-LinkState` | 9 | **Total** | **240** |

Two modules were separated from the directive's seventeen for specific reasons: `M-Authz` from
`M-Security` because F3.1-04 requires role resolution to be replaceable independently of
authentication, and `M-Capability` from `M-Vehicle` because the capability model must serve payload,
power and autonomy equally.

**Painting is a profile, not core.** `aeroraPaint` lives in an L2 payload-module profile; nothing in
L3 mentions paint. No painting, firefighting or agriculture functionality was implemented.

---

## D. Build status

> **No build was performed. No build evidence exists. Nothing is claimed.**

| Measured | Value | Required |
| --- | --- | --- |
| Free disk, sole drive `C:` | **1.00 GB** of 111.1 GB | ~25–40 GB |
| Administrator rights | **No** | Yes, for MSVC and GStreamer |
| Logical cores / memory | 4 / 7.9 GB | Workable but marginal — R4-004 |
| C++ compiler | **None** — no `cl`, `gcc`, `clang`, `clang-cl`, `vswhere`, Visual Studio directory or Windows Kits | MSVC 2022 VCTools + Windows SDK |
| Qt | **None** anywhere on the host | 6.11.1 `win64_msvc2022_64` + 14 modules |
| CMake / Ninja / GStreamer | **All absent** | 3.25+ / any / 1.28.4 devel |
| Present and usable | python 3.12, uv, pip, node, git, java 25, winget | — |

### Nothing was installed, deliberately

| Reason | Detail |
| --- | --- |
| Filling the last gigabyte of a system drive risks destabilising Windows | The paging file and Windows Update need headroom on `C:` |
| A part-completed MSVC installation is worse than none | It leaves a registered-but-broken toolchain that `vswhere` reports as present |
| The admin-free half was also not attempted | Installing 4–6 GB of Qt onto 1 GB of free space fails and leaves a partial tree |
| The directive authorises installation, not damage | §20 |

### Required build record: empty

| Field | Value |
| --- | --- |
| Compiler, Qt, CMake, Ninja, GStreamer versions | **Not measured** |
| Configure / build commands | Specified in [19-build-bootstrap.md](19-build-bootstrap.md) |
| Output executable | **Not produced** |
| Build duration, warnings, errors, test result | **Not measured** |

The bootstrap is written as a four-phase scripted sequence so it runs unattended once disk and
elevation exist.

### The one build-adjacent fact that is verified

| Property | Evidence |
| --- | --- |
| The tree is unmodified upstream at the pinned commit | `git rev-parse HEAD` = `9459a4b5...`; `git diff --stat HEAD` empty; untracked paths are `.kiro/` and the product baseline folder only |

Four phases of specification, zero modifications to QGroundControl.

---

## E. Bootstrap status

| Directive item | State |
| --- | --- |
| §21 baseline QGC build | **Not performed** |
| §22 Aerora repository structure | **Specified, not created** |
| §23 structural overlay | **Specified, not created** |
| §24 build proof | **No evidence produced** |

### What the structural overlay would prove

Eight minimal artefacts, each labelled `BOOTSTRAP`, covering the seven properties §23 lists. None
commands an aircraft, defines an enumeration or implements a product feature.

| Artefact | Proves |
| --- | --- |
| `aerora/CMakeLists.txt` + `cmake/CustomOverrides.cmake` | The overlay contract; `QGC_CUSTOM_DIR` recognition |
| `AeroraPlugin` | Core plugin substitution, engine hook, URL interceptor |
| `AeroraFirmwarePluginFactory` + `AeroraFirmwarePlugin` | Self-registration, selection, `factGroups()` |
| One fact group + one metadata file | Attachment, metadata loading, QML visibility, CSV inclusion |
| One QML module + one tier-2 seam override | Module registration, override resolution |
| Startup self-check | All of the above, in a deployed build |

### Repository layout

Specified in [02-module-architecture.md](02-module-architecture.md), with one deliberate deviation
from the directive's suggested tree: **no `upstream/` subdirectory**. Source evidence contradicts it —
the overlay contract resolves `${CMAKE_SOURCE_DIR}/${QGC_CUSTOM_DIR}` from the real source root, so
Aerora must be a sibling inside the upstream tree, not a peer of it. The directive permits this where
source evidence indicates a better structure.

---

## F. The four Phase 3.1 findings

| Finding | Treatment | Outcome |
| --- | --- | --- |
| **F3.1-01** Signing-key storage | Class, header, every public method, settings group and key path, derivation constants, export path and friend declaration all named with file and line. Seven extension strategies assessed. **The required core modification stated explicitly**: two functions, `MAVLinkSigningKeys.cc:250-277` and `:404-414`, plus a backend interface | `IF-SEC-013` **`BLOCKED`** at `G-RELEASE`. ADR-001's formulation unchanged: zero core edits required, one remains a possible fallback. **Phase 4 does not choose** |
| **F3.1-02** Radio transport | Split into two questions. **QGC capability: ANSWERED** — `SerialLink.cc:177-189` sets a port name and opens it; the only refusal is the bootloader flag; **there is no device whitelist**. A plain USB-serial radio works with zero core edits. **Aerora hardware behaviour: UNANSWERED**, Q3-001, one bench observation. Non-serial cost quantified at **four core edits** | `IF-RAD-001` **`BLOCKED`** at `G-ARCH`. Seven `IF-RAD` interfaces now `IMPLEMENTATION-PLANNED` as inherited behaviour |
| **F3.1-03** Audit-store integrity | Nine requirements enumerated. Existing QGC facilities assessed: `LogManager`, `MAVLinkLogManager`, the CSV writer — **none reusable**. **QGC has no append-only, integrity-protected or tamper-evident facility at all.** Aerora must provide its own writer. The required seam specified with **no default implementation**; the writer refuses to start without an injected one | `IF-LOG-020` **`BLOCKED`** at `G-RELEASE`. 23 of 24 `IF-LOG` interfaces planned. New coupling found: **R4-002** |
| **F3.1-04** Role resolution | Four abstractions specified — identity, role resolution, authorisation, command gate — each independently replaceable. Configuration boundary named. Unavailable-role behaviour is fail-closed. **No role name or permission name invented** | `IF-SEC-003` **`BLOCKED`** at `G-IMPL`. Ten tests writable before Q-029 resolves, because each asserts a property of the abstraction rather than of a policy |

### F3.1-03's new coupling

Any integrity mechanism strong enough to resist **repudiation** needs a key, and the station cannot
currently protect a key. **F3.1-03 cannot be fully solved before F3.1-01.** Two bad outcomes are
available and both look like progress: an unkeyed digest that detects only accidental corruption
while the requirement reads as met, or a keyed chain whose key sits beside the records it protects.
Recorded as R4-002 and Q4-003.

---

## G. Remaining blockers, by class

### Architecture blockers

| Blocker | Detail | Gate |
| --- | --- | --- |
| **Q4-002** — fact groups shared across vehicles | `factGroups()` returns a map owned by a plugin that is a cached singleton. Two vehicles of one firmware class write into the same objects. **R4-001 at `HIGH`.** The fix may change the standing core-edit position | **`G-ARCH`** |
| **Q3-001** — radio host presentation | The answer can invalidate ADR-002 and return six deferred transport core edits | **`G-ARCH`** |

### Implementation blockers

| Blocker | Detail |
| --- | --- |
| **R4-003 / R-009** — no build host | 1.00 GB free disk, no elevation. `BLOCKER`. Items 2 to 4 of the stop condition cannot be satisfied |
| **R-008** — `origin` points at upstream for push | The first implementation commit would target mavlink/qgroundcontrol |
| **Q-035 / R-021** — Aerora test reachability | Tests run inside the application target; an overlay test may not be enumerated |
| **Q4-001 / R4-005** — C++-read resource precedence | The URL interceptor does not cover `QFile(":/json/...")`. One build answers it |
| **F3.1-04 / Q-029** — no deployable role configuration | The code is role-agnostic; a station with no role set denies everything |

### Integration blockers

| Blocker | Detail |
| --- | --- |
| No dialect | 51 interfaces `CONTRACT-ONLY`. Nine candidate families named by the baseline, none defined |
| **Q-018** — Aerora `MAV_AUTOPILOT` | Mitigated by exclusion: both vendor factories disabled, so selection is deterministic |
| **Q3-009 / F3.1-05** — capability presence versus availability | Three interfaces blocked; the fallback discards information rather than deferring it |
| **Q3-004 / R3-003** — watchdog reporting path | The recorded source may name a relay as the originator |
| **Q3-005** — autonomy proposal path | Determines which of two interfaces exists |
| **Q3-006** — payload controller as a component | Determines whether payload silence is distinguishable |

### Hardware evidence blockers

| Blocker | Evidence required | Cost |
| --- | --- | --- |
| **Q3-001** radio enumeration | Attach the radio; record device class and USB identifiers | **One connection** |
| **Q-018** autopilot identifier | Read the heartbeat | One connection |
| **Q3-009** capability field definition | Read the inventory message | One message |
| **Evidence matrix item 14** payload retention | Set state, interrupt, resume, read state | A bench rig |
| **Q3-007** high-voltage reporting | A firmware telemetry definition | Firmware |
| **Q-024, Q-026** power enumerations and transitions | Separate engineering per baseline 49 | Programme work |

The first three are each a single observation and none needs a ground station. They should proceed in
parallel, starting now.

### Release blockers

| Blocker | Requirement |
| --- | --- |
| **F3.1-01** key storage at rest | `AERORA-SEC-REQ-017` |
| **F3.1-03** audit integrity | `AERORA-GCS-REQ-028` |
| **R-011** two dependencies track moving references | `AERORA-SEC-REQ-020` |
| **R-015, Q-032** platform code signing | `AERORA-SEC-REQ-020` |
| **R-020, Q-005** per-dependency licence position | — |
| **R-001** Qt licensing decision | ADR-009 |
| **R-023** settings-schema bump destroys keys | `AERORA-BUILD-REQ-009` |

---

## H. Traceability

```text
Requirement → Interface → Module → QGC seam → Source file → Test → Verification evidence
```

| Link | State |
| --- | --- |
| Requirement → Interface | **Complete** — 120 of 120 Phase 2 requirements referenced |
| Interface → Module | **Complete** — 240 of 240 |
| Module → QGC seam | **Complete** — 12 seams, all verified present |
| Seam → Source file | **Complete** — file and line for all 16 seam groups |
| Source file → Test | **Complete** — per module, 19 modules |
| Test → Verification evidence | **Empty. Nothing executed** |

Complete to the fifth link, empty at the sixth. The sixth column is left blank rather than filled
with intentions.

### Identifier integrity

| Check | Result | Required |
| --- | --- | --- |
| Interface identifiers added or renumbered | **0** | 0 |
| Requirement identifiers added | **0** | **0** |
| Requirements promoted `PROPOSED` → `CONFIRMED` | **0** | **0** |
| Inherited questions closed | **0** | **0** |
| Inherited risks closed or downgraded | **0** | — |
| MAVLink message, command or enumeration identifiers invented | **0** | **0** |
| New Phase 4 questions | 4 — `Q4-001` to `Q4-004` | — |
| New Phase 4 risks | 6 — `R4-001` to `R4-006` | — |
| QGC source files modified | **0** | **0** |
| C++, QML, CMake or dialect files created | **0** | **0** |
| Tests created | **0** | **0** |

---

## Verification performed

Documentation and repository consistency only. **No build, no runtime, no test execution.**

| Check | Tool | Result |
| --- | --- | --- |
| Markdown lint, whole `.kiro` tree | `markdownlint-cli` **0.49.1** — the version pinned in `.pre-commit-config.yaml` — with the repository's own `.markdownlint.json` | **0 findings** |
| Table shape, all 97 documents in `.kiro/specs` | Local checker, escaped-pipe aware | **0 mismatched rows** |
| Identifier existence | Local checker against a freshly extracted ground truth | **0** nonexistent requirement, interface, question, risk, ADR or finding citations |
| Internal links | Local checker | **0 broken** |
| Invented requirement domains | Local checker | **0** |
| Implementation status counts | Local checker over the 240-row map | 51 / 182 / 0 / 0 / 7 — **0 implemented, 0 verified** |
| Repository diff | `git diff --stat HEAD` | **Empty** |
| Host inventory | PowerShell, read-only | Disk, elevation, cores, memory, 15 tools |

### Not run, and why

| Tool | Reason |
| --- | --- |
| `cmake`, `ninja`, a C++ compiler, Qt, `just`, `ctest` | **Absent from the host**, and uninstallable with 1.00 GB free and no elevation |
| `pre-commit`, `clang-format`, `clang-tidy`, `qmllint`, `clazy` | Absent. A partial run would mislead |
| QGC build, unit tests, QML tests, MockLink tests, overlay smoke test | Require the above |

No QGroundControl build or runtime verification is claimed. Every source-derived statement in this
phase was obtained by reading files, and each cites its location.

---

## Document set

| # | Document | Bytes |
| --- | --- | --- |
| 00 | `00-phase-4-readme.md` | 9,032 |
| 01 | `01-implementation-architecture.md` | 8,377 |
| 02 | `02-module-architecture.md` | 10,283 |
| 03 | `03-qgc-extension-map.md` | 17,211 |
| 04 | `04-cpp-implementation-map.md` | 54,791 |
| 05 | `05-qml-implementation-map.md` | 10,607 |
| 06 | `06-factgroup-implementation.md` | 9,674 |
| 07 | `07-firmware-plugin-implementation.md` | 8,554 |
| 08 | `08-mavlink-implementation-boundary.md` | 6,887 |
| 09 | `09-mission-implementation.md` | 8,372 |
| 10 | `10-payload-implementation.md` | 7,235 |
| 11 | `11-power-implementation.md` | 6,643 |
| 12 | `12-security-implementation.md` | 11,239 |
| 13 | `13-audit-implementation.md` | 13,528 |
| 14 | `14-authorization-implementation.md` | 9,392 |
| 15 | `15-cloud-boundary.md` | 9,379 |
| 16 | `16-test-implementation.md` | 8,456 |
| 17 | `17-ci-build-architecture.md` | 7,490 |
| 18 | `18-upstream-drift-detection.md` | 7,241 |
| 19 | `19-build-bootstrap.md` | 15,477 |
| 20 | `20-phase-4-traceability.md` | 8,665 |
| 21 | `21-phase-4-open-questions.md` | 10,491 |
| 22 | `22-phase-4-risks.md` | 12,207 |
| 23 | `23-phase-4-verification-plan.md` | 8,887 |
| 24 | `24-build-readiness.md` | 5,909 |
| — | This report | — |

**25 specification documents** in `.kiro/specs/03-implementation-specification/`, plus this report.
Cumulative across the programme: **97 documents** in `.kiro/specs`.

---

## What a reviewer should decide first

| # | Decision | Why it is first |
| --- | --- | --- |
| 1 | **Free ~40 GB and provide elevation** | Nothing else can start. Items 2 to 4 of the stop condition are gated on it |
| 2 | **Q4-002** — how fact groups attach per vehicle | `G-ARCH`. May change ADR-001's core-edit position. `HIGH` correctness risk |
| 3 | **F3.1-01** — upstream contribution or recorded core edit | `G-RELEASE`, and F3.1-03 depends on it |
| 4 | **Q3-001** — authorise the radio bench observation | One connection. Confirms or withdraws ADR-002 |
| 5 | **Q-029** — the operator role set | Without it the station is correct and unusable |
| 6 | **Q-037 / Q4-003** — which threat audit integrity addresses | Determines whether F3.1-01 must be resolved first |
| 7 | Whether the 19-module register and the L3 purity rule are right | Changing them later changes every module |

Decisions 2 and 3 are the two that touch the standing architecture position. Phase 4 deliberately did
not take either.

---

PHASE 4 IMPLEMENTATION SPECIFICATION + BUILD BOOTSTRAP COMPLETE — READY FOR HUMAN REVIEW

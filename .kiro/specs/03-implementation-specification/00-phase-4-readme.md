# Phase 4 — Implementation Specification and Build Bootstrap

Converting 240 ratified interface contracts into an implementation blueprint, and establishing the
build environment that makes the blueprint executable.

## Status

| Field | Value |
| --- | --- |
| Phase | 4 — implementation specification (Track A) and build bootstrap (Track B) |
| QGC baseline commit | `9459a4b57986f40df8bdc193d5453c0596fbb416` |
| QGC source files modified | **0** |
| Aerora overlay created | **No** — gated on a proven baseline build |
| Interfaces implemented | **0** |
| Interfaces verified | **0** |
| Track A | **Complete** |
| Track B | **BLOCKED** — see [19-build-bootstrap.md](19-build-bootstrap.md) |
| Review status | **AWAITING HUMAN REVIEW** |

Instruction files read before editing: `AGENTS.md`, `CODING_STYLE.md`, `.pre-commit-config.yaml`,
`justfile`, `.github/build-config.json`, `tools/setup/install_dependencies/_windows.py`,
`tools/setup/install_qt.py`, `CMakeLists.txt`, `cmake/CustomOptions.cmake`, `cmake/QGCTest.cmake`.

## Track B is blocked, and it blocks items 2 to 4 of the stop condition

Measured on the host, not assumed:

| Item | Measured | Required |
| --- | --- | --- |
| Free disk, sole drive `C:` | **1.00 GB** of 111.1 GB | **~25–40 GB** |
| Administrator rights | **No** | **Yes**, for MSVC Build Tools and GStreamer |
| C++ compiler | **None** | MSVC 2022 VCTools + Windows SDK |
| Qt | **None** | 6.11.1 `win64_msvc2022_64` + 14 modules |
| CMake, Ninja, GStreamer | **All absent** | 3.25+, any, 1.28.4 devel |

No installation was attempted. Consequently:

| Directive item | State |
| --- | --- |
| §21 baseline QGC build | **Not performed** |
| §22 Aerora repository structure | **Specified, not created** |
| §23 structural overlay | **Specified, not created** |
| §24 build proof | **No evidence produced.** Nothing is claimed |

Everything in Track A was derived by reading source, and every source claim carries a file and line
reference. Nothing in this document set was compiled or run.

## Implementation status vocabulary

Mandatory on every implementation item.

| Status | Meaning |
| --- | --- |
| `CONTRACT-ONLY` | An interface contract exists. No implementation is planned yet because an input is missing |
| `IMPLEMENTATION-PLANNED` | A module, seam and class are assigned; the work is executable once the build exists |
| `IMPLEMENTED` | Source code exists that realises the contract |
| `VERIFIED` | The implementation has been executed and evidence confirms the behaviour |
| `BLOCKED` | Implementation cannot proceed; a named finding stands in the way |

Current distribution across the 240 interfaces:

| Status | Count |
| --- | --- |
| `IMPLEMENTATION-PLANNED` | **182** |
| `CONTRACT-ONLY` | **51** |
| `BLOCKED` | **7** |
| `IMPLEMENTED` | **0** |
| `VERIFIED` | **0** |

> **Specified ≠ Implemented. Implemented ≠ Verified.**

Carried from Phase 3.1 and not reopened. No item in this phase is marked `IMPLEMENTED` or
`VERIFIED`, because no build exists to produce either.

## Invariants carried forward

Unchanged from Phases 2, 2.1, 3 and 3.1. Phase 4 adds nothing to this list and removes nothing.

| Invariant |
| --- |
| The ground station is supervisory; the vehicle is authoritative |
| The ground station does not own stabilisation, failsafe decisions, arming authority, payload actuator safety, payload emergency-stop behaviour, power-source switching or companion autonomy |
| Effective state takes precedence over requested state |
| Telemetry is untrusted external input |
| Behaviour that is not confirmed is not implemented |
| Local before cloud; no cloud service on the command path |
| All parameters are Facts |
| Aerora code lives in the overlay |
| Payload concerns do not leak into flight-control concerns |
| Licence-neutral by construction |

### State model

```text
REPORTED  EFFECTIVE  REQUESTED  PROPOSED  STALE  UNKNOWN  UNAVAILABLE
```

Never silently promoted:

```text
PROPOSED  → EFFECTIVE        REQUESTED → EFFECTIVE
STALE     → CURRENT          UNKNOWN   → CONFIRMED
```

A companion proposal never renders as authoritative aircraft state. The implementation consequence
is in [05-qml-implementation-map.md](05-qml-implementation-map.md): the state class is a property of
the Fact binding, not a styling choice made per screen.

## Document set

| # | Document | Subject |
| --- | --- | --- |
| 00 | This document | Scope, status vocabulary, the blocker |
| 01 | [01-implementation-architecture.md](01-implementation-architecture.md) | Layering, build order, bootstrap sequence |
| 02 | [02-module-architecture.md](02-module-architecture.md) | 17 core modules, 4 product profiles, dependency rules |
| 03 | [03-qgc-extension-map.md](03-qgc-extension-map.md) | **Verified** QGC files, classes, methods and seams |
| 04 | [04-cpp-implementation-map.md](04-cpp-implementation-map.md) | All 240 interfaces mapped to module, seam, class and status |
| 05 | [05-qml-implementation-map.md](05-qml-implementation-map.md) | UI seams, override mechanism, state-class rendering |
| 06 | [06-factgroup-implementation.md](06-factgroup-implementation.md) | Six fact groups, and a newly found multi-vehicle hazard |
| 07 | [07-firmware-plugin-implementation.md](07-firmware-plugin-implementation.md) | Factory registration, selection, linkage requirements |
| 08 | [08-mavlink-implementation-boundary.md](08-mavlink-implementation-boundary.md) | Dialect generation path and insertion point. No identifiers |
| 09 | [09-mission-implementation.md](09-mission-implementation.md) | Mission controllers, complex items, resume boundary |
| 10 | [10-payload-implementation.md](10-payload-implementation.md) | Payload abstraction boundary only |
| 11 | [11-power-implementation.md](11-power-implementation.md) | Power state abstraction. No arbitration |
| 12 | [12-security-implementation.md](12-security-implementation.md) | **F3.1-01** signing-key storage, with exact coupling |
| 13 | [13-audit-implementation.md](13-audit-implementation.md) | **F3.1-03** audit store, with the required seam |
| 14 | [14-authorization-implementation.md](14-authorization-implementation.md) | **F3.1-04** identity, role resolution, authorisation |
| 15 | [15-cloud-boundary.md](15-cloud-boundary.md) | Cloud adapters and the command-path prohibition |
| 16 | [16-test-implementation.md](16-test-implementation.md) | Test architecture and the requirement-to-evidence chain |
| 17 | [17-ci-build-architecture.md](17-ci-build-architecture.md) | Overlay CI, gates, cache strategy |
| 18 | [18-upstream-drift-detection.md](18-upstream-drift-detection.md) | Drift mechanism design. Not implemented |
| 19 | [19-build-bootstrap.md](19-build-bootstrap.md) | **F3.1-02** radio transport, host inventory, scripted bootstrap |
| 20 | [20-phase-4-traceability.md](20-phase-4-traceability.md) | Requirement → interface → module → seam → test |
| 21 | [21-phase-4-open-questions.md](21-phase-4-open-questions.md) | New Phase 4 questions |
| 22 | [22-phase-4-risks.md](22-phase-4-risks.md) | New Phase 4 risks |
| 23 | [23-phase-4-verification-plan.md](23-phase-4-verification-plan.md) | Per-module verification. No results |
| 24 | [24-build-readiness.md](24-build-readiness.md) | The go/no-go gate: measured host versus required host |

Phase 3's `23-build-readiness.md` stated the build **requirements**. Document 19 here is the
**procedure** and document 24 is the **gate**. The three are not duplicates and none restates
another.

## Identifier schemes

| Scheme | Rule |
| --- | --- |
| `IF-<domain>-<nnn>` | 240 existing interfaces. **Referenced, never redefined, never renumbered** |
| `AERORA-*-REQ-nnn` | 175 existing requirements. **Referenced only.** Phase 4 defines none |
| `Q-nnn`, `R-nnn` | Phases 1–2.1. Referenced, never redefined |
| `Q3-nnn`, `R3-nnn` | Phase 3. Referenced, never redefined |
| `F3.1-nn` | Phase 3.1 findings. Referenced; four are first-class Phase 4 items |
| `Q4-nnn`, `R4-nnn` | **New** in Phase 4 |
| `M-<Name>` | Aerora implementation modules, defined in document 02 |

Phase 4 introduces **no new requirement identifiers**, no MAVLink identifiers, no enumeration
members and no command identifiers.

## What Phase 4 is authorised to produce, and did

| Authorised | Produced |
| --- | --- |
| Build environment setup | **Blocked.** Inventory performed; nothing installed |
| Dependency installation | **Blocked.** Not attempted |
| Repository bootstrap | **Specified, not created** |
| Structural overlay to prove a seam | **Specified, not created** |
| Documentation and implementation mapping | **This set** |

| Not authorised | Produced |
| --- | --- |
| Aerora flight features, payload behaviour, autonomy logic, failsafe logic | **None** |
| New MAVLink messages, commands or enumerations | **None** |
| Production security or authorisation policy | **None** |
| Painting, firefighting or agriculture functionality | **None** |

No C++, QML, CMake or MAVLink dialect file was created anywhere in the repository.

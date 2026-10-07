# QGroundControl Baseline Analysis

Phase 1 deliverable for the Aerora Universal GCS programme. This document set records what
QGroundControl **actually is** in the exact checkout present in this repository, so that Aerora
architecture decisions rest on evidence rather than assumption.

## Status of this document set

| Field | Value |
| --- | --- |
| Phase | PHASE 1 — QGroundControl read-only analysis |
| Analysis date | 2026-10-04 |
| QGC version | 5.2.0-dev |
| Git commit | `9459a4b57986f40df8bdc193d5453c0596fbb416` |
| Branch | `master` |
| Working tree | Clean — zero tracked files modified |
| QGC source modified | **No** |
| Review status | **AWAITING HUMAN REVIEW** |

## Scope and non-scope

In scope: repository structure, build system, dependency acquisition, MAVLink stack, vehicle
model, parameter/Fact system, link layer, mission subsystem, UI/QML composition, plugin seams,
test infrastructure, security posture, licensing, and the resulting Aerora extension strategy.

Not in scope, and deliberately not attempted: any Aerora implementation, any modification to
QGroundControl, any statement about Aerora flight-controller or vehicle behaviour that is not
already recorded in the v4.1 product baseline.

## Documents

| Document | Contents |
| --- | --- |
| [repository-architecture.md](repository-architecture.md) | Directory tree, module inventory, application startup path |
| [build-system.md](build-system.md) | CMake topology, `QGC_*` options, presets, toolchain requirements |
| [mavlink-architecture.md](mavlink-architecture.md) | Dialect acquisition, parse loop, routing, signing, command layer |
| [vehicle-architecture.md](vehicle-architecture.md) | Vehicle lifecycle, FactGroups, Fact system, FirmwarePlugin, comm loss |
| [communication-architecture.md](communication-architecture.md) | Link abstraction, threading, lifecycle, reconnect, transport extensibility |
| [mission-architecture.md](mission-architecture.md) | Plan controllers, visual item model, transfer protocol, resume, payload hooks |
| [ui-qml-architecture.md](ui-qml-architecture.md) | QML modules, application shell, extension seams, theming |
| [plugin-architecture.md](plugin-architecture.md) | `QGCCorePlugin`, `QGCOptions`, custom-build overlay, factory registration |
| [testing-architecture.md](testing-architecture.md) | Test framework, labels, MockLink, coverage, sanitizers |
| [security-architecture.md](security-architecture.md) | What security exists, and an honest gap list |
| [dependency-map.md](dependency-map.md) | Every third-party dependency with version and licence exposure |
| [aerora-extension-map.md](aerora-extension-map.md) | Where each Aerora capability attaches to QGC |
| [ownership-matrix.md](ownership-matrix.md) | Component-by-component reuse / extend / replace / do-not-modify |
| [risk-register.md](risk-register.md) | Identified engineering risks with severity and mitigation |
| [open-questions.md](open-questions.md) | Every item that must be answered before implementation |
| [traceability.md](traceability.md) | Requirement identifier scheme and traceability foundation |

## Evidence convention

Every architectural claim in this set is tied to a file path, and to a line number where the
precise location matters. Claims are written only where the referenced file was actually read
during this analysis.

Where something was **not** verified, the text says so explicitly. Treat an absence of evidence
as an open item, not as a negative finding, unless the text states that a search was performed
and returned nothing.

## Status vocabulary

These terms are used consistently across this document set and the Aerora specification corpus.

| Status | Meaning |
| --- | --- |
| `CONFIRMED` | Established by direct evidence — code read, command run, or approved Aerora requirement |
| `PROPOSED` | An engineering recommendation from this analysis, not yet ratified |
| `OPEN` | A decision that must be made by Aerora engineering before implementation can proceed |
| `PLACEHOLDER` | A value or behaviour that exists in an artefact but is explicitly provisional |
| `DEPRECATED` | Superseded; retained only for traceability |
| `UNKNOWN` | Not yet investigated, or not determinable from available evidence |

A `PROPOSED` item never becomes a requirement by being implemented. It becomes a requirement
only when Aerora engineering ratifies it and it is recorded in the product baseline.

## Relationship to the Aerora v4.1 product baseline

The v4.1 baseline is present in this repository at:

```text
Aerora_Universal_GCS_Product_Baseline_Specification_v4.1/
└── Aerora_Universal_GCS_Specification_v4/
    ├── README.md
    ├── PROJECT_STATUS.md
    ├── docs/                     52 documents, 00_PROJECT_MASTER .. 51_AWS_AND_MODULAR_MONOLITH
    └── .kiro/                    steering + specs scaffold (NOT installed at workspace root)
```

That baseline remains the authority on Aerora product requirements. This document set is the
authority on QGroundControl's actual behaviour. Where the two meet — the extension map, the
ownership matrix — this set proposes and the baseline decides.

### Two structural conflicts requiring a decision

Both are recorded in [open-questions.md](open-questions.md) as `OPEN` and are **not** resolved
here.

1. The baseline's own `.kiro/specs/00-qgc-baseline-analysis/design.md` nominates five output
   documents under `docs/`. The task directive that produced this set nominates sixteen
   documents under `.kiro/specs/00-qgc-baseline-analysis/`. This set followed the directive and
   flags the divergence; the baseline's spec 00 `design.md` needs updating in the same change
   that ratifies this set, or this set needs relocating.
2. The baseline ships a `.kiro/` scaffold containing steering files and an `AGENTS.md` that are
   **not** installed at the workspace root, so they are currently inert. The active `AGENTS.md`
   is QGroundControl's own. Promoting the Aerora scaffold is a separate, deliberate act.

## What happens next

PHASE 1 ends here, pending review. The recommended next specification is
`01-aerora-architecture`, scoped by [aerora-extension-map.md](aerora-extension-map.md) and
gated on the `OPEN` items in [open-questions.md](open-questions.md).

No QGroundControl source file may be modified until the extension strategy in this set is
reviewed and ratified.

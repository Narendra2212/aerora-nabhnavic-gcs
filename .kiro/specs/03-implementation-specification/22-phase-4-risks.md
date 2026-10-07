# Phase 4 Risks

Six risks found by mapping contracts onto source and by measuring the host. The 29 inherited risks
and the 8 Phase 3 risks are referenced, never redefined.

## Vocabulary

| Severity | Meaning |
| --- | --- |
| `BLOCKER` | Cannot proceed past its gate |
| `HIGH` | Must be resolved by its gate |
| `MEDIUM` | Should be resolved by its gate |
| `LOW` | Tracked; acceptable to carry |

| Gate | Meaning |
| --- | --- |
| `G-ARCH` | Architecture ratification |
| `G-IMPL` | Start of implementation |
| `G-INTEG` | Integration with vehicle hardware |
| `G-RELEASE` | Production release |
| `G-OPEN` | Carried, no gate |

## Summary

| ID | Risk | Likelihood | Severity | Gate |
| --- | --- | --- | --- | --- |
| R4-001 | Aerora fact groups are shared across vehicles through the plugin singleton | **Certain** if multi-vehicle is used | `HIGH` | **`G-ARCH`** |
| R4-002 | Audit integrity is coupled to unresolved key storage | **High** | `HIGH` | `G-RELEASE` |
| R4-003 | No build host exists, so the entire implementation path is unvalidated | **Certain** today | **`BLOCKER`** | `G-IMPL` |
| R4-004 | The measured host cannot build comfortably even once unblocked | Medium | `MEDIUM` | `G-IMPL` |
| R4-005 | C++-read resource override precedence is unverified | Medium | `MEDIUM` | `G-IMPL` |
| R4-006 | Disabling both vendor factories is irreversible at runtime | Low | `LOW` | `G-OPEN` |

One `BLOCKER`, and it is an environment fact rather than an engineering one.

---

### R4-001

**Risk.** `FirmwarePlugin::factGroups()` returns a map owned by a plugin that is a **cached singleton
shared across every vehicle of a firmware class**. The groups are inserted into each vehicle's map,
and every vehicle's dispatch loop writes into them.

**Consequence.** With two vehicles of the same firmware class, one aircraft's telemetry overwrites
the other's. The operator sees values confidently attributed to the wrong aircraft. This is a
**correctness** failure, not a presentation one, and it is silent.

| Property | Assessment |
| --- | --- |
| Exists upstream today | **Yes** — `ArduSubFirmwarePlugin` has the same shape |
| Why unnoticed | ArduSub is operated single-vehicle in practice |
| Affects Aerora | **Yes.** `AERORA-GCS-REQ-010` mandates this seam and multi-vehicle is an invariant |
| Contradicts | ADR-001's choice of this seam as the zero-core-edit attachment route |

**Likelihood.** Certain, conditional on multi-vehicle operation. The code path is unconditional.

**Severity.** `HIGH`. Wrong telemetry attributed to the wrong aircraft is among the worst failures a
ground station can have.

**Gate.** `G-ARCH` — the fix may change the standing core-edit position.

**Mitigation.** None available in the overlay. The seam signature takes no `Vehicle*`, so the plugin
cannot tell which vehicle is asking. Four options are set out in Q4-002; the preferred one is an
upstream signature change, the honest fallback is an Aerora core edit, and **Phase 4 chooses
neither**.

**Evidence.** `GCS-VERIFIED` across six source locations, listed in
[06-factgroup-implementation.md](06-factgroup-implementation.md).

**Owner.** Architecture.

**Affected.** All Aerora fact-group interfaces. `AERORA-GCS-REQ-010`. Question Q4-002.

**Interim position.** Single-vehicle `IMPLEMENTATION-PLANNED`; multi-vehicle `CONTRACT-ONLY`. A
multi-vehicle isolation property test is specified so the hazard cannot return unnoticed.

---

### R4-002

**Risk.** Any audit-integrity mechanism strong enough to resist repudiation needs a key, and the
station cannot currently protect a key. F3.1-03 therefore cannot be fully solved before F3.1-01.

**Consequence.** Two bad outcomes are available and both look like progress:

| Outcome | Why it is wrong |
| --- | --- |
| Ship an **unkeyed** digest chain | Detects accidental corruption only, while `AERORA-GCS-REQ-028` reads as met |
| Ship a **keyed** chain with the key in unprotected settings | The key sits beside the records it protects |

**Likelihood.** High. The requirement's rationale — "an audit record that can be edited is not
evidence" — points at repudiation, and the obvious implementation is the second row.

**Severity.** `HIGH`. A security control that appears to work is worse than an absent one, because
nobody looks for it again.

**Gate.** `G-RELEASE`.

**Mitigation.** Specified rather than implemented: an abstract integrity interface with **no default
implementation**, and a writer that refuses to start without an injected one. The gap is loud at the
first build rather than silent at the first audit.

**Evidence.** `GCS-DECIDED`. The coupling was found by asking what mechanism Q-037 could choose.

**Owner.** Security.

**Affected.** `IF-LOG-020`, `IF-SEC-013`. `AERORA-GCS-REQ-028`, `AERORA-SEC-REQ-017`. Questions
Q-037, Q4-003. Findings F3.1-01, F3.1-03.

---

### R4-003

**Risk.** No build host exists. The host has **1.00 GB free on its only drive** against a requirement
of roughly 25–40 GB, and **no administrator rights** where MSVC Build Tools and GStreamer both
require elevation.

**Consequence.** Every claim in Phases 1 to 4 is a source-reading claim. Specifically unvalidated:

| Unvalidated | Consequence if wrong |
| --- | --- |
| That the baseline builds on any host at all | Everything downstream is unattributable |
| That the overlay contract works as read | The whole extension strategy |
| That Aerora tests are reachable by the runner | Q-035, R-021 |
| That resource override resolves as traced | Q-020 residual, Q4-001 |
| That the plugin factory links from the overlay | R-010 |
| Four of the twelve architectural properties | They have never been compiled, let alone run |

**Likelihood.** Certain, today. It is a measurement.

**Severity.** `BLOCKER`. Items 2, 3 and 4 of the phase stop condition cannot be satisfied.

**Gate.** `G-IMPL`.

**Supplement — reclamation survey.** The `1.00 GB` above is the original point-in-time reading and is
left intact. Free space has since been observed to **fluctuate between 0.11 GB and 2.58 GB without
agent activity**, so it is not a stable figure and not monotonically falling. A bounded survey of
user-clearable locations was run to establish whether cache cleanup alone can reach the requirement:

| Location | Measured | Reclaimable without elevation |
| --- | --- | --- |
| `%LOCALAPPDATA%\uv` | 3,064.2 MB | Yes — `uv cache clean`, rebuilds on demand |
| `%USERPROFILE%\.cache` | 1,180.3 MB | Yes — reconstructible |
| `%TEMP%` | 1,165.9 MB | Yes |
| `%LOCALAPPDATA%\npm-cache` | 163.4 MB | Yes, but it holds the pinned markdownlint used for doc verification |
| `%LOCALAPPDATA%\pip\cache` | 29.3 MB | Yes — `pip cache purge` |
| Recycle Bin | 1.1 MB | Yes |
| **Cache subtotal** | **≈ 5.47 GB** | — |
| `%USERPROFILE%\Downloads` | 1,931.7 MB | User content, not a cache — user's decision |

**Finding.** Clearing every user-clearable cache yields roughly **8 GB free**, and about **10 GB** if
`Downloads` is included. Both fall short of the 25–40 GB requirement. **Cache cleanup alone cannot
unblock Track B.** This changes the remediation advice: the user should not spend effort clearing
caches expecting it to be sufficient.

The routes that can reach the target are therefore removal of a large tree
(`C:\aerora_quant_backend_updated_final1`, 8,612.4 MB, the user's unrelated project), removal of
installed programs (`C:\Program Files` totals 21,942.2 MB), **elevated** cleanup that Phase 4 cannot
perform (WinSxS via DISM, pagefile, hibernation, Windows Update cache), or an additional drive.

**Measurement gap.** `C:\Windows` was never sized. Four recursive scans exceeded the tool timeout on
its hardlink farms, and it is admin-only to reclaim, so the attempt was abandoned rather than retried.
Roughly 43 GB of the 108.6 GB used is unattributed, most of it presumed to be there.

**Mitigation.** None available to Phase 4. The two requirements are external:

| # | Requirement | Owner |
| --- | --- | --- |
| 1 | Roughly 40 GB free on `C:`, or an additional drive — **not reachable by cache cleanup**, see the survey above | User |
| 2 | An elevated session | User |

The bootstrap is written as a scripted sequence so it runs unattended once both exist. No partial
installation was attempted, because filling the last gigabyte of a system drive risks destabilising
Windows and a half-installed MSVC is worse than none.

**Evidence.** Measured: drive geometry, free space, administrator role, and a tool-by-tool inventory
in [19-build-bootstrap.md](19-build-bootstrap.md).

**Owner.** User, then build.

**Affected.** Everything downstream of step 1 of the build order. Existing risk R-009, now quantified.

---

### R4-004

**Risk.** Even once unblocked, the measured host — **4 logical cores, 7.9 GB memory** — is marginal
for a Qt 6.11 debug build with `QGC_ENABLE_WERROR` on.

**Consequence.** Not a correctness risk; a throughput and reliability one.

| Effect | Detail |
| --- | --- |
| Long first build | A cold build with a cold CPM cache on 4 cores |
| Memory pressure | MSVC on a Qt translation unit is memory-hungry; 4 parallel jobs at ~2 GB each exceeds 7.9 GB with the operating system resident |
| Spurious failures | Out-of-memory compiler crashes read as build errors |
| Slow iteration | Which discourages the tight test loops `AGENTS.md` asks for |

**Likelihood.** Medium. It depends on how much is built at once.

**Severity.** `MEDIUM`.

**Gate.** `G-IMPL`.

**Mitigation.** `--parallel 4` at most, and lower if memory pressure appears; the compiler and moc
caches are already on by default; a Release or RelWithDebInfo configuration costs less memory than
Debug. Longer term, continuous integration rather than the local host should carry full builds.

**Evidence.** Measured host specification.

**Owner.** Build.

**Affected.** The bootstrap procedure. No interface.

---

### R4-005

**Risk.** The precedence of an overlay resource registered at the **same** `:/json` path as a core
resource is unverified, and the URL interceptor does not cover C++ resource reads.

**Consequence.** Phase 3.1 correctly established that QML override resolution is deterministic. The
correction is sound and **narrower than it appears**: it covers the QML engine, not
`QFile(":/json/...")`. Two resources at one path, resolved by the resource system rather than an
interceptor, is exactly the ambiguous case Phase 3.1 thought it had eliminated.

| Affected read path | Risk |
| --- | --- |
| `QGCSerialPortInfo.cc:47` board table | An override would be unsafe — and was rejected for this reason |
| `FactGroup` metadata loads | Safe **only because** Aerora uses new filenames |
| Any future override of a core JSON file | Unsafe until Q4-001 is answered |

**Likelihood.** Medium. The temptation arises the first time someone wants to change a core JSON
default.

**Severity.** `MEDIUM`. The failure is silent in both directions, as R-019 describes.

**Gate.** `G-IMPL`.

**Mitigation.** Aerora metadata files use new names only; no core JSON file is overridden; the
startup self-check asserts resource resolution for the QML and C++ cases **separately**. Q4-001 is
answerable by one build.

**Evidence.** `GCS-VERIFIED` — the interceptor's `DataType` and scheme conditions, and the reference
overlay's `PREFIX "/json"` registration.

**Owner.** Build, by observation.

**Affected.** `IF-UIS-014`. `AERORA-UI-REQ-007`. Question Q4-001. Existing risk R-019.

---

### R4-006

**Risk.** Forcing both vendor firmware factories off is irreversible at runtime, and its effect on
offline plan editing is unexamined.

**Consequence.** Informational rather than hazardous.

| Effect | Assessment |
| --- | --- |
| An Aerora build cannot connect to a PX4 or ArduPilot vehicle as such | Intended for a single-stack product |
| Reversal requires a rebuild | Acceptable |
| A stored plan whose firmware type is now unsupported | **Unexamined** — `Vehicle.cc:483` re-selects a plugin from the stored type |

**Likelihood.** Low for the first two; the third is plausible whenever a plan predates the Aerora
build.

**Severity.** `LOW`.

**Gate.** `G-OPEN`.

**Mitigation.** One observation once a build exists. Recorded as Q4-004 rather than designed around.

**Evidence.** `GCS-VERIFIED`.

**Owner.** Product, under Q-017.

**Affected.** `IF-MIS-001`, `IF-MAV-021`. Question Q4-004.

---

## Inherited risks: movement

**None closed, none downgraded.** Three are better understood after Phase 4's source mapping.

| Risk | Change |
| --- | --- |
| R-003 | **Quantified.** A non-serial transport costs a minimum of four core edits: the `LinkType` enum, two switch statements, and `linkTypeStrings()`. There is no registration hook |
| R-005 | **Located exactly.** Two functions, `MAVLinkSigningKeys.cc:250-277` and `:404-414`, and a class with no virtual members |
| R-009 | **Quantified.** 1.00 GB available against 25–40 GB required, plus an elevation requirement. Now carried as R4-003 as well |
| R-010 | **Mechanism identified.** First-match selection at `FirmwarePluginManager.cc:91-102`, decided by static-initialisation order |
| R3-001 | **Mitigation specified.** Properties 5 to 7 of the twelve architectural-property tests |
| R3-007 | **Two conversions specified.** `IF-PAY-015` and `IF-AUT-012`, as properties 10 and 11 |

## What Phase 4 did not add a risk for

| Considered | Why no risk was raised |
| --- | --- |
| A planned seam not existing | **All twelve seams were found present.** The §28 condition did not trigger |
| QGC licence constraints affecting the architecture | Nothing found beyond the known Qt licensing question, R-001 |
| Plugin registration being impossible through the planned seam | It works as Phase 1 described; self-registration confirmed |
| QML override behaviour differing from Phase 3.1 | It matches. The C++ case is a **narrowing**, raised as R4-005, not a contradiction |
| A Phase 3 requirement being unimplementable without inventing evidence | None found. The four findings are gaps, not impossibilities |

Five of the ten §28 stop-and-report conditions were checked and did not trigger. The two that did are
reported: the build-environment conflict, and the fact-group architecture differing materially from
earlier findings.

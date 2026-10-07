# Open Questions

Questions the architecture work newly raised. The 34 Phase 1 questions and their triage are in
[phase-2-open-question-resolution.md](phase-2-open-question-resolution.md); this register holds only
what Phase 2 added.

Numbering continues from Phase 1: Q-035 through Q-043.

## Vocabulary

Unchanged from [phase-2-open-question-resolution.md](phase-2-open-question-resolution.md).

| Route | Resolved by |
| --- | --- |
| `CODE` | Reading or running code |
| `HARDWARE` | A hardware or firmware decision |
| `PRODUCT` | A product or operations decision |
| `HUMAN` | A named person deciding and recording it |
| `EXTERNAL` | A party outside engineering, such as counsel |

| Impact | Meaning |
| --- | --- |
| `ARCH-BLOCKING` | The architecture cannot be ratified without it |
| `ARCH-SHAPING` | The architecture accommodates any answer, but the answer changes a structure |
| `IMPL-BLOCKING` | Implementation of the affected area cannot start without it |
| `DEFERRABLE` | Can wait; the obligation is stated and only a parameter is missing |

## Summary

| Impact | Count | Questions |
| --- | --- | --- |
| `ARCH-BLOCKING` | 0 | — |
| `ARCH-SHAPING` | 2 | Q-037, Q-042 |
| `IMPL-BLOCKING` | 2 | Q-035, Q-038 |
| `DEFERRABLE` | 5 | Q-036, Q-039, Q-040, Q-041, Q-043 |
| Total | 9 | |

**No Phase 2 question is architecture-blocking.** Together with the Phase 1 triage, which also
left zero architecture-blocking questions, the architecture can be ratified.

Two questions are resolvable by a single person in a single sitting — Q-035 by running a build,
Q-038 by someone deciding — and both are implementation-blocking. They should be taken first.

## Q-035 Where do Aerora tests live, and can the harness reach them?

| Field | Value |
| --- | --- |
| Route | `CODE` |
| Impact | `IMPL-BLOCKING` |
| Blocks | Every architectural-property test |
| Related | R-021, `AERORA-BUILD-REQ-006`, `AERORA-TEST-REQ-009` |

Aerora code belongs in a single overlay directory. The test tree is a separate subdirectory added
to the build after the overlay, and the preprocessor definition that marks a test-capable build is
applied between the two. Whether a test registered from overlay sources is enumerated by the
harness therefore depends on build-ordering behaviour that has not been verified, because the host
this architecture was written on has no build toolchain.

| Candidate | Trade-off |
| --- | --- |
| Overlay source set | All Aerora code stays in the overlay; reachability unconfirmed |
| An Aerora-owned subdirectory of the test tree | Certainly works; needs a documented exception to the single-overlay rule |

**How to resolve.** Configure with testing enabled, build, add one trivial Aerora test, and check
it appears in the registered-test listing. An hour at most.

**What it unlocks.** The nine architectural-property tests, which are the mechanism by which several
structural guarantees stay true over time. Until this is answered, those guarantees rest on review
discipline alone.

## Q-036 What are the per-command-class corroboration windows?

| Field | Value |
| --- | --- |
| Route | `PRODUCT` then `HARDWARE` |
| Impact | `DEFERRABLE` |
| Blocks | Command staleness thresholds |
| Related | `AERORA-GCS-REQ-016`, `AERORA-PAYLOAD-REQ-011` |

The eight-state command model marks a command stale when it has been accepted but has not become
effective within its corroboration window. The architecture commits to the obligation, to the
window being per command class rather than global, and to staleness being visible. It does not set
the values.

A window that is too short reports false staleness on a slow link; too long and an operator waits
for a command that will never take effect.

**How to resolve.** Each Aerora command class declares its window alongside its resume behaviour in
the interface control document. The values depend on actuator response times and on link latency
budgets, so firmware and hardware input is needed after the product decision.

**What it unlocks.** Boundary tests for the staleness rule. The rule itself is testable without the
values, using injected timing.

## Q-037 What provides integrity protection for the local audit record store?

| Field | Value |
| --- | --- |
| Route | `PRODUCT` then `CODE` |
| Impact | `ARCH-SHAPING` |
| Blocks | The audit record store's implementation |
| Related | `AERORA-GCS-REQ-028`, `AERORA-SEC-REQ-019`, R-005 |

The architecture requires the local audit record store to be integrity-protected, on the ground
that a record that can be edited is not evidence. It does not specify the mechanism.

The choices differ in what they actually defend against, which is the part that needs a decision
rather than an implementation:

| Mechanism | Defends against |
| --- | --- |
| A hash chain over records | Undetected modification or deletion of a record in the middle |
| Signing with a station key | The above, plus attribution to a station |
| Append-only file permissions | Casual modification, not a determined local user |
| Upload to the backend as the integrity anchor | Modification after upload, but not before |

The last is tempting and partly circular: it makes integrity depend on the cloud, which the
architecture otherwise keeps off every critical path. It is acceptable as a *second* anchor and not
as the only one.

Note the interaction with R-005: a station signing key has the same custody problem as a MAVLink
signing key, so choosing the signing mechanism inherits that unresolved gap.

**How to resolve.** A product decision on what the audit trail must withstand — casual
modification, a determined local user, or a determined local user with the station key — followed
by the corresponding implementation.

**What it unlocks.** The audit record store, and with it the compliance requirements that depend on
it.

## Q-038 What is the upstream merge cadence, and who owns it?

| Field | Value |
| --- | --- |
| Route | `HUMAN` |
| Impact | `IMPL-BLOCKING` |
| Blocks | The upstream integration procedure |
| Related | `AERORA-BUILD-REQ-007`, `AERORA-UI-REQ-006`, R-026, Q-003 |

Each upstream integration carries five obligations that no tool prompts for: a drift review of
every overridden file, a seam signature and semantics review, a startup self-check run, a
settings-version review, and a dialect compatibility review.

Those obligations need a cadence and a named owner, because an unowned periodic procedure does not
happen.

| Cadence | Trade-off |
| --- | --- |
| Every upstream release | Predictable; larger diffs, more drift per merge |
| Monthly | Smaller diffs; merges land mid-development |
| On demand, when a needed fix appears | Minimal effort; drift accumulates invisibly between merges |
| Never, after the fork point | No merge cost; no upstream fixes either, which is the worst option for a long-lived product |

**How to resolve.** A named person decides, and the decision is recorded next to the fork point.
This is a fifteen-minute decision that nobody will make unless it is assigned.

**What it unlocks.** The merge procedure, and the drift-checking tool's trigger point.

## Q-039 What is the log retention and upload policy?

| Field | Value |
| --- | --- |
| Route | `PRODUCT` |
| Impact | `DEFERRABLE` |
| Blocks | Retention implementation |
| Related | `AERORA-COMPLY-REQ-005`, `AERORA-AWS-REQ-010`, R-022 |

The architecture states what retention must not be able to do: delete an audit record before it has
been uploaded or exported, bound disk use during a flight, or delete without recording the
deletion. It does not set periods, size limits or upload triggers.

Three distinct retention periods are probably needed, and conflating them would be the error:

| Record | Likely driver |
| --- | --- |
| Application diagnostic log | Engineering convenience; shortest |
| Telemetry record | Operational analysis and incident investigation |
| Audit record | Regulatory or contractual obligation; longest |

**How to resolve.** A product and operations decision, informed by whatever regulatory position
applies to the audit trail.

**What it unlocks.** The retention implementation. The constraints are already testable.

## Q-040 What is the Aerora visual design language?

| Field | Value |
| --- | --- |
| Route | `PRODUCT` |
| Impact | `DEFERRABLE` |
| Blocks | Final interface appearance |
| Related | `AERORA-UI-REQ-008`, Q-006 |

The architecture specifies eight presentation rules that are behavioural rather than aesthetic:
effective state primary, stale never shown as current, cloud and link visually unrelated, proposals
subordinate, hide versus disable, and so on. It specifies no palette, typography, iconography or
layout.

QGroundControl provides a palette with a per-colour override seam, so the mechanism exists.

**How to resolve.** A product and design decision. Note that it interacts with Q-006: removing
QGroundControl branding and artwork is partly the same work.

**What it unlocks.** Nothing structural. This is genuinely deferrable, and attempting it early would
produce rework once the presentation rules meet real screens.

## Q-041 How deeply is an uploaded mission verified against the plan file?

| Field | Value |
| --- | --- |
| Route | `PRODUCT` |
| Impact | `DEFERRABLE` |
| Blocks | Verification implementation |
| Related | `AERORA-MISSION-REQ-007`, `AERORA-MISSION-REQ-001` |

Plan-file authority means an uploaded mission that has not been compared against the plan file is
presented as unverified. How thorough that comparison is remains open.

| Depth | Catches |
| --- | --- |
| Item count only | Truncation |
| Count plus command sequence | Truncation and reordering |
| Full parameter comparison | The above, plus a corrupted or altered parameter |
| Read back from the vehicle and compare | The above, plus a vehicle-side storage fault |

The fourth is the only one that detects a fault in the vehicle's own mission store, and it is also
the only one that costs a full mission download on every upload.

**How to resolve.** A product decision weighing upload time against the class of fault worth
detecting. The honest default is the third option, with the fourth available on demand.

**What it unlocks.** The verification implementation. The unverified-presentation requirement is
testable without it.

## Q-042 What is the cloud synchronisation conflict policy?

| Field | Value |
| --- | --- |
| Route | `PRODUCT` |
| Impact | `ARCH-SHAPING` |
| Blocks | Mission library and configuration synchronisation |
| Related | `AERORA-AWS-REQ-007`, `AERORA-BUILD-REQ-012`, Q-030 |

Per-feature degradation means local work continues during a cloud outage. When connectivity
returns, local and remote state can disagree, and the architecture does not say how that is
resolved.

| Case | Conflict |
| --- | --- |
| Mission library | A plan edited locally and also edited by another station |
| Vehicle configuration | A locally applied configuration superseded remotely while offline |
| Fleet records | A local observation contradicting the registry |

Three candidate policies, with different failure modes:

| Policy | Failure mode |
| --- | --- |
| Last write wins | Silently discards someone's work |
| Local always wins | A station that was offline for a week overwrites a week of fleet decisions |
| Surface the conflict to the operator | No data loss; requires an interface and an operator decision at an inconvenient moment |

**How to resolve.** A product decision. The architecture's bias is the third policy for anything
that represents intent — plans and configuration — because silently discarding intent is the one
failure that cannot be recovered from.

**What it unlocks.** Mission library and configuration synchronisation. It is architecture-shaping
because the third policy requires an interface the other two do not.

## Q-043 What are the payload calibration and diagnostics interfaces?

| Field | Value |
| --- | --- |
| Route | `HARDWARE` then `PRODUCT` |
| Impact | `DEFERRABLE` |
| Blocks | The payload setup and diagnostics pages |
| Related | `AERORA-PAYLOAD-REQ-013`, Q-023, Q-009 |

The baseline's generic payload interface names calibration and diagnostics as two of its ten
elements. Their content is undefined, and the architecture adopts them as shape without populating
them — the same boundary [payload-architecture.md](payload-architecture.md) draws for payload state.

| Element | What is needed |
| --- | --- |
| Calibration | Which procedures exist, what the operator does, what the vehicle reports during and after |
| Diagnostics | What the payload reports, and which conditions warrant operator attention |

The architectural commitment already made: calibration is an operator-initiated procedure that is
**observed, not simulated** — the GCS does not model calibration progress, it displays what the
payload reports.

**How to resolve.** Follows Q-023. Once the actuator model exists, the calibration procedures and
diagnostic conditions follow from it.

**What it unlocks.** The payload setup and diagnostics pages. The capability-gating mechanism that
decides whether they appear at all is already specified.

## Priority

Across both registers, the new questions rank as follows relative to the twelve already ranked in
[phase-2-open-question-resolution.md](phase-2-open-question-resolution.md).

| Rank | Question | Why here |
| --- | --- | --- |
| Alongside rank 1-4 | Q-035 test reachability | Resolvable in an hour; gates nine tests |
| Alongside rank 1-4 | Q-038 merge cadence and owner | A fifteen-minute decision nobody will make unless assigned |
| Mid | Q-037 audit integrity mechanism | Architecture-shaping, and it interacts with the R-005 key-custody gap |
| Mid | Q-042 synchronisation conflict policy | Architecture-shaping; needs an interface decision |
| Late | Q-036, Q-039, Q-041, Q-043 | Parameters for obligations already stated and already testable |
| Last | Q-040 visual design language | Early work here would be rework |

The pattern is the same one Phase 1's triage showed: the cheapest questions are often the ones
blocking the most work, and they stay open because nobody owns them rather than because they are
hard.

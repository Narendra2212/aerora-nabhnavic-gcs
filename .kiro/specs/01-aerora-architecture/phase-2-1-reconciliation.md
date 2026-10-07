# Phase 2.1 Reconciliation

Every contradiction, overclaim and count error found in the Phase 2 document set, and what was done
about each.

Phase 2.1 wrote no new architecture. It corrected the record so that a reviewer ratifying the
architecture is ratifying what the evidence supports rather than what the prose asserted.

## Method

Phase 2 was written over several sittings, and the defects are the kind that come from that: a
document written early contradicted by one written later, a derivation hardening into a stated fact
as it was restated across documents, and arithmetic that drifted when requirements were added.

| Principle applied | Consequence |
| --- | --- |
| Do not silently overwrite contradictory history | Superseded text is retained verbatim under an amendment heading |
| Separate what was verified from what was reasoned | Three categories now: verified in source, asserted in a baseline, derived in Phase 2 |
| Count mechanically, never from memory | Every published count in this phase was produced by a script |
| Do not close an open question to tidy a document | Q-019 was downgraded, not closed; eighteen items are held explicitly open |

## Summary of changes

| # | Item | Kind | Documents touched |
| --- | --- | --- | --- |
| 1 | Requirements register header and counts | Count error | 1 |
| 2 | R-016 gate and class | Contradiction between documents | 5 |
| 3 | R-002 payload state retention | Overclaim | 7 |
| 4 | Q-019 resolution status | Overclaim | 1 |
| 5 | Ratification classification | New material | 1 new |
| 6 | Hardware and firmware evidence | New material | 1 new |
| 7 | Core edit terminology | Inconsistent phrasing | 6 |
| 8 | Verification method counts | Count error | 1 |

No architectural decision was reversed. Two were qualified.

## 1. Requirements register header

### Problem

The header read "114 requirements across 14 identifier domains". Both numbers were stale: six
testing requirements were added later in Phase 2, and the testing domain brought the count to
fifteen. The body of the document carried the correct totals, so the document contradicted itself
on its first line.

### Resolution

The header now carries an explicit count block: 120 published, 119 active, 1 withdrawn, 15 domains,
plus the Phase 1 figure and the combined total. All four numbers were produced by counting table
rows mechanically.

The header also now distinguishes the `Status` field from the ratification class, because a reader
seeing both could reasonably assume one derived from the other.

## 2. R-016: a document contradicting itself

### Problem

Three positions existed across the set:

| Where | Position |
| --- | --- |
| Phase 1 risk register | `MEDIUM`, "Qt6::HttpServer purpose is untraced" |
| `phase-2-risk-gates.md`, written early in Phase 2 | `MEDIUM` at `G-IMPL`, and explicitly: "still untraced. Phase 2 did not investigate it" |
| `cloud-architecture.md`, written later in Phase 2 | Fully traced — linked for two type aliases, no server constructed, no listening socket bound |
| `traceability.md` | A reconciliation note saying the open point was answered, but leaving the gate assignment unchanged |

So one document said the dependency was uninvestigated while another in the same set reported the
investigation. The traceability note acknowledged the conflict without resolving the gate.

### Resolution

| Document | Change |
| --- | --- |
| `phase-2-risk-gates.md` | Original text **retained verbatim** as a block quote under "Original Phase 2 assessment, retained as written", followed by a Phase 2.1 amendment with the traced evidence and line references. Class `MEDIUM` → `LOW`, gate `G-IMPL` → `G-OPEN` |
| `phase-2-risk-gates.md` summary table | Counts corrected, with the pre-amendment counts stated so the change is visible |
| `phase-2-risk-gates.md` per-risk table | Row marked "amended in Phase 2.1" |
| `cloud-architecture.md` | New subsection stating that the finding supersedes the earlier assessment, with a link to the amendment |
| `risk-gates.md` | Combined gate table corrected; pre-amendment row stated |
| `traceability.md` | Reconciliation note replaced with a four-row phase-by-phase table; gate and mitigation columns corrected |
| `README.md` | Entry condition reworded from "the HTTP server dependency decision" to "dependency-hygiene decision" |

### What was deliberately not done

R-016 was **not closed**. The security premise behind the `MEDIUM` rating is disproved — there is no
endpoint — but two things remain: a hygiene decision on whether to drop the dependency, carried as
Q-027, and an assurance obligation that keeps the finding true, carried as `AERORA-AWS-REQ-014` with
a startup self-check and a test.

Closing the risk would have removed the assurance obligation, which is the part that matters. A
linked HTTP server with no listener today is one contributor away from having one.

## 3. R-002: an unproven behaviour stated as fact

This is the substantive correction of Phase 2.1.

### Problem

Phase 2 concluded that the resume-filter exposure was resolved by putting payload state authority
with the payload controller. The reasoning was sound. The **status** of the conclusion was not: the
documents stated the enabling behaviour in the indicative mood, as something Aerora already had.

Collected instances:

| Document | Wording |
| --- | --- |
| `architecture-decisions.md` | "The payload controller is authoritative for payload state **and retains it across a mission interruption**" |
| `architecture-decisions.md` | "the mechanism was built on an assumption — that the vehicle retains nothing — **which is false for Aerora**" |
| `architecture-decisions.md` | "the payload controller **retains** state, so there is nothing to replay" |
| `mission-architecture.md` | "**Aerora reality:** payload controller retains state" |
| `mission-architecture.md` | "The premise behind the replay **is false for Aerora**" |
| `mission-architecture.md` | "Payload state **survives** a mission replacement" as a requirement with no evidence column |
| `payload-architecture.md` | "The premise **is wrong for Aerora**, which has a payload controller" |
| `phase-2-risk-gates.md` | "payload configuration state **is** the payload controller's to hold"; layering table row "Owns **and persists**" |
| `phase-2-open-question-resolution.md` | "so that assumption **is wrong for Aerora**" |
| `README.md` | "For Aerora that assumption **is wrong**" |

Nine documents, one unsupported premise, stated with increasing confidence as it propagated.

### What the evidence actually shows

| Checked | Result |
| --- | --- |
| Baseline 10 on retention, persistence, non-volatile storage, survival across resume | **No match.** Searched the document; the terms do not appear |
| Baseline 13 | Assigns payload-local safety and interlocks to the payload controller — an **ownership** statement |
| Baselines 07 and 11 | Require payload state to be **reported** as telemetry |
| Firmware model | `Payload_Interface`, `Painting_System`, `Payload_Actuator_Drivers` present; actuator behaviours are placeholders |
| Bench or flight evidence | None observed |

**The error was conflating ownership with persistence.** A payload controller can own payload state
and lose it on a reset, a power cycle, a mode change or a mission replacement. Whether it retains
state is a firmware design decision that has not been made.

Baseline 13 states the rule that should have been applied: where the firmware model contains a
placeholder, the item is unresolved engineering work rather than a verified capability.

### Resolution

The reclassification keeps both rejections and downgrades only the status of the preferred answer:

| Element | Phase 2 | Phase 2.1 |
| --- | --- | --- |
| QGC core modification of the resume filter | Rejected | **Rejected — unchanged** |
| GCS-side payload state replay | Rejected | **Rejected — unchanged** |
| Payload-controller state retention | Stated as Aerora reality | **Preferred architecture, evidence status `NOT-EVIDENCED`** |
| R-002 class and gate | `HIGH` at `G-ARCH` | `HIGH` at `G-ARCH` — unchanged, now with an explicit evidence gate |
| R-002 mitigation status | Mitigated | **Conditionally mitigated**; the GCS fallback carries it until evidence exists |
| Fallback | Not stated | Plan constraint plus resume-time operator warning |

Document changes:

| Document | Change |
| --- | --- |
| `mission-architecture.md` | Section retitled "The proposed resolution". New subsection "This is a requirement on the vehicle, not an established fact" with the evidence check. Requirements table gains an evidence-status column. New subsection "If retention is not implemented" naming the fallback |
| `payload-architecture.md` | The three-fact table now distinguishes two verified source findings from one proposal, and states what Phase 2 overstepped |
| `architecture-decisions.md` | ADR-005 decision split into an ownership part and a retention part; new "Evidence caveat, added in Phase 2.1" with a four-row basis table; consequences table records transfer rather than resolution; the closing pattern restated correctly |
| `phase-2-risk-gates.md` | New "Phase 2.1 reclassification" subsection naming the specific clause whose inference does not follow |
| `phase-2-open-question-resolution.md` | Q-019 downgraded; the Q-023 cross-reference corrected |
| `README.md` | Phase 2.1 caveat added to the summary |
| `requirements.md` | `AERORA-PAYLOAD-REQ-007` rationale rewritten; verification method `ANALYSIS` → `DEMO`. `AERORA-MISSION-REQ-003` rationale rewritten to hold regardless of the evidence; method `ANALYSIS` → `TEST` |

### The fallback, and why it is not a new requirement

The adopted fallback has two parts. The operator-warning part is already covered by
`AERORA-MISSION-REQ-004`. The plan-constraint part is **not** given a requirement identifier, for
two reasons: the register is fixed at 120 published by directive, and a constraint that only applies
if the evidence lands badly is a conditional obligation rather than a standing one.

It is recorded in `AERORA-MISSION-REQ-003`'s rationale, in the evidence matrix, and in the Phase 3
entry criteria. If the evidence lands badly, it becomes a requirement in Phase 3 scoping.

## 4. Q-019 resolution status

Q-019 — how Aerora payload state is preserved across mission resume — was listed as `RESOLVED` in
Phase 2 with a note that the remainder was a `PRODUCT` confirmation tracked under Q-023.

Three problems: the resolution rested on the overclaim above; a firmware persistence design is a
`HARDWARE` matter, not a `PRODUCT` confirmation; and folding it into Q-023 was wrong because the
actuator model decides *what* state exists while retention decides *whether it survives*. Q-023
could be answered in full with retention still undesigned.

| Field | Phase 2 | Phase 2.1 |
| --- | --- | --- |
| Status | `RESOLVED` | `PARTIALLY RESOLVED` |
| Route for the residual | `PRODUCT`, under Q-023 | `HARDWARE`, tracked separately in the evidence matrix |
| Count of questions resolved in Phase 2 | 6 | **5** |

What remains resolved: the filter's bounded scope, the absence of a seam, and both rejections.

## 5. Ratification classification — new

All 120 published requirements classified by the gating condition for implementing and verifying
them, in [ratification.md](ratification.md).

| Class | Count |
| --- | --- |
| `CONFIRMED` | 29 |
| `PROPOSED — ACCEPTABLE` | 59 |
| `HARDWARE/FIRMWARE EVIDENCE REQUIRED` | 21 |
| `PRODUCT DECISION REQUIRED` | 10 |
| `REJECT/WITHDRAW` | 1 |

Counts produced by script, cross-checked against the register so that every registered requirement
is classified exactly once and no classification refers to a non-existent identifier.

**No `PROPOSED` requirement was promoted to `CONFIRMED`.** The `Status` field is unchanged except
for `AERORA-GCS-REQ-015`, where a factual error was corrected — see change 8.

## 6. Hardware and firmware evidence matrix — new

[hardware-firmware-evidence-matrix.md](hardware-firmware-evidence-matrix.md) covers twenty-two
vehicle-side items, each with requirement, architecture assumption, evidence source, evidence
status, confidence, unresolved question, affected requirements and affected risks.

| Confidence | Count |
| --- | --- |
| `CONFIRMED` | 1 |
| `PROBABLE` | 10 |
| `ASSUMED` | 3 |
| `UNKNOWN` | 8 |

One item of twenty-two is confirmed. The three `ASSUMED` rows are the ones that read like facts in a
design document: radio USB identity, the MAVLink dialect, and payload state retention.

The matrix also records eight vehicle-side unknowns the architecture is **insulated** from, so that
it is not read as a list of blockers. The genuinely load-bearing unevidenced rows are three.

## 7. Core edit terminology

Six variants were in use: "currently zero are planned", "zero core edits", "the single core-edit
candidate", "one identified candidate", "None is planned", and "zero core modifications".

Standardised to two sentences, used verbatim wherever a global claim is made:

> **Zero QGC core edits are required by the current architecture.**
>
> **One QGC core edit remains a possible fallback for signing-key storage if upstream extensibility
> is unavailable.**

Applied in `README.md` (terminology table, principle 7, and a new standard-formulation section),
`system-architecture.md` (layer table and relationship table), `phase-2-risk-gates.md` (R-014),
`security-architecture.md`, `architecture-decisions.md` (ADR-008) and `traceability.md`.

Scoped claims — "zero core edits for transport", "fifteen modules attach with zero core edits" —
were left alone. They are accurate about their own scope and are not global claims.

## 8. Verification method counts, and one source error

Two requirement records changed verification method as a consequence of change 3:
`AERORA-PAYLOAD-REQ-007` from `ANALYSIS` to `DEMO`, because bench verification on real hardware is
what the requirement now needs, and `AERORA-MISSION-REQ-003` from `ANALYSIS` to `TEST`, because the
prohibition is testable once the statement no longer depends on the vehicle.

Those were the only two `ANALYSIS` requirements, so that method now has a count of zero. The method
definition is retained — a future requirement may need it — and the summary table reflects the real
counts.

`AERORA-GCS-REQ-015` previously cited ADR-010, the failsafe boundary, as a source for the eight-state
command model. That was the wrong decision record; the state set comes from the Phase 2 directive.
Source corrected to `DIR` and status to `CONFIRMED`, which is why status `CONFIRMED` is 28 in the
register.

## Amendment history

| Phase | Document | Nature |
| --- | --- | --- |
| Phase 2 | `phase-2-risk-gates.md` | R-016 assessed as untraced at `G-IMPL` |
| Phase 2 | `cloud-architecture.md` | R-016 traced, contradicting the above |
| Phase 2 | `traceability.md` | Contradiction acknowledged, gate left unchanged |
| **Phase 2.1** | `phase-2-risk-gates.md` | R-016 amended to `LOW` at `G-OPEN`; original text retained verbatim |
| Phase 2 | Nine documents | Payload state retention stated as established |
| **Phase 2.1** | Seven documents | Retention reclassified as `NOT-EVIDENCED` with a fallback; ADR-005 qualified |
| Phase 2 | `phase-2-open-question-resolution.md` | Q-019 `RESOLVED` |
| **Phase 2.1** | Same | Q-019 `PARTIALLY RESOLVED`; resolved count 6 → 5 |
| Phase 2 | `requirements.md` | Header stale at 114 and 14 domains |
| **Phase 2.1** | Same | Header corrected to 120, 119, 1, 15 |

Superseded text is retained in every case. No Phase 1 document was modified in Phase 2 or Phase 2.1;
the Phase 1 set remains a frozen, human-reviewed baseline.

## What Phase 2.1 did not change

| Unchanged | Why |
| --- | --- |
| Any architectural decision's direction | Phase 2.1 is reconciliation, not redesign |
| The rejection of QGC core modification for the resume filter | Still correct; the path has no seam |
| The rejection of GCS-side payload replay | Still correct; it would put authoritative state in the ground station |
| The four `G-ARCH` risk assignments | Unchanged |
| The signing-key position | Reaffirmed as release-critical |
| Any `Status` field except one factual correction | Promotion requires evidence or human approval |
| The eighteen items held open | Explicitly held, not narrowed |
| QGroundControl source | Not modified in any phase |

## Residual inconsistency

One remains, recorded rather than fixed.

`phase-2-risk-gates.md` and `phase-2-open-question-resolution.md` are **reviews of Phase 1 artifacts**
written at a particular moment. Phase 2.1 amended them in place where they contradicted later
findings, which makes them partly reviews and partly living documents. The alternative — freezing
them and putting all amendments elsewhere — would have scattered the R-016 and R-002 records across
more documents than a reader could follow.

The compromise: amendments are confined to clearly-labelled Phase 2.1 sections, and superseded text
is quoted rather than deleted. A reader can reconstruct what each phase believed and when.

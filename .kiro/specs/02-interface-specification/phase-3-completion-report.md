# Phase 3 Completion Report

Interface Control and System Specification for the Aerora Universal Ground Control Station.

| Field | Value |
| --- | --- |
| Phase | 3 — Interface Control and System Specification |
| QGC baseline commit | `9459a4b57986f40df8bdc193d5453c0596fbb416` |
| Branch | `master` |
| QGC source files modified | **0** — `git diff --stat HEAD` is empty |
| Implementation started | **No** |
| Documents produced | 25, including this report |
| Review status | **AWAITING HUMAN REVIEW** |

---

## 1. Scope and mandate

Phase 3 was directed to transform the ratified and conditionally ratified Phase 2 and 2.1
architecture into a precise, implementation-ready interface control and system specification.

| Instruction | Compliance |
| --- | --- |
| Convert architecture into interface contracts | 240 interfaces across 16 domains, in 16 defining documents |
| Record evidence for every interface | Every interface carries one of eight evidence codes |
| State what is open and what would resolve it | 56 interfaces `OPEN`; nine new questions with named owners and evidence required |
| Name a verification method per interface | 241 assignments for 240 interfaces; zero results claimed |
| Preserve the Phase 2.1 evidence discipline | No evidence code was upgraded; superseded positions retained |
| Do not create documents to inflate the count | 25 against a planned 25; two planned documents define no interfaces because they are models, and say so |

What Phase 3 is **not**: an approval. An interface specified here is not thereby ratified. Several
were specified precisely so that the missing thing they depend on becomes visible and countable.

---

## 2. Deliverables produced

| # | Document | Bytes | Interfaces defined |
| --- | --- | --- | --- |
| 00 | `00-phase-3-readme.md` | 12,745 | — |
| 01 | `01-gcs-system-icd.md` | 14,764 | 8 |
| 02 | `02-fc-gcs-icd.md` | 16,769 | 24 |
| 03 | `03-mavlink-icd.md` | 17,570 | 28 |
| 04 | `04-radio-link-icd.md` | 11,779 | 9 |
| 05 | `05-gpu-fc-icd.md` | 13,618 | 8 |
| 06 | `06-payload-icd.md` | 15,565 | 15 |
| 07 | `07-power-telemetry-icd.md` | 11,198 | 12 |
| 08 | `08-vehicle-identity-icd.md` | 12,130 | 7 |
| 09 | `09-command-authority-icd.md` | 16,274 | 12 |
| 10 | `10-mission-icd.md` | 14,508 | 16 |
| 11 | `11-telemetry-icd.md` | 14,663 | 13 |
| 12 | `12-parameter-icd.md` | 9,908 | 10 |
| 13 | `13-security-icd.md` | 23,939 | 21 |
| 14 | `14-audit-logging-icd.md` | 23,866 | 24 |
| 15 | `15-cloud-integration-icd.md` | 19,546 | 16 |
| 16 | `16-ui-state-icd.md` | 21,478 | 17 |
| 17 | `17-capability-model.md` | 15,942 | **0 — a model** |
| 18 | `18-error-degradation-model.md` | 16,891 | **0 — a model** |
| 19 | `19-interface-traceability.md` | 49,120 | 0 — maps all 240 |
| 20 | `20-phase-3-open-questions.md` | 21,520 | — |
| 21 | `21-phase-3-risks.md` | 20,241 | — |
| 22 | `22-phase-3-verification-plan.md` | 33,498 | 0 — lists all 240 |
| 23 | `23-build-readiness.md` | 17,513 | — |
| — | `phase-3-completion-report.md` | this document | — |

**445,045 bytes across the 24 specification documents**, plus this report, in
`.kiro/specs/02-interface-specification/`.

Cumulative across the programme: Phase 1 produced 17 documents, Phase 2 produced 26, Phase 2.1
added 3 for 29 totalling 514,324 bytes, and Phase 3 adds 25. **71 documents in `.kiro/specs`.**

---

## 3. Source hierarchy applied

The directive's ten-rank hierarchy was applied as given, strongest first.

| Rank | Source | Interfaces resting primarily on it |
| --- | --- | --- |
| 1 | Confirmed Aerora requirements | Woven throughout; 120 of 120 Phase 2 requirements referenced |
| 2 | Approved product baseline | 47 `BASELINE-ASSERTED` |
| 3 | Approved Phase 2 architecture | 46 `GCS-DECIDED` |
| 4 | Hardware and PCB evidence | **5** `ARTIFACT-OBSERVED` |
| 5 | Simulink and firmware evidence | 17 `MODEL`, 7 `PLACEHOLDER` |
| 6 | Approved ICD evidence | **None. No Aerora ICD exists** |
| 7 | QGroundControl source behaviour | 89 `GCS-VERIFIED` |
| 8 | Official regulations | None invoked |
| 9 | Approved proposals | Carried as `PROPOSED` status, not as evidence |
| 10 | Inference | **Nothing rests on it.** 37 interfaces are marked `NOT-EVIDENCED` rather than inferred |

Rank 6 being empty is the single most important line in this table. There is no approved Aerora
interface control document, which is why Phase 3 wrote one and why its vehicle-side rows are
statements of what the ground station needs rather than records of what the vehicle does.

---

## 4. Interface inventory and identifier scheme

Scheme: `IF-<domain>-<nnn>`, domain-scoped rather than document-scoped, so splitting a document
later renumbers nothing.

| Domain | Count | Subject |
| --- | --- | --- |
| `MAV` | 28 | MAVLink protocol contract |
| `FC` | 24 | Flight controller to ground station |
| `LOG` | 24 | Audit and logging |
| `SEC` | 21 | Security |
| `UIS` | 17 | Interface state |
| `MIS` | 16 | Mission |
| `CLD` | 16 | Cloud integration |
| `PAY` | 15 | Payload |
| `TLM` | 13 | Telemetry model |
| `AUT` | 12 | Command authority |
| `PWR` | 12 | Power telemetry |
| `PAR` | 10 | Parameters |
| `RAD` | 9 | Radio link |
| `GPU` | 8 | Companion computer |
| `SYS` | 8 | System boundary |
| `VID` | 7 | Vehicle identity |
| **Total** | **240** | — |

Granularity was chosen at the **boundary contract**, not the field. A per-field scheme was
considered and rejected: it would have produced roughly 500 items, most of them unspecifiable
because the dialect does not exist.

### Sixteen interfaces specify that nothing crosses

A prohibition stated as an interface is checkable; a prohibition stated as a principle is not.

| Prohibited path | Interfaces |
| --- | --- |
| Ground station to companion | `IF-SYS-005`, `IF-GPU-008` |
| Ground station to payload actuators | `IF-SYS-006`, `IF-PAY-015` |
| Ground station to power switching | `IF-SYS-007` |
| Inbound network connection | `IF-SYS-008`, `IF-CLD-003` |
| Ground-station-initiated failsafe action | `IF-AUT-012` |
| Ground-station-side payload state replay | `IF-MIS-015` |
| Modification of the QGC resume filter | `IF-MIS-016` |
| Security-module dependency on the cloud | `IF-SEC-010` |
| Advanced-UI toggle as an authorisation boundary | `IF-SEC-011` |
| Upload to a non-Aerora endpoint | `IF-LOG-018` |
| Local record deletion on upload | `IF-LOG-019` |
| Cloud on the command path | `IF-CLD-004` |
| Ground-station reporting feature | `IF-CLD-010` |

---

## 5. Evidence position

| Evidence status | Count | Meaning |
| --- | --- | --- |
| `GCS-VERIFIED` | 89 | Read in QGroundControl source |
| `BASELINE-ASSERTED` | 47 | A baseline document states it; no artifact cited |
| `GCS-DECIDED` | 46 | Within Aerora ground-station control |
| `NOT-EVIDENCED` | 37 | Neither artifact nor baseline statement |
| `MODEL` | 17 | Present in the Simulink firmware model |
| `PLACEHOLDER` | 7 | In the model, explicitly provisional |
| `ARTIFACT-OBSERVED` | **5** | A hardware or PCB artifact shows it exists |

Counts exceed 240 because some rows carry compound evidence — a verified transport with an
unevidenced payload, for instance.

**135 of 240 interfaces rest on something Aerora controls or has read.** The remaining 105 describe
a vehicle, and five of those are backed by an observed artifact. That asymmetry is recorded as
[R3-005](21-phase-3-risks.md) at `HIGH` severity, and it is the honest summary of this
specification's standing: strongest where it matters least to the aircraft integration, weakest at
the boundary where two teams meet.

No evidence code was upgraded in Phase 3. Payload state retention remains `NOT-EVIDENCED`
throughout, consistent with Phase 2.1 evidence matrix item 14.

---

## 6. Status position

| Status | Count | Share |
| --- | --- | --- |
| `PROPOSED` | 102 | 42% |
| `CONFIRMED` | 82 | 34% |
| `OPEN` | 56 | 23% |

Fifty-six `OPEN` interfaces means nearly a quarter of the set cannot be implemented as written
because a dialect, a firmware decision or a product decision does not exist. **Every one has a
stated fallback**, and the 184 `CONFIRMED` and `PROPOSED` interfaces are sufficient for a complete
ground station against the standard MAVLink set. The sequencing consequence is recorded as
[R3-004](21-phase-3-risks.md).

**No requirement was promoted from `PROPOSED` to `CONFIRMED`.** The Phase 2 distribution stands
unchanged at 28 `CONFIRMED`, 91 `PROPOSED`, 1 withdrawn.

---

## 7. Conflicts recorded and not resolved

Phase 3 found seven conflicts between sources. The directive's rule was applied in every case:
record the conflict, preserve the earlier position, name the authority needed to settle it.

| Conflict | Document | Authority required |
| --- | --- | --- |
| Signing default — upstream ships signing off; baseline posture implies on | 13 | Product and operations; Q-010, Q-011 |
| Key custody — overlay-only rule versus protection-at-rest requirement | 13 | ADR-008; upstream contribution preferred |
| Audit record integrity — required, no mechanism exists | 14 | Security and product; Q-037 |
| Retention versus evidence — a disconnected station eventually fills its disk | 14, 18 | Q-039 |
| Synchronisation conflict policy — local versus remote plan revision | 15 | Q-042 |
| Hide versus disable for an unauthorised operation | 16 | Q-029 |
| Zero core edits versus zero silent forks — two true claims about different things | 16 | Terminology; both retained |

Plus four conflicts inherited and re-recorded without reopening: the watchdog reporting path, the
authority mode value set, the payload controller's component status, and the high-voltage reporting
scope. Each became a Phase 3 question rather than a decision.

**Nothing was silently chosen.** The R-016 amendment from Phase 2.1 was carried forward as amended
and explicitly not reopened.

---

## 8. Open questions

### New

Nine, defined in [20-phase-3-open-questions.md](20-phase-3-open-questions.md), each with question,
reason, affected interfaces, affected requirements, affected risk, classification, decision owner
and evidence required.

| ID | Subject | Class | Owner |
| --- | --- | --- | --- |
| Q3-001 | Radio serial presentation and USB identity | `IMPL-BLOCKING` | Hardware |
| Q3-002 | Per-command request identifier on the wire | `ARCH-SHAPING` | Dialect |
| Q3-003 | Is the authority mode set closed at three sentinels | `ARCH-SHAPING` | Firmware, Product |
| Q3-004 | Does the watchdog report independently of the primary MCU | `ARCH-SHAPING` | Firmware |
| Q3-005 | Direct or summarised autonomy proposals | `ARCH-SHAPING` | Firmware |
| Q3-006 | Is the payload controller a distinct component | `ARCH-SHAPING` | Hardware, Firmware |
| Q3-007 | Is the high-voltage domain reported at all | `ARCH-SHAPING` | Firmware, Product |
| Q3-008 | Clock skew bound, and is skew recorded | `DEFERRABLE` | Firmware, Operations |
| Q3-009 | Capability presence versus availability | `IMPL-BLOCKING` | Firmware |

These differ in kind from the inherited questions. Inherited questions are mostly about **values**;
these are mostly about **shape** — who produces, who relays, whether a field exists at all. An
unanswered value leaves an interface unpopulated; an unanswered shape leaves two candidate
interfaces, and both are preserved.

### Inherited

**Zero closed.** All 43 existing questions are referenced and none redefined. Thirty-one are cited
in interface rows. The four carried by the most interfaces are Q-007 (17), Q-023 (15), Q-029 (10)
and Q-016 (10).

---

## 9. Risks

### New

Eight, defined in [21-phase-3-risks.md](21-phase-3-risks.md). No `BLOCKER`.

| ID | Risk | Severity | Gate |
| --- | --- | --- | --- |
| R3-001 | Three of four module dependency rules have no test assigned | `MEDIUM` | `G-IMPL` |
| R3-002 | Capability presence and availability are indistinguishable | `MEDIUM` | `G-INTEG` |
| R3-003 | The audit trail may name a relay as an event's originator | `HIGH` | `G-INTEG` |
| R3-004 | Fifty-six interfaces are `OPEN` pending external deliverables | `MEDIUM` | `G-IMPL` |
| R3-005 | The aircraft side rests on five observed artifacts | `HIGH` | `G-INTEG` |
| R3-006 | The split contract layout lets a reader take half a contract | `MEDIUM` | `G-OPEN` |
| R3-007 | Nine of sixteen prohibitions are assigned inspection alone | `MEDIUM` | `G-RELEASE` |
| R3-008 | Phase 3 gaps became questions, and a question carries no verification obligation | `MEDIUM` | `G-IMPL` |

Three of the eight are risks about this specification rather than about the system. R3-008 is the
uncomfortable one: Phase 3 was prohibited from creating requirement identifiers, so nine obligations
it found now live in questions, and a question is closed by a decision rather than by a test.

### Inherited

**Zero reclassified.** All 29 existing risks are referenced and none redefined. Twenty-two are cited
in interface rows; the seven that are not are legal, environment or process risks with no boundary
to cross, each listed with its reason in
[19-interface-traceability.md](19-interface-traceability.md).

R-002 touches fifteen interfaces — more than any other risk — which is the clearest signal in the
traceability data. One unevidenced payload retention behaviour propagates through payload, mission,
flight-controller, MAVLink and logging contracts.

---

## 10. Requirement coverage and traceability

| Measure | Value |
| --- | --- |
| Phase 2 requirements published | 120 |
| Referenced somewhere in Phase 3 | **120** |
| Referenced in an interface row | 111 |
| With no interface row | 9, each with a recorded reason |
| **New requirement identifiers created by Phase 3** | **0** |

The nine without an interface row are eight build, test and process requirements — carried by
[22-phase-3-verification-plan.md](22-phase-3-verification-plan.md) and
[23-build-readiness.md](23-build-readiness.md) — plus `AERORA-UI-REQ-009`, which Phase 2 withdrew as
a duplicate and which it would be an error to reference.

Eleven interfaces carry no requirement. Nine are upstream behaviour Aerora depends on and does not
impose, one is governed by the general telemetry rules, and one is a rejected core edit. **Phase 3
invented no requirement to fill any of them**; each is listed with its reason.

No Phase 1 identifier appears in any interface row. Phase 3 cites the Phase 2 canonical identifier
in every case, which is the condition `AERORA-UI-REQ-009` was withdrawn to establish.

[19-interface-traceability.md](19-interface-traceability.md) maps all 240 interfaces to
requirements, risks, questions, evidence, status and method, with architecture document, decision
record and evidence-matrix item mapped at domain granularity — stated as such, because claiming
per-interface precision for a domain-level decision would be false precision.

---

## 11. Verification position

| Method | Count | Environment |
| --- | --- | --- |
| `INTEGRATION` | 145 | Mock link, existing harness |
| `UNIT` | 36 | No vehicle, no link |
| `BENCH` | 26 | Real hardware on a bench |
| `STATIC` | 18 | Inspection |
| `DEMO` | 6 | Observed operation |
| `AUDIT` | 6 | Process check |
| `HIL` | 4 | Hardware in the loop |

**Zero results are recorded.** Nothing was built, no test was written, no test was run. Every entry
is an assignment.

The 30 `BENCH` and `HIL` assignments sit almost exactly on top of the 37 `NOT-EVIDENCED` rows, which
makes the plan a prioritisation rather than a list. Four bench observations are called out as
first, ordered by cost-to-value; the cheapest is attaching the radio to a host and recording its
device class and USB identifiers, which either confirms ADR-002 or returns six deferred transport
core edits.

Aerora adds no test framework, no base class, no label and no runner, per `AERORA-TEST-REQ-009`. Two
items gate the whole plan and are not resolved by it: whether Aerora tests in the overlay are
reachable by the runner at all (Q-035, R-021), and `MockLink` resource-lock sufficiency.

---

## 12. Build readiness position

Every version below was read from the checkout, not recalled.

| Component | Required |
| --- | --- |
| CMake | 3.25 minimum |
| C++ standard | C++20, required |
| Qt | 6.11.1, minimum 6.11.0, maximum pinned to 6.11.1 |
| Generator | Ninja via CMake presets |
| GStreamer | 1.28.4 default, 1.20.0 minimum |

**The development host cannot build.** `cmake`, `ninja`, a compiler, any Qt installation, `just` and
`pre-commit` are all absent, and there is no build directory. R-009, unchanged since Phase 1, and the
reason every claim in Phases 1 through 3 is a source-reading claim.

The live repository hazard: `origin` is upstream QGroundControl for **both fetch and push**. R-008.
The first implementation commit against the current configuration would target upstream.

[23-build-readiness.md](23-build-readiness.md) lists ten entry criteria for `G-IMPL`. Criteria 5 and
6 — Aerora test enumeration and resource alias precedence — are both cheap, both blocking, and both
impossible before criterion 3. **A build host is the single prerequisite for everything else.**

---

## 13. Capability model, and painting as a profile

Per the directive, painting is specified as the **first concrete product capability profile, not the
universal architecture**.

| Layer | Content |
| --- | --- |
| Vehicle | One aircraft, one identity |
| Product family | Painting, firefighting, agriculture, inspection |
| Capability categories | Vehicle, Payload, Mission, Telemetry, Command — five, because they have different owners and fail independently |

Eight resolution rules govern the model. The load-bearing one is rule 3: capability is never
inferred from the arrival of telemetry, the success of a command, or the presence of a component id.
That inference is correct nine times in ten, and the tenth time it presents controls for an actuator
that is not fitted.

The painting profile declares two fact groups, a product telemetry interface, a coverage pattern as
a layer-2 complex item, and content at three tier-2 QML seams. **Nothing in the hierarchy mentions
paint.** Units, ranges, enumerations, actuator identities, command sets, pattern geometry and
calibration procedures are all left `OPEN` — the six product value names carried from Phase 2 are
names only.

The second-product test is stated so it can be run rather than asserted: adding a product must touch
no layer-3 structure and require no change to the capability hierarchy. Painting is the only profile
that exists, so the test cannot be run yet, and the claim that the structure extends cleanly is
`SPECIFIED-NOT-VERIFIED` rather than demonstrated.

---

## 14. Error and degradation model

Twelve aircraft-reported conditions and eight ground-station conditions, each with detection,
indication, affected capability, response, recovery, authority change and evidence.

**Authority change is "none" in all twenty rows.** That is the system architecture restated rather
than a drafting shortcut: the ground station has no authority to lose when a link degrades and none
to gain when a failsafe fires.

| Deliberately omitted | Reason |
| --- | --- |
| Condition-to-failsafe-action mapping | Q-007; baseline 13 forbids the ground station assigning them |
| Automatic mode changes | `AERORA-SAFE-REQ-008` |
| Return-to-launch, land or hold triggers | Vehicle-owned |
| Severity ranking across conditions | Would be the ground station asserting which reported condition matters more |

`DEG-20` — override resolution failure at startup — is the only condition that refuses rather than
degrades, because it is the only one the operator cannot detect. An Aerora panel that never loaded
looks exactly like an Aerora panel with nothing to report.

---

## 15. Prohibitions honoured

Each directive prohibition, and the check that it held.

| Prohibition | Result |
| --- | --- |
| Do not modify QGroundControl source | **0 files.** `git diff --stat HEAD` empty |
| Do not implement C++, QML, security, auth, payload or failsafe code | No code written in any phase |
| Do not invent MAVLink message or command identifiers | None. Class B declared empty; class C names recorded as baseline candidates only |
| Do not invent enumerations | None. Authority set left open at three evidenced sentinels; payload, failsafe and power enumerations left `OPEN` |
| Do not invent USB vendor or product identifiers | None. Q3-001 asks for them |
| Do not invent fleet identity | None. Identity value is opaque and unparsed; Q-016 open |
| Do not invent power transitions | None. Baseline 49 requires separate engineering |
| Do not invent operator roles | None. Q-029 open; the model accommodates any role set |
| Do not invent payload persistence | None. `NOT-EVIDENCED` throughout; `IF-PAY-014` records the gap |
| Do not invent autonomy thresholds | None. Per-class structure specified, values left open |
| Do not close OPEN questions | **0 of 43 closed** |
| Do not promote `PROPOSED` to `CONFIRMED` | **0 promotions** |
| Do not create new requirement identifiers | **0 created** |
| Do not create documents to inflate the count | Two planned documents define no interfaces and say why; no document was added beyond the plan |

The hardest of these to honour was the last-but-one. Nine genuine obligations had no requirement to
point at, and the correct response under the directive was to record them as questions and then
record the consequence of doing so as [R3-008](21-phase-3-risks.md) — rather than quietly opening a
new requirement domain of Phase 3's own.

---

## 16. Tooling: what ran, and what could not

Reported plainly, per the directive's requirement not to imply a tool ran when it did not.

### Ran

| Tool | Use | Result |
| --- | --- | --- |
| `git` | Read-only: commit, branch, remotes, `diff --stat HEAD`, `status --porcelain` | Confirmed 0 modified tracked files |
| `markdownlint-cli` **0.49.1** | The version the repository pins in `.pre-commit-config.yaml`, run with the repository's own `.markdownlint.json` | **Clean** across the whole `.kiro` tree |
| markdownlint positive control | A probe config lowering the line-length limit to 60 | **3,132 violations**, proving the linter was inspecting these files |
| Ground-truth extractor (local script) | Enumerate the Phase 1, 2 and 2.1 identifier sets | 120 Phase 2 requirements, 55 Phase 1, 29 risks, 43 questions, 11 ADRs, 29 Phase 2 documents |
| Phase 3 consistency checker (local script) | Broken links, duplicate or undefined interfaces, orphan interfaces, nonexistent identifiers, missing evidence or method, unreferenced requirements | **0 problems, 0 warnings** |
| Field extractor (local script) | Header-aware extraction of every interface's fields, to generate documents 19 and 22 mechanically | 240 interfaces, no gaps |
| Table-shape checker (local script) | Every table row's cell count against its header | 0 mismatches across 24 documents |

The markdownlint run used `--dot` because `.kiro` is a dot-directory; the pre-commit hook passes
filenames explicitly and does not need that flag. Same version, same config, same rules.

### Could not run

| Tool | Reason |
| --- | --- |
| `cmake`, `ninja`, a C++ compiler, any Qt installation | **Absent from the host.** Verified, not assumed |
| `just` | Absent |
| `ctest` | Requires a build |
| `pre-commit`, `clang-format`, `clang-tidy`, `qmllint`, `clazy`, `vale` | Absent. A partial run would produce a misleading result, so none was attempted |
| QGC configure, build, unit tests, QML tests, MockLink tests | Require the above |

`uv`, `python` 3.12, `node` and `git` are present. The repository's Python tooling was not invoked:
it runs through `uv --frozen` against the `tools` project, and the pre-commit hook set it drives
requires the absent C++ tools.

**No build, configure, test, lint-via-pre-commit or static-analysis step has been executed in any
phase of this programme.** The only executable verification performed on Phase 3 is markdown linting
and the four local consistency scripts, and the scripts check the specification's internal
consistency rather than the system's behaviour.

The local scripts are development tooling and are not part of the deliverable. Every count in the
document set is reproducible from the documents themselves.

---

## 17. Self-audit

Errors found in Phase 3's own output and corrected before delivery. Listed because a specification
that claims to be checked should say what the checking caught.

| # | Error | How it was found | Correction |
| --- | --- | --- | --- |
| 1 | 58 interfaces in documents 01, 02 and 03 had no verification method | Field extractor | Methods added; vehicle-dependent rows assigned `BENCH` or `HIL` rather than `INTEGRATION` |
| 2 | `IF-MAV-001` to `IF-MAV-011` had no evidence or status; class C and D had no evidence | Field extractor | Added, with the limit of each claim stated |
| 3 | A scripted column append wrote a verification cell into two contract tables that had no such column | Table-shape checker | 24 rows reverted; the checker now runs over all documents |
| 4 | Document 13 cited `IF-VID-004` for the secure element | Review against document 08 | Corrected to `IF-VID-005` |
| 5 | `IF-LOG-015` cited the upload-queue requirement for the record-writer queue | Review | Corrected to `AERORA-GCS-REQ-025`; the two queues now distinguished explicitly |
| 6 | Twelve Phase 2 requirements were unreferenced anywhere in Phase 3 | Consistency checker | Four were genuine omissions, fixed by targeted amendments — including two new interfaces, `IF-MAV-027` and `IF-MAV-028`, for the message-handling seams. Eight are process requirements, now carried by documents 22 and 23 |
| 7 | Document 19 claimed 18 defining documents | Measurement | Corrected to 16 |
| 8 | R3-007 claimed 14 prohibition interfaces with 8 inspection-only, and wrongly included `IF-PAY-013` | Scripted enumeration | Corrected to 16 and 9, with the full list and methods |
| 9 | R3-006 claimed "fourteen of the 18 defining documents" | Measurement | Corrected to all 16 |
| 10 | Document 19's no-requirement table carried three placeholder names | Field extractor | Replaced with actual names and specific reasons |
| 11 | A stray non-English string in a document 23 table cell | Review | Corrected |
| 12 | Unescaped pipes inside a document 23 table cell, breaking the table | markdownlint `MD056` | Replaced with a fenced code block carrying the full command |
| 13 | This report originally illustrated the prohibition in section 15 by naming a fabricated requirement identifier | Consistency checker, which correctly flagged it as nonexistent | Rephrased without a fake identifier. The checker caught a fiction in the report about not writing fictions |

Three further findings were tooling defects rather than document defects, and were fixed in the
tooling because a false positive in a verification tool erodes trust in the true ones:

| Tooling defect | False findings |
| --- | --- |
| The checker's per-row evidence heuristic could not see a two-table layout | 16 |
| Its traceability lookup matched any line mentioning an identifier | 1 |
| It treated documents 19 to 23 as interface-defining, which they are not | 240 |
| The table-shape checker split on escaped pipes, which are literal content in GFM | 2 |

The third is worth noting: a tool that reported 240 problems was wrong about all 240, and the only
reason that was caught rather than acted on is that the "problem" was uniform. A checker that is
wrong in bulk is easier to spot than one that is wrong once.

Interface count moved from 238 to 240 during this reconciliation. Nothing was removed.

### Final state after correction

| Check | Result |
| --- | --- |
| Consistency checker | **0 problems, 0 warnings** |
| markdownlint 0.49.1, repository config, whole `.kiro` tree | **0 findings** |
| markdownlint positive control | 3,132 findings — the linter is reading these files |
| Table shape, all 71 documents in `.kiro/specs` | **0 mismatches** |
| `git diff --stat HEAD` | **Empty** |

---

## 18. Entry criteria for the next phase, and the review required

### What a human reviewer should check first

Ordered by consequence, not by document order.

| # | Check | Why |
| --- | --- | --- |
| 1 | The four conflicts that remain unresolved in documents 13, 14, 15 and 16 | Each needs a named authority's decision, not an engineering answer |
| 2 | The nine Phase 3 questions, especially Q3-001 and Q3-009 | Both `IMPL-BLOCKING`, both answerable by one observation |
| 3 | R3-005 — five observed artifacts behind 105 vehicle-side interfaces | The programme's largest integration exposure |
| 4 | R3-008 — nine obligations now living in questions rather than requirements | The directive's constraint created this; only the requirement owner can close it |
| 5 | Whether the capability model's five categories are the right five | Changing it later changes every capability-dependent interface |
| 6 | That painting is genuinely a profile and not the architecture | Section 13; the test is stated but cannot be run |
| 7 | The 56 `OPEN` interfaces and their fallbacks | Each fallback is a decision a reviewer may disagree with |

### Entry criteria for implementation

Unchanged from [23-build-readiness.md](23-build-readiness.md), summarised: an Aerora repository with
upstream fetch-only, the fork point recorded, a host that can configure and build, Aerora test
reachability confirmed, alias precedence observed, the firmware plugin assertion in place, the
overlay structure established, and the payload-state plan constraint implemented.

The critical path is the third: **a build host unblocks everything else, and nothing else can start
without it.**

### What Phase 3 hands over

| Artifact | Use |
| --- | --- |
| 240 interface contracts with evidence and status | The implementation surface, and the record of what is not yet real |
| 16 prohibition interfaces | The architecture's structural claims, made checkable |
| A mechanically extracted traceability map | Requirement, risk and question coverage, reproducible |
| A verification method for every interface | A prioritised plan, concentrated on the weakest evidence |
| Nine questions with owners and evidence required | The decisions that must be taken, and by whom |
| Eight risks, three of them about this specification | Including the ones the directive's own constraints created |

Phase 3 made the architecture precise enough to argue with. That was the point: a specification that
nobody can disagree with is one that has not said anything.

---

PHASE 3 COMPLETE — READY FOR HUMAN REVIEW

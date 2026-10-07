# Phase 3 Risks

Eight risks found by writing the interface contracts. These are **new** risks about the interfaces
and about this specification itself. The 29 existing risks are referenced, never redefined; their
current gating is in [phase-2-risk-gates.md](../01-aerora-architecture/phase-2-risk-gates.md).

Three of the eight are risks about the specification rather than about the system. That is
deliberate: a document set this size has its own failure modes, and recording them here is cheaper
than discovering them in review.

## Vocabulary

Carried from Phase 2 unchanged.

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
| R3-001 | Three of four module dependency rules have no test assigned | **High** | `MEDIUM` | `G-IMPL` |
| R3-002 | Capability presence and availability are indistinguishable | **High** | `MEDIUM` | `G-INTEG` |
| R3-003 | The audit trail may name a relay as an event's originator | Medium | `HIGH` | `G-INTEG` |
| R3-004 | Fifty-six interfaces are `OPEN` pending firmware and product deliverables | **High** | `MEDIUM` | `G-IMPL` |
| R3-005 | The aircraft side of the interface set rests on five observed artifacts | **High** | `HIGH` | `G-INTEG` |
| R3-006 | The split contract layout lets a reader take half a contract as the whole | Medium | `MEDIUM` | `G-OPEN` |
| R3-007 | Nine of sixteen prohibition interfaces are assigned inspection alone | Medium | `MEDIUM` | `G-RELEASE` |
| R3-008 | Phase 3 gaps became questions, and a question carries no verification obligation | Medium | `MEDIUM` | `G-IMPL` |

No `BLOCKER`. None of the eight blocks architecture ratification, which is consistent with Phase 3
being a specification activity: it can find that something is unverifiable, and it cannot make it
unbuildable.

---

### R3-001

**Risk.** Of the four module dependency rules that constitute the internal boundary contract, only
one has an automated test assigned to it. The other three rest on review discipline. **No test of
any of the four has been written**; the asymmetry is in what the requirement set asks for.

| Rule | Verification route |
| --- | --- |
| Presentation depends on domain only | Review only — no method assigned |
| Domain reaches QGroundControl only through the extension layer | Review only — no method assigned |
| Payload and mission are mutually independent | Review only — no method assigned |
| Cloud depends on security, never the reverse | **Test assigned** under `AERORA-AWS-REQ-012` |

**Consequence.** Three structural properties the architecture asserts about itself can be violated
by a well-intentioned change with no diagnostic. The payload-mission rule is the most exposed: "run
the paint pattern" is the obvious feature and coupling the two modules is the obvious implementation,
as [architecture-decisions.md](../01-aerora-architecture/architecture-decisions.md) already notes.

The fourth rule being the tested one is not an accident — it is the one with a security consequence,
so somebody wrote a test. The other three have an architectural consequence, which nobody tests
until it is already broken.

**Likelihood.** High. Review disciplines decay, and these three will be reviewed by people who did
not write them.

**Severity.** `MEDIUM`. A violation is recoverable, expensively.

**Gate.** `G-IMPL`.

**Mitigation.** Extend the architectural-property tests of `AERORA-TEST-REQ-004` to cover all four
dependency rules rather than one. The mechanism already exists and already runs in the default
continuous-integration gate under `AERORA-TEST-REQ-006`; three more assertions is a small addition
to a test that is already there. Recorded in
[22-phase-3-verification-plan.md](22-phase-3-verification-plan.md).

**Evidence.** `GCS-DECIDED`. The asymmetry was found by reading the four rules against the four
property-test assertions and finding no overlap except the fourth.

**Owner.** Build and test.

**Affected.** `IF-SYS-001` through `IF-SYS-008`, `IF-SEC-010`, `IF-CLD-004`. Requirements
`AERORA-TEST-REQ-004`, `AERORA-TEST-REQ-006`, `AERORA-AWS-REQ-012`. No existing risk; adjacent to
R-014.

---

### R3-002

**Risk.** The vehicle may not report capability presence separately from capability availability, so
the ground station cannot choose correctly between hiding a control and disabling it with a reason.

**Consequence.** `AERORA-UI-REQ-008` requires the distinction and the interface cannot supply it.
Every absent Aerora capability is then presented the same way, and the conservative choice — hide —
loses the actionable reason that a disabled control would have carried. An operator sees nothing
where they might have seen "the spray system is present and the tank is empty".

The failure is quiet. A hidden control is indistinguishable from a control that does not exist, so
nobody reports it.

**Likelihood.** High. The capability inventory is `NOT-EVIDENCED` for its content, and one bit per
capability is the simpler and more likely firmware choice.

**Severity.** `MEDIUM`. The ground station is correct and less useful. It does not become wrong.

**Gate.** `G-INTEG`.

**Mitigation.** Q3-009 asks for the field definition before the inventory is implemented, which is
the cheap moment. If the answer is one bit, `IF-UIS-006` presents the conservative case and the
model records that the actionable reason is unavailable rather than pretending to supply one.

**Evidence.** `NOT-EVIDENCED`.

**Owner.** Firmware, with the ground-station interface as consumer.

**Affected.** `IF-UIS-005`, `IF-UIS-006`, `IF-FC-017`, `IF-PAY-002`, `IF-GPU-007`, `IF-PWR-012`,
`IF-MAV-026`. Requirements `AERORA-UI-REQ-008`, `AERORA-MAV-REQ-009`. Question Q3-009.

---

### R3-003

**Risk.** If the safety and watchdog MCU reports through the primary flight-controller MCU, the
reporting source the ground station records is the relay rather than the originator.

**Consequence.** This is the one risk in this set with a direct accountability consequence. An
operator or investigator reading the audit trail would conclude that the primary MCU was functioning
at the moment of a watchdog event, because the record names it as the reporting source. If the
watchdog fired *because* the primary MCU was not functioning, the record says the opposite of what
happened.

The independence of the safety MCU is the point of the design. A reporting path that is not equally
independent silently removes that property from everything the ground station records about it.

**Likelihood.** Medium. Routing all MAVLink through one MCU is a common and reasonable firmware
choice; so is giving the safety MCU its own path.

**Severity.** `HIGH`. An audit record that misattributes a safety event is worse than no record,
because it will be believed.

**Gate.** `G-INTEG`.

**Mitigation.** Three parts, all already specified. `IF-MAV-022` names the gap. `IF-TLM-010` records
the **sending** component and the documents prohibit presenting it as the originating subsystem.
Q3-004 asks for the routing design, and asks for it to be confirmed by bench observation with the
primary MCU held in reset rather than from a design statement.

**Evidence.** `MODEL` for the watchdog's existence and independence of action; `NOT-EVIDENCED` for
its reporting path.

**Owner.** Firmware.

**Affected.** `IF-MAV-022`, `IF-PWR-011`, `IF-FC-012`, `IF-TLM-010`, `IF-LOG-013`, `IF-AUT-010`.
Requirements `AERORA-GCS-REQ-027`, `AERORA-COMPLY-REQ-004`, `AERORA-SAFE-REQ-011`. Question Q3-004.
Adjacent to existing risk R-006.

---

### R3-004

**Risk.** Fifty-six of 240 interfaces — 23% — carry status `OPEN`, each pending a firmware, hardware
or product deliverable that does not exist.

**Consequence.** Nearly a quarter of the specified interface set cannot be implemented as written.
The ground station can be built and the first increment is viable, because every `OPEN` interface
has a stated fallback. What cannot be scheduled is the work behind those 56 contracts, because its
inputs are external.

The schedule risk is the real one: if the firmware deliverables arrive late, the ground station work
that depends on them compresses into the end of the programme, which is exactly where
`NOT-EVIDENCED` assumptions get implemented as guesses.

**Likelihood.** High. It is a count, not a prediction. The likelihood applies to the consequence,
and firmware deliverables arriving later than the interface specification needs them is the normal
case rather than the exception.

**Severity.** `MEDIUM`. The fallbacks hold, and the first increment does not depend on any of the 56.

**Gate.** `G-IMPL`.

**Mitigation.** The fallback for every `OPEN` interface is stated in its own row and summarised per
question in [20-phase-3-open-questions.md](20-phase-3-open-questions.md). Sequencing: build the
`CONFIRMED` and `PROPOSED` set first — 184 interfaces, enough for a complete ground station against
the standard MAVLink set. Then populate. The capability inventory at `IF-MAV-026` is what makes that
sequencing possible, because a missing message family degrades gracefully rather than breaking a
display.

**Evidence.** `GCS-DECIDED`. The count is mechanically extracted and reproducible.

**Owner.** Programme.

**Affected.** 56 interfaces across 13 domains. All nine Phase 3 questions and 18 inherited open
items.

---

### R3-005

**Risk.** Of the 240 interfaces, five carry `ARTIFACT-OBSERVED` evidence. The aircraft side of this
specification is almost entirely unobserved.

| Evidence | Count | What it rests on |
| --- | --- | --- |
| `GCS-VERIFIED` | 89 | QGroundControl source, read |
| `BASELINE-ASSERTED` | 47 | A baseline document states it; no artifact cited |
| `GCS-DECIDED` | 46 | An Aerora ground-station decision |
| `NOT-EVIDENCED` | 37 | Neither artifact nor baseline statement |
| `MODEL` | 17 | Present in the Simulink firmware model |
| `PLACEHOLDER` | 7 | In the model, explicitly provisional |
| `ARTIFACT-OBSERVED` | 5 | **A hardware or PCB artifact shows it exists** |

**Consequence.** 135 of 240 interfaces rest on `GCS-VERIFIED` or `GCS-DECIDED` — things Aerora
controls or has read. The remaining 105 describe a vehicle, and 5 of them are backed by an observed
artifact. The specification is therefore strongest exactly where it matters least to the aircraft
integration, and weakest at the boundary where two teams meet.

This is not a criticism of the work; it is a measurement of when it was done. The ground station was
available to read and the aircraft was not. But it means the interface control document is, on the
vehicle side, a statement of what the ground station **needs** rather than a record of what the
vehicle **does** — and those are different documents that look identical.

**Likelihood.** High. Again a count rather than a prediction. What is predicted is that some of the
105 will turn out to be wrong, and the likelihood of that is high because nothing has checked them.

**Severity.** `HIGH`. A wrong interface assumption discovered at integration is the most expensive
class of defect in this programme.

**Gate.** `G-INTEG`.

**Mitigation.** The Phase 2.1 evidence matrix already ranks 22 vehicle-side items by confidence and
names the three that are most load-bearing — items 8, 10 and 14. Phase 3's contribution is the per
interface evidence status, which makes the exposure countable. The 26 `BENCH` and 4 `HIL`
verification assignments in [22-phase-3-verification-plan.md](22-phase-3-verification-plan.md) are
concentrated on exactly these interfaces, which is the intended use of the count.

**Evidence.** `GCS-DECIDED`. Mechanically extracted and reproducible.

**Owner.** Programme, with Hardware and Firmware.

**Affected.** All 105 vehicle-side interfaces. Existing risks R-002, R-017. Evidence matrix items 6,
8, 10, 11, 13, 14, 15, 18, 19, 22.

---

### R3-006

**Risk.** All sixteen defining documents split each contract across two tables keyed on the same
identifier — a contract table and a failure-and-traceability table. A reader who reads one and not
the other has half the contract and no indication that it is half.

**Consequence.** The failure behaviour is where most of the specification's value is. "A write
without readback is **not** treated as applied" and "a failsafe reported once and then absent goes
stale, not cleared" are in the second table, not the first. An implementer who works from the
contract table alone would build something that passes a cursory review and violates the second
architectural principle.

**Likelihood.** Medium. The structure is stated in
[00-phase-3-readme.md](00-phase-3-readme.md) under "how to read an interface row", and readers skip
readmes.

**Severity.** `MEDIUM`. Caught in review, if the reviewer knows to look.

**Gate.** `G-OPEN`. Carried.

**Mitigation.** The split exists because the 17 mandatory per-interface fields do not fit one
readable table, so the alternative is worse. Three things reduce the exposure: the readme states the
convention, [19-interface-traceability.md](19-interface-traceability.md) carries the full set in one
row per interface, and the script check confirms every interface appears in both the defining
document and the traceability table.

Accepting this rather than restructuring is a deliberate trade. A single 17-column table would be
complete and unread.

**Evidence.** `GCS-DECIDED`.

**Owner.** Specification.

**Affected.** All sixteen defining documents. [03-mavlink-icd.md](03-mavlink-icd.md) additionally
uses single-table class listings for the message families, and groups its traceability rows by class,
so a reader there has a third place to look rather than two. No requirement; no existing risk.

---

### R3-007

**Risk.** Sixteen interfaces specify that **nothing crosses** a boundary, or that a path is
rejected. Nine of them are **assigned** `STATIC` inspection only, and an inspection that must be
repeated at every change is a control that decays. None has yet been performed.

| Prohibition interface | Subject | Verification |
| --- | --- | --- |
| `IF-SYS-005` | Ground station to companion | `UNIT` |
| `IF-SYS-006` | Ground station to payload actuators | `STATIC` |
| `IF-SYS-007` | Ground station to power switching | `STATIC` |
| `IF-SYS-008` | Ground station to inbound network | `UNIT` |
| `IF-GPU-008` | Ground station to companion | `STATIC`, `UNIT` |
| `IF-PAY-015` | Ground station to payload actuators | `STATIC` |
| `IF-AUT-012` | Ground-station-initiated failsafe action | `STATIC` |
| `IF-MIS-015` | Ground-station-side payload state replay | `STATIC` |
| `IF-MIS-016` | Modification of the QGC resume filter | `STATIC` |
| `IF-SEC-010` | Security-module dependency on the cloud | `UNIT` |
| `IF-SEC-011` | Advanced-UI toggle as an authorisation boundary | `STATIC` |
| `IF-LOG-018` | Upload to a non-Aerora endpoint | `INTEGRATION` |
| `IF-LOG-019` | Local record deletion on upload | `STATIC` |
| `IF-CLD-003` | Inbound network connection | `UNIT` |
| `IF-CLD-004` | Cloud participation in the command path | `INTEGRATION` |
| `IF-CLD-010` | Ground-station reporting feature | `STATIC` |

**Consequence.** Nine of the sixteen rest on inspection. A prohibition stated as an interface is
checkable, which was the reason for stating them that way — but checkable is not the same as
checked. `IF-AUT-012` (no ground-station-initiated failsafe action) and `IF-PAY-015` (no
ground-station path to payload actuators) are the two where a violation would matter most, and both
are `STATIC`.

Four of the sixteen are already automated, and they are the four somebody had a reason to automate:
two have a security consequence and two have a data-governance consequence. The nine left on
inspection are the ones whose consequence is architectural.

**Likelihood.** Medium. A violation requires somebody to add a path that the architecture says
should not exist, which is unlikely deliberately and entirely plausible as a convenience.

**Severity.** `MEDIUM`. For `IF-AUT-012` and `IF-PAY-015` specifically it would be `HIGH`, which is
the argument for converting those two first.

**Gate.** `G-RELEASE`.

**Mitigation.** Convert the prohibitions to assertions where the mechanism allows. A test that no
code path reaches the payload command interface from a flight-control context is writable; a test
that the authority module has no failsafe-initiation entry point is writable. `AERORA-TEST-REQ-004`
already demonstrates the pattern for four other structural properties. Recorded in
[22-phase-3-verification-plan.md](22-phase-3-verification-plan.md).

**Evidence.** `GCS-DECIDED`. The list and its verification methods are mechanically extracted.

**Owner.** Build and test.

**Affected.** The sixteen interfaces above. Requirements `AERORA-SAFE-REQ-008`,
`AERORA-PAYLOAD-REQ-010`, `AERORA-GPU-REQ-003`, `AERORA-AWS-REQ-012`, `AERORA-AWS-REQ-014`,
`AERORA-TEST-REQ-004`.

---

### R3-008

**Risk.** Phase 3 defines no requirement identifiers by directive, so every obligation it found that
no requirement states was recorded as a question instead. A question carries no verification
obligation.

**Consequence.** A requirement appears in the verification plan and fails a gate if unmet. A
question appears in a list and is closed by a decision. Nine Phase 3 questions therefore describe
nine obligations that nothing will test, and the gap between "the question was answered" and "the
answer was implemented and verified" is unmanaged.

The clearest instance is Q3-009. If firmware answers "presence and availability are reported
separately", the ground station must then use both, and no requirement says so. The obligation would
exist in a closed question.

**Likelihood.** Medium. It depends on whether answering a question triggers a requirement being
written.

**Severity.** `MEDIUM`.

**Gate.** `G-IMPL`.

**Mitigation.** Each Phase 3 question names its affected interfaces and requirements, so answering
one points at the rows that must change. The procedural mitigation belongs to whoever owns the
requirement set: **answering a Phase 3 question should produce a requirement, not just a decision.**
Phase 3 cannot do that itself without violating its own constraint, which is why this is recorded as
a risk rather than fixed.

The same applies to the plan-constraint fallback in
[10-mission-icd.md](10-mission-icd.md), which is carried in a requirement's rationale and the
evidence matrix rather than in a requirement of its own, for the same reason.

**Evidence.** `GCS-DECIDED`.

**Owner.** Requirements and architecture.

**Affected.** All nine Phase 3 questions. Adjacent to existing risk R-002, whose mitigation has the
same shape.

---

## What Phase 3 did not add a risk for

Recorded so that the list is not read as exhaustive by omission.

| Considered | Why no risk was raised |
| --- | --- |
| Interface identifier exhaustion within a domain | Three digits per domain against a maximum of 28 used |
| The traceability document being mechanically extracted | A benefit, not a risk. A hand-maintained 240-row table is the risk |
| Interface renumbering if an ICD is split | Identifiers are domain-scoped, not document-scoped, so a split does not renumber |
| Phase 3 contradicting Phase 2 | Checked: no Phase 2 position is reversed. Two conflicts are **recorded** and neither is resolved |
| The 11 interfaces with no requirement | Nine are inherited upstream behaviour, one is governed by general rules, one is a rejected core edit. Each is listed with its reason in [19-interface-traceability.md](19-interface-traceability.md) |
| Dialect identifier collision | Allocation discipline is specified in [03-mavlink-icd.md](03-mavlink-icd.md) and the dialect does not exist yet. It becomes a risk when allocation starts |

The fourth row was checked rather than assumed. Phase 2.1's reconciliation discipline — retain
superseded text, do not silently reverse — was applied to Phase 3's own conflicts, and both are in
their documents' "conflicts recorded" sections with the authority named.

# Mission Interface Control Document

Mission transfer, ownership, resume, and the contract that lets the payload retention question stay
unresolved.

## Mission ownership

| Artifact | Owner | Status |
| --- | --- | --- |
| **Mission intent** | The saved Aerora plan file, held by the ground station | `PROPOSED` |
| Vehicle mission store | The vehicle; a **derived** artifact | `CONFIRMED` |
| Current mission index | The vehicle | `CONFIRMED` |
| Mission structure, complex items | The plan file only | `PROPOSED` |

### Why the vehicle store cannot be authoritative

All four are `GCS-VERIFIED`.

| Fact | Consequence |
| --- | --- |
| A mission item carries command, frame, seven parameters, autoContinue, sequence and jump id — **no vendor-extension slot** | Aerora metadata cannot round-trip through the vehicle |
| Complex items are flattened to primitive items **once**, on upload | A downloaded mission is the flattened form; the original structure is gone |
| Resume **replaces the entire vehicle mission** and resets the current index | The store after a resume is not the planned mission |
| A partial upload failure discards the station's write list **without rolling the vehicle back** | The store can be in a state no plan file describes |

The first is a property of MAVLink's mission representation, not a QGroundControl choice. That is
why R-012 is `LOW`: it is not Aerora-specific, and plan-file authority resolves it.

## Interface contracts

| ID | Interface | Direction | Producer | Consumer | Crossing | Ack | Authority | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-MIS-001` | Mission upload | GCS to vehicle | Mission module | Flight controller | A flattened primitive item sequence | Per protocol transaction | Vehicle accepts or rejects | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MIS-002` | Mission download | Vehicle to GCS | Flight controller | Mission module | The flattened item sequence as stored | Per protocol transaction | Vehicle | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MIS-003` | Mission current index | Vehicle to GCS | Flight controller | Mission module, display | The index being executed | None | Vehicle | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MIS-004` | Mission item reached | Vehicle to GCS | Flight controller | Mission module, display | An item-completion event | None | Vehicle | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MIS-005` | Mission replacement | GCS to vehicle | Mission module | Flight controller | A complete replacement item set | Per protocol transaction | Vehicle | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MIS-006` | Mission resume | GCS to vehicle | Mission module | Flight controller | A generated resume mission, replacing the stored one | Per protocol transaction | Vehicle | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MIS-007` | Mission pause | GCS to vehicle | Operator via GCS | Flight controller | A pause request, via the flight-mode path | Required | Vehicle decides | `GCS-VERIFIED` mechanism; Aerora mode mapping `OPEN` | `OPEN` |
| `IF-MIS-008` | Plan file | GCS internal | Mission module | Local storage, cloud module | Aerora mission intent with complex-item structure | Write confirmation | **Ground station** | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MIS-009` | Plan version and identity | GCS internal | Mission module | Audit, cloud, verification | Plan identity, version, and the Aerora vehicle identity it targets | — | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-MIS-010` | Mission verification | GCS internal | Mission module | Operator display | Comparison of the vehicle store against the plan file's flattened form | — | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-MIS-011` | Aerora complex item | GCS internal | Plan editor | Mission module, map visual | An Aerora mission structure that flattens on upload | — | Ground station | `GCS-VERIFIED` pattern | `OPEN` |
| `IF-MIS-012` | Aerora mission command set | GCS internal | Firmware abstraction | Plan editor command picker | The explicit list of commands the vehicle supports | — | Vehicle capability | `NOT-EVIDENCED` | `OPEN` |
| `IF-MIS-013` | Aerora mission status | Vehicle to GCS | Flight controller | Mission module | Aerora mission state beyond the standard index | None | Vehicle | `BASELINE-ASSERTED` candidate | `PROPOSED` |
| `IF-MIS-014` | Payload state interaction on resume | Vehicle internal | Payload controller | Itself | Whether payload state survives mission replacement | — | Payload controller | **`NOT-EVIDENCED`** | `OPEN` |
| `IF-MIS-015` | GCS-side payload state replay | **None — rejected** | — | — | Nothing | — | **No authority** | `GCS-DECIDED` | `CONFIRMED` |
| `IF-MIS-016` | QGC resume filter modification | **None — rejected** | — | — | Nothing | — | — | `GCS-DECIDED` | `CONFIRMED` |

| ID | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- |
| `IF-MIS-001` | Partial failure leaves the store indeterminate; **presented as unverified**, no rollback attempted | `AERORA-MISSION-REQ-007` | R-012 | Q-041 | `INTEGRATION` |
| `IF-MIS-002` | The downloaded form is presented as a flat mission, **explicitly not as an Aerora plan** | `AERORA-MISSION-REQ-002` | R-012 | Q-022 | `INTEGRATION` |
| `IF-MIS-003` | Index unknown; progress shown as unknown, never estimated | `AERORA-MISSION-REQ-006` | — | — | `INTEGRATION` |
| `IF-MIS-004` | **Completion is never inferred** from elapsed time or position | `AERORA-MISSION-REQ-006`, `AERORA-GCS-REQ-029` | — | — | `INTEGRATION` |
| `IF-MIS-005` | The previous store is gone on success; the plan file remains the record | `AERORA-MISSION-REQ-001` | R-012 | — | `INTEGRATION` |
| `IF-MIS-006` | See the resume contract below | `AERORA-MISSION-REQ-003`, `AERORA-MISSION-REQ-004` | **R-002** | Q-019 | `INTEGRATION` |
| `IF-MIS-007` | Pause unavailable where no Aerora pause mode is confirmed; **the action is hidden, not disabled** | `AERORA-GCS-REQ-011` | R-028 | Q-007 | `INTEGRATION` |
| `IF-MIS-008` | Write failure surfaced; a plan that failed to save is never treated as saved | `AERORA-MISSION-REQ-001` | — | — | `UNIT` |
| `IF-MIS-009` | A plan without a target vehicle identity is usable but **marked as not bound to a vehicle** | `AERORA-GCS-REQ-014`, `AERORA-VEH-REQ-005` | R-017 | Q-016 | `UNIT` |
| `IF-MIS-010` | An unverified upload is **presented as unverified**; depth of comparison is open | `AERORA-MISSION-REQ-007` | — | Q-041 | `INTEGRATION` |
| `IF-MIS-011` | Flattening is one-way; reconstruction from a download is prohibited | `AERORA-MISSION-REQ-002` | R-018 | Q-021 | `UNIT` |
| `IF-MIS-012` | An empty list would mean all commands, offering the operator commands the firmware may not implement; Aerora declares the list explicitly | `AERORA-MISSION-REQ-005` | — | Q-023 | `INTEGRATION` |
| `IF-MIS-013` | Absent Aerora mission status degrades to the standard index only | `AERORA-MAV-REQ-009` | — | — | `INTEGRATION` |
| `IF-MIS-014` | **If absent, the fallback applies** — see below | `AERORA-PAYLOAD-REQ-007`, `AERORA-MISSION-REQ-003` | **R-002** | Q-019 | `BENCH` |
| `IF-MIS-015` | Not applicable — no such mechanism exists | `AERORA-MISSION-REQ-003` | R-002 | — | `STATIC` |
| `IF-MIS-016` | Not applicable — QGC core is unmodified | — | R-002 | — | `STATIC` |

## The resume contract

### What the existing mechanism does

All `GCS-VERIFIED`, from reading the resume generator in full.

| Behaviour | Detail |
| --- | --- |
| A replay prefix is built from a fixed list of fourteen camera, region-of-interest and speed commands | A plain local automatic variable in a non-virtual method of a class with no virtuals |
| Everything at or after the resume index passes through untouched | The inclusion condition contains a clause admitting it |
| An unlisted command **before** the resume index is dropped | With no log line, no warning, no counter and no operator notification |
| The prefix is de-duplicated, latest-wins, with one-shot actions deleted | Which shows the mechanism's purpose is **state restoration** |
| Any jump command anywhere in the mission blocks resume entirely | Aborting before any state change |
| Resume replaces the whole vehicle mission and resets the index | The resume action then disappears |

### Why there is no seam

| Fact | Consequence |
| --- | --- |
| The generator is not virtual, and its class declares no virtual members | No subclass substitution |
| The vehicle constructs the concrete class directly | No factory |
| No core-plugin or firmware-plugin hook exists on this path | No seam |

Changing the filter means editing QGroundControl core, on a path with no test coverage for vendor
commands because upstream has no vendor commands.

### The exposure, bounded

| Case | Outcome |
| --- | --- |
| An Aerora command **at or after** the resume index | Survives untouched |
| An Aerora command **before** the resume index | Dropped silently |

Only the replay prefix is affected. The mission from the resume point onward is reproduced
faithfully: items are deep-copied, the seven parameters are never touched, and only the
current-item flag and sequence number are mutated.

That is why R-002 is `HIGH` rather than `CRITICAL`. It remains `HIGH` because the failure is
**silent**.

### The proposed resolution, and its evidence status

| Element | Position | Status |
| --- | --- | --- |
| Modify the QGC resume filter | **Rejected** | `CONFIRMED` — no seam, carried forever, untested upstream |
| Replay payload state from the ground station | **Rejected** | `CONFIRMED` — would put authoritative state in the ground station |
| Encode payload state in an allow-listed command's parameters | **Rejected** | `CONFIRMED` — makes the wire protocol dishonest and the plan unreadable |
| Reconstruct Aerora structure from a downloaded mission | **Rejected** | `CONFIRMED` — inference presented as fact |
| The payload controller retains payload state across the interruption | **Preferred** | **`NOT-EVIDENCED`** |

Four rejections stand unconditionally. The fifth is a vehicle-side verification requirement, not an
established behaviour. No baseline document states that payload state is retained, and the firmware
payload elements are placeholders. Ownership does not imply persistence.

### The fallback, and its status

If retention is not implemented:

| Fallback | Status | Requirement |
| --- | --- | --- |
| Warn the operator at resume time that state set before the resume point is not re-established | Adopted **regardless** of the evidence | Covered by the existing `AERORA-MISSION-REQ-004` |
| Constrain Aerora plans so payload-state commands do not precede a plausible resume point | Adopted **conditionally** | **No requirement identifier** |

The second has no requirement identifier and does not acquire one in Phase 3. It is a conditional
obligation traceable to ADR-005 and the Phase 2.1 reconciliation, recorded in
`AERORA-MISSION-REQ-003`'s rationale and in the evidence matrix. **It does not silently become a
confirmed requirement**; if the evidence lands badly it becomes one by a human decision in Phase 4
scoping.

### The operator obligation

`AERORA-MISSION-REQ-004`: the operator is told that resume reproduces the mission **from the resume
point**, not the full mission history.

This falls out of an architectural decision and is easy to forget. An operator who believes resume
restores everything will be surprised exactly once, at the worst moment.

## Upstream defects observed on this path

Recorded because they affect anyone touching this code, and because two are candidates for an
upstream contribution.

| Defect | Evidence |
| --- | --- |
| An unguarded index into the mission item list, with no empty-list check | `GCS-VERIFIED` |
| A second unguarded index when setting the current item | `GCS-VERIFIED` |
| The locally built resume items are never deleted — one leak per resume | `GCS-VERIFIED` |
| A resume requested while a transaction is in progress is dropped with only a debug log | `GCS-VERIFIED` |
| The jump refusal reports only through an application message, emitting no signal | `GCS-VERIFIED` |
| The resume-ready signal has **no consumer anywhere** in the source tree | `GCS-VERIFIED` |

The fourth is the one most likely to be seen in the field: an operator who triggers resume during an
in-flight transaction gets no feedback at all.

## Mission verification

| Depth | Catches | Cost |
| --- | --- | --- |
| Item count only | Truncation | Negligible |
| Count plus command sequence | Truncation and reordering | Negligible |
| Full parameter comparison | The above, plus an altered parameter | Negligible |
| Read back from the vehicle and compare | The above, plus a vehicle-side storage fault | A full mission download per upload |

The fourth is the only one that detects a fault in the vehicle's own store, and the only one with a
real cost. **The depth is Q-041 and is not decided here.** What is specified is that an unverified
upload is presented as unverified.

## Complex items

| Property | Position | Evidence |
| --- | --- | --- |
| Aerora complex items are application-layer constructs | `PROPOSED` | `GCS-VERIFIED` pattern exists in the custom-build example |
| They flatten to primitive mission items on upload, once | `CONFIRMED` | `GCS-VERIFIED` |
| Expansion is not re-run on resume | `CONFIRMED` | `GCS-VERIFIED` |
| Which complex items exist | **`OPEN`** | Q-021 and product definition |
| Survey-style insert behaviour | **`OPEN`** | Q-021; accepting the default keeps the pattern core-edit free |

The architecture does not specify which complex items exist. The painting pattern's geometry and
parameters depend on Q-023 and on product definition, and inventing them here would repeat the
error the payload document declines to make.

## Conflicts recorded

None between sources for this document. The mission contract is the best-evidenced area in Phase 3,
because the existing mechanism was read in full — and its one unresolved element, payload retention,
is a vehicle-side gap rather than a disagreement.

## Not specified, and why

| Item | Reason |
| --- | --- |
| Aerora mission command identifiers | No dialect |
| The Aerora mission command set membership | Q-023 |
| Aerora complex-item geometry and parameters | Q-021, Q-023, product definition |
| Verification depth | Q-041 |
| Aerora pause mode | Q-007 |
| Payload state retention | Evidence matrix item 14 |

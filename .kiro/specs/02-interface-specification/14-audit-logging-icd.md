# Audit and Logging Interface Control Document

What gets recorded, by whom, in which of the separate records, in what order relative to the
network, and how two records of the same thing are told apart.

Architecture in [logging-architecture.md](../01-aerora-architecture/logging-architecture.md). Phase 3
adds the record-production contracts and two boundaries across which nothing may cross.

## Three records, and the one that is unconditional

| Record | Audience | Must survive | Interface |
| --- | --- | --- | --- |
| Application diagnostic log | Engineers | Nothing in particular; it is debug output | `IF-LOG-001` |
| Telemetry record | Flight operations and analysis | A ground-station crash | `IF-TLM-012`, defined in [11-telemetry-icd.md](11-telemetry-icd.md) |
| MAVLink stream log | Protocol analysis and replay | A ground-station crash | `IF-LOG-002` |
| Audit record | Accountability and investigation | **Everything, including telemetry logging being switched off** | `IF-LOG-003` |

Collapsing these into one log is the obvious simplification and it is wrong. Different retention,
different consumers, different volumes, and decisively different integrity requirements.
`AERORA-GCS-REQ-020` keeps them separate.

### Why the audit record is unconditional

Upstream's CSV telemetry writer opens its file only once the vehicle is armed, unless a
save-when-not-armed setting is enabled. `GCS-VERIFIED`.

That conditionality is reasonable for telemetry and unacceptable for audit, because the operator
actions an investigation most often needs happen **before** arming:

| Pre-arm action | Would fall outside the telemetry window |
| --- | --- |
| A parameter change | Yes |
| An authority mode request | Yes |
| A payload command | Yes |
| An authorisation denial | Yes |
| A mission upload | Yes |

So the audit record is gated on nothing: not armed state, not a telemetry setting, not a vehicle
being connected. `AERORA-GCS-REQ-021`. A ground station with no vehicle attached still produces
audit records for sign-in, configuration change and authorisation denial.

## Interface contracts — record streams and content

| ID | Interface | Direction | Producer | Consumer | Crossing | Update semantics | Ack | Authority | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-LOG-001` | Application diagnostic log | GCS internal | All GCS code | Local file | Category-interned diagnostic lines with per-category rate buckets | Continuous | — | Ground station | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-LOG-002` | MAVLink stream log | GCS internal | Link layer | Local file | The raw per-vehicle MAVLink stream | Continuous while connected | — | Ground station | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-LOG-003` | Audit record store | GCS internal | Aerora record writer | Local storage | Audit records, append-only | Event-driven, **unconditional** | Local write completes before the operator is told | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-LOG-004` | Operator action record | GCS internal | Operator interaction path | Audit record store | The action, the operator identity, the local time, and the resulting request identity | One per operator action | — | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-LOG-005` | Command lifecycle record | GCS internal | Command tracker | Audit record store | Request identity, operator identity, command and parameters as sent, **every state transition with its time**, terminal state and reason | One record per command, appended through its life | — | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-LOG-006` | Authority mode request record | GCS internal | Authority module | Audit record store | The requested mode, with operator and request identity | Per request | — | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-LOG-007` | Effective authority change record | GCS internal | Authority module | Audit record store | The **vehicle-reported** effective mode change | Per reported change, **separately from the request** | — | Vehicle reports; ground station records | `MODEL` | `PROPOSED` |
| `IF-LOG-008` | Confirmation record | GCS internal | Authority module | Audit record store | That a confirmation was prompted, and that one was given, each with its time | Two events, not one | — | Ground station | `MODEL` | `PROPOSED` |
| `IF-LOG-009` | Security event record | GCS internal | Security module | Audit record store | Sign-in, sign-out, credential expiry, signing state change, key provisioning and rotation | Per event | — | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-LOG-010` | Configuration change record | GCS internal | Settings and configuration path | Audit record store | What changed, from what, to what, by whom | Per change, recorded before it is reported as applied | — | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-LOG-011` | Mission event record | GCS internal | Mission module | Audit record store | Plan identity, upload, download, resume index, verification outcome | Per event | — | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-LOG-012` | Failsafe event record | GCS internal | Authority module | Audit record store | State, reason, source, severity, reported action and freshness — **five items plus freshness, kept separate** | Per reported change, and again when the value goes stale | — | Vehicle reports; ground station records | `PLACEHOLDER` | `OPEN` |
| `IF-LOG-013` | Value provenance in records | GCS internal | Fact system | All record writers | Value, receive time and reporting source, carried through without reduction | Per recorded value | — | Ground station | `GCS-DECIDED` | `PROPOSED` |

| ID | Freshness semantics | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `IF-LOG-001` | Not evidential; no freshness contract | An I/O error is tracked and surfaced, not silently absorbed | `AERORA-GCS-REQ-020` | — | — | `STATIC` |
| `IF-LOG-002` | Stream order is the record order | Loss of the stream log does not affect audit or telemetry; the three are independent | `AERORA-GCS-REQ-020` | R-022 | Q-039 | `INTEGRATION` |
| `IF-LOG-003` | Local timestamps throughout; no service clock | A store that cannot be written is a **visible** failure, and the drop is accounted for. The station does not continue silently unrecorded | `AERORA-GCS-REQ-021`, `AERORA-SEC-REQ-019` | — | Q-037 | `INTEGRATION` |
| `IF-LOG-004` | Stamped at the action, not at the write | An action whose record cannot be written is surfaced to the operator as unrecorded | `AERORA-COMPLY-REQ-002` | — | Q-029 | `INTEGRATION` |
| `IF-LOG-005` | Each transition carries its own time; the record is never collapsed to its terminal state | A command that never terminates still has its transitions recorded; **`STALE` and `FAILED` are recorded outcomes, not missing ones** | `AERORA-GCS-REQ-015`, `AERORA-GCS-REQ-018`, `AERORA-COMPLY-REQ-002` | R-025 | Q-036 | `UNIT` |
| `IF-LOG-006` | Request time is the record time | A request that is never answered is recorded as unanswered | `AERORA-COMPLY-REQ-003` | R-028 | Q-007 | `INTEGRATION` |
| `IF-LOG-007` | Receive time of the vehicle report | **No effective change is recorded on the strength of a request.** If none is reported, none is recorded | `AERORA-COMPLY-REQ-003`, `AERORA-SAFE-REQ-006` | R-028 | Q-007 | `INTEGRATION` |
| `IF-LOG-008` | Both events timestamped independently | A prompt with no confirmation is a recorded state, and is the question a debrief asks | `AERORA-SAFE-REQ-007` | — | Q-007 | `INTEGRATION` |
| `IF-LOG-009` | Local time; credential expiry recorded with its own time basis | Denials are covered by `IF-SEC-006`; this interface does not duplicate them | `AERORA-SEC-REQ-015`, `AERORA-SEC-REQ-019` | R-004 | Q-029 | `INTEGRATION` |
| `IF-LOG-010` | Recorded before the change is reported as applied | A change that cannot be recorded is **not applied**; configuration is one of the few places where the record gates the act | `AERORA-COMPLY-REQ-002`, `AERORA-BUILD-REQ-010` | — | — | `INTEGRATION` |
| `IF-LOG-011` | Plan identity binds the record to a specific plan revision | A mission event with no plan identity is recorded as unidentified, never attributed by guess. **This record plus the plan file are the two sources for post-flight reconstruction; the vehicle mission store is not one of them**, because resume replaces it | `AERORA-MISSION-REQ-007`, `AERORA-MISSION-REQ-008`, `AERORA-COMPLY-REQ-002` | R-012 | Q-041 | `INTEGRATION` |
| `IF-LOG-012` | A failsafe reported once and then absent is recorded as **going stale**, not as cleared | The staleness transition is its own record. The ground station never writes a clearance it did not receive | `AERORA-SAFE-REQ-009`, `AERORA-SAFE-REQ-010` | R-006 | Q-007 | `INTEGRATION` |
| `IF-LOG-013` | The receive time **is** the freshness; it is stamped at parse and never recomputed at write | A value reaching a record without its receive time is a defect detectable by test | `AERORA-GCS-REQ-027`, `AERORA-COMPLY-REQ-004` | — | — | `UNIT` |

### The telemetry record is not re-specified here

`IF-TLM-012` already defines it, including the property that every Aerora Fact reaches the CSV with
no Aerora logging code because the fact groups are attached to the vehicle. Phase 3 does not restate
it.

One constraint does belong in both places, because it is a logging defect enforced as a telemetry
rule: the CSV header is written **once** at file open while each row re-enumerates the fact groups,
so a fact group created after the file opens misaligns every subsequent column. `GCS-VERIFIED`, R-027.
`AERORA-GCS-REQ-022` forbids dynamic fact-group creation, and `IF-TLM-008` carries the contract.

Aerora's own fact groups are created during vehicle construction and so do not add to the problem.
The constraint is cheap to honour now and expensive to discover from a misaligned flight record six
months later.

## Interface contracts — write ordering, upload and integrity

| ID | Interface | Direction | Producer | Consumer | Crossing | Update semantics | Ack | Authority | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-LOG-014` | Record writer queue | GCS internal | All record producers | Aerora log writer thread | Queued records, on a **bounded** queue | Asynchronous; **never blocks the main thread** | — | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-LOG-015` | Drop accounting | GCS internal | Record writer queue | Audit record store, operator display | A monotonic count of dropped records, itself written into the record | On overflow, and periodically | — | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-LOG-016` | Local-first write ordering | GCS internal | Record writer | Local storage, then upload queue | The ordering constraint itself: local write completes before an upload is attempted | Per record | Local write is the acknowledgement | Ground station | `GCS-DECIDED` | `CONFIRMED` |
| `IF-LOG-017` | Upload submission | GCS to Aerora backend | Cloud module | Aerora backend | Already-written local records, by copy | Opportunistic, from a bounded queue | Backend accepts; a local record is not altered by the outcome | Ground station | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-LOG-018` | Upload to a non-Aerora endpoint | — | — | — | **Nothing. Automatic upload to any non-Aerora endpoint is disabled by the Aerora settings defaults** | — | — | Ground station | `GCS-VERIFIED` | `PROPOSED` |
| `IF-LOG-019` | Local record deletion on upload | — | — | — | **Nothing. Deletion of a local log after upload stays disabled and is not operator-configurable** | — | — | Ground station | `GCS-VERIFIED` | `PROPOSED` |
| `IF-LOG-020` | Audit store integrity protection | GCS internal | Audit record store | Itself | An integrity property over the local store | Per record or per segment, **mechanism undecided** | — | Ground station | `NOT-EVIDENCED` | `OPEN` |
| `IF-LOG-021` | Record timestamp basis | GCS internal | Station clock | All record writers | A local monotonic-plus-wall-clock pair, **not a service clock** | Per record | — | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-LOG-022` | Correlation identity | GCS internal | Command tracker, identity binding | All record writers | Request identity, vehicle identity, session identity, and the MAVLink correlation where one exists | Per record | — | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-LOG-023` | Retention deletion record | GCS internal | Retention process | Audit record store | What was deleted, when, and under which policy | Per deletion | — | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-LOG-024` | Record export | GCS to operator | Record store | An operator-chosen destination | A copy of selected records, with their integrity material where it exists | Operator action | Export completion reported | Ground station | `GCS-VERIFIED` | `PROPOSED` |

| ID | Freshness semantics | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `IF-LOG-014` | Queue depth is itself observable | **Drops, never blocks.** A full disk degrades the record; it does not freeze the operator interface. This is the **writer** queue, not the upload queue — see `IF-CLD-014` | `AERORA-GCS-REQ-025` | — | — | `UNIT` |
| `IF-LOG-015` | Counter is current as of the last write | A gap with a count of 412 dropped entries is usable evidence; a gap with no count is not | `AERORA-GCS-REQ-025` | — | — | `UNIT` |
| `IF-LOG-016` | Local write time is the record time; upload time is not | If the local write fails, **no upload is attempted** — an uploaded record with no local counterpart would be unverifiable | `AERORA-AWS-REQ-011`, `AERORA-SEC-REQ-019` | — | — | `INTEGRATION` |
| `IF-LOG-017` | Uploaded data carries its original local timestamps | An unreachable backend leaves records queued; the queue is bounded and its depth and drop count are visible | `AERORA-AWS-REQ-010`, `AERORA-AWS-REQ-011` | — | Q-030, Q-042 | `INTEGRATION` |
| `IF-LOG-018` | — | Verified by the all-network-blocked test observing the attempted connection, not by reading the default | `AERORA-GCS-REQ-023`, `AERORA-TEST-REQ-005` | R-022 | Q-039 | `INTEGRATION` |
| `IF-LOG-019` | — | The local record is the evidence; an upload is a copy. Locked rather than defaulted | `AERORA-GCS-REQ-024`, `AERORA-COMPLY-REQ-005` | R-022 | Q-039 | `STATIC` |
| `IF-LOG-020` | Integrity material is written with the record, not added later | **Unresolved.** Until a mechanism is chosen the store is tamper-evident by nothing, and `AERORA-GCS-REQ-028` is unmet | `AERORA-GCS-REQ-028` | — | Q-037 | `INTEGRATION` |
| `IF-LOG-021` | Monotonic component survives a wall-clock adjustment | A wall-clock jump is recorded as an event; records either side of it remain orderable through the monotonic component | `AERORA-COMPLY-REQ-004` | — | Q3-008 | `UNIT` |
| `IF-LOG-022` | Identity is bound at record creation | A record with no vehicle identity is written with an explicit unidentified marker, **never discarded and never attributed by guess** | `AERORA-GCS-REQ-018`, `AERORA-VEH-REQ-005` | R-017, R-025 | Q-016 | `UNIT` |
| `IF-LOG-023` | Deletion time and policy identity both recorded | A deletion that cannot be recorded does not proceed. Retention never deletes a record not yet uploaded or exported | `AERORA-COMPLY-REQ-005` | — | Q-039 | `INTEGRATION` |
| `IF-LOG-024` | Export copies; it does not move or clear | A partial export is reported as partial. An export is not a substitute for retention | `AERORA-COMPLY-REQ-005` | — | Q-039 | `DEMO` |

### Why two of these specify that nothing crosses

`IF-LOG-018` and `IF-LOG-019` are negative contracts against upstream defaults that are wrong for
Aerora. Both findings are `GCS-VERIFIED`.

| Upstream default | Value | Aerora value | Why |
| --- | --- | --- | --- |
| Auto-upload of flight logs | **Enabled** | Disabled explicitly by the Aerora settings default | The destination defaults to a public third-party log-review service |
| Delete local log after upload | Disabled | Disabled **and locked** | A deleted local record is not evidence |

Three details make the first one worse than it first reads:

| Detail | Consequence |
| --- | --- |
| The only thing preventing transmission is an empty operator email field | Aerora logs are one filled-in text field away from a public service |
| Setting the upload URL to empty **resets it to the upstream default** | Clearing the URL does not disable upload |
| The gate is an unrelated field | A contributor filling in an email for a different reason enables transmission |

So Aerora disables auto-upload at the settings default and does **not** rely on the empty email
address. Where Aerora uploads, it uploads to the Aerora backend through the Cloud module —
`IF-LOG-017`, a different path entirely.

This is also why the verification method for `IF-LOG-018` is `INTEGRATION` rather than `STATIC`. A
static check confirms today's default; the all-network-blocked test of `AERORA-TEST-REQ-005` catches
an accidental re-enablement by observing the attempted connection. R-022 is mitigated by the test,
not by the setting.

## Local-first is a security property

| Artifact | Written first | Then |
| --- | --- | --- |
| Audit record | Local disk, durably enough to survive a crash | Queued for upload |
| Telemetry record | Local disk | Queued for upload |
| Mission plan | Local disk | Synchronised |
| Configuration change | Local disk | Reported as applied |
| Application diagnostic log | Local disk | Not uploaded |

A locally written record exists independent of whether a transfer succeeded, and its timestamp is
the station's rather than a service's. That independence is the point: **a record that only exists
after a successful upload can be suppressed by interfering with the upload.** It is a resilience
property and an anti-repudiation property at the same time, which is why `AERORA-AWS-REQ-011` and
`AERORA-SEC-REQ-019` both state it.

## Correlation: why a ground-station request identity exists

MAVLink acknowledgements are matched by component and command. `GCS-VERIFIED`. There is no unique
per-request identifier on the wire.

| Consequence | Detail |
| --- | --- |
| Two sequential identical commands are indistinguishable in the acknowledgement | R-025 |
| Without a ground-station identity, their audit records cannot be told apart | Which defeats the record |
| `AERORA-GCS-REQ-018` assigns a ground-station request identity | Recorded **alongside** any MAVLink correlation, never instead of it |

Whether a per-command request identifier should exist on the wire at all is Q3-002, raised against
`IF-MAV-024`. It is not assumed here: `IF-LOG-022` works with what the protocol provides.

### What a record is keyed on

| Key | Source | If absent |
| --- | --- | --- |
| Request identity | Assigned by the ground station | Never absent for a command record |
| Vehicle identity | `IF-VID-004` identity binding | Explicit unidentified marker |
| Session identity | Assigned at sign-in | Explicit unidentified marker |
| Operator identity | Security module, `IF-SEC-007` | Recorded as unattributed; the condition is itself an event |
| MAVLink correlation | The acknowledgement, where one exists | Omitted, not fabricated |

Four of the five can be absent and the record still gets written. That is deliberate: an incomplete
record with its gaps named is evidence, and a withheld record is not.

## Timestamps

| Property | Position | Evidence | Status |
| --- | --- | --- | --- |
| Records carry station local time | Required | `GCS-DECIDED` | `PROPOSED` |
| Records carry a monotonic component so ordering survives a clock adjustment | Required | `GCS-DECIDED` | `PROPOSED` |
| A wall-clock adjustment is itself a recorded event | Required | `GCS-DECIDED` | `PROPOSED` |
| Telemetry values carry their receive time, stamped at parse | Required | `GCS-DECIDED` | `PROPOSED` |
| Vehicle-reported time is recorded where the vehicle reports one | Required | `NOT-EVIDENCED` | `OPEN` |
| A required bound on station-to-vehicle clock skew | **Not specified** | — | Q3-008 |

The last two rows are the gap. Correlating an audit record stamped with station time against a
telemetry value stamped at parse is straightforward on one station. Correlating either against
vehicle-reported time across a skewed clock is not, and nothing in the baseline sets a bound on the
skew or requires that it be recorded. Q3-008 asks for that bound rather than assuming one.

## Integrity

`AERORA-GCS-REQ-028` requires the local audit store to be integrity-protected. The mechanism is
Q-037 and is not chosen here.

| Property | Position |
| --- | --- |
| An audit record that can be edited is not evidence | The requirement's rationale |
| The mechanism is undecided | Q-037, `ARCH-SHAPING` |
| Integrity material is written with the record | Not computed later over a store that may already have been altered |
| Export carries the integrity material | `IF-LOG-024` |
| Until a mechanism exists, `IF-LOG-020` is `OPEN` and the requirement is unmet | Stated, not glossed |

Choosing a mechanism here would be inventing a security control, which Phase 3 does not do. What
Phase 3 can do is fix the contract shape — written with the record, exported with the record — so
that whichever mechanism is chosen fits without redesign.

## Retention

Q-039, deferrable. The architecture commits only to what retention must not be able to do, and
Phase 3 adds nothing to that list.

| Constraint | Reason | Requirement |
| --- | --- | --- |
| Shall not delete an audit record before it is uploaded or exported | It is the evidence | `AERORA-COMPLY-REQ-005` |
| Shall not be the mechanism bounding disk use during a flight | That is the queue's job; retention operates between flights | `AERORA-GCS-REQ-025` |
| Every deletion shall be recorded | A gap with a reason is usable; a gap without one is not | `AERORA-COMPLY-REQ-005` |

## Conflicts recorded

| Conflict | Position A | Position B | Resolution authority |
| --- | --- | --- | --- |
| Audit record integrity | `AERORA-GCS-REQ-028` requires integrity protection | No mechanism is specified anywhere, and Q-037 is open | **Unresolved.** Security and product owners, via Q-037 |
| Retention versus evidence | `AERORA-COMPLY-REQ-005` forbids deleting an un-uploaded record | A station with no backend connectivity accumulates records without bound | **Recorded.** Q-039 must set a policy that does not require deleting evidence; `IF-LOG-015` makes the accumulation visible rather than silent |

The second is a real tension rather than a drafting artifact. A disconnected station eventually
fills its disk, and the only honest options are to stop recording with an accounted drop or to
delete an un-uploaded record. The architecture chooses the first and makes it visible; a policy
that prefers the second has to say so explicitly.

## Not specified, and why

| Item | Reason |
| --- | --- |
| Audit record format and serialisation | Not evidenced; no baseline statement. The field set is specified, the encoding is not |
| Integrity mechanism | Q-037 |
| Retention periods and upload cadence | Q-039 |
| Queue depth, drop thresholds and flush intervals | Implementation tuning; the properties are specified, the numbers are not |
| Whether the audit store is one store per station or one per vehicle identity | Not evidenced; depends on Q-016 |
| Station-to-vehicle clock skew bound | Q3-008 |
| Operator identity format | Q-029 |
| Which records the backend is entitled to receive | Q-030, and [15-cloud-integration-icd.md](15-cloud-integration-icd.md) |

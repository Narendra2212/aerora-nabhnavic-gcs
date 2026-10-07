# Audit Implementation

**F3.1-03** treated as a first-class Phase 4 problem. The audit record is the largest body of new
Aerora code and the one with the fewest external dependencies — and its integrity mechanism does not
exist.

## Audit requirements, exactly

| # | Requirement | Obligation |
| --- | --- | --- |
| 1 | `AERORA-GCS-REQ-020` | Application diagnostic, telemetry and audit records are **separate records** |
| 2 | `AERORA-GCS-REQ-021` | The audit record is **unconditional** — not gated on armed state, telemetry settings, or a vehicle being connected |
| 3 | `AERORA-GCS-REQ-025` | The writer **never blocks the main thread** and accounts for every dropped record |
| 4 | `AERORA-GCS-REQ-028` | The local audit store is **integrity-protected**. Mechanism depends on Q-037 |
| 5 | `AERORA-SEC-REQ-019` | Records carry operator identity, request identity and local timestamps, and are **written locally before any upload** |
| 6 | `AERORA-COMPLY-REQ-002` | Every command produces one record with all state transitions and the terminal reason |
| 7 | `AERORA-COMPLY-REQ-003` | An authority mode **request** and the reported **effective** change are separate events |
| 8 | `AERORA-COMPLY-REQ-004` | Every logged value carries its receive time and reporting source |
| 9 | `AERORA-COMPLY-REQ-005` | Retention never deletes a record before upload or export, and every deletion is recorded |

Requirement 4 is the one with no mechanism. Everything else is implementable today.

## Existing QGC logging facilities, and what they are not

| Facility | What it is | Reusable for audit? |
| --- | --- | --- |
| `src/LogManager/LogManager.h` | The **application diagnostic** log: category interning, per-category rate buckets, I/O error tracking, export | **No.** It is debug output with no retention, integrity or schema contract |
| `src/Vehicle/MAVLinkLogManager.h` | The per-vehicle **MAVLink stream** log, with upload and auto-start | **No.** A protocol capture, not a record of operator intent |
| CSV telemetry writer in `Vehicle` | A 1 Hz row of every Fact | **No**, and it is **conditional** on armed state — see below |
| `QGCLoggingCategory` | Category declaration and runtime configuration | Used for diagnostics only |
| `AppSettings` save paths | Separate directories for telemetry, logs, missions, parameters | **Yes** — for path resolution |

### Why the telemetry writer cannot serve as audit

The CSV writer opens its file only once the vehicle is armed, unless a save-when-not-armed setting is
enabled — `src/Vehicle/Vehicle.cc:2826-2829`. `GCS-VERIFIED`.

That conditionality is reasonable for telemetry and unacceptable for audit, because the actions an
investigation most often needs happen **before** arming: a parameter change, an authority mode
request, a payload command, an authorisation denial, a mission upload. All five fall outside the
telemetry window.

### Does QGC have anything append-only, integrity-protected or tamper-evident?

**No.** Stated plainly, because it is the finding.

| Property | Present in QGC |
| --- | --- |
| Append-only record store | No |
| Integrity protection of any log | No |
| Tamper-evidence | No |
| A structured-record writer a custom build could reuse | **No** |
| Flush-on-write durability in a log path | Only in the signing key store's explicit `sync()` |

Therefore **Aerora must provide its own writer**. There is nothing to extend, and nothing to subclass.
That is unusual in this programme — almost everywhere else the answer has been a seam — and it is
also the easiest of the four findings, because owning the whole implementation means owning the whole
contract.

## The implementation

| Artefact | Content | Status |
| --- | --- | --- |
| `AeroraAuditRecord` | The record type: event kind, correlation identity, timestamps, payload fields | `IMPLEMENTATION-PLANNED` |
| `AeroraAuditWriter` | Bounded queue, dedicated thread, local-first ordering, drop accounting | `IMPLEMENTATION-PLANNED` |
| `AeroraAuditStore` | Append-only local store, rotation, export | `IMPLEMENTATION-PLANNED` |
| `AeroraAuditIntegrity` | **The interface only.** No mechanism | **`BLOCKED`** — F3.1-03 |
| `AeroraCorrelation` | Request identity, vehicle identity, session identity, operator identity | `IMPLEMENTATION-PLANNED` |
| Retention process | Policy-driven deletion, each deletion recorded | `CONTRACT-ONLY` — Q-039 |

### Write ordering

```text
event occurs
   │
   ▼
record composed, timestamped at the event
   │
   ▼
enqueued on the bounded writer queue        never blocks the caller
   │
   ▼
written to local storage, durably           THIS is the evidence
   │
   ▼
queued for upload                           a copy, never the record
```

| Rule | Reason |
| --- | --- |
| Local write completes **before** an upload is attempted | `AERORA-AWS-REQ-011`, `AERORA-SEC-REQ-019` |
| If the local write fails, **no upload is attempted** | An uploaded record with no local counterpart is unverifiable |
| The upload outcome never alters the local record | The record is not a function of the network |
| Configuration changes are recorded **before** being reported as applied | `IF-LOG-010` — the one place the record gates the act |

Local-first is a **security** property, not only a resilience one: a record that exists only after a
successful upload can be suppressed by interfering with the upload.

### Durability and crash behaviour

| Concern | Decision | Status |
| --- | --- | --- |
| A record must survive a process crash | Flushed durably enough to survive, before the operator is told the action succeeded | `IMPLEMENTATION-PLANNED` |
| Flush frequency | Per record for audit; batched for telemetry | `IMPLEMENTATION-PLANNED` |
| A torn final record after a crash | Detectable and recorded as truncated, **not silently dropped** | `IMPLEMENTATION-PLANNED` |
| Full disk | **Drop with an accounted counter, never block.** A full disk degrades the record; it does not freeze the interface | `IMPLEMENTATION-PLANNED` |
| Drop accounting | A monotonic count, written into the record and shown to the operator | `IMPLEMENTATION-PLANNED` |

A gap with a count of 412 dropped entries is usable evidence. A gap with no count is not.

### Storage location, rotation, export

| Concern | Decision | Status |
| --- | --- | --- |
| Location | Under the Aerora application save paths, **separate** from telemetry and diagnostic logs | `IMPLEMENTATION-PLANNED` |
| Rotation | By size and time, with rotation itself recorded | `CONTRACT-ONLY` — Q-039 |
| Retention | Never deletes a record not yet uploaded or exported; every deletion recorded | `CONTRACT-ONLY` — Q-039 |
| Export | A copy with its integrity material, operator-initiated, partial exports reported as partial | `IMPLEMENTATION-PLANNED` |
| Deletion to free space | **Never for an un-uploaded audit record.** Stop recording visibly instead | `IMPLEMENTATION-PLANNED` |

That last row is a real tension, recorded rather than resolved: a disconnected station eventually
fills its disk, and the only honest options are to stop recording with an accounted drop or to delete
evidence. The architecture chooses the first.

## F3.1-03 — the integrity mechanism

### The problem

`AERORA-GCS-REQ-028` requires integrity protection. Q-037 is `ARCH-SHAPING` and open. Phase 3 fixed
the **contract shape** — integrity material written with the record, exported with the record — which
makes a mechanism *replaceable*, not *deferrable*.

| Risk | Detail |
| --- | --- |
| A stub satisfying the shape | Would be a security control that does nothing while `AERORA-GCS-REQ-028` reads as met |
| This is why it is `BLOCKED` | Not because the code is hard |

### The required seam, specified

Phase 4 specifies the seam without choosing the mechanism.

| Element | Specification |
| --- | --- |
| Interface | `AeroraAuditIntegrity` — an abstract type with one responsibility: produce integrity material for a record, and verify a record against it |
| Call site | Inside `AeroraAuditWriter`, on the write path, **before** the record is considered written |
| Export | `AeroraAuditStore` export carries the material alongside the records |
| Verification | An offline verification entry point that takes an exported set and reports per-record validity |
| Default implementation | **None shipped.** The writer refuses to start without an injected implementation |

### Why "refuses to start" rather than "no-op default"

A no-op default is the stub failure mode. A writer that will not start without an integrity
implementation makes the gap **loud at the first build** rather than silent at the first audit.

| Consequence | Assessment |
| --- | --- |
| Phase 4 cannot produce a working audit writer | Correct. `IF-LOG-020` is `BLOCKED` |
| The rest of the audit module is still implementable | Yes — record, correlation, queue, ordering, drop accounting, export |
| The blocking surface is one injected dependency | Small, explicit, and impossible to forget |

**This is implementation-blocking for `IF-LOG-020` and for any claim that
`AERORA-GCS-REQ-028` is met.** It is not blocking for the other 23 `IF-LOG` interfaces.

### Tamper considerations, for whoever answers Q-037

Recorded as input, not as a decision.

| Consideration | Note |
| --- | --- |
| "A compromised operator host" is explicitly out of scope | So the mechanism cannot assume the host is trustworthy, and cannot pretend to defend against it |
| The threat addressed is **repudiation**, not host compromise | An operator editing their own record after the fact |
| A keyed mechanism needs a key | Which lands back on F3.1-01's unresolved key storage |
| An unkeyed mechanism detects accidental corruption only | Honest, and weaker than the requirement's rationale implies |
| The two findings are coupled | Recorded as **R4-002** |

That coupling is the substantive discovery here: **F3.1-03 cannot be fully solved before F3.1-01**,
because any integrity mechanism strong enough to resist repudiation needs a key the station cannot
currently protect.

## Correlation

| Key | Source | If absent |
| --- | --- | --- |
| Request identity | Assigned by the ground station | Never absent for a command record |
| Vehicle identity | `IF-VID-004` identity binding | Explicit unidentified marker |
| Session identity | Assigned at sign-in | Explicit unidentified marker |
| Operator identity | `M-Security`, `IF-SEC-007` | Recorded as unattributed; the condition is itself an event |
| MAVLink correlation | The acknowledgement, where one exists | Omitted, **never fabricated** |

Four of five may be absent and the record is still written. An incomplete record with its gaps named
is evidence; a withheld record is not.

The ground-station request identity exists because acknowledgements are matched by `(compId, command)`
— `GCS-VERIFIED` — so two sequential identical commands are otherwise indistinguishable. R-025.

## Timestamps

| Property | Decision | Status |
| --- | --- | --- |
| Station local time, never a service clock | Required | `IMPLEMENTATION-PLANNED` |
| A monotonic component so ordering survives a clock adjustment | Required | `IMPLEMENTATION-PLANNED` |
| A wall-clock adjustment is itself a recorded event | Required | `IMPLEMENTATION-PLANNED` |
| Telemetry values carry their parse-time receive time | Required | `IMPLEMENTATION-PLANNED` |
| Station-to-vehicle clock skew bound | **Not specified** — Q3-008 | `CONTRACT-ONLY` |

## Upload defaults that must change

Two upstream defaults are wrong for Aerora, both `GCS-VERIFIED`.

| Default | Upstream | Aerora | Status |
| --- | --- | --- | --- |
| Auto-upload of flight logs | **Enabled**, to a public third-party log service, gated only by an empty operator email field | **Disabled explicitly** by the Aerora settings default | `IMPLEMENTATION-PLANNED` |
| Delete local log after upload | Disabled | Disabled **and locked**, not operator-configurable | `IMPLEMENTATION-PLANNED` |

Two details make the first worse: setting the upload URL to empty **resets it to the upstream
default**, and the gate is an unrelated field, so a contributor filling in an email for another
reason enables transmission.

Aerora disables it at the settings default and does **not** rely on the empty email. Verification is
`INTEGRATION`, not `STATIC`: the all-network-blocked test catches an accidental re-enablement by
observing the attempted connection. R-022.

## Interface status

| Interface | Subject | Status |
| --- | --- | --- |
| `IF-LOG-001`, `IF-LOG-002` | Diagnostic log, MAVLink stream log | Inherited unchanged |
| `IF-LOG-003` to `IF-LOG-013` | Audit store, operator action, command lifecycle, authority, confirmation, security, configuration, mission, failsafe, provenance | `IMPLEMENTATION-PLANNED` |
| `IF-LOG-014` to `IF-LOG-017` | Writer queue, drop accounting, local-first ordering, upload submission | `IMPLEMENTATION-PLANNED` |
| `IF-LOG-018`, `IF-LOG-019` | Non-Aerora endpoint, deletion on upload | `IMPLEMENTATION-PLANNED` as **prohibition assertions** |
| `IF-LOG-020` | Integrity protection | **`BLOCKED`** — F3.1-03 |
| `IF-LOG-021` to `IF-LOG-024` | Timestamps, correlation, retention deletion, export | `IMPLEMENTATION-PLANNED`; retention policy `CONTRACT-ONLY` |

23 of 24 planned, one blocked. `M-Audit` is therefore the **first module that can be built after the
bootstrap** — it needs no dialect, no hardware and no product decision — and it is the natural pilot
for the module conventions.

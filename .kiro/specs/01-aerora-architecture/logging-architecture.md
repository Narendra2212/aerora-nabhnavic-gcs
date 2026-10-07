# Logging Architecture

The three kinds of record the Aerora GCS produces, why they are kept separate, and the upstream
defaults that must be changed before Aerora ships.

## Three records, not one log

| Record | Audience | Purpose | Survives what |
| --- | --- | --- | --- |
| Application diagnostic log | Engineers | Diagnosing GCS behaviour | Nothing in particular; it is debug output |
| Telemetry record | Flight operations and analysis | What the aircraft did | Must survive a GCS crash |
| Audit record | Accountability and investigation | What the operator did, and what the system told them | Must survive everything, including telemetry logging being off |

Collapsing these into one log is the obvious simplification and it is wrong. They have different
retention periods, different consumers, different volumes, and — decisively — different integrity
requirements. An audit record that can be lost by turning off telemetry logging is not an audit
record.

### The separation that matters most

Telemetry recording in QGroundControl is conditional. The CSV telemetry writer only opens its file
once the vehicle is armed, unless the save-when-not-armed setting is enabled
(`Vehicle.cc:2826-2828`).

That conditionality is reasonable for telemetry and unacceptable for audit. An operator action
taken before arming — a parameter change, an authority mode request, a payload command, an
authorisation denial — is exactly the kind of event an investigation needs, and it would fall
outside the telemetry window entirely.

**The Aerora audit record is unconditional.** It is not gated on armed state, not gated on a
telemetry setting, and not gated on a vehicle being connected.

## What upstream provides

| Subsystem | Role |
| --- | --- |
| `LogManager` | Application diagnostic log: category interning, per-category rate buckets, I/O error tracking, export to a destination file |
| `MAVLinkLogManager` | Per-vehicle MAVLink stream log, with upload and auto-start |
| CSV telemetry writer in `Vehicle` | A 1 Hz row of every vehicle Fact and every FactGroup Fact |
| `AppSettings` save paths | Separate directories for telemetry, missions, parameters, logs, video and photos |

The rate buckets in `LogManager` are worth noting: upstream already solved the problem of a
chattering log category drowning the file, which is the same problem the Aerora log writer's
bounded queue addresses at a different layer.

### Aerora telemetry is logged for free

The CSV writer enumerates `factNames()` and then every FactGroup's facts, for the header at file
open (`Vehicle.cc:2812-2820`) and for each row (`Vehicle.cc:2845-2850`).

Because Aerora telemetry is attached through `FirmwarePlugin::factGroups()` — see
[vehicle-integration.md](vehicle-integration.md) — every Aerora Fact appears in the telemetry CSV
with no Aerora logging code at all. That is the third free benefit of choosing that attachment
route.

### An upstream defect this exposes

The CSV header is written **once**, at file open, from the fact-group set that exists at that
moment. Each row is written from the fact-group set that exists when the row is written.

QGroundControl creates some fact groups dynamically on message receipt — battery, ESC status and
gimbal groups are added at runtime. If one of those appears after the CSV file has been opened,
subsequent rows carry more fields than the header declares, and the columns silently misalign.

| Property | Assessment |
| --- | --- |
| Aerora's six FactGroups are created during vehicle construction | They exist before the first row, so Aerora does not add to the problem |
| The defect is real for upstream dynamic groups | A battery group appearing mid-flight shifts every later column |
| It constrains Aerora | **Aerora must not create FactGroups dynamically** |

That constraint is cheap to honour and would be expensive to discover from a misaligned flight
record six months later. It is listed as an optional upstream contribution in
[risk-gates.md](risk-gates.md).

## Upstream defaults that must change

Two defaults in `MAVLinkLogManager` are wrong for Aerora, and one of them is a data-governance
issue rather than a preference.

| Default | Upstream value | Aerora value | Why |
| --- | --- | --- | --- |
| Auto-upload of flight logs | Enabled | **Disabled** | The destination is a third-party endpoint |
| Delete local log after upload | Disabled | **Disabled, and locked** | A deleted local record is not evidence |

### The auto-upload default

Auto-upload is enabled by default (`MAVLinkLogManager.cc:300`). The upload destination defaults to
an upstream public log-review service (`MAVLinkLogManager.h:234`). The only thing preventing
transmission is that the operator email address defaults to empty, and the upload loop skips a file
when the email address is blank (`MAVLinkLogManager.cc:476`).

So Aerora flight logs would be one filled-in text field away from being posted to a public
third-party service.

Two further details make this worse than it first reads:

| Detail | Consequence |
| --- | --- |
| Setting the upload URL to empty resets it to the upstream default (`MAVLinkLogManager.cc:363`) | Clearing the URL does not disable upload |
| The gate is an unrelated field | A future contributor filling in an email address for a different reason enables transmission |

**Aerora disables auto-upload explicitly through the settings default, and does not rely on the
empty email address.** Where Aerora uploads logs, it uploads them to the Aerora backend through the
Cloud module, which is a different path entirely.

This is also why the all-network-blocked test in [testing-architecture.md](testing-architecture.md)
matters: it would catch an accidental re-enablement by observing the attempted connection.

### The delete-after-upload default

The default is already off, and on successful upload the manager deletes the local file when it is
on (`MAVLinkLogManager.cc:759-762`). Aerora keeps it off and treats it as not operator-configurable,
because the local record is the evidence and an upload is a copy.

## Local-first write ordering

| Artifact | Written first | Then |
| --- | --- | --- |
| Telemetry record | Local disk | Queued for upload |
| Audit record | Local disk, synchronously enough to survive a crash | Queued for upload |
| Mission plan | Local disk | Synchronised |
| Configuration change | Local disk | Reported |
| Application diagnostic log | Local disk | Not uploaded by default |

A locally written record is **evidence**: it exists independent of whether a network transfer
succeeded, and its timestamp is the station's rather than a service's. For an audit trail intended
to survive scrutiny, that independence is the whole point — and it is a security property as much
as a resilience one, because a record that only exists after a successful upload can be suppressed
by interfering with the upload.

## The Aerora log writer

One Aerora background thread owns log writing, per
[runtime-architecture.md](runtime-architecture.md).

| Property | Behaviour |
| --- | --- |
| Queue | Bounded |
| Overflow | Drops, and increments an accounted drop counter |
| Drop counter | Visible, and itself written to the record |
| Blocking | Never — the main thread is never blocked by log I/O |
| I/O error | Surfaced, not silently absorbed |

Dropping rather than blocking is deliberate: a full disk must degrade the record, not freeze the
operator interface. Accounting for the drops is what keeps the record honest, because a record with
a gap and a count of 412 dropped entries is usable evidence, and a record with an unexplained gap
is not.

## Record content

### Every Aerora value carries its provenance

| Field | Why |
| --- | --- |
| Value | The datum |
| Receive time | Freshness, stamped at parse time |
| Reporting source | Which component reported it |

This is the same triple the runtime carries in memory, written through unchanged. Reconstructing
freshness after the fact is impossible, so it is never discarded on the way to the record.

### Command records

Each Aerora command produces one audit record carrying:

| Field | Source |
| --- | --- |
| Aerora request identity | Assigned by the GCS |
| Operator identity | Security module |
| Command and parameters | As sent |
| Each command state transition, with its time | The eight-state model in [authority-model.md](authority-model.md) |
| Terminal state and reason | Including the rejection or failure reason |
| MAVLink correlation, where the dialect provides one | Recorded alongside, not instead |

The GCS-assigned request identity exists because MAVLink acknowledgements are matched by component
and command rather than by a unique request id. Without a GCS-side identity, two sequential
identical commands produce audit records that cannot be told apart — which defeats the purpose.

### Authority and failsafe records

| Event | Recorded |
| --- | --- |
| Authority mode request | With operator identity and request identity |
| Reported effective authority mode change | Separately from the request |
| Confirmation prompted and confirmation given | Both, with times |
| Failsafe raised | State, reason, source, severity, reported action, freshness |
| Failsafe value going stale | As a distinct event, because the GCS does not conclude it cleared |

Recording request and effect separately is what makes the record able to answer "was the operator's
request followed?" — which a single combined entry cannot.

## Retention

Retention and upload policy is Q-039 and is deferrable. The architecture commits only to what
retention policy must not be able to do:

| Constraint | Reason |
| --- | --- |
| Retention shall not delete an audit record before it has been uploaded or exported | It is the evidence |
| Retention shall not be the mechanism that bounds disk use during a flight | That is the queue's job; retention operates between flights |
| Deletion shall be recorded | A gap with a reason is usable; a gap without one is not |

## Core edits required

**None.** The upload and delete defaults are Aerora settings defaults supplied through the
overlay's settings metadata, not modifications to upstream code. Aerora log records are written by
the Aerora Diagnostics and Security modules.

## Open questions

| Question | Subject | Impact |
| --- | --- | --- |
| Q-039 | Log retention and upload policy | `DEFERRABLE` |
| Q-037 | Integrity protection mechanism for the local audit record store | `ARCH-SHAPING` |
| Q-016 | Fleet identity source, which records key on | `ARCH-SHAPING` |
| Q-029 | Operator role set, which the operator identity resolves against | `ARCH-SHAPING` |

## Requirements derived

| Identifier | Statement |
| --- | --- |
| `AERORA-GCS-REQ-020` | Application diagnostic, telemetry and audit records shall be maintained as separate records |
| `AERORA-GCS-REQ-021` | The audit record shall be unconditional and shall not be gated on armed state, telemetry settings, or vehicle connection |
| `AERORA-GCS-REQ-022` | Aerora shall not create FactGroups dynamically after vehicle construction |
| `AERORA-GCS-REQ-023` | Automatic upload of flight logs to any non-Aerora endpoint shall be disabled by the Aerora settings defaults |
| `AERORA-GCS-REQ-024` | Deletion of a local log after upload shall remain disabled and shall not be operator-configurable |
| `AERORA-GCS-REQ-025` | The log writer shall never block the main thread, and shall account for every dropped record |
| `AERORA-COMPLY-REQ-002` | Every Aerora command shall produce one audit record carrying operator identity, request identity, all command state transitions and the terminal reason |
| `AERORA-COMPLY-REQ-003` | An authority mode request and the reported effective change shall be recorded as separate events |
| `AERORA-COMPLY-REQ-004` | Every logged Aerora value shall carry its receive time and reporting source |
| `AERORA-COMPLY-REQ-005` | Retention policy shall not delete an audit record before it has been uploaded or exported, and every deletion shall be recorded |

Full records in [requirements.md](requirements.md).

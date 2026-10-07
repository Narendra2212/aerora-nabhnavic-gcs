# Cloud Boundary

Adapters and prohibitions. **No AWS service is implemented in Phase 4.**

## The structure

```text
Aerora GCS
│
├── Local real-time operation      unaffected by cloud availability
├── Local audit                    the record, written first
├── Local configuration            last-applied retained
│
└── Optional cloud integration     M-Cloud, one background thread
       │
       └── AWS backend             11 modules, typed interfaces
```

## Four empty interfaces

The structural properties, each implemented as an assertion rather than a policy.

| Property | Interface | Implementation | Status |
| --- | --- | --- | --- |
| No command, authorisation, arming check, identity resolution, entitlement check or configuration fetch blocks on a network service | `IF-CLD-004` | The all-network-blocked integration test | `IMPLEMENTATION-PLANNED` |
| The Security module has no reference to the Cloud module | `IF-SEC-010` | Architectural-property test — the **one** dependency rule with a test assigned | `IMPLEMENTATION-PLANNED` |
| No synchronous network call originates on the main thread | `IF-CLD-002` | Threading contract plus a test | `IMPLEMENTATION-PLANNED` |
| No inbound network connection is accepted | `IF-CLD-003` | Property test **and** startup self-check | `IMPLEMENTATION-PLANNED` |

The fourth needs both because a test does not run in a deployed build, and `qthttpserver` is a
**required build dependency** — `.github/build-config.json` lists it among the Qt modules — linked for
two type aliases with no listener ever created. Its presence is what makes the assertion worth making.

## The eight hidden-dependency traps

Named so they can be refused by name, each with the Phase 4 refusal mechanism.

| Trap | How it arrives | Refusal | Mechanism |
| --- | --- | --- | --- |
| Authentication | "Check the token before arming" | Validated offline | `IF-SEC-002`, offline-primary |
| Configuration | "Fetch the configuration at connect" | Cached locally, usable offline | `IF-CLD-007` |
| Fleet identity | "Resolve the identifier against the registry first" | Local identifier; the registry **reconciles**, never resolves | `IF-VID-007` |
| Logging | "Stream telemetry to the cloud so we have the record" | Local first; upload is a second step | `IF-LOG-016` |
| Map data | "Download the tiles for the area" | Pre-cached; a missing tile degrades the map | `IF-CLD-011` |
| Time | "Use network time so timestamps align" | Local clock is authoritative for the record | `IF-LOG-021` |
| Software update | "Check for a mandatory update before flight" | Advisory, **never blocking** | `IF-CLD-008` |
| Licensing or entitlement | "Verify the installation is licensed" | **Not a flight-path check under any circumstances** | No interface exists |

The last one earns its emphasis: an entitlement check on a flight path turns a commercial system into
a safety system.

## Adapters

One adapter per backend module. No AWS SDK call is written in Phase 4.

| Backend module | Adapter | Interface | Status |
| --- | --- | --- | --- |
| Identity and access | `AeroraIdentityAdapter` | `IF-SEC-009` | `IMPLEMENTATION-PLANNED` |
| Organisation and fleet management | `AeroraFleetAdapter` | `IF-CLD-006` | `IMPLEMENTATION-PLANNED` |
| Vehicle registry | `AeroraRegistryAdapter` | `IF-VID-007` | `IMPLEMENTATION-PLANNED` |
| Mission management | `AeroraMissionLibraryAdapter` | `IF-CLD-005` | `IMPLEMENTATION-PLANNED` |
| Telemetry and event ingestion | `AeroraRecordUploadAdapter` | `IF-LOG-017` | `IMPLEMENTATION-PLANNED` |
| Device configuration | `AeroraConfigurationAdapter` | `IF-CLD-007` | `IMPLEMENTATION-PLANNED` |
| Release metadata | `AeroraReleaseAdapter` | `IF-CLD-008` | `IMPLEMENTATION-PLANNED` |
| Audit logging | Shares the record upload adapter | `IF-LOG-017` | `IMPLEMENTATION-PLANNED` |
| Notification and event handling | `AeroraNotificationAdapter` | `IF-CLD-009` | `IMPLEMENTATION-PLANNED` |
| Payload management | Record side only | `IF-PAY` record path | `CONTRACT-ONLY` |
| Reporting | **No adapter** | `IF-CLD-010` carries nothing | `IMPLEMENTATION-PLANNED` as a prohibition |

| Adapter contract | Rule |
| --- | --- |
| Runs on the cloud thread only | Never called from the main thread |
| Returns through queued signals | No blocking return |
| Endpoint, protocol and payload schema | **Not specified** — backend deliverables |
| Authentication scheme to the backend | **Not specified** — not evidenced |
| Failure | Degrades the feature, never the command path |

## Per-feature degradation

There is **no global offline mode** — `AERORA-AWS-REQ-007`. A global mode invites "are we offline?"
where the real question is "is this specific thing available?".

| Feature | Cloud available | Cloud unavailable |
| --- | --- | --- |
| Vehicle connection and control | Unaffected | **Unaffected** |
| Telemetry display | Unaffected | **Unaffected** |
| Mission planning and upload | Unaffected | **Unaffected** |
| Local flight and audit recording | Unaffected | **Unaffected** |
| Operator sign-in | Reconciled against the service | Validated against cached credentials |
| Mission library | Synchronised | Local plans only, marked unsynchronised |
| Fleet view | Current | Last-known, **with its age shown** |
| Registry reconciliation | Live | Deferred, queued |
| Record upload | Continuous | Queued, bounded, **depth visible** |
| Configuration distribution | Applied when received | Last-applied retained |
| Reporting | On the backend | Unavailable, stated plainly |
| Update notification | Shown | Not shown |
| Map and terrain tiles | Fetched and cached | Cached only; gaps shown as gaps |

The first four rows all say the same word, and that is not a coincidence — it is
`AERORA-AWS-REQ-005` and `AERORA-AWS-REQ-006` expressed as a table.

Two rows carry a deliberate obligation. "Last-known, with its age shown" and "depth visible" exist
because the dangerous failure is not the outage; it is an operator who cannot tell current data from
old.

## Two indicators, no shared visual language

| Indicator | Operator question |
| --- | --- |
| Vehicle link | **Can I command the aircraft?** |
| Cloud connection | **Is my data synchronised?** |

`AERORA-AWS-REQ-008`. Conflating them means a cloud outage looks like a link problem, producing
exactly the wrong operator response at exactly the wrong time. Verified by `DEMO`, because whether
two indicators are distinguishable is not a property code can assert.

## Queues

Two distinct queues, confused at the reader's peril.

| Queue | Owner | Bounded | Drop accounting | Interface |
| --- | --- | --- | --- | --- |
| Record **writer** queue | `M-Audit` | Yes | Yes, shown | `IF-LOG-014`, `IF-LOG-015` |
| **Upload** queue | `M-Cloud` | Yes | Yes, shown | `IF-CLD-014` |

The writer queue feeds local disk; the upload queue feeds the network. Unbounded upload queueing
turns a long outage into a memory failure.

## Notifications are informational

| Rule | Consequence |
| --- | --- |
| A notification is informational | `AERORA-AWS-REQ-013` |
| A notification that would direct a flight action is **prohibited** | It would put the cloud on the command path **through the operator** |
| Verification | `STATIC` — the notification path has no action binding |

## Interface status

All 16 `IF-CLD` interfaces: 15 `IMPLEMENTATION-PLANNED`, 1 `CONTRACT-ONLY`.

| Interface | Subject | Status |
| --- | --- | --- |
| `IF-CLD-001`, `IF-CLD-002` | Connectivity state, threading boundary | `IMPLEMENTATION-PLANNED` |
| `IF-CLD-003`, `IF-CLD-004` | Inbound connection, command-path participation | `IMPLEMENTATION-PLANNED` as prohibition assertions |
| `IF-CLD-005` to `IF-CLD-009` | Mission library, fleet view, configuration, release metadata, notification | `IMPLEMENTATION-PLANNED` — adapter shells |
| `IF-CLD-010` | Reporting | `IMPLEMENTATION-PLANNED` as a prohibition |
| `IF-CLD-011` to `IF-CLD-014` | Tiles, data age, per-feature availability, upload queue | `IMPLEMENTATION-PLANNED` |
| `IF-CLD-015` | Synchronisation conflict resolution | `CONTRACT-ONLY` — Q-042. Surface the conflict, **overwrite nothing** |
| `IF-CLD-016` | Backend interface boundary | `IMPLEMENTATION-PLANNED` — typed interfaces, never shared storage |

## Test strategy

| Test | Method | Asserts |
| --- | --- | --- |
| All-network-blocked full operation | `INTEGRATION` | Connect, parameter load, plan, upload, execute, local record — `AERORA-TEST-REQ-005` |
| No listening socket | `UNIT` + startup self-check | `AERORA-TEST-REQ-004`, `AERORA-TEST-REQ-007` |
| Security does not reference Cloud | `UNIT` | The one dependency rule already covered |
| No synchronous main-thread network call | `INTEGRATION` | — |
| Per-feature degradation | `INTEGRATION` | Each cloud-backed feature has a defined local behaviour |
| Upload queue bounded | `UNIT` | Depth and drop count observable |
| Cloud-sourced data shows its age | `UNIT` | — |
| Two indicators distinguishable | `DEMO` | Q-040 gates it |
| Conflict surfaced, nothing overwritten | `INTEGRATION` | — |

The all-network-blocked test is the keystone. It makes the cloud-off-the-command-path property
enforceable rather than aspirational, against eight named traps, and it would catch an accidental
re-enablement of log auto-upload by observing the blocked connection attempt.

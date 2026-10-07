# Cloud Architecture

The Aerora Cloud module, the modular-monolith backend it talks to, and the rules that keep cloud
availability from becoming a flight-safety dependency.

## The one rule

**The cloud is not on the command path.** Nothing an operator does to an aircraft passes through,
waits on, or is authorised by a network service.

The baseline states this as an outcome: AWS must not become a hidden single point of failure for
flight safety. This document treats it as a structural property to be built, because stated as an
outcome it is a thing to be verified repeatedly, and built as a structure it is a thing that is
true by inspection.

| Structural property | How it holds |
| --- | --- |
| The Cloud module has no reference to the command path | Dependency rule, enforced by module boundaries |
| The Authority module has no reference to the Cloud module | Dependency rule; Authority is reachable from Cloud, not the reverse |
| Cloud synchronisation runs on its own background thread | A stalled network call cannot block the main thread the command path uses |
| Every cloud-backed feature has a defined local behaviour | Enumerated below, per feature, not as a global offline mode |

## The hidden-dependency trap

A cloud dependency on the flight path is rarely introduced deliberately. It arrives one convenience
at a time, and each step is individually defensible. These are the specific routes, named so they
can be refused by name:

| Trap | How it would arrive | Refusal |
| --- | --- | --- |
| Authentication | "Operators sign in, so check the token before arming" | Authentication is validated offline; see [security-architecture.md](security-architecture.md) |
| Configuration | "Fetch the vehicle's configuration from the registry at connect" | Configuration is cached locally and usable without the network |
| Fleet identity | "Resolve the vehicle identifier against the registry before trusting it" | The identifier is local; the registry reconciles later |
| Logging | "Stream telemetry to the cloud so we have the record" | Logs are written locally first; upload is a second step |
| Map data | "Download the tiles for the mission area" | Pre-cached; a missing tile degrades the map, not the flight |
| Time | "Use network time so log timestamps align" | Local clock is authoritative for the log; alignment is a post-processing concern |
| Software update | "Check for a mandatory update before flight" | Version checking is advisory and never blocking |
| Licensing or entitlement | "Verify the installation is licensed" | Not a flight-path check under any circumstances |

Eight routes, each of which would be reasonable in a different product. The last one deserves its
emphasis: an entitlement check on a flight path is the single worst version of this trap, because
it makes a commercial system a safety system.

### The test that keeps it true

A build with all network access blocked must complete a full local operation: connect to a vehicle,
load parameters, plan and upload a mission, fly it, and produce a complete local flight record.
This is a test, not a policy — detail in [testing-architecture.md](testing-architecture.md).

## Per-feature degradation

There is no global offline mode. A global mode invites the question "are we offline?" at points
where the real question is "is this specific thing available?". Each feature degrades on its own.

| Feature | Cloud available | Cloud unavailable |
| --- | --- | --- |
| Vehicle connection and control | Unaffected | Unaffected |
| Telemetry display | Unaffected | Unaffected |
| Mission planning and upload | Unaffected | Unaffected |
| Local flight logging | Unaffected | Unaffected |
| Operator sign-in | Validated against the service | Validated against cached credentials |
| Mission library | Synchronised | Local plans only, marked unsynchronised |
| Fleet view | Current | Last-known, with its age shown |
| Vehicle registry reconciliation | Live | Deferred, queued |
| Log upload | Continuous | Queued locally, bounded, with the queue depth visible |
| Configuration distribution | Applied when received | Last-applied configuration retained |
| Reporting and dashboards | Available | Unavailable, stated plainly |
| Update notification | Shown | Not shown |

Two entries carry a deliberate obligation. "Last-known, with its age shown" and "queue depth
visible" both exist because the dangerous failure is not the outage; it is an operator who cannot
tell the difference between current data and old data.

## Cloud connectivity is displayed separately

The baseline requires that cloud connectivity be shown separately from vehicle-link connectivity.
The architecture treats these as unrelated indicators with no shared visual language, because the
operator question behind each is different:

| Indicator | Operator question |
| --- | --- |
| Vehicle link | Can I command the aircraft? |
| Cloud connection | Is my data synchronised? |

Conflating them would mean a cloud outage looks like a link problem, which would produce exactly
the wrong operator response.

## The backend

### Modular monolith

The baseline specifies a modular monolith on AWS with eleven internal modules communicating through
typed application interfaces rather than shared database access. The architecture adopts that
without alteration, and records the GCS-side counterpart of each module:

| Backend module | GCS-side counterpart |
| --- | --- |
| Identity and access | Security module — offline-capable credential validation |
| Organisation and fleet management | Cloud module — fleet view, read-mostly |
| Vehicle registry | Data model — the Aerora vehicle identifier, reconciled not resolved |
| Mission management | Mission module — plan library synchronisation |
| Payload management | Payload module — payload records, not payload control |
| Telemetry and event ingestion | Logging — upload of locally written records |
| Device configuration | Configuration — distribution of a cached, versioned configuration |
| Software and firmware release metadata | Diagnostics — advisory version and compatibility information |
| Audit logging | Logging — the audit trail, written locally first |
| Reporting | None on the GCS; a backend and web concern |
| Notification and event handling | Cloud module — operator notifications, never flight-directive |

The last row's qualifier matters. A notification that instructs an operator to take a flight action
would put the cloud on the command path through the operator. Notifications are informational.

### Typed interfaces, not shared storage

Module communication through typed application interfaces rather than database coupling is what
makes the baseline's future-extraction goal real rather than aspirational. It also has an immediate
benefit for the GCS: the GCS talks to defined interfaces, so a backend refactor that extracts
telemetry ingestion into its own service is invisible to the ground station.

## Local before cloud

The fifth architectural principle, applied concretely:

| Artifact | Written where first | Then |
| --- | --- | --- |
| Flight log | Local disk | Queued for upload |
| Audit record | Local disk | Queued for upload |
| Mission plan | Local disk | Synchronised |
| Configuration change | Local disk | Reported |

Writing locally first is not only a resilience measure. A locally written record is **evidence**: it
exists independent of whether a network transfer succeeded, and its local timestamp is not subject
to a service's clock. For an audit trail intended to survive scrutiny, that distinction is the
whole point.

## Security dependency direction

The Cloud module depends on the Security module. The Security module does not depend on the Cloud
module. This is the fourth dependency rule from
[component-architecture.md](component-architecture.md), and the reason is the same trap as above: a
Security module that calls the cloud to answer an authorisation question has put the cloud on the
authorisation path.

```text
Cloud ──→ Security        permitted
Security ──→ Cloud        prohibited
```

Credential refresh is the case that tests this rule. Refresh is initiated by the Cloud module on
Security's behalf and the result is handed to Security; Security never reaches out.

## An upstream finding: the linked HTTP server

QGroundControl links Qt's HTTP server module into the main application target
(`src/CMakeLists.txt:108`, and privately at `src/Utilities/Network/CMakeLists.txt:20`). Nothing in
the source tree ever constructs a server object or binds a listening socket — a search for server
construction and socket binding finds only the UDP link, GPS and RTCM inputs.

The module is linked for two type aliases in `src/Utilities/Network/QGCNetworkHelper.h`: an HTTP
status-code enum and an HTTP method enum.

| Observation | Assessment |
| --- | --- |
| No listener is created | No inbound attack surface exists today |
| A full server implementation is linked and available | A future contributor could open a listener with no build-system change to flag it |
| The dependency buys two enumerations | The cost-to-benefit ratio is poor |

This is Q-027, ranked third in
[phase-2-open-question-resolution.md](phase-2-open-question-resolution.md) because it is cheap to
resolve and it touches the security posture. The Aerora position is that the aliases should be
replaced with local definitions and the dependency dropped, which is an upstream contribution
rather than an Aerora core edit. Until then, the startup self-check asserts that no listening
socket is open.

### This finding supersedes the earlier R-016 assessment

Phase 1 recorded R-016 as "Qt6::HttpServer purpose is untraced" at `MEDIUM`, and the first Phase 2
pass left it untraced at `G-IMPL` on the stated grounds that an unauthenticated network endpoint
would be a security concern.

The inspection above traces it and shows **no endpoint exists**. Phase 2.1 therefore amends R-016 to
`LOW` at `G-OPEN`, with the residual item being dependency hygiene rather than security. The
amendment is recorded in [phase-2-risk-gates.md](phase-2-risk-gates.md) alongside the superseded
text, and the reconciliation is in
[phase-2-1-reconciliation.md](phase-2-1-reconciliation.md).

The architecture adds no HTTP listener of its own. **The Aerora GCS accepts no inbound network
connections.** If that ever changes it is a security-architecture decision with an authentication
design, not a convenience.

## Threading

One Aerora background thread serves the cloud, per
[runtime-architecture.md](runtime-architecture.md). It owns network I/O, the upload queue and the
synchronisation state machine. It communicates with the main thread by queued signals only.

| Constraint | Reason |
| --- | --- |
| No synchronous network call from the main thread, ever | A stalled DNS lookup would freeze the operator interface |
| The upload queue is bounded, with a drop counter | Unbounded queueing turns a long outage into a memory failure |
| Queue depth and drop count are visible | An operator with a full queue needs to know before the flight record matters |

## Core edits required

**None.** The Cloud module is an Aerora module in the overlay, attaching through the Aerora plugin.
It touches no QGroundControl path.

## Open questions

| Question | Subject | Impact |
| --- | --- | --- |
| Q-027 | The linked HTTP server module | `IMPL-BLOCKING` |
| Q-016 | Fleet identity source, which the vehicle registry depends on | `ARCH-SHAPING` |
| Q-029 | Operator role set, which identity and access depends on | `ARCH-SHAPING` |
| Q-030 | Which cloud dependencies are permitted, and how each degrades | `ARCH-SHAPING` |
| Q-039 | Log retention and upload policy | `DEFERRABLE` |
| Q-042 | Cloud synchronisation conflict policy | `ARCH-SHAPING` |

## Requirements derived

| Identifier | Statement |
| --- | --- |
| `AERORA-AWS-REQ-005` | No operator command to a vehicle shall depend on cloud availability |
| `AERORA-AWS-REQ-006` | The GCS shall complete connection, parameter load, mission planning, mission upload and local flight recording with all network access blocked |
| `AERORA-AWS-REQ-007` | Each cloud-backed feature shall have a defined local behaviour; there shall be no single global offline mode |
| `AERORA-AWS-REQ-008` | Cloud connectivity shall be displayed separately from vehicle-link connectivity, with no shared visual language |
| `AERORA-AWS-REQ-009` | Cloud-sourced data presented when the cloud is unavailable shall be shown with its age |
| `AERORA-AWS-REQ-010` | The upload queue shall be bounded, and its depth and drop count shall be visible to the operator |
| `AERORA-AWS-REQ-011` | Flight logs and audit records shall be written to local storage before any upload is attempted |
| `AERORA-AWS-REQ-012` | The Security module shall not depend on the Cloud module |
| `AERORA-AWS-REQ-013` | Cloud notifications shall be informational and shall not direct a flight action |
| `AERORA-AWS-REQ-014` | The Aerora GCS shall accept no inbound network connections |
| `AERORA-AWS-REQ-015` | No synchronous network call shall be made from the main thread |

Full records in [requirements.md](requirements.md).

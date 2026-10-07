# Cloud Integration Interface Control Document

The cloud is an optional supporting system. This document specifies what crosses the boundary to it,
what each feature does when it is unreachable, and the four boundaries across which nothing may
cross at all.

Architecture in [cloud-architecture.md](../01-aerora-architecture/cloud-architecture.md).

## The one rule, as a contract

**The cloud is not on the command path.** Nothing an operator does to an aircraft passes through,
waits on, or is authorised by a network service.

The baseline states this as an outcome: AWS must not become a hidden single point of failure for
flight safety. Phase 2 made it a structural property. Phase 3 states the structure as four empty
interfaces and one test.

| Structural property | Interface | Enforced by |
| --- | --- | --- |
| The Cloud module has no reference to the command path | `IF-CLD-004` | Module dependency rule |
| The Security module has no reference to the Cloud module | `IF-SEC-010` | Module dependency rule |
| No synchronous network call from the main thread | `IF-CLD-002` | Threading contract |
| No inbound network connection is accepted | `IF-CLD-003` | No listener exists |
| Every cloud-backed feature has a defined local behaviour | `IF-CLD-013` | Per-feature table, not a global mode |

Stated as an outcome, this is a thing to be re-verified at every change. Stated as four interfaces
that carry nothing, it is a thing an architectural-property test can assert once and keep asserting.
`AERORA-TEST-REQ-004` and `AERORA-TEST-REQ-005` are that test.

## Interface contracts — the empty boundaries

| ID | Interface | Direction | Producer | Consumer | Crossing | Authority | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-CLD-001` | Cloud connectivity state | GCS internal | Cloud module, from its own observation | Operator display | Reachability and synchronisation state, **with no visual language shared with the vehicle link** | Ground station | `BASELINE-ASSERTED` | `CONFIRMED` |
| `IF-CLD-002` | Cloud threading boundary | GCS internal | Cloud module background thread | Main thread | Queued signals only. **No synchronous network call originates on the main thread** | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-CLD-003` | Inbound network connection | — | — | — | **Nothing. No listening socket is opened** | Ground station | `GCS-VERIFIED` | `PROPOSED` |
| `IF-CLD-004` | Cloud participation in the command path | — | — | — | **Nothing. No command, authorisation, arming check, identity resolution, entitlement check or configuration fetch blocks on a network service** | Ground station | `GCS-DECIDED` | `CONFIRMED` |

| ID | Update and freshness semantics | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `IF-CLD-001` | On change; carries the time of the last successful exchange | Unreachable is displayed as unreachable. **A cloud outage must never read as a link problem** | `AERORA-AWS-REQ-008` | — | — | `DEMO` |
| `IF-CLD-002` | Not applicable | A stalled name resolution degrades synchronisation and leaves the operator interface responsive | `AERORA-AWS-REQ-015` | — | — | `INTEGRATION` |
| `IF-CLD-003` | Not applicable | Asserted by architectural-property test **and** by a startup self-check, because a test does not run in a deployed build | `AERORA-AWS-REQ-014`, `AERORA-TEST-REQ-004`, `AERORA-TEST-REQ-007` | R-016 | Q-027 | `UNIT` |
| `IF-CLD-004` | Not applicable | Asserted by the all-network-blocked test completing connect, parameter load, plan, upload, execution and local record | `AERORA-AWS-REQ-005`, `AERORA-AWS-REQ-006`, `AERORA-TEST-REQ-005` | — | Q-030 | `INTEGRATION` |

### The hidden-dependency trap, named route by route

A cloud dependency on the flight path is rarely introduced deliberately. It arrives one convenience
at a time and each step is individually defensible. `IF-CLD-004` is the empty interface these eight
routes would fill.

| Trap | How it would arrive | Refusal |
| --- | --- | --- |
| Authentication | "Operators sign in, so check the token before arming" | Validated offline — `IF-SEC-002` |
| Configuration | "Fetch the vehicle's configuration from the registry at connect" | Cached locally and usable offline — `IF-CLD-007` |
| Fleet identity | "Resolve the identifier against the registry before trusting it" | Local identifier; the registry reconciles later — `IF-VID-007` |
| Logging | "Stream telemetry to the cloud so we have the record" | Local first; upload is a second step — `IF-LOG-016` |
| Map data | "Download the tiles for the mission area" | Pre-cached; a missing tile degrades the map, not the flight — `IF-CLD-011` |
| Time | "Use network time so timestamps align" | Local clock is authoritative for the record — `IF-LOG-021` |
| Software update | "Check for a mandatory update before flight" | Advisory, never blocking — `IF-CLD-008` |
| Licensing or entitlement | "Verify the installation is licensed" | **Not a flight-path check under any circumstances** |

Each of the eight is reasonable in a different product. The last one earns its emphasis: an
entitlement check on a flight path turns a commercial system into a safety system.

### The HTTP server finding, carried forward unchanged

`IF-CLD-003` says nothing crosses, and the evidence is `GCS-VERIFIED` rather than asserted.

| Observation | Assessment |
| --- | --- |
| Qt's HTTP server module is linked into the main application target | A full server implementation is available at link time |
| Nothing in the tree constructs a server or binds a listening socket | **No inbound attack surface exists today** |
| The module is linked for two type aliases — an HTTP status enum and an HTTP method enum | The cost-to-benefit ratio is poor |

Phase 1 recorded this as R-016 at `MEDIUM` on the grounds that an unauthenticated endpoint would be
a security concern. The inspection traced it and showed no endpoint exists, so Phase 2.1 amended
R-016 to `LOW` at `G-OPEN` with dependency hygiene as the residual item. **Phase 3 reopens nothing**:
the amended position stands, Q-027 remains the hygiene question, and the Aerora position is still
that the aliases should be replaced locally and the dependency dropped as an upstream contribution.

Until then the startup self-check asserts no listening socket is open, which is why `IF-CLD-003`
carries both `AERORA-TEST-REQ-004` and `AERORA-TEST-REQ-007`. The first covers the build; the
second covers the deployment, where no test is running.

## Interface contracts — what actually crosses

| ID | Interface | Direction | Producer | Consumer | Crossing | Update semantics | Ack | Authority | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-CLD-005` | Mission library synchronisation | Bidirectional | Mission module and backend mission management | Each other | Plans, with their identity and revision | Opportunistic | Backend accepts; the local plan is unaffected by the outcome | Local plan is authoritative until reconciled | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-CLD-006` | Fleet view data | Backend to GCS | Organisation and fleet management | Cloud module, operator display | Fleet membership and last-known vehicle attributes, **read-mostly** | Opportunistic | — | Backend | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-CLD-007` | Configuration distribution | Backend to GCS | Device configuration | Configuration module | A versioned configuration set | Opportunistic; applied when received | Applied version reported back | Backend proposes; the station applies and records | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-CLD-008` | Release and compatibility metadata | Backend to GCS | Software and firmware release metadata | Diagnostics module | Available versions and compatibility statements, **advisory** | Opportunistic | — | Backend informs; **never gates** | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-CLD-009` | Operator notification | Backend to GCS | Notification and event handling | Operator display | Informational messages | Opportunistic | — | Backend informs | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-CLD-010` | Reporting and dashboards | — | — | — | **Nothing. Reporting is a backend and web concern with no ground-station counterpart** | — | — | Backend | `BASELINE-ASSERTED` | `CONFIRMED` |
| `IF-CLD-011` | Map and terrain tile cache | Backend or tile source to GCS | Tile source | Map display | Tiles, pre-cached before flight | Opportunistic, before flight | — | Ground station | `GCS-VERIFIED` | `PROPOSED` |
| `IF-CLD-012` | Cloud-sourced data age | GCS internal | Cloud module | Every consumer of cloud-sourced data | The retrieval time of each cloud-sourced value | With the value | — | Ground station | `BASELINE-ASSERTED` | `CONFIRMED` |
| `IF-CLD-013` | Per-feature availability state | GCS internal | Cloud module | Each cloud-backed feature | Per-feature availability. **There is no global offline mode** | On change | — | Ground station | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-CLD-014` | Upload queue state | GCS internal | Cloud module upload queue | Operator display | Queue depth and drop count | On change | — | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-CLD-015` | Synchronisation conflict resolution | Bidirectional | Mission module and backend | Each other | A conflict between a local and a remote revision | Per conflict | **Policy undecided** | `UNKNOWN` | `NOT-EVIDENCED` | `OPEN` |
| `IF-CLD-016` | Backend module interface boundary | GCS to backend | Cloud module | Backend modules | Typed application interfaces, **never shared database access** | Per call | Per interface | Backend | `BASELINE-ASSERTED` | `CONFIRMED` |

| ID | Freshness semantics | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `IF-CLD-005` | Each plan carries its revision and last-synchronised time | Unreachable backend leaves **local plans only, marked unsynchronised**. Planning, upload and flight are unaffected | `AERORA-AWS-REQ-006`, `AERORA-AWS-REQ-007` | — | Q-042 | `INTEGRATION` |
| `IF-CLD-006` | Shown as last-known **with its age**, never as current | Unreachable backend shows the last-known fleet with its age. A stale fleet view is not a connected vehicle | `AERORA-AWS-REQ-009` | R-017 | Q-016 | `INTEGRATION` |
| `IF-CLD-007` | Applied version and received version are distinct values | Unreachable backend retains the **last-applied** configuration. A station never waits for configuration to operate | `AERORA-AWS-REQ-007`, `AERORA-BUILD-REQ-010` | — | Q-042 | `INTEGRATION` |
| `IF-CLD-008` | Advisory data shown with its retrieval time | Unreachable backend shows no update notification. **An unavailable version check never blocks a flight** | `AERORA-AWS-REQ-007`, `AERORA-BUILD-REQ-010` | — | — | `INTEGRATION` |
| `IF-CLD-009` | Shown with its retrieval time | A notification that would direct a flight action is **prohibited**, not merely discouraged — it would put the cloud on the command path through the operator | `AERORA-AWS-REQ-013` | — | — | `STATIC` |
| `IF-CLD-010` | — | No ground-station degradation, because there is no ground-station feature. Reporting unavailability is stated plainly where an operator might look for it | `AERORA-AWS-REQ-007` | — | — | `STATIC` |
| `IF-CLD-011` | Tiles carry their cache age | A missing tile degrades the map and is **shown as missing**, not drawn blank and left ambiguous. The flight is unaffected | `AERORA-AWS-REQ-006` | — | — | `DEMO` |
| `IF-CLD-012` | **This interface is the freshness contract** for every cloud-sourced value | A value whose age is unknown is presented as unknown age, which is itself information | `AERORA-AWS-REQ-009` | — | — | `UNIT` |
| `IF-CLD-013` | Per feature, evaluated on use rather than cached globally | A feature with no defined local behaviour is a defect. **The absence of a global offline mode is the contract** | `AERORA-AWS-REQ-007` | — | Q-030 | `STATIC` |
| `IF-CLD-014` | Depth is current; drop count is monotonic | **Bounded queue.** An outage drops with an accounted counter rather than growing without limit. This is the **upload** queue, not the record-writer queue of `IF-LOG-014` | `AERORA-AWS-REQ-010` | — | Q-039 | `UNIT` |
| `IF-CLD-015` | Both revisions carry times; neither is silently preferred | **Unresolved.** No policy exists, so Phase 3 specifies only that a conflict is surfaced and that **no side is silently overwritten** | `AERORA-AWS-REQ-007`, `AERORA-BUILD-REQ-012` | — | Q-042 | `INTEGRATION` |
| `IF-CLD-016` | Per interface | A backend refactor that extracts a module into its own service is invisible to the ground station, which is the point of typed interfaces | `AERORA-AWS-REQ-007` | — | — | `STATIC` |

## Per-feature degradation

There is no global offline mode. A global mode invites the question "are we offline?" at points
where the real question is "is this specific thing available?". Each feature degrades on its own,
per `AERORA-AWS-REQ-007`.

| Feature | Cloud available | Cloud unavailable | Interface |
| --- | --- | --- | --- |
| Vehicle connection and control | Unaffected | **Unaffected** | `IF-CLD-004` |
| Telemetry display | Unaffected | **Unaffected** | `IF-CLD-004` |
| Mission planning and upload | Unaffected | **Unaffected** | `IF-CLD-004` |
| Local flight and audit recording | Unaffected | **Unaffected** | `IF-LOG-016` |
| Operator sign-in | Reconciled against the service | Validated against cached credentials | `IF-SEC-002`, `IF-SEC-009` |
| Mission library | Synchronised | Local plans only, marked unsynchronised | `IF-CLD-005` |
| Fleet view | Current | Last-known, with its age shown | `IF-CLD-006` |
| Vehicle registry reconciliation | Live | Deferred, queued | `IF-VID-007` |
| Record upload | Continuous | Queued locally, bounded, depth visible | `IF-LOG-017`, `IF-CLD-014` |
| Configuration distribution | Applied when received | Last-applied configuration retained | `IF-CLD-007` |
| Reporting and dashboards | Available on the backend | Unavailable, stated plainly | `IF-CLD-010` |
| Update notification | Shown | Not shown | `IF-CLD-008` |
| Map and terrain tiles | Fetched and cached | Cached tiles only; gaps shown as gaps | `IF-CLD-011` |

The first four rows are the ones that matter, and they all say the same word. Everything an operator
needs to fly an aircraft is in the **unaffected** column, and that is not a coincidence — it is
`AERORA-AWS-REQ-005` and `AERORA-AWS-REQ-006` expressed as a table.

Two further rows carry a deliberate obligation. "Last-known, with its age shown" and "depth visible"
exist because the dangerous failure is not the outage. It is an operator who cannot tell current
data from old data.

## Two indicators, no shared visual language

| Indicator | Operator question | Interface |
| --- | --- | --- |
| Vehicle link | **Can I command the aircraft?** | `IF-RAD` set, and `IF-UIS` presentation |
| Cloud connection | **Is my data synchronised?** | `IF-CLD-001` |

The baseline requires these be shown separately. `AERORA-AWS-REQ-008` adds that they share no
visual language, because conflating them means a cloud outage looks like a link problem — which
would produce exactly the wrong operator response at exactly the wrong time.

This is one of the few requirements in the set verified by `DEMO`. Whether two indicators are
genuinely distinguishable is not a property a unit test can assert.

## The backend counterpart map

The baseline specifies a modular monolith with eleven internal modules communicating through typed
application interfaces. Phase 3 adopts it unchanged and records which ground-station interface faces
each module.

| Backend module | GCS-side counterpart | Interface |
| --- | --- | --- |
| Identity and access | Security module — offline-capable validation | `IF-SEC-009` |
| Organisation and fleet management | Cloud module — fleet view | `IF-CLD-006` |
| Vehicle registry | Identity binding — reconciled, **not resolved** | `IF-VID-007` |
| Mission management | Mission module — plan library | `IF-CLD-005` |
| Payload management | Payload records, **not payload control** | `IF-PAY` set, record side only |
| Telemetry and event ingestion | Upload of locally written records | `IF-LOG-017` |
| Device configuration | Configuration module | `IF-CLD-007` |
| Software and firmware release metadata | Diagnostics — advisory only | `IF-CLD-008` |
| Audit logging | Audit trail, written locally first | `IF-LOG-016`, `IF-LOG-017` |
| Reporting | **None** | `IF-CLD-010` |
| Notification and event handling | Operator notifications, never flight-directive | `IF-CLD-009` |

Two rows carry their qualifier as the contract. "Reconciled, not resolved" means the station never
waits on the registry to know which aircraft it is talking to. "Payload records, not payload
control" means the backend never commands a payload; that boundary is in
[06-payload-icd.md](06-payload-icd.md).

## Threading

One Aerora background thread serves the cloud. It owns network I/O, the upload queue and the
synchronisation state machine, and it reaches the main thread by queued signals only.

| Constraint | Reason | Interface |
| --- | --- | --- |
| No synchronous network call from the main thread, ever | A stalled name resolution would freeze the operator interface | `IF-CLD-002` |
| The upload queue is bounded, with a drop counter | Unbounded queueing turns a long outage into a memory failure | `IF-CLD-014` |
| Queue depth and drop count are visible | An operator with a full queue needs to know **before** the flight record matters | `IF-CLD-014` |

## Conflicts recorded

| Conflict | Position A | Position B | Resolution authority |
| --- | --- | --- | --- |
| Synchronisation conflict policy | `AERORA-AWS-REQ-007` requires each feature to have a defined local behaviour | No conflict policy exists for a plan edited locally and remotely | **Unresolved.** Q-042; product and operations owners. `IF-CLD-015` specifies only that neither side is silently overwritten |
| HTTP server dependency | Phase 1 R-016 assessed it as a security concern | Phase 2.1 traced it and found no listener, amending to hygiene | **Resolved in Phase 2.1.** Both positions retained; Phase 3 carries the amended one and does not reopen it |

The first is the only genuinely open cloud contract. Phase 3 deliberately specifies the weakest
useful thing — surface the conflict, overwrite nothing — because any stronger rule would be a
product decision dressed as an interface.

## Not specified, and why

| Item | Reason |
| --- | --- |
| Backend endpoint addresses, protocols and payload schemas | Backend deliverables; no ground-station decision is needed to specify the boundary |
| Authentication scheme between station and backend | Not evidenced; a backend and security decision |
| Synchronisation conflict policy | Q-042 |
| Which cloud dependencies are permitted per feature beyond the table above | Q-030 |
| Upload cadence, batch sizes and retry schedules | Implementation tuning; the bounded-and-visible properties are the contract |
| Retention of uploaded records on the backend | Q-039, and a backend concern |
| Map tile source and licensing | Q-005 for licence position; tile source is a product decision |
| Whether the HTTP server dependency is removed | Q-027; an upstream contribution, not an Aerora core edit |

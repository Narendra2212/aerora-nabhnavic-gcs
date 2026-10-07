# MAVLink Interface Control Document

The Aerora MAVLink contract: what is standard, what Aerora adds, what is proposed, and what is
unknown.

## The four-way split

Every message and command concept in the Aerora system falls into one of four classes. Keeping them
separate is the whole point of this document, because the failure mode it guards against is a
proposed message being implemented as though it were ratified.

| Class | Meaning | Count | Evidence |
| --- | --- | --- | --- |
| **A. Standard MAVLink** | Already defined upstream, already parsed by QGroundControl, used unchanged | Many | `GCS-VERIFIED` |
| **B. Aerora extensions** | Defined in the Aerora dialect | **Zero** | — |
| **C. Proposed extensions** | Named as candidates by the baseline; no fields, no identifiers | 9 families | `BASELINE-ASSERTED` |
| **D. Unknown or to be determined** | Needed by the architecture, not yet named by anything | Several | `NOT-EVIDENCED` |

**Class B is empty.** No Aerora message exists. Everything the architecture needs from an Aerora
dialect is in class C or D. That is the single most important fact in this document and it is why
Phase 3 specifies no message identifiers.

## Class A — standard MAVLink used unchanged

Phase 1 and Phase 2 read QGroundControl's handling of these. They are adopted without modification
and without an Aerora wrapper.

| ID | Concept | Standard family | Aerora use | Evidence | Status |
| --- | --- | --- | --- | --- | --- |
| `IF-MAV-001` | Heartbeat, base and custom mode | Heartbeat | Vehicle presence, arming, flight mode, autopilot and vehicle type | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MAV-002` | Attitude, position, velocity, altitude | Attitude, global position, VFR families | Vehicle state display and logging | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MAV-003` | System and sensor health | System status | Health display and diagnostics | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MAV-004` | Battery state | Battery status | Power display, alongside Aerora tether reporting | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MAV-005` | GPS and estimator state | GPS and estimator status families | Standard handling | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MAV-006` | Command send and acknowledgement | Command long, command int, command acknowledgement | All operator commands | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MAV-007` | Mission transfer and progress | Mission protocol families | Mission upload, download, current index | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MAV-008` | Parameter protocol | Parameter families, with file transfer where supported | All vehicle parameters, as Facts | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MAV-009` | Status text and events | Status text, event families | Diagnostics, operator messages | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MAV-010` | Radio status | Radio status | Link quality where the radio supplies it; also latches radio-link treatment | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MAV-011` | Message signing | MAVLink 2 signing | Retained unchanged | `GCS-VERIFIED` | `CONFIRMED` |

Evidence is `GCS-VERIFIED` for all eleven because the ground-station handling was read in Phase 1.
It says nothing about whether the Aerora firmware emits them — that is `IF-FC` territory, and
several rows there are `NOT-EVIDENCED` for exactly that reason.

## Class C — proposed Aerora families

Baseline document 07 names these nine candidates. Phase 3 records the names **as the baseline's
candidates** and specifies nothing further.

| ID | Candidate family | Intended subject | Evidence | Status |
| --- | --- | --- | --- | --- |
| `IF-MAV-012` | Aerora system status | Aerora system state, dialect version, authority mode, link arbitration, vehicle identity | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-MAV-013` | Aerora vehicle capabilities | The capability inventory | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-MAV-014` | Aerora payload status | Payload state, health, safety state | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-MAV-015` | Aerora paint status | Product-specific application telemetry | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-MAV-016` | Aerora pump status | Product-specific | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-MAV-017` | Aerora gimbal status | Product-specific | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-MAV-018` | Aerora autonomy status | Companion proposals, perception state, acceptance outcome | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-MAV-019` | Aerora mission status | Aerora mission state beyond the standard index | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-MAV-020` | Aerora diagnostics | Aerora diagnostic reporting | `BASELINE-ASSERTED` | `PROPOSED` |

`BASELINE-ASSERTED` is the strongest claim available: baseline document 07 names the families and no
artifact, model element or interface control document defines any of them.

### What is not specified for any class C family

| Not specified | Why |
| --- | --- |
| Message identifier | Allocation is a dialect decision; inventing one would collide or mislead |
| Field layout, names, types | No interface control document exists |
| Units and ranges | Follow from fields |
| Enumerations | Q-007 for failsafe, Q-023 for payload, and the authority set is not closed |
| Rates | Not evidenced; needed against the channel and Fact budgets |

### Allocation discipline, when the dialect is written

Recorded here because it is the moment when a mistake becomes permanent.

| Rule | Reason |
| --- | --- |
| Allocate from a range reserved for the Aerora dialect | Avoids collision with upstream additions |
| Never reuse an identifier, even for a withdrawn message | A field recording is forever |
| Version the dialect, and report the version in telemetry | Makes the compatibility matrix checkable |
| Prefer extending an existing Aerora message over adding one | MAVLink extension fields are backward compatible; new messages are not free |
| Use standard messages where semantically sufficient | The baseline's instruction, and it reduces what Aerora must maintain |

## Class D — needed, not yet named

Required by the architecture and named by nothing. Each is a gap, not a design.

| ID | Needed for | Required by | Evidence | Status |
| --- | --- | --- | --- | --- |
| `IF-MAV-021` | Reporting the Aerora autopilot identifier in the heartbeat | Deterministic firmware-plugin selection | `NOT-EVIDENCED` | `OPEN` — Q-018 |
| `IF-MAV-022` | Reporting which component originated a relayed value | The reporting-source half of the freshness triple | `NOT-EVIDENCED` | `OPEN` — Q3-004 |
| `IF-MAV-023` | Reporting proposal acceptance or rejection explicitly | Prohibition on inferring acceptance | `NOT-EVIDENCED` | `OPEN` — Q-008 |
| `IF-MAV-024` | A per-command request identifier for audit correlation | Distinguishing two identical commands in the record | `NOT-EVIDENCED` | `OPEN` — Q3-002 |
| `IF-MAV-025` | Declaring the resume behaviour of a state-setting command | `AERORA-MAV-REQ-008` | `NOT-EVIDENCED` | `OPEN` — evidence matrix item 14 |

Five gaps, five `NOT-EVIDENCED`. Nothing in this table is a design; each row is a statement that the
architecture needs something the dialect does not yet contain.

## Dialect ownership and versioning

| Property | Position | Evidence | Status |
| --- | --- | --- | --- |
| Ownership | An Aerora-controlled fork of the MAVLink definitions repository | `GCS-DECIDED`, ADR-004 | `PROPOSED` |
| Selection | Through the existing build variables for repository, tag and dialect | `GCS-VERIFIED` | `CONFIRMED` |
| Pinning | **An immutable commit, never a branch** | `GCS-DECIDED` | `PROPOSED` |
| Generation | At configure time, by the upstream generator inside the fetched definitions repository | `GCS-VERIFIED` | `CONFIRMED` |
| Checked-in headers | None; QGroundControl checks in no generated message headers | `GCS-VERIFIED` | `CONFIRMED` |
| Dialect version reporting | A field in the Aerora system status family | `PROPOSED` | `PROPOSED` |
| Generator version | Pinned with the definitions repository commit | `GCS-VERIFIED` | `PROPOSED` |

### Why pinning is an interface requirement

Not a build preference. A dialect tracked by branch means two builds of the same Aerora commit can
embed different message definitions, which makes the compatibility matrix the baseline requires
meaningless. The matrix is: ground-station version, by flight-controller firmware, by payload
firmware, by dialect version — and a floating dialect removes the fourth axis.

Two upstream dependencies currently track moving references. That is R-011, and it is the same
defect in a different place.

## Signing

| Property | Position | Evidence | Status |
| --- | --- | --- | --- |
| Mechanism | MAVLink 2 signing, QGroundControl implementation retained unchanged | `GCS-VERIFIED` | `CONFIRMED` |
| Per-link state | A per-link state machine with deferred-confirm enable and disable | `GCS-VERIFIED` | `CONFIRMED` |
| Key auto-detection | Stored keys are tried against an inbound signature, verifying the HMAC before configuring the channel | `GCS-VERIFIED` | `CONFIRMED` |
| Timestamp persistence | Last-used timestamp persisted per key for forward progress across restarts | `GCS-VERIFIED` | `CONFIRMED` |
| Unsigned acceptance | An explicit policy callback, not an implicit allow | `GCS-VERIFIED` | `CONFIRMED` |
| Default policy | **`OPEN` as Q-010** | — | `OPEN` |
| Key provisioning | Randomly generated raw keys, not passphrase-derived | `GCS-DECIDED` | `PROPOSED` |
| Key storage | **Unresolved; release-critical** | `GCS-VERIFIED` defect | `OPEN` |

The signing *protocol* contract is settled and good. The *custody* contract is not, and it is a
release blocker. Detail in [13-security-icd.md](13-security-icd.md).

## Channel allocation

| Property | Value | Evidence | Status |
| --- | --- | --- | --- |
| Total MAVLink channels available | 15 | `GCS-VERIFIED` | `CONFIRMED` |
| Channels consumed | One per active link | `GCS-VERIFIED` | `CONFIRMED` |
| Worst-case concurrent link count | **`UNKNOWN`** | — | `OPEN` — Q-015 |
| Budget risk | R-013 | — | — |

The budget is a hard limit in the upstream implementation. With three radios per aircraft, the
ceiling is reached at five aircraft on one ground station if every link is held open — which is why
the worst-case count is a question rather than an assumption.

## System and component identifiers

| Property | Position | Evidence | Status |
| --- | --- | --- | --- |
| One system id per aircraft | Required | `GCS-VERIFIED` | `CONFIRMED` |
| Flight controller component | Component 1 | `GCS-VERIFIED` as a QGroundControl assumption | `CONFIRMED` as a firmware obligation |
| Companion component | A distinct non-autopilot component id, sharing the aircraft system id | `GCS-VERIFIED` mechanism; firmware decision `NOT-EVIDENCED` | `PROPOSED` |
| Payload component | `UNKNOWN` — may be a component, or relayed by the flight controller | — | `OPEN` |
| Ground-station system id | Operator-editable setting | `GCS-VERIFIED` | `CONFIRMED` |
| System id as identity | **Prohibited.** It is addressing, not identity | `GCS-DECIDED` | `CONFIRMED` |

The last row is the contract. The system id is an 8-bit, operator-configurable, reassignable
addressing value, and nothing that outlives a connection may be keyed on it.

## Capability discovery

| ID | Contract | Evidence | Status |
| --- | --- | --- | --- |
| `IF-MAV-026` | The vehicle reports a capability inventory on connection and on change; the ground station presents only the capabilities reported | `NOT-EVIDENCED` | `OPEN` |

This is the mechanism by which one ground-station build serves multiple Aerora products, and the
mechanism by which a missing message family degrades gracefully instead of producing a broken
display. It is also entirely unevidenced: no inventory message exists.

Its absence has a defined fallback: with no inventory, no Aerora capability is presented and the
interface degrades to the standard QGroundControl set. That fallback is what makes the first
increment buildable before the dialect lands.

## Message handling seams

Two ground-station-internal seams sit between the parser and the consumers. Both are
`GCS-VERIFIED`, both are the supported extension points Aerora uses, and **both can drop a message
without a trace**.

| ID | Interface | Direction | Producer | Consumer | Crossing | Ack | Authority | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-MAV-027` | Inbound message adjustment seam | GCS internal | Link parser | Aerora firmware abstraction, then the vehicle model | Every inbound message, with a continue-or-veto outcome | — | Ground station | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-MAV-028` | Core-plugin message seam | GCS internal | Link parser | Aerora plugin, then standard handling | Every inbound message, with a continue-or-veto outcome | — | Ground station | `GCS-VERIFIED` | `CONFIRMED` |

| ID | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- |
| `IF-MAV-027` | **A vetoed message is logged.** Aerora vetoes nothing by default; a silent drop here would be undiagnosable from the operator's seat | `AERORA-MAV-REQ-010` | R-002 | — | `UNIT` |
| `IF-MAV-028` | **A vetoed message is logged.** Same contract, different seam | `AERORA-MAV-REQ-010` | R-002 | — | `UNIT` |

These two are specified because they are the generalised form of R-002. R-002 was a message set
being silently excluded from a replay; these seams are the places where any message can be silently
excluded from everything. The mitigation is the same in both cases and it is not a code change — it
is refusing to let a drop be silent.

Aerora's intended use of both seams is **observation, not interception**: the inbound seam lets the
Aerora firmware abstraction route Aerora families, and the core-plugin seam is available and unused.
Specifying the veto contract now means the obligation exists before the first temptation to use it.

## Telemetry freshness at the protocol level

| Rule | Status |
| --- | --- |
| Receive time is stamped at parse, not at display | `CONFIRMED` |
| Freshness is carried with the value through memory, display and record | `CONFIRMED` |
| Staleness thresholds are per message class | `PROPOSED` |
| Threshold values | **Not specified** — depend on unevidenced rates |
| A stale value is never presented as current | `CONFIRMED` |

## Compatibility

### Backward and forward compatibility rules

| Rule | Rationale |
| --- | --- |
| Add fields using MAVLink extension fields, never by changing existing field meaning | Extension fields are ignored by older parsers; redefinition is silent corruption |
| A ground station receiving an unknown message ignores it without error | Standard MAVLink behaviour; required so a newer vehicle does not break an older station |
| A ground station receiving a known message with unknown enumeration values presents the raw value, marked unrecognised | Prevents a new vehicle state displaying as a wrong known state |
| A vehicle reporting a dialect version the station does not recognise is flagged, not refused | The station is supervisory; refusing to display is worse than displaying with a warning |
| Dialect version mismatch is surfaced in diagnostics, not hidden | The compatibility matrix is only useful if mismatch is visible |

The third rule deserves emphasis. An unrecognised enumeration value rendered as the nearest known
value is the worst available behaviour, and it is the default outcome if nobody specifies otherwise.

### Compatibility matrix

| Axis | Source | Status |
| --- | --- | --- |
| Ground-station version | Build, from the version-control description | `CONFIRMED` |
| Dialect version | Aerora system status field, plus the pinned commit in the build | `PROPOSED` |
| Flight-controller firmware version | Vehicle-reported | `PROPOSED` |
| Payload firmware version | Payload status family | `PROPOSED` |
| Capability inventory | Vehicle capabilities family | `OPEN` |

All five are recorded in the log header and reported to the fleet registry, and the diagnostics
module surfaces a mismatch.

## Conflicts recorded

None found between sources for this document. The MAVLink contract's problem is not conflict; it is
absence.

## Traceability

| ID | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- |
| `IF-MAV-001` to `IF-MAV-011` | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-029` | — | — | `INTEGRATION` |
| `IF-MAV-012` to `IF-MAV-020` | `AERORA-MAV-REQ-005`, `AERORA-MAV-REQ-006`, `AERORA-MAV-REQ-007` | R-011 | Q-003 | `BENCH` |
| `IF-MAV-021` | `AERORA-GCS-REQ-007`, `AERORA-GCS-REQ-008` | R-010 | Q-018 | `BENCH` |
| `IF-MAV-022` | `AERORA-GCS-REQ-027`, `AERORA-COMPLY-REQ-004` | — | — | `BENCH` |
| `IF-MAV-023` | `AERORA-GPU-REQ-005` | R-014 | Q-008 | `HIL` |
| `IF-MAV-024` | `AERORA-GCS-REQ-018`, `AERORA-COMPLY-REQ-002` | R-025 | — | `BENCH` |
| `IF-MAV-025` | `AERORA-MAV-REQ-008` | R-002 | Q-023 | `BENCH` |
| `IF-MAV-026` | `AERORA-MAV-REQ-009`, `AERORA-GCS-REQ-013`, `AERORA-PAYLOAD-REQ-013` | — | Q-023 | `BENCH` |
| `IF-MAV-027`, `IF-MAV-028` | `AERORA-MAV-REQ-010` | R-002 | — | `UNIT` |

Class A is `INTEGRATION` because the mock link exercises it end to end. Everything from class C
onward is `BENCH` or `HIL`: the contract cannot be verified against a mock that Aerora itself
authored, because that would verify the mock.

Full mapping including verification methods in
[19-interface-traceability.md](19-interface-traceability.md).

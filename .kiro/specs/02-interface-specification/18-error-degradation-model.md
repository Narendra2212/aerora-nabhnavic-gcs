# Error and Degradation Model

Twelve named aircraft-reported conditions and eight ground-station conditions: how each is detected,
how it is indicated, which capability it affects, what the ground station does, how it recovers, and
whether authority changes.

**The last column is almost always "no".** The ground station is supervisory, so its response to
nearly every degradation is representational. This document is mostly a specification of what *not*
to do, which is why it is written as a table rather than a flowchart.

## The rule that shapes every row

| Position | Source |
| --- | --- |
| The ground station does not decide or autonomously initiate a failsafe action | `AERORA-SAFE-REQ-008` |
| The ground station does not infer a failsafe condition the vehicle has not reported | `AERORA-SAFE-REQ-009` |
| The ground station does not clear a reported failsafe state locally | `AERORA-SAFE-REQ-009` |
| The ground station does not retry a stale command | `AERORA-GCS-REQ-017` |
| The ground station does not derive state the vehicle has not reported | `AERORA-GCS-REQ-029` |

So "GCS response" in the tables below means: what is shown, what is recorded, and what is
suppressed. It does not mean what the aircraft does. **No failsafe action, no automatic mode change
and no autonomous recovery is specified anywhere in this document**, because the mapping of
conditions to actions is Q-007 and belongs to the aircraft.

## Aircraft-reported conditions

### Detection and effect

| ID | Condition | Detection | Indication | Affected capability |
| --- | --- | --- | --- | --- |
| `DEG-01` | Total link loss | Every link's inbound traffic past its freshness threshold | A distinct **all-stale** state, not a per-link stale state repeated | All command capability; telemetry becomes last-known with age |
| `DEG-02` | Partial link degradation | One link stale while another is current, or the vehicle-reported active link changes | Per-link state **and** the vehicle-reported active link, shown separately | None directly; command capability continues on the surviving link |
| `DEG-03` | Vehicle heartbeat loss | Heartbeat absent past the connection threshold | Vehicle shown as disconnected; last-known values retained with their age | All vehicle, payload, mission and command capability |
| `DEG-04` | Telemetry value staleness | A value past its per-class freshness threshold | The value with a stale marking **and its age** | The specific capability that value supports |
| `DEG-05` | Command acknowledgement timeout | No acknowledgement within the timeout, retries exhausted | The command in `FAILED`, with the failure reason | That command only |
| `DEG-06` | Command accepted, not effective | `ACCEPTED` reached, corroboration window elapsed without effect | The command in `STALE`, **surfaced** | That command only; the capability is reported present but not responding |
| `DEG-07` | Vehicle-reported failsafe | A failsafe report from the vehicle | State, reason, source, severity and reported action, **five items shown separately**, with freshness | As the vehicle reports; the station asserts nothing |
| `DEG-08` | Watchdog event reported | A reported watchdog state change | The watchdog state and the arming state, each as **independently reported** items with their own receive times | Arming; downstream capability as the vehicle reports |
| `DEG-09` | Payload emergency stop active | A stop-active report, with its initiating source | The stop state, distinct from payload state and from `DEG-08` | Payload command capability as the payload controller reports |
| `DEG-10` | Payload state or capability unreported | No payload status or no capability set for the connection | **No payload controls presented at all**; the absence is explicit | All payload capability |
| `DEG-11` | Companion absent or silent | No companion health report, versus a report carrying no proposal | **Two different displays.** Absent and silent are never the same pixel | Autonomy proposal display only; never command capability |
| `DEG-12` | Power or tether condition reported | A power or tether state report | The reported configuration and condition, with no transition inferred | As the vehicle reports; the station initiates no power action |

### Response and authority

| ID | GCS response | Recovery | Authority change | Evidence | Requirements |
| --- | --- | --- | --- | --- | --- |
| `DEG-01` | Show all-stale; retain last-known with age; suppress controls whose effect cannot be observed; record the transition | On traffic resuming, values resume individually with fresh stamps. **No replay, no catch-up, no backfill** | **None.** The station has no authority to gain or lose | `GCS-VERIFIED` detection; `MODEL` for vehicle-reported arbitration | `AERORA-COMM-REQ-008`, `AERORA-COMM-REQ-009` |
| `DEG-02` | Show both the per-link view and the vehicle-reported active link; **never promote the local observation over the vehicle's** | Per link, independently | **None** | `MODEL` | `AERORA-COMM-REQ-007`, `AERORA-COMM-REQ-008` |
| `DEG-03` | Show disconnected; retain the last-known set with ages; **do not discard**; record the disconnection | On reconnection, identity is re-established and the capability set is re-acquired. Nothing is carried across | **None** | `GCS-VERIFIED` | `AERORA-VEH-REQ-003`, `AERORA-GCS-REQ-027` |
| `DEG-04` | Mark stale and show the age. Do not hide it, which loses the last reading; do not show it plain, which is a lie | On the next report, the stamp replaces the old one wholly | **None** | `GCS-DECIDED` | `AERORA-GCS-REQ-027` |
| `DEG-05` | Surface the failure with its reason; record every transition; **do not retry beyond the protocol's retry limit** | The operator reissues, which is a new command with a new request identity | **None** | `GCS-VERIFIED` | `AERORA-GCS-REQ-015`, `AERORA-GCS-REQ-018` |
| `DEG-06` | Surface it. **This is the single most valuable condition the command model detects** and it is worthless if not shown | The operator decides. The station **never** reissues automatically | **None** | `GCS-DECIDED` | `AERORA-GCS-REQ-016`, `AERORA-GCS-REQ-017` |
| `DEG-07` | Display and record the five items separately. **A failsafe reported once and then absent goes stale, not cleared** | Only the vehicle clears a failsafe. The station reports what it is told | **None.** `IF-AUT-012` carries nothing | `PLACEHOLDER` for the content | `AERORA-SAFE-REQ-008`, `AERORA-SAFE-REQ-009`, `AERORA-SAFE-REQ-010` |
| `DEG-08` | Show and record each reported item with its time. **Assert no causal link** — not to the disarm, not to the payload stop, not to a power change, not to a failsafe | Vehicle-side | **None.** The watchdog path is observed, never mediated | `MODEL` via baseline 13; the **reported** linkage is `NOT-EVIDENCED` | `AERORA-SAFE-REQ-011`, `AERORA-GCS-REQ-029` |
| `DEG-09` | Show the stop and its initiating source. Do not present it as a payload fault or as a consequence of `DEG-08` | Payload-controller-side. An operator stop request is effective only when reported | **None.** The request is not the stop | `MODEL` | `AERORA-PAYLOAD-REQ-008`, `AERORA-PAYLOAD-REQ-009`, `AERORA-SAFE-REQ-011` |
| `DEG-10` | Present no payload controls, and make the absence explicit rather than blank | On a capability set arriving for the connection | **None** | `NOT-EVIDENCED` | `AERORA-PAYLOAD-REQ-013` |
| `DEG-11` | Two distinct displays. **Acceptance of a proposal is never inferred from the proposal disappearing** | On a health report or a proposal arriving | **None.** Companion output is non-authoritative in both states | `MODEL`, partly future work | `AERORA-GPU-REQ-006`, `AERORA-GPU-REQ-007` |
| `DEG-12` | Show the reported configuration and condition. **Infer no transition, initiate no power action, estimate no remaining energy the vehicle has not reported** | Vehicle-side | **None** | `ARTIFACT-OBSERVED` hardware; reporting `NOT-EVIDENCED` | `AERORA-POWER-REQ-007`, `AERORA-POWER-REQ-008` |

### The watchdog relationships, separated

Added in Phase 3.1. `DEG-08` is the condition most likely to acquire an invented causal story, so
each candidate relationship is recorded with its own evidence rather than inherited from the one
next to it.

| Relationship | Aircraft side | Reported to the ground station | Ground station may display it |
| --- | --- | --- | --- |
| Watchdog exists as a distinct device | `ARTIFACT-OBSERVED` via baseline 46 | — | Not applicable |
| Watchdog state is observable to the operator | — | `BASELINE-ASSERTED` — baseline 11 lists watchdog state in the display set. **No carrying message is defined** | Yes, as a reported value |
| Watchdog trigger semantics: timeout, reset, latch | **`NOT-EVIDENCED`** — baseline 46 lists it as unresolved | `NOT-EVIDENCED` | **No** |
| Watchdog → disarm | `MODEL` via baseline 13, `PROPOSED` | **`NOT-EVIDENCED`** | **No** — show both states, assert no link |
| Watchdog → payload emergency stop | `MODEL` via baseline 13, `PROPOSED` | **`NOT-EVIDENCED`**, except the initiating-source field of `IF-PAY-006`, itself `NOT-EVIDENCED` | Only the reported initiating source, if it ever exists |
| Watchdog → power transition | **`NOT-EVIDENCED`** — no source states any relationship | `NOT-EVIDENCED` | **No** |
| Watchdog → failsafe state | **`NOT-EVIDENCED`** — baseline 13 defers failsafe behaviour entirely | `NOT-EVIDENCED` | **No** — `DEG-07` and `DEG-08` stay separate conditions |
| Watchdog reports independently of the primary MCU | `NOT-EVIDENCED` | `NOT-EVIDENCED` | Not applicable — Q3-004, R3-003 |

Two of the eight are the ones a reader will expect to find and will not: **watchdog to power** and
**watchdog to failsafe**. Neither appears in any baseline document, any model description, any
schematic record or any requirement. Their absence is recorded here as an explicit `NOT-EVIDENCED`
rather than left to be inferred from silence, because silence is what invited the inference in the
first place.

Phase 3.1 adds no watchdog enumeration, no timeout value, no failsafe action and no precedence rule.

### Reading the authority column

Twelve conditions, twelve times **none**. That is not a drafting shortcut; it is the system
architecture restated. The ground station has no authority to lose when a link degrades and none to
gain when a failsafe fires. Every row that might have read "the GCS takes over" is a row where the
architecture would have been wrong.

The two rows worth pausing on:

| Row | Why it looks like an exception and is not |
| --- | --- |
| `DEG-01` | A total link loss is the moment a ground station is most tempted to act. It cannot: with no link there is no path, and with a path there is no loss |
| `DEG-07` | A reported failsafe is the moment it is most tempted to help. `IF-AUT-012` is the empty interface that prevents it |

## Ground-station conditions

These are the station's own degradations. The station owns them, so here it does act.

### Detection and effect

| ID | Condition | Detection | Indication | Affected capability |
| --- | --- | --- | --- | --- |
| `DEG-13` | Parameter set incomplete | The parameter manager's own synchronisation state | Incomplete synchronisation is **visible**; dependent setup pages are not presented | Setup and configuration capability |
| `DEG-14` | Mission transfer failure | A protocol transaction that does not complete | The transfer as failed, with the vehicle's reason where given | Mission upload or download; **the on-vehicle plan state is reported, never assumed** |
| `DEG-15` | Record write failure | An I/O error from the record writer | Surfaced, not absorbed into a log line | Recording capability for the affected record |
| `DEG-16` | Record queue overflow | Bounded queue at capacity | A drop count, written into the record and shown | Record completeness, with the gap accounted for |
| `DEG-17` | Local storage exhaustion | Free-space threshold crossed | Warned **before** the record matters, not at the moment of loss | All local recording |
| `DEG-18` | Cloud unreachable | The Cloud module's own observation | A cloud indicator with **no visual language shared with the vehicle link** | Only the cloud-backed features in the per-feature table |
| `DEG-19` | Unsigned or unverifiable inbound traffic | The signing state machine's own verification | The link's signing state; the policy outcome | Depends on the acceptance policy, which is `OPEN` |
| `DEG-20` | Override resolution failure at startup | The startup self-check | **A visible startup error, not a log line** | Potentially the entire Aerora interface |

### Response and authority

| ID | GCS response | Recovery | Authority change | Evidence | Requirements |
| --- | --- | --- | --- | --- | --- |
| `DEG-13` | **A partial set is never presented as complete.** Hide the pages that would show wrong values | A reload, triggered by the protocol's own validity check | None | `GCS-VERIFIED` | `AERORA-GCS-REQ-012`, `AERORA-GCS-REQ-013` |
| `DEG-14` | Report the failure; re-read the vehicle's mission state rather than assuming the previous plan survived | Operator retries the transfer; the station verifies by download | None | `GCS-VERIFIED` | `AERORA-MISSION-REQ-007` |
| `DEG-15` | Surface it. A station that continues silently unrecorded is the failure this condition exists to prevent | Operator intervention; the gap is recorded with its reason | None | `GCS-VERIFIED` | `AERORA-GCS-REQ-025` |
| `DEG-16` | **Drop, never block.** A full disk degrades the record; it does not freeze the operator interface | Queue drains; the drop count stays monotonic as evidence | None | `GCS-DECIDED` | `AERORA-GCS-REQ-025` |
| `DEG-17` | Warn early. **Do not delete an un-uploaded audit record to make room** | Operator action. Retention operates between flights, not during one | None | `GCS-DECIDED` | `AERORA-COMPLY-REQ-005` |
| `DEG-18` | Degrade per feature. **There is no global offline mode**, and no vehicle capability is affected | On reachability returning; queued work resumes | None | `BASELINE-ASSERTED` | `AERORA-AWS-REQ-007`, `AERORA-AWS-REQ-008` |
| `DEG-19` | Apply the configured policy explicitly. **Do not accept unsigned traffic by silence** | Key provisioning or policy change | None | `GCS-VERIFIED` mechanism; the Aerora default is `OPEN` | `AERORA-SAFE-REQ-010` |
| `DEG-20` | **Refuse to present a partial interface.** An interface quietly missing its Aerora layer is worse than one that will not start | A corrected build. There is no runtime recovery | None | `GCS-DECIDED` | `AERORA-UI-REQ-007`, `AERORA-TEST-REQ-007` |

### Why `DEG-20` fails loudly and the others do not

Every other condition degrades: something is shown as stale, absent, failed or queued, and the
station keeps working. `DEG-20` is the one that refuses, and the reason is that it is the only
condition the operator **cannot detect**.

A stale value looks stale. A failed command looks failed. An Aerora panel that never loaded looks
exactly like an Aerora panel with nothing to report. So the station asserts the condition at startup
and stops, because the alternative is an interface that lies by omission for an entire flight.

## Compound conditions

Two conditions at once is the normal case, and the rule is that each is presented on its own terms.

| Compound | Correct presentation | The tempting mistake |
| --- | --- | --- |
| `DEG-03` with a pending command | Disconnected, **and** the command in its last known state with its age | Marking the command failed, which the station cannot know |
| `DEG-07` with `DEG-01` | Last-known failsafe with its age, **and** all-stale | Showing the failsafe as current, or as cleared |
| `DEG-08` with `DEG-09` | Two independent events | One presented as the consequence of the other |
| `DEG-04` across a whole fact group | Each value with its own age | A single group-level stale badge that hides which value is old |
| `DEG-18` with `DEG-01` | Two unrelated indicators | One indicator that means "something is wrong" |
| `DEG-11` with `DEG-06` | Companion silent, **and** a command not corroborated | Concluding the companion caused the command to fail |

The last row is the general shape of the error: two conditions arriving together invite a causal
story, and a ground station that tells one has derived something it was not told.

## What this model deliberately omits

| Omission | Reason |
| --- | --- |
| Condition-to-failsafe-action mapping | Q-007. Baseline 13 forbids the ground station assigning them |
| Automatic mode changes on any condition | `AERORA-SAFE-REQ-008` |
| Return-to-launch, land or hold triggers | Vehicle-owned; the station reports what the vehicle reports |
| Freshness threshold values | Q-036; the per-class structure is specified, the numbers are not |
| Retry counts and timeouts beyond the protocol's own | The upstream mechanism was read in Phase 1 and is retained unchanged |
| Severity ranking across conditions | Would imply an operator priority order that no baseline states |
| Operator alerting policy — audible, modal, persistent | Q-040 and a product decision |

The absence of a severity ranking is deliberate and worth defending. Ranking `DEG-07` above
`DEG-09` would be the ground station asserting which of two reported conditions matters more, and
nothing in the baseline gives it that standing. The conditions are presented; the operator ranks
them.

## Conflicts recorded

| Conflict | Position A | Position B | Resolution authority |
| --- | --- | --- | --- |
| Unsigned-traffic policy on degradation | `DEG-19` needs a default to apply | No Aerora default exists; upstream ships signing off | **Unresolved.** Q-010, Q-011. `DEG-19` specifies that the policy is applied explicitly, not what it is |
| Storage exhaustion versus record retention | `DEG-17` forbids deleting an un-uploaded audit record | A full disk eventually stops recording | **Recorded.** Same tension as in [14-audit-logging-icd.md](14-audit-logging-icd.md); Q-039 must choose, and the architecture's choice is to stop recording visibly rather than delete evidence |

## Traceability

Each condition maps to the interfaces that detect and present it. Full mapping in
[19-interface-traceability.md](19-interface-traceability.md).

| Condition | Primary interfaces |
| --- | --- |
| `DEG-01`, `DEG-02` | `IF-RAD` set, `IF-FC-007`, `IF-TLM-006`, `IF-UIS-009` |
| `DEG-03` | `IF-RAD` set, `IF-VID-004`, `IF-FC-017` |
| `DEG-04` | `IF-TLM-010`, `IF-TLM-011`, `IF-UIS-004` |
| `DEG-05`, `DEG-06` | `IF-AUT-007`, `IF-AUT-008`, `IF-AUT-009`, `IF-UIS-007`, `IF-LOG-005` |
| `DEG-07` | `IF-AUT-010`, `IF-AUT-012`, `IF-LOG-012` |
| `DEG-08` | `IF-FC-012`, `IF-PAY-006`, `IF-UIS-009` |
| `DEG-09` | `IF-PAY-006`, `IF-PAY-010`, `IF-UIS-009` |
| `DEG-10` | `IF-PAY-002`, `IF-PAY-003`, `IF-UIS-005` |
| `DEG-11` | `IF-GPU` set, `IF-FC-011`, `IF-UIS-003` |
| `DEG-12` | `IF-PWR` set, `IF-FC-006` |
| `DEG-13` | `IF-PAR-001`, `IF-PAR-008` |
| `DEG-14` | `IF-MIS` set |
| `DEG-15`, `DEG-16`, `DEG-17` | `IF-LOG-014`, `IF-LOG-015`, `IF-LOG-023` |
| `DEG-18` | `IF-CLD-001`, `IF-CLD-013`, `IF-CLD-014` |
| `DEG-19` | `IF-SEC-014`, `IF-SEC-015`, `IF-SEC-017` |
| `DEG-20` | `IF-UIS-014`, `IF-UIS-015` |

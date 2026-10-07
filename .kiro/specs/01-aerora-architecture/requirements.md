# Requirements

The Aerora requirements register.

| Field | Value |
| --- | --- |
| Published requirements | **120** |
| Active | **119** |
| Withdrawn | **1** (`AERORA-UI-REQ-009`, duplicate of `AERORA-SEC-REQ-014`) |
| Identifier domains | **15** |
| Phase 1 requirements, carried separately | 55 |
| Combined Phase 1 and Phase 2 | 175 |
| Last revised | Phase 2.1 |

Every requirement carries a status, source, rationale, affected components and verification method.
Since Phase 2.1 every active requirement additionally carries a **ratification class** — see
[ratification.md](ratification.md).

Statements are reproduced verbatim from the architecture document that derived them. This register
adds the fields those documents do not carry.

## How to read this register

### Status

| Status | Meaning |
| --- | --- |
| `CONFIRMED` | Traceable to an explicit statement in the Aerora product baseline or the Phase 2 directive |
| `PROPOSED` | Derived during Phase 2 from code analysis or architectural reasoning; requires human ratification |
| `WITHDRAWN` | Published then retracted; the identifier is retired and not reused |

Status records **where a requirement came from**. The ratification class in
[ratification.md](ratification.md) records **what has to happen before it can be accepted**. The two
are independent: a `PROPOSED` requirement may be acceptable as it stands, and a `CONFIRMED` one may
still await hardware evidence for its parameters.

**No requirement in this register carries status `OPEN`.** Where a requirement's *parameter* is
unresolved — a payload state enumeration, a failsafe action, a corroboration window — the
requirement states the obligation and the parameter is tracked as an open question. An obligation
whose parameter is open is still an obligation. This distinction is deliberate: it keeps the
register from using a status to hide the fact that the architecture has committed.

### Source

| Code | Meaning |
| --- | --- |
| `B-nn` | Aerora product baseline document `nn` |
| `QGC` | QGroundControl source, read in Phase 1 or Phase 2 |
| `DIR` | The Phase 2 directive |
| `ADR-nn` | An architecture decision in [architecture-decisions.md](architecture-decisions.md) |
| `DERIVED` | Reasoned from another requirement or from an architectural principle |

### Verification method

| Method | Meaning |
| --- | --- |
| `TEST` | Automated test, runnable in continuous integration |
| `BUILD` | Verified by build configuration inspection or a configure-time check |
| `INSPECT` | Code or document review against a stated criterion |
| `ANALYSIS` | Reasoned argument recorded as an artifact |
| `DEMO` | Observed operation, including the startup self-check |
| `AUDIT` | Process check at a defined point, such as an upstream merge or a release gate |

`AUDIT` is the weakest of the six and is used only where a requirement governs a human procedure
rather than a software behaviour. Seven requirements rely on it; each is a candidate for conversion
to `TEST` once the relevant tooling exists.

## Ground station core

| ID | Statement | Status | Source | Rationale | Affected components | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `AERORA-GCS-REQ-007` | Aerora shall attach its firmware abstraction through a self-registering `FirmwarePluginFactory` requiring no core modification | `PROPOSED` | `QGC`, `ADR-001` | The factory register is the supported extension point and keeps the overlay core-edit free | `AeroraFirmwarePlugin`, Build | `TEST` |
| `AERORA-GCS-REQ-008` | `AeroraFirmwarePlugin` shall derive from `FirmwarePlugin` and shall not derive from a vendor firmware plugin | `PROPOSED` | `QGC`, `DERIVED` | Inheriting a vendor plugin imports flight-mode and custom-mode assumptions that are invisible at the point of use | `AeroraFirmwarePlugin` | `INSPECT` |
| `AERORA-GCS-REQ-009` | Aerora C++ relying on static self-registration shall be compiled into the main application target, not into an Aerora static library | `PROPOSED` | `QGC` | A self-registering global in a static library can be discarded by the linker, with no diagnostic | Build | `TEST` |
| `AERORA-GCS-REQ-010` | Aerora telemetry shall be exposed as FactGroups registered through `FirmwarePlugin::factGroups()` | `PROPOSED` | `QGC`, `ADR-001` | Yields message dispatch, the telemetry value picker and CSV logging without core edits | `FactGroups`, `AeroraFirmwarePlugin` | `TEST` |
| `AERORA-GCS-REQ-011` | A flight-mode accessor with no confirmed Aerora equivalent shall return empty so that the dependent action is hidden | `PROPOSED` | `QGC`, `DERIVED` | Hiding is the safe failure; a mis-wired mode name would present a non-existent capability | `AeroraFirmwarePlugin`, `UI` | `TEST` |
| `AERORA-GCS-REQ-012` | Aerora flight-controller parameters shall be addressed on MAVLink component 1 | `PROPOSED` | `QGC` | Parameter handling assumes the autopilot is component 1 throughout | `AeroraFirmwarePlugin`, Firmware | `TEST` |
| `AERORA-GCS-REQ-013` | Aerora setup pages shall be gated on the reported capability inventory rather than on hard-coded product assumptions | `PROPOSED` | `B-07`, `DERIVED` | Capability gating is the mechanism by which one build serves multiple Aerora products | `AeroraAutoPilotPlugin`, `UI` | `TEST` |
| `AERORA-GCS-REQ-014` | Aerora artifacts that outlive a connection shall key on the Aerora vehicle identifier and not on the MAVLink system id | `PROPOSED` | `QGC`, `DERIVED` | The system id is 8-bit, operator-configurable at both ends, and can change on reconfiguration | `Diagnostics`, `Cloud`, `Mission`, `Security` | `INSPECT` |
| `AERORA-GCS-REQ-015` | Every Aerora command shall be tracked through the eight-state command model | `CONFIRMED` | `DIR` | The directive specifies the state set; `EFFECTIVE` and `STALE` are what the model adds over upstream | `Authority`, `Payload`, `Mission` | `TEST` |
| `AERORA-GCS-REQ-016` | A command that reaches accepted and does not become effective within its corroboration window shall be marked stale and surfaced to the operator | `PROPOSED` | `DERIVED` | The accepted-but-not-effective gap is the failure most easily hidden and most worth detecting | `Authority`, `UI` | `TEST` |
| `AERORA-GCS-REQ-017` | A stale command shall not be retried automatically | `PROPOSED` | `B-48`, `DERIVED` | Re-issuing an operator action whose moment has passed would be the GCS deciding to command the aircraft | `Authority` | `TEST` |
| `AERORA-GCS-REQ-018` | Each Aerora command shall carry a GCS-assigned request identity in the log and audit record, independent of MAVLink acknowledgement correlation | `PROPOSED` | `QGC`, `DERIVED` | Acknowledgements are matched by component and command, so two identical commands are indistinguishable on the wire | `Authority`, `Diagnostics` | `TEST` |
| `AERORA-GCS-REQ-019` | An operator command shall pass both the operator authorisation gate and the vehicle authority gate; authorisation alone shall not be sufficient | `PROPOSED` | `B-48`, `DERIVED` | Operator authorisation and vehicle authority are independent; conflating them would let the GCS appear to grant vehicle authority | `Security`, `Authority` | `TEST` |
| `AERORA-GCS-REQ-020` | Application diagnostic, telemetry and audit records shall be maintained as separate records | `PROPOSED` | `DERIVED` | They differ in retention, consumer, volume and integrity requirement | `Diagnostics`, `Security` | `INSPECT` |
| `AERORA-GCS-REQ-021` | The audit record shall be unconditional and shall not be gated on armed state, telemetry settings, or vehicle connection | `PROPOSED` | `QGC`, `DERIVED` | Upstream telemetry recording opens only once armed, which would exclude pre-arm operator actions | `Security`, `Diagnostics` | `TEST` |
| `AERORA-GCS-REQ-022` | Aerora shall not create FactGroups dynamically after vehicle construction | `PROPOSED` | `QGC` | The CSV header is written once at file open while rows re-enumerate groups, so a later group misaligns every subsequent column | `FactGroups` | `TEST` |
| `AERORA-GCS-REQ-023` | Automatic upload of flight logs to any non-Aerora endpoint shall be disabled by the Aerora settings defaults | `PROPOSED` | `QGC`, `B-51` | Upstream enables auto-upload by default to a public third-party endpoint, gated only by an empty email field | `Settings`, `Diagnostics` | `TEST` |
| `AERORA-GCS-REQ-024` | Deletion of a local log after upload shall remain disabled and shall not be operator-configurable | `PROPOSED` | `DERIVED` | The local record is the evidence; an upload is a copy | `Settings`, `Diagnostics` | `TEST` |
| `AERORA-GCS-REQ-025` | The log writer shall never block the main thread, and shall account for every dropped record | `PROPOSED` | `DERIVED` | A full disk must degrade the record, not freeze the operator interface; an unexplained gap is not usable evidence | `Diagnostics` | `TEST` |
| `AERORA-GCS-REQ-026` | Every Aerora value shall be represented as a Fact, with enumerations and bounds supplied by metadata rather than code | `CONFIRMED` | `B-03`, `DIR` | The sixth architectural principle; also what lets an unresolved enumeration be filled in later without touching display, log or audit | `FactGroups`, `Settings` | `INSPECT` |
| `AERORA-GCS-REQ-027` | Every Aerora value shall carry its value, receive time and reporting source through memory, display and record without reduction | `PROPOSED` | `B-48`, `DERIVED` | Freshness cannot be reconstructed after the fact, and a value of unknown provenance cannot be assessed | `FactGroups`, `UI`, `Diagnostics` | `TEST` |
| `AERORA-GCS-REQ-028` | The local audit record store shall be integrity-protected | `PROPOSED` | `B-51`, `DERIVED` | An audit record that can be edited is not evidence. Mechanism depends on Q-037 | `Security` | `TEST` |
| `AERORA-GCS-REQ-029` | The GCS shall not derive vehicle state, identity, freshness, proposal acceptance, mission structure or failsafe clearance that the vehicle has not reported | `CONFIRMED` | `B-03`, `B-48` | Baseline rule that telemetry is untrusted external input, and that the GCS is supervisory | All domain modules | `INSPECT` |

## MAVLink

| ID | Statement | Status | Source | Rationale | Affected components | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `AERORA-MAV-REQ-005` | The Aerora dialect shall be maintained in an Aerora-controlled fork pinned to an immutable commit | `PROPOSED` | `QGC`, `ADR-004` | Aerora must own its message definitions, and a floating reference makes two builds of one commit differ | Dialect, Build | `BUILD` |
| `AERORA-MAV-REQ-006` | The GCS-to-FC interface and the FC-to-companion interface shall be separately specified in the interface control document | `CONFIRMED` | `B-07` | Baseline requirement; the two interfaces differ in trust level, change cadence and security posture | Dialect, Firmware | `INSPECT` |
| `AERORA-MAV-REQ-007` | The Aerora GCS shall implement the GCS-to-FC interface only and shall not emit FC-to-companion interface messages | `PROPOSED` | `B-07`, `DERIVED` | Structurally prevents the GCS from impersonating the companion computer | `Autonomy`, `AeroraPlugin` | `TEST` |
| `AERORA-MAV-REQ-008` | Every Aerora command that sets persistent state shall declare its resume behaviour | `PROPOSED` | `QGC`, `ADR-005` | Mission resume replays only a fixed upstream command set; declared behaviour is what makes the gap assessable | Dialect, `Mission`, `Payload` | `INSPECT` |
| `AERORA-MAV-REQ-009` | The vehicle shall report a capability inventory, and the GCS shall adapt its presented capabilities to it | `PROPOSED` | `B-07`, `DERIVED` | The mechanism by which the multi-product goal is achieved without product branching in the GCS | Firmware, `AeroraFirmwarePlugin`, `UI` | `TEST` |
| `AERORA-MAV-REQ-010` | A message vetoed at an Aerora extension seam shall be logged | `PROPOSED` | `QGC`, `DERIVED` | Both message seams can silently drop a message; a silent drop is undiagnosable — the generalised R-002 lesson | `AeroraFirmwarePlugin`, `AeroraPlugin` | `TEST` |

## Communications

| ID | Statement | Status | Source | Rationale | Affected components | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `AERORA-COMM-REQ-006` | Each Aerora radio shall connect as a QGroundControl serial link requiring no core modification | `PROPOSED` | `QGC`, `ADR-002`, `ADR-003` | Q-014 resolved: the serial path applies no device-identity gate and accepts arbitrary baud | `LinkState`, Hardware | `DEMO` |
| `AERORA-COMM-REQ-007` | The GCS shall present GCS-local link observation separately from vehicle-reported link state | `CONFIRMED` | `B-48`, `B-49` | The baseline requires the GCS mirror vehicle link arbitration without claiming authority over it | `LinkState`, `UI` | `TEST` |
| `AERORA-COMM-REQ-008` | The GCS shall display the vehicle-reported active link and shall not present a GCS-local observation as the active link | `CONFIRMED` | `B-48` | Link arbitration is a vehicle function; a GCS-local guess shown as the active link would misrepresent it | `LinkState`, `UI` | `TEST` |
| `AERORA-COMM-REQ-009` | The GCS shall display the vehicle-reported all-stale condition distinctly from GCS-observed communication loss | `CONFIRMED` | `B-48`, `B-49` | They are different conditions with different operator responses | `LinkState`, `UI` | `TEST` |
| `AERORA-COMM-REQ-010` | The GCS shall take no vehicle-directed action on communication loss beyond what QGroundControl already performs for link failover | `CONFIRMED` | `B-13`, `B-48` | The baseline excludes the GCS from being the safety controller | `LinkState` | `INSPECT` |
| `AERORA-COMM-REQ-011` | Aerora radio hardware shall use a vendor and product identifier that does not collide with QGroundControl's existing board-identification entries | `PROPOSED` | `QGC` | A stock CP2102 or FT231X bridge is identified as a SiK radio, auto-connected at a fixed baud, and takes the port reservation | Hardware | `INSPECT` |
| `AERORA-COMM-REQ-012` | Aerora settings defaults shall disable SiK radio auto-connect | `PROPOSED` | `QGC` | Second, independent mitigation of the same misidentification trap | `Settings` | `TEST` |

## Mission

| ID | Statement | Status | Source | Rationale | Affected components | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `AERORA-MISSION-REQ-001` | The saved Aerora plan file shall be the authoritative record of mission intent | `PROPOSED` | `QGC`, `ADR-005` | A mission item has no vendor-extension slot and the vehicle store can be left indeterminate by a partial upload | `Mission` | `INSPECT` |
| `AERORA-MISSION-REQ-002` | The GCS shall not reconstruct Aerora mission structure from a mission downloaded from the vehicle | `PROPOSED` | `QGC`, `DERIVED` | Complex items are flattened once on upload; reconstruction would be inference presented as fact | `Mission` | `TEST` |
| `AERORA-MISSION-REQ-003` | Mission resume shall not depend on the GCS replaying payload-state commands | `PROPOSED` | `QGC`, `ADR-005` | GCS-side replay would put authoritative state in the ground station. This prohibition holds regardless of whether the vehicle retains state; where it does not, the plan-constraint and resume-warning fallbacks apply | `Mission`, Payload controller | `TEST` |
| `AERORA-MISSION-REQ-004` | The operator shall be informed that resume reproduces the mission from the resume point and not the full mission history | `PROPOSED` | `QGC`, `DERIVED` | An operator who believes resume restores everything will be surprised once, at the worst moment | `UI`, `Mission` | `DEMO` |
| `AERORA-MISSION-REQ-005` | The Aerora mission command set shall be declared explicitly rather than by the empty-list default | `PROPOSED` | `QGC`, `DERIVED` | The empty-list default means all commands, offering the operator commands the firmware may not implement. Parameter depends on Q-023 | `AeroraFirmwarePlugin`, `Mission` | `TEST` |
| `AERORA-MISSION-REQ-006` | The GCS shall not infer mission item completion; it shall present the vehicle-reported mission index | `CONFIRMED` | `B-03`, `B-48` | Telemetry is the only source of vehicle state | `Mission`, `UI` | `TEST` |
| `AERORA-MISSION-REQ-007` | A mission that is uploaded but not verified against the plan file shall be presented as unverified | `PROPOSED` | `DERIVED` | Follows from plan-file authority: an unverified vehicle store is not known to match intent | `Mission`, `UI` | `TEST` |
| `AERORA-MISSION-REQ-008` | Post-flight mission reconstruction shall use the plan file and the log, not the vehicle mission store | `PROPOSED` | `QGC`, `DERIVED` | Resume replaces the vehicle mission, so the post-flight store is not the planned mission | `Mission`, `Diagnostics` | `INSPECT` |

## Payload

| ID | Statement | Status | Source | Rationale | Affected components | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `AERORA-PAYLOAD-REQ-006` | The payload controller shall be authoritative for payload state; the GCS shall not maintain an independent payload state model | `CONFIRMED` | `B-10`, `B-13` | The baseline assigns payload-local safety and state to the payload controller | `Payload`, Payload controller | `INSPECT` |
| `AERORA-PAYLOAD-REQ-007` | The payload controller shall retain payload state across a mission interruption so that mission resume requires no GCS-side command replay | `PROPOSED` | `ADR-005` | **Unproven vehicle-side behaviour.** No baseline document states that payload state is retained, and the firmware payload elements are placeholders. Addressed to the payload controller team as a design-and-verify obligation; the GCS carries a fallback if it is not implemented | Payload controller firmware | `DEMO` — bench verification on real hardware |
| `AERORA-PAYLOAD-REQ-008` | Payload emergency stop shall be represented separately from the payload state enumeration, with its initiating source reported | `CONFIRMED` | `B-46`, `B-13` | The baseline describes the stop as an independent cut, not a state in a sequence | `Payload`, `FactGroups` | `TEST` |
| `AERORA-PAYLOAD-REQ-009` | The GCS shall not initiate a payload emergency stop as an autonomous safety response | `CONFIRMED` | `B-13` | The GCS is not the safety controller | `Payload` | `INSPECT` |
| `AERORA-PAYLOAD-REQ-010` | No payload condition shall cause the GCS to issue a flight-control command | `CONFIRMED` | `B-03`, `B-13` | Eighth architectural principle: payload concerns must not leak into flight-control concerns | `Payload`, `Authority` | `TEST` |
| `AERORA-PAYLOAD-REQ-011` | A payload command that is accepted but never becomes effective shall be surfaced to the operator | `PROPOSED` | `DERIVED` | Domain specialisation of `AERORA-GCS-REQ-016`: an acknowledgement means arrival, not actuation | `Payload`, `UI` | `TEST` |
| `AERORA-PAYLOAD-REQ-012` | Product-independent payload state shall be separated from product-specific application telemetry into distinct FactGroups | `PROPOSED` | `B-00`, `DERIVED` | The mechanism by which the platform serves more than one Aerora product | `Payload`, `FactGroups` | `INSPECT` |
| `AERORA-PAYLOAD-REQ-013` | Payload controls shall be presented according to the reported payload capability set | `PROPOSED` | `B-10`, `DERIVED` | Capability gating rather than product branching | `Payload`, `UI` | `TEST` |

## Power

| ID | Statement | Status | Source | Rationale | Affected components | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `AERORA-POWER-REQ-007` | The watchdog fault shall be represented separately from the power state and shall not be modelled as a power state value | `PROPOSED` | `B-46`, `B-49` | The watchdog acts directly and independently; an enum value would imply it is one state in a sequence | `Power`, `FactGroups` | `TEST` |
| `AERORA-POWER-REQ-008` | The GCS shall not initiate a power transition as a safety response | `CONFIRMED` | `B-13`, `B-49` | The baseline excludes the GCS from high-voltage isolation, converter protection and tether fault handling | `Power` | `INSPECT` |

## Companion computer

| ID | Statement | Status | Source | Rationale | Affected components | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `AERORA-GPU-REQ-001` | The companion computer shall share the flight controller's MAVLink system id and use a distinct component id | `PROPOSED` | `QGC` | Message delivery filters on system id only; a different system id discards every companion message | Firmware, Hardware | `TEST` |
| `AERORA-GPU-REQ-002` | The companion computer shall not emit messages using the autopilot component id | `PROPOSED` | `QGC` | An autopilot component id would create a phantom second vehicle, and component-scoped handling assumes component 1 is the autopilot | Firmware | `TEST` |
| `AERORA-GPU-REQ-003` | The Aerora Autonomy module shall have no outbound command path | `PROPOSED` | `B-48`, `ADR-011` | Structural enforcement of the non-authoritative companion rule; a policy rule is one review away from violation | `Autonomy` | `TEST` |
| `AERORA-GPU-REQ-004` | The GCS shall present companion proposals as subordinate to effective vehicle state and shall label them as proposed | `CONFIRMED` | `B-48` | Baseline HMI authority distinction; equal visual weight invites the reading that the aircraft is following the proposal | `Autonomy`, `UI` | `DEMO` |
| `AERORA-GPU-REQ-005` | The GCS shall not infer proposal acceptance from subsequent vehicle state | `PROPOSED` | `B-48`, `DERIVED` | An inferred acceptance is indistinguishable from a coincidence, so the audit trail would record a correlation as a decision | `Autonomy` | `TEST` |
| `AERORA-GPU-REQ-006` | Every companion-sourced value shall carry a receive time and shall be marked stale past its class threshold | `CONFIRMED` | `B-48` | Baseline rule that the GCS shall not transform a stale companion command into a current one | `Autonomy`, `FactGroups` | `TEST` |
| `AERORA-GPU-REQ-007` | The GCS shall distinguish an absent companion from a companion reporting no proposal | `PROPOSED` | `DERIVED` | Different situations with different operator responses; one blank field conflates them | `Autonomy`, `UI` | `TEST` |
| `AERORA-GPU-REQ-008` | The GCS shall not perform perception or autonomy inference on the ground-station host | `PROPOSED` | `B-13`, `DERIVED` | Scope boundary that prevents a ground-side result ever becoming authoritative | `Autonomy` | `INSPECT` |

## Cloud

| ID | Statement | Status | Source | Rationale | Affected components | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `AERORA-AWS-REQ-005` | No operator command to a vehicle shall depend on cloud availability | `CONFIRMED` | `B-51` | Baseline real-time boundary: the manual flight command path must remain operational without AWS | `Cloud`, `Authority` | `TEST` |
| `AERORA-AWS-REQ-006` | The GCS shall complete connection, parameter load, mission planning, mission upload and local flight recording with all network access blocked | `CONFIRMED` | `B-51` | Baseline lists six capabilities that must work without AWS; this is the executable form of that list | `Cloud`, `Mission`, `Diagnostics` | `TEST` |
| `AERORA-AWS-REQ-007` | Each cloud-backed feature shall have a defined local behaviour; there shall be no single global offline mode | `PROPOSED` | `B-51`, `DERIVED` | A global mode invites the wrong question; the real question is always per feature | `Cloud`, `UI` | `INSPECT` |
| `AERORA-AWS-REQ-008` | Cloud connectivity shall be displayed separately from vehicle-link connectivity, with no shared visual language | `CONFIRMED` | `B-51` | Explicit baseline requirement; conflating them makes a cloud outage look like a link problem | `Cloud`, `LinkState`, `UI` | `DEMO` |
| `AERORA-AWS-REQ-009` | Cloud-sourced data presented when the cloud is unavailable shall be shown with its age | `PROPOSED` | `B-51`, `DERIVED` | The dangerous failure is an operator unable to distinguish current from old data | `Cloud`, `UI` | `TEST` |
| `AERORA-AWS-REQ-010` | The upload queue shall be bounded, and its depth and drop count shall be visible to the operator | `PROPOSED` | `DERIVED` | Unbounded queueing turns an outage into a memory failure; invisible depth hides a lost flight record | `Cloud`, `UI` | `TEST` |
| `AERORA-AWS-REQ-011` | Flight logs and audit records shall be written to local storage before any upload is attempted | `CONFIRMED` | `B-51` | Baseline requires local logging for flight records; a locally written record is evidence independent of transfer | `Diagnostics`, `Security`, `Cloud` | `TEST` |
| `AERORA-AWS-REQ-012` | The Security module shall not depend on the Cloud module | `PROPOSED` | `B-51`, `DERIVED` | A Security module that calls the cloud to answer an authorisation question has put the cloud on the authorisation path | `Security`, `Cloud` | `TEST` |
| `AERORA-AWS-REQ-013` | Cloud notifications shall be informational and shall not direct a flight action | `PROPOSED` | `B-51`, `DERIVED` | A flight-directive notification puts the cloud on the command path through the operator | `Cloud`, `UI` | `INSPECT` |
| `AERORA-AWS-REQ-014` | The Aerora GCS shall accept no inbound network connections | `PROPOSED` | `QGC`, `B-51` | Removes the entire inbound attack surface; upstream links an HTTP server module but never opens a listener | `Cloud`, Build | `TEST` |
| `AERORA-AWS-REQ-015` | No synchronous network call shall be made from the main thread | `PROPOSED` | `DERIVED` | A stalled name resolution would freeze the operator interface | `Cloud` | `TEST` |

## Security

| ID | Statement | Status | Source | Rationale | Affected components | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `AERORA-SEC-REQ-011` | Operator credential validation shall succeed without network access as the primary path | `CONFIRMED` | `B-51` | Baseline offline-first principle; a fallback path is the less-tested path and this one must work every flight | `Security` | `TEST` |
| `AERORA-SEC-REQ-012` | The authorisation gate shall be applied before a command is written to a link | `PROPOSED` | `DERIVED` | A post-hoc check is not a gate | `Security`, `Authority` | `TEST` |
| `AERORA-SEC-REQ-013` | The set of authorisation-gated operations shall be declared explicitly and shall not be derived from interface visibility | `PROPOSED` | `DERIVED` | Deriving the gated set from visibility would make the advanced-UI mistake structural | `Security` | `INSPECT` |
| `AERORA-SEC-REQ-014` | The advanced-UI toggle shall not be used as an authorisation boundary for any operation | `PROPOSED` | `DIR`, `QGC` | The toggle changes visibility and anyone at the keyboard can turn it on; it is a security control one click deep | `Security`, `UI` | `INSPECT` |
| `AERORA-SEC-REQ-015` | An authorisation denial shall be recorded as an audit event with the operator identity and the denial reason | `PROPOSED` | `B-51`, `DERIVED` | Denials are the events an investigation most often needs and the ones most often unlogged | `Security`, `Diagnostics` | `TEST` |
| `AERORA-SEC-REQ-016` | Aerora signing keys shall be provisioned as randomly generated raw keys and not derived from operator passphrases | `PROPOSED` | `QGC` | Passphrase derivation uses a fixed application-wide salt with an eight-character minimum, making a precomputation reusable across every installation | `Security`, Process | `AUDIT` |
| `AERORA-SEC-REQ-017` | A production release shall not persist signing key material without protection at rest | `CONFIRMED` | `B-51` | Baseline requires managed secrets and encrypted storage; keys are currently written to the settings store unprotected (R-005) | `Security` | `INSPECT` |
| `AERORA-SEC-REQ-018` | A settings-schema version change shall be treated as a key-destroying event requiring fleet reprovisioning until key custody is moved out of the settings store | `PROPOSED` | `QGC` | A schema version mismatch clears all settings before any subsystem starts, and keys share that store | `Security`, Build, Process | `AUDIT` |
| `AERORA-SEC-REQ-019` | Audit records shall carry operator identity, request identity and local timestamps, and shall be written locally before any upload | `PROPOSED` | `B-51`, `DERIVED` | A record that exists only after a successful upload can be suppressed by interfering with the upload | `Security`, `Diagnostics` | `TEST` |
| `AERORA-SEC-REQ-020` | The MAVLink dialect and all third-party dependencies shall be pinned to immutable references before release | `CONFIRMED` | `B-51` | Baseline requires signed and versioned release artifacts; two dependencies currently track moving references (R-011) | Build, Dialect | `BUILD` |

## Operator interface

| ID | Statement | Status | Source | Rationale | Affected components | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `AERORA-UI-REQ-004` | An Aerora interface change shall attach at the lowest available seam tier: a C++ virtual before a purpose-built QML seam before overriding a populated upstream file | `PROPOSED` | `QGC`, `ADR-001` | The three tiers differ in maintenance cost by an order of magnitude, and the most expensive one fails silently | `UI` | `INSPECT` |
| `AERORA-UI-REQ-005` | Each override of a populated upstream file shall be recorded with its reason, fork-point commit and named owner | `PROPOSED` | `DIR`, `DERIVED` | A resource override leaves version control clean, so nothing surfaces the fork without an inventory | `UI`, Build | `INSPECT` |
| `AERORA-UI-REQ-006` | Each upstream merge shall include a drift review of every overridden populated file | `PROPOSED` | `DERIVED` | The only mechanism that catches an upstream fix being discarded by an override | `UI`, Process | `AUDIT` |
| `AERORA-UI-REQ-007` | A startup self-check shall verify that every Aerora override resource resolves to the Aerora content, and shall fail visibly if it does not | `PROPOSED` | `DERIVED` | R-019: a silently missing Aerora layer is worse than a refusal to start. Parameter depends on Q-020 | `AeroraPlugin`, `UI` | `DEMO` |
| `AERORA-UI-REQ-008` | An action the aircraft cannot perform shall be hidden; an action it cannot perform in its current state shall be disabled with a stated reason | `PROPOSED` | `DERIVED` | A disabled control invites waiting; waiting is wrong when the capability does not exist on this aircraft | `UI` | `TEST` |
| `AERORA-UI-REQ-009` | Withdrawn — duplicate of `AERORA-SEC-REQ-014` | `WITHDRAWN` | `DERIVED` | One obligation should have one identifier; the authorisation domain is the correct home | None | None |
| `AERORA-UI-REQ-010` | Aerora QML shall be delivered as named QML modules rather than loose files | `PROPOSED` | `QGC`, `DERIVED` | A module URI is a namespace, so an Aerora control cannot collide with an upstream control of the same name | `UI`, Build | `BUILD` |

## Safety and authority

| ID | Statement | Status | Source | Rationale | Affected components | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `AERORA-SAFE-REQ-006` | The GCS shall display the vehicle-reported effective authority mode and shall not display a requested mode as effective | `CONFIRMED` | `B-48` | Baseline HMI authority distinction; the second architectural principle applied to authority | `Authority`, `UI` | `TEST` |
| `AERORA-SAFE-REQ-007` | A pending authority confirmation shall be represented as a distinct visible state | `CONFIRMED` | `B-46`, `B-48` | Semi-automatic supervised operation requires explicit pilot confirmation; an operator unaware a confirmation is pending is the failure that makes it unsafe | `Authority`, `UI` | `TEST` |
| `AERORA-SAFE-REQ-008` | The GCS shall not decide or autonomously initiate a failsafe action | `CONFIRMED` | `B-13`, `B-00` | Baseline distributes safety and excludes the GCS as safety controller; a stated non-goal is making the GCS the sole safety mechanism | `Authority` | `INSPECT` |
| `AERORA-SAFE-REQ-009` | The GCS shall not infer a failsafe condition the vehicle has not reported, and shall not clear a reported failsafe state locally | `PROPOSED` | `B-03`, `DERIVED` | A failsafe reported once and then absent is stale, not cleared | `Authority` | `TEST` |
| `AERORA-SAFE-REQ-010` | Failsafe state, reason, source, severity and reported action shall each be represented separately | `PROPOSED` | `B-13`, `ADR-010` | Five distinct questions; collapsing them loses the information a debrief needs. Values depend on Q-007 | `Authority`, `FactGroups` | `INSPECT` |
| `AERORA-SAFE-REQ-011` | The watchdog-initiated disarm and the payload emergency stop shall be presented as independent events | `CONFIRMED` | `B-46` | The firmware model shows the watchdog reaching both independently; presenting one as a consequence of the other would be a GCS-side inference | `Authority`, `Payload`, `UI` | `TEST` |

## Build and configuration

| ID | Statement | Status | Source | Rationale | Affected components | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `AERORA-BUILD-REQ-004` | Aerora shall maintain its own repository with upstream QGroundControl configured as a fetch-only remote | `PROPOSED` | `QGC`, `DIR` | The analysed checkout points at upstream for both fetch and push; fetch-only removes the possibility of an accidental push | Build, Process | `INSPECT` |
| `AERORA-BUILD-REQ-005` | The upstream fork point commit shall be recorded and shall match the commit the baseline analysis was performed against | `PROPOSED` | `DERIVED` | Makes upstream drift answerable, and keeps the Phase 1 documents describing the same code | Build | `INSPECT` |
| `AERORA-BUILD-REQ-006` | All Aerora code and resources shall reside in a single overlay directory | `CONFIRMED` | `DIR`, `ADR-001` | Seventh architectural principle; also what keeps the core-edit count verifiable | Build | `INSPECT` |
| `AERORA-BUILD-REQ-007` | Each upstream integration shall include a drift review of every overridden file and a seam signature review | `PROPOSED` | `DERIVED` | A changed virtual signature is a compile error; changed virtual semantics are not | Build, Process | `AUDIT` |
| `AERORA-BUILD-REQ-008` | The MAVLink dialect shall be pinned to an immutable commit and never to a branch | `PROPOSED` | `QGC`, `ADR-004` | Without it, two builds of one Aerora commit can embed different message definitions and the compatibility matrix is meaningless | Build, Dialect | `BUILD` |
| `AERORA-BUILD-REQ-009` | A settings-schema version change shall require a recorded reprovisioning plan while signing keys are persisted in the settings store | `PROPOSED` | `QGC` | The schema version and key custody share one store, so a routine build edit destroys key material | Build, `Security`, Process | `AUDIT` |
| `AERORA-BUILD-REQ-010` | A deployed GCS shall be able to report its build version, fork point, dialect commit, settings schema version, applied configuration version and dependency versions | `PROPOSED` | `B-07`, `DERIVED` | A compatibility matrix is useless if a station cannot identify itself | `Diagnostics`, Build | `TEST` |
| `AERORA-BUILD-REQ-011` | An Aerora setting shall not shadow or cache a vehicle parameter | `PROPOSED` | `B-03`, `DERIVED` | Vehicle parameters are authoritative; a shadowing setting creates two answers to one question | `Settings` | `INSPECT` |
| `AERORA-BUILD-REQ-012` | Distributed vehicle configuration shall be versioned, cached, usable without network access, and applied deliberately rather than mid-operation | `PROPOSED` | `B-51`, `DERIVED` | Prevents configuration becoming a cloud dependency, and prevents a mid-flight behaviour change the operator did not plan for | `Cloud`, `Settings` | `TEST` |

## Compliance and records

| ID | Statement | Status | Source | Rationale | Affected components | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `AERORA-COMPLY-REQ-002` | Every Aerora command shall produce one audit record carrying operator identity, request identity, all command state transitions and the terminal reason | `PROPOSED` | `B-51`, `DERIVED` | Makes an operator action attributable; MAVLink correlation alone cannot distinguish two identical commands | `Security`, `Diagnostics` | `TEST` |
| `AERORA-COMPLY-REQ-003` | An authority mode request and the reported effective change shall be recorded as separate events | `PROPOSED` | `B-48`, `DERIVED` | Only separate records can answer whether the operator's request was followed | `Authority`, `Diagnostics` | `TEST` |
| `AERORA-COMPLY-REQ-004` | Every logged Aerora value shall carry its receive time and reporting source | `PROPOSED` | `B-48`, `DERIVED` | Freshness and provenance cannot be reconstructed from a record that omits them | `Diagnostics` | `TEST` |
| `AERORA-COMPLY-REQ-005` | Retention policy shall not delete an audit record before it has been uploaded or exported, and every deletion shall be recorded | `PROPOSED` | `DERIVED` | A gap with a reason is usable evidence; a gap without one is not. Parameter depends on Q-039 | `Diagnostics`, `Cloud` | `TEST` |

## Vehicle identity

| ID | Statement | Status | Source | Rationale | Affected components | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `AERORA-VEH-REQ-001` | The Aerora vehicle identifier shall be distinct from the MAVLink system id | `PROPOSED` | `QGC`, `DERIVED` | R-017: the system id is 8-bit, operator-configurable at both ends, and reassignable | `FactGroups`, `Cloud`, `Diagnostics` | `INSPECT` |
| `AERORA-VEH-REQ-002` | The Aerora vehicle identifier shall be stable for the life of the aircraft, unique across the fleet, reportable over MAVLink, and not operator-editable | `PROPOSED` | `DERIVED` | Four properties a record-bearing identity needs. Source depends on Q-016 | Firmware, Hardware | `INSPECT` |
| `AERORA-VEH-REQ-003` | The binding between MAVLink system id and Aerora vehicle identifier shall be established from vehicle-reported telemetry on each connection | `PROPOSED` | `B-03`, `DERIVED` | Telemetry is the only source; a cached binding would survive a hardware swap | `FactGroups`, `Diagnostics` | `TEST` |
| `AERORA-VEH-REQ-004` | No consumer shall parse the internal structure of the Aerora vehicle identifier | `PROPOSED` | `DERIVED` | A parsed identifier becomes a dependency on its format; opacity remains correct even after Q-016 resolves | All domain modules | `INSPECT` |
| `AERORA-VEH-REQ-005` | Records requiring an Aerora identity shall be buffered until the identity is reported, and a connection that reports none shall produce records explicitly attributed to an unidentified vehicle | `PROPOSED` | `DERIVED` | Discarding the pre-identity span would lose the connection and parameter-load events | `Diagnostics`, `Security` | `TEST` |

## Testing

| ID | Statement | Status | Source | Rationale | Affected components | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `AERORA-TEST-REQ-004` | Architectural-property tests shall verify that the Autonomy module emits no MAVLink, that the Aerora firmware plugin is selected, that no FactGroup is created after vehicle construction, and that no listening socket is open | `PROPOSED` | `DERIVED` | These verify properties the architecture asserts about itself, which no behavioural test would catch | `Autonomy`, `AeroraFirmwarePlugin`, `FactGroups`, `Cloud` | `TEST` |
| `AERORA-TEST-REQ-005` | A test shall verify full local operation — connect, parameter load, mission plan, upload, execution and local flight record — with all network access blocked | `PROPOSED` | `B-51`, `ADR-006` | Makes the cloud-off-the-command-path property enforceable rather than aspirational, against eight named traps | `Cloud`, `Mission`, `Diagnostics` | `TEST` |
| `AERORA-TEST-REQ-006` | Architectural-property tests shall run in the default continuous-integration gate and shall not be placed in an optional job | `PROPOSED` | `DERIVED` | A property test in an optional job stops running the first time that job is slow | Build | `INSPECT` |
| `AERORA-TEST-REQ-007` | A startup self-check shall assert override resource resolution, Aerora QML module importability, firmware plugin selection and the absence of a listening socket, and shall fail visibly | `PROPOSED` | `DERIVED` | R-019 and R-010 both fail silently in a deployed build, where no test is running | `AeroraPlugin`, `UI` | `DEMO` |
| `AERORA-TEST-REQ-008` | Overridden-file drift shall be checked mechanically against recorded fork-point commits rather than by procedure alone | `PROPOSED` | `DERIVED` | Converts three merge-time procedures that people forget into a check that fails | `UI`, Build | `TEST` |
| `AERORA-TEST-REQ-009` | Aerora tests shall use the upstream test framework, base classes and labels without modification | `PROPOSED` | `QGC`, `ADR-001` | The framework is in good shape; divergence would cost maintenance for no gain | Build | `INSPECT` |

## Identifier domain reconciliation

### The power domain prefix

Phase 1 published six power requirements under `AERORA-PWR-REQ-`. The Phase 2 directive names the
domain `AERORA-POWER-REQ-`.

| Decision | Detail |
| --- | --- |
| Canonical prefix from Phase 2 onward | `AERORA-POWER-REQ-` |
| Phase 1 identifiers | `AERORA-PWR-REQ-001` through `AERORA-PWR-REQ-006`, retained as superseded aliases |
| Numbering | Continuous: Phase 2 power requirements start at 007 |
| Phase 1 documents | Not edited — they are a frozen, human-reviewed baseline, and rewriting them would invalidate that review |

The alias mapping is in [traceability.md](traceability.md). The directive outranks a Phase 1
analysis output, which is why the directive's spelling wins; identifier stability is why the Phase 1
numbers are preserved rather than renumbered.

### Domains in use

| Domain | Phase 1 range | Phase 2 range | Phase 2 count |
| --- | --- | --- | --- |
| `AERORA-GCS-REQ-` | 001-006 | 007-029 | 23 |
| `AERORA-MAV-REQ-` | 001-004 | 005-010 | 6 |
| `AERORA-COMM-REQ-` | 001-005 | 006-012 | 7 |
| `AERORA-MISSION-REQ-` | None | 001-008 | 8 |
| `AERORA-PAYLOAD-REQ-` | 001-005 | 006-013 | 8 |
| `AERORA-POWER-REQ-` | 001-006 as `PWR` | 007-008 | 2 |
| `AERORA-GPU-REQ-` | None | 001-008 | 8 |
| `AERORA-AWS-REQ-` | 001-004 | 005-015 | 11 |
| `AERORA-SEC-REQ-` | 001-010 | 011-020 | 10 |
| `AERORA-UI-REQ-` | 001-003 | 004-010 | 7 |
| `AERORA-SAFE-REQ-` | 001-005 | 006-011 | 6 |
| `AERORA-BUILD-REQ-` | 001-003 | 004-012 | 9 |
| `AERORA-COMPLY-REQ-` | 001 | 002-005 | 4 |
| `AERORA-VEH-REQ-` | None | 001-005 | 5 |
| `AERORA-TEST-REQ-` | 001-003 | 004-009 | 6 |

Phase 2 total: **120**, of which one is withdrawn, leaving **119 active**.

## Status summary

| Status | Count |
| --- | --- |
| `CONFIRMED` | 28 |
| `PROPOSED` | 91 |
| `WITHDRAWN` | 1 |
| Total | 120 |

Twenty-eight requirements trace to an explicit baseline or directive statement. The remaining
ninety-one are Phase 2 derivations and **require human ratification** — they are the architecture's
reasoning expressed as obligations, not pre-existing mandates. That ratio is the honest summary of
this register: three quarters of it is proposal.

## Verification method summary

Revised in Phase 2.1. Two requirements moved off `ANALYSIS` as a consequence of the R-002
reclassification, leaving that method with a count of zero.

| Method | Count |
| --- | --- |
| `TEST` | 69 |
| `INSPECT` | 34 |
| `DEMO` | 7 |
| `AUDIT` | 5 |
| `BUILD` | 4 |
| `ANALYSIS` | 0 |
| None (withdrawn) | 1 |
| Total | 120 |

`ANALYSIS` retains its definition because a future requirement may need it, but nothing uses it now.
That is an improvement: a requirement verified only by a reasoned argument is a requirement nobody
can fail.

Sixty-nine of the 119 active requirements are verifiable by automated test — 58 per cent. The five
`AUDIT` requirements are the register's weakest link, because they depend on a human remembering a
procedure:

| Requirement | Procedure |
| --- | --- |
| `AERORA-SEC-REQ-016` | Key provisioning uses random raw keys |
| `AERORA-SEC-REQ-018` | Settings-version bump triggers reprovisioning |
| `AERORA-UI-REQ-006` | Drift review at each upstream merge |
| `AERORA-BUILD-REQ-007` | Drift and seam review at each upstream merge |
| `AERORA-BUILD-REQ-009` | Reprovisioning plan recorded before a schema bump |

Two of the five disappear once signing key custody moves out of the settings store. The other three
are merge-time procedures that could be partially automated by a tool comparing overridden files
against their recorded fork points — a worthwhile early investment, noted in
[testing-architecture.md](testing-architecture.md).

## Overlapping requirements

Three sets of requirements address related obligations from different angles. They are not
contradictory and none is redundant, but a verifier should know the division so as not to
double-count or assume conflict.

| Set | Members | Division |
| --- | --- | --- |
| Dialect and dependency pinning | `AERORA-MAV-REQ-005`, `AERORA-BUILD-REQ-008`, `AERORA-SEC-REQ-020` | Ownership and pinning; pinning mechanism and the prohibition on branches; supply-chain scope including third-party dependencies |
| Local-first records | `AERORA-AWS-REQ-011`, `AERORA-SEC-REQ-019` | Write ordering for logs and audit records; audit record content and the security rationale |
| Accepted-but-not-effective | `AERORA-GCS-REQ-016`, `AERORA-PAYLOAD-REQ-011` | The general rule; the payload specialisation, where effectiveness means a reported state change rather than an acknowledgement |

## Traceability

Every requirement maps to its deriving document, the risks it mitigates and the open questions it
depends on in [traceability.md](traceability.md).

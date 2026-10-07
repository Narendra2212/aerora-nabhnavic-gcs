# Phase 2 Open Question Resolution

Triage of all 34 open questions raised in Phase 1
([open-questions.md](../00-qgc-baseline-analysis/open-questions.md)).

No question is answered by guessing. Each is routed to the evidence source that can actually
answer it, and prioritised by architectural impact.

## Resolution routes

| Route | Meaning |
| --- | --- |
| `CODE` | Answerable by reading the QGroundControl source. Aerora can close it without external input |
| `HARDWARE` | Requires schematic, PCB, bench or flight evidence |
| `PRODUCT` | Requires an Aerora product or operational-concept decision |
| `HUMAN` | Requires a named human decision; no amount of analysis will produce it |
| `EXTERNAL` | Requires validation outside Aerora — counsel, a regulator, a vendor, or a certification body |
| `RESOLVED` | Closed in Phase 2 |

Several questions need more than one route; the primary route is listed first.

## Impact classification

| Impact | Meaning |
| --- | --- |
| `ARCH-BLOCKING` | The architecture cannot be settled without it |
| `ARCH-SHAPING` | The architecture accommodates either answer, but the answer changes a component |
| `IMPL-BLOCKING` | Implementation cannot start without it |
| `DEFERRABLE` | Can be answered during or after implementation |

## Status

| Outcome | Count | Questions |
| --- | --- | --- |
| `RESOLVED` in Phase 2 | 5 | Q-001, Q-002, Q-013, Q-014, Q-022 |
| `PARTIALLY RESOLVED` — amended in Phase 2.1 | 1 | Q-019 — see the correction in its section below |
| `ARCH-BLOCKING` remaining | 0 | — |
| `ARCH-SHAPING` | 9 | Q-003, Q-007, Q-008, Q-012, Q-016, Q-017, Q-023, Q-029, Q-030 |
| `IMPL-BLOCKING` | 6 | Q-004, Q-006, Q-018, Q-020, Q-027, Q-032 |
| `DEFERRABLE` | 13 | Q-005, Q-009, Q-010, Q-011, Q-015, Q-021, Q-024, Q-025, Q-026, Q-028, Q-031, Q-033, Q-034 |

**No question blocks architecture ratification.** The nine `ARCH-SHAPING` questions are parameters
of an architecture that is otherwise settled; each is accommodated by an explicit extension point
rather than a guess.

---

## Resolved in Phase 2

### Q-014 Can an Aerora radio present as a standard serial device?

| Field | Value |
| --- | --- |
| Route | `CODE` — **`RESOLVED`** |
| Impact | Was `ARCH-SHAPING`; resolution removed six planned core edits |

**Answer: yes.** A generic, unrecognised serial device carries MAVLink through QGroundControl's
existing `SerialLink` with **zero core changes**. Nothing in the manual connect path consults USB
vendor or product identifiers, board type, or the board-info database.

Verified at every step: port enumeration is unfiltered and discards the board-info result; the UI
lists all ports; arbitrary baud is settable through a `Custom` field with an integer validator;
configuration persists port name, baud, data bits, flow control, stop bits, parity and a DTR-low
flag; the worker simply opens the port and applies those settings; port reservation refuses only
for identity-independent reasons.

The only behavioural difference for an unrecognised device is that it is never auto-connected —
the user creates the link once and the configuration persists.

**Three practical frictions** carried forward as implementation notes, not blockers:

1. A stock Silicon Labs CP2102 identifier is already mapped to "SiK Radio" in QGC's board-info
   database, with description fallbacks for FT231X. An Aerora radio on a stock bridge chip will be
   misidentified, auto-connected at 57600 baud, and will take the port reservation, causing the
   manual link to fail. Mitigation: a distinct vendor and product identifier, or disable SiK
   auto-connect.
2. The default Android backend requires the device to be in a probe table; a custom identifier may
   not enumerate. Mainstream bridge chips are covered.
3. macOS filters certain serial node names and uses an alternate node.

Consequences recorded as ADR-002 and ADR-003 in
[architecture-decisions.md](architecture-decisions.md), with detail in
[communication-architecture.md](communication-architecture.md). Q-012 is reduced from a decision
to a deferral as a direct result.

### Q-019 How is Aerora payload state preserved across mission resume?

| Field | Value |
| --- | --- |
| Route | `CODE` then `PRODUCT` — **`RESOLVED` architecturally** |
| Impact | Was `ARCH-BLOCKING` |

**Answer: by not relying on the GCS to reconstruct it.**

Phase 2 read the resume generator in full. The decisive finding is that the inclusion condition
contains a clause admitting everything at or after the resume index, so Aerora commands positioned
at or after the resume point already survive untouched. The filter only affects the replayed
prefix.

The deeper finding is why the prefix replay exists at all: QGC replays configuration commands
*because it assumes the vehicle retains nothing*. Per the v4.1 baseline, the payload controller owns
actuator-level payload behaviour and payload-local interlocks, so the proposed fix is to make the
payload controller authoritative for its own configuration and to have it retain that configuration
across an interruption.

Decision recorded as ADR-005. Detail in [mission-architecture.md](mission-architecture.md).
**No QGroundControl core change is required.**

### Phase 2.1 correction to this resolution

The text above originally continued "so that assumption is wrong for Aerora", and classified the
remainder as a `PRODUCT` confirmation. Both were too strong.

| Original | Phase 2.1 |
| --- | --- |
| The QGC assumption "is wrong for Aerora" | The assumption is **proposed to be made wrong**; it is not yet |
| Remaining work is a `PRODUCT` confirmation | Remaining work is a `HARDWARE` design decision followed by bench verification |
| Tracked as part of Q-023 | Tracked on its own, because Q-023 is the actuator model and retention is a separate behaviour |

Q-019 is therefore **downgraded from `RESOLVED` to `PARTIALLY RESOLVED`**. What is resolved: the
filter's bounded scope, the absence of a seam, and the rejection of both core modification and
GCS-side replay. What is not: whether the payload controller will retain state. That is an evidence
item in the [hardware and firmware evidence matrix](hardware-firmware-evidence-matrix.md), and the
Phase 2.1 count of questions resolved in Phase 2 drops from six to five.

### Q-013 Does FHSS24 and RF433 hardware actually exist?

| Field | Value |
| --- | --- |
| Route | `HARDWARE` — **`RESOLVED` as a scope decision** |
| Impact | Was `ARCH-SHAPING` |

The question is not closed as a hardware fact, but it is **removed from the architecture's critical
path**, which is the outcome that matters for Phase 2.

The v4.1 baseline states plainly that FHSS24 and RF433 physical implementation must be confirmed
separately before being called onboard hardware, while the LoRa module is observable in the
schematic. Phase 1 confirmed none of the three has any QGroundControl support.

Because Q-014 resolved to serial-first, the architecture no longer needs to know how many radios
exist. Each radio that presents as a serial port is a configuration, not a component. The link
model in [communication-architecture.md](communication-architecture.md) represents an
arbitrary number of links and mirrors the vehicle-reported active link, which is what the baseline
requires.

Hardware confirmation is still required before any radio-specific integration work, and before any
claim about link redundancy. It is `DEFERRABLE` to Phase 5.

### Q-001 Which output structure governs the baseline analysis?

| Field | Value |
| --- | --- |
| Route | `HUMAN` — **`RESOLVED` by instruction** |
| Impact | Process only |

The Phase 1 review instruction named `.kiro/specs/00-qgc-baseline-analysis/` as the authoritative
location and listed the sixteen documents produced there. That settles the conflict in favour of
the structure Phase 1 delivered.

Residual action: the v4.1 baseline's own `.kiro/specs/00-qgc-baseline-analysis/design.md` still
nominates five documents under `docs/`. It should be updated to match, so the two stop disagreeing.
Recorded in [open-questions.md](open-questions.md) as a documentation-consistency action.

### Q-002 Should the baseline's `.kiro/` scaffold be promoted to the workspace root?

| Field | Value |
| --- | --- |
| Route | `HUMAN` — **`RESOLVED` by observation** |
| Impact | Process only |

Resolved in the negative for now, by the fact that two phases have been executed successfully
without it. The active agent guide remains QGroundControl's own, which is correct for work that
reads QGC source and must respect QGC conventions.

The Aerora steering content is not lost — it is preserved in the baseline package. Promoting it
would require reconciling it with QGC's own guide, which remains binding for anything touching QGC
source. That reconciliation is a Phase 3 action if Aerora wants it.

### Q-022 Must a painting plan survive download from the vehicle?

| Field | Value |
| --- | --- |
| Route | `PRODUCT` — **`RESOLVED` architecturally** |
| Impact | Was `DEFERRABLE` |

**Answer: no, and it should not be relied upon.**

Phase 2 established this is inherent to the MAVLink mission representation rather than a QGC
defect: a mission item carries a command, a frame, seven parameters and a few flags, with no
vendor-extension slot. Pattern structure cannot survive a round trip. The three stock survey-style
patterns have the same limitation.

The architecture declares the **saved plan file authoritative**, not the vehicle's mission store.
For an industrial operation where the plan is the job specification this is better practice
regardless, and it aligns with the cloud mission-synchronisation module in the v4.1 baseline.

Recorded as ADR-005. The workflow constraint must be documented for operators.

---

## Architecture-shaping — accommodated, answer still required

These nine change a component but not the architecture. Each has an explicit extension point so
either answer can be adopted without redesign.

### Q-003 Fork and upstream-tracking strategy

| Field | Value |
| --- | --- |
| Route | `HUMAN` |
| Impact | `ARCH-SHAPING`, and `IMPL-BLOCKING` |
| Accommodated by | [configuration-architecture.md](configuration-architecture.md) |

The architecture assumes a fork with upstream tracked as a second remote and the Phase 1 commit
recorded as the fork point. The cadence and conflict ownership are human decisions. Nothing can be
committed until this is set, which is why it is also implementation-blocking.

### Q-007 Aerora failsafe behaviour

| Field | Value |
| --- | --- |
| Route | `HARDWARE` then `PRODUCT` — awaiting the approved Simulink implementation |
| Impact | `ARCH-SHAPING` |
| Accommodated by | [authority-model.md](authority-model.md) |

**Deliberately deferred by the v4.1 baseline and not inferred here.** The architecture provides
the interfaces the Phase 2 instruction requires — failsafe state, reason, source, severity and
action — without prescribing any action value.

The critical implementation constraint, recorded as a requirement: the Aerora firmware plugin's
named flight-mode accessors must **not** be populated with guessed mode names, because QGC uses
them to decide what to command. A guess becomes a command.

Phase 2 did learn useful structure from the baseline's Simulink evidence: the firmware contains
authority sentinels for manual, autonomous coverage and supervised semi-autonomous operation, with
manual structurally dominant, explicit pilot confirmation required in semi-auto, and a
proposal-rejection path to a safe mode whose code is itself flagged as requiring confirmation. The
architecture mirrors that structure as reported state.

### Q-008 Distinguishing a proposed command from an effective command

| Field | Value |
| --- | --- |
| Route | `PRODUCT` then `HARDWARE` |
| Impact | `ARCH-SHAPING` |
| Accommodated by | [authority-model.md](authority-model.md), [gpu-integration.md](gpu-integration.md) |

Phase 2 designed the full state model the instruction requires — requested, sent, received,
accepted, rejected, effective, stale, failed — and identified that QGC already has the raw material
for part of it: acknowledgement tracking distinguishes sent from accepted, and an existing
operator-control handshake with takeover semantics provides authority state.

What remains `OPEN` is the Aerora-specific content: which messages carry a proposal, whether
acceptance is operator-side or flight-controller-side, and the staleness threshold. The baseline
records that the firmware has GPU-message freshness wiring but that the timeout and bounds are
engineering decisions.

### Q-012 Aerora transport granularity

| Field | Value |
| --- | --- |
| Route | `PRODUCT` |
| Impact | Downgraded from `ARCH-SHAPING` to `DEFERRABLE` by Q-014 |
| Accommodated by | [communication-architecture.md](communication-architecture.md) |

Q-014's resolution makes this moot for the first product: serial links need no new link type at
all, so there is no granularity decision to take. The question only returns if a future transport
cannot be expressed as a serial port.

### Q-016 Aerora fleet vehicle identity

| Field | Value |
| --- | --- |
| Route | `HARDWARE` then `PRODUCT` |
| Impact | `ARCH-SHAPING` |
| Accommodated by | [data-model.md](data-model.md) |

A MAVLink system id is a small, locally assigned, reusable integer and is not a durable fleet
identity. The architecture defines a separate Aerora vehicle identity with a mapping from system
id, so the cloud registry, audit trail and configuration distribution all key on something stable.

The candidate source is the ATECC608A secure element recorded in the baseline schematic. Whether it
is provisioned, readable, and exposed over MAVLink is `UNKNOWN` and requires hardware and firmware
evidence.

### Q-017 Is Aerora single-stack or multi-stack?

| Field | Value |
| --- | --- |
| Route | `PRODUCT` |
| Impact | `ARCH-SHAPING` |
| Accommodated by | [vehicle-integration.md](vehicle-integration.md) |

If Aerora vehicles only ever run Aerora firmware, both stock firmware factories can be disabled,
which removes the silent factory-conflict hazard and simplifies the UI. If PX4 or ArduPilot support
is needed for development, simulation or mixed fleets, both must remain and the conflict must be
managed.

The architecture works either way; the configuration differs by two CMake options.

### Q-023 Aerora payload actuator interfaces

| Field | Value |
| --- | --- |
| Route | `HARDWARE` |
| Impact | `ARCH-SHAPING` |
| Accommodated by | [payload-architecture.md](payload-architecture.md) |

**Explicitly deferred by the baseline**, which records that the firmware model contains structural
placeholders for gimbal pan and tilt, spray on and off, spray rate, pump control, payload power and
safety interlocks, and reaction-force compensation, with actuator ranges and pin mappings pending
real datasheets.

The architecture defines the payload abstraction down to the state-name boundary and stops. Each
state needs a MAVLink representation, a commandable-versus-observed classification, and units and
bounds for its metadata — all of which require the hardware answer.

**Phase 2.1 correction.** This question was originally said to carry the Q-019 residual — whether
the payload controller will persist its own configuration. That residual is now tracked separately,
because the actuator model and state persistence are different questions with different owners: the
actuator model decides *what* state exists, persistence decides *whether it survives*. Q-023 could
be answered in full while retention remains undesigned. The residual is an evidence item in the
[hardware and firmware evidence matrix](hardware-firmware-evidence-matrix.md).

### Q-029 Aerora operator role model

| Field | Value |
| --- | --- |
| Route | `PRODUCT` then `EXTERNAL` |
| Impact | `ARCH-SHAPING` |
| Accommodated by | [security-architecture.md](security-architecture.md) |

No foundation exists in QGC. The architecture provides the identity and authorisation module
structure; the role set itself is a product decision with a likely regulatory input.

A specific trap to avoid, recorded as a requirement: the existing advanced-UI toggle is a
convenience, not authorisation, and must not be used as one.

### Q-030 Permitted cloud dependencies and degradation

| Field | Value |
| --- | --- |
| Route | `PRODUCT` |
| Impact | `ARCH-SHAPING` |
| Accommodated by | [cloud-architecture.md](cloud-architecture.md) |

The baseline requires that cloud loss never imply loss of aircraft control, and that every cloud
dependency be documented explicitly. The architecture enforces this structurally by placing the
cloud module off the command path entirely.

What remains is the per-feature degradation specification. The trap, called out in the
architecture: a feature that is technically optional but operationally required becomes a hidden
dependency.

---

## Implementation-blocking

Must be answered before Phase 5, but do not affect the architecture.

| Question | Route | Note |
| --- | --- | --- |
| Q-004 Qt licensing path | `EXTERNAL` | Reclassified as an open legal question — see [phase-2-risk-gates.md](phase-2-risk-gates.md) R-001. Architecture is licence-neutral, so this blocks distribution rather than design |
| Q-006 Remove QGC branding and artwork | `EXTERNAL` then `PRODUCT` | The code licence does not grant the marks. The overlay makes replacement straightforward; completeness must be verified |
| Q-018 Does the Aerora flight controller report a distinct autopilot identifier? | `HARDWARE` | Determines which firmware class the Aerora factory must claim, and therefore whether the Q-017 conflict arises |
| Q-020 Can an overlay shadow an existing JSON resource alias? | `CODE` | **Cheap and high value.** If yes, Aerora payload commands can appear in the Plan UI with zero core edits. Resolvable by a single experiment |
| Q-027 What consumes the embedded HTTP server component? | `CODE` | **Cheap.** Must be closed before any security claim. An unauthenticated network endpoint would be a finding |
| Q-032 Which platforms must Aerora support? | `PRODUCT` | Drives the build matrix, the signing work and the test matrix |

---

## Deferrable

Answerable during or after implementation without rework.

| Question | Route | Note |
| --- | --- | --- |
| Q-005 Third-party licence audit | `EXTERNAL` | Prerequisite for Q-004. CI already generates a dependency bill of materials |
| Q-009 Payload emergency-stop behaviour | `HARDWARE` then `PRODUCT` | Baseline records a watchdog-fault path directly into payload emergency stop. Behaviour not frozen |
| Q-010 Mandate signing by default? | `PRODUCT` | Settable through settings metadata with no core edit |
| Q-011 Link-layer security for Aerora transports | `HARDWARE` then `PRODUCT` | Depends on radio capability |
| Q-015 Worst-case concurrent link count | `PRODUCT` | Needed to validate the 15-channel budget |
| Q-021 Does the Aerora pattern need survey-style insert behaviour? | `PRODUCT` | Accepting the default keeps the pattern a zero-core-edit addition |
| Q-024 Frozen power architecture | `HARDWARE` | Baseline lists source type, isolation, converter rating, protection, connector system, grounding and interlocks as controlled items. The GCS cannot report a configuration that is not defined |
| Q-025 How does the GCS learn the tether versus battery configuration? | `HARDWARE` then `PRODUCT` | Sensed or operator-declared. The architecture requires that a mismatch between the two be surfaced |
| Q-026 Is an in-flight power transition supported? | `HARDWARE` | Baseline states seamless transfer must not be assumed |
| Q-028 Custom TLS policy | `CODE` | Phase 1 search was truncated; strongly indicated but not proven |
| Q-031 Is video required at all? | `PRODUCT` | Disabling it removes a large dependency and licence surface |
| Q-033 Air-gapped or reproducible build required? | `PRODUCT` | Dependencies are fetched at configure time; two use floating references |
| Q-034 Splash-screen requirement | `PRODUCT` | No mechanism exists in QGC; would be platform launch imagery |

---

## Priority order by architectural impact

The order in which answers would most reduce programme uncertainty.

| Rank | Question | Why it ranks here |
| --- | --- | --- |
| 1 | Q-003 Fork strategy | Blocks every commit. Cheap. Human decision only |
| 2 | Q-020 Resource alias shadowing | Cheap experiment; could eliminate the last plausible core edit |
| 3 | Q-027 HTTP server consumer | Cheap; closes a security unknown |
| 4 | Q-018 Autopilot identifier | Determines firmware plugin registration and whether Q-017 matters |
| 5 | Q-023 Payload actuator interfaces | Largest single body of deferred detail; gates the payload module |
| 6 | Q-008 Proposal versus effective command | Core baseline principle; gates the authority model's content |
| 7 | Q-016 Fleet identity | Gates cloud registry, audit and configuration distribution |
| 8 | Q-017 Single-stack or multi-stack | Two CMake options, but affects the whole UI surface |
| 9 | Q-029 Role model | Gates the security module's content |
| 10 | Q-024 Power architecture | Gates the power state model's content |
| 11 | Q-007 Failsafe | Gates flight-mode accessors and Phase 8. Cannot be accelerated |
| 12 | Q-004 Qt licensing | Blocks distribution, not design. Long lead time, so start early |

Items 1 to 4 are resolvable within days and together remove most of the remaining structural
uncertainty. Items 5, 7, 10 and 11 depend on engineering work that has not been done yet and cannot
be accelerated by analysis.

## New questions arising from Phase 2

Recorded in [open-questions.md](open-questions.md), so that this document remains a clean review of
the Phase 1 register.

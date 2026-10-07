# Phase 3 Open Questions

Nine questions raised by writing the interface contracts. Each is a question Phase 2 did not need to
ask because architecture can describe a boundary without settling what crosses it, and an interface
control document cannot.

**Phase 3 closes no question, inherited or new.** The 43 existing questions are referenced, never
redefined; the inherited open set is in
[ratification.md](../01-aerora-architecture/ratification.md) and
[open-questions.md](../01-aerora-architecture/open-questions.md).

## How these differ from the inherited questions

| Inherited questions | Phase 3 questions |
| --- | --- |
| "What is the payload actuator interface?" — content unknown | "Is the payload controller a distinct component?" — **topology** unknown |
| Mostly about values: enumerations, thresholds, identifiers | Mostly about **shape**: who produces, who relays, whether a field exists at all |
| Answering them fills in a specified interface | Answering them determines **which interface is correct** |

That difference matters for sequencing. An unanswered inherited question leaves an interface
specified and unpopulated. An unanswered Phase 3 question leaves two candidate interfaces, and the
documents above preserve both rather than picking one.

## Classification vocabulary

Carried from Phase 2 unchanged.

| Class | Meaning |
| --- | --- |
| `IMPL-BLOCKING` | Implementation of the affected interface cannot begin until it is answered |
| `ARCH-SHAPING` | The interface exists and is specified; the answer changes its content or producer |
| `DEFERRABLE` | Can be answered after implementation starts without rework |

## Summary

| ID | Subject | Class | Owner | Interfaces |
| --- | --- | --- | --- | --- |
| Q3-001 | Does the Aerora radio present as a serial device, and with what USB identity | `IMPL-BLOCKING` | Hardware | `IF-RAD-001`, `IF-RAD-003` |
| Q3-002 | Should a per-command request identifier exist on the wire | `ARCH-SHAPING` | Dialect, with Security | `IF-MAV-024` |
| Q3-003 | Is the authority mode value set closed at the three sentinels | `ARCH-SHAPING` | Firmware, with Product | `IF-AUT-001`, `IF-FC-003` |
| Q3-004 | Does the watchdog report independently of the primary flight-controller MCU | `ARCH-SHAPING` | Firmware | `IF-MAV-022`, `IF-PWR-011` |
| Q3-005 | Do autonomy proposals reach the ground station from the companion or summarised by the flight controller | `ARCH-SHAPING` | Firmware | `IF-FC-010`, `IF-GPU-002` |
| Q3-006 | Is the payload controller a distinct MAVLink component or a flight-controller partition | `ARCH-SHAPING` | Hardware, with Firmware | `IF-PAR-009`, all `IF-PAY` |
| Q3-007 | Is the high-voltage domain reported to the ground station at all | `ARCH-SHAPING` | Firmware, with Product | `IF-PWR-006` |
| Q3-008 | Is there a required bound on station-to-vehicle clock skew, and is the skew recorded | `DEFERRABLE` | Firmware, with Operations | `IF-LOG-021` |
| Q3-009 | Does the vehicle report capability presence separately from capability availability | `IMPL-BLOCKING` | Firmware | `IF-UIS-005`, `IF-UIS-006` |

Two are `IMPL-BLOCKING`, six are `ARCH-SHAPING`, one is `DEFERRABLE`. Neither blocking question is
blocked on an Aerora decision — both need a hardware or firmware fact.

---

### Q3-001

**Question.** Does the Aerora primary radio present to the host operating system as a standard
serial device, and if so what USB vendor and product identifiers does it present?

**Why it is being asked.** ADR-002 chose a serial-first transport strategy and deferred six
transport-layer core edits on the strength of it. Phase 1 confirmed that QGroundControl's serial
path works unmodified for any device that presents as a serial port, which is what makes the
zero-core-edit claim hold. If the Aerora radio does not present that way, ADR-002 is invalid and the
six deferred edits return.

The identifier half matters separately. QGroundControl carries a USB board-identification table and
auto-connect behaviour keyed on vendor and product identifiers. A colliding identity causes the
station to misidentify the radio — the specific case observed in Phase 1 was a common USB-to-serial
bridge being treated as a telemetry radio.

**Affected interfaces.** `IF-RAD-001` (serial device presentation), `IF-RAD-003` (USB identity).
Transitively every interface that reaches the vehicle, because they all travel over `IF-SYS-002`.

**Affected requirements.** `AERORA-COMM-REQ-006`, `AERORA-COMM-REQ-011`, `AERORA-COMM-REQ-012`.

**Affected risks.** R-003 — transport extensibility requires core-tree modification.

**Classification.** `IMPL-BLOCKING`. Not because the ground station cannot be built without the
answer, but because the architecture's central cost claim rests on it.

**Decision owner.** Hardware, for the radio module selection and USB descriptor configuration.

**Evidence required.** A bench observation: attach the radio to a host and record the enumerated
device class and the reported vendor and product identifiers. This is the cheapest high-value
evidence item in the programme — one connection, one observation, and ADR-002 is either confirmed or
withdrawn. Phase 2.1 evidence matrix items 6 and 8 cover it.

**Mitigation already in place, independent of the answer.** Aerora disables the serial
auto-connect behaviour that performs the misidentification in its settings defaults. That is
entirely ground-station-side and is the designated primary mitigation, because the non-colliding
identifier may not be controllable.

---

### Q3-002

**Question.** Should the Aerora dialect carry a per-command request identifier, echoed in the
acknowledgement, so that two identical commands can be distinguished on the wire?

**Why it is being asked.** MAVLink acknowledgements are matched by component and command.
`GCS-VERIFIED`. There is no unique per-request identifier, so two sequential identical commands are
indistinguishable in their acknowledgements. `AERORA-GCS-REQ-018` works around this by assigning a
ground-station request identity, which is sufficient for the audit record and **not** sufficient to
know which of two outstanding commands an acknowledgement answers.

Phase 3 does not assume the answer is yes. The workaround is adequate for accountability, and adding
a wire field to solve a correlation problem the ground station has already solved locally may not be
worth a dialect change.

**Affected interfaces.** `IF-MAV-024` (the named gap), `IF-AUT-007`, `IF-AUT-008`, `IF-LOG-005`,
`IF-LOG-022`.

**Affected requirements.** `AERORA-GCS-REQ-018`, `AERORA-COMPLY-REQ-002`.

**Affected risks.** R-025 — acknowledgement correlation cannot distinguish commands.

**Classification.** `ARCH-SHAPING`. The record is correct either way; the question is whether the
ground station can resolve an ambiguous acknowledgement or only record that it was ambiguous.

**Decision owner.** Dialect owner, with Security as the consumer of the audit consequence.

**Evidence required.** None external. This is an Aerora design decision, and the input it needs is
an operational one: does any investigation scenario require resolving which of two identical
commands was acknowledged, or is recording the ambiguity sufficient?

---

### Q3-003

**Question.** Is the authority mode value set closed at the three evidenced sentinels, or are
additional modes expected?

**Why it is being asked.** The firmware model carries three authority mode sentinels with a
documented dominance relationship. Nothing states whether that is the complete set. The baseline
also flags the identity of the safe or default mode as requiring confirmation, which implies at
least the possibility of a fourth value.

Phase 3 specifies the interface against the three evidenced values and **does not close the set**.
The metadata-driven enumeration means an additional value can be added without touching display,
log or audit code, so the cost of being wrong in the open direction is low. The cost of closing the
set and being wrong is a mode the operator sees as unrecognised.

**Affected interfaces.** `IF-AUT-001`, `IF-AUT-002`, `IF-FC-003`, `IF-FC-020`, `IF-LOG-006`,
`IF-LOG-007`.

**Affected requirements.** `AERORA-SAFE-REQ-006`, `AERORA-GCS-REQ-026`.

**Affected risks.** R-028 — authority mode and flight mode may be conflated. A larger mode set makes
the conflation more likely, not less.

**Classification.** `ARCH-SHAPING`.

**Decision owner.** Firmware, with Product for the operational mode set.

**Evidence required.** A firmware statement of the complete authority mode enumeration, or an
explicit statement that it is open-ended. Either answer is usable; the absence of both is what
forces the unrecognised-value handling to exist.

---

### Q3-004

**Question.** Does the safety and watchdog MCU report its state to the ground station
independently, or does its reporting pass through the primary flight-controller MCU?

**Why it is being asked.** The aircraft carries a separate safety MCU that can initiate disarm and
payload emergency stop without the primary MCU's involvement. That independence is the point of the
design. If its **reporting** path is not equally independent, then a primary-MCU failure removes the
ground station's visibility of the component that is still working.

The consequence is specific and bad: the source the ground station records would be the relay rather
than the originator, and an operator reading the audit trail would draw the wrong conclusion about
what was still alive at the moment of the event.

Phase 3 adopts neither position. The affected rows name the producer in a form that is correct under
either reading.

**Affected interfaces.** `IF-MAV-022` (reporting which component originated a relayed value),
`IF-PWR-011` (watchdog event), `IF-FC-012`, `IF-TLM-010`, `IF-LOG-013`, `IF-AUT-010`.

**Affected requirements.** `AERORA-GCS-REQ-027`, `AERORA-COMPLY-REQ-004`, `AERORA-SAFE-REQ-011`,
`AERORA-POWER-REQ-007`.

**Affected risks.** None existing. The specification risk is recorded as
[R3-003](21-phase-3-risks.md).

**Classification.** `ARCH-SHAPING`. The reporting-source field exists either way; what changes is
whether it can be trusted to mean the originator.

**Decision owner.** Firmware.

**Evidence required.** The firmware's MAVLink routing design, or a bench observation with the
primary MCU held in reset. The second is the stronger evidence and is the only one that
distinguishes a design intent from a behaviour. Phase 2.1 evidence matrix items 2 and 20 cover the
component; neither covers its reporting path.

---

### Q3-005

**Question.** Do companion-originated autonomy proposals reach the ground station directly, under
the companion's own component identifier, or are they summarised and re-emitted by the flight
controller?

**Why it is being asked.** The two readings produce different interfaces. A direct path means the
companion is a MAVLink component the ground station receives from, and freshness is stamped against
the companion's own reporting. A summarised path means the flight controller is the producer, the
companion's output is an input to it, and the ground station cannot see companion staleness at all —
only the staleness of the flight controller's summary.

`AERORA-GPU-REQ-006` requires every companion-sourced value to carry a receive time and be marked
stale past its class threshold. Under the summarised reading, that requirement is satisfied against
the wrong clock unless the summary carries the companion's own timestamp.

Phase 3 keeps `IF-FC-010` and `IF-GPU-002` as **two separate interfaces** rather than collapsing
them, precisely because it does not know which exists.

**Affected interfaces.** `IF-FC-010`, `IF-FC-011`, `IF-GPU-001` through `IF-GPU-005`, `IF-TLM-007`,
`IF-UIS-003`.

**Affected requirements.** `AERORA-GPU-REQ-001`, `AERORA-GPU-REQ-002`, `AERORA-GPU-REQ-004`,
`AERORA-GPU-REQ-006`, `AERORA-GPU-REQ-007`.

**Affected risks.** R-014 — overlay maintenance burden, through the volume of autonomy presentation
that depends on the answer.

**Classification.** `ARCH-SHAPING`.

**Decision owner.** Firmware.

**Evidence required.** The flight-controller to companion interface design, including whether
companion traffic is bridged onto the radio link or terminated at the flight controller. Phase 2.1
evidence matrix items 9 and 21 cover the topology; neither covers the message path.

---

### Q3-006

**Question.** Is the payload controller a distinct MAVLink component with its own component
identifier, or a partition within the flight-controller firmware?

**Why it is being asked.** Baseline documents 10 and 13 describe a payload controller as a distinct
authority with its own local interlocks, and the firmware model contains payload elements. Neither
establishes whether it is a distinct **device** or a distinct **responsibility** inside one device.

The interface consequences are real. A distinct component has its own component identifier, its own
heartbeat, its own parameter space and its own failure mode in which it stops reporting while the
flight controller continues. A firmware partition has none of those: its silence is the flight
controller's silence.

There is a further consequence for parameters. QGroundControl's parameter handling assumes the
autopilot is component 1, verified in both the parameter manager and the fact panel controller.
Parameters on another component are second-class throughout, which is why
[12-parameter-icd.md](12-parameter-icd.md) prefers that payload configuration **not** be a MAVLink
parameter — a preference that only makes sense if the payload controller might be a separate
component.

Phase 3 names the producer as "payload controller via flight controller" in every payload row, which
is correct under either reading and commits to neither.

**Affected interfaces.** `IF-PAR-009`, and the whole `IF-PAY` set. Also `IF-FC-012` and `IF-FC-021`.

**Affected requirements.** `AERORA-PAYLOAD-REQ-006`, `AERORA-PAYLOAD-REQ-010`,
`AERORA-PAYLOAD-REQ-013`, `AERORA-GCS-REQ-012`.

**Affected risks.** R-002 — payload state commands silently dropped from a resume mission. The
answer affects where retained state could live.

**Classification.** `ARCH-SHAPING`.

**Decision owner.** Hardware, for whether a separate controller exists, with Firmware for its
MAVLink presentation.

**Evidence required.** A PCB or block-diagram artifact showing the payload controller as a separate
device, plus the firmware's component-identifier allocation. Phase 2.1 evidence matrix item 13
records the controller as `BASELINE-ASSERTED` with `PROBABLE` confidence; this question asks for the
artifact that would make it `ARTIFACT-OBSERVED`.

---

### Q3-007

**Question.** Are the high-voltage tether domain's voltage and current reported to the ground
station at all, and if so over what range?

**Why it is being asked.** Two sources point in different directions. The power architecture shows a
ground-supplied high-voltage domain converted onboard, so the quantities exist. Baseline document 13
explicitly permits the ground station to display validated power-system status and faults, so
displaying them is allowed. But nothing states that the high-voltage side is **reported** rather
than only monitored onboard, and `IF-PWR-006`'s range is `UNKNOWN` partly for this reason.

This is not a display preference. If the high-voltage quantities are not reported, the ground
station's tether presentation is limited to a state enumeration and a fault indication, and an
operator cannot see a degrading supply before it becomes a fault.

**Affected interfaces.** `IF-PWR-006`, `IF-PWR-009`, `IF-PWR-010`, `IF-FC-006`, `IF-TLM-005`.

**Affected requirements.** `AERORA-POWER-REQ-007`, `AERORA-POWER-REQ-008`, `AERORA-GCS-REQ-029`.

**Affected risks.** None existing.

**Classification.** `ARCH-SHAPING`.

**Decision owner.** Firmware, for what is reported; Product, for what an operator needs.

**Evidence required.** A firmware telemetry definition for the power domain, or a statement that the
high-voltage side is monitored but not reported. Phase 2.1 evidence matrix items 17 and 18 cover the
architecture and the transition behaviour; neither covers reporting scope.

**What Phase 3 does in the meantime.** `IF-PWR-006` is specified with an `UNKNOWN` range, and the
ground station presents unknown as unknown. It does not estimate, derive from the 48 V domain, or
infer the supply state from the converter state.

---

### Q3-008

**Question.** Is there a required bound on the skew between the ground station's clock and the
vehicle's, and is the observed skew itself recorded?

**Why it is being asked.** Audit records carry station local time, by design, because a locally
written record with a local timestamp is evidence independent of any service. Telemetry values carry
a receive time stamped at parse, also station time. Both are internally consistent.

The gap appears when either must be correlated against vehicle-reported time — a mission index, a
failsafe event, an onboard log. Nothing in the baseline sets a bound on the skew or requires that it
be recorded, so a post-flight reconstruction that interleaves a station audit record with an onboard
log has no stated basis for aligning them.

**Affected interfaces.** `IF-LOG-021` (record timestamp basis), `IF-LOG-013`, `IF-LOG-022`,
`IF-TLM-010`.

**Affected requirements.** `AERORA-COMPLY-REQ-004`, `AERORA-GCS-REQ-027`.

**Affected risks.** None existing.

**Classification.** `DEFERRABLE`. The monotonic-plus-wall-clock contract at `IF-LOG-021` keeps
station-side ordering correct regardless of the answer, so this can be settled after implementation
without rework. It cannot be settled after the first investigation.

**Decision owner.** Firmware, for whether the vehicle reports a usable time base; Operations, for
whether cross-source correlation is required.

**Evidence required.** Whether the vehicle reports a time base at all, and whether it is
GNSS-derived. A GNSS-derived vehicle clock would make the question largely moot; a free-running one
would make recording the observed skew the only workable answer.

---

### Q3-009

**Question.** Does the vehicle report capability **presence** separately from capability
**availability**?

**Why it is being asked.** `AERORA-UI-REQ-008` requires an action the aircraft cannot perform to be
hidden, and an action it cannot perform in its current state to be disabled with a stated reason.
Choosing between those two presentations requires knowing which of two different facts is true:
the capability does not exist on this aircraft, or it exists and is currently unusable.

The capability inventory is `NOT-EVIDENCED` for its content, so whether it carries one bit or two
per capability is unknown. Without the distinction, every absent capability has to be presented the
same way, and the only defensible choice is the conservative one — which loses the actionable reason
a disabled control would have carried.

**Affected interfaces.** `IF-UIS-005` (unknown and unavailable presentation), `IF-UIS-006` (action
availability), `IF-FC-017`, `IF-PAY-002`, `IF-GPU-007`, `IF-PWR-012`, `IF-MAV-026`.

**Affected requirements.** `AERORA-UI-REQ-008`, `AERORA-MAV-REQ-009`, `AERORA-GCS-REQ-013`,
`AERORA-PAYLOAD-REQ-013`.

**Affected risks.** None existing. The specification risk is recorded as
[R3-002](21-phase-3-risks.md).

**Classification.** `IMPL-BLOCKING` for the Aerora capability-dependent interface, not for the
ground station as a whole. Standard MAVLink capabilities are unaffected, and the first increment
degrades to the standard capability set.

**Decision owner.** Firmware.

**Evidence required.** The capability inventory's field definition. One field decision answers it,
and the answer determines whether [17-capability-model.md](17-capability-model.md) can be populated
for Aerora capabilities or only structurally correct.

---

## What these questions do not do

| They do not | Because |
| --- | --- |
| Close or re-open any inherited question | Phase 3 references, never redefines |
| Introduce a requirement to cover the gap | Phase 3 defines no requirement identifiers |
| Pick a position where sources conflict | The conflict rule: record it, preserve both, name the authority |
| Block the first build increment | Every affected interface has a stated fallback |

The fallback property is worth checking rather than asserting. For each question, what the ground
station does while it is unanswered:

| Question | Behaviour while unanswered |
| --- | --- |
| Q3-001 | Serial path as specified; auto-connect misidentification disabled by settings default |
| Q3-002 | Ground-station request identity in the record; wire ambiguity recorded as ambiguous |
| Q3-003 | Three evidenced sentinels; an unrecognised value displays as unrecognised |
| Q3-004 | Reporting source recorded as the **sending** component, never presented as the originator |
| Q3-005 | Two separate interfaces retained; companion staleness shown only where reported |
| Q3-006 | Producer named as "payload controller via flight controller"; payload configuration not a parameter |
| Q3-007 | `IF-PWR-006` range `UNKNOWN`; unknown presented as unknown, never estimated |
| Q3-008 | Monotonic plus wall-clock ordering; clock adjustment recorded as an event |
| Q3-009 | Absent capability hidden; the actionable reason is simply not available |

Nine questions, nine defined fallbacks. None of them is a guess dressed as a default.

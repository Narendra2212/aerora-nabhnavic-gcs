# Hardware and Firmware to GCS Architecture Evidence Matrix

What the ground station architecture assumes about the aircraft, what evidence exists for each
assumption, and what remains to be verified.

This document exists because Phase 2 made several statements about vehicle behaviour in the
indicative mood — "the payload controller retains state", "the premise is false for Aerora" — that
were derivations rather than findings. Phase 2.1 separates the two. Every row below says what the
architecture needs, what is actually known, and the gap between them.

## Why this matters

The ground station cannot verify vehicle behaviour. It can only state what it depends on. An
architecture that records its vehicle-side dependencies as facts will eventually be wrong in a way
nobody notices until integration; one that records them as dependencies with evidence status is
wrong in a way that shows up on a review page.

Baseline document 13 sets the standard this matrix follows: where the firmware model contains a
placeholder, the item is unresolved engineering work rather than a verified capability. Baseline 49
applies the same discipline to power, stating plainly that the schematic "does not, by itself,
prove" converter topology, source arbitration or transition behaviour.

## Vocabulary

### Evidence source

| Code | Meaning |
| --- | --- |
| `SCHEMATIC` | The uploaded flight-controller schematic, as recorded in baseline 46 |
| `PCB` | The uploaded PCB artifact, as recorded in baseline 46 |
| `MODEL` | The Simulink flight-controller model, as recorded in baseline 46 |
| `BASELINE-nn` | Aerora product baseline document `nn` |
| `QGC-SOURCE` | QGroundControl source, read directly in Phase 1 or Phase 2 |
| `NONE` | No artifact and no baseline statement observed |

### Evidence status

| Status | Meaning |
| --- | --- |
| `ARTIFACT-OBSERVED` | A named artifact shows the item exists |
| `BASELINE-ASSERTED` | A baseline document states it; no artifact is cited for it |
| `PLACEHOLDER` | Present in the firmware model and explicitly marked provisional |
| `SPECIFIED-NOT-VERIFIED` | Defined in a document, not bench or flight verified |
| `NOT-EVIDENCED` | Neither an artifact nor a baseline statement exists |
| `GCS-VERIFIED` | Verified by reading QGroundControl source — applies to ground-station behaviour only |
| `GCS-DECIDED` | Within Aerora ground-station control and decided in the Phase 2 set |

### Confidence

| Level | Meaning |
| --- | --- |
| `CONFIRMED` | Established by evidence sufficient to design against |
| `PROBABLE` | Strongly indicated but not established; design against it with a fallback |
| `ASSUMED` | The architecture assumes it with no supporting evidence |
| `UNKNOWN` | Not determinable from available artifacts |

`ARTIFACT-OBSERVED` does not imply `CONFIRMED`. A part on a schematic is evidence that the part was
selected, not that its interface, driver, calibration or behaviour is defined. Several rows below
are `ARTIFACT-OBSERVED` at `PROBABLE` for exactly that reason.

## Summary

| # | Item | Evidence status | Confidence |
| --- | --- | --- | --- |
| 1 | Flight controller MCU | `ARTIFACT-OBSERVED` | `CONFIRMED` |
| 2 | Safety and security MCU | `ARTIFACT-OBSERVED` | `PROBABLE` |
| 3 | Inertial measurement units | `ARTIFACT-OBSERVED` | `PROBABLE` |
| 4 | Barometers | `ARTIFACT-OBSERVED` | `PROBABLE` |
| 5 | GPS | `ARTIFACT-OBSERVED` | `UNKNOWN` |
| 6 | LoRa radio | `ARTIFACT-OBSERVED` | `PROBABLE` |
| 7 | Secondary radios: FHSS24 and RF433 | `MODEL` only | `UNKNOWN` |
| 8 | Radio USB identity | `NOT-EVIDENCED` | `ASSUMED` |
| 9 | Flight controller to companion interface | `ARTIFACT-OBSERVED` | `UNKNOWN` |
| 10 | MAVLink interface and dialect | `BASELINE-ASSERTED` | `ASSUMED` |
| 11 | Aerora autopilot identifier | `NOT-EVIDENCED` | `UNKNOWN` |
| 12 | Telemetry interfaces | `BASELINE-ASSERTED` | `PROBABLE` |
| 13 | Payload controller | `BASELINE-ASSERTED` | `PROBABLE` |
| 14 | **Payload state retention across interruption** | **`NOT-EVIDENCED`** | **`ASSUMED`** |
| 15 | Payload actuator interfaces | `PLACEHOLDER` | `UNKNOWN` |
| 16 | Payload emergency stop | `MODEL` | `PROBABLE` |
| 17 | Power architecture | `ARTIFACT-OBSERVED` | `PROBABLE` |
| 18 | Power transition behaviour | `NOT-EVIDENCED` | `UNKNOWN` |
| 19 | Failsafe behaviour | `PLACEHOLDER` | `UNKNOWN` |
| 20 | Watchdog behaviour | `MODEL` | `PROBABLE` |
| 21 | Companion and autonomy authority | `MODEL` | `PROBABLE` |
| 22 | Fleet identity and secure element | `ARTIFACT-OBSERVED` | `UNKNOWN` |

| Confidence | Count |
| --- | --- |
| `CONFIRMED` | 1 |
| `PROBABLE` | 10 |
| `ASSUMED` | 3 |
| `UNKNOWN` | 8 |
| Total | 22 |

**One item of twenty-two is confirmed.** That is the honest state of the vehicle-side inputs to this
architecture, and it is why the architecture is built from capability reporting and freshness rather
than from fixed assumptions about the aircraft.

The three `ASSUMED` rows are the dangerous ones, because an assumption with no evidence behind it
reads exactly like a fact in a design document. They are items 8, 10 and 14.

## 1. Flight controller MCU

| Field | Value |
| --- | --- |
| Requirement | The GCS shall not depend on the flight-controller MCU part |
| Architecture assumption | A single primary flight-controller MCU runs the Aerora flight stack |
| Evidence source | `SCHEMATIC`, `MODEL`, `BASELINE-50` |
| Evidence status | `ARTIFACT-OBSERVED` — `STM32H743VITx`, present in the schematic and the model's target device |
| Confidence | `CONFIRMED` |
| Unresolved question | None affecting the GCS |
| Affected requirements | `AERORA-GCS-REQ-012` |
| Affected risks | None |

The part is confirmed and the GCS does not care what it is. Recorded so that no future document
starts depending on it.

## 2. Safety and security MCU

| Field | Value |
| --- | --- |
| Requirement | The GCS shall present watchdog and safety state as reported, attributed to its source |
| Architecture assumption | A second MCU holds the watchdog and some safety or security partition |
| Evidence source | `SCHEMATIC`, `BASELINE-50` |
| Evidence status | `ARTIFACT-OBSERVED` — `STM32G071CBTx`, associated with the watchdog interface |
| Confidence | `PROBABLE` — the part is present; baseline 50 states its exact safety and security partition "must be formally defined" |
| Unresolved question | Which functions sit on the safety MCU, and what it reports independently of the primary MCU |
| Affected requirements | `AERORA-SAFE-REQ-010`, `AERORA-SAFE-REQ-011`, `AERORA-GCS-REQ-027` |
| Affected risks | R-004 |

This matters to the GCS more than it looks. `AERORA-GCS-REQ-027` requires every value to carry its
reporting source. If the watchdog reports through the primary MCU rather than independently, the
"source" the GCS records is the relay, not the originator — and an operator reading the audit trail
would draw the wrong conclusion about what was still alive.

## 3. Inertial measurement units

| Field | Value |
| --- | --- |
| Requirement | The GCS shall present sensor health and estimator state as reported capabilities |
| Architecture assumption | Two IMUs exist; redundancy state may be reported |
| Evidence source | `SCHEMATIC`, `BASELINE-50` |
| Evidence status | `ARTIFACT-OBSERVED` — `ICM-42688-P` and `ICM-20948` |
| Confidence | `PROBABLE` — parts observed; no interface, calibration or redundancy-arbitration definition |
| Unresolved question | Whether per-IMU health is reported separately, and whether the GCS should present redundancy state |
| Affected requirements | `AERORA-MAV-REQ-009`, `AERORA-GCS-REQ-013` |
| Affected risks | None |

Two IMUs of different classes suggests a primary and a cross-check rather than a symmetric pair, but
that is inference and is recorded as such.

## 4. Barometers

| Field | Value |
| --- | --- |
| Requirement | As item 3 |
| Architecture assumption | Two barometers exist |
| Evidence source | `SCHEMATIC`, `BASELINE-50` |
| Evidence status | `ARTIFACT-OBSERVED` — two `BMP390L` references |
| Confidence | `PROBABLE` — baseline 50 says "two BMP390L references", which is weaker than two fitted devices |
| Unresolved question | Whether both are populated, and whether disagreement between them is reported |
| Affected requirements | `AERORA-MAV-REQ-009` |
| Affected risks | None |

"Two references" is schematic language. It may mean two devices or one device referenced twice. Not
a GCS problem, but not a confirmed redundancy claim either.

## 5. GPS

| Field | Value |
| --- | --- |
| Requirement | The GCS shall present GPS state through existing QGroundControl handling |
| Architecture assumption | A GPS is present and reports through standard MAVLink |
| Evidence source | `SCHEMATIC`, `BASELINE-50` |
| Evidence status | `ARTIFACT-OBSERVED` — a "GPS interface" with **no part identified** |
| Confidence | `UNKNOWN` — an interface is not a receiver |
| Unresolved question | Which receiver, which constellations, whether RTK is in scope |
| Affected requirements | None specific to Aerora; QGroundControl handles standard GPS telemetry |
| Affected risks | None |

RTK is the only part of this that would reach the architecture, because QGroundControl's RTK support
has its own configuration surface. Nothing indicates RTK is in scope, and nothing excludes it.

## 6. LoRa radio

| Field | Value |
| --- | --- |
| Requirement | Each Aerora radio shall connect as a QGroundControl serial link with no core modification |
| Architecture assumption | The radio presents a standard serial interface carrying MAVLink |
| Evidence source | `SCHEMATIC`, `BASELINE-50`, `BASELINE-49` |
| Evidence status | `ARTIFACT-OBSERVED` — `E22-900M22S` module |
| Confidence | `PROBABLE` — the module is present; its host-side presentation is not evidenced |
| Unresolved question | Whether the ground-side radio presents as a USB serial device, at what baud, and with what framing |
| Affected requirements | `AERORA-COMM-REQ-006`, `AERORA-COMM-REQ-011`, `AERORA-COMM-REQ-012` |
| Affected risks | R-003 |

ADR-002's entire transport strategy rests on this row. The serial path in QGroundControl was verified
to impose no device-identity gate — that part is `GCS-VERIFIED`. What is not verified is that the
Aerora ground radio presents as a serial device at all. If it presents as a network interface or a
custom USB class, ADR-002 needs revisiting and the six deferred transport core edits return.

**This is the single most load-bearing unverified row in the matrix after item 14.**

## 7. Secondary radios: FHSS24 and RF433

| Field | Value |
| --- | --- |
| Requirement | The GCS shall display the vehicle-reported active link and all-stale condition |
| Architecture assumption | Up to three communication paths exist with vehicle-side arbitration |
| Evidence source | `MODEL`, `BASELINE-49` |
| Evidence status | `MODEL` only — the firmware model contains three paths with a documented priority order and an all-stale state. **No hardware artifact observed for FHSS24 or RF433** |
| Confidence | `UNKNOWN` — firmware structure exists; hardware does not appear in the schematic evidence |
| Unresolved question | Q-013 — whether the hardware exists at all, and at what maturity |
| Affected requirements | `AERORA-COMM-REQ-007`, `AERORA-COMM-REQ-008`, `AERORA-COMM-REQ-009` |
| Affected risks | R-003 |

Phase 2 resolved Q-013 as a **scope decision** rather than a hardware finding: the GCS represents
whatever links the vehicle reports, so the architecture does not depend on how many exist. That
remains correct. This row records that the firmware's three-path structure is not matched by
observed hardware for two of the three.

## 8. Radio USB identity

| Field | Value |
| --- | --- |
| Requirement | Aerora radio hardware shall use vendor and product identifiers that do not collide with QGroundControl's board-identification entries |
| Architecture assumption | Aerora controls the ground radio's USB identity and will choose non-colliding values |
| Evidence source | `NONE` for the Aerora side; `QGC-SOURCE` for the collision |
| Evidence status | `NOT-EVIDENCED` — no Aerora decision recorded |
| Confidence | `ASSUMED` |
| Unresolved question | What USB bridge the ground radio uses, and what identifiers it presents |
| Affected requirements | `AERORA-COMM-REQ-011`, `AERORA-COMM-REQ-012` |
| Affected risks | R-003 |

The collision itself is `GCS-VERIFIED`: QGroundControl's board table maps the stock Silicon Labs
CP2102 identifiers, and an FTDI description pattern, to "SiK Radio". A radio on either bridge is
auto-connected at a fixed baud and takes the port reservation, so the operator's own link then
fails to open — with the cause three subsystems away from the symptom.

Two independent mitigations are specified. One is a hardware requirement Aerora may not control if
it buys an off-the-shelf module; the other, disabling SiK auto-connect in the Aerora settings
defaults, is entirely within the GCS and should be treated as the primary mitigation for that
reason.

## 9. Flight controller to companion interface

| Field | Value |
| --- | --- |
| Requirement | The GCS shall implement the GCS-to-FC interface only and shall not emit FC-to-companion messages |
| Architecture assumption | The companion connects to the flight controller over a serial link, not to the GCS |
| Evidence source | `SCHEMATIC`, `MODEL`, `BASELINE-50` |
| Evidence status | `ARTIFACT-OBSERVED` — companion UART plus power and I/O; the model receives autonomy commands through a MAVLink subsystem |
| Confidence | `UNKNOWN` — baseline 50 states the module, protocol configuration, baud rate and connector assignment "must be frozen in the interface control document before implementation" |
| Unresolved question | The companion module and its interface parameters |
| Affected requirements | `AERORA-MAV-REQ-006`, `AERORA-MAV-REQ-007`, `AERORA-GPU-REQ-001`, `AERORA-GPU-REQ-002` |
| Affected risks | R-014 |

The architecture depends on the **topology**, not the parameters: the companion is reached through
the flight controller, so the GCS has no direct path to it and cannot command it even if a later
change wanted it to. The topology is observed. The parameters are open and do not affect the GCS.

## 10. MAVLink interface and dialect

| Field | Value |
| --- | --- |
| Requirement | The Aerora dialect shall be maintained in an Aerora-controlled fork pinned to an immutable commit |
| Architecture assumption | The vehicle speaks MAVLink 2 with an Aerora dialect carrying the named message families |
| Evidence source | `BASELINE-07`, `MODEL` |
| Evidence status | `BASELINE-ASSERTED` — baseline 07 names nine candidate message families. **No dialect file exists** |
| Confidence | `ASSUMED` — the message families are candidates, not definitions |
| Unresolved question | Every field of every Aerora message; the interface control document does not exist |
| Affected requirements | `AERORA-MAV-REQ-005` through `AERORA-MAV-REQ-010`, and every requirement that depends on a vehicle-reported value |
| Affected risks | R-011 |

This row underpins a large fraction of the register. Most Aerora requirements of the form "the GCS
shall display the vehicle-reported X" depend on a message carrying X existing. None does yet.

That is not a defect in the architecture — the dialect is downstream work and the extension
mechanism is `GCS-VERIFIED` — but it does mean the architecture's **interface obligations on the
vehicle are the largest single body of unevidenced dependency in this matrix**. The capability
inventory in `AERORA-MAV-REQ-009` is the mechanism that makes this survivable: a GCS that adapts to
a reported capability set degrades gracefully when a message family is absent.

## 11. Aerora autopilot identifier

| Field | Value |
| --- | --- |
| Requirement | The Aerora firmware abstraction shall be selected deterministically for Aerora vehicles |
| Architecture assumption | The Aerora flight controller reports a `MAV_AUTOPILOT` value that is neither PX4 nor ArduPilot |
| Evidence source | `NONE` |
| Evidence status | `NOT-EVIDENCED` |
| Confidence | `UNKNOWN` |
| Unresolved question | Q-018 — what value the flight controller reports |
| Affected requirements | `AERORA-GCS-REQ-007`, `AERORA-GCS-REQ-008` |
| Affected risks | R-010 |

The architecture is insulated from the answer: any value that is not PX4 or ArduPilot maps to the
generic firmware class, which no upstream factory claims, so the Aerora factory wins unambiguously.
What the answer changes is only whether Aerora needs to distinguish itself from other generic
autopilots in a multi-stack build.

**Remains explicitly OPEN** per the Phase 2.1 directive.

## 12. Telemetry interfaces

| Field | Value |
| --- | --- |
| Requirement | Every Aerora value shall carry its value, receive time and reporting source |
| Architecture assumption | The vehicle reports the state the GCS must display, at a usable rate |
| Evidence source | `BASELINE-07`, `BASELINE-11`, `MODEL` |
| Evidence status | `BASELINE-ASSERTED` — baselines 07 and 11 enumerate the state categories to report, including payload state, watchdog state, mission state and power-source state |
| Confidence | `PROBABLE` — the obligation is stated; rates, fields and units are not |
| Unresolved question | Message rates against the main-thread Fact budget; which values are reported at all |
| Affected requirements | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-027`, `AERORA-COMPLY-REQ-004` |
| Affected risks | R-013 |

The freshness architecture needs receive time, which the GCS stamps itself at parse, so that part is
`GCS-DECIDED` and safe. Reporting **source** needs the message to identify its originator, which is
a dialect obligation and is not yet designed. See item 2 for why that matters.

## 13. Payload controller

| Field | Value |
| --- | --- |
| Requirement | The payload controller shall be authoritative for payload state; the GCS shall not maintain an independent payload state model |
| Architecture assumption | A payload controller exists as a distinct authority from the flight controller |
| Evidence source | `BASELINE-10`, `BASELINE-13`, `MODEL` |
| Evidence status | `BASELINE-ASSERTED` — baseline 13 assigns payload-local safety to it; the model contains `Payload_Interface`, `Painting_System` and `Payload_Actuator_Drivers` |
| Confidence | `PROBABLE` — the ownership assignment is explicit; whether it is a separate physical controller or a partition of the flight controller is not established |
| Unresolved question | Whether the payload controller is a distinct device, and what it reports independently |
| Affected requirements | `AERORA-PAYLOAD-REQ-006`, `AERORA-PAYLOAD-REQ-009`, `AERORA-PAYLOAD-REQ-010` |
| Affected risks | R-002 |

The ownership statement is the strongest vehicle-side support in this matrix for a GCS prohibition:
because the payload controller owns payload state, the GCS modelling it independently would be
wrong regardless of anything else. That prohibition is safe to ratify.

## 14. Payload state retention across a mission interruption

| Field | Value |
| --- | --- |
| Requirement | The payload controller shall retain payload state across a mission interruption so that mission resume requires no GCS-side command replay |
| Architecture assumption | Payload state survives a mission replacement, so the resume replay prefix has nothing Aerora-specific to carry |
| Evidence source | **`NONE`** |
| Evidence status | **`NOT-EVIDENCED`** |
| Confidence | **`ASSUMED`** |
| Unresolved question | Whether retention will be designed at all; what state is retained; across which events |
| Affected requirements | `AERORA-PAYLOAD-REQ-007`, `AERORA-MISSION-REQ-003`, `AERORA-MAV-REQ-008` |
| Affected risks | **R-002 — `G-ARCH`** |

### Why this row is separated from item 13

Phase 2 treated ownership and retention as one property and concluded that QGroundControl's
assumption was "false for Aerora". Checking that:

| Checked | Result |
| --- | --- |
| Baseline 10 statement on retention, persistence, non-volatile storage or survival across resume | **None.** Searched; no match |
| Baseline 13 statement | Assigns payload-local safety and interlocks — an ownership statement |
| Baselines 07 and 11 | Require payload state to be **reported**; say nothing about retaining it |
| Firmware model | Payload elements present; actuator behaviours are placeholders |
| Bench or flight evidence | None observed |

**Ownership does not imply retention.** A payload controller can own its state and still lose it on
a reset, a power cycle, a mode change or a mission replacement. Retention is a firmware design
decision that has not been made.

### Consequence

R-002 is **conditionally** mitigated, not mitigated. Until evidence exists:

| Mitigation | Status |
| --- | --- |
| Reject QGC core modification of the resume filter | **Unchanged — rejected** |
| Reject GCS-side payload state replay | **Unchanged — rejected** |
| Payload-controller retention | Preferred, unproven |
| Constrain Aerora plans so payload-state commands do not precede a plausible resume point | GCS-side fallback, adopted |
| Warn the operator at resume time that state set before the resume point is not re-established | GCS-side, adopted regardless |

### Verification required

| Step | Owner |
| --- | --- |
| Decide whether the payload controller will retain state, and enumerate what is retained | Payload controller firmware |
| Specify the events across which retention holds — mission replacement, reset, power cycle, mode change | Payload controller firmware |
| Bench-verify on real hardware: set payload state, interrupt the mission, resume, confirm the state | Firmware and integration |
| Record the result against `AERORA-PAYLOAD-REQ-007` | Engineering |

Until the third step produces a result, the GCS fallback carries the mitigation.

## 15. Payload actuator interfaces

| Field | Value |
| --- | --- |
| Requirement | Payload controls shall be presented according to the reported payload capability set |
| Architecture assumption | Payload state is an enumerated Fact whose members come from metadata |
| Evidence source | `MODEL`, `BASELINE-10` |
| Evidence status | `PLACEHOLDER` — the model contains placeholders for gimbal pan and tilt, spray on and off, spray rate, pump control, payload power and safety interlocks, and reaction-force compensation |
| Confidence | `UNKNOWN` |
| Unresolved question | Q-023 — the actuator model |
| Affected requirements | `AERORA-PAYLOAD-REQ-012`, `AERORA-PAYLOAD-REQ-013`, `AERORA-MISSION-REQ-005` |
| Affected risks | R-002 |

The architecture deliberately stops at the state-name boundary for this reason. Inventing an
enumeration would put unconfirmed values into a dialect, a metadata file, a log schema, an audit
record and an operator display simultaneously.

**Remains explicitly OPEN** per the Phase 2.1 directive.

## 16. Payload emergency stop

| Field | Value |
| --- | --- |
| Requirement | Payload emergency stop shall be represented separately from the payload state enumeration, with its initiating source reported |
| Architecture assumption | The emergency stop is an independent cut, not a state in a sequence |
| Evidence source | `MODEL`, `BASELINE-13`, `BASELINE-46` |
| Evidence status | `MODEL` — a watchdog-fault path directly into payload emergency stop is an observed path |
| Confidence | `PROBABLE` — the path is observed; behaviour is not frozen |
| Unresolved question | Q-009 — the emergency-stop behaviour: what it does, how it is cleared, whether it is distinct from a mode change |
| Affected requirements | `AERORA-PAYLOAD-REQ-008`, `AERORA-PAYLOAD-REQ-009`, `AERORA-SAFE-REQ-011` |
| Affected risks | R-002 |

The observed path is strong enough to justify the GCS representation decision — separate Fact, not
an enum value — because the path exists independently of the payload state machine. What it does
when it fires is open.

## 17. Power architecture

| Field | Value |
| --- | --- |
| Requirement | The GCS shall expose a power-source and configuration model rather than a battery-only model |
| Architecture assumption | Two operating configurations exist: tether power and onboard battery |
| Evidence source | `SCHEMATIC`, `BASELINE-49`, `BASELINE-50` |
| Evidence status | `ARTIFACT-OBSERVED` — dual tether and battery input connectors, power regulation, `INA226` monitoring; 480 V DC tether input specified at product level |
| Confidence | `PROBABLE` |
| Unresolved question | Q-024 — the frozen power architecture; Q-025 — how the GCS learns which configuration is in use |
| Affected requirements | `AERORA-POWER-REQ-007`, `AERORA-POWER-REQ-008` |
| Affected risks | None directly |

Baseline 49 states the limit of this evidence itself: the schematic "does not, by itself, prove" the
converter topology, source arbitration, ideal-diode behaviour, tether-loss detection or transition
transient performance.

## 18. Power transition behaviour

| Field | Value |
| --- | --- |
| Requirement | The GCS shall not initiate a power transition as a safety response |
| Architecture assumption | Power sequencing is vehicle-side; the GCS displays and logs it |
| Evidence source | `BASELINE-49` |
| Evidence status | `NOT-EVIDENCED` — baseline 49 states the two configurations are "not yet a claim of automatic in-flight source switching" and that any transition "requires separate engineering and verification" |
| Confidence | `UNKNOWN` |
| Unresolved question | Q-026 — whether in-flight transition is supported at all |
| Affected requirements | `AERORA-POWER-REQ-008` |
| Affected risks | None directly |

The GCS prohibition is safe to ratify and does not depend on the answer: the GCS not initiating a
transition is correct whether or not transitions exist. What depends on the answer is whether the
power display needs a transition state at all.

**Remains explicitly OPEN** per the Phase 2.1 directive.

## 19. Failsafe behaviour

| Field | Value |
| --- | --- |
| Requirement | Failsafe state, reason, source, severity and reported action shall each be represented separately; the GCS shall not decide or initiate a failsafe action |
| Architecture assumption | The vehicle determines and reports failsafe behaviour; the GCS displays it |
| Evidence source | `MODEL`, `BASELINE-13` |
| Evidence status | `PLACEHOLDER` — geofence, velocity and acceleration proposal-rejection paths are skeletons; parameter validation and boot-state paths are skeletons |
| Confidence | `UNKNOWN` |
| Unresolved question | Q-007 — the failsafe definition |
| Affected requirements | `AERORA-SAFE-REQ-008`, `AERORA-SAFE-REQ-009`, `AERORA-SAFE-REQ-010` |
| Affected risks | R-006 |

Baseline 13 defers final failsafe behaviour explicitly and instructs that the GCS specification
"shall not assign final RTL, LAND, HOLD, termination, mode-change or other emergency responses".
ADR-010 complies: five interfaces by shape, no values.

**Remains explicitly OPEN** per the Phase 2.1 directive.

## 20. Watchdog behaviour

| Field | Value |
| --- | --- |
| Requirement | The watchdog-initiated disarm and the payload emergency stop shall be presented as independent events |
| Architecture assumption | The watchdog acts directly, reaching disarm and payload stop independently |
| Evidence source | `MODEL`, `BASELINE-13`, `BASELINE-46` |
| Evidence status | `MODEL` — two observed paths: watchdog fault into disarm logic, and watchdog fault directly into payload emergency stop |
| Confidence | `PROBABLE` — paths observed; baseline 13 instructs that each observed path "must be converted into a formal requirement and verified against the real FC hardware" |
| Unresolved question | Timing, latency and whether the watchdog reports independently of the primary MCU — see item 2 |
| Affected requirements | `AERORA-SAFE-REQ-011`, `AERORA-POWER-REQ-007` |
| Affected risks | None directly |

The two-independent-paths observation is what the GCS representation decision rests on, and it is
the clearest firmware evidence in the matrix. The GCS presenting one as a consequence of the other
would be an inference the model does not support.

## 21. Companion and autonomy authority

| Field | Value |
| --- | --- |
| Requirement | The Aerora Autonomy module shall have no outbound command path; the GCS shall not infer proposal acceptance |
| Architecture assumption | The companion proposes; the flight controller decides; manual authority is structurally dominant |
| Evidence source | `MODEL`, `BASELINE-46`, `BASELINE-48` |
| Evidence status | `MODEL` — authority mode sentinels present; manual structurally dominant; semi-automatic requires explicit pilot confirmation; proposal rejection paths present |
| Confidence | `PROBABLE` — the authority structure is observed; the autonomy blending wiring is marked as future work, and final authority, freshness timeout and bounds are open |
| Unresolved question | Q-008 — whether proposal acceptance is explicitly reported |
| Affected requirements | `AERORA-GPU-REQ-003` through `AERORA-GPU-REQ-008`, `AERORA-SAFE-REQ-006`, `AERORA-SAFE-REQ-007` |
| Affected risks | R-014 |

The read-only Autonomy module is `GCS-DECIDED` and needs no vehicle evidence — it is a structural
property of Aerora's own code. What needs evidence is acceptance reporting, without which the GCS
can display a proposal and the effective state but must assert no causal link between them.

## 22. Fleet identity and secure element

| Field | Value |
| --- | --- |
| Requirement | The Aerora vehicle identifier shall be stable, unique, reportable over MAVLink and not operator-editable |
| Architecture assumption | A durable vehicle identity exists and is reportable, distinct from the MAVLink system id |
| Evidence source | `SCHEMATIC`, `BASELINE-50` |
| Evidence status | `ARTIFACT-OBSERVED` — `ATECC608A` secure element present |
| Confidence | `UNKNOWN` — presence of the part says nothing about provisioning, readability, or whether firmware will expose an identity from it |
| Unresolved question | Q-016 — the fleet identity source |
| Affected requirements | `AERORA-VEH-REQ-001` through `AERORA-VEH-REQ-005`, `AERORA-GCS-REQ-014` |
| Affected risks | **R-017 — `G-ARCH`** |

The architecture treats the identifier as opaque, which is what keeps Q-016 from blocking: eight of
the nine inter-entity references in the data model pass through it, and none interprets it.

Note the boundary with signing-key custody: the secure element is a genuine asset for vehicle-side
identity and key storage, and it does **not** address ground-station key storage. Conflating the two
would let real hardware appear to close a gap it cannot reach.

**Remains explicitly OPEN** per the Phase 2.1 directive.

## What the architecture does not depend on

Recorded so that the matrix is not read as a list of blockers. These are vehicle-side unknowns the
ground-station architecture is insulated from, and the insulation is deliberate:

| Unknown | Why the GCS is insulated |
| --- | --- |
| Flight controller MCU part | Never referenced |
| Which GPS receiver | Standard MAVLink handling, unchanged |
| How many radios exist | The GCS represents whatever links the vehicle reports |
| The autopilot identifier value | Any non-PX4, non-ArduPilot value resolves to the generic class |
| The failsafe action set | Interfaces by shape; no values assumed |
| The payload state enumeration | Members come from metadata, not code |
| Whether in-flight power transition exists | The GCS never initiates one |
| The identifier's internal structure | Opaque by design |

Eight insulations against twenty-two dependencies. The ones that remain load-bearing are items 6,
10 and 14, and all three are named in the ratification list.

## Evidence gates

| Gate | Items | Meaning |
| --- | --- | --- |
| `G-ARCH` | 14, 22 | Architecture ratification depends on these being acknowledged as unevidenced with fallbacks named |
| `G-INTEG` | 6, 8, 9, 12, 16, 20, 21 | Must produce evidence at hardware and firmware integration |
| `G-RELEASE` | 19 | Failsafe behaviour must be frozen and verified before release |
| `G-OPEN` | 1, 2, 3, 4, 5, 7, 11, 13, 15, 17, 18 | Carried; the architecture accommodates any answer |

Items 14 and 22 are at `G-ARCH` not because the architecture cannot proceed without the evidence,
but because **ratification must be an informed act**. A reviewer ratifying ADR-005 should know that
its preferred resolution is unproven. That is the whole purpose of this document.

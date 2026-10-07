# Phase 3.1 — Targeted Correction and Evidence Audit

A correction and audit pass over the accepted Phase 3 interface specification, so that it is safe to
hand to Phase 4.

| Field | Value |
| --- | --- |
| Phase | 3.1 — targeted correction and evidence audit |
| Phase 3 status | **ACCEPTED**, and remains accepted |
| QGC baseline commit | `9459a4b57986f40df8bdc193d5453c0596fbb416` |
| QGC source files modified | **0** — `git diff --stat HEAD` empty |
| Architecture changed | **No** |
| Requirements added, changed or promoted | **0** |
| Questions closed | **0** |
| Interface identifiers added | **0** |
| Implementation started | **No** |
| Review status | **AWAITING HUMAN REVIEW** |

Instruction files read before editing: `AGENTS.md` (injected), `CODING_STYLE.md` (in full),
`.pre-commit-config.yaml`, `justfile`, `.github/build-config.json`. No C++, QML, CMake or dialect
file was created or modified; this phase touched documentation only.

---

## 1. Executive summary

Phase 3 stands. Six corrections were applied to its wording and evidence references, eight findings
were recorded, and nothing was redesigned.

| Question | Answer |
| --- | --- |
| Does Phase 3 remain accepted? | **Yes.** No contract was withdrawn, no interface removed, no identifier renumbered |
| Did any architecture change occur? | **No** |
| Did any requirement change? | **No.** 120 Phase 2 requirements, unchanged, all still referenced |
| Were any questions closed? | **No.** Q-020 is recorded as **partially resolved** with its residual named; the register itself is left to the reviewer |
| Were any `PROPOSED` items promoted to `CONFIRMED`? | **No** |
| Did any QGroundControl source file change? | **No** |

### The three substantive outcomes

**One factual error corrected.** Phase 2 and the first Phase 3 pass framed QML override resolution
as *alias precedence* — two resources aliased to one URL, with an undetermined winner. That premise
is wrong. The mechanism is an explicit URL interceptor that rewrites a `qrc:` request to a
`/Custom`-prefixed mirror **if and only if that mirror exists**. Five source locations confirm it.
No alias collision is possible, the rule is deterministic, and the startup self-check assertion the
first pass believed was blocked on Q-020 is writable today.

**One evidence-provenance understatement corrected.** The Phase 3 vocabulary defined `MODEL` as
"present in the Simulink firmware model". No Simulink artifact exists in this repository. Every
`MODEL` and `PLACEHOLDER` row rests on baseline documents 13 and 46 **describing** the model. Phase
2.1's evidence matrix was already precise about this; Phase 3 had dropped the qualifier, and a Phase
4 reader would reasonably have believed a model had been inspected.

**One class of inference separated out.** The watchdog's relationship to disarm and to the payload
emergency stop is evidenced **in the aircraft** at `MODEL` strength and `PROBABLE` confidence. The
*reported* relationship — any field that tells the ground station one caused the other — is
`NOT-EVIDENCED`. Phase 3 used the word "consequence" for items the ground station receives
separately. Corrected: the aircraft-side path and the reported linkage are now stated as two
different claims with two different evidence levels.

### What Phase 3.1 deliberately did not do

| Not done | Why |
| --- | --- |
| Add a watchdog enumeration, timeout, latch rule or precedence | Prohibited; `NOT-EVIDENCED` |
| Add a failsafe action or mapping | Prohibited; baseline 13 defers failsafe entirely |
| Close any of the 56 `OPEN` interfaces | The audit verifies isolation, not closure |
| Edit any Phase 1, Phase 2 or Phase 2.1 document | Those phases are accepted and frozen. Supersessions are recorded here |
| Implement a drift checker | A Phase 4 deliverable |
| Create a requirement for any gap found | Prohibited; gaps became findings |

---

## 2. Watchdog evidence audit

### Sources inspected

| Source | What it gave |
| --- | --- |
| Product baseline `docs/13_SAFETY_AND_FAILSAFE.md` | The two observed paths, and the instruction that they are design inputs requiring hardware verification |
| `.../docs/46_PRODUCT_BASELINE_ANALYSIS.md` | The schematic record: `STM32G071CBTx` plus named watchdog nets; and "final watchdog timeout and reset/latch behavior" listed as unresolved |
| `.../docs/50_HARDWARE_SOFTWARE_INTERFACE_BASELINE.md` | The safety/security partition "must be formally defined" |
| `.../docs/11_GCS_UI_UX_SPECIFICATION.md` | Watchdog state listed in the ground-station display set |
| `.../docs/03_SYSTEM_ARCHITECTURE.md` | The safety/watchdog MCU in the block diagram |
| `.../PROJECT_STATUS.md` | Watchdog among the placeholders that "must be resolved before production claims" |
| Phase 2.1 evidence matrix items 2, 16, 20 | Existing classification: `ARTIFACT-OBSERVED`/`PROBABLE`, `MODEL`/`PROBABLE` |
| Phase 2 `authority-model.md`, ADR-010 | Watchdog path observed, never mediated |
| Phase 3 docs 02, 06, 07, 09, 16, 18, 21 | Every watchdog statement in the interface set |
| Repository search for `*.slx` / `*.mdl` | **Zero files.** No model artifact is present |

### The ten determinations, separately

| # | Determination | Finding | Evidence | Status |
| --- | --- | --- | --- | --- |
| 1 | **Existence** | A second MCU with a dedicated watchdog interface and named nets — `WATCHDOG_CMD_RX/TX`, `WATCHDOG_HEARTBEAT`, `WATCHDOG_NRST`, `WATCHDOG_SWDIO`, `WATCHDOG_3V3`, test points | `ARTIFACT-OBSERVED` via baseline 46 | `CONFIRMED` for presence; partition `PROBABLE` |
| 2 | **Observability** | Baseline 11 lists watchdog state in the display set, so it is **required to be displayable**. No carrying message is defined, and the reporting path is contested | `BASELINE-ASSERTED` for the obligation; `NOT-EVIDENCED` for the mechanism | `OPEN` |
| 3 | **Trigger semantics** | Timeout, reset and latch behaviour explicitly unresolved in baseline 46 | `NOT-EVIDENCED` | `OPEN` |
| 4 | **Response** | Two observed model paths, treated by baseline 13 as **design inputs, not proof of completion** | `MODEL` via baseline 13 | `PROPOSED` |
| 5 | **Watchdog → disarm** | "watchdog fault into disarm logic" — an observed path **in the aircraft**. No reported field carries the linkage | Aircraft: `MODEL`/`PROBABLE`. Reported: **`NOT-EVIDENCED`** | `PROPOSED` / `OPEN` |
| 6 | **Watchdog → payload stop** | "watchdog fault **directly** into payload E-stop" — "directly" is what supports independence from the disarm path. The only candidate reported linkage is the initiating-source field of `IF-PAY-006`, itself unevidenced | Aircraft: `MODEL`/`PROBABLE`. Reported: **`NOT-EVIDENCED`** | `PROPOSED` / `OPEN` |
| 7 | **Watchdog → power** | **No source states any relationship**, in either direction | **`NOT-EVIDENCED`** | `OPEN` |
| 8 | **Watchdog → failsafe** | **No source relates them.** Baseline 13 defers failsafe behaviour entirely pending the flight-controller contract | **`NOT-EVIDENCED`** | `OPEN` |
| 9 | **May the GCS display the relationship?** | It may display each reported item with its receive time, and an initiating source **only where reported**. It may not render a causal link, and may not read temporal order as causal order | `GCS-DECIDED`, under `AERORA-GCS-REQ-029` | `PROPOSED` |
| 10 | **Is the relationship confirmed?** | **No.** Baseline 13: each observed path "must be converted into a formal requirement and verified against the real FC hardware". None has been. No model artifact exists to inspect | `MODEL` at `PROBABLE` | `PROPOSED` |

### Existing statements, assessed

| Item | Existing statement | Evidence | Correct status | Correction required |
| --- | --- | --- | --- | --- |
| Doc 06 obligation table | "Show the disarm — **it is one consequence**"; "the payload stop — **the other consequence**" | Aircraft path `MODEL`; reported linkage `NOT-EVIDENCED` | Aircraft-side `PROPOSED`; reported-side `OPEN` | **Yes.** "Consequence" asserts a linkage the ground station does not receive. Rewritten as aircraft side versus ground-station side |
| Doc 18 `DEG-08` name | "Watchdog-initiated disarm reported" | The vehicle reports states, not an initiated-disarm event | — | **Yes.** Renamed "Watchdog event reported" |
| Doc 18 `DEG-08` response | "Show that the watchdog fired **and that disarm followed**" | Temporal correlation computed by the station | — | **Yes.** Rewritten: show and record each reported item with its time; assert no causal link |
| Doc 18 `DEG-08` evidence | `MODEL` | Correct for the aircraft path; silent on the reported linkage | — | **Yes.** Now `MODEL` via baseline 13, with the reported linkage `NOT-EVIDENCED` |
| Doc 07 `IF-PWR-011` | Separate from power state "because the watchdog acts independently" | Correct, and the non-relationship was left implicit | `PROPOSED` | **Yes.** Added an explicit watchdog-to-power `NOT-EVIDENCED` table |
| Doc 09 watchdog section | Two independent observed paths, neither as a consequence of the other | Correct | `PROPOSED` | **Yes, additive.** Added that the watchdog is not a failsafe interface and the relationship is `NOT-EVIDENCED` |
| Doc 16 indicator pair | "Watchdog-initiated disarm and payload emergency stop" must share no visual language | The pair naming follows `AERORA-SAFE-REQ-011` verbatim | `PROPOSED` | **No.** The row's point is that conflating them is a ground-station inference, which is correct |
| Doc 02 Q3-004 note | Reporting source would be the relay, not the originator | `NOT-EVIDENCED` | `OPEN` | **No.** Already correct and already a question |
| `AERORA-SAFE-REQ-011` | "The watchdog-initiated disarm and the payload emergency stop shall be presented as independent events" | `CONFIRMED` from baseline 46 | Unchanged | **No.** The requirement mandates the conservative presentation. Its phrase describes the aircraft, not a display instruction. **Not modified** |
| R3-003 | Reporting path may name a relay as originator | `MODEL` for existence; `NOT-EVIDENCED` for reporting | `HIGH`, `G-INTEG` | **No.** Already correctly scoped |
| ADR-010 | Failsafe boundary; GCS decides no failsafe action | `CONFIRMED` | Unchanged | **No** |

**No watchdog enumeration, threshold, failsafe action or precedence rule was added.** The audit
added one table of non-relationships and corrected three wordings.

---

## 3. Interface state terminology audit

### The distinction now carried in the Phase 3 readme

| State | Meaning | Interfaces in it |
| --- | --- | --- |
| **Interface contract** | Specified in an interface control document | **240** |
| **Implemented interface** | Source code exists that realises the contract | **0** |
| **Verified interface** | Executed, with evidence confirming the behaviour | **0** |

The readme now also states that an interface is not implemented because it has an identifier, a
status, a ratification class, a named QGroundControl seam, a planned fact group, an identified C++
class or an identified QML module; and not verified because it has an assigned verification method.

### `GCS-VERIFIED` is the trap

The code contains the word "verified" and means **a statement about upstream QGroundControl
established by reading upstream source**. It is not an execution result and says nothing about
Aerora code. 89 interface rows carry it.

Renaming it was rejected: it is Phase 2.1 vocabulary, carried by 89 rows and the frozen evidence
matrix, and renaming would break traceability to buy clarity that a definition buys more cheaply.
The definition was tightened instead.

### Corrections applied

| Document | Before | After | Why |
| --- | --- | --- | --- |
| 00 readme | `MODEL` — "Present in the Simulink firmware model" | "...**as recorded in baselines 13 and 46**", plus a provenance-limits table stating no model artifact exists in this repository | A Phase 4 reader would have believed a model was inspected |
| 00 readme | `ARTIFACT-OBSERVED` — "A named hardware or PCB artifact shows it exists" | "...**as recorded in baseline 46**" | Same reason |
| 00 readme | `GCS-VERIFIED` — "Verified by reading QGroundControl source" | "Established by **reading QGroundControl source**. Not an execution result" | Removes the implication of a test result |
| 00 readme | — | Added the contract/implemented/verified table and the present-tense clarification | Phase 4 inheritance |
| 01 | "three of the four **are verified by a test** rather than a policy" | "are **assigned** a test... No such test has been written" | Asserted tests that do not exist |
| 01 | "it **is verified by** a single integration test, which is what makes the property enforceable" | "a single integration test is **assigned** to it — which is what **will** make the property enforceable once the test exists" | Same |
| 04 | "Four links in the chain. **Three are verified**; the first is not." | "Three are **evidenced by reading upstream source**; the first is not evidenced at all", with the meaning of "evidenced" spelled out | "Verified" read as a test result |
| 05 | Table row "**Implemented by the ground station**"; "The ground station **implements** interface A only" | "Ground station **participates in it**"; "is **specified to participate** in interface A only... Neither interface is implemented; both are contracts" | Present-tense claim about non-existent code |
| 13 | "Both **are verified by** `AUDIT` because there is nothing to test" | "Both are **assigned** `AUDIT`... No audit has been performed" | Implied the audit happened |
| 18 | "The upstream mechanism **is verified** and retained" | "was **read in Phase 1** and is retained unchanged" | Same ambiguity as `GCS-VERIFIED` |
| 21 | R3-001: "only one **is verified by** an automated test"; column "Verified by" | "only one **has an automated test assigned**... **No test of any of the four has been written**"; column "Verification route" | The tested rule is not tested either |
| 21 | R3-007: "Nine of them **are verified by** `STATIC` inspection only" | "are **assigned** `STATIC` inspection only... None has yet been performed" | Same |
| 21, report | R3-001 and R3-007 summary rows | Reworded to "no test assigned" / "assigned inspection alone" | Consistency |
| 23 | "Both **are verified by** `AUDIT` rather than by test" | "Both are **assigned** `AUDIT`... No audit has been performed" | Same |

### Terms audited and left unchanged

| Term | Where | Why it is already correct |
| --- | --- | --- |
| `observed` | Watchdog paths, model elements | Used only of the baseline's record of the model, and now qualified by the readme provenance table |
| `confirmed` | Status `CONFIRMED` | Defined as "established by direct evidence or an approved requirement" — a specification-grade claim, not an execution claim |
| `proposed` | Status `PROPOSED` | Defined as "an engineering recommendation, not yet ratified" |
| `open`, `unknown` | Statuses | Already the weakest claims; nothing to weaken |
| Normative present tense | Throughout | Conventional in an interface control document. The readme now states that every such sentence is a contract statement, not a statement that code exists |

That last row is the deliberate limit of this audit. Rewriting 240 contracts into the subjunctive
would damage readability to restate once what the readme now states once.

---

## 4. OPEN interface isolation audit

56 of 240 interfaces carry status `OPEN`. **None was closed.** The audit asks only whether
implementation can proceed without hard-coding an unknown value.

Mechanically extracted; the count and the per-domain split are reproducible.

| Domain | `OPEN` |
| --- | --- |
| `PAY` | 9 |
| `PWR` | 7 |
| `FC` | 6 |
| `GPU` | 6 |
| `MAV` | 6 |
| `MIS` | 4 |
| `AUT` | 3 |
| `SEC` | 3 |
| `TLM` | 3 |
| `RAD` | 2 |
| `LOG` | 2 |
| `VID` | 2 |
| `CLD` | 1 |
| `PAR` | 1 |
| `UIS` | 1 |
| Total | **56** |

### Isolation audit

| Interface | Status | Unknown element | Isolation strategy | Evidence | Phase 4 impact |
| --- | --- | --- | --- | --- | --- |
| `IF-AUT-002` | `OPEN` | The command that carries an authority mode request | Command abstraction behind the eight-state tracker; control hidden until capability is reported | `NOT-EVIDENCED` | Tracker implementable; the command is a deferred mapping entry |
| `IF-AUT-005` | `OPEN` | The confirmation command | Same command abstraction; pending display driven by reported state only | `NOT-EVIDENCED` | Display implementable; send path deferred |
| `IF-AUT-010` | `OPEN` | Failsafe state, reason, source, severity, action enumerations | Five separate Facts, enumerations from metadata, unrecognised value shown raw | `PLACEHOLDER` | Facts and metadata file implementable; members are a metadata deliverable, no code change |
| `IF-CLD-015` | `OPEN` | The synchronisation conflict policy | Conflict surfaced, **neither side overwritten**; policy behind a configuration boundary | `NOT-EVIDENCED` | Detect-and-surface implementable; resolution is a pluggable policy |
| `IF-FC-002` | `OPEN` | Aerora custom flight-mode mapping | Firmware-plugin mode accessor returns empty, so dependent actions are hidden | `GCS-VERIFIED` mechanism; mapping `NOT-EVIDENCED` | Plugin implementable with an empty mode table |
| `IF-FC-010` | `OPEN` | Autonomy message, fields, rates | Fact group registered at vehicle construction; values unknown until reported | `MODEL` | Fact group implementable empty |
| `IF-FC-012` | `OPEN` | Payload state, health and safety enumerations | Fact group plus metadata enumerations; explicit `UNKNOWN` | `BASELINE-ASSERTED`, `PLACEHOLDER` | Implementable empty |
| `IF-FC-017` | `OPEN` | Capability inventory field definition | Capability model rules 1 and 6: not reported is unknown; unrecognised identifier retained and ignored | `NOT-EVIDENCED` | Model implementable. **Presence-versus-availability is a real gap** — see F3.1-05 |
| `IF-FC-018` | `OPEN` | Whether an identity is reported, and from what source | Opaque value, parsed by no consumer; explicit unidentified attribution | `ARTIFACT-OBSERVED`, `NOT-EVIDENCED` | Identity binding implementable with an unidentified state |
| `IF-FC-021` | `OPEN` | Payload command set | Command abstraction plus capability gating | `BASELINE-ASSERTED`, `PLACEHOLDER` | Path implementable; command list deferred |
| `IF-GPU-001` | `OPEN` | Companion component id | Component-id filter is **configuration, not a constant**; non-autopilot components are not promoted to vehicles | `NOT-EVIDENCED` | Configuration boundary; no hard-coded id |
| `IF-GPU-002` | `OPEN` | Proposal message, fields, classes | Autonomy fact group; per-class freshness structure with values from configuration | `MODEL` | Fact group implementable; thresholds a config table |
| `IF-GPU-003` | `OPEN` | Whether an outcome is reported at all | No inference: proposal and effective state shown side by side with **no causal link** | `NOT-EVIDENCED` | Display implementable; there is no inference code to write |
| `IF-GPU-004` | `OPEN` | Companion health fields | Fact group; unknown is not healthy | `NOT-EVIDENCED` | Implementable empty |
| `IF-GPU-005` | `OPEN` | How presence is signalled | Absence by freshness threshold; absent and silent are two distinct display states | `NOT-EVIDENCED` | Implementable with a configured threshold |
| `IF-GPU-007` | `OPEN` | Autonomy capability identifiers | Capability model; no capability means no autonomy presentation | `NOT-EVIDENCED` | Implementable |
| `IF-LOG-012` | `OPEN` | The five failsafe enumerations | Record the reported Facts with metadata; retain raw value for unrecognised members | `PLACEHOLDER` | Record writer implementable; schema carries opaque enumerated values |
| `IF-LOG-020` | `OPEN` | Audit-store integrity mechanism | Contract shape fixed: integrity material written with the record, exported with it. **Shape only — no mechanism** | `NOT-EVIDENCED` | **F3.1-03.** A shape-satisfying stub would be a security control that does nothing |
| `IF-MAV-021` | `OPEN` | The Aerora autopilot identifier | Competing vendor factories disabled by existing build options, so the Aerora plugin is selected regardless; startup self-check asserts the selection | `NOT-EVIDENCED` | Implementable; the deterministic form needs Q-018 |
| `IF-MAV-022` | `OPEN` | Whether an originating-component field exists | Record the **sending** component; never present it as originator | `NOT-EVIDENCED` | Implementable; stronger behaviour waits on Q3-004 |
| `IF-MAV-023` | `OPEN` | Whether acceptance is reported | No inference; side-by-side display | `NOT-EVIDENCED` | Implementable |
| `IF-MAV-024` | `OPEN` | Whether a wire request identifier exists | Ground-station-assigned request identity, recorded alongside any MAVLink correlation | `NOT-EVIDENCED` | **Fully implementable without the wire field** |
| `IF-MAV-025` | `OPEN` | Per-command resume declaration | The mission plan-constraint fallback applies unconditionally | `NOT-EVIDENCED` | Implementable |
| `IF-MAV-026` | `OPEN` | The whole inventory contract | Absent inventory degrades to the standard capability set | `NOT-EVIDENCED` | Implementable; this is the graceful-degradation keystone |
| `IF-MIS-007` | `OPEN` | Vehicle-side pause semantics | Upstream pause mechanism retained; no Aerora semantics added | `GCS-VERIFIED` | Inherit upstream unchanged |
| `IF-MIS-011` | `OPEN` | Pattern geometry and parameters | Complex item as a layer-2 class following the upstream pattern; geometry is product definition | `GCS-VERIFIED` mechanism | Class implementable; parameters a product deliverable |
| `IF-MIS-012` | `OPEN` | Aerora mission commands | The supported-mission-commands accessor returns the standard set until the dialect lands | `NOT-EVIDENCED` | Implementable via the existing plugin accessor |
| `IF-MIS-014` | `OPEN` | Whether payload state survives a resume | Plan constraint plus resume warning, **unconditional** | `NOT-EVIDENCED` | Implementable; the fallback is the mitigation for R-002 |
| `IF-PAR-009` | `OPEN` | Whether payload configuration is parameter-backed | Preferred route is the payload interface; if parameter-backed it is a Fact like any other | `NOT-EVIDENCED` | No code depends on the answer |
| `IF-PAY-002` | `OPEN` | Payload capability identifiers | Capability model; no capability set means no payload controls at all | `NOT-EVIDENCED` | Implementable |
| `IF-PAY-003` | `OPEN` | Payload state enumeration | Metadata-driven enumeration, explicit `UNKNOWN`, raw value retained | `PLACEHOLDER` | Implementable; members a metadata deliverable |
| `IF-PAY-008` | `OPEN` | Configuration values and mechanism | Readback-confirmed write; absent means no configuration surface | `NOT-EVIDENCED` | Implementable |
| `IF-PAY-009` | `OPEN` | Payload command set | Command abstraction; effectiveness requires a reported state change, not an acknowledgement | `NOT-EVIDENCED` | Implementable; command list deferred |
| `IF-PAY-010` | `OPEN` | The emergency-stop command | Command abstraction; the request is not the stop | `NOT-EVIDENCED` | Implementable; control hidden until capability reported |
| `IF-PAY-011` | `OPEN` | Calibration procedures | Progress **observed, never simulated** | `BASELINE-ASSERTED` | Implementable as an observer |
| `IF-PAY-012` | `OPEN` | Diagnostic content | Logged and surfaced, **never parsed to derive state** | `BASELINE-ASSERTED` | Implementable as pass-through |
| `IF-PAY-013` | `OPEN` | Actuator semantics | Vehicle-internal; **no ground-station path exists** | `PLACEHOLDER` | Nothing to implement; assertion only |
| `IF-PAY-014` | `OPEN` | Whether state is retained across interruption | Ground station assumes nothing; plan constraint plus resume warning | `NOT-EVIDENCED` | Implementable; R-002's mitigation |
| `IF-PWR-003` | `OPEN` | Power source enumeration | Metadata enumeration; unknown shown as unknown | `BASELINE-ASSERTED` | Implementable |
| `IF-PWR-004` | `OPEN` | Transition behaviour and timing | **No transition inferred**; reported state only; station initiates nothing | `BASELINE-ASSERTED` | Implementable; there is no transition model to write |
| `IF-PWR-007` | `OPEN` | Fault enumeration | Metadata enumeration | `BASELINE-ASSERTED` | Implementable |
| `IF-PWR-008` | `OPEN` | Whether the vehicle validates remaining energy | Presented only where reported; **no ground-side estimation algorithm** | `BASELINE-ASSERTED` | Implementable; the absence of an algorithm is the design |
| `IF-PWR-009` | `OPEN` | Degraded-mode enumeration | Metadata enumeration; no limitation inferred | `BASELINE-ASSERTED` | Implementable |
| `IF-PWR-010` | `OPEN` | Whether converter state is reported | Unknown shown as unknown | `ARTIFACT-OBSERVED`, `NOT-EVIDENCED` | Implementable |
| `IF-PWR-012` | `OPEN` | Power capability identifiers | Capability model; absent means the generic battery model only | `NOT-EVIDENCED` | Implementable |
| `IF-RAD-001` | `OPEN` | Whether the radio enumerates as a serial device | **None available in the overlay.** Transport extensibility itself requires core-tree modification | `NOT-EVIDENCED` | **F3.1-02.** The answer determines whether ADR-002 holds |
| `IF-RAD-003` | `OPEN` | USB vendor and product identifiers | **Disable serial auto-connect in the Aerora settings defaults** — entirely ground-station-side, no identifier needed | `NOT-EVIDENCED` | Implementable; the settings default *is* the isolation |
| `IF-SEC-003` | `OPEN` | The operator role set | Roles from configuration, not code; gated-operation set declared explicitly; an unresolvable role set grants nothing | `NOT-EVIDENCED` | **F3.1-04.** Code is role-agnostic; no usable default configuration exists without Q-029 |
| `IF-SEC-013` | `OPEN` | Key protection at rest | **None available in the overlay.** The storage class has no virtual members | `GCS-VERIFIED` that no seam exists | **F3.1-01.** ADR-008 keeps two routes; neither is an isolation |
| `IF-SEC-017` | `OPEN` | The Aerora unsigned-acceptance default | Explicit settings value against an existing upstream policy callback | `GCS-VERIFIED` mechanism | Implementable; the default value is a product decision |
| `IF-TLM-003` | `OPEN` | Payload telemetry fields | Fact group plus metadata | `BASELINE-ASSERTED` | Implementable empty |
| `IF-TLM-004` | `OPEN` | Product telemetry fields | Product fact group inside the payload module; absent means the product panel is not presented | `PLACEHOLDER` | Implementable |
| `IF-TLM-007` | `OPEN` | Autonomy telemetry fields and thresholds | Fact group; per-class thresholds from configuration | `MODEL` | Implementable |
| `IF-UIS-014` | `OPEN` | Whether Aerora's own overlay resources resolve correctly | **Corrected in Phase 3.1.** The resolution rule is a deterministic existence check performed by a URL interceptor; the self-check asserts interception per overridden URL | `GCS-VERIFIED` mechanism; Aerora wiring `NOT-EVIDENCED` | Isolation now exists. Residual needs a build |
| `IF-VID-003` | `OPEN` | Whether an identity is reported | Opaque value; unidentified attribution; records buffered, never discarded | `NOT-EVIDENCED` | Implementable |
| `IF-VID-005` | `OPEN` | Which source the identity derives from | Vehicle-internal; opacity makes it irrelevant to the ground station | `NOT-EVIDENCED` | Nothing to implement |

**52 of 56 have a safe isolation strategy.** Four do not, and are recorded as findings.

### Pattern distribution

| Isolation pattern | Interfaces |
| --- | --- |
| Fact group plus metadata-driven enumeration | 14 |
| Explicit `UNKNOWN` / unavailable state | 11 |
| Capability model gating | 6 |
| Command abstraction with deferred mapping | 6 |
| Configuration boundary | 5 |
| Prohibition — nothing to implement | 4 |
| Upstream mechanism inherited unchanged | 4 |
| Deferred mapping table | 2 |
| **None available** | **4** |

The two patterns doing most of the work are the same pair the architecture rests on: everything is a
Fact with metadata-supplied enumerations, and anything not reported is explicitly unknown. Between
them they isolate 25 of the 56.

### Findings

| ID | Finding | Interfaces | Existing record | Why it is a finding |
| --- | --- | --- | --- | --- |
| **F3.1-01** | Signing-key protection at rest cannot be isolated from the overlay. The storage class has no virtual members and its only friend declaration is a test class | `IF-SEC-013` | R-005 `G-RELEASE`, ADR-008, `AERORA-SEC-REQ-017` | ADR-008 offers an upstream contribution or a core edit. Neither is an isolation strategy: there is no abstraction Phase 4 can write that defers the decision |
| **F3.1-02** | Radio host presentation cannot be isolated. If the radio does not enumerate as a serial device, ADR-002 is invalid and six deferred transport core edits return | `IF-RAD-001` | R-003 `G-ARCH`, ADR-002, Q3-001 | The usual answer — abstract the transport — is itself the thing R-003 says requires core edits. Mitigation is a one-connection bench observation, not a design |
| **F3.1-03** | Audit-store integrity has a fixed contract shape and no mechanism. A stub satisfying the shape would be a security control that does nothing, while `AERORA-GCS-REQ-028` reads as met | `IF-LOG-020` | Q-037 `ARCH-SHAPING`, `AERORA-GCS-REQ-028` | The shape makes the mechanism *replaceable*, not *deferrable*. Phase 4 must not ship the shape alone |
| **F3.1-04** | Role resolution is code-isolated but not deployable. The code is role-set-agnostic; no usable default configuration can exist without Q-029, and the safe default grants nothing | `IF-SEC-003` | Q-029 `ARCH-SHAPING`, `AERORA-SEC-REQ-013` | Isolation at the code boundary is not isolation at the delivery boundary. A station with no role set is correct and unusable |
| **F3.1-05** | Capability presence and availability may be indistinguishable, and the conservative fallback **loses** the actionable reason rather than deferring it | `IF-FC-017`, `IF-UIS-005`, `IF-UIS-006` | R3-002 `G-INTEG`, Q3-009 | Most isolations defer a value. This one discards information permanently, and the loss is invisible to the operator |
| **F3.1-06** | The Phase 3 evidence vocabulary understated the provenance of `MODEL` and `ARTIFACT-OBSERVED`. No model or hardware artifact exists in this repository | All 22 `MODEL`/`PLACEHOLDER` rows and all 5 `ARTIFACT-OBSERVED` rows | Phase 2.1 evidence matrix was already precise | **Corrected** in doc 00. Recorded so the correction is auditable |
| **F3.1-07** | Phase 2's `ui-architecture.md` and the Phase 2 record of Q-020 carry the superseded alias-precedence premise | `IF-UIS-014`, Q-020 | Phase 2 `ui-architecture.md`, Phase 2 `open-questions.md` | Phase 2 is accepted and frozen, so Phase 3.1 corrected Phase 3 and records the supersession here. **Both positions retained** |
| **F3.1-08** | Phase 3 never named the overlay activation mechanism, so a Phase 4 implementer reading only Phase 3 could not tell how the overlay is enabled | Doc 16, doc 23 | Phase 1 `plugin-architecture.md`, `build-system.md` | **Corrected** in doc 23 by naming `QGC_CUSTOM_DIR`, the overlay contract and the path validation, with references to the Phase 1 documents |

None of the eight was solved by inventing a requirement or a value.

---

## 5. UI override and upstream drift audit

### The corrected mechanism

The single substantive correction of Phase 3.1. All five steps `GCS-VERIFIED` by reading the
checkout at the baseline commit.

| Step | Location | Behaviour |
| --- | --- | --- |
| 1 | `src/API/QGCCorePlugin.h:117` | `createQmlApplicationEngine(QObject*)` is **virtual**; a symmetric destroy hook is documented as the release point for URL interceptors |
| 2 | `src/QGCApplication.cc:275` | The application obtains its engine **through the plugin instance** |
| 3 | `custom-example/CMakeLists.txt:93-97` | Override resources declared under `PREFIX "/Custom/qml"` with `QT_RESOURCE_ALIAS` set to the **stock** path |
| 4 | `custom-example/src/CustomPlugin.cc:240-248` | The plugin override installs a `QQmlAbstractUrlInterceptor` |
| 5 | `custom-example/src/CustomPlugin.cc:273-295` | `intercept()` forms `:/Custom<origPath>` and returns the rewritten URL **if and only if `QFile::exists()`**; otherwise returns the original |
| — | `src/QGCApplication.cc:311` | `interceptUrl()` is called explicitly for the Linux window icon, which does not route through the engine |
| — | `docs/en/qgc-dev-guide/custom_build/resource_override.md` | The upstream developer guide states the same mechanism and the `/Custom` prefix convention |

| Superseded position | Corrected position |
| --- | --- |
| "Two resources aliased to the same URL is precisely the situation this mechanism creates, and the precedence — which one the QML engine resolves — is the substance of Q-020" | **No two resources ever share a URL.** The overlay copy lives under a different prefix. Resolution is a deterministic existence check, not a contest |
| "The third assertion cannot be written until Q-020 is resolved, because it needs to know what 'resolved content' is observable as" | **It can be written today.** Assert that interception of each overridden stock URL returns a `/Custom`-prefixed URL, which reduces to asserting the mirrored resource exists |

The superseded text is retained above, per the Phase 2.1 discipline. Phase 2's documents were not
edited.

### The six required statements, verified present

| # | Statement | Where it now appears |
| --- | --- | --- |
| 1 | Resource override may avoid direct upstream source modification | Doc 16, "Why `IF-UIS-012` is specified at all"; doc 23 overlay structure |
| 2 | Resource override can still create behavioural divergence | Doc 16, now stated as a block-quoted rule Phase 4 must inherit |
| 3 | Alias precedence must be verified with an actual build or runtime test | Doc 16 "What remains open"; doc 23 smoke test; `IF-UIS-014` method `INTEGRATION`; `IF-UIS-015` method `DEMO` |
| 4 | Upstream changes affecting overridden resources must be detectable | Doc 16 `IF-UIS-012` obligations; `AERORA-UI-REQ-006`, `AERORA-TEST-REQ-008`; R-026 |
| 5 | Drift detection must become a Phase 4 engineering and CI mechanism | Doc 16, added explicitly; doc 23 CI requirements names `AERORA-TEST-REQ-008` as the highest-value addition |
| 6 | No claim of zero behavioural divergence may be made without evidence | Doc 16, in the block quote and the retained "zero core edits versus zero silent forks" conflict |

### The distinction, stated once

> **"No QGroundControl source modification" does not mean "no behavioural divergence from
> upstream."**

Both claims are true of different things and Phase 3 keeps them separate rather than letting the
first imply the second. The core-edit count remains **zero required, one candidate** (signing-key
storage, ADR-008). The silent-fork count is **one** — the fly-view tool strip — and it is a real
behavioural divergence with a recorded fork point and an unassigned owner.

### Phase 4 dependency

| Deliverable | Requirement | Not done in Phase 3.1 |
| --- | --- | --- |
| Mechanical overridden-file drift check against recorded fork-point commits | `AERORA-TEST-REQ-008` | **No checker was implemented** |
| Startup self-check asserting interception per overridden URL | `AERORA-UI-REQ-007`, `AERORA-TEST-REQ-007` | **No code was written** |
| Named owner for the tier-3 override | `AERORA-UI-REQ-005`; Q-038 | **Not assigned** — a process decision |

---

## 6. Q3 question audit

All nine inspected. **None was closed. None was accidentally closed.** Classifications, owners and
evidence requirements were checked against what each question actually needs.

| Question | Current status | Evidence required | Gate | Phase 4 impact |
| --- | --- | --- | --- | --- |
| Q3-001 | `OPEN`, `IMPL-BLOCKING` | Bench: attach the radio, record enumerated device class and USB vendor/product identifiers | **`G-ARCH`** — the answer can invalidate ADR-002 | **Highest.** Confirms or withdraws the serial-first transport strategy and six deferred core edits. Interim mitigation — auto-connect disabled by settings default — is ground-station-side and holds either way. See F3.1-02 |
| Q3-002 | `OPEN`, `ARCH-SHAPING` | None external; an Aerora dialect and operations decision | `G-IMPL` | Low. The ground-station request identity already satisfies the audit requirement; the wire field would only let an ambiguous acknowledgement be resolved rather than recorded as ambiguous |
| Q3-003 | `OPEN`, `ARCH-SHAPING` | A firmware statement of the complete authority mode enumeration, or that it is open-ended | `G-INTEG` | Low. Metadata-driven enumeration absorbs additional values with no code change; unrecognised values display as unrecognised |
| Q3-004 | `OPEN`, `ARCH-SHAPING` | Firmware MAVLink routing design, **plus** a bench observation with the primary MCU held in reset | `G-INTEG` | Medium. Determines whether the recorded reporting source can be trusted to mean the originator. Carries R3-003 at `HIGH` |
| Q3-005 | `OPEN`, `ARCH-SHAPING` | The flight-controller to companion interface design: is companion traffic bridged or terminated | `G-INTEG` | Medium. Determines which of `IF-FC-010` and `IF-GPU-002` exists, and against which clock companion staleness is measured |
| Q3-006 | `OPEN`, `ARCH-SHAPING` | A PCB or block-diagram artifact showing a separate payload controller, plus its component-id allocation | `G-INTEG` | Medium. Determines whether payload silence is distinguishable from flight-controller silence, and whether payload parameters are second-class |
| Q3-007 | `OPEN`, `ARCH-SHAPING` | A firmware telemetry definition for the power domain, or a statement that the high-voltage side is monitored but not reported | `G-INTEG` | Low to medium. Determines whether the tether display is a state enumeration only, or carries measured quantities |
| Q3-008 | `OPEN`, `DEFERRABLE` | Whether the vehicle reports a time base, and whether it is GNSS-derived | `G-RELEASE` | Low. Station-side ordering is correct regardless through the monotonic component. Cannot be settled after the first investigation |
| Q3-009 | `OPEN`, `IMPL-BLOCKING` | The capability inventory's field definition — one field decision | `G-IMPL` | **High.** Determines whether the capability model can be populated for Aerora capabilities or only be structurally correct. Carries R3-002. See F3.1-05 |

### Changes made

| Change | Reason |
| --- | --- |
| **Gates assigned for the first time.** Phase 3 gave each Q3 question a blocking classification — `IMPL-BLOCKING`, `ARCH-SHAPING`, `DEFERRABLE` — but no gate. The table above adds one per question | A classification says how urgently an answer is needed; a gate says what cannot pass without it. Phase 4 needs both, and the two are not interchangeable |
| Q3-001 placed at `G-ARCH` rather than `G-IMPL` | An answer of "not a serial device" invalidates an architecture decision record. A question that can invalidate ADR-002 is an architecture gate, notwithstanding its `IMPL-BLOCKING` classification |
| No classification changed | Each was checked against its evidence requirement and found correctly placed |
| No question closed | Nothing is objectively evidenced to closure |

The gates live in this audit only. Document 20 retains the classifications it was accepted with;
Phase 3.1 adds no column to it.

### Q-020, the one inherited question with new evidence

| Field | Value |
| --- | --- |
| Phase 2 record | `IMPL-BLOCKING` — "whether a custom build can shadow an existing resource alias, and which wins" |
| Phase 3.1 assessment | **PARTIALLY RESOLVED.** The premise is wrong and the mechanism is settled by source evidence. The residual is whether Aerora's own overlay resources are declared and resolved correctly, which needs a build |
| Precedent | Phase 2.1 gave Q-019 the same `PARTIALLY RESOLVED` treatment |
| Action taken | Evidence recorded here and in doc 16; `IF-UIS-014` reframed with corrected evidence; the Phase 2 question register **not edited** |
| Action for the reviewer | Decide whether to record Q-020 as `PARTIALLY RESOLVED` in the Phase 2 register. Phase 3.1 does not change a frozen phase |

The resolved-question count for Phase 3.1 is therefore **zero**.

---

## 7. R3 risk audit

All eight inspected. **None was closed, downgraded, or accepted on the strength of a mitigation that
does not yet exist.**

| Risk | Status | Mitigation | Remaining exposure | Gate |
| --- | --- | --- | --- | --- |
| R3-001 | `OPEN`, `MEDIUM` | **Proposed, not built.** Extend the architectural-property tests to all four dependency rules | **Full.** No test of any of the four rules has been written. The one rule with a test *assigned* is no better off than the three without until Phase 4 writes them | `G-IMPL` |
| R3-002 | `OPEN`, `MEDIUM` | Q3-009 asks for the field definition before the inventory is implemented | **Full, and the fallback loses information.** A question is not a mitigation. See F3.1-05 | `G-INTEG` |
| R3-003 | `OPEN`, `HIGH` | Record the **sending** component and never present it as originator; Q3-004 asks for the routing design | **Partial.** The wording mitigation is real and is now carried in doc 18 and doc 02. The accountability exposure persists until the bench observation | `G-INTEG` |
| R3-004 | `OPEN`, `MEDIUM` | Sequencing: build the 184 `CONFIRMED` and `PROPOSED` interfaces first; every `OPEN` interface has a stated fallback | **Partial.** The fallbacks are verified present by section 4. The schedule exposure — external deliverables arriving late — is unmitigated and unmitigable from the ground station | `G-IMPL` |
| R3-005 | `OPEN`, `HIGH` | **Visibility, not mitigation.** The per-interface evidence status makes the exposure countable, and `BENCH`/`HIL` assignments concentrate on it | **Full.** 105 vehicle-side interfaces, 5 backed by an observed artifact. Phase 3.1 **worsened the stated position** by establishing that no model or hardware artifact exists in this repository at all | `G-INTEG` |
| R3-006 | `OPEN`, `MEDIUM` | Readme convention, the traceability document's single-row view, and a scripted both-tables check | **Accepted deliberately.** A single 17-column table would be complete and unread | `G-OPEN` |
| R3-007 | `OPEN`, `MEDIUM` | Convert inspection-only prohibitions to assertions, `IF-AUT-012` and `IF-PAY-015` first | **Full.** No prohibition assertion has been written, and nine of sixteen rest on inspection | `G-RELEASE` |
| R3-008 | `OPEN`, `MEDIUM` | Each question names its affected interfaces and requirements; the procedural fix belongs to the requirement owner | **Full, and Phase 3.1 cannot close it.** Phase 3.1 added eight findings, which are also not requirements. The gap widened | `G-IMPL` |

### Observations

| Observation | Detail |
| --- | --- |
| **No risk was accepted because the architecture has a mitigation** | Five of eight carry "full" remaining exposure. Mitigations that are proposals were relabelled as proposals |
| R3-005 moved in the wrong direction | Phase 3.1's evidence audit established that `MODEL` and `ARTIFACT-OBSERVED` rows rest on baseline descriptions rather than inspected artifacts. The risk is unchanged in severity and better understood |
| R3-008 widened | Phase 3.1 found eight items that are neither requirements nor questions. They are findings, which carry even less verification obligation than questions |
| No new R3 risk was created | Each Phase 3.1 finding maps to an existing risk or question. A ninth risk identifier would have duplicated R3-002, R-003, R-005, Q-029 or Q-037 |

---

## 8. Change log

### Documents changed

| Document | Change | Kind |
| --- | --- | --- |
| `00-phase-3-readme.md` | `MODEL` and `ARTIFACT-OBSERVED` provenance qualified; `GCS-VERIFIED` definition tightened; provenance-limits table added; contract/implemented/verified table added; present-tense clarification added | Evidence and terminology |
| `01-gcs-system-icd.md` | Two "is verified by a test" claims changed to "assigned"; `IF-SYS` traceability table gained no new rows | Terminology |
| `04-radio-link-icd.md` | "Three are verified" changed to "evidenced by reading upstream source", with the meaning stated | Terminology |
| `05-gpu-fc-icd.md` | "Implemented by the ground station" changed to "participates in"; "implements interface A" changed to "specified to participate" | Terminology |
| `06-payload-icd.md` | Watchdog section split into aircraft side and ground-station side; "consequence" removed; evidence levels stated per row | Watchdog evidence |
| `07-power-telemetry-icd.md` | Explicit watchdog-to-power `NOT-EVIDENCED` table added | Watchdog evidence |
| `09-command-authority-icd.md` | Watchdog paths attributed to baseline 13 with evidence level; watchdog-to-failsafe `NOT-EVIDENCED` stated | Watchdog evidence |
| `13-security-icd.md` | "Both are verified by `AUDIT`" changed to "assigned"; "No audit has been performed" added | Terminology |
| `16-ui-state-icd.md` | `IF-UIS-014` renamed, reframed and re-evidenced; override-resolution mechanism section added with five source locations; divergence rule block quote added; drift Phase 4 dependency stated; "not specified" row corrected | Factual correction |
| `18-error-degradation-model.md` | `DEG-08` renamed and its response rewritten; watchdog relationship table added; "upstream mechanism is verified" reworded | Watchdog evidence, terminology |
| `21-phase-3-risks.md` | R3-001 statement, summary row and column header reworded; R3-007 statement and summary row reworded | Terminology |
| `23-build-readiness.md` | Smoke-test assertions corrected and extended from three to four; Q-020 residual restated; `QGC_CUSTOM_DIR` and the overlay contract named; "verified by `AUDIT`" changed to "assigned" | Factual correction, terminology |
| `phase-3-completion-report.md` | R3-001 and R3-007 summary rows reworded to match | Terminology |
| `24-phase-3.1-correction-audit.md` | **New.** This document | Addition |

Thirteen existing documents edited; one added. No document was regenerated, duplicated or renamed.

### Counts

| Item | Count | Required |
| --- | --- | --- |
| Interface identifiers added | **0** | 0 |
| Interface identifiers removed | **0** | — |
| Interface identifiers renumbered | **0** | — |
| Requirements added | **0** | **0** |
| Requirements changed | **0** | 0 |
| Requirements promoted `PROPOSED` → `CONFIRMED` | **0** | **0** |
| Question identifiers added | **0** | 0 |
| Questions closed | **0** | **0** |
| Risk identifiers added | **0** | 0 |
| Risks closed or downgraded | **0** | — |
| ADRs added or changed | **0** | 0 |
| QGC source files modified | **0** | **0** |
| C++, QML, CMake, MAVLink dialect files created | **0** | **0** |
| Tests created | **0** | **0** |
| Phase 3.1 findings recorded | 8 | — |

### Status classifications changed

| Item | Before | After | Basis |
| --- | --- | --- | --- |
| `IF-UIS-014` evidence | `NOT-EVIDENCED` | `GCS-VERIFIED` mechanism; Aerora wiring `NOT-EVIDENCED` | Five source locations in the checkout |
| `IF-UIS-014` status | `OPEN` | **`OPEN`, unchanged** | The residual genuinely requires a build |
| `IF-UIS-014` authority | "Undetermined" | "Ground station" | The interceptor is installed by Aerora's own plugin override |
| `DEG-08` evidence | `MODEL` | `MODEL` via baseline 13; reported linkage `NOT-EVIDENCED` | Separation of aircraft-side from reported |
| Q3-001 gate | `G-IMPL` | `G-ARCH` | The answer can invalidate ADR-002 |
| Interface status distribution | 82 / 102 / 56 | **82 `CONFIRMED` / 102 `PROPOSED` / 56 `OPEN`, unchanged** | No promotion, no closure |

---

## 9. Verification

Documentation and repository consistency checks only. **No build, no runtime, no test execution.**

| Check | Tool | Result |
| --- | --- | --- |
| Markdown lint, whole `.kiro` tree | `markdownlint-cli` **0.49.1** — the version the repository pins in `.pre-commit-config.yaml` — with the repository's own `.markdownlint.json` | **0 findings** |
| Lint positive control | Same tool, probe config lowering the line-length limit to 60 | **3,759 findings** across the Phase 3 set, proving the linter reads these files rather than skipping them |
| Interface consistency | Local checker | **240 interfaces**, each defined in exactly one interface control document; **0** defined twice; **0** referenced but undefined |
| Traceability | Local checker | **0** orphan interfaces; every interface present in document 19 and document 22 |
| Identifier uniqueness | Local checker | **0** duplicate `Q3-` or `R3-` definitions; 9 Q3 and 8 R3 defined and all references resolve |
| Identifier existence | Local checker against a freshly extracted ground truth | **0** nonexistent requirement, risk, question or ADR citations |
| Requirement coverage | Local checker | **120 / 120** Phase 2 requirements referenced |
| Table shape | Local checker, escaped-pipe aware | **0** rows whose cell count differs from their header, across all 25 documents |
| Internal links | Local checker | **0** broken links |
| Ground truth | Local extractor over Phases 1, 2 and 2.1 | 120 Phase 2 requirements, 55 Phase 1-only, 29 risks `R-001..R-029`, 43 questions `Q-001..Q-043`, 11 ADRs, 17 Phase 1 documents, 29 Phase 2 documents |
| Repository diff | `git diff --stat HEAD` | **Empty.** Untracked: `.kiro/` and the product baseline folder |

### Not run, and why

| Tool | Reason |
| --- | --- |
| `cmake`, `ninja`, a C++ compiler, any Qt installation, `just`, `ctest` | **Absent from the host.** Re-verified in this phase: no `cmake`, no `ninja`, no `cl`, no `qmake`, no `just`, no `build/` directory |
| `pre-commit`, `clang-format`, `clang-tidy`, `qmllint`, `clazy`, `vale` | Absent. A partial run would produce a misleading result |
| QGC build, unit tests, QML tests, MockLink tests, overlay smoke test | Require the above |

**No QGroundControl build or runtime verification is claimed.** The build environment remains
unestablished. Every source-derived statement in this audit was obtained by reading files, and each
cites its location.

---

## 10. Phase 4 entry gate

| Item | Status |
| --- | --- |
| Phase 3 ICD accepted | **ACCEPTED** |
| Watchdog wording audited | **FINDING** — three wordings corrected; two non-relationships now explicitly `NOT-EVIDENCED`; no enumeration, threshold or action added |
| Interface terminology audited | **FINDING** — thirteen wordings corrected; contract/implemented/verified distinction added; 0 interfaces implemented, 0 verified |
| OPEN interfaces isolated | **FINDING** — 52 of 56 isolated; 4 have no safe isolation strategy (F3.1-01 to F3.1-04) |
| UI drift boundary audited | **FINDING** — override resolution mechanism corrected; six required statements verified present; drift checker is a Phase 4 deliverable |
| Q3 questions audited | **PASS** — nine inspected, none closed, one gate raised |
| R3 risks audited | **PASS** — eight inspected, none closed or downgraded; five carry full remaining exposure |
| New requirements | **0** |
| New interface, question or risk identifiers | **0** |
| Requirements promoted | **0** |
| Questions closed | **0** |
| QGC source modifications | **0** |
| Implementation started | **NO** |
| Phase 4 ready | **YES — conditional** |

### The condition

Phase 4 may begin on the documentation set. It may **not** treat the following as deferred:

| Must be settled before the work it gates | Gate | Finding |
| --- | --- | --- |
| A build host — CMake 3.25+, Qt 6.11.1 with the declared modules, Ninja, a C++20 compiler | Everything. Nothing else can start | R-009 |
| An Aerora repository with upstream as a **fetch-only** remote | The first commit | R-008 |
| The radio bench observation | ADR-002, and six transport core edits | F3.1-02, Q3-001 |
| A route for signing-key storage — upstream contribution or a recorded core edit | Release, not implementation | F3.1-01, ADR-008 |
| An audit-store integrity mechanism | Any claim that `AERORA-GCS-REQ-028` is met | F3.1-03 |
| An operator role set | A deployable configuration, not the code | F3.1-04, Q-029 |
| The capability inventory field definition | Whether the capability model can be populated | F3.1-05, Q3-009 |

The first two are the critical path and neither is an engineering problem.

Phase 4 must also inherit two rules intact.

> An **interface contract** is not an **implemented interface**, and an implemented interface is not
> a **verified interface**. All 240 are contracts. None is implemented. None is verified.
>
> **"No QGroundControl source modification" does not mean "no behavioural divergence from
> upstream."** No claim of zero behavioural divergence may be made without drift evidence.

---

PHASE 3.1 COMPLETE — READY FOR HUMAN REVIEW

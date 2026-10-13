# Risk Register and Unresolved Decisions

Owner: **Tab 1**. Every risk, every open decision, every blocker, with a named owner.

Status vocabulary: [00-README](00-README.md#status-vocabulary).

## Contents

- [1. Blockers](#1-blockers)
- [2. Open decisions](#2-open-decisions)
- [3. New open decisions raised by the benchmark](#3-new-open-decisions-raised-by-the-benchmark)
- [4. Risk register](#4-risk-register)
- [5. Risks the overlay already mitigates](#5-risks-the-overlay-already-mitigates)
- [6. Honesty risks](#6-honesty-risks)
- [7. Decision escalation](#7-decision-escalation)

## 1. Blockers

Four. Each stops a class of work entirely.

| ID | Blocker | Owner | Stops | Status |
| --- | --- | --- | --- | --- |
| `D-006` | **Build host.** No `cmake`, `ninja`, `qmake`, `ctest`, `qmllint`, `clang-format`, `clang-tidy`, `pre-commit` or `just`. `C:\Qt` absent. Disk insufficient; no administrator elevation — two separate gates | **User** | Every compile, lint, test, launch and `VERIFIED` claim | `NO-GO` — re-measured this pass, unchanged |
| `D-005` | **Aerora git origin.** `origin` is `https://github.com/mavlink/qgroundcontrol.git`, fetch **and push** | **User** | Any Aerora push | `BLOCKED` |
| `D-007` | **Product authorization.** External evidence required | **External** | Every product feature (`P7-PROF-003`) | `EXTERNAL EVIDENCE REQUIRED` |
| `Q-029` | **Operator role set.** No approved roles exist, so the authorization gate evaluates nothing | **Product + operations** | All authorization, all operator action controls, the payload sink, autonomy approval | `OPEN` |

### Why `D-006` is listed first

It does not block a single screen. It blocks every *claim*. 105 C++ files and 4 QML files have never
been through a compiler or a linter, 14 tests have never run, and the honest ceiling for all Phase 7
work is `IMPLEMENTED-UNCOMPILED`. That is not a small caveat — it means the programme is producing
reviewed source rather than verified software, and the gap between those two is unmeasured.

**The build-host prohibition is unchanged by Phase 7.** No build was attempted.

### Why `Q-029` is a blocker, not a decision

It is both, but its effect is a hard stop. `AeroraCorePlugin::authorizeMavCommand()` returns `true`
and warns once, and `isAuthorizationEnforced()` reports `false`. The station is honest about it,
which is the best available position, and it means **the product's second differentiator does not
exist yet** ([02 §7.2](02-gcs-reference-benchmark.md#72-operator-roles-permissions-and-a-command-audit-trail)).

## 2. Open decisions

Carried from Phase 6 plus Phase 7 additions. 23 entries.

### Product decisions

| ID | Decision | Blocks | Status |
| --- | --- | --- | --- |
| `Q-029` | Operator role set | See [§1](#1-blockers) | `OPEN` |
| `Q-021` / `Q-023` | Aerora complex item and mission command set | 5 screens, journeys 6.1 and 6.5 | `OPEN` |
| `Q-036` | Freshness budget values | `P7-MOD-013`, `P7-TEL-002`, `SCR-TEL-03` content | `OPEN` — **no value invented** |
| `Q-024` | Mission/payload command interaction | Fact-group implementation gate | `OPEN` |
| `Q4-004` | Consequences of disabling vendor firmware factories | — | `OPEN` — neither factory disabled |
| — | Fleet identity format | 2 screens, journey 6.7. **MAVLink sysid is explicitly not it** | `OPEN` |
| — | Power thresholds, arbitration, tether/battery transition, safety actions | `P7-PWR-003`..`006` | `OPEN` — **none invented**; four `false` flags and `kDefinedThresholdCount == 0` enforce it |
| — | Payload ICD / painting reported-value set | `SCR-LIV-03`, `SCR-MIS-05`/`06`, journey 6.1 | `OPEN` |
| — | Reported-consumable set and an approved consumption model | `SCR-MIS-06` | `OPEN` — [benchmark §7.4](02-gcs-reference-benchmark.md#74-consumable-constrained-multi-pass-work) |
| — | Must an unauditable *allow* be denied? | `P7-AUDIT-003` | `OPEN` — losses counted, commands not blocked |
| `D-NAV-01` | Top-level entry: hub (decided) vs `SelectViewDropdown.qml` fork | Nothing — hub is sufficient | **DECIDED** (hub); fork deferred and available |

### Architecture decisions

| ID | Decision | Owner | Blocks | Status |
| --- | --- | --- | --- | --- |
| `ADR-014` | Outbound authorization scope, Options A/B/C | Architecture | Any general authorization claim | `OPEN — ARCHITECTURAL DECISION REQUIRED` |
| `P7-DAT-002` | Local persistence design | Tab 1 | 7 screens; carries `Q-037` durability but not tamper-evidence | **CLOSED — `DESIGNED`.** [09](09-local-persistence-architecture.md). Seven new decisions `D-DAT-01`..`07` fall out of it, four of them product calls |
| `P7-AI-004` | GPU/autonomy interface | Architecture + external | 2 screens, journey 6.4 | `OPEN` — **no endpoint, schema or response may be invented** |
| `D-UI-06` | Multi-station authority | Architecture | `SCR-SHL-03`, `SCR-LIV-07` | `OPEN` — [new, §3](#3-new-open-decisions-raised-by-the-benchmark) |
| — | Authorization lifetime: re-evaluate on retry? | Architecture + Tab 2 | — | `OPEN` — bounded to enqueue time; **no second check added** |
| — | Absorb `aerora/src/Supervisory/` into Telemetry? | **Tab 1** | — | `OPEN` — **deferred**; ownership resolved to Tab 4 instead |

### Engineering decisions

| ID | Decision | Owner | Blocks | Status |
| --- | --- | --- | --- | --- |
| `Q-037` / `Q4-003` | Audit integrity mechanism | Tab 2 | 2 screens, journey 6.6 | **`PARTIALLY RESOLVED`** — [09](09-local-persistence-architecture.md) designs durability (WAL + `synchronous=FULL` for audit, `ON DELETE RESTRICT`, write-failure accounting). **Tamper-evidence stays `OPEN`** and is now explicitly blocked on `D-DAT-05`, a protected chain anchor, which is blocked on `AeroraPlatformKeyProtector` having a backend, which is blocked on `D-006` |
| — | Platform key protection per OS | Tab 2 + architecture | `P7-SEC-003` | `OPEN` — **no backend on any of the 5 platforms, and gated on `D-006`: an untested crypto backend is worse than none** |
| `Q-035` / `R-021` | Overlay test configuration leg | Tab 8 + CI | All denial-path tests | `OPEN` |
| `Q-018` | Aerora `MAV_AUTOPILOT` value | Firmware | — | `OPEN` — **Aerora claims `Generic`; no value invented** |
| `Q-007` | Flight-mode / authority mapping | Firmware + product | `P7-MOD-030` content | `OPEN` |
| `Q3-009` | Capability presence reporting | Firmware | `SCR-VEH-03` content, autonomy preconditions | `OPEN` |
| `IF-MAV-022` | Telemetry source attribution | Dialect | `P7-TEL-005` | `OPEN` |
| `D-004` | Radio transport / ADR-002 | Hardware | `P7-RAD-005` | `DEFERRED` — **no radio attached** |
| `D-UI-01` | QML-safe enforcement read path | Tab 2 + Tab 7 | `SCR-SHL-03`, `SCR-SEC-01` | `OPEN` |
| `D-UI-02` | `UI-A11Y-03` contrast target value | Tab 7 + product | Acceptance | `OPEN` |
| `D-UI-05` | Aerora Qt Quick Test target timing | Tab 1 + Tab 7 | 16 QML tests | `OPEN` — recommend at first need |
| `D-UI-07` | Should `AeroraUiSafetyTest::auditedTypes()` be **derived** rather than hand-listed? | Tab 7 | Nothing today; it is the durable fix for `R7-19` | `OPEN` — raised this pass. A hand-maintained audit list fails the same way as the surface it guards |
| `Q-040` | Visual distinguishability verification | Product | `DEMO` verification | `OPEN` |

### Resolved this pass

| ID | Decision | Resolution |
| --- | --- | --- |
| `P7-AUTH-003` | `src/FactSystem` / `PARAM_SET` ownership | **`OPEN — UNASSIGNED` → `OPEN — ASSIGNED (Tab 9)`.** The decision is still open; the owner is not. [07 §2](07-phase-7-work-packages-and-ownership.md#2-tab-numbering-resolution) |
| — | Tab 7 numbering collision (UI vs Fact System) | **Resolved.** Tab 7 = GUI, Tab 8 = build/test, Tab 9 = Fact System |
| — | `aerora/src/Supervisory/` ownership | **Resolved to Tab 4.** The directory move stays deferred |
| `D-NAV-01` | Top-level entry point | **Decided: hub via `analyzePages()`. Zero forks.** Fork deferred and available |
| `D-UI-03` | `AeroraStateValue::capability` `WRITE` | **CLOSED.** U3, `a7b317f89`, `IMPLEMENTED-UNCOMPILED` |
| `D-UI-04` | `AeroraControlsModule` missing `IMPORTS QGC` + alias | **CLOSED.** U2, `a73699796`, `IMPLEMENTED-UNCOMPILED` |
| HO-TAB7-02 | Interceptor install | **CLOSED.** U1, `84bbb3a6e`, `IMPLEMENTED-UNCOMPILED` |
| `P7-DAT-002` / `P7-DAT-005` | Local persistence | **CLOSED at `DESIGNED`.** [09](09-local-persistence-architecture.md) |

### New decisions raised by WP1-C

Seven, in [09 §14](09-local-persistence-architecture.md#14-open-decisions). Four are product calls
(`D-DAT-01` retention caps, `D-DAT-02` whether audit may be deleted at all, `D-DAT-03` fleet
identity, `D-DAT-06` encryption at rest), one is security architecture (`D-DAT-05` protected chain
anchor), two are engineering (`D-DAT-04` event categories, `D-DAT-07` multi-instance behaviour).

**No default retention value was invented.** A retention period carries regulatory weight in some
domains, and a placeholder would outlive the decision it substituted for (`P7-NOINV-002`).

## 3. New open decisions raised by the benchmark

Three. Each is a finding from [02](02-gcs-reference-benchmark.md), not a pre-existing gap.

### `D-UI-06` — multi-station authority

| Field | Detail |
| --- | --- |
| Raised by | **Veronte** (multiple interconnected stations operating simultaneously) and **Visionair** (an explicit "designated operator" concept) |
| The gap | Aerora supports multi-*vehicle* through upstream `MultiVehicleManager`. It has **no requirement covering multi-*station* authority** — which station holds authority, how it is displayed, how it transfers |
| Why it matters | Two independent commercial products treat this as a first-class concept. An industrial customer with two operators will ask |
| Owner | **Architecture** |
| Blocks | `SCR-SHL-03`, `SCR-LIV-07` content. Not blocking today — Aerora is single-station |
| Status | `OPEN` |

### Consumable-constrained multi-pass work

| Field | Detail |
| --- | --- |
| Raised by | **QBase 3D** segments large sites by flyable area; **eMotion** manages battery changes. **Neither models a job bounded by a payload-carried consumable** |
| The gap | Painting, firefighting and agriculture are all consumable-bounded, not battery-bounded. No reference product addresses it |
| Constraint | `P7-ARCH-005` prohibits a station-derived consumption projection without an approved method. `kRemainingEnergyEstimationImplemented = false` is the precedent |
| Owner | **Product + engineering** (Tab 6 specifies, product approves the model) |
| Blocks | `SCR-MIS-06`, and the realistic form of journey 6.1 |
| Status | `OPEN` |

### HowayGIS product identity

| Field | Detail |
| --- | --- |
| The gap | "HowayGIS" resolves to Shanghai HowayGIS Infotech, a GNSS and geospatial vendor whose documented products are field devices. **No GCS or flight-control software was located** |
| Action needed | If a specific HowayGIS flight application is intended, the exact product name. Otherwise the entry is dropped |
| Also unresolved | "MissionGuidance" and "Route Planning" did not resolve to products. "U-Pilot" is an **autopilot** — the GCS counterpart is U-See, which was benchmarked instead |
| Owner | **Product** (whoever supplied the reference list) |
| Blocks | Nothing. The benchmark has 15 verified entries |
| Status | `OPEN` |

## 4. Risk register

Ranked by exposure. Probability and impact are judgements; the evidence column is not.

| ID | Risk | P | I | Evidence | Mitigation | Owner |
| --- | --- | --- | --- | --- | --- | --- |
| `R7-01` | **The first build reveals substantial compile and lint failure**, and the gap between "reviewed source" and "working software" turns out to be large | **High** | High | 105 C++ files never compiled; 4 QML files with **zero** tooling applied. Declaration/definition parity checked by reading only | Build module by module, not all at once. Treat the first result as a measurement, not a regression. [04 §10](04-subsystem-and-interface-completion-matrix.md#10-first-build-expectations) sets the expectation in writing | Tab 8 |
| `R7-02` | **`QGC_CUSTOM_BUILD` is not set and nobody notices.** The overlay compiles, the application starts, the authorization gate is never consulted | Medium | **Critical** | ADR-012. `QGCCorePlugin::instance()` routes to `CUSTOMCLASS` only under that definition | Make the CI leg **assert** it. Surface it in `SCR-SHL-04` provenance. This is the worst failure mode in the build because it is invisible | Tab 8 + Tab 1 |
| `R7-03` | **`Q-029` stays open indefinitely**, and the product ships with an authorization point that permits everything | Medium | **Critical** | `isAuthorizationEnforced()` returns `false` today | `P7-FLY-004` is `FORBIDDEN-UNTIL`, so no action control can ship into the gap. `SCR-SHL-03` makes the state visible to the operator. **Escalate — see [§7](#7-decision-escalation)** | Product |
| `R7-04` | **A tab invents a value to unblock itself** — a freshness budget, a threshold, a capability, a MAVLink identifier | Medium | High | 11 `OPEN` product decisions block real work. The temptation is structural | The ten `false` flags, `kDefinedThresholdCount == 0`, and the static MAVLink-identifier check. `P7-NOINV-002`: emit the requirement ID, not a number | All tabs; Tab 1 enforces |
| `R7-05` | **The class-X fork count rises above 0** and upstream rebase stops being mechanical | Medium | High | 3 fork candidates identified, all currently declined | `AeroraQmlOverrides::forbiddenPaths()` + `AeroraUiFoundationTest`. Every class-X fork needs a recorded fork point, a named owner and a drift review (`P7-ARCH-023`) | Tab 7; Tab 1 approves |
| `R7-06` | **A source is added without a manifest entry.** It compiles into nothing, silently | Medium | Medium | The manifest is hand-maintained; the aggregation does not glob | Re-run `aerora-manifest-check.ps1` after **every** commit that adds a file. It has caught 0 problems so far across 109 entries | All tabs |
| `R7-07` | **`AeroraUiSafetyTest` fails on first run due to a list error**, and the failure is read as a regression or, worse, the test is weakened to make it pass | **High** | High | Its reviewed-method lists were derived by reading headers, never by running | Recorded in advance ([04 §8](04-subsystem-and-interface-completion-matrix.md#coverage-gaps)). **Fix the list, never the assertion.** The test is the QML safety boundary | Tab 7 |
| `R7-08` | **The 19 `SPECIFIED-NOT-DESIGNED` screens get designed anyway**, by inventing the decisions that block them | Medium | High | 16 of 19 are decision-blocked. Schedule pressure acts on exactly these | [05 §4](05-implementation-dependency-graph.md#4-what-is-blocked-and-by-what) names the blocker per screen. A design that resolves its own blocker must be rejected in review | Tab 1 |
| `R7-09` | **`D-006` is never resolved** and the programme accumulates unverified source indefinitely | Medium | **Critical** | 109 files, 14 tests, zero executions, across Phases 4–7 | Every document caps its claims at `IMPLEMENTED-UNCOMPILED`. **Escalate — the cost of this risk compounds with every commit** | User |
| `R7-10` | **`geoMapVisualQML()` is omitted** on the Aerora complex item and the item is silently invisible under the GeoMap engine | Medium | Medium | `mapVisualQML()` is pure virtual and enforced; `geoMapVisualQML()` returns `QString()` by default | `SCR-MIS-04` is a **separate** inventory entry so the omission is visible in the screen count, not buried in an item's definition of done | Tab 5 |
| `R7-11` | **An interface gets a mock implementation** to make a screen demonstrable | Low | **Critical** | 8 interfaces with no implementation, 6 of them decision-gated | `AeroraPayloadCommandSink` and `AeroraPlatformKeyProtector` are deliberately unimplemented for safety. Phase B rule: **identify missing functionality, do not create mock production behaviour** | All tabs; Tab 1 enforces |
| `R7-12` | **`registerCustomSettingsGroup()` is called with a wrong accessor name** and the group is silently deleted | Medium | Low | The call **takes ownership** and deletes a rejected group. Accessor must be camelCase JSON stem + `Settings`, matching `^[a-z_][A-Za-z0-9_]*$`, no collision with a built-in | Record the exact expected accessor name in the settings specification. A startup assertion would be better | Tab 7 |
| `R7-13` | ~~U3 lands without the test entry removal~~ | — | — | — | **CLOSED.** U3 landed with both halves in one change (`a7b317f89`). The deviation list is now empty, so the assertion is stricter and this failure mode no longer exists | Tab 4 |
| `R7-14` | **Upstream drift breaks a consumed seam silently.** A signature change is a compile error (safe); a semantic change is not | Low | High | The interceptor's self-check reports `isInstalled() == false` if the engine is created elsewhere. Other seams have no such check | WP8-D drift tooling. Prefer seams whose breakage is a compile error | Tab 8 |
| `R7-15` | **`PARAM_SET` remains unowned** and someone claims Aerora authorizes vehicle reconfiguration | Low | High | `ParameterManager.cc:319`, `:398`. The command gate covers `MAV_CMD` only | Tab 9 created. `P7-AUTH-003` now assigned. **No parameter-authorization claim is supportable until WP9-A completes** | Tab 9 |
| `R7-16` | **Accessibility and WCAG get claimed** on the strength of the design rules rather than testing | Low | Medium | `P7-SET-005` already says conformance is not claimed | Keep `P7-SET-005` in every deployment document. Conformance needs assistive-technology testing and expert review | Tab 7 |
| `R7-17` | **The hub becomes a dashboard of green tiles** that are green because nothing was checked | Medium | Medium | It is the default shape for a hub screen | `SCR-SHL-01`'s anti-requirement is explicit: it must be the overlay's honest self-report, including what did **not** attach | Tab 7 |
| `R7-18` | **A latent type error blocks a whole test binary at first build.** Confirmed, not hypothetical | **Confirmed** | Medium | `AeroraMissionTest.cc` passes `QString` to `expectLogMessage(const char *)` at 11 sites. One declaration, no overload, no conversion | `HO-TAB1-01` to Tab 5. The scan that found it also cleared the other 47 call sites and verified every category string, so the class is closed rather than sampled | Tab 5 |
| `R7-19` | **A new QML-reachable type is added without being added to the safety audit**, so `P7-AUTH-004`'s ratchet silently stops covering the whole surface | **Occurred** | High | 12 QML-registered overlay types, 10 in `auditedTypes()`. `AeroraSecurityStatus` arrived with Tab 2's WP2-B; `AeroraFreshness` has been uncovered since Phase 6 | `HO-TAB1-03` to Tab 7. **The durable fix is a test that derives the audited set rather than listing it** — an audit whose membership is hand-maintained has the same failure mode as the thing it audits. Recorded as `D-UI-07` | Tab 7 |
| `R7-19` | **A status row is closed while the prose still describes the defect as live.** Observed, not hypothetical | **Observed** | Medium | `HO-TAB4-03`'s first pass closed `D-UI-03`, HO-TAB7-01 and `R7-13` but left five passages in the same six documents calling the writable property a live defect | A documentation handoff closes when a grep for the old claim comes back empty, not when the status column flips. [07 §7](07-phase-7-work-packages-and-ownership.md#7-handoff-protocol) records the rule | Tab 1 |

### Exposure summary

| Impact | Count | IDs |
| --- | --- | --- |
| **Critical** | 4 | `R7-02`, `R7-03`, `R7-09`, `R7-11` |
| High | 7 | `R7-01`, `R7-04`, `R7-05`, `R7-07`, `R7-08`, `R7-14`, `R7-15` |
| Medium | 5 | `R7-06`, `R7-10`, `R7-16`, `R7-17`, plus `R7-12` at low impact |
| High probability | 2 | `R7-01`, `R7-07` — both first-build effects, both pre-recorded |

The four critical risks share a shape: **each is a way for the programme to look finished while
something load-bearing is absent**. `R7-02` is an invisible build misconfiguration, `R7-03` an
enforcement point that permits everything, `R7-09` source that was never verified, `R7-11` a mock
standing in for a real interface. The mitigations are correspondingly uniform — make the absence
visible, mechanically, and refuse to let a status column hide it.

## 5. Risks the overlay already mitigates

Worth recording, because these are design decisions that bought down real risk and could be undone
by someone who does not know why they are there.

| Risk | Mitigation already in place |
| --- | --- |
| A QML write manufactures a state the vehicle never reported | `AeroraUiSafetyTest` asserts the writable property set is **empty** since U3. The one recorded deviation is gone, so the assertion is stricter than when it was written |
| The UI acquires an actuating path | The same test reads the meta-objects of all ten QML-reachable types and fails when the surface grows |
| A station-side threshold acquires safety meaning | `kDefinedThresholdCount == 0`, asserted by `AeroraPowerModelTest` |
| A payload actuator appears | `AeroraPayloadCommandSink` has no implementation; two `false` flags assert it |
| An energy projection is presented as reported data | `kRemainingEnergyEstimationImplemented = false` |
| A populated upstream file is forked by accident | `forbiddenPaths()` + `AeroraUiFoundationTest`; the `CustomMapItems.qml` mistake is **unrepresentable**, not merely discouraged |
| The URL interceptor affects a resource it should not | Explicit compile-time allow-list, then existence. Asserted over six URL shapes including `MainWindow.qml` and a core `:/res` icon |
| A test mock reaches a shipped binary | Test sources gated on `QGC_BUILD_TESTING`; `AeroraVehicleMock` additionally `#error`s without `QGC_UNITTEST_BUILD`. **Both guards must be defeated together** |
| A module is dropped and the vehicle silently gets an incomplete service set | `AERORA_VEHICLE_REQUIRED_MODULES` + the configure-time sentinel assertion with `FATAL_ERROR` |
| A static-registration object is discarded by the linker | Sources compile into the application target, not a static archive. Documented as required, not incidental |
| A firmware behaviour is assumed rather than evidenced | `AeroraFirmwarePlugin` derives from the base; four safe defaults are **not** overridden and `AeroraVehicleIntegrationTest` asserts each one |
| Mission resume replays station-side payload state | `kGcsPayloadStateReplayImplemented = false` |

This list is the overlay's strongest property. Each entry converts a prohibition from a claim into an
assertion, and that is why [06 §7](06-requirements-to-test-traceability.md#a-note-on-the-static-class)
can verify 18 requirements on a host with no compiler.

## 6. Honesty risks

A distinct category: ways the programme could misrepresent its own state without anyone intending to.

| # | Risk | Guard |
| --- | --- | --- |
| 1 | `IMPLEMENTED-UNCOMPILED` gets reported as "implemented" in a summary, a slide or a conversation | The status is defined in [00-README](00-README.md#status-vocabulary) and used uniformly. `VERIFIED` appears nowhere |
| 2 | `DESIGNED` radio interfaces get described as "supported" | `P7-RAD-003` and `P7-RAD-004` exist specifically for this. **No radio hardware has been attached** |
| 3 | A `CONTRACT-ONLY` interface gets counted as a delivered subsystem | [04 §5](04-subsystem-and-interface-completion-matrix.md#5-interface-completion) separates the three interface categories and counts them |
| 4 | "The station authorizes commands" is said while the policy permits everything | `isAuthorizationEnforced()` returns `false`, and `SCR-SHL-03` displays it |
| 5 | "The station authorizes vehicle reconfiguration" is said while `PARAM_SET` is unowned | `R7-15`; Tab 9 assigned |
| 6 | Regulatory or standards conformance gets claimed | `P7-NOINV-001` includes regulatory compliance claims. Veronte's "partial STANAG 4586" is the cautionary precedent |
| 7 | A benchmark feature gets attributed to a product whose identity was not verified | [02 §2](02-gcs-reference-benchmark.md#2-identity-corrections) reports all three corrections rather than substituting plausible products |
| 8 | A screen count is quoted as a completion measure | 45 screens, of which 2 have a service behind them and 19 are decision-blocked. The breakdown accompanies the number everywhere it appears |
| 9 | Test *registration* gets reported as test *coverage* | 14 tests registered, **0 executed**. Stated in [04](04-subsystem-and-interface-completion-matrix.md) and [06](06-requirements-to-test-traceability.md) |
| 10 | "Production ready" gets said | Prohibited until actual build, tests and hardware validation support it. Phase D ends with an explicit statement that it is **not** claimed |

## 7. Decision escalation

Three decisions should be escalated now, because each one's cost grows with time.

### 1. `D-006` — the build host

| Field | Detail |
| --- | --- |
| Owner | **User** |
| Cost of delay | **Compounds with every commit.** Every file added without a compiler is another file whose correctness is assumed. 109 files and 14 tests are already in that state, accumulated across four phases |
| What resolving it buys | The first real measurement of the overlay. `R7-01` and `R7-07` both resolve into facts rather than probabilities |
| Minimum needed | CMake ≥ 3.25, Ninja, MSVC, Qt 6.11.1, sufficient disk, and administrator elevation — the last two are separate gates, and cache cleanup was already established as insufficient (`R4-003`) |
| Recommendation | Resolve before Phase C adds substantially more unverified source. If a local host is not viable, a CI-only build is better than none |

### 2. `Q-029` — the operator role set

| Field | Detail |
| --- | --- |
| Owner | **Product + operations** |
| Cost of delay | Blocks the longest chain in the programme: `Q-029` → ADR-014 → policy → command path → **every operator action**. Journeys 6.2 and 6.4 cannot complete |
| What it is not | An engineering decision. The gate, the chain and the enforcement point are all implemented. What is missing is the list of roles an operating organisation will actually authorise |
| Minimum needed | The role names, and for each, which command classes it may authorise. Not a full policy — enough to make the policy implementable |
| Recommendation | A small decision with a large unblock. Highest ratio of any open item |

### 3. `P7-DAT-002` — the persistence design

| Field | Detail |
| --- | --- |
| Owner | **Tab 1** |
| Cost of delay | 5 screens blocked, and `Q-037` audit durability stays a separate unsolved problem when one mechanism could serve both |
| Why escalated | It is the **only** high-leverage blocker Tab 1 can resolve without external input. Leaving it open while waiting on product decisions is self-inflicted |
| Recommendation | WP1-C, immediately, in parallel with U1 and U2 |

### Decisions deliberately not escalated

| Decision | Why not |
| --- | --- |
| `D-005` git origin | Blocks pushes only. Work accumulates locally without loss, and the checkpoint branch is intact |
| `D-004` radio transport | No radio attached. Escalating a hardware decision before hardware exists adds nothing |
| Supervisory absorption | Cosmetic. Ownership is resolved, which is what actually mattered |
| `Q-040` visual distinguishability | Needs screens to look at first |
| `D-UI-02` contrast target | Needs a design system to apply it to first |

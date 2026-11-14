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

**Two.** `D-005` and `Q-029` have since resolved and their rows are kept rather than deleted, so the
register reads as a history instead of only as a snapshot.

| ID | Blocker | Owner | Stops | Status |
| --- | --- | --- | --- | --- |
| `D-006` | **Build host.** No `cmake`, `ninja`, `qmake`, `ctest`, `qmllint`, `clang-format`, `clang-tidy`, `pre-commit` or `just`. `C:\Qt` absent. Disk insufficient; no administrator elevation — two separate gates | **User** | Local compile, lint, launch and any claim needing a running application or hardware. **No longer every compile and test** — the Aerora CI legs build the overlay and run its suites | `NO-GO` on this host — unchanged. Narrowed in effect by CI, not resolved |
| `D-007` | **Product authorization.** External evidence required | **External** | Every product feature (`P7-PROF-003`) | `EXTERNAL EVIDENCE REQUIRED` |
| ~~`D-005`~~ | **Aerora git origin.** `origin` stays `https://github.com/mavlink/qgroundcontrol.git` and has never been pushed to | **User** | — | **RESOLVED.** An `aerora` remote is configured and `aerora/phase7-product-completion` is published on it |
| ~~`Q-029`~~ | **Operator role set** | **Product + operations** | — | **RESOLVED.** `DG-ROL` is `CLOSED`; the role set is settled and signed in `closures/DG-ROL.role-set.json`. See below |

### Why `D-006` is listed first

It was listed first because it blocked every *claim*: 105 C++ files and 4 QML files had never been
through a compiler or a linter, 14 tests had never run, and the honest ceiling for all Phase 7 work
was `IMPLEMENTED-UNCOMPILED`.

**Three Aerora CI legs have since changed what it blocks, and the row above is narrowed rather than
closed.** `aerora-lint.yml`, `aerora-linux.yml` and `aerora-windows.yml` build the overlay and run
its registered suites on both platforms — 34 `Unit` and 9 `Integration` entries green on the latest
Linux run — so "never compiled, never executed" is no longer true of the overlay. What `D-006` still
blocks is unchanged and is not small: no build on the deployment host, no linter or QML tooling
locally, no running application to look at, and therefore no claim that rests on observing the
software behave. Every CI run is a Debug build against `MockLink` on a hosted runner, so the gap that
remains is not compilation but **execution against real hardware**.

The ceiling moved accordingly: from "reviewed source" to "compiled and CI-tested against a simulated
vehicle". That is a real rung, and it is still several below anything that may be called verified.

### `Q-029` was a blocker as well as a decision, and it has resolved

It was both, and its effect was a hard stop: `AeroraCorePlugin::authorizeMavCommand()` returns `true`
and warns once, and `isAuthorizationEnforced()` reports `false`. The station was honest about it,
which was the best available position, and it meant **the product's second differentiator did not
exist yet** ([02 §7.2](02-gcs-reference-benchmark.md#72-operator-roles-permissions-and-a-command-audit-trail)).

`DG-ROL` has since closed over both parts. The role set is settled and signed, `ADR-014` is selected
as Option C and signed, and both accepted records carry a `guardPermissions` verdict permitting
`isAuthorizationEnforced()` to become true. What remains is the three installables the predicate
reads — identity provider, role resolver and a configured policy — which is implementation rather
than decision. [15](15-operator-identity-and-the-single-role-deployment.md) records what specifies
each, and the one input still owed by Product: what authenticates an operator, as distinct from what
identifies them.

The source still reports `false` and that is still correct. It changes when something is installed,
not when a document says it may be.

## 2. Open decisions

Carried from Phase 6 plus Phase 7 additions. 23 entries.

### Product decisions

| ID | Decision | Blocks | Status |
| --- | --- | --- | --- |
| ~~`Q-029`~~ | Operator role set | — | **SETTLED AND SIGNED.** Four roles, `closures/DG-ROL.role-set.json`, Product and operations, 2026-11-05. See [§1](#1-blockers) and [15](15-operator-identity-and-the-single-role-deployment.md) |
| `Q-021` / `Q-023` | Aerora complex item and mission command set | 5 screens, journeys 6.1 and 6.5 | `OPEN` |
| `Q-036` | Freshness budget values | `P7-MOD-013`, `P7-TEL-002`, `SCR-TEL-03` content | `OPEN` — **no value invented** |
| `Q-024` | Mission/payload command interaction | Fact-group implementation gate | `OPEN` |
| `Q4-004` | Consequences of disabling vendor firmware factories | — | `OPEN` — neither factory disabled |
| ~~`D-DAT-03`~~ | Fleet identity format | — | **SETTLED, SIGNED AND BUILT.** `closures/DG-FLT.identity-format.json`, Product, 2026-11-05: a UUIDv4 minted by the station on first contact from `AUTOPILOT_VERSION.uid2` falling back to `uid`, permanent for the airframe, and derived from the board rather than the MAVLink sysid. `AeroraVehicleStore` implements it and is CI-tested |
| — | Power thresholds, arbitration, tether/battery transition, safety actions | `P7-PWR-003`..`006` | `OPEN` — **none invented**; four `false` flags and `kDefinedThresholdCount == 0` enforce it |
| — | Payload ICD / painting reported-value set | `SCR-LIV-03`, `SCR-MIS-05`/`06`, journey 6.1 | `OPEN` |
| — | Reported-consumable set and an approved consumption model | `SCR-MIS-06` | `OPEN` — [benchmark §7.4](02-gcs-reference-benchmark.md#74-consumable-constrained-multi-pass-work) |
| ~~`P7-AUDIT-003`~~ | Must an unauditable *allow* be denied? | — | **ANSWERED: deny for actuating classes.** `closures/DG-ROL.authorization-scope.json`, Architecture, 2026-11-05 — denied for `payload` and `manual-control`, counted and allowed for `command`, `parameter` and `mission`, so an audit outage cannot start refusing the command to land. Not yet implemented: the two actuating seams are unbuilt, and `AeroraCommandAuthorizer` counts without blocking today |
| `D-NAV-01` | Top-level entry: hub (decided) vs `SelectViewDropdown.qml` fork | Nothing — hub is sufficient | **DECIDED** (hub); fork deferred and available |

### Architecture decisions

| ID | Decision | Owner | Blocks | Status |
| --- | --- | --- | --- | --- |
| ~~`ADR-014`~~ | Outbound authorization scope, Options A/B/C | Architecture | — | **SELECTED: Option C**, signed in `closures/DG-ROL.authorization-scope.json`, Architecture, 2026-11-05. [Doc 30's own ADR-014 status line still reads `OPEN`](../03-implementation-specification/30-outbound-command-security-audit.md#adr-014-outbound-authorization-scope) and is Architecture's to update |
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
| `D-UI-01` | QML-safe enforcement read path | Tab 2 + Tab 7 | — | **CLOSED** — `AeroraSecurityStatus` (WP2-B). A read-only projection that holds no security state and reaches no security type; audited by `AeroraUiSafetyTest` with no writable property, no `Q_INVOKABLE` and no slot. Found still marked `OPEN` at this reconciliation after the code had landed |
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
| ~~`R7-01`~~ | **The first build reveals substantial compile and lint failure**, and the gap between "reviewed source" and "working software" turns out to be large | — | — | Was: 105 C++ files never compiled, 4 QML files with zero tooling | **LARGELY REALISED AND ABSORBED.** The overlay now builds and tests green on two platforms in CI, so the measurement was taken. It cost a run of compile errors fixed forward one at a time — missing includes, an incomplete type, a mis-suffixed Qt header — which is the predicted shape rather than a surprise. The residue is the Windows `AeroraProfileRegistryTest` process-exit timeout, a confirmed intermittent | Tab 8 |
| `R7-02` | **`QGC_CUSTOM_BUILD` is not set and nobody notices.** The overlay compiles, the application starts, the authorization gate is never consulted | Medium | **Critical** | ADR-012. `QGCCorePlugin::instance()` routes to `CUSTOMCLASS` only under that definition | Make the CI leg **assert** it. Surface it in `SCR-SHL-04` provenance. This is the worst failure mode in the build because it is invisible | Tab 8 + Tab 1 |
| `R7-03` | **The product ships with an authorization point that permits everything.** No longer because `Q-029` is open — that is signed — but because no credential seam can produce an authenticated operator | Medium | **Critical** | `isAuthorizationEnforced()` returns `false` today, and `authorizeMavCommand()` returns `true` with one latched warning | `P7-FLY-004` is `FORBIDDEN-UNTIL`, so no action control can ship into the gap. `SCR-SHL-03` and `SCR-SEC-01` make the state visible to the operator, and the audit trail now durably records every command that passes. **Escalate — see [§7.2](#2-q-029--the-operator-role-set-resolved--superseded-by-the-credential-seam)** | Product |
| `R7-04` | **A tab invents a value to unblock itself** — a freshness budget, a threshold, a capability, a MAVLink identifier | Medium | High | 11 `OPEN` product decisions block real work. The temptation is structural | The ten `false` flags, `kDefinedThresholdCount == 0`, and the static MAVLink-identifier check. `P7-NOINV-002`: emit the requirement ID, not a number | All tabs; Tab 1 enforces |
| `R7-05` | **The class-X fork count rises above 0** and upstream rebase stops being mechanical | Medium | High | 3 fork candidates identified, all currently declined | `AeroraQmlOverrides::forbiddenPaths()` + `AeroraUiFoundationTest`. Every class-X fork needs a recorded fork point, a named owner and a drift review (`P7-ARCH-023`) | Tab 7; Tab 1 approves |
| `R7-06` | **A source is added without a manifest entry.** It compiles into nothing, silently | Medium | Medium | The manifest is hand-maintained; the aggregation does not glob | Re-run `aerora-manifest-check.ps1` after **every** commit that adds a file. It has caught 0 problems so far across 109 entries | All tabs |
| ~~`R7-07`~~ | **`AeroraUiSafetyTest` fails on first run due to a list error**, and the failure is read as a regression or, worse, the test is weakened to make it pass | — | — | Its reviewed-method lists were derived by reading headers, never by running | **DID NOT OCCUR.** The suite runs in the Aerora `Unit` label on both platforms and passes, so the hand-derived lists were correct. `D-UI-07` — deriving the audited set rather than listing it — stays open as the durable fix, because the list being right once is not the same as it staying right | Tab 7 |
| `R7-08` | **The 19 `SPECIFIED-NOT-DESIGNED` screens get designed anyway**, by inventing the decisions that block them | Medium | High | 16 of 19 are decision-blocked. Schedule pressure acts on exactly these | [05 §4](05-implementation-dependency-graph.md#4-what-is-blocked-and-by-what) names the blocker per screen. A design that resolves its own blocker must be rejected in review | Tab 1 |
| `R7-09` | **`D-006` is never resolved** and the programme accumulates source that has never run on real hardware | Medium | **High** | Was 109 files with zero executions; CI has since removed the compile-and-test half of this. What remains has had **zero executions on the deployment host, zero on hardware and zero in flight** | CI caps the compounding, which is why the impact is no longer Critical. It does not substitute for hardware: a Debug build against `MockLink` cannot establish a flight claim. **Still escalate** | User |
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
| `R7-20` | **A status row is closed while the prose still describes the defect as live.** Observed, not hypothetical | **Observed** | Medium | `HO-TAB4-03`'s first pass closed `D-UI-03`, HO-TAB7-01 and `R7-13` but left five passages in the same six documents calling the writable property a live defect | A documentation handoff closes when a grep for the old claim comes back empty, not when the status column flips. [07 §7](07-phase-7-work-packages-and-ownership.md#7-handoff-protocol) records the rule | Tab 1 |
| `R7-21` | **Two tabs commit into one shared working tree and one publishes the other's unfinished state.** Occurred three times this session | **Occurred** | Medium | `a7b317f89` published Tab 4's U3 mid-validation; `7e41a529e` absorbed Tab 1's staged reconciliation and Tab 9's doc 10. No work was lost in any case, but `7e41a529e`'s message asserts `HO-TAB1-02` is `PARTIALLY APPLIED` while the document it committed says `CLOSED` | [07 §6 Who commits what](07-phase-7-work-packages-and-ownership.md#who-commits-what): stage by explicit path, never `git add -A`, verify `git diff --cached` before committing, and let the owner confirm the published state | Tab 1 |

### Exposure summary

| Impact | Count | IDs |
| --- | --- | --- |
| **Critical** | 3 | `R7-02`, `R7-03`, `R7-11` |
| High | 6 | `R7-04`, `R7-05`, `R7-08`, `R7-09`, `R7-14`, `R7-15` |
| Medium | 5 | `R7-06`, `R7-10`, `R7-16`, `R7-17`, plus `R7-12` at low impact |
| Retired by the first build | 2 | `R7-01` largely realised and absorbed, `R7-07` did not occur |

The three critical risks share a shape: **each is a way for the programme to look finished while
something load-bearing is absent**. `R7-02` is an invisible build misconfiguration, `R7-03` an
enforcement point that permits everything, `R7-11` a mock standing in for a real interface. The
mitigations are correspondingly uniform — make the absence visible, mechanically, and refuse to let
a status column hide it.

`R7-09` dropped from Critical to High for one reason only: CI removed the never-compiled half of it.
The half it did not remove is that nothing has run on the deployment host or on an aircraft, and
`R7-02` is the reason that still matters — the Aerora legs and `aerora-package.yml` each assert
`QGC_CUSTOM_DIR` is `aerora` before trusting a build, because without that assertion a green run
could be stock QGroundControl.

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
| 9 | Test *execution* gets reported as *validation* | The suites now run: 34 `Unit` and 9 `Integration` entries green on the Aerora Linux and Windows legs. Every one is a Debug build against `MockLink`, so what is established is that the code behaves against a simulated vehicle — not on hardware, and not in flight |
| 10 | "Production ready" gets said | Prohibited until actual build, tests and hardware validation support it. Phase D ends with an explicit statement that it is **not** claimed |

## 7. Decision escalation

Two of the three below have resolved since they were written, and their rows are kept rather than
deleted so this section reads as a history. **One live escalation replaces them**, and it is now the
single thing standing between the overlay and an enforcing command gate: see
[§7.2](#2-q-029--the-operator-role-set-resolved--superseded-by-the-credential-seam).

### 1. `D-006` — the build host

| Field | Detail |
| --- | --- |
| Owner | **User** |
| Cost of delay | **No longer compounding, and still real.** This row's own recommendation — "if a local host is not viable, a CI-only build is better than none" — was taken, and the Aerora legs now compile and test every push. What still accumulates is code that has never run on the deployment host, never on hardware and never in flight |
| What resolving it buys | What CI cannot: a running application on the target, QML tooling locally, and the hardware validation every flight claim rests on. `R7-01` has already resolved into fact through CI; `R7-07` resolved the same way |
| Minimum needed | CMake ≥ 3.25, Ninja, MSVC, Qt 6.11.1, sufficient disk, and administrator elevation — the last two are separate gates, and cache cleanup was already established as insufficient (`R4-003`) |
| Recommendation | The CI fallback is in place, so this is no longer the critical path for *correctness*. It is still the critical path for anything demonstrable: a reviewable installer is produced by `aerora-package.yml` on dispatch, and a Codespace can build and display the overlay through `.devcontainer/desktop`, but neither is the deployment host |

### 2. `Q-029` — the operator role set (resolved) → superseded by the credential seam

`Q-029` is **settled and signed**: four roles with their command classes, in
`closures/DG-ROL.role-set.json`, and `ADR-014` Option C signed alongside it. The decision that was
escalated here is closed, and the chain it blocked is unblocked as far as *decisions* go.

**What replaced it is narrower and is the live escalation.**
`AeroraCommandAuthorizer::isFullyConfigured()` requires three installables, and two of them are now
specified by signed records — the role resolver reads the signed table, the policy is `ADR-014`
Option C. The third cannot be built from anything in this repository:

| Field | Detail |
| --- | --- |
| The gap | Nothing can produce an **authenticated operator identity**. `AeroraCachedCredentialIdentityProvider` reports an operator only once something calls `setCredential()`, and the cloud login that would do so does not exist — its own header records that. [15 §10](15-operator-identity-and-the-single-role-deployment.md) names the authority as the Aeroradynamics cloud user database; no endpoint, credential format or client exists for it |
| Why it cannot be worked around | `IF-SEC-009` delivers **credential and role data together** from the cloud, inbound and non-blocking, and `IF-SEC-010` records that the station never calls out. So a local provisioning file would be inventing a second provisioning path the architecture does not have. A stand-in always-authenticated provider is the `R7-11` mock failure in its purest form: it would make the station report enforcement while a hardcoded identity passed every command |
| Consequence while it is open | `isAuthorizationEnforced()` is `false`, `authorizeMavCommand()` returns `true` for every command, and `R7-03` stands at **Critical**. The audit trail records the commands; nothing refuses one |
| Owner | **Product** for what issues a credential, **security architecture** for where it is stored given no protected store exists (`D-DAT-05`, `F3.1-01`) |
| Minimum needed | How a credential reaches the station, in a form that does not require the station to call out at flight time. [15 §9](15-operator-identity-and-the-single-role-deployment.md) carries the four narrower questions this splits into |
| Recommendation | Highest ratio of any open item, same as the row it replaces. Two of three installables are specified; this is the one that keeps the enforcement point from evaluating anything |

### 3. `P7-DAT-002` — the persistence design

| Field | Detail |
| --- | --- |
| Owner | **Tab 1** |
| Status | **RESOLVED AND BUILT.** No longer an escalation. The row is kept because this register reads as a history |
| What landed | `AeroraLocalStore` and its schema, `AeroraStorePaths`, `AeroraSqliteAuditSink` with the `AeroraAuditWriter` async front, `AeroraStoreExport`, `AeroraStoreRetention`, the `Events`/`EventValues` writer and the `Vehicles` writer — all compiled and CI-tested on Linux and Windows, and the audit path installed by default at plugin startup |
| What it did not resolve | Tamper-evidence stays `OPEN` (`D-DAT-05` → `AeroraPlatformKeyProtector`, no backend on any platform), and the retention caps `D-DAT-01` stay a product call, so `AeroraStoreRetention` takes an explicit cutoff and ships no default |

### Decisions deliberately not escalated

| Decision | Why not |
| --- | --- |
| `D-005` git origin | Blocks pushes only. Work accumulates locally without loss, and the checkpoint branch is intact |
| `D-004` radio transport | No radio attached. Escalating a hardware decision before hardware exists adds nothing |
| Supervisory absorption | Cosmetic. Ownership is resolved, which is what actually mattered |
| `Q-040` visual distinguishability | Needs screens to look at first |
| `D-UI-02` contrast target | Needs a design system to apply it to first |

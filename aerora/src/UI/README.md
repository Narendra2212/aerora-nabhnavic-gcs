# Aerora Operator Interface (`M-Ui`)

The UI foundation: the QML override seam, the QML route to per-vehicle Aerora services, and the
controls that render a value according to its telemetry state class.

**Build status: `IMPLEMENTED-UNCOMPILED`.** No compiler, CMake, Qt, `qmllint` or `pre-commit` exists
in the environment this was written in. Nothing here has been built, linted, loaded or executed.

Full design records, evidence and handoffs:

- [00-ui-architecture-map.md](../../../.kiro/specs/04-ui-implementation/00-ui-architecture-map.md) —
  the seam inventory and the fourteen-area classification
- [01-ui-foundation-closure.md](../../../.kiro/specs/04-ui-implementation/01-ui-foundation-closure.md) —
  Q7-001 to Q7-004 closure, the safety audit, and the four handoffs

## What is here

| File | Purpose |
| --- | --- |
| `AeroraQmlOverrides.{h,cc}` | The **allow-listed** QML URL interceptor. The list is empty: Aerora overrides no upstream file |
| `AeroraVehicleUiContext.{h,cc}` | `AeroraUi` — typed, read-only QML access to the active vehicle's Aerora services |
| `AeroraUiLog.{h,cc}` | The `Aerora.Ui` logging category |
| `Controls/AeroraStateText.qml` | Renders a value by state class. The one place the class decides the treatment |
| `Controls/AeroraAgeLabel.qml` | Age, with "currency unverified" while no freshness threshold exists (Q-036) |
| `Controls/AeroraTokenDisplay.qml` | One `AeroraReportedValue`: name, token, class label, age |
| `Controls/AeroraValueDisplay.qml` | One `AeroraStateValue`: Fact value, units, class label, age |
| `Controls/AeroraSelfReportRow.qml` | One self-report line. Carries three outcomes, so "not checked" cannot render as a finding |
| `Indicators/AeroraEnforcementIndicator.qml` | `SCR-SHL-03`. The toolbar strip that says command authorization is not enforced |
| `Screens/AeroraOperationsHub.qml` | `SCR-SHL-01`. The overlay's honest self-report: services attached, enforcement state, audit posture |
| `Screens/AeroraMissionProgressPage.qml` | `SCR-LIV-06`. An `AnalyzePage`: what the vehicle is flying, what the ground station holds, and whether the two agree |
| `Screens/AeroraServiceDiagnosticsPage.qml` | `SCR-VEH-06`. A `SetupPage`: which Aerora services attached to this vehicle, and why a classification is not an identification |
| `Ui.cmake` | Source manifest, and the integration instructions for `aerora/CMakeLists.txt` |

`Controls/`, `Indicators/` and `Screens/` are a source-tree split only. The resource alias flattens
every QML file to `qrc:/qml/Aerora/Controls/<Name>.qml` in one module, which is why the hub needs no
import to reach `AeroraSelfReportRow` and why its registration URL carries no `Screens/` segment.

## What is deliberately not here

No telemetry panel, no product surface, no guided action, no tool-strip entry. Every Aerora fact
group is `CONTRACT-ONLY` for want of a dialect, so there is no Aerora value to populate a panel
with — building one would be shape with nothing behind it.

The three screens that do exist are not that panel and are not an exception to it. Each renders
state the overlay computes in C++ — services attached, enforcement posture, observed mission
progress, vehicle composition — and none reads a fact group.

The one indicator that does exist is the exception that proves the rule: it renders a C++ singleton
the overlay already computes, so there is something behind it. It is also read-only — no click
handler, no drawer, no command path — because its drill-in target `SCR-SEC-01` does not exist.

The interface is **observation and supervision**, not flight authority. QML cannot reach payload
command submission, power switching, mission commanding, command authorization or security policy;
`AeroraUiSafetyTest` asserts that against each type's meta-object rather than leaving it to review.

## Outstanding, in other workstreams' files

| Handoff | Owner | Effect while open |
| --- | --- | --- |
| HO-TAB7-06 | Tab 1 — override `toolBarIndicators()` to register the enforcement strip | `SCR-SHL-03` is compiled into the resource and never loaded: the toolbar shows no Aerora indicator, so the station still does not say authorization is unenforced. **No longer blocked** — its `HO-TAB1-10` prerequisite is this module's `Indicators/AeroraEnforcementIndicator.qml`, and the URL to register is `qrc:/qml/Aerora/Controls/AeroraEnforcementIndicator.qml`. That seam carries bare `QUrl`s, not `QmlComponentInfo` objects |

### Closed

Recorded rather than deleted, because each row describes a gap that was real and the commit that
closed it is the evidence. Three of these were still listed above as open after they had landed,
which is the staleness `HO-TAB1-04` was raised against — the rows were corrected together because
leaving two of three would reproduce the same defect.

| Handoff | Owner | Closed by |
| --- | --- | --- |
| HO-TAB7-01 | Tab 4 — make `AeroraStateValue::capability` read-only in QML | U3, `a7b317f89`. The `WRITE` is gone; `setCapability()` stays public C++ |
| HO-TAB7-02 | Tab 1 — install the interceptor on the QML engine | U1, `84bbb3a6e` |
| HO-TAB7-03 | Tab 1 — create the `Aerora.Controls` QML module target | U2, `a73699796`, including the two load-bearing lines (`IMPORTS QGC`, resource alias) |
| HO-TAB7-05 | Tab 1 — override `analyzePages()` to register the Operations Hub | `39406ff5f`. Registration alone was not sufficient: the module also needed `IMPORTS QGroundControl.AnalyzeView`, found and fixed at closure |
| HO-TAB1-03 | Tab 7 — audit the two unaudited QML-registered types | `67f7137d0`. `AeroraFreshness` and `AeroraSecurityStatus` are audited; the closure is now derived |
| HO-TAB4-01 | Tab 7 — the `\section known` note described a live writable-property deviation | `67f7137d0`. `AeroraUiSafetyTest.h` now states the set is empty, in the past tense |
| HO-TAB4-02 | Tab 7 — two files still said the capability property carries a `WRITE` | `67f7137d0`. `AeroraVehicleUiContext.h:65` and `01-ui-foundation-closure.md` both read `carried`, and the closure doc records the audit gap the enumeration missed |
| HO-TAB1-04 | Tab 7 — two further stale HO-TAB7-01 rows outside the six documents swept by `HO-TAB4-03` | `3b19d112a`. `00-ui-architecture-map.md:533` and this file's row above are both struck and attributed to U3 |

**Tab 1 and Tab 4 still carry the status rows for the three above.**
`05-product-completion/07 §7` lists `HO-TAB1-03`, `HO-TAB4-01`, `HO-TAB4-02` and `HO-TAB1-04` with a
severity and no closure marker, and `Telemetry/README.md`'s `HO-TAB4-02` row is the same shape. The
work is delivered and the commits are above; flipping those rows is the owning tab's edit, not one
to make here. Recorded so the lag is visible rather than discovered later — which is the
`R7-20` failure mode this set was raised against in the first place.

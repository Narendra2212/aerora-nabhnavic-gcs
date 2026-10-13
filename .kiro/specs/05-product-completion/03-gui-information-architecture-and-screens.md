# GUI Information Architecture and Screen Specification

Owner: **Tab 7 (`M-Ui`)**, architecture frozen by Tab 1. Extends
[04-ui-implementation/00-ui-architecture-map.md](../04-ui-implementation/00-ui-architecture-map.md)
and [01-ui-foundation-closure.md](../04-ui-implementation/01-ui-foundation-closure.md) from the UI
foundation to the complete screen set.

Status: `DESIGNED`. No screen in this document exists as a file except the four foundation controls
noted in [§8](#8-screen-inventory). Seam facts were verified against this checkout this pass.

## Contents

- [1. Information architecture](#1-information-architecture)
- [2. Navigation model](#2-navigation-model)
- [3. The CONSTANT registry constraint](#3-the-constant-registry-constraint)
- [4. Delivery classes](#4-delivery-classes)
- [5. Design system](#5-design-system)
- [6. State rendering rules](#6-state-rendering-rules)
- [7. Responsive layout and accessibility](#7-responsive-layout-and-accessibility)
- [8. Screen inventory](#8-screen-inventory)
- [9. Why there are no action buttons yet](#9-why-there-are-no-action-buttons-yet)
- [10. Seam budget](#10-seam-budget)
- [11. Open UI decisions](#11-open-ui-decisions)

## 1. Information architecture

Five operator-facing zones, chosen to match the user journeys in
[01 §6](01-product-requirements-and-completion-plan.md#6-user-journeys) rather than to match the
subsystem decomposition. A zone is a place an operator goes with an intent; a subsystem is a place
an engineer goes with a change.

```text
                         ┌──────────────────────────────────┐
                         │   ALWAYS-ON STATUS SURFACE       │
                         │   toolbar indicators +           │
                         │   enforcement-state strip        │
                         └──────────────────────────────────┘
   ┌───────────┬──────────────┬──────────────┬──────────────┬──────────────┐
   │   FLY     │   PLAN       │   VEHICLE    │   REVIEW     │   MANAGE     │
   │ live ops  │ Mission      │ setup,       │ session,     │ fleet,       │
   │ supervise │ Studio       │ health,      │ logs, audit, │ profiles,    │
   │ approve   │ validate     │ diagnostics  │ timeline     │ settings,    │
   │           │ verify       │              │              │ security     │
   └───────────┴──────────────┴──────────────┴──────────────┴──────────────┘
         │             │              │              │              │
    FlyView      PlanView +     VehicleConfig    Analyze       Analyze +
    custom       complex        via Aerora       pages         Settings
    layer        mission item   AutoPilotPlugin
```

| Zone | Operator intent | Upstream host | Aerora delivery |
| --- | --- | --- | --- |
| **Fly** | Supervise a running operation; approve or abort | `src/FlyView/` | `FlyViewCustomLayer.qml` seam + toolbar indicators |
| **Plan** | Define work before flight | `src/PlanView/` | Complex mission item (editor + two map visuals) |
| **Vehicle** | Make one aircraft ready and understand its state | `VehicleConfigView.qml` | `AeroraAutoPilotPlugin` → `vehicleComponents()` |
| **Review** | Understand what happened | `src/AnalyzeView/` | `QGCCorePlugin::analyzePages()` registry |
| **Manage** | Operate the programme, not the aircraft | `AnalyzeView` + `AppSettings` | `analyzePages()` + settings JSON overlay |

Rationale for folding **Manage** into Analyze and Settings rather than giving it a top-level slot:
both are open registries requiring no fork, and the alternative costs a class-X fork of
`SelectViewDropdown.qml`. See [D-NAV-01](#d-nav-01--the-top-level-entry-point).

## 2. Navigation model

### What the shell actually does

Verified in `src/MainWindow/MainWindow.qml`:

| Mechanism | Lines | Used for |
| --- | --- | --- |
| Two always-instantiated siblings, switched by `visible` | `:330-345` | Fly and Plan. Never destroyed |
| Full-screen `toolDrawer` Rectangle with a single `Loader` | `:414-478` | Analyze, Vehicle Config, Settings. Destroyed on exit (`toolDrawerLoader.source = ""`) |
| `mainWindow.showTool(toolTitle, toolSource, toolIcon)` | `:148` | **The generic entry point. Any QML can call it with any URL** |
| `SelectViewDropdown.qml` | `src/Toolbar/`, shown via `:380-412` | The view picker. **No `QGCCorePlugin` virtual exists for its contents** |

There is no `StackView` and no view registry. `showTool()` is the only general-purpose navigation
primitive, and it is open.

### Aerora navigation, as frozen

```text
Toolbar (always visible)
 ├─ QGC logo ─────────────► SelectViewDropdown (upstream, unforked)
 │                           ├─ Fly ──────────► FlyView + Aerora custom layer
 │                           ├─ Plan ─────────► PlanView + Aerora mission items
 │                           ├─ Vehicle Config ► Aerora setup pages
 │                           ├─ Analyze ──────► AnalyzeView
 │                           │                   ├─ (6 upstream pages)
 │                           │                   ├─ Aerora Operations Hub  ◄── Aerora nav root
 │                           │                   ├─ Aerora Session Review
 │                           │                   ├─ Aerora Telemetry Explorer
 │                           │                   ├─ Aerora Audit Log
 │                           │                   └─ Aerora Diagnostics
 │                           └─ Settings ─────► AppSettings + Aerora pages (JSON overlay)
 │
 ├─ Aerora enforcement-state strip  (SCR-SHL-03)
 └─ Aerora indicator cluster        (SCR-SHL-02)

Aerora Operations Hub (SCR-SHL-01)
 └─ calls mainWindow.showTool(...) for:
      Fleet Inventory · Product Profiles · Mission Validation Report
      Proposal Review · Export · State Vocabulary Reference
```

The hub is the resolution of the top-level-entry problem. It is **one** entry in an existing open
registry, and from it `showTool()` reaches any Aerora screen with no fork.

### D-NAV-01 — the top-level entry point

**Decision: Option C. Hub page in the `analyzePages()` registry. Zero forks.**

| Option | Fork cost | Verdict |
| --- | --- | --- |
| A — fork `SelectViewDropdown.qml` for top-level Aerora entries | 1 class-X fork of a populated file, permanent rebase cost | **Deferred.** Available if product requires top-level placement |
| B — register every Aerora screen directly as an analyze page | 0 | **Partially adopted** for the five genuinely analytical screens |
| **C — one hub page, `showTool()` for the rest** | **0** | **Adopted** |
| D — replace `MainWindow.qml` via `createRootWindow()` | Maximum; forks the shell | **Rejected.** Contradicts `P7-ARCH-023` |

Option A stays open as a **single, registered** class-X fork with a named owner and a drift review,
if the product decides industrial operators will not accept operational screens behind an "Analyze"
label. That is a product judgement, not an engineering one, and it is recorded as `OPEN` in
[§11](#11-open-ui-decisions). What is decided now is that Phase 7 implementation does not depend on
it: nothing is blocked by deferring it, and adopting it later changes one file.

### Why the hub is not a dashboard

It is a navigation root plus the overlay's own self-report: which services attached, which interfaces
are available, whether authorization is enforced. That makes the one screen an operator always sees
first the screen that cannot hide a missing subsystem. A screen full of green tiles that are green
because nothing was checked is the failure mode this avoids.

## 3. The CONSTANT registry constraint

The most consequential constraint on the whole design, and it is easy to miss.

These are declared `CONSTANT` and are therefore **read exactly once** when QML first binds them:

| Property | Declaration |
| --- | --- |
| `analyzePages` | `src/API/QGCCorePlugin.h:52` |
| `toolBarIndicators` | `src/API/QGCCorePlugin.h:53` |
| `options` | `src/API/QGCCorePlugin.h:50` |
| `customMapItems` | `src/API/QGCCorePlugin.h:51` |
| `showInstrumentPanel`, `showMultiVehicleList`, `showMapScale` | `src/API/QGCOptions.h:16-18` |

A value that becomes known **after** first bind is never picked up. Consequences:

| Consequence | Rule |
| --- | --- |
| The Aerora page list cannot depend on which vehicle is connected | `P7-FLEET-004`. Register the **full** page set; gate **content** at runtime |
| The page list cannot depend on the active product profile | Same. A profile selects what a page *shows*, never whether it exists |
| The indicator list cannot depend on reported capability | Register the indicator; it decides its own `showIndicator` from live state |
| `QGCOptions` booleans cannot react to capability | Anything capability-driven must live inside a component, not in an option |

**This is why `P7-FLEET-004` exists.** A profile-keyed navigation tree is unimplementable through
these registries. The workable shape is: static registration, runtime content resolution. Every
screen in [§8](#8-screen-inventory) is designed to that shape.

The per-vehicle indicator list (`FirmwarePlugin::toolIndicators(const Vehicle*)`,
`src/FirmwarePlugin/FirmwarePlugin.h:344`) is the one exception — it has a
`toolIndicatorsChanged()` signal and is genuinely dynamic. `AeroraFirmwarePlugin` does not override
it today; `SCR-SHL-02` is where that changes.

## 4. Delivery classes

Carried unchanged from the UI architecture map. The class determines maintenance cost, so it is
recorded per screen.

| Class | Meaning | Cost |
| --- | --- | --- |
| **R** | Upstream UI reused unchanged | None |
| **C** | Upstream UI reconfigured through a virtual, JSON or palette override | None |
| **O** | New Aerora content at a seam upstream provides for the purpose | Low |
| **X** | A populated upstream file replaced wholesale via URL interception | **High — a silent fork** |
| **P** | Content belonging to one product family, loaded by profile id | Low, isolated |

Rule (`AERORA-UI-REQ-004`): use the lowest-cost class that works. Current targets:

| Class | Target count | Current count |
| --- | --- | --- |
| X (forks) | **0** | **0** |
| O (seam content) | as needed | 4 controls `IMPLEMENTED-UNCOMPILED` |

## 5. Design system

Original to Aerora. Derived from `QGCPalette` and `ScreenTools` so Aerora content scales and themes
with the rest of the application. No reference product's visual language is reproduced
(`P7-SHELL-005`).

### Foundations

| Token group | Source | Rule |
| --- | --- | --- |
| Colour | `QGCPalette` roles, plus Aerora semantic roles via `QGCCorePlugin::paletteOverride()` | **No colour literal in any Aerora QML.** Light and dark, enabled and disabled, all four combinations route through the override |
| Spacing | `ScreenTools.defaultFontPixelWidth/Height` multiples | **No pixel literal.** A 4-step scale: `0.5×`, `1×`, `2×`, `4×` of `defaultFontPixelWidth` |
| Type | `ScreenTools.smallFontPointSize`, default, `largeFontPointSize` | Three sizes. A fourth requires a recorded reason |
| Control set | `QGCButton`, `QGCLabel`, `QGCTextField`, `QGCComboBox`, `QGCCheckBox`, `QGCGroupBox`, `QGCFlickable`, `QGCPopupDialog`, `SubMenuButton`, `ToolStrip` | Aerora composes these. It does not restyle them in place |
| Icons | Aerora-authored SVG under `aerora/src/UI/Resources/` | Original artwork. State iconography is shape-distinct, not colour-distinct — see [§7](#7-responsive-layout-and-accessibility) |

### Aerora semantic colour roles

Seven roles, one per telemetry state, mapped through `paletteOverride()`. **Colour is never the sole
encoder** — each role pairs with a glyph and a text token.

| Role | State | Glyph | Text token |
| --- | --- | --- | --- |
| `aeroraReported` | `REPORTED` | none (plain value) | the value |
| `aeroraEffective` | `EFFECTIVE` | filled marker | the value |
| `aeroraRequested` | `REQUESTED` | outline marker | value + `REQUESTED` |
| `aeroraProposed` | `PROPOSED` | dashed outline | value + `PROPOSED` |
| `aeroraStale` | `STALE` | clock | value + age + `STALE` |
| `aeroraUnknown` | `UNKNOWN` | question | `UNKNOWN` |
| `aeroraUnavailable` | `UNAVAILABLE` | barred circle | `UNAVAILABLE` |

### Existing primitives

Four controls are already written (`IMPLEMENTED-UNCOMPILED`, never linted — no `qmllint` on this
host):

| Control | File | Role |
| --- | --- | --- |
| `AeroraValueDisplay` | `aerora/src/UI/Controls/AeroraValueDisplay.qml` | Value + state class + age, the atomic display unit |
| `AeroraStateText` | `.../AeroraStateText.qml` | State class as a word. Renders `UNKNOWN` as the word, never as blank |
| `AeroraAgeLabel` | `.../AeroraAgeLabel.qml` | Freshness age |
| `AeroraTokenDisplay` | `.../AeroraTokenDisplay.qml` | Discrete token/enum state |

Registered as the `Aerora.Controls` QML module (`AeroraControlsModule`,
`aerora/CMakeLists.txt:190-210`), which reaches the binary via `_aerora_libraries` →
`CUSTOM_LIBRARIES` (`:208`, `:292`). The `IMPORTS QGC` / `qgc_set_qt_resource_alias` lines that
[HO-TAB7-03](../04-ui-implementation/01-ui-foundation-closure.md#ho-tab7-03--create-the-aeroracontrols-qml-module-target)
specifies are now present (`:194`, `:201-202`); [D-UI-04](#11-open-ui-decisions) is closed by U2,
`a73699796`. Every file in the module lands flat at `qrc:/qml/Aerora/Controls/<Name>.qml` — the
alias discards the `Controls/`, `Indicators/` and `Screens/` source subdirectories.

### Composite components to build

| Component | Composes | Used by |
| --- | --- | --- |
| `AeroraValueGrid` | `AeroraValueDisplay` | All panels |
| `AeroraStatePill` | `AeroraStateText` | Indicators, timeline |
| `AeroraFreshnessBar` | `AeroraAgeLabel` | `SCR-TEL-03` |
| `AeroraProposalCard` | `AeroraValueDisplay`, `AeroraStateText` | `SCR-LIV-05`, `SCR-AIS-01` |
| `AeroraCapabilityRow` | `AeroraTokenDisplay` | `SCR-VEH-03` |
| `AeroraPanelFrame` | `QGCGroupBox`, occluder publication | Every Fly-view panel |
| `AeroraEvidenceBlock` | `AeroraValueDisplay` | `SCR-MIS-07`, `SCR-AIS-01` |
| `AeroraDerivedBadge` | `QGCLabel` | **Every station-derived value** (`P7-ARCH-004`) |

`AeroraDerivedBadge` is not cosmetic. `P7-ARCH-004` requires station-derived values to be visually
distinct from reported ones; a single component makes that a composition rule rather than a
reviewer's vigilance.

## 6. State rendering rules

Normative. A violation is a defect, not a style preference.

| # | Rule |
| --- | --- |
| `UI-ST-01` | Every displayed value renders its state class. A bare number is prohibited |
| `UI-ST-02` | `UNKNOWN`, `UNAVAILABLE` and `STALE` are mutually distinguishable and distinguishable from a zero value. **A blank field is prohibited for all three** |
| `UI-ST-03` | `REQUESTED` is never displayed in the position or styling of `EFFECTIVE` (`P7-MOD-030`) |
| `UI-ST-04` | `PROPOSED` is never displayed as actionable unless an authorized command path exists |
| `UI-ST-05` | A station-derived value carries `AeroraDerivedBadge` |
| `UI-ST-06` | `STALE` shows the age, not only the staleness |
| `UI-ST-07` | No Aerora UI writes a capability. `UNAVAILABLE` comes only from a vehicle report (`IF-UIS-005`) |
| `UI-ST-08` | Colour is never the sole encoder of a state class |
| `UI-ST-09` | A panel with no data says which state applies to the whole panel; it does not render empty |

`UI-ST-07` had the one outstanding defect behind it, and it is now closed:
[HO-TAB7-01](../04-ui-implementation/01-ui-foundation-closure.md#ho-tab7-01--aerorastatevaluecapability-was-qml-writable--closed)
— `AeroraStateValue::capability` carried `WRITE setCapability`, so QML could have written the
classification. U3 (`a7b317f89`) removed the `WRITE`; the property stays readable and
`setCapability()` remains public C++ for the capability model.
`AeroraUiSafetyTest::_onlyTheRecordedPropertyDeviationIsWritable` now requires the writable set to be
empty, so the ratchet fails on the first writable property to appear rather than on the second.
**`SCR-LIV-02`'s prerequisite is met;** its remaining gate is `Q4-002` for fact-group content.

## 7. Responsive layout and accessibility

### Breakpoints

Expressed in `ScreenTools.defaultFontPixelWidth` so they scale with density, per `P7-SHELL-004`.

| Breakpoint | Width | Behaviour |
| --- | --- | --- |
| Wide | ≥ 120 units | Panels side by side; full value grids |
| Medium | 80–120 units | Panels stack; grids reduce to two columns |
| Narrow | < 80 units | One panel at a time; grids single-column |

| # | Rule |
| --- | --- |
| `UI-RL-01` | **A safety-relevant value is never horizontally truncated.** It wraps, reflows, or the panel scrolls |
| `UI-RL-02` | A state glyph and its text are never separated by a reflow |
| `UI-RL-03` | Fly-view panels publish their rect into `customOccluders` at every breakpoint, or the map hides the vehicle beneath them (`FlyView.qml:48-49`) |
| `UI-RL-04` | No Aerora layout assumes a pointer. Touch targets are ≥ 2 units square |

### Accessibility

| # | Rule | Status |
| --- | --- | --- |
| `UI-A11Y-01` | Every interactive element is keyboard reachable with a defined focus order | `DESIGNED` |
| `UI-A11Y-02` | State classes are shape- and text-distinct, not only colour-distinct (`UI-ST-08`) | `DESIGNED` |
| `UI-A11Y-03` | Text and glyphs meet a contrast target in all four palette combinations | `DESIGNED` — target value is `OPEN` |
| `UI-A11Y-04` | All user-visible strings are `qsTr()`-wrapped | `DESIGNED` |
| `UI-A11Y-05` | Critical state changes are announced, not only rendered | `SPECIFIED-NOT-DESIGNED` |

**WCAG conformance is not claimed.** Full validation requires manual testing with assistive
technologies and expert accessibility review, neither of which has occurred (`P7-SET-005`).

## 8. Screen inventory

45 screens across 12 areas. Every one maps to a verified seam or a named new interface.

Columns: **Class** per [§4](#4-delivery-classes) · **Seam** the exact mechanism · **Status** per
[00-README](00-README.md#status-vocabulary) · **Owner** the tab · **Blocked by** the gating item.

### Shell — 4 screens

| ID | Screen | Class | Seam | Status | Owner | Blocked by |
| --- | --- | --- | --- | --- | --- | --- |
| `SCR-SHL-01` | Aerora Operations Hub | O | `analyzePages()` + `mainWindow.showTool()` | `IMPLEMENTED-UNCOMPILED` | 7 | — (navigation half deferred: no destination screen exists) |
| `SCR-SHL-02` | Aerora indicator cluster | O | `QGCCorePlugin::toolBarIndicators()` (non-vehicle) + `FirmwarePlugin::toolIndicators()` (per-vehicle) | `IMPLEMENTED-UNCOMPILED` (non-vehicle half) | 7 + 3 | Non-vehicle: **closed** — `HO-TAB7-06` registers `AeroraEnforcementIndicator.qml`. Per-vehicle: implemented and correctly inert — `AeroraFirmwarePlugin::toolIndicators()` returns the base list untouched because `AeroraToolIndicators::declaredIndicators()` is empty (U5), awaiting an evidenced capability identifier (`Q3-009`) |
| `SCR-SHL-03` | Enforcement-state strip | O | Indicator reading `AeroraSecurity.enforcement` | `IMPLEMENTED-UNCOMPILED` | 7 + 2 | Read path **closed** (`D-UI-01`, `AeroraSecurityStatus`); QML **closed** (`HO-TAB1-10`, `602b38a18`); registration **closed** (`HO-TAB7-06`) — see [§ The indicator seam](#the-indicator-seam-and-what-it-is-allowed-to-carry). Renders `NotEnforced` vs `EnforcedAtCommandGate` only; no roles, policies or command paths |
| `SCR-SHL-04` | Build provenance / about | O | `analyzePages()` entry | `SPECIFIED-NOT-DESIGNED` | 1 | `P7-HELP-004` — no provenance source exists |

`SCR-SHL-03` is the screen that makes `P7-AUTH-002` visible: the station must be able to say "not
enforced". That read path now exists: `AeroraSecurityStatus` (`QML_NAMED_ELEMENT(AeroraSecurity)`,
`QML_SINGLETON`) projects enforcement and audit posture as read-only properties, holds no security
state of its own, and is audited by `AeroraUiSafetyTest` with no writable property, no
`Q_INVOKABLE` and no slot. `D-UI-01` is closed by it. The strip itself needed a
`toolBarIndicators()` override, which is a `QGCCorePlugin` virtual and therefore Tab 1's —
HO-TAB7-06, now implemented.

### The indicator seam, and what it is allowed to carry

`HO-TAB7-06` is **implemented** (`IMPLEMENTED-UNCOMPILED`). It was deliberately held back on the
previous pass because the seam had nothing to carry; `HO-TAB1-10` then delivered
`AeroraEnforcementIndicator.qml` (`602b38a18`), and the override registers exactly that one file.

The two registries are not interchangeable, which is why the work splits across two tabs:

| | `QGCCorePlugin::toolBarIndicators()` | `FirmwarePlugin::toolIndicators(const Vehicle *)` |
| --- | --- | --- |
| Scope | Non-vehicle | Per-vehicle |
| Re-read after first bind | **No** — the `Q_PROPERTY` is `CONSTANT` (`QGCCorePlugin.h:55`) | **Yes** — carries `toolIndicatorsChanged()` (`FirmwarePlugin.h:344`) |
| Owner | Tab 1 (`AeroraCorePlugin`) | Tab 3 (`AeroraFirmwarePlugin`) |
| State | Implemented (`HO-TAB7-06`), appends one `QUrl` | Implemented (U5), returns the base list untouched |

Anything whose membership depends on reported capability must go in the per-vehicle registry,
because the non-vehicle one is read once and never again.

#### The three constraints the override has to satisfy

The seam carries **`QUrl` values**, not `QmlComponentInfo` objects — unlike `analyzePages()`. The
base returns a single-element list holding `RTKGPSIndicator.qml` (`QGCCorePlugin.cc:333`), and
`FlyViewToolBarIndicators.qml` consumes it as:

```qml
Repeater {
    model: QGroundControl.corePlugin.toolBarIndicators
    Loader { source: modelData; visible: item.showIndicator }
}
```

1. **`visible: item.showIndicator` has no null guard.** The loaded root must declare
   `property bool showIndicator`, and a URL with no resource behind it produces a binding error on a
   null `item` rather than a quietly absent indicator. `AeroraEnforcementIndicator.qml` declares it
   (`:74`); the URL is the one `aerora/src/UI/Ui.cmake:162` states, which is why that string is
   copied from the owning manifest rather than reconstructed from the source path.
2. **The URL carries no `Indicators/` segment.** `qgc_set_qt_resource_alias()`
   (`aerora/CMakeLists.txt:194`) aliases every `AERORA_UI_QML_FILES` entry to its bare name, so the
   whole module lands flat at `qrc:/qml/Aerora/Controls/<Name>.qml`. `Controls/`, `Indicators/` and
   `Screens/` are a source-tree distinction only.
3. **The override keeps its own static.** The base returns a reference to its own function-local
   `static const QVariantList`; appending to it would mutate the base's list, and returning a
   reference to a lambda-local would dangle. The override therefore holds its own static, seeded
   from the base call — net effect two entries, `RTKGPSIndicator.qml` plus the Aerora one.

Returning the base list unchanged was the alternative and is dead code by `CODING_STYLE.md`'s own
rule — the same reason `AeroraFirmwarePlugin` leaves four base defaults un-overridden and asserts
them in a test instead.

Tab 3's half stays empty for an unrelated reason recorded in `AeroraToolIndicators.h`: no capability
identifier is evidenced (`Q3-009`) for a *per-vehicle* indicator. Its `isPublishable()` checks
resource existence precisely because of the unguarded binding above. That gate is implemented and
tested; the content it would carry still does not exist.

The read path needed nothing new — `AeroraSecurity` is a `QML_SINGLETON`, reachable with no vehicle,
which is exactly what makes the enforcement strip a *non-vehicle* indicator.

### `SCR-SHL-01`: four facts, held apart

The hub is the first Aerora screen to exist, and "implemented" is doing less work in that sentence
than it looks. Four separate things have to be true before an operator can read it, and they are
recorded separately because three of them have failed silently in this programme already:

| Fact | State | Evidence |
| --- | --- | --- |
| The screen exists in source | **Yes** | `aerora/src/UI/Screens/AeroraOperationsHub.qml` and `Controls/AeroraSelfReportRow.qml`, committed in `3b19d112a` |
| It is compiled into the application resources | **Yes, `SOURCE-INSPECTED`** | Declared in `AERORA_UI_QML_FILES` (`Ui.cmake:141`), which `aerora/CMakeLists.txt` passes to `qt_add_qml_module` through `QML_FILES` after `qgc_set_qt_resource_alias()`. Read from the build files; no build has run |
| It is reachable through navigation | **Yes, `SOURCE-INSPECTED`** | `AeroraCorePlugin::analyzePages()` appends it to the base registry — HO-TAB7-05, applied. Before that it was in the resource with no menu entry and no error of any kind |
| **Its root type resolves** | **Yes, `SOURCE-INSPECTED` — and it did not until this pass** | See below |

#### The fourth fact was a real defect, not a formality

The hub roots on `AnalyzePage`, which is the correct upstream convention — `MAVLinkInspectorPage.qml`
does the same. But upstream's analyze pages resolve that type without importing anything for it
because they are **members of the module that declares it**: `AnalyzePage.qml` and
`MAVLinkInspector/MAVLinkInspectorPage.qml` are both listed in `AnalyzeViewModule`
(`src/AnalyzeView/CMakeLists.txt:19-29`, URI `QGroundControl.AnalyzeView`), and types within a QML
module are mutually visible.

The hub is a member of `Aerora.Controls` instead, and that module imported only `QGC`,
`QGroundControl` and `QGroundControl.Controls`. So `AnalyzePage` was **unresolvable from the hub**:
the resource would be present, the page registered, the menu entry live, and opening it would have
failed with "AnalyzePage is not a type" — at runtime, with nothing wrong at configure or compile
time to catch it.

Mitigated by adding `IMPORTS QGroundControl.AnalyzeView` to the module declaration in
`aerora/CMakeLists.txt`. Declared there rather than as a file-level `import` in the hub because that
block is where this module's dependencies are stated and because the CMake is M-Plugin's file while
the hub is M-Ui's.

**That mitigation is not provably sufficient, and the claim is withdrawn.** Qt documents `IMPORTS`
as: *if a QML file imports this module*, it also imports everything listed under `IMPORTS`
([qt_add_qml_module](https://doc.qt.io/qt-6/qt-add-qml-module.html)) — a statement about consumers
of `Aerora.Controls`, not about its own members. Whether a module member's implicit self-import
propagates the generated `qmldir` import entry is not documented either way. The in-repo reference
pattern sides against relying on it: `custom-example` declares no module `IMPORTS` at all and its
QML files import `QGroundControl.PlanView` and `QGroundControl.FlightMap` at file level. A one-line
file-level import in the hub makes it unconditional and is filed as `HO-TAB1-13`, Tab 7.
`aerora-manifest-check.ps1` reports this row as `ASSUMED`, which it defines as unresolved rather
than passed. `AeroraEnforcementIndicator.qml` is unaffected — it file-imports
`QGroundControl.Controls` for its `QGCLabel` root, and the checker marks it `OK`.

This is the third silent-registration failure in the same family — HO-TAB7-02 (interceptor never
installed), `D-UI-04` (`IMPORTS QGC` missing), and now this. All three were invisible without a
build, all three would have presented as "the Aerora UI just isn't there", and none would have
produced an error. The pattern is worth naming: **a QML module's `IMPORTS` list is load-bearing and
is not checkable by any tool available on this host.**
| Its root type resolves | **Unresolved — `HO-TAB1-13`** | The hub roots on `AnalyzePage`, which upstream declares in `AnalyzeViewModule` under URI `QGroundControl.AnalyzeView` (`src/AnalyzeView/CMakeLists.txt:19-24`). Upstream's own pages import nothing for it because they are members of that module; the hub is a member of `Aerora.Controls`, so `IMPORTS QGroundControl.AnalyzeView` was added to the module block. That is a documented guarantee for *consumers* of `Aerora.Controls`, not for its own members — a file-level import is wanted |

That fourth row was found at this reconciliation and is the reason the list is four rows long rather
than three. Each of the four fails **silently and differently**, which is why they are tracked
apart rather than collapsed into "implemented":

| If this is missing | What an operator sees | When it shows up |
| --- | --- | --- |
| Source | Nothing | Immediately |
| Resource entry | Nothing, and the QML never compiles into the binary | Build time, if anything |
| `analyzePages()` entry | No menu entry. No error anywhere | Never, without someone looking |
| `IMPORTS QGroundControl.AnalyzeView` | Menu entry present, page fails to load: *AnalyzePage is not a type* | **Runtime only**, on the first click |

The last one is the worst of the four: everything else looks correct, the entry is in the menu, and
the failure needs a running application to observe — which `D-006` means nobody has.

What is **not** claimed: the screen has never been compiled, parsed by `qmllint`, loaded, or
rendered. Its safety-status bindings — the fail-safe enforcement derivation, and
`AeroraSelfReportRow` letting `unchecked` outrank `warning` — have **never been executed**. They
are pure QML logic that the C++ test harness cannot reach, so they are guarded by review only
until the Qt Quick Test target of HO-TAB7-04 exists. `IMPLEMENTED-UNCOMPILED` is the ceiling, and
`D-006` is why.

The navigation half of the hub is deliberately absent rather than incomplete: it is specified to
reach other Aerora screens through `mainWindow.showTool()`, and no destination screen exists yet.

### Live operations — 9 screens

| ID | Screen | Class | Seam | Status | Owner | Blocked by |
| --- | --- | --- | --- | --- | --- | --- |
| `SCR-LIV-01` | Live Operations overlay (root) | O | `FlyViewCustomLayer.qml` (tier 2, empty seam, `FlyView.qml:145`) | `DESIGNED` | 7 | **unblocked** — HO-TAB7-02 closed by U1 |
| `SCR-LIV-02` | Vehicle state panel | O | child of `SCR-LIV-01`, binds `AeroraUi` | `DESIGNED` | 7 | **HO-TAB7-01 closed by U3**; `Q4-002` still gates fact-group content |
| `SCR-LIV-03` | Payload supervision panel | O/P | child; `AeroraUi.payload` | `DESIGNED` | 7 + 6 | `P7-PAY-004` reported-value set undefined |
| `SCR-LIV-04` | Power & tether panel | O | child; `AeroraUi.power` | `DESIGNED` | 7 + 6 | `P7-PWR-006` tether item set undefined |
| `SCR-LIV-05` | Proposal queue / approval | O | child | `SPECIFIED-NOT-DESIGNED` | 7 + 6 | `P7-AI-001` proposal model; `Q-029` |
| `SCR-LIV-06` | Mission progress & verification | O | child; `AeroraUi.mission` | `DESIGNED` | 7 + 5 | — |
| `SCR-LIV-07` | Per-link state panel | O | child | `DESIGNED` | 7 | `P7-RAD-002`; label as `DESIGNED` not supported |
| `SCR-LIV-08` | Critical-state alert surface | O | child + `QGCPopupDialog` | `DESIGNED` | 7 | `P7-FLY-005` |
| `SCR-LIV-09` | Aerora pre-flight checklist | C | `QGCOptions::preFlightChecklistUrl()` — replaces wholesale, **no fork** | `DESIGNED` | 7 | `P7-CHK-004` unevaluable-check handling |

Every panel from `SCR-LIV-02` to `SCR-LIV-08` must publish its rect into `customOccluders`
(`UI-RL-03`). `AeroraPanelFrame` exists so that is structural rather than remembered.

### Mission Studio — 9 screens

| ID | Screen | Class | Seam | Status | Owner | Blocked by |
| --- | --- | --- | --- | --- | --- | --- |
| `SCR-MIS-01` | Scene editor (asset + obstruction geometry) | O | new `ComplexMissionItem` editor, or a Plan-view-hosted editor | `SPECIFIED-NOT-DESIGNED` | 5 | `Q-021`/`Q-023`; benchmark pattern 1 |
| `SCR-MIS-02` | Industrial surface item editor | O | `VisualMissionItem::editorQml` + `complexMissionItemNames()` / `createComplexMissionItem()` | `SPECIFIED-NOT-DESIGNED` | 5 | `Q-021`/`Q-023` |
| `SCR-MIS-03` | Surface map visual (QtLocation) | O | `VisualMissionItem::mapVisualQML()` — **pure virtual, mandatory** | `SPECIFIED-NOT-DESIGNED` | 5 | as above |
| `SCR-MIS-04` | Surface visual (GeoMap engine) | O | `VisualMissionItem::geoMapVisualQML()` — optional, **omission ⇒ invisible under that engine** | `SPECIFIED-NOT-DESIGNED` | 5 | as above |
| `SCR-MIS-05` | Coverage & pass plan | O | panel inside `SCR-MIS-02`; reuse `TransectStyleComplexItem*` scaffolding | `SPECIFIED-NOT-DESIGNED` | 5 | consumption model is `OPEN` (`P7-ARCH-005`) |
| `SCR-MIS-06` | Consumable / pass segmentation | O | panel inside `SCR-MIS-02` | `SPECIFIED-NOT-DESIGNED` | 5 + 6 | benchmark gap [§7.4](02-gcs-reference-benchmark.md#74-consumable-constrained-multi-pass-work) — no reported consumable item set |
| `SCR-MIS-07` | Mission validation report | O | `analyzePages()` or Plan-view panel | `DESIGNED` | 5 | `P7-CHK-002` |
| `SCR-MIS-08` | 3D plan review | R/C | existing `src/Viewer3D/` Qt Quick 3D viewport | `DESIGNED` | 7 | `P7-MIS-003`. Note: force-disabled on Android upstream |
| `SCR-MIS-09` | Mission verification (snapshot agreement) | O | `AeroraMissionObserver::verify()` — the one `Q_INVOKABLE` on the mission layer | `IMPLEMENTED-UNCOMPILED` (service) / `DESIGNED` (screen) | 5 + 7 | — |

`SCR-MIS-04` is listed separately on purpose. Two map engines exist
(`flyViewSettings.useGeoMapEngine`, reboot-required, one instantiated per session), and a complex
item that implements only `mapVisualQML()` is **silently invisible** under the GeoMap engine. That is
a defect class, not an optional polish item.

### Vehicle — 6 screens

| ID | Screen | Class | Seam | Status | Owner | Blocked by |
| --- | --- | --- | --- | --- | --- | --- |
| `SCR-VEH-01` | Aerora setup page index | O | `AeroraAutoPilotPlugin::vehicleComponents()` via `FirmwarePlugin::autopilotPlugin()` (`FirmwarePlugin.h:122`) | `DESIGNED` | 3 | **`AeroraAutoPilotPlugin` does not exist** |
| `SCR-VEH-02` | Identity & profile | O | `VehicleComponent` subclass; `AeroraUi.identity` | `DESIGNED` | 3 + 7 | fleet identity format `OPEN` |
| `SCR-VEH-03` | Capability report | O | `VehicleComponent`; `AeroraUi.capabilities` | `DESIGNED` | 3 + 7 | `Q3-009` capability presence reporting |
| `SCR-VEH-04` | Sensor & health | O | `VehicleComponent`, or the JSON-driven `vehicleConfigJson()` path | `DESIGNED` | 3 | `P7-VEH-004` |
| `SCR-VEH-05` | Calibration (authorization-gated) | C/O | upstream calibration pages + the ADR-015 authorization seam | `IMPLEMENTED-UNCOMPILED` (gate) / `DESIGNED` (display) | 3 + 2 | `Q-029` — gate permits everything |
| `SCR-VEH-06` | Service attachment diagnostics | O | `VehicleComponent` reading `AeroraVehicleServices` | `DESIGNED` | 3 | — |

`SCR-VEH-04` should prefer the JSON-driven `vehicleConfigJson()` path: the base class then derives
section ids (with `repeat` group expansion probed against live params), a search keyword index,
display names and per-section completeness. For a parameter-driven industrial page that is
substantially less QML than writing it by hand.

### Telemetry — 3 screens

| ID | Screen | Class | Seam | Status | Owner | Blocked by |
| --- | --- | --- | --- | --- | --- | --- |
| `SCR-TEL-01` | Telemetry explorer | O | `analyzePages()`; iterates `AeroraTelemetrySet` fact groups | `DESIGNED` | 4 + 7 | `Q4-002` — all 6 fact groups are `CONTRACT-ONLY`, `isRegistrationAvailable()` is false |
| `SCR-TEL-02` | Event timeline | O | `analyzePages()` | `SPECIFIED-NOT-DESIGNED` | 4 | `P7-TEL-004`; needs the local store (`P7-DAT-002`) |
| `SCR-TEL-03` | Freshness / quality dashboard | O | `analyzePages()` | `DESIGNED` | 4 + 7 | `Q-036` — no budget values exist |

`SCR-TEL-01` adopts benchmark pattern 11 (Visionair): the explorer is **derived from the telemetry
set**, so a newly defined item is observable the day it is defined rather than when someone adds a
widget. That is only possible because `AeroraTelemetrySet` enumerates its groups.

### AI supervision — 3 screens

| ID | Screen | Class | Seam | Status | Owner | Blocked by |
| --- | --- | --- | --- | --- | --- | --- |
| `SCR-AIS-01` | Proposal detail & basis | O | `analyzePages()` + `SCR-LIV-05` drill-in | `SPECIFIED-NOT-DESIGNED` | 6 + 7 | `P7-AI-001`, `P7-AI-004` |
| `SCR-AIS-02` | Autonomy source status | O | `analyzePages()` | `SPECIFIED-NOT-DESIGNED` | 6 | `P7-AI-004` — GPU interface `OPEN`, nothing may be invented |
| `SCR-AIS-03` | Approval audit view | O | `analyzePages()` | `SPECIFIED-NOT-DESIGNED` | 2 + 7 | `Q-037` audit durability |

### Fleet & profiles — 3 screens

| ID | Screen | Class | Seam | Status | Owner | Blocked by |
| --- | --- | --- | --- | --- | --- | --- |
| `SCR-FLT-01` | Fleet inventory | O | `analyzePages()`; `MultiVehicleManager` + persisted records | `SPECIFIED-NOT-DESIGNED` | 3 + 7 | fleet identity `OPEN`; `P7-DAT-002` |
| `SCR-FLT-02` | Vehicle record detail | O | `showTool()` from `SCR-FLT-01` | `SPECIFIED-NOT-DESIGNED` | 3 + 7 | as above |
| `SCR-FLT-03` | Product profile manager | O/P | `analyzePages()`; `AeroraProductProfileRegistry` | `DESIGNED` | 3 + 7 | `P7-FLEET-004` — static registration, runtime content |

### Video — 1 screen

| ID | Screen | Class | Seam | Status | Owner | Blocked by |
| --- | --- | --- | --- | --- | --- | --- |
| `SCR-VID-01` | Aerora video overlay | O | child of `SCR-LIV-01` over `FlyViewVideo`; precedent `ObstacleDistanceOverlayVideo.qml`, `ProximityRadarVideoView.qml` | `DESIGNED` | 7 | `P7-VID-004` |

The three video C++ seams (`createVideoReceiver`, `createVideoSink`, `releaseVideoSink`) are **not**
overridden and should stay that way: the sink is an opaque `void *` whose lifetime belongs to
whichever plugin created it, so the three must be overridden as a set or not at all. Aerora has no
reason to.

### Review & export — 2 screens

| ID | Screen | Class | Seam | Status | Owner | Blocked by |
| --- | --- | --- | --- | --- | --- | --- |
| `SCR-LOG-01` | Aerora session review | O | `analyzePages()`; base on `AnalyzePage.qml` for the popout lifecycle | `SPECIFIED-NOT-DESIGNED` | 7 + 4 | no durable Aerora record (`Q-037`, `P7-DAT-002`) |
| `SCR-LOG-02` | Export with provenance | O | `showTool()` from `SCR-LOG-01` | `SPECIFIED-NOT-DESIGNED` | 7 | `P7-LOG-003` |

Upstream Log Viewer, Onboard Logs, GeoTag, MAVLink Console, MAVLink Inspector and Vibration are
class **R** — reused unchanged, not re-listed.

### Security — 4 screens

| ID | Screen | Class | Seam | Status | Owner | Blocked by |
| --- | --- | --- | --- | --- | --- | --- |
| `SCR-SEC-01` | Authorization & enforcement state | O | `analyzePages()`; drill-in from `SCR-SHL-03` | `SPECIFIED-NOT-DESIGNED` | 2 + 7 | `P7-SEC-005`; needs a QML-safe read path |
| `SCR-SEC-02` | Operator identity & role | O | `analyzePages()` | `SPECIFIED-NOT-DESIGNED` | 2 | `Q-029`; `AeroraIdentityProvider`/`AeroraRoleResolver` are `CONTRACT-ONLY` |
| `SCR-SEC-03` | Audit log viewer | O | `analyzePages()` | `SPECIFIED-NOT-DESIGNED` | 2 + 7 | `Q-037` — sink is neither durable nor tamper-evident |
| `SCR-SEC-04` | Signing key management | R/C | upstream `src/AppSettings/SigningKeyManager.qml` + `AeroraSigningKeyStore` | `IMPLEMENTED-UNCOMPILED` (store) / `DESIGNED` (any Aerora addition) | 2 | `AeroraPlatformKeyProtector` has **no backend on any target platform** |

Security types carry **no** `Q_OBJECT`, `Q_GADGET` or QML macro, by design. These four screens must
reach security state through a deliberate, read-only, minimal accessor — not by making security types
QML-visible. `AeroraUiSafetyTest` fails if that boundary moves, which is the intent.

### Settings — 4 screens

| ID | Screen | Class | Seam | Status | Owner | Blocked by |
| --- | --- | --- | --- | --- | --- | --- |
| `SCR-SET-01` | Aerora general settings | C | custom-dir JSON overlay (`src/AppSettings/CMakeLists.txt:25-40`) + `registerCustomSettings()` | `DESIGNED` | 7 | accessor naming constraint — see below |
| `SCR-SET-02` | Freshness & display settings | C | same | `DESIGNED` | 7 + 4 | `Q-036` — a settings page for values that do not exist yet |
| `SCR-SET-03` | Profile settings | C | same | `DESIGNED` | 7 + 3 | — |
| `SCR-SET-04` | Accessibility settings | C | same | `DESIGNED` | 7 | `UI-A11Y-03` contrast target `OPEN` |

Hard constraint on `registerCustomSettingsGroup()`: the accessor must be the camelCase form of the
JSON stem plus `Settings` (so `Aerora.SettingsGroup.json` → `aeroraSettings`), validated against
`^[a-z_][A-Za-z0-9_]*$`, rejected on collision with a built-in property — **and a rejected group is
deleted**, because the call takes ownership. Getting the name wrong destroys the group silently at
startup.

### Simulation — 1 screen

| ID | Screen | Class | Seam | Status | Owner | Blocked by |
| --- | --- | --- | --- | --- | --- | --- |
| `SCR-SIM-01` | Simulated source control & labelling | O | `analyzePages()`; upstream MockLink settings are class R | `DESIGNED` | 8 | `P7-SIM-002` — labelling must reach every display, not just this screen |

### Help & diagnostics — 3 screens

| ID | Screen | Class | Seam | Status | Owner | Blocked by |
| --- | --- | --- | --- | --- | --- | --- |
| `SCR-HLP-01` | Diagnostics report | O | `analyzePages()`; feeds `SCR-SHL-01` | `SPECIFIED-NOT-DESIGNED` | 1 + 7 | `P7-HELP-001` |
| `SCR-HLP-02` | State vocabulary reference | O | `showTool()` from `SCR-SHL-01` | `DESIGNED` | 7 | `P7-HELP-003` |
| `SCR-HLP-03` | Error report assembly | O | `analyzePages()` | `DESIGNED` | 7 | `P7-HELP-002` — explicit operator action only, no auto-send |

`SCR-HLP-02` is cheap and unusually valuable: the seven-state vocabulary is the product's main claim
and an operator who does not understand it cannot use the product correctly.

### Inventory totals

| Status | Count |
| --- | --- |
| `IMPLEMENTED-UNCOMPILED` (service behind the screen) | 2 |
| `DESIGNED` | 24 |
| `SPECIFIED-NOT-DESIGNED` | 19 |
| **Total Aerora screens** | **45** |
| Class X (forks) | **0** |
| Upstream screens reused unchanged (class R) | Not enumerated — the entire QGC surface |

The 19 `SPECIFIED-NOT-DESIGNED` screens are not a backlog of unwritten code. **16 of them are blocked
on a product or architecture decision**, not on engineering capacity. Designing them before the
decision would mean inventing the decision. See
[05 §4](05-implementation-dependency-graph.md#4-what-is-blocked-and-by-what).

## 9. Why there are no action buttons yet

The screen inventory has 45 entries and, today, **zero operator action controls**. That is deliberate
and worth stating plainly, because it looks like an omission.

An operator action requires a command path. A command path requires authorization. Authorization
requires a role policy, which is `Q-029`, `OPEN`. The chain:

```text
Q-029 (role set)  →  ADR-014 (authorization scope)  →  an authorized command path
                                                     →  an operator action control
```

`AeroraCorePlugin::authorizeMavCommand()` currently returns `true` and warns once, because the
alternatives are worse: denying every command makes the station unusable, and claiming a boundary
that performs no evaluation would be dishonest. `isAuthorizationEnforced()` reports `false` so the
station can say so.

Three consequences, all enforced mechanically rather than by review:

| Consequence | Mechanism |
| --- | --- |
| The two action-introducing QML seams stay unused | `CustomGuidedActionsController.qml` and `FlyViewAdditionalCustomActionsList.qml` are pre-approved but commented out in `AeroraQmlOverrides::overrideEntries()` |
| No payload actuator path exists | `AeroraPayloadCommandSink` has no implementation anywhere in the overlay; `kGroundStationActuatorPathImplemented = false` |
| No QML-reachable actuating method exists | `AeroraUiSafetyTest` reads the meta-objects of the ten types reachable from `AeroraUi` and fails when the surface grows |

**An inert button that looks live is worse than no button.** `P7-FLY-004` is therefore
`FORBIDDEN-UNTIL` rather than `SPECIFIED-NOT-DESIGNED`: the work is not pending, it is prohibited
until its precondition closes.

## 10. Seam budget

Every seam Aerora consumes, with its current state. This is the full UI contact surface with
upstream.

### Tier 1 — C++ virtuals

| Seam | Declaration | Aerora use | State |
| --- | --- | --- | --- |
| `createQmlApplicationEngine` / `destroyQmlApplicationEngine` | `QGCCorePlugin.h:117`, `:122` | Install the override interceptor | **`OPEN` — HO-TAB7-02** |
| `analyzePages()` | `QGCCorePlugin.h:52` + base `.cc:73-114` | 19 Aerora pages | `DESIGNED` |
| `toolBarIndicators()` | `QGCCorePlugin.h:53` + base `.cc:333-342` | Aerora non-vehicle indicators | `DESIGNED` |
| `FirmwarePlugin::toolIndicators(const Vehicle*)` | `FirmwarePlugin.h:344` | Aerora per-vehicle indicators | `DESIGNED` — not overridden today |
| `FirmwarePlugin::autopilotPlugin(Vehicle*)` | `FirmwarePlugin.h:122` | `AeroraAutoPilotPlugin` for setup pages | `DESIGNED` — plugin does not exist |
| `paletteOverride()` | `QGCCorePlugin.h` | Aerora semantic colour roles | `DESIGNED` |
| `registerCustomSettings()` | `QGCCorePlugin.h` | Aerora settings groups | `DESIGNED` |
| `overrideSettingsGroupVisibility()` | `QGCCorePlugin.h` | Hide groups a profile does not use | `DESIGNED` |
| `adjustSettingMetaData()` | `QGCCorePlugin.h` | Retune individual settings | `DESIGNED` |
| `complexMissionItemNames()` / `createComplexMissionItem()` | `QGCCorePlugin.h` | Aerora surface item | `SPECIFIED-NOT-DESIGNED` |
| `planCreators()` | `QGCCorePlugin.h` | Aerora plan templates | `SPECIFIED-NOT-DESIGNED` |
| `customMapItems()` | `QGCCorePlugin.h:158` | Aerora map items | `DESIGNED` |
| `preFlightChecklistUrl()` | `QGCOptions.h:95` | Aerora checklist | `DESIGNED` |
| `QGCOptions` / `QGCFlyViewOptions` booleans | `QGCOptions.h` | Hide surfaces a profile does not use | `DESIGNED` |
| `factValueGridCreateDefaultSettings()` | `QGCCorePlugin.h` | Aerora default value-grid cells | `DESIGNED` |
| `getOfflineCameraDefinitionFile()` | `QGCCorePlugin.h` | Aerora camera definitions | `DESIGNED` |
| `firstRunPromptCustomIds()` / `firstRunPromptResource()` | `QGCCorePlugin.h` | Aerora first-run disclosure (ids ≥ 10000) | `DESIGNED` |
| plan-file hooks ×6 | `QGCCorePlugin.h` | Aerora plan metadata round-trip | `SPECIFIED-NOT-DESIGNED` |
| `authorizeMavCommand()` | `QGCCorePlugin.h` | **The authorization point** | `IMPLEMENTED-UNCOMPILED`, evaluates nothing |
| `mavlinkMessage()` | `QGCCorePlugin.h` | Inbound observation | Not used |
| `createVideoReceiver` / `createVideoSink` / `releaseVideoSink` | `QGCCorePlugin.h` | — | **Deliberately not overridden** |
| `createRootWindow()` | `QGCCorePlugin.h` | — | **Deliberately not overridden** (would fork the shell) |

### Tier 2 — purpose-built QML seams

| Seam | Instantiated at | Aerora use | State |
| --- | --- | --- | --- |
| `FlyViewCustomLayer.qml` | `FlyView.qml:145` | `SCR-LIV-01` and all its children | `DESIGNED` |
| `CustomMapItems.qml` | `FlyViewMap.qml:248` | Aerora map items | `DESIGNED` — on `forbiddenPaths()` for *override*; the C++ `customMapItems()` route is the supported one |
| `CustomGuidedActionsController.qml` | `GuidedActionsController.qml:188` | Aerora guided actions | **Unused.** Gated on an authorized command path |
| `FlyViewAdditionalCustomActionsList.qml` | `FlyViewAdditionalActionsButton.qml:28` | Aerora additional actions | **Unused.** Same gate |

`CustomMapItems.qml` deserves its note: it *reads* as a fourth empty seam but upstream populates it
with an `Instantiator` over `QGCCorePlugin::customMapItems()`. Overriding the QML would fork a
populated file **and** discard the C++ seam that is the supported way in. It is on
`AeroraQmlOverrides::forbiddenPaths()` and
`AeroraUiFoundationTest::_customMapItemsIsForbiddenAndUndeclared` asserts it is both forbidden and
undeclared — the mistake is unrepresentable, not merely discouraged.

### Tier 3 — populated-file replacement

**Count: 0. Target: 0.**

| Candidate | Would enable | Decision |
| --- | --- | --- |
| `SelectViewDropdown.qml` | Top-level Aerora view entries | **Deferred.** [D-NAV-01](#d-nav-01--the-top-level-entry-point) resolves it with the hub instead |
| `FlyViewToolStripActionList.qml` | An Aerora tool-strip action | **Not needed.** Every candidate action is a command with no path (`Q-038`) |
| `FlyViewToolBar.qml` | Restructured toolbar layout | **Not needed.** Indicators can be added cleanly; restructuring is cosmetic |

Keeping this at zero is the main reason an upstream rebase stays mechanical (`P7-ARCH-023`).

## 11. Open UI decisions

| ID | Decision | Owner | Blocks | Status |
| --- | --- | --- | --- | --- |
| `D-NAV-01` | Top-level entry point: hub vs `SelectViewDropdown.qml` fork | **Product** | Nothing — hub is adopted and sufficient | **DECIDED** (hub); fork deferred and available |
| `D-UI-01` | A QML-safe read path for `isAuthorizationEnforced()` without widening the security surface | Tab 2 + Tab 7 | — | **CLOSED** — `AeroraSecurityStatus` (WP2-B), `QML_NAMED_ELEMENT(AeroraSecurity)` + `QML_SINGLETON`, six `READ`-only properties, no `WRITE`, no `Q_INVOKABLE`, no slot, and in `AeroraUiSafetyTest::auditedTypes()`. This row read `OPEN` while three others in this document and `08` read closed — the `R7-20` failure mode, corrected here |
| `D-UI-02` | `UI-A11Y-03` contrast target value | Tab 7 + product | Nothing structurally; needed for acceptance | `OPEN` |
| `D-UI-03` | `AeroraStateValue::capability` must lose `WRITE` (HO-TAB7-01) | Tab 4 | — | **CLOSED** — U3, `a7b317f89`. `IMPLEMENTED-UNCOMPILED` |
| `D-UI-04` | `AeroraControlsModule` block omits `IMPORTS QGC` and `qgc_set_qt_resource_alias` versus HO-TAB7-03 | Tab 1 | — | **CLOSED** — U2, `a73699796`. `IMPLEMENTED-UNCOMPILED` |
| `D-UI-05` | Whether an Aerora QML Quick Test target is created now (HO-TAB7-04) | Tab 1 + Tab 7 | QML rendering tests only | `OPEN` — recommend at the first QML behaviour needing assertion |
| `D-UI-06` | Multi-station authority display — which station holds authority | **Architecture** | `SCR-SHL-03`, `SCR-LIV-07` | `OPEN` — raised by [benchmark §7.3](02-gcs-reference-benchmark.md#73-which-station-holds-authority-when-several-are-connected) |

`D-UI-04` was the one with a concrete cost, and it was unusually well evidenced because **Tab 7's
own manifest documented the requirement that Tab 1's aggregation had omitted**. The evidence is kept
because it is what the alias behaviour rests on, and `HO-TAB7-06` depends on that behaviour again:

| Fact | Source |
| --- | --- |
| `IMPORTS QGC` is "the load-bearing line" — how `AeroraTelemetryState` and `AeroraUi` resolve inside the module | `aerora/src/UI/Ui.cmake:64` |
| Without the alias the resource lands at `qrc:/qml/Aerora/Controls/src/UI/Controls/<Name>.qml` instead of `qrc:/qml/Aerora/Controls/<Name>.qml` | `aerora/src/UI/Ui.cmake:57-61` |
| The function sets `QT_RESOURCE_ALIAS` to the bare filename | `cmake/Helpers.cmake:12-22` |
| `AERORA_UI_QML_FILES` holds absolute paths, so the nesting is real rather than hypothetical | `aerora/src/UI/Ui.cmake:136-143` |
| The aggregation **now carries both lines**; it previously omitted them on shadowing grounds | `aerora/CMakeLists.txt:194`, `:201-202` |

The omission's justification is the interesting part: it reasons that aliases exist to shadow stock
QGroundControl files by basename, and that `Aerora.Controls` shadows nothing. That is true about
*shadowing* and irrelevant to *path flattening*, which is what the alias actually does here. Both
effects are build-time facts a first configure would surface, and neither is detectable on this host.

Assigned to Tab 1 because `aerora/CMakeLists.txt` is Tab 1's file.

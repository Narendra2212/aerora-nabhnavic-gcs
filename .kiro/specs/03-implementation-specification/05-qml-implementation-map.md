# QML Implementation Map

Every approved UI seam, the override mechanism as it actually works, and how the seven state classes
reach a pixel.

## Seam inventory

Counted in the checkout. All `GCS-VERIFIED`.

| Tier | Mechanism | Count | Maintenance cost |
| --- | --- | --- | --- |
| 1 | C++ virtual on `QGCOptions`, `QGCFlyViewOptions`, `QGCCorePlugin` | 36+ | None — a supported interface |
| 2 | Purpose-built empty QML seam provided by upstream | 4 | Low |
| 3 | Override of a populated upstream QML file | 1 | **High — a silent fork** |

### Tier 2 — the four purpose-built seams

| Seam file | Instantiated by | Upstream content | Aerora use | Status |
| --- | --- | --- | --- | --- |
| `src/FlyView/FlyViewCustomLayer.qml` | `FlyView.qml:145` | Empty `Item`, three properties | Aerora fly-view overlay: payload, authority, link, power | `IMPLEMENTATION-PLANNED` |
| `src/FlyView/CustomGuidedActionsController.qml` | `GuidedActionsController.qml:188` | `QtObject`, two stubs returning false | Aerora guided actions | `IMPLEMENTATION-PLANNED` |
| `src/FlyView/FlyViewAdditionalCustomActionsList.qml` | `FlyViewAdditionalActionsButton.qml:28` | Invisible `Item`, empty model | Aerora entries in the additional-actions list | `IMPLEMENTATION-PLANNED` |
| `src/FlightMap/MapItems/CustomMapItems.qml` | `FlyViewMap.qml:248` | Empty `Item`, a `map` property | Aerora mission and product map visuals | `IMPLEMENTATION-PLANNED` |

The guided-actions seam is the most valuable of the four. Its two stubs return `false` to mean "not
handled here", which is a clean delegation protocol: Aerora returns `true` for Aerora action codes
and `false` for everything else, with no change to upstream dispatch.

### Tier 3 — the one override

| File | Reason | Upstream content | Obligation |
| --- | --- | --- | --- |
| `src/FlyView/FlyViewToolStripActionList.qml` | The tool strip is a flat model literal with no insertion seam | Eight model entries | Recorded fork point, named owner, drift review at every merge |

The owner is still unassigned — Q-038. Phase 4 does not assign it; that is a process decision.
Keeping this list at **one** entry is the mitigation, and proposing a tool-strip insertion seam
upstream is preferable to adding a second.

### What is deliberately not overridden

| File | Reached instead by |
| --- | --- |
| `FlyView.qml` | `FlyViewCustomLayer.qml` |
| `GuidedActionsController.qml` | `CustomGuidedActionsController.qml` |
| `FlyViewMap.qml` | `CustomMapItems.qml` |
| Settings and setup QML | The `QGCCorePlugin` visibility virtuals |
| Toolbar QML | The `QGCOptions` toolbar accessors |

Each is a file a custom build might reasonably have forked and does not need to.

## Override resolution — the mechanism, not the myth

Phase 3.1 corrected this and the correction is authoritative. **Do not reintroduce the
alias-collision model for QML.**

| Step | Location | Behaviour |
| --- | --- | --- |
| 1 | `src/API/QGCCorePlugin.h:117` | `createQmlApplicationEngine` is virtual, with a symmetric destroy hook for interceptor release |
| 2 | `src/QGCApplication.cc:275` | The application obtains its engine through the plugin, so the override is on the real boot path |
| 3 | `custom-example/CMakeLists.txt:84-97` | Overlay QML declared under `PREFIX "/Custom/qml"` with `QT_RESOURCE_ALIAS` set to the **stock** path |
| 4 | `custom-example/src/CustomPlugin.cc:247-248` | `addUrlInterceptor(new CustomOverrideInterceptor())` |
| 5 | `custom-example/src/CustomPlugin.cc:273-295` | `intercept()` forms `:/Custom<origPath>` and returns it **only if `QFile::exists()`** |

```text
engine requests  qrc:/qml/QGroundControl/FlyView/FlyViewCustomLayer.qml
      │
      ▼
interceptor tests  :/Custom/qml/QGroundControl/FlyView/FlyViewCustomLayer.qml
      │
      ├── exists  ──►  overlay file loads
      └── absent  ──►  upstream file loads, unchanged
```

No two resources share a URL. Resolution is one existence check. No link order, no ambiguity.

### A new Phase 4 finding: the interceptor does not cover C++ resource reads

The interceptor is installed on the **QML engine** and acts on `DataType::QmlFile` and
`DataType::UrlString` where `url.scheme() == "qrc"`. A C++ `QFile(":/json/...")` read never passes
through it.

| Resource consumer | Path form | Covered by the interceptor? |
| --- | --- | --- |
| QML engine loading a component | `qrc:/qml/...` | **Yes** |
| `QGCApplication.cc:311` window icon | `qrc:/res/...`, then `interceptUrl()` called **explicitly** | Yes, by an explicit call |
| `QGCSerialPortInfo.cc:47` board table | `":/json/USBBoardInfo.json"` | **No** |
| `FactGroup` metadata load | `":/json/Vehicle/<Name>Fact.json"` | **No** |

The reference overlay registers its own JSON at `PREFIX "/json"`
(`custom-example/CMakeLists.txt:121-135`) — the **same prefix** as the core resource, not a
`/Custom` mirror. So for C++-read resources the override genuinely is an alias question, and the
precedence of two resources registered at one path is still unverified.

| Consequence | Detail |
| --- | --- |
| Adding a **new** JSON file at `/json` is safe | No collision; the name is unique |
| Overriding an **existing** core JSON file at `/json` is unverified | This is Q-020's true residual, narrowed to C++-read resources |
| Aerora's fact-metadata files are new names | So they are in the safe category |
| Overriding `USBBoardInfo.json` would be in the unverified category | Which is why it is not the chosen F3.1-02 mitigation |

Recorded as **Q4-001**. The startup self-check must therefore assert resource resolution for both
classes separately, not once.

## State-class rendering

The mandatory distinction, as an implementation rule rather than a style guide.

| State class | Binding source | Visual treatment | Never |
| --- | --- | --- | --- |
| `EFFECTIVE` | Vehicle-reported Fact | Primary reading | Shown when none has been reported |
| `REPORTED` | Vehicle-reported Fact within its freshness threshold | Primary, freshness available | — |
| `REQUESTED` | Command tracker state | Subordinate, labelled | Promoted to effective |
| `PROPOSED` | Autonomy fact group | Subordinate, labelled, visually distinct from vehicle state | Rendered as authoritative |
| `STALE` | Freshness evaluator | Marked, with its age | Shown plain; hidden |
| `UNKNOWN` | No report received | Explicit unknown marking | Rendered as zero, dash-as-zero, or empty |
| `UNAVAILABLE` | Capability model says absent | Element absent | Rendered as unknown; shown disabled |

### The implementation rule

> The state class is a property of the **binding**, not a decision made per screen.

Concretely: an Aerora value reaches QML as a Fact plus a state-class property supplied by
`M-Telemetry`. A single reusable control consumes both. A screen author cannot accidentally render a
proposal as effective state, because the control decides the treatment from the class.

This is the only defence that scales. A rule that each screen must remember is a rule that one
screen will forget, and the forgetting is invisible.

| Artefact | Module | Status |
| --- | --- | --- |
| `AeroraStateValue` — the Fact-plus-class binding type | `M-Telemetry` | `IMPLEMENTATION-PLANNED` |
| `AeroraValueDisplay` — the control that renders by class | `M-Ui` | `IMPLEMENTATION-PLANNED` |
| `AeroraActionButton` — hide versus disable by capability | `M-Ui` | **`BLOCKED`** — F3.1-05 |

## Aerora QML modules

| Module URI | Content | Registration |
| --- | --- | --- |
| Aerora widgets | Reusable controls, including the state-class display | `qt_add_qml_module`, exported as a library |
| Aerora fly view | Fly-view overlay content | Same |
| Aerora plan | Complex-item visuals and editors | Same |
| Aerora setup | Aerora setup pages | Same |

A module URI is a namespace, so an Aerora control may share a name with an upstream control without
colliding. Import paths are added in `createQmlApplicationEngine`, following
`custom-example/src/CustomPlugin.cc:244-245`.

Division of labour, which the reference overlay demonstrates and which is easy to get backwards:

| Artefact | Goes into |
| --- | --- |
| Self-registering C++ | The **application target**, via `CUSTOM_SOURCES` |
| QML modules | Aerora **libraries**, via `qt_add_qml_module` |

## Indicator separation

`IF-UIS-009` enumerates six pairs that must share no visual language. The implementation obligation
is that each pair uses a different control, not merely a different colour.

| Pair | Implementation consequence |
| --- | --- |
| Cloud connectivity and vehicle link | Two distinct indicator types in the toolbar, not one type with two instances |
| Vehicle-reported active link and local link observation | Two rows in the link panel, differently shaped |
| Payload emergency stop and payload state | A dedicated stop indicator, not a state enumeration member |
| Watchdog event and payload emergency stop | Two independent event rows, never nested |
| Authority mode and flight mode | Two separate selectors in different regions |
| Requested and effective authority mode | One primary reading plus a subordinate labelled pending row |

Verification is `DEMO` for all six: whether two indicators are genuinely distinguishable is not a
property a unit test can assert. Q-040 gates it, because "share no visual language" cannot be
assessed before a visual language exists.

## Startup self-check

`AERORA-UI-REQ-007` and `AERORA-TEST-REQ-007`. Must fail **visibly**, not into a log line.

| Assertion | Detects | Writable today? |
| --- | --- | --- |
| Every Aerora override resource exists at its mirrored `/Custom` path | A build-system alias mistake | Yes |
| Interception of each overridden stock QML URL returns the `/Custom` URL | An override that silently did not take effect | **Yes** — the Phase 3.1 correction |
| No stock QML URL is intercepted that Aerora did not intend to override | An override that silently replaced upstream behaviour | Yes |
| Every Aerora QML module URI is importable | A missing or misnamed module | Yes |
| Each Aerora `:/json` resource resolves to Aerora content | The C++-read resource case | **Partly** — Q4-001 |
| The selected firmware plugin is the Aerora one | The R-010 first-match failure | Yes |
| No listening socket is open | The `qthttpserver` exposure | Yes |

Six of seven are writable now. The seventh is narrowed to a question rather than blocked.

A quietly missing Aerora layer is worse than a refusal to start, because an operator cannot
distinguish "the payload panel is absent" from "the payload has nothing to report".

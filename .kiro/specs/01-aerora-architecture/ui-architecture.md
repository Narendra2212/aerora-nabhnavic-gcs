# UI Architecture

How the Aerora operator interface is composed over QGroundControl's: the seam preference order, the
inventory of what Aerora overrides, and the presentation rules the rest of the architecture imposes
on the interface.

## The seam preference order

Every Aerora interface change attaches in one of three ways. They are not equivalent, and the
preference order is the most important rule in this document.

| Tier | Mechanism | Maintenance cost |
| --- | --- | --- |
| 1 | A C++ virtual on `QGCCorePlugin`, `QGCOptions` or `QGCFlyViewOptions` | None. The signature is a supported interface |
| 2 | A purpose-built empty QML seam that upstream provides for this purpose | Low. Upstream has no reason to change it, and if it does the change is small |
| 3 | Overriding a populated upstream QML file by resource alias | High. It is a silent fork: upstream improvements to that file are invisibly discarded |

**Use the lowest tier that works.** Tier 3 is permitted but each instance is a recorded decision
with a named owner and a fork point, because tier 3 has a failure mode the other two do not: when
upstream fixes a bug in an overridden file, the Aerora build keeps the bug and nothing reports it.
No compiler warning, no lint rule, no merge conflict. It simply stays broken.

## Tier 1: the C++ seams

QGroundControl exposes a large, supported surface for custom builds. Counted in the current
checkout:

| Seam | Virtuals | Controls |
| --- | --- | --- |
| `QGCOptions` | 30 | Toolbar, menus, sensor calibration pages, firmware upgrade, joystick, mission constraints, multi-vehicle, pixel ratio, preflight checklist URL |
| `QGCFlyViewOptions` | 6 | Multi-vehicle list, map scale, instrument panel, guided-bar emergency stop, orbit, region of interest |
| `QGCCorePlugin` | Several | Analyze pages, settings-group visibility, palette colour override, advanced-UI message, initial-setup prompts, fact value grid defaults |

Three of these are worth calling out because they replace work that would otherwise be done badly:

| Seam | Aerora use |
| --- | --- |
| `QGCOptions::preFlightChecklistUrl()` | Returns an Aerora checklist URL. An Aerora checklist needs no QML override at all |
| `QGCCorePlugin::overrideSettingsGroupVisibility()` | Hides settings groups Aerora does not use, by name, without touching settings QML |
| `QGCOptions::multiVehicleEnabled()` | The correct seam for a single-vehicle product — not the unreachable `singleFirmwareSupport` flag described in [vehicle-integration.md](vehicle-integration.md) |

`QGCOptions::combineSettingsAndSetup()` and the sensor-calibration visibility accessors between
them remove most of the motivation for overriding settings and setup QML, which is where a custom
build would otherwise accumulate tier-3 overrides fastest.

## Tier 2: the purpose-built QML seams

Upstream provides four QML files that exist solely to be overridden. Each is effectively empty and
each is instantiated by a populated upstream file, so overriding the seam reaches the composition
point without forking the composer.

| Seam file | Instantiated by | Content upstream | Fork point |
| --- | --- | --- | --- |
| `src/FlyView/FlyViewCustomLayer.qml` | `FlyView.qml:145` | Empty `Item` with three properties | `f2f163196` (2026-10-01) |
| `src/FlyView/CustomGuidedActionsController.qml` | `GuidedActionsController.qml:188` | `QtObject` with two stubs that return false | `9eb09113d` (2026-01-02) |
| `src/FlyView/FlyViewAdditionalCustomActionsList.qml` | `FlyViewAdditionalActionsButton.qml:28` | Invisible `Item` with an empty model | `9eb09113d` (2026-01-02) |
| `src/FlightMap/MapItems/CustomMapItems.qml` | `FlyViewMap.qml:248` | Empty `Item` with a `map` property | `9eb09113d` (2026-01-02) |

Each carries an upstream comment stating it is there for custom builds to override, so the contract
is explicit rather than inferred.

The guided-actions seam is the most valuable of the four. Its two stub functions return false to
mean "not handled here", which is a clean delegation protocol: Aerora guided actions are added by
returning true for Aerora action codes and false for everything else, with no change to upstream's
action dispatch.

| Aerora use | Seam |
| --- | --- |
| Aerora fly-view overlay: payload status, authority state, link state, power | `FlyViewCustomLayer.qml` |
| Aerora guided actions: payload commands, authority changes | `CustomGuidedActionsController.qml` |
| Aerora entries in the additional-actions list | `FlyViewAdditionalCustomActionsList.qml` |
| Aerora mission and paint-pattern map visuals | `CustomMapItems.qml` |

Four seams cover the Aerora fly-view surface. That is the finding that keeps this architecture
cheap, and it was not obvious before the inventory was taken.

## Tier 3: the override inventory

### Current content

One tier-3 override is anticipated, and it is the same one `custom-example` takes:

| Overridden file | Why | Upstream content | Fork point | Owner |
| --- | --- | --- | --- | --- |
| `src/FlyView/FlyViewToolStripActionList.qml` | The tool strip is a flat model literal with no insertion seam | Eight model entries | `b2db29fe4` (2026-02-03) | `OPEN` |

The tool strip is the one place where upstream composes by a literal array rather than through a
seam. There is no way to add an Aerora tool-strip action without replacing the list.

### The obligation that comes with it

Each tier-3 override carries four commitments:

| Commitment | Purpose |
| --- | --- |
| The fork-point commit is recorded in this table | Makes "what has upstream changed since?" answerable by one command |
| A named owner | Someone is accountable for reviewing upstream changes to that file |
| An upstream-drift review at each upstream merge | The only way the silent-fork failure mode gets caught |
| A recorded reason | So the override can be removed if upstream later adds a seam |

The drift review is a procedure, and procedures decay. The mitigation is to keep the tier-3 list
very short — one entry is maintainable, a dozen is not — and to prefer proposing a seam upstream
over adding a second entry. A tool-strip insertion seam is a small, obviously useful upstream
contribution and is listed as optional in [risk-gates.md](risk-gates.md).

### What is explicitly not overridden

| File | Why not |
| --- | --- |
| `FlyView.qml` | Composes the whole view; `FlyViewCustomLayer.qml` reaches it |
| `GuidedActionsController.qml` | Large and central; `CustomGuidedActionsController.qml` reaches it |
| `FlyViewMap.qml` | `CustomMapItems.qml` reaches it |
| Settings and setup QML | The C++ visibility seams reach them |
| Toolbar QML | The `QGCOptions` toolbar accessors reach it |

Each row is a file a custom build might reasonably have overridden and does not need to.

## The override mechanism

A QML override works by resource aliasing: the Aerora file is added to the build with a
`QT_RESOURCE_ALIAS` naming the upstream resource path, so the Aerora file occupies the URL the
upstream file would have.

```text
overlay/src/AeroraFlyViewCustomLayer.qml
        │  QT_RESOURCE_ALIAS = QGroundControl/FlyView/FlyViewCustomLayer.qml
        ▼
qt_add_resources(PREFIX "/Aerora/qml")
        │
        ▼
the URL that FlyView.qml resolves at runtime
```

### Q-020: alias precedence must be confirmed

Two resources aliased to the same URL is precisely the situation this mechanism creates, and the
precedence — which one the QML engine resolves — is the substance of Q-020, ranked second in
[phase-2-open-question-resolution.md](phase-2-open-question-resolution.md).

This is ranked that high because the failure is silent and bidirectional. If the Aerora resource
does not win, the Aerora interface is simply absent with no error. If it wins for a file Aerora did
not intend to override, upstream behaviour disappears with no error. Either way the build succeeds.

It is resolvable in days — build the overlay, inspect what the engine resolves — and it must be
resolved before any tier-2 or tier-3 override is relied upon.

## Startup self-check

R-019 is the risk that an override silently fails. The mitigation is a startup self-check that runs
before the interface is shown, per [runtime-architecture.md](runtime-architecture.md):

| Assertion | Detects |
| --- | --- |
| Every Aerora override resource exists at its aliased URL | A build-system mistake in the alias |
| Every Aerora QML module URI is importable | A missing or misnamed QML module |
| The resolved content at each overridden URL is the Aerora content | The Q-020 precedence failure |
| The selected firmware plugin is the Aerora one | The R-010 factory-conflict failure |

The third assertion is the one that catches the silent fork. It is also the only assertion here
that cannot be written until Q-020 is resolved, because it needs to know what "resolved content" is
observable as.

A failed self-check is a visible startup error, not a log line. An interface that is quietly
missing its Aerora layer is worse than one that refuses to start, because an operator cannot tell
the difference between "the payload panel is absent" and "the payload has nothing to report".

## Aerora QML modules

Aerora QML lives in its own modules with their own URIs, registered through `qt_add_qml_module` and
exported to the main build as libraries, following the overlay's existing library export.

| Module | Content |
| --- | --- |
| Aerora widgets | Reusable Aerora controls |
| Aerora fly view | The fly-view overlay content |
| Aerora plan | Aerora complex-item visuals and editors |
| Aerora setup | Aerora setup pages |

Modules are the right unit here rather than loose files, because a URI is a namespace: an Aerora
control named the same as an upstream control cannot collide.

Note the division of labour with the C++ build layout from
[vehicle-integration.md](vehicle-integration.md): QML modules go into Aerora **libraries**,
self-registering C++ goes into the main **target**. Putting either in the other's place fails —
quietly in one direction, loudly in the other.

## Presentation rules

These are not style preferences. Each is an architectural commitment from another document that
the interface is where it either holds or fails.

| Rule | Source |
| --- | --- |
| Effective state is primary; requested state is subordinate and labelled | Second architectural principle |
| Every Aerora value shows its age when stale; stale is never shown as current | [runtime-architecture.md](runtime-architecture.md) |
| Cloud connectivity and vehicle-link connectivity share no visual language | [cloud-architecture.md](cloud-architecture.md) |
| Companion proposals are visually subordinate to vehicle state and labelled as proposed | [gpu-integration.md](gpu-integration.md) |
| An unavailable action is hidden, not shown disabled | [vehicle-integration.md](vehicle-integration.md) |
| Vehicle-reported active link is distinguished from GCS-local link observation | [communication-architecture.md](communication-architecture.md) |
| Payload emergency-stop state is shown distinctly from payload state | [payload-architecture.md](payload-architecture.md) |
| A command accepted but not effective is surfaced | [authority-model.md](authority-model.md) |

### On hiding rather than disabling

The hide-not-disable rule deserves its reasoning, because the conventional interface advice is the
opposite. A disabled control tells an operator the capability exists and is momentarily
unavailable. For an Aerora action whose availability depends on a vehicle-reported capability, that
message is wrong: the capability may not exist on this aircraft at all. A disabled button invites
the operator to wait for it to become available, and waiting is the wrong response.

Where the distinction genuinely is temporal — an action unavailable because the vehicle is in the
wrong state — disabled with a stated reason is correct. The rule is therefore: hide what the
aircraft cannot do, disable with a reason what it cannot do *right now*.

## The advanced-UI toggle is not an authorisation boundary

QGroundControl's advanced-UI toggle reveals additional controls and carries a confirmation message
through `QGCCorePlugin::showAdvancedUIMessage()`. It is a **disclosure** control: it changes what
is visible.

It is not authentication, not authorisation, and not an access control. Anyone at the keyboard can
turn it on. Treating it as a permission boundary — "only advanced users can reach the payload
calibration page, because it is behind the advanced toggle" — would be a security control that is
one click deep.

Aerora may use the toggle to reduce routine visual clutter. Aerora must not use it as the mechanism
protecting any operation that requires an authorised operator. The authorisation model is in
[security-architecture.md](security-architecture.md), and the two mechanisms are independent: an
operation may require both an authorised role and the advanced toggle, but the toggle alone never
suffices.

## Core edits required

**None.** Tier 1 and tier 2 are supported interfaces. Tier 3 is a resource override, which is a
build-configuration action in the overlay and not a modification of an upstream file.

That distinction is worth keeping precise: a tier-3 override leaves the upstream file untouched in
the working tree. `git diff` stays empty. That is what makes the drift silent, and it is why the
inventory table above exists instead of relying on version control to surface it.

## Open questions

| Question | Subject | Impact |
| --- | --- | --- |
| Q-020 | Whether a custom build can shadow an existing resource alias, and which wins | `IMPL-BLOCKING` |
| Q-029 | Operator role set, which determines what the interface gates | `ARCH-SHAPING` |
| Q-007 | Flight-mode mapping, which determines which guided actions appear | `ARCH-SHAPING` |
| Q-040 | Aerora visual design language | `DEFERRABLE` |

## Requirements derived

| Identifier | Statement |
| --- | --- |
| `AERORA-UI-REQ-004` | An Aerora interface change shall attach at the lowest available seam tier: a C++ virtual before a purpose-built QML seam before overriding a populated upstream file |
| `AERORA-UI-REQ-005` | Each override of a populated upstream file shall be recorded with its reason, fork-point commit and named owner |
| `AERORA-UI-REQ-006` | Each upstream merge shall include a drift review of every overridden populated file |
| `AERORA-UI-REQ-007` | A startup self-check shall verify that every Aerora override resource resolves to the Aerora content, and shall fail visibly if it does not |
| `AERORA-UI-REQ-008` | An action the aircraft cannot perform shall be hidden; an action it cannot perform in its current state shall be disabled with a stated reason |
| `AERORA-UI-REQ-010` | Aerora QML shall be delivered as named QML modules rather than loose files |

The advanced-UI obligation is carried by `AERORA-SEC-REQ-014` rather than duplicated here.
`AERORA-UI-REQ-009` was withdrawn as a duplicate; the withdrawal is recorded in
[requirements.md](requirements.md) so the identifier is not reused.

Full records in [requirements.md](requirements.md).

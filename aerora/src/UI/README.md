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
| `Ui.cmake` | Source manifest, and the integration instructions for `aerora/CMakeLists.txt` |

## What is deliberately not here

No panel, no indicator, no product surface, no guided action, no tool-strip entry. Every Aerora
fact group is `CONTRACT-ONLY` for want of a dialect, so there is no Aerora value to populate a panel
with — building one would be shape with nothing behind it.

The interface is **observation and supervision**, not flight authority. QML cannot reach payload
command submission, power switching, mission commanding, command authorization or security policy;
`AeroraUiSafetyTest` asserts that against each type's meta-object rather than leaving it to review.

## Outstanding, in other workstreams' files

| Handoff | Owner | Effect while open |
| --- | --- | --- |
| HO-TAB7-02 | Tab 1 — install the interceptor on the QML engine | No QML override takes effect |
| HO-TAB7-03 | Tab 1 — create the `Aerora.Controls` QML module target | The four controls are not registered |
| HO-TAB7-01 | Tab 4 — make `AeroraStateValue::capability` read-only in QML | Latent; not reachable from the current surface |

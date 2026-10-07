# 11 — GCS UI/UX Specification

## Primary views

- Dashboard
- Fly
- Map
- Mission
- Vehicle
- Payload
- AI
- Video
- Diagnostics
- Logs
- Parameters
- Settings
- Simulation/Test

## Safety UI

Critical states must be visible without requiring deep navigation.

Examples:
- disconnected
- degraded telemetry
- GPS/estimator issue
- battery critical
- failsafe
- payload fault
- AI unavailable
- mission validation failure

## Interaction principles

- deliberate confirmation for high-consequence commands
- clear mode/state indication
- consistent terminology
- no hidden automatic action
- distinguish live vs simulation
- operator error prevention
- keyboard/mouse and touch-aware layout where supported
- accessibility considerations

## Localization

Use Qt translation mechanisms. Do not hard-code user-visible strings in a way that prevents translation.

## Product-specific operator views

The dashboard shall include at minimum:
- active vehicle/control mode
- active communications link
- tether/battery power source
- battery/energy state
- GPU/autonomy health
- flight-controller health
- watchdog state
- payload state
- mission state
- geofence/airspace state where available

The painting workflow shall include payload readiness, tank/pump/nozzle/gimbal status, mission geometry and emergency payload stop state.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

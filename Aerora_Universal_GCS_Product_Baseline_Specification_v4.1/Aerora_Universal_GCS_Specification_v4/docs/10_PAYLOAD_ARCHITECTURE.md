# 10 — Universal Payload Architecture

## Generic interface

```text
Payload
├── identity
├── version
├── capabilities
├── health
├── telemetry
├── configuration
├── commands
├── calibration
├── safety state
└── diagnostics
```

## Painting

Candidate capabilities:
- tank level
- pump
- pressure
- flow
- nozzle
- color/channel
- gimbal
- payload emergency stop

## Firefighting

Future candidate capabilities:
- suppressant tank
- pump
- nozzle
- flow
- targeting/gimbal
- thermal camera

## Agriculture

Future candidate capabilities:
- tank
- pump
- flow
- spray/nozzle
- coverage parameters

## Rule

Payload-specific code must not leak into generic flight-control modules.

## Product baseline

The first target payload is painting. The firmware model contains `Painting_System`, `Payload_Actuator_Drivers`, payload E-stop and reaction-compensation outputs. It also contains placeholders for gimbal pan/tilt, spray state/rate and pump control.

The GCS payload framework must therefore support both:
- continuous payload telemetry
- mission-synchronized payload actions

The painting module shall be implemented as one payload plugin within the universal payload framework.

## Painting supply configurations

### Tethered mode
- Ground supplies paint through the tether paint hose.
- The onboard paint tank is detached.
- The 480 V DC tether power cable and tether paint hose are attached as the tethered operating set.
- The GCS shall display tether paint-supply status separately from aircraft electrical power status.

### Battery mode
- The onboard battery supplies aircraft power.
- An onboard paint tank is attached and becomes the active paint source.
- The GCS shall expose tank level/availability and payload configuration state.

The GCS must treat the two configurations as explicit equipment states rather than assuming that both paint sources are simultaneously installed. Mechanical attachment detection and the final payload interlock implementation are still to be defined.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

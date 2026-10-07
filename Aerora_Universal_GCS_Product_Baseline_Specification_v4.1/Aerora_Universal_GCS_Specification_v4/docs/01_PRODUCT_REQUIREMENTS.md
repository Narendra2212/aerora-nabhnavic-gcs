# 01 — Product Requirements

## Requirement conventions

- `PR` = product requirement
- `SR` = system requirement
- `SAF` = safety requirement
- `SEC` = security requirement
- `IF` = interface requirement
- `VR` = verification requirement

## Product requirements

| ID | Requirement | Criticality |
|---|---|---|
| PR-001 | Connect to supported MAVLink vehicles and expose connection state. | Critical |
| PR-002 | Display vehicle position, attitude, altitude, speed, battery and link health. | Critical |
| PR-003 | Support mission creation, validation, upload, execution-state monitoring and recovery workflows. | Critical |
| PR-004 | Support vehicle configuration/parameter workflows appropriate to the vehicle. | High |
| PR-005 | Provide pre-flight health/readiness visibility. | Critical |
| PR-006 | Provide prominent, actionable fault/failsafe information. | Critical |
| PR-007 | Identify Aerora vehicle capabilities. | Critical |
| PR-008 | Implement a generic payload capability interface. | High |
| PR-009 | Implement painting payload integration without coupling it to generic flight-control code. | High |
| PR-010 | Permit future firefighting/agriculture payload modules without redesigning the GCS core. | High |
| PR-011 | Record operator, command, fault and mission events. | Critical |
| PR-012 | Provide a distinct simulation/test mode. | Critical |
| PR-013 | Support versioned Aerora MAVLink extensions. | Critical |
| PR-014 | Integrate AI through explicit health/status and safety boundaries. | Critical |
| PR-015 | Integrate ROS 2 through documented interfaces. | High |
| PR-016 | Display software, firmware, protocol, configuration and AI versions. | High |
| PR-017 | Support reproducible production builds. | High |
| PR-018 | Maintain third-party license/dependency records. | High |
| PR-019 | Provide controlled update and rollback mechanisms. | High |
| PR-020 | Support accessibility and operator usability requirements. | Medium |
| PR-021 | Support internationalization architecture even if initial release is English. | Medium |
| PR-022 | Preserve enough upstream alignment to enable controlled QGC updates. | High |
| PR-023 | Support diagnostic export for field support. | High |
| PR-024 | Prevent simulation/live-vehicle mode confusion through explicit state and UI separation. | Critical |
| PR-025 | Permit validated multi-vehicle operation as a future capability. | Medium |

## Product acceptance

Each requirement requires:
- acceptance criteria
- implementation reference
- verification method
- evidence
- release status


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.


## Product-specific operating configuration requirements (v4)

### Tethered painting
- The vehicle shall support a tethered painting configuration using **480 V DC supplied from the ground**.
- The vehicle power subsystem shall convert the tether input to the aircraft 48 V power domain onboard.
- Paint shall be supplied from the ground through the tether paint hose.
- The onboard paint tank shall be detached in the tethered painting configuration.

### Battery painting
- The vehicle shall support battery-powered painting using an onboard battery.
- The vehicle shall carry an onboard paint tank in battery painting configuration.
- The configuration shall support operation in complex/remote areas where tether operation is unsuitable.

### Failsafe status
- Final vehicle failsafe responses are deferred until the Simulink flight-controller implementation is updated and reviewed.

### Cloud/software architecture
- AWS shall be the primary cloud platform for cloud-side Aerora GCS services.
- The core backend shall prefer a modular-monolith architecture.
- Real-time vehicle control shall remain local to the operator GCS and vehicle systems and shall not require AWS availability.

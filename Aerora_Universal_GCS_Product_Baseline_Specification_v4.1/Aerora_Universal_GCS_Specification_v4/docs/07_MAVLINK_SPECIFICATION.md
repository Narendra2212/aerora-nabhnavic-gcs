# 07 — Aerora MAVLink Specification

## Principles

- Use standard MAVLink messages when semantically sufficient.
- Add Aerora messages only where standard semantics are inadequate.
- Keep dialect versioned.
- Define units explicitly.
- Define acknowledgement semantics.
- Define timeout/retry behavior.
- Define compatibility.

## Candidate message families

`AERORA_SYSTEM_STATUS`  
`AERORA_VEHICLE_CAPABILITIES`  
`AERORA_PAYLOAD_STATUS`  
`AERORA_PAINT_STATUS`  
`AERORA_PUMP_STATUS`  
`AERORA_GIMBAL_STATUS`  
`AERORA_AI_STATUS`  
`AERORA_MISSION_STATUS`  
`AERORA_DIAGNOSTICS`

## Command definition template

Each command must document:
- ID
- sender
- receiver
- parameters
- units
- valid range
- preconditions
- acknowledgement
- timeout
- retry policy
- failure behavior
- operator feedback
- safety classification

## Compatibility

Maintain a matrix of:
GCS version × FC firmware × payload firmware × MAVLink dialect version.

## Current product role of MAVLink

The uploaded firmware model contains a `Mavlink` subsystem receiving data through a UART/USART path and exposing `Auto_Cmds`/GPU-link state. Therefore MAVLink is an explicit FC <-> onboard-GPU interface in the current product architecture, not merely a future GCS protocol.

The GCS shall use the same versioned protocol architecture for vehicle telemetry and control where appropriate. The FC <-> GPU interface and GCS <-> FC interface should be explicitly separated in the ICD even if they share the MAVLink dialect.

## Required MAVLink domains

- vehicle heartbeat/identity
- command authority
- manual/autonomous mode
- autonomy proposal/acceptance/rejection
- GPU message freshness
- mission state
- payload state
- safety/failsafe state
- power-source state
- link state
- sensor health
- diagnostics
- logs/version information


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

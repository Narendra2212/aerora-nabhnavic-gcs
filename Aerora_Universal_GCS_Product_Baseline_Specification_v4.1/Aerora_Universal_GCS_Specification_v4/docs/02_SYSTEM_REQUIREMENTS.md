# 02 — System Requirements

## 1. Ground platform

Initial supported platform:
- Windows x64

Architecture should avoid unnecessary platform-specific coupling because QGC supports multiple platforms.

## 2. External connectivity

Potential interfaces:
- USB
- serial
- telemetry radio
- Ethernet
- Wi-Fi
- cellular network
- UDP/TCP where appropriate
- video streams

Each interface must have defined discovery, timeout, error and recovery behavior.

## 3. Vehicle-side systems

Possible systems:
- Aerora flight controller
- Aerora ESCs
- Jetson/companion computer
- payload controller
- cameras
- telemetry modem
- positioning sensors

AUGCS must not assume all vehicles have all systems.

## 4. Resource requirements

The final product shall define:
- minimum RAM
- recommended RAM
- CPU requirement
- GPU requirement for optional AI/video functions
- storage requirement
- network requirement
- supported display resolution

These values must be established by benchmark rather than guessed.

## 5. Security requirements

Production deployment shall define:
- authentication model
- credential storage
- transport security where applicable
- signed software/update artifacts
- role model
- audit logging
- secure configuration

## 6. Time

All timestamped data shall define:
- source clock
- time base
- units
- synchronization assumptions
- behavior when clocks disagree


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

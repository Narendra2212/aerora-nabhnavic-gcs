# 18 — Configuration Management

## Configuration identity

Every released system must identify:
- GCS version
- QGC commit
- Aerora code version
- MAVLink dialect/version
- FC firmware
- payload firmware
- ROS 2 version
- AI model
- configuration schema

## Configuration classes

- build configuration
- vehicle configuration
- operator configuration
- mission configuration
- payload configuration
- AI configuration
- security configuration

## Rule

Configuration changes that affect behavior must be versioned and, where appropriate, validated before use.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

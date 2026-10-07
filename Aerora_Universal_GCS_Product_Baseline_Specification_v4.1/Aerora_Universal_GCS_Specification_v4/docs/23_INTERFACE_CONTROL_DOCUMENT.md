# 23 — Interface Control Document

## Interfaces

1. GCS <-> Flight Controller
2. GCS <-> Payload Controller
3. GCS <-> Jetson/Companion
4. GCS <-> ROS 2
5. GCS <-> Video
6. GCS <-> Telemetry Transport
7. GCS <-> File/Log storage
8. GCS <-> Update service

## Interface record

Each interface requires:
- protocol
- version
- endpoint
- message format
- timing
- units
- state machine
- error handling
- security
- compatibility
- test method

No undocumented flight-critical interface should be treated as stable.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

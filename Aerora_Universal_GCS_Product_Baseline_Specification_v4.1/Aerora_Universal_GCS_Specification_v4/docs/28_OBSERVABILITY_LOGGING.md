# 28 — Observability and Logging

## Log categories

- application
- connection
- MAVLink/protocol
- mission
- vehicle
- payload
- AI
- ROS 2
- security
- update
- operator action

## Event record

Minimum:
- timestamp
- severity
- subsystem
- event ID
- message
- relevant vehicle/payload ID
- software version

## Privacy/security

Logs must not expose secrets. Sensitive data must have a documented retention/export policy.

## Field diagnostics

Provide a controlled diagnostic export that bundles relevant versions, configuration metadata and logs.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

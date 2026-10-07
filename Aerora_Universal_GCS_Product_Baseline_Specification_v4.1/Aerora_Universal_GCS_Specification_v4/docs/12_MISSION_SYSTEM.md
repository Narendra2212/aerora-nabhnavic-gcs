# 12 — Mission System

## Mission object

A mission includes:
- vehicle capability requirements
- payload requirements
- geographic geometry
- altitude/speed constraints
- payload actions
- safety constraints
- execution state
- completion criteria

## Candidate mission types

- waypoint
- survey
- structure scan
- facade/painting
- inspection
- payload route

## Validation

Before execution:
- capability check
- geofence
- mission geometry
- altitude limits
- payload compatibility
- sensor requirements
- energy/battery estimate where supported
- communications assumptions
- action sequencing
- recovery behavior

## State model

`Draft -> Validating -> Ready -> Uploaded -> Executing -> Paused/Aborted -> Completed`

Invalid transitions must be rejected and logged.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

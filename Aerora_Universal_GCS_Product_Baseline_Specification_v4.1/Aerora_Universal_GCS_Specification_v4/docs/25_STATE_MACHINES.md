# 25 — Critical State Machines

## Connection

`Disconnected -> Connecting -> Connected -> Degraded -> Reconnecting -> Disconnected`

## Mission

`Draft -> Validating -> Ready -> Uploading -> Uploaded -> Executing -> Paused -> Completed/Aborted`

## Payload

`Unavailable -> Detected -> Initializing -> Ready -> Active -> Fault -> Stopped`

## AI

`Unavailable -> Initializing -> Ready -> Processing -> Degraded -> Fault`

## Rule

Every state machine must define:
- legal transitions
- illegal transitions
- timeout
- recovery
- persistence/logging
- operator visibility


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

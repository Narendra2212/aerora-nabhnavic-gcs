# 24 — Data Model and Telemetry

## Data classes

- vehicle state
- link state
- navigation state
- battery
- propulsion
- mission
- payload
- AI
- diagnostics
- operator actions

## Telemetry quality

Every important value should define:
- source
- timestamp
- unit
- validity
- freshness
- optional confidence

## Stale data

UI should distinguish:
- valid/current
- valid but stale
- unavailable
- invalid

Never present stale critical telemetry as current.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

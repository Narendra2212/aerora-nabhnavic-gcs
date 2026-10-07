# 15 — Simulation and Testing

## Test pyramid

```text
Unit
  -> Component
  -> Integration
  -> SITL
  -> HIL
  -> Bench
  -> Controlled flight
  -> Production validation
```

## Test categories

- functional
- regression
- protocol
- UI
- mission
- fault injection
- performance
- security
- compatibility
- installation/update
- recovery
- AI
- payload

## Fault injection examples

- disconnect telemetry
- drop MAVLink packets
- delay messages
- corrupt/invalid messages
- kill AI process
- overload AI queue
- remove video
- report payload fault
- restart companion
- restart GCS

## Evidence

Each test records:
- software version
- configuration
- environment
- input
- expected result
- actual result
- logs
- verdict


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

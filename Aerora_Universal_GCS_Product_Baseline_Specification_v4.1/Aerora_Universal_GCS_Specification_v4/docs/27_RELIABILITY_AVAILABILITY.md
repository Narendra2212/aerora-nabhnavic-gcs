# 27 — Reliability and Availability

## Reliability goals

- deterministic startup
- controlled shutdown
- no silent command failure
- bounded resource use
- recoverable communication loss
- robust malformed-input handling
- crash diagnostics
- version compatibility checks

## Long-duration testing

Production candidates should undergo soak testing with:
- telemetry
- map
- video where applicable
- mission updates
- reconnect cycles
- logging
- payload telemetry

## Availability

Define product-specific availability targets only after operational usage data exists.

## Product-specific reliability

Reliability must include degraded operation across:
- tether/battery power transitions
- LoRa/FHSS24/RF433 link changes
- GPU unavailable/stale
- GCS network loss
- sensor degradation
- payload failure
- storage/logging failure

The FC must retain local safety authority if the GCS is unavailable.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

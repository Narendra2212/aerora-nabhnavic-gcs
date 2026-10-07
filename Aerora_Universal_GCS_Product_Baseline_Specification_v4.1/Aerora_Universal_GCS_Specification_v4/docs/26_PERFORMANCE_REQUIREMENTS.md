# 26 — Performance Requirements

## Performance categories

Measure rather than guess:
- application startup
- UI responsiveness
- telemetry update latency
- command acknowledgement latency
- mission upload throughput
- map rendering
- video latency
- AI inference latency
- memory use
- CPU/GPU use
- long-duration stability

## Initial engineering targets

Targets must be established through representative workloads and recorded in release-specific benchmarks.

## Performance rule

No optimization should compromise correctness or safety without explicit review.

## Product timing baseline

The uploaded Simulink model contains multiple sample times, including observed 2 ms, 10 ms, 20 ms and 100 ms paths. The model identifies 100 ms as the base rate for specific logic and uses STM32H7 Embedded Coder configuration. These observations are not a final flight-control timing specification.

The GCS shall not impose UI/network timing assumptions on the FC control loop. Instead, it shall expose telemetry freshness and command acknowledgement deadlines defined by the FC/GCS ICD.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

# 40 — Product Roadmap

## Stage 0 — Foundation
- specification
- QGC baseline
- build reproducibility
- license/dependency inventory

## Stage 1 — Aerora GCS shell
- custom build
- branding
- configuration
- basic Aerora plugin structure

## Stage 2 — Aerora vehicle
- vehicle identification
- telemetry
- health
- parameters
- command integration

## Stage 3 — Universal payload
- payload capability model
- painting integration

## Stage 4 — Mission
- industrial mission model
- painting mission

## Stage 5 — AI/ROS2
- perception
- Jetson
- ROS2 interfaces

## Stage 6 — Additional payloads
- firefighting
- agriculture

## Stage 7 — Product hardening
- security
- reliability
- update/recovery
- field diagnostics
- production release process

The roadmap is a sequence of engineering gates, not a promise of dates.

## Product-specific sequencing

### Phase A — Current hexacopter baseline
- dual-power model
- six-motor vehicle model
- FC/GPU MAVLink
- manual/semi-auto/auto authority
- LoRa + additional link abstraction
- painting payload

### Phase B — Universal GCS
- capability-based vehicle model
- generic payload API
- transport abstraction
- configuration/version compatibility

### Phase C — Industrial autonomy
- AI observability
- mission/payload synchronization
- inspection/firefighting/agriculture modules

### Phase D — Assurance hardening
- DGCA evidence integration
- cybersecurity
- SBOM
- update/rollback
- fault injection
- long-duration testing

### Phase E — Future high-assurance/defense-capable architecture
- stronger identity/key management
- secure communications adapters
- hardened logging/audit
- role-based operator controls
- offline operation
- customer-specific assurance/certification profiles

This is a technical scalability target, not a claim of defense certification.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

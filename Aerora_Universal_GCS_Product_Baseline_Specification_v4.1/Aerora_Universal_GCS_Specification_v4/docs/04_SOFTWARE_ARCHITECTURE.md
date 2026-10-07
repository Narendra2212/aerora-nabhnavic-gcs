# 04 — Software Architecture

## Layers

1. Platform
2. QGC foundation
3. Aerora domain
4. Application/UI
5. Optional external services

## Module principles

Every significant module should have:
- responsibility
- public interface
- lifecycle
- error model
- thread model
- test strategy
- owner

## Dependency direction

Higher-level UI may depend on domain services. Domain services must not depend on concrete UI elements.

## Concurrency

- UI thread must remain responsive.
- High-rate telemetry processing must not block UI.
- Video/AI processing must use bounded queues.
- Shutdown must be deterministic.
- Cancellation must be supported for long operations.

## Memory/resource management

Use RAII and existing Qt ownership patterns consistently. Avoid raw owning pointers.

## Error handling

No silent failure for:
- command rejection
- malformed telemetry
- stale state
- connection loss
- incompatible versions
- mission validation failures
- payload faults

## API stability

Public Aerora interfaces require versioning or explicit compatibility policy.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

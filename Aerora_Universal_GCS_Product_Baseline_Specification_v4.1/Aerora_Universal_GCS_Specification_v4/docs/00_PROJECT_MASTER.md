# 00 — Project Master Specification

## 1. Product

**Name:** Aerora Universal Ground Control Station (AUGCS)  
**Company:** Aerora Dynamics Private Limited  
**Product class:** Commercial UAV ground control software  
**Foundation:** QGroundControl controlled baseline  
**Primary development IDE:** Kiro  
**Primary languages:** C++, QML, supporting Python/CMake where justified

## 2. Vision

Build one reusable GCS platform for Aerora's intelligent industrial UAV family instead of building separate GCS applications for every payload.

The GCS shall support a common vehicle layer and a modular payload layer.

## 3. Product families

Initial target:
- painting UAV
- firefighting UAV
- agriculture UAV
- industrial inspection UAV

Future product categories shall reuse the same platform where technically appropriate.

## 4. Core principle

**Reuse mature GCS infrastructure; build Aerora differentiation in modular layers.**

QGroundControl's current documentation explicitly supports custom builds through `QGCCorePlugin`, `FirmwarePlugin`, and `AutoPilotPlugin`, and its current CMake configuration supports a custom overlay directory. These are preferred extension points before invasive upstream modification.

## 5. Product boundaries

### GCS owns
- operator interaction
- mission authoring
- vehicle/payload visualization
- configuration workflows
- telemetry presentation
- command orchestration
- logging
- diagnostics
- simulation integration
- validated mission/payload workflows

### Flight controller owns
- stabilization
- low-level flight control
- actuator control
- vehicle-local failsafes
- estimator/control-loop authority

### Payload controller owns
- actuator-level payload behavior
- payload-local interlocks
- payload emergency behavior

### AI/companion owns
- perception/inference
- defined high-level robotics services
- non-authoritative decision support unless separately safety-qualified

## 6. Design goals

- reliable
- maintainable
- modular
- testable
- secure
- auditable
- versioned
- updateable
- commercially distributable subject to applicable licenses and regulations
- adaptable to future Aerora vehicles
- AWS-first cloud infrastructure with a predominantly modular-monolith service architecture
- local/offline-capable real-time GCS operation

## 7. Non-goals

- rewriting QGC for cosmetic reasons
- replacing MAVLink without a demonstrated need
- making the GCS the sole safety mechanism
- allowing AI to bypass validated command interfaces
- uncontrolled autonomous agent changes
- adding dependencies without review

## 8. Product maturity gates

### Prototype
Basic connection, telemetry, mission, configuration.

### Engineering Alpha
Aerora vehicle integration, payload framework, diagnostics, automated tests.

### Beta
SITL/HIL, controlled hardware validation, security/license inventory, release process.

### Production
Defined requirements, verification evidence, signed artifacts, support documentation, configuration manifest, approved release.

## 9. Master rule

No feature is considered production-ready solely because it works on a developer workstation.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

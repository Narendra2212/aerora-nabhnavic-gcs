# 45 — Compliance Readiness Framework

## Purpose

Prepare evidence so future certification, procurement or regulated-customer requirements can be addressed without reconstructing engineering history.

## Evidence categories

- requirements
- architecture
- risk/hazard analysis
- configuration management
- software verification
- security
- dependency/SBOM
- release integrity
- operator documentation
- incident/change records

## Regulatory principle

Do not assert compliance with a standard or government program unless the exact applicable version, scope, product configuration and evidence have been reviewed by the responsible organization/qualified expert.

## Future work

When a target market or customer is identified, create a dedicated compliance matrix:
requirement -> evidence -> responsible owner -> status.

## Product-specific DGCA target

The GCS shall be designed to support the aircraft's applicable DGCA obligations, including configuration/type-certificate traceability, operational checks, airspace/geofence awareness, NPNT-related status where applicable, command-and-control monitoring, tracking/logging and maintenance evidence. Current DGCA/eGCA requirements must be checked at the time of certification because the regulatory portal and service processes can change.

The product shall have a compliance profile layer so a future regulatory change can update the compliance adapter rather than require rewriting the flight-control architecture.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.


## v4 product configuration note

The compliance baseline shall distinguish the two physical operating configurations. Tethered operation introduces a ground-supplied 480 V DC system and tether paint supply, while battery operation uses onboard battery power and an onboard paint tank. The applicable regulatory and operational assessment shall be performed against the actual final configuration and current DGCA requirements.

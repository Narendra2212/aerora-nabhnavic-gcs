# 47 — DGCA Compliance-Readiness Framework

## Scope

Aerora wants the GCS architecture to be designed for DGCA compliance and future certification. This document establishes a compliance-ready architecture; it does **not** declare that the current aircraft, firmware or GCS is compliant or type-certified.

## Current regulatory baseline

The current DGCA website continues to list the Drone Rules, 2021 under drone regulations. DGCA's public material states that drone-related regulatory services D-1 to D-5 were migrated from Digital Sky to eGCA in 2025. DGCA's current portal also states that registration of non-type-certified UAS is temporarily suspended. Therefore, regulatory workflows must be treated as versioned external requirements and checked against the current DGCA/eGCA process before each certification/operational release.

The Drone Rules, 2021 and related official guidance include safety concepts such as type certification/registration, NPNT, geo-fencing, tracking, reliable command-and-control, flight data logging and other equipment/operational requirements depending on UAS category and intended operation. The exact applicability must be assessed against the actual aircraft category, operating envelope and current DGCA rules/guidance.

## GCS compliance capabilities

The GCS architecture shall be able to support:

- vehicle identity/UIN-related records where applicable
- type-certificate/configuration records
- remote-pilot/operator records where applicable
- pre-flight checklist
- airspace/geographical-zone awareness where official data/services are available
- geofence configuration and status
- NPNT/permission state integration where applicable
- real-time tracking/telemetry interfaces where applicable
- flight-data logging
- command-and-control link monitoring
- emergency/recovery state presentation
- maintenance/configuration records
- software/firmware version traceability
- incident/event logging
- exportable evidence

## GCS must not falsely claim compliance

The application shall distinguish:
- `Compliant/verified`
- `Configured`
- `Not configured`
- `Unavailable`
- `Unknown`
- `Not applicable`

A UI green indicator must never mean “DGCA compliant” unless a defined compliance rule and evidence source support that claim.

## NPNT architecture

Where NPNT is applicable, the GCS should act as an integration/presentation component. The authoritative permission verification and tamper-resistant enforcement must remain in the designated flight-module/hardware/firmware architecture required by the applicable specification.

## Airspace

The GCS should support a replaceable airspace-data provider so that changes in official services do not require redesigning the flight-control architecture.

## BVLOS/autonomous operations

Autonomy must not be assumed to mean regulatory authorization for BVLOS or any other operation. The GCS shall support configurable operational constraints and shall surface when the planned operation exceeds the approved operational envelope.

## Compliance evidence

Maintain:
- requirement-to-test traceability
- configuration baseline
- firmware/GCS versions
- logs
- test evidence
- cybersecurity evidence
- dependency/SBOM
- type/certification documents
- approved operational limitations

## Sources

Use the current DGCA regulations/guidance portal and current eGCA/Digital Sky instructions as the authoritative source at certification time. Do not freeze a regulatory URL or rule interpretation into software without a version/update process.

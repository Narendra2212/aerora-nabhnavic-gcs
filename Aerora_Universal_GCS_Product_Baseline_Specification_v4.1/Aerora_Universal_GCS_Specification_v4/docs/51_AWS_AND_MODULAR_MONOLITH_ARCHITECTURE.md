# 51 — AWS and Predominantly Modular-Monolith Architecture

## 1. Architecture decision

Aerora shall use AWS as the primary cloud platform and shall prefer a **modular monolith** for the main application/service structure. The objective is to keep the system understandable, testable and deployable as one coherent product while retaining clean internal boundaries for future extraction of individual services if justified.

This is not a requirement to put real-time flight control in the cloud. The operator GCS remains an edge application running on the operator computer and must remain capable of real-time local command/control without AWS availability.

## 2. Logical structure

```text
Operator PC
  |
  +-- Aerora Universal GCS (local executable)
  |      +-- UI/QML
  |      +-- Vehicle domain
  |      +-- MAVLink
  |      +-- Link/transport abstraction
  |      +-- Mission
  |      +-- Payload
  |      +-- AI/companion integration
  |      +-- Local logging
  |      +-- Local configuration/cache
  |
  +-- Optional AWS connectivity
         |
         v
   AWS Modular Monolith
         +-- authentication/authorization
         +-- fleet/device registry
         +-- mission/document synchronization
         +-- telemetry ingestion where appropriate
         +-- flight/payload records
         +-- audit/event records
         +-- user/organization management
         +-- configuration distribution
         +-- update metadata
         +-- reporting
         +-- API layer
         +-- admin/operations
         |
         +-- AWS managed infrastructure
                +-- object storage
                +-- relational data
                +-- secrets/keys
                +-- logs/metrics
                +-- CI/CD
                +-- backups
```

## 3. Monolith boundaries

The modular monolith shall have explicit internal modules such as:

- Identity and access
- Organization/fleet management
- Vehicle registry
- Mission management
- Payload management
- Telemetry/event ingestion
- Device configuration
- Software/firmware release metadata
- Audit logging
- Reporting
- Notification/event handling

Modules communicate through typed application interfaces rather than direct database coupling. A single deployable backend is preferred until scale, reliability or organizational requirements justify service extraction.

## 4. Real-time boundary

The following must remain operational without AWS:

- manual flight command path
- local vehicle telemetry display
- local emergency/operator interfaces supported by the approved operational concept
- MAVLink communication to the vehicle
- payload control functions that are intended to operate locally
- local logging needed for flight records

AWS must not become a hidden single point of failure for flight safety.

## 5. AWS responsibilities

AWS is the preferred platform for:

- source/CI infrastructure
- build and release automation
- artifact storage
- model/data storage where applicable
- fleet and vehicle records
- mission synchronization
- centralized configuration metadata
- operational dashboards
- audit records
- backups/disaster recovery
- controlled software update distribution
- engineering telemetry aggregation where permitted by the operational/security design

## 6. Security

Cloud components shall use least-privilege IAM, encrypted storage/transport, managed secrets, audit logging, environment separation and signed/versioned release artifacts. Flight-critical credentials and device keys must not be hard-coded into the application.

## 7. Offline-first operational principle

Loss of AWS connectivity shall degrade cloud synchronization and fleet services but shall not by itself create an undefined flight behavior. The GCS shall clearly show cloud connectivity separately from vehicle-link connectivity.

## 8. Future scalability

The modular-monolith design shall preserve clean interfaces so that high-load or high-assurance deployments can later extract selected functions such as telemetry ingestion, mission synchronization, authentication, reporting or fleet management into independent services without redesigning the vehicle protocol or GCS domain model.

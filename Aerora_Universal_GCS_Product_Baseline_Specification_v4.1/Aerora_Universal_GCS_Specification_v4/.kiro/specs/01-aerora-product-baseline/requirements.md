# Aerora Product Baseline — Requirements v4

## Goal
Translate the analyzed product evidence and latest product decisions into an approved baseline for future GCS architecture work.

### R1 — Dual operating power configurations
The architecture shall support two defined aircraft operating configurations:
1. Tethered painting: **480 V DC** ground supply through the tether, converted onboard by a DC-DC stage to the aircraft 48 V power domain.
2. Battery painting: onboard battery operation for complex/remote areas.

The GCS shall model the active power configuration and validated power telemetry. The current baseline does not claim seamless automatic in-flight switching between configurations.

### R2 — Tethered painting supply
In tethered painting configuration, the ground shall supply paint through the tether paint hose and the onboard paint tank shall be detached. The tether power cable and paint hose shall be represented as part of the physical operating configuration.

### R3 — Battery painting supply
In battery painting configuration, the onboard battery shall supply the aircraft and an onboard paint tank shall be attached and represented as the active paint source.

### R4 — Control authority
The architecture shall support manual, supervised semi-auto and fully autonomous modes with explicit authority state.

### R5 — Multi-link control
The GCS shall support transport abstraction for LoRa and additional RF links and display authoritative active-link state.

### R6 — GPU autonomy
The GCS shall integrate onboard GPU status and MAVLink-based FC/GPU command state without bypassing FC safety authority.

### R7 — Universal platform
The GCS shall support multiple future Aerora vehicle and payload types through capability-based interfaces.

### R8 — Painting
Painting shall be the first payload implementation within a generic payload architecture and shall support the tethered and battery painting configurations defined above.

### R9 — DGCA readiness
The architecture shall provide versioned interfaces for applicable DGCA operational/compliance requirements.

### R10 — High-assurance scalability
The architecture shall support future stronger security, audit, offline and role-control requirements without redesigning the core vehicle abstraction.

### R11 — Failsafe definition status
Final flight-controller failsafe behavior shall remain deferred until the evolving Simulink implementation is updated and reviewed. The GCS shall not invent final RTL/LAND/HOLD/termination behavior.

### R12 — AWS and modular monolith
AWS shall be the primary cloud platform for cloud-side Aerora services. The main backend/application service structure shall prefer a modular monolith with explicit internal module boundaries. Real-time flight control shall remain local and shall not depend on AWS availability.

## Acceptance
- Product baseline report reviewed.
- Power/control/link/AI/payload boundaries documented.
- Tethered and battery painting configurations documented.
- AWS/modular-monolith deployment boundary documented.
- Open firmware placeholders identified.
- Final failsafe behavior explicitly deferred until the updated Simulink baseline is available.
- No QGC source modification required for this specification task.

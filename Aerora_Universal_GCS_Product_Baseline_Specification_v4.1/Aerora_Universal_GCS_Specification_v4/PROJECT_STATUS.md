# Aerora Universal GCS — Project Status (v4)

## Current phase
Product-specific specification and evidence baseline.

## Source modification status
Aerora-specific QGC source modification: **NOT STARTED**.

## Product baseline analyzed
- FC KiCad schematic
- FC KiCad PCB
- Simulink firmware/control model

## Current product architecture
- Hexacopter
- tether + battery dual power
- tethered painting: 480 V ground input -> onboard conversion -> 48 V aircraft power; ground paint via tether hose; onboard tank detached
- battery painting: onboard battery + onboard paint tank for complex-area operation
- manual / semi-auto / fully autonomous control
- LoRa + additional RF command links
- onboard GPU intelligence
- MAVLink GPU <-> FC interface
- painting payload first
- AWS-first cloud infrastructure with predominantly modular-monolith application architecture
- universal GCS for future Aerora products

## Latest decisions and controlled open items

### Power / paint configuration
- Tether mode uses 480 V supplied from ground and converted onboard to the aircraft 48 V domain.
- Tether mode uses ground paint through a tether hose; onboard paint tank is detached.
- Battery mode uses onboard battery power and an onboard paint tank.
- Exact 480 V source type, converter topology/rating, isolation, protection, connectorization and grounding are not yet frozen.

### Failsafe
- Final failsafe behavior is deliberately deferred until the Simulink implementation is updated.
- No final RTL/LAND/HOLD/termination behavior is claimed by this specification.

### Software deployment
- AWS is the primary cloud platform.
- Prefer a modular monolith for the core application/services rather than a microservice-heavy architecture.
- Real-time GCS flight control must remain local/offline-capable and must not depend on AWS availability.

## Critical open items
The uploaded firmware model explicitly contains placeholders for several safety, timing, geofence, parameter, watchdog and payload values. These must be resolved before production claims.

## Next task
Complete QGC baseline analysis without modifying QGC source, then reconcile the QGC baseline with this v4 product specification before creating the first implementation Spec.

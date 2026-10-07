# Aerora Universal GCS — Engineering Specification Package v4

This package is the product-specific specification baseline for Aerora Universal GCS after analysis of the uploaded hexacopter FC schematic, PCB and Simulink firmware model. v4 incorporates the latest product decisions for tethered/battery painting operation, detachable paint supply configuration, deferred failsafe definition, and an AWS-first predominantly modular-monolith software architecture.

## Product baseline

Aerora's current target is a hexacopter with:
- dual power architecture: 480 V ground tether power converted onboard to a 48 V aircraft bus in tethered operation + onboard battery for battery operation and complex-area access
- tethered painting configuration: ground paint supply through tether hose; onboard paint tank detached
- battery painting configuration: onboard battery power + onboard paint tank attached
- manual, supervised semi-autonomous and fully autonomous operation
- multiple command links including LoRa and additional RF links
- onboard GPU/companion intelligence
- MAVLink between GPU and flight controller
- painting as the first industrial payload
- universal GCS architecture for future Aerora products
- DGCA compliance-readiness as a design objective
- architecture that can be hardened/scaled for future high-assurance/defense customer requirements

## New product decisions incorporated in v4

### Power and painting configurations

**Tethered painting mode**
- Ground system supplies 480 V through the tether power cable.
- The aircraft contains the onboard conversion stage that converts the tether input to the aircraft 48 V power domain.
- Paint is supplied from the ground through the tether paint hose.
- The onboard paint tank is detached in this configuration.
- The tether power cable and tether paint hose are attached as the tethered operating set.

**Battery painting mode**
- The onboard battery is the aircraft power source.
- The aircraft carries the onboard paint tank.
- This configuration is intended for complex/remote areas where tethered operation is unsuitable.

The exact electrical input type of the 480 V source (for example AC versus DC), isolation topology, converter rating, cable protection, connector system, grounding, interlocks and source-transition behavior remain controlled engineering items until the power-system design is frozen.

### Failsafe status

Final failsafe behavior is intentionally **not frozen in this version**. The evolving Simulink flight-controller implementation will provide the next authoritative definition. The GCS documentation therefore defines observability, authority and logging requirements but does not invent RTL/LAND/HOLD or other vehicle responses.

### AWS and software architecture

The project shall use AWS as the primary cloud platform. The software should be implemented as a **predominantly modular monolith** rather than prematurely splitting the system into many microservices. Cloud-side capabilities may be deployed as one primary application with well-defined internal modules and supporting managed AWS services. The real-time operator GCS remains a local application and must not require AWS connectivity for flight-critical command/control.

## Important status

The uploaded Simulink model contains explicit placeholders and pending integration items. These are recorded as open engineering work, not treated as completed safety features.

## Next engineering step

Run `.kiro/specs/00-qgc-baseline-analysis` against the exact QGC checkout. Then review the resulting QGC baseline together with this product baseline before creating the first implementation Spec.

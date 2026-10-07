# 13 — Safety and Failsafe

## Safety architecture

Safety is distributed:
- flight controller local safety
- payload local safety
- communication safety
- GCS supervisory safety
- operator procedures

## Faults to cover

- GCS crash
- telemetry loss
- command timeout
- malformed message
- stale telemetry
- GPS degradation
- estimator fault
- battery critical
- motor/ESC fault
- sensor fault
- geofence
- companion failure
- AI failure
- video failure
- payload failure
- storage failure

## For every hazard

Document:
1. detection
2. classification
3. immediate safe state
4. operator notification
5. recovery
6. logging
7. verification

## Important

Exact flight responses such as RTL, LAND, HOLD or other behavior must be defined by the actual flight-controller architecture and validated in simulation/bench testing. They must not be invented by the GCS UI layer.

## Product-specific safety architecture

The uploaded model already contains multiple safety mechanisms and also explicitly identifies unfinished placeholders. The specification shall treat these as design inputs, not proof of completion.

Important observed paths:
- watchdog fault into disarm logic
- watchdog fault directly into payload E-stop
- manual-dominant autonomy blending concept
- semi-auto explicit pilot confirmation
- autonomous proposal rejection
- all-links-stale detection
- GPS/optical-flow plausibility candidate
- parameter validation skeleton
- boot/arming state skeleton

Before production, each must be converted into a formal requirement and verified against the real FC hardware and operational concept.

## Failsafe definition status

Final vehicle failsafe behavior is intentionally **deferred**. The next authoritative definition will come from the evolving Simulink flight-controller implementation. Until that update is incorporated, this GCS specification shall not assign final RTL, LAND, HOLD, termination, mode-change or other emergency responses. The GCS shall instead provide the framework to display, timestamp and log the authoritative FC safety/failsafe state once the FC contract is frozen.

## Power-related safety boundary

The tethered configuration introduces a 480 V ground power source and an onboard conversion stage to 48 V. The GCS shall not be treated as the safety controller for high-voltage isolation, converter protection, grounding, connector interlock or tether electrical fault protection. Those functions belong to the electrical/power-system design and its local protection mechanisms. The GCS may display validated power-system status and faults.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

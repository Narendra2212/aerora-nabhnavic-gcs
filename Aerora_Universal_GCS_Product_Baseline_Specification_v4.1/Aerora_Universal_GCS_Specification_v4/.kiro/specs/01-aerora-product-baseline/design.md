# Aerora Product Baseline — Design v4

Use the uploaded schematic, PCB and Simulink model as evidence. Separate confirmed implementation facts from product requirements and unresolved assumptions.

The resulting architecture must keep:
- vehicle control
- autonomy
- communications
- payload
- power
- GCS
- cloud/backend
- compliance
as separate but explicitly interfaced domains.

## Operating configuration boundary

The painting UAV has two defined physical configurations:

```text
TETHERED PAINTING
480 V DC ground
   |
   +-- tether power cable --> onboard DC-DC conversion --> 48 V aircraft bus
   |
   +-- tether paint hose --> painting payload
                              onboard tank detached

BATTERY PAINTING
onboard battery --> aircraft power
onboard paint tank --> painting payload
```

## Software deployment boundary

```text
Local operator GCS
   |
   +-- real-time vehicle/payload control
   +-- MAVLink/radio communication
   +-- local telemetry/logging
   |
   +-- optional AWS connectivity
           |
           +-- modular-monolith cloud backend
           +-- fleet/mission/configuration records
           +-- artifacts/data/audit/operations
```

AWS availability must not be a prerequisite for flight-critical local operation.

## Failsafe boundary

Final vehicle failsafe behavior remains a flight-controller/Simulink-controlled design item. The GCS consumes and displays authoritative safety state and does not invent final emergency responses.

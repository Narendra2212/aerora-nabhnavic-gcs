# 03 — System Architecture

```text
                       AERORA UNIVERSAL GCS
+----------------------------------------------------------------+
|                           UI / QML                             |
| Dashboard | Map | Mission | Vehicle | Payload | AI | Logs     |
+-------------------------------+--------------------------------+
                                |
+-------------------------------v--------------------------------+
|                        Application Services                    |
| Vehicle | Mission | Command | Telemetry | Parameters | Video   |
| Payload | Diagnostics | Logging | Configuration | Update       |
+-------------------------------+--------------------------------+
                                |
+-------------------------------v--------------------------------+
|                     Aerora Extension Layer                     |
| Aerora Core | Vehicle | MAVLink | Mission | Payload           |
| AI | ROS2 | Safety Presentation | Product UI                  |
+-------------------------------+--------------------------------+
                                |
+-------------------------------v--------------------------------+
|                    QGroundControl Foundation                   |
| MAVLink | Vehicle | Mission | Map | Video | Parameters | Logs|
+-------------------------------+--------------------------------+
                                |
                         External interfaces
                                |
             +------------------+------------------+
             |                  |                  |
          Aerora FC        Companion/Jetson    Payload MCU
```

## Architectural rules

1. QGC foundation remains as close to upstream as practical.
2. Aerora code is separated from generic QGC code.
3. Commands pass through a validated command path.
4. Telemetry is treated as untrusted external input.
5. UI never directly manipulates low-level transport.
6. AI/ROS2 cannot silently bypass safety gates.
7. Payloads are capability modules.
8. All cross-module interfaces are documented.

## Data flow

Telemetry:
`transport -> MAVLink -> vehicle model -> domain state -> UI/services`

Command:
`UI/service -> validation -> command abstraction -> MAVLink -> vehicle`

Payload:
`UI/mission -> payload service -> capability checks -> payload protocol -> controller`

AI:
`camera -> acquisition -> inference -> result validation -> application/mission integration`

## Aerora product architecture

The current product is a dual-power hexacopter with tether + battery operation, onboard GPU intelligence, three control modes and multiple command links. The GCS shall therefore be a supervisory/control platform for a distributed system rather than a simple remote waypoint application.

```text
Operator
   |
   v
AUGCS
   |
   +-- command/mission/telemetry services
   |
   +-- link abstraction
        +-- LoRa
        +-- FHSS24
        +-- RF433
   |
   v
Aerora Flight Controller
   |
   +-- STM32H743 primary control
   +-- STM32G071 safety/watchdog domain
   +-- sensor fusion/control
   +-- hexacopter mixer
   +-- local failsafes
   |
   +<-- MAVLink --> Onboard GPU/Companion
   |                    |
   |                    +-- perception
   |                    +-- autonomy
   |                    +-- painting intelligence
   |                    +-- future industrial intelligence
   |
   +--> Payload controllers / actuators
```

The GCS must show **effective authority** rather than assuming that the operator or GPU is always authoritative.

## Painting operating configurations

The first product shall support two physical painting configurations:

### Tethered painting configuration
```text
Ground power: 480 V
      |
      | tether power cable
      v
Onboard conversion
      |
      v
Aircraft 48 V power domain

Ground paint supply
      |
      | tether paint hose
      v
Painting payload / nozzle

Onboard paint tank: DETACHED
```

### Battery painting configuration
```text
Onboard battery
      |
      v
Aircraft power system

Onboard paint tank
      |
      v
Painting payload / nozzle
```

The battery configuration is intended to support complex/remote areas where tether operation is unsuitable. The GCS shall identify the active physical configuration and prevent configuration-specific controls from being presented as available when the required hardware is not detected/declared.

The exact 480 V input electrical form (AC/DC), converter topology/rating, isolation, protection, grounding, connectorization and tether transition behavior remain open until the power subsystem is frozen.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

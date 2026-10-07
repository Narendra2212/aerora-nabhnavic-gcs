# 48 — Operational Control and Authority Architecture

## Control modes

Aerora's product specification requires three operator/control modes:

1. Manual
2. Semi-autonomous / supervised
3. Fully autonomous

The firmware artifact already contains explicit mode-authority logic. The GCS must mirror the same state model rather than create a competing authority model.

## Authority hierarchy

### Manual
Pilot commands are authoritative within the flight controller's validated input limits.

### Semi-autonomous
The onboard intelligence may propose/assist, but the pilot must have an explicit authority/confirmation path as defined by the FC firmware. The GCS shall show whether that confirmation is active.

### Fully autonomous
The onboard autonomy stack can execute an approved mission within defined limits. The GCS remains supervisory and must retain emergency/operator interfaces supported by the approved operational concept.

## GPU command path

The uploaded firmware model includes a MAVLink-based GPU command path. The GCS shall expose:
- GPU connection
- message freshness
- autonomy health
- command authority
- command rejection
- current effective command/mode
- degraded state

The GCS shall not transform a stale GPU command into a current command.

## Control-link architecture

The firmware model contains LoRa, FHSS24 and RF433 arbitration. The GCS shall expose each link separately and expose the authoritative/active link reported by the vehicle.

The GCS should support link-independent command semantics so that changing the physical transport does not change the mission/vehicle application layer.

## Link-loss behavior

Exact vehicle behavior is a flight-controller requirement. The GCS must display the state and log the event. It must not invent an emergency maneuver merely because the UI lost its own network connection.

## Human-machine authority

The UI shall make these distinctions explicit:
- proposed command
- accepted command
- effective command
- operator command
- autonomous command
- failsafe command

This is essential for industrial and future high-assurance deployments.

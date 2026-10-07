# 08 — ROS 2 Architecture

## Role

ROS 2 is a robotics middleware/integration layer.

It is not the sole flight safety mechanism.

## Boundary

```text
AUGCS
  |
  +-- MAVLink path
  |
  +-- ROS2 bridge
        +-- perception
        +-- planning
        +-- AI
        +-- companion services
```

## Required interface documentation

For every ROS 2 interface:
- name
- message/service/action type
- QoS
- publisher/subscriber
- expected rate
- timeout
- stale-data behavior
- lifecycle
- security assumptions

## Failure policy

Loss of ROS 2 must have deterministic behavior and must not corrupt the core GCS command state.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

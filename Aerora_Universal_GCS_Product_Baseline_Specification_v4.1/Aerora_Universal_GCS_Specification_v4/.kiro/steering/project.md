---
inclusion: always
---
# Aerora Universal GCS — Project Rules

This repository is Aerora Dynamics' commercial GCS project based on a controlled QGroundControl baseline.

Read `docs/00_PROJECT_MASTER.md` and the relevant domain documentation before architectural work.

Priorities:
1. safety
2. requirements
3. correctness
4. security
5. maintainability
6. performance
7. convenience

Never make broad repository changes without an approved task/spec.

## Current product baseline

The first target vehicle is a dual-power hexacopter using tether + battery power, with manual, supervised semi-autonomous and fully autonomous modes. An onboard GPU/companion provides intelligence and communicates with the flight controller through MAVLink. Current firmware evidence includes LoRa, FHSS24 and RF433 communication arbitration and painting/payload interfaces.

Read `docs/46_PRODUCT_BASELINE_ANALYSIS.md` before making changes involving vehicle control, power, communications, AI or payloads.

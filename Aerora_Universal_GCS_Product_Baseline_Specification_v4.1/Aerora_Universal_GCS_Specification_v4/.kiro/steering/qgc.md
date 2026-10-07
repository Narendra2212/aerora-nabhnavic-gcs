---
inclusion: always
---
# QGroundControl Rules

Treat QGroundControl as an upstream-controlled foundation.

Prefer:
- custom build overlay
- QGCCorePlugin
- FirmwarePlugin
- AutoPilotPlugin
- documented extension mechanisms

Before changing upstream core:
1. identify the requirement
2. identify extension options
3. document why they are insufficient
4. assess upstream divergence
5. create regression coverage
6. obtain human review

Never delete or weaken existing safety functionality merely to simplify Aerora UI.

Aerora GCS must be designed as a universal platform. Do not hard-code painting-specific assumptions into generic QGC vehicle infrastructure. Use capability discovery and payload interfaces.

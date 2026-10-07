---
inclusion: always
---
# Safety Rules

Never invent vehicle failsafe behavior.

The flight controller remains responsible for appropriate local flight safety.

Every flight-critical feature requires:
- requirement
- design
- failure analysis as appropriate
- implementation
- verification evidence

AI/ROS2/payload modules must not bypass validated command boundaries.

The current uploaded firmware model contains explicit placeholders and pending items. Never convert a placeholder value into a claimed production safety limit without engineering evidence.

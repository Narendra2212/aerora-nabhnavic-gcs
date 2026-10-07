# 30 — Preliminary Hazard Analysis

This is a planning document, not a certification safety case.

## Candidate hazards

| ID | Hazard | Example cause | Required treatment |
|---|---|---|---|
| H-001 | unintended command | UI/state error | command validation + testing |
| H-002 | stale telemetry treated as current | communication delay | freshness indicators |
| H-003 | wrong vehicle selected | multi-vehicle/operator error | explicit identity confirmation |
| H-004 | wrong payload activated | capability/config error | capability checks + confirmation |
| H-005 | mission incompatible with vehicle | validation gap | pre-execution validation |
| H-006 | GCS crash | software fault | vehicle-local safety |
| H-007 | AI false result | model error | confidence/validation + bounded authority |
| H-008 | corrupted update | supply-chain/storage failure | signing/integrity/rollback |

Detailed hazard analysis must be performed against the actual vehicle and operational concept.

## Additional product hazards

- H-009: power-source transition causes control-system brownout
- H-010: tether-loss transition is not detected or handled correctly
- H-011: incorrect communication-link arbitration changes control authority
- H-012: stale GPU command is treated as current
- H-013: autonomous command is applied outside configured bounds
- H-014: payload reaction force or actuator fault affects vehicle stability
- H-015: GCS displays an incorrect active-link/authority state
- H-016: dual-sensor disagreement is not detected or is misclassified

These hazards require system-level analysis and test cases.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

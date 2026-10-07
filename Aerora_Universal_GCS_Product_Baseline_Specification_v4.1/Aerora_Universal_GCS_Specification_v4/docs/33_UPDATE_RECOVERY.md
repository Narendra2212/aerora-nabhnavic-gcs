# 33 — Update and Recovery

## Update requirements

- authenticated package where applicable
- integrity verification
- version compatibility check
- configuration compatibility check
- rollback capability
- clear operator feedback

## Rollback

Maintain a known-good version.

If an update fails:
1. preserve diagnostic evidence
2. prevent partial/inconsistent state
3. restore last known-good version where supported
4. report failure
5. log exact versions

## Configuration migration

Schema changes require explicit migration logic and backup where appropriate.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

# 43 — Kiro Specification Workflow

Kiro's current IDE supports Specs with requirements, design and tasks.

## Workflow

1. Identify requirement ID.
2. Create Spec.
3. Write user/system requirements.
4. Write design.
5. Review design.
6. Generate tasks.
7. Implement tasks one at a time.
8. Run tests.
9. Review diff.
10. Update traceability.
11. Close task.

## Rule

Do not use free-form chat to make architecture-changing edits when a Spec should exist.

## Spec naming

`<domain>-<short-purpose>`

Examples:
- `aerora-vehicle-plugin`
- `aerora-payload-framework`
- `aerora-mavlink-dialect`
- `aerora-paint-mission`

## Acceptance

Every task ends with explicit evidence.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

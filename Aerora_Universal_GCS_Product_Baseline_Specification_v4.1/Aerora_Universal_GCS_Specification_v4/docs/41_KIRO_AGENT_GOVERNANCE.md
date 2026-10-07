# 41 — Kiro Agent Governance

## Principle

Kiro is an engineering assistant, not the authority for architecture, safety, licensing or release decisions.

## Agent may

- inspect code
- explain architecture
- propose designs
- create tests
- implement approved tasks
- run permitted build/test commands
- update documentation tied to approved work

## Agent must not autonomously decide

- safety architecture
- release approval
- legal/license interpretation
- security exception
- deletion of critical safety mechanisms
- broad upstream refactoring
- production deployment

## Required context

Kiro should use:
- `docs/`
- `.kiro/steering/`
- active `.kiro/specs/`
- repository instructions

## Review

Every meaningful agent change must be inspected through Git diff and tests.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

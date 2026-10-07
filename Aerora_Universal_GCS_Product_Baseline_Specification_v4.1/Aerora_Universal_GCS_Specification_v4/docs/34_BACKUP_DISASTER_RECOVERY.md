# 34 — Backup and Disaster Recovery

## Protect

- source repositories
- specifications
- CI configurations
- signing metadata
- release artifacts
- test evidence
- SBOMs
- configuration schemas

## Recovery

Maintain documented recovery procedures for:
- developer workstation loss
- repository corruption
- CI outage
- release artifact loss
- compromised release
- accidental agent modification

## Principle

A production release must be reproducible independently of one developer workstation.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

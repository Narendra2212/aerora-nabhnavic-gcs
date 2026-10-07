# 19 — Release Management

## Stages

Development -> Alpha -> Beta -> Release Candidate -> Production -> LTS

## Release gates

- requirements complete
- test suite pass
- critical defects resolved/accepted
- security review
- license inventory
- configuration manifest
- artifact integrity
- release notes
- rollback package
- support documentation

## Versioning

Use semantic versioning unless an approved product-specific version scheme replaces it.

## Release artifact

A release must include:
- installer/package
- version manifest
- checksums
- release notes
- compatibility matrix
- third-party notices
- known issues


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

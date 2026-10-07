# 32 — Dependency and SBOM Process

## Inventory

Track direct and transitive dependencies.

## For each dependency

- name
- version
- source
- license
- hash where practical
- maintainer/source trust
- known vulnerabilities
- usage
- modification
- distribution obligations

## SBOM

Generate an SBOM for production releases using a standard machine-readable format selected by engineering/security.

## Update process

Dependency update -> compatibility test -> security review -> license review -> regression -> approval.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

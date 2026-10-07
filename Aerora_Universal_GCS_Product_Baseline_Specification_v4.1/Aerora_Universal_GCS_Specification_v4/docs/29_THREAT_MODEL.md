# 29 — Threat Model

## Assets

Vehicle control, missions, credentials, software, firmware, models, logs, customer data.

## Threat actors

- unauthorized user
- malicious local process
- compromised network peer
- compromised dependency
- malicious insider
- supply-chain attacker
- compromised companion computer

## Threat analysis

For each threat:
- asset
- entry point
- precondition
- impact
- likelihood
- controls
- residual risk
- test

## Security design principle

Treat all external data as untrusted until validated.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

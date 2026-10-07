# 14 — Security Architecture

## Assets

- vehicle commands
- mission files
- configuration
- credentials
- firmware
- GCS software
- AI models
- logs
- telemetry
- customer data

## Threat classes

- unauthorized operator
- malicious software
- compromised dependency
- credential theft
- command injection
- telemetry spoofing
- mission tampering
- malicious update
- log tampering
- compromised companion computer
- supply-chain compromise

## Controls

- least privilege
- authentication
- authorization
- secure secret storage
- signed artifacts
- integrity verification
- dependency scanning
- secret scanning
- audit logs
- secure update/rollback
- network isolation where required

## Security lifecycle

Threat model -> requirements -> implementation -> testing -> release review -> incident response.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

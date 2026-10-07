# 17 — License and Third-Party Governance

## QGroundControl

The current official QGC developer documentation states that QGC source is dual-licensed under Apache 2.0 and GPLv3. The QGC licensing guide must be consulted for the exact distribution model.

QGC's current contribution guidance states contributions must be compatible with its dual-license system and be original or from compatible licenses.

## Commercial product rule

Do not assume the QGC license automatically determines the license of every dependency.

Maintain a complete inventory.

## Inventory fields

- component
- version
- repository/source
- license
- copyright
- direct/transitive
- modification
- notice requirement
- source availability obligation
- commercial compatibility
- security status

## Legal review

Before commercial distribution, Aerora should obtain qualified legal review of:
- QGC licensing route
- Qt licensing
- all dependencies
- trademarks/branding
- redistribution notices
- customer contract/IP terms
- export/control obligations applicable to the actual product and destination

This document is not legal advice.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

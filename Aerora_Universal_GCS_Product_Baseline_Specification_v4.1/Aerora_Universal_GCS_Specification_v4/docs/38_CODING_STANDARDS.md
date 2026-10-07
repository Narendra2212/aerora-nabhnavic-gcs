# 38 — Coding Standards

## General

Follow QGC's existing coding conventions where modifying QGC-adjacent code unless Aerora has an approved reason to diverge.

## C++

- RAII
- clear ownership
- const correctness
- avoid unnecessary global state
- explicit interfaces
- bounded concurrency
- defensive external-input validation

## QML

- keep business logic out of views
- use models/services
- avoid duplicated state
- accessible labels for critical states
- no hidden command side effects

## Tests

New logic should include appropriate tests.

## Formatting

Do not reformat unrelated files.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

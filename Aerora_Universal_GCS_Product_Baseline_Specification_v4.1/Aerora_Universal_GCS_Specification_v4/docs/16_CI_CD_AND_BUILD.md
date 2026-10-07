# 16 — CI/CD and Build

## Build principles

- reproducible
- version-pinned
- traceable
- automated
- reviewable

## Pipeline

```text
Commit
 -> configure
 -> compile
 -> static checks
 -> unit tests
 -> integration tests
 -> simulation/regression
 -> security/license checks
 -> package
 -> sign
 -> release candidate
```

## Build artifacts

Record:
- source commit
- QGC baseline
- dependencies
- toolchain
- build options
- artifact hash
- signing information

## Local vs CI

Local builds are for development. CI is the release gate.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

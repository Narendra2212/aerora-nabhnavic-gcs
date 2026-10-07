# 42 — Kiro Hooks Plan

Kiro supports event-driven hooks including pre-tool-use, post-tool-use, file events and spec-task events.

## Planned hooks

### PreToolUse
Block or warn on:
- destructive file operations
- production deployment
- release tagging
- modifications to protected upstream modules without approved task
- changes to license files

### PostToolUse/FileSave
Optionally:
- run formatter/linter on changed files
- run targeted tests
- update generated metadata only when explicitly configured

### PreTaskExecution
Check:
- active spec exists
- task has acceptance criteria
- working tree state is understood

### PostTaskExecution
Generate:
- test summary
- changed-file summary
- requirement/verification reminder

Hooks must be introduced gradually and tested so they do not create unsafe automation loops.


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

# Kiro Hooks — Planned, Not Automatically Enabled

Kiro supports event-driven hooks. These files are planning guidance only.

Do not enable destructive/pre-tool hooks until their behavior has been tested on a disposable branch.

Recommended future gates:
- pre-task: verify active spec
- pre-tool: protect release/destructive operations
- post-task: run targeted verification
- file-save: optional lint/test for selected paths

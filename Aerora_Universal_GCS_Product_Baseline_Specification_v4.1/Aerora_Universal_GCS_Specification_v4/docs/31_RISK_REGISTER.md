# 31 — Engineering Risk Register

| ID | Risk | Impact | Likelihood | Mitigation | Owner | Status |
|---|---|---|---|---|---|---|
| R-001 | Excessive QGC fork divergence | High | Medium | extension-first architecture | TBD | Open |
| R-002 | License incompatibility | High | Medium | dependency inventory + legal review | TBD | Open |
| R-003 | AI introduces unsafe authority | High | Medium | explicit command boundary | TBD | Open |
| R-004 | Unverified payload behavior | High | Medium | payload interface + test gates | TBD | Open |
| R-005 | Protocol version mismatch | High | Medium | compatibility matrix | TBD | Open |
| R-006 | Kiro makes broad changes | High | Medium | steering/spec/hooks + review | TBD | Open |
| R-007 | Poor long-duration reliability | High | Medium | soak testing | TBD | Open |
| R-008 | Security dependency issue | High | Medium | SBOM/scanning | TBD | Open |

Risk ratings must be revisited with evidence.

## Product-specific open risks

| ID | Risk | Impact | Mitigation |
|---|---|---|---|
| R-009 | Tether/battery failover not fully specified | High | power architecture + transition testing |
| R-010 | GPU command authority unclear | High | formal authority state machine + MAVLink ICD |
| R-011 | Multiple radio arbitration not aligned between FC/GCS | High | authoritative FC state + protocol tests |
| R-012 | Firmware model contains placeholders | High | convert placeholders into requirements before production |
| R-013 | GCS compliance assumptions become stale | High | versioned DGCA compliance matrix |
| R-014 | Future defense requirements cause architecture divergence | Medium/High | security/assurance-ready interfaces now |


## Product baseline cross-reference

This document shall be interpreted against `docs/46_PRODUCT_BASELINE_ANALYSIS.md`, which records the uploaded FC schematic, PCB and Simulink evidence. Where the uploaded firmware model contains a `PLACEHOLDER`, `PENDING`, or `NOT YET INTEGRATED` statement, this specification treats the item as unresolved engineering work rather than a verified capability.

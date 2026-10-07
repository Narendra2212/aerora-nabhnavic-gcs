# Interface Traceability

Every interface defined in Phase 3, mapped to the requirements it serves, the risks and questions it
carries, its evidence status, its status, and its verification method.

This document is **mechanically extracted** from the interface control documents rather than
maintained by hand, and checked by script. A row here that disagrees with its defining document is a
defect in the extractor, not a second opinion.

## Scope of the mapping

| Mapped per interface | Mapped per domain |
| --- | --- |
| Requirements | Architecture document |
| Risks | Architecture decision record |
| Questions | Phase 2.1 evidence matrix item |
| Evidence status | — |
| Status | — |
| Verification method | — |

Architecture document, decision record and evidence-matrix item are mapped at **domain** granularity
because that is the granularity at which they were written. Claiming a per-interface decision record
where the decision was taken for a whole domain would be false precision.

## Summary

| Measure | Value |
| --- | --- |
| Interfaces defined | **240** |
| Domains | 16 |
| Defining documents | 16 |
| Interfaces defined in more than one document | **0** — checked |
| Interfaces referenced but never defined | **0** — checked |
| Interfaces absent from this table | **0** — checked |
| Interfaces with no verification method | **0** — checked |
| Interfaces with no evidence status | **0** — checked |

### Interfaces per domain

| Domain | Count | Defining document |
| --- | --- | --- |
| `SYS` | 8 | [01-gcs-system-icd.md](01-gcs-system-icd.md) |
| `FC` | 24 | [02-fc-gcs-icd.md](02-fc-gcs-icd.md) |
| `MAV` | 28 | [03-mavlink-icd.md](03-mavlink-icd.md) |
| `RAD` | 9 | [04-radio-link-icd.md](04-radio-link-icd.md) |
| `GPU` | 8 | [05-gpu-fc-icd.md](05-gpu-fc-icd.md) |
| `PAY` | 15 | [06-payload-icd.md](06-payload-icd.md) |
| `PWR` | 12 | [07-power-telemetry-icd.md](07-power-telemetry-icd.md) |
| `VID` | 7 | [08-vehicle-identity-icd.md](08-vehicle-identity-icd.md) |
| `AUT` | 12 | [09-command-authority-icd.md](09-command-authority-icd.md) |
| `MIS` | 16 | [10-mission-icd.md](10-mission-icd.md) |
| `TLM` | 13 | [11-telemetry-icd.md](11-telemetry-icd.md) |
| `PAR` | 10 | [12-parameter-icd.md](12-parameter-icd.md) |
| `SEC` | 21 | [13-security-icd.md](13-security-icd.md) |
| `LOG` | 24 | [14-audit-logging-icd.md](14-audit-logging-icd.md) |
| `CLD` | 16 | [15-cloud-integration-icd.md](15-cloud-integration-icd.md) |
| `UIS` | 17 | [16-ui-state-icd.md](16-ui-state-icd.md) |
| Total | **240** | — |

[17-capability-model.md](17-capability-model.md) and
[18-error-degradation-model.md](18-error-degradation-model.md) define **no** interface identifiers.
Both are models over interfaces defined elsewhere, and both say so in their own text.

### Status distribution

| Status | Count | Share |
| --- | --- | --- |
| `PROPOSED` | 102 | 42% |
| `CONFIRMED` | 82 | 34% |
| `OPEN` | 56 | 23% |

Fifty-six `OPEN` interfaces is the honest headline of Phase 3. Nearly a quarter of the interface set
cannot be implemented as specified because something it depends on — a dialect, a firmware decision,
a product decision — does not exist yet. The alternative was to write them as `PROPOSED` and let the
dependency disappear.

### Evidence distribution

Counts exceed 240 because some rows carry compound evidence, such as a verified transport with an
unevidenced payload.

| Evidence status | Count |
| --- | --- |
| `GCS-VERIFIED` | 89 |
| `BASELINE-ASSERTED` | 47 |
| `GCS-DECIDED` | 46 |
| `NOT-EVIDENCED` | 37 |
| `MODEL` | 17 |
| `PLACEHOLDER` | 7 |
| `ARTIFACT-OBSERVED` | 5 |

`GCS-VERIFIED` leads because Phase 1 read the ground-station source exhaustively. The five
`ARTIFACT-OBSERVED` rows are the whole hardware contribution to the interface set, which is a fair
measure of how much of this specification rests on the aircraft being built rather than observed.

### Verification distribution

| Method | Count |
| --- | --- |
| `INTEGRATION` | 145 |
| `UNIT` | 36 |
| `BENCH` | 26 |
| `STATIC` | 18 |
| `DEMO` | 6 |
| `AUDIT` | 6 |
| `HIL` | 4 |

`INTEGRATION` dominates because the mock link can exercise most of the ground station's side of
every contract. The 30 `BENCH` and `HIL` assignments are the ones that need an aircraft, and they
are concentrated exactly where the evidence is weakest. Counts total 241 because `IF-GPU-008` carries
two methods.

**No result is recorded in this document.** Methods are assigned in
[22-phase-3-verification-plan.md](22-phase-3-verification-plan.md) and nothing has been run.

## Domain to architecture, decision record and evidence item

| Domain | Phase 2 architecture document | Decision records | Evidence matrix items |
| --- | --- | --- | --- |
| `SYS` | [system-architecture.md](../01-aerora-architecture/system-architecture.md), [component-architecture.md](../01-aerora-architecture/component-architecture.md) | ADR-001 | — |
| `FC` | [vehicle-integration.md](../01-aerora-architecture/vehicle-integration.md) | ADR-001, ADR-004 | 1, 2, 12 |
| `MAV` | [mavlink-architecture.md](../01-aerora-architecture/mavlink-architecture.md) | ADR-004 | 10, 11 |
| `RAD` | [communication-architecture.md](../01-aerora-architecture/communication-architecture.md) | ADR-002, ADR-003 | 6, 7, 8 |
| `GPU` | [gpu-integration.md](../01-aerora-architecture/gpu-integration.md) | ADR-011 | 9, 21 |
| `PAY` | [payload-architecture.md](../01-aerora-architecture/payload-architecture.md) | ADR-005 | 13, 14, 15, 16 |
| `PWR` | [system-architecture.md](../01-aerora-architecture/system-architecture.md) | — | 17, 18 |
| `VID` | [data-model.md](../01-aerora-architecture/data-model.md) | — | 22 |
| `AUT` | [authority-model.md](../01-aerora-architecture/authority-model.md) | ADR-010, ADR-011 | 19, 20 |
| `MIS` | [mission-architecture.md](../01-aerora-architecture/mission-architecture.md) | ADR-005 | 14 |
| `TLM` | [data-model.md](../01-aerora-architecture/data-model.md), [vehicle-integration.md](../01-aerora-architecture/vehicle-integration.md) | ADR-001 | 12 |
| `PAR` | [vehicle-integration.md](../01-aerora-architecture/vehicle-integration.md), [configuration-architecture.md](../01-aerora-architecture/configuration-architecture.md) | ADR-001 | — |
| `SEC` | [security-architecture.md](../01-aerora-architecture/security-architecture.md) | ADR-008 | — |
| `LOG` | [logging-architecture.md](../01-aerora-architecture/logging-architecture.md) | ADR-006 | — |
| `CLD` | [cloud-architecture.md](../01-aerora-architecture/cloud-architecture.md) | ADR-006, ADR-007 | — |
| `UIS` | [ui-architecture.md](../01-aerora-architecture/ui-architecture.md) | ADR-001 | — |

Five domains map to no evidence-matrix item, and all five are ground-station-internal: security,
logging, cloud, interface state and parameters. That is the expected shape — the evidence matrix
records vehicle-side unknowns, so a domain with no entry is a domain Aerora controls.

ADR-009 (Qt licensing neutrality) maps to no domain. It constrains the whole delivery model rather
than any interface, and it is carried in [23-build-readiness.md](23-build-readiness.md).

## The interface table

Columns: identifier, name, requirements, risks, questions, evidence status, status, verification
method.

| ID | Interface | Requirements | Risks | Questions | Evidence | Status | Verification |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-AUT-001` | Effective authority mode | `AERORA-SAFE-REQ-006` | R-028 | Q-007 | `MODEL` | `PROPOSED` | `INTEGRATION` |
| `IF-AUT-002` | Authority mode change request | `AERORA-COMPLY-REQ-003`, `AERORA-SAFE-REQ-006` | R-028 | Q-007 | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-AUT-003` | Authority source | `AERORA-GCS-REQ-029` | — | Q-007 | `MODEL` | `PROPOSED` | `INTEGRATION` |
| `IF-AUT-004` | Confirmation pending | `AERORA-SAFE-REQ-007` | — | Q-007 | `MODEL` | `PROPOSED` | `INTEGRATION` |
| `IF-AUT-005` | Operator confirmation | `AERORA-SAFE-REQ-007` | — | Q-007 | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-AUT-006` | Command submission | `AERORA-GCS-REQ-019`, `AERORA-SEC-REQ-012` | — | Q-029 | `GCS-VERIFIED` | `CONFIRMED` | `UNIT` |
| `IF-AUT-007` | Command acknowledgement | `AERORA-GCS-REQ-015` | R-025 | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-AUT-008` | Command state | `AERORA-COMPLY-REQ-002`, `AERORA-GCS-REQ-015`, `AERORA-GCS-REQ-018` | R-025 | — | `GCS-DECIDED` | `PROPOSED` | `UNIT` |
| `IF-AUT-009` | Effectiveness corroboration | `AERORA-GCS-REQ-016`, `AERORA-GCS-REQ-017` | — | Q-036 | `GCS-DECIDED` | `PROPOSED` | `UNIT` |
| `IF-AUT-010` | Failsafe state | `AERORA-SAFE-REQ-008`, `AERORA-SAFE-REQ-009`, `AERORA-SAFE-REQ-010` | R-006 | Q-007 | `PLACEHOLDER` | `OPEN` | `INTEGRATION` |
| `IF-AUT-011` | Operator authorisation decision | `AERORA-SEC-REQ-015` | R-004 | Q-029 | `GCS-DECIDED` | `PROPOSED` | `UNIT` |
| `IF-AUT-012` | GCS-initiated failsafe action | `AERORA-SAFE-REQ-008` | — | Q-007 | `GCS-DECIDED` | `CONFIRMED` | `STATIC` |
| `IF-CLD-001` | Cloud connectivity state | `AERORA-AWS-REQ-008` | — | — | `BASELINE-ASSERTED` | `CONFIRMED` | `DEMO` |
| `IF-CLD-002` | Cloud threading boundary | `AERORA-AWS-REQ-015` | — | — | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-CLD-003` | Inbound network connection | `AERORA-AWS-REQ-014`, `AERORA-TEST-REQ-004`, `AERORA-TEST-REQ-007` | R-016 | Q-027 | `GCS-VERIFIED` | `PROPOSED` | `UNIT` |
| `IF-CLD-004` | Cloud participation in the command path | `AERORA-AWS-REQ-005`, `AERORA-AWS-REQ-006`, `AERORA-TEST-REQ-005` | — | Q-030 | `GCS-DECIDED` | `CONFIRMED` | `INTEGRATION` |
| `IF-CLD-005` | Mission library synchronisation | `AERORA-AWS-REQ-006`, `AERORA-AWS-REQ-007` | — | Q-042 | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-CLD-006` | Fleet view data | `AERORA-AWS-REQ-009` | R-017 | Q-016 | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-CLD-007` | Configuration distribution | `AERORA-AWS-REQ-007`, `AERORA-BUILD-REQ-010` | — | Q-042 | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-CLD-008` | Release and compatibility metadata | `AERORA-AWS-REQ-007`, `AERORA-BUILD-REQ-010` | — | — | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-CLD-009` | Operator notification | `AERORA-AWS-REQ-013` | — | — | `BASELINE-ASSERTED` | `PROPOSED` | `STATIC` |
| `IF-CLD-010` | Reporting and dashboards | `AERORA-AWS-REQ-007` | — | — | `BASELINE-ASSERTED` | `CONFIRMED` | `STATIC` |
| `IF-CLD-011` | Map and terrain tile cache | `AERORA-AWS-REQ-006` | — | — | `GCS-VERIFIED` | `PROPOSED` | `DEMO` |
| `IF-CLD-012` | Cloud-sourced data age | `AERORA-AWS-REQ-009` | — | — | `BASELINE-ASSERTED` | `CONFIRMED` | `UNIT` |
| `IF-CLD-013` | Per-feature availability state | `AERORA-AWS-REQ-007` | — | Q-030 | `BASELINE-ASSERTED` | `PROPOSED` | `STATIC` |
| `IF-CLD-014` | Upload queue state | `AERORA-AWS-REQ-010` | — | Q-039 | `GCS-DECIDED` | `PROPOSED` | `UNIT` |
| `IF-CLD-015` | Synchronisation conflict resolution | `AERORA-AWS-REQ-007`, `AERORA-BUILD-REQ-012` | — | Q-042 | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-CLD-016` | Backend module interface boundary | `AERORA-AWS-REQ-007` | — | — | `BASELINE-ASSERTED` | `CONFIRMED` | `STATIC` |
| `IF-FC-001` | Vehicle state: attitude, position, velocity, altitude | `AERORA-GCS-REQ-027` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-FC-002` | Flight mode | `AERORA-GCS-REQ-011` | R-028 | Q-007 | `GCS-VERIFIED`, `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-FC-003` | Authority mode, effective | `AERORA-SAFE-REQ-006`, `AERORA-SAFE-REQ-007` | R-028 | Q-007 | `MODEL` | `PROPOSED` | `INTEGRATION` |
| `IF-FC-004` | Arming state | — | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-FC-005` | System health and sensor status | `AERORA-GCS-REQ-029` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-FC-006` | Battery and power telemetry | `AERORA-POWER-REQ-007`, `AERORA-POWER-REQ-008` | — | Q-024, Q-025, Q-026 | `ARTIFACT-OBSERVED`, `NOT-EVIDENCED` | `PROPOSED` | `BENCH` |
| `IF-FC-007` | Link state, vehicle-reported | `AERORA-COMM-REQ-007`, `AERORA-COMM-REQ-008`, `AERORA-COMM-REQ-009` | R-003 | Q-013, Q-015 | `MODEL` | `PROPOSED` | `BENCH` |
| `IF-FC-008` | GPS state | — | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-FC-009` | Estimator state | — | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-FC-010` | Autonomy state and proposals | `AERORA-GPU-REQ-005`, `AERORA-GPU-REQ-006` | R-014 | Q-008 | `MODEL` | `OPEN` | `HIL` |
| `IF-FC-011` | Companion health and presence | `AERORA-GPU-REQ-001`, `AERORA-GPU-REQ-007` | R-014 | Q-008 | `ARTIFACT-OBSERVED`, `NOT-EVIDENCED` | `PROPOSED` | `HIL` |
| `IF-FC-012` | Payload state, health, safety state | `AERORA-PAYLOAD-REQ-006`, `AERORA-PAYLOAD-REQ-008` | R-002 | Q-009, Q-023 | `BASELINE-ASSERTED`, `PLACEHOLDER` | `OPEN` | `BENCH` |
| `IF-FC-013` | Command acknowledgement | `AERORA-GCS-REQ-015`, `AERORA-GCS-REQ-018` | R-025 | Q-036 | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-FC-014` | Mission state and current index | `AERORA-MISSION-REQ-006` | R-002 | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-FC-015` | Parameters | `AERORA-GCS-REQ-012`, `AERORA-GCS-REQ-026` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-FC-016` | Diagnostics and status text | `AERORA-GCS-REQ-029` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-FC-017` | Capability inventory | `AERORA-GCS-REQ-013`, `AERORA-MAV-REQ-009` | — | — | `NOT-EVIDENCED` | `OPEN` | `BENCH` |
| `IF-FC-018` | Aerora vehicle identity | `AERORA-VEH-REQ-002`, `AERORA-VEH-REQ-003`, `AERORA-VEH-REQ-005` | R-017 | Q-016 | `ARTIFACT-OBSERVED`, `NOT-EVIDENCED` | `OPEN` | `BENCH` |
| `IF-FC-019` | Operator flight commands | `AERORA-GCS-REQ-015`, `AERORA-GCS-REQ-016`, `AERORA-GCS-REQ-017` | R-025 | Q-036 | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-FC-020` | Authority mode change request | `AERORA-COMPLY-REQ-003`, `AERORA-SAFE-REQ-006` | R-028 | Q-007 | `MODEL` | `PROPOSED` | `HIL` |
| `IF-FC-021` | Payload commands | `AERORA-PAYLOAD-REQ-010`, `AERORA-PAYLOAD-REQ-011` | R-002 | Q-023 | `BASELINE-ASSERTED`, `PLACEHOLDER` | `OPEN` | `BENCH` |
| `IF-FC-022` | Mission transfer | `AERORA-MISSION-REQ-001`, `AERORA-MISSION-REQ-007` | R-002, R-012 | Q-041 | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-FC-023` | Parameter write | `AERORA-BUILD-REQ-011` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-FC-024` | Ground station heartbeat | — | — | — | `GCS-VERIFIED` | `CONFIRMED` | `UNIT` |
| `IF-GPU-001` | Companion addressing | `AERORA-GPU-REQ-001`, `AERORA-GPU-REQ-002` | R-014 | — | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-GPU-002` | Proposal | `AERORA-GPU-REQ-004`, `AERORA-GPU-REQ-006` | R-014 | Q-008 | `MODEL` | `OPEN` | `INTEGRATION` |
| `IF-GPU-003` | Proposal outcome | `AERORA-GPU-REQ-005` | R-014 | Q-008 | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-GPU-004` | Companion health | `AERORA-GPU-REQ-006` | — | — | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-GPU-005` | Companion presence | `AERORA-GPU-REQ-007` | — | — | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-GPU-006` | Effective state for corroboration | `AERORA-SAFE-REQ-006` | R-028 | Q-007 | `MODEL` | `PROPOSED` | `INTEGRATION` |
| `IF-GPU-007` | Autonomy capability | `AERORA-MAV-REQ-009` | — | — | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-GPU-008` | Ground station to companion | `AERORA-GPU-REQ-003`, `AERORA-GPU-REQ-008`, `AERORA-MAV-REQ-007` | R-014 | — | `GCS-DECIDED` | `CONFIRMED` | `STATIC`, `UNIT` |
| `IF-LOG-001` | Application diagnostic log | `AERORA-GCS-REQ-020` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `STATIC` |
| `IF-LOG-002` | MAVLink stream log | `AERORA-GCS-REQ-020` | R-022 | Q-039 | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-LOG-003` | Audit record store | `AERORA-GCS-REQ-021`, `AERORA-SEC-REQ-019` | — | Q-037 | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-LOG-004` | Operator action record | `AERORA-COMPLY-REQ-002` | — | Q-029 | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-LOG-005` | Command lifecycle record | `AERORA-COMPLY-REQ-002`, `AERORA-GCS-REQ-015`, `AERORA-GCS-REQ-018` | R-025 | Q-036 | `GCS-DECIDED` | `PROPOSED` | `UNIT` |
| `IF-LOG-006` | Authority mode request record | `AERORA-COMPLY-REQ-003` | R-028 | Q-007 | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-LOG-007` | Effective authority change record | `AERORA-COMPLY-REQ-003`, `AERORA-SAFE-REQ-006` | R-028 | Q-007 | `MODEL` | `PROPOSED` | `INTEGRATION` |
| `IF-LOG-008` | Confirmation record | `AERORA-SAFE-REQ-007` | — | Q-007 | `MODEL` | `PROPOSED` | `INTEGRATION` |
| `IF-LOG-009` | Security event record | `AERORA-SEC-REQ-015`, `AERORA-SEC-REQ-019` | R-004 | Q-029 | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-LOG-010` | Configuration change record | `AERORA-BUILD-REQ-010`, `AERORA-COMPLY-REQ-002` | — | — | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-LOG-011` | Mission event record | `AERORA-COMPLY-REQ-002`, `AERORA-MISSION-REQ-007`, `AERORA-MISSION-REQ-008` | R-012 | Q-041 | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-LOG-012` | Failsafe event record | `AERORA-SAFE-REQ-009`, `AERORA-SAFE-REQ-010` | R-006 | Q-007 | `PLACEHOLDER` | `OPEN` | `INTEGRATION` |
| `IF-LOG-013` | Value provenance in records | `AERORA-COMPLY-REQ-004`, `AERORA-GCS-REQ-027` | — | — | `GCS-DECIDED` | `PROPOSED` | `UNIT` |
| `IF-LOG-014` | Record writer queue | `AERORA-GCS-REQ-025` | — | — | `GCS-DECIDED` | `PROPOSED` | `UNIT` |
| `IF-LOG-015` | Drop accounting | `AERORA-GCS-REQ-025` | — | — | `GCS-DECIDED` | `PROPOSED` | `UNIT` |
| `IF-LOG-016` | Local-first write ordering | `AERORA-AWS-REQ-011`, `AERORA-SEC-REQ-019` | — | — | `GCS-DECIDED` | `CONFIRMED` | `INTEGRATION` |
| `IF-LOG-017` | Upload submission | `AERORA-AWS-REQ-010`, `AERORA-AWS-REQ-011` | — | Q-030, Q-042 | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-LOG-018` | Upload to a non-Aerora endpoint | `AERORA-GCS-REQ-023`, `AERORA-TEST-REQ-005` | R-022 | Q-039 | `GCS-VERIFIED` | `PROPOSED` | `INTEGRATION` |
| `IF-LOG-019` | Local record deletion on upload | `AERORA-COMPLY-REQ-005`, `AERORA-GCS-REQ-024` | R-022 | Q-039 | `GCS-VERIFIED` | `PROPOSED` | `STATIC` |
| `IF-LOG-020` | Audit store integrity protection | `AERORA-GCS-REQ-028` | — | Q-037 | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-LOG-021` | Record timestamp basis | `AERORA-COMPLY-REQ-004` | — | Q3-008 | `GCS-DECIDED` | `PROPOSED` | `UNIT` |
| `IF-LOG-022` | Correlation identity | `AERORA-GCS-REQ-018`, `AERORA-VEH-REQ-005` | R-017, R-025 | Q-016 | `GCS-DECIDED` | `PROPOSED` | `UNIT` |
| `IF-LOG-023` | Retention deletion record | `AERORA-COMPLY-REQ-005` | — | Q-039 | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-LOG-024` | Record export | `AERORA-COMPLY-REQ-005` | — | Q-039 | `GCS-VERIFIED` | `PROPOSED` | `DEMO` |
| `IF-MAV-001` | Heartbeat, base and custom mode | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-029` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MAV-002` | Attitude, position, velocity, altitude | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-029` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MAV-003` | System and sensor health | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-029` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MAV-004` | Battery state | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-029` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MAV-005` | GPS and estimator state | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-029` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MAV-006` | Command send and acknowledgement | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-029` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MAV-007` | Mission transfer and progress | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-029` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MAV-008` | Parameter protocol | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-029` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MAV-009` | Status text and events | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-029` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MAV-010` | Radio status | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-029` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MAV-011` | Message signing | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-029` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MAV-012` | Aerora system status | `AERORA-MAV-REQ-005`, `AERORA-MAV-REQ-006`, `AERORA-MAV-REQ-007` | R-011 | Q-003 | `BASELINE-ASSERTED` | `PROPOSED` | `BENCH` |
| `IF-MAV-013` | Aerora vehicle capabilities | `AERORA-MAV-REQ-005`, `AERORA-MAV-REQ-006`, `AERORA-MAV-REQ-007` | R-011 | Q-003 | `BASELINE-ASSERTED` | `PROPOSED` | `BENCH` |
| `IF-MAV-014` | Aerora payload status | `AERORA-MAV-REQ-005`, `AERORA-MAV-REQ-006`, `AERORA-MAV-REQ-007` | R-011 | Q-003 | `BASELINE-ASSERTED` | `PROPOSED` | `BENCH` |
| `IF-MAV-015` | Aerora paint status | `AERORA-MAV-REQ-005`, `AERORA-MAV-REQ-006`, `AERORA-MAV-REQ-007` | R-011 | Q-003 | `BASELINE-ASSERTED` | `PROPOSED` | `BENCH` |
| `IF-MAV-016` | Aerora pump status | `AERORA-MAV-REQ-005`, `AERORA-MAV-REQ-006`, `AERORA-MAV-REQ-007` | R-011 | Q-003 | `BASELINE-ASSERTED` | `PROPOSED` | `BENCH` |
| `IF-MAV-017` | Aerora gimbal status | `AERORA-MAV-REQ-005`, `AERORA-MAV-REQ-006`, `AERORA-MAV-REQ-007` | R-011 | Q-003 | `BASELINE-ASSERTED` | `PROPOSED` | `BENCH` |
| `IF-MAV-018` | Aerora autonomy status | `AERORA-MAV-REQ-005`, `AERORA-MAV-REQ-006`, `AERORA-MAV-REQ-007` | R-011 | Q-003 | `BASELINE-ASSERTED` | `PROPOSED` | `BENCH` |
| `IF-MAV-019` | Aerora mission status | `AERORA-MAV-REQ-005`, `AERORA-MAV-REQ-006`, `AERORA-MAV-REQ-007` | R-011 | Q-003 | `BASELINE-ASSERTED` | `PROPOSED` | `BENCH` |
| `IF-MAV-020` | Aerora diagnostics | `AERORA-MAV-REQ-005`, `AERORA-MAV-REQ-006`, `AERORA-MAV-REQ-007` | R-011 | Q-003 | `BASELINE-ASSERTED` | `PROPOSED` | `BENCH` |
| `IF-MAV-021` | Reporting the Aerora autopilot identifier in the heartbeat | `AERORA-GCS-REQ-007`, `AERORA-GCS-REQ-008` | R-010 | Q-018 | `NOT-EVIDENCED` | `OPEN` | `BENCH` |
| `IF-MAV-022` | Reporting which component originated a relayed value | `AERORA-COMPLY-REQ-004`, `AERORA-GCS-REQ-027` | — | Q3-004 | `NOT-EVIDENCED` | `OPEN` | `BENCH` |
| `IF-MAV-023` | Reporting proposal acceptance or rejection explicitly | `AERORA-GPU-REQ-005` | R-014 | Q-008 | `NOT-EVIDENCED` | `OPEN` | `HIL` |
| `IF-MAV-024` | A per-command request identifier for audit correlation | `AERORA-COMPLY-REQ-002`, `AERORA-GCS-REQ-018` | R-025 | Q3-002 | `NOT-EVIDENCED` | `OPEN` | `BENCH` |
| `IF-MAV-025` | Declaring the resume behaviour of a state-setting command | `AERORA-MAV-REQ-008` | R-002 | Q-023 | `NOT-EVIDENCED` | `OPEN` | `BENCH` |
| `IF-MAV-026` | The vehicle reports a capability inventory on connection and on cha... | `AERORA-GCS-REQ-013`, `AERORA-MAV-REQ-009`, `AERORA-PAYLOAD-REQ-013` | — | Q-023 | `NOT-EVIDENCED` | `OPEN` | `BENCH` |
| `IF-MAV-027` | Inbound message adjustment seam | `AERORA-MAV-REQ-010` | R-002 | — | `GCS-VERIFIED` | `CONFIRMED` | `UNIT` |
| `IF-MAV-028` | Core-plugin message seam | `AERORA-MAV-REQ-010` | R-002 | — | `GCS-VERIFIED` | `CONFIRMED` | `UNIT` |
| `IF-MIS-001` | Mission upload | `AERORA-MISSION-REQ-007` | R-012 | Q-041 | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MIS-002` | Mission download | `AERORA-MISSION-REQ-002` | R-012 | Q-022 | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MIS-003` | Mission current index | `AERORA-MISSION-REQ-006` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MIS-004` | Mission item reached | `AERORA-GCS-REQ-029`, `AERORA-MISSION-REQ-006` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MIS-005` | Mission replacement | `AERORA-MISSION-REQ-001` | R-012 | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MIS-006` | Mission resume | `AERORA-MISSION-REQ-003`, `AERORA-MISSION-REQ-004` | R-002 | Q-019 | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-MIS-007` | Mission pause | `AERORA-GCS-REQ-011` | R-028 | Q-007 | `GCS-VERIFIED` | `OPEN` | `INTEGRATION` |
| `IF-MIS-008` | Plan file | `AERORA-MISSION-REQ-001` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `UNIT` |
| `IF-MIS-009` | Plan version and identity | `AERORA-GCS-REQ-014`, `AERORA-VEH-REQ-005` | R-017 | Q-016 | `GCS-DECIDED` | `PROPOSED` | `UNIT` |
| `IF-MIS-010` | Mission verification | `AERORA-MISSION-REQ-007` | — | Q-041 | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-MIS-011` | Aerora complex item | `AERORA-MISSION-REQ-002` | R-018 | Q-021 | `GCS-VERIFIED` | `OPEN` | `UNIT` |
| `IF-MIS-012` | Aerora mission command set | `AERORA-MISSION-REQ-005` | — | Q-023 | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-MIS-013` | Aerora mission status | `AERORA-MAV-REQ-009` | — | — | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-MIS-014` | Payload state interaction on resume | `AERORA-MISSION-REQ-003`, `AERORA-PAYLOAD-REQ-007` | R-002 | Q-019 | `NOT-EVIDENCED` | `OPEN` | `BENCH` |
| `IF-MIS-015` | GCS-side payload state replay | `AERORA-MISSION-REQ-003` | R-002 | — | `GCS-DECIDED` | `CONFIRMED` | `STATIC` |
| `IF-MIS-016` | QGC resume filter modification | — | R-002 | — | `GCS-DECIDED` | `CONFIRMED` | `STATIC` |
| `IF-PAR-001` | Parameter discovery | `AERORA-GCS-REQ-012` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-PAR-002` | Parameter download | `AERORA-GCS-REQ-026` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-PAR-003` | Parameter write | `AERORA-BUILD-REQ-011` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-PAR-004` | Parameter change notification | `AERORA-GCS-REQ-029` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-PAR-005` | Parameter metadata | `AERORA-GCS-REQ-026` | — | — | `GCS-VERIFIED` | `PROPOSED` | `UNIT` |
| `IF-PAR-006` | Parameter validation | `AERORA-GCS-REQ-026` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `UNIT` |
| `IF-PAR-007` | Parameter cache | — | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-PAR-008` | Parameter synchronisation state | `AERORA-GCS-REQ-013` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-PAR-009` | Payload configuration as parameters | `AERORA-PAYLOAD-REQ-006` | — | Q-023, Q3-006 | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-PAR-010` | Parameter reset to defaults | `AERORA-COMPLY-REQ-002` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-PAY-001` | Payload identity and version | `AERORA-PAYLOAD-REQ-006` | — | — | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-PAY-002` | Payload capability set | `AERORA-PAYLOAD-REQ-013` | — | Q-023 | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-PAY-003` | Payload state | `AERORA-GCS-REQ-029`, `AERORA-PAYLOAD-REQ-006` | R-002 | Q-023 | `PLACEHOLDER` | `OPEN` | `INTEGRATION` |
| `IF-PAY-004` | Payload health | `AERORA-PAYLOAD-REQ-006` | — | — | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-PAY-005` | Payload safety state | `AERORA-PAYLOAD-REQ-010` | — | Q-009 | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-PAY-006` | Payload emergency stop state | `AERORA-PAYLOAD-REQ-008`, `AERORA-SAFE-REQ-011` | — | Q-009 | `MODEL` | `PROPOSED` | `INTEGRATION` |
| `IF-PAY-007` | Payload telemetry | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-027` | — | Q-023 | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-PAY-008` | Payload configuration | `AERORA-BUILD-REQ-011` | — | Q-023 | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-PAY-009` | Payload command | `AERORA-GCS-REQ-016`, `AERORA-PAYLOAD-REQ-011` | R-002 | Q-023 | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-PAY-010` | Operator emergency stop request | `AERORA-PAYLOAD-REQ-009` | — | Q-009 | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-PAY-011` | Payload calibration | `AERORA-PAYLOAD-REQ-013` | — | Q-043 | `BASELINE-ASSERTED` | `OPEN` | `BENCH` |
| `IF-PAY-012` | Payload diagnostics | `AERORA-GCS-REQ-029` | — | Q-043 | `BASELINE-ASSERTED` | `OPEN` | `INTEGRATION` |
| `IF-PAY-013` | Payload actuator interface | `AERORA-PAYLOAD-REQ-010` | — | Q-023 | `PLACEHOLDER` | `OPEN` | `STATIC` |
| `IF-PAY-014` | Payload state retention across interruption | `AERORA-MISSION-REQ-003`, `AERORA-PAYLOAD-REQ-007` | R-002 | Q-023 | `NOT-EVIDENCED` | `OPEN` | `BENCH` |
| `IF-PAY-015` | Ground station to payload actuators | `AERORA-PAYLOAD-REQ-010` | — | — | `GCS-DECIDED` | `CONFIRMED` | `STATIC` |
| `IF-PWR-001` | Tether connected or disconnected | `AERORA-GCS-REQ-029` | — | Q-025 | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-PWR-002` | Battery present or absent | `AERORA-GCS-REQ-029` | — | Q-025 | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-PWR-003` | Active source | `AERORA-GCS-REQ-029` | — | Q-024, Q-025 | `BASELINE-ASSERTED` | `OPEN` | `INTEGRATION` |
| `IF-PWR-004` | Source transition | `AERORA-POWER-REQ-008` | — | Q-026 | `BASELINE-ASSERTED` | `OPEN` | `INTEGRATION` |
| `IF-PWR-005` | Battery state of charge | — | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-PWR-006` | Voltage, current, power | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-027` | — | Q-024 | `ARTIFACT-OBSERVED` | `PROPOSED` | `INTEGRATION` |
| `IF-PWR-007` | Power-system fault | `AERORA-SAFE-REQ-009` | — | Q-024 | `BASELINE-ASSERTED` | `OPEN` | `INTEGRATION` |
| `IF-PWR-008` | Estimated remaining energy, where validated | `AERORA-GCS-REQ-029` | — | Q-024 | `BASELINE-ASSERTED` | `OPEN` | `INTEGRATION` |
| `IF-PWR-009` | Degraded or limited-power mode | `AERORA-GCS-REQ-029` | — | Q-024 | `BASELINE-ASSERTED` | `OPEN` | `INTEGRATION` |
| `IF-PWR-010` | Converter state | `AERORA-GCS-REQ-029` | — | Q-024 | `ARTIFACT-OBSERVED`, `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-PWR-011` | Watchdog fault | `AERORA-POWER-REQ-007`, `AERORA-SAFE-REQ-011` | — | Q3-004 | `MODEL` | `PROPOSED` | `INTEGRATION` |
| `IF-PWR-012` | Power capability | `AERORA-MAV-REQ-009` | — | Q-024 | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-RAD-001` | Radio host presentation | `AERORA-COMM-REQ-006` | R-003 | Q3-001 | `NOT-EVIDENCED` | `OPEN` | `BENCH` |
| `IF-RAD-002` | Serial port configuration | `AERORA-COMM-REQ-006` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-RAD-003` | USB device identity | `AERORA-COMM-REQ-011`, `AERORA-COMM-REQ-012` | R-003 | Q3-001 | `NOT-EVIDENCED` | `OPEN` | `BENCH` |
| `IF-RAD-004` | Byte stream | — | — | — | `GCS-VERIFIED` | `CONFIRMED` | `UNIT` |
| `IF-RAD-005` | Link lifecycle | `AERORA-COMM-REQ-010` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-RAD-006` | Auto-connect decision | `AERORA-COMM-REQ-012` | R-003 | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-RAD-007` | GCS-local link observation | `AERORA-COMM-REQ-007`, `AERORA-COMM-REQ-008` | — | Q-015 | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-RAD-008` | Vehicle-reported link state | `AERORA-COMM-REQ-009` | — | Q-013 | `MODEL` | `PROPOSED` | `INTEGRATION` |
| `IF-RAD-009` | Reconnection | `AERORA-VEH-REQ-003` | R-017 | Q-016 | `GCS-VERIFIED` | `PROPOSED` | `INTEGRATION` |
| `IF-SEC-001` | Operator sign-in | `AERORA-SEC-REQ-011` | R-004 | Q-029 | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-SEC-002` | Local credential validation | `AERORA-AWS-REQ-006`, `AERORA-SEC-REQ-011` | R-004 | — | `BASELINE-ASSERTED` | `CONFIRMED` | `INTEGRATION` |
| `IF-SEC-003` | Role resolution | `AERORA-SEC-REQ-013` | R-004 | Q-029 | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-SEC-004` | Authorisation decision | `AERORA-GCS-REQ-019`, `AERORA-SEC-REQ-012` | R-004 | — | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-SEC-005` | Gated-operation set | `AERORA-SEC-REQ-013` | R-004 | Q-029 | `GCS-DECIDED` | `PROPOSED` | `STATIC` |
| `IF-SEC-006` | Authorisation denial event | `AERORA-GCS-REQ-025`, `AERORA-SEC-REQ-015`, `AERORA-SEC-REQ-019` | R-004 | — | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-SEC-007` | Audit identity | `AERORA-COMPLY-REQ-002`, `AERORA-SEC-REQ-019` | — | Q-016 | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-SEC-008` | Credential lifetime state | `AERORA-SEC-REQ-011` | — | — | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-SEC-009` | Credential reconciliation | `AERORA-AWS-REQ-009`, `AERORA-SEC-REQ-011` | — | Q-030 | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-SEC-010` | Security-to-cloud dependency | `AERORA-AWS-REQ-012` | — | — | `GCS-DECIDED` | `PROPOSED` | `UNIT` |
| `IF-SEC-011` | Advanced-UI toggle as an authorisation boundary | `AERORA-SEC-REQ-014` | — | — | `GCS-VERIFIED` | `PROPOSED` | `STATIC` |
| `IF-SEC-012` | Signing key provisioning | `AERORA-SEC-REQ-016` | R-024 | — | `GCS-VERIFIED` | `PROPOSED` | `AUDIT` |
| `IF-SEC-013` | Signing key storage at rest | `AERORA-SEC-REQ-017` | R-005 | Q-010 | `GCS-VERIFIED` | `OPEN` | `AUDIT` |
| `IF-SEC-014` | Signing key selection | — | R-006 | Q-010 | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-SEC-015` | Per-link signing state change | — | R-006 | Q-010 | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-SEC-016` | Signing timestamp persistence | — | R-006 | — | `GCS-VERIFIED` | `CONFIRMED` | `UNIT` |
| `IF-SEC-017` | Unsigned-message acceptance policy | `AERORA-SAFE-REQ-010` | R-006 | Q-010, Q-011 | `GCS-VERIFIED` | `OPEN` | `INTEGRATION` |
| `IF-SEC-018` | Signing key rotation | `AERORA-SEC-REQ-017` | R-005 | Q-016 | `GCS-VERIFIED` | `PROPOSED` | `AUDIT` |
| `IF-SEC-019` | Settings-schema version change | `AERORA-BUILD-REQ-009`, `AERORA-SEC-REQ-018` | R-023 | — | `GCS-VERIFIED` | `CONFIRMED` | `AUDIT` |
| `IF-SEC-020` | Telemetry field validation | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-029` | — | — | `GCS-VERIFIED` | `PROPOSED` | `UNIT` |
| `IF-SEC-021` | Dialect and dependency pin | `AERORA-BUILD-REQ-008`, `AERORA-SEC-REQ-020` | R-011, R-020 | Q-005, Q-032 | `GCS-VERIFIED` | `PROPOSED` | `AUDIT` |
| `IF-SYS-001` | Operator to ground station | `AERORA-GCS-REQ-015`, `AERORA-GCS-REQ-016` | — | Q-029 | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-SYS-002` | Ground station to vehicle | `AERORA-COMM-REQ-006`, `AERORA-COMM-REQ-010` | R-003 | Q-013, Q-015 | `GCS-VERIFIED`, `NOT-EVIDENCED` | `PROPOSED` | `INTEGRATION` |
| `IF-SYS-003` | Ground station to fleet backend | `AERORA-AWS-REQ-005`, `AERORA-AWS-REQ-006` | R-015 | Q-030 | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-SYS-004` | Ground station to local storage | `AERORA-GCS-REQ-025` | — | Q-039 | `GCS-VERIFIED` | `PROPOSED` | `UNIT` |
| `IF-SYS-005` | Ground station to companion | `AERORA-GPU-REQ-003`, `AERORA-MAV-REQ-007` | R-014 | — | `GCS-DECIDED` | `CONFIRMED` | `UNIT` |
| `IF-SYS-006` | Ground station to payload actuators | `AERORA-PAYLOAD-REQ-009`, `AERORA-PAYLOAD-REQ-010` | — | Q-009 | `GCS-DECIDED` | `CONFIRMED` | `STATIC` |
| `IF-SYS-007` | Ground station to power switching | `AERORA-POWER-REQ-008` | — | Q-026 | `GCS-DECIDED` | `CONFIRMED` | `STATIC` |
| `IF-SYS-008` | Ground station to inbound network | `AERORA-AWS-REQ-014` | R-016 | Q-027 | `GCS-VERIFIED` | `PROPOSED` | `UNIT` |
| `IF-TLM-001` | Standard vehicle telemetry | `AERORA-GCS-REQ-027` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-TLM-002` | Aerora system telemetry | `AERORA-MAV-REQ-009` | — | — | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-TLM-003` | Aerora payload telemetry | `AERORA-PAYLOAD-REQ-006` | R-002 | Q-023 | `BASELINE-ASSERTED` | `OPEN` | `INTEGRATION` |
| `IF-TLM-004` | Aerora product telemetry | `AERORA-PAYLOAD-REQ-012` | — | Q-023 | `PLACEHOLDER` | `OPEN` | `INTEGRATION` |
| `IF-TLM-005` | Aerora power telemetry | `AERORA-POWER-REQ-008` | — | Q-024 | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |
| `IF-TLM-006` | Aerora link telemetry | `AERORA-COMM-REQ-007`, `AERORA-COMM-REQ-008` | — | Q-015 | `MODEL` | `PROPOSED` | `INTEGRATION` |
| `IF-TLM-007` | Aerora autonomy telemetry | `AERORA-GPU-REQ-005`, `AERORA-GPU-REQ-006` | R-014 | Q-008 | `MODEL` | `OPEN` | `INTEGRATION` |
| `IF-TLM-008` | Fact group registration | `AERORA-GCS-REQ-010`, `AERORA-GCS-REQ-022` | R-027 | — | `GCS-VERIFIED` | `CONFIRMED` | `UNIT` |
| `IF-TLM-009` | Fact metadata | `AERORA-GCS-REQ-026` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `UNIT` |
| `IF-TLM-010` | Freshness stamping | `AERORA-GCS-REQ-027` | — | — | `GCS-DECIDED` | `PROPOSED` | `UNIT` |
| `IF-TLM-011` | Staleness evaluation | `AERORA-GCS-REQ-027`, `AERORA-GPU-REQ-006` | — | Q-036 | `GCS-DECIDED` | `PROPOSED` | `UNIT` |
| `IF-TLM-012` | Telemetry record | `AERORA-COMPLY-REQ-004`, `AERORA-GCS-REQ-025` | — | Q-039 | `GCS-VERIFIED` | `CONFIRMED` | `UNIT` |
| `IF-TLM-013` | Telemetry value selection | `AERORA-GCS-REQ-010` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-UIS-001` | Effective state presentation | `AERORA-GCS-REQ-029`, `AERORA-SAFE-REQ-006` | R-028 | — | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-UIS-002` | Requested state presentation | `AERORA-GCS-REQ-016`, `AERORA-GCS-REQ-017` | R-028 | Q-036 | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-UIS-003` | Proposed state presentation | `AERORA-GPU-REQ-005`, `AERORA-GPU-REQ-006`, `AERORA-GPU-REQ-007` | — | Q-008 | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-UIS-004` | Staleness presentation | `AERORA-GCS-REQ-027` | — | Q-036 | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-UIS-005` | Unknown and unavailable presentation | `AERORA-GCS-REQ-026`, `AERORA-GPU-REQ-007` | — | Q3-009 | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-UIS-006` | Action availability | `AERORA-UI-REQ-008` | — | Q3-009 | `GCS-VERIFIED` | `PROPOSED` | `DEMO` |
| `IF-UIS-007` | Command progress presentation | `AERORA-GCS-REQ-015`, `AERORA-GCS-REQ-016` | R-025 | Q-036 | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-UIS-008` | Confirmation presentation | `AERORA-SAFE-REQ-007` | — | Q-007 | `MODEL` | `PROPOSED` | `INTEGRATION` |
| `IF-UIS-009` | Independent-indicator separation | `AERORA-AWS-REQ-008`, `AERORA-SAFE-REQ-011` | — | Q-040 | `BASELINE-ASSERTED` | `PROPOSED` | `DEMO` |
| `IF-UIS-010` | Tier-1 C++ option seam | `AERORA-UI-REQ-004` | R-014 | Q-038 | `GCS-VERIFIED` | `CONFIRMED` | `STATIC` |
| `IF-UIS-011` | Tier-2 purpose-built QML seam | `AERORA-UI-REQ-004` | R-014 | Q-020 | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-UIS-012` | Tier-3 populated-file override | `AERORA-UI-REQ-004`, `AERORA-UI-REQ-005`, `AERORA-UI-REQ-006` | R-014, R-019, R-026 | Q-038 | `GCS-VERIFIED` | `PROPOSED` | `AUDIT` |
| `IF-UIS-013` | Aerora QML module registration | `AERORA-UI-REQ-010` | R-019 | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-UIS-014` | Resource alias precedence | `AERORA-TEST-REQ-007`, `AERORA-UI-REQ-007` | R-019 | Q-020 | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-UIS-015` | Startup self-check | `AERORA-TEST-REQ-007`, `AERORA-UI-REQ-007` | R-010, R-019 | Q-020 | `GCS-DECIDED` | `PROPOSED` | `DEMO` |
| `IF-UIS-016` | Operator action origination | `AERORA-GCS-REQ-019`, `AERORA-SEC-REQ-012` | R-004 | Q-029 | `GCS-VERIFIED` | `CONFIRMED` | `UNIT` |
| `IF-UIS-017` | Advanced-UI disclosure | `AERORA-SEC-REQ-014` | — | Q-029 | `GCS-VERIFIED` | `CONFIRMED` | `STATIC` |
| `IF-VID-001` | MAVLink system id | `AERORA-VEH-REQ-001` | R-017 | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-VID-002` | MAVLink component id | `AERORA-GPU-REQ-002` | — | — | `GCS-VERIFIED` | `CONFIRMED` | `INTEGRATION` |
| `IF-VID-003` | Aerora vehicle identity report | `AERORA-VEH-REQ-002`, `AERORA-VEH-REQ-003`, `AERORA-VEH-REQ-004` | R-017 | Q-016 | `NOT-EVIDENCED` | `OPEN` | `INTEGRATION` |
| `IF-VID-004` | Identity binding | `AERORA-VEH-REQ-003`, `AERORA-VEH-REQ-005` | R-017 | Q-016 | `GCS-DECIDED` | `PROPOSED` | `INTEGRATION` |
| `IF-VID-005` | Identity source selection | `AERORA-VEH-REQ-002` | R-017 | Q-016 | `NOT-EVIDENCED` | `OPEN` | `BENCH` |
| `IF-VID-006` | Unidentified-vehicle attribution | `AERORA-VEH-REQ-005` | R-017 | — | `GCS-DECIDED` | `PROPOSED` | `UNIT` |
| `IF-VID-007` | Registry reconciliation | `AERORA-AWS-REQ-005`, `AERORA-AWS-REQ-009` | R-015 | Q-030, Q-042 | `BASELINE-ASSERTED` | `PROPOSED` | `INTEGRATION` |

## Interfaces with no Aerora requirement

Eleven interfaces carry no requirement identifier. Phase 3 **does not invent one**, so each is
recorded here with the reason.

| ID | Interface | Why no requirement |
| --- | --- | --- |
| `IF-FC-004` | Arming state | Standard MAVLink, standard upstream handling, unchanged by Aerora |
| `IF-FC-008` | GPS state | Standard handling, unchanged |
| `IF-FC-009` | Estimator state | Standard handling, unchanged |
| `IF-FC-024` | Ground station heartbeat | Standard behaviour; absence has no ground-station consequence |
| `IF-MIS-016` | QGC resume filter modification | A **prohibition** rejected in Phase 2.1 as a core edit. There is nothing to require, only something not to do |
| `IF-PAR-007` | Parameter cache | Upstream mechanism with its own validity check; Aerora adds nothing |
| `IF-PWR-005` | Battery state of charge | Governed by the general telemetry rules at `IF-TLM-010` and `IF-TLM-011`, not by a power-specific requirement |
| `IF-RAD-004` | Byte stream | Upstream transport behaviour retained unchanged |
| `IF-SEC-014` | Signing key selection | Upstream signing implementation retained unchanged |
| `IF-SEC-015` | Per-link signing state change | Upstream signing implementation retained unchanged |
| `IF-SEC-016` | Signing timestamp persistence | Upstream signing implementation retained unchanged |

Nine of the eleven are the same observation: upstream already does this correctly and Aerora inherits
it. Specifying them without a requirement is the point — they are contracts Aerora **depends on**
and does not impose. If upstream changed one, the ICD row is where a reviewer would notice.

The two exceptions are `IF-PWR-005`, which the general telemetry rules already govern, and
`IF-MIS-016`, which is a rejected core edit rather than an inherited behaviour. A prohibition needs
no requirement to point at; it needs a decision, and ADR-005 is that decision.

## Requirement coverage

### Phase 2 requirements

| Measure | Value |
| --- | --- |
| Phase 2 requirements published | 120 |
| Active | 119 |
| Referenced by at least one interface row | **111** |
| Not referenced by an interface row | 9 |

| Not referenced | Reason | Carried by |
| --- | --- | --- |
| `AERORA-BUILD-REQ-004` | Repository and remote configuration — a process obligation, not an interface | [23-build-readiness.md](23-build-readiness.md) |
| `AERORA-BUILD-REQ-005` | Fork-point recording — process | [23-build-readiness.md](23-build-readiness.md) |
| `AERORA-BUILD-REQ-006` | Overlay-directory rule — build structure | [23-build-readiness.md](23-build-readiness.md) |
| `AERORA-BUILD-REQ-007` | Drift and seam review at each merge — process | [23-build-readiness.md](23-build-readiness.md), and `IF-UIS-012` carries the same obligation under `AERORA-UI-REQ-006` |
| `AERORA-GCS-REQ-009` | Static-registration linkage rule — build structure | [23-build-readiness.md](23-build-readiness.md) |
| `AERORA-TEST-REQ-006` | Property tests in the default CI gate — process | [22-phase-3-verification-plan.md](22-phase-3-verification-plan.md), [23-build-readiness.md](23-build-readiness.md) |
| `AERORA-TEST-REQ-008` | Mechanical drift checking — tooling | [22-phase-3-verification-plan.md](22-phase-3-verification-plan.md) |
| `AERORA-TEST-REQ-009` | Use the upstream test framework unmodified — process | [22-phase-3-verification-plan.md](22-phase-3-verification-plan.md) |
| `AERORA-UI-REQ-009` | **Withdrawn** in Phase 2 as a duplicate of `AERORA-SEC-REQ-014` | Nothing. Correctly unreferenced |

Eight of the nine are build, test or process requirements with no boundary to cross, and they are
referenced by the two Phase 3 documents that cover build and verification. The ninth is withdrawn,
and referencing it would have been an error.

**There is no requirement in the Phase 2 set that Phase 3 has silently dropped.**

### Phase 1 requirements

Phase 1 published 55 requirements under earlier identifiers, some of which Phase 2 renumbered — the
`AERORA-PWR-REQ-` to `AERORA-POWER-REQ-` change being the documented case.

**No Phase 1 identifier appears in any Phase 3 interface row.** Phase 3 cites the Phase 2 canonical
identifier in every case, which is deliberate: two identifiers for one obligation is the condition
`AERORA-UI-REQ-009` was withdrawn to avoid. The alias mapping is in
[traceability.md](../01-aerora-architecture/traceability.md).

## Risk coverage

| Measure | Value |
| --- | --- |
| Existing risks | 29 |
| Referenced by at least one interface row | **22** |
| Not referenced | 7 |

| Not referenced | Subject | Why no interface carries it |
| --- | --- | --- |
| R-001 | Qt licensing decision gates the delivery model | A legal and commercial question; ADR-009 and [23-build-readiness.md](23-build-readiness.md) |
| R-007 | No signature verification on updates or firmware | A release-process gap; no ground-station interface crosses it |
| R-008 | Checkout targets upstream, not an Aerora fork | Repository configuration; [23-build-readiness.md](23-build-readiness.md) |
| R-009 | Host cannot build QGroundControl | An environment fact; [23-build-readiness.md](23-build-readiness.md) |
| R-013 | MAVLink channel budget is 15 | A resource budget, not a boundary. Recorded in the radio and runtime documents |
| R-021 | Overlay test reachability unverified | Test placement; [22-phase-3-verification-plan.md](22-phase-3-verification-plan.md) |
| R-029 | Single-firmware UI simplification unreachable | Informational; the alternative route is `IF-UIS-010` under `AERORA-UI-REQ-004` |

The three risks carried by the most interfaces are worth naming, because they indicate where the
specification is most load-bearing:

| Risk | Interfaces | Subject |
| --- | --- | --- |
| R-002 | 15 | Payload state commands silently dropped from a resume mission |
| R-014 | 12 | Overlay maintenance burden against a moving upstream |
| R-028 | 11 | Authority mode and flight mode may be conflated |

R-002 touching fifteen interfaces is the clearest signal in this table. A single unevidenced
retention behaviour propagates through payload, mission, flight-controller, MAVLink and logging
contracts, which is why the Phase 2.1 evidence matrix separated it as item 14 and why the mission
fallback is unconditional.

## Question coverage

| Measure | Value |
| --- | --- |
| Existing questions, Phases 1 to 2.1 | 43 |
| Referenced by at least one interface row | **31** |
| New Phase 3 questions | 9 |
| Phase 3 questions referenced by at least one interface row | 6 |

The three Phase 3 questions not cited in an interface row — Q3-003, Q3-005 and Q3-007 — are cited in
the body text of the documents that raise them, because each concerns the shape of a whole interface
group rather than one row. All nine are defined in
[20-phase-3-open-questions.md](20-phase-3-open-questions.md).

The questions carried by the most interfaces:

| Question | Interfaces | Subject |
| --- | --- | --- |
| Q-007 | 17 | Failsafe behaviour and mode mapping |
| Q-023 | 15 | Payload actuator interfaces |
| Q-029 | 10 | Operator role model |
| Q-016 | 10 | Fleet vehicle identity |

Q-007 and Q-023 between them touch 32 interface rows. Both are `ARCH-SHAPING` rather than blocking,
which remains the correct classification — the interfaces exist and are specified; their enumerations
do not.

## Checks performed

Run against the document set as committed. These are structural checks on the specification, not
verification of any interface.

| Check | Result |
| --- | --- |
| Every interface defined in exactly one document | **Pass** |
| Every interface referenced is defined | **Pass** |
| Every defined interface appears in this table | **Pass** |
| Every interface has an evidence status | **Pass** |
| Every interface has a status | **Pass** |
| Every interface has a verification method | **Pass** |
| Every requirement identifier cited exists in the Phase 1 or Phase 2 set | **Pass** |
| Every risk identifier cited exists in R-001 to R-029 | **Pass** |
| Every question identifier cited exists, or is defined as a Q3 question | **Pass** |
| No new requirement identifier is defined by Phase 3 | **Pass** |
| Every table row has the same cell count as its header | **Pass** |
| Internal document links resolve | **Pass** |

The extraction and the checks are scripted. The scripts are development tooling and are not part of
the deliverable; the counts in this document are reproducible from the documents themselves.

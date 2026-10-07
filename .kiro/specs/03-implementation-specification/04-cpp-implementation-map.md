# C++ Implementation Map

All 240 interface contracts mapped to an owning module, a QGroundControl seam, a class, a state-model
position, a MAVLink boundary, a test target and an implementation status.

Mechanically generated from the Phase 3 interface set and the module register, then reviewed. A row
here that disagrees with its interface control document is a defect in the generator, not a second
opinion.

## Status distribution

| Status | Count | Share |
| --- | --- | --- |
| `IMPLEMENTATION-PLANNED` | **182** | 76% |
| `CONTRACT-ONLY` | **51** | 21% |
| `BLOCKED` | **7** | 3% |
| `IMPLEMENTED` | **0** | — |
| `VERIFIED` | **0** | — |

### How status was assigned

| Rule | Result |
| --- | --- |
| The interface is `CONFIRMED` or `PROPOSED` in Phase 3, and no finding blocks it | `IMPLEMENTATION-PLANNED` |
| The interface is `OPEN` in Phase 3 — an input it needs does not exist | `CONTRACT-ONLY` |
| A Phase 3.1 finding establishes that implementation cannot proceed safely | `BLOCKED` |
| Code exists | `IMPLEMENTED` — **never, no build exists** |
| Code has been executed with evidence | `VERIFIED` — **never** |

`CONTRACT-ONLY` is not a lesser plan; it is the correct status for an interface whose fallback is
specified but whose content is a firmware or product deliverable. All 51 have a stated fallback,
audited in Phase 3.1.

### The seven blocked interfaces

| Interface | Finding | Why implementation cannot proceed |
| --- | --- | --- |
| `IF-SEC-013` | F3.1-01 | Key storage has no seam. Any overlay-side implementation would be a core edit in disguise |
| `IF-LOG-020` | F3.1-03 | Audit integrity has a fixed contract shape and no mechanism. A shape-satisfying stub would read as compliant while doing nothing |
| `IF-RAD-001` | F3.1-02 | Radio host presentation is unevidenced. Implementing either branch would commit to an unverified hardware fact |
| `IF-SEC-003` | F3.1-04 | Role resolution is code-isolated but has no deployable default without Q-029 |
| `IF-FC-017` | F3.1-05 | Capability inventory field definition unknown; the conservative fallback discards information |
| `IF-UIS-005` | F3.1-05 | Unknown-versus-unavailable cannot be populated for Aerora capabilities |
| `IF-UIS-006` | F3.1-05 | Hide-versus-disable cannot be chosen correctly |

`BLOCKED` here means *do not write this code yet*, not *the architecture is wrong*. Each has a named
owner and a named piece of evidence that would unblock it.

## Column meanings

| Column | Content |
| --- | --- |
| Interface | The Phase 3 identifier. Unchanged, never renumbered |
| Owner | The `M-*` module from [02-module-architecture.md](02-module-architecture.md) |
| QGC extension seam | The verified seam from [03-qgc-extension-map.md](03-qgc-extension-map.md) |
| Class / QML module | The Aerora type that will realise it |
| State model | Which state classes the interface produces |
| MAVLink boundary | Whether and how the interface touches the protocol |
| Test target | The verification method carried from the Phase 3 plan |
| Status | The implementation status |

Module, seam, class and MAVLink boundary are assigned at **domain** granularity, because that is the
granularity at which the module register was written. Per-interface class assignment inside a module
is an implementation detail and would be false precision here.

## The map

| Interface | Name | Owner | QGC extension seam | Class / QML module | State model | MAVLink boundary | Test target | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-AUT-001` | Effective authority mode | AeroraAuthority | Vehicle::sendMavCommand observation | AeroraCommandTracker | REPORTED / EFFECTIVE | Standard command protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-AUT-002` | Authority mode change request | AeroraAuthority | Vehicle::sendMavCommand observation | AeroraCommandTracker | UNKNOWN / UNAVAILABLE until reported | Standard command protocol | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-AUT-003` | Authority source | AeroraAuthority | Vehicle::sendMavCommand observation | AeroraCommandTracker | REPORTED / EFFECTIVE | Standard command protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-AUT-004` | Confirmation pending | AeroraAuthority | Vehicle::sendMavCommand observation | AeroraCommandTracker | REPORTED / EFFECTIVE | Standard command protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-AUT-005` | Operator confirmation | AeroraAuthority | Vehicle::sendMavCommand observation | AeroraCommandTracker | UNKNOWN / UNAVAILABLE until reported | Standard command protocol | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-AUT-006` | Command submission | AeroraAuthority | Vehicle::sendMavCommand observation | AeroraCommandTracker | REPORTED / EFFECTIVE | Standard command protocol | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-AUT-007` | Command acknowledgement | AeroraAuthority | Vehicle::sendMavCommand observation | AeroraCommandTracker | REPORTED / EFFECTIVE | Standard command protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-AUT-008` | Command state | AeroraAuthority | Vehicle::sendMavCommand observation | AeroraCommandTracker | REPORTED / EFFECTIVE | Standard command protocol | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-AUT-009` | Effectiveness corroboration | AeroraAuthority | Vehicle::sendMavCommand observation | AeroraCommandTracker | REPORTED / EFFECTIVE | Standard command protocol | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-AUT-010` | Failsafe state | AeroraAuthority | Vehicle::sendMavCommand observation | AeroraCommandTracker | UNKNOWN / UNAVAILABLE until reported | Standard command protocol | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-AUT-011` | Operator authorisation decision | AeroraAuthority | Vehicle::sendMavCommand observation | AeroraCommandTracker | REPORTED / EFFECTIVE | Standard command protocol | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-AUT-012` | GCS-initiated failsafe action | AeroraAuthority | Vehicle::sendMavCommand observation | AeroraCommandTracker | REPORTED / EFFECTIVE | Standard command protocol | `STATIC` | **IMPLEMENTATION-PLANNED** |
| `IF-CLD-001` | Cloud connectivity state | AeroraCloud | None — Aerora-owned, background thread | AeroraCloudAdapter | REPORTED / EFFECTIVE | None — prohibited | `DEMO` | **IMPLEMENTATION-PLANNED** |
| `IF-CLD-002` | Cloud threading boundary | AeroraCloud | None — Aerora-owned, background thread | AeroraCloudAdapter | REPORTED / EFFECTIVE | None — prohibited | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-CLD-003` | Inbound network connection | AeroraCloud | None — Aerora-owned, background thread | AeroraCloudAdapter | REPORTED / EFFECTIVE | None — prohibited | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-CLD-004` | Cloud participation in the command path | AeroraCloud | None — Aerora-owned, background thread | AeroraCloudAdapter | REPORTED / EFFECTIVE | None — prohibited | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-CLD-005` | Mission library synchronisation | AeroraCloud | None — Aerora-owned, background thread | AeroraCloudAdapter | REPORTED / EFFECTIVE | None — prohibited | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-CLD-006` | Fleet view data | AeroraCloud | None — Aerora-owned, background thread | AeroraCloudAdapter | REPORTED / EFFECTIVE | None — prohibited | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-CLD-007` | Configuration distribution | AeroraCloud | None — Aerora-owned, background thread | AeroraCloudAdapter | REPORTED / EFFECTIVE | None — prohibited | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-CLD-008` | Release and compatibility metadata | AeroraCloud | None — Aerora-owned, background thread | AeroraCloudAdapter | REPORTED / EFFECTIVE | None — prohibited | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-CLD-009` | Operator notification | AeroraCloud | None — Aerora-owned, background thread | AeroraCloudAdapter | REPORTED / EFFECTIVE | None — prohibited | `STATIC` | **IMPLEMENTATION-PLANNED** |
| `IF-CLD-010` | Reporting and dashboards | AeroraCloud | None — Aerora-owned, background thread | AeroraCloudAdapter | REPORTED / EFFECTIVE | None — prohibited | `STATIC` | **IMPLEMENTATION-PLANNED** |
| `IF-CLD-011` | Map and terrain tile cache | AeroraCloud | None — Aerora-owned, background thread | AeroraCloudAdapter | REPORTED / EFFECTIVE | None — prohibited | `DEMO` | **IMPLEMENTATION-PLANNED** |
| `IF-CLD-012` | Cloud-sourced data age | AeroraCloud | None — Aerora-owned, background thread | AeroraCloudAdapter | REPORTED / EFFECTIVE | None — prohibited | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-CLD-013` | Per-feature availability state | AeroraCloud | None — Aerora-owned, background thread | AeroraCloudAdapter | REPORTED / EFFECTIVE | None — prohibited | `STATIC` | **IMPLEMENTATION-PLANNED** |
| `IF-CLD-014` | Upload queue state | AeroraCloud | None — Aerora-owned, background thread | AeroraCloudAdapter | REPORTED / EFFECTIVE | None — prohibited | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-CLD-015` | Synchronisation conflict resolution | AeroraCloud | None — Aerora-owned, background thread | AeroraCloudAdapter | UNKNOWN / UNAVAILABLE until reported | None — prohibited | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-CLD-016` | Backend module interface boundary | AeroraCloud | None — Aerora-owned, background thread | AeroraCloudAdapter | REPORTED / EFFECTIVE | None — prohibited | `STATIC` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-001` | Vehicle state: attitude, position, velocity... | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-002` | Flight mode | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | UNKNOWN / UNAVAILABLE until reported | Standard + future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-FC-003` | Authority mode, effective | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-004` | Arming state | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-005` | System health and sensor status | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-006` | Battery and power telemetry | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `BENCH` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-007` | Link state, vehicle-reported | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `BENCH` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-008` | GPS state | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-009` | Estimator state | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-010` | Autonomy state and proposals | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | UNKNOWN / UNAVAILABLE until reported | Standard + future dialect | `HIL` | **CONTRACT-ONLY** |
| `IF-FC-011` | Companion health and presence | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `HIL` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-012` | Payload state, health, safety state | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | UNKNOWN / UNAVAILABLE until reported | Standard + future dialect | `BENCH` | **CONTRACT-ONLY** |
| `IF-FC-013` | Command acknowledgement | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-014` | Mission state and current index | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-015` | Parameters | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-016` | Diagnostics and status text | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-017` | Capability inventory | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | UNKNOWN / UNAVAILABLE until reported | Standard + future dialect | `BENCH` | **BLOCKED** |
| `IF-FC-018` | Aerora vehicle identity | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | UNKNOWN / UNAVAILABLE until reported | Standard + future dialect | `BENCH` | **CONTRACT-ONLY** |
| `IF-FC-019` | Operator flight commands | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-020` | Authority mode change request | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `HIL` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-021` | Payload commands | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | UNKNOWN / UNAVAILABLE until reported | Standard + future dialect | `BENCH` | **CONTRACT-ONLY** |
| `IF-FC-022` | Mission transfer | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-023` | Parameter write | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-FC-024` | Ground station heartbeat | AeroraVehicle | FirmwarePlugin::factGroups() | AeroraFirmwarePlugin | REPORTED / EFFECTIVE | Standard + future dialect | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-GPU-001` | Companion addressing | AeroraAutonomy | FirmwarePlugin::factGroups() | AeroraAutonomyFactGroup | UNKNOWN / UNAVAILABLE until reported | Read-only, no emit | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-GPU-002` | Proposal | AeroraAutonomy | FirmwarePlugin::factGroups() | AeroraAutonomyFactGroup | UNKNOWN / UNAVAILABLE until reported | Read-only, no emit | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-GPU-003` | Proposal outcome | AeroraAutonomy | FirmwarePlugin::factGroups() | AeroraAutonomyFactGroup | UNKNOWN / UNAVAILABLE until reported | Read-only, no emit | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-GPU-004` | Companion health | AeroraAutonomy | FirmwarePlugin::factGroups() | AeroraAutonomyFactGroup | UNKNOWN / UNAVAILABLE until reported | Read-only, no emit | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-GPU-005` | Companion presence | AeroraAutonomy | FirmwarePlugin::factGroups() | AeroraAutonomyFactGroup | UNKNOWN / UNAVAILABLE until reported | Read-only, no emit | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-GPU-006` | Effective state for corroboration | AeroraAutonomy | FirmwarePlugin::factGroups() | AeroraAutonomyFactGroup | REPORTED / EFFECTIVE | Read-only, no emit | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-GPU-007` | Autonomy capability | AeroraAutonomy | FirmwarePlugin::factGroups() | AeroraAutonomyFactGroup | UNKNOWN / UNAVAILABLE until reported | Read-only, no emit | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-GPU-008` | Ground station to companion | AeroraAutonomy | FirmwarePlugin::factGroups() | AeroraAutonomyFactGroup | REPORTED / EFFECTIVE | Read-only, no emit | `STATIC`, `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-001` | Application diagnostic log | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `STATIC` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-002` | MAVLink stream log | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-003` | Audit record store | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-004` | Operator action record | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-005` | Command lifecycle record | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-006` | Authority mode request record | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-007` | Effective authority change record | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-008` | Confirmation record | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-009` | Security event record | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-010` | Configuration change record | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-011` | Mission event record | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-012` | Failsafe event record | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | UNKNOWN / UNAVAILABLE until reported | None — local | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-LOG-013` | Value provenance in records | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-014` | Record writer queue | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-015` | Drop accounting | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-016` | Local-first write ordering | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-017` | Upload submission | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-018` | Upload to a non-Aerora endpoint | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-019` | Local record deletion on upload | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `STATIC` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-020` | Audit store integrity protection | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | UNKNOWN / UNAVAILABLE until reported | None — local | `INTEGRATION` | **BLOCKED** |
| `IF-LOG-021` | Record timestamp basis | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-022` | Correlation identity | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-023` | Retention deletion record | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-LOG-024` | Record export | AeroraAudit | None — Aerora-owned writer | AeroraAuditWriter | REPORTED / EFFECTIVE | None — local | `DEMO` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-001` | Heartbeat, base and custom mode | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-002` | Attitude, position, velocity, altitude | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-003` | System and sensor health | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-004` | Battery state | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-005` | GPS and estimator state | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-006` | Command send and acknowledgement | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-007` | Mission transfer and progress | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-008` | Parameter protocol | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-009` | Status text and events | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-010` | Radio status | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-011` | Message signing | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-012` | Aerora system status | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `BENCH` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-013` | Aerora vehicle capabilities | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `BENCH` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-014` | Aerora payload status | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `BENCH` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-015` | Aerora paint status | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `BENCH` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-016` | Aerora pump status | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `BENCH` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-017` | Aerora gimbal status | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `BENCH` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-018` | Aerora autonomy status | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `BENCH` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-019` | Aerora mission status | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `BENCH` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-020` | Aerora diagnostics | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `BENCH` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-021` | Reporting the Aerora autopilot identifier i... | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | UNKNOWN / UNAVAILABLE until reported | Protocol boundary | `BENCH` | **CONTRACT-ONLY** |
| `IF-MAV-022` | Reporting which component originated a rela... | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | UNKNOWN / UNAVAILABLE until reported | Protocol boundary | `BENCH` | **CONTRACT-ONLY** |
| `IF-MAV-023` | Reporting proposal acceptance or rejection ... | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | UNKNOWN / UNAVAILABLE until reported | Protocol boundary | `HIL` | **CONTRACT-ONLY** |
| `IF-MAV-024` | A per-command request identifier for audit ... | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | UNKNOWN / UNAVAILABLE until reported | Protocol boundary | `BENCH` | **CONTRACT-ONLY** |
| `IF-MAV-025` | Declaring the resume behaviour of a state-s... | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | UNKNOWN / UNAVAILABLE until reported | Protocol boundary | `BENCH` | **CONTRACT-ONLY** |
| `IF-MAV-026` | The vehicle reports a capability inventory ... | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | UNKNOWN / UNAVAILABLE until reported | Protocol boundary | `BENCH` | **CONTRACT-ONLY** |
| `IF-MAV-027` | Inbound message adjustment seam | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-MAV-028` | Core-plugin message seam | AeroraMavlink | Dialect generation + message seams | AeroraMessageRouter | REPORTED / EFFECTIVE | Protocol boundary | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-MIS-001` | Mission upload | AeroraMission | QGCCorePlugin complex items + PlanMasterController | AeroraComplexItem | REPORTED / EFFECTIVE | Standard mission protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MIS-002` | Mission download | AeroraMission | QGCCorePlugin complex items + PlanMasterController | AeroraComplexItem | REPORTED / EFFECTIVE | Standard mission protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MIS-003` | Mission current index | AeroraMission | QGCCorePlugin complex items + PlanMasterController | AeroraComplexItem | REPORTED / EFFECTIVE | Standard mission protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MIS-004` | Mission item reached | AeroraMission | QGCCorePlugin complex items + PlanMasterController | AeroraComplexItem | REPORTED / EFFECTIVE | Standard mission protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MIS-005` | Mission replacement | AeroraMission | QGCCorePlugin complex items + PlanMasterController | AeroraComplexItem | REPORTED / EFFECTIVE | Standard mission protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MIS-006` | Mission resume | AeroraMission | QGCCorePlugin complex items + PlanMasterController | AeroraComplexItem | REPORTED / EFFECTIVE | Standard mission protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MIS-007` | Mission pause | AeroraMission | QGCCorePlugin complex items + PlanMasterController | AeroraComplexItem | UNKNOWN / UNAVAILABLE until reported | Standard mission protocol | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-MIS-008` | Plan file | AeroraMission | QGCCorePlugin complex items + PlanMasterController | AeroraComplexItem | REPORTED / EFFECTIVE | Standard mission protocol | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-MIS-009` | Plan version and identity | AeroraMission | QGCCorePlugin complex items + PlanMasterController | AeroraComplexItem | REPORTED / EFFECTIVE | Standard mission protocol | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-MIS-010` | Mission verification | AeroraMission | QGCCorePlugin complex items + PlanMasterController | AeroraComplexItem | REPORTED / EFFECTIVE | Standard mission protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MIS-011` | Aerora complex item | AeroraMission | QGCCorePlugin complex items + PlanMasterController | AeroraComplexItem | UNKNOWN / UNAVAILABLE until reported | Standard mission protocol | `UNIT` | **CONTRACT-ONLY** |
| `IF-MIS-012` | Aerora mission command set | AeroraMission | QGCCorePlugin complex items + PlanMasterController | AeroraComplexItem | UNKNOWN / UNAVAILABLE until reported | Standard mission protocol | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-MIS-013` | Aerora mission status | AeroraMission | QGCCorePlugin complex items + PlanMasterController | AeroraComplexItem | REPORTED / EFFECTIVE | Standard mission protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-MIS-014` | Payload state interaction on resume | AeroraMission | QGCCorePlugin complex items + PlanMasterController | AeroraComplexItem | UNKNOWN / UNAVAILABLE until reported | Standard mission protocol | `BENCH` | **CONTRACT-ONLY** |
| `IF-MIS-015` | GCS-side payload state replay | AeroraMission | QGCCorePlugin complex items + PlanMasterController | AeroraComplexItem | REPORTED / EFFECTIVE | Standard mission protocol | `STATIC` | **IMPLEMENTATION-PLANNED** |
| `IF-MIS-016` | QGC resume filter modification | AeroraMission | QGCCorePlugin complex items + PlanMasterController | AeroraComplexItem | REPORTED / EFFECTIVE | Standard mission protocol | `STATIC` | **IMPLEMENTATION-PLANNED** |
| `IF-PAR-001` | Parameter discovery | AeroraVehicle | ParameterManager (unchanged) | None — Facts only | REPORTED / EFFECTIVE | Standard parameter protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PAR-002` | Parameter download | AeroraVehicle | ParameterManager (unchanged) | None — Facts only | REPORTED / EFFECTIVE | Standard parameter protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PAR-003` | Parameter write | AeroraVehicle | ParameterManager (unchanged) | None — Facts only | REPORTED / EFFECTIVE | Standard parameter protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PAR-004` | Parameter change notification | AeroraVehicle | ParameterManager (unchanged) | None — Facts only | REPORTED / EFFECTIVE | Standard parameter protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PAR-005` | Parameter metadata | AeroraVehicle | ParameterManager (unchanged) | None — Facts only | REPORTED / EFFECTIVE | Standard parameter protocol | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-PAR-006` | Parameter validation | AeroraVehicle | ParameterManager (unchanged) | None — Facts only | REPORTED / EFFECTIVE | Standard parameter protocol | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-PAR-007` | Parameter cache | AeroraVehicle | ParameterManager (unchanged) | None — Facts only | REPORTED / EFFECTIVE | Standard parameter protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PAR-008` | Parameter synchronisation state | AeroraVehicle | ParameterManager (unchanged) | None — Facts only | REPORTED / EFFECTIVE | Standard parameter protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PAR-009` | Payload configuration as parameters | AeroraVehicle | ParameterManager (unchanged) | None — Facts only | UNKNOWN / UNAVAILABLE until reported | Standard parameter protocol | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-PAR-010` | Parameter reset to defaults | AeroraVehicle | ParameterManager (unchanged) | None — Facts only | REPORTED / EFFECTIVE | Standard parameter protocol | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PAY-001` | Payload identity and version | AeroraPayload | FirmwarePlugin::factGroups() | AeroraPayloadFactGroup | REPORTED / EFFECTIVE | Future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PAY-002` | Payload capability set | AeroraPayload | FirmwarePlugin::factGroups() | AeroraPayloadFactGroup | UNKNOWN / UNAVAILABLE until reported | Future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-PAY-003` | Payload state | AeroraPayload | FirmwarePlugin::factGroups() | AeroraPayloadFactGroup | UNKNOWN / UNAVAILABLE until reported | Future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-PAY-004` | Payload health | AeroraPayload | FirmwarePlugin::factGroups() | AeroraPayloadFactGroup | REPORTED / EFFECTIVE | Future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PAY-005` | Payload safety state | AeroraPayload | FirmwarePlugin::factGroups() | AeroraPayloadFactGroup | REPORTED / EFFECTIVE | Future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PAY-006` | Payload emergency stop state | AeroraPayload | FirmwarePlugin::factGroups() | AeroraPayloadFactGroup | REPORTED / EFFECTIVE | Future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PAY-007` | Payload telemetry | AeroraPayload | FirmwarePlugin::factGroups() | AeroraPayloadFactGroup | REPORTED / EFFECTIVE | Future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PAY-008` | Payload configuration | AeroraPayload | FirmwarePlugin::factGroups() | AeroraPayloadFactGroup | UNKNOWN / UNAVAILABLE until reported | Future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-PAY-009` | Payload command | AeroraPayload | FirmwarePlugin::factGroups() | AeroraPayloadFactGroup | UNKNOWN / UNAVAILABLE until reported | Future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-PAY-010` | Operator emergency stop request | AeroraPayload | FirmwarePlugin::factGroups() | AeroraPayloadFactGroup | UNKNOWN / UNAVAILABLE until reported | Future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-PAY-011` | Payload calibration | AeroraPayload | FirmwarePlugin::factGroups() | AeroraPayloadFactGroup | UNKNOWN / UNAVAILABLE until reported | Future dialect | `BENCH` | **CONTRACT-ONLY** |
| `IF-PAY-012` | Payload diagnostics | AeroraPayload | FirmwarePlugin::factGroups() | AeroraPayloadFactGroup | UNKNOWN / UNAVAILABLE until reported | Future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-PAY-013` | Payload actuator interface | AeroraPayload | FirmwarePlugin::factGroups() | AeroraPayloadFactGroup | UNKNOWN / UNAVAILABLE until reported | Future dialect | `STATIC` | **CONTRACT-ONLY** |
| `IF-PAY-014` | Payload state retention across interruption | AeroraPayload | FirmwarePlugin::factGroups() | AeroraPayloadFactGroup | UNKNOWN / UNAVAILABLE until reported | Future dialect | `BENCH` | **CONTRACT-ONLY** |
| `IF-PAY-015` | Ground station to payload actuators | AeroraPayload | FirmwarePlugin::factGroups() | AeroraPayloadFactGroup | REPORTED / EFFECTIVE | Future dialect | `STATIC` | **IMPLEMENTATION-PLANNED** |
| `IF-PWR-001` | Tether connected or disconnected | AeroraPower | FirmwarePlugin::factGroups() | AeroraPowerFactGroup | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PWR-002` | Battery present or absent | AeroraPower | FirmwarePlugin::factGroups() | AeroraPowerFactGroup | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PWR-003` | Active source | AeroraPower | FirmwarePlugin::factGroups() | AeroraPowerFactGroup | UNKNOWN / UNAVAILABLE until reported | Standard + future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-PWR-004` | Source transition | AeroraPower | FirmwarePlugin::factGroups() | AeroraPowerFactGroup | UNKNOWN / UNAVAILABLE until reported | Standard + future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-PWR-005` | Battery state of charge | AeroraPower | FirmwarePlugin::factGroups() | AeroraPowerFactGroup | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PWR-006` | Voltage, current, power | AeroraPower | FirmwarePlugin::factGroups() | AeroraPowerFactGroup | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PWR-007` | Power-system fault | AeroraPower | FirmwarePlugin::factGroups() | AeroraPowerFactGroup | UNKNOWN / UNAVAILABLE until reported | Standard + future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-PWR-008` | Estimated remaining energy, where validated | AeroraPower | FirmwarePlugin::factGroups() | AeroraPowerFactGroup | UNKNOWN / UNAVAILABLE until reported | Standard + future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-PWR-009` | Degraded or limited-power mode | AeroraPower | FirmwarePlugin::factGroups() | AeroraPowerFactGroup | UNKNOWN / UNAVAILABLE until reported | Standard + future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-PWR-010` | Converter state | AeroraPower | FirmwarePlugin::factGroups() | AeroraPowerFactGroup | UNKNOWN / UNAVAILABLE until reported | Standard + future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-PWR-011` | Watchdog fault | AeroraPower | FirmwarePlugin::factGroups() | AeroraPowerFactGroup | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-PWR-012` | Power capability | AeroraPower | FirmwarePlugin::factGroups() | AeroraPowerFactGroup | UNKNOWN / UNAVAILABLE until reported | Standard + future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-RAD-001` | Radio host presentation | AeroraLinkState | SerialLink / SerialConfiguration | AeroraLinkStateModel | UNKNOWN / UNAVAILABLE until reported | Transport only | `BENCH` | **BLOCKED** |
| `IF-RAD-002` | Serial port configuration | AeroraLinkState | SerialLink / SerialConfiguration | AeroraLinkStateModel | REPORTED / EFFECTIVE | Transport only | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-RAD-003` | USB device identity | AeroraLinkState | SerialLink / SerialConfiguration | AeroraLinkStateModel | UNKNOWN / UNAVAILABLE until reported | Transport only | `BENCH` | **CONTRACT-ONLY** |
| `IF-RAD-004` | Byte stream | AeroraLinkState | SerialLink / SerialConfiguration | AeroraLinkStateModel | REPORTED / EFFECTIVE | Transport only | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-RAD-005` | Link lifecycle | AeroraLinkState | SerialLink / SerialConfiguration | AeroraLinkStateModel | REPORTED / EFFECTIVE | Transport only | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-RAD-006` | Auto-connect decision | AeroraLinkState | SerialLink / SerialConfiguration | AeroraLinkStateModel | REPORTED / EFFECTIVE | Transport only | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-RAD-007` | GCS-local link observation | AeroraLinkState | SerialLink / SerialConfiguration | AeroraLinkStateModel | REPORTED / EFFECTIVE | Transport only | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-RAD-008` | Vehicle-reported link state | AeroraLinkState | SerialLink / SerialConfiguration | AeroraLinkStateModel | REPORTED / EFFECTIVE | Transport only | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-RAD-009` | Reconnection | AeroraLinkState | SerialLink / SerialConfiguration | AeroraLinkStateModel | REPORTED / EFFECTIVE | Transport only | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-001` | Operator sign-in | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-002` | Local credential validation | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-003` | Role resolution | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | UNKNOWN / UNAVAILABLE until reported | Signing path | `INTEGRATION` | **BLOCKED** |
| `IF-SEC-004` | Authorisation decision | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-005` | Gated-operation set | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `STATIC` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-006` | Authorisation denial event | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-007` | Audit identity | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-008` | Credential lifetime state | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-009` | Credential reconciliation | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-010` | Security-to-cloud dependency | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-011` | Advanced-UI toggle as an authorisation boun... | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `STATIC` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-012` | Signing key provisioning | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `AUDIT` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-013` | Signing key storage at rest | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | UNKNOWN / UNAVAILABLE until reported | Signing path | `AUDIT` | **BLOCKED** |
| `IF-SEC-014` | Signing key selection | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-015` | Per-link signing state change | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-016` | Signing timestamp persistence | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-017` | Unsigned-message acceptance policy | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | UNKNOWN / UNAVAILABLE until reported | Signing path | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-SEC-018` | Signing key rotation | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `AUDIT` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-019` | Settings-schema version change | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `AUDIT` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-020` | Telemetry field validation | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-SEC-021` | Dialect and dependency pin | AeroraSecurity | None for key storage — see F3.1-01 | AeroraSecurityModule | REPORTED / EFFECTIVE | Signing path | `AUDIT` | **IMPLEMENTATION-PLANNED** |
| `IF-SYS-001` | Operator to ground station | AeroraPlugin | QGCCorePlugin subclass | AeroraPlugin | REPORTED / EFFECTIVE | None — internal | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-SYS-002` | Ground station to vehicle | AeroraPlugin | QGCCorePlugin subclass | AeroraPlugin | REPORTED / EFFECTIVE | None — internal | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-SYS-003` | Ground station to fleet backend | AeroraPlugin | QGCCorePlugin subclass | AeroraPlugin | REPORTED / EFFECTIVE | None — internal | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-SYS-004` | Ground station to local storage | AeroraPlugin | QGCCorePlugin subclass | AeroraPlugin | REPORTED / EFFECTIVE | None — internal | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-SYS-005` | Ground station to companion | AeroraPlugin | QGCCorePlugin subclass | AeroraPlugin | REPORTED / EFFECTIVE | None — internal | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-SYS-006` | Ground station to payload actuators | AeroraPlugin | QGCCorePlugin subclass | AeroraPlugin | REPORTED / EFFECTIVE | None — internal | `STATIC` | **IMPLEMENTATION-PLANNED** |
| `IF-SYS-007` | Ground station to power switching | AeroraPlugin | QGCCorePlugin subclass | AeroraPlugin | REPORTED / EFFECTIVE | None — internal | `STATIC` | **IMPLEMENTATION-PLANNED** |
| `IF-SYS-008` | Ground station to inbound network | AeroraPlugin | QGCCorePlugin subclass | AeroraPlugin | REPORTED / EFFECTIVE | None — internal | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-TLM-001` | Standard vehicle telemetry | AeroraTelemetry | FactGroup + FactMetaData JSON | AeroraFactGroupBase | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-TLM-002` | Aerora system telemetry | AeroraTelemetry | FactGroup + FactMetaData JSON | AeroraFactGroupBase | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-TLM-003` | Aerora payload telemetry | AeroraTelemetry | FactGroup + FactMetaData JSON | AeroraFactGroupBase | UNKNOWN / UNAVAILABLE until reported | Standard + future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-TLM-004` | Aerora product telemetry | AeroraTelemetry | FactGroup + FactMetaData JSON | AeroraFactGroupBase | UNKNOWN / UNAVAILABLE until reported | Standard + future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-TLM-005` | Aerora power telemetry | AeroraTelemetry | FactGroup + FactMetaData JSON | AeroraFactGroupBase | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-TLM-006` | Aerora link telemetry | AeroraTelemetry | FactGroup + FactMetaData JSON | AeroraFactGroupBase | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-TLM-007` | Aerora autonomy telemetry | AeroraTelemetry | FactGroup + FactMetaData JSON | AeroraFactGroupBase | UNKNOWN / UNAVAILABLE until reported | Standard + future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-TLM-008` | Fact group registration | AeroraTelemetry | FactGroup + FactMetaData JSON | AeroraFactGroupBase | REPORTED / EFFECTIVE | Standard + future dialect | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-TLM-009` | Fact metadata | AeroraTelemetry | FactGroup + FactMetaData JSON | AeroraFactGroupBase | REPORTED / EFFECTIVE | Standard + future dialect | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-TLM-010` | Freshness stamping | AeroraTelemetry | FactGroup + FactMetaData JSON | AeroraFactGroupBase | REPORTED / EFFECTIVE | Standard + future dialect | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-TLM-011` | Staleness evaluation | AeroraTelemetry | FactGroup + FactMetaData JSON | AeroraFactGroupBase | REPORTED / EFFECTIVE | Standard + future dialect | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-TLM-012` | Telemetry record | AeroraTelemetry | FactGroup + FactMetaData JSON | AeroraFactGroupBase | REPORTED / EFFECTIVE | Standard + future dialect | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-TLM-013` | Telemetry value selection | AeroraTelemetry | FactGroup + FactMetaData JSON | AeroraFactGroupBase | REPORTED / EFFECTIVE | Standard + future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-UIS-001` | Effective state presentation | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | REPORTED / EFFECTIVE | None — presentation | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-UIS-002` | Requested state presentation | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | REPORTED / EFFECTIVE | None — presentation | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-UIS-003` | Proposed state presentation | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | REPORTED / EFFECTIVE | None — presentation | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-UIS-004` | Staleness presentation | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | REPORTED / EFFECTIVE | None — presentation | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-UIS-005` | Unknown and unavailable presentation | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | REPORTED / EFFECTIVE | None — presentation | `INTEGRATION` | **BLOCKED** |
| `IF-UIS-006` | Action availability | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | REPORTED / EFFECTIVE | None — presentation | `DEMO` | **BLOCKED** |
| `IF-UIS-007` | Command progress presentation | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | REPORTED / EFFECTIVE | None — presentation | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-UIS-008` | Confirmation presentation | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | REPORTED / EFFECTIVE | None — presentation | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-UIS-009` | Independent-indicator separation | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | REPORTED / EFFECTIVE | None — presentation | `DEMO` | **IMPLEMENTATION-PLANNED** |
| `IF-UIS-010` | Tier-1 C++ option seam | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | REPORTED / EFFECTIVE | None — presentation | `STATIC` | **IMPLEMENTATION-PLANNED** |
| `IF-UIS-011` | Tier-2 purpose-built QML seam | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | REPORTED / EFFECTIVE | None — presentation | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-UIS-012` | Tier-3 populated-file override | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | REPORTED / EFFECTIVE | None — presentation | `AUDIT` | **IMPLEMENTATION-PLANNED** |
| `IF-UIS-013` | Aerora QML module registration | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | REPORTED / EFFECTIVE | None — presentation | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-UIS-014` | Override resolution | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | UNKNOWN / UNAVAILABLE until reported | None — presentation | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-UIS-015` | Startup self-check | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | REPORTED / EFFECTIVE | None — presentation | `DEMO` | **IMPLEMENTATION-PLANNED** |
| `IF-UIS-016` | Operator action origination | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | REPORTED / EFFECTIVE | None — presentation | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-UIS-017` | Advanced-UI disclosure | AeroraUi | QGCOptions / tier-2 QML seams / interceptor | Aerora QML modules | REPORTED / EFFECTIVE | None — presentation | `STATIC` | **IMPLEMENTATION-PLANNED** |
| `IF-VID-001` | MAVLink system id | AeroraIdentity | FirmwarePlugin::factGroups() | AeroraIdentityBinding | REPORTED / EFFECTIVE | Future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-VID-002` | MAVLink component id | AeroraIdentity | FirmwarePlugin::factGroups() | AeroraIdentityBinding | REPORTED / EFFECTIVE | Future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-VID-003` | Aerora vehicle identity report | AeroraIdentity | FirmwarePlugin::factGroups() | AeroraIdentityBinding | UNKNOWN / UNAVAILABLE until reported | Future dialect | `INTEGRATION` | **CONTRACT-ONLY** |
| `IF-VID-004` | Identity binding | AeroraIdentity | FirmwarePlugin::factGroups() | AeroraIdentityBinding | REPORTED / EFFECTIVE | Future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |
| `IF-VID-005` | Identity source selection | AeroraIdentity | FirmwarePlugin::factGroups() | AeroraIdentityBinding | UNKNOWN / UNAVAILABLE until reported | Future dialect | `BENCH` | **CONTRACT-ONLY** |
| `IF-VID-006` | Unidentified-vehicle attribution | AeroraIdentity | FirmwarePlugin::factGroups() | AeroraIdentityBinding | REPORTED / EFFECTIVE | Future dialect | `UNIT` | **IMPLEMENTATION-PLANNED** |
| `IF-VID-007` | Registry reconciliation | AeroraIdentity | FirmwarePlugin::factGroups() | AeroraIdentityBinding | REPORTED / EFFECTIVE | Future dialect | `INTEGRATION` | **IMPLEMENTATION-PLANNED** |

## Worked example

The directive's example, traced through verified source rather than asserted:

```text
IF-FC-001  Vehicle state: attitude, position, velocity, altitude
   │
   ▼  owner
M-Vehicle  (AeroraVehicle)
   │
   ▼  seam                       src/FirmwarePlugin/FirmwarePlugin.h:353
FirmwarePlugin::factGroups()
   │
   ▼  attachment                 src/Vehicle/Vehicle.cc:364-370
Vehicle::_nameToFactGroupMap
   │
   ▼  dispatch                   src/Vehicle/Vehicle.cc:588-590
FactGroup::handleMessage(Vehicle*, const mavlink_message_t&)
   │
   ▼  protocol                   standard attitude / global position / VFR families
MAVLink, unchanged
   │
   ▼  test                       cmake/QGCTest.cmake:156, LABELS Integration
MockLink integration test, derived from VehicleTest
```

`IF-FC-001` is `IMPLEMENTATION-PLANNED` and needs **no new Aerora code at all** — upstream already
handles these families, and Aerora's only obligation is the freshness and staleness contract, which
`M-Telemetry` supplies generically. Twenty-eight of the 182 planned interfaces are of this shape:
planned, and satisfied by configuration plus a presentation rule.

## What this map does not do

| Does not | Reason |
| --- | --- |
| Name source files | File layout is an implementation decision; the module and class are the contract |
| Assign per-interface classes within a module | False precision at this stage |
| Record any result | Nothing has been built |
| Introduce an interface | All 240 are Phase 3 identifiers, referenced |
| Resolve a `CONTRACT-ONLY` interface by choosing a value | Prohibited |

# Phase 4 Traceability

The required chain, maintained end to end:

```text
Requirement → Interface → Module → QGC seam → Source file → Test → Verification evidence
```

## Chain completeness

| Link | Source | State |
| --- | --- | --- |
| Requirement → Interface | Phase 3 traceability | **Complete** — 120 of 120 Phase 2 requirements referenced |
| Interface → Module | [04-cpp-implementation-map.md](04-cpp-implementation-map.md) | **Complete** — 240 of 240 |
| Module → QGC seam | [03-qgc-extension-map.md](03-qgc-extension-map.md) | **Complete** — 12 seams, all verified present |
| QGC seam → Source file | [03-qgc-extension-map.md](03-qgc-extension-map.md) | **Complete** — file and line for every seam |
| Source file → Test | [16-test-implementation.md](16-test-implementation.md) | **Complete** — per module |
| Test → Verification evidence | — | **Empty. Nothing has been executed** |

Complete to the fifth link, empty at the sixth. That is the correct state for a phase with no build,
and the sixth column is left blank rather than filled with intentions.

## Module ownership of interfaces

Every interface has exactly one owning module; no interface has two.

| Module | Interfaces owned | Count | Implementation status |
| --- | --- | --- | --- |
| `M-Plugin` | `IF-SYS-001` to `IF-SYS-008` | 8 | 8 planned |
| `M-Vehicle` | `IF-FC` 24, `IF-PAR` 10 | 34 | 27 planned, 6 contract-only, 1 blocked |
| `M-Mavlink` | `IF-MAV-001` to `IF-MAV-028` | 28 | 22 planned, 6 contract-only |
| `M-LinkState` | `IF-RAD-001` to `IF-RAD-009` | 9 | 7 planned, 1 contract-only, 1 blocked |
| `M-Autonomy` | `IF-GPU-001` to `IF-GPU-008` | 8 | 2 planned, 6 contract-only |
| `M-Payload` | `IF-PAY-001` to `IF-PAY-015` | 15 | 6 planned, 9 contract-only |
| `M-Power` | `IF-PWR-001` to `IF-PWR-012` | 12 | 5 planned, 7 contract-only |
| `M-Identity` | `IF-VID-001` to `IF-VID-007` | 7 | 5 planned, 2 contract-only |
| `M-Authority` | `IF-AUT-001` to `IF-AUT-012` | 12 | 9 planned, 3 contract-only |
| `M-Mission` | `IF-MIS-001` to `IF-MIS-016` | 16 | 12 planned, 4 contract-only |
| `M-Telemetry` | `IF-TLM-001` to `IF-TLM-013` | 13 | 10 planned, 3 contract-only |
| `M-Security` + `M-Authz` | `IF-SEC-001` to `IF-SEC-021` | 21 | 16 planned, 3 contract-only, 2 blocked |
| `M-Audit` | `IF-LOG-001` to `IF-LOG-024` | 24 | 21 planned, 2 contract-only, 1 blocked |
| `M-Cloud` | `IF-CLD-001` to `IF-CLD-016` | 16 | 15 planned, 1 contract-only |
| `M-Ui` | `IF-UIS-001` to `IF-UIS-017` | 17 | 14 planned, 1 contract-only, 2 blocked |
| **Total** | — | **240** | **182 / 51 / 7** |

## Requirement to module

All 120 Phase 2 requirements, grouped by the module that carries them. The nine with no interface row
are carried by a module through a process or build obligation rather than a boundary.

| Requirement domain | Count | Primary module | Note |
| --- | --- | --- | --- |
| `AERORA-GCS-REQ-` | 23 | `M-Vehicle`, `M-Audit`, `M-Authority` | `GCS-REQ-009` is a build obligation |
| `AERORA-MAV-REQ-` | 6 | `M-Mavlink` | — |
| `AERORA-COMM-REQ-` | 7 | `M-LinkState` | — |
| `AERORA-MISSION-REQ-` | 8 | `M-Mission` | — |
| `AERORA-PAYLOAD-REQ-` | 8 | `M-Payload` | — |
| `AERORA-POWER-REQ-` | 2 | `M-Power` | — |
| `AERORA-GPU-REQ-` | 8 | `M-Autonomy` | — |
| `AERORA-AWS-REQ-` | 11 | `M-Cloud` | — |
| `AERORA-SEC-REQ-` | 10 | `M-Security`, `M-Authz` | — |
| `AERORA-UI-REQ-` | 7 | `M-Ui` | `UI-REQ-009` withdrawn |
| `AERORA-SAFE-REQ-` | 6 | `M-Authority` | — |
| `AERORA-BUILD-REQ-` | 9 | Build and process | Five are process obligations |
| `AERORA-COMPLY-REQ-` | 4 | `M-Audit` | — |
| `AERORA-VEH-REQ-` | 5 | `M-Identity` | — |
| `AERORA-TEST-REQ-` | 6 | Test architecture | Three are process obligations |
| **Total** | **120** | — | — |

**Phase 4 defines no new requirement identifier.**

## Phase 3.1 findings to Phase 4 treatment

| Finding | Subject | Phase 4 document | Treatment | Status |
| --- | --- | --- | --- | --- |
| **F3.1-01** | Signing-key storage has no seam | [12](12-security-implementation.md) | Exact class, API, storage path, derivation constants and coupling named; seven strategies assessed; the required core modification stated explicitly with file and function | `IF-SEC-013` **`BLOCKED`**, `G-RELEASE` |
| **F3.1-02** | Radio host presentation | [19](19-build-bootstrap.md) | **QGC capability half answered**: no whitelist, any port plus baud suffices, zero core edits. Hardware half remains `NOT-EVIDENCED`. Non-serial cost quantified at four core edits | `IF-RAD-001` **`BLOCKED`**, `G-ARCH` |
| **F3.1-03** | Audit-store integrity | [13](13-audit-implementation.md) | Requirements enumerated; QGC facilities assessed and found insufficient; **no append-only or integrity facility exists**; the required seam specified with no default implementation | `IF-LOG-020` **`BLOCKED`**, `G-RELEASE` |
| **F3.1-04** | Operator role resolution | [14](14-authorization-implementation.md) | Four abstractions specified: identity, role resolution, authorisation, command gate. Configuration boundary named. No role or permission invented | `IF-SEC-003` **`BLOCKED`**, `G-IMPL` |
| F3.1-05 | Capability presence versus availability | [05](05-qml-implementation-map.md) | Carried; `AeroraActionButton` blocked | 3 interfaces **`BLOCKED`** |
| F3.1-06 | Evidence provenance | — | Corrected in Phase 3.1; carried unchanged | Closed in 3.1 |
| F3.1-07 | Superseded alias premise in Phase 2 | [05](05-qml-implementation-map.md) | Carried; **narrowed** by the new C++-read finding | Q4-001 |
| F3.1-08 | Overlay mechanism unnamed in Phase 3 | [03](03-qgc-extension-map.md), [19](19-build-bootstrap.md) | Corrected in Phase 3.1; Phase 4 names every element with file and line | Closed |

## Seam to source file

| Seam | Source | Verified |
| --- | --- | --- |
| Overlay activation | `cmake/CustomOptions.cmake:11-14`; `CMakeLists.txt:47-54`, `:65-76`, `:402-404`; `src/CMakeLists.txt:207-208` | Yes |
| Core plugin substitution | `src/API/QGCCorePlugin.h`, `.cc:64` | Yes |
| QML engine hook | `src/API/QGCCorePlugin.h:117`, `:119-122`; `src/QGCApplication.cc:275`, `:311` | Yes |
| URL interceptor | `custom-example/src/CustomPlugin.cc:247-248`, `:273-295` | Yes |
| Firmware plugin factory | `src/FirmwarePlugin/FirmwarePluginFactory.h:9-46`, `.cc:11-31`; `FirmwarePluginManager.cc:75-102` | Yes |
| Fact group attachment | `src/FirmwarePlugin/FirmwarePlugin.h:353`; `src/Vehicle/Vehicle.cc:364-370` | Yes |
| Fact group API | `src/FactSystem/FactGroup.h:25-27`, `:48-49`, `:59-65` | Yes |
| Message dispatch | `src/Vehicle/Vehicle.cc:525-592` | Yes |
| Message veto seams | `src/Vehicle/Vehicle.cc:564-566`, `:569-571` | Yes |
| CSV writer | `src/Vehicle/Vehicle.cc:2796-2852` | Yes |
| Serial transport | `src/Comms/Serial/SerialLink.cc:36-358` | Yes |
| Board identification | `src/Comms/Serial/QGCSerialPortInfo.cc:47`, `:179-208`; `SerialAutoConnect.cc:79-99` | Yes |
| Link type enumeration | `src/Comms/LinkConfiguration.h:103-117`, `.cc:56-103` | Yes |
| Signing key store | `src/MAVLink/Signing/MAVLinkSigningKeys.h:48-158`, `.cc:148-414` | Yes |
| Factory disable options | `cmake/CustomOptions.cmake:187`, `:190` | Yes |
| Test registration | `cmake/QGCTest.cmake:140-239` | Yes |

Sixteen seam groups, every one read in the checkout. No seam was found missing.

## Identifier integrity

| Check | Result |
| --- | --- |
| Interface identifiers added | **0** |
| Interface identifiers renumbered | **0** |
| Requirement identifiers added | **0** |
| Requirements promoted | **0** |
| Inherited questions closed | **0** |
| Inherited risks closed or downgraded | **0** |
| New Phase 4 questions | 4 — `Q4-001` to `Q4-004` |
| New Phase 4 risks | 6 — `R4-001` to `R4-006` |
| MAVLink identifiers invented | **0** |
| Enumeration members invented | **0** |
| QGC source files modified | **0** |

## Verification evidence: empty, by column

Kept as a table so the emptiness is explicit rather than implied.

| Evidence class | Expected source | Recorded |
| --- | --- | --- |
| Configure result | `just configure` | **None** |
| Compile result | `just build` | **None** |
| Link result | Same | **None** |
| Application startup | Launch | **None** |
| QML resource resolution | Startup self-check | **None** |
| Plugin registration | Startup self-check | **None** |
| Upstream regression | `ctest -L Unit` | **None** |
| Existing QGC test suite | Same | **None** |
| Tool versions | `just info` | **None** |
| Any interface marked `VERIFIED` | — | **None** |

Ten evidence classes, ten empty. The reason is one measurement: 1.00 GB of free disk and no
administrator rights.

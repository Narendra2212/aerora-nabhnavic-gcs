# Implementation Architecture

How 240 contracts become code: the layering, what must exist before what, and the order in which
the blockers come off.

## Four layers, one target

| Layer | Content | Owned by |
| --- | --- | --- |
| L1 — Upstream | QGroundControl at the pinned commit | Upstream. **Not modified** |
| L2 — Product profiles | Painting, firefighting, agriculture, inspection | Aerora, per product |
| L3 — Aerora platform | 17 product-independent modules | Aerora |
| L4 — Extension surface | Core plugin, firmware plugin, fact groups, QML seams, settings | Aerora, attaching to L1 |

The dependency direction is strict:

```text
L2 product profile
   │  may depend on
   ▼
L3 Aerora platform
   │  may depend on
   ▼
L4 extension surface
   │  may depend on
   ▼
L1 upstream QGroundControl
```

Nothing flows upward. L3 contains no `#include` of an L2 header and no reference to a product name.
The test for L3 purity is in [16-test-implementation.md](16-test-implementation.md); it is the
second-product test from the capability model made executable.

### One CMake target, not a plugin system

Verified: QGC has **no runtime plugin system**. All extension is compile-time plus resource
override. Every module uses `target_sources(${CMAKE_PROJECT_NAME} PRIVATE ...)`, and the overlay
reaches the application target by exporting `CUSTOM_SOURCES` to the parent scope, consumed at
`src/CMakeLists.txt:207-208`.

| Consequence | Detail |
| --- | --- |
| Aerora C++ compiles **into the application target** | Not into an Aerora static library |
| Self-registering globals must be in that target | A static library can have them discarded by the linker with no diagnostic — `AERORA-GCS-REQ-009` |
| Aerora QML is delivered as named QML modules exported as libraries | `AERORA-UI-REQ-010` |
| The two placements are opposite and both mandatory | Putting either in the other's place fails — quietly in one direction |

That asymmetry is the single most likely first-day build mistake, and it is why the structural
overlay proof of §23 exists at all.

## Build order

Each step is gated on the one above it. Nothing below step 2 has been performed.

```text
1  Build host                         BLOCKED — disk, admin
2  Baseline QGC build, unmodified     BLOCKED by 1
3  Aerora repository structure        BLOCKED by 2
4  Overlay structural proof           BLOCKED by 3
5  Extension seam proof               BLOCKED by 4
6  Platform module skeletons          BLOCKED by 5
7  Product profile                    Not authorised yet
```

| Step | Proves | Evidence required |
| --- | --- | --- |
| 1 | A compiler, Qt, CMake, Ninja and GStreamer exist | Tool versions recorded |
| 2 | Upstream builds at the pinned commit on this host | Configure, compile, link, startup, `ctest -L Unit` |
| 3 | Upstream and Aerora are separable | Fetch-only upstream remote; recorded fork point |
| 4 | `QGC_CUSTOM_DIR` is honoured and the overlay contract is satisfied | Configure log shows the overlay detected |
| 5 | The four seams work: plugin factory, fact groups, QML override, settings | Startup self-check passes |
| 6 | Module boundaries hold | Architectural-property tests pass |

Step 2 must be **unmodified upstream**. If the baseline does not build, nothing downstream can be
attributed to Aerora, and the first Aerora commit would be debugging two problems at once.

## The structural proof is minimal by design

Step 5 proves the seams with the least code that can prove them. Each artefact is labelled
`BOOTSTRAP` and is not product implementation.

| Artefact | Proves | Size |
| --- | --- | --- |
| `<overlay>/CMakeLists.txt` | The overlay contract half that provides sources | Required by the contract |
| `<overlay>/cmake/CustomOverrides.cmake` | The other contract half; forces the factory-disable options | Required by the contract |
| `AeroraPlugin` — `QGCCorePlugin` subclass | `CUSTOMCLASS` substitution; the engine hook; the URL interceptor | One class |
| `AeroraFirmwarePluginFactory` + `AeroraFirmwarePlugin` | Self-registration, selection, `factGroups()` | Two classes, no behaviour |
| One Aerora fact group with one Fact and one metadata entry | Fact group attachment, metadata loading, QML visibility, CSV inclusion | One class, one JSON file |
| One Aerora QML module with one control | Module registration and import path | One QML file |
| One tier-2 seam override | The interceptor resolves overlay content | One QML file |
| Startup self-check | All of the above, in a deployed build | One function |

Eight artefacts. None of them commands an aircraft, defines an enumeration, or implements a product
feature. Together they discharge every structural assumption the architecture rests on.

## What the implementation inherits unchanged

Counted from the Phase 3 evidence distribution: 89 interfaces rest on `GCS-VERIFIED` upstream
behaviour. Those are not implementation work; they are behaviour Aerora must not break.

| Inherited unchanged | Interfaces |
| --- | --- |
| Serial transport, port enumeration, link lifecycle | `IF-RAD` set |
| Standard MAVLink families, command protocol, acknowledgement matching | `IF-MAV-001` to `IF-MAV-011` |
| Parameter protocol, caching, metadata, bulk transfer | `IF-PAR` set |
| Mission protocol, plan file authority, complex item pattern | `IF-MIS` set |
| MAVLink 2 signing state machine, key auto-detection, timestamp persistence | `IF-SEC-014` to `IF-SEC-018` |
| Fact system, fact groups, metadata-driven enumerations | `IF-TLM-008`, `IF-TLM-009` |
| Diagnostic logging, MAVLink stream logging | `IF-LOG-001`, `IF-LOG-002` |

The implementation posture toward all of it is the same: **use it, do not wrap it, do not
reimplement it**. Where Aerora adds a rule on top — a settings default, a presentation constraint, a
record obligation — the rule is additive and the upstream mechanism is untouched.

## Where implementation genuinely begins

Of 240 interfaces, the ones that are new Aerora code:

| Module | New interfaces | Nature |
| --- | --- | --- |
| `AeroraAudit` | 24 | A record writer, correlation, local-first ordering. Largest single body of new code |
| `AeroraSecurity` | 21 | Identity, authorisation gate, key provisioning procedure |
| `AeroraUi` | 17 | State-class rendering, seam content, self-check |
| `AeroraCloud` | 16 | Adapters, bounded queues, per-feature degradation |
| `AeroraAuthority` | 12 | Eight-state command tracker, corroboration, staleness |
| `AeroraTelemetry` | 13 | Fact group base, freshness stamping, staleness evaluation |
| `AeroraPower` | 12 | State abstraction, no arbitration |
| `AeroraPayload` | 15 | Abstraction boundary, no actuator behaviour |

`AeroraAudit` being the largest is the correct shape. It is the module with the fewest external
dependencies, the clearest contract, and no need for a dialect — so it is also the first module that
can be built after the bootstrap, and the natural pilot for the module conventions.

## Threading

Carried from Phase 2 without change, and stated here because it constrains every module.

| Thread | Owns |
| --- | --- |
| Main | Qt object graph, Fact updates, QML, the command path |
| Aerora log writer | Record serialisation and local disk writes |
| Aerora cloud | Network I/O, upload queue, synchronisation state machine |

| Rule | Reason |
| --- | --- |
| No synchronous network call originates on the main thread | A stalled name resolution would freeze the operator interface |
| The log writer never blocks the main thread | A full disk degrades the record, not the interface |
| Cross-thread communication is by queued signal only | No shared mutable state between the three |

## Error handling posture

| Situation | Response |
| --- | --- |
| A value is not reported | Present `UNKNOWN`. Never zero, never a default |
| A capability is not reported | Present nothing. Never a disabled control implying it exists |
| A command is not acknowledged | Retry to the protocol limit, then `FAILED`. Never silently |
| A command is acknowledged but not corroborated | `STALE`, surfaced, never retried |
| A record cannot be written | Surface it and account for the drop. Never continue silently unrecorded |
| An override does not resolve | Visible startup failure. Never a partial interface |

Six rules, and five of them are refusals. That ratio is the architecture.

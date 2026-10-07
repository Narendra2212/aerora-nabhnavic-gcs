# FactGroup Implementation

Six Aerora fact groups, their concrete construction, and a multi-vehicle hazard in the chosen
attachment seam that Phase 4 found and Phases 1 to 3 did not.

## ⚠ Finding: the `factGroups()` seam shares objects across vehicles

This is a §28 "architecture differs materially from earlier findings" condition and is reported as
such.

### What the code does

| Step | Location | Behaviour |
| --- | --- | --- |
| 1 | `src/FirmwarePlugin/FirmwarePluginManager.cc:75-89` | A factory returns a **cached singleton** `FirmwarePlugin`. The reference overlay caches `_pluginInstance`; the generic fallback caches `_genericFirmwarePlugin` |
| 2 | `src/FirmwarePlugin/FirmwarePluginFactory.h:16-19` | The header documents the return as a "Singleton FirmwarePlugin instance" |
| 3 | `src/Vehicle/Vehicle.cc:229` | Each `Vehicle` obtains that **shared** plugin |
| 4 | `src/FirmwarePlugin/APM/ArduSubFirmwarePlugin.cc:281-284` | The only in-tree override returns `&_nameToFactGroupMap` — **a member of the plugin** |
| 5 | `src/Vehicle/Vehicle.cc:364-370` | Each `Vehicle` inserts those groups into **its own** `_nameToFactGroupMap` |
| 6 | `src/Vehicle/Vehicle.cc:588-590` | Each `Vehicle`'s dispatch loop calls `handleMessage` on every group in its map |

### The consequence

```text
Vehicle A ──┐
            ├──►  shared FirmwarePlugin  ──►  one set of FactGroup objects
Vehicle B ──┘                                         ▲         ▲
                                                      │         │
            A's dispatch loop writes here ────────────┘         │
            B's dispatch loop writes here ──────────────────────┘
```

Two vehicles of the same firmware class write into the **same** fact group objects. Vehicle B's
telemetry overwrites Vehicle A's, and the operator sees one aircraft's values attributed to another.

| Property | Assessment |
| --- | --- |
| Does the hazard exist upstream today? | **Yes** — `ArduSubFirmwarePlugin` has the same shape |
| Why has nobody hit it? | ArduSub is operated single-vehicle in practice |
| Does it affect Aerora? | **Yes.** `AERORA-GCS-REQ-010` mandates this exact seam, and multi-vehicle is an architectural invariant |
| Is it a correctness or a presentation problem? | **Correctness.** Wrong values, attributed confidently |
| Does Phase 2's ADR-001 depend on this seam? | Yes — it is the named zero-core-edit attachment route |

### Options, with costs

| Option | Core edit? | Assessment |
| --- | --- | --- |
| **A.** Return a per-vehicle map from `factGroups()` | No | The signature takes no `Vehicle*`, so the plugin cannot tell which vehicle is asking. **Not possible as the seam stands** |
| **B.** Construct Aerora groups in the plugin, one set per vehicle, keyed internally | No | Requires knowing the calling vehicle. Same obstacle as A |
| **C.** Upstream contribution: change the signature to `factGroups(Vehicle*)` | Upstream | **Preferred.** Small, obviously correct, fixes ArduSub too |
| **D.** Aerora core edit to `Vehicle.cc:364-370` to construct Aerora groups per vehicle | **Yes** | Honest fallback; must be recorded as a real core edit |
| **E.** Attach Aerora groups from `AeroraPlugin` rather than the firmware plugin | Unknown | `QGCCorePlugin` has no fact-group hook. Needs investigation |
| **F.** Accept single-vehicle operation | No | Contradicts an architectural invariant. **Rejected** |

**Phase 4 selects neither C nor D unilaterally.** Both change the standing core-edit position, which
is an architecture decision recorded in ADR-001 and ADR-008 and belongs to the reviewer. Recorded as
**Q4-002** and **R4-001**.

Until it is settled, every Aerora fact group is `CONTRACT-ONLY` for multi-vehicle operation and
`IMPLEMENTATION-PLANNED` for single-vehicle operation, and the distinction is stated in each row
below.

### What Phase 4 does not do

It does not quietly pick option D and describe it as "the overlay". That is precisely the behaviour
the directive prohibits for F3.1-01, and the same standard applies here.

## The six fact groups

Names carried from Phase 2. **No Fact names, units, ranges or enumeration members are specified** —
those follow the dialect, which does not exist.

| Group | Class | Owner | Subject | Protocol binding | Status |
| --- | --- | --- | --- | --- | --- |
| `aeroraSystem` | `AeroraSystemFactGroup` | `M-Vehicle` | Authority mode, arming, watchdog state, boot state, version inventory | **OPEN** — `IF-MAV-012` candidate | `CONTRACT-ONLY` |
| `aeroraPayload` | `AeroraPayloadFactGroup` | `M-Payload` | Payload identity, capabilities, health, safety state | **OPEN** — `IF-MAV-014` candidate | `CONTRACT-ONLY` |
| `aeroraPaint` | `AeroraPaintFactGroup` | `M-ProfilePainting` (L2) | Product telemetry — six value names, names only | **OPEN** — `IF-MAV-015` candidate | `CONTRACT-ONLY` |
| `aeroraPower` | `AeroraPowerFactGroup` | `M-Power` | Source, tether state, converter state, battery state, energy | **OPEN** — `IF-MAV-012` candidate | `CONTRACT-ONLY` |
| `aeroraLink` | `AeroraLinkFactGroup` | `M-LinkState` | Per-link validity, freshness, quality, vehicle-reported active link | **OPEN** — `IF-MAV-012` candidate | `CONTRACT-ONLY` |
| `aeroraAutonomy` | `AeroraAutonomyFactGroup` | `M-Autonomy` | Companion proposals, perception state, acceptance outcome | **OPEN** — `IF-MAV-018` candidate | `CONTRACT-ONLY` |

All six are `CONTRACT-ONLY` because none has a message to parse. The **class** is implementable
today; the `handleMessage` body is not. That distinction is the whole point of building the
abstraction before the protocol.

## Per-group implementation contract

Identical for all six, which is why a shared base class exists.

| Aspect | Decision | Evidence |
| --- | --- | --- |
| Base class | `AeroraFactGroupBase : public FactGroup` | `src/FactSystem/FactGroup.h:25-27` |
| Constructor | The `metaDataFile` overload, with a `:/json/Aerora/<Name>Fact.json` path | Convention at `src/Vehicle/FactGroups/VehicleGPSFactGroup.cc:13` |
| Metadata registration | `qgc_add_json_resources(... PREFIX "/json" ...)` from the overlay | `src/Comms/Serial/CMakeLists.txt:4-7` |
| Update rate | `0` — immediate `valueChanged`, so freshness is never coalesced away | `FactGroup.cc` `_setupTimer` |
| Fact declaration | Members, registered with `_addFact(Fact*, name)` in the constructor | `FactGroup.h:59-62` |
| Creation time | **During vehicle construction only** | `AERORA-GCS-REQ-022`; R-027 |
| Dispatch | Override `handleMessage(Vehicle*, const mavlink_message_t&)` | `FactGroup.h:48-49` |
| Freshness | Stamp receive time at parse, in `handleMessage`, before any consumer sees the value | `AERORA-GCS-REQ-027` |
| Staleness | Per-class threshold, evaluated by `M-Telemetry`, not per group | `IF-TLM-011` |
| Unknown value | Fact absent or explicitly unknown. **Never defaulted to zero** | `IF-UIS-005` |
| Capability gating | The group exists; its **presentation** is gated by `M-Capability` | `IF-PAY-002`, `IF-PWR-012` |
| QML exposure | Through the vehicle's fact-group map, as for every upstream group | `FactGroup.h:43` |
| Telemetry availability | `_setTelemetryAvailable(true)` on first successful parse | `FactGroup.h:65` |

### Why update rate 0 rather than a coalescing timer

A coalescing timer batches `valueChanged` signals. For a value whose **freshness is part of its
meaning**, batching moves the apparent receive time. Upstream groups that use a non-zero rate are
display conveniences; Aerora values feed the audit record, so the timestamp must be the parse
timestamp.

## The CSV constraint

| Fact | Location |
| --- | --- |
| The header is enumerated once at file open | `Vehicle.cc:2796-2821`, written at `:2820` |
| Each row re-enumerates the same structures | `Vehicle.cc:2823-2852`, written at `:2851` |
| A group added after the header is written shifts every later column | R-027 |

Aerora's six groups are created during vehicle construction, so they exist before the first row.
**Aerora adds no group dynamically** — `AERORA-GCS-REQ-022`, and the architectural-property test
asserts it.

Free consequence worth naming: because the groups are attached to the vehicle, every Aerora Fact
appears in the telemetry CSV with **no Aerora logging code**. That is the third benefit of the
attachment route, and it survives whichever option resolves Q4-002.

## Test strategy per group

| Test | Method | Asserts |
| --- | --- | --- |
| Construction | `UNIT` | The group exists after vehicle construction, with its metadata bound |
| Metadata completeness | `UNIT` | Every registered Fact has a metadata entry; a Fact without one is not presented |
| No dynamic creation | `UNIT` | No fact group is added after construction — the existing property test |
| Freshness stamping | `UNIT` | A value reaching a consumer without a receive time is a detectable defect |
| Staleness | `UNIT` | A value past its class threshold is marked, with its age |
| Unknown handling | `UNIT` | An absent value never renders as zero |
| Message parse | `INTEGRATION` | **Deferred** — needs a dialect |
| Multi-vehicle isolation | `INTEGRATION` | **Two MockLink vehicles must not share values.** This is the test that would have caught the hazard above |
| CSV column alignment | `INTEGRATION` | Header and row cardinality match after a full connection cycle |

The multi-vehicle isolation test is the highest-value new test in Phase 4, and it is writable before
the dialect exists: attach two MockLink vehicles, set a Fact through one, assert the other is
unchanged.

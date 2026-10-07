# Phase 4 Open Questions

Four questions raised by mapping contracts onto source. Each is a question that only appears once you
try to write the code.

**Phase 4 closes no question.** The 43 inherited questions plus the 9 Phase 3 questions are
referenced, never redefined. Q-020 remains partially resolved as Phase 3.1 recorded it.

## Classification

Carried unchanged.

| Class | Meaning |
| --- | --- |
| `IMPL-BLOCKING` | Implementation of the affected interface cannot begin until it is answered |
| `ARCH-SHAPING` | The interface exists; the answer changes its content or its owner |
| `DEFERRABLE` | Can be answered after implementation starts without rework |

## Summary

| ID | Subject | Class | Owner | Gate |
| --- | --- | --- | --- | --- |
| Q4-001 | Precedence of an overlay resource registered at a core `:/json` path | `IMPL-BLOCKING` | Build, by observation | `G-IMPL` |
| Q4-002 | How Aerora fact groups are attached per vehicle | `ARCH-SHAPING` | Architecture, then upstream | **`G-ARCH`** |
| Q4-003 | Whether audit integrity requires a key, and where it lives | `ARCH-SHAPING` | Security | `G-RELEASE` |
| Q4-004 | Consequences of disabling both vendor firmware factories | `DEFERRABLE` | Product | `G-INTEG` |

Q4-002 is at `G-ARCH` because the answer may change the standing core-edit position, which is an
architecture decision recorded in ADR-001.

---

### Q4-001

**Question.** When the overlay registers a resource at the **same** `:/json` path as a core resource,
which one does a C++ `QFile(":/json/...")` read resolve?

**Why it is being asked.** Phase 3.1 established that QML override resolution is a deterministic
existence check performed by a URL interceptor, not an alias contest. That correction is sound and
covers the QML engine. It does **not** cover C++ resource reads:

| Consumer | Path form | Interceptor applies? |
| --- | --- | --- |
| QML engine loading a component | `qrc:/qml/...` | **Yes** |
| `src/Comms/Serial/QGCSerialPortInfo.cc:47` board table | `":/json/USBBoardInfo.json"` | **No** |
| `FactGroup` metadata load | `":/json/Vehicle/<Name>Fact.json"` | **No** |

The reference overlay registers its own JSON at `PREFIX "/json"`
(`custom-example/CMakeLists.txt:121-135`) — the same prefix as core, not a `/Custom` mirror. So for
C++-read resources, two resources at one path is a real possibility and the precedence is unverified.

**Affected interfaces.** `IF-UIS-014`, and transitively every fact-group metadata file.

**Affected requirements.** `AERORA-UI-REQ-007`, `AERORA-GCS-REQ-026`.

**Affected risks.** R-019; new risk R4-005.

**Classification.** `IMPL-BLOCKING` for any override of an existing core JSON file. **Not** blocking
for Aerora's own metadata files, which use new names and therefore cannot collide.

**Decision owner.** Build, by observation. This is not a design question.

**Evidence required.** Register a probe resource at an existing core `:/json` path in the overlay,
build, and read it from C++. One build answers it.

**What Phase 4 does in the meantime.** Aerora metadata files use **new names only**. Overriding
`USBBoardInfo.json` was considered as an F3.1-02 mitigation and **rejected** for exactly this reason;
the chosen mitigation is a settings default instead. The startup self-check must assert resource
resolution for the QML and C++ cases **separately**, not once.

---

### Q4-002

**Question.** How should Aerora fact groups be attached to a vehicle, given that
`FirmwarePlugin::factGroups()` returns a map owned by a plugin that is shared across all vehicles of
a firmware class?

**Why it is being asked.** Phase 4 traced the attachment route and found a hazard Phases 1 to 3 did
not record:

| Step | Location |
| --- | --- |
| The factory returns a cached singleton plugin | `FirmwarePluginManager.cc:75-89` |
| Each vehicle obtains that shared plugin | `Vehicle.cc:229` |
| The only in-tree override returns a **plugin member** map | `ArduSubFirmwarePlugin.cc:281-284` |
| Each vehicle inserts those groups into **its own** map | `Vehicle.cc:364-370` |
| Each vehicle's dispatch loop writes into them | `Vehicle.cc:588-590` |

Two vehicles of the same firmware class therefore write into the same `FactGroup` objects. One
aircraft's telemetry is attributed to another. The hazard exists upstream today and has gone
unnoticed because `ArduSub` is operated single-vehicle.

`AERORA-GCS-REQ-010` mandates this exact seam, and multi-vehicle support is an architectural
invariant. The two cannot both hold as the seam stands.

**Affected interfaces.** Every Aerora fact-group interface: the `IF-TLM`, `IF-PAY`, `IF-PWR`,
`IF-GPU` and `IF-VID` sets, and `IF-FC` where it depends on an Aerora group.

**Affected requirements.** `AERORA-GCS-REQ-010`, `AERORA-GCS-REQ-022`.

**Affected risks.** New risk R4-001, at `HIGH`.

**Classification.** `ARCH-SHAPING`. Single-vehicle operation is unaffected; multi-vehicle is not.

**Decision owner.** Architecture, then upstream if the signature change is pursued.

**Evidence required.** None external — the behaviour is `GCS-VERIFIED`. What is needed is a
**decision** between:

| Option | Core edit? |
| --- | --- |
| Upstream contribution changing the signature to `factGroups(Vehicle*)` | Upstream, not Aerora. **Preferred** — it fixes `ArduSub` too |
| An Aerora core edit to `Vehicle.cc:364-370` | **Yes.** Honest fallback, must be recorded |
| Investigate whether `QGCCorePlugin` offers a per-vehicle attachment point | Unknown |
| Accept single-vehicle operation | No. **Contradicts an invariant** |

**What Phase 4 does in the meantime.** Every Aerora fact group is `IMPLEMENTATION-PLANNED` for
single-vehicle and `CONTRACT-ONLY` for multi-vehicle, and the distinction is stated per group. The
multi-vehicle isolation property test is specified so the hazard cannot return silently.

---

### Q4-003

**Question.** Does the audit-store integrity mechanism require a key, and if so, where is that key
stored given that F3.1-01 leaves key storage unresolved?

**Why it is being asked.** `AERORA-GCS-REQ-028` requires the local audit store to be
integrity-protected, and Q-037 leaves the mechanism open. Phase 4 found that the two findings are
coupled:

| Mechanism class | Defends against | Needs a key? |
| --- | --- | --- |
| Unkeyed digest chain | Accidental corruption, and nothing else | No |
| Keyed chain or signature | **Repudiation** — an operator editing their own record | **Yes** |

The requirement's rationale is "an audit record that can be edited is not evidence", which is a
repudiation concern. That points at a keyed mechanism, which needs a key the station cannot currently
protect — the exact gap F3.1-01 describes.

A further constraint: "a compromised operator host" is explicitly **out of scope**, so the mechanism
cannot assume the host is trustworthy and must not pretend to defend against host compromise.

**Affected interfaces.** `IF-LOG-020`, `IF-LOG-024`, and transitively `IF-SEC-013`.

**Affected requirements.** `AERORA-GCS-REQ-028`, `AERORA-SEC-REQ-017`.

**Affected risks.** New risk R4-002.

**Classification.** `ARCH-SHAPING`.

**Decision owner.** Security.

**Evidence required.** None external. A decision on which threat the mechanism addresses, which then
determines whether F3.1-01 must be resolved first.

**What Phase 4 does in the meantime.** `IF-LOG-020` is **`BLOCKED`**. The seam is specified — an
abstract integrity interface, called on the write path, with the material exported alongside the
records — and **no default implementation is shipped**. The writer refuses to start without an
injected implementation, so the gap is loud at the first build rather than silent at the first audit.

---

### Q4-004

**Question.** What are the consequences of forcing both `QGC_DISABLE_APM_PLUGIN_FACTORY` and
`QGC_DISABLE_PX4_PLUGIN_FACTORY` on, beyond preventing the first-match selection hazard?

**Why it is being asked.** Disabling both is the chosen mitigation for R-010 and it works: with no
vendor factory registered, only Aerora's remains. But it has effects beyond selection:

| Effect | Location | Assessment |
| --- | --- | --- |
| ArduPilot and PX4 leave `supportedFirmwareClasses()` | `FirmwarePluginManager.cc:28-38` | Intended |
| The setup UI prunes unsupported classes | `src/Settings/AppSettings.cc:64-68` | Intended |
| Offline editing re-selects a plugin from a stored firmware type | `Vehicle.cc:483` | **Unexamined.** What happens to a stored plan whose firmware type is now unsupported? |
| The plugin implementations are still compiled | `PX4/CMakeLists.txt:8-15` | Only the factory translation unit is guarded |
| An Aerora build cannot connect to a PX4 vehicle as PX4 | Runtime | Intended, and irreversible without a rebuild |

The third row is the open part. A plan file carrying a firmware type Aerora no longer supports is a
plausible operational case — a plan authored before an Aerora build was deployed — and the behaviour
is not established.

**Affected interfaces.** `IF-MIS-001` plan authority, `IF-MAV-021` autopilot identifier.

**Affected requirements.** `AERORA-GCS-REQ-007`, `AERORA-GCS-REQ-008`.

**Affected risks.** New risk R4-006, informational.

**Classification.** `DEFERRABLE`. It affects an edge case, not the build or the architecture.

**Decision owner.** Product, for whether a single-stack build is the intent; Q-017 already asks that.

**Evidence required.** One observation once a build exists: open an offline plan with an unsupported
firmware type and record what happens.

---

## What these questions do not do

| They do not | Because |
| --- | --- |
| Close or reopen any inherited question | Phase 4 references, never redefines |
| Introduce a requirement | Phase 4 defines no requirement identifiers |
| Resolve Q4-002 by choosing a core edit | That changes ADR-001's standing position |
| Block the bootstrap | All four are downstream of a build host, which is the actual blocker |

## Fallback behaviour while each is open

| Question | Behaviour in the meantime |
| --- | --- |
| Q4-001 | Aerora metadata uses new filenames only; no core JSON is overridden; the self-check asserts both resource cases separately |
| Q4-002 | Single-vehicle implementation proceeds; multi-vehicle is `CONTRACT-ONLY`; the isolation test is specified |
| Q4-003 | `IF-LOG-020` blocked; the writer refuses to start without an integrity implementation |
| Q4-004 | Both factories disabled; the offline-editing edge case recorded and untested |

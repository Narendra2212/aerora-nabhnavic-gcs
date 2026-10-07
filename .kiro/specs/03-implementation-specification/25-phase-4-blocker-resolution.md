# Phase 4 — Blocker Resolution

Evidence-first pass over the Phase 4 blockers. Every item below is either a measurement taken on this
host, a reading of pinned QGC source at `9459a4b5`, or an explicit statement that the evidence does
not exist yet.

Nothing here is a product implementation. No QGC source file was modified. No requirement, interface,
MAVLink identifier, enumeration, role name or USB identifier was invented.

| Field | Value |
| --- | --- |
| Phase | 4, blocker-resolution pass |
| QGC HEAD | `9459a4b57986f40df8bdc193d5453c0596fbb416` |
| `git describe` | `v5.2.0-dev-102-g9459a4b57` |
| QGC source diff | **0** |
| Observation date | 2026-10-06 |
| New requirements | **0** |
| New interfaces | **0** |

---

## A. Radio Observation

**Result: no Aerora radio is attached to this host.** The observation was taken, it succeeded, and
its outcome is negative. This is a recorded measurement, not an absence of effort.

Method: read-only enumeration. No port was opened, no baud rate was set, no device state was changed.
Tooling is [`.kiro/tools/aerora-radio-observe.ps1`](../../tools/aerora-radio-observe.ps1).

### Observed serial state

| Source | Observation |
| --- | --- |
| `HKLM\HARDWARE\DEVICEMAP\SERIALCOMM` | **Key absent** — no serial endpoint is registered at all |
| `[System.IO.Ports.SerialPort]::GetPortNames()` | `<none>` |
| `Win32_SerialPort` | count **0** |
| `Win32_PnPEntity` matching `(COM<n>)` | count **0** |
| PnP class `Ports` | exactly **1** device: `ECP Printer Port (LPT1)`, `ACPI\PNP0401\5`, Microsoft driver `10.0.19041.1` |

The single `Ports`-class device is an ACPI legacy parallel port on the mainboard. It is not USB, not
serial, and not related to any radio.

### Observed USB state

Seven present USB VID/PID pairs, all attributable to host-internal or unrelated hardware:

| VID:PID | Class | Name as enumerated | Attribution |
| --- | --- | --- | --- |
| `0BDA:5686` | Camera, USB | `Integrated Webcam`, bus-reported `Integrated_Webcam_HD` | Host-internal webcam |
| `0A5C:5832` | USB, SmartCardReader, CVAULT | `Dell ControlVault w/o Fingerprint Sensor` | Host-internal composite security device |
| `0CF3:E007` | Bluetooth | `Qualcomm QCA61x4A Bluetooth` | Host-internal radio, paired with the WLAN part |
| `22B8:2E82` | WPD | `moto g34 5G` | A phone on MTP. Unrelated to the programme |

No USB-to-serial bridge of any kind is present: no CP210x, no FTDI, no CH340, no CDC-ACM interface,
no device presenting as a SiK radio.

### What was deliberately not done

| Not done | Why |
| --- | --- |
| Infer VID/PID from the radio module | The SX1262/E22 is the radio IC. Its silicon identity is **not** necessarily the USB identity. Inferring it would fabricate the exact value Q3-001 asks for |
| Infer a COM port number | None exists on this host |
| Infer baud capability | Not discoverable without a device, and not discoverable at all without changing device state |
| Treat the phone as a candidate | It enumerates as WPD and is unrelated |

### Baseline value of a negative result

The host now has a recorded **zero-serial, four-known-VID baseline**. The tool tags anything outside
that set as `[NEW]`. When the radio is attached, one re-run isolates it by pure difference, with no
guesswork about which device is which. The bench step is now a single command whose output can be
pasted verbatim.

---

## B. Q3-001 Resolution

**Status: `OPEN`.** Unchanged, and it cannot be changed from this host.

| Aspect | State |
| --- | --- |
| Whether the Aerora radio presents as a serial device | **Unknown** — no device observed |
| Device class | **Unknown** |
| USB VID / PID | **Unknown** |
| Friendly name, manufacturer, driver, driver provider, driver version | **Unknown** |
| Hardware IDs, compatible IDs, serial descriptor | **Unknown** |
| COM port assignment | **Unknown** |
| Baud capability | **Unknown** |
| Host observation procedure | **Known and now tooled** |
| This host's no-radio baseline | **Known and recorded** |

What changed is only the cost of answering it. The remaining input is physical access to the radio.

### QGC half of the question, already settled in Phase 4

Recorded here for the reviewer's convenience; this is unchanged, not new.

| Finding | Evidence |
| --- | --- |
| `SerialLink` has no device whitelist. Any port name and baud works | `src/Comms/Serial/SerialLink.cc:177-189` |
| The only refusal path is the bootloader flag | `SerialLink.cc:179-184` |
| A serial radio therefore costs **zero** core edits | Same |
| A non-serial transport costs a **minimum of four** core edits | See the table below — all four re-verified this pass |

The four edits a non-serial transport would require, re-verified against pinned source:

| # | Edit site | Evidence |
| --- | --- | --- |
| 1 | `LinkType` enum, closed and terminated by `TypeLast` | `src/Comms/LinkConfiguration.h:103-117` |
| 2 | Link construction switch | `src/Comms/LinkManager.cc:147-182` |
| 3 | Configuration construction switch | `src/Comms/LinkManager.cc:418-443` |
| 4 | `LinkManager::linkTypeStrings()` | `src/Comms/LinkManager.cc:568`, whose comment at `:571` requires the same order as the enum |

There is no registration hook: the enum is closed and both switches are exhaustive, so a new
transport cannot be added from an overlay.

So the decision tree's branch cost is known precisely. Only the branch selector is missing.

---

## C. ADR-002 Decision

**Status: `OPEN`.** Not retained, not modified, not withdrawn.

The directive offers retain / modify / withdraw, and the honest answer is that none of the three is
available. ADR-002 rests on the premise that the Aerora radio presents as a serial device. That
premise is exactly what section A failed to observe. Declaring "retain" would assert the premise
without evidence; declaring "withdraw" would discard it without evidence.

| Decision-tree case | Would mean | Can it be selected? |
| --- | --- | --- |
| **Case A** — standard serial device | Retain `SerialLink`, mark ADR-002 accepted, withdraw the six deferred transport core edits | **No.** No serial endpoint observed |
| **Case B** — serial but discovery/reservation collides | Configuration-only mitigation, ADR-002 holds with a caveat | **No.** Requires an enumerated device to collide |
| **Case C** — non-serial | ADR-002 invalid, six transport core edits return | **No.** Requires an observed non-serial interface |

**The six deferred transport core edits therefore remain deferred and cannot be withdrawn.** The
directive's conditional — "remove/withdraw the six deferred transport-core edits if the evidence
supports doing so" — is not satisfied, so they stay.

No Phase 4 implementation-map row changed as a result of this pass. `IF-RAD-001` remains `BLOCKED` at
`G-ARCH`, and the seven `IF-RAD` interfaces remain `IMPLEMENTATION-PLANNED` as inherited serial
behaviour.

---

## D. F3.1-02 Impact

**Status: `OPEN`. The transport-core concern remains live.**

F3.1-02 states that radio host presentation cannot be isolated behind an abstraction, because the
abstraction itself is the thing that costs core edits. This pass does not change that.

| Element | State after this pass |
| --- | --- |
| Can the concern be isolated in the overlay? | **No.** Unchanged |
| Is the mitigation a design? | **No.** It is still a one-connection observation |
| Primary mitigation on record | Disable SiK serial auto-connect in the Aerora settings defaults — entirely ground-station-side, needs no identifier |
| Is that mitigation still correct? | **Yes**, and it is still the right call precisely because it requires no identifier |
| Residual exposure | If the radio is non-serial, four-plus core edits return and `AERORA-COMM-REQ-006` changes shape |

The settings-default mitigation is unaffected by the missing observation, which is why it was chosen
over overriding `USBBoardInfo.json`. That reasoning holds.

---

## E. Four Reviewer Decisions

None of these are closed by this pass. Each is recorded with the evidence that now exists, so the
decision is cheaper, not pre-empted.

| Decision | Current State | Required Evidence/Decision | Owner | Blocks |
| --- | --- | --- | --- | --- |
| **Q4-002** — how Aerora fact groups attach per vehicle | `OPEN` | A choice between an upstream signature change to `FirmwarePlugin::factGroups()` and a recorded Aerora core edit. No new evidence needed; the mechanism is fully traced | Architecture, then upstream | `G-ARCH`. R4-001 at `HIGH`. May change ADR-001's zero-core-edit position |
| **F3.1-01** — signing-key storage at rest | `OPEN` | A route decision: upstream contribution, recorded Aerora core edit, or accept the current exposure. Engineering detail is complete — section 9 below | Security, then upstream | `G-RELEASE`. `IF-SEC-013` `BLOCKED`. Q4-003 depends on it |
| **Q-029** — operator role set | `OPEN` | A product and operations decision, with likely regulatory input. No candidate role set is recorded anywhere, deliberately | Product, then external | `G-IMPL`. `IF-SEC-003` `BLOCKED`, `AERORA-SEC-REQ-013`. Without it the station is correct and unusable |
| **Q-037 / Q4-003** — which threat audit integrity addresses | `OPEN` | A decision on whether the mechanism defends against accidental corruption or against repudiation. That choice determines whether a key is needed, and therefore whether F3.1-01 must be resolved first | Security | `G-RELEASE`. `IF-LOG-020` `BLOCKED`, `AERORA-GCS-REQ-028`. Coupled to F3.1-01 via R4-002 |

No abstraction in the Phase 4 document set closes any of these. Where an abstraction exists it is
recorded as a seam with the decision still outstanding, and for audit integrity no default
implementation is shipped at all.

---

### F3.1-01 — signing-key storage, engineering detail

Read from pinned source. **Nothing was modified.** The purpose is to make the reviewer decision easy.

| Item | Finding |
| --- | --- |
| Source files | `src/MAVLink/Signing/MAVLinkSigningKeys.h`, `.cc` |
| Classes | `MAVLinkSigningKey` (one entry, `QML_ELEMENT`, `QML_UNCREATABLE`), `MAVLinkSigningKeys` (the store, `QML_ANONYMOUS`) |
| Instancing | `Q_APPLICATION_STATIC` at `.cc:38`, `instance()` at `.cc:39` |
| Virtual members | **None** except the destructor. There is no overridable storage API |
| Only access seam | `friend class SigningTest` in the header |
| Storage API | `QSettings`, default-constructed **inside** `_save()` and `_load()` |
| Settings layout | Group `MAVLinkSigningKeys`; `manifest` = key-name `QStringList` (no secrets); `keys/<name>` = raw bytes; `timestamps/<name>` = `quint64` |
| Serialization | Raw 32 bytes, `reinterpret_cast<const char*>(bytes.data())` into a `QByteArray`, written verbatim. No encryption, no OS keystore |
| Write path | `_save()` `.cc:248-278`. Writes every key, prunes names dropped since the previous manifest, writes the manifest, explicit `sync()`. `QGC::secureZero` on the local copy |
| Read path | `_load()` `.cc:399`. Reads the manifest, then `keys/<name>` per entry via `MAVLinkSigning::makeSigningKey`, then the timestamp. `secureZero` on each buffer |
| Deletion path | `removeKey()` `.cc:221`, `removeAllKeys()` `.cc:236`. Both mutate in memory then call `_save()`, which performs the actual settings removal |
| Key selection | `tryDetectKey()` `.cc:346` — trial-matches **every** stored key against an incoming signed message, then configures the channel. Selection is by brute-force trial, not configuration |
| Derivation | `addKey()` `.cc:147`. PBKDF2-HMAC-SHA256, 600,000 iterations, 32-byte output, **fixed app-wide salt** `"QGroundControl-MAVLink-Signing-v1"` |
| Raw import | `addRawKey()` `.cc:174`, exactly 32 bytes from hex |
| Export path | `keyHexByName()` `.cc:206`, `Q_INVOKABLE` — returns plaintext key hex to QML |
| Capacity | `kMaxKeys` 64; minimum passphrase length 8 |
| Concurrency | No cross-process lock, by design; the comment at `.cc:251` relies on `RunGuard` single-instance |

**Security consequences, as observed:**

- Keys sit at rest as raw bytes in `QSettings`, which on Windows is the user's registry hive. No
  DPAPI, no CNG, no platform keystore, no encryption.
- The salt is a compile-time constant and derivation is deterministic by design, so the same
  passphrase yields the same key on every install. That is an intentional interoperability property,
  but it means derivation contributes no per-install entropy and a precomputed dictionary is portable
  across all QGC installations.
- `keyHexByName()` is invokable from QML, so a plaintext export surface exists above the storage layer.
- In-memory hygiene is careful and consistent — `secureZero` on passphrase, derived buffer, serialized
  copy and load buffer. **The gap is specifically at rest, not in memory.** Any fix should preserve
  the existing in-memory discipline.

**Rotation implications:**

- Trial-matching in `tryDetectKey()` means several keys coexist and the right one is auto-selected, so
  add-new-then-retire-old rotation works without a flag day.
- Per-key forward-progress timestamps are removed with the key by `_save()`. Re-adding the same name
  restarts its timestamp at zero, which bears on replay protection.
- Changing the salt's version suffix invalidates every passphrase-derived key simultaneously. That is
  a flag day, not a rotation, and the header comment says to coordinate it with deployment.

**Seam options, with blast radius:**

| Option | Core edit? | Assessment |
| --- | --- | --- |
| Subclass and override | — | **Impossible.** No virtuals |
| Use `friend class SigningTest` | No | **Rejected.** A production path through a test seam is worse than a core edit, and dishonest |
| Register a custom `QSettings` format app-wide | No | Technically available, but it changes persistence for **all** QGC settings, not just keys. Disproportionate, and it hides a security property inside a global format |
| Store Aerora keys in a separate Aerora store | No | Does not help. The vulnerable store is the one QGC actually reads on the signing path |
| **Backend interface behind `_save()` / `_load()`** | **Yes** | **Minimum viable core modification: exactly two functions**, `.cc:248-278` and `.cc:399`. Everything else — derivation, trial-matching, model, QML — is untouched |

The minimum core modification is therefore small, well-bounded and upstreamable. Phase 4 still does
not choose between contributing it upstream and carrying it as a recorded Aerora core edit, because
that choice changes ADR-001's standing zero-core-edit position and belongs to the reviewer.

---

### Q-029 — role resolution injection points

No role, permission or default authority was invented. The chain is mapped against real seams.

```text
Identity
   |
Role Resolution
   |
Authorization
   |
Command Gate
   |
MAVLink
```

| Stage | Where it can live | Core edit? |
| --- | --- | --- |
| Identity | Aerora module, overlay-side. No QGC foundation exists to conflict with | No |
| Role resolution | Aerora module, fed from configuration rather than code | No |
| Authorization | Aerora module, consulted by the gate | No |
| **Command gate** | See below — this is the constrained stage | **Depends on the enforcement guarantee wanted** |
| MAVLink | Unchanged. No dialect work | No |

**The command gate is where the evidence bites.** Observed facts:

| Observation | Evidence |
| --- | --- |
| All six `sendMavCommand*` variants delegate to one member | `src/Vehicle/Vehicle.cc:2146-2193` |
| That member is the command queue | `src/Vehicle/Vehicle.h:1056`, `MavCommandQueue* _mavCmdQueue` |
| All typed wrappers converge on one full-control entry point | `src/Vehicle/MavCommandQueue.h:44-47`, `sendWorker()` |
| The actual transmit is private | `MavCommandQueue.h:102`, `_sendFromList(int)` |
| `MavCommandQueue` has **no virtual members** | Full header inspected. Not overridable |
| `QGCCorePlugin`'s only MAVLink hook is **incoming** | `src/API/QGCCorePlugin.h:136`, `mavlinkMessage()`, documented as traffic *to* a vehicle, `false` meaning the vehicle should not process it |
| There is **no outgoing-message veto hook** anywhere in `QGCCorePlugin` | Complete virtual surface of the header inspected |

Consequence, stated plainly:

- A gate placed **above** `Vehicle::sendMavCommand*`, inside Aerora's own call paths, costs **zero
  core edits** — but it is **advisory, not enforcing**. Any stock QGC UI path that calls
  `sendMavCommand*` directly bypasses it.
- A gate that is **enforcing for every command** requires **one core edit**, at
  `MavCommandQueue::sendWorker()` or `_sendFromList()`.

That is a reviewer-visible trade between a zero-core-edit advisory gate and a one-core-edit enforcing
gate. Phase 4 does not choose it, and it is a distinct decision from Q-029's role set. The role set
remains the blocking input: the mechanism accommodates any role set, and an unresolvable role grants
nothing.

---

### Q-037 / Q4-003 — the exact evidence requirement

Quoted from the existing record rather than reinterpreted. Q4-003 asks whether the audit-store
integrity mechanism requires a key, and if so where that key lives given F3.1-01 is unresolved.

| Mechanism class | Defends against | Needs a key? |
| --- | --- | --- |
| Unkeyed digest chain | Accidental corruption, and nothing else | No |
| Keyed chain or signature | Repudiation — an operator editing their own record | **Yes** |

The recorded evidence requirement is **"None external. A decision on which threat the mechanism
addresses."** It is a decision, not an observation, so no amount of host or source investigation can
close it. It stays `OPEN`.

Two constraints already on record and still binding:

- `AERORA-GCS-REQ-028`'s rationale is "an audit record that can be edited is not evidence", which is a
  repudiation concern, and that points at a keyed mechanism.
- "A compromised operator host" is explicitly out of scope, so the mechanism must not pretend to
  defend against host compromise.

Those two pull in opposite directions, which is precisely why it needs a named decision. `IF-LOG-020`
stays `BLOCKED`, with the seam specified and **no default implementation**, so the gap is loud at the
first build rather than silent at the first audit.

---

## F. Build Host Readiness

Measured on this host. No large toolchain was installed, because storage is below requirement.

### KNOWN INSTALLED

| Item | Observed |
| --- | --- |
| Python | 3.12.10 |
| Git | 2.55.0.windows |
| Node | v24.18.0 |
| Java JDK | 25 |
| `uv`, `pip`, `winget` | Present |
| CPU | 4 logical cores — meets the floor |
| QGC checkout at pinned commit | `9459a4b5`, source diff 0 |

### KNOWN MISSING

| Item | Required | Observed |
| --- | --- | --- |
| CMake | ≥ 3.25 (`CMakeLists.txt:5`) | Absent |
| Ninja | Build generator | Absent |
| MSVC C++ toolset | 2022 VCTools + Windows SDK | Absent. `vswhere.exe` absent, no Visual Studio directory |
| Qt | **6.11.1**, `win64_msvc2022_64`, 14 modules | Absent. `qmake` not on `PATH` |
| GStreamer | **1.28.4** to `C:\gstreamer\1.0\msvc_x86_64` | Absent |
| `just`, `pre-commit`, `ctest` | Workflow and test entry points | Absent |
| Visual Studio components | `Microsoft.VisualStudio.Workload.VCTools --includeRecommended` | Not installed |

### UNKNOWN

| Item | Why it is unknown |
| --- | --- |
| Whether the baseline configures and builds at all | Never attempted. No toolchain |
| Whether the overlay contract behaves as read | Never compiled |
| Whether Aerora tests are reachable by the runner | Q-035, R-021 |
| Whether overlay resource override resolves as traced | Q4-001, R4-005 |
| Whether the plugin factory links from the overlay | R-010 |
| Peak build disk and memory draw | No build has run, so the 25–40 GB figure stays a range |
| Network and dependency availability under CI-equivalent load | Not exercised |
| `C:\Windows` size | Four recursive scans exceeded the tool timeout. Admin-only to reclaim regardless |

### BLOCKED

| Item | Blocker |
| --- | --- |
| Toolchain installation | Storage below requirement, **and** no administrator rights. Two independent gates |
| MSVC and GStreamer specifically | Both installers require elevation |
| First baseline build | Depends on the above |
| Aerora Git origin | No Aerora repository URL available — section H |
| Product implementation | §27 authorization — section I |

Environment variables are not yet applicable: none of `CMAKE_PREFIX_PATH`, a Qt root, or a GStreamer
root can be set meaningfully until the corresponding toolchain exists.

Current gate state, re-run this pass:
[`.kiro/tools/aerora-preflight.ps1`](../../tools/aerora-preflight.ps1) → **6 PASS / 8 FAIL / 1 WARN,
VERDICT NO-GO, exit 1**.

---

## G. Storage Decision

```text
Cache cleanup is NOT a sufficient build-host remediation.
```

This is a measured conclusion, recorded in full under R4-003 in
[22-phase-4-risks.md](22-phase-4-risks.md). Summary:

| Measured | Value |
| --- | --- |
| Reclaimable user-clearable cache | **≈ 5.47 GB** |
| Plus `Downloads`, if the user chooses | ≈ 7.4 GB |
| Best case free after all of it | **≈ 8–10 GB** |
| Requirement | **~25–40 GB** |

Free space is **volatile**, observed between **0.11 GB and 2.58 GB** without agent activity. It is
**not** a monotonic downward trend; an earlier characterisation of it as steadily falling was wrong
and has been corrected. Volatile readings must not be quoted as a trend.

The requirement stays expressed as a range. It is **not** converted into an exact minimum, because no
build has run on any host to establish one.

Remediation classes, none performed automatically:

| # | Class | Note |
| --- | --- | --- |
| 1 | Attach or use another drive | Valid infrastructure option. No deletion required |
| 2 | Deliberately remove a sufficiently large unrelated tree | **Requires explicit owner decision.** `aerora_quant_backend_updated_final1` is 8.6 GB and unrelated — **must not be deleted or modified** |
| 3 | Deliberately uninstall sufficiently large software | `C:\Program Files` totals ≈ 21.9 GB. Application-level decisions, not an agent action |
| 4 | Elevated Windows cleanup | DISM/WinSxS, pagefile, hibernation, Windows Update cache. **Cannot currently be performed** — no elevation |
| 5 | Other verified storage expansion | Any option that is measured rather than assumed |

No further broad cache survey will be run, and `C:\Windows` will not be scanned again.

---

## H. Git Origin Readiness

**Status: `BLOCKED`.**

Inspected read-only. No remote was added, renamed or removed. Nothing was pushed.

| Observation | Value |
| --- | --- |
| `origin` fetch | `https://github.com/mavlink/qgroundcontrol.git` |
| `origin` push | `https://github.com/mavlink/qgroundcontrol.git` |
| Current branch | `master` |
| Tracking | `origin/master` |
| HEAD | `9459a4b57`, `v5.2.0-dev-102-g9459a4b57` |
| Local branches | `master` only. No Aerora branch |
| Aerora remote | **None exists** |
| Fork status | **Not a fork.** This is a direct clone of upstream |

This confirms R-008 exactly: `origin` is upstream for **push** as well as fetch, so a first Aerora
implementation commit on `master` would target `mavlink/qgroundcontrol`.

Exact remaining action, for the owner:

| # | Action | Note |
| --- | --- | --- |
| 1 | Create or nominate an Aerora-owned repository | **The URL is not available to Phase 4.** It will not be invented |
| 2 | Point `origin` at that repository | Push target must stop being upstream |
| 3 | Add upstream as a separate **fetch-only** remote | Preserves the ability to track and diff upstream, per the drift-detection plan |
| 4 | Create an Aerora working branch off `9459a4b5` | So `master` keeps tracking pristine upstream |

Until step 1 produces a real URL, this stays `BLOCKED`. No remote change is safe to guess at.

---

## I. §27 Authorization Dependency

**Status: `BLOCKED`. No regulatory or product approval is claimed.**

§27 was searched for as a repository artifact rather than recalled from memory.

| Search | Result |
| --- | --- |
| `.kiro/**` for a §27 authorization grant | No authorization document |
| Untracked `Aerora_Universal_GCS_Product_Baseline_Specification_v4.1/` (52 documents) | No authorization grant. Matches for "authorized" are threat-model actor names only — `14_SECURITY_ARCHITECTURE.md`, `29_THREAT_MODEL.md`, `39_TEST_CASE_CATALOG.md` |

**Finding: §27 exists as a directive-level instruction, not as a repository artifact.** The
authoritative authorization scope is therefore not currently reviewable from inside the repository.

| Required evidence | State |
| --- | --- |
| A written authorization record naming which product features are authorized | **Does not exist in the repository** |
| Its owner and effective date | Unknown |
| Mapping from authorized scope to Aerora requirement IDs | Cannot be produced without the above |

Two compliance-related documents do exist in the untracked baseline —
`45_COMPLIANCE_READINESS.md` and `47_DGCA_COMPLIANCE_FRAMEWORK.md` — but readiness and framework
material is **not** an approval, and they are not treated as one here.

Consequently: no Aerora flight, payload, autonomy, failsafe, production-security or
production-authorization behaviour is implemented, and no regulatory approval is asserted or implied.
The dependency is recorded and left `OPEN`.

---

## J. Phase 4 Gate

| Gate | Status |
| --- | --- |
| Radio evidence | **OPEN** — observation performed, no radio attached |
| Q3-001 | **OPEN** |
| ADR-002 | **OPEN** — cannot be accepted, modified or withdrawn without the observation |
| F3.1-02 | **OPEN** |
| F3.1-01 | **OPEN** — engineering detail complete, route decision outstanding |
| Q-029 | **OPEN** |
| Q-037/Q4-003 | **OPEN** |
| Build storage | **NO-GO** |
| Aerora Git origin | **BLOCKED** — no Aerora URL available |
| §27 authorization | **BLOCKED** — not present as a repository artifact |
| QGC source modified | **NO** |
| Product implementation started | **NO** |

---

## Next execution gate

```text
Resolve storage/infrastructure
        |
Establish Aerora repository origin
        |
Baseline QGC configure/build
        |
Structural Aerora overlay
        |
Plugin/QML/resource verification
        |
Human review
        |
Actual Aerora product implementation
```

The radio bench observation remains fully parallel to all of it, needs no build host, and is now a
single command.

PHASE 4 BLOCKER RESOLUTION COMPLETE — READY FOR NEXT EXECUTION GATE

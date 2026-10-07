# Risk Register

Engineering risks identified during the QGroundControl baseline analysis. Every entry is grounded
in evidence recorded elsewhere in this document set.

## Scoring

Severity reflects consequence if unmitigated. Likelihood reflects the probability of the risk
materialising given the current plan. Neither is a schedule estimate.

| Severity | Meaning |
| --- | --- |
| `CRITICAL` | Blocks the programme, or creates a flight-safety or legal exposure |
| `HIGH` | Significant rework, or a compliance gap that must be closed before operational use |
| `MEDIUM` | Material cost or quality impact, manageable with planning |
| `LOW` | Nuisance, or easily absorbed |

## Register

### R-001 Qt licensing decision gates the entire delivery model

| Field | Value |
| --- | --- |
| Severity | `CRITICAL` |
| Likelihood | Certain — the decision cannot be avoided |
| Domain | Legal, commercial, architecture |
| Evidence | `.github/COPYING.md`, `LICENSE-APACHE`, `LICENSE-GPL`, `docs/en/qgc-dev-guide/contribute/licences.md`. See [dependency-map.md](dependency-map.md) |

QGroundControl is dual-licensed Apache 2.0 or GPL v3. `COPYING.md` states that the Apache path
allows proprietary derivatives but **requires a commercial Qt licence**, while the GPL path is
compatible with the open-source Qt build but requires conveying complete corresponding source
under GPL v3.

Aerora is a commercial industrial product. Either Aerora buys commercial Qt licences, or Aerora
releases its GCS source under GPL v3. There is no third option, and the choice determines whether
the Aerora overlay can remain proprietary.

**Mitigation.** Escalate to Aerora commercial and legal ownership immediately. This decision should
precede Phase 5. Note the obligation also extends to GStreamer (LGPL) and every CPM dependency.

**Residual.** Unavoidable; only resolvable by decision.

---

### R-002 Aerora payload state commands are silently dropped from a generated resume mission

| Field | Value |
| --- | --- |
| Severity | `CRITICAL` |
| Likelihood | High if a painting mission uses custom state-setting commands |
| Domain | Safety, mission integrity |
| Evidence | `MissionManager::generateResumeMission`, `src/MissionManager/MissionManager.cc:71-158`. See [mission-architecture.md](mission-architecture.md) |

Resume mission generation replays state-setting DO commands from the mission start so the aircraft
resumes correctly configured. The set of commands replayed is a **hardcoded core allow-list**,
`includedResumeCommands`, containing only camera, gimbal, ROI, speed, and fence commands.

An Aerora painting payload using custom MAV_CMD state commands — pump enable, spray rate, tank
valve — would have those commands **silently omitted**. The aircraft would resume the mission with
the payload in the wrong state, with no warning to the operator.

There is a second, related hazard in the same function: **resume is refused outright if the mission
contains any `MAV_CMD_DO_JUMP`**.

**Mitigation.** Either extend the core allow-list as an enumerated core edit, or design the Aerora
payload so that state is re-established by the pattern itself at resume rather than by prefix
replay. Add a test asserting payload state survives resume generation — it would currently fail.

**Residual.** Low once mitigated, but the mitigation choice is an `OPEN` decision.

---

### R-003 Transport extensibility requires core-tree modification

| Field | Value |
| --- | --- |
| Severity | `HIGH` |
| Likelihood | Certain |
| Domain | Architecture, maintainability |
| Evidence | Exhaustive edit list in [communication-architecture.md](communication-architecture.md) |

Link types are not pluggable. Adding a transport requires editing one enum, four switch statements,
and one order-coupled string list across three core files, plus a CMake entry.

The consequence is a permanent merge burden against upstream. Every upstream change to
`LinkConfiguration.h`, `LinkConfiguration.cc`, or `LinkManager.cc` risks a conflict with the Aerora
transport registration.

A secondary hazard: the `LinkType` enum is conditionally compiled on `QGC_NO_SERIAL_LINK` and
`QT_DEBUG`, and **its integer values are persisted to `QSettings`**. Inserting a value anywhere but
immediately before `TypeLast` silently invalidates users' saved link configurations.

**Mitigation.** Keep the edits minimal and contiguous so conflicts are trivial to resolve. Consider
a single Aerora `LinkType` with a sub-type discriminator in the configuration, paying the core edit
exactly once — the trade-off is a less idiomatic settings UI, and the decision is `OPEN`. Add a
regression test asserting `linkTypeStrings().size() == TypeLast`.

**Residual.** Medium. Inherent to the architecture.

---

### R-004 No application authentication or authorisation exists

| Field | Value |
| --- | --- |
| Severity | `HIGH` |
| Likelihood | Certain |
| Domain | Security, compliance |
| Evidence | Searched; nothing auth-related beyond MAVLink signing. See [security-architecture.md](security-architecture.md) |

QGC has no login, no user accounts, no roles, no session management, and no operator-permission
model. The GCS trusts whoever runs the binary.

Aerora targets DGCA compliance readiness, audit logging, role separation, and future high-assurance
scalability. All of that has no existing foundation.

**Mitigation.** Treat as a new Aerora module, scoped in the `09-security` specification. Note that
`QGCCorePlugin::showAdvancedUI` is a UI toggle, not authorisation, and must not be mistaken for one.

**Residual.** Low once built, but it is substantial new work on the critical path to any
compliance claim.

---

### R-005 MAVLink signing keys are stored in plaintext

| Field | Value |
| --- | --- |
| Severity | `HIGH` |
| Likelihood | Certain |
| Domain | Security |
| Evidence | `src/MAVLink/Signing/MAVLinkSigningKeys.h` — `QSettings` group `MAVLinkSigningKeys`, subgroup `keys/<name>` |

The key material protecting MAVLink message authenticity is persisted unencrypted in `QSettings`,
with no OS keychain or credential-store integration. Any local process or user with filesystem
access can read it.

Compounding factor: the PBKDF2 salt is fixed and app-wide
(`"QGroundControl-MAVLink-Signing-v1"`), deliberately so that multiple GCS stations derive the same
key from the same passphrase. This makes precomputation viable against weak passphrases, and the
minimum passphrase length is only 8.

**Mitigation.** Replace the storage backend behind `MAVLinkSigningKeys` while keeping the signing
logic. Prefer `addRawKey` with `generateRandomHexKey` over passphrase derivation, and treat key
distribution as a managed process.

**Residual.** Low once mitigated.

---

### R-006 Signing is off by default and no link-layer encryption exists

| Field | Value |
| --- | --- |
| Severity | `HIGH` |
| Likelihood | High |
| Domain | Security, safety |
| Evidence | `UnsignedAcceptancePolicy` is configurable; all transports are plaintext |

An unsigned or spoofed MAVLink stream is accepted unless the operator explicitly enables signing
for that link. Serial, UDP, and TCP traffic is readable and injectable by anyone on the path.

For Aerora this intersects with the radio transports that do not yet exist. A command-injection
attack on an industrial UAV operating near structures is a physical-safety risk, not only a data
risk.

**Mitigation.** Decide whether Aerora mandates signing-on by default, and whether the Aerora
transports provide their own link-layer security. Both are `OPEN`. Consider making
`UnsignedAcceptancePolicy::Strict` the Aerora default via settings metadata adjustment.

**Residual.** Depends entirely on the decision.

---

### R-007 No signature verification on updates or firmware downloads

| Field | Value |
| --- | --- |
| Severity | `HIGH` |
| Likelihood | Medium |
| Domain | Security, supply chain |
| Evidence | `QGCVersionCheck.cc` compares a plain text file over HTTPS; `QGCFileDownload::setExpectedHash` exists but no evidence it is populated on that path |

There is no signed manifest for application updates and no in-application auto-updater with
signature validation. Firmware images are fetched over HTTPS and flashed.

**Mitigation.** New Aerora module for signed update distribution. Note that
`stableVersionCheckFileUrl()` already returns an empty string under `QGC_CUSTOM_BUILD`, so the
stock check is disabled in an Aerora build by default — Aerora must build its own rather than
inherit a weak one.

**Residual.** Low once built.

---

### R-008 Checkout targets upstream, not an Aerora fork

| Field | Value |
| --- | --- |
| Severity | `HIGH` |
| Likelihood | Certain if unaddressed |
| Domain | Configuration management |
| Evidence | `git remote -v` shows `origin` as `https://github.com/mavlink/qgroundcontrol.git`, branch `master` |

Committing Aerora work from this checkout would target upstream QGroundControl. There is also no
Aerora branch, no fork, and no configured upstream-tracking strategy.

**Mitigation.** Create the Aerora fork, reconfigure `origin` to it, add `upstream` as a second
remote, and define the rebase or merge cadence against upstream before any implementation phase.
Record the baseline commit `9459a4b57986f40df8bdc193d5453c0596fbb416` as the fork point.

**Residual.** Negligible once done.

---

### R-009 Host cannot build QGroundControl

| Field | Value |
| --- | --- |
| Severity | `MEDIUM` |
| Likelihood | Certain |
| Domain | Development environment |
| Evidence | `cmake`, `ninja`, `cl`, `qmake`, and `just` all absent from `PATH`; no `build/` directory; no `CMakeUserPresets.json` |

Read-only analysis is unaffected, but the definition-of-done gates — `just build`, `just lint`,
`ctest` — cannot run. No code change can be verified on this host as configured.

Compounding factor: drive C: had **zero bytes free** at the start of this session, which prevented
even `git status` from running until space was cleared. Only about 235 MB is free now. A QGC build
tree with CPM sources and object files will require substantially more.

**Mitigation.** Provision the toolchain per `.github/build-config.json` — Qt 6.11.1, CMake 3.25+,
MSVC, Ninja, `just` — and allocate adequate disk. Verify with `just info` and a clean
`just configure && just build` before Phase 5.

**Residual.** None once provisioned.

---

### R-010 Firmware plugin factory conflict is silent

| Field | Value |
| --- | --- |
| Severity | `MEDIUM` |
| Likelihood | Medium |
| Domain | Architecture |
| Evidence | `FirmwarePluginManager::_findPluginFactory()` returns the first factory claiming a firmware class; registration order is static-initialisation order across translation units |

Two factories claiming the same `FirmwareClass_t` is a silent conflict with effectively
unspecified resolution. There is no priority mechanism and no conflict detection.

A second, related hazard: registration depends on a file-scope static object's constructor running.
If Aerora places its factory in a separate static library without force-linking, **registration
silently does not happen** and the vehicle falls back to the generic plugin.

**Mitigation.** Follow the `custom-example` pattern exactly — including the `extern` declaration
that anchors the static object — and set `QGC_DISABLE_PX4_PLUGIN_FACTORY` and
`QGC_DISABLE_APM_PLUGIN_FACTORY` if Aerora is single-stack. Add a test asserting the Aerora factory
is the one selected for the Aerora firmware class.

**Residual.** Low once the pattern is followed.

---

### R-011 Build reproducibility is broken by two floating dependency references

| Field | Value |
| --- | --- |
| Severity | `MEDIUM` |
| Likelihood | Certain |
| Domain | Supply chain, configuration management |
| Evidence | `src/FirmwarePlugin/APM/CMakeLists.txt:41` pulls `ArduPilotParams` from branch `main`; `src/Android/CMakeLists.txt:36` uses the mutable tag `qt6-beta1` |

Two builds of the same QGC commit can embed different third-party content. This defeats build
reproducibility and makes any SBOM a point-in-time claim rather than a fact — a problem for
compliance evidence and for software-update integrity.

**Mitigation.** Pin both in the Aerora overlay if the variables permit, or carry a small core patch
pinning them. Record the resolved commits in the Aerora build manifest. Note that
`ArduPilotParams` is irrelevant if Aerora disables the APM factory, which reduces this to the
Android case.

**Residual.** Low.

---

### R-012 Non-landing mission patterns do not survive a download from the vehicle

| Field | Value |
| --- | --- |
| Severity | `MEDIUM` |
| Likelihood | High |
| Domain | Mission integrity, usability |
| Evidence | `MissionController::_scanForAdditionalSettings()` only scans for landing patterns; the code comment states a complex item cannot have sections |

Survey, CorridorScan, and StructureScan are **not** reconstructed from a mission downloaded from a
vehicle. An Aerora painting pattern would likewise be lost, appearing as a flat list of waypoints.

For an industrial operation where the plan is the job specification, losing the pattern structure on
download is a real operational limitation.

**Mitigation.** Either implement an Aerora `scanForItems` equivalent, or require that the
authoritative plan always come from the saved `.plan` file rather than from the vehicle. The second
option is cheaper and arguably better practice, but it is a workflow constraint that must be
documented and accepted.

**Residual.** Depends on the choice; `OPEN`.

---

### R-013 MAVLink channel budget is 15

| Field | Value |
| --- | --- |
| Severity | `MEDIUM` |
| Likelihood | Low to medium |
| Domain | Architecture, scalability |
| Evidence | `MAVLINK_COMM_NUM_BUFFERS = 16` with channel 0 permanently reserved via a bitmask initialised to `1`; `LinkManager::allocateMavlinkChannel()` |

Fifteen channels are allocatable per QGC instance. One channel per link, two for MockLink, and one
extra consumed by `APMFirmwarePlugin` for v1 re-encoding.

A multi-transport Aerora aircraft with several concurrent links, across a fleet of vehicles in one
GCS instance, could approach the ceiling. Exhaustion is handled — it returns an invalid channel and
logs a warning — but the link simply fails to connect.

**Mitigation.** Model the worst-case concurrent link count early. If Aerora needs more, the
`MAVLINK_COMM_NUM_BUFFERS` definition in `src/MAVLink/MAVLinkMessageType.h` is a core edit with
memory implications, since `MAVLinkProtocol` holds a `[channels][256][256]` sequence array.

**Residual.** Low if modelled.

---

### R-014 Long-lived overlay maintenance burden against a moving upstream

| Field | Value |
| --- | --- |
| Severity | `MEDIUM` |
| Likelihood | Certain |
| Domain | Maintainability |
| Evidence | No runtime plugin system; all extension is compile-time plus resource override. HEAD is 102 commits past the `v5.2.0-dev` marker, so upstream moves quickly |

An Aerora build is a fork-by-overlay. QML file overrides are particularly exposed: the interceptor
replaces a file wholesale, so an upstream change to an overridden file is silently not picked up —
there is no merge conflict to alert anyone.

**Mitigation.** Keep the override set as small as possible and prefer `FlyViewCustomLayer.qml`, the
purpose-built empty seam, over overriding populated core files. Maintain a documented inventory of
every overridden file with the upstream commit it was forked from, and add a CI check comparing
that commit against current upstream.

**Residual.** Medium. Inherent to the model.

---

### R-015 Windows, Android, and Linux artefacts are not signed by the build

| Field | Value |
| --- | --- |
| Severity | `MEDIUM` |
| Likelihood | Certain |
| Domain | Security, distribution |
| Evidence | Only `deploy/windows/qgc-windows-store-plan.md` exists for Windows — a plan, not an implementation. `QT_ANDROID_SIGN_APK` and `QT_ANDROID_SIGN_AAB` default OFF. No GPG step found for Linux packages |

macOS and iOS signing and notarisation are properly implemented; the other three platforms are not.
Aerora requires signed artefacts.

**Mitigation.** New build work per platform. The macOS implementation in
`cmake/install/SignMacBundle.cmake` is a good model for structure.

**Residual.** Low once built.

---

### R-016 Qt6::HttpServer purpose is untraced

| Field | Value |
| --- | --- |
| Severity | `MEDIUM` |
| Likelihood | Unknown |
| Domain | Security |
| Evidence | Required component in the root `find_package(Qt6 ...)` and linked in `src/CMakeLists.txt`; the consuming feature was not identified |

Any network-exposed HTTP endpoint without authentication is a security concern. Whether one exists,
and whether it is authenticated, is unknown.

**Mitigation.** Identify the consumer before making any Aerora security claim. If it is not needed,
consider removing the dependency. Tracked as an action item in
[open-questions.md](open-questions.md).

**Residual.** Unknown until investigated.

---

### R-017 Vehicle identity is system-id only, with no UID matching

| Field | Value |
| --- | --- |
| Severity | `MEDIUM` |
| Likelihood | High for fleet operation |
| Domain | Architecture, fleet management |
| Evidence | `_systemID` is the sole identity; `vehicleUID` and `vehicleUIDStr` exist as properties but are not used to key the vehicle list. `MultiVehicleManager::getVehicleById()` scans by `id()` |

A MAVLink system id is a small integer, locally assigned and reusable. For an Aerora fleet with
cloud-side vehicle records, audit trails, and per-airframe maintenance history, system id is not a
durable identity.

QGC already warns when a vehicle's system id collides with the GCS system id, which shows the
weakness of the scheme.

**Mitigation.** Define the Aerora fleet vehicle identity and the mapping from system id to it.
Likely a new Aerora concern layered above `Vehicle`, keyed on a hardware identifier — the baseline
mentions an ATECC608A secure element, which may be relevant. `OPEN`.

**Residual.** Low once defined.

---

### R-018 Survey-style insert behaviour is unavailable to a plugin-supplied pattern

| Field | Value |
| --- | --- |
| Severity | `LOW` |
| Likelihood | Medium |
| Domain | Mission, usability |
| Evidence | `MissionController::_insertComplexMissionItemWorker()` hardcodes identity by `qobject_cast` against the three stock survey-style classes; `MissionController.cc:444-480`, `513`, `527` |

An Aerora pattern deriving from `ComplexMissionItem` will not receive survey-style insertion
behaviour — automatic camera mode and gimbal pointing. For a painting payload that may be
irrelevant or even desirable, but it also means the removal bookkeeping does not apply.

**Mitigation.** Either accept the default, derive from `SurveyComplexItem` to opt in via the cast,
or make the enumerated core edit. Decide in Phase 2.

**Residual.** Negligible.

---

### R-019 Silent failure of QML and resource overrides

| Field | Value |
| --- | --- |
| Severity | `LOW` |
| Likelihood | Medium |
| Domain | Quality |
| Evidence | The URL interceptor uses `QFile::exists()` and falls through to core on a miss. Extensible UI lists are URLs resolved by a `Loader` |

A typo in a `QT_RESOURCE_ALIAS`, or a missing resource registration, means the override silently
does not apply — the application runs with stock behaviour and no error. Likewise a bad URL in a
toolbar indicator or settings page list fails silently at the `Loader`.

**Mitigation.** Add a startup self-check in `AeroraPlugin::init()` that asserts every expected
override resource exists, and a test that enumerates every Aerora-supplied QML URL and verifies it
resolves.

**Residual.** Negligible once checked.

---

### R-020 Per-dependency licence inventory is unverified

| Field | Value |
| --- | --- |
| Severity | `LOW` to `MEDIUM` |
| Likelihood | Certain |
| Domain | Legal |
| Evidence | The upstream licence texts of the individual CPM dependencies were **not** read during this analysis. See [dependency-map.md](dependency-map.md) |

Roughly twenty-five third-party packages are fetched. Their licences are indicated but not
confirmed. GStreamer's LGPL status is particularly relevant to the dynamic-plugin layout.

**Mitigation.** Perform a formal licence audit of every dependency before distribution. The SBOM
generation already in CI (`generate_cpm_sbom.py`) is a useful starting point.

**Residual.** Low once audited.

---

## Risks explicitly out of scope

Recorded so that their absence is not mistaken for an oversight.

| Topic | Why out of scope |
| --- | --- |
| Failsafe behaviour risk | Behaviour is undefined by the baseline. A risk cannot be assessed against an undefined requirement. See [open-questions.md](open-questions.md) |
| Power transition risk | Transition behaviour is a controlled open item in the baseline |
| 480 V electrical safety | Not a GCS concern. Electrical design domain |
| Paint supply and hose risk | Mechanical and process domain |
| GPU and autonomy algorithm risk | FC and GPU subsystem domain |
| FHSS24 and RF433 hardware existence | The baseline itself states these must be confirmed separately before being called onboard hardware |

## Summary by severity

| Severity | Count | Identifiers |
| --- | --- | --- |
| `CRITICAL` | 2 | R-001, R-002 |
| `HIGH` | 6 | R-003, R-004, R-005, R-006, R-007, R-008 |
| `MEDIUM` | 9 | R-009, R-010, R-011, R-012, R-013, R-014, R-015, R-016, R-017 |
| `LOW` | 3 | R-018, R-019, R-020 |

The two critical items are a commercial decision and a safety-relevant core limitation. Neither is
a code-quality problem, and neither can be resolved by this analysis.

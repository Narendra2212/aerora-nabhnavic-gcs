# Phase 2 Risk Gates

Gate decisions for all 20 risks raised in Phase 1
([risk-register.md](../00-qgc-baseline-analysis/risk-register.md)).

Each risk is reclassified for Phase 2 and assigned a gate. The gate answers one question: **what
must happen before this risk stops blocking progress?**

## Classification and gate vocabulary

| Class | Meaning |
| --- | --- |
| `BLOCKER` | Work cannot proceed past its gate until resolved |
| `HIGH` | Must be resolved before the named gate; schedule it explicitly |
| `MEDIUM` | Manage actively; resolve before the named gate |
| `LOW` | Track; resolve opportunistically |
| `INFORMATIONAL` | No action required beyond awareness |

| Gate | Meaning |
| --- | --- |
| `G-ARCH` | Must resolve **before architecture is ratified** — it changes the architecture |
| `G-IMPL` | Must resolve **before implementation starts** — it does not change the architecture |
| `G-INTEG` | Must resolve before Phase 7 integration |
| `G-RELEASE` | Must resolve before any release or distribution |
| `G-OPEN` | May remain open; carried as a managed item |

## Summary

| Gate | Count | Risks |
| --- | --- | --- |
| `G-ARCH` | 4 | R-002, R-003, R-004, R-017 |
| `G-IMPL` | 5 | R-008, R-009, R-010, R-011, R-019 |
| `G-INTEG` | 2 | R-012, R-013 |
| `G-RELEASE` | 5 | R-001, R-005, R-007, R-015, R-020 |
| `G-OPEN` | 4 | R-006, R-014, R-016, R-018 |

Three Phase 1 risks were **downgraded** by Phase 2 investigation: R-003 (from `HIGH` to `MEDIUM`),
R-002 (from `CRITICAL` to `HIGH`), and R-012 (from `MEDIUM` to `LOW`). One was **reclassified in
kind** rather than severity: R-001 is now an open legal question, not an engineering risk.

No risk was upgraded.

**Phase 2.1 amendment to this summary.** R-016 moved from `G-IMPL` to `G-OPEN` and from `MEDIUM` to
`LOW`. The counts above reflect that change; the original counts were `G-IMPL` 6 including R-016,
and `G-OPEN` 3. The reasoning is in the R-016 section below, where the superseded Phase 2 text is
retained verbatim. Four Phase 1 risks are now downgraded rather than three.

---

## R-001 Qt and QGroundControl licensing

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `CRITICAL` | **`BLOCKER` — reclassified as an OPEN LEGAL / LICENSING QUESTION** |
| Gate | — | `G-RELEASE` |

### Reclassification

Phase 1 stated the position as a binary engineering conclusion. **That was an overreach.** Phase 2
reclassifies R-001 as an open legal and licensing question. This document records only the
technical facts observable in the repository and the verification required. It offers no legal
advice and reaches no legal conclusion.

### Technical facts observed in the repository

| Fact | Evidence |
| --- | --- |
| Two licence texts are present in full | `LICENSE-APACHE` (Apache License 2.0), `LICENSE-GPL` (GNU GPL v3, 29 June 2007) |
| `.github/COPYING.md` states QGroundControl is dual-licensed under Apache 2.0 and GPL v3, and that either may be chosen | `.github/COPYING.md` |
| That file describes the Apache 2.0 path as permitting use in proprietary applications and mobile app stores, and includes the sentence that it requires a commercial Qt licence | `.github/COPYING.md` |
| That file describes GPL v3 as strong copyleft, compatible with the open-source build of Qt | `.github/COPYING.md` |
| Inbound contributions must be compatible with both; acceptable inbound licences listed are original work, BSD-2-Clause, BSD-3-Clause, MIT, Apache 2.0; GPL-only code is stated as not acceptable | `.github/COPYING.md` |
| A licensing contact is named | `.github/COPYING.md` |
| `docs/en/qgc-dev-guide/contribute/licences.md` repeats the dual-licence statement and covers documentation, artwork and images in a separate section | that file |
| QGC's own sources carry **no** per-file SPDX header convention; the only `SPDX-License-Identifier` hits under `src/` are vendored Qt files | search performed in Phase 1 |
| Qt itself is obtained via `find_package`, not vendored; the licence under which a given Qt build is obtained is a property of that Qt installation, not of this repository | [build-system.md](../00-qgc-baseline-analysis/build-system.md) |
| Roughly 25 third-party packages are fetched via CPM, each with its own terms, none of which were read | [dependency-map.md](../00-qgc-baseline-analysis/dependency-map.md) |
| GStreamer is LGPL-relevant and is loaded as dynamic plugins; it can be disabled entirely with `QGC_ENABLE_GST_VIDEOSTREAMING=OFF` | [dependency-map.md](../00-qgc-baseline-analysis/dependency-map.md) |
| Branding variables (`QGC_APP_NAME`, `QGC_ORG_NAME`, icon paths) are designed to be overridden by an overlay | [plugin-architecture.md](../00-qgc-baseline-analysis/plugin-architecture.md) |

### What the architecture does about it

The architecture is **licence-neutral by construction**, and this is a deliberate design
constraint rather than a consequence. See
[architecture-decisions.md](architecture-decisions.md) ADR-009.

Concretely: all Aerora code lives in a single overlay directory, the core-edit surface is
enumerated and minimal, and no architectural decision in this Phase 2 set depends on which licence
path is chosen. The same architecture is buildable under either outcome. If the outcome requires
source release, the overlay is what gets released; if it permits a proprietary overlay, nothing
changes structurally.

### Verification required

Aerora legal and commercial ownership must obtain, from qualified counsel, a determination of:

1. Which licence path Aerora elects for its derivative of QGroundControl.
2. What obligations that election creates for the Aerora overlay, for the enumerated core edits,
   and for distribution of binaries.
3. What Qt licence Aerora requires for the elected path and for each target platform.
4. The obligations arising from each third-party dependency, which requires the audit in R-020.
5. Whether GStreamer is retained, and the obligations if so.
6. The treatment of QGroundControl trademarks and artwork, which `licences.md` covers separately
   from code.

### Gate rationale

`G-RELEASE`, not `G-ARCH`. The architecture does not depend on the answer. Distribution does.
Aerora may proceed through Phases 3 to 6 without this resolved, provided no artefact is
distributed outside Aerora.

---

## R-002 Aerora payload state commands dropped from a generated resume mission

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `CRITICAL` | **`HIGH`** — downgraded |
| Gate | — | `G-ARCH` |

### Why it was downgraded

Phase 2 read `generateResumeMission()` in full. Two findings reduce the severity:

1. **The filter only affects commands positioned *before* the resume index.** The inclusion
   condition at `MissionManager.cc:134` contains the clause `i >= resumeIndex`, so any command at
   or after the resume point — including any Aerora command — is copied through untouched. The
   problem is bounded to prefix replay.
2. **The allow-list is not a deny policy.** The stale header comment at `MissionManager.h:30-31`
   promises "all the CMD_DO items", while the implementation enumerates 14. The list is the set of
   commands `CameraSection` and `SpeedSection` can emit — it was intended as exhaustive for what
   QGC itself generates, not as a filter against vendors.

It remains `HIGH` because the failure mode is silent. There is no log, no warning, no counter, and
no user notification for a dropped command. The only diagnostics in the function are a debug log
for the in-progress early return and an application message for the DO_JUMP refusal.

### Full technical finding

| Aspect | Finding |
| --- | --- |
| Allow-list scope | A **plain local automatic variable**, `MissionManager.cc:110-126`. Not static, not const, not a member. Unreachable from outside the function |
| Overridability | `MissionManager.h` declares **no virtuals at all**. `generateResumeMission` is non-virtual. `Vehicle` constructs the concrete class directly at `Vehicle.cc:243`. No subclass substitution, no factory, no `QGCCorePlugin` hook anywhere on this path |
| Design intent | State restoration, inferred from the de-duplication tail at `MissionManager.cc:145-194`: latest-wins collapsing for `SET_CAMERA_MODE` and `DO_SET_ROI`; a single shared slot across the capture-command group; and outright deletion of `IMAGE_START_CAPTURE` with `param3 != 0` under the comment about removing commands that do not trigger by time. Replay lasting configuration, suppress one-shot actions |
| Item fidelity | Items are deep-copied. `param1` through `param7` are **never touched**. Only `setIsCurrentItem` and `setSequenceNumber` are mutated |
| Item representation | `MissionItem` holds command, frame, `param1-7`, autoContinue, sequence number, isCurrentItem, doJumpId. **No string, blob, or vendor-extension slot** |
| Complex items | Flattened **once** on upload via `MissionController::_convertToMissionItems`. Expansion is **not** re-run on resume; `generateResumeMission` operates on the flat item list |
| DO_JUMP | Any `MAV_CMD_DO_JUMP` **anywhere** in the mission blocks resume entirely, aborting before any state change |
| Blast radius | Resume **replaces the entire vehicle mission**. On success the original full mission no longer exists on the vehicle, and the resume button hides because the index resets |
| Partial failure | QGC discards its own write list but performs **no rollback**. The vehicle mission store is left partial and indeterminate until the user retries |

Four latent defects were also observed and are recorded in [open-questions.md](open-questions.md)
as upstream-contribution candidates: an unguarded index at `MissionManager.cc:94` with no
empty-list check, an unguarded index at line 202, a per-resume leak of the local item list, and
the silent drop of a resume request when a transaction is in progress.

### Architectural recommendation

**The fix does not belong in QGroundControl core.** Full reasoning in
[mission-architecture.md](mission-architecture.md); decision recorded as ADR-005 in
[architecture-decisions.md](architecture-decisions.md).

The reasoning in brief: QGC replays a command prefix *because it assumes the vehicle retains
nothing*. That assumption is what should change, not the allow-list. Per the v4.1 baseline, the
payload controller owns actuator-level payload behaviour and payload-local interlocks — so payload
configuration state is the payload controller's to hold, not the GCS's to reconstruct.

The recommended layering is:

| Layer | Responsibility |
| --- | --- |
| Payload controller | Owns and persists its own configuration state. Authoritative |
| Flight controller | Relays and reports payload state; owns the safety interlock path |
| Aerora GCS | Emits payload state at or after the resume point, where it already survives; displays reported state; never assumes its own command made state effective |
| QGC core | Unchanged |

This also satisfies the authority principle in [authority-model.md](authority-model.md): the GCS
displays reported payload state rather than inferring it from commands it sent.

### Gate rationale

`G-ARCH`. The decision about where payload state authority sits is an architectural decision, and
it is made in this Phase 2 set. What remains for later phases is verification.

### Phase 2.1 reclassification

The severity downgrade and the two rejections above stand. What changes is the **status of the
recommended resolution**.

| Element | Phase 2 position | Phase 2.1 position |
| --- | --- | --- |
| Class | `HIGH` | `HIGH` — unchanged |
| Gate | `G-ARCH` | `G-ARCH` — unchanged, now with an explicit evidence gate |
| QGC core modification | Rejected | **Rejected — unchanged** |
| GCS-side payload state replay | Rejected | **Rejected — unchanged** |
| Payload-controller state retention | Presented as the Aerora reality | **Preferred architecture, evidence status `NOT-EVIDENCED`** |
| Fallback if retention is not implemented | Not stated | Plan constraint plus resume-time operator warning |

The paragraph above beginning "The reasoning in brief" says the payload controller "owns
actuator-level payload behaviour and payload-local interlocks — so payload configuration state is
the payload controller's to hold". **The inference in that clause does not follow.** Ownership is
baseline-supported; holding state across a mission interruption is a separate behaviour that no
baseline document states and no firmware artifact evidences. The layering table's first row,
"Owns and persists its own configuration state", asserts persistence that has not been designed.

R-002 is therefore **not mitigated** by this architecture. It is mitigated *conditionally*, on a
vehicle-side behaviour recorded as an evidence item in the
[hardware and firmware evidence matrix](hardware-firmware-evidence-matrix.md). Until that evidence
exists, the GCS-side fallback carries the mitigation.

Full treatment in [mission-architecture.md](mission-architecture.md); reconciliation in
[phase-2-1-reconciliation.md](phase-2-1-reconciliation.md).

---

## R-003 Transport extensibility requires core-tree modification

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `HIGH` | **`MEDIUM`** — downgraded |
| Gate | — | `G-ARCH` |

### Why it was downgraded — Q-014 resolved

Phase 2 resolved Q-014 and the answer removes the immediate need for any transport core edit.

**A generic, unrecognised serial device carries MAVLink through QGC's existing `SerialLink` with
zero core changes.** Verified at every step of the manual connect path:

| Step | Finding |
| --- | --- |
| Port enumeration | Unfiltered. `SerialPortManager::_enumeratePorts()` calls `getBoardInfo()` but **discards the result**; every port is appended |
| UI port list | `LinkManager::_updateSerialPorts()` lists all ports; the settings page even re-appends a saved-but-absent port |
| Baud | **Arbitrary baud is supported.** The settings page offers a `Custom` entry with an integer validator accepting any value above zero, passed straight to `QSerialPort::setBaudRate()` |
| Configuration | `SerialConfiguration` persists port name, baud, data bits, flow control, stop bits, parity, and a DTR-low flag. `usbDirect` and `sikRadio` are runtime-only and never persisted |
| Connect | `SerialWorker::connectToPort()` opens the port and applies the settings. **No identity gate anywhere** |
| Port reservation | Refusal reasons are identity-independent: empty name, already reserved, or the Android single-port restriction |
| Radio treatment | `isRadioLink()` latches true on any `RADIO_STATUS` frame, so an unidentified radio gets radio-link treatment automatically |

The single behavioural difference for an unrecognised device is that
`SerialAutoConnect::_allowed()` returns false in its `default:` case, so it is never
auto-connected. The user creates the link once; the configuration persists.

### Three practical frictions, none of them blockers

1. **Misidentification hazard.** `USBBoardInfo.json` maps VID 4292 / PID 60000 — the stock Silicon
   Labs CP2102 identifier — to "SiK Radio", with description fallbacks for FT231X and an
   Android-only catch-all. An Aerora radio built on a stock CP2102 or FT231X bridge **will be
   misidentified**, auto-connected at 57600 baud, and will take the port reservation, causing the
   manual link to fail. Mitigation is to disable SiK auto-connect, or to use a distinct VID/PID.
2. **Android enumeration.** The default Android backend requires the device to be in the
   usb-serial-for-android probe table or in QGC's own prober. A custom VID/PID may not enumerate.
   Mainstream bridge chips are covered.
3. **macOS node filtering.** `tty.usbserial*` and `tty.usbmodem*` are filtered out; the `cu.*`
   node is used.

### Architectural consequence

Transport work is **deferred, not cancelled**. Decision recorded as ADR-002 and ADR-003 in
[architecture-decisions.md](architecture-decisions.md), detailed in
[communication-architecture.md](communication-architecture.md).

If a later product requirement needs a transport QGC cannot express as a serial port — a native
network radio, a cloud relay, a protocol requiring framing QGC does not provide — the six
enumerated core edits become necessary. The architecture keeps that option open without paying for
it now.

### Gate rationale

`G-ARCH`. The decision to adopt serial-first is architectural and is taken here. The residual
risk after that decision is low.

---

## R-004 No application authentication or authorisation exists

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `HIGH` | `HIGH` — unchanged |
| Gate | — | `G-ARCH` |

Unchanged. QGC has no login, no accounts, no roles, no sessions, and no operator-permission model.
`QGCCorePlugin::showAdvancedUI` is a UI toggle and must not be mistaken for authorisation.

Aerora targets DGCA compliance readiness, audit logging, and role separation, none of which have a
foundation to extend.

`G-ARCH` because the identity model shapes the cloud architecture, the audit model, the UI gating
model, and the offline behaviour. Those are architectural. Architecture is provided in
[security-architecture.md](security-architecture.md); the role set itself remains `OPEN` as Q-029.

---

## R-005 MAVLink signing keys stored in plaintext

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `HIGH` | `HIGH` — unchanged |
| Gate | — | `G-RELEASE` |

Unchanged. Key material is persisted unencrypted in application settings with no keychain
integration, and the PBKDF2 salt is fixed app-wide by deliberate design so that multiple stations
derive the same key from the same passphrase.

The architecture replaces the storage backend while keeping QGC's signing logic, which is sound.
See [security-architecture.md](security-architecture.md).

`G-RELEASE` rather than `G-ARCH` because the replacement is behind an existing interface and does
not change the architecture. It must not ship unresolved.

Note the hardware opportunity: the v4.1 baseline records an ATECC608A secure element on the flight
controller. That is a vehicle-side asset and does not solve GCS-side key storage, but it is
relevant to the wider key-management design.

---

## R-006 Signing off by default and no link-layer encryption

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `HIGH` | `HIGH` — unchanged |
| Gate | — | `G-OPEN` |

Unchanged in substance. An unsigned MAVLink stream is accepted unless signing is explicitly
enabled, and no transport provides confidentiality or integrity.

`G-OPEN` because the answer depends on Q-010 and Q-011, which are product and hardware decisions
not yet available. The architecture provides the mechanism — Aerora can set the signing default
through settings metadata adjustment without a core edit — but the policy is Aerora's to set.

Carried as an explicitly managed item. It must be closed before operational use, not before
implementation.

---

## R-007 No signature verification on updates or firmware downloads

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `HIGH` | `HIGH` — unchanged |
| Gate | — | `G-RELEASE` |

Unchanged. One Phase 2 observation reduces the inherited exposure:
`QGCCorePlugin::stableVersionCheckFileUrl()` is compiled to return an empty string under
`QGC_CUSTOM_BUILD`, so an Aerora build does **not** inherit the stock version check. Aerora builds
its own or has none.

`G-RELEASE`. A distributed update mechanism without integrity verification is not acceptable; an
absent update mechanism during development is.

---

## R-008 Checkout targets upstream rather than an Aerora fork

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `HIGH` | `HIGH` — unchanged |
| Gate | — | `G-IMPL` |

Unchanged. `origin` points at `https://github.com/mavlink/qgroundcontrol.git` on branch `master`.
There is no Aerora fork, no Aerora branch, and no upstream-tracking strategy.

`G-IMPL`, and it is the **first** item of implementation. Record
`9459a4b57986f40df8bdc193d5453c0596fbb416` as the fork point. See
[configuration-architecture.md](configuration-architecture.md).

---

## R-009 Host cannot build QGroundControl

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `MEDIUM` | `MEDIUM` — unchanged |
| Gate | — | `G-IMPL` |

Unchanged. CMake, Ninja, the MSVC compiler, qmake, and `just` are all absent from `PATH`; there is
no build directory. Disk space was also exhausted earlier in the programme and remains tight.

No Phase 2 output required a build, so this did not impede architecture. It blocks every
definition-of-done gate from Phase 5 onward.

---

## R-010 Firmware plugin factory conflict is silent

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `MEDIUM` | `MEDIUM` — unchanged |
| Gate | — | `G-IMPL` |

Unchanged. First factory claiming a firmware class wins, and registration order is
static-initialisation order across translation units. Registration also depends on a file-scope
static object's constructor running, so a factory in a separate static library without force-linking
**silently does not register**.

Mitigated by configuration, not architecture: if Aerora is single-stack, disable both stock
factories. That depends on Q-017. See [vehicle-integration.md](vehicle-integration.md).

---

## R-011 Build reproducibility broken by two floating dependency references

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `MEDIUM` | `MEDIUM` — unchanged |
| Gate | — | `G-IMPL` |

Unchanged. `ArduPilotParams` tracks branch `main`; `qtandroidextensions` uses a mutable tag.

Note that the `ArduPilotParams` instance becomes moot if Aerora disables the ArduPilot factory,
reducing this to the Android case. Depends on Q-017.

---

## R-012 Non-landing mission patterns do not survive a download from the vehicle

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `MEDIUM` | **`LOW`** — downgraded |
| Gate | — | `G-INTEG` |

### Why it was downgraded

Phase 2 established that this is not an Aerora-specific limitation and not a defect: it is
inherent to the MAVLink mission representation. `MissionItem` has no vendor-extension slot, so a
pattern's structure cannot survive a round trip through the vehicle. Survey, CorridorScan, and
StructureScan have the same limitation upstream.

The architecture resolves it by **declaring the saved plan file authoritative**, not the vehicle's
mission store. This is also better practice for an industrial operation where the plan is the job
specification, and it aligns with the cloud mission-synchronisation module in the v4.1 baseline.

Recorded as ADR-005 in [architecture-decisions.md](architecture-decisions.md).

`G-INTEG` because it is a workflow constraint to validate during integration, not an architectural
unknown.

---

## R-013 MAVLink channel budget is 15

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `MEDIUM` | `MEDIUM` — unchanged |
| Gate | — | `G-INTEG` |

Unchanged. Fifteen channels allocatable, one per link, two for MockLink, one extra consumed by the
ArduPilot plugin for v1 re-encoding.

Phase 2 note: the serial-first decision does not change the budget, because each serial link still
consumes one channel. The v4.1 baseline's three-link architecture plus a fleet in one GCS instance
is the case to model. Depends on Q-015.

---

## R-014 Long-lived overlay maintenance burden against a moving upstream

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `MEDIUM` | `MEDIUM` — unchanged |
| Gate | — | `G-OPEN` |

Unchanged, and inherent to the model. Phase 2 reduces the exposure in one specific way: deferring
transport core edits means that **zero QGC core edits are required by the current architecture**, so
the overlay is pure addition. That materially lowers merge risk. **One QGC core edit remains a
possible fallback for signing-key storage if upstream extensibility is unavailable** — see ADR-008.

The residual exposure is QML file overrides, where the interceptor replaces a file wholesale and an
upstream change to an overridden file is silently not picked up — there is no merge conflict to
alert anyone.

Mitigation is architectural and is adopted: prefer the purpose-built empty seam over overriding
populated core files, and maintain an inventory of every overridden file with its fork-point
commit. See [ui-architecture.md](ui-architecture.md).

`G-OPEN` as a permanent managed item with a recurring review.

---

## R-015 Windows, Android, and Linux artefacts not signed by the build

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `MEDIUM` | `MEDIUM` — unchanged |
| Gate | — | `G-RELEASE` |

Unchanged. macOS and iOS signing and notarisation are properly implemented; Windows has only a
planning document; Android signing defaults off; no Linux package signing was found.

Scope depends on Q-032. See [deployment-architecture.md](deployment-architecture.md).

---

## R-016 Qt6::HttpServer purpose untraced

| Field | Phase 1 | Phase 2 | Phase 2.1 |
| --- | --- | --- | --- |
| Class | `MEDIUM` | `MEDIUM` — unchanged | **`LOW`** — downgraded |
| Gate | — | `G-IMPL` | **`G-OPEN`** |

### Original Phase 2 assessment, retained as written

> Unchanged, and **still untraced**. Phase 2 did not investigate it, because it does not affect the
> architecture. It is a required Qt component and is linked.
>
> Any network-exposed endpoint without authentication is a security concern. This must be closed
> before any Aerora security claim. It is cheap: identify the consumer, and if none is needed,
> consider dropping the dependency.

### Phase 2.1 amendment

**The statement above is superseded.** It was written early in Phase 2, before the dependency was
investigated. Later in the same phase the cloud architecture work traced it, so the document
contradicted itself and the earlier text is quoted rather than deleted so the sequence stays
visible.

The traced finding, recorded in [cloud-architecture.md](cloud-architecture.md):

| Observation | Evidence |
| --- | --- |
| The module is linked into the main application target | `src/CMakeLists.txt:108` |
| It is also linked privately by the network utility library | `src/Utilities/Network/CMakeLists.txt:20` |
| It is used for exactly two type aliases: an HTTP status-code enum and an HTTP method enum | `src/Utilities/Network/QGCNetworkHelper.h:57`, `:106` |
| No server object is constructed anywhere in the source tree | Searched |
| No listening socket is bound anywhere in the source tree | Searched; the only binds are UDP — `UDPLink.cc:330`, `NMEASourceManager.cc:141`, `RTCMUdpInput.cc:44`, `UDPGPSTransport.cc:39` |

So the premise behind the original `MEDIUM` rating — that an unauthenticated network endpoint might
exist — **is false**. There is no endpoint. The residual item is a dependency-hygiene decision, not
a security risk, which is why the class drops to `LOW` and the gate to `G-OPEN`.

| Residual item | Carried as |
| --- | --- |
| Whether to drop the dependency and define the two enums locally | Q-027, and an optional upstream contribution in [risk-gates.md](risk-gates.md) |
| Assurance that no listener appears later | `AERORA-AWS-REQ-014`, asserted by a startup self-check and a test |

Phase 2.1 does **not** treat R-016 as closed. It treats the security question as answered and the
hygiene question as open. The distinction matters: closing the risk entirely would remove the
assurance obligation that keeps the answer true.

---

## R-017 Vehicle identity is system-id only, with no UID matching

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `MEDIUM` | `MEDIUM` — unchanged |
| Gate | — | `G-ARCH` |

Unchanged in substance, but **promoted to `G-ARCH`** because Phase 2 established that fleet
identity is load-bearing for three separate architectural domains: the cloud vehicle registry, the
audit trail, and configuration distribution. All three need a durable identifier that a MAVLink
system id cannot provide.

The v4.1 baseline records an ATECC608A secure element on the flight controller, which is a
candidate source of device identity. Whether it is provisioned and readable is `UNKNOWN`.

Architecture provided in [data-model.md](data-model.md); the identifier source remains `OPEN` as
Q-016.

---

## R-018 Survey-style insert behaviour unavailable to a plugin-supplied pattern

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `LOW` | `LOW` — unchanged |
| Gate | — | `G-OPEN` |

Unchanged. A pattern supplied through the plugin factory does not receive the automatic camera-mode
and gimbal-pointing behaviour that the three stock survey-style patterns get, because identity is
checked by cast.

For a paint-spray payload that behaviour is probably unwanted anyway. Accepting the default keeps
the Aerora pattern a zero-core-edit addition. `G-OPEN`.

---

## R-019 Silent failure of QML and resource overrides

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `LOW` | `LOW` — unchanged |
| Gate | — | `G-IMPL` |

Unchanged. The URL interceptor falls through to core on a miss, and extensible UI lists are URLs
resolved by a loader, so a typo yields stock behaviour with no error.

Mitigation is adopted architecturally: a startup self-check asserting every expected override
resource exists, plus a test enumerating every Aerora-supplied QML URL. See
[testing-architecture.md](testing-architecture.md).

---

## R-020 Per-dependency licence inventory unverified

| Field | Phase 1 | Phase 2 |
| --- | --- | --- |
| Class | `LOW` to `MEDIUM` | `MEDIUM` |
| Gate | — | `G-RELEASE` |

Raised to a firm `MEDIUM` because R-001's verification depends on it. Roughly 25 CPM packages plus
Qt and GStreamer, none of whose licence texts were read.

CI already generates a CPM software bill of materials, which is the starting point.

---

## Gate decision table

| Risk | Class | Gate | Blocks architecture ratification? |
| --- | --- | --- | --- |
| R-001 | `BLOCKER` (legal) | `G-RELEASE` | No |
| R-002 | `HIGH` | `G-ARCH` | **Yes — decided in this set** |
| R-003 | `MEDIUM` | `G-ARCH` | **Yes — decided in this set** |
| R-004 | `HIGH` | `G-ARCH` | **Yes — architecture provided, role set open** |
| R-005 | `HIGH` | `G-RELEASE` | No |
| R-006 | `HIGH` | `G-OPEN` | No |
| R-007 | `HIGH` | `G-RELEASE` | No |
| R-008 | `HIGH` | `G-IMPL` | No |
| R-009 | `MEDIUM` | `G-IMPL` | No |
| R-010 | `MEDIUM` | `G-IMPL` | No |
| R-011 | `MEDIUM` | `G-IMPL` | No |
| R-012 | `LOW` | `G-INTEG` | No |
| R-013 | `MEDIUM` | `G-INTEG` | No |
| R-014 | `MEDIUM` | `G-OPEN` | No |
| R-015 | `MEDIUM` | `G-RELEASE` | No |
| R-016 | `LOW` — amended in Phase 2.1 | `G-OPEN` — amended in Phase 2.1 | No |
| R-017 | `MEDIUM` | `G-ARCH` | **Yes — architecture provided, source open** |
| R-018 | `LOW` | `G-OPEN` | No |
| R-019 | `LOW` | `G-IMPL` | No |
| R-020 | `MEDIUM` | `G-RELEASE` | No |

### Can architecture be ratified?

Four risks gate architecture ratification. All four are **addressed by decisions in this Phase 2
set**:

| Risk | Resolved by |
| --- | --- |
| R-002 | ADR-005 — payload state authority sits with the payload controller, not the GCS |
| R-003 | ADR-002 and ADR-003 — serial-first transport strategy, core edits deferred |
| R-004 | [security-architecture.md](security-architecture.md) — identity module architecture; role set remains `OPEN` |
| R-017 | [data-model.md](data-model.md) — fleet identity model; identifier source remains `OPEN` |

Two of the four carry a residual `OPEN` item that does not block ratification, because the
architecture accommodates either answer. R-004's role set and R-017's identifier source are both
parameters of an architecture that is otherwise settled.

**Conclusion: the architecture in this Phase 2 set can be ratified subject to human review.** No
risk blocks it.

### New risks arising from Phase 2

Recorded in [risk-gates.md](risk-gates.md) rather than here, so that this document remains a clean
review of the Phase 1 register.

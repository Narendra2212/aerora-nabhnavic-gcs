# Phase 4 — Reviewer Decision Pack

Seven decisions, each converted from evidence already established and verified against pinned QGC
source at `9459a4b5`. No decision is taken here. No option is selected. Nothing is implemented.

This document adds no requirement, interface, MAVLink identifier, enumeration, role name, USB
identifier, repository URL or authorization state.

| Field | Value |
| --- | --- |
| QGC HEAD | `9459a4b57986f40df8bdc193d5453c0596fbb416` |
| QGC source modifications | **0** |
| Product implementation started | **NO** |
| Decisions requiring a human | **6** (`D-001`, `D-002`, `D-003`, `D-005`, `D-006`, `D-007`) |
| Decisions requiring physical evidence first | **1** (`D-004`) |
| Options pre-selected by this document | **0** |

## Governing rule for this pack

The objective is **minimum necessary core modification**, not zero core modification at any cost.
Where a core edit is objectively required to make a security boundary real, it is named here openly,
with its scope bounded and its drift cost stated. No required core change is concealed behind the
Aerora overlay, and no security boundary is downgraded to an advisory mechanism merely to preserve a
zero-edit claim.

---

## D-001 — Command authorization enforcement

`HUMAN DECISION REQUIRED`

### Established evidence

| Finding | Evidence |
| --- | --- |
| All six `sendMavCommand*` variants delegate to one member | `src/Vehicle/Vehicle.cc:2146-2193` |
| That member is the command queue | `src/Vehicle/Vehicle.h:1056` |
| All typed wrappers converge on one full-control entry point | `src/Vehicle/MavCommandQueue.h:44-47`, `sendWorker()` |
| The actual transmit is private | `src/Vehicle/MavCommandQueue.h:102`, `_sendFromList(int)` |
| `MavCommandQueue` has **no virtual members** | Full header inspected. Not overridable from an overlay |
| The only core-plugin MAVLink hook is **incoming** | `src/API/QGCCorePlugin.h:136`, `mavlinkMessage()`, documented as traffic *to* a vehicle |
| No outgoing-message veto hook exists anywhere in `QGCCorePlugin` | Complete virtual surface of the header inspected |

The consequence is structural, not stylistic: an overlay has no seam on the outgoing command path.

### Option A — Advisory zero-core-edit gate

```text
Aerora authorization
        |
Aerora UI/controller
        |
stock QGC command path
```

Advantages:

- Zero QGC core modification.
- Easier upstream synchronization.

Disadvantage:

- Cannot guarantee enforcement, because other stock UI paths can call `sendMavCommand*` directly and
  reach the vehicle without consulting the Aerora authorization layer.

Security consequence:

- **NOT a true authorization boundary.** It is a convention observed by Aerora-originated commands
  only. Any stock QGC screen that sends a command bypasses it. It must not be described, documented or
  presented to an operator or an auditor as an authorization control.

### Option B — Enforcing narrow core gate

```text
Aerora authorization
        |
central QGC command-send gate
        |
MavCommandQueue
        |
MAVLink
```

Candidate edit, one of:

- `MavCommandQueue::sendWorker()` — `src/Vehicle/MavCommandQueue.h:44-47`. The convergence point of
  every typed wrapper.
- `MavCommandQueue::_sendFromList()` — `src/Vehicle/MavCommandQueue.h:102`. The private transmit, and
  therefore the last point before the wire.

Advantages:

- Centralized enforcement at a single chokepoint.
- Stock command paths cannot bypass the gate.

Disadvantages:

- One controlled QGC core modification.
- Upstream drift must be tracked at that call site.
- Security review required, because the gate becomes a safety-relevant control path.

### What this decision does not cover

The choice between `sendWorker()` and `_sendFromList()` is a second, narrower decision and is
deliberately left open. The two differ in whether queued-but-not-yet-sent commands are gated at
enqueue time or at transmit time, which matters for retries and for delayed sends. That distinction
should be settled by whoever takes D-001, not before.

---

## D-002 — MAVLink signing key storage

`HUMAN DECISION REQUIRED`

### Established evidence

| Finding | Evidence |
| --- | --- |
| Source | `src/MAVLink/Signing/MAVLinkSigningKeys.h`, `.cc` |
| Instancing | `Q_APPLICATION_STATIC` at `.cc:38`, `instance()` at `.cc:39` |
| Virtual members | **None** except the destructor. No overridable storage API |
| Only access seam | `friend class SigningTest` in the header |
| Storage coupling | `QSettings`, default-constructed **inside** `_save()` and `_load()` |
| Serialization at rest | Raw 32 bytes written verbatim. No encryption, no OS keystore |
| Write path | `_save()` `.cc:248-278` |
| Read path | `_load()` `.cc:399` |
| Plaintext export surface | `keyHexByName()` `.cc:206`, `Q_INVOKABLE`, reachable from QML |
| In-memory hygiene | `QGC::secureZero` applied to passphrase, derived buffer, serialized copy and load buffer |

The gap is specifically **at rest**. In-memory discipline is already careful and consistent, and any
option chosen must preserve it.

Minimum core boundary, as bounded by the investigation:

```text
_save()  - .cc:248-278
_load()  - .cc:399
```

Intended architecture if an abstraction is chosen:

```text
MAVLinkSigningKeys
        |
Storage Backend Interface
        |
Aerora Secure Storage
```

### Option A — Retain `QSettings` with documented risk

No code change. The exposure is accepted and recorded explicitly rather than silently.

### Option B — Storage backend abstraction plus an Aerora secure backend

Introduce an interface behind the two functions above and route persistence through it. **The secure
backend implementation is not selected here** and is a separate decision — see the constraint note
below.

### Option C — App-wide custom `QSettings` format

Repository-evidenced and therefore listed. `QSettings` supports a registered custom format, so
persistence could be redirected without touching `MAVLinkSigningKeys` at all. The reason it is listed
separately rather than folded into Option B is its blast radius: it changes persistence for **every**
QGC setting, not only signing keys.

### Comparison

| Dimension | Option A — retain | Option B — backend interface | Option C — custom settings format |
| --- | --- | --- | --- |
| **Security consequence** | Keys remain raw bytes in the user registry hive. No protection at rest. The documented risk is real and unmitigated | Protection at rest becomes possible and is confined to the key store. The actual strength depends entirely on the unselected backend | Protection could apply at rest, but a security property is hidden inside a global persistence format, where it is easy to lose in review and easy to regress |
| **Core-edit scope** | **Zero** | **Two functions**, `.cc:248-278` and `.cc:399`. Derivation, trial-matching, model and QML untouched | **Zero** to `MAVLinkSigningKeys`, but a global registration affecting all settings |
| **Implementation complexity** | None | Low to moderate. The interface is small; complexity sits in the backend | Moderate, and deceptively so. Correctly reimplementing a settings format for all QGC data is broader than it first appears |
| **Test impact** | None | The existing `SigningTest` friend declaration still works. A fake backend becomes injectable, which makes at-rest behaviour testable for the first time | Hard to test in isolation. A format change is global, so signing tests cannot exercise it independently of unrelated settings |
| **Upstream maintenance impact** | None | Two call sites to track on rebase. Upstreamable as a contribution, which would reduce the drift to zero | No upstream drift at those functions, but the project carries a global behaviour divergence that upstream has no reason to accommodate |
| **Release implications** | The at-rest exposure must be stated in release and security documentation. Blocks `G-RELEASE` for `AERORA-SEC-REQ-017` unless formally accepted | Unblocks the requirement if the backend is adequate. Requires security review of the backend before release | Requires security review of the format **and** a regression pass over all settings persistence |

### Constraint the decision must account for

QGC targets Windows, Linux, macOS, Android and iOS. Any backend built on a single platform's keystore
is therefore a partial answer, and the decision needs to state what happens on the platforms it does
not cover. Fail-closed, fail-open and platform-conditional behaviour are materially different security
postures. No platform mechanism is named or recommended here.

### Coupled decision

`Q4-003` asks whether audit-store integrity needs a key. If the answer is a keyed mechanism, it needs
a key the station can protect, which is this decision. **D-002 should be taken before or together
with `Q4-003`**, not after. Recorded as risk `R4-002`.

---

## D-003 — Operator role model

`HUMAN DECISION REQUIRED`

`Q-029` remains `OPEN`. No role name is proposed, implied or defaulted in this document or anywhere in
the Phase 4 set. The role set is a product and operations decision with likely regulatory input.

### The distinction this pack makes explicit

```text
Role model
!=
Command enforcement mechanism
```

These have been conflated before and must not be. The role model says *who may do what*. The
enforcement mechanism determines *whether that statement is true at runtime*. A perfect role model
behind an advisory gate is documentation, not a control.

### Architecture, which is decided and role-agnostic

```text
Operator Identity
        |
Role Resolution
        |
Authorization Policy
        |
Command Authorization
        |
MAVLink
```

| Stage | Placement | Core edit? | Note |
| --- | --- | --- | --- |
| Operator identity | Aerora module, overlay-side | No | No QGC foundation exists to conflict with |
| Role resolution | Aerora module, **configuration-fed and replaceable** | No | Must remain swappable without touching callers |
| Authorization policy | Aerora module, consulted by the gate | No | Declares the gated-operation set explicitly |
| Command authorization | **See D-001** | **Depends on D-001** | This is where the chain becomes real or stays advisory |
| MAVLink | Unchanged | No | No dialect work |

Properties already fixed, independent of the role set:

- The resolver is replaceable and configuration-driven, not compiled-in.
- An unresolvable role grants **nothing**. Fail-closed.
- No default role carries implicit authority.
- The existing advanced-UI toggle is disclosure, not authorization, and must never be used as one.

### Dependency on D-001, stated plainly

If D-001 resolves to Option A, then D-003 cannot produce a security boundary regardless of which role
set is chosen, because stock QGC command paths bypass enforcement. In that case the role model is an
operational convention and must be documented as such. **D-003's value as a control is conditional on
D-001 Option B.**

Blocks `IF-SEC-003`, `AERORA-SEC-REQ-013`. Gate `G-IMPL`.

---

## D-004 — Radio transport

**This is an evidence dependency, not an architectural decision.** There is nothing for a reviewer to
decide until a measurement exists.

### Observed host state

```text
Aerora radio enumeration = NOT OBSERVED
```

| Source | Observation |
| --- | --- |
| `HKLM\HARDWARE\DEVICEMAP\SERIALCOMM` | Key **absent** |
| `GetPortNames()` | `<none>` |
| `Win32_SerialPort` | **0** |
| PnP class `Ports` | Only an ACPI legacy parallel port |
| Present USB VID/PIDs | All host-internal or unrelated |

### Status held, deliberately

| Item | Status |
| --- | --- |
| `ADR-002` | **`DEFERRED`** — not retained, not withdrawn |
| Six transport core edits | **`DEFERRED`** |
| Case A / Case B / Case C | **None selected** |
| USB VID / PID | **Not invented, not inferred** |
| E22 / SX1262 silicon identity | **Not treated as the USB identity** |
| `Q3-001` | `OPEN` |
| `F3.1-02` | `OPEN` |

### Exact next evidence event

```text
Attach actual Aerora radio
        |
Run aerora-radio-observe.ps1
        |
Compare against zero-radio baseline
        |
Inspect Windows enumeration
        |
Evaluate SerialLink
```

Tooling is in place: [`.kiro/tools/aerora-radio-observe.ps1`](../../tools/aerora-radio-observe.ps1)
carries the zero-radio baseline and tags anything outside it as `[NEW]`. The step needs no build host
and no ground station, so it is fully parallel to every other decision here.

Branch cost is already quantified, so the observation selects rather than investigates:

| Outcome | Cost |
| --- | --- |
| Serial device | **Zero** core edits. `SerialLink` has no whitelist, any port and baud work |
| Non-serial device | **Four** core edits minimum: `LinkConfiguration.h:103-117`, `LinkManager.cc:147-182`, `LinkManager.cc:418-443`, `LinkManager.cc:568` |

---

## D-005 — Aerora Git origin

`HUMAN DECISION REQUIRED` — and **`GIT ORIGIN = BLOCKED`** until an input is supplied.

### Observed repository state

| Observation | Value |
| --- | --- |
| `origin` fetch | `https://github.com/mavlink/qgroundcontrol.git` |
| `origin` push | `https://github.com/mavlink/qgroundcontrol.git` |
| Current branch | `master`, tracking `origin/master` |
| `git describe` | `v5.2.0-dev-102-g9459a4b57` |
| Local branches | `master` only |
| Aerora remote | **None** |
| Fork status | **Not a fork.** Direct clone of upstream |

This confirms `R-008`: a first Aerora implementation commit on `master` would target
`mavlink/qgroundcontrol`.

### Required transition

```text
Upstream QGC
      |
Aerora fork
      |
Aerora origin
```

### Required inputs, none of which may be invented

| # | Input | State |
| --- | --- | --- |
| 1 | Aerora Git hosting provider | **Not supplied** |
| 2 | Repository URL | **Not supplied.** Will not be fabricated |
| 3 | Branch strategy | **Not decided** — long-lived Aerora branch, or Aerora `main` with upstream tracked separately |
| 4 | Upstream remote name and fetch-only policy | **Not decided.** Fetch-only is recommended so upstream can never be a push target |
| 5 | Aerora origin remote | **Cannot be set** without input 2 |
| 6 | Fork point | Proposed `9459a4b5`, matching the pinned baseline and §21's unmodified-first-build rule |
| 7 | Upstream synchronization strategy | **Not decided** — rebase cadence, who owns conflict resolution, and how core-edit drift is detected |

Input 6 is the only one with a defensible default, because the entire Phase 1–4 evidence base is
pinned to that commit. It is still recorded as requiring confirmation.

### Held

No remote was added, renamed or removed. Nothing was pushed. No URL was guessed.

---

## D-006 — Build storage

`HUMAN DECISION REQUIRED`

Build host remains **`NO-GO`**. Preflight this pass: **6 PASS / 8 FAIL / 1 WARN**, exit 1.

### Established

| Finding | Value |
| --- | --- |
| Requirement | **~25–40 GB** working capacity, deliberately kept as a range |
| Reclaimable user-clearable cache | **≈ 5.47 GB** |
| Best case after clearing everything reclaimable | **≈ 8–10 GB** |
| Conclusion | **Cache cleanup is NOT a sufficient build-host remediation** |
| Free-space readings | Volatile, observed **0.11–2.58 GB**. Not a monotonic trend and must not be quoted as one |
| Elevated Windows cleanup | **Unavailable** — not administrator |

The requirement is **not** converted into an exact minimum, because no build has run on any host to
establish one.

### Options

| Option | Action | Note |
| --- | --- | --- |
| **A** | Attach or use another drive | Requires no deletion. Lowest risk to existing work |
| **B** | Authorized removal of a sufficiently large unrelated tree | `aerora_quant_backend_updated_final1` is 8.6 GB and unrelated. **Must not be deleted or modified without explicit authorization**, and 8.6 GB alone does not reach the requirement |
| **C** | Authorized uninstall or removal of large software | `C:\Program Files` totals ≈ 21.9 GB. These are application-level decisions for the owner, not agent actions |
| **D** | Elevated Windows cleanup | DISM/WinSxS, pagefile, hibernation, Windows Update cache. **Currently impossible** — requires elevation that does not exist in this session |

Options B, C and D are destructive or privileged. None was performed, and none will be performed
without explicit authorization. Option A is the only non-destructive route.

### Note on option independence

Elevation is a **separate gate** from storage. Even if storage is fully resolved by Option A, MSVC
Build Tools and GStreamer still require an elevated session. Resolving D-006 alone does not produce a
build host.

---

## D-007 — §27 / product authorization

`EXTERNAL EVIDENCE REQUIRED`

```text
§27 authorization evidence = NOT FOUND
```

### What was searched

| Scope | Result |
| --- | --- |
| `.kiro/**` | No authorization grant |
| Untracked `Aerora_Universal_GCS_Product_Baseline_Specification_v4.1/`, 52 documents | No authorization grant. Matches for "authorized" are threat-model actor names only |

Two documents exist that are adjacent but are **not** approval evidence:

- `45_COMPLIANCE_READINESS.md` — readiness material.
- `47_DGCA_COMPLIANCE_FRAMEWORK.md` — a framework.

Readiness and framework material describe how compliance would be approached. Neither is an approval,
and neither is treated as one here.

### Finding

§27 exists as a **directive-level instruction**, not as a repository artifact. Its authoritative scope
is therefore not reviewable from inside the repository.

### Authoritative external source that must be supplied or verified

| # | Required artifact | Why |
| --- | --- | --- |
| 1 | A written authorization record naming which product features are authorized for implementation | Without it, the boundary between authorized and unauthorized work is unauditable |
| 2 | Its issuing owner and effective date | An authorization without an owner cannot be relied upon |
| 3 | A mapping from authorized scope to Aerora requirement identifiers | So implementation can be checked against it mechanically |
| 4 | If regulatory: the issuing authority, instrument and applicability | Required before any compliance claim. **No regulatory approval is asserted or implied by this programme** |

No compliance state and no authorization state is claimed. The dependency is recorded and left `OPEN`.

---

## Decision matrix

| Decision | Evidence Status | Engineering Finding | Human Decision | Blocks |
| --- | --- | --- | --- | --- |
| D-001 Command gate | Evidence complete | Core gate required for true enforcement | OPEN | Security implementation |
| D-002 Key storage | Evidence complete | Two-function core boundary identified | OPEN | Security implementation |
| D-003 Roles | Partial | Role model still undefined | OPEN | Authorization implementation |
| D-004 Radio | Hardware absent | Cannot evaluate transport | DEFERRED | Radio integration |
| D-005 Git origin | URL absent | Cannot establish Aerora origin | BLOCKED | Aerora commits |
| D-006 Build storage | Insufficient | Host remains NO-GO | OPEN | QGC build |
| D-007 §27 | Evidence absent | Cannot establish authorization | OPEN | Release/compliance |

None of the above is resolved. None is partially resolved except D-003, which is marked `Partial`
because its architecture is settled while its role set is not.

---

## What may continue without these decisions

Specification and design work that is decision-independent. Note the hard constraint first:
**nothing in this list can be compiled, tested or committed to an Aerora repository** until D-005 and
D-006 are resolved, so all of it remains specification until then.

| Work | Why it is safe | Gated by |
| --- | --- | --- |
| Module scaffolding design | Structural, no behaviour | — |
| `FactGroup` class mapping | Mechanism fully traced | `Q4-002` affects attachment, not the mapping |
| QML module mapping | Override mechanism verified | `Q4-001` affects C++-read JSON only |
| Capability abstraction | Shape is independent of values | Values need firmware evidence |
| Telemetry and state model abstraction | Independent of role and transport | — |
| Mission architecture mapping | Independent of all seven decisions | — |
| Product profile architecture | Structural | — |
| Test architecture | Writable before policy exists, because each test asserts a property of the abstraction | Execution needs a build host |
| Upstream drift detection tooling design | **Raised in value by D-001 and D-002.** Both may produce core edits that must be tracked | — |
| Build scripts and documentation | §27-authorized non-product tooling | — |
| Interface-to-source mapping | Reading, not writing | — |

### What must not proceed

| Blocked work | Blocked by |
| --- | --- |
| Any authorization enforcement code | D-001, D-003 |
| Any key-storage change | D-002 |
| Any role name, permission name or default role | D-003 |
| Any transport work beyond inherited serial behaviour | D-004 |
| Any commit intended for an Aerora repository | D-005 |
| Any build or test execution claim | D-006 |
| Any flight, payload, autonomy or failsafe behaviour | D-007 |
| The seven `BLOCKED` interfaces | `IF-SEC-013`, `IF-LOG-020`, `IF-RAD-001`, `IF-SEC-003`, `IF-FC-017`, `IF-UIS-005`, `IF-UIS-006` |

---

## Implementation order after decisions

```text
HUMAN DECISIONS
      |
AERORA GIT ORIGIN
      |
BUILD STORAGE
      |
BASELINE QGC BUILD
      |
STRUCTURAL AERORA OVERLAY
      |
PLUGIN REGISTRATION
      |
QML RESOURCE RESOLUTION
      |
FACTGROUPS
      |
TELEMETRY / STATE MODEL
      |
AUTHORIZATION + SECURITY
      |
MISSION
      |
PAYLOAD
      |
PRODUCT PROFILES
      |
INTEGRATION TESTS
```

Two properties of this order are deliberate and should not be traded away:

- **The baseline QGC build is unmodified.** Per §21, the first successful build is stock upstream at
  `9459a4b5`, before any Aerora code exists. It is the only way a later failure is attributable.
- **Authorization and security sit above mission, payload and product profiles.** They are not
  reordered below product functionality for convenience. If D-001 resolves to Option B, the core gate
  must exist before any product feature relies on being gated by it, otherwise features ship
  unenforced and the gate becomes a retrofit.

---

## Architectural rule carried forward

The project must not optimize for "zero QGC core edits" at the expense of actual security enforcement.
The correct objective is:

```text
MINIMUM NECESSARY CORE MODIFICATION
```

not:

```text
ZERO CORE MODIFICATION AT ANY COST
```

Where a core edit is objectively required for a security boundary, it is documented, isolated, tested
and tracked for upstream drift. Two candidates are already named and bounded in this pack — D-001's
command gate and D-002's two-function storage boundary. Neither is hidden behind the overlay, and
neither is softened into an advisory mechanism to protect a zero-edit claim.

This rule has a consequence for `ADR-001`, whose standing position is zero core edits. If D-001 or
D-002 resolves toward a core modification, `ADR-001` must be amended rather than reinterpreted. That
amendment is part of the decision, not a follow-up.

---

PHASE 4 REVIEWER DECISION PACK COMPLETE — HUMAN DECISIONS REQUIRED

# Audit Sink Contract and QML-Safe Security Status

Owner: **Tab 2** (Security). Covers Phase 7 `WP2-A` (audit writer interface) and `WP2-B` /
`D-UI-01` (QML-safe enforcement read path).

Status vocabulary: [05-product-completion/00-README](../05-product-completion/00-README.md#status-vocabulary).

| Item | Status |
| --- | --- |
| `AeroraAuditSink` contract | `IMPLEMENTED-UNCOMPILED` (interface + producer); **no sink implementation exists** |
| Audit record composition and loss accounting | `IMPLEMENTED-UNCOMPILED` |
| Audit durability mechanism | `OPEN` → see [§2.9](#29-the-durability-decision-and-what-it-depends-on). Design supplied by WP1-C; **not implemented** |
| Audit tamper-evidence | `OPEN` — blocked on a protected anchor → `AeroraPlatformKeyProtector` → `D-006` |
| `AeroraSecurityStatus` | `IMPLEMENTED-UNCOMPILED` |
| Tests | **Written, never executed.** No compiler on this host (`D-006`) |

**Nothing in this document has been compiled, linted by `clang-tidy`/`clang-format`, or executed.**
`VERIFIED` is not claimed. [§5](#5-what-was-actually-checked) records exactly which checks ran.

## Contents

- [1. Why these two items are one document](#1-why-these-two-items-are-one-document)
- [2. WP2-A — the audit-sink contract](#2-wp2-a--the-audit-sink-contract)
- [3. WP2-B — the QML-safe security status](#3-wp2-b--the-qml-safe-security-status)
- [4. ADR-018 — an audit write failure does not block a command](#4-adr-018--an-audit-write-failure-does-not-block-a-command)
- [5. What was actually checked](#5-what-was-actually-checked)
- [6. Traceability delta](#6-traceability-delta)
- [7. Handoffs raised](#7-handoffs-raised)
- [8. Open decisions and what they block](#8-open-decisions-and-what-they-block)
- [9. What was deliberately not done](#9-what-was-deliberately-not-done)

## 1. Why these two items are one document

They share a single fact: **the station's audit and enforcement posture is currently worse than a
casual reading of the code suggests, and both work packages exist to make that visible rather than
to improve it.**

Before this pass, `AeroraAuditSink::record()` returned `void`. A sink that discarded every record
was indistinguishable, at the call site, from one that stored them — and
`droppedAuditRecordCount()` counted only the *no sink installed* case, so a storage failure
incremented nothing. `WP2-A` closes that. `WP2-B` is the only path by which the resulting counts,
and the fact that nothing is enforced, reach an operator.

## 2. WP2-A — the audit-sink contract

Interface: `aerora/src/Security/AeroraAuditSink.h`, `.cc`.
Producer: `aerora/src/Security/AeroraCommandAuthorizer.{h,cc}`.

### 2.1 Purpose and boundary

`AeroraAuditSink` is the seam between a **record producer** and a **record destination**. One
method wide, deliberately.

```text
producer                     this interface              the storage layer (does not exist)
AeroraCommandAuthorizer  ->  AeroraAuditSink::record  ->  AeroraAuditWriter    IF-LOG-014
                                                          AeroraAuditStore     IF-LOG-003..013
                                                          AeroraAuditIntegrity IF-LOG-020
```

Everything the storage layer owns is absent from the interface: storage location, schema, record
encoding, the bounded writer queue and its thread, rotation, retention, pruning, deletion records,
export, integrity material, and upload. The three types on the right are specified in
[13-audit-implementation.md](13-audit-implementation.md) and **none is implemented**.

A sink **never transmits**. `P7-ARCH-010` is local-first and `P7-DAT-004` prohibits automatic
outbound transmission of logs; `IF-LOG-016` additionally fixes the ordering — the local write *is*
the record and an upload is a later copy of it.

### 2.2 Record fields, and the requirement that justifies each

No field is present for a question no requirement asks. The record is deliberately narrower than
[13-audit-implementation.md](13-audit-implementation.md)'s eventual `AeroraAuditRecord`, whose extra
fields need a correlation identity (`IF-LOG-022`) and a session identity this layer is not given.

| Field | Question answered | If unavailable | Justified by |
| --- | --- | --- | --- |
| `type` | Allowed, denied, requested, transmitted | — | `AERORA-COMPLY-REQ-002`, `-003` |
| `utc` | When, for human reading | — | `IF-LOG-021` |
| `monotonicUs` | When, for ordering | Zero = unstamped | `IF-LOG-021` |
| `sequence` | In what exact order | Zero = unstamped | `IF-LOG-021` |
| `operatorId` | Who | Empty; a gap, never backfilled | `AERORA-SEC-REQ-019` |
| `reasonCode` | Why | Empty on allow | `AERORA-COMPLY-REQ-002` |
| `vehicleMavlinkSystemId` + `vehicleResolved` | Which vehicle | `vehicleResolved == false` | `IF-LOG-022` |
| `authorizationEnforced` | Was a policy evaluating | — | `P7-AUTH-002` |
| `recordsLostBeforeThis` | How much was already lost | — | `IF-LOG-015` |
| `detail` | What exactly was requested | — | `AERORA-COMPLY-REQ-002`, ADR-017 |

Three fields are **new this pass**: `monotonicUs`, `sequence` and `authorizationEnforced`.
`vehicleMavlinkSystemId` is **not** a fleet identity — that format is `OPEN` and no consumer may
derive one from it.

### 2.3 Record ordering

Three rules. Wall-clock time cannot carry any of them.

1. **Producers assign order, not sinks.** All three time fields are stamped at composition, which
   is what [13-audit-implementation.md](13-audit-implementation.md)'s write ordering requires
   ("record composed, timestamped at the event"). A sink must not re-stamp any of them.
   `AeroraAuditEvent::stampNow()` sets all three together so a producer cannot set one and forget
   another.
2. **`utc` is for reading, never for ordering.** Wall-clock time steps backwards on an NTP
   correction, a GNSS fix or an operator clock change, which would silently reorder a trail.
   `monotonicUs` is immune to all three. This is the same split, for the same reason, as
   `AuditRecords.occurredUtcMs` / `monotonicUs` in
   [09-local-persistence-architecture.md](../05-product-completion/09-local-persistence-architecture.md).
3. **`sequence` is what makes the order strict.** `monotonicUs` is non-decreasing, but two reads
   microseconds apart can return the same value on a coarse clock, and `authorize()` emits up to
   four records per command in a fixed, meaning-bearing order. A tie makes that order
   unrecoverable, so a dense counter supplies the tiebreak a clock cannot.

A sink **must** preserve the order in which records were handed to it. It is **not** required to
establish a total order across threads: the record already carries one.

### 2.4 Success and failure reporting

`record()` returns `AeroraAuditWriteOutcome` and is `[[nodiscard]]`. An ignored outcome is an
unaccounted record, which is the failure `AERORA-GCS-REQ-025` prohibits.

| Outcome | Meaning | Justified by |
| --- | --- | --- |
| `Accepted` | The sink took responsibility for the record | — |
| `DroppedQueueFull` | Bounded queue full; record discarded | `IF-LOG-014` — drops, never blocks |
| `FailedStorage` | The write was attempted and failed | `IF-LOG-016` |
| `SinkUnavailable` | Installed but not accepting — not started, shut down, refusing | `IF-LOG-020` writer refuses to start without integrity |

**`Accepted` is not a persistence claim.** `IF-LOG-014` requires the write path to be asynchronous
and never block the main thread, so a sink that returns promptly cannot have completed durable
I/O. Whether a record survives a crash is answered by `isDurable()`, which is false for every sink
that exists.

There is deliberately **no** `RejectedInvalid` outcome. `IF-LOG-022` requires a record with a
missing operator or vehicle identity to be written with the gap marked; refusing it would destroy
the evidence that the gap occurred.

### 2.5 Behaviour when the sink cannot accept or write

| Condition | Producer behaviour |
| --- | --- |
| No sink installed | `++droppedAuditRecordCount`, continue |
| Sink returns any non-`Accepted` outcome | `++failedAuditWriteCount`, continue |
| First failure after a sink is installed | One `qCWarning` naming the sink, the outcome and the record's `sequence` |
| Subsequent failures | Counted only. The warning is suppressed so a storage outage does not bury the condition it reports |
| Any failure | **The authorization decision is unchanged and the command is not blocked.** ADR-018 |

A sink must never block the caller, never drop silently, never discard an incomplete record, never
re-enter the producer, never throw, and never transmit.

### 2.6 Loss accounting

Three monotonic counters, none of which ever resets — including on sink installation, because
records lost under a previous configuration were still lost, and resetting would let a station
erase the evidence of its own audit gap by installing a sink late.

| Accessor | Counts |
| --- | --- |
| `droppedAuditRecordCount()` | No sink installed — a **configuration** state |
| `failedAuditWriteCount()` | Sink present and did not accept — an **operational** fault |
| `lostAuditRecordCount()` | The sum. The single number `IF-LOG-015` asks be shown to the operator |

Two causes rather than one, because collapsing them would report a station that was never
configured to record and a station whose disk filled as the same condition.

Additionally, `recordsLostBeforeThis` puts the count **into** each record, which is `IF-LOG-015`'s
actual requirement: a reader of an exported set learns records were lost without needing the live
counter. With `sequence` dense, a gap in a recovered set is itself positive evidence of loss.

**No counter reports records written, stored or retained.** Nothing stores them, so there is
nothing to count, and a "written" counter would be read as persistence.

### 2.7 Ownership and lifecycle

| Concern | Contract |
| --- | --- |
| Ownership | The installer owns the sink. `AeroraCommandAuthorizer` holds it through `std::shared_ptr` so replacing or clearing one cannot destroy it while a record is in flight |
| No sink installed | A **valid configuration, not an error.** It is the shipped state: nothing in the overlay constructs a sink |
| Thread | `record()` is called synchronously on the thread running the command gate — the vehicle's thread for both existing call sites, `MavCommandQueue::sendWorker()` and `Vehicle::startCalibration()`. The interface does not promise a single producer thread, which is why `nextSequence()` is atomic |
| Blocking | Prohibited. A sink needing file I/O hands off to its own queue and thread — `AeroraAuditWriter`'s job |
| Re-entrancy | A sink must not call back into `AeroraCommandAuthorizer`; `record()` runs inside the authorization path |

### 2.8 Boundary with the future storage implementation

WP1-C designs an `AuditRecords` table and assigns its writer — the first real `AeroraAuditSink`
implementation — to **Tab 2**. The column mapping is recorded in `AeroraAuditSink.h` so that writer
is a translation rather than a redesign. Two columns come from the store and not the producer
(`recordId`, `sessionId`), and two must never come from the producer (`prevHash`, `recordHash`).

The `actor` column is `NOT NULL` and WP1-C specifies the literal `unresolved` for an unknown
operator. **The writer substitutes it; the producer does not.** An empty `operatorId` stays empty
in the record, because backfilling at composition time would make an unattributed decision
indistinguishable from an attributed one.

### 2.9 The durability decision, and what it depends on

WP2-A was instructed not to choose a durability mechanism, and does not. The question it was
required to document — *does the same local store serve audit durability?* — has since been
answered by Tab 1 in
[09-local-persistence-architecture.md §7](../05-product-completion/09-local-persistence-architecture.md#7-q-037-assessment-can-this-carry-audit).
Recorded here rather than re-decided:

| Question | Tab 1's answer | Consequence for this interface |
| --- | --- | --- |
| Can the WP1-C store carry audit **durability**? | **Yes**, via WAL + `synchronous=FULL` + `ON DELETE RESTRICT`, subject to the hardware honouring fsync | A future sink may return true from `isDurable()` **once implemented and tested**, not before |
| Can it carry **tamper-evidence**? | **No, and must not be claimed.** A hash chain is tamper-evident only against an actor who cannot recompute it | `isTamperEvident()` stays false **even after the hash chain is implemented** |
| `Q-037` status | `OPEN` → **`PARTIALLY RESOLVED`** | Durability: designed, unimplemented. Tamper-evidence: still `OPEN` |

The tamper-evidence blocker is a chain, not a choice: a protected anchor requires
`AeroraPlatformKeyProtector` to have a backend, which requires a build host (`D-006`). An untested
crypto backend is worse than none (`WP2-C`), so nothing is implemented.

**The consequence that matters most for this contract:** implementing `prevHash`/`recordHash` does
**not** entitle a sink to report tamper-evidence. That is written into `AeroraAuditSink.h` beside
`isTamperEvident()`, because it is exactly the inference a future implementer would otherwise make.

### 2.10 Acceptance criteria

Each maps to a test in `aerora/test/Security/AeroraAuditSinkTest.cc`. **All written, none
executed.**

| # | Criterion | Test |
| --- | --- | --- |
| A1 | A default record is unstamped: zero sequence, zero monotonic, invalid `utc` | `_sequenceIsMonotonicAndNeverZero` |
| A2 | `nextSequence()` never returns zero and never repeats | `_sequenceIsMonotonicAndNeverZero` |
| A3 | `stampNow()` sets all three time fields; sequence strictly advances, clocks non-decreasing | `_stampNowSetsAllThreeTimeFields` |
| A4 | The records of one command are strictly orderable by `sequence`, in the documented type order | `_recordsOfOneCommandAreOrderableBySequence` |
| A5 | Every record states the enforcement posture as it was at composition | `_recordCapturesTheEnforcementPostureAtCompositionTime` |
| A6 | Every record carries the producer's loss count as of that record | `_recordCarriesTheProducersLossCountAtCompositionTime` |
| A7 | Every outcome renders to a distinct, non-`Unknown` string | `_outcomeToStringDistinguishesEveryOutcome` |
| A8 | `Accepted` is the only outcome that is not a loss | `_onlyAcceptedIsNotALoss` |
| A9 | No sink: drops counted, decision still reached, failure counter untouched | `_noSinkCountsADropAndStillDecides` |
| A10 | Failing sink: failures counted, **an allow stays an allow**, sink actually called | `_failingSinkCountsAFailedWriteAndDoesNotChangeTheDecision` |
| A11 | Each of the three failure outcomes is counted as a loss | `_eachFailureOutcomeIsCountedAsALoss` (3 rows) |
| A12 | Accepted writes increment nothing — the positive control | `_acceptedWritesAreNotCounted` |
| A13 | Counters survive sink replacement and removal without reset | `_lossCountersAreMonotonicAcrossSinkReplacement` |
| A14 | Drops and failures are attributed to separate counters; the total is their sum | `_dropAndFailureAreCountedSeparately` |
| A15 | A minimal implementor inherits neither durability nor tamper-evidence | `_defaultSinkReportsNeitherDurableNorTamperEvident` |

**Not asserted, deliberately:** durability, crash survival, tamper-evidence, retention, export.
There is no mechanism behind any of them, and a test passing against a stub would convert an open
decision into an apparently-met requirement — risk `R7-11`.

## 3. WP2-B — the QML-safe security status

Accessor: `aerora/src/Security/AeroraSecurityStatus.{h,cc}`. QML name: `AeroraSecurity`.

### 3.1 Why a projection rather than a type exposure

[04-subsystem-and-interface-completion-matrix.md](../05-product-completion/04-subsystem-and-interface-completion-matrix.md)
specifies this precisely: the security screens need "a deliberate minimal accessor (`D-UI-01`)
**rather than a type exposure**". `P7-SEC-004` requires security state not be QML-visible or
QML-writable, and the reason is concrete — `AeroraIdentityProvider`, `AeroraRoleResolver`,
`AeroraAuthorizationPolicy`, `AeroraCommandAuthorizer`, `AeroraAuditSink` and
`AeroraSigningKeyStore` are all credential-bearing, policy-bearing or write paths.

So `AeroraSecurityStatus` **declares no data member at all**. It holds no identity, role, policy,
key, sink, or pointer to any of them. Every property is computed on read from
`AeroraCommandAuthorizer`'s read-only predicates, none of which returns a security object. It is
the single, reviewable exception to "no security type carries a QML macro"; every other type in the
module stays free of `Q_OBJECT`, `Q_GADGET` and QML macros.

### 3.2 Exactly what it exposes

Six properties. All `READ`-only. No `WRITE`, no `Q_INVOKABLE`, no public slot.

| Property | Type | Notify | Reports |
| --- | --- | --- | --- |
| `enforcement` | `Enforcement` enum | `statusChanged` | `NotEnforced` or `EnforcedAtCommandGate` |
| `enforcementVerified` | `bool` | `CONSTANT` | **Always false.** No build, no execution, no flight |
| `auditSinkInstalled` | `bool` | `statusChanged` | Whether a destination exists |
| `auditDurable` | `bool` | `statusChanged` | False — no sink, and no sink self-reports durable |
| `auditTamperEvident` | `bool` | `statusChanged` | False — `Q-037` tamper-evidence is `OPEN` |
| `auditRecordsLost` | `qulonglong` | `statusChanged` | `lostAuditRecordCount()` |

### 3.3 What it does not expose, and cannot

| Not exposed | Why |
| --- | --- |
| Operator identity, `opaqueId`, roles, role ids | Credential and authorization content. `Q-029` has not even defined a role set |
| Any policy, resolver, provider or sink object | A pointer property is how a type exposure happens |
| Signing keys, key material, `protectsAtRest()` | Out of scope for `SCR-SHL-03`/`SCR-SEC-01`; `SCR-SEC-04` uses the upstream key manager |
| Any mutator for roles, permissions, policy, decisions or configuration | There is no method here at all to call |
| Coverage of `PARAM_SET`, mission writes, `MANUAL_CONTROL`, RC override | **Tab 9 owns the `PARAM_SET` investigation** (`P7-AUTH-003`) and `ADR-014` has not settled authorization scope. A coverage property would assert a result that investigation has not produced |
| Payload actuation, power switching, stabilization, failsafe actions | Prohibited. This is a status reader |

### 3.4 The three distinctions the type refuses to collapse

A status display is where overclaiming happens, so the shape makes the honest answer the easy one.

| Distinction | How it is kept |
| --- | --- |
| A mechanism existing ≠ a policy being configured | `enforcement` reports `NotEnforced` for "decision point present, nothing evaluating" — not unknown, not pending, not blank |
| A policy configured ≠ enforcement verified | `enforcementVerified` is a compile-time `false`, the same device as `kRemainingEnergyEstimationImplemented = false` |
| A sink installed ≠ a record being durable | Three separate properties, because they are three separate facts |

The enumerator `EnforcedAtCommandGate` **names its own scope**, so a GUI label derived from it
cannot widen the claim. The scope is the `MAV_CMD` traffic reaching
`QGCCorePlugin::authorizeMavCommand()`: `MavCommandQueue::sendWorker()` for queued commands and
`Vehicle::startCalibration()` for the one gated direct-send path (ADR-015). Nothing more.

There is deliberately **no `Unknown` enumerator**. The authorization chain is a process singleton
whose configuration is always readable, so `Unknown` could never be produced, and an unreachable
value invites a consumer to render a state that never occurs. Before `AeroraCorePlugin::init()`
runs, nothing is installed and `NotEnforced` is the accurate answer rather than a missing one. The
genuinely unresolved fact — that enforcement has never been observed — is carried by
`enforcementVerified`.

### 3.5 Change notification

`AeroraCommandAuthorizer` gains `setChangeNotifier(void (*)())`, a plain function pointer with no
captured state, so an observer registered once for the process lifetime cannot dangle. The
authorizer learns nothing about its observer — in particular nothing about QML — so the dependency
runs one way, from the projection to the chain.

`AeroraSecurityStatus`'s constructor registers `&AeroraSecurityStatus::notifyChanged`, which emits
`statusChanged()` on every live instance. Instances are tracked in a list, not a single pointer,
because `QML_SINGLETON` is per-engine: `QGCCorePlugin::createQmlApplicationEngine()` can be called
again, and a single-pointer registry would let a second instance silently take over notifications
from the first.

`setChangeNotifier()` fires once immediately, so an observer registering after the chain was
configured sees the current state rather than waiting for a change that may never come — the
shipped station installs no policy, so without this the first notification would never arrive.

**Consumer contract for Tab 7:** bind to the properties; they are live on read. `statusChanged()`
fires on configuration changes and on every counted audit loss. Do not poll. `enforcementVerified`
is `CONSTANT` — a binding on it is evaluated once, which is correct.

### 3.6 `AeroraUiSafetyTest` is unchanged, and why that is not a loophole

`WP2-B`'s constraint is that `AeroraUiSafetyTest` must still pass unchanged, and it does: the new
type adds no writable property, and `recordedWritablePropertyDeviations()` — now empty after Tab 4's
`U3` (`a7b317f89`) — stays empty.

But a new QML-reachable type that is simply absent from `auditedTypes()` would be unguarded, and
that would be a loophole. So the equivalent ratchet is implemented in
`aerora/test/Security/AeroraSecurityStatusTest.cc` instead, in Tab 2's own suite, and it is
*stricter* than the UI suite's:

| UI suite | This suite |
| --- | --- |
| Writable set must equal a recorded list | Writable set must be **empty**, with no exception mechanism |
| Non-signal methods must match a reviewed list | Non-signal methods must be **empty** — property getters are not slots and correctly never reach the meta-object |
| Forbidden names must not appear | Same, over security-specific names |
| — | **No property may be a pointer type**, asserted on `QMetaType` flags, so a pointer to a type nobody has written yet is caught too |

`AeroraUiSafetyTest.cc` was not edited. It belongs to Tab 7 and already carried the one sanctioned
cross-boundary edit this phase; adding this type to its audited set is `HO-TAB2-02`. **No assertion
in it was weakened or modified.**

### 3.7 Acceptance criteria

Each maps to a test in `aerora/test/Security/AeroraSecurityStatusTest.cc`. **All written, none
executed.**

| # | Criterion | Test |
| --- | --- | --- |
| B1 | An unconfigured station reports `NotEnforced`, cross-checked against the chain | `_unconfiguredStationReportsNotEnforced` |
| B2 | A configured chain reports `EnforcedAtCommandGate`; the enum has exactly two keys and the scope-naming enumerator | `_configuredChainReportsEnforcedAtTheCommandGate` |
| B3 | `enforcementVerified` is false even with the chain fully configured | `_enforcementIsNeverReportedAsVerified` |
| B4 | Sink presence and durability are reported separately; installing a sink does not make audit durable | `_auditPropertiesReportAbsenceAndNonDurabilitySeparately` |
| B5 | A lost record becomes visible and equals the chain's count; accepted writes do not raise it | `_auditRecordsLostIsVisibleAndMonotonic` |
| B6 | `statusChanged()` fires on a configuration change and on a counted loss | `_statusChangedIsEmittedOnConfigurationAndOnLoss` |
| B7 | No property is QML-writable, and properties do exist | `_noPropertyIsQmlWritable` |
| B8 | The type declares no invokable and no slot | `_theTypeDeclaresNoQmlCallableMethod` |
| B9 | No property is a pointer; all are bool, enum or `qulonglong` | `_noSecurityTypeIsReachableThroughAProperty` |
| B10 | 22 named security write paths and accessors are absent as methods and properties | `_forbiddenSecurityNamesAreNotQmlReachable` |

## 4. ADR-018 — an audit write failure does not block a command

Tab 2 reserves even ADR numbers for Phase 7. Recorded in a new document per the conflict-avoidance
rule rather than appended to the contended
[27-implementation-adrs.md](27-implementation-adrs.md).

| Field | Content |
| --- | --- |
| **Decision** | When a record cannot be written, the producer counts the loss, warns once, and returns the authorization decision unchanged. An unauditable **allow** stays an allow |
| **Status** | `ACCEPTED` for the interface. The underlying product question stays `OPEN` |
| **Context** | `P7-AUDIT-003` asks whether an unauditable allow must be denied, and records it as a **product** decision. `AERORA-GCS-REQ-025` requires every dropped record be accounted for |
| **Alternative rejected — deny** | Makes an audit outage a flight outage. A full disk would ground the aircraft. That is a safety-relevant policy choice and engineering may not make it |
| **Alternative rejected — allow silently** | The status quo before this pass, and the reason a storage failure was invisible |
| **Consequence** | The gap is counted, surfaced through `AeroraSecurityStatus.auditRecordsLost`, and written into each record as `recordsLostBeforeThis`. The decision stays available to product without a redesign: it would become a check at one place, `_audit()`'s caller |
| **Consistency** | Matches WP1-C's "deliberate asymmetry": an audit write failure is user-visible, an event write failure is not; **visible, not blocking** |
| **Pinned by test** | `_failingSinkCountsAFailedWriteAndDoesNotChangeTheDecision` asserts the allow survives. Changing the behaviour requires the product decision first, not a test edit |

## 5. What was actually checked

Run on this host, with results as observed. **No compiler, CMake, Qt, `clang-tidy`,
`clang-format`, `qmllint`, `ctest` or `pre-commit` exists here** (`D-006`).

| Check | Command | Result |
| --- | --- | --- |
| Overlay manifest cross-reference | `.kiro/tools/aerora-manifest-check.ps1` | **`problems found: 0`.** 127 declared entries resolve; **0 undeclared** sources; no duplicates; 18 self-registering tests, 18 `add_qgc_test` entries, all six checks clean. Run twice — see the note below |
| UI static check | `.kiro/tools/aerora-ui-static-check.ps1` | 15 files checked, **0 issues** (unaffected by this change) |
| UI parity check | `.kiro/tools/aerora-ui-parity-check.ps1` | **0 parity problems** (unaffected) |
| Test slot declaration/definition parity | Ad-hoc regex scan over both new suites | `AeroraAuditSinkTest` 15/15, `AeroraSecurityStatusTest` 10/10, **parity OK** |
| Line length, tabs, line endings, trailing newline | Ad-hoc scan, 11 changed files | **0 problems** after fixing 4 over-length doc comment lines |
| `Q_ASSERT`, `QTest::ignoreMessage`, fixed `qWait`, bare `qDebug()` | Ad-hoc scan, 11 changed files | **0 occurrences** |
| `expectLogMessage` argument type | Ad-hoc scan | All call sites pass `constexpr const char *`. No `QString` wrapper — the `HO-TAB1-01` defect class is absent from these files |
| `record()` override signature consistency | Tree-wide scan | All 5 overrides match the new pure virtual; the single call site captures the result |

The manifest check was run twice, and the two results differ for a reason worth recording. On the
first run it reported `problems found: 8`, including `AeroraAuditSinkTest` and
`AeroraSecurityStatusTest` as `NOT REGISTERED` / `NO CTEST ENTRY` — the expected signal for
`HO-TAB2-01`, which was still outstanding. Between the two runs Tab 1 applied the registration in
`aerora/CMakeLists.txt`, and the second run was clean. The other six problems on the first run were
Tab 3's `AeroraProfileRegistryTest` and `AeroraVehicleSetupTest`, in the same state for the same
reason, and were likewise resolved by Tab 1.

### What this does not establish

The checks above are text and file-existence analysis. They do not compile, instantiate a
`QMetaObject`, or run an assertion. In particular:

- **No test in either new suite has been executed.** Every acceptance criterion in
  [§2.10](#210-acceptance-criteria) and [§3.7](#37-acceptance-criteria) is *written*, not *met*.
- `moc` has never processed `AeroraSecurityStatus`. The meta-object assertions in `B7`–`B10` are
  predictions about what moc will generate, not observations of it.
- `AeroraSecurityStatus` is the first `Q_OBJECT` in `aerora/src/Security/`, so this module's
  `AUTOMOC` path is exercised for the first time at the first build.
- The three new `AeroraAuditEvent` fields change a struct two Phase 6 test suites construct. Those
  suites were updated for the `record()` signature and read, not compiled.

## 6. Traceability delta

[06-requirements-to-test-traceability.md](../05-product-completion/06-requirements-to-test-traceability.md)
is Tab 1's file. These are the rows this pass changes, filed as `HO-TAB2-03` rather than edited.

| Requirement | Was | Now | Test |
| --- | --- | --- | --- |
| `P7-AUDIT-001` decisions recorded with time/actor/subject/outcome | `AeroraAuditSink` / `TBW` / `CONTRACT-ONLY` | `IMPLEMENTED-UNCOMPILED` (record + producer); sink still `CONTRACT-ONLY` | `AeroraAuditSinkTest` A4–A6 |
| `P7-AUDIT-002` durability and tamper-evidence | `—` / `TBW` / `OPEN — Q-037` | **`PARTIALLY RESOLVED`** per WP1-C: durability `DESIGNED`, tamper-evidence `OPEN` | `AeroraAuditSinkTest` A15; `AeroraSecurityStatusTest` B4 |
| `P7-AUDIT-003` unauditable events counted | `AeroraAuditSink` / `TBW` / `CONTRACT-ONLY` | `IMPLEMENTED-UNCOMPILED`. ADR-018 records the non-blocking decision; the product question stays `OPEN` | `AeroraAuditSinkTest` A9–A14 |
| `P7-AUTH-002` station reports when not enforced | `isAuthorizationEnforced()` / `TBW` + `SCR-SHL-03` QML | `IMPLEMENTED-UNCOMPILED` with a C++ test; QML consumer still `DESIGNED` | `AeroraSecurityStatusTest` B1–B3 |
| `P7-SEC-004` security state not QML-visible | `no QML macros in Security/` / `AeroraUiSafetyTest` | Restated: **one** reviewed projection type exists; the invariant is now "no security type holding state is QML-visible" | `AeroraSecurityStatusTest` B7–B10 |
| `P7-SEC-005` security screen can say "not enforced" | `SCR-SEC-01` / `TBW` QML / `SPECIFIED-NOT-DESIGNED` | Read path `IMPLEMENTED-UNCOMPILED`; the screen remains Tab 7's and `SPECIFIED-NOT-DESIGNED` | `AeroraSecurityStatusTest` B1 |
| `P7-MOD-014` four distinct recorded events | `AeroraAuditSink` / `TBW` / `CONTRACT-ONLY` | Unchanged. The four event *types* exist; the autonomy events that would populate them do not (`P7-AI-004`) | — |
| `P7-CHK-005` checklist completion as audit evidence | `BLOCKED — Q-037` | Unchanged — still blocked. Durability being designed is not durability being implemented | — |
| `D-UI-01` QML-safe enforcement read path | `OPEN` | **`IMPLEMENTED-UNCOMPILED`** on the Tab 2 side; consumer contract in [§3.5](#35-change-notification) for Tab 7 | B1–B10 |

Also stale after this pass:
[04 §5](../05-product-completion/04-subsystem-and-interface-completion-matrix.md) lists
`AeroraAuditSink` at `AeroraAuditSink.h:72` as `CONTRACT-ONLY — neither durable nor tamper-evident`.
The line number has moved and the gap row 12 ("Durable, tamper-evident audit sink … `Q-037`") should
now read `Q-037` **partially resolved — durability designed, tamper-evidence blocked on `D-006`**.

## 7. Handoffs raised

Per the [handoff protocol](../05-product-completion/07-phase-7-work-packages-and-ownership.md#7-handoff-protocol).
`HO-TAB1-01` was already taken by Tab 1 this pass, so these are numbered `HO-TAB2-*`.

### `HO-TAB2-01` — register two CTest entries — **APPLIED**

| Field | Content |
| --- | --- |
| Owner | **Tab 1** |
| File | `aerora/CMakeLists.txt`, the `if(QGC_BUILD_TESTING)` block |
| Change | `add_qgc_test(AeroraAuditSinkTest LABELS Unit)` and `add_qgc_test(AeroraSecurityStatusTest LABELS Unit)` |
| Invariant at risk | `AERORA-GCS-REQ-025`, `P7-SEC-004`. Both suites self-register via `UT_REGISTER_TEST` and compile, so they are reachable with `--unittest:<name>`, but `ctest -L Unit` would not see them |
| Blocks | Execution of all 25 test functions once a build host exists |
| Tracked by | `.kiro/tools/aerora-manifest-check.ps1` checks 4 and 5. `AERORA_SECURITY_TEST_NAMES` in `aerora/src/Security/Security.cmake` is the request |
| Status | **CLOSED.** Applied and committed by Tab 1 in `47c15feee`, together with Tab 3's two. Tab 2 did not edit `aerora/CMakeLists.txt`, and the change is not part of Tab 2's commit |
| Still true | Registration is not execution. Both suites remain **written and never run** (`D-006`) |

### `HO-TAB2-02` — add `AeroraSecurityStatus` to the audited type set

| Field | Content |
| --- | --- |
| Owner | **Tab 7** |
| File | `aerora/test/UI/AeroraUiSafetyTest.cc`, `auditedTypes()` and `reviewedMethodsFor()` |
| Change | Append `&AeroraSecurityStatus::staticMetaObject` to `auditedTypes()`, and `{QStringLiteral("AeroraSecurityStatus"), {}}` to `reviewedMethodsFor()` — the empty list is correct, the type declares no non-signal method |
| Invariant at risk | `P7-AUTH-004`, `P7-SEC-004`. An eleventh QML-reachable Aerora type exists and is not in the UI suite's closure |
| Blocks | Nothing. `AeroraSecurityStatusTest` B7–B10 covers the same surface more strictly in the meantime, so this is defence in depth, not a gap |
| Tracked by | `AeroraSecurityStatusTest::_noPropertyIsQmlWritable`, `_theTypeDeclaresNoQmlCallableMethod`, `_noSecurityTypeIsReachableThroughAProperty` |
| No test entry to remove | — |
| Note | **Do not add a writable-property deviation.** The set is empty after `U3` and this type adds none |

### `HO-TAB2-03` — traceability and matrix rows

| Field | Content |
| --- | --- |
| Owner | **Tab 1** |
| File | `05-product-completion/06-requirements-to-test-traceability.md`; `04-subsystem-and-interface-completion-matrix.md` §5 and gap row 12; `08-risk-register-and-open-decisions.md` `Q-037` row |
| Change | Apply [§6](#6-traceability-delta) |
| Invariant at risk | Honesty risk 9 — test *registration* reported as test *coverage*. The new rows must stay `IMPLEMENTED-UNCOMPILED`, never `VERIFIED` |
| Blocks | Nothing |
| Tracked by | This document |

### Coordination, not handoffs

| Tab | Item | Position |
| --- | --- | --- |
| **Tab 9** | `PARAM_SET` / `P7-AUTH-003` | **No overlap created.** `AeroraSecurityStatus` exposes no coverage property, so nothing here asserts or pre-empts `WP9-A`'s assessment. When it completes, a coverage property becomes possible — Tab 9's result would be the input, not this type's assumption |
| **Tab 1** | `AeroraCorePlugin.*` | **Not edited.** `isAuthorizationEnforced()` is untouched; `AeroraSecurityStatus` reads `AeroraCommandAuthorizer` directly rather than routing through the plugin, so no core-plugin change is needed and no handoff is filed |
| **Tab 1** | WP1-C store | The `AuditRecords` writer is assigned to Tab 2 and is **not started**. [§2.8](#28-boundary-with-the-future-storage-implementation) is the mapping it will implement |
| **Tab 4** | `aerora/test/UI/AeroraUiSafetyTest.cc` | **Not edited.** `U3` has landed; no assertion was weakened or modified |
| **Tab 7** | `SCR-SHL-03`, `SCR-SEC-01` | Consumer contract in [§3.5](#35-change-notification). No GUI component was created or edited |

## 8. Open decisions and what they block

| ID | Decision | Owner | Effect on this work |
| --- | --- | --- | --- |
| `Q-029` | Operator role set | Product + operations | `enforcement` can only ever report `NotEnforced` in a shipped build. `AeroraAuthorizationPolicy` stays unimplemented — `WP2-C` |
| `ADR-014` | Outbound authorization scope | Architecture | No coverage property may exist. No retry-time check was added |
| `Q-037` | Audit integrity | Tab 2 | **Partially resolved.** Durability designed by WP1-C; tamper-evidence `OPEN`. No sink implemented |
| `P7-AUDIT-003` | Must an unauditable allow be denied? | **Product** | ADR-018 counts and does not block. Reversible at one place |
| `D-006` | Build host | User | `enforcementVerified` stays false. No test executed. Blocks the platform key protector, and through it tamper-evidence |
| `D-UI-02` | Contrast target | Tab 7 + product | Not this work. The status type carries no presentation |
| `D-DAT-02` | May an operator delete audit records? | Product + compliance | Affects the future writer, not this interface |

## 9. What was deliberately not done

| Not done | Why |
| --- | --- |
| `AeroraAuthorizationPolicy`, any role mapping, allowlist, permission or approval rule | `Q-029` is `OPEN`. A policy over an unapproved role set is an invented policy — `WP2-C` |
| A retry-time authorization check | Explicitly excluded. Authorization stays bounded to enqueue time |
| An `AeroraPlatformKeyProtector` backend | `D-006`. An untested crypto backend is worse than none |
| A sink implementation of any kind | `R7-11`. A mock sink would make `P7-AUDIT-001` read as met |
| A durability or tamper-evidence mechanism | `Q-037`. The interface reports both false and the contract forbids inferring either from a hash chain |
| A coverage property for `PARAM_SET` and other direct-send paths | Tab 9 owns that investigation. Asserting coverage would overclaim |
| Blocking a command on an audit write | ADR-018, and `P7-AUDIT-003` belongs to product |
| Any outbound audit transmission | `P7-DAT-004`, `P7-ARCH-010`. Local-first is preserved |
| Editing `AeroraCorePlugin.*`, `aerora/CMakeLists.txt`, `AeroraUiSafetyTest.cc`, or any GUI file | Other tabs own them. Three handoffs filed instead |
| Modifying `includedResumeCommands` | Prohibited |
| Any payload, power, stabilization or failsafe surface on the status type | Prohibited. It is a status reader |

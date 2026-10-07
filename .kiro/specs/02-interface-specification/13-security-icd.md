# Security Interface Control Document

Operator identity, the authorisation gate, MAVLink signing and key custody. This is the one document
in the set where the specification is complete and the implementation route is not: key storage at
rest has no seam, and saying so precisely is the deliverable.

Architecture in [security-architecture.md](../01-aerora-architecture/security-architecture.md).
Phase 3 adds the boundary contracts, including four boundaries across which **nothing is permitted
to cross**.

## Scope

| In scope | Out of scope, and where it lives |
| --- | --- |
| Operator sign-in, role resolution, authorisation decisions | The role set itself — Q-029 |
| The audit identity stamped on records | Record content and format — [14-audit-logging-icd.md](14-audit-logging-icd.md) |
| MAVLink signing key provisioning, storage, selection, rotation | Signature computation — upstream, retained unchanged |
| The prohibition on cloud involvement in authorisation | Cloud contracts — [15-cloud-integration-icd.md](15-cloud-integration-icd.md) |
| Dialect and dependency pinning as a supply-chain contract | Build mechanics — [23-build-readiness.md](23-build-readiness.md) |

**No credential format, no credential store format, no role name, no permission name and no key
length other than the one upstream already uses are specified here.** None is evidenced, and
Q-029 is open.

## The two gates are independent

The single most consequential contract in this document, and the one most easily collapsed by a
convenient implementation.

| Gate | Question it answers | Owner | Where |
| --- | --- | --- | --- |
| Operator authorisation | Is **this operator** permitted to request this? | Ground station | Before the link write |
| Vehicle authority | Will **the vehicle** accept it? | Vehicle | On the vehicle |

```text
operator action
      │
      ▼
  authorisation gate  ── denied ──►  audit record, no link write, no command exists
      │ permitted
      ▼
   link write
      │
      ▼
  vehicle authority   ── rejected ─►  audit record with the vehicle's reason
      │ accepted
      ▼
  reported effective change
```

Authorisation is **necessary and not sufficient**, per `AERORA-GCS-REQ-019`. A ground station that
treated a passed authorisation check as acceptance would be claiming vehicle authority it does not
have — the failure [09-command-authority-icd.md](09-command-authority-icd.md) exists to prevent.

A denied operation produces an audit record and **no command**. There is nothing on the wire to
acknowledge, so there is no acknowledgement; the absence must not be presented as a timeout.

## Interface contracts — identity and authorisation

| ID | Interface | Direction | Producer | Consumer | Crossing | Update semantics | Ack | Authority | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-SEC-001` | Operator sign-in | Operator to GCS | Operator | Security module | Credential material, form unspecified | Event, on operator action | Accepted or rejected, synchronously | Ground station | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-SEC-002` | Local credential validation | GCS internal | Security module | Local credential store | A validation request and its result | Event, per sign-in | Synchronous | Ground station | `BASELINE-ASSERTED` | `CONFIRMED` |
| `IF-SEC-003` | Role resolution | GCS internal | Security module | Authorisation decision point | The authenticated operator's role set | On sign-in, and on credential refresh | Synchronous | Ground station | `NOT-EVIDENCED` | `OPEN` |
| `IF-SEC-004` | Authorisation decision | GCS internal | Command origination path | Security module | Operation identifier, operator identity; returns permit or deny with reason | Event, **per command, before the link write** | Synchronous, mandatory | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-SEC-005` | Gated-operation set | Build and configuration to GCS | Aerora overlay declaration | Security module | The explicit set of operations requiring authorisation | Build time; **never derived from interface visibility** | — | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-SEC-006` | Authorisation denial event | GCS internal | Security module | Audit record writer | Operator identity, operation, denial reason, local time | Event, per denial | Written before the operator is told | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-SEC-007` | Audit identity | GCS internal | Security module | Audit record writer | The operator identity stamped on every record | Per record | — | Ground station | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-SEC-008` | Credential lifetime state | GCS internal | Security module | Operator display | Validity, expiry and refresh state as distinct values | On change | — | Ground station | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-SEC-009` | Credential reconciliation | Cloud to GCS | Cloud module | Security module | Refreshed credential and role data | Opportunistic, **never blocking** | Not required | Ground station | `BASELINE-ASSERTED` | `PROPOSED` |
| `IF-SEC-010` | Security-to-cloud dependency | GCS internal | — | — | **Nothing. No call from the Security module into the Cloud module exists** | — | — | Ground station | `GCS-DECIDED` | `PROPOSED` |
| `IF-SEC-011` | Advanced-UI toggle as an authorisation boundary | GCS internal | — | — | **Nothing. The toggle grants no permission to any operation** | — | — | Ground station | `GCS-VERIFIED` | `PROPOSED` |

| ID | Freshness semantics | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `IF-SEC-001` | Not a telemetry value; no staleness concept | A rejected sign-in grants nothing and is recorded; no partial session exists | `AERORA-SEC-REQ-011` | R-004 | Q-029 | `INTEGRATION` |
| `IF-SEC-002` | Credential validity is time-bounded; expiry is a reported state, not an inference | **Validation must succeed with all network access blocked.** A network error is not an authorisation outcome | `AERORA-SEC-REQ-011`, `AERORA-AWS-REQ-006` | R-004 | — | `INTEGRATION` |
| `IF-SEC-003` | Carried with the session; refreshed state replaces it wholly, never merges | An unresolvable role set grants **no** permissions; it does not fall back to a permissive default | `AERORA-SEC-REQ-013` | R-004 | Q-029 | `INTEGRATION` |
| `IF-SEC-004` | Decided at request time; a decision is not cached across a credential change | A gate that cannot reach a decision **denies**. An unavailable gate is a denied command, never a permitted one | `AERORA-SEC-REQ-012`, `AERORA-GCS-REQ-019` | R-004 | — | `INTEGRATION` |
| `IF-SEC-005` | Fixed for a build; reported with the build version | An operation absent from the set is **not** thereby ungated; the set is reviewed as a release artifact | `AERORA-SEC-REQ-013` | R-004 | Q-029 | `STATIC` |
| `IF-SEC-006` | Local timestamp, written before any upload | A record that cannot be written is itself an accounted-for drop, per `AERORA-GCS-REQ-025` | `AERORA-SEC-REQ-015`, `AERORA-SEC-REQ-019` | R-004 | — | `INTEGRATION` |
| `IF-SEC-007` | Stamped at record creation, not at upload | An unidentified operator cannot produce an attributable record; the condition is recorded, not papered over | `AERORA-SEC-REQ-019`, `AERORA-COMPLY-REQ-002` | — | Q-016 | `INTEGRATION` |
| `IF-SEC-008` | Expiry is displayed with its own time basis | Expired credentials deny; they do not silently extend | `AERORA-SEC-REQ-011` | — | — | `INTEGRATION` |
| `IF-SEC-009` | Reconciled data carries its retrieval time; stale cloud data is shown with its age | A failed refresh leaves the local credential in force until its own expiry. **A refresh failure is not a sign-out** | `AERORA-AWS-REQ-009`, `AERORA-SEC-REQ-011` | — | Q-030 | `INTEGRATION` |
| `IF-SEC-010` | — | Verified by architectural-property test, not by review alone | `AERORA-AWS-REQ-012` | — | — | `UNIT` |
| `IF-SEC-011` | — | The toggle continues to control visibility only. Combining it with a role is permitted; relying on it alone is not | `AERORA-SEC-REQ-014` | — | — | `STATIC` |

### Why two of these specify that nothing crosses

`IF-SEC-010` and `IF-SEC-011` are negative contracts. They exist because the positive form is the
natural implementation and the wrong one.

| Boundary | The convenient mistake | Why it is specified as empty |
| --- | --- | --- |
| Security to Cloud | Ask the service whether this operator may do this | Puts a network round trip on the authorisation path, so a cloud outage becomes an authorisation failure |
| Advanced-UI toggle | Hide the dangerous control behind the toggle | The toggle changes visibility; anyone at the keyboard can turn it on. It is a control one click deep |

The toggle finding is `GCS-VERIFIED`: upstream exposes it as a settings-backed visibility flag with
a core-plugin confirmation hook. Nothing about it authenticates anybody.

The cloud-independence direction is one-way and deliberate. The Cloud module may hand refreshed
material to the Security module (`IF-SEC-009`); the Security module never calls outward. This is the
first of the eight hidden-dependency traps enumerated in
[cloud-architecture.md](../01-aerora-architecture/cloud-architecture.md), and
`AERORA-TEST-REQ-005` makes it testable rather than aspirational.

## Interface contracts — MAVLink signing

Upstream's signing implementation is retained unchanged. Phase 1 assessed it as well engineered and
closer reading confirmed it. These contracts describe what Aerora **does with** it, which is where
the two findings live.

| ID | Interface | Direction | Producer | Consumer | Crossing | Update semantics | Ack | Authority | Evidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `IF-SEC-012` | Signing key provisioning | Provisioning to GCS | Provisioning procedure | Signing key store | A randomly generated raw key and its name | Event, per provisioning | Confirmed by key presence | Process | `GCS-VERIFIED` | `PROPOSED` |
| `IF-SEC-013` | Signing key storage at rest | GCS internal | Signing key store | Platform persistence | 32-byte key material and per-key last-used timestamp | On change | — | Ground station | `GCS-VERIFIED` | `OPEN` |
| `IF-SEC-014` | Signing key selection | Link to GCS | Inbound signed message | Signing key store | A signature matched against stored keys, **HMAC verified before the channel is configured** | Event, on first signed inbound message | Verification is the acknowledgement | Ground station | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-SEC-015` | Per-link signing state change | Operator to GCS to link | Operator | Link signing state machine | Enable or disable, with deferred confirm and atomic commit | Event, per operator action | Deferred confirm | Ground station | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-SEC-016` | Signing timestamp persistence | GCS internal | Link signing state machine | Platform persistence | Per-key last-used timestamp, for forward progress across restarts | On use | — | Ground station | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-SEC-017` | Unsigned-message acceptance policy | GCS internal | Aerora settings defaults | Link signing state machine | An explicit accept-or-reject policy for unsigned inbound messages | Configuration; evaluated per message | — | Ground station | `GCS-VERIFIED` | `OPEN` |
| `IF-SEC-018` | Signing key rotation | Provisioning to GCS | Provisioning procedure | Signing key store | Addition of a new key and removal of an old one, as separate operations | Event | Key presence | Process | `GCS-VERIFIED` | `PROPOSED` |
| `IF-SEC-019` | Settings-schema version change | Build to GCS | Build configuration value | Settings store, then signing key store | **Destruction of all stored settings, including every signing key, before any subsystem starts** | Event, on a version mismatch at startup | None. No key-specific warning is emitted | Ground station | `GCS-VERIFIED` | `CONFIRMED` |
| `IF-SEC-020` | Telemetry field validation | Link to GCS | Inbound MAVLink | Fact system via metadata bounds | Range and enumeration checks on every parsed field | Per message | — | Ground station | `GCS-VERIFIED` | `PROPOSED` |
| `IF-SEC-021` | Dialect and dependency pin | Build internal | Aerora build configuration | Build system | Immutable commit references for the dialect and every third-party dependency | Per release | Reproducible build | Process | `GCS-VERIFIED` | `PROPOSED` |

| ID | Freshness semantics | Failure behaviour | Requirements | Risks | Questions | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| `IF-SEC-012` | Not time-varying | A passphrase-derived key is **not** an acceptable substitute — see below | `AERORA-SEC-REQ-016` | R-024 | — | `AUDIT` |
| `IF-SEC-013` | Not time-varying | **Unresolved.** Keys persist unprotected; no seam exists to replace the backend. Release-gated, not implementation-gated | `AERORA-SEC-REQ-017` | R-005 | Q-010 | `AUDIT` |
| `IF-SEC-014` | Verified per inbound signature | A key that fails verification is not adopted; the channel is left unconfigured rather than hopefully configured | — | R-006 | Q-010 | `INTEGRATION` |
| `IF-SEC-015` | Per link, not global | A failed commit leaves the previous state in force; there is no half-enabled link | — | R-006 | Q-010 | `INTEGRATION` |
| `IF-SEC-016` | Monotonic per key; survives a restart with a skewed clock | A lost timestamp risks replay-window regression and is a provisioning concern, not a runtime recovery | — | R-006 | — | `UNIT` |
| `IF-SEC-017` | Evaluated per message, not cached | **Aerora has not set this default.** Upstream ships signing off; the Aerora value awaits Q-010 | `AERORA-SAFE-REQ-010` | R-006 | Q-010, Q-011 | `INTEGRATION` |
| `IF-SEC-018` | Two keys may be valid during a rotation window | Removing the old key before every station and vehicle carries the new one severs the link. Rotation is a fleet procedure, not a GCS action | `AERORA-SEC-REQ-017` | R-005 | Q-016 | `AUDIT` |
| `IF-SEC-019` | One-shot, at startup | **Treated as a key-destroying event.** A schema bump requires a recorded fleet reprovisioning plan before release | `AERORA-SEC-REQ-018`, `AERORA-BUILD-REQ-009` | R-023 | — | `AUDIT` |
| `IF-SEC-020` | Applies to the value, not to its freshness, which is `IF-TLM` business | An out-of-range field is marked invalid, not clamped and not used. **No GCS-side correction** | `AERORA-GCS-REQ-026`, `AERORA-GCS-REQ-029` | — | — | `UNIT` |
| `IF-SEC-021` | Per release artifact | A floating reference fails the release gate. Two dependencies currently track moving references | `AERORA-SEC-REQ-020`, `AERORA-BUILD-REQ-008` | R-011, R-020 | Q-005, Q-032 | `AUDIT` |

### What upstream provides, retained unchanged

All `GCS-VERIFIED`. Aerora inherits every row and modifies none.

| Element | Detail |
| --- | --- |
| Per-link state machine | Deferred-confirm enable and disable with atomic check-and-commit |
| Key auto-detection | Verifies the HMAC **before** configuring the channel |
| Timestamp persistence | Per-key last-used value, for forward progress across a restart with a skewed clock |
| Unsigned-acceptance policy | An explicit policy callback, not an implicit allow |
| Memory hygiene | Non-elidable wipes with a volatile-loop fallback; in-memory keys wiped on destruction |
| Key derivation | PBKDF2-HMAC-SHA256 at 600000 iterations |

The auto-detection design is the part worth noting: it verifies a signature before accepting a key,
rather than configuring a channel and seeing whether traffic flows. That ordering is a contract, and
`IF-SEC-014` records it as one.

### Provisioning: random raw keys, not passphrases

Both paths exist upstream. Aerora uses one of them and the reason is specific.

| Property | Passphrase-derived | Random raw key |
| --- | --- | --- |
| Iteration count | 600000, strong | Not applicable |
| Salt | **Fixed application-wide**, by design, so stations sharing a vehicle derive the same key | Not applicable |
| Entropy ceiling | The passphrase, minimum eight characters | Full key length |
| Precomputation reuse | A table built against that salt is reusable against **every** installation worldwide | None |
| Multi-station interoperability | Achieved by sharing the passphrase | Achieved by distributing the key |

The interoperability goal the fixed salt serves is real. Distributing a shared random key achieves
the same goal without leaving a reusable precomputation target. `AERORA-SEC-REQ-016` therefore
records provisioning as random raw keys — a configuration and procedural decision requiring **no
code change**, which removes R-024 entirely.

This is the cheapest mitigation in the whole programme: a procedure replaces a vulnerability.

### Key storage: the gap, stated plainly

| Observation | Status |
| --- | --- |
| Key material is serialised into the application settings store without encryption | `GCS-VERIFIED` |
| It is read back from the same location at startup | `GCS-VERIFIED` |
| The serialisation buffer is wiped after the write; the stored value is unprotected | `GCS-VERIFIED` |
| Key bytes are exportable as hex from the interface layer | `GCS-VERIFIED` |
| The storage class has no virtual members; its only friend declaration is a test class | `GCS-VERIFIED` |

So the implementation is scrupulous about memory and silent about storage, and **there is no seam**.
The backend cannot be replaced from the overlay.

| Route | Assessment |
| --- | --- |
| Upstream contribution adding a pluggable key-storage backend | **Preferred.** Benefits everyone and keeps Aerora core-edit free |
| An Aerora core edit replacing the storage backend | Acceptable fallback, and must be recorded as a real core edit |
| Random raw keys plus full-disk encryption and station handling procedures | Mitigates; does not remediate |
| Ship with keys in unprotected settings | **Not acceptable for a production release** |

The standing formulation is unchanged by Phase 3:

> **Zero QGC core edits are required by the current architecture.**
>
> **One QGC core edit remains a possible fallback for signing-key storage if upstream extensibility
> is unavailable.**

`IF-SEC-013` carries status `OPEN` for exactly this reason. R-005 is gated at `G-RELEASE`: it blocks
neither the architecture nor the implementation, and it does block shipping. Recorded as ADR-008.

The vehicle-side secure element does not close this. A secure element on the aircraft cannot protect
a key sitting in a file on the operator's laptop. `IF-VID-005` and Q-016 are a different problem that
happens to involve the same hardware, and conflating the two would let a real hardware capability
appear to close a gap it cannot reach.

### The settings-version coupling

`IF-SEC-019` is specified as an interface because it is one: a build-configuration value reaches key
material through the settings store.

```text
application startup
      │
      ├── stored settings version != build value  →  entire settings store cleared
      │                                               (before any subsystem starts)
      ▼
   signing key store loads, and finds nothing
```

| Property | Consequence |
| --- | --- |
| A build value silently destroys key material | A schema change becomes a fleet reprovisioning event |
| No key-specific warning exists | The operator sees a generic settings-upgraded notice |
| Aerora will change persisted settings shape, so Aerora will bump this value | Aerora will hit this |

Two obligations follow, and they are process obligations rather than code: `AERORA-SEC-REQ-018`
treats a schema bump as key-destroying, and `AERORA-BUILD-REQ-009` requires a recorded reprovisioning
plan. Both are **assigned** `AUDIT` because there is nothing to test — the behaviour is correct for
settings and wrong for keys, and only the release process can tell the difference. No audit has been
performed.

This coupling is also the strongest argument for moving key custody out of the settings store: the
two concerns have no business sharing a lifetime.

## Telemetry as untrusted input

The third architectural principle, expressed as a boundary.

| Property | Position | Evidence |
| --- | --- | --- |
| Every parsed field is bounds-checked against its Fact metadata | Required | `GCS-VERIFIED` mechanism |
| An out-of-range value is marked invalid | Required | `GCS-DECIDED` |
| An out-of-range value is **not** clamped into range | Required | `GCS-DECIDED` |
| An unknown enumeration value is displayed as unknown, with the raw value retained | Required | `GCS-DECIDED` |
| No ground-station correction, smoothing or interpolation makes a field plausible | Required | `AERORA-GCS-REQ-029` |

Clamping is the failure mode worth naming. A clamped value looks reasonable, and a reasonable-looking
wrong value is worse than an obviously wrong one, because it will be believed.

## Threat scope

Unchanged from Phase 2 and repeated because the out-of-scope column is what makes the in-scope column
meaningful.

| In scope | Treatment | Interfaces |
| --- | --- | --- |
| Unauthorised operator use of a station | Authorisation gate before the wire | `IF-SEC-001` to `IF-SEC-006` |
| Spoofed or replayed MAVLink | MAVLink 2 signing, retained unchanged | `IF-SEC-012` to `IF-SEC-018` |
| Malformed or hostile telemetry | Untrusted input, metadata bounds, no inference | `IF-SEC-020` |
| Inbound network attack | No listener exists | `IF-CLD` set |
| Repudiation of an operator action | Local-first audit with operator and request identity | `IF-SEC-006`, `IF-SEC-007`, `IF-LOG` set |
| Supply-chain substitution | Immutable pins, signed release artifacts | `IF-SEC-021` |

| Out of scope | Reason |
| --- | --- |
| A compromised operator host | Credentials and keys are readable by anything running as that user; an operational control |
| Physical access to the aircraft | Vehicle-side concern |
| Radio-layer jamming | Link arbitration is a vehicle-side function the ground station observes |
| Cloud backend security | Covered by the baseline's cloud requirements, not by ground-station architecture |

"A compromised operator host" being out of scope is precisely why R-005 matters less than it first
appears and still cannot ship. Unprotected key storage does not create the exposure. It makes the
exposure trivially harvestable.

## Conflicts recorded

| Conflict | Position A | Position B | Resolution authority |
| --- | --- | --- | --- |
| Signing default | Upstream ships signing off, and `AERORA-SAFE-REQ-010` does not set a default | The baseline's security posture implies signing on | **Unresolved.** Q-010 and Q-011; product and operations owners |
| Key custody route | `AERORA-BUILD-REQ-006` requires all Aerora code in the overlay | `AERORA-SEC-REQ-017` cannot be met from the overlay | **Recorded, not resolved.** ADR-008 keeps both positions; upstream contribution is preferred |

Neither is closed here. The second is the only place in the programme where two confirmed-direction
requirements genuinely cannot both be satisfied by the current code, and flattening it into a
preference would hide that.

## Not specified, and why

| Item | Reason |
| --- | --- |
| Operator roles and permission names | Q-029; a product and operations decision. The architecture accommodates any role set |
| Credential format and credential store format | Not evidenced; no baseline statement |
| Whether sign-in is per session, per station or per shift | Not evidenced; an operations decision |
| The default unsigned-message acceptance policy | Q-010, Q-011 |
| Audit record format and integrity mechanism | [14-audit-logging-icd.md](14-audit-logging-icd.md) and Q-037 |
| Key-storage protection mechanism | No seam exists; ADR-008 route undecided |
| Platform code-signing scope | Q-032 |
| Per-dependency licence position | Q-005; R-020 |

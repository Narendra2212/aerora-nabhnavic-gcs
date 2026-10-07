# Security Implementation

Operator identity through to the command gate, and **F3.1-01** treated as a first-class Phase 4
problem with the exact QGroundControl boundary named.

## The two independent gates

```text
Operator
   ↓
Authentication            M-Security   offline-primary
   ↓
Role Resolution           M-Authz      F3.1-04, see doc 14
   ↓
Authorisation Decision    M-Authz      per operation, before the wire
   ↓
Command Gate              M-Authority  nothing unauthorised reaches a link
   ↓
MAVLink
   ↓
Vehicle Authority         VEHICLE      an independent second gate
```

| Gate | Question | Owner |
| --- | --- | --- |
| Operator authorisation | May **this operator** request this? | Ground station |
| Vehicle authority | Will **the vehicle** accept it? | Vehicle |

Authorisation is necessary and **not sufficient** — `AERORA-GCS-REQ-019`. A denied operation produces
an audit record and **no command**; there is nothing on the wire to acknowledge, so the absence must
not be presented as a timeout.

## F3.1-01 — Signing-key storage

### The exact QGroundControl boundary

All `GCS-VERIFIED`, read at the baseline commit.

| Field | Value |
| --- | --- |
| Class | `MAVLinkSigningKeys` |
| Header | `src/MAVLink/Signing/MAVLinkSigningKeys.h:48` |
| Implementation | `src/MAVLink/Signing/MAVLinkSigningKeys.cc` |
| Form | Singleton via `instance()`; `QObject`; `QML_ANONYMOUS` |
| **Virtual members** | **None**, apart from the destructor |
| **Friend declarations** | **`friend class SigningTest`** — and nothing else |
| Settings group | `kSettingsGroup = "MAVLinkSigningKeys"` (`.h:131`) |
| Key path | `<group>/keys/<name>` (`kKeySubgroup`, `.h:133`) |
| Manifest | `<group>/manifest` — a `QStringList` of key names, no secrets (`.h:132`) |
| Timestamps | `<group>/timestamps/<name>` as `quint64` (`.h:134`) |
| Write | `.cc:250-277` — `settings.setValue(..., serialized)` where `serialized` is the **raw 32 bytes**, then `sync()` |
| Read | `.cc:404-414` — `settings.value(...).toByteArray()` → `MAVLinkSigning::makeSigningKey` |
| Capacity | `kMaxKeys = 64` (`.h:145`) |

### Public API, in full

| Method | Reachable from QML | Note |
| --- | --- | --- |
| `addKey(name, passphrase)` | **Yes** | PBKDF2-derived. Aerora does not use this path |
| `addRawKey(name, hexKey)` | **Yes** | **The Aerora provisioning path** |
| `generateRandomHexKey()` | **Yes**, static | `QRandomGenerator::system()` |
| `removeKey(name)`, `removeAllKeys()` | **Yes** | — |
| `isKeyInUse(name)` | **Yes** | — |
| `keyHexByName(name)` | **Yes** | **Exports key material as hex** |
| `keyAt`, `keyBytesByName`, `lastTimestamp`, `recordTimestamp(s)`, `flushAllTimestamps`, `tryDetectKey`, `keys()` | C++ | — |
| `setPbkdf2IterationsForTesting(int)` | Static | Test-only override |

### The derivation findings

| Property | Value | Location | Assessment |
| --- | --- | --- | --- |
| Algorithm | PBKDF2-HMAC-SHA256 | `.cc:160-161` | Appropriate |
| Iterations | `600'000` | `.h:141` | At or above current guidance. **Not the weak point** |
| **Salt** | `"QGroundControl-MAVLink-Signing-v1"` | `.h:139` | **A compile-time constant, identical in every installation worldwide** |
| Minimum passphrase | `8` | `.h:146` | The entropy ceiling |

The code comments state the fixed salt is deliberate, so that stations sharing one vehicle derive the
same key from the same passphrase. The goal is real; the means leaves a precomputation target
reusable against every installation.

**Aerora provisions random raw keys** — `AERORA-SEC-REQ-016`. That achieves the same
interoperability by distributing key material instead of a passphrase, requires **no code change**,
and removes R-024 entirely. It is the cheapest mitigation in the programme: a procedure replacing a
vulnerability.

### The storage gap

| Observation | Status |
| --- | --- |
| Keys persist as raw bytes in the settings store | `GCS-VERIFIED` |
| The serialisation buffer is wiped after the write; the **stored value** is not protected | `GCS-VERIFIED` |
| Key bytes are exportable as hex from QML | `GCS-VERIFIED` |
| The class has no virtual members and no injection point | `GCS-VERIFIED` |
| **There is no seam.** The backend cannot be replaced from the overlay | `GCS-VERIFIED` |

### Extension strategies, assessed

| # | Strategy | Core edit? | Assessment |
| --- | --- | --- | --- |
| 1 | **Upstream contribution: a pluggable key-storage backend** | No, for Aerora | **Preferred.** Benefits everyone, keeps the overlay core-edit free, well-scoped change |
| 2 | Aerora core edit replacing the two persistence functions | **Yes** | Acceptable fallback. Two functions, `.cc:250-277` and `.cc:404-414`. **Must be recorded as a real core edit** |
| 3 | Subclass `MAVLinkSigningKeys` from the overlay | — | **Impossible.** No virtuals, and `instance()` returns the concrete singleton |
| 4 | Exploit `friend class SigningTest` | — | **Rejected.** A production path through a test seam is worse than a core edit, and dishonest |
| 5 | Pre-populate the settings store externally with protected values | — | **Does not work.** The read path expects raw bytes |
| 6 | Random raw keys plus full-disk encryption and station handling procedures | No | **Mitigates, does not remediate.** Does not satisfy `AERORA-SEC-REQ-017` |
| 7 | Ship with keys in unprotected settings | No | **Not acceptable for a production release** |

### The required core modification, stated explicitly

If strategy 1 is not available, strategy 2 is required and is a genuine core edit. Phase 4 does not
hide it under "overlay".

| Field | Value |
| --- | --- |
| Files | `src/MAVLink/Signing/MAVLinkSigningKeys.cc` (and `.h` for the backend interface) |
| Functions | The persistence write at `.cc:250-277` and the load at `.cc:404-414` |
| Change shape | Replace direct `QSettings` access with calls to an abstract backend; provide a default backend preserving today's behaviour |
| Size | Two functions plus one small interface. Small, but **in the core tree** |
| Standing position | ADR-001's formulation is unchanged: **zero core edits required; one remains a possible fallback for signing-key storage** |
| Decision owner | Architecture, via ADR-008. **Phase 4 does not choose** |

### Security consequences while unresolved

| Consequence | Detail |
| --- | --- |
| Keys for every vehicle a station has paired with sit readable by anything running as that user | On a general-purpose operator laptop |
| "A compromised operator host" is explicitly **out of scope** | Which is why R-005 matters less than it first appears, and still cannot ship |
| The exposure is not created by this gap; it is made **trivially harvestable** | The honest framing |
| A settings-schema version change destroys every key | See below |

### Testing strategy

| Test | Method | Asserts |
| --- | --- | --- |
| Provisioning uses random raw keys | `AUDIT` | No passphrase-derived key exists in a release build |
| No key material reaches a log or an audit record | `UNIT` | A grep-style assertion plus a record-content test |
| Key export is operator-initiated only | `STATIC` | `keyHexByName` is reachable only from an explicit export action |
| Storage protection | **Unassignable** | **There is nothing to test until a mechanism exists.** `IF-SEC-013` stays `BLOCKED` |
| Schema-bump reprovisioning plan exists | `AUDIT` | Process check at release |

### Release gate

| Gate | Condition |
| --- | --- |
| `G-IMPL` | Not blocked. Implementation proceeds with keys in unprotected settings, recorded |
| `G-RELEASE` | **Blocked.** `AERORA-SEC-REQ-017` is unmet. Either strategy 1 lands upstream or strategy 2 is approved and recorded |

R-005 blocks neither the architecture nor the implementation, and it does block shipping.

## The settings-version coupling

A build-configuration value that destroys key material.

| Element | Value |
| --- | --- |
| Current value | `QGC_SETTINGS_VERSION` `"9"` — `cmake/CustomOptions.cmake:46-49` |
| Behaviour | A stored-version mismatch clears the **entire** settings store before any subsystem starts |
| Coupling | Signing keys live in that store |
| Consequence | **Incrementing it destroys every stored signing key**, with only a generic settings-upgraded notice |
| Aerora exposure | Aerora will change persisted settings shape, so Aerora will bump it |

| Obligation | Requirement | Verification |
| --- | --- | --- |
| Treat a schema bump as a key-destroying event | `AERORA-SEC-REQ-018` | `AUDIT` |
| Require a recorded fleet reprovisioning plan | `AERORA-BUILD-REQ-009` | `AUDIT` |

Both are `AUDIT` because there is nothing to test: the behaviour is correct for settings and wrong
for keys, and only the release process can tell the difference. `IF-SEC-019`; R-023.

## Authentication

| Property | Decision | Status |
| --- | --- | --- |
| Offline validation is the **primary** path, not a fallback | `AERORA-SEC-REQ-011`, `CONFIRMED` | `IMPLEMENTATION-PLANNED` |
| Nothing in the authorisation path blocks on the network | Required | `IMPLEMENTATION-PLANNED` |
| Cloud reconciliation is opportunistic and one-way inward | `IF-SEC-009` | `IMPLEMENTATION-PLANNED` |
| A refresh failure is **not** a sign-out | Required | `IMPLEMENTATION-PLANNED` |
| Credential format, store format, session scope | **Not specified** — not evidenced | `CONTRACT-ONLY` |

The Security module loads **before** any network interface is brought up, and is reachable from the
Cloud module but never the reverse — `AERORA-AWS-REQ-012`, the one dependency rule that already has
a test assigned.

## Telemetry as untrusted input

| Rule | Consequence |
| --- | --- |
| Every parsed field is bounds-checked against its Fact metadata | Mechanism is `GCS-VERIFIED` |
| An out-of-range value is marked invalid | Not used |
| An out-of-range value is **not clamped** | A clamped value looks reasonable, and a reasonable-looking wrong value will be believed |
| An unknown enumeration value is displayed as unknown, raw value retained | Never the nearest known value |
| No ground-station correction, smoothing or interpolation | `AERORA-GCS-REQ-029` |

## Interface status

| Interface | Subject | Status |
| --- | --- | --- |
| `IF-SEC-001`, `IF-SEC-002` | Sign-in, local validation | `IMPLEMENTATION-PLANNED` |
| `IF-SEC-003` | Role resolution | **`BLOCKED`** — F3.1-04, doc 14 |
| `IF-SEC-004` to `IF-SEC-009` | Gate, gated set, denial record, audit identity, lifetime, reconciliation | `IMPLEMENTATION-PLANNED` |
| `IF-SEC-010`, `IF-SEC-011` | Cloud dependency and advanced-UI toggle | `IMPLEMENTATION-PLANNED` as **prohibition assertions** |
| `IF-SEC-012` | Key provisioning | `IMPLEMENTATION-PLANNED` — procedural |
| `IF-SEC-013` | Key storage at rest | **`BLOCKED`** — F3.1-01 |
| `IF-SEC-014` to `IF-SEC-016` | Key selection, per-link state, timestamp persistence | Inherited unchanged |
| `IF-SEC-017` | Unsigned-acceptance policy | `CONTRACT-ONLY` — Q-010, Q-011 |
| `IF-SEC-018`, `IF-SEC-019` | Rotation, schema-version event | `IMPLEMENTATION-PLANNED` — procedural |
| `IF-SEC-020` | Telemetry field validation | `IMPLEMENTATION-PLANNED` |
| `IF-SEC-021` | Dialect and dependency pin | `IMPLEMENTATION-PLANNED` — build |

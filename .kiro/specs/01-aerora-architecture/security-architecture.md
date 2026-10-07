# Security Architecture

Operator identity and authorisation, MAVLink signing and key custody, the audit trail, and the one
place in this architecture where a core edit or an upstream contribution is genuinely required.

## Posture

| Property | Position |
| --- | --- |
| The GCS accepts no inbound network connections | Structural — see [cloud-architecture.md](cloud-architecture.md) |
| Telemetry is untrusted input | Third architectural principle; applies to every parsed field |
| Authorisation is a GCS-side gate before the wire | Never a post-hoc check |
| Authentication works offline | A cloud outage must not become an authorisation failure |
| Key material custody is an unresolved gap | Recorded below, gated at `G-RELEASE` |

## Operator identity and authorisation

### The Security module

The Security module is an Aerora domain module. It loads before any network interface is brought
up, per the startup sequence in [runtime-architecture.md](runtime-architecture.md), and it is
reachable from the Cloud module but never the reverse.

| Responsibility | Detail |
| --- | --- |
| Credential validation | Against locally cached credentials; no network round trip required |
| Role resolution | The authenticated operator's role set |
| Authorisation decisions | Whether a given operation is permitted to the current operator |
| Credential lifetime | Expiry and refresh state, with refresh initiated by the Cloud module |
| Audit identity | The operator identity stamped on every audit record |

### Offline-capable by design, not as a fallback

Offline validation is the primary path, not a degraded one. The distinction matters because a
"fallback" tends to be the less-tested path, and this one must work on every flight.

```text
operator sign-in
      │
      ├── validate against local credential store     ← always
      │
      └── Cloud module reconciles and refreshes        ← opportunistic
```

Nothing in the authorisation path blocks on the network. This is the first of the eight
hidden-dependency traps enumerated in [cloud-architecture.md](cloud-architecture.md), and it is the
one most likely to be introduced by a well-meaning change.

### The authorisation gate

| Property | Statement |
| --- | --- |
| The gate sits before the command is written to a link | An unauthorised command never reaches the wire |
| Authorisation is necessary, not sufficient | Vehicle authority is a second, independent gate |
| A denied operation is logged with the operator identity and the denial reason | Denials are audit events |
| The gated operation set is explicit | Not derived from UI visibility |

That last row is the important one and it leads directly to the next section.

### The advanced-UI toggle is not an authorisation boundary

QGroundControl's advanced-UI toggle reveals additional controls and shows a confirmation message
through a core-plugin virtual. It changes **visibility**. Anyone at the keyboard can turn it on.

**Aerora must not use the advanced-UI toggle as the mechanism protecting any operation that
requires an authorised operator.**

| Mechanism | What it does | What it is not |
| --- | --- | --- |
| Advanced-UI toggle | Reduces routine visual clutter | Not authentication, authorisation or access control |
| Operator role | Determines permitted operations | Not a display preference |

The two are independent and may be combined — an operation may require both an authorised role and
the toggle — but the toggle alone never suffices. Deriving the gated operation set from UI
visibility would make this mistake structural, which is why the gated set is declared explicitly in
the Security module rather than inferred from the interface.

The role set itself is Q-029 and is routed to the product and operations owners. The architecture
accommodates any role set, so this is shaping rather than blocking.

## MAVLink signing

### What upstream provides, and it is good

Phase 1 found QGroundControl's signing implementation well engineered, and closer reading confirms
it:

| Element | Detail |
| --- | --- |
| Per-link state machine | Deferred-confirm enable and disable, with atomic check-and-commit |
| Key auto-detection | Tries stored keys against an inbound signature, verifying the HMAC before configuring the channel |
| Timestamp persistence | Last-used timestamp persisted per key for forward progress across restarts with a skewed clock |
| Unsigned-acceptance policy | Explicit policy callback rather than an implicit allow |
| Memory hygiene | Non-elidable wipes using platform primitives, with a volatile-loop fallback; in-memory key bytes wiped on destruction |
| Key derivation | PBKDF2-HMAC-SHA256 at 600000 iterations, at or above current published guidance |

The auto-detection design is notably careful: it verifies the signature before accepting the key,
rather than configuring a channel and seeing whether traffic flows.

Aerora retains all of this unchanged.

### R-005: the persistence gap

Key material is persisted into application settings without encryption.

| Observation | Location |
| --- | --- |
| The 32-byte key is serialised and written to `QSettings` under a key-name subgroup | `MAVLinkSigningKeys.cc:248-278` |
| It is read back from the same location at startup | `MAVLinkSigningKeys.cc:399-412` |
| The serialisation buffer is wiped after the write, and in-memory keys on destruction | Careful in memory; the stored value is unprotected |
| Key bytes are exportable as hex from QML | An invokable accessor exists |

So the code is scrupulous about memory and silent about storage. The result is that on a
general-purpose operator laptop, the signing keys for every vehicle a station has ever been paired
with sit in a settings file readable by anything running as that user.

### A second finding: the fixed derivation salt

Passphrase-derived keys use a **fixed application-wide salt**, with a comment stating this is
deliberate so that several ground stations sharing one vehicle derive the same key from the same
passphrase. The minimum passphrase length is eight characters.

| Consequence | Assessment |
| --- | --- |
| The iteration count is strong | 600000 is not the weak point |
| The salt is constant across every installation worldwide | A precomputation against that salt is reusable against every Aerora station and every QGroundControl station |
| An eight-character minimum behind a known salt is the exposure | The entropy ceiling is the passphrase, and it is low |
| Raw-key entry and random key generation are both available | These paths avoid the problem entirely |

The interoperability goal the fixed salt serves is real — multiple stations do need the same key —
but a shared random key distributed as key material achieves it without a reusable precomputation
target. **Aerora provisions signing keys as randomly generated raw keys, not as passphrase-derived
keys.** That is a configuration and procedural decision requiring no code change, and it removes
the second finding entirely.

### A third finding: the settings-version coupling

The application clears **all** settings when the stored settings-schema version differs from the
build's value, before any subsystem starts:

```text
QGCApplication startup
      │
      ├── stored SettingsVersion != QGC_SETTINGS_VERSION  →  settings.clear()
      │                                                      (QGCApplication.cc:129-137)
      ▼
   subsystems start, MAVLinkSigningKeys::_load() finds an empty store
```

Signing keys live in that same settings store. Therefore **incrementing the settings-schema version
destroys every stored signing key.** The build's current value is 9.

| Property | Consequence |
| --- | --- |
| A build-configuration value silently destroys key material | A schema change becomes a fleet reprovisioning event |
| There is no warning specific to key loss | The user sees a generic settings-upgraded notice |
| Aerora will need to bump this value when it changes persisted settings shape | So Aerora will hit this |

This coupling is recorded in [configuration-architecture.md](configuration-architecture.md) as a
release-process obligation. It is also the strongest argument for moving key custody out of the
settings store, because the two concerns have no business sharing a lifetime.

### Remediation, honestly

There is **no seam**. `MAVLinkSigningKeys` is a singleton with no virtual members; its only friend
declaration is a test class. Key storage cannot be replaced from the overlay.

| Route | Assessment |
| --- | --- |
| Upstream contribution adding a pluggable key-storage backend | **Preferred.** Benefits everyone, keeps Aerora core-edit-free, and the change is well-scoped |
| An Aerora core edit replacing the storage backend | Acceptable fallback; it is a real core edit and must be recorded as one |
| Provision raw random keys and rely on full-disk encryption plus station handling procedures | Mitigates, does not remediate |
| Ship with keys in unprotected settings | Not acceptable for a production release |

This is the **only** place in the Aerora architecture where a core edit is a genuine candidate.
Everywhere else the answer has been a seam. The standard formulation applies:

> **Zero QGC core edits are required by the current architecture.**
>
> **One QGC core edit remains a possible fallback for signing-key storage if upstream extensibility
> is unavailable.**

R-005 is gated at `G-RELEASE` in [phase-2-risk-gates.md](phase-2-risk-gates.md): it does not block
the architecture or the implementation, and it does block shipping. Recorded as ADR-008.

### The vehicle-side secure element is not the answer to this

The flight controller carries a secure element. It is a genuine asset for device identity and
vehicle-side key custody, and it is relevant to how keys are provisioned across a fleet.

It does not address GCS-side key storage. A secure element on the aircraft cannot protect a key
sitting in a file on the operator's laptop. Conflating the two would let a real hardware capability
appear to close a gap it cannot reach.

## Audit trail

| Property | Statement |
| --- | --- |
| Every operator command is recorded with operator identity, request identity, and command state transitions | Per [authority-model.md](authority-model.md) |
| Every authorisation denial is recorded | With the operator identity and the denial reason |
| Every authority mode request and reported effective change is recorded | Request and effect separately |
| Records are written locally before any upload | Local write is the evidence |
| Records carry local timestamps | Not service clock time |

The local-first rule is a security property, not only a resilience one: a record that exists
independent of a successful network transfer cannot be suppressed by interfering with that
transfer. Detail in [logging-architecture.md](logging-architecture.md).

## Supply chain

| Concern | Position |
| --- | --- |
| MAVLink dialect | Aerora-controlled fork pinned to an immutable commit, never a branch |
| Upstream QGroundControl | Pinned fork point, merged deliberately — see [configuration-architecture.md](configuration-architecture.md) |
| Third-party dependencies tracking moving references | R-011; must be pinned before release |
| Release artifacts | Signed and versioned, per the baseline |
| Platform code signing | Q-032; the Windows gap is recorded in [deployment-architecture.md](deployment-architecture.md) |

Dialect pinning appears in three documents because it serves three different purposes:
reproducibility, compatibility-matrix validity, and supply-chain integrity. It is the same
mechanism answering all three.

## Threat scope

What this architecture addresses, and what it explicitly does not:

| In scope | Treatment |
| --- | --- |
| Unauthorised operator use of a station | Operator authorisation gate |
| Spoofed or replayed MAVLink | MAVLink 2 signing, retained unchanged |
| Malformed or hostile telemetry | Telemetry treated as untrusted; metadata bounds; no inference |
| Inbound network attack | No listener exists |
| Repudiation of an operator action | Local-first audit trail with operator and request identity |

| Out of scope | Reason |
| --- | --- |
| A compromised operator host | Key material and credentials are readable by anything running as that user; this is an operational control |
| Physical access to the aircraft | Vehicle-side concern |
| Radio-layer jamming | Link arbitration is a vehicle-side function the GCS observes |
| Cloud backend security | Covered by the baseline's cloud security requirements, not by GCS architecture |

Naming the out-of-scope items is the point. "A compromised operator host" being out of scope is
precisely why R-005 matters less than it first appears and still cannot be shipped: it does not
create the exposure, but it makes the exposure trivially harvestable.

## Core edits required

**One candidate, and only one across the whole architecture**: replacing the signing key storage
backend, if the upstream contribution route is not taken. Everything else in this document attaches
at an existing seam or is a configuration decision.

## Open questions

| Question | Subject | Impact |
| --- | --- | --- |
| Q-029 | Operator role set | `ARCH-SHAPING` |
| Q-010 | Default signing policy | `DEFERRABLE` |
| Q-016 | Fleet identity source, which the audit trail keys on | `ARCH-SHAPING` |
| Q-032 | Platform signing scope | `IMPL-BLOCKING` |
| Q-027 | The linked HTTP server module | `IMPL-BLOCKING` |

## Requirements derived

| Identifier | Statement |
| --- | --- |
| `AERORA-SEC-REQ-011` | Operator credential validation shall succeed without network access as the primary path |
| `AERORA-SEC-REQ-012` | The authorisation gate shall be applied before a command is written to a link |
| `AERORA-SEC-REQ-013` | The set of authorisation-gated operations shall be declared explicitly and shall not be derived from interface visibility |
| `AERORA-SEC-REQ-014` | The advanced-UI toggle shall not be used as an authorisation boundary for any operation |
| `AERORA-SEC-REQ-015` | An authorisation denial shall be recorded as an audit event with the operator identity and the denial reason |
| `AERORA-SEC-REQ-016` | Aerora signing keys shall be provisioned as randomly generated raw keys and not derived from operator passphrases |
| `AERORA-SEC-REQ-017` | A production release shall not persist signing key material without protection at rest |
| `AERORA-SEC-REQ-018` | A settings-schema version change shall be treated as a key-destroying event requiring fleet reprovisioning until key custody is moved out of the settings store |
| `AERORA-SEC-REQ-019` | Audit records shall carry operator identity, request identity and local timestamps, and shall be written locally before any upload |
| `AERORA-SEC-REQ-020` | The MAVLink dialect and all third-party dependencies shall be pinned to immutable references before release |

Full records in [requirements.md](requirements.md).

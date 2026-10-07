# Security Architecture

What security mechanisms actually exist in this checkout, and an honest gap list. This document is
deliberately unflattering, because the Aerora programme targets DGCA compliance readiness and
future high-assurance scalability, and an aspirational security baseline would be worse than none.

## Summary

| Domain | State |
| --- | --- |
| MAVLink message authentication | **Present and well engineered** — full MAVLink 2 signing stack |
| Link-layer encryption | **Absent** — all transports are plaintext |
| Application authentication and authorisation | **Absent entirely** — no users, roles, or sessions |
| Key storage at rest | **Weak** — plaintext in `QSettings`, no OS keychain |
| Update and firmware integrity | **Weak** — HTTPS transport only, no signed manifest |
| Code signing | **macOS and iOS only**; Windows, Android, and Linux unsigned by the build |
| Build and supply-chain hardening | **Good** — hardening flags, sanitizers, fuzzing, CodeQL, SBOM attestation |
| Secrets management | **Absent** |

## What exists

### MAVLink 2 message signing

The only real cryptographic mechanism in the product, and it is a complete implementation. Full
architecture in [mavlink-architecture.md](mavlink-architecture.md). Summary of the security-relevant
properties:

| Component | File | Security role |
| --- | --- | --- |
| `MAVLinkSigning` | `src/MAVLink/Signing/MAVLinkSigning.{h,cc}` | Primitives, signing epoch, timestamp ticks, `UnsignedAcceptancePolicy{Strict, Pending}`, signature verification |
| `SigningChannel` | `src/MAVLink/Signing/SigningChannel.{h,cc}` | Per-channel `mavlink_signing_t` behind a read-write lock |
| `SigningController` | `src/MAVLink/Signing/SigningController.{h,cc}` | Per-link state machine for `SETUP_SIGNING`, 5 s confirmation timeout, cancel on primary-link change, bad-signature burst detector at threshold 3, 1 Hz outgoing timestamp wall-clock refresh |
| `MAVLinkSigningKeys` | `src/MAVLink/Signing/MAVLinkSigningKeys.{h,cc}` | Named key bag with auto-detection by trying stored keys against an incoming signature |
| `VehicleSigningController` | `src/Vehicle/VehicleSigningController.cc` | Per-vehicle driver, sends `SETUP_SIGNING` twice for lossy-link redundancy |

Cryptographic parameters:

| Parameter | Value |
| --- | --- |
| Derivation | PBKDF2-HMAC-SHA256 via `QPasswordDigestor` |
| Iterations | 600000 — documented as the OWASP 2023+ floor, roughly 250 ms per key on a 2024 laptop |
| Key size | 32 bytes |
| Random key generation | `generateRandomHexKey()`, cryptographically random |
| Memory hygiene | Passphrase buffers wiped with `QGC::secureZero` (`src/Utilities/.../SecureMemory.h`) |
| Limits | `kMaxKeys = 64`, `kMinPassphraseLength = 8` |

Protocol hygiene, verified in code:

- `SETUP_SIGNING` is never forwarded to another link (`MAVLinkProtocol.cc:192`, `213`).
- `SETUP_SIGNING` is never written to telemetry logs (`.cc:232-235`), with a comment noting it
  contains the secret key.
- All logged packets are signature-stripped via `serializeUnsignedCopy`.
- Forwarded packets are signature-stripped so a foreign key does not cause `BAD_SIGNATURE`
  downstream.
- Signing is compiled in: `MAVLINK_NO_SIGN_PACKET` and `MAVLINK_NO_SIGNATURE_CHECK` are present
  but commented out in `src/MAVLink/MAVLinkLib.h:4-5`.
- All outbound sends route through the single `LinkInterface::sendMessageThreadSafe` chokepoint
  specifically so signing cannot be bypassed.

The timestamp handling deserves credit: a 1 Hz timer catches the signing timestamp up to wall clock
because libmavlink only bumps it per packet, so idle outbound paths would otherwise sign with a
stale value and be rejected by peers pinning signing timestamps to wall clock. The code cites the
upstream issue.

### Build and supply-chain hardening

| Mechanism | Location |
| --- | --- |
| Global hardening flags | `cmake/modules/Hardening.cmake`, applied via `qgc_apply_global_hardening()` |
| Sanitizers | `cmake/modules/Sanitizers.cmake` — ASan, UBSan, TSan, MSan, with CI lanes |
| Warnings as errors | `QGC_ENABLE_WERROR` ON by default |
| Static analysis | `QGC_ENABLE_CLANG_TIDY`, plus clazy and four clang-tidy shards in `analysis.yml` |
| Fuzzing | `.clusterfuzzlite/` plus `test/Fuzz/` |
| CodeQL | `.github/codeql/codeql-config.yml`; C++ built and uploaded by the Linux leg, Actions/Java/Kotlin/Python by the standalone workflow |
| Dependency pinning | Most CPM packages pinned to a commit or tag |
| Checksum verification | `GStreamer_REQUIRE_CHECKSUM` ON by default, fails closed; `URL_HASH` for the android-openssl archive |
| SBOM and attestation | `.github/actions/attest-and-upload`, `.github/actions/attest-sbom`; provenance and SBOM attestations attached to published image digests on trusted upstream pushes |
| Dependency review | `.github/workflows/dependency-review.yml`, OpenSSF Scorecard |
| Secret scanning | `trufflehog` pre-commit hook with `--only-verified` |

Code-quality hooks with security relevance: `check-no-qassert` forbids `Q_ASSERT` in production
code (it compiles out in release builds, so an assertion is not a check), and `vehicle-null-check`
enforces null-checking `Vehicle*` access.

### Code signing and notarisation

**macOS and iOS only.** `cmake/install/SignMacBundle.cmake` hard-requires
`QGC_MACOS_SIGNING_IDENTITY`, `QGC_MACOS_NOTARIZATION_USERNAME`, `_TEAM_ID`, and `_PASSWORD`
environment variables. It signs dynamic libraries, GStreamer framework internals, all frameworks,
then the bundle with `codesign --timestamp --options=runtime` (hardened runtime), verifies with
`codesign --verify --deep --strict`, then submits to `xcrun notarytool submit --wait`.

Non-release and development builds use ad-hoc signing
(`cmake/install/Install.cmake:247-255`). CI imports certificates via
`apple-actions/import-codesign-certs` in `macos.yml:142` and `ios.yml:98`, and
`.github/scripts/ios_package.py:73` verifies an embedded provisioning profile.

macOS sandboxing entitlements exist at `deploy/macos/qgroundcontrol.entitlements`.

### Download integrity plumbing

`QGCFileDownload` (`src/Utilities/Network/QGCFileDownload.{h,cc}`) supports `setExpectedHash()`
and streaming decompression. Networking routes through
`QGCNetworkHelper::createNetworkManager/createRequest`.

Credential hygiene in logging: `QGCFileDownload` logs URLs with
`RemoveUserInfo|RemoveQuery|RemoveFragment`, so credentials embedded in a URL are not echoed.
`LogManager` has a token-bucket rate limiter, which mitigates log-flooding as a denial-of-service
or evidence-destruction vector.

## Gap list

Stated plainly. Each item is a real gap in this checkout, not a hypothetical.

### 1. No application authentication or authorisation at all

**Severity: high for the Aerora programme.**

There is no login, no user accounts, no roles, no session management, and no operator-permission
model anywhere in `src/`. The GCS trusts whoever runs the binary. A search of the tree found
nothing auth-related beyond MAVLink signing.

This is reasonable for a single-operator open-source GCS and unacceptable for the Aerora targets
of DGCA compliance readiness, audit logging, role separation, and future high-assurance
scalability. **Aerora must build this, and it is a new module, not an extension.**

### 2. Signing key storage is plaintext in QSettings

**Severity: high.**

`MAVLinkSigningKeys.h` persists key bytes under `MAVLinkSigningKeys/keys/<name>`, with a
names-only `manifest` and per-key `timestamps/<name>`. There is no OS keychain or
credential-store integration and no encryption at rest. Any local process or user with filesystem
access to the settings file can read the keys.

On Windows that is under the roaming application data directory. The key material protecting
MAVLink authenticity is therefore only as strong as filesystem access control.

### 3. Fixed, hard-coded PBKDF2 salt

**Severity: medium.**

`kPbkdf2Salt = "QGroundControl-MAVLink-Signing-v1"`, deliberately fixed so the same passphrase
derives the same key across installations, which is what lets multiple GCS stations share one
vehicle. The header comment documents the intent and warns that bumping the version suffix
invalidates all stored passphrase keys.

The consequence is that precomputation and rainbow-table attacks are viable against weak
passphrases, and the minimum length is only 8. The 600000 iterations raise the cost per guess but
do not restore per-install uniqueness.

Aerora should prefer `addRawKey` with `generateRandomHexKey` over passphrase derivation, and
treat key distribution as a managed process. `PROPOSED`.

### 4. Signing is opt-in and off by default

**Severity: high.**

An unsigned or spoofed MAVLink stream is accepted unless the operator explicitly enables signing
for that link, and the unsigned-acceptance policy is configurable. There is **no link-layer
encryption anywhere** — serial, UDP, and TCP links are plaintext, so MAVLink traffic is readable
and injectable by anyone on the path.

For Aerora this intersects directly with the LoRa, FHSS24, and RF433 transports that do not yet
exist. Whether those transports provide their own link-layer security, and whether Aerora mandates
signing-on by default, are `OPEN` decisions recorded in
[open-questions.md](open-questions.md).

### 5. No signature verification of updates or downloaded payloads

**Severity: high.**

`QGCVersionCheck.cc` downloads a plain `QGC.version.txt` over HTTPS and compares strings. There is
no signed manifest.

Firmware images are fetched over HTTPS and flashed —
`FirmwareUpgradeController.cc` uses the PX4 GitHub releases API and the ArduPilot manifest.
`QGCFileDownload::setExpectedHash` exists, but **no evidence was found that it is populated for
the update or version-check path**.

There is no in-application auto-updater with signature validation.

Aerora requires software update integrity and signed artefacts. This is new work.

### 6. Windows, Android, and Linux binaries are not signed by the build

**Severity: medium to high depending on distribution model.**

| Platform | State |
| --- | --- |
| Windows | **Unsigned.** The only signing material is a *plan* document, `deploy/windows/qgc-windows-store-plan.md`, proposing AzureSignTool with Key Vault. No `signtool` or AzureSignTool step exists in `cmake/` or the Windows workflow |
| Android | **Off by default.** `QT_ANDROID_SIGN_APK` and `QT_ANDROID_SIGN_AAB` are OFF, relying on externally supplied keystores |
| Linux | No GPG or package signing step found for AppImage, DEB, or RPM |
| macOS / iOS | Signed and notarised — see above |

### 7. Video credentials handled in-URL

**Severity: medium.**

`GstSourceFactory.cc:327-333` reads `QUrl::userName()` and `QUrl::password()` and passes them to
GStreamer as `user-id` and `user-pw`. RTSP credentials therefore live in the plaintext settings
store as part of the URL.

### 8. No custom TLS policy

**Severity: low to medium.**

A search found no `QSslConfiguration`, `sslErrors`, or `ignoreSslErrors` usage in `src/`, so HTTPS
behaviour is whatever Qt's defaults are: system trust store, no certificate pinning, no explicit
minimum TLS version.

**Caveat on confidence:** that search was truncated by unrelated matches, so treat "no custom TLS
handling" as strongly indicated rather than exhaustively proven. Recorded as `UNKNOWN` pending
confirmation.

### 9. Qt6::HttpServer is linked, purpose untraced

**Severity: unknown — requires investigation.**

`Qt6::HttpServer` is a required component in the root `find_package(Qt6 ...)` and is linked in
`src/CMakeLists.txt`. **Which feature uses it, and whether any locally exposed HTTP endpoint is
authenticated, was not determined.**

Any network-exposed endpoint without authentication is a security concern. This is flagged as an
explicit action item in [open-questions.md](open-questions.md) and should be resolved before any
Aerora security claim is made.

### 10. No secrets management, encrypted settings, or runtime tamper detection

There is no secrets abstraction, settings are stored unencrypted via `QSettings`, and there is no
integrity check on configuration or on the binary at runtime. Aerora's configuration-integrity and
mission-integrity requirements have no existing foundation to build on.

### 11. Reproducibility gaps with security impact

Two dependencies are not pinned to immutable references:

| Package | Reference | Location |
| --- | --- | --- |
| `ArduPilotParams` | branch `main` — **floating** | `src/FirmwarePlugin/APM/CMakeLists.txt:41` |
| `qtandroidextensions` | tag `qt6-beta1` — mutable | `src/Android/CMakeLists.txt:36` |

A floating reference means two builds of the same QGC commit can embed different third-party
content, which defeats build reproducibility and makes an SBOM a point-in-time claim rather than a
fact. Recorded in [risk-register.md](risk-register.md) and [dependency-map.md](dependency-map.md).

## Threat-surface observations for Aerora

Not a threat model — the v4.1 baseline has `docs/29_THREAT_MODEL.md` for that. These are
observations from the code that should feed it.

### The command authority surface

The product baseline requires that the GCS never falsely represent a proposed GPU or autonomy
command as an effective aircraft command, and that command freshness, authority, acceptance, and
rejection remain explicit.

QGC has partial raw material for this, which Aerora should build on rather than replace:

| Mechanism | Location | Relevance |
| --- | --- | --- |
| `MavCommandQueue` ack tracking with `MAV_RESULT_IN_PROGRESS` handling | `src/Vehicle/MavCommandQueue.{h,cc}` | Distinguishes sent from accepted |
| `commandResult` signal carrying `ackResult` and `failureCode` | `MavCommandQueue.h:71` | Rejection is observable |
| The control-status handler block in `Vehicle.h` — `gcsMain`, `gcsControlStatusFlags_SystemManager`, `gcsControlStatusFlags_TakeoverAllowed`, `firstControlStatusReceived`, `requestOperatorControl`, `startTimerRevertAllowTakeover` | `Vehicle.h:1160-1190` region | **An existing operator-control authority model.** This is the closest thing in QGC to the Aerora authority requirement and should be studied before designing anything new |
| `HealthAndArmingCheckReport` | `src/Vehicle/` | Vehicle-reported readiness |
| `prearmError` with a timeout | `Vehicle.cc:2260+` | Vehicle-reported refusal |

The `requestOperatorControl(bool allowOverride, int requestTimeoutSecs)` and
`gcsControlStatusFlags_TakeoverAllowed` mechanism is a genuine multi-GCS authority handshake. It is
the right foundation for the Aerora authority model, and it was not previously identified in the
v4.1 baseline. Recorded as a finding in [aerora-extension-map.md](aerora-extension-map.md).

### Local-first is already the QGC architecture

Worth stating because it aligns with a hard Aerora requirement. QGC has **no cloud dependency for
flight control**. The only outbound network calls found are the version check, firmware download,
map tiles, terrain tiles, and NTRIP corrections — all optional, none on the command path. Aerora's
requirement that AWS never become a mandatory dependency for flight safety is therefore consistent
with the inherited architecture rather than fighting it.

The corollary: **any Aerora cloud module must be additive and failure-tolerant**, and must not be
placed on the MAVLink command path. That is an architectural constraint for the
`07-cloud-aws` specification.

## What Aerora must build

`PROPOSED`. Ordered by dependency, not priority.

| Capability | Foundation in QGC | Verdict |
| --- | --- | --- |
| Operator authentication | None | **New module** |
| Role-based authorisation | None; `showAdvancedUI` is a UI toggle, not authorisation | **New module** |
| Audit logging | `LogManager` with rate limiting | **Extend** |
| Secure key storage | `MAVLinkSigningKeys` with plaintext `QSettings` | **Replace the storage backend** |
| Signed software update | `QGCFileDownload::setExpectedHash` exists but is unused on that path | **New module** |
| Signed artefacts, Windows and Android | SBOM attestation exists; platform signing does not | **New build work** |
| Configuration integrity | None | **New module** |
| Mission integrity | Plan file is plain JSON with no signature | **New work** |
| Secure communications | MAVLink signing only; no link encryption | **Transport-layer decision, `OPEN`** |
| Command authority model | `requestOperatorControl` and control-status flags | **Extend — this is the right base** |
| Traceability | None | **New, see** [traceability.md](traceability.md) |

No Aerora security claim should be made on the basis of inherited QGC behaviour. The inherited
posture is appropriate for an open-source single-operator GCS and is not sufficient for the stated
Aerora targets.

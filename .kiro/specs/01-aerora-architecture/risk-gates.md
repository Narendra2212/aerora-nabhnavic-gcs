# Risk Gates

Risks that Phase 2 discovered, with their gate assignments. The twenty Phase 1 risks and their
re-gating are in [phase-2-risk-gates.md](phase-2-risk-gates.md); this register holds only what the
architecture work newly surfaced.

## Gate vocabulary

Unchanged from [phase-2-risk-gates.md](phase-2-risk-gates.md).

| Gate | Meaning |
| --- | --- |
| `G-ARCH` | Must be resolved before the architecture is ratified |
| `G-IMPL` | Must be resolved during implementation, before the affected code is written |
| `G-INTEG` | Must be resolved at hardware or firmware integration |
| `G-RELEASE` | Must be resolved before a production release |
| `G-OPEN` | Carries no gate; tracked for awareness |

## Summary

| Gate | Count | Risks |
| --- | --- | --- |
| `G-ARCH` | 0 | — |
| `G-IMPL` | 5 | R-021, R-022, R-025, R-026, R-028 |
| `G-INTEG` | 0 | — |
| `G-RELEASE` | 2 | R-023, R-024 |
| `G-OPEN` | 2 | R-027, R-029 |
| Total | 9 | |

**No Phase 2 risk is gated at `G-ARCH`.** Combined with the Phase 1 re-gating, the architecture can
be ratified: nothing discovered in Phase 2 invalidates a structural decision. Two new risks block
release, and both are in the key-custody area that
[ADR-008](architecture-decisions.md#adr-008-security-architecture) already identifies as the
architecture's single unremediated gap.

## R-021 Overlay test reachability is unverified

| Field | Value |
| --- | --- |
| Severity | `MEDIUM` |
| Gate | `G-IMPL` |
| Source | Build-ordering analysis during Phase 2 |
| Related | Q-035, `AERORA-BUILD-REQ-006`, `AERORA-TEST-REQ-009` |

Aerora code belongs in the overlay. The test tree is a separate subdirectory added after the
overlay, and the preprocessor definition marking a test-capable build is applied between the two.
Whether a test registered from overlay sources is reachable by the harness depends on build-ordering
details that could not be verified, because the host has no build toolchain.

**Impact if unresolved.** A test suite that silently does not run is worse than no test suite,
because it produces a passing gate. Every architectural-property test in
[testing-architecture.md](testing-architecture.md) depends on this.

**Mitigation.** Resolve it as the scaffold's first proof: configure with testing enabled, build, and
confirm an Aerora test appears in the registered-test listing. One hour of work once a build host
exists. The fallback — an Aerora-owned subdirectory of the test tree — is known to work and costs a
documented exception to the single-overlay rule.

## R-022 Flight log auto-upload default targets a third-party endpoint

| Field | Value |
| --- | --- |
| Severity | `HIGH` |
| Gate | `G-IMPL` |
| Source | QGroundControl source reading in Phase 2 |
| Related | `AERORA-GCS-REQ-023`, `AERORA-GCS-REQ-024`, R-015 |

Automatic upload of flight logs is enabled by default and the destination defaults to an upstream
public log-review service. The only thing preventing transmission is that the operator email
address defaults to empty, and the upload loop skips files when it is blank.

Two details make it worse than it first reads: setting the upload URL to empty resets it to the
upstream default, so clearing the URL does not disable upload; and the gate is an unrelated field,
so a contributor filling in an email address for a different reason enables transmission.

**Impact if unresolved.** Aerora flight data posted to a public third-party service. This is a
data-governance and customer-confidence exposure, not a flight-safety one, which is why it is
`HIGH` rather than a blocker.

**Mitigation.** Disable auto-upload explicitly through the Aerora settings defaults. Do not rely on
the empty email field. Assert the setting in a test, and note that the all-network-blocked test
would also observe the attempted connection.

## R-023 A settings-schema version bump destroys every signing key

| Field | Value |
| --- | --- |
| Severity | `HIGH` |
| Gate | `G-RELEASE` |
| Source | QGroundControl source reading in Phase 2 |
| Related | R-005, `AERORA-SEC-REQ-018`, `AERORA-BUILD-REQ-009`, ADR-008 |

The application clears all persisted settings when the stored settings-schema version differs from
the build's value, before any subsystem starts. Signing keys are persisted in that same store.
Therefore incrementing a build-configuration integer destroys the key material for every vehicle a
station has been paired with.

Aerora will change its persisted settings shape, so Aerora will reach this.

**Impact if unresolved.** A routine build edit becomes a fleet reprovisioning event, discovered
after the fact, with no key-specific warning — the operator sees a generic settings-upgraded
notice.

**Mitigation, in order of preference.**

| Mitigation | Effect |
| --- | --- |
| Move key custody out of the settings store | Removes the coupling entirely, and also resolves R-005 and R-024 |
| Treat a version bump as a release decision with a recorded reprovisioning plan | Procedural; the interim position |
| Add an Aerora key-loss warning on the settings-upgraded path | Reduces surprise; does not prevent loss |

The first mitigation resolves three risks at once, which is the strongest argument for doing it.

## R-024 The key-derivation salt is fixed across all installations

| Field | Value |
| --- | --- |
| Severity | `MEDIUM` |
| Gate | `G-RELEASE` |
| Source | QGroundControl source reading in Phase 2 |
| Related | R-005, `AERORA-SEC-REQ-016`, ADR-008 |

Passphrase-derived signing keys use a fixed application-wide salt, with a comment stating this is
deliberate so that several stations sharing one vehicle derive the same key from the same
passphrase. The minimum passphrase length is eight characters.

The iteration count is strong and is not the weak point. The constant salt is: a precomputation
against it is reusable against every installation of QGroundControl and every Aerora station
worldwide, and the entropy ceiling is an eight-character passphrase.

**Impact if unresolved.** A vehicle's signing key becomes recoverable from a captured signed packet
by an attacker who has precomputed against a known salt and a weak passphrase.

**Mitigation.** Provision signing keys as randomly generated raw keys rather than passphrase-derived
keys. The interoperability goal the fixed salt serves — several stations, one vehicle — is met by
distributing a shared random key as key material. **No code change is required**, which makes this
the cheapest mitigation in either risk register.

Severity is `MEDIUM` rather than `HIGH` because the mitigation is complete and costless; it is
gated at `G-RELEASE` because it is a provisioning procedure that must exist before keys are issued.

## R-025 Acknowledgement correlation cannot distinguish identical commands

| Field | Value |
| --- | --- |
| Severity | `MEDIUM` |
| Gate | `G-IMPL` |
| Source | QGroundControl source reading in Phase 2 |
| Related | `AERORA-GCS-REQ-018`, `AERORA-COMPLY-REQ-002` |

Command acknowledgements are matched to pending entries by the pair of component identifier and
command. There is no unique request identifier. Upstream guards concurrency by refusing duplicates,
but two *sequential* identical commands produce acknowledgements the mechanism cannot tell apart.

**Impact if unresolved.** Tolerable for display; not tolerable for an audit trail. A record stating
that an operator command was accepted must be attributable to that command and no other, and
without a request identity it is not.

**Mitigation.** Assign a GCS-side Aerora request identity, recorded alongside any MAVLink
correlation the dialect provides. This needs no protocol change, because the identity is required
for the record rather than for the wire.

## R-026 Override drift is invisible to tooling

| Field | Value |
| --- | --- |
| Severity | `MEDIUM` |
| Gate | `G-IMPL` |
| Source | Phase 2 analysis of the override mechanism |
| Related | R-019, `AERORA-UI-REQ-005`, `AERORA-UI-REQ-006`, `AERORA-TEST-REQ-008` |

Overriding an upstream file works by resource aliasing: the upstream file is untouched in the
working tree. Version control stays clean. No compiler warning, no lint rule, no merge conflict
surfaces the fork.

So when upstream fixes a bug in an overridden file, the Aerora build keeps the bug and nothing
reports it.

**Impact if unresolved.** Silent, accumulating divergence in exactly the files a custom build cares
most about, discovered only when someone notices a behaviour that upstream fixed months earlier.

**Mitigation.** The inventory in [ui-architecture.md](ui-architecture.md) with recorded fork-point
commits, plus the drift-checking tool described in
[testing-architecture.md](testing-architecture.md). Keep the tier-3 override list very short — one
entry is maintainable, a dozen is not — and prefer proposing a seam upstream over adding a second
entry.

## R-027 CSV telemetry columns misalign with runtime-created FactGroups

| Field | Value |
| --- | --- |
| Severity | `LOW` |
| Gate | `G-OPEN` |
| Source | QGroundControl source reading in Phase 2 |
| Related | `AERORA-GCS-REQ-022` |

The CSV telemetry header is written once, at file open, from the fact-group set that exists at that
moment. Each row is written from the set that exists when the row is written. Upstream creates some
groups dynamically on message receipt, so a group appearing after the file is open shifts every
subsequent column.

**Impact for Aerora.** None directly: the six Aerora FactGroups are created during vehicle
construction, so they exist before the first row. The risk is that a future Aerora change
introduces a dynamic group and inherits the defect.

**Mitigation.** The constraint is recorded as `AERORA-GCS-REQ-022` and asserted by a test. The
upstream defect is listed as an optional contribution below.

Severity `LOW` and gate `G-OPEN` because Aerora is not exposed today and the constraint is cheap to
honour.

## R-028 Authority mode and flight mode may be conflated

| Field | Value |
| --- | --- |
| Severity | `MEDIUM` |
| Gate | `G-IMPL` |
| Source | Phase 2 architecture analysis |
| Related | Q-007, `AERORA-SAFE-REQ-006`, `AERORA-GCS-REQ-011` |

QGroundControl's flight mode answers "what is the aircraft doing". Aerora authority mode answers
"who may command it". They are different axes. Whether they map one-to-one in the Aerora firmware is
part of Q-007 and is unresolved.

The hazard is that the implementation treats them as one concept because the GCS has a well-worn
flight-mode path and the authority mode has none.

**Impact if unresolved.** A single conflated value would appear in the display, the log schema and
the audit trail simultaneously, and separating them afterwards means migrating all three.

**Mitigation.** The architecture keeps them in separate Facts with separate names, and
[authority-model.md](authority-model.md) treats authority as its own state machine. The
flight-mode accessors are deliberately left unpopulated rather than filled with authority values.

## R-029 The single-firmware UI simplification is unreachable

| Field | Value |
| --- | --- |
| Severity | `LOW` |
| Gate | `G-OPEN` |
| Source | QGroundControl source reading in Phase 2 |
| Related | Q-017, `AERORA-UI-REQ-004` |

The supported-firmware-class list appends every registered factory's classes and then
unconditionally appends the generic class, into a container that does not de-duplicate. With the
Aerora factory registered the list always has at least two entries, so the single-firmware flag can
never be true — including in a single-stack build, and including when the Aerora factory claims the
generic class itself.

**Impact if unresolved.** Wasted effort: someone sets a CMake option expecting the flag to flip, and
then looks for the fault in the wrong place.

**Mitigation.** Recorded here and in [vehicle-integration.md](vehicle-integration.md), with the two
seams that reach the same outcome: a core-plugin virtual for the setup prompt, and QML overrides for
the two consumers. Nothing is blocked.

## Optional upstream contributions

Four small, clearly correct changes that would benefit everyone. **Aerora depends on none of them**
and does not wait for any.

| Contribution | Benefit | Related |
| --- | --- | --- |
| Log a line when mission resume drops an item | Makes a silent drop diagnosable for every vendor, not just Aerora | R-002 |
| Add an insertion seam to the fly-view tool strip | Removes the only anticipated tier-3 override | R-026 |
| De-duplicate the supported-firmware-class list | Makes the single-firmware simplification reachable | R-029 |
| Write the CSV telemetry header lazily, or re-emit it when the group set changes | Fixes silent column misalignment for all users | R-027 |
| Add a pluggable signing-key storage backend | Lets any build protect key material at rest; would resolve R-005, R-023 and R-024 | R-005 |

That is five, not four. The fifth is in a different category from the others: it is the one Aerora
would otherwise have to make as a core edit, and it is the only item on this list whose absence has
a release-blocking consequence.

## Combined risk position

| Register | `G-ARCH` | `G-IMPL` | `G-INTEG` | `G-RELEASE` | `G-OPEN` | Total |
| --- | --- | --- | --- | --- | --- | --- |
| Phase 1 risks, re-gated | 4 | 5 | 2 | 5 | 4 | 20 |
| Phase 2 risks | 0 | 5 | 0 | 2 | 2 | 9 |
| Combined | 4 | 10 | 2 | 7 | 6 | 29 |

The Phase 1 row reflects the **Phase 2.1 amendment to R-016**, which moved from `G-IMPL` to
`G-OPEN` once the dependency was traced. The pre-amendment row read `G-IMPL` 6 and `G-OPEN` 3. The
amendment, and the superseded Phase 2 text it replaces, are in
[phase-2-risk-gates.md](phase-2-risk-gates.md).

The four `G-ARCH` risks are all Phase 1 risks addressed by Phase 2 decisions. Phase 2 added none,
and Phase 2.1 added none.

Seven risks now block release. Four of the seven — R-005, R-023, R-024, and R-001 — concentrate in
two areas: signing key custody and the open licensing question. Neither is an architectural problem,
and both have identified owners outside the GCS engineering work.

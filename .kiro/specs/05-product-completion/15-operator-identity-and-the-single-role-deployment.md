# Operator Identity and the Single-Role Deployment

The owner's statement on operator login and role, recorded against a Gate that is already closed.
This document settles nothing and closes nothing. Its substance is that the question it was asked to
answer is not the question that is open.

## Status

| Item | Status |
| --- | --- |
| This document | `PROPOSED` — owner-stated, unsigned |
| `DG-ROL` | **`CLOSED`.** Both parts carry accepted Closure_Records, signed 2026-11-05 |
| `Q-029`, the role set | **Settled and signed.** Four roles, in `closures/DG-ROL.role-set.json` |
| The owner's "one role, Pilot" | **Read as a deployment, not as a role set.** Section 3. On that reading it contradicts nothing signed |
| The owner's login statement | **New, and the input that was actually missing.** Section 5 |
| New Contract_Values introduced | **None.** No role name is invented here, no credential format is authored, and no permission is added to a signed role |
| What this unblocks | The identity provider, which is one of the three installables `isFullyConfigured()` requires. Section 7 |
| Document number | 15, allocated here rather than by Tab 1, the same irregularity [13](13-autonomy-source-path-and-companion-topology.md) records about its own number |

## 1. Three things, conflated into one question

The question arrived as "which command classes may a Pilot authorise, and is that a Gate blocker".
It is three statements about three different layers, and separating them is most of the work:

| Statement | Layer | Where it belongs | State |
| --- | --- | --- | --- |
| There is one role, called Pilot | The **deployed role assignment** — which of the settled roles a station actually provisions | Configuration, and section 3 | Owner-stated |
| Login is a product code plus a pilot identifier | The **identity binding** — how a human becomes an `opaqueId` | `AeroraIdentityProvider`, and section 5 | Owner-stated, and previously absent |
| A Pilot may authorise everything | The **role set** — which roles exist and what each may authorise | `Q-029`, and **signed** | Settled already, and not as stated |

Only the third touches `Q-029`, and `Q-029` is closed. The first two are the layers `Q-029` was
holding hostage, and they are exactly the layers that were empty.

## 2. `Q-029` is not open, and that changes the act being requested

The accepted Closure_Record settles **four** roles and the command classes each may authorise:

| Role | Command classes it may authorise |
| --- | --- |
| Remote Pilot | `command`, `mission`, `manual-control`, `payload` |
| Payload Operator | `payload` |
| Mission Planner | `mission` |
| Maintenance Technician | `command`, `parameter` |

Signed by **Product and operations**, the owners the Gate_Dossier records for the part, with
`DG-ROL-SOC-01` and `DG-ROL-SOC-02` carrying satisfied verdicts. `SCR-SEC-02` and the calibration
display of `SCR-VEH-05` are recorded Released on the back of it, and the approval half of
`SCR-LIV-05` with them.

So "one role, Pilot" is not an answer to an open question. Read as a change to the role set it is a
**retraction of accepted, signed content**, and that is a different act with different rules:

| | Answering an open question | Changing a signed record |
| --- | --- | --- |
| What it needs | A Closure_Record and its owners' signatures | A path to retract a settled key |
| Whether the process has one | Yes, and it was used | **No.** See below |
| Cost of getting it wrong | A record that has to be amended | Signed content silently rewritten, and three screens released on a basis that no longer exists |

### The amendment mechanism does not reach this

`closure-record.schema.json` carries an `amendments` array, and it is the obvious place to look. It
does not fit, and the reason is worth stating because the near-miss is the dangerous kind.

An amendment attaches **its own signatures and sign-off date to one key the record already
settles**. `check_closure_amendments` rejects an amendment whose key the record settles no entry
under, and rejects two amendments to the same key. It is a signature-scoping device for a key that
arrives late, which is what Requirement 8 criterion 5 built it for.

It is therefore not an edit path. It cannot delete `Payload Operator`, and it cannot widen
`Remote Pilot`'s class list. Retracting or rewriting a settled key has **no recorded path at all**,
and inventing one is the process owner's act rather than this document's.

### And criterion 11 makes the retraction self-blocking

Even granted a path, deleting roles has a consequence the signed verdict already names.
`DG-ROL-SOC-02`'s accepted note records which role covers each class, and `parameter` is covered by
**Maintenance Technician alone**. Requirement 9 criterion 11 then says:

> IF THE `DG-ROL` Closure_Record carries a role that maps to no command class, or leaves a command
> class the station can emit mapped to no role, THEN THE Gate_Review SHALL hold the `Q-029` part at
> `OPEN` and SHALL name the unmapped role or command class.

So a role set collapsed to one role, without that role gaining `parameter`, leaves `parameter`
mapped to nothing and **reopens the Gate** — which un-releases `SCR-SEC-02` and the `SCR-VEH-05`
calibration display. A retraction that looks like a simplification would cost three screens and a
signature.

## 3. The reading that costs nothing

"One role, Pilot" is far more likely to describe **who the station issues credentials to** than to
be a redesign of the role vocabulary. On that reading it is a deployment statement, it contradicts
nothing signed, and it needs no Gate act whatsoever.

The code was built for exactly this. `AeroraRoleAssignment::roleIds` is a `QStringList` of opaque
strings, and the type deliberately enumerates and validates no role name. A deployment that
provisions one role is a list of length one. Nothing in the overlay changes.

This is also the gap `F3.1-04` named, and it named it precisely:

> Role resolution is code-isolated but not deployable. The code is role-set-agnostic; no usable
> default configuration can exist without `Q-029`, and the safe default grants nothing.

`Q-029` has since closed, so a usable configuration became possible, and what was still missing was
the configuration itself. The owner has now supplied it. **The finding is answered by a deployment,
not by a Gate.**

| | Role set | Deployed assignment |
| --- | --- | --- |
| Answers | Which roles may exist, and what each may authorise | Which of them this station issues, and to whom |
| Owner | Product and operations, **signed** | Product, per deployment |
| Changing it | Needs a retraction path that does not exist | Configuration |
| The owner's statement | Untouched | **This is it** |

Which reading the owner intends is the one thing this document cannot decide for them. It is stated
as a question in section 9, and until it is answered the signed set stands, because the signed set
standing is the reversible option and the retraction is not.

## 4. What a one-role deployment cannot do

The useful consequence, and it follows from the signed table rather than from a preference.

If the only provisioned role is `Remote Pilot`, then the `parameter` class is authorised by no
provisioned role, because the signed set gives `parameter` to `Maintenance Technician` only. A
station deployed that way **cannot write a vehicle parameter at all** — not as a refusal to
implement, but as the correct evaluation of the policy it was given.

| Affected | Consequence |
| --- | --- |
| [Document 10](10-parameter-write-path-and-authorization-boundary.md)'s write path | The `parameter` seam at `ParameterManager::_mavlinkParamSet`, candidate C1, would be built and would deny every write under a one-role deployment |
| `SCR-VEH-05` calibration | Calibration is the `command` class under `ADR-015`, which `Remote Pilot` holds, so this is unaffected. Worth saying, because "pilot cannot configure the aircraft" would be the wrong inference |
| The separation the signed set encoded | Splitting `parameter` away from the flying roles was deliberate. A one-role deployment that later gains `parameter` merges tuning authority into the person flying, which is a trade to make on purpose rather than by collapsing a list |

Three ways out, and they are not interchangeable:

| Option | What it costs |
| --- | --- |
| Provision `Maintenance Technician` as well, to a maintainer | Nothing. Two provisioned roles out of a signed four, and `parameter` is authorised by the role the signed set gives it to |
| Accept that the deployment cannot write parameters | Nothing signed moves. It is a real posture, and a defensible one for a painting aircraft whose tuning is a workshop activity |
| Widen `Remote Pilot` to hold `parameter` | A settled key rewritten, with no path for it, and the separation of duties gone. Section 2 |

The first is almost certainly what is wanted and it requires no decision from anybody.

## 5. The identity binding, which is the input that was missing

This is the part of the owner's statement with no prior record anywhere in the tree, and it is the
one worth capturing.

| Owner-stated | Recorded shape |
| --- | --- |
| Login is a product code, unique per aircraft, plus a pilot identifier | Two inputs that the provider reduces to **one `opaqueId`** |
| Multiple products means the set of connected product identifiers | A session spanning products, which the role assignment does not vary over |

`AeroraOperatorIdentity` carries `opaqueId` and `authenticated`, and its header is explicit that the
identifier "carries no assumed structure and is not a credential". Two consequences follow and
neither is optional:

| Consequence | Why |
| --- | --- |
| Whatever the provider composes, **nothing downstream parses inside it** | The policy, the audit record and the UI see one opaque string. A consumer that split it back into a product code and a pilot identifier would re-create the structure the type exists to deny, and would couple the audit trail to a login format |
| The product set is **session scope, not identity** | `AeroraRoleAssignment` resolves per identity and carries no vehicle. The fleet spec's `AeroraOperationAddress` already carries the addressed vehicles separately, so "which products this pilot is connected to" belongs on the address and not inside the identity |

The second matters for a reason the single-role framing hides. One role and several aircraft means
one authorization decision addressed to many vehicles, which is the atomic Fleet_Operation the fleet
spec already built and property-tested: authorization applies to every addressed member or to none.
The owner's multi-product statement lands on code that exists.

## 6. The product code is an identifier, not a secret

Recorded as a finding, not as a design change, because the login scheme is the owner's.

A product code unique per aircraft is printed on the airframe, shown in the fleet list and carried
in telemetry. Used as half of a login, it contributes no secrecy, so the credential's strength is
the pilot identifier alone — and a "pilot id" is usually an identifier too.

`AERORA-SEC-REQ-011` requires operator credential validation to succeed offline as the primary
path, which rules out leaning on a remote identity service to compensate. So the authenticating
secret has to be local and it has to be something, and neither of the two stated inputs is a
candidate.

| What this does not say | What it does say |
| --- | --- |
| That the scheme is wrong. Identifying *which* aircraft and *which* pilot is exactly what an audit trail needs, and `AERORA-SEC-REQ-019` requires operator identity on every record | That `authenticated` is a separate field from `opaqueId` for this reason, and nothing stated so far sets it to true honestly |

The question for Product is one sentence, and it is in section 9: what authenticates the pilot, as
distinct from what identifies them?

## 7. What this unblocks, concretely

Enforcement is not gated on a decision any more. `AeroraCommandAuthorizer::isFullyConfigured()`
returns `_identityProvider && _roleResolver && _policy && _policy->isConfigured()`, and
`AeroraSecurityStatus::enforcement()` derives from that single predicate rather than re-deriving it.
Three installables, and the Gate closure plus the owner's statement now specify all three:

| Installable | What specifies it | State |
| --- | --- | --- |
| `AeroraIdentityProvider` | The owner's login statement, section 5, plus an answer on what authenticates | **Specified in shape**, pending section 9's question |
| `AeroraRoleResolver` | The signed role table of section 2, and the deployment of section 3 | **Specified.** The table is signed and the resolver validates no name |
| `AeroraAuthorizationPolicy` | `ADR-014` Option C, signed in `closures/DG-ROL.authorization-scope.json`: a seam per class, each gated where that class converges | **Specified.** The `command` seam is built today at `MavCommandQueue::sendWorker()` |

Both accepted `DG-ROL` records carry a `guardPermissions` verdict permitting
`AeroraCorePlugin::isAuthorizationEnforced()` to become true. So the predicate reaching true is
permitted by the register, is held open only by the three installables, and is implementation rather
than decision. `AeroraVehicleIntegrationTest` asserts it false today, which is the correct assertion
for a station with nothing installed and the test to change deliberately when something is.

Both records also refuse to let that spread. `kGroundStationActuatorPathImplemented` and
`kAutonomousPayloadActionImplemented` are recorded as **not** permitted by either part, so settling
who may authorise a payload operation permits no station-side actuation and no station-originated
payload action.

## 8. What signing this would and would not do

It would record the owner's login statement as the identity-provider input, record the one-role
statement as a deployment rather than as a role-set change, and record the three findings of
sections 4, 6 and 7.

It would **not** close or reopen any Gate. `DG-ROL` is closed and this touches none of its
Decision_Content. It would not add a role, remove one, or widen a signed class list, all of which
section 2 shows have no path. It would not set a credential format, which section 6 refers to
Product. It introduces nothing into the overlay: no role name, no permission and no identity scheme
is declared in source by this document.

## 9. What is still open after this

| Open | Owner | Why it is still open |
| --- | --- | --- |
| Whether "one role, Pilot" means a **deployment** or a **retraction** of three signed roles | Product, with the process owner if it is the second | Section 3 reads it as the first because that reading is free and reversible. The second needs a retraction path the process does not have, and would reopen the Gate under criterion 11 |
| What **authenticates** the pilot, as distinct from what identifies them | Product, with security architecture | Section 6. Neither a product code nor a pilot identifier is a secret, and `AERORA-SEC-REQ-011` requires the offline path to be the primary one |
| Whether the deployment provisions `Maintenance Technician` to anyone | Product | Section 4. If not, the station cannot write a vehicle parameter, which is a defensible posture and must be a chosen one |
| Whether a retraction path should exist at all | The process owner | Raised by this document and not answered by it. Every Gate closed so far has been additive; this is the first request to take something back |

### Stale `Q-029` citations

`DG-ROL` closed after much of the tree was written, and several documents still read `Q-029` and
`ADR-014` as open. The split below is deliberate: a **dashboard** that lists a resolved blocker
misdirects work and was corrected, while an **analysis** written against the open state was correct
when written and is listed instead.

Corrected this pass:

| Site | What it said, and what it says now |
| --- | --- |
| [08-risk-register-and-open-decisions.md](08-risk-register-and-open-decisions.md) § 1, § 2 | `Q-029` as one of four hard blockers, and `ADR-014` as `OPEN — ARCHITECTURAL DECISION REQUIRED`. Both rows kept and marked resolved against the signed records, so the register still reads as a history |
| [00-README.md](00-README.md) § Next integration blockers | `Q-029` / `ADR-014` as integration blocker 4. Marked decided, with the three installables named as what is left |
| `aerora/src/Security/AeroraAuthorization.h` | `AeroraRoleAssignment`'s comment gave "the role set is `Q-029` and remains an unresolved product decision" as the reason the type enumerates no role name. The design is right for a different reason — a compiled-in role name could not be changed without a release — and that reason is now what the comment says |

Listed, not chased:

| Site | What it still says |
| --- | --- |
| [10-parameter-write-path-and-authorization-boundary.md](10-parameter-write-path-and-authorization-boundary.md) § 7, § 8, § 9 | "`Q-029` and ADR-014 are `OPEN`", and candidate test T5 blocked on both. Its analysis of the write path is unaffected; what changed is that selecting a seam is now permitted |
| [04-subsystem-and-interface-completion-matrix.md](04-subsystem-and-interface-completion-matrix.md) | The three authorization interfaces recorded as gated on `Q-029`, and `SCR-VEH-05` as withheld by decision. The register now records that screen's calibration display as Released |
| [30-outbound-command-security-audit.md](../03-implementation-specification/30-outbound-command-security-audit.md) | `ADR-014`'s own status line reads `OPEN — ARCHITECTURAL DECISION REQUIRED` while the signed closure record selects Option C. An ADR status is Architecture's to move, not this document's |
| [aerora-fleet-and-authority/tasks.md](../aerora-fleet-and-authority/tasks.md) | Treats `DG-ROL` as open throughout, including its "Held by a Gate" table. Left alone: it is a completed plan and was correct when written |

The register itself is current. `tools/validate_gate_register.py` derives `DG-ROL` as `CLOSED` and
the three screens as Released from the records, so the prose is what lags, not the data.

## Status vocabulary

Defined programme-wide in [00-README.md](00-README.md#status-vocabulary). This document redefines
none of them and claims `VERIFIED` for nothing.

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
| When the role takes effect | **Answered: at connection and sign-in.** That is `IF-SEC-003` as already written. Section 10 |
| What authenticates an operator | **Answered: the Aeroradynamics cloud user database.** Section 10 |
| Whether that check is per-flight or enrolment only | **Answered: per-flight.** Section 10's compatibility conclusion is superseded by section 14 and kept for its reasoning |
| What that reversal costs | **Four `CONFIRMED` positions**, three of them baseline-sourced, plus one architectural-property test. Section 14, and the `cloud-gated-flight-authorisation` hazard |
| The sharpest consequence | A link drop or a 24-hour expiry **in flight** demands a login the landed-only rule makes impossible. The repair is document 14 § 6's, applied to the credential: it gates entry, not continuation. Section 14 |
| Contract_Values | **None authored here.** The 24-hour period is **owner-stated** and recorded in section 14; no credential format, key mechanism or landed-state source is authored |
| What this unblocks | The identity provider, which is one of the three installables `isFullyConfigured()` requires. Section 7 |
| What it newly blocks on | `F3.1-01`, key protection at rest. Section 13. Note that per-flight verification reduces the reliance on a local trust anchor without removing it, because the 24-hour window is still validated locally |
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

The question this section raised — what authenticates the pilot, as distinct from what identifies
them — **is answered in section 10.** The answer is the Aeroradynamics cloud user database, which
means neither stated login input has to carry secrecy: they identify the aircraft and the operator,
and a separately issued credential authenticates. Sections 11 to 13 are what that costs.

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
statement as a deployment rather than as a role-set change, and record the findings of sections 4, 6
and 7. With sections 10 to 13 it would also record the identity authority by name, confirm
`IF-SEC-003`'s resolution timing, and accept the three gaps that follow from authenticating against
a cloud the station is forbidden to call.

It would **not** close or reopen any Gate. `DG-ROL` is closed and this touches none of its
Decision_Content. It would not add a role, remove one, or widen a signed class list, all of which
section 2 shows have no path. It would not move any `CONFIRMED` row of the security or cloud
interface control documents, which section 10 leaves standing and section 11 asks the interface
owner to qualify. It would not set a credential format, a validity period or a key-protection
mechanism, which sections 12 and 13 refer to Product and security architecture. It introduces
nothing into the overlay: no role name, no permission and no identity scheme is declared in source
by this document.

## 9. What is still open after this

Sections 10 to 13 answer the second row below and replace it with four narrower items. The table is
rewritten rather than appended to, so it reads as the current open set.

| Open | Owner | Why it is still open |
| --- | --- | --- |
| Whether "one role, Pilot" means a **deployment** or a **retraction** of three signed roles | Product, with the process owner if it is the second | Section 3 reads it as the first because that reading is free and reversible. The second needs a retraction path the process does not have, and would reopen the Gate under criterion 11 |
| ~~What authenticates the pilot~~ | — | **Answered.** The Aeroradynamics cloud user database, with registration as a separate prior act. Section 10 |
| ~~Whether registration is checked at flight time~~ | — | **Answered: yes, every operation.** Section 14, and it is the reading that reverses four `CONFIRMED` positions |
| ~~The credential validity period~~ | — | **Answered: 24 hours**, owner-stated. Section 14. Revocation latency is now bounded where it was unbounded |
| Whether the credential gates **entry to an operation only**, or continuation as well | Product with security architecture | Section 14, and it carries the safety case. Continuation leaves an airborne aircraft whose operator cannot re-authenticate until it lands, and landing is what the state forbids |
| Whether "landed" means **disarmed** or a landed-state report, and what an `UNKNOWN` state does | Product with Firmware | Section 14. The first is reported in the heartbeat today; the second needs a message the relayed set does not include |
| Whether the gate sits at **station sign-in** or at **arming** | Architecture | Section 14. It decides whether `AERORA-AWS-REQ-005` survives the change or joins the four |
| Where the cached credential and its trust anchor are **stored**, given that no protected store exists | Security architecture, with Product | Section 13. `AeroraPlatformKeyProtector` returns `nullptr` on every platform and `F3.1-01` records that neither candidate route is an isolation. `AERORA-SEC-REQ-018` additionally clears the settings store on a schema bump |
| Whether `IF-SEC-002`'s `CONFIRMED` offline guarantee should carry its **precondition** explicitly | The interface owner | Section 11. The guarantee holds for a previously cached operator and cannot hold for a first sign-in, and `AERORA-AWS-REQ-006`'s test outcome depends on which is assumed |
| Whether the deployment provisions `Maintenance Technician` to anyone | Product | Section 4. If not, the station cannot write a vehicle parameter, which is a defensible posture and must be a chosen one |
| Whether a retraction path should exist at all | The process owner | Raised by this document and not answered by it. Every Gate closed so far has been additive; this is the first request to take something back |
| A product requirement for **registration as a prerequisite to flight** | Product | Nothing in the requirement set says an operator must be registered before flying. The statement is owner-stated and has no identifier behind it |

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

## 10. The two answers, and the architecture they land on

The owner has answered both questions section 9 raised.

| Question | Answer, owner-stated |
| --- | --- |
| When does the role take effect? | At the time the aircraft connects and the operator logs in to the ground station |
| What authenticates the operator? | The Aeroradynamics user database in the cloud. A pilot registers with Aeroradynamics **separately, before flying** |

The first confirms a row that was already written. The second looks at first like a collision with a
`CONFIRMED` requirement and is not, because the architecture already separates the two halves of it.

### Role timing: this is `IF-SEC-003` as written

[13-security-icd.md](../02-interface-specification/13-security-icd.md)'s `IF-SEC-003`, role
resolution, carries its update semantics as **"On sign-in, and on credential refresh"**, and its
freshness as "carried with the session; refreshed state replaces it wholly, never merges". The
owner's answer is that row.

The row was held `OPEN` with `Q-029` in its Questions column. `Q-029` is closed and the timing is now
owner-stated, so what held it open is gone. The row is not moved here, because this document is
unsigned and the row's evidence class is a separate judgement.

One thing that follows and should be said before somebody implements the opposite. Binding a role at
sign-in is **not** caching an authorization decision. `IF-SEC-004` is explicit that a decision is
"decided at request time; a decision is not cached across a credential change", and
`AeroraCommandAuthorizer` re-reads the role assignment inside the per-command path rather than
holding one from sign-in. Both hold at once: the session carries the binding, the gate decides per
command. A reader who read "role at connection" as licence to cache a verdict would break
`AERORA-SEC-REQ-012`.

### Authentication: the cloud is the authority, and it is already in the architecture

`AERORA-SEC-REQ-011` is `CONFIRMED`: operator credential validation shall succeed **without network
access as the primary path**. `IF-SEC-002` is `CONFIRMED` and states the failure behaviour as
"validation must succeed with all network access blocked; a network error is not an authorisation
outcome". `IF-CLD-004` is `CONFIRMED` and says no authorisation check, arming check or **identity
resolution** blocks on a network service. The cloud interface control document lists cloud-gated
authentication as the **first named trap** and refuses it by name.

A reader could stop there and call the owner's answer a contradiction. It is not, because the same
documents already carry the shape that reconciles it:

| Recorded | Where |
| --- | --- |
| The backend's **identity and access** module maps to the station's security module as "offline-capable validation" | Cloud ICD, backend-to-counterpart table |
| `IF-SEC-009`, credential reconciliation, **Cloud to GCS**, "Refreshed credential **and role data**", opportunistic and **never blocking** | Security ICD |
| "A failed refresh leaves the local credential in force until its own expiry. **A refresh failure is not a sign-out**" | `IF-SEC-009` failure behaviour |
| Operator sign-in: "Reconciled against the service" when the cloud is available, "Validated against cached credentials" when it is not | Cloud ICD, degradation table |
| `IF-SEC-010`: no call from the security module into the cloud module exists. The dependency is one-way — cloud hands material in, security never calls out | Security ICD |

So the architecture already has exactly one cloud identity authority, reaching the station by a
non-blocking inbound path. The owner's answer **names** it. Aeroradynamics is the identity and access
backend that `IF-SEC-009` was written against, and nothing in the owner's statement asks the station
to call out.

The reconciliation is the word "separately" in the owner's own sentence:

| Act | When | Network | Satisfies |
| --- | --- | --- | --- |
| **Registration** with Aeroradynamics | Once, before flying, as a separate act | Online, necessarily | The owner's statement |
| **Sign-in** on the station, each flight | At connection | Offline, against the cached credential | `AERORA-SEC-REQ-011`, `IF-SEC-002`, `IF-CLD-004` |
| **Reconciliation** of credential and role data | Opportunistically, whenever the cloud is reachable | Online, never blocking | `IF-SEC-009` |

Registration is enrolment and it is not in the pre-flight sequence. If it ever becomes a per-flight
check, three `CONFIRMED` rows break at once and a pilot with no signal cannot fly — which for an
aircraft painting the inside of a structure or a facade in an industrial area is an operational
failure, not a spec disagreement.

**That is the one thing to hold onto from this section.** The answer is compatible on the enrolment
reading and incompatible on the per-flight reading, and the difference between them is whether the
aircraft flies without a network.

## 11. `IF-SEC-002`'s precondition, which is unstated and now matters

`IF-SEC-002`'s `CONFIRMED` failure row says validation must succeed with all network access blocked.
It states no precondition. With a cloud-registered identity there is one, and it is load-bearing:

> Offline validation succeeds only for an operator whose credential has **already** been cached on
> **this** station by a previous successful online sign-in.

Three cases the row does not distinguish, and only the first is the one it was written about:

| Case | Offline outcome | Correct? |
| --- | --- | --- |
| A pilot who has signed in on this station before | Validated against the cached credential | **Yes.** This is the row as intended |
| A newly registered pilot, first ever sign-in on this station | **Cannot validate.** There is nothing cached to validate against | Correct behaviour, undocumented consequence |
| Any pilot on a freshly provisioned or freshly reset station | **Cannot validate** | Same |

None of these is a defect. The second and third are the irreducible cost of a cloud identity
authority, and they are acceptable precisely because they fall outside the flight sequence. What is
wrong is that they are **unwritten**, and two things depend on them being written:

| Depends on it | How |
| --- | --- |
| `AERORA-AWS-REQ-006`'s all-network-blocked test | It asserts connection, parameter load, planning, upload and local recording with the network blocked. Whether it passes depends on whether the fixture pre-seeds a credential, and nothing says it must. A test that signs in offline against an empty store would fail for the right reason and read as the wrong finding |
| `AERORA-SEC-REQ-018` | A settings-schema version change clears all settings before any subsystem starts. If the cached credential lives there, **a station update strands every pilot until each re-registers online** — and the requirement currently describes this as a key-destroying event, not a credential-destroying one |

The second is the sharper one. It is the same failure `AERORA-SEC-REQ-018` already names, reaching a
second asset nobody had counted, and it turns a routine station update into a fleet-wide grounding.

## 12. The credential lifetime is the revocation latency

`IF-SEC-002` records that credential validity is time-bounded and that expiry is a reported state
rather than an inference. `IF-SEC-009` records that a failed refresh leaves the local credential in
force until its own expiry. Put the owner's answer beside those two and a number appears that nobody
has set:

**If Aeroradynamics revokes a pilot's registration, that pilot can keep flying until the cached
credential expires.** The validity period *is* the revocation latency. There is no other mechanism,
because the station never calls out — `IF-SEC-010` makes that structural rather than incidental.

This is not a flaw in the design. It is the price of offline-first, it is the correct price, and the
alternative is the per-flight check that section 10 rules out. But it makes the validity period a
**safety and security value rather than a convenience one**, and it is exactly the shape of value
this programme does not invent:

| | |
| --- | --- |
| What it is | A single duration, held by Product with security architecture |
| What it trades | Revocation latency against how long a disconnected station stays usable. Short strands field crews; long leaves a revoked pilot airborne |
| Why no number is here | It is a Contract_Value, the same class as the freshness budgets `DG-FRS` holds and the thresholds `kDefinedThresholdCount` is pinned at zero to keep the station out of |
| What exists already | `IF-SEC-008`, credential lifetime state, which carries "validity, expiry and refresh state as distinct values" — the display contract is written, the duration is not |

The honest station behaviour in the meantime is the one `IF-SEC-008` already specifies: show the
validity, the expiry and the refresh state as three separate values, and let the operator see how
old their authority is.

## 13. Offline validation needs a trust anchor the station cannot protect

The finding with the widest consequence, and it is derivable entirely from what is already recorded.

To validate a cloud-issued credential **without** reaching the issuer, the station must verify it
locally. That needs verification material on the station and a trust anchor for it. `IF-SEC-001`
leaves the credential's form unspecified — `BASELINE-ASSERTED`, `PROPOSED` — so no mechanism is
chosen anywhere, and none is chosen here.

What is recorded is that the station has nowhere safe to keep one:

| Recorded | Consequence for this |
| --- | --- |
| `AERORA-SEC-REQ-017`, `CONFIRMED`: a production release shall not persist signing key material without protection at rest | A trust anchor is key material in the sense that matters |
| `AeroraPlatformKeyProtector::createForCurrentPlatform()` returns `nullptr` on **every** platform, deliberately, because `D-006` leaves an untested crypto backend worse than none | There is no protected store to put it in |
| `F3.1-01` and `ADR-008`: key protection at rest has two candidate routes and **neither is an isolation**. Recorded against `G-RELEASE` | The gap is release-critical and already escalated |

A trust anchor does not need secrecy; it needs **integrity**. An attacker who can replace the anchor
in an unprotected settings store can mint credentials the station will accept offline, and offline is
where every flight-time validation happens. So the integrity half of the same gap is what
authentication now rests on.

**What changes is the scope of `F3.1-01`, not its severity.** It was load-bearing for audit signing.
The owner's answer makes it load-bearing for **authentication** as well, which means the `G-RELEASE`
gate now holds the product's second differentiator and not only its audit trail.

Nothing here proposes a mechanism. Choosing one is security architecture's with Product, it is
blocked behind the same build host, and a credential-validation scheme written without a compiler or
a test would be the security control that does nothing which `F3.1-03` already names once.

## 14. The verification is per-flight, which is the reading that breaks four `CONFIRMED` positions

Section 10 said the cloud answer was compatible on the enrolment reading and incompatible on the
per-flight reading, and that the difference between them was whether an aircraft flies without a
network. The owner has now stated the per-flight reading.

| Owner-stated | |
| --- | --- |
| A login is required after **every disconnection** between aircraft and station | |
| With no disconnection, the credential period is **24 hours** | |
| Verification can only happen while the aircraft is **landed** | |
| **Every time**, the operator is verified against the Aeroradynamics cloud database, and the operation starts only on a match | |

Section 10 is therefore superseded in its conclusion and kept for its reasoning, which is the
convention [13](13-autonomy-source-path-and-companion-topology.md) § 7 uses for the same situation.
The register carries this as the `cloud-gated-flight-authorisation` hazard.

### What it contradicts, stated plainly

Four positions, and three of the four are sourced to the baseline:

| Position | Status | What the rule does to it |
| --- | --- | --- |
| `AERORA-SEC-REQ-011`: credential validation shall succeed without network access **as the primary path** | `CONFIRMED`, `B-51` | **Inverted.** Online becomes the primary path and offline becomes nothing |
| `IF-SEC-002`: "validation must succeed with all network access blocked. **A network error is not an authorisation outcome**" | `CONFIRMED` | **Broken.** Under the rule a network error is precisely an authorisation outcome: the operation does not start |
| `IF-CLD-004`: "no command, **authorisation**, arming check, **identity resolution**, **entitlement check** or configuration fetch blocks on a network service" | `CONFIRMED`, `GCS-DECIDED` | **Broken three times by name.** This row was written to close every door this request could come through |
| `IF-SEC-010`: "no call from the Security module into the Cloud module exists" | `GCS-DECIDED` | **Broken structurally**, and `AERORA-TEST-REQ-005` asserts it as an architectural property, so a test has to be inverted rather than a comment changed |

And the cloud interface control document's first named trap is the request verbatim:

> Authentication — "Operators sign in, so check the token before arming" — **Refusal: validated
> offline, `IF-SEC-002`.**

Eight hidden-dependency routes are enumerated there, each introduced "one convenience at a time,
each step individually defensible". This is route one, arriving as a deliberate product policy
rather than as a convenience. That is a better way for it to arrive, and it is still route one.

### What survives, which is more than it looks

Three things, and they matter because they bound the change:

| Survives | Why |
| --- | --- |
| `AERORA-AWS-REQ-006`, `CONFIRMED` | It names five capabilities that must work with all network access blocked: connection, parameter load, mission planning, mission upload and local flight recording. **None of them is flight authorisation.** A pilot with no signal can still connect, read parameters, plan, upload and record. They cannot start an operation |
| `AERORA-AWS-REQ-007` and `IF-CLD-013` | They require each cloud-backed feature to have a **defined local behaviour** and record that the absence of a global offline mode is the contract. The rule gives flight authorisation a defined local behaviour, which is to refuse. That is the one cloud position the answer satisfies rather than breaks |
| `IF-CLD-002`, `GCS-DECIDED` | No synchronous network call originates on the main thread. A verification written the obvious way — block until the cloud answers, then start — breaks it and freezes the interface on a slow name resolution. The rule is implementable without breaking it, and only if that is known in advance |

`AERORA-AWS-REQ-005`, no operator command to a vehicle depending on cloud availability, depends on
**where the gate sits**. At station sign-in it survives, because a sign-in is not a command to a
vehicle. At arming it does not. The rule as stated does not say which, and that is section 14's
second question.

### The rule contradicts itself in flight, and the repair is already in this tree

This is the finding to act on, and it follows from the owner's two rules alone without reference to
any requirement.

A link drop between station and aircraft is **normal**. The whole link architecture is built around
it: `IF-RAD-009` covers reconnection, `IF-RAD-008` covers vehicle-reported link state, and
[14](14-ground-link-classes-and-control-authority.md) § 6 is a finding about what link loss does to
authority. Chain the rules against that:

| Step | |
| --- | --- |
| The link drops while the aircraft is airborne | Routine |
| Rule 2: a login is now required | |
| Rule 2's own constraint: verification can only happen while landed | |
| The aircraft is airborne, so verification **cannot happen** | |
| Result | The station reconnects to an airborne aircraft and the operator holds no valid credential |

A 24-hour expiry reached in flight produces the identical state by a different trigger. So both
triggers converge on one question, and it is the question to answer: **what authority does the
station have over an airborne aircraft whose operator credential is no longer valid?**

Both obvious answers are wrong in the same way they were wrong in document 14:

| Answer | Where it fails |
| --- | --- |
| None. Commands are refused until the operator re-authenticates | The operator cannot tell an airborne aircraft to land, return or abort — and landing is the precondition for re-authenticating, so the only way out of the state is the one thing the state forbids. It is a deadlock with an aircraft in it |
| Full. The credential gates nothing once flying | Then "every time, verified before the operation starts" is satisfied by a login at dawn, and a 24-hour expiry means nothing |

**This is the second instance of one defect class.** Document 14 § 6 found it on the
Manual_Authority_Grant axis and stated the resolution: *may manual control begin* and *may manual
control continue* are different questions with different answers, so a grant is required to **enter**
manual authority and expiry does not **end** it. The credential axis reproduces it exactly, and the
same resolution applies:

> **A valid credential is required to *enter* an operation. Its loss or expiry does not *end* one.**

| Case | Behaviour |
| --- | --- |
| No valid credential, operator tries to start an operation | Refused. This is the owner's intent — an unregistered pilot does not fly — and it holds |
| Credential valid, operation starts, link then drops in flight | The credential is marked requiring re-verification and recorded as such. The operator keeps command of the airborne aircraft, including the command to land |
| Credential expires at 24 hours while airborne | Same. Recorded, displayed, and not enforced until the aircraft is down |
| Aircraft lands, operator wants to start another operation | Re-verification required, which needs the cloud. The gate is real |

The owner's own landed-only constraint is what makes this the reading rather than a compromise. **A
check that can only be performed when the aircraft is landed cannot be a continuation gate**, because
continuation is the state where it cannot run. The rule already says entry; writing it down as entry
only makes it say what it means.

### The landed state has no reported value behind it

The rule's precondition depends on the station knowing the aircraft is landed, and the station may
not infer it — `AERORA-GCS-REQ-029` and `IF-UIS-005` keep the station to what the vehicle reports,
and `kRemainingEnergyEstimationImplemented` is the standing precedent for not computing what was not
reported.

If the relayed message set of [13](13-autonomy-source-path-and-companion-topology.md) § 14 is
confirmed, the flight controller sends heartbeat, system status, attitude, global position, GPS,
battery, head-up values and status text. **No extended system state, so no landed state.** What *is*
reported is the armed flag, which the heartbeat carries.

| Reading of "landed" | Reported today |
| --- | --- |
| Disarmed | **Yes**, in the heartbeat. Usable now |
| A landed-state report distinct from disarmed | **No.** It needs a message the relayed set does not include |

Those are not the same condition: an armed aircraft sitting on the ground is disarmed-false and
landed-true. Which one the rule means is a one-sentence answer from the owner, and on the first
reading the rule is implementable against what the aircraft already sends.

An `UNKNOWN` landed state also needs a defined behaviour, and neither default is free. Fail closed
and an aircraft that has not reported the state cannot be authorised. Fail open and the landed-only
constraint is advisory. The State_Vocabulary has the right answer available — report `UNKNOWN` and
say so — but what the gate then does is a decision.

### The 24-hour period, recorded

The owner has set the value section 12 asked for. It is recorded as owner-stated and it is not
authored here.

| | |
| --- | --- |
| What it bounds | Section 12 asked for the credential validity period because it is the revocation latency. At 24 hours the revocation latency is **bounded at 24 hours**, down from unbounded. That is a strict improvement on the previous state |
| What else it is | A hard cap on how long a station stays usable without a cloud round trip, and under the entry-not-continuation reading, a cap on when the next operation may start rather than on the current one |
| What it does not settle | The in-flight expiry case above, which is behaviour rather than duration |

### What this needs, and from whom

Nothing below is done here. Each is a named owner's act, and three of them move `CONFIRMED` content.

| Action | Owner |
| --- | --- |
| Decide whether the credential gates **entry only**, as recommended above, or continuation as well | Product with security architecture, and it carries the safety case for the airborne aircraft |
| Amend or withdraw `AERORA-SEC-REQ-011`, which the rule inverts | The requirement owner. A `CONFIRMED` baseline-sourced requirement, and changing one is the owner's act rather than this document's |
| Move `IF-SEC-002`'s status and rewrite its failure row, and add the named member to `IF-CLD-004`'s empty set | The interface owner |
| Withdraw or narrow `IF-SEC-010`, and invert the `AERORA-TEST-REQ-005` architectural-property test that asserts it | Architecture with Tab 2 |
| Withdraw the cloud document's trap-one refusal, or record the exception against it | Architecture |
| State whether "landed" means disarmed or a landed-state report, and what an `UNKNOWN` state does | Product with Firmware |
| State whether the gate sits at station sign-in or at arming, which decides whether `AERORA-AWS-REQ-005` survives | Architecture |
| Author the product requirement for registration as a prerequisite to flight, which still does not exist | Product |

### Why this is recorded rather than resisted

The owner's goal is clear and legitimate: a pilot not currently registered with Aeroradynamics must
not fly. That is a licensing control, it is the kind of control an operating organisation is
accountable for, and no offline mechanism delivers it — an offline credential is by construction a
statement about the past.

So this is not a convenience that slipped onto the flight path. It is a deliberate reversal with a
real reason, and the correct response is an impact statement rather than a refusal. What the record
owes it is that the reversal is **visible**: four positions move, one architectural-property test
inverts, and the in-flight case gets an answer before any of it is built. All of that is cheap now
and expensive once a login screen exists.

## 15. The owner's answers, and what each one costs

`PROPOSED`, owner-stated, unsigned. Six questions this document raised are answered. Four are the
reading it recommended; one is not, and that one is the expensive answer, so it is stated first
rather than buried in a table.

| # | Question | Answer |
| --- | --- | --- |
| 1 | What authority survives a credential lost or expired in flight | **Entry only.** A valid credential is required to *enter* an operation and its loss or expiry does not *end* one |
| 2 | Where the cloud check sits | **At arming**, not at station sign-in |
| 3 | What "landed" means | **Disarmed**, owner-glossed as "flight positioned at ground" |
| 4 | Whether "one role, Pilot" was a deployment or a retraction | **A deployment.** The signed four-role set stands |
| 5 | Whether `Maintenance Technician` is provisioned | **Yes**, to a maintainer |
| 6 | Whether `WP2-C`'s key-protector prohibition lifts | **Yes, for Linux and Windows only** — the two platforms CI can test |

### Answer 2 costs a fifth `CONFIRMED` position, and it is the one that grounds an aircraft

Section 14 recorded four reversals and said a fifth depended on where the gate sits. It sits at
arming, so the fifth falls:

> `AERORA-AWS-REQ-005`, `CONFIRMED`, source `B-51`: **No operator command to a vehicle shall depend
> on cloud availability.** Rationale as recorded: "the manual flight command path must remain
> operational without AWS."

Arming is an operator command to a vehicle. Under answer 2 it depends on cloud availability, so the
requirement is contradicted in its own terms. That is **three baseline-sourced `CONFIRMED`
requirements** now reversed by this policy, plus two `GCS-DECIDED` interface rows and an
architectural-property test.

The operational consequence, stated without softening: **an aircraft cannot be armed where there is
no network.** Not "degrades", not "falls back" — cannot arm. For a painting aircraft working a
facade in an industrial area, inside a structure, or in a basement car park, that is the common case
rather than the edge case.

`AERORA-AWS-REQ-006` still survives, and the gap between the two is now the whole operational story:
a crew with no signal can connect, load parameters, plan a mission and upload it. They cannot start
it.

### Answers 2 and 3 compose better than either does alone

This is the good news in the set, and it was not obvious when the questions were asked separately.

Section 14 recorded that the landed precondition had no reported value behind it: the relayed
firmware message set carries no extended system state, so no landed state is reported, and an
`UNKNOWN` landed state needed a defined behaviour that neither default made free.

**Answer 2 dissolves that question entirely.** A gate at arming is reachable only while the aircraft
is disarmed, because an armed aircraft is not arming. So "verification only happens while landed" is
structural rather than something the station must check:

| What section 14 asked for | What answer 2 does to it |
| --- | --- |
| A reported landed state | **Not needed.** The gate's own position establishes it |
| `EXTENDED_SYS_STATE` from firmware | **Not needed.** No firmware change is required for this |
| A defined behaviour for an `UNKNOWN` landed state | **Does not arise.** There is no landed state to read |

It also makes answer 3's proxy exact where it is used. Disarmed and on-the-ground are not the same
condition in general — an aircraft can be armed on the ground before takeoff, and disarmed in the
air after a kill — so "disarmed" would be a lossy stand-in for the owner's "positioned at ground"
anywhere else. At a pre-arm gate the two coincide, because the aircraft has not left the ground yet.
The gloss and the mechanism agree, and they agree only because of answer 2.

### Answer 1 makes the recommended repair the chosen one

The `cloud-gated-flight-authorisation` hazard recorded the entry-not-continuation rule as a
*proposed* repair, unsigned. It is now the owner's answer, so the in-flight deadlock is closed by
decision rather than by recommendation:

| Case | Behaviour |
| --- | --- |
| No valid credential, operator tries to start an operation | Refused. This is the owner's intent and it holds |
| Credential valid, armed, link then drops in flight | Recorded as requiring re-verification. The operator keeps command of the airborne aircraft, including the command to land |
| Credential expires at 24 hours while airborne | Same. Recorded and displayed, enforced only at the next arming |
| Aircraft lands and disarms, operator wants another operation | Re-verification required, which needs the cloud. The gate is real |

### Answers 4 and 5 cost nothing and settle the role layer

Nothing reopens. The signed four-role set stands, criterion 11 is not engaged, the three released
screens stay released, and no settled key is rewritten. `parameter` is authorised by the role the
signed set gives it to, so the station can write a vehicle parameter and the separation of duties
the signed `DG-ROL-SOC-02` verdict deliberately encoded is preserved.

The consequence for implementation is that **the role resolver is now fully specified**: an identity
maps to configured role identifiers, drawn from a set that is signed and a deployment that is
stated. `AeroraRolePolicy` already evaluates against exactly that shape, so the two meet without
either being redesigned.

### Answer 6 lifts a prohibition, and lifts it for a reason that changed

`WP2-C` prohibited an `AeroraPlatformKeyProtector` backend on the ground that "an untested crypto
backend is worse than none". That reason rested on `D-006`, which has narrowed to this host: the CI
legs build and test the overlay on Linux and Windows. The signed `DG-STO:tamper-evidence` record
additionally **names a backend per platform**, so the choice is settled rather than open.

| Platform | Backend, per the signed record | Prohibition |
| --- | --- | --- |
| Linux | libsecret, through the Secret Service | **Lifted.** CI tests it |
| Windows | DPAPI | **Lifted.** CI tests it |
| macOS, Android, iOS | Keychain, Android Keystore, Keychain | **Stays.** No CI leg exercises them, so the original reason survives unchanged |

What that unblocks, and what it does not:

| | |
| --- | --- |
| Unblocked | The protected chain anchor of `D-DAT-05`, which is the other half of the audit construction whose keyless half is now built as `AeroraAuditChain`. And the trust anchor that offline validation of a cloud-issued credential needs, which section 13 records as the widest consequence of the cloud answer |
| Not unblocked | `isTamperEvident()`. A lifted prohibition is permission to implement, not an implementation, and the register's guard verdict already said *permitted* rather than earned |
| Unchanged | `AERORA-SEC-REQ-017`, `CONFIRMED`: a production release shall not persist signing key material without protection at rest. Two platforms gaining a protector does not satisfy it for the other three, so `F3.1-01` stays release-critical against `G-RELEASE` |

### Answered: the 24 hours is an offline flying window

Answers 2 and the 24-hour period looked as though they did not compose, because two readings of
"every time it will verify from our cloud database" differ by whether an aircraft flies at all
without a network. The owner has chosen the second.

| Reading | What it means | Chosen |
| --- | --- | --- |
| **(i)** Every arming contacts the cloud | The 24 hours is a login-frequency setting; the cloud is reached at each arming regardless, so no network means no flight ever | — |
| **(ii)** The cloud is contacted at **login**; the gate at arming checks the **cached** credential | The 24 hours is an offline flying window: a crew flies for up to 24 hours after their last successful login | **Yes** |

So the shape is: a successful online login against the Aeroradynamics cloud caches a credential that
carries its own expiry, 24 hours out by default; the arming gate reads the **cached** credential and
contacts nothing. `AERORA-AWS-REQ-005` is still contradicted — arming consults a locally cached
credential whose existence depended on a past cloud call — but the contradiction is at login rather
than at every arming, which is the smaller reversal and the one that lets a disconnected crew keep
working through a shift.

The 24 hours is owner-stated and is **not** authored in code. The credential carries `issuedUtcMs`
and `expiresUtcMs`, and whatever issues it sets the gap, exactly as `AeroraManualAuthorityGrant`
carries its own issue and expiry rather than compiling in a grant duration. A deployment that wants
a different window changes the issuer, not the type.

### What this settles, and the one thing it does not

It settles the last input the **identity provider** needed. `AeroraCachedCredentialIdentityProvider`
is that type, and it answers two separate questions rather than one: `currentIdentity()` reports the
operator as authenticated whenever a credential is **present** — logged in, fresh or not — and
`isAuthenticationFresh()` reports whether the window is still open. Keeping them apart is what lets
an expired-but-logged-in operator keep commanding an operation in force while a new entry is still
refused, which is the owner's entry-not-continuation rule. Folding them — reporting unauthenticated
once stale — would deny every command after a mid-flight expiry, the failure that rule forbids.

It does **not** make the provider safe to install on `AeroraCommandAuthorizer` yet, and the reason is
answer 1. The command authorizer reads `currentIdentity()` on **every** queued command and denies an
unauthenticated one. If a cached credential expires at the 24-hour mark while the aircraft is
airborne, a provider wired straight to that gate would begin denying every subsequent command —
which is continuation-blocking, the exact failure answer 1 forbids. So the three installables that
`isFullyConfigured()` reads now all exist and are verified, and installing them together is blocked
on an operation-entry gate that distinguishes *entering* an operation from *continuing* one.

### The operation-entry gate, as decided

The owner's answers A1–A3 settle that gate's behaviour, and `AeroraOperationGate` now implements it
as a pure state machine. Freshness is consulted on **exactly one transition** — entering an
operation from outside one — and every other command is never freshness-blocked:

| Phase | Command | Credential | Gate |
| --- | --- | --- | --- |
| Not in operation | arm (entry) | fresh | Allowed, enters |
| Not in operation | arm (entry) | stale | **Blocked**, stays out |
| Not in operation | a ground command | stale | Allowed — freshness is checked only at entry, so an operator whose credential just lapsed can still configure and upload, and is refused only at the arm |
| **In operation** | **anything, incl. land/RTL/disarm** | **stale** | **Allowed** — this is A2: expiry does not end an operation |
| In operation | disarm (exit) | either | Allowed, ends the operation (A3) |
| Not in operation | re-arm after disarm | stale | Blocked — a new entry re-checks (A1) |

The gate only ever *adds* a refusal; it never authorizes. A command it does not block still faces
role authorization in `AeroraCommandAuthorizer`, which is the one thing that may permit a command.
And nothing is installed: the shipped build still enforces nothing, because
`isAuthorizationEnforced()` stays false until a deployment installs the chain.

What remains is the **live wiring** — the one change that touches the real command path and so is
staged on its own rather than folded in with the state machine:

| Wiring step | State |
| --- | --- |
| Split freshness out of `AeroraCachedCredentialIdentityProvider` so `currentIdentity()` reports the operator while *logged in* and freshness is a separate read | **Done.** `currentIdentity()` now follows presence and `isAuthenticationFresh()` follows the window, added to the `AeroraIdentityProvider` base with a default of true so a provider that never expires never blocks an entry |
| An end-to-end test that installs the full chain plus the gate and asserts a stale arm is refused while a post-expiry command is not | **Done.** `AeroraCommandAuthorizer::authorizeThroughOperationGate` composes the gate, the freshness read and the role chain, per vehicle; `AeroraOperationGateEnforcementTest` installs the real provider, resolver and policy and proves the safety case on a built binary — stale arm refused and audited, command in flight survives expiry, stale re-arm refused, phase independent per vehicle |
| Classify arm/disarm at `AeroraCorePlugin::authorizeMavCommand`, where the raw `mavCommand` and `param1` already are, and route through `authorizeThroughOperationGate()` | **Done.** `classifyOperationBoundary` reads the standard arm-disarm command id — its file and the test that drives it are allow-listed for that identifier — and `authorizeMavCommand` now routes through the gate. The shipped default is untouched: the `!isFullyConfigured()` early-return runs first, so `isAuthorizationEnforced()` stays false and nothing changes on the live path until a deployment installs the chain. A deployment that installs it must also wire vehicle-disconnect to `resetOperationGate()`, recorded as an install requirement |

That rewire changes behaviour on the drone's command path, interacts with the shipped fail-closed
default and the `AeroraVehicleIntegrationTest` assertion that enforcement is off, and is code this
host cannot run. So it is landed deliberately and on its own, with the state machine proven first.

### The clock the window is measured on, and its limit

The credential's validity is wall-clock, because "24 hours since login" is a human duration and,
if the credential is ever persisted across a restart, a monotonic clock would reset under it. The
cost is recorded rather than hidden: a wall clock steps on an NTP correction, a GNSS fix or an
operator change, so a backward step extends a window and a forward step shortens it. This is the
same exposure `IF-SEC-008` already carries for credential lifetime — "expiry is a reported state,
not an inference" — and it is not solved here. Solving it needs a clock an attacker cannot move,
which is the trust-anchor concern of section 13 and `F3.1-01`, not a property this type can assert
on its own. Whether the cached credential is held in memory only or persisted is left open and is a
`P7-DAT-002` question; the type is correct either way.

## Status vocabulary

Defined programme-wide in [00-README.md](00-README.md#status-vocabulary). This document redefines
none of them and claims `VERIFIED` for nothing.

# Authorization Implementation

**F3.1-04**. The abstraction that lets a future role matrix arrive without rewriting the ground
station. **No role name and no permission name is invented.**

## The chain

```text
Operator Identity
       ↓                    who is at the keyboard
Role Resolution Interface
       ↓                    what role set they hold
Authorization Decision
       ↓                    may this operation proceed
Command Authorization Gate
       ↓                    before the link write
MAVLink
```

Three abstractions, each replaceable independently. That separation is the deliverable; the content
of the middle one is Q-029 and stays open.

## The four abstractions

### 1. Identity abstraction

| Element | Specification | Status |
| --- | --- | --- |
| Type | `AeroraOperatorIdentity` — an opaque identity value plus its validity window | `IMPLEMENTATION-PLANNED` |
| Opacity | **No consumer parses its internal structure.** Same discipline as the vehicle identity under `AERORA-VEH-REQ-004` | `IMPLEMENTATION-PLANNED` |
| Source | `M-Security` authentication, offline-primary | `IMPLEMENTATION-PLANNED` |
| Format | **Not specified** — not evidenced | `CONTRACT-ONLY` |
| Unidentified state | An explicit value, distinguishable from "not yet signed in" | `IMPLEMENTATION-PLANNED` |
| Audit use | Stamped on every record at creation, never at upload | `IMPLEMENTATION-PLANNED` |

Opacity is what makes the format decision deferrable. If nothing parses the identity, the format can
change without touching a consumer.

### 2. Role-resolution abstraction

| Element | Specification | Status |
| --- | --- | --- |
| Interface | `AeroraRoleResolver` — maps an identity to an opaque role set | `IMPLEMENTATION-PLANNED` |
| Role set type | An opaque collection of opaque role tokens. **No role is named in code** | `IMPLEMENTATION-PLANNED` |
| Source of the mapping | A **configuration artifact**, not code | `CONTRACT-ONLY` — Q-029 |
| Refresh | The whole set is **replaced**, never merged | `IMPLEMENTATION-PLANNED` |
| Unresolvable set | Grants **nothing**. No permissive default exists | `IMPLEMENTATION-PLANNED` |
| Future role matrix location | The configuration boundary below | `CONTRACT-ONLY` |

### 3. Authorization abstraction

| Element | Specification | Status |
| --- | --- | --- |
| Interface | `AeroraAuthorizer` — given an operation identifier and a role set, returns permit or deny **with a reason** | `IMPLEMENTATION-PLANNED` |
| Gated operation set | **Declared explicitly** in the overlay, never derived from interface visibility | `IMPLEMENTATION-PLANNED` |
| Decision caching | **None across a credential change.** Decided at request time | `IMPLEMENTATION-PLANNED` |
| Unreachable decision | **Denies.** An unavailable gate is a denied command, never a permitted one | `IMPLEMENTATION-PLANNED` |
| Denial reason | Required, and recorded | `IMPLEMENTATION-PLANNED` |
| Policy content | **Not specified** — Q-029 | `CONTRACT-ONLY` |

The explicit gated-operation set is what prevents the advanced-UI mistake becoming structural.
Deriving the gated set from visibility would make a display preference into an access control —
`AERORA-SEC-REQ-013`, `AERORA-SEC-REQ-014`.

### 4. Command authorization gate

| Element | Specification | Status |
| --- | --- | --- |
| Position | **Before the link write.** An unauthorised command never reaches the wire | `IMPLEMENTATION-PLANNED` |
| Requirement | `AERORA-SEC-REQ-012`; a post-hoc check is not a gate | — |
| Relationship to vehicle authority | Independent and additional. Authorisation is **necessary, not sufficient** — `AERORA-GCS-REQ-019` | `IMPLEMENTATION-PLANNED` |
| On denial | An audit record and **no command**. No acknowledgement is expected, and the absence is not a timeout | `IMPLEMENTATION-PLANNED` |
| Integration point | `M-Authority`'s command submission path, before the eight-state tracker enters `SENT` | `IMPLEMENTATION-PLANNED` |

## Configuration boundary

Where the future role matrix will live, specified so that its arrival is a configuration change.

| Property | Decision |
| --- | --- |
| Form | A configuration artifact loaded by `M-Config`, versioned with the applied configuration |
| Not in code | No role name, no permission name and no policy rule is compiled in |
| Not in settings | It is an operational artifact, not an operator preference |
| Distribution | Through `IF-CLD-007` configuration distribution where the cloud is available; a local file otherwise |
| Versioning | Reported as part of `AERORA-BUILD-REQ-010`'s applied configuration version |
| Audit | A change to it is a configuration-change record — `IF-LOG-010` |

## Unavailable-role behaviour

The honest answer, and the reason F3.1-04 is a finding rather than a solved problem.

| Situation | Behaviour | Consequence |
| --- | --- | --- |
| No role configuration present | The resolver yields an empty set | **Every gated operation is denied** |
| An identity with no role | Same | Same |
| A role token the authorizer does not recognise | Retained, recorded, and **grants nothing** | Conservative |
| The resolver is unreachable | Denies | Fail-closed |

### The finding

The code is role-set-agnostic and therefore correctly isolated. **It is not deployable.** A station
with no role configuration is correct and unusable: every gated operation denies.

| Consequence | Assessment |
| --- | --- |
| Isolation at the **code** boundary | Achieved |
| Isolation at the **delivery** boundary | **Not achieved** |
| What would close it | Q-029 — a product and operations decision, not an engineering one |
| What Phase 4 must not do | Invent a default role matrix to make the station usable |
| Status of `IF-SEC-003` | **`BLOCKED`** |

Shipping a permissive default "for now" is the failure mode this finding exists to prevent. A
default that grants everything is not a placeholder; it is a security decision taken by omission.

## Audit identity

| Property | Decision |
| --- | --- |
| Every audit record carries the operator identity | `AERORA-SEC-REQ-019` |
| Stamped at record creation | Not at upload |
| An unidentified operator yields an explicit unattributed record | **Never a withheld record** |
| The unattributed condition is itself a recorded event | So the gap has a reason |
| Authorisation denials carry the identity and the reason | `AERORA-SEC-REQ-015`; `IF-SEC-006` |

## What must not be used as an authorisation boundary

| Mechanism | What it is | What it is not |
| --- | --- | --- |
| The advanced-UI toggle | A visibility preference, settings-backed, with a core-plugin confirmation hook | **Not** authentication, authorisation or access control |

Anyone at the keyboard can turn it on. It may be **combined** with a role; it never **substitutes**
for one. `IF-SEC-011` specifies that nothing crosses that boundary, and Phase 4 implements it as a
static assertion that no gated operation's permission derives from the toggle.

## Hide versus disable for an unauthorised operation

Recorded as unresolved, deliberately.

| Position | Argument |
| --- | --- |
| Hide it | Conceals the system's shape from an unauthorised operator |
| Disable it with a reason | Discloses what exists, and is actionable for an operator who should request access |

`AERORA-UI-REQ-008` governs **capability**, not authorisation, so it does not decide this case.
`IF-UIS-006` permits either and prohibits relying on visibility as the gate. It is a product and
operations decision under Q-029.

## Interface status

| Interface | Subject | Status |
| --- | --- | --- |
| `IF-SEC-001`, `IF-SEC-002` | Sign-in, local validation | `IMPLEMENTATION-PLANNED` |
| `IF-SEC-003` | Role resolution | **`BLOCKED`** — F3.1-04 |
| `IF-SEC-004` | Authorisation decision | `IMPLEMENTATION-PLANNED` — the abstraction; policy `CONTRACT-ONLY` |
| `IF-SEC-005` | Gated-operation set | `IMPLEMENTATION-PLANNED` — the declaration mechanism |
| `IF-SEC-006`, `IF-SEC-007` | Denial record, audit identity | `IMPLEMENTATION-PLANNED` |
| `IF-SEC-008` | Credential lifetime state | `IMPLEMENTATION-PLANNED` |
| `IF-SEC-011` | Advanced-UI toggle as a boundary | `IMPLEMENTATION-PLANNED` as a **prohibition assertion** |
| `IF-AUT-006` | Command submission | `IMPLEMENTATION-PLANNED` |
| `IF-AUT-011` | Operator authorisation decision | `IMPLEMENTATION-PLANNED` |

## Test strategy

| Test | Method | Asserts |
| --- | --- | --- |
| Gate precedes the wire | `UNIT` | No link write occurs for a denied operation |
| Denial produces a record and no command | `UNIT` | And the absence is not reported as a timeout |
| Unreachable gate denies | `UNIT` | Fail-closed |
| Empty role set denies everything | `UNIT` | No permissive default exists |
| Unrecognised role token grants nothing | `UNIT` | And is recorded |
| Decision not cached across a credential change | `UNIT` | — |
| Gated set is not derived from visibility | `STATIC` | The declaration is explicit |
| Advanced-UI toggle grants nothing | `STATIC` | **R3-007 conversion candidate** |
| Authorisation is necessary and not sufficient | `INTEGRATION` | An authorised command still passes the vehicle gate |
| Offline authorisation | `INTEGRATION` | Part of the all-network-blocked test |

Ten tests, all writable before Q-029 resolves, because every one asserts a property of the
**abstraction** rather than of a policy.

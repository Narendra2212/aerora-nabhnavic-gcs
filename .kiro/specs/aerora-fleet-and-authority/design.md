# Design Document

## Overview

This design serves the eleven requirements of
[requirements.md](requirements.md): the Aerora_Station as the single point of command authority over
a fleet of aircraft, their ground tether units, and the links between them.

Two constraints shape every decision in it, and neither is negotiable here.

**`DG-ROL` is `OPEN`, so the station may originate no operator command.** Requirement 5 of the
gate-closure spec holds the read-only posture until an operator role policy exists, and the
`actionable-control` rule in the source half rejects a new actionable control before it enters the
repository. Four of the eleven requirements describe command origination. None of that behaviour can
be built yet, and this design says so per requirement rather than leaving it to be discovered.

**`ADR-014` is left open at the product owner's request.** Whether a parameter write passes the
authorization boundary is undecided. The design's first structural decision exists to make that
deferral free rather than blocking, and it is the reason this document can be written at all.

## Architecture

### The decision that makes `ADR-014` deferrable

`AeroraGatedOperation` must grow for a Fleet_Operation, and it would also grow for a parameter
write. Those look like one change to one type, which is why deferring the second appeared to block
the first. They are not one change. They are **two orthogonal axes**:

| Axis | What it answers | What changes it |
| --- | --- | --- |
| **Addressing** | To whom does this operation apply? | A Fleet_Operation: one vehicle becomes an addressed set |
| **Subject** | What is being done? | `ADR-014`: a MAVLink command becomes one subject form among others |

Separating them lets the addressing axis be designed and built now while the subject axis stays at
exactly one form. When `ADR-014` closes, it adds a subject form and touches no addressing code. If
it closes the other way and parameter writes stay outside the boundary, nothing built here is
wasted.

The separation also yields fleet parameter writes for free if both are ever wanted, which a single
flat struct would not.

### No core edit is required

The existing hook is `QGCCorePlugin::authorizeMavCommand()`, consumed at
`MavCommandQueue::sendWorker()`. Changing that signature would be a second core edit of the
`ADR-016` class, needing its own owner signature. This design does not change it.

A Fleet_Operation is authorized **as a set, in the Aerora layer, before any command is enqueued**.
Only if the set is permitted are the resulting commands enqueued, and each one then passes the
unchanged per-command hook. So:

```text
operator action
      |
      v
Aerora fleet authorization        atomic: the whole set or none
      |
      +-- refused --> nothing is enqueued, every refusal named
      |
      v
per-command enqueue
      |
      v
QGCCorePlugin::authorizeMavCommand()      unchanged core hook, fires per command
      |
      v
MavCommandQueue::sendWorker()
```

Atomicity lives where it can be atomic. Transmission cannot be atomic, which Requirement 3
criterion 7 states outright, so the design does not pretend otherwise: it reports per-aircraft
transmission outcome separately from the single authorization decision.

### What is buildable while `DG-ROL` is open

The partition matters more than any other table in this document, because building the wrong half
would be rejected by the project's own checkers.

| Requirement | Buildable now | Held by |
| --- | --- | --- |
| 1 Command authority | The audit-before-transmit ordering, per-path reportability | Origination itself: `DG-ROL` |
| 2 Manual authority | The grant type, the mirrored-input read path | Issuing a grant is a command: `DG-ROL` |
| 3 Fleet operations | The addressing type, the all-or-nothing evaluation and its tests | Transmission and the UI control: `DG-ROL` |
| 4 Capability addressing | Resolution mechanism, and the unresolvable report of criterion 7 | The capability vocabulary: `DG-PAY` |
| 5 Link budget | **All of it.** Profile shape, channel accounting, derivation, unknown-without-profile | Nothing. Criterion 12 carries the profile with values absent |
| 6 Telemetry tiers | Tier reporting, the not-carried-versus-unknown distinction, tier derivation | Tier membership: `DG-TLM` |
| 7 Per-link freshness | The evaluation mechanism and the unachievable-threshold check | Threshold values: `DG-FRS` |
| 8 Tether endpoint | Attachment-versus-supply reporting, Power_Mode derivation | Commands to the unit: `DG-ROL`. Interlock set: `DG-PAY` |
| 9 Extensible namespace | Unprofiled and ambiguous reporting; the no-branch rule as a checker | Command namespace: `DG-MIS` |
| 10 Compliance | The hazard log, the overlay-edit record, and criterion 11 as a checker | Identifier format: `DG-FLT`. Retention: `DG-STO` |
| 11 Power failover | Criteria 1 and 3 as a checker; the failover observation read path | Battery reserve: `DG-PAY` |

Requirement 5 is the one requirement buildable end to end, which is why it leads the task plan.

### The derivation chain

One recorded input, everything else derived. This is the shape Requirement 5 asks for and the reason
the register holds no throughput figure of its own.

```text
Link_Profile          recorded, values absent until Q-015 closes
      |
      +--> sustained throughput per Link_Class
      |          |
      |          +--> aircraft count the class sustains at a tier
      |          |
      |          +--> which Telemetry_Tiers the class can carry
      |          |
      |          +--> achievable update interval
      |                     |
      |                     v
      |          freshness threshold achievability   (threshold itself stays recorded by DG-FRS)
      |
      +--> reported with every derived figure: the profile it came from
```

The asymmetry is deliberate. Throughput is arithmetic and is derived. A staleness threshold is a
safety judgement and stays recorded. What the profile contributes is telling an owner when their
recorded judgement is physically unachievable on a given link.

## Components and Interfaces

### The addressing axis

```cpp
/// To whom a gated operation applies. This axis grows independently of what is being done.
///
/// A single member is an Independent_Operation. More than one member with allOrNothing set is a
/// Fleet_Operation, whose authorization applies to every member or to none.
struct AeroraOperationAddress {
    QList<AeroraVehicleContext> vehicles;
    bool allOrNothing = false;

    /// The capability a Fleet_Operation requires, resolved against each vehicle's reported
    /// inventory. Carried as an opaque string for the reason mavCommand is carried as an int:
    /// no capability identifier is invented, declared or redefined by Aerora. Empty for an
    /// operation addressed directly rather than by capability.
    QString capabilityRequired;
};
```

### The subject axis

```cpp
/// What a gated operation does. Exactly one form today.
///
/// ADR-014 may add a second form for a parameter write. It would sit beside this one and touch no
/// addressing code, which is the whole purpose of the separation.
struct AeroraCommandSubject {
    int mavCommand = 0;
    int targetCompId = 0;
    bool commandInt = false;
    float param1 = 0.0f;
    float param2 = 0.0f;
    float param3 = 0.0f;
    float param4 = 0.0f;
    double param5 = 0.0;
    double param6 = 0.0;
    float param7 = 0.0f;
};

struct AeroraGatedOperation {
    AeroraOperationAddress address;
    AeroraCommandSubject command;
};
```

Today's flat fields move into `command`, and today's single `vehicle` becomes an `address` carrying
one member. `AeroraCorePlugin::authorizeMavCommand()` assembles this shape from the core hook's
arguments, so `QGCCorePlugin::authorizeMavCommand()` keeps its signature and no new core edit is
required.

### The fleet decision

```cpp
/// The outcome of authorizing one Fleet_Operation.
///
/// `permitted` is the all-or-nothing verdict. `perVehicle` carries a decision per addressed
/// vehicle so a refusal names which. `excludedByCapability` is distinct from a refusal: an aircraft
/// reporting no satisfying capability is removed from the addressed set *before* authorization and
/// reported, which Requirement 4 criterion 3 requires.
struct AeroraFleetDecision {
    bool permitted = false;
    QList<AeroraAuthorizationDecision> perVehicle;
    QStringList excludedByCapability;
};
```

### The link profile

```cpp
/// The recorded parameter set of a Link_Class and the value of each parameter.
///
/// Neither the parameter set nor its values are written in Aerora source. Both are register
/// content, and a profile carrying no values is reported as unknown rather than defaulted, which
/// Requirement 5 criterion 9 requires. Every figure below is derived on read and none is stored,
/// so a profile change cannot leave a stale copy behind.
class AeroraLinkProfile
{
public:
    [[nodiscard]] QString linkClass() const;
    [[nodiscard]] bool carriesValues() const;

    [[nodiscard]] std::optional<double> sustainedThroughput() const;
    [[nodiscard]] std::optional<int> aircraftSustainedAtTier(const QString &tier) const;
    [[nodiscard]] std::optional<double> achievableUpdateInterval(const QString &tier) const;

    /// Identifies which profile a derived figure came from, per criterion 8.
    [[nodiscard]] QString revision() const;
};
```

### The manual authority grant

```cpp
/// A time-bounded authorization under which a Flight_Controller may accept manual control input.
///
/// The station issues it and the Flight_Controller enforces it. The station is never on the manual
/// control path, which Requirement 2 criterion 1 forbids, so this type carries no stick value.
struct AeroraManualAuthorityGrant {
    QString operatorId;                 ///< Opaque. Not a credential.
    qint64 issuedAtMonotonicUs = 0;
    qint64 expiresAtMonotonicUs = 0;

    [[nodiscard]] bool inForce(qint64 nowMonotonicUs) const;
};
```

### The tether endpoint

The Tether_Unit is addressed over its own link and speaks its own protocol, so it is allocated no
MAVLink channel. Attachment and supply are separate observations, because an attached tether that
has stopped supplying is neither absent nor healthy:

```cpp
enum class AeroraTetherAttachment { Unknown, Absent, Attached };
enum class AeroraTetherSupply     { Unknown, NotSupplying, Supplying };
```

Power_Mode is derived from the capability inventory the aircraft reports and never from the presence
of a tether link, which Requirement 8 criterion 8 requires. A station that inferred mode from link
presence would report battery mode for a tethered aircraft whose link had merely dropped.

### The power failover observation

Read-only by construction. The station observes a failover the aircraft reports; it never causes
one and never participates in one. The type therefore carries no transition command and no timeout.

## Data Models

Three additions to the Gate_Register, each following a pattern the register already uses.

### `linkProfiles`

One entry per Link_Class. `parameters` may be an empty object, which is the recorded state of a
profile whose values `Q-015` has not settled, and `heldBy` names what settles them. This is the
`CONTRACT-ONLY` pattern applied to data: the shape is reviewable before the values exist.

### `overlayEdits`

The allow-list of files outside `aerora/` that legitimately carry Aerora identifiers, each with its
path, the reason and the owner who authorised it. Same ratchet shape as
`mavlinkIdentifiers.allowList`: an over-approximating rule needs a recorded baseline or it is a
cliff rather than a ratchet.

Five sites exist today, and naming them is what makes a sixth a reviewable decision:

| Path | What it carries |
| --- | --- |
| `src/Vehicle/Vehicle.h` | The `ADR-016` friend declaration, already gated by `adr-016-pair` |
| `test/Vehicle/MavCommandAuthorizationTest.cc` | A core-tree test including Aerora security headers |
| `cmake/QGCTest.cmake` | A comment referring to the overlay's test registration |
| `.github/workflows/aerora-linux.yml` | The overlay's Linux CI leg |
| `.github/workflows/aerora-windows.yml` | The overlay's Windows CI leg |

### `hazards`

The hazard log Requirement 10 criterion 9 requires: an identifier, the statement, the severity, the
mitigation and the owner who accepted it. Two entries arrive with this design, and both record a
decision to keep software out of a path rather than to put it in one.

| Hazard | Mitigation recorded |
| --- | --- |
| Manual control input routed through a general-purpose operating system has unbounded worst-case latency | Sticks reach the aircraft directly; the station grants and revokes mode |
| A tether supply interruption long enough to brown out an ESC stops the motors | The transfer is electrical and the station is forbidden from participating |

## Correctness Properties

### Property 1: A Fleet_Operation is authorized atomically

*For any* addressed set and *any* per-member authorization outcome, the operation is transmitted to
every member or to none, and a refusal names every member refused.
**Validates: Requirements 3.3, 3.4.**

### Property 2: Addressing and subject vary independently

*For any* address and *any* subject, the authorization decision depends on both, and no addressing
code reads a subject field. This is the property that keeps `ADR-014` deferrable: if it holds, a
second subject form touches no addressing code.
**Validates: Requirements 3.1, 3.2, 3.8.**

### Property 3: Every derived link figure follows from its profile

*For any* Link_Profile, each figure the station reports as derived is reproducible from that profile
alone, and changing any parameter changes every dependent figure.
**Validates: Requirements 5.5, 5.7.**

### Property 4: A profile carrying no values yields no figure

*For any* Link_Class whose profile carries no values, every dependent figure reports unknown and
none reports a default.
**Validates: Requirements 5.9.**

### Property 5: Capability exclusion is not refusal

*For any* addressed set, an aircraft excluded for capability is reported excluded and never
refused, and its exclusion does not prevent the operation reaching the rest.
**Validates: Requirements 4.3.**

### Property 6: Tether attachment and supply are independent

*For any* attachment state and *any* supply state, the station reports both, and no combination is
collapsed into one value or reported as absent-and-healthy.
**Validates: Requirements 8.11.**

### Property 7: An unachievable threshold is named, not applied

*For any* recorded freshness threshold and *any* Link_Profile, a threshold shorter than the
profile's achievable update interval is reported unachievable on that class rather than marking
every value it carries `STALE`.
**Validates: Requirements 7.8.**

### Property 8: The station is absent from the failover path

*For any* reported Power_Source_Failover, no station code path participates, and the source tree
carries no station-originated transition command and no station timeout governing it.
**Validates: Requirements 11.1, 11.3.**

### Property 9: Aerora logic stays inside the overlay

*For any* file outside `aerora/`, it carries an Aerora identifier only where `overlayEdits` records
it, with a reason and an owner.
**Validates: Requirements 10.10, 10.11.**

## Error Handling

Four distinct absences, which must not collapse into one another. Each loses different information
if merged.

| Absence | Means | Where it comes from |
| --- | --- | --- |
| `UNKNOWN` | Never reported, and the vehicle has not said whether it can | State_Vocabulary |
| `UNAVAILABLE` | The vehicle reported that it cannot provide this | State_Vocabulary, producible only from a capability report |
| Not carried by the live tier | The value exists, and this Telemetry_Tier omits it | Requirement 6 criterion 4 |
| Unknown for want of a profile | No Link_Profile values, so nothing to derive from | Requirement 5 criterion 9 |

A station that reported all four as `UNKNOWN` would be telling an operator the same thing about a
value the aircraft cannot supply, a value this link does not carry, and a value nobody has
configured a link for.

## Testing Strategy

The authoring host has no compiler, no Qt and 3.9 GB of free disk, so what is testable where is a
design constraint rather than a preference.

| Layer | Where | Needs |
| --- | --- | --- |
| The three new register blocks | `tools/tests/test_gate_register.py` | Python only, runs on the authoring host |
| The derivation chain as a model | `tools/tests/` | Python only |
| The new checker rules | `tools/tests/test_gate_guard_check.py` | Python only |
| Addressing and subject types, fleet evaluation | `aerora/test/Security/` | A build: CI only |
| Tier and freshness reporting | `aerora/test/Telemetry/` | A build: CI only |
| Tether attachment versus supply | `aerora/test/` | A build: CI only |
| Manual grant expiry | `aerora/test/Security/` | A build: CI only |
| The failover itself | Bench, with hardware | **Not establishable in software** |

The last row is stated rather than omitted. No software test can show that a battery picks up a
kilowatt load without an ESC brownout; that is an electrical measurement, and a test suite that
appeared to cover it would be worse than none.

## Design decisions and alternatives considered

**Separating addressing from subject, rather than one flat operation struct.** A flat struct
carrying both a vehicle set and an optional parameter name would make every addressing change a
parameter-write change and vice versa, which is exactly why `ADR-014` looked blocking. Rejected
because it couples two decisions with different owners and different timelines.

**Authorizing a Fleet_Operation in the Aerora layer, rather than widening the core hook.**
Widening `QGCCorePlugin::authorizeMavCommand()` to take a vehicle set would be a second core edit of
the `ADR-016` class, needing its own owner signature and adding upstream divergence. Rejected:
atomicity belongs where the decision is made, and the per-command hook still fires unchanged
afterwards.

**Grant-based manual authority, rather than routing sticks through the station.** Passthrough was
the product owner's first preference and gives authority only while the host is healthy; a stalled
host yields no control at all. Rejected on the hazard, with the grant model giving equal authority,
bounded latency and a defined failure mode. Recorded in the hazard log rather than only here.

**Deriving throughput, rather than recording it.** A recorded throughput figure is a second number
to keep in step with the radio it describes, and nothing would detect the two diverging. Rejected
in favour of deriving every figure from the profile and reporting which profile it came from.

**An `overlayEdits` allow-list, rather than a blanket prohibition on Aerora identifiers outside
`aerora/`.** A blanket rule would reject the tree today, because five legitimate sites exist, and a
rule that fails on a clean tree gets removed rather than obeyed. The allow-list makes the five
reviewable and a sixth a decision.

## Open items this design hands on

| Item | Owner | What it blocks |
| --- | --- | --- |
| `ADR-014`: whether a parameter write passes the boundary | Architecture | A second subject form. Nothing else, by construction |
| `Q-015`: Link_Profile parameter values | Architecture | Every derived link figure; the shape is buildable now |
| `DG-ROL`: the operator role policy | Product, operations | Every origination path: fleet transmission, grant issue, tether commands |
| `DG-PAY`: capability vocabulary, interlock set, battery reserve | Product, engineering, Firmware | Capability resolution, the interlock list, tethered-flight readiness |
| `DG-TLM`: tier membership | Firmware, Product | Which values each tier carries |
| `DG-FRS`: per-class thresholds | Product | Freshness classification, though achievability is derivable now |
| `DG-MIS`: the command namespace and its allocation rule | Product | Per-product mission commands |
| The cloud and licensing boundary | Product | Held at the product owner's request |
| Whether the fifteen-channel ceiling is raised | Architecture | Dual-link operation above seven aircraft |

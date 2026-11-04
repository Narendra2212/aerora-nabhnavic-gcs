# Requirements Document

## Introduction

The Aerora_Station as the single point of command authority over a fleet of aircraft, the ground
tether units that power them, and the links between them.

This document extends [aerora-decision-gate-closure](../aerora-decision-gate-closure/requirements.md)
and does not replace it. The ten Decision_Gates, the read-only posture and the Contract_Value
prohibition all continue to apply here unchanged.

### What this document does not define

**It defines no Contract_Value.** No fact name, unit, value range or enumeration member; no
freshness threshold; no role name or permission; no capability identifier; no tether item; no
interlock identifier; no MAVLink message, command or enumeration identifier; no telemetry tier
membership; no product-family identifier.

Every one of those is settled by a Decision_Gate and signed by a named owner. This document settles
**shapes and obligations**: how many aircraft the station may command and what bounds that number,
what a fleet operation is and how it fails, which paths may carry a command and which may carry only
telemetry, and what the station must report while a decision is still open. Where a requirement
needs a value it does not have, it names the Gate that owns it.

Inventing a value here would be the failure mode the gate mechanism exists to prevent, and the
source-half checkers would reject the change before it entered the repository.

### Gates this document reframes

Three Gates are currently scoped to settle a **list** where the Aerora product range needs them to
settle a **scheme**. None of the three holds an accepted Closure_Record, so reframing them now costs
nothing; reframing them after signature would cost a re-signature per product.

| Gate | Currently scoped as | Must be scoped as | Why |
| --- | --- | --- | --- |
| `DG-MIS` | One enumerated mission command set | An extensible command namespace with per-product groups and an allocation rule | Each new product family carries its own mission commands under its own firmware |
| `DG-TLM` | One telemetry dialect | A tiered dialect, one tier per Link_Class | A low-rate link cannot carry the full set for a fleet |
| `DG-FRS` | One freshness threshold set | One threshold set per Link_Class | A reading two seconds old is stale on a high-rate link and normal on a low-rate one |

## Glossary

These extend the glossary of the gate-closure requirements. Each names a shape; none carries a
value.

- **Independent_Operation**: an authorized operation addressed to exactly one aircraft.
- **Fleet_Operation**: a single authorized operation addressed to more than one aircraft, which
  either applies to every addressed aircraft or to none.
- **Capability_Address**: the addressing of a Fleet_Operation by the capability it requires rather
  than by airframe type or by vehicle identity.
- **Manual_Authority_Grant**: a time-bounded authorization, issued by the Aerora_Station, under
  which a Flight_Controller may accept manual control input. It expires without refresh.
- **Tether_Unit**: the ground power and spool system, addressable by the Aerora_Station over its own
  link and independent of any aircraft link.
- **Tether_Interlock**: a member of a declared, enumerated set of safety responses that a Tether_Unit
  and a Flight_Controller may exchange directly, carrying no operator command.
- **Link_Class**: a category of communication link distinguished by its sustained throughput and its
  latency characteristics.
- **Telemetry_Tier**: a named subset of the telemetry dialect sized to a Link_Class.
- **Power_Mode**: either tether or battery. In battery Power_Mode no Tether_Unit is present.

## Requirements

### Requirement 1: The Aerora_Station is the single point of command authority

**User Story:** As the product owner, I want every operator command to originate at the
Aerora_Station, so that no path reaches an aircraft or a Tether_Unit without passing the station's
authorization and its audit trail.

#### Acceptance Criteria

1. THE Aerora_Station SHALL be the only originator of an operator command to a Flight_Controller or
   to a Tether_Unit.
2. THE Aerora_Station SHALL record every operator command it originates in the audit trail before
   the command is transmitted.
3. WHERE a path exists between a Flight_Controller and a Tether_Unit that the Aerora_Station does
   not mediate, THE Gate_Register SHALL record that path, the content class it carries, and that it
   carries no operator command.
4. IF a proposed repository change introduces a path able to carry an operator command to a
   Flight_Controller or a Tether_Unit without passing the Aerora_Station, THEN THE Gate_Review SHALL
   reject the change before the change enters the repository.
5. THE Aerora_Station SHALL accept no inbound network connection for the purpose of originating a
   command, which is `IF-CLD-003` unchanged.
6. WHILE `DG-ROL` is `OPEN`, THE Aerora_Station SHALL originate no operator command, which is
   Requirement 5 of the gate-closure document unchanged.
7. THE Aerora_Station SHALL report, per command path it holds, whether that path is presently able
   to carry a command, and SHALL report the reason where it is not.

### Requirement 2: Manual control authority without a latency dependency

**User Story:** As a flight-safety reviewer, I want manual control input to reach the aircraft
without traversing the ground station, while the station still decides whether manual control is
permitted, so that command authority does not become a single point of failure for flight control.

#### Acceptance Criteria

1. THE Aerora_Station SHALL NOT carry manual control input on the path between a manual control
   device and a Flight_Controller.
2. THE Aerora_Station SHALL issue a Manual_Authority_Grant as an authorized operation recorded in
   the audit trail.
3. A Manual_Authority_Grant SHALL carry an expiry, and SHALL require refresh to remain in force.
4. WHILE no unexpired Manual_Authority_Grant is in force, THE Flight_Controller SHALL accept no
   manual control input.
5. WHEN the Aerora_Station loses its link to a Flight_Controller, THE Manual_Authority_Grant SHALL
   expire without refresh.
6. THE Gate_Register SHALL record the behaviour a Flight_Controller adopts when a
   Manual_Authority_Grant expires in flight, which `DG-ROL` settles.
7. THE Flight_Controller SHALL report manual control input positions and the current grant state to
   the Aerora_Station as telemetry, so that the station observes manual control without carrying it.
8. THE Aerora_Station SHALL record every Manual_Authority_Grant issued, refused, refreshed and
   expired, with the operator identity and the time of each.
9. IF a proposed repository change places manual control input on a path through the Aerora_Station,
   THEN THE Gate_Review SHALL reject the change and SHALL name the hazard record that rejects it.
10. THE Gate_Register SHALL record the hazard motivating criterion 1, its severity, its mitigation
    and the owner who accepted the mitigation.

### Requirement 3: Independent and fleet operations

**User Story:** As an operator, I want to command one aircraft or a group of aircraft, so that a
group acts together while a single aircraft can still be handled on its own.

#### Acceptance Criteria

1. THE Aerora_Station SHALL support an Independent_Operation addressed to exactly one aircraft.
2. THE Aerora_Station SHALL support a Fleet_Operation addressed to more than one aircraft.
3. A Fleet_Operation SHALL apply to every addressed aircraft or to none.
4. IF authorization of a Fleet_Operation is refused for any addressed aircraft, THEN THE
   Aerora_Station SHALL transmit the operation to no addressed aircraft and SHALL name each aircraft
   whose authorization was refused.
5. THE Aerora_Station SHALL record a Fleet_Operation as one operation carrying its addressed set,
   rather than as one operation per aircraft.
6. THE Aerora_Station SHALL record the transmission outcome per addressed aircraft.
7. WHERE a Fleet_Operation was authorized and then partially transmitted because a link failed, THE
   Aerora_Station SHALL name each aircraft that did not receive it, because authorization is atomic
   and transmission cannot be.
8. THE Aerora_Station SHALL NOT synthesise a Fleet_Operation from Independent_Operations, nor an
   Independent_Operation from a Fleet_Operation.
9. THE Gate_Register SHALL record which operations may be addressed as a Fleet_Operation, which
   `DG-ROL` settles.

### Requirement 4: Fleet operations are addressed by capability

**User Story:** As an operator of a mixed fleet, I want a fleet command addressed to what an
aircraft can do rather than to what it is, so that a fixed wing and a multirotor can serve one
mission without the station translating per airframe type.

#### Acceptance Criteria

1. A Fleet_Operation SHALL carry a Capability_Address.
2. THE Aerora_Station SHALL resolve a Capability_Address against the capability inventory each
   aircraft reports.
3. IF an addressed aircraft reports no capability satisfying the Capability_Address, THEN THE
   Aerora_Station SHALL exclude that aircraft from the addressed set before authorization and SHALL
   report the exclusion.
4. THE Aerora_Station SHALL NOT infer a capability from an airframe type, a firmware identifier or a
   product family.
5. THE Aerora_Station SHALL NOT translate one Fleet_Operation into differing per-airframe
   operations; that translation belongs to each airframe's firmware plugin.
6. THE Gate_Register SHALL record the capability vocabulary a Capability_Address resolves against,
   which the capability part of `DG-PAY` settles.
7. WHILE the capability part of `DG-PAY` holds no accepted Closure_Record, THE Aerora_Station SHALL
   report a Capability_Address as unresolvable rather than addressing every aircraft.

### Requirement 5: Fleet size is derived from the link budget, not asserted

**User Story:** As a systems architect, I want the supported fleet size to follow from the measured
link budget rather than from a number written in a specification, so that a configuration which
cannot work is refused before a flight rather than discovered during one.

#### Acceptance Criteria

1. THE Aerora_Station SHALL allocate one MAVLink channel per active link and SHALL treat fifteen
   allocatable channels per station instance as the ceiling, which `R-013` records.
2. THE Aerora_Station SHALL report the number of channels allocated and the number remaining.
3. IF a proposed link allocation would exceed the channel ceiling, THEN THE Aerora_Station SHALL
   refuse the allocation and SHALL name the ceiling.
4. THE Aerora_Station SHALL derive, for each Link_Class, the sustained throughput the active
   Telemetry_Tier requires per aircraft.
5. IF the aggregate requirement of the addressed fleet exceeds the sustained throughput of its
   Link_Class, THEN THE Aerora_Station SHALL report the shortfall and SHALL name the aircraft count
   that Link_Class sustains at the active Telemetry_Tier.
6. THE Aerora_Station SHALL NOT present a fleet size that its active links cannot sustain.
7. THE Gate_Register SHALL record the sustained throughput of each Link_Class, which `Q-015`
   presently holds open.
8. WHERE the channel ceiling is raised by an upstream core edit, THE Gate_Register SHALL record the
   edit, its memory consequence and the owner who authorised it.

### Requirement 6: A tiered telemetry dialect

**User Story:** As an operator on a low-rate link, I want the station to tell me which telemetry
tier is live, so that I know whether I am seeing full aircraft state or the safety minimum.

#### Acceptance Criteria

1. THE Gate_Register SHALL record more than one Telemetry_Tier, each with its membership and the
   Link_Class it is sized for, which `DG-TLM` settles.
2. THE Gate_Register SHALL record one Telemetry_Tier as the fleet-safety tier, whose membership is
   the minimum an operator needs to supervise an aircraft.
3. THE Aerora_Station SHALL report which Telemetry_Tier is live, per aircraft.
4. IF a value is absent because the live Telemetry_Tier does not carry it, THEN THE Aerora_Station
   SHALL report it as not carried by the live tier and SHALL NOT report it as `UNKNOWN`.
5. THE Aerora_Station SHALL report the age of any value retained from a Telemetry_Tier that is no
   longer live.
6. THE Aerora_Station SHALL NOT derive a value the live Telemetry_Tier does not carry.
7. WHEN the live Telemetry_Tier changes for an aircraft, THE Aerora_Station SHALL report the change.
8. WHILE `DG-TLM` holds no accepted Closure_Record, THE Aerora_Station SHALL report the Gate
   identifier in place of each value the dialect would carry, which is Requirement 5 criterion 1 of
   the gate-closure document unchanged.

### Requirement 7: Freshness is evaluated per Link_Class

**User Story:** As a flight-safety reviewer, I want staleness judged against the link that carried
the value, so that a healthy low-rate link is not reported stale and a stalled high-rate link is not
reported fresh.

#### Acceptance Criteria

1. THE Gate_Register SHALL record one freshness threshold set per Link_Class, which `DG-FRS`
   settles.
2. THE Aerora_Station SHALL evaluate a value's freshness against the threshold set of the
   Link_Class that carried it.
3. THE Aerora_Station SHALL report, with each value it classifies `STALE`, the Link_Class whose
   threshold the value exceeded.
4. IF a value arrives on a Link_Class for which no threshold set is recorded, THEN THE
   Aerora_Station SHALL report that value's freshness as unevaluated and SHALL NOT report it fresh.
5. THE Aerora_Station SHALL NOT apply one Link_Class's threshold set to a value carried by another.
6. WHEN the Link_Class carrying a value changes, THE Aerora_Station SHALL re-evaluate that value's
   freshness against the new threshold set.
7. WHILE `DG-FRS` holds no accepted Closure_Record, THE Aerora_Station SHALL report observed
   freshness as unevaluated, which the `AeroraTelemetryState::ObservedFreshnessUnevaluated` guard
   holds.

### Requirement 8: The Tether_Unit endpoint and the two Power_Modes

**User Story:** As an operator in tether mode, I want the station to command the ground tether
system directly while the tether and the aircraft exchange only telemetry and declared interlocks,
so that command authority is preserved without placing a ground station inside a safety loop.

#### Acceptance Criteria

1. THE Aerora_Station SHALL address a Tether_Unit over a link independent of any aircraft link.
2. THE Aerora_Station SHALL originate every operator command to a Tether_Unit.
3. THE Aerora_Station SHALL record every command it originates to a Tether_Unit in the audit trail.
4. THE Gate_Register SHALL record the Tether_Interlock set as an enumerated, closed list, which the
   power part of `DG-PAY` settles.
5. A Tether_Interlock SHALL carry no operator command.
6. IF a proposed repository change adds a member to the Tether_Interlock set, THEN THE Gate_Review
   SHALL reject the change unless the Gate_Register records the addition together with the owner who
   authorised it.
7. THE Aerora_Station SHALL report the Power_Mode of each aircraft.
8. THE Aerora_Station SHALL derive Power_Mode from the capability inventory the aircraft reports,
   and SHALL NOT infer it from the presence or absence of a tether link.
9. WHILE an aircraft is in battery Power_Mode, THE Aerora_Station SHALL report every tether value as
   `UNAVAILABLE`, and SHALL NOT report it as `UNKNOWN` or as a numeric default.
10. WHERE a Tether_Unit speaks a protocol other than MAVLink, THE Aerora_Station SHALL allocate it
    no MAVLink channel.

### Requirement 9: An extensible product and mission namespace

**User Story:** As the product owner, I want a new product family to be an addition rather than a
change to shared code, so that a painting, firefighting or agriculture aircraft integrates without
reopening a decision an owner has already signed.

#### Acceptance Criteria

1. THE Gate_Register SHALL record the mission command namespace as extensible, carrying per-product
   command groups and an allocation rule, which `DG-MIS` settles.
2. THE allocation rule SHALL state who allocates a command identifier within a product group, and
   against which issued dialect or interface control document.
3. THE Aerora_Station SHALL resolve a product family through the product-profile interface and SHALL
   NOT branch on a product-family identifier in shared code.
4. THE Aerora_Station SHALL resolve a product profile from the capability inventory the aircraft
   reports.
5. IF no product profile matches an aircraft's capability inventory, THEN THE Aerora_Station SHALL
   attach no profile and SHALL report the aircraft as unprofiled.
6. IF more than one product profile matches an aircraft's capability inventory, THEN THE
   Aerora_Station SHALL attach no profile and SHALL report the ambiguity.
7. THE Aerora_Station SHALL NOT transmit, parse or match a product-family identifier against a
   MAVLink field.
8. WHEN a product family is added, THE Gate_Register SHALL record its command group, and THE
   Gate_Review SHALL NOT require re-signature of a command group already accepted.

### Requirement 10: Civil and industrial compliance, with the defence posture preserved

**User Story:** As the product owner, I want full civil and industrial regulatory compliance now and
the defence option kept open, so that the first market is served without foreclosing the second.

#### Acceptance Criteria

1. THE Gate_Register SHALL record the registration identifier format, its issuing authority, its
   uniqueness scope and its lifetime, which `DG-FLT` settles.
2. THE Gate_Register SHALL record which regulatory regime each deployment targets, and SHALL NOT
   assume one regime for every deployment.
3. THE Aerora_Station SHALL write a flight log locally before any upload, which `IF-LOG-016` states.
4. THE Gate_Register SHALL record the flight-log retention period and the tamper-evidence mechanism,
   which `DG-STO` settles.
5. THE Aerora_Station SHALL report airspace zone data together with its retrieval age, which
   `IF-CLD-012` states.
6. IF airspace zone data is unavailable, THEN THE Aerora_Station SHALL report it unavailable and
   SHALL NOT present an unbounded operating area.
7. WHERE regulation requires a permission artefact before flight, THE Gate_Register SHALL record
   that requirement and the artefact's validation mechanism, and THE Aerora_Station SHALL validate
   the artefact locally.
8. THE Aerora_Station SHALL NOT gate a flight on the reachability of a regulatory service where
   regulation does not require it.
9. THE Gate_Register SHALL record a hazard log, each entry carrying its severity, its mitigation and
   the owner who accepted the mitigation.
10. THE Gate_Register SHALL record every edit to the tree outside `aerora/` together with the owner
    who authorised it, so that the overlay's separability stays measurable.
11. IF a proposed repository change places Aerora logic outside `aerora/`, THEN THE Gate_Review
    SHALL reject the change unless the Gate_Register records the authorisation.

## Source Material

| Document | Supplies |
| --- | --- |
| [aerora-decision-gate-closure/requirements.md](../aerora-decision-gate-closure/requirements.md) | The ten Decision_Gates, the read-only posture, the Contract_Value prohibition and the State_Vocabulary |
| [02-interface-specification/15-cloud-integration-icd.md](../02-interface-specification/15-cloud-integration-icd.md) | `IF-CLD-003` no inbound connection, `IF-CLD-012` cloud-sourced data age, `IF-CLD-013` per-feature availability, `IF-LOG-016` local-first logging |
| [00-qgc-baseline-analysis/risk-register.md](../00-qgc-baseline-analysis/risk-register.md) | `R-013`, the fifteen-channel ceiling and its evidence |
| [00-qgc-baseline-analysis/open-questions.md](../00-qgc-baseline-analysis/open-questions.md) | `Q-015`, the worst-case concurrent link count, still open |
| `aerora/src/Vehicle/AeroraProductProfile.h` | The product-profile interface and its explicit registry |
| `aerora/src/Security/AeroraAuthorization.h` | `AeroraGatedOperation`, which carries exactly one vehicle today |

## What Stays Open

| Item | Owner | Why it is not settled here |
| --- | --- | --- |
| Where the backend runs, and the licensing boundary | Product | Held at the product owner's request; analysed but not decided |
| Whether `AeroraGatedOperation` gains a parameter-write form alongside its fleet form | Architecture | `ADR-014`; the two extensions are one structural change and should be designed together |
| Sustained throughput per Link_Class | Architecture | `Q-015`; needs the radio link budget |
| Whether a permission artefact is required before flight | Product, with regulatory counsel | Sources disagree; the answer changes whether a validation path must be built |

# Requirements Document

## Introduction

Fourteen of the Aerora screens are `VERIFIED` and one is `SUPERSEDED`. The screens that remain are
not blocked on code. Each one waits on a decision that nobody has made: a mission command set, a
telemetry dialect, a freshness budget, an operator role policy, a payload and tether reported-value
set, an autonomy proposal model, a durable store, a fleet identity format, a fork point, or a
release-build discriminator for simulated sources.

This specification defines **what each of those ten decisions must settle, who may sign it, what
makes it signable, and which screens it releases.** It is a decision-closure specification, not an
implementation specification.

It defines **no contract of its own.** No fact name, unit, value range or enumeration member; no
freshness threshold; no role name or permission; no payload or tether item; no fleet identifier
format; no proposal schema element; no MAVLink identifier. Inventing any of those is the failure
mode this document exists to prevent, because every one of them is a flight-safety decision and a
placeholder would outlive the decision it substituted for (`P7-NOINV-002`).

Three things are deliberately out of scope because they already work and are verified in a booted
application window on CI:

- **The seams.** The `FlyViewCustomLayer.qml` override seam, `analyzePages()` and
  `toolBarIndicators()` registration, `AeroraVehicleComponent` setup-page publication with an empty
  `unresolvedPageResources()`, and the `AeroraUi` typed read-only QML context.
- **The state model.** The seven-class `AeroraTelemetryState` vocabulary, `AeroraSecurityStatus`,
  `AeroraProvenance`, and the `AeroraSelfReportRow` / `AeroraOverlayValueRow` / `AeroraStateText`
  controls.
- **The harness.** `QmlUITestBase` and the suites that click through the real UI.

Re-specifying any of those is a defect against this document.

The specification also records the mistake that produced five mislabelled blockers: the screen
inventory's "Blocked by" column was carrying both gates and acceptance criteria, and four
deliverable screens sat idle because a statement about what a screen must *do* was read as a reason
work could not *start*. Requirement 2 makes that distinction a rule rather than a reviewer's
vigilance.

## Glossary

- **Aerora_Station**: the QGroundControl fork at `c:\aerora\AeroraGCS` with the Aerora overlay
  compiled in and running.
- **Gate**: an absent decision, an absent source of reported data, or an absent seam that prevents
  work on a screen from starting. The ten Gates are identified `DG-MIS`, `DG-TLM`, `DG-FRS`,
  `DG-ROL`, `DG-PAY`, `DG-AUT`, `DG-STO`, `DG-FLT`, `DG-VID` and `DG-SIM`.
- **Acceptance_Criterion**: a statement of what a screen displays, or how a screen behaves, once the
  screen is built. An Acceptance_Criterion is not a Gate.
- **Decision_Content**: the elements a Gate settles. Recorded as element names rather than as element
  values.
- **Closure_Record**: the artifact an owner signs, carrying one Gate's settled Decision_Content.
- **Sign_Off_Condition**: a condition a reviewer checks against a Closure_Record by inspection,
  before the Gate is recorded `CLOSED`.
- **Gate_Register**: the single authoritative record of the ten Gates, their status, owners,
  Sign_Off_Conditions, Release_Sets and excluded Acceptance_Criteria.
- **Gate_Dossier**: the per-Gate section of the Gate_Register. Referenced as "THE `DG-xxx`
  Gate_Dossier".
- **Gate_Review**: the review that checks a submitted Closure_Record against its Gate's
  Sign_Off_Conditions, and checks a proposed repository change against the open Gates.
- **Release_Set**: the screens a Gate releases when its Closure_Record is accepted.
- **Released**: a screen's remaining work may start. A released screen is neither delivered nor
  verified.
- **Two_Gate_Screen**: a screen that appears in two Release_Sets. `SCR-LIV-05`, `SCR-AIS-03`,
  `SCR-MIS-05` and `SCR-MIS-06`.
- **Delivered_Screen**: one of the 14 `VERIFIED` screens or the 1 `SUPERSEDED` screen.
- **Verified_Infrastructure**: the seams, state model and harness listed in the Introduction.
- **State_Vocabulary**: the seven `AeroraTelemetryState` classes. `REPORTED`, `EFFECTIVE`,
  `REQUESTED`, `PROPOSED`, `STALE`, `UNKNOWN`, `UNAVAILABLE`.
- **Contract_Value**: any of a fact name, a unit, a value range, an enumeration member, a freshness
  threshold, a role name, a permission, a payload item, a tether item, a consumable item, a fleet
  identifier format, a proposal schema element, or a MAVLink message, command or enumeration
  identifier.
- **Action_Introducing_Change**: a repository change that adds an operator action control, an
  outbound command path, a mission upload, start or resume path, or a call to
  `AeroraMissionObserver::verify()`.

## Requirements

### Requirement 1: Ten independently signable gates

**User Story:** As the programme lead, I want each gate recorded with its own owner and release set,
so that closing one decision releases its screens without waiting on the other nine.

#### Acceptance Criteria

1. THE Gate_Register SHALL record exactly ten Gates, identified `DG-MIS`, `DG-TLM`, `DG-FRS`,
   `DG-ROL`, `DG-PAY`, `DG-AUT`, `DG-STO`, `DG-FLT`, `DG-VID` and `DG-SIM`, with each identifier
   recorded once and no further Gate recorded.
2. THE Gate_Register SHALL hold one Gate_Dossier per recorded Gate, each recording the Gate's status
   as `OPEN` or `CLOSED`, the Decision_Content as element names rather than as element values, the
   signing owner of each part of the Decision_Content, the implementing owner, the
   Sign_Off_Conditions, the Release_Set, and the Acceptance_Criteria excluded from the Gate.
3. WHEN THE Gate_Review accepts every part of one Gate's Closure_Record, THE Gate_Register SHALL
   record as Released each screen in that Gate's Release_Set that is not a Two_Gate_Screen,
   independently of the status of the other nine Gates and in the repository change that records the
   Gate as `CLOSED`.
4. WHERE a screen is a Two_Gate_Screen, WHEN THE Gate_Review accepts the second of the two Gates'
   Closure_Records, THE Gate_Register SHALL record the screen as Released.
5. THE Gate_Register SHALL record `SCR-LIV-05` against `DG-ROL` and `DG-AUT`, `SCR-AIS-03` against
   `DG-AUT` and `DG-STO`, and `SCR-MIS-05` and `SCR-MIS-06` against `DG-MIS` and `DG-PAY`, with each
   of those four screens in the Release_Set of both of its Gates and no other screen in more than one
   Release_Set.
6. THE Gate_Register SHALL record each undelivered screen of the screen inventory either in at
   least one Release_Set or as Released with every entry in the screen's blocking list classified
   as an Acceptance_Criterion, and SHALL record no undelivered screen under both.
7. THE Gate_Register SHALL record every Delivered_Screen as outside every Release_Set, and SHALL
   record each screen of the screen inventory as either a Delivered_Screen or an undelivered
   screen.
8. THE Gate_Register SHALL record every element of Verified_Infrastructure as outside every Gate's
   Decision_Content.
9. IF a Gate_Dossier states a Decision_Content element that an element of Verified_Infrastructure
   already supplies, THEN THE Gate_Review SHALL remove the stated element from that Gate's
   Decision_Content and SHALL record the supplying element of Verified_Infrastructure in its place.
10. IF one of a Two_Gate_Screen's two Gates holds an accepted Closure_Record and the other does not,
    THEN THE Gate_Register SHALL record the screen as not Released and SHALL name the Gate whose
    Closure_Record is not accepted.
11. IF a Gate_Dossier records a Delivered_Screen in its Release_Set, THEN THE Gate_Review SHALL
    remove the screen from that Release_Set and SHALL record the screen's status in the screen
    inventory as the reason for the removal.
12. IF an undelivered screen appears in no Release_Set and an entry in that screen's blocking list is
    classified as a Gate, THEN THE Gate_Review SHALL record the screen as not Released and SHALL name
    the Gate whose Release_Set must carry the screen.

### Requirement 2: A gate and an acceptance criterion are distinguished

**User Story:** As a reviewer, I want each blocking entry classified as a gate or an acceptance
criterion, so that a statement about what a screen must do stops deferring deliverable work.

#### Acceptance Criteria

1. THE Gate_Register SHALL classify each entry recorded in a screen's blocking list in the screen
   inventory as exactly one of a Gate or an Acceptance_Criterion.
2. THE Gate_Register SHALL classify as an Acceptance_Criterion every entry that states what a screen
   displays or how a screen behaves once the screen is built, including an entry the screen satisfies
   by displaying an empty reading, by displaying a class of the State_Vocabulary, or by having no path
   for the action the entry restricts.
3. THE Gate_Register SHALL classify an entry as a Gate only where the entry names an absent decision,
   an absent source of reported data or an absent seam recorded as Decision_Content of one of the ten
   Gates, and no construction of the screen satisfies the entry while that absence stands.
4. WHEN an entry is classified as an Acceptance_Criterion, THE Gate_Register SHALL record the entry
   against the screen's own definition rather than as a reason the screen's work cannot start.
5. WHEN the classification of a screen's blocking list leaves no entry classified as a Gate, THE
   Gate_Register SHALL record the screen as Released.
6. IF a Gate_Dossier states a condition that a released screen satisfies once built, THEN THE
   Gate_Review SHALL record that condition as an Acceptance_Criterion excluded from the Gate's
   Decision_Content.
7. THE Gate_Register SHALL record the five screens whose blocking entries were misclassified, with the
   misread entry and its reclassification as an Acceptance_Criterion: `SCR-VID-01` against
   `P7-VID-004`, `SCR-VEH-03` against `Q3-009`, `SCR-VEH-04` against `P7-VEH-004`, `SCR-SEC-01`
   against `P7-SEC-005`, and `SCR-HLP-03` against `P7-HELP-002`.
8. THE Gate_Register SHALL record as Released the undelivered screens that appear in no Release_Set,
   with each remaining entry classified as an Acceptance_Criterion: `SCR-LIV-08`, `SCR-LIV-09`,
   `SCR-SET-01`, `SCR-SET-03` and `SCR-SET-04`.
9. IF an entry in a screen's blocking list does not meet both conditions of criterion 3, THEN THE
   Gate_Review SHALL classify the entry as an Acceptance_Criterion.
10. IF a proposed repository change adds an entry to a screen's blocking list without classifying the
    entry under criterion 2 or criterion 3, THEN THE Gate_Review SHALL reject the change and name the
    unclassified entry.
11. THE Gate_Register SHALL record `SCR-VEH-03`, `SCR-VEH-04`, `SCR-SEC-01` and `SCR-HLP-03` as
    retaining no entry classified as a Gate, and `SCR-VID-01` as retaining one entry classified as a
    Gate, which is the absent seam recorded against `DG-VID`.

### Requirement 3: Closure records, sign-off and screen release

**User Story:** As a gate owner, I want a defined artifact to sign and a defined check against it,
so that my signature closes the gate and the release that follows is unambiguous.

#### Acceptance Criteria

1. THE Closure_Record SHALL carry the Gate identifier, the Decision_Content the record settles as
   either the Gate's whole Decision_Content or one part the Gate_Dossier records, a settled entry
   for each element of that Decision_Content, the signature of each owner the Gate_Dossier records
   for it, the sign-off date, and the commit at which the record enters the repository.
2. WHEN a Closure_Record is submitted, THE Gate_Review SHALL check the record against every
   Sign_Off_Condition the Gate_Dossier records for the Decision_Content the record settles, and
   SHALL record, for each of those Sign_Off_Conditions, whether the record satisfies it.
3. IF a submitted Closure_Record carries no settled entry for an element of the Decision_Content the
   record settles, THEN THE Gate_Review SHALL hold the Gate at `OPEN` and name the omitted element.
4. IF a submitted Closure_Record or an amendment to a Closure_Record omits the signature of an owner
   the Gate_Dossier records for the Decision_Content the record settles, or carries a signature from
   a party the Gate_Dossier does not record as an owner of that Decision_Content, THEN THE
   Gate_Review SHALL hold the Gate at `OPEN` and name each owner whose signature is required.
5. WHEN a submitted Closure_Record satisfies every Sign_Off_Condition recorded for the
   Decision_Content it settles, THE Gate_Review SHALL record that Closure_Record as accepted, with
   the accepting review and the sign-off date.
6. WHERE a Gate's Decision_Content divides into parts with separate owners, THE Gate_Register SHALL
   record, for each part, the Decision_Content elements the part settles, each owner authorised to
   sign the part, the Sign_Off_Conditions the part must satisfy, and whether the part is accepted.
7. WHEN every Decision_Content element the Gate_Dossier records for a Gate is settled by an accepted
   Closure_Record, THE Gate_Register SHALL record that Gate as `CLOSED`.
8. WHEN a Gate is recorded `CLOSED`, THE Gate_Register SHALL record as Released each screen in the
   Gate's Release_Set other than a Two_Gate_Screen whose other Gate is not recorded `CLOSED`, and
   SHALL leave the delivery status of every screen in that Release_Set unchanged.
9. THE Gate_Register SHALL record a Released screen's delivery status from the status vocabulary of
   the screen inventory, and SHALL record `VERIFIED` only for a screen with a recorded observation
   of that screen navigated to and drawn in a booted application window.
10. WHILE a Decision_Content element the Gate_Dossier records for a Gate is not settled by an
    accepted Closure_Record, THE Gate_Register SHALL record no screen in that Gate's Release_Set as
    Released.
11. IF an accepted Closure_Record is found to carry no settled entry for a Decision_Content element
    the Gate_Dossier records, THEN THE Gate_Review SHALL return the Gate to `OPEN`, name the omitted
    element and the amendment required to settle it, and retain the accepted status of the elements
    the record settles.
12. WHILE a Decision_Content element the Gate_Dossier records is not settled by an accepted
    Closure_Record, THE Gate_Review SHALL reject a proposed repository change that introduces a
    Contract_Value that element settles and SHALL name the element.

### Requirement 4: No contract value is authored before its gate closes

**User Story:** As the programme lead, I want an open gate to produce its own identifier instead of a
plausible value, so that no flight-safety contract enters the product by invention.

#### Acceptance Criteria

1. WHILE a Gate is `OPEN`, THE Aerora_Station SHALL report the Gate identifier in place of each
   Contract_Value the Gate settles, on every screen on which that Contract_Value would otherwise
   appear.
2. IF a proposed repository change introduces a Contract_Value whose Gate holds no accepted
   Closure_Record, THEN THE Gate_Review SHALL reject the change before the change enters the
   repository and SHALL name the introduced element together with the Gate whose Decision_Content
   records it, or, where no Gate_Dossier records the element, name the element as unassigned
   Decision_Content.
3. WHILE a Gate is `OPEN`, THE Aerora_Station SHALL render each value the Gate settles in exactly
   one class from the State_Vocabulary, rather than as a blank field or as an unclassified reading.
4. WHILE a Gate is `OPEN`, THE Aerora_Station SHALL derive the `UNAVAILABLE` class from a vehicle
   report alone, rather than from the Gate's open status or from a station-side default, which is
   `UI-ST-07`.
5. THE Gate_Register SHALL record each mechanical guard that holds an open Gate visible, with the
   value the guard holds and the Gate or Gates that value holds open, covering at least
   `AeroraPowerModel::kDefinedThresholdCount` at zero,
   `AeroraVehicleFactGroupRegistrar::isRegistrationAvailable()` at false,
   `AeroraCorePlugin::isAuthorizationEnforced()` at false,
   `kGroundStationActuatorPathImplemented` at false, `kAutonomousPayloadActionImplemented` at false,
   `kAutomaticSourceTransitionImplemented` at false, `kRemainingEnergyEstimationImplemented` at
   false, `kGcsPayloadStateReplayImplemented` at false, and the static MAVLink-identifier check.
6. WHEN a Closure_Record is accepted, THE Gate_Review SHALL record, for each guard recorded under
   criterion 5, whether the accepted Closure_Record permits that guard to change value.
7. WHILE a guard recorded under criterion 5 is mapped to a Gate that is `OPEN`, IF a proposed
   repository change alters that guard's value, THEN THE Gate_Review SHALL reject the change before
   the change enters the repository, name the guard and that Gate, and hold the Gate at `OPEN`.
8. THE Gate_Register SHALL record, for each of the ten Gates, every guard recorded under criterion 5
   that holds the Gate visible, or SHALL record that no guard holds the Gate visible and that the
   Gate rests on the Gate_Review of criterion 2 alone.
9. WHILE a guard recorded under criterion 5 is mapped to a Gate that is `OPEN`, IF a proposed
   repository change removes that guard or removes the check that asserts its recorded value, THEN
   THE Gate_Review SHALL reject the change before the change enters the repository, name the guard
   and that Gate, and hold the Gate at `OPEN`.
10. WHILE a Gate is `OPEN`, THE Aerora_Station SHALL leave every Contract_Value the Gate settles
    absent, rather than carrying a default, an example or a placeholder value in its place.

### Requirement 5: Read-only operation is preserved until `DG-ROL` closes

**User Story:** As a flight-safety reviewer, I want every Aerora screen to stay read-only until the
role policy exists, so that no control implies an authorization the station does not perform.

#### Acceptance Criteria

1. WHILE `DG-ROL` is `OPEN`, THE Aerora_Station SHALL present every Aerora screen as read-only,
   carrying zero controls whose activation reaches an outbound command path, a mission upload, start
   or resume path, or a call to `AeroraMissionObserver::verify()`, with the clipboard copy on
   `SCR-HLP-03`, which acts on the Aerora_Station's own clipboard and requires no authority, as the
   single recorded exception to read-only presentation.
2. WHILE `DG-ROL` is `OPEN`, IF a proposed repository change adds a control criterion 1 forbids, adds
   an outbound command path, adds a mission upload, start or resume path, or adds a call to
   `AeroraMissionObserver::verify()`, THEN THE Gate_Review SHALL classify the change as an
   Action_Introducing_Change, reject it, and name `DG-ROL`.
3. WHILE `DG-ROL` is `OPEN`, THE Aerora_Station SHALL report authorization as not enforced on
   `SCR-SHL-03` and `SCR-SEC-01`.
4. WHILE `DG-ROL` is `OPEN`, THE Gate_Review SHALL reject the addition of
   `CustomGuidedActionsController.qml` or `FlyViewAdditionalCustomActionsList.qml` to the override
   allow-list in `AeroraQmlOverrides::overrideEntries()`, SHALL name `DG-ROL` as the Gate the
   operator actions each seam introduces wait on, and SHALL retain both seams as pre-approved and
   undeclared.
5. WHILE `DG-ROL` is `OPEN`, THE Aerora_Station SHALL display a value in the `PROPOSED` class with
   every approval control for that value disabled, accepting no activation and reaching no outbound
   command path.
6. WHILE `DG-ROL` is `OPEN`, THE Aerora_Station SHALL state the absence of an authorized command path
   on each screen that displays a value in the `PROPOSED` class, alongside the disabled approval
   control and without an operator action to reveal the statement.
7. WHEN a Gate other than `DG-ROL` is recorded `CLOSED`, THE Gate_Register SHALL record each screen in
   that Gate's Release_Set as read-only under criterion 1 until `DG-ROL` is recorded `CLOSED`.
8. THE Gate_Register SHALL record `P7-FLY-004` as `FORBIDDEN-UNTIL` `DG-ROL` is recorded `CLOSED`.
9. WHILE `DG-ROL` is `OPEN`, THE Gate_Register SHALL record a screen Released by a Gate other than
   `DG-ROL` as buildable and testable to its full displayed content in the classes of the
   State_Vocabulary, carrying none of the elements criterion 2 names.
10. WHILE `DG-ROL` is `OPEN`, IF a proposed repository change leaves undetermined whether it adds an
    element criterion 2 names, THEN THE Gate_Review SHALL classify the change as an
    Action_Introducing_Change, reject it, and name `DG-ROL`.
11. WHILE `DG-ROL` is `OPEN`, IF a proposed repository change adds a further exception to criterion 1,
    THEN THE Gate_Review SHALL reject the change, name `DG-ROL`, and retain the clipboard copy on
    `SCR-HLP-03` as the single recorded exception.

### Requirement 6: `DG-MIS` — the Aerora mission command set

**User Story:** As the product owner, I want the Aerora mission command set and complex item settled,
so that the nine Mission Studio screens can be built without inventing a command.

#### Acceptance Criteria

1. THE `DG-MIS` Gate_Dossier SHALL record the Decision_Content of `Q-021` and `Q-023` as element
   names rather than as element values: each mission command the Aerora mission set uses with its
   issued MAVLink command identifier or its existing upstream command, the Aerora complex-item type
   name as `complexMissionItemNames()` reports it, the item's parameter set, the asset and
   obstruction geometry the scene editor edits expressed as item parameters, and the plan-file
   representation.
2. THE `DG-MIS` Gate_Dossier SHALL record Product as the owner authorised to sign the Closure_Record
   for the whole of the Gate's Decision_Content, and Tab 5 as the implementing owner.
3. THE `DG-MIS` Gate_Dossier SHALL record as a Sign_Off_Condition that every MAVLink identifier in the
   Closure_Record is traceable to an issued dialect or interface control document.
4. THE `DG-MIS` Gate_Dossier SHALL record as a Sign_Off_Condition that the Closure_Record states, for
   each mission command, whether the station may emit the command.
5. THE `DG-MIS` Gate_Dossier SHALL record as a Sign_Off_Condition that the Closure_Record defines both
   the `mapVisualQML()` and the `geoMapVisualQML()` representation of the Aerora item, because an item
   defining only the former is invisible under the GeoMap engine.
6. WHEN THE `DG-MIS` Closure_Record is accepted, THE Gate_Register SHALL record `SCR-MIS-01`,
   `SCR-MIS-02`, `SCR-MIS-03`, `SCR-MIS-04`, `SCR-MIS-07`, `SCR-MIS-08` and the screen half of
   `SCR-MIS-09` as Released, and SHALL record `SCR-MIS-05` and `SCR-MIS-06`, which are
   Two_Gate_Screens, as Released when THE `DG-PAY` Closure_Record is also accepted.
7. WHEN THE `DG-MIS` Closure_Record is accepted, THE Gate_Register SHALL record each Released mission
   screen as an editor and a display of the settled Decision_Content, and SHALL hold every mission
   upload, start and resume path at `FORBIDDEN-UNTIL` `DG-ROL` is recorded `CLOSED`.
8. WHEN THE `DG-MIS` Closure_Record is accepted, THE Gate_Register SHALL hold every call to
   `AeroraMissionObserver::verify()` at `FORBIDDEN-UNTIL` `DG-ROL` is recorded `CLOSED`.
9. THE `DG-MIS` Gate_Dossier SHALL record as Acceptance_Criteria, excluded from the Gate, the
   plan-file round-trip property that a written Aerora mission item read back produces an equivalent
   item, `P7-CHK-002` for `SCR-MIS-07`, and `P7-MIS-003` for `SCR-MIS-08`.
10. THE `DG-MIS` Gate_Dossier SHALL record the implemented service half of `SCR-MIS-09` as outside the
    Gate's Decision_Content, and the screen half as the part of `SCR-MIS-09` the Gate releases.
11. IF THE `DG-MIS` Closure_Record carries a mission command with neither an issued MAVLink command
    identifier nor a named existing upstream command, THEN THE Gate_Review SHALL hold the Gate at
    `OPEN` and SHALL name that mission command.
12. IF THE `DG-MIS` Closure_Record defines the Aerora complex item with a `mapVisualQML()`
    representation and no `geoMapVisualQML()` representation, THEN THE Gate_Review SHALL hold the Gate
    at `OPEN` and SHALL name `SCR-MIS-04` as the screen the omission leaves undrawn.
13. IF THE `DG-MIS` Closure_Record carries no settled entry for the plan-file representation, THEN THE
    Gate_Review SHALL hold the Gate at `OPEN` and SHALL name the plan-file representation together
    with the round-trip Acceptance_Criterion of criterion 9 that the omission leaves unverifiable.
14. WHILE `DG-MIS` holds no accepted Closure_Record, IF a proposed repository change introduces a
    mission command identifier, a complex-item type name, an item parameter or a plan-file element,
    THEN THE Gate_Review SHALL reject the change before the change enters the repository and SHALL
    name `DG-MIS`.
15. IF a proposed repository change adds a mission upload, start or resume path, or a call to
    `AeroraMissionObserver::verify()`, to a screen the `DG-MIS` Closure_Record releases, THEN THE
    Gate_Review SHALL classify the change as an Action_Introducing_Change, reject it, and name
    `DG-ROL`.
16. IF THE `DG-MIS` Gate_Dossier records a condition of criterion 9 or the service half of
    `SCR-MIS-09` as Decision_Content, THEN THE Gate_Review SHALL remove the condition from the Gate's
    Decision_Content and SHALL record the condition as an Acceptance_Criterion excluded from the Gate.
17. THE `DG-MIS` Gate_Dossier SHALL record the static MAVLink-identifier check as the guard that holds
    the mission command part of the Decision_Content visible, and SHALL record
    `kGcsPayloadStateReplayImplemented` at false as a guard that holds at false after the Gate is
    recorded `CLOSED`.
18. WHEN THE `DG-MIS` Closure_Record is submitted, THE Gate_Review SHALL record, for each MAVLink
    identifier the Closure_Record carries, the issued dialect or interface control document the
    identifier is traced to.

### Requirement 7: `DG-TLM` — the Aerora telemetry dialect and per-vehicle fact-group attachment

**User Story:** As the firmware lead, I want the Aerora dialect and the fact-group attachment settled,
so that an Aerora value can reach a panel, the value picker and the telemetry record.

#### Acceptance Criteria

1. THE `DG-TLM` Gate_Dossier SHALL record the dialect part of the Decision_Content as element names
   rather than as element values: for each Aerora fact, the fact name, the unit, the value range, the
   enumeration members, the owning fact group among the six the telemetry interface control document
   names, the freshness class, and the inbound message that populates the fact.
2. THE `DG-TLM` Gate_Dossier SHALL record the attachment part of the Decision_Content: the
   authorisation of the `ADR-016` core edit, comprising the
   `friend class AeroraVehicleFactGroupRegistrar` declaration in `src/Vehicle/Vehicle.h` and the
   `AERORA_VEHICLE_FACTGROUP_FRIENDSHIP` compile definition.
3. THE `DG-TLM` Gate_Dossier SHALL record Firmware and Product as the owners authorised to sign the
   dialect part, Tab 1 as the owner authorised to sign the `ADR-016` core edit, and Tab 4 as the
   implementing owner.
4. THE `DG-TLM` Gate_Dossier SHALL record `Q4-002` as `DECIDED` at Option B and `ADR-016`, so that the
   Gate's open part is the dialect content and the core-edit authorisation rather than the attachment
   design.
5. THE `DG-TLM` Gate_Dossier SHALL record as a Sign_Off_Condition that every fact in the Closure_Record
   carries all seven attributes of criterion 1.
6. THE `DG-TLM` Gate_Dossier SHALL record as a Sign_Off_Condition that the Closure_Record creates every
   Aerora fact group during vehicle construction, so that the telemetry record's column header stays
   stable per `R-027` and `AERORA-GCS-REQ-022`.
7. THE `DG-TLM` Gate_Dossier SHALL record as a Sign_Off_Condition that the Closure_Record requires two
   vehicles of the Aerora firmware class to hold distinct fact-group instances, which is the `R4-001`
   regression property.
8. WHEN both the dialect part and the `ADR-016` core-edit part of THE `DG-TLM` Closure_Record are
   accepted, THE Gate_Register SHALL record `SCR-TEL-01` and `SCR-LIV-02` as Released.
9. WHILE `DG-TLM` is `OPEN`, THE Aerora_Station SHALL display every Aerora-reported value on a released
   screen in the `UNKNOWN` class.
10. THE `DG-TLM` Gate_Dossier SHALL record that a screen released by another Gate is buildable and
    testable against the State_Vocabulary while `DG-TLM` is `OPEN`.
11. THE `DG-TLM` Gate_Dossier SHALL record `R4-001` as an upstream defect that stays open after this
    Gate closes.
12. IF THE `DG-TLM` Closure_Record carries a fact that omits one of the seven attributes of
    criterion 1, THEN THE Gate_Review SHALL hold the dialect part at `OPEN` and SHALL name the fact
    together with the omitted attribute.
13. IF THE `DG-TLM` Closure_Record assigns a fact to a fact group other than the six the telemetry
    interface control document names, THEN THE Gate_Review SHALL hold the dialect part at `OPEN` and
    SHALL name the fact together with the unrecognised fact group.
14. IF THE `DG-TLM` Closure_Record creates an Aerora fact group after vehicle construction, THEN THE
    Gate_Review SHALL hold the dialect part at `OPEN` and SHALL name `R-027` and `AERORA-GCS-REQ-022`
    as the column-header stability the creation breaks.
15. IF the dialect part of THE `DG-TLM` Closure_Record is accepted while the `ADR-016` core-edit part
    carries no signature from Tab 1, THEN THE Gate_Register SHALL record `DG-TLM` as `OPEN`, record
    `SCR-TEL-01` and `SCR-LIV-02` as not Released, and name the core-edit part.
16. WHILE the dialect part of `DG-TLM` holds no accepted Closure_Record, IF a proposed repository
    change introduces a fact name, a unit, a value range, an enumeration member, a freshness class
    assignment or an inbound message identifier for an Aerora fact, THEN THE Gate_Review SHALL reject
    the change before the change enters the repository and SHALL name `DG-TLM`.
17. WHILE the `ADR-016` core-edit part of `DG-TLM` holds no accepted Closure_Record, IF a proposed
    repository change adds the `friend class AeroraVehicleFactGroupRegistrar` declaration to
    `src/Vehicle/Vehicle.h` or defines `AERORA_VEHICLE_FACTGROUP_FRIENDSHIP`, THEN THE Gate_Review
    SHALL reject the change before the change enters the repository and SHALL name Tab 1 as the owner
    whose signature the core edit requires.
18. THE `DG-TLM` Gate_Dossier SHALL record `AeroraVehicleFactGroupRegistrar::isRegistrationAvailable()`
    at false as the guard that holds the Gate visible, and SHALL record the acceptance of both parts as
    the condition under which Requirement 4 permits that guard to change value.

### Requirement 8: `DG-FRS` — the freshness budget values

**User Story:** As the product owner, I want a freshness threshold per freshness class, so that the
station reaches a staleness verdict instead of reporting an age with no verdict.

#### Acceptance Criteria

1. THE `DG-FRS` Gate_Dossier SHALL record the Decision_Content of `Q-036` as element names rather than
   as element values: for each freshness class, a threshold value, the unit of the threshold, and the
   source the value is drawn from.
2. THE `DG-FRS` Gate_Dossier SHALL record Product as the owner authorised to sign the Closure_Record,
   and Tab 4 as the implementing owner.
3. THE `DG-FRS` Gate_Dossier SHALL record as a Sign_Off_Condition that each freshness class the
   Closure_Record names carries exactly one threshold.
4. THE `DG-FRS` Gate_Dossier SHALL record as a Sign_Off_Condition that the Closure_Record preserves the
   latched `STALE` verdict and the three-outcome evaluation, so that an unevaluated class resolves to
   `ObservedFreshnessUnevaluated` rather than to a fresh verdict.
5. WHERE `DG-TLM` names a freshness class absent from an accepted `DG-FRS` Closure_Record, THE
   Gate_Register SHALL record an amendment to the `DG-FRS` Closure_Record for that class and SHALL
   retain the accepted status of the classes already signed.
6. WHEN THE `DG-FRS` Closure_Record is accepted, THE Gate_Register SHALL record `SCR-TEL-03` and
   `SCR-SET-02` as Released.
7. WHILE `DG-FRS` is `OPEN`, THE Aerora_Station SHALL resolve every freshness evaluation to
   `ObservedFreshnessUnevaluated`.
8. WHILE `DG-FRS` is `OPEN`, THE Aerora_Station SHALL display the observed age of each Aerora-reported
   value without a staleness verdict beside the age.
9. THE `DG-FRS` Gate_Dossier SHALL record that the `STALE` class of the State_Vocabulary is unreachable
   while the Gate is `OPEN`.
10. IF THE `DG-FRS` Closure_Record carries a threshold with no unit or with no named source, THEN THE
    Gate_Review SHALL hold the Gate at `OPEN` and SHALL name the freshness class together with the
    omitted unit or source.
11. IF THE `DG-FRS` Closure_Record carries more than one threshold for one freshness class, THEN THE
    Gate_Review SHALL hold the Gate at `OPEN` and SHALL name that freshness class together with each
    threshold recorded against the class.
12. IF THE `DG-FRS` Closure_Record resolves an unevaluated freshness class to a verdict other than
    `ObservedFreshnessUnevaluated`, THEN THE Gate_Review SHALL hold the Gate at `OPEN` and SHALL name
    `ObservedFreshnessUnevaluated` as the outcome criterion 4 requires.
13. WHILE `DG-FRS` holds no accepted Closure_Record for a freshness class, IF a proposed repository
    change introduces a threshold value for that freshness class, THEN THE Gate_Review SHALL reject
    the change before the change enters the repository and SHALL name `DG-FRS` together with the
    freshness class.
14. WHERE a freshness class is settled by an amendment under criterion 5, THE Gate_Register SHALL
    record that amendment's signatures and sign-off date against that freshness class alone, and SHALL
    retain the recorded acceptance of each freshness class signed earlier.
15. THE `DG-FRS` Gate_Dossier SHALL record the three-outcome freshness evaluation that resolves an
    unevaluated class to `ObservedFreshnessUnevaluated` as the guard that holds the Gate visible, and
    SHALL record each guard of Requirement 4 criterion 5 as outside the Gate.
16. WHILE `DG-FRS` is `OPEN`, IF a proposed repository change alters the freshness evaluation so that
    an unevaluated freshness class resolves to a verdict other than `ObservedFreshnessUnevaluated`,
    THEN THE Gate_Review SHALL reject the change before the change enters the repository and SHALL
    hold the Gate at `OPEN`.

### Requirement 9: `DG-ROL` — the operator role set and authorization scope

**User Story:** As the operations lead, I want the role set and the authorization scope settled, so
that the station can enforce a command boundary and an operator control can exist.

#### Acceptance Criteria

1. THE `DG-ROL` Gate_Dossier SHALL record the `Q-029` part of the Decision_Content as element names
   rather than as element values: the role names, and for each role, the command classes the role may
   authorise.
2. THE `DG-ROL` Gate_Dossier SHALL record the `ADR-014` part of the Decision_Content: the selected
   outbound authorization scope among the recorded options A, B and C, and the point at which the
   station evaluates an authorization.
3. THE `DG-ROL` Gate_Dossier SHALL record Product and operations as the owners authorised to sign the
   `Q-029` part, Architecture as the owner authorised to sign the `ADR-014` part, and Tab 2 as the
   implementing owner.
4. THE `DG-ROL` Gate_Dossier SHALL record as a Sign_Off_Condition that each role in the Closure_Record
   maps to at least one command class.
5. THE `DG-ROL` Gate_Dossier SHALL record as a Sign_Off_Condition that each command class the station
   can emit maps to at least one role, so that `AeroraCorePlugin::authorizeMavCommand()` reaches a
   verdict for every command the station receives.
6. THE `DG-ROL` Gate_Dossier SHALL record as a Sign_Off_Condition that the Closure_Record states
   whether an unauditable allow is denied, which is `P7-AUDIT-003`.
7. WHEN both parts of THE `DG-ROL` Closure_Record are accepted, THE Gate_Register SHALL record
   `SCR-SEC-02` and the calibration display of `SCR-VEH-05` as Released, and SHALL record the approval
   half of `SCR-LIV-05`, which is a Two_Gate_Screen, as Released with the screen recorded as Released
   when THE `DG-AUT` Closure_Record is also accepted.
8. WHEN both parts of THE `DG-ROL` Closure_Record are accepted, THE Gate_Register SHALL record the
   precondition for an operator action control as met, and SHALL record each control added thereafter
   as subject to the authorization scope the `ADR-014` part settles.
9. WHILE `DG-ROL` is `OPEN`, THE Aerora_Station SHALL report `isAuthorizationEnforced()` as false.
10. THE `DG-ROL` Gate_Dossier SHALL record as Acceptance_Criteria, excluded from the Gate, the
    implemented `ADR-015` calibration authorization seam of `SCR-VEH-05`, and `P7-SEC-005`, which
    `SCR-SEC-01` already delivers.
11. IF THE `DG-ROL` Closure_Record carries a role that maps to no command class, or leaves a command
    class the station can emit mapped to no role, THEN THE Gate_Review SHALL hold the `Q-029` part at
    `OPEN` and SHALL name the unmapped role or command class.
12. IF THE `DG-ROL` Closure_Record carries no settled entry for the outbound authorization scope, or
    names a scope other than the options the `ADR-014` record holds, THEN THE Gate_Review SHALL hold
    the `ADR-014` part at `OPEN` and SHALL name the recorded options A, B and C.
13. IF THE `DG-ROL` Closure_Record leaves undetermined whether an unauditable allow is denied, THEN
    THE Gate_Review SHALL hold the `ADR-014` part at `OPEN` and SHALL name `P7-AUDIT-003`.
14. IF one part of THE `DG-ROL` Closure_Record is accepted while the other part is not accepted, THEN
    THE Gate_Register SHALL record `DG-ROL` as `OPEN`, record each screen of the `DG-ROL` Release_Set
    as not Released, and name the part that is not accepted.
15. WHILE the `Q-029` part of `DG-ROL` holds no accepted Closure_Record, IF a proposed repository
    change introduces a role name or a permission, THEN THE Gate_Review SHALL reject the change before
    the change enters the repository and SHALL name `DG-ROL`.
16. THE `DG-ROL` Gate_Dossier SHALL record `AeroraCorePlugin::isAuthorizationEnforced()` at false as
    the guard that holds the Gate visible, SHALL record `P7-FLY-004` at `FORBIDDEN-UNTIL`, and SHALL
    record both pre-approved seams of Requirement 5 criterion 4 as waiting on the Gate.
17. WHEN both parts of THE `DG-ROL` Closure_Record are accepted, THE Gate_Review SHALL record whether
    the acceptance permits `AeroraCorePlugin::isAuthorizationEnforced()`,
    `kGroundStationActuatorPathImplemented` and `kAutonomousPayloadActionImplemented` to change value.
18. WHEN `DG-ROL` is recorded `CLOSED`, THE Gate_Register SHALL record the read-only presentation of
    Requirement 5 criterion 1 as lifted for each screen Released by another Gate, and SHALL leave the
    delivery status of each of those screens unchanged.

### Requirement 10: `DG-PAY` — the payload and tether reported-value sets

**User Story:** As the product owner, I want the reported payload, tether and consumable items
settled, so that the supervision panels display reported values rather than station-derived guesses.

#### Acceptance Criteria

1. THE `DG-PAY` Gate_Dossier SHALL record the payload part of the Decision_Content from the payload
   interface control document, as element names rather than as element values: for each reported
   payload item, the item name, the unit, the value range, the enumeration members, and the reporting
   message.
2. THE `DG-PAY` Gate_Dossier SHALL record the power part of the Decision_Content of `P7-PWR-006`: for
   each reported tether item, the item name, the unit, the value range, the enumeration members, and
   the reporting message.
3. THE `DG-PAY` Gate_Dossier SHALL record the capability part of the Decision_Content of `Q3-009`: the
   form of a capability identifier, and the means by which a vehicle reports capability presence
   separately from capability state.
4. THE `DG-PAY` Gate_Dossier SHALL record the consumable part of the Decision_Content for `SCR-MIS-06`:
   the reported consumable item set, and the approved consumption model that `P7-ARCH-005` requires
   before any consumption projection is displayed.
5. THE `DG-PAY` Gate_Dossier SHALL record Product and engineering as the owners authorised to sign the
   payload, tether and consumable parts, Firmware as the owner authorised to sign the `Q3-009` part,
   and Tab 6 as the implementing owner.
6. THE `DG-PAY` Gate_Dossier SHALL record as a Sign_Off_Condition that every item in the Closure_Record
   is a vehicle-reported item.
7. IF THE `DG-PAY` Closure_Record contains a station-derived projection without an approved method,
   THEN THE Gate_Review SHALL hold the part that carries the projection at `OPEN` and name
   `P7-ARCH-005`.
8. WHEN every part of THE `DG-PAY` Closure_Record is accepted, THE Gate_Register SHALL record
   `SCR-LIV-03` and `SCR-LIV-04` as Released, and SHALL record `SCR-MIS-05` and `SCR-MIS-06`, which
   are Two_Gate_Screens, as Released when THE `DG-MIS` Closure_Record is also accepted.
9. WHEN every part of THE `DG-PAY` Closure_Record is accepted, THE Gate_Register SHALL record the
   per-vehicle half of `SCR-SHL-02` as Released, and SHALL record the per-key row of `SCR-VEH-03` as
   content a Delivered_Screen displays rather than as an entry in the `DG-PAY` Release_Set.
10. WHILE `DG-PAY` is `OPEN`, THE Aerora_Station SHALL report `controlsPermitted` as false, which
    `IF-PAY-002` requires.
11. THE `DG-PAY` Gate_Dossier SHALL record as Acceptance_Criteria, excluded from the Gate,
    `P7-PAY-004`'s rule that painting supervision displays reported values alone, and `SCR-VEH-03`'s
    empty-inventory reading, which the delivered screen already carries.
12. IF THE `DG-PAY` Closure_Record carries a reported item that omits the item name, the unit, the
    value range, the enumeration members or the reporting message, THEN THE Gate_Review SHALL hold the
    part that carries the item at `OPEN` and SHALL name the item together with the omitted element.
13. IF THE `DG-PAY` Closure_Record carries a capability identifier whose form provides no means of
    reporting capability presence separately from capability state, THEN THE Gate_Review SHALL hold the
    `Q3-009` part at `OPEN` and SHALL name the per-vehicle half of `SCR-SHL-02` together with the
    per-key row of `SCR-VEH-03` as the content the part supplies.
14. IF THE `DG-PAY` Closure_Record carries a reported consumable item set with no approved consumption
    model, THEN THE Gate_Review SHALL hold the consumable part at `OPEN`, name `P7-ARCH-005`, and name
    `SCR-MIS-06` as the screen the part releases.
15. WHEN one part of THE `DG-PAY` Closure_Record is accepted, THE Gate_Register SHALL record that
    part's settled elements as accepted, SHALL record each screen whose content an unaccepted part
    supplies as not Released, and SHALL name each part that is not accepted.
16. WHILE a part of `DG-PAY` holds no accepted Closure_Record, IF a proposed repository change
    introduces a payload item, a tether item, a consumable item or a capability identifier that part
    settles, THEN THE Gate_Review SHALL reject the change before the change enters the repository and
    SHALL name `DG-PAY` together with the part.
17. THE `DG-PAY` Gate_Dossier SHALL record `AeroraPowerModel::kDefinedThresholdCount` at zero,
    `kAutomaticSourceTransitionImplemented` at false and `kRemainingEnergyEstimationImplemented` at
    false as the guards that hold the power part visible, and `kGroundStationActuatorPathImplemented`
    and `kAutonomousPayloadActionImplemented` at false as the guards that hold the payload command
    path absent until `DG-ROL` is recorded `CLOSED`.
18. WHILE `DG-PAY` or `DG-ROL` is `OPEN`, IF a proposed repository change implements
    `AeroraPayloadCommandSink` or alters a guard recorded under criterion 17, THEN THE Gate_Review
    SHALL reject the change before the change enters the repository, name each of those two Gates that
    is `OPEN`, and hold each named Gate at `OPEN`.
19. IF THE `DG-PAY` Closure_Record is accepted while `DG-MIS` holds no accepted Closure_Record, THEN
    THE Gate_Register SHALL record `SCR-MIS-05` and `SCR-MIS-06` as not Released and SHALL name
    `DG-MIS`.
20. THE `DG-PAY` Gate_Dossier SHALL record, for each part of the Decision_Content, the screens and the
    screen parts whose content that part supplies: the payload part for `SCR-LIV-03`, the power part
    for `SCR-LIV-04`, the consumable part for `SCR-MIS-06`, and the `Q3-009` part for the per-vehicle
    half of `SCR-SHL-02` and the per-key row of `SCR-VEH-03`.

### Requirement 11: `DG-AUT` — the autonomy proposal model and source interface

**User Story:** As the architecture owner, I want the proposal model and the autonomy source interface
settled, so that a proposal can be displayed with its basis and its lifecycle.

#### Acceptance Criteria

1. THE `DG-AUT` Gate_Dossier SHALL record the proposal-model part of the Decision_Content of
   `P7-AI-001` as element names rather than as element values: the proposal's source, timestamp,
   subject and basis fields, the lifecycle states, the permitted transitions between lifecycle states,
   and the expiry rule.
2. THE `DG-AUT` Gate_Dossier SHALL record the source-interface part of the Decision_Content of
   `P7-AI-004`: the autonomy source transport, the request and response schema, and the station's
   behaviour on a response the station cannot evaluate.
3. THE `DG-AUT` Gate_Dossier SHALL record Architecture as the owner authorised to sign the proposal
   model, Architecture together with the external interface authority as the owners authorised to sign
   the source interface, and Tab 6 as the implementing owner.
4. THE `DG-AUT` Gate_Dossier SHALL record as a Sign_Off_Condition that the Closure_Record cites an
   issued GPU-to-flight-controller interface control document for every schema element.
5. THE `DG-AUT` Gate_Dossier SHALL record as a Sign_Off_Condition that each lifecycle state in the
   Closure_Record maps to a class of the State_Vocabulary.
6. THE `DG-AUT` Gate_Dossier SHALL record as a Sign_Off_Condition that the Closure_Record defines the
   display of a proposal the station cannot evaluate, which is `P7-AI-005`.
7. WHEN both parts of THE `DG-AUT` Closure_Record are accepted, THE Gate_Register SHALL record
   `SCR-AIS-01` and `SCR-AIS-02` as Released, and SHALL record the proposal half of `SCR-LIV-05`,
   which is a Two_Gate_Screen, as Released with the screen recorded as Released when THE `DG-ROL`
   Closure_Record is also accepted.
8. WHEN both THE `DG-AUT` and THE `DG-STO` Closure_Records are accepted, THE Gate_Register SHALL record
   `SCR-AIS-03` as Released.
9. WHILE `DG-AUT` is `OPEN`, THE Aerora_Station SHALL report the autonomy source status as `UNKNOWN`.
10. THE `DG-AUT` Gate_Dossier SHALL record as Acceptance_Criteria, excluded from the Gate, `P7-AI-002`'s
    rule that a proposal becomes a command only on an explicit operator approval event, and
    `P7-AI-003`'s rule that the approval surface is disabled and states why while no authorized command
    path exists.
11. IF THE `DG-AUT` Closure_Record carries a lifecycle state that maps to no class of the
    State_Vocabulary, THEN THE Gate_Review SHALL hold the proposal-model part at `OPEN` and SHALL name
    that lifecycle state.
12. IF THE `DG-AUT` Closure_Record carries a permitted transition whose source or target is a lifecycle
    state the record does not define, or carries no settled entry for the expiry rule, THEN THE
    Gate_Review SHALL hold the proposal-model part at `OPEN` and SHALL name the undefined lifecycle
    state or the expiry rule.
13. IF THE `DG-AUT` Closure_Record carries a schema element with no citation to an issued
    GPU-to-flight-controller interface control document, THEN THE Gate_Review SHALL hold the
    source-interface part at `OPEN` and SHALL name the uncited schema element.
14. IF THE `DG-AUT` Closure_Record leaves undefined the station's behaviour on a response the station
    cannot evaluate, THEN THE Gate_Review SHALL hold the source-interface part at `OPEN` and SHALL name
    `P7-AI-005`.
15. IF one part of THE `DG-AUT` Closure_Record is accepted while the other part is not accepted, THEN
    THE Gate_Register SHALL record `DG-AUT` as `OPEN`, record `SCR-AIS-01`, `SCR-AIS-02`, `SCR-AIS-03`
    and the proposal half of `SCR-LIV-05` as not Released, and name the part that is not accepted.
16. IF THE `DG-AUT` Closure_Record is accepted while `DG-STO` holds no accepted Closure_Record, THEN
    THE Gate_Register SHALL record `SCR-AIS-03` as not Released and SHALL name `DG-STO`.
17. WHILE `DG-AUT` holds no accepted Closure_Record, IF a proposed repository change introduces a
    proposal schema element, a lifecycle state, a transition or an autonomy source transport, THEN THE
    Gate_Review SHALL reject the change before the change enters the repository and SHALL name
    `DG-AUT`.
18. WHEN both parts of THE `DG-AUT` Closure_Record are accepted, THE Gate_Register SHALL record every
    approval control on a screen the Gate releases as disabled until `DG-ROL` is recorded `CLOSED`, and
    SHALL record `P7-AI-002`'s explicit operator approval event as reachable only thereafter.
19. THE `DG-AUT` Gate_Dossier SHALL record the `UNKNOWN` autonomy source status of criterion 9 as the
    guard that holds the Gate visible, and SHALL record each guard of Requirement 4 criterion 5 as
    outside the Gate.

### Requirement 12: `DG-STO` — the durable, tamper-evident local store

**User Story:** As the security lead, I want the tamper-evidence mechanism and its protected anchor
settled, so that an audit record survives and can be shown to be unaltered.

#### Acceptance Criteria

1. THE `DG-STO` Gate_Dossier SHALL record the tamper-evidence part of the Decision_Content of `Q-037`
   and `Q4-003` as element names rather than as element values: the audit chain construction, the
   protected chain anchor of `D-DAT-05`, and the `AeroraPlatformKeyProtector` backend for each target
   platform.
2. THE `DG-STO` Gate_Dossier SHALL record the retention part of the Decision_Content: the retention
   caps of `D-DAT-01`, whether an audit record may be deleted at all per `D-DAT-02`, and the event
   categories of `D-DAT-04`.
3. THE `DG-STO` Gate_Dossier SHALL record Tab 2 together with security architecture as the owners
   authorised to sign the tamper-evidence part, Product as the owner authorised to sign the retention
   part, and Tab 1 as the implementing owner of the store.
4. THE `DG-STO` Gate_Dossier SHALL record `P7-DAT-002` as `CLOSED` at `DESIGNED`, so that the Gate's
   open part is tamper-evidence, the anchor and retention rather than the persistence design.
5. THE `DG-STO` Gate_Dossier SHALL record as a Sign_Off_Condition that the Closure_Record names an
   anchor backend for each of the five target platforms, or records an explicit exception for a platform
   with no backend.
6. THE `DG-STO` Gate_Dossier SHALL record as a Sign_Off_Condition that the Closure_Record defines the
   write-failure accounting behaviour, so that a lost audit record is counted rather than silently
   dropped.
7. WHEN every part of THE `DG-STO` Closure_Record is accepted, THE Gate_Register SHALL record
   `SCR-SEC-03`, `SCR-LOG-01`, `SCR-LOG-02` and `SCR-TEL-02` as Released.
8. WHEN every part of THE `DG-STO` Closure_Record is accepted, THE Gate_Register SHALL record the
   platform-backend dependency of `SCR-SEC-04`, which the tamper-evidence part settles, as resolved,
   and SHALL record `SCR-SEC-04` as Released.
9. WHILE `DG-STO` is `OPEN`, THE Aerora_Station SHALL report the durability and the tamper-evidence of
   the audit sink as false.
10. WHILE `DG-STO` is `OPEN`, THE Aerora_Station SHALL count each audit record the station fails to
    write as lost.
11. THE `DG-STO` Gate_Dossier SHALL record as Acceptance_Criteria, excluded from the Gate, the
    implementation of `AeroraLocalStore` against the persistence design, `P7-TEL-004`'s event model for
    `SCR-TEL-02`, and `P7-LOG-003`'s export provenance for `SCR-LOG-02`.
12. IF THE `DG-STO` Closure_Record names no anchor backend for a target platform and records no
    explicit exception for that platform, THEN THE Gate_Review SHALL hold the tamper-evidence part at
    `OPEN` and SHALL name that target platform.
13. WHERE THE `DG-STO` Closure_Record records an explicit exception for a target platform, THE
    Aerora_Station SHALL report the tamper-evidence of the audit sink as absent on that platform.
14. IF THE `DG-STO` Closure_Record carries no settled entry for the protected chain anchor of
    `D-DAT-05`, THEN THE Gate_Review SHALL hold the tamper-evidence part at `OPEN` and SHALL name the
    anchor together with the audit chain construction the anchor protects.
15. IF THE `DG-STO` Closure_Record leaves undetermined whether an audit record may be deleted, THEN THE
    Gate_Review SHALL hold the retention part at `OPEN` and SHALL name `D-DAT-02`.
16. IF THE `DG-STO` Closure_Record leaves the write-failure accounting behaviour undefined, THEN THE
    Gate_Review SHALL hold the tamper-evidence part at `OPEN` and SHALL name the write-failure
    accounting behaviour together with the lost-record count of criterion 10.
17. IF one part of THE `DG-STO` Closure_Record is accepted while the other part is not accepted, THEN
    THE Gate_Register SHALL record `DG-STO` as `OPEN`, record each screen named in criterion 7 as not
    Released, and name the part that is not accepted.
18. IF THE `DG-STO` Closure_Record is accepted while `DG-AUT` holds no accepted Closure_Record, THEN
    THE Gate_Register SHALL record `SCR-AIS-03` as not Released and SHALL name `DG-AUT`.
19. WHILE a part of `DG-STO` holds no accepted Closure_Record, IF a proposed repository change
    introduces an audit event category, a retention cap or an anchor backend that part settles, THEN
    THE Gate_Review SHALL reject the change before the change enters the repository and SHALL name
    `DG-STO` together with the part.
20. THE `DG-STO` Gate_Dossier SHALL record the audit sink's reported durability and tamper-evidence of
    criterion 9, together with the lost-record count of criterion 10, as the guards that hold the Gate
    visible, and SHALL record each guard of Requirement 4 criterion 5 as outside the Gate.

### Requirement 13: `DG-FLT` — the fleet identity format

**User Story:** As the product owner, I want the fleet identifier format settled, so that a fleet
screen can identify a vehicle across sessions without substituting the MAVLink system id.

#### Acceptance Criteria

1. THE `DG-FLT` Gate_Dossier SHALL record the Decision_Content of the fleet identity format and
   `D-DAT-03` as element names rather than as element values: the identifier's form, the authority
   that issues an identifier, the scope within which an identifier is unique, and the identifier's
   lifetime.
2. THE `DG-FLT` Gate_Dossier SHALL record Product as the owner authorised to sign the Closure_Record,
   Tab 3 as the implementing owner of the profile model, and Tab 7 as the implementing owner of the
   profile-keyed UI.
3. THE `DG-FLT` Gate_Dossier SHALL record as a Sign_Off_Condition that the Closure_Record states the
   display for a vehicle observed before an identifier is issued, so that `SCR-FLT-01` can render a
   first contact.
4. THE `DG-FLT` Gate_Dossier SHALL record as a Sign_Off_Condition that the Closure_Record derives the
   identifier from a source other than the MAVLink system id.
5. THE `DG-FLT` Gate_Dossier SHALL record `P7-FLEET-004` as `DESIGNED`, resolving the `CONSTANT`
   registry constraint as static registration of the full page set with runtime content resolution, so
   that the Gate's open part is the identifier format alone.
6. WHEN THE `DG-FLT` Closure_Record is accepted, THE Gate_Register SHALL record `SCR-FLT-01`,
   `SCR-FLT-02` and `SCR-FLT-03` as Released.
7. WHILE `DG-FLT` is `OPEN`, THE Aerora_Station SHALL display the fleet identifier field of
   `SCR-VEH-02` in the `UNKNOWN` class.
8. THE `DG-FLT` Gate_Dossier SHALL record as Acceptance_Criteria, excluded from the Gate, the static
   registration of the Aerora page set, the runtime content resolution of each page, and
   `P7-FLEET-002`'s rule that a profile grants no capability.
9. IF THE `DG-FLT` Closure_Record derives the fleet identifier from the MAVLink system id, THEN THE
   Gate_Review SHALL hold the Gate at `OPEN` and SHALL name the MAVLink system id as the source
   criterion 4 excludes.
10. IF THE `DG-FLT` Closure_Record carries no settled entry for the issuing authority, the uniqueness
    scope or the identifier's lifetime, THEN THE Gate_Review SHALL hold the Gate at `OPEN` and SHALL
    name the omitted element.
11. IF THE `DG-FLT` Closure_Record leaves undefined the display for a vehicle observed before an
    identifier is issued, THEN THE Gate_Review SHALL hold the Gate at `OPEN` and SHALL name
    `SCR-FLT-01` as the screen that renders a first contact.
12. WHILE `DG-FLT` holds no accepted Closure_Record, IF a proposed repository change introduces a
    fleet identifier format, THEN THE Gate_Review SHALL reject the change before the change enters the
    repository and SHALL name `DG-FLT`.
13. WHILE `DG-FLT` is `OPEN`, THE Aerora_Station SHALL withhold the fleet identifier on `SCR-VEH-02`,
    which is a Delivered_Screen, rather than substituting the MAVLink system id in the field.
14. WHEN THE `DG-FLT` Closure_Record is accepted, THE Gate_Register SHALL record the fleet identifier
    field of `SCR-VEH-02` as content a Delivered_Screen displays, and SHALL record `SCR-VEH-02` as
    outside the `DG-FLT` Release_Set.
15. IF THE `DG-FLT` Gate_Dossier records an Acceptance_Criterion of criterion 8 as Decision_Content,
    THEN THE Gate_Review SHALL remove the Acceptance_Criterion from the Gate's Decision_Content and
    SHALL record the Gate's open part as the identifier format alone.
16. THE `DG-FLT` Gate_Dossier SHALL record the `UNKNOWN` fleet identifier field of criterion 7 as the
    guard that holds the Gate visible, and SHALL record each guard of Requirement 4 criterion 5 as
    outside the Gate.

### Requirement 14: `DG-VID` — the fullscreen video fork point

**User Story:** As the UI owner, I want the fork point and owner process for `FlyViewVideo.qml`
recorded, so that the Aerora overlay can reach the video surface in fullscreen with a tracked fork.

#### Acceptance Criteria

1. THE `DG-VID` Gate_Dossier SHALL record the Decision_Content of `Q-038` and `IF-UIS-012` as element
   names rather than as element values: the `Tier::PopulatedFile` override target
   `src/FlyView/FlyViewVideo.qml`, the upstream commit the replacement is authored against, the named
   fork owner, the drift-review cadence, and the recorded fork reason.
2. THE `DG-VID` Gate_Dossier SHALL record the process owner as the party authorised to sign the fork
   point, Tab 1 as the approver of a class-X fork, and Tab 7 as the implementing owner.
3. THE `DG-VID` Gate_Dossier SHALL record the fork reason: both precedents, `ProximityRadarVideoView`
   and `ObstacleDistanceOverlayVideo`, are instantiated inside the populated upstream file, which
   offers no child seam.
4. THE `DG-VID` Gate_Dossier SHALL record fullscreen video as the single uncovered case, because
   `FlyView.qml` binds the widget layer, the Aerora custom layer and the bottom-right column to
   `!QGroundControl.videoManager.fullScreen`.
5. THE `DG-VID` Gate_Dossier SHALL record as a Sign_Off_Condition that the Closure_Record raises the
   recorded class-X fork target from zero to one, with the drift review scheduled and the fork listed
   in the seam budget.
6. THE `DG-VID` Gate_Dossier SHALL record as a Sign_Off_Condition that the Closure_Record records the
   alternatives considered and the reason each was declined, which `P7-ARCH-023` requires.
7. WHEN THE `DG-VID` Closure_Record is accepted, THE Gate_Register SHALL record `SCR-VID-01` as
   Released.
8. WHILE `DG-VID` is `OPEN`, THE Aerora_Station SHALL draw the `SCR-LIV-01` overlay over the video
   surface whenever the widget layer is visible.
9. THE `DG-VID` Gate_Dossier SHALL record as Acceptance_Criteria, excluded from the Gate, `P7-VID-004`'s
   rule that a video overlay displays a safety value with its state class, and the decision to leave
   `createVideoReceiver`, `createVideoSink` and `releaseVideoSink` un-overridden.
10. IF THE `DG-VID` Closure_Record carries no settled entry for the upstream commit the replacement is
    authored against, the named fork owner or the drift-review cadence, THEN THE Gate_Review SHALL
    hold the Gate at `OPEN` and SHALL name the omitted element.
11. IF THE `DG-VID` Closure_Record carries no approval from Tab 1, THEN THE Gate_Review SHALL hold the
    Gate at `OPEN` and SHALL name Tab 1 as the approver a class-X fork requires.
12. IF THE `DG-VID` Closure_Record records no alternative considered and no reason for declining each
    alternative, THEN THE Gate_Review SHALL hold the Gate at `OPEN` and SHALL name `P7-ARCH-023`.
13. WHILE `DG-VID` holds no accepted Closure_Record, IF a proposed repository change adds a
    `Tier::PopulatedFile` entry to `AeroraQmlOverrides::overrideEntries()`, THEN THE Gate_Review SHALL
    reject the change before the change enters the repository and SHALL name `DG-VID`.
14. WHEN THE `DG-VID` Closure_Record is accepted, THE Gate_Register SHALL record the class-X fork count
    as one, record the scheduled drift review against the named fork owner, and record the fork in the
    seam budget.
15. THE `DG-VID` Gate_Dossier SHALL record `SCR-VID-01` as retaining one entry classified as a Gate,
    which is the absent video overlay seam of criterion 3, and SHALL record `P7-VID-004` as the
    Acceptance_Criterion the screen satisfies once built.
16. THE `DG-VID` Gate_Dossier SHALL record `AeroraQmlOverrides::populatedFileOverrideCount()` at zero
    as the guard that holds the Gate visible, and SHALL record the acceptance of THE `DG-VID`
    Closure_Record as the condition under which Requirement 4 permits that guard to change value.

### Requirement 15: `DG-SIM` — the simulated-source discriminator

**User Story:** As the test owner, I want the simulated-source flag placed on the reported value and a
discriminator that exists in a release build, so that simulated data is labelled where an operator
reads it.

#### Acceptance Criteria

1. THE `DG-SIM` Gate_Dossier SHALL record the flag-placement part of the Decision_Content of
   `P7-SIM-002` as element names rather than as element values: the location of the simulated-source
   flag on a reported value, and the means by which the flag reaches each display that shows the
   value.
2. THE `DG-SIM` Gate_Dossier SHALL record the discriminator part of the Decision_Content: a means of
   identifying a simulated link that is present in a release configuration.
3. THE `DG-SIM` Gate_Dossier SHALL record Architecture as the owner authorised to sign the flag
   placement, Tab 4 as the implementing owner of the reported value, and Tab 8 as the implementing
   owner of `SCR-SIM-01`.
4. THE `DG-SIM` Gate_Dossier SHALL record the defect the Gate exists to avoid: the only QML-reachable
   discriminator compares `LinkConfiguration.linkType` against `TypeMock`, which
   `LinkConfiguration.h` declares inside `#ifdef QT_DEBUG`, so a release build reports every simulated
   link as not simulated.
5. THE `DG-SIM` Gate_Dossier SHALL record as a Sign_Off_Condition that the Closure_Record states the
   treatment of log replay as a distinct classification from simulation.
6. THE `DG-SIM` Gate_Dossier SHALL record as a Sign_Off_Condition that the Closure_Record makes the
   simulated classification a property of the reported value rather than a property of a screen.
7. WHEN both parts of THE `DG-SIM` Closure_Record are accepted, THE Gate_Register SHALL record
   `SCR-SIM-01` as Released.
8. WHILE `DG-SIM` is `OPEN`, THE Aerora_Station SHALL report the simulated-source classification of
   every reported value as `UNKNOWN`.
9. THE `DG-SIM` Gate_Dossier SHALL record as Acceptance_Criteria, excluded from the Gate,
   `P7-SIM-003`'s rule that a simulated vehicle cannot satisfy a capability requirement for an
   authorization decision, and `P7-SIM-004`'s build-configuration constraint on test mocks.
10. IF THE `DG-SIM` Closure_Record places the simulated-source flag on a screen rather than on a
    reported value, THEN THE Gate_Review SHALL hold the flag-placement part at `OPEN` and SHALL name
    the reported value as the carrier criterion 6 requires.
11. IF THE `DG-SIM` Closure_Record names a discriminator that a release configuration compiles out,
    THEN THE Gate_Review SHALL hold the discriminator part at `OPEN` and SHALL name the release
    configuration together with the defect of criterion 4.
12. IF THE `DG-SIM` Closure_Record treats log replay and simulation as one classification, THEN THE
    Gate_Review SHALL hold the discriminator part at `OPEN` and SHALL name log replay as the distinct
    classification criterion 5 requires.
13. IF one part of THE `DG-SIM` Closure_Record is accepted while the other part is not accepted, THEN
    THE Gate_Register SHALL record `DG-SIM` as `OPEN`, record `SCR-SIM-01` as not Released, and name
    the part that is not accepted.
14. WHILE a part of `DG-SIM` holds no accepted Closure_Record, IF a proposed repository change
    introduces a simulated-source discriminator or a simulated-source flag on a reported value, THEN
    THE Gate_Review SHALL reject the change before the change enters the repository and SHALL name
    `DG-SIM` together with the part.
15. WHILE `DG-SIM` is `OPEN`, THE Aerora_Station SHALL report the simulated-source classification of a
    value reported over a simulated link as `UNKNOWN` on every display that shows the value, rather
    than reporting the value as not simulated.
16. THE `DG-SIM` Gate_Dossier SHALL record `SCR-SIM-01` as retaining one entry classified as a Gate,
    which is the absent discriminator of criterion 2, and SHALL record each guard of Requirement 4
    criterion 5 as outside the Gate.
17. THE `DG-SIM` Gate_Dossier SHALL record the owner authorised to sign the discriminator part of the
    Decision_Content.
18. IF THE `DG-SIM` Gate_Dossier records no owner for the discriminator part, THEN THE Gate_Review
    SHALL hold the discriminator part at `OPEN` and SHALL name the discriminator part as carrying no
    recorded owner.

## Source Material

| Document | Supplies |
| --- | --- |
| [03-gui-information-architecture-and-screens.md](../05-product-completion/03-gui-information-architecture-and-screens.md) | The screen inventory, per-screen seam, owner and blocker, and the record of why five blockers were misread |
| [01-product-requirements-and-completion-plan.md](../05-product-completion/01-product-requirements-and-completion-plan.md) | The `P7-*` requirement statements and statuses |
| [08-risk-register-and-open-decisions.md](../05-product-completion/08-risk-register-and-open-decisions.md) | The `Q-*` and `D-*` decisions with owners and statuses |
| [05-implementation-dependency-graph.md](../05-product-completion/05-implementation-dependency-graph.md) | The blocker-to-screen mapping and the decision-to-work mapping |
| [09-local-persistence-architecture.md](../05-product-completion/09-local-persistence-architecture.md) | The persistence design and the `D-DAT-01`..`07` decisions |
| [00-ui-architecture-map.md](../04-ui-implementation/00-ui-architecture-map.md) | The verified state, the seam budget and the `qmllint` finding |
| [11-telemetry-icd.md](../02-interface-specification/11-telemetry-icd.md) | The six Aerora fact groups and the constructor-time registration constraint |
| [32-integration-adrs.md](../03-implementation-specification/32-integration-adrs.md) | `ADR-016`, the `Q4-002` decision record and the `R4-001` hazard |

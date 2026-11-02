#!/usr/bin/env python3
"""Tests for the premise loading and the cross-document checks in tools/validate_gate_register.py.

The fixture register is small but not fictional, because most of the checks compare the register
against facts requirements.md names: the four Two_Gate_Screens of Requirement 1 criterion 5 with the
two Gates each waits on, the five Released-with-no-Gate screens of Requirement 2 criterion 8, and the
five misread entries of criterion 7 with the retained-Gate statement of criterion 11. A fixture that
renamed those screens would test the checks against a different document from the one they protect.

Everything else is the smallest shape the checks read: the ten Gates, because the schema requires
ten and the Release_Set derivation is keyed by them; one single-Gate screen; one delivered screen;
and no Decision_Content at all. The register records element names, and a fixture inventing one
would invent the Contract_Value this spec exists to withhold.

Each test mutates one axis of the baseline and asserts the finding of the check that owns that axis.
The baseline is asserted to produce no finding from any check, because a check that rejects the
agreeing case is as broken as one that accepts the disagreeing case.

Requirements 1.2 to 1.7, 1.10 to 1.12, 2.5 and 2.8: a recorded Release_Set equals the set derived
from the classifications, the recorded Release_Sets and the Released-with-no-Gate set partition the
undelivered screens, a recorded Released flag equals the derived one, no Release_Set holds a
delivered screen, and the register's screens and the inventory's rows are one set.

Requirements 2.1, 2.7, 2.10 and 2.11: the register's classified entries and the inventory's
"Blocked by" cell are one set, the five misreads carry their reclassification, and the retained-Gate
statement holds. The inventory's declared totals agree with its own rows, which is the only check
here with evidence outside a fixture and is tested against the real inventory as well.

Requirements 4.5, 4.6 and 4.8: the registry covers the nine guards Requirement 4 criterion 5 names,
each Gate records the guards that hold it visible or records that it rests on the Gate_Review alone,
every change permission resolves to a part that exists, and a guard is free exactly when every part
permitted to change it has recorded a permitting verdict for it. The guard tests are the one group
here that reads a Closure_Record's content against the register rather than against itself, because
criterion 6 puts the verdict in the record and the mapping in the register.

Requirement 3.9: every `VERIFIED` row carries an observation naming a suite and a slot that resolve
against `aerora/test/**` and the overlay's CMake registration, and every status the inventory's rows
carry is a member of the vocabulary the inventory declares. That check reads three files no fixture
register can carry, so it is tested twice -- against a fixture tree under ``tmp_path``, which is what
makes the suite-resolution cases independent of which Aerora suites exist today, and against the
repository itself, which is the only evidence it resolves a real observation.
"""

from __future__ import annotations

import json
import re
from dataclasses import replace
from pathlib import PurePosixPath
from typing import TYPE_CHECKING

import pytest
from common.gate_register import (
    ABSENT,
    ACCEPTED_PART_STATE,
    INVENTORY_COLUMNS,
    Amendment,
    BlockingEntry,
    ClosureRecord,
    DecisionContentElement,
    GateDossier,
    GatePart,
    GateRegister,
    Guard,
    GuardPermission,
    InventoryReference,
    Observation,
    OpenStateReading,
    RationaleReference,
    ReadOnlyException,
    ReadOnlyPosture,
    ReleaseSetEntry,
    ScreenRecord,
    SettledElement,
    SettledEntry,
    Signature,
    SignOffCondition,
    SignOffConditionVerdict,
    dump_closure_record,
    dump_gate_register,
    guard_freedom,
    is_entry_id,
    parse_screen_inventory,
    part_reference,
    released,
)
from common.git import AddCommit, run_git
from common.markdown import md_table
from validate_gate_register import (
    ACCEPTED_RECORD_STATE,
    CLOSED_STATUS,
    CLOSURE_AMENDMENTS,
    CLOSURE_COMPLETENESS,
    CLOSURE_RECORD_SCHEMA_PATH,
    CLOSURE_SIGNATURES,
    DELIVERED_OUTSIDE,
    ENTRY_CLASSIFICATION,
    ENTRY_COMMIT,
    GATE_STATUS_DERIVATION,
    GUARD_REGISTRY,
    INVENTORY_COVERAGE,
    INVENTORY_OWNER,
    INVENTORY_TOTALS,
    KEYED_SHAPE,
    MAPPING_SHAPE,
    MAVLINK_IDENTIFIER_GUARD,
    MECHANICAL_GUARDS,
    NOT_DELIVERED_LABEL,
    OPEN_STATUS,
    OVERLAY_TREE,
    PARTITION,
    PREMISES,
    RECLASSIFICATIONS,
    RECLASSIFIED_ENTRIES,
    RECORDED_IDENTITY,
    REGISTER_PATH,
    REGISTER_SCHEMA_PATH,
    RELEASE_SET_DERIVATION,
    RELEASED_DERIVATION,
    RELEASED_WITH_NO_GATE,
    RETAINED_GATES,
    SCHEMA,
    SIGNATURE_NOTICE,
    SIGNED_BY_KEY,
    SIGNOFF_VERDICTS,
    SINGLE_SHAPE,
    SUPERSEDED_STATUS,
    TEST_TREE,
    TOTAL_LABEL,
    TOTALS_COLUMNS,
    TWO_GATE,
    TWO_GATE_SCREENS,
    VERIFIED_OBSERVATION,
    VERIFIED_STATUS,
    NamedClosureRecord,
    Premises,
    _pointer,
    check_closure_amendments,
    check_closure_completeness,
    check_closure_entry_commit,
    check_closure_signatures,
    check_delivered_outside,
    check_entry_classification,
    check_gate_status_derivation,
    check_guard_registry,
    check_inventory_coverage,
    check_inventory_totals,
    check_partition,
    check_reclassifications,
    check_release_set_derivation,
    check_released_derivation,
    check_signoff_verdicts,
    check_two_gate,
    check_verified_observation,
    derive,
    load_premises,
    main,
    run_checks,
)

from ._helpers import REPO_ROOT

if TYPE_CHECKING:
    from collections.abc import Iterable, Mapping, Sequence
    from pathlib import Path

    from common.gate_register import GuardFreedom
    from validate_gate_register import Finding

# The register names the inventory, so the fixture inventory is written where the real one lives and
# no test invents a path of its own.
INVENTORY_PATH = ".kiro/specs/05-product-completion/03-gui-information-architecture-and-screens.md"

# The document the inventory's Status column legend names, and the legend itself. The fixture
# inventory carries the real reference, so a fixture row's status is read against the vocabulary the
# real document declares and no test here lists a status word of its own.
VOCABULARY_PATH = ".kiro/specs/05-product-completion/00-README.md"
STATUS_LEGEND = "Columns: **Status** per [00-README](00-README.md#status-vocabulary)"

# The ten Gates of Requirement 1 criterion 1, which the schema requires the register to carry.
FIXTURE_GATES = (
    "DG-MIS",
    "DG-TLM",
    "DG-FRS",
    "DG-ROL",
    "DG-PAY",
    "DG-AUT",
    "DG-STO",
    "DG-FLT",
    "DG-VID",
    "DG-SIM",
)

# A Gate no fixture screen names, so a test can record a Release_Set against it without disturbing
# any other check.
UNUSED_GATE = "DG-FRS"

SINGLE_GATE_SCREEN = "SCR-TEL-01"
SINGLE_GATE = "DG-TLM"
DELIVERED_SCREEN = "SCR-SHL-01"
UNDELIVERED_STATUS = "DESIGNED"

# The one screen the schema pins the read-only exception to. It is also one of Requirement 2
# criterion 7's five misreads, so the baseline carries a screen record for it; no set check reads
# the posture.
EXCEPTION_SCREEN = "SCR-HLP-03"

# The one misread screen Requirement 2 criterion 11 records as retaining a Gate-classified entry.
RETAINED_GATE_SCREEN = "SCR-VID-01"

# The four misread screens that retain no Gate-classified entry are rows the real inventory reports
# VERIFIED, so the baseline records them delivered. An undelivered screen with no Gate-classified
# entry is a partition finding unless Requirement 2 criterion 8 names it, and these four are not
# among the five it names.
DELIVERED_SCREENS = frozenset({DELIVERED_SCREEN}) | {
    screen for screen, gates in RETAINED_GATES.items() if not gates
}

GATE = "Gate"
ACCEPTANCE_CRITERION = "Acceptance_Criterion"


def _entry(
    entry: str,
    *,
    gate: str | None = None,
    classification: str | None = None,
    reclassified_from: str | None = None,
    inventory_text: str | None = None,
) -> BlockingEntry:
    """Build a blocking entry, classified as a Gate exactly when it names one."""
    return BlockingEntry(
        id=entry,
        classification=classification or (GATE if gate is not None else ACCEPTANCE_CRITERION),
        gate=gate,
        reclassified_from=reclassified_from,
        requirement=None,
        inventory_text=inventory_text,
    )


def _reclassified(screen: str) -> BlockingEntry:
    """The misread entry Requirement 2 criterion 7 records for *screen*, as the register records it.

    An Acceptance_Criterion carrying the Gate it was misread as, which is what the
    ``reclassifications`` check reads.
    """
    return _entry(RECLASSIFIED_ENTRIES[screen], reclassified_from=GATE)


def _gate_entries(screen: str, *gates: str) -> list[BlockingEntry]:
    """One Gate-classified entry per Gate, with an entry identifier derived from the screen."""
    area = screen.removeprefix("SCR-")
    return [_entry(f"P7-{area}-{index:03d}", gate=gate) for index, gate in enumerate(gates, 1)]


# The suite and slot the baseline records an observation of. Real names, because `_premises` roots
# the premises at the repository and `verified-observation` resolves every recorded observation
# against the tree it finds there; the fixture tree `_test_tree` lays out declares and registers the
# same pair, so one pair resolves against either root and the baseline agrees with both.
BASELINE_SUITE = "AeroraSceneAttachmentUITest"
BASELINE_SLOT = "_provenancePageIsReachableAndReadsTheRegisteredSingleton"


def _observation() -> Observation:
    """The observation a delivered fixture screen records."""
    return Observation(suite=BASELINE_SUITE, slot=BASELINE_SLOT)


def _screen(
    screen: str,
    *entries: BlockingEntry,
    observation: Observation | None = None,
) -> ScreenRecord:
    """A screen record whose Released flag is the one the derivation produces for its entries.

    Every fixture Gate is open, so the flag is the absence of a Gate-classified entry. Recording the
    derived value is what makes the baseline agree; a test that wants disagreement flips it.
    """
    return ScreenRecord(
        id=screen,
        released=not any(entry.classification == GATE for entry in entries),
        observation=observation,
        blocking_entries=list(entries),
    )


def _baseline_screens() -> list[ScreenRecord]:
    """One record per fixture screen, in the shape the register is in while all ten Gates are open.

    The four Two_Gate_Screens carry their two Gates, the five Released-with-no-Gate screens carry
    one Acceptance_Criterion each, the five misread screens carry their reclassified entry and the
    Gates criterion 11 records them retaining, one screen carries a single Gate, and one is
    delivered.
    """
    screens = [
        _screen(screen, *_gate_entries(screen, *sorted(gates)))
        for screen, gates in sorted(TWO_GATE_SCREENS.items())
    ]
    screens.append(_screen(SINGLE_GATE_SCREEN, *_gate_entries(SINGLE_GATE_SCREEN, SINGLE_GATE)))
    screens.extend(
        _screen(screen, _entry(f"Q4-{index:03d}"))
        for index, screen in enumerate(sorted(RELEASED_WITH_NO_GATE), 1)
    )
    screens.extend(
        _screen(
            screen,
            _reclassified(screen),
            *_gate_entries(screen, *gates),
            observation=_observation() if screen in DELIVERED_SCREENS else None,
        )
        for screen, gates in sorted(RETAINED_GATES.items())
    )
    screens.append(_screen(DELIVERED_SCREEN, observation=_observation()))
    return screens


def _recorded_release_sets(screens: Sequence[ScreenRecord]) -> dict[str, list[str]]:
    """The Release_Sets the classifications imply, which is what the baseline records.

    Derived from the same entries the check derives from, so the baseline agrees by construction and
    every disagreement a test asserts is one the test wrote.
    """
    recorded: dict[str, list[str]] = {gate: [] for gate in FIXTURE_GATES}
    for record in screens:
        for entry in record.blocking_entries:
            named = entry.classification == GATE and entry.gate is not None
            if named and record.id not in recorded[str(entry.gate)]:
                recorded[str(entry.gate)].append(record.id)
    return recorded


# The one part every baseline Gate records, which is what a change permission resolves against.
BASELINE_PART = "whole"

# The three Gates besides SINGLE_GATE that requirements.md records a guard of criterion 5 against.
MISSION_GATE = "DG-MIS"
ROLE_GATE = "DG-ROL"
PAYLOAD_GATE = "DG-PAY"

# The eight guards criterion 5 names by symbol. The ninth is the MAVLink-identifier check, which
# the criterion names as a check rather than as a symbol, and MAVLINK_IDENTIFIER_GUARD is the name
# the registry records it under.
POWER_GUARD = "AeroraPowerModel::kDefinedThresholdCount"
TELEMETRY_GUARD = "AeroraVehicleFactGroupRegistrar::isRegistrationAvailable()"
AUTHORIZATION_GUARD = "AeroraCorePlugin::isAuthorizationEnforced()"
ACTUATOR_PATH_GUARD = "kGroundStationActuatorPathImplemented"
PAYLOAD_ACTION_GUARD = "kAutonomousPayloadActionImplemented"
SOURCE_TRANSITION_GUARD = "kAutomaticSourceTransitionImplemented"
ENERGY_ESTIMATION_GUARD = "kRemainingEnergyEstimationImplemented"
REPLAY_GUARD = "kGcsPayloadStateReplayImplemented"

# Each of the nine against the Gates requirements.md records it holding open: the power, source
# transition and energy guards against DG-PAY (10.17); the fact-group registrar against DG-TLM
# (7.18); the authorization guard against DG-ROL (9.16); the actuator-path and payload-action
# guards against both DG-PAY and DG-ROL (10.17, 10.18, 9.17), which is the conjunction the
# permission rule turns on; and the replay guard with the MAVLink-identifier check against DG-MIS
# (6.17).
#
# Real symbols and the mapping requirements.md names, for the reason the Two_Gate_Screens are real:
# a fixture that renamed them would test the check against a different document from the one it
# protects. The key set is asserted equal to MECHANICAL_GUARDS below, so a drift between the two is
# a failing test rather than a check with nothing left to compare.
BASELINE_GUARD_GATES: Mapping[str, tuple[str, ...]] = {
    POWER_GUARD: (PAYLOAD_GATE,),
    TELEMETRY_GUARD: (SINGLE_GATE,),
    AUTHORIZATION_GUARD: (ROLE_GATE,),
    ACTUATOR_PATH_GUARD: (PAYLOAD_GATE, ROLE_GATE),
    PAYLOAD_ACTION_GUARD: (PAYLOAD_GATE, ROLE_GATE),
    SOURCE_TRANSITION_GUARD: (PAYLOAD_GATE,),
    ENERGY_ESTIMATION_GUARD: (PAYLOAD_GATE,),
    REPLAY_GUARD: (MISSION_GATE,),
    MAVLINK_IDENTIFIER_GUARD: (MISSION_GATE,),
}

# Where the fixture records a guard declared and asserted, and what it records against a guard
# criterion 5 names no value for. All three are fixtures because the data half reads none of them:
# `guard-registry` covers, maps and permits, and reading a declaration is `guard-value`'s rule in
# tools/analyzers/gate_guard_check.py.
GUARD_DECLARATION = "aerora/src/AFixtureGuard.h"
GUARD_ASSERTION = f"{TEST_TREE}/AFixtureGuardTest.cc"
FIXTURE_GUARD_VALUE = "a fixture literal"

# A guard outside the nine, which the registry may carry: criterion 5 covers "at least" the nine,
# and each Gate's own named guard is one more.
FIXTURE_GUARD = "AFixtureNamespace::kAFixtureGuard"


def _guard(
    symbol: str,
    *,
    holds_open: Sequence[str] | None = None,
    permitted_to_change_by: Sequence[str] | None = None,
    recorded_value: str | None = None,
) -> Guard:
    """One registry entry, permitted to change by the `whole` part of each Gate it holds open.

    The recorded value defaults to the one criterion 5 names, which is what keeps the baseline both
    pinned and agreeing: every baseline part is unaccepted, so no guard is free and every recorded
    value is read.
    """
    gates = BASELINE_GUARD_GATES.get(symbol, ()) if holds_open is None else tuple(holds_open)
    permitting = (
        [part_reference(gate, BASELINE_PART) for gate in gates]
        if permitted_to_change_by is None
        else list(permitted_to_change_by)
    )
    return Guard(
        symbol=symbol,
        declared_in=GUARD_DECLARATION,
        recorded_value=recorded_value or MECHANICAL_GUARDS.get(symbol) or FIXTURE_GUARD_VALUE,
        asserted_in=[GUARD_ASSERTION],
        holds_open=list(gates),
        holds_no_gate_open=None if gates else True,
        permitted_to_change_by=permitting,
        disagreement=None,
    )


def _gate_guards(registry: Sequence[Guard]) -> dict[str, list[str]]:
    """The guards each Gate_Dossier records, transposed from the registry it is given.

    Derived rather than written twice, so the two ends of one relation agree by construction and
    every disagreement a test asserts is one the test wrote.
    """
    recorded: dict[str, list[str]] = {gate: [] for gate in FIXTURE_GATES}
    for guard in registry:
        for gate in guard.holds_open:
            recorded[gate].append(guard.symbol)
    return recorded


def _dossier(gate: str, release_set: Iterable[str], guards: Iterable[str] = ()) -> GateDossier:
    """An OPEN Gate with one unaccepted part, recording *release_set* and *guards*.

    A Gate recording no guard records that it rests on the Gate_Review alone, because Requirement 4
    criterion 8 requires the absence to be recorded rather than read off an empty list. A Gate that
    records one leaves the flag absent, which is the only other thing the schema admits.
    """
    recorded = list(guards)
    return GateDossier(
        id=gate,
        status="OPEN",
        implementing_owner="Product",
        rationale=RationaleReference(document=INVENTORY_PATH, anchor="#a-fixture-anchor"),
        parts=[
            GatePart(
                id=BASELINE_PART,
                signing_owners=["Product"],
                state="open",
                closure_record=None,
                elements=[],
                sign_off_conditions=[],
            )
        ],
        release_set=[ReleaseSetEntry(screen=screen, part=None) for screen in release_set],
        excluded_acceptance_criteria=[],
        guards=recorded,
        rests_on_review_alone=None if recorded else True,
        open_state_reading=OpenStateReading(
            state_class="a fixture class", reports_gate_identifier=True
        ),
    )


def _register(
    screens: Sequence[ScreenRecord] | None = None,
    release_sets: Mapping[str, Iterable[str]] | None = None,
    guards: Sequence[Guard] | None = None,
) -> GateRegister:
    """The fixture register. Every axis defaults to the agreeing baseline."""
    records = _baseline_screens() if screens is None else list(screens)
    recorded = _recorded_release_sets(records) if release_sets is None else release_sets
    registry = (
        [_guard(symbol) for symbol in BASELINE_GUARD_GATES] if guards is None else list(guards)
    )
    mapped = _gate_guards(registry)
    return GateRegister(
        schema_ref=None,
        inventory=InventoryReference(
            path=INVENTORY_PATH,
            authoritative_for=["the screen set", "each screen's delivery status"],
        ),
        gates=[_dossier(gate, recorded.get(gate, ()), mapped[gate]) for gate in FIXTURE_GATES],
        screens=records,
        guards=registry,
        read_only=ReadOnlyPosture(
            gate="DG-ROL",
            exceptions=[
                ReadOnlyException(
                    screen=EXCEPTION_SCREEN,
                    file="aerora/src/UI/AFixturePage.qml",
                    control="aFixtureControl",
                    reason="a fixture reason",
                    requirement=None,
                )
            ],
            forbidden_until=[],
            pre_approved_undeclared_seams=[],
        ),
        contract_surface=[],
    )


def _blocked_by(register: GateRegister) -> dict[str, str]:
    """The "Blocked by" cell each screen's classified entries imply, which the baseline carries.

    Only the entries whose key is an inventory identifier render as a code span, because a
    register-assigned key is the register's and the cell it matches states its entry in prose. A
    test that wants that prose appends it to the cell.
    """
    return {
        record.id: "; ".join(
            f"`{entry.id}`" for entry in record.blocking_entries if is_entry_id(entry.id)
        )
        or "-"
        for record in register.screens
    }


def _declared_totals(rows: Mapping[str, str]) -> list[str]:
    """The declarations inventory-totals compares, every figure computed from *rows*.

    The fixture document agrees with itself for the reason the real inventory does -- each figure is
    counted from the rows -- so a test asserting a disagreement rewrites one site and nothing else
    moves.
    """
    screens = len(rows)
    verified = sum(1 for status in rows.values() if status == VERIFIED_STATUS)
    superseded = sum(1 for status in rows.values() if status == SUPERSEDED_STATUS)
    undelivered = screens - verified - superseded
    totals = md_table(
        TOTALS_COLUMNS,
        [
            [f"`{VERIFIED_STATUS}` drawn in a booted window", f"**{verified}**", "a fixture list"],
            [f"`{SUPERSEDED_STATUS}` by a delivered screen", superseded, "a fixture list"],
            [NOT_DELIVERED_LABEL, undelivered, "a fixture note"],
            [f"**{TOTAL_LABEL}**", f"**{screens}**", ""],
        ],
    )
    return [
        "### Inventory totals",
        "",
        totals,
        "",
        f"The {undelivered} undelivered screens are a fixture paragraph.",
        "",
        "## 9. Why there are no action buttons yet",
        "",
        f"The screen inventory has {screens} entries and zero operator action controls.",
    ]


def _inventory_text(rows: Mapping[str, str], blocked: Mapping[str, str] | None = None) -> str:
    """Render *rows* as one area table of the inventory's seven columns, with its own totals.

    *blocked* is the "Blocked by" cell per screen; a screen it omits carries no entry.
    """
    cells = {} if blocked is None else blocked
    table = md_table(
        INVENTORY_COLUMNS,
        [
            [
                f"`{screen}`",
                "A screen",
                "O",
                "a seam",
                f"`{status}`",
                "7",
                cells.get(screen, "-"),
            ]
            for screen, status in rows.items()
        ],
    )
    lines = [
        "## 8. Screen inventory",
        "",
        f"{len(rows)} screens across 1 area. Every one maps to a verified seam.",
        "",
        STATUS_LEGEND,
        "",
        f"### A fixture area -- {len(rows)} screens",
        "",
        table,
        "",
        *_declared_totals(rows),
        "",
    ]
    return "\n".join(lines)


def _baseline_rows(register: GateRegister) -> dict[str, str]:
    """A row per register screen, delivered for the screens the fixture records as delivered."""
    return {
        record.id: VERIFIED_STATUS if record.id in DELIVERED_SCREENS else UNDELIVERED_STATUS
        for record in register.screens
    }


def _premises_from_text(
    register: GateRegister,
    text: str,
    closures: Mapping[str, NamedClosureRecord] | None = None,
) -> Premises:
    """Premises over a register and an inventory document the caller wrote.

    *closures* is empty by default, which is the shape the register is in while no part names a
    Closure_Record. A settlement test attaches the records its parts name.
    """
    inventory = parse_screen_inventory(text, path=INVENTORY_PATH)
    return Premises(
        root=REPO_ROOT,
        register=register,
        inventory=inventory,
        derived=derive(register, inventory),
        closures=dict(closures or {}),
    )


def _premises(
    register: GateRegister | None = None,
    rows: Mapping[str, str] | None = None,
    blocked: Mapping[str, str] | None = None,
    closures: Mapping[str, NamedClosureRecord] | None = None,
) -> Premises:
    """Premises built the way load_premises builds them, over a fixture register and inventory."""
    recorded = _register() if register is None else register
    return _premises_from_text(
        recorded,
        _inventory_text(
            _baseline_rows(recorded) if rows is None else rows,
            _blocked_by(recorded) if blocked is None else blocked,
        ),
        closures,
    )


def _with_screen(
    screens: Sequence[ScreenRecord], screen: str, **changes: object
) -> list[ScreenRecord]:
    """Replace one screen record's fields, leaving every other record as it was."""
    return [replace(record, **changes) if record.id == screen else record for record in screens]


def _with_gate(register: GateRegister, gate: str, **changes: object) -> GateRegister:
    """Replace one Gate_Dossier's fields, leaving every other dossier as it was."""
    return replace(
        register,
        gates=[
            replace(dossier, **changes) if dossier.id == gate else dossier
            for dossier in register.gates
        ],
    )


def _with_guard(register: GateRegister, symbol: str, **changes: object) -> GateRegister:
    """Replace one registry entry's fields, leaving every other entry and every dossier as it was."""
    return replace(
        register,
        guards=[
            replace(guard, **changes) if guard.symbol == symbol else guard
            for guard in register.guards
        ],
    )


def _messages(findings: Sequence[Finding], check: str) -> list[str]:
    assert {finding.check for finding in findings} <= {check}
    return [finding.message for finding in findings]


# ---------------------------------------------------------------------------
# The baseline
# ---------------------------------------------------------------------------


def test_the_agreeing_register_produces_no_finding_from_any_check() -> None:
    assert run_checks(_premises()) == []


def test_the_baseline_carries_the_named_facts_the_checks_read() -> None:
    premises = _premises()

    # The fixture is the shape the checks compare against, so these are the premises of every test
    # below rather than assertions about the register the spec will write.
    assert set(premises.derived.two_gate_screens) == set(TWO_GATE_SCREENS)
    assert premises.derived.undelivered == frozenset(
        record.id for record in premises.register.screens if record.id not in DELIVERED_SCREENS
    )
    assert not premises.derived.releases[SINGLE_GATE_SCREEN].released
    assert all(premises.derived.releases[screen].released for screen in RELEASED_WITH_NO_GATE)
    assert (
        premises.derived.blocking_gates[RETAINED_GATE_SCREEN]
        == RETAINED_GATES[RETAINED_GATE_SCREEN]
    )


# ---------------------------------------------------------------------------
# release-set-derivation
# ---------------------------------------------------------------------------


def test_release_set_derivation_names_a_screen_the_gate_does_not_record() -> None:
    recorded = _recorded_release_sets(_baseline_screens())
    recorded[SINGLE_GATE] = []

    findings = check_release_set_derivation(_premises(_register(release_sets=recorded)))

    (message,) = _messages(findings, RELEASE_SET_DERIVATION)
    assert SINGLE_GATE_SCREEN in message
    assert f"{SINGLE_GATE} does not record" in message


def test_release_set_derivation_names_a_screen_no_entry_classifies_against() -> None:
    held = sorted(RELEASED_WITH_NO_GATE)[0]
    recorded = _recorded_release_sets(_baseline_screens())
    recorded[UNUSED_GATE] = [held]

    findings = check_release_set_derivation(_premises(_register(release_sets=recorded)))

    (message,) = _messages(findings, RELEASE_SET_DERIVATION)
    assert f"{UNUSED_GATE} records {held}" in message
    assert "classifies as a Gate" in message


def test_release_set_derivation_reports_an_exchanged_screen_as_two_findings() -> None:
    held = sorted(RELEASED_WITH_NO_GATE)[0]
    recorded = _recorded_release_sets(_baseline_screens())
    recorded[SINGLE_GATE] = [held]

    findings = check_release_set_derivation(_premises(_register(release_sets=recorded)))

    messages = _messages(findings, RELEASE_SET_DERIVATION)
    assert len(messages) == 2
    assert any(held in message and "no blocking entry" in message for message in messages)
    assert any(
        SINGLE_GATE_SCREEN in message and "does not record" in message for message in messages
    )


def test_release_set_derivation_reads_two_entries_naming_one_gate_as_one_membership() -> None:
    screens = _with_screen(
        _baseline_screens(),
        SINGLE_GATE_SCREEN,
        blocking_entries=_gate_entries(SINGLE_GATE_SCREEN, SINGLE_GATE, SINGLE_GATE),
    )

    assert check_release_set_derivation(_premises(_register(screens))) == []


# ---------------------------------------------------------------------------
# released-derivation
# ---------------------------------------------------------------------------


def test_released_derivation_names_the_open_gate_of_a_screen_recorded_released() -> None:
    screens = _with_screen(_baseline_screens(), SINGLE_GATE_SCREEN, released=True)

    findings = check_released_derivation(_premises(_register(screens)))

    (message,) = _messages(findings, RELEASED_DERIVATION)
    assert f"{SINGLE_GATE_SCREEN} is recorded Released" in message
    assert f"open Gate {SINGLE_GATE}" in message


def test_released_derivation_names_both_open_gates_of_a_two_gate_screen() -> None:
    screen = sorted(TWO_GATE_SCREENS)[0]
    screens = _with_screen(_baseline_screens(), screen, released=True)

    findings = check_released_derivation(_premises(_register(screens)))

    (message,) = _messages(findings, RELEASED_DERIVATION)
    for gate in TWO_GATE_SCREENS[screen]:
        assert gate in message


def test_released_derivation_rejects_a_screen_recorded_held_by_nothing() -> None:
    held = sorted(RELEASED_WITH_NO_GATE)[0]
    screens = _with_screen(_baseline_screens(), held, released=False)

    findings = check_released_derivation(_premises(_register(screens)))

    (message,) = _messages(findings, RELEASED_DERIVATION)
    assert f"{held} is recorded as not Released" in message
    assert "no Gate its blocking list names is open" in message


def test_released_derivation_releases_a_screen_once_its_only_gate_is_accepted() -> None:
    accepted = replace(
        _dossier(SINGLE_GATE, [SINGLE_GATE_SCREEN]),
        status="CLOSED",
        parts=[
            replace(
                _dossier(SINGLE_GATE, []).parts[0],
                state="accepted",
                closure_record=f"closures/{SINGLE_GATE}.whole.json",
            )
        ],
    )
    register = _register()
    register = replace(
        register,
        gates=[accepted if dossier.id == SINGLE_GATE else dossier for dossier in register.gates],
        screens=_with_screen(register.screens, SINGLE_GATE_SCREEN, released=True),
    )

    assert check_released_derivation(_premises(register)) == []


# ---------------------------------------------------------------------------
# two-gate
# ---------------------------------------------------------------------------


def test_two_gate_rejects_a_fifth_screen_carrying_two_gates() -> None:
    screens = _with_screen(
        _baseline_screens(),
        SINGLE_GATE_SCREEN,
        blocking_entries=_gate_entries(SINGLE_GATE_SCREEN, SINGLE_GATE, UNUSED_GATE),
    )

    findings = check_two_gate(_premises(_register(screens)))

    (message,) = _messages(findings, TWO_GATE)
    assert f"{SINGLE_GATE_SCREEN} carries two Gate-classified entries" in message
    assert SINGLE_GATE in message
    assert UNUSED_GATE in message


def test_two_gate_names_a_recorded_two_gate_screen_that_lost_a_gate() -> None:
    screen = sorted(TWO_GATE_SCREENS)[0]
    kept, _ = sorted(TWO_GATE_SCREENS[screen])
    screens = _with_screen(
        _baseline_screens(), screen, blocking_entries=_gate_entries(screen, kept)
    )

    findings = check_two_gate(_premises(_register(screens)))

    (message,) = _messages(findings, TWO_GATE)
    assert f"{screen} is recorded a Two_Gate_Screen against" in message
    assert f"its blocking list names {kept}" in message


def test_two_gate_names_a_recorded_two_gate_screen_whose_entries_lost_both_gates() -> None:
    screen = sorted(TWO_GATE_SCREENS)[0]
    screens = _with_screen(_baseline_screens(), screen, blocking_entries=[_entry("Q4-900")])

    findings = check_two_gate(_premises(_register(screens)))

    messages = _messages(findings, TWO_GATE)
    assert any("classifies no entry as a Gate" in message for message in messages)


def test_two_gate_rejects_a_two_gate_screen_recorded_against_the_wrong_gate() -> None:
    screen = sorted(TWO_GATE_SCREENS)[0]
    kept, _ = sorted(TWO_GATE_SCREENS[screen])
    screens = _with_screen(
        _baseline_screens(), screen, blocking_entries=_gate_entries(screen, kept, UNUSED_GATE)
    )

    findings = check_two_gate(_premises(_register(screens)))

    (message,) = _messages(findings, TWO_GATE)
    assert f"{screen} is recorded a Two_Gate_Screen against" in message
    assert UNUSED_GATE in message


def test_two_gate_rejects_a_screen_carrying_three_gates() -> None:
    screen = sorted(TWO_GATE_SCREENS)[0]
    first, second = sorted(TWO_GATE_SCREENS[screen])
    screens = _with_screen(
        _baseline_screens(),
        screen,
        blocking_entries=_gate_entries(screen, first, second, UNUSED_GATE),
    )

    findings = check_two_gate(_premises(_register(screens)))

    messages = _messages(findings, TWO_GATE)
    # Three Gates is no Two_Gate_Screen, so the screen also goes missing from the derived set. Both
    # findings are the point: a third Gate is invisible to a set of pairs.
    assert any(f"{screen} carries 3 Gate-classified entries" in message for message in messages)
    assert any("in more than two Release_Sets" in message for message in messages)


# ---------------------------------------------------------------------------
# partition
# ---------------------------------------------------------------------------


def test_partition_names_the_gate_whose_release_set_must_carry_an_uncovered_screen() -> None:
    recorded = _recorded_release_sets(_baseline_screens())
    recorded[SINGLE_GATE] = []

    findings = check_partition(_premises(_register(release_sets=recorded)))

    (message,) = _messages(findings, PARTITION)
    assert f"{SINGLE_GATE_SCREEN} is undelivered, appears in no Release_Set" in message
    assert f"names {SINGLE_GATE}, whose Release_Set must carry it" in message


def test_partition_rejects_a_screen_recorded_under_both_branches() -> None:
    held = sorted(RELEASED_WITH_NO_GATE)[0]
    recorded = _recorded_release_sets(_baseline_screens())
    recorded[UNUSED_GATE] = [held]

    findings = check_partition(_premises(_register(release_sets=recorded)))

    (message,) = _messages(findings, PARTITION)
    assert f"{held} is recorded in the Release_Set of {UNUSED_GATE}" in message
    assert "both branches of Requirement 1 criterion 6" in message


def test_partition_rejects_a_sixth_screen_released_with_no_gate() -> None:
    screens = _with_screen(
        _baseline_screens(), SINGLE_GATE_SCREEN, blocking_entries=[_entry("Q4-901")], released=True
    )

    findings = check_partition(_premises(_register(screens)))

    (message,) = _messages(findings, PARTITION)
    assert f"{SINGLE_GATE_SCREEN} carries no Gate-classified entry" in message
    assert "Requirement 2 criterion 8 records only" in message


def test_partition_names_a_recorded_no_gate_screen_that_gained_a_gate() -> None:
    held = sorted(RELEASED_WITH_NO_GATE)[0]
    screens = _with_screen(
        _baseline_screens(), held, blocking_entries=_gate_entries(held, UNUSED_GATE), released=False
    )

    findings = check_partition(_premises(_register(screens)))

    messages = _messages(findings, PARTITION)
    assert any(f"{held} is recorded Released with no Gate" in message for message in messages)


def test_partition_ignores_a_screen_the_register_omits() -> None:
    """A screen the register drops is inventory-coverage's finding, reported once and not twice."""
    screens = [record for record in _baseline_screens() if record.id != SINGLE_GATE_SCREEN]
    rows = _baseline_rows(_register())

    assert check_partition(_premises(_register(screens), rows)) == []
    assert _messages(
        check_inventory_coverage(_premises(_register(screens), rows)), INVENTORY_COVERAGE
    )


# ---------------------------------------------------------------------------
# delivered-outside
# ---------------------------------------------------------------------------


def test_delivered_outside_names_the_inventory_status_as_the_reason() -> None:
    recorded = _recorded_release_sets(_baseline_screens())
    recorded[UNUSED_GATE] = [DELIVERED_SCREEN]

    premises = _premises(_register(release_sets=recorded))
    findings = check_delivered_outside(premises)

    (message,) = _messages(findings, DELIVERED_OUTSIDE)
    assert f"{UNUSED_GATE} records {DELIVERED_SCREEN} in its Release_Set" in message
    assert VERIFIED_STATUS in message


def test_delivered_outside_accepts_a_release_set_of_undelivered_screens_only() -> None:
    assert check_delivered_outside(_premises()) == []


# ---------------------------------------------------------------------------
# inventory-coverage
# ---------------------------------------------------------------------------


def test_inventory_coverage_names_a_row_with_no_screen_record() -> None:
    screens = [record for record in _baseline_screens() if record.id != SINGLE_GATE_SCREEN]

    findings = check_inventory_coverage(_premises(_register(screens), _baseline_rows(_register())))

    (message,) = _messages(findings, INVENTORY_COVERAGE)
    assert f"carries a row for {SINGLE_GATE_SCREEN}" in message
    assert "records no screen record for it" in message


def test_inventory_coverage_names_a_screen_record_with_no_row() -> None:
    rows = {
        screen: status
        for screen, status in _baseline_rows(_register()).items()
        if screen != SINGLE_GATE_SCREEN
    }

    findings = check_inventory_coverage(_premises(rows=rows))

    (message,) = _messages(findings, INVENTORY_COVERAGE)
    assert f"records a screen record for {SINGLE_GATE_SCREEN}" in message
    assert "carries no row for it" in message


# ---------------------------------------------------------------------------
# entry-classification
# ---------------------------------------------------------------------------

# A key of the shape task 9.2 records for a blocking entry the inventory states in prose, with the
# substring it matches on. Neither is an inventory identifier, which is the whole point of them.
REGISTER_KEY = "seam:a-fixture-seam"
REGISTER_KEY_TEXT = "an absent fixture seam"


def _single_gate_entry() -> BlockingEntry:
    """The one Gate-classified entry the baseline's single-Gate screen carries."""
    (entry,) = _gate_entries(SINGLE_GATE_SCREEN, SINGLE_GATE)
    return entry


def _with_key(entry: BlockingEntry) -> GateRegister:
    """The baseline register with *entry* added to the single-Gate screen's blocking list."""
    return _register(
        _with_screen(
            _baseline_screens(),
            SINGLE_GATE_SCREEN,
            blocking_entries=[_single_gate_entry(), entry],
        )
    )


def test_entry_classification_names_an_entry_the_inventory_cell_does_not_carry() -> None:
    register = _register()
    cells = dict(_blocked_by(register))
    cells[SINGLE_GATE_SCREEN] = "-"

    findings = check_entry_classification(_premises(register, blocked=cells))

    (message,) = _messages(findings, ENTRY_CLASSIFICATION)
    assert f"{SINGLE_GATE_SCREEN}: the register classifies {_single_gate_entry().id}" in message
    assert "carries no entry with that identifier" in message


def test_entry_classification_names_an_entry_the_register_does_not_classify() -> None:
    register = _register()
    cells = dict(_blocked_by(register))
    cells[SINGLE_GATE_SCREEN] = f"{cells[SINGLE_GATE_SCREEN]}; `Q4-900`"

    findings = check_entry_classification(_premises(register, blocked=cells))

    (message,) = _messages(findings, ENTRY_CLASSIFICATION)
    assert "carries Q4-900" in message
    assert "classifies no entry with that identifier" in message


def test_entry_classification_reports_an_exchanged_entry_as_two_findings() -> None:
    register = _register()
    cells = dict(_blocked_by(register))
    cells[SINGLE_GATE_SCREEN] = "`Q4-900`"

    findings = check_entry_classification(_premises(register, blocked=cells))

    messages = _messages(findings, ENTRY_CLASSIFICATION)
    assert len(messages) == 2
    assert any(_single_gate_entry().id in message for message in messages)
    assert any("Q4-900" in message for message in messages)


def test_entry_classification_rejects_a_register_key_recording_no_inventory_text() -> None:
    findings = check_entry_classification(_premises(_with_key(_entry(REGISTER_KEY))))

    (message,) = _messages(findings, ENTRY_CLASSIFICATION)
    assert f"{REGISTER_KEY!r} is a register-assigned key" in message
    assert "records no inventoryText" in message


def test_entry_classification_rejects_an_inventory_text_the_cell_does_not_carry() -> None:
    entry = _entry(REGISTER_KEY, inventory_text=REGISTER_KEY_TEXT)

    findings = check_entry_classification(_premises(_with_key(entry)))

    (message,) = _messages(findings, ENTRY_CLASSIFICATION)
    assert f"matches {REGISTER_KEY!r} on the inventoryText {REGISTER_KEY_TEXT!r}" in message
    assert "'Blocked by' cell" in message
    assert "does not carry that text" in message


def test_entry_classification_accepts_a_register_key_the_cell_states_in_prose() -> None:
    register = _with_key(_entry(REGISTER_KEY, inventory_text=REGISTER_KEY_TEXT))
    cells = dict(_blocked_by(register))
    cells[SINGLE_GATE_SCREEN] = f"{cells[SINGLE_GATE_SCREEN]}; {REGISTER_KEY_TEXT}"

    assert check_entry_classification(_premises(register, blocked=cells)) == []


def test_entry_classification_leaves_a_screen_the_register_omits_to_inventory_coverage() -> None:
    screens = [record for record in _baseline_screens() if record.id != SINGLE_GATE_SCREEN]
    rows = _baseline_rows(_register())
    premises = _premises(_register(screens), rows, _blocked_by(_register()))

    assert check_entry_classification(premises) == []
    assert _messages(check_inventory_coverage(premises), INVENTORY_COVERAGE)


# ---------------------------------------------------------------------------
# reclassifications
# ---------------------------------------------------------------------------


def _retaining_no_gate() -> str:
    """One of the four misread screens Requirement 2 criterion 11 records retaining no Gate."""
    return sorted(screen for screen, gates in RETAINED_GATES.items() if not gates)[0]


def test_reclassifications_name_a_misread_entry_the_register_does_not_record() -> None:
    gates = RETAINED_GATES[RETAINED_GATE_SCREEN]
    screens = _with_screen(
        _baseline_screens(),
        RETAINED_GATE_SCREEN,
        blocking_entries=_gate_entries(RETAINED_GATE_SCREEN, *gates),
    )

    findings = check_reclassifications(_premises(_register(screens)))

    (message,) = _messages(findings, RECLASSIFICATIONS)
    assert RECLASSIFIED_ENTRIES[RETAINED_GATE_SCREEN] in message
    assert "records no blocking entry with that identifier" in message


def test_reclassifications_reject_a_misread_entry_left_classified_as_a_gate() -> None:
    screen = RETAINED_GATE_SCREEN
    (gate,) = RETAINED_GATES[screen]
    misread = _entry(RECLASSIFIED_ENTRIES[screen], gate=gate, reclassified_from=GATE)
    screens = _with_screen(
        _baseline_screens(),
        screen,
        blocking_entries=[misread, *_gate_entries(screen, gate)],
    )

    findings = check_reclassifications(_premises(_register(screens)))

    (message,) = _messages(findings, RECLASSIFICATIONS)
    assert f"the register classifies {misread.id} as {GATE}" in message
    assert f"reclassified as an {ACCEPTANCE_CRITERION}" in message


def test_reclassifications_reject_an_entry_recording_no_reclassification() -> None:
    screen = RETAINED_GATE_SCREEN
    screens = _with_screen(
        _baseline_screens(),
        screen,
        blocking_entries=[
            _entry(RECLASSIFIED_ENTRIES[screen]),
            *_gate_entries(screen, *RETAINED_GATES[screen]),
        ],
    )

    findings = check_reclassifications(_premises(_register(screens)))

    (message,) = _messages(findings, RECLASSIFICATIONS)
    assert f"records {RECLASSIFIED_ENTRIES[screen]} as misread as a {GATE}" in message
    assert "records no reclassification for it" in message


def test_reclassifications_name_a_screen_that_gained_a_gate() -> None:
    screen = _retaining_no_gate()
    screens = _with_screen(
        _baseline_screens(),
        screen,
        blocking_entries=[_reclassified(screen), *_gate_entries(screen, UNUSED_GATE)],
        released=False,
    )

    findings = check_reclassifications(_premises(_register(screens)))

    (message,) = _messages(findings, RECLASSIFICATIONS)
    assert f"records it retaining no entry classified as a {GATE}" in message
    assert UNUSED_GATE in message


def test_reclassifications_name_the_retained_gate_a_screen_lost() -> None:
    screen = RETAINED_GATE_SCREEN
    (gate,) = RETAINED_GATES[screen]
    screens = _with_screen(
        _baseline_screens(), screen, blocking_entries=[_reclassified(screen)], released=True
    )

    findings = check_reclassifications(_premises(_register(screens)))

    (message,) = _messages(findings, RECLASSIFICATIONS)
    assert f"records it retaining one entry classified as a {GATE}, naming {gate}" in message
    assert f"its blocking list retains no entry classified as a {GATE}" in message


# ---------------------------------------------------------------------------
# inventory-totals
#
# Named with the `classification` prefix of the two checks above, because the three are implemented
# and selected as one pass and the other two carry the word in the name of the check itself.
# ---------------------------------------------------------------------------

# A count no fixture row count can equal, so a rewritten declaration disagrees with the rows
# whatever the fixture's size.
DISAGREEING_COUNT = 99


def _rewritten(old: str, new: str) -> Premises:
    """Premises over the baseline register and an inventory with one declaration rewritten.

    The replaced text must be unique, so a test that stops rewriting the site it names fails here
    rather than passing against an unchanged document.
    """
    register = _register()
    text = _inventory_text(_baseline_rows(register), _blocked_by(register))
    assert text.count(old) == 1, old
    return _premises_from_text(register, text.replace(old, new))


def _with_totals_count(label: str, count: int) -> Premises:
    """Premises over an inventory whose Inventory totals row *label* declares *count*."""
    register = _register()
    text = _inventory_text(_baseline_rows(register), _blocked_by(register))
    pattern = re.compile(rf"^(\|[^|]*{re.escape(label)}[^|]*\|)[^|]*(\|)", re.MULTILINE)
    rewritten, replaced = pattern.subn(rf"\1 {count} \2", text)
    assert replaced == 1, label
    return _premises_from_text(register, rewritten)


def _row_count() -> int:
    """The fixture inventory's own row count, which every declaration in it is derived from."""
    return len(_baseline_rows(_register()))


def test_classification_inventory_totals_reports_nothing_against_the_real_inventory() -> None:
    """Task 1.2's regression guard, and the only evidence for this check outside a fixture.

    The real inventory's thirteen headings, lead lines, totals table and undelivered paragraph were
    corrected to its own rows, so every site must agree. Nothing here states what the counts are:
    the check derives each from the rows and so does the document.
    """
    register = _register()
    text = (REPO_ROOT / INVENTORY_PATH).read_text(encoding="utf-8")

    assert check_inventory_totals(_premises_from_text(register, text)) == []


def test_classification_inventory_totals_names_a_disagreeing_area_heading() -> None:
    heading = f"### A fixture area -- {_row_count()} screens"
    premises = _rewritten(heading, f"### A fixture area -- {DISAGREEING_COUNT} screens")

    (message,) = _messages(check_inventory_totals(premises), INVENTORY_TOTALS)
    assert "the area heading" in message
    assert f"declares {DISAGREEING_COUNT}, and its own rows give {_row_count()}" in message
    assert f"{INVENTORY_OWNER} owns the inventory" in message


def test_classification_inventory_totals_names_a_disagreeing_screen_count() -> None:
    premises = _rewritten(
        f"{_row_count()} screens across 1 area", f"{DISAGREEING_COUNT} screens across 1 area"
    )

    (message,) = _messages(check_inventory_totals(premises), INVENTORY_TOTALS)
    assert "the section 8 lead line's screen count" in message
    assert f"declares {DISAGREEING_COUNT}, and the area tables' rows give {_row_count()}" in message


def test_classification_inventory_totals_names_a_disagreeing_area_count() -> None:
    premises = _rewritten("screens across 1 area.", f"screens across {DISAGREEING_COUNT} areas.")

    (message,) = _messages(check_inventory_totals(premises), INVENTORY_TOTALS)
    assert "the section 8 lead line's area count" in message
    assert f"declares {DISAGREEING_COUNT}, and the area headings give 1" in message


@pytest.mark.parametrize(
    "label",
    [VERIFIED_STATUS, SUPERSEDED_STATUS, NOT_DELIVERED_LABEL, TOTAL_LABEL],
    ids=["verified", "superseded", "not-delivered", "total"],
)
def test_classification_inventory_totals_names_a_disagreeing_totals_row(label: str) -> None:
    premises = _with_totals_count(label, DISAGREEING_COUNT)

    (message,) = _messages(check_inventory_totals(premises), INVENTORY_TOTALS)
    assert f"the Inventory totals table's {label!r} row declares {DISAGREEING_COUNT}" in message
    assert f"{INVENTORY_OWNER} owns the inventory" in message


def test_classification_inventory_totals_names_a_disagreeing_undelivered_paragraph() -> None:
    undelivered = _row_count() - len(DELIVERED_SCREENS)
    premises = _rewritten(
        f"The {undelivered} undelivered screens",
        f"The {DISAGREEING_COUNT} undelivered screens",
    )

    (message,) = _messages(check_inventory_totals(premises), INVENTORY_TOTALS)
    assert "the undelivered-screens paragraph" in message
    assert f"declares {DISAGREEING_COUNT}" in message
    assert f"the rows the inventory records undelivered give {undelivered}" in message


def test_classification_inventory_totals_names_a_disagreeing_entry_count() -> None:
    premises = _rewritten(
        f"The screen inventory has {_row_count()} entries",
        f"The screen inventory has {DISAGREEING_COUNT} entries",
    )

    (message,) = _messages(check_inventory_totals(premises), INVENTORY_TOTALS)
    assert "the section 9 lead line's entry count" in message
    assert f"declares {DISAGREEING_COUNT}, and the area tables' rows give {_row_count()}" in message


def test_classification_inventory_totals_reports_a_declaration_it_cannot_read() -> None:
    """A reworded declaration stops being compared, and that is the finding."""
    premises = _rewritten(
        f"The screen inventory has {_row_count()} entries",
        "The screen inventory has been reworded",
    )

    (message,) = _messages(check_inventory_totals(premises), INVENTORY_TOTALS)
    assert (
        "the section 9 lead line's entry count declares no count this check could read" in message
    )
    assert "is not a declaration that agrees" in message


def test_classification_inventory_totals_reports_every_row_of_a_table_it_cannot_find() -> None:
    """A renamed header column stops four declarations being compared, and each is a finding."""
    premises = _rewritten("| Status | Count | Screens |", "| State | Count | Screens |")

    messages = _messages(check_inventory_totals(premises), INVENTORY_TOTALS)
    assert len(messages) == 4
    for label in (VERIFIED_STATUS, SUPERSEDED_STATUS, NOT_DELIVERED_LABEL, TOTAL_LABEL):
        assert any(
            f"the Inventory totals table's {label!r} row declares no count" in message
            for message in messages
        )


# ---------------------------------------------------------------------------
# closure-completeness, and the Closure_Record half of the premises it rests on
#
# Named with the `settlement` prefix for the reason the inventory-totals tests carry
# `classification`: the three settlement shapes and the record loading underneath them are
# implemented and selected as one pass. The loading tests sit here rather than with the rest of the
# error contract for the same reason.
#
# `closures/` does not exist and no Closure_Record is authored by this spec, because the content of
# one is an owner's signature. Every record below is therefore a fixture in this file, and every
# value in one is obviously a fixture: the register records element *names*, and a fixture value
# that read like a real Contract_Value would outlive the decision it stood in for.
# ---------------------------------------------------------------------------

# The parts a settlement test records against UNUSED_GATE, which no fixture screen names: accepting
# a part of it releases no screen, so every set check reads the baseline unchanged.
SETTLED_PART = "a-fixture-part"
SECOND_PART = "another-fixture-part"

# One element name per shape, plus the element a `keySource` resolves against and two names nothing
# records.
FIXTURE_ELEMENT = "a fixture single element"
FIXTURE_KEY_SOURCE_ELEMENT = "a fixture key-source element"
FIXTURE_KEYED_ELEMENT = "a fixture keyed element"
FIXTURE_MAPPED_ELEMENT = "a fixture mapped element"
UNRECORDED_ELEMENT = "a fixture element no part records"
UNRESOLVABLE_KEY_SOURCE = "a fixture key source the register records no element for"

FIXTURE_ATTRIBUTES = ("aFixtureAttribute", "anotherFixtureAttribute")
FIXTURE_KEYS = ("a-fixture-key", "another-fixture-key")
FIXTURE_MEMBERS = ("a-fixture-member", "another-fixture-member")
FIXTURE_VALUE = "a fixture settled value"
FIXTURE_SOURCE = "a fixture issued document"
EXCLUDED_SOURCE = "a fixture source the Gate excludes"

FIXTURE_DATE = "2001-02-03"
FIXTURE_COMMIT = "0" * 40


def _record_path(gate: str, part: str) -> str:
    """Where a Closure_Record lives: one file per part, under the spec directory.

    Derived from the register's own path, so no test here invents a second place for a record to
    live. The path is only ever a string the register records and this check reads back.
    """
    return f"{PurePosixPath(REGISTER_PATH).parent}/closures/{gate}.{part}.json"


def _content(
    name: str,
    shape: str,
    *,
    required: Sequence[str] = (),
    key_source: str | None = None,
    excluded_source: str | None = None,
) -> DecisionContentElement:
    """One Decision_Content element, named rather than valued, in the shape the dossier records."""
    return DecisionContentElement(
        name=name,
        source=None,
        settlement_shape=shape,
        required_attributes=list(required) or None,
        key_source=key_source,
        excluded_source=excluded_source,
    )


def _settled_entry(
    key: str | None = None,
    value: object = ABSENT,
    source: str | None = None,
    exception: bool | None = None,
) -> SettledEntry:
    """One settled entry. The default carries no value, which is what an exception entry is."""
    return SettledEntry(key=key, value=value, source=source, exception=exception)


def _settled_element(element: str, shape: str, *entries: SettledEntry) -> SettledElement:
    return SettledElement(element=element, shape=shape, entries=list(entries))


def _carrying(*attributes: str) -> dict[str, str]:
    """A settled value carrying *attributes*, which is what "carries" can mean to the check.

    An object whose keys are the attribute names. The check can see the names and nothing inside
    them, which is why every value here is the same fixture string.
    """
    return dict.fromkeys(attributes, FIXTURE_VALUE)


def _elements() -> list[DecisionContentElement]:
    """One element of each of the three shapes, plus the element both key sources resolve against.

    The keyed element and the mapped element both name the key-source element, so one settled
    element supplies the key set of the first and the second set of the relation of the second.
    """
    return [
        _content(FIXTURE_ELEMENT, SINGLE_SHAPE, required=FIXTURE_ATTRIBUTES),
        _content(FIXTURE_KEY_SOURCE_ELEMENT, KEYED_SHAPE),
        _content(
            FIXTURE_KEYED_ELEMENT,
            KEYED_SHAPE,
            required=FIXTURE_ATTRIBUTES,
            key_source=FIXTURE_KEY_SOURCE_ELEMENT,
        ),
        _content(FIXTURE_MAPPED_ELEMENT, MAPPING_SHAPE, key_source=FIXTURE_KEY_SOURCE_ELEMENT),
    ]


def _settled_elements() -> list[SettledElement]:
    """The settlement that satisfies all three shapes, which is what the accepting case records.

    The mapped element relates one member of its first set to one member of its second, which makes
    the relation total in both directions: both members relate to something, and both keys of the
    key-source element are related to by something.
    """
    return [
        _settled_element(
            FIXTURE_ELEMENT,
            SINGLE_SHAPE,
            _settled_entry(value=_carrying(*FIXTURE_ATTRIBUTES), source=FIXTURE_SOURCE),
        ),
        _settled_element(
            FIXTURE_KEY_SOURCE_ELEMENT,
            KEYED_SHAPE,
            *(_settled_entry(key=key, value=FIXTURE_VALUE) for key in FIXTURE_KEYS),
        ),
        _settled_element(
            FIXTURE_KEYED_ELEMENT,
            KEYED_SHAPE,
            *(
                _settled_entry(key=key, value=_carrying(*FIXTURE_ATTRIBUTES))
                for key in FIXTURE_KEYS
            ),
        ),
        _settled_element(
            FIXTURE_MAPPED_ELEMENT,
            MAPPING_SHAPE,
            *(
                _settled_entry(key=member, value=key)
                for member, key in zip(FIXTURE_MEMBERS, FIXTURE_KEYS, strict=True)
            ),
        ),
    ]


def _resettled(element: str, *entries: SettledEntry) -> list[SettledElement]:
    """The accepting settlement with one element's entries replaced by *entries*."""
    return [
        replace(item, entries=list(entries)) if item.element == element else item
        for item in _settled_elements()
    ]


def _recontented(name: str, element: DecisionContentElement) -> list[DecisionContentElement]:
    """The accepting Decision_Content with the element named *name* replaced."""
    return [element if item.name == name else item for item in _elements()]


def _gate_part(
    part: str, elements: Sequence[DecisionContentElement] = (), *, state: str = "accepted"
) -> GatePart:
    """A Gate part recording *elements* and naming the Closure_Record that settles it."""
    return GatePart(
        id=part,
        signing_owners=["Product"],
        state=state,
        closure_record=_record_path(UNUSED_GATE, part),
        elements=list(elements),
        sign_off_conditions=[],
    )


def _closure_record(
    settled: Sequence[SettledElement] = (),
    *,
    gate: str = UNUSED_GATE,
    part: str = SETTLED_PART,
    state: str = "accepted",
    permissions: Sequence[GuardPermission] = (),
) -> ClosureRecord:
    """One part's Closure_Record. Every field a signature settles is a fixture.

    *permissions* is the verdict the acceptance records for each guard the registry maps to the
    part. It defaults to none, which is what a part of a Gate no guard rests on records.
    """
    return ClosureRecord(
        schema_ref=None,
        gate=gate,
        part=part,
        state=state,
        sign_off_date=FIXTURE_DATE,
        entry_commit=FIXTURE_COMMIT if state == "accepted" else None,
        signatures=[
            Signature(owner="Product", date=FIXTURE_DATE, trailer=None, signature_verified=None)
        ],
        sign_off_condition_verdicts=[],
        guard_permissions=list(permissions),
        settled=list(settled),
        amendments=None,
    )


def _settling_register(*parts: GatePart) -> GateRegister:
    """The baseline register with UNUSED_GATE's dossier recording *parts* instead of its open one.

    The Gate's recorded status follows its parts, so the derivations read a Gate that is closed
    exactly when every part of it is accepted.
    """
    register = _register()
    closed = all(part.state == "accepted" for part in parts)
    dossier = replace(
        next(item for item in register.gates if item.id == UNUSED_GATE),
        status="CLOSED" if closed else "OPEN",
        parts=list(parts),
    )
    return replace(
        register, gates=[dossier if item.id == UNUSED_GATE else item for item in register.gates]
    )


def _closures(
    *records: ClosureRecord, add_commit: AddCommit | None = None
) -> dict[str, NamedClosureRecord]:
    """Each record attached to the part that names it, the way load_premises attaches them.

    *add_commit* is the resolution the premises carry for each record. It defaults to none, which
    is what `load_premises` carries for a record that is not `accepted`: no commit is resolved for
    a record the register says is not in the repository yet. A test over an accepted record passes
    the resolution it wants read, which is what lets the recorded-against-resolved rule be checked
    without a repository.
    """
    return {
        f"{record.gate}:{record.part}": NamedClosureRecord(
            gate=record.gate,
            part=record.part,
            path=_record_path(record.gate, record.part),
            record=record,
            add_commit=add_commit,
        )
        for record in records
    }


def _settlement(
    settled: Sequence[SettledElement] | None = None,
    elements: Sequence[DecisionContentElement] | None = None,
) -> Premises:
    """Premises over the baseline plus one submitted part of UNUSED_GATE and the record settling it.

    Both axes default to the accepting case, so a test rewrites one element of the dossier or one
    element's entries in the record and nothing else moves.

    `submitted` rather than `accepted`, which is Requirement 3 criterion 3's own state and the one
    that keeps this fixture honest: an accepted record asserts it is in the repository, and the
    record these premises name is in none -- they are built in code against the repository root, so
    no commit adds the path they record. The checks that read git history therefore belong to the
    tests that build a repository, and the accepted case is asserted there.
    """
    part = _gate_part(
        SETTLED_PART, _elements() if elements is None else elements, state="submitted"
    )
    record = _closure_record(_settled_elements() if settled is None else settled, state="submitted")
    return _premises(_settling_register(part), closures=_closures(record))


def _settlement_messages(premises: Premises) -> list[str]:
    return _messages(check_closure_completeness(premises), CLOSURE_COMPLETENESS)


# ---------------------------------------------------------------------------
# The accepting case
# ---------------------------------------------------------------------------


def test_settlement_accepts_a_record_settling_every_element_by_its_recorded_shape() -> None:
    """The baseline for every test below, and the evidence that no other check is disturbed."""
    assert run_checks(_settlement()) == []


def test_settlement_reads_no_part_recorded_open() -> None:
    """A part nobody has signed names no record, and there is nothing to read one against."""
    part = _gate_part(SETTLED_PART, _elements(), state="open")
    register = _settling_register(replace(part, closure_record=None))

    assert check_closure_completeness(_premises(register)) == []


def test_settlement_names_a_part_recorded_submitted_that_names_no_record() -> None:
    part = _gate_part(SETTLED_PART, _elements(), state="submitted")
    register = _settling_register(replace(part, closure_record=None))

    (message,) = _settlement_messages(_premises(register))
    assert f"{UNUSED_GATE}:{SETTLED_PART}: the part is recorded submitted" in message
    assert "names no Closure_Record" in message


# ---------------------------------------------------------------------------
# The omitted element of Requirement 3 criterion 3
# ---------------------------------------------------------------------------


def test_settlement_names_the_element_the_record_omits() -> None:
    settled = [item for item in _settled_elements() if item.element != FIXTURE_ELEMENT]

    (message,) = _settlement_messages(_settlement(settled))
    assert f"{UNUSED_GATE}:{SETTLED_PART}: the record settles no entry for the element " in message
    assert FIXTURE_ELEMENT in message
    assert "the Gate stays OPEN" in message


def test_settlement_names_an_element_the_record_settles_and_the_dossier_does_not() -> None:
    settled = [
        *_settled_elements(),
        _settled_element(UNRECORDED_ELEMENT, SINGLE_SHAPE, _settled_entry(value=FIXTURE_VALUE)),
    ]

    (message,) = _settlement_messages(_settlement(settled))
    assert f"the record settles the element {UNRECORDED_ELEMENT!r}" in message
    assert "records no Decision_Content element of that name for the part" in message


def test_settlement_names_an_element_the_record_settles_twice() -> None:
    settled = [*_settled_elements(), _settled_elements()[0]]

    (message,) = _settlement_messages(_settlement(settled))
    assert f"the record settles the element {FIXTURE_ELEMENT!r} more than once" in message
    assert "more than one set of entries to read" in message


def test_settlement_names_a_shape_the_dossier_and_the_record_disagree_on() -> None:
    settled = [
        replace(item, shape=KEYED_SHAPE, entries=[_settled_entry(key=FIXTURE_KEYS[0])])
        if item.element == FIXTURE_ELEMENT
        else item
        for item in _settled_elements()
    ]

    (message,) = _settlement_messages(_settlement(settled))
    assert f"records the element {FIXTURE_ELEMENT!r} as {SINGLE_SHAPE}-shaped" in message
    assert f"settles it as {KEYED_SHAPE}-shaped" in message
    assert "until the two agree" in message


# ---------------------------------------------------------------------------
# single
# ---------------------------------------------------------------------------


def test_settlement_single_names_an_element_settled_by_no_entry() -> None:
    (message,) = _settlement_messages(_settlement(_resettled(FIXTURE_ELEMENT)))
    assert f"the element {FIXTURE_ELEMENT!r} is recorded {SINGLE_SHAPE}-shaped" in message
    assert "settles it with no entry" in message


def test_settlement_single_names_an_element_settled_by_two_entries() -> None:
    settled = _resettled(
        FIXTURE_ELEMENT,
        _settled_entry(value=_carrying(*FIXTURE_ATTRIBUTES)),
        _settled_entry(value=_carrying(*FIXTURE_ATTRIBUTES)),
    )

    (message,) = _settlement_messages(_settlement(settled))
    assert "settles it with 2 entries" in message
    assert "2 answers for one element" in message


def test_settlement_single_names_the_one_attribute_the_entry_omits() -> None:
    carried, omitted = FIXTURE_ATTRIBUTES
    settled = _resettled(FIXTURE_ELEMENT, _settled_entry(value=_carrying(carried)))

    (message,) = _settlement_messages(_settlement(settled))
    assert f"records the required attribute {omitted!r}" in message
    assert f"value names {carried}" in message


def test_settlement_single_names_each_of_two_omitted_attributes_separately() -> None:
    settled = _resettled(FIXTURE_ELEMENT, _settled_entry(value={}))

    messages = _settlement_messages(_settlement(settled))
    assert len(messages) == len(FIXTURE_ATTRIBUTES)
    for attribute in FIXTURE_ATTRIBUTES:
        assert any(f"required attribute {attribute!r}" in message for message in messages)
        assert any("names no attribute" in message for message in messages)


def test_settlement_single_reads_an_attribute_settled_as_null_as_omitted() -> None:
    """Null and a placeholder are indistinguishable, and Requirement 4 criterion 10 leaves an
    unsettled value absent rather than carrying either in its place."""
    carried, omitted = FIXTURE_ATTRIBUTES
    value = {**_carrying(carried), omitted: None}
    settled = _resettled(FIXTURE_ELEMENT, _settled_entry(value=value))

    (message,) = _settlement_messages(_settlement(settled))
    assert f"records the required attribute {omitted!r}" in message


def test_settlement_single_names_the_type_of_a_value_that_is_not_an_object() -> None:
    settled = _resettled(FIXTURE_ELEMENT, _settled_entry(value=FIXTURE_VALUE))

    messages = _settlement_messages(_settlement(settled))
    assert len(messages) == len(FIXTURE_ATTRIBUTES)
    assert all("value is a string rather than an object" in message for message in messages)


# ---------------------------------------------------------------------------
# keyed
# ---------------------------------------------------------------------------


def test_settlement_keyed_names_a_key_the_record_settles_with_no_entry() -> None:
    kept, missing = FIXTURE_KEYS
    settled = _resettled(
        FIXTURE_KEYED_ELEMENT,
        _settled_entry(key=kept, value=_carrying(*FIXTURE_ATTRIBUTES)),
    )

    (message,) = _settlement_messages(_settlement(settled))
    assert f"the key {missing!r} of the element {FIXTURE_KEYED_ELEMENT!r}" in message
    assert "is settled by no entry" in message
    assert f"the keySource {FIXTURE_KEY_SOURCE_ELEMENT!r} carries that key" in message


def test_settlement_keyed_names_a_key_the_record_settles_twice() -> None:
    duplicated = FIXTURE_KEYS[0]
    settled = _resettled(
        FIXTURE_KEYED_ELEMENT,
        *(
            _settled_entry(key=key, value=_carrying(*FIXTURE_ATTRIBUTES))
            for key in (*FIXTURE_KEYS, duplicated)
        ),
    )

    (message,) = _settlement_messages(_settlement(settled))
    assert f"the key {duplicated!r} of the element {FIXTURE_KEYED_ELEMENT!r}" in message
    assert "is settled by 2 entries" in message
    assert "exactly one entry or one explicit exception settles a key" in message


def test_settlement_keyed_accepts_a_key_settled_by_an_explicit_exception() -> None:
    """Requirement 12 criterion 5's "or records an explicit exception" is a pass, not a gap."""
    valued, excepted = FIXTURE_KEYS
    settled = _resettled(
        FIXTURE_KEYED_ELEMENT,
        _settled_entry(key=valued, value=_carrying(*FIXTURE_ATTRIBUTES)),
        _settled_entry(key=excepted, exception=True),
    )

    assert check_closure_completeness(_settlement(settled)) == []


def test_settlement_keyed_names_the_key_of_an_entry_omitting_an_attribute() -> None:
    carried, omitted = FIXTURE_ATTRIBUTES
    first, second = FIXTURE_KEYS
    settled = _resettled(
        FIXTURE_KEYED_ELEMENT,
        _settled_entry(key=first, value=_carrying(*FIXTURE_ATTRIBUTES)),
        _settled_entry(key=second, value=_carrying(carried)),
    )

    (message,) = _settlement_messages(_settlement(settled))
    assert f"the key {second!r} of the element {FIXTURE_KEYED_ELEMENT!r}" in message
    assert f"records the required attribute {omitted!r}" in message


def test_settlement_keyed_names_a_key_the_key_set_does_not_name() -> None:
    unnamed = "a-fixture-key-no-source-names"
    settled = _resettled(
        FIXTURE_KEYED_ELEMENT,
        *(
            _settled_entry(key=key, value=_carrying(*FIXTURE_ATTRIBUTES))
            for key in (*FIXTURE_KEYS, unnamed)
        ),
    )

    (message,) = _settlement_messages(_settlement(settled))
    assert f"the record settles the key {unnamed!r}" in message
    assert f"the keySource {FIXTURE_KEY_SOURCE_ELEMENT!r} does not carry that key" in message


def test_settlement_keyed_names_a_key_source_the_check_cannot_resolve() -> None:
    """An unresolved premise is a failure, never a pass: a key set the check cannot establish would
    leave every per-key rule over it passing over nothing."""
    elements = _recontented(
        FIXTURE_KEYED_ELEMENT,
        _content(
            FIXTURE_KEYED_ELEMENT,
            KEYED_SHAPE,
            required=FIXTURE_ATTRIBUTES,
            key_source=UNRESOLVABLE_KEY_SOURCE,
        ),
    )

    (message,) = _settlement_messages(_settlement(elements=elements))
    assert f"names the keySource {UNRESOLVABLE_KEY_SOURCE!r}" in message
    assert "records no Decision_Content element with that name" in message
    assert "the key set could not be established" in message


def test_settlement_keyed_rejects_an_element_naming_itself_as_its_key_source() -> None:
    """A self-reference reads the keys the record already carries, which every entry satisfies."""
    elements = _recontented(
        FIXTURE_KEYED_ELEMENT,
        _content(
            FIXTURE_KEYED_ELEMENT,
            KEYED_SHAPE,
            required=FIXTURE_ATTRIBUTES,
            key_source=FIXTURE_KEYED_ELEMENT,
        ),
    )

    (message,) = _settlement_messages(_settlement(elements=elements))
    assert f"names the keySource {FIXTURE_KEYED_ELEMENT!r}" in message
    assert "that is the element's own name" in message
    assert "would pass over nothing" in message


def test_settlement_keyed_names_a_key_source_whose_part_names_no_record() -> None:
    """The key-source element moves to a part nobody has signed, so its keys are unknown."""
    elements = _elements()
    source = next(item for item in elements if item.name == FIXTURE_KEY_SOURCE_ELEMENT)
    settling = _gate_part(
        SETTLED_PART, [item for item in elements if item.name != FIXTURE_KEY_SOURCE_ELEMENT]
    )
    unsigned = replace(_gate_part(SECOND_PART, [source], state="open"), closure_record=None)
    settled = [item for item in _settled_elements() if item.element != FIXTURE_KEY_SOURCE_ELEMENT]
    record = _closure_record(settled)
    premises = _premises(_settling_register(settling, unsigned), closures=_closures(record))

    messages = _settlement_messages(premises)
    assert len(messages) == 2
    assert all(
        f"names the keySource {FIXTURE_KEY_SOURCE_ELEMENT!r}" in message for message in messages
    )
    assert all(
        f"{UNUSED_GATE}:{SECOND_PART}, which settles that element, names no Closure_Record"
        in message
        for message in messages
    )


def test_settlement_keyed_names_an_element_enumerating_no_key_and_naming_no_source() -> None:
    """The key-source element enumerates its own keys, so settling none empties both key sets."""
    settled = _resettled(FIXTURE_KEY_SOURCE_ELEMENT)

    messages = _settlement_messages(_settlement(settled))
    assert any(
        f"the element {FIXTURE_KEY_SOURCE_ELEMENT!r} records no keySource and the record "
        f"enumerates no key" in message
        for message in messages
    )
    assert all("would pass over nothing" in message for message in messages)


# ---------------------------------------------------------------------------
# mapping
# ---------------------------------------------------------------------------


def test_settlement_mapping_names_a_member_of_the_first_set_relating_to_nothing() -> None:
    """Total in the second direction only: every key of the second set is covered, and one member of
    the first set relates to nothing."""
    unmapped, mapped = FIXTURE_MEMBERS
    settled = _resettled(
        FIXTURE_MAPPED_ELEMENT,
        _settled_entry(key=unmapped, value=[]),
        _settled_entry(key=mapped, value=list(FIXTURE_KEYS)),
    )

    (message,) = _settlement_messages(_settlement(settled))
    assert f"the member {unmapped!r} of the keys the record settles relates to no member" in message
    assert f"the key set of the keySource {FIXTURE_KEY_SOURCE_ELEMENT!r}" in message
    assert FIXTURE_MAPPED_ELEMENT in message


def test_settlement_mapping_names_a_member_of_the_second_set_nothing_relates_to() -> None:
    """Total in the first direction only: every member of the first set relates to something, and
    one member of the second set is related to by nothing."""
    related, unrelated = FIXTURE_KEYS
    settled = _resettled(
        FIXTURE_MAPPED_ELEMENT,
        *(_settled_entry(key=member, value=related) for member in FIXTURE_MEMBERS),
    )

    (message,) = _settlement_messages(_settlement(settled))
    assert f"the member {unrelated!r} of the key set of the keySource " in message
    assert FIXTURE_KEY_SOURCE_ELEMENT in message
    assert "is related to by no member of the keys the record settles" in message
    assert FIXTURE_MAPPED_ELEMENT in message


def test_settlement_mapping_names_a_related_member_the_second_set_does_not_name() -> None:
    unnamed = "a-fixture-member-no-source-names"
    settled = _resettled(
        FIXTURE_MAPPED_ELEMENT,
        _settled_entry(key=FIXTURE_MEMBERS[0], value=[*FIXTURE_KEYS, unnamed]),
        _settled_entry(key=FIXTURE_MEMBERS[1], value=FIXTURE_KEYS[0]),
    )

    (message,) = _settlement_messages(_settlement(settled))
    assert f"relates a key of the element {FIXTURE_MAPPED_ELEMENT!r} to {unnamed!r}" in message
    assert "does not carry that member" in message


def test_settlement_mapping_names_an_element_recording_no_second_set() -> None:
    elements = _recontented(FIXTURE_MAPPED_ELEMENT, _content(FIXTURE_MAPPED_ELEMENT, MAPPING_SHAPE))

    (message,) = _settlement_messages(_settlement(elements=elements))
    assert f"the element {FIXTURE_MAPPED_ELEMENT!r} is recorded {MAPPING_SHAPE}-shaped" in message
    assert "records no keySource" in message
    assert "the direction over it would pass over nothing" in message


def test_settlement_mapping_names_the_type_of_a_value_naming_no_member() -> None:
    settled = _resettled(
        FIXTURE_MAPPED_ELEMENT,
        _settled_entry(key=FIXTURE_MEMBERS[0], value=list(FIXTURE_KEYS)),
        _settled_entry(key=FIXTURE_MEMBERS[1], value={"aFixtureKey": FIXTURE_VALUE}),
    )

    (message,) = _settlement_messages(_settlement(settled))
    assert f"the key {FIXTURE_MEMBERS[1]!r} of the element {FIXTURE_MAPPED_ELEMENT!r}" in message
    assert "whose value is an object" in message
    assert f"a {MAPPING_SHAPE} entry relates its key to one member" in message


# ---------------------------------------------------------------------------
# The excluded source
# ---------------------------------------------------------------------------


def test_settlement_rejects_an_entry_declaring_the_excluded_source() -> None:
    elements = _recontented(
        FIXTURE_ELEMENT,
        _content(
            FIXTURE_ELEMENT,
            SINGLE_SHAPE,
            required=FIXTURE_ATTRIBUTES,
            excluded_source=EXCLUDED_SOURCE,
        ),
    )
    settled = _resettled(
        FIXTURE_ELEMENT,
        _settled_entry(value=_carrying(*FIXTURE_ATTRIBUTES), source=EXCLUDED_SOURCE),
    )

    (message,) = _settlement_messages(_settlement(settled, elements))
    assert f"the element {FIXTURE_ELEMENT!r} is settled by an entry traced to " in message
    assert EXCLUDED_SOURCE in message
    assert "may not be derived from" in message


def test_settlement_accepts_an_entry_declaring_any_other_source() -> None:
    elements = _recontented(
        FIXTURE_ELEMENT,
        _content(
            FIXTURE_ELEMENT,
            SINGLE_SHAPE,
            required=FIXTURE_ATTRIBUTES,
            excluded_source=EXCLUDED_SOURCE,
        ),
    )

    assert check_closure_completeness(_settlement(elements=elements)) == []


# ---------------------------------------------------------------------------
# The records the premises load
# ---------------------------------------------------------------------------


def _settling_repository(
    root: Path,
    *parts: GatePart,
    closures: Mapping[str, object | str] | None = None,
) -> Path:
    """A repository root whose UNUSED_GATE records *parts*, carrying the records *closures* has."""
    register = _settling_register(*parts)
    return _repository(
        root,
        register=dump_gate_register(register),
        inventory=_inventory_text(_baseline_rows(register), _blocked_by(register)),
        closures=closures,
    )


def test_settlement_premises_load_the_record_each_part_names(tmp_path: Path) -> None:
    """The whole path, from the register naming a record to every check running over it.

    The record is `submitted`, for the reason :func:`_settlement` builds a submitted one: the tree
    here carries no repository, so an accepted record -- which asserts it is in one -- is the
    signature section's case to assert against a repository it builds.
    """
    part = _gate_part(SETTLED_PART, _elements(), state="submitted")
    record = _closure_record(_settled_elements(), state="submitted")
    root = _settling_repository(
        tmp_path,
        part,
        closures={_record_path(UNUSED_GATE, SETTLED_PART): dump_closure_record(record)},
    )

    premises = load_premises(root)

    assert not isinstance(premises, list), premises
    assert set(premises.closures) == {f"{UNUSED_GATE}:{SETTLED_PART}"}
    assert run_checks(premises) == []


def test_settlement_premises_fail_naming_a_record_the_tree_lacks(tmp_path: Path) -> None:
    root = _settling_repository(tmp_path, _gate_part(SETTLED_PART, _elements()))

    (finding,) = _findings(load_premises(root))

    assert finding.check == PREMISES
    assert finding.message == f"{_record_path(UNUSED_GATE, SETTLED_PART)} does not exist"


def test_settlement_premises_fail_naming_the_line_of_a_record_that_is_not_json(
    tmp_path: Path,
) -> None:
    path = _record_path(UNUSED_GATE, SETTLED_PART)
    root = _settling_repository(
        tmp_path, _gate_part(SETTLED_PART, _elements()), closures={path: '{\n  "gate": ,\n}\n'}
    )

    (finding,) = _findings(load_premises(root))

    assert finding.check == PREMISES
    assert finding.message.startswith(f"{path}:2:11:")


def test_settlement_premises_fail_with_a_json_pointer_per_record_schema_error(
    tmp_path: Path,
) -> None:
    path = _record_path(UNUSED_GATE, SETTLED_PART)
    root = _settling_repository(
        tmp_path, _gate_part(SETTLED_PART, _elements()), closures={path: {"gate": UNUSED_GATE}}
    )

    findings = _findings(load_premises(root))

    assert {finding.check for finding in findings} == {SCHEMA}
    assert all(finding.message.startswith(f"{path}: ") for finding in findings)
    assert any("'settled' is a required property" in finding.message for finding in findings)


def test_settlement_premises_fail_naming_a_record_that_settles_another_part(
    tmp_path: Path,
) -> None:
    path = _record_path(UNUSED_GATE, SETTLED_PART)
    record = _closure_record(_settled_elements(), part=SECOND_PART)
    root = _settling_repository(
        tmp_path,
        _gate_part(SETTLED_PART, _elements()),
        closures={path: dump_closure_record(record)},
    )

    (finding,) = _findings(load_premises(root))

    assert finding.check == PREMISES
    assert f"{path} records that it settles {UNUSED_GATE}:{SECOND_PART}" in finding.message
    assert f"{UNUSED_GATE}:{SETTLED_PART} names it" in finding.message


def test_settlement_premises_fail_naming_two_parts_that_name_one_record(tmp_path: Path) -> None:
    path = _record_path(UNUSED_GATE, SETTLED_PART)
    record = _closure_record(_settled_elements())
    root = _settling_repository(
        tmp_path,
        _gate_part(SETTLED_PART, _elements()),
        replace(_gate_part(SECOND_PART), closure_record=path),
        closures={path: dump_closure_record(record)},
    )

    (finding,) = _findings(load_premises(root))

    assert finding.check == PREMISES
    assert f"{UNUSED_GATE}:{SECOND_PART} names {path}" in finding.message
    assert f"so does {UNUSED_GATE}:{SETTLED_PART}" in finding.message
    assert "one record settles one part" in finding.message


# ---------------------------------------------------------------------------
# closure-signatures
#
# Named with the `signature` prefix for the reason the settlement tests carry `settlement`: it is
# what this task's command selects.
#
# The row half is pure, so it runs over premises built in code. The trailer half resolves the
# commit that added the record, and nothing short of a commit answers that, so it runs over a
# repository built under `tmp_path`: one `git init` and one commit per example, and two examples.
# Those two are the only tests here that pay for a repository.
# ---------------------------------------------------------------------------

# The owner the settlement fixtures record, and two further labels. Each is verbatim from
# requirements.md by way of `OWNER_LABELS`: an owner label is not a Contract_Value, and no fixture
# here invents one. APPROVING_OWNER is Tab 1 because Requirement 14 criterion 11 names Tab 1 as the
# approver a class-X fork requires, so the omitted-owner finding below names the party that
# criterion does.
SIGNING_OWNER = "Product"
APPROVING_OWNER = "Tab 1"
SECOND_OWNER = "Firmware"

# A label the Gate_Dossier records as no owner of the part, which is the second direction of
# Requirement 3 criterion 4.
UNRECORDED_OWNER = "Tab 7"


def _signature(owner: str) -> Signature:
    """One signing owner's row. The row records no trailer: the commit carries its own."""
    return Signature(owner=owner, date=FIXTURE_DATE, trailer=None, signature_verified=None)


def _signing(
    owners: Sequence[str] = (SIGNING_OWNER,),
    signers: Sequence[str] = (SIGNING_OWNER,),
    *,
    sign_off_date: str = FIXTURE_DATE,
) -> Premises:
    """Premises over the baseline plus one submitted part recording *owners*, signed by *signers*.

    The settlement is the accepting one, so a test rewrites the owner set, the signature rows or
    the sign-off date and nothing else moves. Submitted for the reason :func:`_settlement` is
    submitted: the record these premises name is in no repository, and the trailer half reads one.
    """
    part = replace(
        _gate_part(SETTLED_PART, _elements(), state="submitted"), signing_owners=list(owners)
    )
    record = replace(
        _closure_record(_settled_elements(), state="submitted"),
        signatures=[_signature(owner) for owner in signers],
        sign_off_date=sign_off_date,
    )
    return _premises(_settling_register(part), closures=_closures(record))


def _signature_messages(premises: Premises) -> list[str]:
    return _messages(check_closure_signatures(premises), CLOSURE_SIGNATURES)


def _git(root: Path, *args: str) -> str:
    """Run git in *root* alone, which is where every setting these tests write stays."""
    return run_git(*args, cwd=root, check=True).stdout.strip()


def _git_init(root: Path) -> None:
    """Initialise a repository in *root*, writing every setting it needs in *root* alone.

    `user.name`, `user.email` and `commit.gpgsign` are written in this repository and never in the
    one under test, following the precedent `tools/tests/test_translation_placeholders.py` sets.
    """
    _git(root, "init", "-q")
    for setting, value in (
        ("user.name", "Test"),
        ("user.email", "test@example.invalid"),
        ("commit.gpgsign", "false"),
    ):
        _git(root, "config", setting, value)


def _git_commit(root: Path, message: str, *parties: str) -> str:
    """Commit what is staged in *root*, with one `Gate-Signed-By:` trailer per party."""
    trailers = "".join(f"\n\n{SIGNED_BY_KEY}: {party}" for party in parties)
    _git(root, "-c", "core.hooksPath=/dev/null", "commit", "-q", "-m", f"{message}{trailers}")
    return _git(root, "rev-parse", "HEAD")


def _signed_repository(root: Path, *parties: str) -> tuple[Path, str]:
    """A repository whose accepted Closure_Record was added by a commit naming *parties*.

    One `git init` and one commit.

    The record's ``entryCommit`` is written back after the commit that adds it, because a file
    cannot carry the hash of the commit that adds it. That is the order an owner records it in, and
    it leaves the record agreeing with the history the checks read.
    """
    path = _record_path(UNUSED_GATE, SETTLED_PART)
    record = _closure_record(_settled_elements())
    _settling_repository(
        root,
        _gate_part(SETTLED_PART, _elements()),
        closures={path: dump_closure_record(record)},
    )
    _git_init(root)
    _git(root, "add", "--all")
    sha = _git_commit(root, "Add a fixture Closure_Record", *parties)
    _write(
        root / path, json.dumps(dump_closure_record(replace(record, entry_commit=sha)), indent=2)
    )
    return root, sha


def test_signatures_accept_a_record_signed_by_every_owner_the_dossier_records() -> None:
    """Set equality holds in both directions, and no other check moves.

    The signature rows are in the opposite order to the recorded owners, because set equality is
    what Requirement 3 criterion 4 asks for and a check comparing sequences would pass this by
    accident.
    """
    premises = _signing((SIGNING_OWNER, APPROVING_OWNER), (APPROVING_OWNER, SIGNING_OWNER))

    assert run_checks(premises) == []


def test_signatures_name_the_owner_whose_signature_the_record_omits() -> None:
    """Requirement 14 criterion 11's shape: the finding names Tab 1 as the approver it requires."""
    premises = _signing((SIGNING_OWNER, APPROVING_OWNER), (SIGNING_OWNER,))

    (message,) = _signature_messages(premises)
    assert message.startswith(f"{UNUSED_GATE}:{SETTLED_PART}: ")
    assert f"the Gate_Dossier records {APPROVING_OWNER} as an owner" in message
    assert f"carries no signature from {APPROVING_OWNER}" in message


def test_signatures_name_each_of_two_omitted_owners_separately() -> None:
    """Two owners to collect are two findings: a reviewer collects them one at a time."""
    premises = _signing((SIGNING_OWNER, APPROVING_OWNER, SECOND_OWNER), (SIGNING_OWNER,))

    messages = _signature_messages(premises)
    assert len(messages) == 2
    for owner in (APPROVING_OWNER, SECOND_OWNER):
        assert any(f"carries no signature from {owner}" in message for message in messages)


def test_signatures_name_a_signatory_the_dossier_does_not_record() -> None:
    premises = _signing((SIGNING_OWNER,), (SIGNING_OWNER, UNRECORDED_OWNER))

    (message,) = _signature_messages(premises)
    assert f"carries a signature from {UNRECORDED_OWNER}" in message
    assert "does not record as an owner of the Decision_Content the record settles" in message
    assert f"It records {SIGNING_OWNER}" in message


def test_signatures_report_an_omitted_owner_and_an_unrecognised_signatory_separately() -> None:
    """Two different repairs: a signature to collect, and a signature to remove or an owner set to
    widen. One finding reporting that the two sets differ would name neither."""
    premises = _signing((SIGNING_OWNER,), (UNRECORDED_OWNER,))

    messages = _signature_messages(premises)
    assert len(messages) == 2
    assert any(f"carries no signature from {SIGNING_OWNER}" in message for message in messages)
    assert any(f"carries a signature from {UNRECORDED_OWNER}" in message for message in messages)


def test_signatures_name_a_part_recording_no_owner_rather_than_passing_it() -> None:
    """The vacuous pass: no recorded owner and no signature satisfies set equality and protects
    nothing, which is what Requirement 15 criterion 18 rejects."""
    premises = _signing(owners=(), signers=())

    (message,) = _signature_messages(premises)
    assert message.startswith(f"{UNUSED_GATE}:{SETTLED_PART}: ")
    assert "records no owner authorised to sign the part" in message
    assert "nobody is authorised to sign it rather than anybody being" in message


def test_signatures_name_a_part_recording_no_owner_that_carries_a_signature() -> None:
    """An empty owner set authorises nobody, so the signature it carries is unrecognised too."""
    premises = _signing(owners=(), signers=(SIGNING_OWNER,))

    messages = _signature_messages(premises)
    assert len(messages) == 2
    assert any("records no owner authorised to sign the part" in message for message in messages)
    assert any(f"carries a signature from {SIGNING_OWNER}" in message for message in messages)
    assert any("It records no owner" in message for message in messages)


def test_signatures_name_a_part_recording_no_owner_before_any_record_arrives() -> None:
    """Requirement 15 criterion 18's trigger is the Gate_Dossier recording no owner, not a record
    arriving, so a part nobody is authorised to sign is named while it is still open."""
    part = replace(_gate_part(SETTLED_PART, state="open"), signing_owners=[], closure_record=None)

    (message,) = _signature_messages(_premises(_settling_register(part)))
    assert "records no owner authorised to sign the part" in message


def test_signatures_name_a_record_carrying_no_sign_off_date() -> None:
    (message,) = _signature_messages(_signing(sign_off_date=""))
    assert "carries no sign-off date" in message
    assert "Requirement 3 criterion 1" in message


def test_signatures_accept_a_committed_record_whose_trailers_name_the_recorded_owner(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """The accepted record end to end, over a repository: one `git init` and one commit.

    It is also where the run's own output is read, because the passing case is where a reader is
    most likely to take a signature for more than it is.
    """
    root, sha = _signed_repository(tmp_path, SIGNING_OWNER)

    premises = load_premises(root)

    assert not isinstance(premises, list), premises
    assert premises.closures[f"{UNUSED_GATE}:{SETTLED_PART}"].record.entry_commit == sha
    assert run_checks(premises) == []

    monkeypatch.setattr("validate_gate_register.ROOT", root)
    assert main() == 0
    assert SIGNATURE_NOTICE in capsys.readouterr().out


def test_signatures_name_both_directions_of_a_trailer_set_that_does_not_match(
    tmp_path: Path,
) -> None:
    """A commit naming a party the Gate_Dossier does not record: one `git init` and one commit.

    The recorded rows agree with the recorded owner set here, so the two findings are the trailer
    set's alone, and each says what the comparison establishes.
    """
    root, sha = _signed_repository(tmp_path, UNRECORDED_OWNER)

    premises = load_premises(root)

    assert not isinstance(premises, list), premises
    messages = _signature_messages(premises)
    assert len(messages) == 2
    assert all(sha in message and RECORDED_IDENTITY in message for message in messages)
    assert any(f"no trailer there names {SIGNING_OWNER}" in message for message in messages)
    assert any(
        f"trailers name {UNRECORDED_OWNER}, whom the Gate_Dossier does not record" in message
        for message in messages
    )


# ---------------------------------------------------------------------------
# closure-entry-commit
#
# Named with the `entry_commit` prefix for the reason the signature tests carry `signature`: it is
# what this task's command selects.
#
# The add-commit is resolved once per record and travels with the record, so a rule about the
# recorded value against the resolved one is checked over premises built in code and costs no
# history at all. Two tests here pay for a repository -- one `git init` and one commit each -- and
# they are the two whose subject *is* the history: the record whose recorded commit is the commit
# that added it, and the repository whose history does not reach that commit. The third history
# case, a tree with no repository in it, costs neither.
# ---------------------------------------------------------------------------

# A second valid hash, which is what a record recording the wrong commit records: the finding has to
# name both, so the two values a test compares have to differ and both have to be readable as one.
ANOTHER_COMMIT = "1" * 40


def _entry_commit_messages(premises: Premises) -> list[str]:
    return _messages(check_closure_entry_commit(premises), ENTRY_COMMIT)


def _entry_commit_premises(
    *, state: str = ACCEPTED_RECORD_STATE, commit: str | None, resolved: AddCommit | None
) -> Premises:
    """Premises over one part of UNUSED_GATE whose record records *commit*, with *resolved* read.

    The settlement is the accepting one, so a test rewrites the record's state, its recorded commit
    or the resolution the premises carry and nothing else moves. The part is recorded `submitted`
    whatever the record says, because the parts recorded `accepted` close the Gate and this check
    reads the record's state rather than the part's.
    """
    part = _gate_part(SETTLED_PART, _elements(), state="submitted")
    record = replace(_closure_record(_settled_elements(), state=state), entry_commit=commit)
    return _premises(_settling_register(part), closures=_closures(record, add_commit=resolved))


def test_entry_commit_accepts_a_record_recording_the_commit_that_adds_it(tmp_path: Path) -> None:
    """The accepted record end to end, over a repository: one `git init` and one commit.

    It is also the evidence that the two checks read one resolution: the commit the premises
    carry for the record is the one git reports, and both checks are satisfied by it.
    """
    root, sha = _signed_repository(tmp_path, SIGNING_OWNER)

    premises = load_premises(root)

    assert not isinstance(premises, list), premises
    closure = premises.closures[f"{UNUSED_GATE}:{SETTLED_PART}"]
    assert closure.add_commit is not None
    assert closure.add_commit.sha == sha
    assert check_closure_entry_commit(premises) == []
    assert run_checks(premises) == []


def test_entry_commit_names_the_recorded_commit_and_the_resolved_one() -> None:
    """A record recording a commit that is not the one that added it, and no history at all.

    Both values are named, because one of the two is wrong and a finding carrying either alone
    leaves a reviewer to work the other out for themselves.

    This is what sharing the resolution buys: the comparison reads what the premises carry, so the
    rule and its finding cost no `git init`. The test above is what establishes that a real
    resolution reaches the comparison.
    """
    premises = _entry_commit_premises(
        commit=FIXTURE_COMMIT, resolved=AddCommit(sha=ANOTHER_COMMIT, trailers=(), reason=None)
    )

    (message,) = _entry_commit_messages(premises)
    assert message.startswith(f"{UNUSED_GATE}:{SETTLED_PART}: ")
    assert f"the record at {_record_path(UNUSED_GATE, SETTLED_PART)} is recorded " in message
    assert f"records the entry commit {FIXTURE_COMMIT}" in message
    assert f"the commit that adds that path is {ANOTHER_COMMIT}" in message


def test_entry_commit_accepts_a_submitted_record_carrying_no_commit() -> None:
    """The accepting case for the second rule, and the state every record starts in."""
    assert check_closure_entry_commit(_settlement()) == []


def test_entry_commit_names_a_submitted_record_carrying_a_commit() -> None:
    """A file cannot contain the commit that adds it, so a record not yet in carries none.

    Checked without history, because the rule needs none: the record's own state and its own
    recorded commit are the whole of it.
    """
    premises = _entry_commit_premises(state="submitted", commit=FIXTURE_COMMIT, resolved=None)

    (message,) = _entry_commit_messages(premises)
    assert message.startswith(f"{UNUSED_GATE}:{SETTLED_PART}: ")
    assert f"is recorded submitted and records the entry commit {FIXTURE_COMMIT}" in message
    assert "a file cannot contain the commit that adds it" in message
    assert CLOSURE_RECORD_SCHEMA_PATH in message


def test_entry_commit_is_the_second_line_against_a_submitted_record_carrying_a_commit(
    tmp_path: Path,
) -> None:
    """The schema rejects it first, and the check's own guard is what records the rule.

    `closure-record.schema.json` requires `entryCommit` to be null for every state other than
    `accepted`, so a record like this never reaches the check through `load_premises`. The guard
    above is therefore the second line, and this test is what says so: the premises fail against
    the schema, naming the member that carries the commit.
    """
    path = _record_path(UNUSED_GATE, SETTLED_PART)
    record = replace(
        _closure_record(_settled_elements(), state="submitted"), entry_commit=FIXTURE_COMMIT
    )
    root = _settling_repository(
        tmp_path,
        _gate_part(SETTLED_PART, _elements(), state="submitted"),
        closures={path: dump_closure_record(record)},
    )

    (finding,) = _findings(load_premises(root))

    assert finding.check == SCHEMA
    assert finding.message.startswith(f"{path}: /entryCommit: ")


def test_entry_commit_fails_naming_a_tree_that_is_not_a_git_repository(tmp_path: Path) -> None:
    """A tree with no repository in it, which costs no commit at all.

    An unanswerable history is a finding naming the record and the reason, never a skip: an
    unresolved premise is a failure, and `pre-commit.yml` checks out full history so CI can answer.

    It is also where the one-report rule is asserted. `closure-signatures` rests on the same
    resolution and reads the trailers of the same commit, and one unanswerable history is one
    defect: this check names it, and that check reports nothing.
    """
    path = _record_path(UNUSED_GATE, SETTLED_PART)
    record = _closure_record(_settled_elements())
    root = _settling_repository(
        tmp_path,
        _gate_part(SETTLED_PART, _elements()),
        closures={path: dump_closure_record(record)},
    )

    premises = load_premises(root)

    assert not isinstance(premises, list), premises
    (message,) = _entry_commit_messages(premises)
    assert f"the record at {path} is recorded {ACCEPTED_RECORD_STATE}" in message
    assert "the tree is not a git repository" in message
    assert "a failure rather than a pass" in message
    assert check_closure_signatures(premises) == []


def test_entry_commit_fails_naming_a_history_that_does_not_reach_the_record(
    tmp_path: Path,
) -> None:
    """A repository whose history carries the register and not the record: one `git init`, one commit.

    The commit is what makes this the reason it is. A repository with no commit at all answers that
    its history could not be read, which is a different reason read off a different branch.
    """
    path = _record_path(UNUSED_GATE, SETTLED_PART)
    record = _closure_record(_settled_elements())
    root = _settling_repository(
        tmp_path,
        _gate_part(SETTLED_PART, _elements()),
        closures={path: dump_closure_record(record)},
    )
    _git_init(root)
    _git(root, "add", "--", REGISTER_PATH)
    _git_commit(root, "Add a fixture Gate_Register")

    premises = load_premises(root)

    assert not isinstance(premises, list), premises
    (message,) = _entry_commit_messages(premises)
    assert f"the record at {path} is recorded {ACCEPTED_RECORD_STATE}" in message
    assert "no commit in this repository's history adds it" in message
    assert "pre-commit.yml checks out full history" in message


# ---------------------------------------------------------------------------
# gate-status-derivation
#
# Named with the `status` prefix for the reason the signature tests carry `signature`: it is what
# this task's command selects.
#
# Two documents and nothing else: the status a Gate_Dossier records, and the states its parts
# record. The derivation is `gate_open`'s, evaluated once in the premises, so no test here builds a
# second reading of it -- and the baseline's ten Gates are each OPEN with one unaccepted part, so
# the agreeing case comes for free.
# ---------------------------------------------------------------------------


def _status_part(part: str, state: str) -> GatePart:
    """One part in *state*, naming a Closure_Record exactly where the schema requires one.

    An accepted part must name a record and a part in any other state need not. These premises load
    none either way, which is what keeps the status rule the only one these tests exercise: a part
    naming a record the premises did not load is the premise failure `load_premises` reports
    against the path, and `closure-completeness` reports it nowhere.
    """
    recorded = _gate_part(part, state=state)
    return recorded if state == ACCEPTED_PART_STATE else replace(recorded, closure_record=None)


def _status(status: str, *states: str) -> Premises:
    """Premises over the baseline with UNUSED_GATE recording *status* and one part per state.

    The recorded status is passed rather than derived, because the disagreement between the two is
    the whole subject: `_settling_register` records the status its parts imply, and every test here
    overrides it.
    """
    parts = [
        _status_part(f"{SETTLED_PART}-{index}", state) for index, state in enumerate(states, 1)
    ]
    register = _settling_register(*parts)
    dossier = replace(
        next(item for item in register.gates if item.id == UNUSED_GATE), status=status
    )
    return _premises(
        replace(
            register,
            gates=[dossier if item.id == UNUSED_GATE else item for item in register.gates],
        )
    )


def _status_messages(premises: Premises) -> list[str]:
    return _messages(check_gate_status_derivation(premises), GATE_STATUS_DERIVATION)


def test_status_derivation_accepts_the_baseline_ten_open_gates() -> None:
    """The baseline records ten Gates OPEN with one unaccepted part each, so the derivation agrees.

    The whole register is asserted, not one Gate, because the rule runs over all ten and a check
    that read the wrong direction would reject every one of them.
    """
    assert check_gate_status_derivation(_premises()) == []


def test_status_derivation_accepts_a_gate_recorded_closed_with_every_part_accepted() -> None:
    """The other agreeing case, and the evidence that closing a Gate disturbs no other check."""
    assert run_checks(_status(CLOSED_STATUS, ACCEPTED_PART_STATE, ACCEPTED_PART_STATE)) == []


def test_status_derivation_names_the_unaccepted_part_of_a_gate_recorded_closed() -> None:
    premises = _status(CLOSED_STATUS, ACCEPTED_PART_STATE, ACCEPTED_PART_STATE, "submitted")

    (message,) = _status_messages(premises)
    assert message.startswith(f"{UNUSED_GATE} is recorded {CLOSED_STATUS}, ")
    assert f"records {UNUSED_GATE}:{SETTLED_PART}-3 as not accepted" in message
    # The two accepted parts are not named: what holds the Gate open is the part that is not.
    assert f"{SETTLED_PART}-1" not in message
    assert f"{SETTLED_PART}-2" not in message


def test_status_derivation_names_every_unaccepted_part_of_a_gate_recorded_closed() -> None:
    """Two parts outstanding are two names in one finding.

    Requirement 9 criterion 14 and Requirement 10 criterion 15 each require the part that is not
    accepted to be named, and a Gate recorded closed over two of them needs both: a finding naming
    one would send a reviewer to accept it and leave the Gate recorded closed over the other.
    """
    premises = _status(CLOSED_STATUS, ACCEPTED_PART_STATE, "submitted", "open")

    (message,) = _status_messages(premises)
    assert f"{UNUSED_GATE}:{SETTLED_PART}-2, {UNUSED_GATE}:{SETTLED_PART}-3" in message


def test_status_derivation_names_a_gate_recorded_open_with_every_part_accepted() -> None:
    """The second direction, which is not cosmetic: Requirement 3 criterion 7 records the Gate
    CLOSED once every element is settled, and nothing else would report this.

    `released-derivation` reads part acceptance rather than this status, so the screens stay
    unreleased either way -- which is exactly the reason the disagreement has to be reported here.
    """
    premises = _status(OPEN_STATUS, ACCEPTED_PART_STATE, ACCEPTED_PART_STATE)

    (message,) = _status_messages(premises)
    assert message.startswith(f"{UNUSED_GATE} is recorded {OPEN_STATUS}, ")
    assert (
        f"every part of it accepted: {UNUSED_GATE}:{SETTLED_PART}-1, "
        f"{UNUSED_GATE}:{SETTLED_PART}-2" in message
    )
    assert "holds its Release_Set shut for no recorded reason" in message


@pytest.mark.parametrize(
    ("status", "state", "expected"),
    [
        (CLOSED_STATUS, "submitted", "as not accepted"),
        (OPEN_STATUS, ACCEPTED_PART_STATE, "every part of it accepted"),
    ],
    ids=["closed-over-an-unaccepted-part", "open-over-an-accepted-part"],
)
def test_status_derivation_reads_a_one_part_gate_as_it_reads_a_many_part_one(
    status: str, state: str, expected: str
) -> None:
    """There is no special case at one part: the quantifier is the one a three-part Gate answers."""
    (message,) = _status_messages(_status(status, state))
    assert expected in message
    assert f"{UNUSED_GATE}:{SETTLED_PART}-1" in message


# ---------------------------------------------------------------------------
# signoff-verdicts
#
# Named with the `verdict` prefix for the reason above: it is what this task's command selects.
#
# Every test here is about totality. None of them asserts that a verdict's value decided anything,
# because the check never evaluates one -- and the one test that records `satisfied: false` asserts
# that the finding says so in its own text.
# ---------------------------------------------------------------------------

# Two Sign_Off_Conditions, recorded the way the register records one: an identifier the dossier
# assigns and the requirement criterion it comes from. The identifiers are fixtures; the criteria
# are the two this check discharges.
FIXTURE_CONDITIONS = ("a-fixture-condition", "another-fixture-condition")
FIXTURE_CRITERIA = ("3.2", "3.5")
UNRECORDED_CONDITION = "a-fixture-condition-no-dossier-records"

# The reviewer a verdict names. A reviewer is not an owner label: Requirement 3 criterion 2 records
# who reached the verdict, and the schema takes any non-empty string for it.
FIXTURE_REVIEWER = "a fixture reviewer"


def _condition(condition: str, requirement: str) -> SignOffCondition:
    """One Sign_Off_Condition, which is a condition a reviewer checks by inspection."""
    return SignOffCondition(
        id=condition,
        requirement=requirement,
        text="a fixture condition a reviewer checks against the record",
        verification=None,
    )


def _verdict(
    condition: str, *, satisfied: bool = True, reviewer: str = FIXTURE_REVIEWER
) -> SignOffConditionVerdict:
    """One reviewer's verdict on one condition. Nothing reads ``satisfied`` as evidence."""
    return SignOffConditionVerdict(
        id=condition, satisfied=satisfied, reviewer=reviewer, date=FIXTURE_DATE, note=None
    )


def _resolved() -> AddCommit:
    """The resolution the premises carry for an accepted record, agreeing with what it records.

    An accepted record asserts it is in the repository, so `closure-entry-commit` compares its
    recorded commit against the resolved one and the trailer half of `closure-signatures` reads the
    same commit's trailers. Supplying the agreeing resolution is what lets an accepted part be
    built in code without those two reporting the history a fixture tree does not have.
    """
    return AddCommit(
        sha=FIXTURE_COMMIT, trailers=(f"{SIGNED_BY_KEY}: {SIGNING_OWNER}",), reason=None
    )


def _reviewed(
    conditions: Sequence[SignOffCondition] | None = None,
    verdicts: Sequence[SignOffConditionVerdict] | None = None,
    *,
    state: str = ACCEPTED_PART_STATE,
) -> Premises:
    """Premises over the baseline plus one part of UNUSED_GATE recording *conditions*, settled by a
    record carrying *verdicts*.

    Both default to the accepting case -- the two fixture conditions, each answered by a verdict
    naming a reviewer -- so a test rewrites one side and nothing else moves. The settlement is the
    accepting one, because this check reads the conditions and the verdicts alone.
    """
    recorded = (
        [
            _condition(name, criterion)
            for name, criterion in zip(FIXTURE_CONDITIONS, FIXTURE_CRITERIA, strict=True)
        ]
        if conditions is None
        else list(conditions)
    )
    answered = [_verdict(item.id) for item in recorded] if verdicts is None else list(verdicts)
    part = replace(_gate_part(SETTLED_PART, _elements(), state=state), sign_off_conditions=recorded)
    record = replace(
        _closure_record(_settled_elements(), state=state), sign_off_condition_verdicts=answered
    )
    accepted = state == ACCEPTED_PART_STATE
    return _premises(
        _settling_register(part),
        closures=_closures(record, add_commit=_resolved() if accepted else None),
    )


def _verdict_messages(premises: Premises) -> list[str]:
    return _messages(check_signoff_verdicts(premises), SIGNOFF_VERDICTS)


def test_verdicts_accept_an_accepted_part_whose_conditions_all_carry_verdicts() -> None:
    """The accepting case, and the evidence that accepting a part disturbs no other check."""
    assert run_checks(_reviewed()) == []


def test_verdicts_name_a_condition_the_record_leaves_unanswered() -> None:
    first, second = FIXTURE_CONDITIONS

    (message,) = _verdict_messages(_reviewed(verdicts=[_verdict(first)]))
    assert message.startswith(f"{UNUSED_GATE}:{SETTLED_PART}: ")
    assert f"records the Sign_Off_Condition {second!r} for the part" in message
    assert "not satisfied by going unmentioned" in message


def test_verdicts_name_each_of_two_unanswered_conditions_separately() -> None:
    """Two verdicts to reach are two findings: a reviewer reaches them one at a time."""
    messages = _verdict_messages(_reviewed(verdicts=[]))

    assert len(messages) == 2
    for condition in FIXTURE_CONDITIONS:
        assert any(repr(condition) in message for message in messages)


def test_verdicts_name_a_verdict_naming_no_reviewer() -> None:
    """The schema requires a non-empty reviewer, so whitespace is the shape that reaches here."""
    first, second = FIXTURE_CONDITIONS

    (message,) = _verdict_messages(
        _reviewed(verdicts=[_verdict(first), _verdict(second, reviewer=" ")])
    )
    assert f"the verdict on the Sign_Off_Condition {second!r} names no reviewer" in message
    assert "The verdict is a reviewer's judgement" in message


def test_verdicts_name_a_verdict_for_a_condition_the_dossier_does_not_record() -> None:
    verdicts = [_verdict(condition) for condition in (*FIXTURE_CONDITIONS, UNRECORDED_CONDITION)]

    (message,) = _verdict_messages(_reviewed(verdicts=verdicts))
    assert f"carries a verdict on {UNRECORDED_CONDITION!r}" in message
    assert "records no Sign_Off_Condition of that identifier for the part" in message
    assert all(repr(condition) in message for condition in FIXTURE_CONDITIONS)


def test_verdicts_name_an_unsatisfied_verdict_as_a_disagreement_between_two_records() -> None:
    """The one place the recorded verdict is read, and it is read exactly as recorded.

    The condition is not evaluated here -- that judgement is the reviewer's, and it has been made.
    What the finding reports is that the register records the part accepted while the record
    records its reviewer finding a condition unsatisfied, which Requirement 3 criterion 5 does not
    admit. Staying silent would leave the register able to record an acceptance its own reviewer
    contradicted, with every other check passing.
    """
    first, second = FIXTURE_CONDITIONS

    (message,) = _verdict_messages(
        _reviewed(verdicts=[_verdict(first), _verdict(second, satisfied=False)])
    )
    assert (
        f"the verdict {FIXTURE_REVIEWER} recorded on the Sign_Off_Condition {second!r}" in message
    )
    assert "records it unsatisfied" in message
    assert "does not evaluate the condition" in message
    assert "Requirement 3 criterion 5" in message


def test_verdicts_read_no_part_recorded_submitted() -> None:
    """A record in front of a reviewer is what an unanswered condition looks like, and a verdict of
    unsatisfied on one is a review doing its job rather than a finding."""
    verdicts = [_verdict(FIXTURE_CONDITIONS[0], satisfied=False)]

    assert check_signoff_verdicts(_reviewed(verdicts=verdicts, state="submitted")) == []


# ---------------------------------------------------------------------------
# closure-amendments
#
# Named with the `amendment` prefix for the reason above: it is what this task's command selects.
#
# The load-bearing test is the non-regression one. Requirement 3 criterion 11 retains the accepted
# status of the elements the record settles, so an amendment that disturbed another key would break
# the one clause the rule exists for -- and the way to demonstrate that it does not is to assert
# that the other keys produce no finding and that the record's own rows still satisfy the two
# checks that read them.
# ---------------------------------------------------------------------------

# A key the accepting settlement settles exactly once, under the mapped element, and the other
# member of that element. FIXTURE_KEYS are the keys of both the key-source element and the keyed
# element, so each of them is settled twice, which is the ambiguity the more-than-once rule names.
AMENDED_KEY = FIXTURE_MEMBERS[0]
UNAMENDED_KEY = FIXTURE_MEMBERS[1]
TWICE_SETTLED_KEY = FIXTURE_KEYS[0]
UNSETTLED_KEY = "a-fixture-key-the-record-does-not-settle"

# A second sign-off date, because an amendment carries its own: the record's date is the one its
# part was signed off on, and the amendment's is the one its key was.
AMENDMENT_DATE = "2004-05-06"


def _amendment(
    key: str,
    *,
    signers: Sequence[str] = (SIGNING_OWNER,),
    sign_off_date: str = AMENDMENT_DATE,
    entry_commit: str | None = None,
) -> Amendment:
    """One amendment, settling *key* alone and carrying its own signatures and date."""
    return Amendment(
        key=key,
        sign_off_date=sign_off_date,
        signatures=[_signature(owner) for owner in signers],
        entry_commit=entry_commit,
    )


def _amended(*amendments: Amendment, owners: Sequence[str] = (SIGNING_OWNER,)) -> Premises:
    """Premises over the baseline plus one submitted part of UNUSED_GATE carrying *amendments*.

    The record's own signature rows name every owner in *owners*, so `closure-signatures` is
    satisfied by the record itself and every finding a test asserts is an amendment's.
    """
    part = replace(
        _gate_part(SETTLED_PART, _elements(), state="submitted"), signing_owners=list(owners)
    )
    record = replace(
        _closure_record(_settled_elements(), state="submitted"),
        signatures=[_signature(owner) for owner in owners],
        amendments=list(amendments),
    )
    return _premises(_settling_register(part), closures=_closures(record))


def _amendment_messages(premises: Premises) -> list[str]:
    return _messages(check_closure_amendments(premises), CLOSURE_AMENDMENTS)


def test_amendment_accepts_one_settling_a_key_the_record_settles_once() -> None:
    """The accepting case, and the evidence that an amendment disturbs no other check."""
    assert run_checks(_amended(_amendment(AMENDED_KEY))) == []


def test_amendment_leaves_every_other_key_the_acceptance_already_recorded() -> None:
    """Requirement 3 criterion 11's third clause, which is what this check is for.

    The amendment omits an owner the dossier records, so there is a finding to scope -- and it names
    the amended key and nothing else. The record's own rows still satisfy `closure-signatures` and
    every element it settles still satisfies `closure-completeness`, which is what retaining the
    accepted status of the elements the record settles means here.
    """
    premises = _amended(
        _amendment(AMENDED_KEY, signers=(SIGNING_OWNER,)), owners=(SIGNING_OWNER, APPROVING_OWNER)
    )

    (message,) = _amendment_messages(premises)
    assert f"the amendment to the key {AMENDED_KEY!r} carries no signature" in message
    assert APPROVING_OWNER in message
    for untouched in (UNAMENDED_KEY, *FIXTURE_KEYS):
        assert untouched not in message
    assert check_closure_signatures(premises) == []
    assert check_closure_completeness(premises) == []


def test_amendment_names_a_key_the_record_does_not_settle() -> None:
    """An amendment to a key nothing settles amends nothing: Requirement 3 criterion 11 amends an
    element the record records and leaves settled."""
    (message,) = _amendment_messages(_amended(_amendment(UNSETTLED_KEY)))
    assert message.startswith(f"{UNUSED_GATE}:{SETTLED_PART}: ")
    assert f"the amendment settles the key {UNSETTLED_KEY!r}" in message
    assert "settles no entry under that key" in message
    assert "amends nothing the record records" in message


def test_amendment_names_a_key_the_record_settles_more_than_once() -> None:
    """The ambiguity the scoping cannot survive: the amendment's signatures and date would attach
    to two settled entries, and nothing says which."""
    settled = _settled_elements()
    assert (
        sum(1 for element in settled for entry in element.entries if entry.key == TWICE_SETTLED_KEY)
        == 2
    ), "the fixture settlement settles this key under two elements"

    (message,) = _amendment_messages(_amended(_amendment(TWICE_SETTLED_KEY)))
    assert f"the amendment settles the key {TWICE_SETTLED_KEY!r}" in message
    assert "settles that key 2 times" in message
    assert FIXTURE_KEY_SOURCE_ELEMENT in message
    assert FIXTURE_KEYED_ELEMENT in message


def test_amendment_names_an_owner_whose_signature_it_omits() -> None:
    """The owner set is the part's, which is the set equality `closure-signatures` asserts over the
    record's own rows: Requirement 3 criterion 4 names an amendment alongside a submitted record."""
    (message,) = _amendment_messages(_amended(_amendment(AMENDED_KEY, signers=())))
    assert f"carries no signature from {SIGNING_OWNER}" in message
    assert "signatures it carries attach to that key alone" in message


def test_amendment_names_a_signatory_the_dossier_does_not_record() -> None:
    premises = _amended(_amendment(AMENDED_KEY, signers=(SIGNING_OWNER, UNRECORDED_OWNER)))

    (message,) = _amendment_messages(premises)
    assert f"carries a signature from {UNRECORDED_OWNER}" in message
    assert f"It records {SIGNING_OWNER}" in message


def test_amendment_names_one_carrying_no_sign_off_date() -> None:
    (message,) = _amendment_messages(_amended(_amendment(AMENDED_KEY, sign_off_date="")))
    assert f"the amendment to the key {AMENDED_KEY!r} carries no sign-off date" in message
    assert "Requirement 8 criterion 14" in message


def test_amendment_names_a_key_carrying_two_amendments() -> None:
    """Two amendments to one key carry two sets of signatures and two dates against it.

    One finding, and the per-amendment rules do not run over the pair: the repair is to choose one
    of the two, and the second amendment here breaks both of the other rules without adding a
    finding of its own.
    """
    premises = _amended(
        _amendment(AMENDED_KEY), _amendment(AMENDED_KEY, signers=(), sign_off_date="")
    )

    (message,) = _amendment_messages(premises)
    assert f"carries 2 amendments to the key {AMENDED_KEY!r}" in message
    assert "nothing to choose between them" in message


@pytest.mark.parametrize("commit", [None, FIXTURE_COMMIT], ids=["none", "recorded"])
def test_amendment_reads_no_entry_commit_against_history(commit: str | None) -> None:
    """An amendment's commit is not compared against git history, either way it is recorded.

    The resolution the premises carry is an *add*-commit, and an amendment is a change to a file
    already added, so it enters at a modify-commit that resolution never reports. The schema bounds
    the shape -- null, or forty hex characters -- and the check establishes nothing further rather
    than comparing the amendment against a commit that cannot be its own.
    """
    assert check_closure_amendments(_amended(_amendment(AMENDED_KEY, entry_commit=commit))) == []


# ---------------------------------------------------------------------------
# verified-observation
# ---------------------------------------------------------------------------

# The slot every fixture suite declares, beside the two declarations that are not private slots: the
# `init()` override every Aerora UI suite carries under `protected slots:`, and a private helper
# that is a method rather than a slot. Neither is decoration -- a check that read every `void` in
# the class as a private slot would resolve an observation of something no test runs.
FIXTURE_SLOT = "_aFixtureScreenIsDrawnInABootedWindow"
PROTECTED_SLOT = "init"
PRIVATE_HELPER = "_aFixtureHelper"

# Three further suites the fixture tree declares, each breaking exactly one of the three facts a
# suite has to satisfy, and a fourth name no source declares at all.
UNREGISTERED_SUITE = "AFixtureUnregisteredTest"
UNDECLARED_SUITE = "AFixtureUndeclaredTest"
OUTSIDE_SUITE = "AFixtureOutsideTest"
MISSING_SUITE = "AFixtureMissingTest"

# Where the fixture tree writes the suite outside the overlay's test tree: the core test tree, which
# is where a suite that is not an Aerora one lives.
OUTSIDE_SOURCE = "test/AFixtureOutsideTest.h"

# A status no vocabulary declares.
UNDECLARED_STATUS = "INVENTED"

# The phrase each unresolved fact is reported by, in the order the check reports them.
SUITE_REASON = f"no source under {TEST_TREE}/** declares a class with that name"
SLOT_REASON = "declares no private slot with that name"
REGISTERED_REASON = "no add_qgc_test call"
DECLARED_REASON = "no *_TEST_NAMES list"

# A real private slot of a real registered Aerora suite for each row the real inventory records
# VERIFIED, grouped by the suite whose own header declares it. These pairs are this test file's,
# chosen because they resolve: which observation each screen's record names is task 9.2's to record.
REAL_OBSERVATIONS = {
    "AeroraVehicleConfigUITest": {
        "SCR-SHL-01": "_vehicleConfigurationIsReachableFromTheOperationsHub",
        "SCR-VEH-01": "_aeroraSetupPagesRenderInTheConfigureView",
        "SCR-VEH-02": "_identityPageReadsTheLiveVehicleAndWithholdsAFleetIdentity",
        "SCR-VEH-03": "_capabilityPageReportsAnEmptyInventoryWithoutClaimingAbsence",
        "SCR-VEH-04": "_healthPageWithholdsAVerdictUntilTheAircraftReportsOne",
        "SCR-VEH-06": "_aeroraSetupPagesRenderInTheConfigureView",
    },
    "AeroraFlyViewOverlayUITest": {
        "SCR-SHL-03": "_enforcementIndicatorReachesTheToolbarAndReportsNotEnforced",
        "SCR-LIV-01": "_flyViewLoadsTheAeroraOverlayWithItsSeamContractBound",
        "SCR-LIV-07": "_linkStatePanelReportsOneRowPerLink",
    },
    BASELINE_SUITE: {
        "SCR-SHL-04": BASELINE_SLOT,
        "SCR-LIV-06": "_missionProgressPageIsReachableFromTheAnalyzeMenu",
        "SCR-SEC-01": "_securityPosturePageIsReachableAndReportsNotEnforced",
        "SCR-HLP-02": "_stateVocabularyPageIsReachableAndRendersEveryClass",
        "SCR-HLP-03": "_errorReportPageIsReachableAndAssemblesRealStationState",
    },
}


def _write(path: Path, text: str) -> None:
    """Write *text* to *path*, creating the directories above it."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def _suite_source(suite: str, slot: str) -> str:
    """A test header of the shape the Aerora suites have: the class, its slots, and no body."""
    return (
        "#pragma once\n"
        "\n"
        '#include "UnitTest.h"\n'
        "\n"
        f"class {suite};\n"
        "\n"
        f"class {suite} : public UnitTest\n"
        "{\n"
        "    Q_OBJECT\n"
        "\n"
        "protected slots:\n"
        f"    void {PROTECTED_SLOT}() override;\n"
        "\n"
        "private slots:\n"
        f"    void {slot}();\n"
        "\n"
        "private:\n"
        f"    void {PRIVATE_HELPER}();\n"
        "};\n"
    )


def _test_tree(root: Path) -> Path:
    """Lay out everything `verified-observation` reads from a repository root.

    Four suites, because the three facts a suite has to satisfy are independent: the baseline suite,
    declared in a manifest and registered; one declared and registered nowhere, which is the shape
    `AeroraSceneBootOnlyTest` really has; one registered with no manifest entry, which is the shape
    the two telemetry suites really have; and one declared and registered but written outside
    `aerora/test/**`.

    `Ui.cmake` carries a commented-out registration of the unregistered suite, because the real
    `aerora/src/UI/Ui.cmake` holds a commented copy of the whole block as the prescription for the
    aggregation point. A check that read that copy would call the suite registered.

    The vocabulary document is the real one, copied in. The fixture inventory's Status legend names
    it by the reference the real inventory carries, so a fixture row's status is compared against
    the programme vocabulary rather than against a list written here.
    """
    for suite, slot in (
        (BASELINE_SUITE, BASELINE_SLOT),
        (UNREGISTERED_SUITE, FIXTURE_SLOT),
        (UNDECLARED_SUITE, FIXTURE_SLOT),
    ):
        _write(root / TEST_TREE / "UI" / f"{suite}.h", _suite_source(suite, slot))
    _write(root / OUTSIDE_SOURCE, _suite_source(OUTSIDE_SUITE, FIXTURE_SLOT))
    _write(
        root / OVERLAY_TREE / "src" / "UI" / "Ui.cmake",
        f"set(AERORA_UI_TEST_NAMES\n"
        f"    {BASELINE_SUITE}\n"
        f"    {UNREGISTERED_SUITE}\n"
        f"    {OUTSIDE_SUITE}\n"
        f")\n"
        f"\n"
        f"#   add_qgc_test({UNREGISTERED_SUITE} LABELS Unit)\n",
    )
    _write(
        root / OVERLAY_TREE / "CMakeLists.txt",
        f"if(QGC_BUILD_TESTING)\n"
        f"    add_qgc_test({BASELINE_SUITE} LABELS Integration)\n"
        f"    add_qgc_test({UNDECLARED_SUITE} LABELS Unit)\n"
        f"    add_qgc_test({OUTSIDE_SUITE} LABELS Unit)\n"
        f"endif()\n",
    )
    _write(root / VOCABULARY_PATH, (REPO_ROOT / VOCABULARY_PATH).read_text(encoding="utf-8"))
    return root


def _observed(root: Path, observation: Observation | None) -> Premises:
    """Premises against the tree at *root*, with one delivered screen's observation replaced.

    Every other delivered screen keeps the baseline observation, which both trees resolve, so a test
    that breaks one axis gets the findings of that axis alone.
    """
    screens = _with_screen(_baseline_screens(), DELIVERED_SCREEN, observation=observation)
    return replace(_premises(_register(screens)), root=root)


def test_verified_observation_names_a_verified_row_with_no_observation(tmp_path: Path) -> None:
    findings = check_verified_observation(_observed(_test_tree(tmp_path), None))

    (message,) = _messages(findings, VERIFIED_OBSERVATION)
    assert f"{DELIVERED_SCREEN}: {INVENTORY_PATH} records the screen {VERIFIED_STATUS}" in message
    assert "records no observation of it navigated to and drawn in a booted" in message


@pytest.mark.parametrize(
    ("suite", "slot", "expected"),
    [
        (MISSING_SUITE, FIXTURE_SLOT, (SUITE_REASON, REGISTERED_REASON, DECLARED_REASON)),
        (OUTSIDE_SUITE, FIXTURE_SLOT, (SUITE_REASON,)),
        (BASELINE_SUITE, "_aMissingSlot", (SLOT_REASON,)),
        (BASELINE_SUITE, PROTECTED_SLOT, (SLOT_REASON,)),
        (BASELINE_SUITE, PRIVATE_HELPER, (SLOT_REASON,)),
        (UNREGISTERED_SUITE, FIXTURE_SLOT, (REGISTERED_REASON,)),
        (UNDECLARED_SUITE, FIXTURE_SLOT, (DECLARED_REASON,)),
    ],
    ids=[
        "no-suite",
        "outside-tree",
        "no-slot",
        "protected-slot",
        "private-method",
        "unregistered",
        "undeclared",
    ],
)
def test_verified_observation_names_each_fact_an_observation_fails_to_resolve(
    tmp_path: Path, suite: str, slot: str, expected: tuple[str, ...]
) -> None:
    """Three facts, reported separately: a suite nothing declares fails all three at once.

    "Not found" is not "present", so a suite written outside `aerora/test/**` is reported as
    unfound there even though it exists, and a suite the overlay compiles but registers nowhere is
    reported even though its source is right where the observation says.
    """
    premises = _observed(_test_tree(tmp_path), Observation(suite=suite, slot=slot))

    messages = _messages(check_verified_observation(premises), VERIFIED_OBSERVATION)
    assert len(messages) == len(expected)
    for reason, message in zip(expected, messages, strict=True):
        assert reason in message
        assert message.startswith(f"{DELIVERED_SCREEN}: ")
        assert suite in message


def test_verified_observation_names_the_source_a_missing_slot_was_searched_for_in(
    tmp_path: Path,
) -> None:
    premises = _observed(_test_tree(tmp_path), Observation(suite=BASELINE_SUITE, slot="_absent"))

    (message,) = _messages(check_verified_observation(premises), VERIFIED_OBSERVATION)
    assert f"the slot _absent of {BASELINE_SUITE}" in message
    assert f"{TEST_TREE}/UI/{BASELINE_SUITE}.h {SLOT_REASON}" in message


def test_verified_observation_accepts_an_observation_the_fixture_tree_resolves(
    tmp_path: Path,
) -> None:
    assert check_verified_observation(_observed(_test_tree(tmp_path), _observation())) == []


def test_verified_observation_names_a_row_carrying_a_status_no_vocabulary_declares(
    tmp_path: Path,
) -> None:
    rows = {**_baseline_rows(_register()), SINGLE_GATE_SCREEN: UNDECLARED_STATUS}
    premises = replace(_premises(rows=rows), root=_test_tree(tmp_path))

    (message,) = _messages(check_verified_observation(premises), VERIFIED_OBSERVATION)
    assert f"the row for {SINGLE_GATE_SCREEN} records the status {UNDECLARED_STATUS}" in message
    assert "the vocabulary the inventory declares is" in message
    # Both halves of the vocabulary are named: the document the legend references declares the
    # undelivered statuses, and the inventory's own totals table declares the delivered ones.
    assert UNDELIVERED_STATUS in message
    assert SUPERSEDED_STATUS in message


def test_verified_observation_reports_a_vocabulary_legend_it_cannot_read() -> None:
    """A reworded legend stops the rows being compared against anything, and that is the finding."""
    premises = _rewritten(STATUS_LEGEND, "Columns: as described above")

    (message,) = _messages(check_verified_observation(premises), VERIFIED_OBSERVATION)
    assert f"{INVENTORY_PATH} carries no Status column legend" in message
    assert "is not a vocabulary every row satisfies" in message


def test_verified_observation_names_a_vocabulary_document_the_tree_lacks(tmp_path: Path) -> None:
    root = _test_tree(tmp_path)
    (root / VOCABULARY_PATH).unlink()

    premises = _observed(root, _observation())

    (message,) = _messages(check_verified_observation(premises), VERIFIED_OBSERVATION)
    assert f"reads its Status vocabulary from {VOCABULARY_PATH}" in message
    assert f"{VOCABULARY_PATH} does not exist" in message


def test_verified_observation_resolves_every_real_verified_row_against_the_real_tree() -> None:
    """The only evidence this check resolves a real observation rather than a fixture one.

    The register does not exist yet, so the observations are this file's: a real private slot of a
    real registered Aerora suite for each row the real inventory records `VERIFIED`. What it
    establishes is that such a pair resolves against the tree, the overlay's CMake registration and
    the real status vocabulary -- and it is what will stop task 9.2 recording a pair that does not.
    """
    text = (REPO_ROOT / INVENTORY_PATH).read_text(encoding="utf-8")
    inventory = parse_screen_inventory(text, path=INVENTORY_PATH)
    observations = {
        screen: Observation(suite=suite, slot=slot)
        for suite, slots in REAL_OBSERVATIONS.items()
        for screen, slot in slots.items()
    }
    verified = {row.screen for row in inventory.rows if row.status == VERIFIED_STATUS}
    assert observations.keys() == verified, "one observation per VERIFIED row, and no others"

    register = _register(
        [_screen(row.screen, observation=observations.get(row.screen)) for row in inventory.rows]
    )
    premises = Premises(
        root=REPO_ROOT,
        register=register,
        inventory=inventory,
        derived=derive(register, inventory),
        closures={},
    )

    assert check_verified_observation(premises) == []


# ---------------------------------------------------------------------------
# guard-registry
#
# Named with the `guard` prefix for the reason the status tests carry `status`: it is what this
# task's command selects.
#
# Two documents, read against each other. The register maps a guard to the Gates it holds open and
# records which Gate parts could permit it to move; the Closure_Record settling such a part records
# whether that acceptance did. Nothing here evaluates a verdict -- a guard every permitting part
# permitted is free, a guard one of them withheld stays pinned, and both are records an owner
# signed. What the check establishes is coverage, the mapping in both directions, that every
# permission resolves, and that no acceptance left a guard mapped to it unanswered.
# ---------------------------------------------------------------------------

# A part no Gate_Dossier records, which is what a change permission must not resolve to.
UNRECORDED_PART = "a-fixture-part-no-gate-records"

# A second fixture literal, for the one guard whose recorded value criterion 5 does not name.
ANOTHER_GUARD_VALUE = "another fixture literal"


def _permission(symbol: str, *, permits: bool = True) -> GuardPermission:
    """One verdict on whether accepting a part permits one guard to change value."""
    return GuardPermission(symbol=symbol, permits=permits, reason=None)


def _permissions(gate: str, *, permits: bool = True) -> list[GuardPermission]:
    """A verdict on every guard the baseline registry maps to *gate*'s one part.

    Totality is the rule, so the accepting case answers for every guard mapped to the part and a
    test that wants an omission drops one.
    """
    return [
        _permission(symbol, permits=permits)
        for symbol, gates in BASELINE_GUARD_GATES.items()
        if gate in gates
    ]


def _accepting(
    register: GateRegister, permissions: Mapping[str, Sequence[GuardPermission]]
) -> Premises:
    """Premises over *register* with the `whole` part of each Gate in *permissions* accepted.

    Each Gate's recorded status and each screen's Released flag follow its parts, as
    `_settling_register` does, so the register the guard rules read agrees with itself on every
    other axis and a finding here is a finding about a guard.

    The two checks that read git history are not read here, for the reason `_settlement` builds a
    submitted record: an accepted record asserts it is in the repository, and these premises are
    built in code against the repository root, so no commit adds the path they record.
    """
    for gate in permissions:
        register = _with_gate(
            register,
            gate,
            status=CLOSED_STATUS,
            parts=[
                replace(
                    _dossier(gate, ()).parts[0],
                    state=ACCEPTED_PART_STATE,
                    closure_record=_record_path(gate, BASELINE_PART),
                )
            ],
        )
    derived = released(register)
    register = replace(
        register,
        screens=[
            replace(record, released=derived[record.id].released) for record in register.screens
        ],
    )
    return _premises(
        register,
        closures=_closures(
            *(
                _closure_record(gate=gate, part=BASELINE_PART, permissions=verdicts)
                for gate, verdicts in permissions.items()
            )
        ),
    )


def _freedom(premises: Premises) -> dict[str, GuardFreedom]:
    """The freedom derivation over established premises, as check_guard_registry reads it."""
    return guard_freedom(
        premises.register,
        {reference: closure.record for reference, closure in premises.closures.items()},
    )


def _guard_messages(premises: Premises) -> list[str]:
    return _messages(check_guard_registry(premises), GUARD_REGISTRY)


def test_the_baseline_guard_registry_carries_the_named_facts_the_checks_read() -> None:
    register = _register()

    # The premises of every test below rather than assertions about the registry the spec will
    # write: the nine symbols, the Gates requirements.md records them against, and that every one
    # of them is pinned while no part is accepted.
    assert {guard.symbol for guard in register.guards} == set(MECHANICAL_GUARDS)
    assert BASELINE_GUARD_GATES[TELEMETRY_GUARD] == (SINGLE_GATE,)
    assert BASELINE_GUARD_GATES[POWER_GUARD] == (PAYLOAD_GATE,)
    assert BASELINE_GUARD_GATES[REPLAY_GUARD] == (MISSION_GATE,)
    assert BASELINE_GUARD_GATES[ACTUATOR_PATH_GUARD] == (PAYLOAD_GATE, ROLE_GATE)
    assert not any(verdict.is_free for verdict in _freedom(_premises(register)).values())


def test_guard_registry_accepts_the_agreeing_registry() -> None:
    assert check_guard_registry(_premises()) == []


@pytest.mark.parametrize("symbol", sorted(MECHANICAL_GUARDS))
def test_guard_registry_names_a_criterion_5_guard_the_registry_omits(symbol: str) -> None:
    register = _register(guards=[_guard(item) for item in BASELINE_GUARD_GATES if item != symbol])

    (message,) = _guard_messages(_premises(register))
    assert message.startswith(f"{symbol}: Requirement 4 criterion 5 records the guard")
    assert "records no entry for it" in message


def test_guard_registry_accepts_a_guard_outside_the_nine_criterion_5_names() -> None:
    """Criterion 5 covers "at least" the nine, and each Gate's own named guard is one more."""
    register = _register(
        guards=[
            *(_guard(symbol) for symbol in BASELINE_GUARD_GATES),
            _guard(FIXTURE_GUARD, holds_open=[UNUSED_GATE]),
        ]
    )

    assert check_guard_registry(_premises(register)) == []


def test_guard_registry_names_a_pinned_guard_recorded_at_another_value() -> None:
    register = _with_guard(_register(), TELEMETRY_GUARD, recorded_value=FIXTURE_GUARD_VALUE)

    (message,) = _guard_messages(_premises(register))
    assert message.startswith(f"{TELEMETRY_GUARD}: Requirement 4 criterion 5 records the guard at ")
    assert repr(MECHANICAL_GUARDS[TELEMETRY_GUARD]) in message
    assert repr(FIXTURE_GUARD_VALUE) in message
    assert f"{part_reference(SINGLE_GATE, BASELINE_PART)}: the part is not accepted" in message


def test_guard_registry_reads_the_recorded_value_of_a_freed_guard_for_nothing() -> None:
    """Criterion 6 lets a permitted guard move, so the value criterion 5 names stops being a rule.

    A check that still demanded it would reject the one state Requirement 4 exists to allow.
    """
    register = _with_guard(_register(), TELEMETRY_GUARD, recorded_value=FIXTURE_GUARD_VALUE)
    premises = _accepting(register, {SINGLE_GATE: _permissions(SINGLE_GATE)})

    assert _freedom(premises)[TELEMETRY_GUARD].is_free
    assert check_guard_registry(premises) == []


def test_guard_registry_reads_no_value_for_the_mavlink_identifier_check() -> None:
    """The ninth guard is a check, not a symbol holding a literal, so criterion 5 names no value.

    The registry names it by the source-half check that enforces it, and what it records against it
    is `mavlink-identifier`'s own rule to read rather than this one's.
    """
    assert MECHANICAL_GUARDS[MAVLINK_IDENTIFIER_GUARD] is None
    register = _with_guard(
        _register(), MAVLINK_IDENTIFIER_GUARD, recorded_value=ANOTHER_GUARD_VALUE
    )

    assert check_guard_registry(_premises(register)) == []


def test_guard_registry_names_a_gate_recording_neither_guards_nor_review_alone() -> None:
    register = _with_gate(_register(), UNUSED_GATE, guards=[], rests_on_review_alone=None)

    (message,) = _guard_messages(_premises(register))
    assert message.startswith(f"{UNUSED_GATE}: the Gate_Dossier records no guard")
    assert "does not record that the Gate rests on the Gate_Review alone" in message
    assert "Requirement 4 criterion 8" in message


def test_guard_registry_names_a_gate_recording_guards_and_review_alone() -> None:
    register = _with_gate(_register(), SINGLE_GATE, rests_on_review_alone=True)

    (message,) = _guard_messages(_premises(register))
    assert message.startswith(f"{SINGLE_GATE}: the Gate_Dossier records {TELEMETRY_GUARD}")
    assert "records that the Gate rests on the Gate_Review alone" in message


def test_guard_registry_is_the_second_line_against_a_gate_recording_neither(
    tmp_path: Path,
) -> None:
    """The schema rejects it first, and the check's own guard is what records the rule.

    `gate-register.schema.json` requires `restsOnReviewAlone: true` alongside an empty `guards`
    list, so a dossier recording neither never reaches the check through `load_premises`. The guard
    above is therefore the second line, and this test is what says so.
    """
    register = _with_gate(_register(), UNUSED_GATE, guards=[], rests_on_review_alone=None)
    root = _repository(
        tmp_path,
        register=dump_gate_register(register),
        inventory=_inventory_text(_baseline_rows(register), _blocked_by(register)),
    )

    findings = _findings(load_premises(root))

    assert {finding.check for finding in findings} == {SCHEMA}
    assert any(
        "'restsOnReviewAlone' is a required property" in finding.message for finding in findings
    )


def test_the_schema_requires_a_gate_dossier_to_record_a_guards_list(tmp_path: Path) -> None:
    """The other half of the same absence: the list itself cannot be left out either."""
    register = _register()
    document = dump_gate_register(register)
    del document["gates"][0]["guards"]
    root = _repository(
        tmp_path,
        register=document,
        inventory=_inventory_text(_baseline_rows(register), _blocked_by(register)),
    )

    findings = _findings(load_premises(root))

    assert {finding.check for finding in findings} == {SCHEMA}
    assert any("'guards' is a required property" in finding.message for finding in findings)


def test_guard_registry_names_a_registry_entry_recording_neither_gate_nor_verdict() -> None:
    """The guard side of the same obligation, which the schema also rejects first."""
    register = _with_guard(_register(), TELEMETRY_GUARD, holds_open=[], holds_no_gate_open=None)

    messages = _guard_messages(_premises(register))
    assert any("records no Gate the guard holds open" in message for message in messages)
    # The dossier still records the guard, so the mapping disagrees as well. Both are findings.
    assert any(f"{SINGLE_GATE}: the Gate_Dossier records the guard" in m for m in messages)


def test_guard_registry_names_a_registry_entry_recording_a_gate_and_no_gate_at_once() -> None:
    register = _with_guard(_register(), TELEMETRY_GUARD, holds_no_gate_open=True)

    (message,) = _guard_messages(_premises(register))
    assert message.startswith(f"{TELEMETRY_GUARD}: the registry records the guard as holding ")
    assert "records that no Gate rests on it" in message


def test_guard_registry_names_a_guard_a_dossier_records_and_the_registry_does_not() -> None:
    register = _with_gate(
        _register(), UNUSED_GATE, guards=[FIXTURE_GUARD], rests_on_review_alone=None
    )

    (message,) = _guard_messages(_premises(register))
    assert message.startswith(
        f"{UNUSED_GATE}: the Gate_Dossier records the guard {FIXTURE_GUARD!r}"
    )
    assert "the guard registry records no entry for it" in message
    assert "The registry is the authority" in message


def test_guard_registry_names_a_dossier_recording_a_guard_the_registry_holds_elsewhere() -> None:
    register = _with_gate(
        _register(), UNUSED_GATE, guards=[REPLAY_GUARD], rests_on_review_alone=None
    )

    (message,) = _guard_messages(_premises(register))
    assert message.startswith(f"{UNUSED_GATE}: the Gate_Dossier records the guard {REPLAY_GUARD!r}")
    assert f"records it as holding {MISSION_GATE} open" in message


def test_guard_registry_names_a_dossier_that_drops_a_guard_the_registry_maps_to_it() -> None:
    register = _with_gate(_register(), SINGLE_GATE, guards=[], rests_on_review_alone=True)

    (message,) = _guard_messages(_premises(register))
    assert message.startswith(
        f"{TELEMETRY_GUARD}: the registry records the guard as holding {SINGLE_GATE} open"
    )
    assert "that Gate_Dossier records no guard" in message
    assert "Requirement 4 criterion 8" in message


def test_guard_registry_names_a_permitting_part_the_register_does_not_record() -> None:
    reference = part_reference(SINGLE_GATE, UNRECORDED_PART)
    register = _with_guard(_register(), TELEMETRY_GUARD, permitted_to_change_by=[reference])

    (message,) = _guard_messages(_premises(register))
    assert message.startswith(f"{TELEMETRY_GUARD}: the registry records {reference} as permitted")
    assert "the register records no part of that name" in message


def test_guard_registry_names_a_guard_the_register_records_twice() -> None:
    """Two entries for one symbol leave the permission rule with two answers and no way to choose."""
    register = _register(
        guards=[
            *(_guard(symbol) for symbol in BASELINE_GUARD_GATES),
            _guard(TELEMETRY_GUARD, recorded_value=FIXTURE_GUARD_VALUE),
        ]
    )

    (message,) = _guard_messages(_premises(register))
    assert message == f"{TELEMETRY_GUARD}: the register records more than one guard of that symbol"


def test_guard_registry_names_an_accepted_part_recording_no_verdict_for_a_guard() -> None:
    """Criterion 6 read as totality: omission is a finding, not a default."""
    premises = _accepting(_register(), {SINGLE_GATE: []})

    (message,) = _guard_messages(premises)
    assert message.startswith(
        f"{part_reference(SINGLE_GATE, BASELINE_PART)}: the registry maps the guard "
        f"{TELEMETRY_GUARD!r} to the part"
    )
    assert "the record carries no verdict" in message
    assert "an omission is a finding and never a default" in message
    assert not _freedom(premises)[TELEMETRY_GUARD].is_free


def test_guard_registry_names_a_verdict_on_a_guard_the_part_cannot_change() -> None:
    permissions = [*_permissions(SINGLE_GATE), _permission(FIXTURE_GUARD)]
    premises = _accepting(_register(), {SINGLE_GATE: permissions})

    (message,) = _guard_messages(premises)
    assert f"the record carries a verdict on the guard {FIXTURE_GUARD!r}" in message
    assert "records no permission for this part to change that guard" in message


def test_guard_registry_names_two_verdicts_on_one_guard() -> None:
    permissions = [_permission(TELEMETRY_GUARD), _permission(TELEMETRY_GUARD, permits=False)]
    premises = _accepting(_register(), {SINGLE_GATE: permissions})

    (message,) = _guard_messages(premises)
    assert f"the record carries 2 verdicts on the guard {TELEMETRY_GUARD!r}" in message
    assert not _freedom(premises)[TELEMETRY_GUARD].is_free


def test_guard_freedom_frees_a_guard_its_one_permitting_part_permitted() -> None:
    premises = _accepting(_register(), {SINGLE_GATE: _permissions(SINGLE_GATE)})

    verdict = _freedom(premises)[TELEMETRY_GUARD]
    assert verdict.is_free
    assert verdict.permitting_parts == (part_reference(SINGLE_GATE, BASELINE_PART),)
    assert check_guard_registry(premises) == []


def test_guard_freedom_leaves_a_guard_pinned_by_a_non_permitting_verdict() -> None:
    """Requirement 6 criterion 17: DG-MIS closes and the replay guard does not move.

    The MAVLink-identifier check is the same part's other guard and this acceptance permits it, so
    one signature frees one of the two guards put to it and leaves the other pinned. An
    implementation reading "the Gate closed" as "its guards are free" would let the second go, and
    nothing else would notice.
    """
    permissions = [
        _permission(REPLAY_GUARD, permits=False),
        _permission(MAVLINK_IDENTIFIER_GUARD),
    ]
    premises = _accepting(_register(), {MISSION_GATE: permissions})

    freedom = _freedom(premises)
    assert not freedom[REPLAY_GUARD].is_free
    assert freedom[REPLAY_GUARD].withholding_parts == (part_reference(MISSION_GATE, BASELINE_PART),)
    assert freedom[MAVLINK_IDENTIFIER_GUARD].is_free
    assert check_guard_registry(premises) == []


def test_guard_freedom_pins_a_two_gate_guard_until_both_parts_permit_it() -> None:
    """The conjunction is the check: one of the two Gates has not been signed."""
    premises = _accepting(_register(), {PAYLOAD_GATE: _permissions(PAYLOAD_GATE)})

    freedom = _freedom(premises)
    assert not freedom[ACTUATOR_PATH_GUARD].is_free
    assert freedom[ACTUATOR_PATH_GUARD].unaccepted_parts == (
        part_reference(ROLE_GATE, BASELINE_PART),
    )
    assert not freedom[PAYLOAD_ACTION_GUARD].is_free
    # The guards DG-PAY alone holds open are freed by the same acceptance.
    assert freedom[POWER_GUARD].is_free
    assert check_guard_registry(premises) == []


def test_guard_freedom_frees_a_two_gate_guard_once_both_parts_permit_it() -> None:
    premises = _accepting(
        _register(),
        {PAYLOAD_GATE: _permissions(PAYLOAD_GATE), ROLE_GATE: _permissions(ROLE_GATE)},
    )

    freedom = _freedom(premises)
    assert freedom[ACTUATOR_PATH_GUARD].is_free
    assert freedom[PAYLOAD_ACTION_GUARD].is_free
    assert check_guard_registry(premises) == []


def test_guard_freedom_pins_a_two_gate_guard_one_of_its_parts_withheld() -> None:
    """Both parts accepted, one of them withholding: the conjunction reads the withheld verdict."""
    premises = _accepting(
        _register(),
        {
            PAYLOAD_GATE: _permissions(PAYLOAD_GATE),
            ROLE_GATE: _permissions(ROLE_GATE, permits=False),
        },
    )

    freedom = _freedom(premises)
    assert not freedom[ACTUATOR_PATH_GUARD].is_free
    assert freedom[ACTUATOR_PATH_GUARD].withholding_parts == (
        part_reference(ROLE_GATE, BASELINE_PART),
    )
    assert freedom[POWER_GUARD].is_free
    assert check_guard_registry(premises) == []


def test_guard_freedom_pins_a_guard_no_part_is_permitted_to_change() -> None:
    """The conjunction over an empty set is true, and freedom is not what that should read as.

    A guard naming no permitting part is one no signature was ever recorded as unlocking, which is
    the vacuity `gate_open` rejects for a Gate recording no part.
    """
    premises = _premises(_with_guard(_register(), TELEMETRY_GUARD, permitted_to_change_by=[]))

    verdict = _freedom(premises)[TELEMETRY_GUARD]
    assert not verdict.is_free
    assert verdict.permitting_parts == ()
    assert check_guard_registry(premises) == []


def test_guard_freedom_pins_a_guard_whose_accepted_part_names_no_record() -> None:
    """An unreadable verdict is not a permitting one, and the missing record is reported elsewhere.

    `closure-completeness` names a part recorded accepted that names no Closure_Record, so this
    check reports nothing and leaves the guard pinned.
    """
    accepted = replace(
        _dossier(SINGLE_GATE, ()).parts[0], state=ACCEPTED_PART_STATE, closure_record=None
    )
    register = _with_gate(_register(), SINGLE_GATE, status=CLOSED_STATUS, parts=[accepted])
    premises = _premises(register)

    verdict = _freedom(premises)[TELEMETRY_GUARD]
    assert not verdict.is_free
    assert verdict.unrecorded_parts == (part_reference(SINGLE_GATE, BASELINE_PART),)
    assert check_guard_registry(premises) == []


# ---------------------------------------------------------------------------
# The error contract
# ---------------------------------------------------------------------------


def _repository(
    root: Path,
    *,
    register: object | str | None,
    inventory: str | None,
    schema: bool = True,
    closures: Mapping[str, object | str] | None = None,
) -> Path:
    """Lay out a repository root carrying the real schemas and the given documents.

    The test tree comes with it, because `run_checks` resolves the baseline's observation against
    the root it is given and a root with no `aerora/test/**` would report every observation unfound.

    *closures* is a Closure_Record per repository-relative path, which is the path the part naming
    it records. A path it omits is a record the tree lacks.
    """
    _test_tree(root)
    (root / REGISTER_PATH).parent.mkdir(parents=True, exist_ok=True)
    if schema:
        for path in (REGISTER_SCHEMA_PATH, CLOSURE_RECORD_SCHEMA_PATH):
            (root / path).write_text(
                (REPO_ROOT / path).read_text(encoding="utf-8"), encoding="utf-8"
            )
    if register is not None:
        text = register if isinstance(register, str) else json.dumps(register, indent=2)
        (root / REGISTER_PATH).write_text(text, encoding="utf-8")
    if inventory is not None:
        (root / INVENTORY_PATH).parent.mkdir(parents=True, exist_ok=True)
        (root / INVENTORY_PATH).write_text(inventory, encoding="utf-8")
    for path, document in (closures or {}).items():
        text = document if isinstance(document, str) else json.dumps(document, indent=2)
        _write(root / path, text)
    return root


def _findings(result: Premises | list[Finding]) -> list[Finding]:
    assert isinstance(result, list), "expected a premise failure, and the premises were established"
    assert result, "a premise failure must name the premise it could not establish"
    return result


def test_premises_fail_naming_a_register_that_does_not_exist(tmp_path: Path) -> None:
    root = _repository(tmp_path, register=None, inventory=_inventory_text({}))

    (finding,) = _findings(load_premises(root))

    assert finding.check == PREMISES
    assert finding.message == f"{REGISTER_PATH} does not exist"


def test_premises_fail_naming_a_schema_that_does_not_exist(tmp_path: Path) -> None:
    root = _repository(tmp_path, register={}, inventory=None, schema=False)

    (finding,) = _findings(load_premises(root))

    assert finding.check == PREMISES
    assert finding.message == f"{REGISTER_SCHEMA_PATH} does not exist"


def test_premises_fail_naming_the_line_of_a_register_that_is_not_json(tmp_path: Path) -> None:
    root = _repository(tmp_path, register='{\n  "inventory": {,\n}\n', inventory=None)

    (finding,) = _findings(load_premises(root))

    assert finding.check == PREMISES
    assert finding.message.startswith(f"{REGISTER_PATH}:2:17:")


def test_premises_fail_with_a_json_pointer_per_schema_error(tmp_path: Path) -> None:
    root = _repository(tmp_path, register={"gates": []}, inventory=None)

    findings = _findings(load_premises(root))

    assert {finding.check for finding in findings} == {SCHEMA}
    assert all(finding.message.startswith(f"{REGISTER_PATH}: ") for finding in findings)
    messages = [finding.message for finding in findings]
    assert any("the document root: 'inventory' is a required property" in m for m in messages)
    assert any("/gates: " in m for m in messages)


def test_premises_fail_naming_an_inventory_the_register_names_and_the_tree_lacks(
    tmp_path: Path,
) -> None:
    root = _repository(tmp_path, register=dump_gate_register(_register()), inventory=None)

    (finding,) = _findings(load_premises(root))

    assert finding.check == PREMISES
    assert finding.message == f"{INVENTORY_PATH} does not exist"


def test_premises_fail_naming_an_inventory_with_no_area_table(tmp_path: Path) -> None:
    root = _repository(
        tmp_path,
        register=dump_gate_register(_register()),
        inventory="# Inventory\n\nProse only.\n",
    )

    (finding,) = _findings(load_premises(root))

    assert finding.check == PREMISES
    assert "carries no area table heading" in finding.message


def test_premises_fail_naming_an_inventory_that_carries_no_screen_row(tmp_path: Path) -> None:
    root = _repository(
        tmp_path, register=dump_gate_register(_register()), inventory=_inventory_text({})
    )

    (finding,) = _findings(load_premises(root))

    assert finding.check == PREMISES
    assert "carry no screen row" in finding.message


def test_the_schema_rejects_a_gate_classified_entry_that_names_no_gate(tmp_path: Path) -> None:
    """The entry the release derivation cannot resolve never reaches it.

    The schema's conditional on ``classification`` requires ``gate`` alongside ``Gate``, so the
    derivation's own guard is a second line rather than the first, and no schema-valid register can
    carry a Gate-classified entry naming nothing.
    """
    screens = _with_screen(
        _baseline_screens(),
        SINGLE_GATE_SCREEN,
        blocking_entries=[_entry("P7-TEL-01-001", classification=GATE)],
    )
    register = _register(screens)
    root = _repository(
        tmp_path,
        register=dump_gate_register(register),
        inventory=_inventory_text(_baseline_rows(register), _blocked_by(register)),
    )

    findings = _findings(load_premises(root))

    assert {finding.check for finding in findings} == {SCHEMA}
    assert any("'gate' is a required property" in finding.message for finding in findings)


def test_premises_fail_once_naming_a_screen_the_register_records_twice(tmp_path: Path) -> None:
    """An unresolvable derivation is one premise failure, not one finding per check that reads it.

    Two records for one screen pass the schema's ``uniqueItems``, because they differ in the flag
    that makes them irreconcilable: the derivation has two answers for the screen's release and no
    way to choose.
    """
    screens = _baseline_screens()
    (duplicated,) = (record for record in screens if record.id == SINGLE_GATE_SCREEN)
    register = _register([*screens, replace(duplicated, released=True)])
    root = _repository(
        tmp_path,
        register=dump_gate_register(register),
        inventory=_inventory_text(_baseline_rows(register), _blocked_by(register)),
    )

    (finding,) = _findings(load_premises(root))

    assert finding.check == PREMISES
    assert finding.message.startswith(f"{SINGLE_GATE_SCREEN}: the register records more than one")


def test_the_fixture_register_validates_and_runs_every_check(tmp_path: Path) -> None:
    """The error-contract fixtures are only evidence if the agreeing one gets past the schema."""
    register = _register()
    root = _repository(
        tmp_path,
        register=dump_gate_register(register),
        inventory=_inventory_text(_baseline_rows(register), _blocked_by(register)),
    )

    premises = load_premises(root)

    assert not isinstance(premises, list), premises
    assert len(premises.register.gates) == len(FIXTURE_GATES)
    assert len(premises.inventory.rows) == len(register.screens)
    assert run_checks(premises) == []


@pytest.mark.parametrize(
    ("tokens", "expected"),
    [([], "the document root"), (["gates"], "/gates"), (["gates", 0, "id"], "/gates/0/id")],
    ids=["root", "property", "indexed"],
)
def test_the_json_pointer_addresses_what_the_schema_rejected(
    tokens: list[object], expected: str
) -> None:
    assert _pointer(tokens) == expected

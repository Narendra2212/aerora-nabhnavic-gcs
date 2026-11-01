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
from typing import TYPE_CHECKING

import pytest
from common.gate_register import (
    INVENTORY_COLUMNS,
    BlockingEntry,
    GateDossier,
    GatePart,
    GateRegister,
    InventoryReference,
    Observation,
    OpenStateReading,
    RationaleReference,
    ReadOnlyException,
    ReadOnlyPosture,
    ReleaseSetEntry,
    ScreenRecord,
    dump_gate_register,
    is_entry_id,
    parse_screen_inventory,
)
from common.markdown import md_table
from validate_gate_register import (
    DELIVERED_OUTSIDE,
    ENTRY_CLASSIFICATION,
    INVENTORY_COVERAGE,
    INVENTORY_OWNER,
    INVENTORY_TOTALS,
    NOT_DELIVERED_LABEL,
    OVERLAY_TREE,
    PARTITION,
    PREMISES,
    RECLASSIFICATIONS,
    RECLASSIFIED_ENTRIES,
    REGISTER_PATH,
    REGISTER_SCHEMA_PATH,
    RELEASE_SET_DERIVATION,
    RELEASED_DERIVATION,
    RELEASED_WITH_NO_GATE,
    RETAINED_GATES,
    SCHEMA,
    SUPERSEDED_STATUS,
    TEST_TREE,
    TOTAL_LABEL,
    TOTALS_COLUMNS,
    TWO_GATE,
    TWO_GATE_SCREENS,
    VERIFIED_OBSERVATION,
    VERIFIED_STATUS,
    Premises,
    _pointer,
    check_delivered_outside,
    check_entry_classification,
    check_inventory_coverage,
    check_inventory_totals,
    check_partition,
    check_reclassifications,
    check_release_set_derivation,
    check_released_derivation,
    check_two_gate,
    check_verified_observation,
    derive,
    load_premises,
    run_checks,
)

from ._helpers import REPO_ROOT

if TYPE_CHECKING:
    from collections.abc import Iterable, Mapping, Sequence
    from pathlib import Path

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


def _dossier(gate: str, release_set: Iterable[str]) -> GateDossier:
    """An OPEN Gate with one unaccepted part, recording *release_set* as its Release_Set."""
    return GateDossier(
        id=gate,
        status="OPEN",
        implementing_owner="Product",
        rationale=RationaleReference(document=INVENTORY_PATH, anchor="#a-fixture-anchor"),
        parts=[
            GatePart(
                id="whole",
                signing_owners=["Product"],
                state="open",
                closure_record=None,
                elements=[],
                sign_off_conditions=[],
            )
        ],
        release_set=[ReleaseSetEntry(screen=screen, part=None) for screen in release_set],
        excluded_acceptance_criteria=[],
        guards=[],
        rests_on_review_alone=True,
        open_state_reading=OpenStateReading(
            state_class="a fixture class", reports_gate_identifier=True
        ),
    )


def _register(
    screens: Sequence[ScreenRecord] | None = None,
    release_sets: Mapping[str, Iterable[str]] | None = None,
) -> GateRegister:
    """The fixture register. Both axes default to the agreeing baseline."""
    records = _baseline_screens() if screens is None else list(screens)
    recorded = _recorded_release_sets(records) if release_sets is None else release_sets
    return GateRegister(
        schema_ref=None,
        inventory=InventoryReference(
            path=INVENTORY_PATH,
            authoritative_for=["the screen set", "each screen's delivery status"],
        ),
        gates=[_dossier(gate, recorded.get(gate, ())) for gate in FIXTURE_GATES],
        screens=records,
        guards=[],
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


def _premises_from_text(register: GateRegister, text: str) -> Premises:
    """Premises over a register and an inventory document the caller wrote."""
    inventory = parse_screen_inventory(text, path=INVENTORY_PATH)
    return Premises(
        root=REPO_ROOT,
        register=register,
        inventory=inventory,
        derived=derive(register, inventory),
    )


def _premises(
    register: GateRegister | None = None,
    rows: Mapping[str, str] | None = None,
    blocked: Mapping[str, str] | None = None,
) -> Premises:
    """Premises built the way load_premises builds them, over a fixture register and inventory."""
    recorded = _register() if register is None else register
    return _premises_from_text(
        recorded,
        _inventory_text(
            _baseline_rows(recorded) if rows is None else rows,
            _blocked_by(recorded) if blocked is None else blocked,
        ),
    )


def _with_screen(
    screens: Sequence[ScreenRecord], screen: str, **changes: object
) -> list[ScreenRecord]:
    """Replace one screen record's fields, leaving every other record as it was."""
    return [replace(record, **changes) if record.id == screen else record for record in screens]


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
    )

    assert check_verified_observation(premises) == []


# ---------------------------------------------------------------------------
# The error contract
# ---------------------------------------------------------------------------


def _repository(
    root: Path,
    *,
    register: object | str | None,
    inventory: str | None,
    schema: bool = True,
) -> Path:
    """Lay out a repository root carrying the real schema and the given register and inventory.

    The test tree comes with it, because `run_checks` resolves the baseline's observation against
    the root it is given and a root with no `aerora/test/**` would report every observation unfound.
    """
    _test_tree(root)
    (root / REGISTER_PATH).parent.mkdir(parents=True, exist_ok=True)
    if schema:
        (root / REGISTER_SCHEMA_PATH).write_text(
            (REPO_ROOT / REGISTER_SCHEMA_PATH).read_text(encoding="utf-8"), encoding="utf-8"
        )
    if register is not None:
        text = register if isinstance(register, str) else json.dumps(register, indent=2)
        (root / REGISTER_PATH).write_text(text, encoding="utf-8")
    if inventory is not None:
        (root / INVENTORY_PATH).parent.mkdir(parents=True, exist_ok=True)
        (root / INVENTORY_PATH).write_text(inventory, encoding="utf-8")
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

#!/usr/bin/env python3
"""Tests for the premise loading and the set checks in tools/validate_gate_register.py.

The fixture register is small but not fictional, because four of the six set checks compare the
register against facts requirements.md names: the four Two_Gate_Screens of Requirement 1 criterion 5
with the two Gates each waits on, and the five Released-with-no-Gate screens of Requirement 2
criterion 8. A fixture that renamed those screens would test the checks against a different document
from the one they protect.

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
"""

from __future__ import annotations

import json
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
    parse_screen_inventory,
)
from common.markdown import md_table
from validate_gate_register import (
    DELIVERED_OUTSIDE,
    INVENTORY_COVERAGE,
    PARTITION,
    PREMISES,
    REGISTER_PATH,
    REGISTER_SCHEMA_PATH,
    RELEASE_SET_DERIVATION,
    RELEASED_DERIVATION,
    RELEASED_WITH_NO_GATE,
    SCHEMA,
    TWO_GATE,
    TWO_GATE_SCREENS,
    Premises,
    _pointer,
    check_delivered_outside,
    check_inventory_coverage,
    check_partition,
    check_release_set_derivation,
    check_released_derivation,
    check_two_gate,
    derive,
    load_premises,
    run_checks,
)

from ._helpers import REPO_ROOT

if TYPE_CHECKING:
    from collections.abc import Iterable, Mapping, Sequence
    from pathlib import Path

    from common.gate_register import ScreenInventory
    from validate_gate_register import Finding

# The register names the inventory, so the fixture inventory is written where the real one lives and
# no test invents a path of its own.
INVENTORY_PATH = ".kiro/specs/05-product-completion/03-gui-information-architecture-and-screens.md"

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
DELIVERED_STATUS = "VERIFIED"
UNDELIVERED_STATUS = "DESIGNED"

# The one screen the schema pins the read-only exception to. It carries no screen record, because no
# set check reads the posture.
EXCEPTION_SCREEN = "SCR-HLP-03"

GATE = "Gate"
ACCEPTANCE_CRITERION = "Acceptance_Criterion"


def _entry(
    entry: str, *, gate: str | None = None, classification: str | None = None
) -> BlockingEntry:
    """Build a blocking entry, classified as a Gate exactly when it names one."""
    return BlockingEntry(
        id=entry,
        classification=classification or (GATE if gate is not None else ACCEPTANCE_CRITERION),
        gate=gate,
        reclassified_from=None,
        requirement=None,
        inventory_text=None,
    )


def _gate_entries(screen: str, *gates: str) -> list[BlockingEntry]:
    """One Gate-classified entry per Gate, with an entry identifier derived from the screen."""
    area = screen.removeprefix("SCR-")
    return [_entry(f"P7-{area}-{index:03d}", gate=gate) for index, gate in enumerate(gates, 1)]


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
    one Acceptance_Criterion each, one screen carries a single Gate, and one is delivered.
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
    screens.append(
        _screen(DELIVERED_SCREEN, observation=Observation(suite="AFixtureTest", slot="_aSlot"))
    )
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


def _inventory_text(rows: Mapping[str, str]) -> str:
    """Render *rows* as one area table of the inventory's seven columns."""
    table = md_table(
        INVENTORY_COLUMNS,
        [
            [f"`{screen}`", "A screen", "O", "a seam", f"`{status}`", "7", "-"]
            for screen, status in rows.items()
        ],
    )
    return f"### A fixture area -- {len(rows)} screens\n\n{table}\n"


def _baseline_rows(register: GateRegister) -> dict[str, str]:
    """A row per register screen, delivered for the one screen the fixture records as delivered."""
    return {
        record.id: DELIVERED_STATUS if record.id == DELIVERED_SCREEN else UNDELIVERED_STATUS
        for record in register.screens
    }


def _inventory(rows: Mapping[str, str]) -> ScreenInventory:
    return parse_screen_inventory(_inventory_text(rows), path=INVENTORY_PATH)


def _premises(
    register: GateRegister | None = None, rows: Mapping[str, str] | None = None
) -> Premises:
    """Premises built the way load_premises builds them, over a fixture register and inventory."""
    recorded = _register() if register is None else register
    inventory = _inventory(_baseline_rows(recorded) if rows is None else rows)
    return Premises(
        root=REPO_ROOT,
        register=recorded,
        inventory=inventory,
        derived=derive(recorded, inventory),
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
        record.id for record in premises.register.screens if record.id != DELIVERED_SCREEN
    )
    assert not premises.derived.releases[SINGLE_GATE_SCREEN].released
    assert all(premises.derived.releases[screen].released for screen in RELEASED_WITH_NO_GATE)


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
    assert DELIVERED_STATUS in message


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
# The error contract
# ---------------------------------------------------------------------------


def _repository(
    root: Path,
    *,
    register: object | str | None,
    inventory: str | None,
    schema: bool = True,
) -> Path:
    """Lay out a repository root carrying the real schema and the given register and inventory."""
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
        inventory=_inventory_text(_baseline_rows(register)),
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
        inventory=_inventory_text(_baseline_rows(register)),
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
        inventory=_inventory_text(_baseline_rows(register)),
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

#!/usr/bin/env python3
"""Tests for the screen inventory parser and the derivations in tools/common/gate_register.py.

The document under test for the parser is the real screen inventory, because it is the authority for
the screen set: a parser checked against a fixture alone would keep passing after the document was
restructured, and every check the Gate_Review runs is a predicate over these rows.

Requirements 1.7, 2.1 and 2.10: each inventory row is a screen the register has to account for, and
the entries the register classifies are the identifier tokens of that row's "Blocked by" cell.

The derivation tests build registers in code instead, and deliberately build them small and
fictional. The register itself does not exist yet, the ten-Gate count is the schema's rule rather
than the equations', and a fixture naming a real Release_Set would be the remembered membership the
equations exist to derive. So the Gates come from :data:`GATE_IDS` by position and the screens are
fictional identifiers.

Requirements 1.3, 1.4, 1.10, 1.12, 2.5, 3.7, 3.8, 3.10 and 5.7: a screen is Released exactly when no
Gate-classified entry of its blocking list names an open Gate, a Gate is open exactly when one of
its parts is unaccepted, and each derivation names the screen and the Gate a caller has to report.
"""

from __future__ import annotations

from dataclasses import replace
from typing import TYPE_CHECKING

import pytest
from common.gate_register import (
    DELIVERED_STATUSES,
    GATE_CLASSIFICATION,
    GATE_IDS,
    INVENTORY_COLUMNS,
    BlockingEntry,
    GateDossier,
    GatePart,
    GateRegister,
    GateRegisterError,
    InventoryReference,
    OpenStateReading,
    RationaleReference,
    ReadOnlyPosture,
    ScreenInventoryError,
    ScreenRecord,
    blocking_gates,
    gate_open,
    parse_screen_inventory,
    release_sets,
    released,
    two_gate_screens,
    undelivered,
)
from common.markdown import md_table, parse_md_tables

from ._helpers import REPO_ROOT

if TYPE_CHECKING:
    from collections.abc import Mapping, Sequence

    from common.gate_register import ScreenInventory
    from common.markdown import MarkdownTable

INVENTORY_PATH = (
    REPO_ROOT
    / ".kiro"
    / "specs"
    / "05-product-completion"
    / "03-gui-information-architecture-and-screens.md"
)

# The thirteen areas the inventory declares: Shell, Live operations, Mission Studio, Vehicle,
# Telemetry, AI supervision, Fleet & profiles, Video, Review & export, Security, Settings,
# Simulation, Help & diagnostics. How many screens each one holds stays the document's own
# declaration, which the rows are counted against.
AREA_TABLES = 13

_FIXTURE_ROW = ["`SCR-ZZZ-01`", "A screen", "O", "a seam", "`DESIGNED`", "7", "-"]
_ROW_WITHOUT_AN_IDENTIFIER = ["A screen with no identifier", *_FIXTURE_ROW[1:]]
_ROW_WITHOUT_A_STATUS = [*_FIXTURE_ROW[:4], "DESIGNED", *_FIXTURE_ROW[5:]]


def _inventory_text() -> str:
    return INVENTORY_PATH.read_text(encoding="utf-8")


def _parse_inventory() -> ScreenInventory:
    return parse_screen_inventory(_inventory_text(), path=INVENTORY_PATH.name)


def _only_table(tables: list[MarkdownTable], headers: tuple[str, ...]) -> MarkdownTable:
    """Return the one table with those headers, failing if the document no longer carries it."""
    matches = [table for table in tables if table.headers == headers]
    assert len(matches) == 1, f"expected one table headed {headers}, found {len(matches)}"
    return matches[0]


def _rows_that_look_like_screen_rows(table: MarkdownTable) -> int:
    """Count the rows whose first cell cites a screen identifier.

    This is the heuristic the parser deliberately does not use, and counting it is how the tests
    show that anchoring on the area headings reads fewer rows than the shape of a row would.
    """
    return sum(1 for row in table.rows if row.cells and "SCR-" in row.cells[0])


def _fixture_document(*body: str) -> str:
    return "\n".join(body) + "\n"


def test_inventory_rows_cover_the_thirteen_area_tables() -> None:
    inventory = _parse_inventory()

    assert len(inventory.areas) == AREA_TABLES
    for area in inventory.areas:
        assert len(area.rows) == area.declared_screens, area.heading
    # The rows are the evidence for the total, and the thirteen headings are what it is counted
    # against. Nothing here remembers the number.
    assert len(inventory.rows) == sum(area.declared_screens for area in inventory.areas)


def test_inventory_rows_exclude_the_misread_blocker_and_totals_tables() -> None:
    text = _inventory_text()
    inventory = parse_screen_inventory(text, path=INVENTORY_PATH.name)
    tables = parse_md_tables(text)

    misread = _only_table(tables, ("Screen", "Recorded blocker", "What it actually was"))
    totals = _only_table(tables, ("Status", "Count", "Screens"))

    # Both tables cite screen identifiers, so neither is being excluded by accident.
    assert _rows_that_look_like_screen_rows(misread) == len(misread.rows)
    assert any("SCR-" in cell for row in totals.rows for cell in row.cells)

    # Reading every row that looks like a screen row would take the misread table's five as well.
    looks_like_a_screen_row = sum(_rows_that_look_like_screen_rows(table) for table in tables)
    assert looks_like_a_screen_row == len(inventory.rows) + len(misread.rows)

    screens = [row.screen for row in inventory.rows]
    assert sorted(screens) == sorted(set(screens))


def test_inventory_rows_carry_the_status_and_the_blocking_tokens_of_the_real_cells() -> None:
    rows = {row.screen: row for row in _parse_inventory().rows}

    telemetry = rows["SCR-TEL-01"]
    assert telemetry.status == "DESIGNED"
    assert telemetry.blocking_tokens == frozenset({"Q4-002"})
    # The cell is kept as written: an entry carrying no identifier is matched on its text.
    assert "nothing to explore" in telemetry.blocked_by

    # A status written in bold, and a status written as two halves: each cell leads with the status.
    assert rows["SCR-HLP-01"].status == "SUPERSEDED"
    assert rows["SCR-MIS-09"].status == "IMPLEMENTED-UNCOMPILED"

    # A cell that records no blocker carries no token.
    assert rows["SCR-SHL-01"].blocking_tokens == frozenset()


def test_inventory_rows_read_identifiers_and_not_the_other_code_spans_of_a_cell() -> None:
    cell = "`P7-ZZZ-001` and `Q-029`; neither `DESIGNED` nor `AeroraFooTest` is a blocking entry"
    document = _fixture_document(
        "### Shell -- 1 screen",
        "",
        md_table(INVENTORY_COLUMNS, [[*_FIXTURE_ROW[:-1], cell]]),
    )

    (row,) = parse_screen_inventory(document).rows

    assert row.screen == "SCR-ZZZ-01"
    assert row.status == "DESIGNED"
    assert row.blocked_by == cell
    assert row.blocking_tokens == frozenset({"P7-ZZZ-001", "Q-029"})


def test_inventory_rows_fail_when_the_document_carries_no_area_table() -> None:
    document = _fixture_document(
        "## Screen inventory",
        "",
        md_table(INVENTORY_COLUMNS, [_FIXTURE_ROW]),
    )

    with pytest.raises(ScreenInventoryError, match="no area table heading"):
        parse_screen_inventory(document)


def test_inventory_rows_fail_when_the_area_tables_carry_no_row() -> None:
    document = _fixture_document(
        "### Shell -- 1 screen",
        "",
        md_table(INVENTORY_COLUMNS, []),
    )

    with pytest.raises(ScreenInventoryError, match="carry no screen row"):
        parse_screen_inventory(document)


def test_inventory_rows_fail_naming_the_line_of_an_area_heading_with_no_table() -> None:
    document = _fixture_document("# Inventory", "", "### Shell -- 4 screens", "", "Prose only.")

    with pytest.raises(ScreenInventoryError, match=r"screen-inventory:3: the area 'Shell -- 4"):
        parse_screen_inventory(document)


def test_inventory_rows_fail_naming_the_line_of_a_table_with_other_columns() -> None:
    document = _fixture_document(
        "# Inventory",
        "",
        "### Shell -- 1 screen",
        "",
        md_table(["Screen", "Recorded blocker"], [["`SCR-ZZZ-01`", "`P7-ZZZ-001`"]]),
    )

    with pytest.raises(ScreenInventoryError, match=r"screen-inventory:5: expected the columns"):
        parse_screen_inventory(document)


def test_inventory_rows_fail_naming_the_line_of_a_row_with_a_missing_cell() -> None:
    document = _fixture_document(
        "### Shell -- 1 screen",
        "",
        md_table(INVENTORY_COLUMNS, []),
        "| `SCR-ZZZ-01` | A screen |",
    )

    message = r"screen-inventory:5: expected 7 cells, found 2"
    with pytest.raises(ScreenInventoryError, match=message):
        parse_screen_inventory(document)


@pytest.mark.parametrize(
    ("cells", "message"),
    [
        (_ROW_WITHOUT_AN_IDENTIFIER, r"screen-inventory:5: the ID cell"),
        (_ROW_WITHOUT_A_STATUS, r"screen-inventory:5: the Status cell"),
    ],
    ids=["no-screen-identifier", "no-delivery-status"],
)
def test_inventory_rows_fail_naming_the_line_of_an_unreadable_row(
    cells: list[str], message: str
) -> None:
    document = _fixture_document(
        "### Shell -- 1 screen",
        "",
        md_table(INVENTORY_COLUMNS, [cells]),
    )

    with pytest.raises(ScreenInventoryError, match=message):
        parse_screen_inventory(document)


# ---------------------------------------------------------------------------
# The derivation equations
# ---------------------------------------------------------------------------

# Three Gates by position. Which three does not matter to any equation, and taking them by position
# is how these tests show that: no derivation reads a Gate identifier, only the dossier it keys.
GATE_A, GATE_B, GATE_C = GATE_IDS[:3]

# The Gate the read-only posture reads, which the schema pins. No derivation under test reads it;
# the fixtures carry a posture because the register does.
READ_ONLY_GATE = "DG-ROL"

SCREEN_A = "SCR-ZZZ-01"
SCREEN_B = "SCR-ZZZ-02"
SCREEN_C = "SCR-ZZZ-03"

ACCEPTANCE_CRITERION = "Acceptance_Criterion"


def _part(part: str, *, accepted: bool) -> GatePart:
    return GatePart(
        id=part,
        signing_owners=["Product"],
        state="accepted" if accepted else "open",
        closure_record=f"closures/{part}.json" if accepted else None,
        elements=[],
        sign_off_conditions=[],
    )


def _dossier(
    gate: str,
    parts: Mapping[str, bool] | None = None,
    *,
    status: str | None = None,
) -> GateDossier:
    """Build a Gate_Dossier whose parts are the mapping's keys, accepted where its value is true.

    *status* overrides the recorded status, which is what a register that disagrees with its own
    parts looks like. No derivation reads it.
    """
    recorded = {"whole": False} if parts is None else dict(parts)
    return GateDossier(
        id=gate,
        status=status or ("CLOSED" if all(recorded.values()) else "OPEN"),
        implementing_owner="Product",
        rationale=RationaleReference(document=INVENTORY_PATH.name, anchor="#an-anchor"),
        parts=[_part(part, accepted=accepted) for part, accepted in recorded.items()],
        release_set=[],
        excluded_acceptance_criteria=[],
        guards=[],
        rests_on_review_alone=True,
        open_state_reading=OpenStateReading(
            state_class="a recorded class", reports_gate_identifier=True
        ),
    )


def _entry(
    entry: str, *, gate: str | None = None, classification: str | None = None
) -> BlockingEntry:
    """Build a blocking entry, classified as a Gate exactly when it names one.

    *classification* overrides that, which is how a Gate-classified entry naming no Gate is built.
    """
    return BlockingEntry(
        id=entry,
        classification=classification
        or (GATE_CLASSIFICATION if gate is not None else ACCEPTANCE_CRITERION),
        gate=gate,
        reclassified_from=None,
        requirement=None,
        inventory_text=None,
    )


def _screen(screen: str, *entries: BlockingEntry, recorded_release: bool = False) -> ScreenRecord:
    return ScreenRecord(
        id=screen,
        released=recorded_release,
        observation=None,
        blocking_entries=list(entries),
    )


def _register(
    gates: Sequence[GateDossier] = (),
    screens: Sequence[ScreenRecord] = (),
) -> GateRegister:
    """Build a register carrying only what the equations read.

    The ten-Gate count is the schema's rule, so a fixture holding one Gate is enough to derive over:
    an equation that needed ten would be remembering a count instead of reading the register.
    """
    return GateRegister(
        schema_ref=None,
        inventory=InventoryReference(
            path=INVENTORY_PATH.name, authoritative_for=["the screen set"]
        ),
        gates=list(gates),
        screens=list(screens),
        guards=[],
        read_only=ReadOnlyPosture(
            gate=READ_ONLY_GATE,
            exceptions=[],
            forbidden_until=[],
            pre_approved_undeclared_seams=[],
        ),
        contract_surface=[],
    )


def _inventory_row(screen: str, status: str) -> list[str]:
    return [f"`{screen}`", "A screen", "O", "a seam", f"`{status}`", "7", "-"]


def _fixture_inventory(*rows: list[str]) -> ScreenInventory:
    document = _fixture_document(
        f"### An area -- {len(rows)} screens",
        "",
        md_table(INVENTORY_COLUMNS, rows),
    )
    return parse_screen_inventory(document)


def test_gate_open_names_every_unaccepted_part() -> None:
    register = _register([_dossier(GATE_A, {"one": True, "two": False, "three": False})])

    openness = gate_open(register)[GATE_A]

    assert openness.gate == GATE_A
    assert openness.is_open
    assert openness.unaccepted_parts == ("two", "three")


def test_gate_open_has_no_special_case_at_one_part() -> None:
    one_part_open = _register([_dossier(GATE_A, {"whole": False})])
    one_part_accepted = _register([_dossier(GATE_A, {"whole": True})])
    four_parts_one_unaccepted = _register(
        [_dossier(GATE_A, {"one": True, "two": True, "three": True, "four": False})]
    )
    four_parts_accepted = _register(
        [_dossier(GATE_A, {"one": True, "two": True, "three": True, "four": True})]
    )

    assert gate_open(one_part_open)[GATE_A].is_open
    assert not gate_open(one_part_accepted)[GATE_A].is_open
    assert gate_open(four_parts_one_unaccepted)[GATE_A].unaccepted_parts == ("four",)
    assert not gate_open(four_parts_accepted)[GATE_A].is_open
    assert gate_open(four_parts_accepted)[GATE_A].unaccepted_parts == ()


def test_gate_open_reads_a_submitted_part_as_unaccepted() -> None:
    submitted = GatePart(
        id="whole",
        signing_owners=["Product"],
        state="submitted",
        closure_record=None,
        elements=[],
        sign_off_conditions=[],
    )

    openness = gate_open(_register([replace(_dossier(GATE_A), parts=[submitted])]))[GATE_A]

    assert openness.is_open
    assert openness.unaccepted_parts == ("whole",)


def test_gate_open_does_not_read_the_recorded_gate_status() -> None:
    disagrees = _register([_dossier(GATE_A, {"one": True, "two": False}, status="CLOSED")])
    also_disagrees = _register([_dossier(GATE_A, {"one": True}, status="OPEN")])

    assert gate_open(disagrees)[GATE_A].is_open
    assert not gate_open(also_disagrees)[GATE_A].is_open


def test_gate_open_fails_naming_a_gate_that_records_no_part() -> None:
    register = _register([_dossier(GATE_A, {})])

    with pytest.raises(GateRegisterError, match=f"{GATE_A}: the Gate_Dossier records no part"):
        gate_open(register)


def test_gate_open_fails_naming_a_gate_recorded_twice() -> None:
    register = _register([_dossier(GATE_A), _dossier(GATE_A)])

    with pytest.raises(GateRegisterError, match=f"{GATE_A}: the register records more than one"):
        gate_open(register)


def test_release_sets_transpose_the_gate_classified_entries() -> None:
    register = _register(
        [_dossier(GATE_A), _dossier(GATE_B), _dossier(GATE_C)],
        [
            _screen(SCREEN_A, _entry("P7-ZZZ-001", gate=GATE_A)),
            _screen(SCREEN_B, _entry("P7-ZZZ-002", gate=GATE_A)),
            _screen(SCREEN_C, _entry("P7-ZZZ-003", gate=GATE_B)),
        ],
    )

    derived = release_sets(register)

    assert derived[GATE_A] == frozenset({SCREEN_A, SCREEN_B})
    assert derived[GATE_B] == frozenset({SCREEN_C})
    # A Gate no screen names has an empty Release_Set, and is still one of the register's Gates.
    assert derived[GATE_C] == frozenset()


def test_release_sets_read_two_entries_naming_one_gate_as_one_gate() -> None:
    register = _register(
        [_dossier(GATE_A)],
        [_screen(SCREEN_A, _entry("P7-ZZZ-001", gate=GATE_A), _entry("P7-ZZZ-002", gate=GATE_A))],
    )

    assert blocking_gates(register)[SCREEN_A] == (GATE_A,)
    assert release_sets(register)[GATE_A] == frozenset({SCREEN_A})
    assert two_gate_screens(register) == {}


def test_release_sets_ignore_an_acceptance_criterion_entry() -> None:
    register = _register(
        [_dossier(GATE_A)],
        [
            _screen(SCREEN_A, _entry("P7-ZZZ-001", gate=GATE_A), _entry("Q4-002")),
            _screen(SCREEN_B, _entry("Q4-003"), _entry("Q4-004")),
        ],
    )

    assert blocking_gates(register) == {SCREEN_A: (GATE_A,), SCREEN_B: ()}
    assert release_sets(register)[GATE_A] == frozenset({SCREEN_A})


def test_blocking_gates_leave_a_screen_carrying_three_gates_visible() -> None:
    register = _register(
        [_dossier(GATE_A), _dossier(GATE_B), _dossier(GATE_C)],
        [
            _screen(
                SCREEN_A,
                _entry("P7-ZZZ-003", gate=GATE_C),
                _entry("P7-ZZZ-001", gate=GATE_A),
                _entry("P7-ZZZ-002", gate=GATE_B),
            )
        ],
    )

    # The order is the Gate identifier order, not the order the entries happen to be recorded in.
    assert blocking_gates(register)[SCREEN_A] == (GATE_A, GATE_B, GATE_C)
    # Three is not two, so the screen is no Two_Gate_Screen. Rejecting the third is the register
    # check's finding, which it reports from here.
    assert two_gate_screens(register) == {}


def test_blocking_gates_fail_naming_an_entry_classified_as_a_gate_that_names_none() -> None:
    register = _register(
        [_dossier(GATE_A)],
        [_screen(SCREEN_A, _entry("P7-ZZZ-001", classification=GATE_CLASSIFICATION))],
    )

    message = f"{SCREEN_A}: the blocking entry 'P7-ZZZ-001' is classified Gate and names no Gate"
    with pytest.raises(GateRegisterError, match=message):
        blocking_gates(register)


def test_blocking_gates_fail_naming_an_entry_whose_gate_has_no_dossier() -> None:
    register = _register([_dossier(GATE_A)], [_screen(SCREEN_A, _entry("P7-ZZZ-001", gate=GATE_B))])

    message = f"{SCREEN_A}: the blocking entry 'P7-ZZZ-001' names {GATE_B}, which the register"
    with pytest.raises(GateRegisterError, match=message):
        blocking_gates(register)


def test_blocking_gates_fail_naming_a_screen_recorded_twice() -> None:
    register = _register([_dossier(GATE_A)], [_screen(SCREEN_A), _screen(SCREEN_A)])

    message = f"{SCREEN_A}: the register records more than one screen record"
    with pytest.raises(GateRegisterError, match=message):
        blocking_gates(register)


@pytest.mark.parametrize(
    ("first_accepted", "second_accepted", "expected_open_gates"),
    [
        (False, False, (GATE_A, GATE_B)),
        (True, False, (GATE_B,)),
        (False, True, (GATE_A,)),
        (True, True, ()),
    ],
    ids=["neither", "first-only", "second-only", "both"],
)
def test_released_is_the_absence_of_an_open_gate_classified_entry(
    first_accepted: bool, second_accepted: bool, expected_open_gates: tuple[str, ...]
) -> None:
    register = _register(
        [
            _dossier(GATE_A, {"whole": first_accepted}),
            _dossier(GATE_B, {"whole": second_accepted}),
        ],
        [_screen(SCREEN_A, _entry("P7-ZZZ-001", gate=GATE_A), _entry("P7-ZZZ-002", gate=GATE_B))],
    )

    release = released(register)[SCREEN_A]

    assert release.screen == SCREEN_A
    assert release.open_gates == expected_open_gates
    # A screen held by two Gates is Released after the second closes, whichever closes first.
    assert release.released == (expected_open_gates == ())


def test_released_is_true_for_a_screen_with_no_gate_classified_entry() -> None:
    register = _register(
        [_dossier(GATE_A, {"whole": False})],
        [
            _screen(SCREEN_A),
            _screen(SCREEN_B, _entry("Q4-002"), _entry("Q4-003")),
        ],
    )

    releases = released(register)

    assert releases[SCREEN_A].released
    assert releases[SCREEN_A].open_gates == ()
    # The one open Gate holds neither screen, because neither screen's blocking list names it.
    assert releases[SCREEN_B].released
    assert releases[SCREEN_B].open_gates == ()


def test_released_names_the_open_gates_in_the_gate_identifier_order() -> None:
    entries = (_entry("P7-ZZZ-002", gate=GATE_B), _entry("P7-ZZZ-001", gate=GATE_A))
    register = _register(
        [_dossier(GATE_A, {"whole": False}), _dossier(GATE_B, {"whole": False})],
        [_screen(SCREEN_A, *entries), _screen(SCREEN_B, *reversed(entries))],
    )

    releases = released(register)

    assert releases[SCREEN_A].open_gates == (GATE_A, GATE_B)
    assert releases[SCREEN_B].open_gates == (GATE_A, GATE_B)


def test_released_reads_part_acceptance_rather_than_the_recorded_gate_status() -> None:
    register = _register(
        [_dossier(GATE_A, {"one": True, "two": False}, status="CLOSED")],
        [_screen(SCREEN_A, _entry("P7-ZZZ-001", gate=GATE_A), recorded_release=True)],
    )

    release = released(register)[SCREEN_A]

    assert not release.released
    assert release.open_gates == (GATE_A,)


def test_two_gate_screens_are_the_screens_carrying_two_distinct_gates() -> None:
    register = _register(
        [_dossier(GATE_A), _dossier(GATE_B)],
        [
            _screen(SCREEN_A, _entry("P7-ZZZ-001", gate=GATE_A)),
            _screen(SCREEN_B, _entry("P7-ZZZ-002", gate=GATE_B), _entry("P7-ZZZ-003", gate=GATE_A)),
            _screen(SCREEN_C, _entry("Q4-002")),
        ],
    )

    assert two_gate_screens(register) == {SCREEN_B: (GATE_A, GATE_B)}


def test_undelivered_removes_the_delivered_rows() -> None:
    inventory = _fixture_inventory(
        _inventory_row(SCREEN_A, "VERIFIED"),
        _inventory_row(SCREEN_B, "SUPERSEDED"),
        _inventory_row(SCREEN_C, "DESIGNED"),
    )

    assert undelivered(inventory) == frozenset({SCREEN_C})


def test_undelivered_removes_nothing_from_an_inventory_with_no_delivered_row() -> None:
    inventory = _fixture_inventory(
        _inventory_row(SCREEN_A, "DESIGNED"),
        _inventory_row(SCREEN_B, "IMPLEMENTED-UNCOMPILED"),
    )

    assert undelivered(inventory) == frozenset({SCREEN_A, SCREEN_B})


def test_undelivered_partitions_the_real_inventory_rows() -> None:
    inventory = _parse_inventory()

    screens = {row.screen for row in inventory.rows}
    delivered = {row.screen for row in inventory.rows if row.status in DELIVERED_STATUSES}
    undelivered_screens = undelivered(inventory)

    # No row in both and none in neither, and no count anywhere: the rows are the evidence.
    assert undelivered_screens | delivered == screens
    assert not undelivered_screens & delivered
    assert delivered
    assert undelivered_screens


def test_undelivered_fails_naming_a_screen_carrying_two_rows() -> None:
    inventory = _fixture_inventory(
        _inventory_row(SCREEN_A, "DESIGNED"),
        _inventory_row(SCREEN_A, "VERIFIED"),
    )

    with pytest.raises(ScreenInventoryError, match=f"{SCREEN_A}: the inventory carries more"):
        undelivered(inventory)

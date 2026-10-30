#!/usr/bin/env python3
"""Tests for the screen inventory parser in tools/common/gate_register.py.

The document under test is the real screen inventory, because it is the authority for the screen
set: a parser checked against a fixture alone would keep passing after the document was
restructured, and every check the Gate_Review runs is a predicate over these rows.

Requirements 1.7, 2.1 and 2.10: each inventory row is a screen the register has to account for, and
the entries the register classifies are the identifier tokens of that row's "Blocked by" cell.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from common.gate_register import (
    INVENTORY_COLUMNS,
    ScreenInventoryError,
    parse_screen_inventory,
)
from common.markdown import md_table, parse_md_tables

from ._helpers import REPO_ROOT

if TYPE_CHECKING:
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

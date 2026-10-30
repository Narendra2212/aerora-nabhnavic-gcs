#!/usr/bin/env python3
"""Tests for tools/common/markdown.py."""

from __future__ import annotations

import pytest
from common.markdown import md_table, parse_md_tables


def test_basic_table() -> None:
    out = md_table(["A", "B"], [["1", "2"], ["3", "4"]])
    assert out == "| A | B |\n| --- | --- |\n| 1 | 2 |\n| 3 | 4 |"


def test_no_trailing_newline() -> None:
    assert not md_table(["A"], [["1"]]).endswith("\n")


def test_empty_rows_keeps_header_and_separator() -> None:
    assert md_table(["A", "B"], []) == "| A | B |\n| --- | --- |"


def test_cells_are_stringified() -> None:
    assert "| Count | 42 |" in md_table(["K", "V"], [["Count", 42]])


def test_alignment_separators() -> None:
    out = md_table(["A", "B", "C"], [], align=["left", "right", "center"])
    assert out.splitlines()[1] == "| --- | ---: | :---: |"


def test_align_length_mismatch_raises() -> None:
    with pytest.raises(ValueError, match="align must match headers length"):
        md_table(["A", "B"], [], align=["left"])


def test_invalid_align_value_raises() -> None:
    with pytest.raises(ValueError, match="invalid align value"):
        md_table(["A"], [], align=["centre"])


def test_row_length_mismatch_raises() -> None:
    with pytest.raises(ValueError, match="row 1 has 1 cells; expected 2"):
        md_table(["A", "B"], [["1", "2"], ["3"]])


def test_crlf_flattened_to_br() -> None:
    assert "| a<br>b |" in md_table(["H"], [["a\r\nb"]])
    assert "\r" not in md_table(["H"], [["a\rb"]])


def test_parse_md_tables_reads_back_what_md_table_wrote() -> None:
    rows = [["1", "2"], ["3", "4"]]
    (table,) = parse_md_tables(md_table(["A", "B"], rows))
    assert table.headers == ("A", "B")
    assert [list(row.cells) for row in table.rows] == rows


def test_parse_md_tables_reports_one_based_lines() -> None:
    document = f"# Title\n\n{md_table(['A'], [['1'], ['2']])}\n"
    (table,) = parse_md_tables(document)
    assert table.line == 3
    assert [row.line for row in table.rows] == [5, 6]


def test_parse_md_tables_undoes_the_cell_escaping() -> None:
    (table,) = parse_md_tables(md_table(["H"], [["a|b"], ["c\\d"]]))
    assert [row.cells[0] for row in table.rows] == ["a|b", "c\\d"]


def test_parse_md_tables_keeps_a_windows_path_as_written() -> None:
    (table,) = parse_md_tables("| H |\n| --- |\n| C:\\new |\n")
    assert table.rows[0].cells == ("C:\\new",)


def test_parse_md_tables_skips_a_fenced_table() -> None:
    document = f"```markdown\n{md_table(['A'], [['1']])}\n```\n"
    assert parse_md_tables(document) == []


def test_parse_md_tables_requires_a_delimiter_row() -> None:
    assert parse_md_tables("| not | a table |\n| nor | this |\n") == []


def test_parse_md_tables_keeps_a_ragged_row_for_the_caller() -> None:
    (table,) = parse_md_tables("| A | B |\n| --- | --- |\n| 1 |\n")
    assert table.rows[0].cells == ("1",)


def test_parse_md_tables_finds_every_table_in_order() -> None:
    document = f"{md_table(['A'], [['1']])}\n\n{md_table(['B'], [['2'], ['3']])}\n"
    assert [(table.headers, len(table.rows)) for table in parse_md_tables(document)] == [
        (("A",), 1),
        (("B",), 2),
    ]

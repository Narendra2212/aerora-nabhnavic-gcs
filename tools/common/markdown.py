"""GitHub-flavoured Markdown table builder and reader.

:func:`md_table` replaces hand-rolled ``header`` + ``|---|`` separator + row-join blocks in
ccache_helper, size_analysis, collect_build_status, gstreamer_archive, coverage_comment and
test_duration_report.

:func:`parse_md_tables` is its inverse over a whole document: it finds the tables and undoes the
cell escaping the builder applies, so a table written here reads back as the rows that went in. It
keeps the line each header and row came from, because a caller that rejects one has to say which.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import Iterable, Sequence

__all__ = ["MarkdownRow", "MarkdownTable", "md_table", "parse_md_tables"]

_ALIGN_SEP = {"left": "---", "right": "---:", "center": ":---:"}


def _cell(value: object) -> str:
    # A literal pipe or newline in a cell breaks the row; escape/flatten them.
    text = str(value).replace("\\", "\\\\").replace("|", "\\|")
    return text.replace("\r\n", "<br>").replace("\n", "<br>").replace("\r", "<br>")


def md_table(
    headers: Sequence[str],
    rows: Iterable[Sequence[object]],
    *,
    align: Sequence[str] | None = None,
) -> str:
    """Return a GitHub-flavoured Markdown table (no trailing newline).

    Cells are stringified. *align* is an optional per-column list of
    ``"left"``/``"right"``/``"center"``; defaults to all-left.
    """
    aligns = list(align) if align is not None else ["left"] * len(headers)
    if len(aligns) != len(headers):
        raise ValueError("align must match headers length")
    bad = [a for a in aligns if a not in _ALIGN_SEP]
    if bad:
        raise ValueError(f"invalid align value(s) {bad}; expected one of {sorted(_ALIGN_SEP)}")
    sep = [_ALIGN_SEP[a] for a in aligns]
    out = [
        "| " + " | ".join(_cell(h) for h in headers) + " |",
        "| " + " | ".join(sep) + " |",
    ]
    for i, row in enumerate(rows):
        cells = list(row)
        if len(cells) != len(headers):
            raise ValueError(f"row {i} has {len(cells)} cells; expected {len(headers)}")
        out.append("| " + " | ".join(_cell(cell) for cell in cells) + " |")
    return "\n".join(out)


# An opener and a closer look the same, so the reader toggles on each one.
_FENCE = re.compile(r"^\s{0,3}(?:```|~~~)")
_DELIMITER_CELL = re.compile(r":?-+:?")


@dataclass(frozen=True)
class MarkdownRow:
    """One body row of a table, with the 1-based line it was read from."""

    cells: tuple[str, ...]
    line: int


@dataclass(frozen=True)
class MarkdownTable:
    """One table read out of a document, with the 1-based line of its header row."""

    headers: tuple[str, ...]
    rows: tuple[MarkdownRow, ...]
    line: int


def _is_row(line: str) -> bool:
    """Report whether *line* is a table row.

    Both outer pipes are required. GitHub accepts a row without them, but :func:`md_table` writes
    both and every table in this repository carries both, so requiring them keeps prose that merely
    contains a pipe out of a table.
    """
    text = line.strip()
    return len(text) > 1 and text.startswith("|") and text.endswith("|")


def _split_cells(line: str) -> list[str]:
    """Split one row on its unescaped pipes, undoing :func:`_cell`'s escaping.

    Any backslash sequence other than ``\\\\`` and ``\\|`` is left as written: a Windows path in a
    cell is not an escape. ``<br>`` is left as written too, because a cell may carry that text and
    the flattening of a newline into it is not reversible.
    """
    cells: list[str] = []
    current: list[str] = []
    escaped = False
    for char in line.strip()[1:-1]:
        if escaped:
            current.append(char if char in "\\|" else "\\" + char)
            escaped = False
        elif char == "\\":
            escaped = True
        elif char == "|":
            cells.append("".join(current).strip())
            current = []
        else:
            current.append(char)
    if escaped:
        current.append("\\")
    cells.append("".join(current).strip())
    return cells


def _is_delimiter(cells: list[str]) -> bool:
    return bool(cells) and all(_DELIMITER_CELL.fullmatch(cell) for cell in cells)


def parse_md_tables(text: str) -> list[MarkdownTable]:
    """Return every table in *text*, in document order, with 1-based line numbers.

    A table is a row, a delimiter row, and the rows that follow until the first line that is not a
    row. Rows inside a fenced code block are skipped: a fenced table is an example of the markup,
    not a table of the document.

    Each header and row carries the cells it was written with, however many that is. What a missing
    or an extra cell means belongs to the caller, which knows the columns it expects and can name
    the line.
    """
    lines = text.splitlines()
    tables: list[MarkdownTable] = []
    index = 0
    fenced = False
    while index < len(lines):
        if _FENCE.match(lines[index]):
            fenced = not fenced
            index += 1
            continue
        following = lines[index + 1] if index + 1 < len(lines) else ""
        if fenced or not _is_row(lines[index]) or not _is_row(following):
            index += 1
            continue
        if not _is_delimiter(_split_cells(following)):
            index += 1
            continue
        rows: list[MarkdownRow] = []
        cursor = index + 2
        while cursor < len(lines) and _is_row(lines[cursor]):
            rows.append(MarkdownRow(cells=tuple(_split_cells(lines[cursor])), line=cursor + 1))
            cursor += 1
        tables.append(
            MarkdownTable(
                headers=tuple(_split_cells(lines[index])),
                rows=tuple(rows),
                line=index + 1,
            )
        )
        index = cursor
    return tables

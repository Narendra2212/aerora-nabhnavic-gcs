"""Validate the Aerora Gate_Register against the screen inventory and its own schema.

The data half of the Gate_Review. It reads the register, the schema beside it and the screen
inventory the register names, and reports every disagreement between what an owner recorded and
what the documents derive. It never writes: the only thing an auto-fix could write here is a
Released flag, a classification or a guard value that no owner signed, so a finding is reported and
the exit status is non-zero.

Premises are established before any derived check runs, and the order is the error contract's. A
missing register, a register that is not JSON, a register the schema rejects, an inventory whose
area tables cannot be parsed, or an inventory carrying no screen row each stop the run there and
are reported as themselves. The governing rule is the one ``aerora-manifest-check.ps1`` states for
its own ``ASSUMED`` rows: **an unresolved premise is a failure, never a pass.** A derived check over
a malformed register would report noise that hides the real error, and one over an inventory
silently parsed as zero rows would satisfy every predicate over those rows trivially.

This module establishes those premises. Every derived check the design's table names is a predicate
over :class:`Premises`, reporting :class:`Finding` values that carry its own check name, so that one
stream of output says which rule rejected which record.
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import TYPE_CHECKING, Any, Final

from common.gate_register import (
    ACCEPTANCE_CRITERION_CLASSIFICATION,
    DELIVERED_STATUSES,
    GATE_CLASSIFICATION,
    GATE_IDS,
    GateRegisterError,
    ScreenInventoryError,
    blocking_gates,
    is_entry_id,
    load_gate_register,
    parse_screen_inventory,
    release_sets,
    released,
    two_gate_screens,
    undelivered,
)
from common.markdown import parse_md_tables
from jsonschema import Draft202012Validator
from jsonschema.exceptions import SchemaError

if TYPE_CHECKING:
    from collections.abc import Iterable, Mapping, Sequence

    from common.gate_register import GateRegister, Observation, ScreenInventory, ScreenRelease
    from common.markdown import MarkdownTable

ROOT: Final = Path(__file__).resolve().parents[1]

# Repository-relative paths, used verbatim in every message, so a finding reads the same way on the
# authoring host and in CI. The inventory is not among them: the register names it, and naming it
# here as well would make this module a second authority for where it lives.
_SPEC_DIR: Final = ".kiro/specs/aerora-decision-gate-closure"
REGISTER_PATH: Final = f"{_SPEC_DIR}/gate-register.json"
REGISTER_SCHEMA_PATH: Final = f"{_SPEC_DIR}/gate-register.schema.json"

# The two checks that run before any derived one, kept apart from them so that a premise which
# cannot be established is reported as itself rather than as the many findings a missing register
# would otherwise produce.
PREMISES: Final = "premises"
SCHEMA: Final = "schema"

PARTITION: Final = "partition"
RELEASE_SET_DERIVATION: Final = "release-set-derivation"
TWO_GATE: Final = "two-gate"
RELEASED_DERIVATION: Final = "released-derivation"
DELIVERED_OUTSIDE: Final = "delivered-outside"
INVENTORY_COVERAGE: Final = "inventory-coverage"
ENTRY_CLASSIFICATION: Final = "entry-classification"
RECLASSIFICATIONS: Final = "reclassifications"
VERIFIED_OBSERVATION: Final = "verified-observation"
INVENTORY_TOTALS: Final = "inventory-totals"

# Requirement 1 criterion 5: the four Two_Gate_Screens, each with the two Gates whose Release_Sets
# must both carry it. Recorded here rather than derived from the register, because a check that took
# its expectation from the document it checks agrees with that document by construction.
TWO_GATE_SCREENS: Final[Mapping[str, frozenset[str]]] = {
    "SCR-LIV-05": frozenset({"DG-ROL", "DG-AUT"}),
    "SCR-AIS-03": frozenset({"DG-AUT", "DG-STO"}),
    "SCR-MIS-05": frozenset({"DG-MIS", "DG-PAY"}),
    "SCR-MIS-06": frozenset({"DG-MIS", "DG-PAY"}),
}

# Requirement 2 criterion 8: the undelivered screens that appear in no Release_Set, because every
# entry of each one's blocking list classifies as an Acceptance_Criterion.
RELEASED_WITH_NO_GATE: Final = frozenset(
    {"SCR-LIV-08", "SCR-LIV-09", "SCR-SET-01", "SCR-SET-03", "SCR-SET-04"}
)

# Requirement 2 criterion 7: the five screens whose blocking entry was misread, each with the entry
# the register must record reclassified from a Gate to an Acceptance_Criterion. Recorded here for
# the reason RELEASED_WITH_NO_GATE is: a check that read its expectation out of the register would
# agree with the register by construction.
RECLASSIFIED_ENTRIES: Final[Mapping[str, str]] = {
    "SCR-VID-01": "P7-VID-004",
    "SCR-VEH-03": "Q3-009",
    "SCR-VEH-04": "P7-VEH-004",
    "SCR-SEC-01": "P7-SEC-005",
    "SCR-HLP-03": "P7-HELP-002",
}

# Requirement 2 criterion 11: what each of those five retains once the misread entry is
# reclassified. Four retain no entry classified as a Gate; SCR-VID-01 retains one, which is the
# absent seam recorded against DG-VID.
RETAINED_GATES: Final[Mapping[str, tuple[str, ...]]] = {
    "SCR-VID-01": ("DG-VID",),
    "SCR-VEH-03": (),
    "SCR-VEH-04": (),
    "SCR-SEC-01": (),
    "SCR-HLP-03": (),
}

# The screen inventory is Tab 7's document, so a disagreement between the totals it declares and
# the rows it carries is Tab 7's to settle. `inventory-totals` names the owner and takes no side:
# the register records no count, so the check has nothing to prefer.
INVENTORY_OWNER: Final = "Tab 7"

# The two members of DELIVERED_STATUSES by name. The Inventory totals table declares a count per
# member, so the check needs each separately; the vocabulary is the inventory's and nothing here
# adds to it.
VERIFIED_STATUS: Final = "VERIFIED"
SUPERSEDED_STATUS: Final = "SUPERSEDED"

# The Inventory totals table's columns, which is how the check finds that table rather than
# counting tables or anchoring on the heading above it, and the Status-cell text identifying each
# of the two rows that is not a status name. The table's remaining rows -- the fork count and the
# unenumerated upstream surface -- declare nothing about these rows, so nothing derives them.
TOTALS_COLUMNS: Final = ("Status", "Count", "Screens")
NOT_DELIVERED_LABEL: Final = "Not delivered"
TOTAL_LABEL: Final = "Total Aerora screens"

# The three document-level declarations, each anchored on the words around the number rather than
# on the number, so a stale figure is compared and a reworded declaration is reported as unreadable
# instead of quietly ceasing to be compared.
_LEAD_LINE: Final = re.compile(r"^(?P<screens>\d+) screens? across (?P<areas>\d+) areas?\b", re.M)
_UNDELIVERED_PARAGRAPH: Final = re.compile(r"\bThe (?P<undelivered>\d+) undelivered screens\b")
_ENTRY_COUNT: Final = re.compile(r"\bThe screen inventory has (?P<entries>\d+) entries\b")

_COUNT: Final = re.compile(r"\d+")

# How a finding names each declared site, and the two row sets more than one site is counted
# against. Every figure beside them is derived at call time; these are prose, not counts.
_LEAD_LINE_SITE: Final = "the section 8 lead line"
_TOTALS_TABLE_SITE: Final = "the Inventory totals table"
_UNDELIVERED_SITE: Final = "the undelivered-screens paragraph"
_ENTRY_COUNT_SITE: Final = "the section 9 lead line"
_AREA_ROWS: Final = "the area tables' rows"
_UNDELIVERED_ROWS: Final = "the rows the inventory records undelivered"

# Where an observation must resolve, and where the registration that makes a suite run is written.
# Requirement 3 criterion 9 is about an Aerora screen navigated to and drawn in a booted
# application window, so a suite outside the overlay's own test tree witnesses no such thing.
TEST_TREE: Final = "aerora/test"
OVERLAY_TREE: Final = "aerora"

_SOURCE_SUFFIXES: Final = (".h", ".cc")
_CMAKE_PATTERNS: Final = ("CMakeLists.txt", "*.cmake")

# How this tree registers a test suite, matched rather than invented. `aerora/CMakeLists.txt` is the
# aggregation point and calls `add_qgc_test` directly; `aerora/src/Vehicle/Vehicle.cmake` documents
# `cmake_language(DEFER ... CALL add_qgc_test <name> ...)` as the equivalent a module may use from
# its own manifest, so the call name may be followed by a paren or by whitespace. Each module
# manifest exports the CTest names separately as `AERORA_<MODULE>_TEST_NAMES`, which is the list
# `.kiro/tools/aerora-manifest-check.ps1` reads in its fourth and seventh checks.
_ADD_QGC_TEST: Final = re.compile(r"\badd_qgc_test[\s(]\s*([A-Za-z0-9_]+)")
_TEST_NAMES_LIST: Final = re.compile(r"\bset\(\s*[A-Za-z0-9_]*_TEST_NAMES\b([^)]*)\)")

# A class definition rather than a forward declaration: the name is followed by a base-class list
# or by the opening brace. `class AeroraSceneAttachmentUITest;` names a type whose slots the file
# does not declare, and reading it as the declaration would resolve a slot against the wrong file.
_CLASS_END: Final = re.compile(r"^\};", re.M)
_ACCESS_SPECIFIER: Final = re.compile(
    r"^[ \t]*(?P<access>public|protected|private)(?P<slots>[ \t]+slots)?[ \t]*:", re.M
)
_SLOT_DECLARATION: Final = re.compile(r"\bvoid\s+(\w+)\s*\(")

# The inventory's Status column legend, which names the document and the anchor its vocabulary is
# declared under rather than restating the vocabulary. Anchored on the column name, so a reworded
# legend is reported as unreadable instead of quietly ceasing to be read.
_STATUS_LEGEND: Final = re.compile(
    r"\*\*Status\*\*\s+per\s+\[[^\]]*\]\((?P<document>[^)#]*)#(?P<anchor>[^)\s]*)\)"
)
_HEADING: Final = re.compile(r"^(?P<hashes>#{1,6})[ \t]+(?P<title>.+?)[ \t]*$", re.M)
_SLUG_DROP: Final = re.compile(r"[^\w\- ]+")

# A Markdown code span. The status a cell carries is its leading span and what follows is prose,
# which is how `parse_screen_inventory` reads a row's Status cell; the vocabulary tables carry the
# same shape, a status name followed by its gloss.
_CODE_SPAN: Final = re.compile(r"`([^`]+)`")

_VOCABULARY_COLUMN: Final = "Status"

_GATE_ORDER: Final = {gate: index for index, gate in enumerate(GATE_IDS)}


@dataclass(frozen=True)
class Finding:
    """One rejection, naming the check that made it.

    The check name belongs to the finding rather than to a heading over a group of them, because
    the hook prints findings in one stream and the reader has to be able to tell which rule
    rejected which record.
    """

    check: str
    message: str

    def __str__(self) -> str:
        return f"{self.check}: {self.message}"


@dataclass(frozen=True)
class Derived:
    """The derivation equations, evaluated once over the premises.

    Evaluated here rather than inside each check because four of the set checks rest on the same
    sub-expression over the Gate-classified blocking entries. An entry classified as a Gate that
    names no Gate is an unresolved premise, and deriving it once per check would report that single
    defect four times and bury it among the consequences.
    """

    blocking_gates: dict[str, tuple[str, ...]]
    release_sets: dict[str, frozenset[str]]
    releases: dict[str, ScreenRelease]
    two_gate_screens: dict[str, tuple[str, ...]]
    undelivered: frozenset[str]


@dataclass(frozen=True)
class Premises:
    """The documents every derived check reads, read and validated once each.

    ``root`` is how a check reaches a third file -- a test source, a guard declaration, git history
    -- without discovering the repository for itself.
    """

    root: Path
    register: GateRegister
    inventory: ScreenInventory
    derived: Derived


def _pointer(tokens: Sequence[object]) -> str:
    """Render *tokens* as an RFC 6901 JSON pointer.

    The empty pointer addresses the whole document and reads as nothing at the end of a message, so
    it is spelled out instead.
    """
    if not tokens:
        return "the document root"
    escaped = (str(token).replace("~", "~0").replace("/", "~1") for token in tokens)
    return "/" + "/".join(escaped)


def _read_text(root: Path, relative: str) -> tuple[str | None, list[Finding]]:
    """Read a repository file as UTF-8, naming the path and the reason on failure."""
    try:
        return (root / relative).read_text(encoding="utf-8"), []
    except FileNotFoundError:
        return None, [Finding(PREMISES, f"{relative} does not exist")]
    except OSError as error:
        return None, [Finding(PREMISES, f"{relative} could not be read: {error.strerror}")]
    except UnicodeDecodeError as error:
        return None, [Finding(PREMISES, f"{relative} is not valid UTF-8: {error.reason}")]


def _read_json(root: Path, relative: str) -> tuple[Any, list[Finding]]:
    """Read a repository file as JSON, naming the line and column of a parse error."""
    text, findings = _read_text(root, relative)
    if text is None:
        return None, findings
    try:
        return json.loads(text), []
    except json.JSONDecodeError as error:
        return None, [Finding(PREMISES, f"{relative}:{error.lineno}:{error.colno}: {error.msg}")]


def _schema_findings(document: Any, schema: Any) -> list[Finding]:
    """Validate *document* against *schema*, reporting a JSON pointer per error.

    A schema that is not itself a valid schema is a premise failure rather than a schema finding:
    nothing was validated, so reporting it as a rejection of the register would name the wrong
    document.

    The errors are ordered by the pointer they address, so a run reports the same findings in the
    same order and a reviewer can read them against the document top to bottom.
    """
    try:
        Draft202012Validator.check_schema(schema)
    except SchemaError as error:
        pointer = _pointer(error.absolute_path)
        return [Finding(PREMISES, f"{REGISTER_SCHEMA_PATH}: {pointer}: {error.message}")]

    errors = sorted(
        Draft202012Validator(schema).iter_errors(document),
        key=lambda error: ([str(token) for token in error.absolute_path], error.message),
    )
    return [
        Finding(SCHEMA, f"{REGISTER_PATH}: {_pointer(error.absolute_path)}: {error.message}")
        for error in errors
    ]


def derive(register: GateRegister, inventory: ScreenInventory) -> Derived:
    """Evaluate every derivation the checks read, raising on an unresolvable premise.

    One function, so a test building premises in code derives them the way :func:`load_premises`
    does rather than the way it remembers.
    """
    return Derived(
        blocking_gates=blocking_gates(register),
        release_sets=release_sets(register),
        releases=released(register),
        two_gate_screens=two_gate_screens(register),
        undelivered=undelivered(inventory),
    )


def load_premises(root: Path) -> Premises | list[Finding]:
    """Establish what the derived checks read, or report the premise that failed.

    A list is returned exactly when a premise could not be established, and it is never empty, so a
    caller that gets no findings has the premises.
    """
    schema, findings = _read_json(root, REGISTER_SCHEMA_PATH)
    if findings:
        return findings

    document, findings = _read_json(root, REGISTER_PATH)
    if findings:
        return findings

    findings = _schema_findings(document, schema)
    if findings:
        return findings

    try:
        register = load_gate_register(document, path=REGISTER_PATH)
    except GateRegisterError as error:
        # The schema accepted the document and the model did not, so the two disagree about the
        # register's shape. Saying so names the repair, which is to one of them rather than to the
        # register.
        return [
            Finding(
                PREMISES,
                f"{error}. The register validates against {REGISTER_SCHEMA_PATH}, so the schema "
                f"and the model disagree about its shape",
            )
        ]

    inventory_path = register.inventory.path
    text, findings = _read_text(root, inventory_path)
    if text is None:
        return findings

    try:
        # Zero screen rows raises from here, which is the vacuity the error contract rejects: every
        # check over the inventory is a predicate over these rows.
        inventory = parse_screen_inventory(text, path=inventory_path)
    except ScreenInventoryError as error:
        return [Finding(PREMISES, str(error))]

    try:
        derived = derive(register, inventory)
    except (GateRegisterError, ScreenInventoryError) as error:
        return [Finding(PREMISES, str(error))]

    return Premises(root=root, register=register, inventory=inventory, derived=derived)


def _gate_sort_key(gate: str) -> tuple[int, str]:
    """Order Gate identifiers as :data:`GATE_IDS` does, and anything else after them by name."""
    return (_GATE_ORDER.get(gate, len(GATE_IDS)), gate)


def _gates(gates: Iterable[str]) -> str:
    """Render a Gate set in the order :data:`GATE_IDS` declares it.

    A message therefore reads the same way however the register happened to order the entries that
    produced it, which is what lets a reviewer diff one run's findings against another's.
    """
    return ", ".join(sorted(gates, key=_gate_sort_key))


def _screens(screens: Iterable[str]) -> str:
    """Render a screen set in identifier order, for the same reason :func:`_gates` orders Gates."""
    return ", ".join(sorted(screens))


def _recorded_release_sets(register: GateRegister) -> dict[str, frozenset[str]]:
    """The Release_Set each Gate_Dossier records, by Gate, as screens alone.

    A screen released in halves is recorded once with the half it releases. Which half is the
    ``closure-completeness`` check's concern; membership is this one's.
    """
    return {
        dossier.id: frozenset(entry.screen for entry in dossier.release_set)
        for dossier in register.gates
    }


def check_release_set_derivation(premises: Premises) -> list[Finding]:
    """Each recorded Release_Set equals the set derived from the blocking-entry classifications.

    Both directions, screen by screen and Gate by Gate, because a recorded Release_Set that has
    gained a screen and lost another is two findings rather than one mismatch.
    """
    recorded = _recorded_release_sets(premises.register)
    findings: list[Finding] = []
    for gate, derived in premises.derived.release_sets.items():
        findings.extend(
            Finding(
                RELEASE_SET_DERIVATION,
                f"{gate} records {screen} in its Release_Set, and no blocking entry of {screen} "
                f"classifies as a Gate naming {gate}",
            )
            for screen in sorted(recorded[gate] - derived)
        )
        findings.extend(
            Finding(
                RELEASE_SET_DERIVATION,
                f"{screen} carries a blocking entry classified as a Gate naming {gate}, and "
                f"{gate} does not record {screen} in its Release_Set",
            )
            for screen in sorted(derived - recorded[gate])
        )
    return findings


def check_released_derivation(premises: Premises) -> list[Finding]:
    """Each screen's recorded ``released`` flag equals the derived one, naming the open Gate.

    The derivation reads part acceptance rather than a Gate's recorded status, so a Gate recorded
    `CLOSED` over an unaccepted part cannot release a screen through this check. That disagreement
    is ``gate-status-derivation``'s to report.
    """
    findings: list[Finding] = []
    for record in premises.register.screens:
        release = premises.derived.releases[record.id]
        if record.released == release.released:
            continue
        if record.released:
            findings.append(
                Finding(
                    RELEASED_DERIVATION,
                    f"{record.id} is recorded Released, and its blocking list names the open "
                    f"Gate {_gates(release.open_gates)}",
                )
            )
        else:
            findings.append(
                Finding(
                    RELEASED_DERIVATION,
                    f"{record.id} is recorded as not Released, and no Gate its blocking list "
                    f"names is open",
                )
            )
    return findings


def check_two_gate(premises: Premises) -> list[Finding]:
    """Exactly the four recorded Two_Gate_Screens carry two Gate-classified entries, against the
    two Gates each is recorded against, and no screen carries a third.

    The third-Gate rule is checked here rather than in the derivation, because a screen carrying
    three Gate-classified entries is in no Two_Gate_Screen set and would otherwise pass unseen.
    """
    derived = premises.derived.two_gate_screens
    carried = premises.derived.blocking_gates
    findings: list[Finding] = []

    findings.extend(
        Finding(
            TWO_GATE,
            f"{screen} carries two Gate-classified entries, naming {_gates(derived[screen])}, and "
            f"Requirement 1 criterion 5 records no screen other than "
            f"{_screens(TWO_GATE_SCREENS)} in more than one Release_Set",
        )
        for screen in sorted(derived.keys() - TWO_GATE_SCREENS.keys())
    )
    for screen in sorted(TWO_GATE_SCREENS.keys() - derived.keys()):
        expected = _gates(TWO_GATE_SCREENS[screen])
        if screen not in carried:
            observed = "the register records no screen record for it"
        elif not carried[screen]:
            observed = "its blocking list classifies no entry as a Gate"
        else:
            observed = f"its blocking list names {_gates(carried[screen])}"
        findings.append(
            Finding(
                TWO_GATE,
                f"{screen} is recorded a Two_Gate_Screen against {expected}, and {observed}",
            )
        )
    findings.extend(
        Finding(
            TWO_GATE,
            f"{screen} is recorded a Two_Gate_Screen against "
            f"{_gates(TWO_GATE_SCREENS[screen])}, and its blocking list names "
            f"{_gates(derived[screen])}",
        )
        for screen in sorted(derived.keys() & TWO_GATE_SCREENS.keys())
        if frozenset(derived[screen]) != TWO_GATE_SCREENS[screen]
    )
    findings.extend(
        Finding(
            TWO_GATE,
            f"{screen} carries {len(gates)} Gate-classified entries, naming {_gates(gates)}, and "
            f"Requirement 1 criterion 5 records no screen in more than two Release_Sets",
        )
        for screen, gates in sorted(carried.items())
        if len(gates) > 2
    )
    return findings


def check_partition(premises: Premises) -> list[Finding]:
    """The recorded Release_Sets and the Released-with-no-Gate set partition the undelivered
    screens the register records, and that second set is exactly the one Requirement 2 criterion 8
    records.

    The partition is two-part rather than eleven-part: a Two_Gate_Screen is in two Release_Sets, so
    the ten are not pairwise disjoint and Requirement 1 criterion 6's rule is the either/or between
    the union of them and the no-Gate set.

    The predicate runs over the screens the register records. That this set is the inventory's rows
    is ``inventory-coverage``'s finding, so a screen the register omits is reported once, there,
    rather than again here as a hole in the partition.
    """
    recorded = _recorded_release_sets(premises.register)
    in_a_release_set = frozenset().union(*recorded.values())
    records = {record.id for record in premises.register.screens}
    undelivered_records = premises.derived.undelivered & records
    no_gate = frozenset(
        screen for screen in undelivered_records if not premises.derived.blocking_gates[screen]
    )

    findings: list[Finding] = [
        Finding(
            PARTITION,
            f"{screen} is undelivered, appears in no Release_Set, and its blocking list names "
            f"{_gates(premises.derived.blocking_gates[screen])}, whose Release_Set must carry it",
        )
        for screen in sorted(undelivered_records - in_a_release_set - no_gate)
    ]
    findings.extend(
        Finding(
            PARTITION,
            f"{screen} is recorded in the Release_Set of "
            f"{_gates(gate for gate, screens in recorded.items() if screen in screens)} and every "
            f"entry of its blocking list classifies as an Acceptance_Criterion, so it is recorded "
            f"under both branches of Requirement 1 criterion 6",
        )
        for screen in sorted(in_a_release_set & no_gate)
    )
    findings.extend(
        Finding(
            PARTITION,
            f"{screen} carries no Gate-classified entry, and Requirement 2 criterion 8 records "
            f"only {_screens(RELEASED_WITH_NO_GATE)} as Released with no Gate",
        )
        for screen in sorted(no_gate - RELEASED_WITH_NO_GATE)
    )
    findings.extend(
        Finding(
            PARTITION,
            f"{screen} is recorded Released with no Gate, and it is not an undelivered screen "
            f"whose blocking list classifies every entry as an Acceptance_Criterion",
        )
        for screen in sorted(RELEASED_WITH_NO_GATE - no_gate)
    )
    return findings


def check_delivered_outside(premises: Premises) -> list[Finding]:
    """No Release_Set holds a screen the inventory reports as delivered.

    The finding names the inventory's status, which is the reason Requirement 1 criterion 11
    requires alongside the removal.
    """
    statuses = {row.screen: row.status for row in premises.inventory.rows}
    return [
        Finding(
            DELIVERED_OUTSIDE,
            f"{dossier.id} records {entry.screen} in its Release_Set, and "
            f"{premises.register.inventory.path} records that screen {statuses[entry.screen]}",
        )
        for dossier in premises.register.gates
        for entry in dossier.release_set
        if statuses.get(entry.screen) in DELIVERED_STATUSES
    ]


def check_inventory_coverage(premises: Premises) -> list[Finding]:
    """The register's screen records and the inventory's rows are the same set, both directions.

    One direction alone passes while the register quietly drops a screen, which is the whole of
    what Requirement 1 criterion 7's second half asks about.
    """
    inventory_path = premises.register.inventory.path
    rows = {row.screen for row in premises.inventory.rows}
    records = {record.id for record in premises.register.screens}
    findings: list[Finding] = [
        Finding(
            INVENTORY_COVERAGE,
            f"{inventory_path} carries a row for {screen}, and the register records no screen "
            f"record for it",
        )
        for screen in sorted(rows - records)
    ]
    findings.extend(
        Finding(
            INVENTORY_COVERAGE,
            f"the register records a screen record for {screen}, and {inventory_path} carries no "
            f"row for it",
        )
        for screen in sorted(records - rows)
    )
    return findings


def check_entry_classification(premises: Premises) -> list[Finding]:
    """The register's classified entries and the inventory's "Blocked by" cell are one set.

    Both directions, entry by entry, so a cell that has gained an entry and lost another is two
    findings rather than one mismatch. The comparison is by identifier, which is the token the
    inventory parser picks out of the cell, so it runs over the entries whose register key has that
    shape.

    An entry whose key does not -- the register assigns one to an entry the inventory states in
    prose -- is matched on the ``inventoryText`` substring instead. A key recording no such
    substring and a substring the cell does not carry are distinct findings: the first is an entry
    nothing could compare, the second is a cell reworded without the classification being re-read.

    The predicate runs over the screens both documents carry. A screen in only one of them is
    ``inventory-coverage``'s finding, reported once there rather than again here as every entry of
    that screen going missing.
    """
    inventory_path = premises.register.inventory.path
    rows = {row.screen: row for row in premises.inventory.rows}
    records = {record.id: record for record in premises.register.screens}
    findings: list[Finding] = []
    for screen in sorted(rows.keys() & records.keys()):
        row = rows[screen]
        record = records[screen]
        classified = {entry.id for entry in record.blocking_entries if is_entry_id(entry.id)}
        findings.extend(
            Finding(
                ENTRY_CLASSIFICATION,
                f"{screen}: the register classifies {token}, and the 'Blocked by' cell of "
                f"{inventory_path} carries no entry with that identifier",
            )
            for token in sorted(classified - row.blocking_tokens)
        )
        findings.extend(
            Finding(
                ENTRY_CLASSIFICATION,
                f"{screen}: the 'Blocked by' cell of {inventory_path} carries {token}, and the "
                f"register classifies no entry with that identifier",
            )
            for token in sorted(row.blocking_tokens - classified)
        )
        for entry in record.blocking_entries:
            if is_entry_id(entry.id):
                continue
            if entry.inventory_text is None:
                findings.append(
                    Finding(
                        ENTRY_CLASSIFICATION,
                        f"{screen}: {entry.id!r} is a register-assigned key rather than an "
                        f"inventory entry identifier, and the register records no inventoryText "
                        f"for it to match in the 'Blocked by' cell of {inventory_path}",
                    )
                )
            elif entry.inventory_text not in row.blocked_by:
                findings.append(
                    Finding(
                        ENTRY_CLASSIFICATION,
                        f"{screen}: the register matches {entry.id!r} on the inventoryText "
                        f"{entry.inventory_text!r}, and the 'Blocked by' cell of "
                        f"{inventory_path} does not carry that text",
                    )
                )
    return findings


def _retained(gates: Iterable[str]) -> str:
    """Render what a screen retains, in the terms Requirement 2 criterion 11 states it."""
    ordered = sorted(gates, key=_gate_sort_key)
    if not ordered:
        return f"no entry classified as a {GATE_CLASSIFICATION}"
    if len(ordered) == 1:
        return f"one entry classified as a {GATE_CLASSIFICATION}, naming {ordered[0]}"
    return f"{len(ordered)} entries classified as a {GATE_CLASSIFICATION}, naming {_gates(ordered)}"


def check_reclassifications(premises: Premises) -> list[Finding]:
    """The five recorded misreads carry their reclassification, and the retained-Gate statement
    holds.

    A missing entry, an entry left classified as a Gate and an entry recording no reclassification
    are three findings rather than one: the first is a classification nobody made, the second is the
    misread still standing, and the third is the misread corrected with no record that it was one.

    The retained-Gate statement is read from the derived Gate set rather than from the entries
    directly, so two entries naming one Gate count once and the statement is about decisions rather
    than about rows.
    """
    records = {record.id: record for record in premises.register.screens}
    findings: list[Finding] = []
    for screen, entry_id in sorted(RECLASSIFIED_ENTRIES.items()):
        record = records.get(screen)
        entry = (
            None
            if record is None
            else next((item for item in record.blocking_entries if item.id == entry_id), None)
        )
        if entry is None:
            findings.append(
                Finding(
                    RECLASSIFICATIONS,
                    f"{screen}: Requirement 2 criterion 7 records {entry_id} as the misread entry "
                    f"reclassified as an {ACCEPTANCE_CRITERION_CLASSIFICATION}, and the register "
                    f"records no blocking entry with that identifier",
                )
            )
            continue
        if entry.classification != ACCEPTANCE_CRITERION_CLASSIFICATION:
            findings.append(
                Finding(
                    RECLASSIFICATIONS,
                    f"{screen}: the register classifies {entry_id} as {entry.classification}, and "
                    f"Requirement 2 criterion 7 records it reclassified as an "
                    f"{ACCEPTANCE_CRITERION_CLASSIFICATION}",
                )
            )
        if entry.reclassified_from != GATE_CLASSIFICATION:
            recorded = (
                "records no reclassification for it"
                if entry.reclassified_from is None
                else f"records it reclassified from {entry.reclassified_from}"
            )
            findings.append(
                Finding(
                    RECLASSIFICATIONS,
                    f"{screen}: Requirement 2 criterion 7 records {entry_id} as misread as a "
                    f"{GATE_CLASSIFICATION}, and the register {recorded}",
                )
            )

    carried = premises.derived.blocking_gates
    for screen, expected in sorted(RETAINED_GATES.items()):
        observed = (
            "the register records no screen record for it"
            if screen not in carried
            else f"its blocking list retains {_retained(carried[screen])}"
        )
        if screen in carried and frozenset(carried[screen]) == frozenset(expected):
            continue
        findings.append(
            Finding(
                RECLASSIFICATIONS,
                f"{screen}: Requirement 2 criterion 11 records it retaining "
                f"{_retained(expected)}, and {observed}",
            )
        )
    return findings


@dataclass(frozen=True)
class _TestTree:
    """What ``aerora/test/**`` and the overlay's CMake files say about a suite, read once per run.

    Once, because the predicate runs over every recorded observation and most of them name one of a
    few suites. Re-reading the tree per observation would walk it fifty times for no added evidence.
    """

    sources: dict[str, str]
    registered: frozenset[str]
    declared: frozenset[str]


def _live_cmake(text: str) -> str:
    """Drop whole-line comments, so a commented-out call is not read as a registration.

    ``aerora/src/UI/Ui.cmake`` carries a commented-out copy of the whole ``add_qgc_test`` block as
    the prescription for the aggregation point, and ``aerora-manifest-check.ps1`` strips comments
    for exactly this reason: reading that copy as live configuration reads a prescription as a
    registration.
    """
    return "\n".join(line for line in text.splitlines() if not line.lstrip().startswith("#"))


def _read_tree(root: Path) -> _TestTree:
    """Read the overlay's test sources and the CTest names its CMake files carry.

    A source that cannot be decoded is read with replacement characters rather than skipped: a
    replacement in a comment cannot change whether a class or a slot is declared, and skipping the
    file would turn a declaration into a "not found".
    """
    sources: dict[str, str] = {}
    tree = root / TEST_TREE
    if tree.is_dir():
        for path in sorted(tree.rglob("*")):
            if path.suffix not in _SOURCE_SUFFIXES or not path.is_file():
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            sources[path.relative_to(root).as_posix()] = text.replace("\r\n", "\n")

    overlay = root / OVERLAY_TREE
    live = ""
    if overlay.is_dir():
        paths = sorted(
            path for pattern in _CMAKE_PATTERNS for path in overlay.rglob(pattern) if path.is_file()
        )
        live = "\n".join(
            _live_cmake(path.read_text(encoding="utf-8", errors="replace")) for path in paths
        )

    return _TestTree(
        sources=sources,
        registered=frozenset(match[1] for match in _ADD_QGC_TEST.finditer(live)),
        declared=frozenset(
            name for block in _TEST_NAMES_LIST.finditer(live) for name in block[1].split()
        ),
    )


def _class_definition(suite: str) -> re.Pattern[str]:
    """The pattern matching *suite*'s own class definition."""
    return re.compile(rf"\bclass\s+{re.escape(suite)}\s*(?::|\{{|$)", re.M)


def _private_slots(text: str, suite: str) -> set[str]:
    """The private slot names *suite*'s definition in *text* declares.

    The class body is read to its closing brace and split on its access specifiers, so a slot under
    ``protected slots:`` -- which every Aerora UI suite has, for the ``init()`` override -- is not a
    private slot, and a method under ``private:`` is a method rather than a slot.
    """
    definition = _class_definition(suite).search(text)
    if definition is None:
        return set()
    body = text[definition.end() :]
    end = _CLASS_END.search(body)
    if end is not None:
        body = body[: end.start()]

    names: set[str] = set()
    sections = list(_ACCESS_SPECIFIER.finditer(body))
    for index, section in enumerate(sections):
        if section["access"] != "private" or section["slots"] is None:
            continue
        stop = sections[index + 1].start() if index + 1 < len(sections) else len(body)
        names.update(match[1] for match in _SLOT_DECLARATION.finditer(body[section.end() : stop]))
    return names


def _resolve_observation(screen: str, observation: Observation, tree: _TestTree) -> list[Finding]:
    """Resolve one observation against the test sources and the CTest registration.

    Four facts, reported separately, because they are repaired differently: a suite no source
    declares, a slot the suite does not declare, a suite no ``add_qgc_test`` call registers, and a
    suite no manifest declares. Each names the symbol and the tree that was searched, because
    **"not found" is not "present"** -- the discipline the design states for a guard declaration,
    and the same one ``aerora-manifest-check.ps1`` states for its ``ASSUMED`` rows.
    """
    suite = observation.suite
    definition = _class_definition(suite)
    declaring = [path for path, text in tree.sources.items() if definition.search(text)]
    findings: list[Finding] = []

    if not declaring:
        findings.append(
            Finding(
                VERIFIED_OBSERVATION,
                f"{screen}: the observation names the suite {suite}, and no source under "
                f"{TEST_TREE}/** declares a class with that name. A suite this check cannot find "
                f"there witnesses no Aerora screen drawn in a booted application window",
            )
        )
    else:
        for path in declaring:
            if observation.slot in _private_slots(tree.sources[path], suite):
                continue
            findings.append(
                Finding(
                    VERIFIED_OBSERVATION,
                    f"{screen}: the observation names the slot {observation.slot} of {suite}, and "
                    f"{path} declares no private slot with that name",
                )
            )

    if suite not in tree.registered:
        findings.append(
            Finding(
                VERIFIED_OBSERVATION,
                f"{screen}: the observation names the suite {suite}, and no add_qgc_test call in "
                f"the CMake files under {OVERLAY_TREE}/ registers it, so no CTest entry runs it",
            )
        )
    if suite not in tree.declared:
        findings.append(
            Finding(
                VERIFIED_OBSERVATION,
                f"{screen}: the observation names the suite {suite}, and no *_TEST_NAMES list in "
                f"the CMake files under {OVERLAY_TREE}/ declares it",
            )
        )
    return findings


def _slug(title: str) -> str:
    """Render a heading's text as the anchor a link to it carries."""
    return _SLUG_DROP.sub("", title.strip().lower()).replace(" ", "-")


def _section(text: str, anchor: str) -> str | None:
    """The text under the heading *anchor* addresses, to the next heading of the same depth."""
    headings = [(match, len(match["hashes"])) for match in _HEADING.finditer(text)]
    for index, (heading, depth) in enumerate(headings):
        if _slug(heading["title"]) != anchor:
            continue
        end = next(
            (other.start() for other, below in headings[index + 1 :] if below <= depth), len(text)
        )
        return text[heading.end() : end]
    return None


def _status_names(table: MarkdownTable | None) -> frozenset[str]:
    """The status names the first column of *table* declares.

    A row whose first cell is prose -- the undelivered count, the totals row, the fork count --
    declares no status and contributes none.
    """
    return frozenset(
        span[1]
        for row in (table.rows if table is not None else ())
        if row.cells and (span := _CODE_SPAN.search(row.cells[0])) is not None
    )


def _referenced_vocabulary(
    text: str, document: str, anchor: str
) -> tuple[frozenset[str], list[Finding]]:
    """The vocabulary *document* declares under *anchor*, or the finding that it declares none."""
    section = _section(text, anchor)
    if section is None:
        return frozenset(), [
            Finding(
                VERIFIED_OBSERVATION,
                f"{document} carries no heading the anchor {anchor!r} addresses, so the status "
                f"vocabulary could not be read from it",
            )
        ]
    table = next(
        (item for item in parse_md_tables(section) if item.headers[:1] == (_VOCABULARY_COLUMN,)),
        None,
    )
    names = _status_names(table)
    if not names:
        return frozenset(), [
            Finding(
                VERIFIED_OBSERVATION,
                f"{document}: the section {anchor!r} carries no table whose first column is "
                f"{_VOCABULARY_COLUMN!r} and whose rows name a status, so the vocabulary could not "
                f"be read from it",
            )
        ]
    return names, []


def _status_vocabulary(premises: Premises) -> tuple[frozenset[str], list[Finding]]:
    """The status vocabulary the inventory declares, or the finding that it declares none.

    The inventory declares it in two places and neither is a list in this module. Its Status column
    legend names the document and the anchor the programme vocabulary is declared under, which is
    the wider set every undelivered row is read against; its own Inventory totals table names the
    delivered members with the gloss this document gives them. The union is the vocabulary, and a
    status in neither is a finding.
    """
    path = premises.register.inventory.path
    text = premises.inventory.text
    legend = _STATUS_LEGEND.search(text)
    if legend is None:
        return frozenset(), [
            Finding(
                VERIFIED_OBSERVATION,
                f"{path} carries no {_VOCABULARY_COLUMN} column legend naming the document its "
                f"vocabulary is declared in, so the rows could not be read against one. A "
                f"vocabulary this check cannot locate is not a vocabulary every row satisfies",
            )
        ]

    document = legend["document"]
    if not document:
        referenced, failures = _referenced_vocabulary(text, path, legend["anchor"])
    else:
        document = (PurePosixPath(path).parent / document).as_posix()
        source, unreadable = _read_text(premises.root, document)
        if source is None:
            return frozenset(), [
                Finding(
                    VERIFIED_OBSERVATION,
                    f"{path} reads its {_VOCABULARY_COLUMN} vocabulary from {document}, and "
                    f"{unreadable[0].message}",
                )
            ]
        referenced, failures = _referenced_vocabulary(source, document, legend["anchor"])
    if failures:
        return frozenset(), failures

    totals = next((item for item in parse_md_tables(text) if item.headers == TOTALS_COLUMNS), None)
    return referenced | _status_names(totals), []


def check_verified_observation(premises: Premises) -> list[Finding]:
    """Every ``VERIFIED`` row carries an observation that resolves, and every status is declared.

    Three rules, each reported as itself.

    A row the inventory records ``VERIFIED`` must have a register screen record carrying an
    observation: Requirement 3 criterion 9 admits that status only for a screen observed navigated
    to and drawn in a booted application window. The predicate runs over the screens both documents
    carry, as ``partition`` and ``entry-classification`` do -- a row the register records no screen
    record for is ``inventory-coverage``'s finding, reported once there rather than again here.

    Every recorded observation must resolve, whatever its row's status: an observation that names a
    suite or a slot nothing declares is wrong wherever it is recorded. Resolution is text
    inspection of the sources and the CMake manifests, never a build -- this host has no compiler
    and no Qt, and the check would be worth less if it pretended otherwise.

    Every status the inventory's rows carry must be a member of the vocabulary the inventory
    declares. The register carries no delivery status -- the schema's screen record says the
    inventory is authoritative for it -- so "every recorded delivery status" is the inventory's own
    Status cells, which is the only place one is recorded.
    """
    vocabulary, findings = _status_vocabulary(premises)
    records = {record.id: record for record in premises.register.screens}

    findings.extend(
        Finding(
            VERIFIED_OBSERVATION,
            f"{row.screen}: {premises.register.inventory.path} records the screen "
            f"{VERIFIED_STATUS}, and the register records no observation of it navigated to and "
            f"drawn in a booted application window",
        )
        for row in premises.inventory.rows
        if row.status == VERIFIED_STATUS
        and row.screen in records
        and records[row.screen].observation is None
    )

    tree = _read_tree(premises.root)
    findings.extend(
        finding
        for record in premises.register.screens
        if record.observation is not None
        for finding in _resolve_observation(record.id, record.observation, tree)
    )

    findings.extend(
        Finding(
            VERIFIED_OBSERVATION,
            f"{premises.register.inventory.path}: the row for {row.screen} records the status "
            f"{row.status}, and the vocabulary the inventory declares is "
            f"{', '.join(sorted(vocabulary))}",
        )
        for row in premises.inventory.rows
        if vocabulary and row.status not in vocabulary
    )
    return findings


def _declared_count(
    path: str, site: str, declared: int, derived: int, counted: str
) -> list[Finding]:
    """Compare one declared count against the count *counted* names, taking no side."""
    if declared == derived:
        return []
    return [
        Finding(
            INVENTORY_TOTALS,
            f"{path}: {site} declares {declared}, and {counted} give {derived}. "
            f"{INVENTORY_OWNER} owns the inventory: correct the totals, or correct the rows. The "
            f"register records no count, and this check reports the disagreement either way",
        )
    ]


def _unreadable_declaration(path: str, site: str, derived: int, counted: str) -> Finding:
    """Report a declaration the check could not read. "Not found" is not "agrees"."""
    return Finding(
        INVENTORY_TOTALS,
        f"{path}: {site} declares no count this check could read, so the {derived} that {counted} "
        f"give could not be compared against it. {INVENTORY_OWNER} owns the inventory, and a "
        f"declaration this check cannot find is not a declaration that agrees",
    )


def _declared_in_text(text: str, pattern: re.Pattern[str], group: str) -> int | None:
    """The count one document-level declaration names, or ``None`` where *text* does not carry it."""
    match = pattern.search(text)
    return None if match is None else int(match[group])


def _declared_in_totals(table: MarkdownTable | None, label: str) -> int | None:
    """The count the Inventory totals row identified by *label* declares, or ``None`` for neither.

    A table this check could not find, a row the table does not carry and a row whose Count cell
    carries no number are three ways for the declaration to be unreadable, and the caller reports
    each of them against the row that stopped being compared.
    """
    for row in table.rows if table is not None else ():
        if len(row.cells) != len(TOTALS_COLUMNS) or label not in row.cells[0]:
            continue
        declared = _COUNT.search(row.cells[1])
        return None if declared is None else int(declared.group())
    return None


def check_inventory_totals(premises: Premises) -> list[Finding]:
    """Every count the inventory declares agrees with the rows the inventory carries.

    Each declaration is compared against a figure derived from the rows at call time. Nothing here
    holds a screen count, a delivered count or an undelivered count: encoding one would make this
    check the second place the number can be wrong, which is the whole of what Finding 2 was.

    A declaration the check cannot read is a finding rather than a silent pass, for the reason the
    source half states for a guard declaration it cannot find: a reworded heading must not quietly
    stop the comparison.

    The finding names Tab 7 and takes no side. Correct the totals, or correct the rows.
    """
    inventory = premises.inventory
    text = inventory.text
    path = premises.register.inventory.path
    rows = inventory.rows
    screens = len(rows)
    undelivered_rows = len(premises.derived.undelivered)
    verified = sum(1 for row in rows if row.status == VERIFIED_STATUS)
    superseded = sum(1 for row in rows if row.status == SUPERSEDED_STATUS)
    table = next((item for item in parse_md_tables(text) if item.headers == TOTALS_COLUMNS), None)

    def totals_row(label: str, derived: int, counted: str) -> tuple[str, int | None, int, str]:
        return (
            f"{_TOTALS_TABLE_SITE}'s {label!r} row",
            _declared_in_totals(table, label),
            derived,
            counted,
        )

    # Each entry is one declared count, the count the inventory's own rows give for it, and how the
    # finding names what that figure was counted from.
    declarations: tuple[tuple[str, int | None, int, str], ...] = (
        *(
            (
                f"the area heading {area.heading!r} at line {area.line}",
                area.declared_screens,
                len(area.rows),
                "its own rows",
            )
            for area in inventory.areas
        ),
        (
            f"{_LEAD_LINE_SITE}'s screen count",
            _declared_in_text(text, _LEAD_LINE, "screens"),
            screens,
            _AREA_ROWS,
        ),
        (
            f"{_LEAD_LINE_SITE}'s area count",
            _declared_in_text(text, _LEAD_LINE, "areas"),
            len(inventory.areas),
            "the area headings",
        ),
        totals_row(VERIFIED_STATUS, verified, f"the rows the inventory records {VERIFIED_STATUS}"),
        totals_row(
            SUPERSEDED_STATUS, superseded, f"the rows the inventory records {SUPERSEDED_STATUS}"
        ),
        totals_row(NOT_DELIVERED_LABEL, undelivered_rows, _UNDELIVERED_ROWS),
        totals_row(TOTAL_LABEL, screens, _AREA_ROWS),
        (
            _UNDELIVERED_SITE,
            _declared_in_text(text, _UNDELIVERED_PARAGRAPH, "undelivered"),
            undelivered_rows,
            _UNDELIVERED_ROWS,
        ),
        (
            f"{_ENTRY_COUNT_SITE}'s entry count",
            _declared_in_text(text, _ENTRY_COUNT, "entries"),
            screens,
            _AREA_ROWS,
        ),
    )

    findings: list[Finding] = []
    for site, declared, derived, counted in declarations:
        if declared is None:
            findings.append(_unreadable_declaration(path, site, derived, counted))
        else:
            findings.extend(_declared_count(path, site, declared, derived, counted))
    return findings


# The derived checks, in the order the design's table lists them. Each reports findings carrying
# its own check name, so the registry needs no name of its own.
_CHECKS: Final = (
    check_partition,
    check_release_set_derivation,
    check_two_gate,
    check_released_derivation,
    check_delivered_outside,
    check_inventory_coverage,
    check_entry_classification,
    check_reclassifications,
    check_verified_observation,
    check_inventory_totals,
)


def run_checks(premises: Premises) -> list[Finding]:
    """Run every derived check over established premises."""
    return [finding for check in _CHECKS for finding in check(premises)]


def report(findings: Sequence[Finding]) -> int:
    """Print every finding and return the exit status, which is non-zero for any of them."""
    for finding in findings:
        print(finding)
    return int(bool(findings))


def main() -> int:
    premises = load_premises(ROOT)
    if isinstance(premises, list):
        return report(premises)

    findings = run_checks(premises)
    if findings:
        return report(findings)

    # The run states what it established rather than that the register is valid, and every figure
    # in it is measured here and now: a remembered count is the one that goes stale.
    print(
        f"{REGISTER_PATH} validates against {REGISTER_SCHEMA_PATH} and agrees with "
        f"{premises.register.inventory.path}: {len(premises.register.gates)} Gate_Dossiers, "
        f"{len(premises.register.screens)} screen records, "
        f"{len(premises.derived.undelivered)} undelivered of {len(premises.inventory.rows)} rows"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

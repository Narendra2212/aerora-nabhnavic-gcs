"""Validate the Aerora Gate_Register against the screen inventory and its own schema.

The data half of the Gate_Review. It reads the register, the schema beside it, the screen inventory
the register names and the Closure_Record each Gate part names, and reports every disagreement
between what an owner recorded and what the documents derive. It never writes: the only thing an
auto-fix could write here is a Released flag, a classification, a guard value or a settled entry
that no owner signed, so a finding is reported and the exit status is non-zero.

Premises are established before any derived check runs, and the order is the error contract's. A
missing register, a register that is not JSON, a register the schema rejects, an inventory whose
area tables cannot be parsed, an inventory carrying no screen row, or a Closure_Record a part names
and the tree lacks each stop the run there and are reported as themselves. The governing rule is
the one ``aerora-manifest-check.ps1`` states for its own ``ASSUMED`` rows: **an unresolved premise
is a failure, never a pass.** A derived check over a malformed register would report noise that
hides the real error, and one over an inventory silently parsed as zero rows would satisfy every
predicate over those rows trivially.

This module establishes those premises. Every derived check the design's table names is a predicate
over :class:`Premises`, reporting :class:`Finding` values that carry its own check name, so that one
stream of output says which rule rejected which record.
"""

from __future__ import annotations

import json
import re
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import TYPE_CHECKING, Any, Final

from common.gate_register import (
    ABSENT,
    ACCEPTANCE_CRITERION_CLASSIFICATION,
    ACCEPTED_PART_STATE,
    DELIVERED_STATUSES,
    GATE_CLASSIFICATION,
    GATE_IDS,
    RECORD_STATES,
    GateRegisterError,
    ScreenInventoryError,
    blocking_gates,
    gate_open,
    is_entry_id,
    load_closure_record,
    load_gate_register,
    parse_screen_inventory,
    release_sets,
    released,
    two_gate_screens,
    undelivered,
)
from common.git import resolve_add_commit
from common.markdown import parse_md_tables
from jsonschema import Draft202012Validator
from jsonschema.exceptions import SchemaError

if TYPE_CHECKING:
    from collections.abc import Iterable, Sequence

    from common.gate_register import (
        Amendment,
        ClosureRecord,
        DecisionContentElement,
        GateOpenness,
        GatePart,
        GateRegister,
        Observation,
        ScreenInventory,
        ScreenRelease,
        SettledElement,
        SettledEntry,
    )
    from common.git import AddCommit
    from common.markdown import MarkdownTable

ROOT: Final = Path(__file__).resolve().parents[1]

# Repository-relative paths, used verbatim in every message, so a finding reads the same way on the
# authoring host and in CI. The inventory is not among them: the register names it, and naming it
# here as well would make this module a second authority for where it lives.
_SPEC_DIR: Final = ".kiro/specs/aerora-decision-gate-closure"
REGISTER_PATH: Final = f"{_SPEC_DIR}/gate-register.json"
REGISTER_SCHEMA_PATH: Final = f"{_SPEC_DIR}/gate-register.schema.json"

# The Closure_Records are not among them either, for the reason the inventory is not: each Gate
# part names the record that settles it, and naming a directory here would make this module a
# second authority for where a record lives. The schema beside them is this module's to read,
# because it is the schema a record is validated against rather than a document a record names.
CLOSURE_RECORD_SCHEMA_PATH: Final = f"{_SPEC_DIR}/closure-record.schema.json"

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
CLOSURE_COMPLETENESS: Final = "closure-completeness"
CLOSURE_SIGNATURES: Final = "closure-signatures"
ENTRY_COMMIT: Final = "closure-entry-commit"
GATE_STATUS_DERIVATION: Final = "gate-status-derivation"
SIGNOFF_VERDICTS: Final = "signoff-verdicts"
# The amendment rule carries its own check name rather than joining one of its neighbours. A
# finding names the rule that rejected the record, and an amendment is repaired on its own: the
# repair is to one amendment's key, signatures or date and never to the record's own rows.
# `closure-signatures` makes the same point from the other side -- it deliberately does not read an
# amendment's signatures, because the owner set those rows answer to is scoped to the amendment's
# key -- so folding them back in would undo the scoping Requirement 3 criterion 11 turns on.
CLOSURE_AMENDMENTS: Final = "closure-amendments"
VERIFIED_OBSERVATION: Final = "verified-observation"
INVENTORY_TOTALS: Final = "inventory-totals"

# The part states a Closure_Record is in front of the Gate_Review in, which are the two members of
# RECORD_STATES: a part is `submitted` or `accepted` exactly when a record settles it, and the
# third part state -- `open` -- is a part nobody has signed.
SETTLING_PART_STATES: Final = RECORD_STATES

# The member of RECORD_STATES at which a record asserts it is in the repository. Aliased from the
# part state for the reason SETTLING_PART_STATES is aliased: the two vocabularies coincide on this
# literal, and a second spelling of it would be a second thing to keep in step.
ACCEPTED_RECORD_STATE: Final = ACCEPTED_PART_STATE

# The commit trailer a signature is recorded as. Resolving the commit that added a file and reading
# its trailers belongs to `common.git`; which trailer names a signing party is the Closure_Record's
# own convention, so the key stays here.
SIGNED_BY_KEY: Final = "Gate-Signed-By"

# What `closure-signatures` establishes, and what it does not. Every trailer finding carries the
# short form and the run prints the long form whenever a record was read, because a check that
# compared commit trailers without saying what they establish would read as a stronger claim than
# it is. `AeroraSecurityStatus` is the precedent: the station reports *not enforced* rather than
# claiming a boundary it does not evaluate. Making this stronger needs branch protection on the
# register path, which is a repository setting outside this repository and is named rather than
# assumed.
RECORDED_IDENTITY: Final = "recorded identity, not cryptographic authorship"
SIGNATURE_NOTICE: Final = (
    f"{CLOSURE_SIGNATURES} compares {RECORDED_IDENTITY}: a signature is an owner label the "
    f"Gate_Dossier records, and a {SIGNED_BY_KEY} trailer on the commit that added the record, "
    f"and neither establishes that the named party agreed"
)

# The three members of SETTLEMENT_SHAPES by name. The settlement rules branch on these and on
# nothing else: the shape, the required attributes, the key source and the excluded source are what
# the Gate_Dossier records for an element, so the ten Gates settle very different content through
# the same three rules and no rule here reads a Gate identifier.
SINGLE_SHAPE: Final = "single"
KEYED_SHAPE: Final = "keyed"
MAPPING_SHAPE: Final = "mapping"

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

# The two members of GATE_STATUSES by name. `gate-status-derivation` is the one check that reads a
# Gate's recorded status, and it reads both members: a Gate recorded `CLOSED` over an unaccepted
# part and a Gate left `OPEN` once every part is accepted are both findings.
OPEN_STATUS: Final = "OPEN"
CLOSED_STATUS: Final = "CLOSED"

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

    ``gate_open`` is carried for the same reason, even though the release derivation evaluates it
    too: ``gate-status-derivation`` compares a Gate's recorded status against it, and a check that
    derived its own answer would be a second evaluation of the equation every other check reads.
    """

    blocking_gates: dict[str, tuple[str, ...]]
    gate_open: dict[str, GateOpenness]
    release_sets: dict[str, frozenset[str]]
    releases: dict[str, ScreenRelease]
    two_gate_screens: dict[str, tuple[str, ...]]
    undelivered: frozenset[str]


@dataclass(frozen=True)
class NamedClosureRecord:
    """One part's Closure_Record, with the Gate, the part and the path that name it.

    The path travels with the record because the record is the only document here that a part names
    rather than this module knowing: a finding about it has to say which file to repair.

    ``add_commit`` is what git history says about the commit that added ``path``, resolved once for
    the reason :class:`Derived` evaluates the derivations once: ``closure-signatures`` reads the
    trailers of that commit and ``closure-entry-commit`` compares its hash against the recorded
    ``entryCommit``, and two resolutions would be two ``git log`` invocations per record and two
    chances to disagree about which commit added it. It is ``None`` for a record that is not
    recorded `accepted`: such a record is one the register itself says is not in the repository
    yet, so neither check reads a commit for it and none is resolved.
    """

    gate: str
    part: str
    path: str
    record: ClosureRecord
    add_commit: AddCommit | None


@dataclass(frozen=True)
class Premises:
    """The documents every derived check reads, read and validated once each.

    ``root`` is how a check reaches a third file -- a test source, a guard declaration, git history
    -- without discovering the repository for itself.

    ``closures`` is keyed by the part that names the record, in the ``<GATE>:<part>`` notation the
    register already uses for a part reference in ``permittedToChangeBy``. A part recording no
    Closure_Record has no entry: that is a part nobody has signed rather than a missing premise.
    """

    root: Path
    register: GateRegister
    inventory: ScreenInventory
    derived: Derived
    closures: dict[str, NamedClosureRecord]


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


def _schema_findings(document: Any, schema: Any, *, path: str, schema_path: str) -> list[Finding]:
    """Validate *document* against *schema*, reporting a JSON pointer per error.

    A schema that is not itself a valid schema is a premise failure rather than a schema finding:
    nothing was validated, so reporting it as a rejection of the document would name the wrong
    file.

    The errors are ordered by the pointer they address, so a run reports the same findings in the
    same order and a reviewer can read them against the document top to bottom.
    """
    try:
        Draft202012Validator.check_schema(schema)
    except SchemaError as error:
        pointer = _pointer(error.absolute_path)
        return [Finding(PREMISES, f"{schema_path}: {pointer}: {error.message}")]

    errors = sorted(
        Draft202012Validator(schema).iter_errors(document),
        key=lambda error: ([str(token) for token in error.absolute_path], error.message),
    )
    return [
        Finding(SCHEMA, f"{path}: {_pointer(error.absolute_path)}: {error.message}")
        for error in errors
    ]


def _part_reference(gate: str, part: str) -> str:
    """Name one Gate part the way the register names one in ``permittedToChangeBy``."""
    return f"{gate}:{part}"


def _load_closures(
    root: Path, register: GateRegister
) -> tuple[dict[str, NamedClosureRecord], list[Finding]]:
    """Read the Closure_Record each part names, on the error contract the register is read on.

    A record a part names and the tree lacks, a record that is not JSON, a record the schema
    rejects and a record the model rejects after the schema accepted it are reported the way the
    register's own failures are, against the path the part named. A part recording ``null`` is not
    among them: that is a part nobody has signed.

    Two inconsistencies between the two sides are reported here as well, because they leave a
    record that cannot be attributed to a part: a record whose own ``gate`` or ``part`` disagrees
    with the part naming it, and two parts naming one record file.

    The schema is read only once a part names a record. Reading it unconditionally would fail a
    tree that carries no record against a premise that nothing in it rests on.

    An unanswerable git history is not reported here. It is a premise of two checks rather than of
    the run -- every set check over the register and the inventory holds whatever git can say -- so
    the resolution travels with the record and ``closure-entry-commit`` reports the reason.
    """
    named: list[tuple[str, GatePart, str]] = [
        (dossier.id, part, record)
        for dossier in register.gates
        for part in dossier.parts
        if (record := part.closure_record) is not None
    ]
    if not named:
        return {}, []

    schema, findings = _read_json(root, CLOSURE_RECORD_SCHEMA_PATH)
    if findings:
        return {}, findings

    closures: dict[str, NamedClosureRecord] = {}
    naming: dict[str, str] = {}
    for gate, part, path in named:
        reference = _part_reference(gate, part.id)
        if path in naming:
            findings.append(
                Finding(
                    PREMISES,
                    f"{reference} names {path} as the Closure_Record that settles it, and so does "
                    f"{naming[path]}. The unit of signature is the part, so one record settles one "
                    f"part",
                )
            )
            continue
        naming[path] = reference

        document, failures = _read_json(root, path)
        if failures:
            findings.extend(failures)
            continue
        failures = _schema_findings(
            document, schema, path=path, schema_path=CLOSURE_RECORD_SCHEMA_PATH
        )
        if failures:
            findings.extend(failures)
            continue
        try:
            record = load_closure_record(document, path=path)
        except GateRegisterError as error:
            findings.append(
                Finding(
                    PREMISES,
                    f"{error}. The record validates against {CLOSURE_RECORD_SCHEMA_PATH}, so the "
                    f"schema and the model disagree about its shape",
                )
            )
            continue
        if (record.gate, record.part) != (gate, part.id):
            findings.append(
                Finding(
                    PREMISES,
                    f"{path} records that it settles "
                    f"{_part_reference(record.gate, record.part)}, and {reference} names it as the "
                    f"Closure_Record that settles that part",
                )
            )
            continue
        closures[reference] = NamedClosureRecord(
            gate=gate,
            part=part.id,
            path=path,
            record=record,
            # Once per record, for the two checks that read it. Resolved for an `accepted` record
            # alone: a record in any other state says it is not in the repository yet, and neither
            # check reads a commit for one, so resolving it would be a `git log` per unsigned part
            # that nothing goes on to read.
            add_commit=(
                resolve_add_commit(path, repo_root=root)
                if record.state == ACCEPTED_RECORD_STATE
                else None
            ),
        )
    return closures, findings


def derive(register: GateRegister, inventory: ScreenInventory) -> Derived:
    """Evaluate every derivation the checks read, raising on an unresolvable premise.

    One function, so a test building premises in code derives them the way :func:`load_premises`
    does rather than the way it remembers.
    """
    return Derived(
        blocking_gates=blocking_gates(register),
        gate_open=gate_open(register),
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

    findings = _schema_findings(
        document, schema, path=REGISTER_PATH, schema_path=REGISTER_SCHEMA_PATH
    )
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

    # Last, because every check reads the register and the inventory and only the closure checks
    # read a record: a malformed register reports itself rather than reporting one failure per part
    # of it.
    closures, findings = _load_closures(root, register)
    if findings:
        return findings

    return Premises(
        root=root,
        register=register,
        inventory=inventory,
        derived=derived,
        closures=closures,
    )


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


def _owners(owners: Iterable[str]) -> str:
    """Render an owner set in label order, and say so where it is empty.

    Ordered for the reason :func:`_gates` orders Gates. An empty set is rendered rather than
    omitted, because a message that trailed off into nothing where an owner set should be would
    read as a rendering defect instead of as the finding that the set is empty.
    """
    return ", ".join(sorted(owners)) or "no owner"


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


def _element_named(element: DecisionContentElement, key: str | None = None) -> str:
    """Name the element a finding is about, and the key where the finding is about one."""
    named = f"the element {element.name!r}"
    return named if key is None else f"the key {key!r} of {named}"


def _key_set_named(element: DecisionContentElement) -> str:
    """Name where the key set came from, which is half of what an unmapped member means."""
    source = element.key_source
    if source is None:
        return "the key set the record enumerates"
    return f"the key set of the keySource {source!r}"


def _json_type(value: Any) -> str:
    """Name *value*'s JSON type, so a finding says what the record carried and not only what it
    lacked."""
    if isinstance(value, bool):
        return "a boolean"
    if isinstance(value, str):
        return "a string"
    if isinstance(value, (int, float)):
        return "a number"
    if isinstance(value, list):
        return "an array"
    if isinstance(value, Mapping):
        return "an object"
    return "null"


def _carried(value: Any) -> frozenset[str]:
    """The attribute names a settled value carries.

    An object's keys, less the keys whose value is null. The rest of the JSON types name no
    attribute at all: a settled value is whatever an owner signs it as, and a string, a number or
    an array carries no name this check could read an attribute off.
    """
    if not isinstance(value, Mapping):
        return frozenset()
    return frozenset(str(name) for name, carried in value.items() if carried is not None)


def _carrying(entry: SettledEntry) -> str:
    """Describe what a settled entry carries, so a reviewer reads the repair and not a verdict."""
    if entry.value is ABSENT:
        return "the settled entry carries no value"
    if not isinstance(entry.value, Mapping):
        return (
            f"the settled entry's value is {_json_type(entry.value)} rather than an object naming "
            f"its attributes"
        )
    carried = _carried(entry.value)
    if not carried:
        return "the settled entry's value names no attribute"
    return f"the settled entry's value names {', '.join(sorted(carried))}"


def _attributes(
    reference: str,
    element: DecisionContentElement,
    entry: SettledEntry,
    key: str | None = None,
) -> list[Finding]:
    """Every name in ``requiredAttributes``, named one at a time where the entry omits it.

    One finding per omitted attribute, because a reviewer repairs them one at a time and a finding
    that said "three attributes are missing" would have to be read against the element to find out
    which.

    What "carries" means is as much as the schema admits: ``entries[].value`` takes any JSON type,
    so the only reading available is that the value is an object and the attribute is a key of it.
    A key whose value is null is an omission too -- null and a placeholder are indistinguishable,
    and Requirement 4 criterion 10 leaves an unsettled value absent rather than carrying a
    placeholder in its place. What the check cannot see is anything inside a carried value: whether
    it is right, whether it is issued, whether anyone approved it. That stays with the reviewer,
    which is what the Sign_Off_Conditions are for.

    An entry recorded as an explicit ``exception`` settles its element or key as an exception rather
    than by a value, so no attribute is demanded of it.
    """
    required = element.required_attributes
    if not required or entry.exception:
        return []
    carried = _carried(entry.value)
    return [
        Finding(
            CLOSURE_COMPLETENESS,
            f"{reference}: {_element_named(element, key)} records the required attribute "
            f"{attribute!r}, and {_carrying(entry)}",
        )
        for attribute in required
        if attribute not in carried
    ]


def _excluded_source(
    reference: str, element: DecisionContentElement, settled: SettledElement
) -> list[Finding]:
    """An element recording an excluded source rejects a settled entry declaring that source.

    The finding says *why* the source is excluded rather than only that it is: the Gate_Dossier
    records it as a source a settled entry may not be derived from, which is the Sign_Off_Condition
    behind Requirement 13 criterion 4 and the carrier Requirement 15 criterion 6 requires. An entry
    traced to it settles the element from the one source the Gate excludes, so the part stays
    `OPEN` until the entry is traced to another.
    """
    excluded = element.excluded_source
    if excluded is None:
        return []
    return [
        Finding(
            CLOSURE_COMPLETENESS,
            f"{reference}: {_element_named(element, entry.key)} is settled by an entry traced to "
            f"{excluded!r}, which the Gate_Dossier records as the source a settled entry of that "
            f"element may not be derived from",
        )
        for entry in settled.entries
        if entry.source == excluded
    ]


def _single(
    reference: str, element: DecisionContentElement, settled: SettledElement
) -> list[Finding]:
    """Exactly one settled entry, carrying every name the element records as required.

    Zero entries and more than one are distinct findings. The first is the omission Requirement 3
    criterion 3 names; the second is a record carrying two answers for one element, with nothing to
    choose between them.
    """
    entries = settled.entries
    if not entries:
        return [
            Finding(
                CLOSURE_COMPLETENESS,
                f"{reference}: {_element_named(element)} is recorded {SINGLE_SHAPE}-shaped, and "
                f"the record settles it with no entry",
            )
        ]
    if len(entries) > 1:
        return [
            Finding(
                CLOSURE_COMPLETENESS,
                f"{reference}: {_element_named(element)} is recorded {SINGLE_SHAPE}-shaped, and "
                f"the record settles it with {len(entries)} entries, so the record carries "
                f"{len(entries)} answers for one element",
            )
        ]
    return _attributes(reference, element, entries[0])


def _named_key_set(
    premises: Premises, reference: str, element: DecisionContentElement, source: str
) -> tuple[frozenset[str] | None, list[Finding]]:
    """The key set an element's ``keySource`` names, or the finding that it names none.

    A ``keySource`` names a Decision_Content element the register records, and the keys the record
    settling that element carries are the key set. That is where a key set this check can see
    lives: the register records element *names*, so a set named rather than enumerated is another
    element's settled keys -- which is the relation Requirement 8 criterion 5 reads when `DG-TLM`
    names a freshness class an accepted `DG-FRS` record has not settled.

    Every branch that cannot get there names the ``keySource`` and establishes no set, because a
    per-key rule over a set this check cannot see would pass over nothing.
    """
    named = [
        _part_reference(dossier.id, part.id)
        for dossier in premises.register.gates
        for part in dossier.parts
        for item in part.elements
        if item.name == source
    ]
    unresolved = f"{reference}: {_element_named(element)} names the keySource {source!r}, and "
    if source == element.name:
        return None, [
            Finding(
                CLOSURE_COMPLETENESS,
                f"{unresolved}that is the element's own name, so the key set would be the keys the "
                f"record already enumerates and every rule over it would pass over nothing",
            )
        ]
    if not named:
        return None, [
            Finding(
                CLOSURE_COMPLETENESS,
                f"{unresolved}the register records no Decision_Content element with that name, so "
                f"the key set could not be established",
            )
        ]
    if len(named) > 1:
        return None, [
            Finding(
                CLOSURE_COMPLETENESS,
                f"{unresolved}the register records that element in {', '.join(sorted(named))}, so "
                f"the key set has more than one answer",
            )
        ]
    (settles,) = named
    closure = premises.closures.get(settles)
    if closure is None:
        return None, [
            Finding(
                CLOSURE_COMPLETENESS,
                f"{unresolved}{settles}, which settles that element, names no Closure_Record, so "
                f"the key set could not be established",
            )
        ]
    item = next((entry for entry in closure.record.settled if entry.element == source), None)
    if item is None:
        return None, [
            Finding(
                CLOSURE_COMPLETENESS,
                f"{unresolved}{closure.path} settles no entry for that element, so the key set "
                f"could not be established",
            )
        ]
    keys = frozenset(entry.key for entry in item.entries if entry.key is not None)
    if not keys:
        return None, [
            Finding(
                CLOSURE_COMPLETENESS,
                f"{unresolved}{closure.path} settles that element with no keyed entry, so the key "
                f"set is empty and every rule over it would pass over nothing",
            )
        ]
    return keys, []


def _key_set(
    premises: Premises,
    reference: str,
    element: DecisionContentElement,
    enumerated: frozenset[str],
) -> tuple[frozenset[str] | None, list[Finding]]:
    """The key set the per-key rule runs over, or the finding that it could not be established.

    One of two places, and the element records which: the keys the record itself enumerates, or the
    set the element's ``keySource`` names. ``None`` is returned exactly when no set could be
    established, which is a failure rather than a pass.
    """
    source = element.key_source
    if source is not None:
        return _named_key_set(premises, reference, element, source)
    if not enumerated:
        return None, [
            Finding(
                CLOSURE_COMPLETENESS,
                f"{reference}: {_element_named(element)} records no keySource and the record "
                f"enumerates no key, so the key set is empty and every rule over it would pass "
                f"over nothing",
            )
        ]
    return enumerated, []


def _keyed(
    premises: Premises, reference: str, element: DecisionContentElement, settled: SettledElement
) -> list[Finding]:
    """Exactly one entry per key, or one explicit exception per key.

    A key with no entry and a key with more than one are distinct findings, each naming the key:
    the first is a key nobody settled, the second is a key settled twice with nothing to choose
    between the two, and a reviewer repairs them differently.

    An entry recorded as an explicit ``exception`` settles its key as an exception rather than by a
    value, which is what lets a key with no answer be a recorded pass rather than a gap. It is one
    entry for that key like any other, so a key carrying an exception *and* a value is settled
    twice.

    A key the record settles that the key set does not carry is reported as well, so a mistyped key
    reads as the entry nothing resolves against rather than only as the key that went missing.
    """
    findings: list[Finding] = []
    entries: dict[str, list[SettledEntry]] = {}
    for entry in settled.entries:
        if entry.key is None:
            findings.append(
                Finding(
                    CLOSURE_COMPLETENESS,
                    f"{reference}: {_element_named(element)} is recorded {KEYED_SHAPE}-shaped, and "
                    f"the record settles it with an entry carrying no key",
                )
            )
            continue
        entries.setdefault(entry.key, []).append(entry)

    keys, failures = _key_set(premises, reference, element, frozenset(entries))
    findings.extend(failures)
    if keys is None:
        return findings

    for key in sorted(keys):
        settling = entries.get(key, [])
        if not settling:
            findings.append(
                Finding(
                    CLOSURE_COMPLETENESS,
                    f"{reference}: {_element_named(element, key)} is settled by no entry, and "
                    f"{_key_set_named(element)} carries that key",
                )
            )
            continue
        if len(settling) > 1:
            findings.append(
                Finding(
                    CLOSURE_COMPLETENESS,
                    f"{reference}: {_element_named(element, key)} is settled by {len(settling)} "
                    f"entries, and exactly one entry or one explicit exception settles a key",
                )
            )
            continue
        findings.extend(_attributes(reference, element, settling[0], key))

    findings.extend(
        Finding(
            CLOSURE_COMPLETENESS,
            f"{reference}: the record settles {_element_named(element, key)}, and "
            f"{_key_set_named(element)} does not carry that key",
        )
        for key in sorted(entries.keys() - keys)
    )
    return findings


def _related(
    reference: str, element: DecisionContentElement, entry: SettledEntry
) -> tuple[frozenset[str], list[Finding]]:
    """The members of the second set one mapping entry relates its key to.

    A string is one member and an array of strings is several. An absent value, a null value and an
    empty array are an entry relating its key to nothing, which the totality rule names rather than
    this one, as is an entry recorded as an explicit exception. Any other value names no member at
    all, and the finding says so with the type the record carried: a relation this check cannot
    read is not a relation it found total.
    """
    value = entry.value
    if entry.exception or value is ABSENT or value is None:
        return frozenset(), []
    if isinstance(value, str):
        return frozenset({value}), []
    if isinstance(value, list) and all(isinstance(member, str) for member in value):
        return frozenset(value), []
    return frozenset(), [
        Finding(
            CLOSURE_COMPLETENESS,
            f"{reference}: {_element_named(element, entry.key)} is settled by an entry whose value "
            f"is {_json_type(value)}, and a {MAPPING_SHAPE} entry relates its key to one member "
            f"named as a string or to several named as an array of them",
        )
    ]


def _mapping(
    premises: Premises, reference: str, element: DecisionContentElement, settled: SettledElement
) -> list[Finding]:
    """A relation total in both directions, naming the unmapped member and the set it came from.

    How a mapping entry records its two sides: the entry's ``key`` is a member of the first set,
    and its ``value`` names the members of the second set that key relates to -- one as a string,
    several as an array of them. The first set is the keys the record settles; the second is the
    set the element's ``keySource`` names, resolved the way a `keyed` element's key set is. That is
    the shape Requirement 9 criteria 4, 5 and 11 read: each role relates to at least one command
    class, each command class the station can emit is related to by at least one role, and the
    finding names the unmapped member *together with the set it came from*, because "unmapped" says
    nothing without saying which side was being exhausted.

    A `mapping` element recording no ``keySource`` is a finding. The second set would otherwise be
    whatever the entries happen to name, which every entry satisfies by construction, and a
    direction that cannot fail is not a direction that was checked.

    Two entries naming one key are two pairs of one relation rather than a key settled twice. That
    is the one place `keyed` and `mapping` read the same record differently, because cardinality is
    `keyed`'s rule and totality is this one's.
    """
    findings: list[Finding] = []
    relation: dict[str, set[str]] = {}
    excused: set[str] = set()
    for entry in settled.entries:
        if entry.key is None:
            findings.append(
                Finding(
                    CLOSURE_COMPLETENESS,
                    f"{reference}: {_element_named(element)} is recorded {MAPPING_SHAPE}-shaped, "
                    f"and the record settles it with an entry carrying no key, so the entry names "
                    f"no member of the first set",
                )
            )
            continue
        members, failures = _related(reference, element, entry)
        findings.extend(failures)
        relation.setdefault(entry.key, set()).update(members)
        # Two reasons a key is excused from the first direction: an explicit exception, which is a
        # recorded pass, and a value this check could not read, which is reported as itself above
        # rather than again as the consequence that the key relates to nothing.
        if entry.exception or failures:
            excused.add(entry.key)

    second, failures = _second_set(premises, reference, element)
    findings.extend(failures)
    if second is None:
        return findings

    findings.extend(
        Finding(
            CLOSURE_COMPLETENESS,
            f"{reference}: the member {key!r} of the keys the record settles relates to no member "
            f"of {_key_set_named(element)} under {_element_named(element)}",
        )
        for key, members in sorted(relation.items())
        if not members and key not in excused
    )
    related = {member for members in relation.values() for member in members}
    findings.extend(
        Finding(
            CLOSURE_COMPLETENESS,
            f"{reference}: the member {member!r} of {_key_set_named(element)} is related to by no "
            f"member of the keys the record settles under {_element_named(element)}",
        )
        for member in sorted(second - related)
    )
    findings.extend(
        Finding(
            CLOSURE_COMPLETENESS,
            f"{reference}: the record relates a key of {_element_named(element)} to {member!r}, "
            f"and {_key_set_named(element)} does not carry that member",
        )
        for member in sorted(related - second)
    )
    return findings


def _second_set(
    premises: Premises, reference: str, element: DecisionContentElement
) -> tuple[frozenset[str] | None, list[Finding]]:
    """The second set of a mapping relation, which the element has to name."""
    source = element.key_source
    if source is None:
        return None, [
            Finding(
                CLOSURE_COMPLETENESS,
                f"{reference}: {_element_named(element)} is recorded {MAPPING_SHAPE}-shaped and "
                f"records no keySource, so the second set of the relation could not be established "
                f"and the direction over it would pass over nothing",
            )
        ]
    return _named_key_set(premises, reference, element, source)


def _shaped(
    premises: Premises, reference: str, element: DecisionContentElement, settled: SettledElement
) -> list[Finding]:
    """Apply the one settlement rule the element's recorded shape names, and the excluded source.

    Three rules and no fourth: ``SETTLEMENT_SHAPES`` has three members, so the last branch is
    `mapping`.
    """
    findings = _excluded_source(reference, element, settled)
    shape = element.settlement_shape
    if shape == SINGLE_SHAPE:
        findings.extend(_single(reference, element, settled))
    elif shape == KEYED_SHAPE:
        findings.extend(_keyed(premises, reference, element, settled))
    else:
        findings.extend(_mapping(premises, reference, element, settled))
    return findings


def _part_settlement(
    premises: Premises, reference: str, part: GatePart, record: ClosureRecord
) -> list[Finding]:
    """Every Decision_Content element the part records, settled by the shape the part records.

    The Gate_Dossier is the authority for the shape, so a record declaring a different one for an
    element is reported and no settlement rule runs over that element: applying either shape would
    check a rule one of the two documents does not record, and the repair is to one of them.

    An element the record settles that the part does not record is reported too. A record settling
    content the dossier never named settles something no owner was recorded as owning.
    """
    findings: list[Finding] = []
    settled: dict[str, SettledElement] = {}
    for item in record.settled:
        if item.element in settled:
            findings.append(
                Finding(
                    CLOSURE_COMPLETENESS,
                    f"{reference}: the record settles the element {item.element!r} more than once, "
                    f"so the settlement rule has more than one set of entries to read",
                )
            )
            continue
        settled[item.element] = item

    for element in part.elements:
        item = settled.get(element.name)
        if item is None:
            findings.append(
                Finding(
                    CLOSURE_COMPLETENESS,
                    f"{reference}: the record settles no entry for {_element_named(element)}, "
                    f"which the Gate_Dossier records as Decision_Content of the part, so the Gate "
                    f"stays OPEN until an amendment settles it",
                )
            )
            continue
        if item.shape != element.settlement_shape:
            findings.append(
                Finding(
                    CLOSURE_COMPLETENESS,
                    f"{reference}: the Gate_Dossier records {_element_named(element)} as "
                    f"{element.settlement_shape}-shaped, and the record settles it as "
                    f"{item.shape}-shaped, so no settlement rule runs over it until the two agree",
                )
            )
            continue
        findings.extend(_shaped(premises, reference, element, item))

    recorded = {element.name for element in part.elements}
    findings.extend(
        Finding(
            CLOSURE_COMPLETENESS,
            f"{reference}: the record settles the element {name!r}, and the Gate_Dossier records "
            f"no Decision_Content element of that name for the part",
        )
        for name in sorted(settled.keys() - recorded)
    )
    return findings


def check_closure_completeness(premises: Premises) -> list[Finding]:
    """Every Decision_Content element a part records is settled by the record that settles the part.

    Requirement 3 criterion 3 is the governing rule: a submitted record carrying no settled entry
    for an element of the Decision_Content it settles holds the Gate at `OPEN`, and the finding
    names the omitted element. Every finding here names the Gate, the part and the element, because
    a reviewer repairs one element of one part at a time.

    The rule runs over the parts a record is in front of -- the ones recorded `submitted` or
    `accepted`. A part recorded `open` names no record and there is nothing to read one against.

    Three generic rules and no per-Gate schema. The element's recorded ``settlementShape`` chooses
    between `single`, `keyed` and `mapping`, and ``requiredAttributes``, ``keySource`` and
    ``excludedSource`` are the rest of what those rules read. Nothing here branches on a Gate
    identifier: the ten Gates settle very different content, and a rule that knew which Gate it was
    reading would be a schema per Gate wearing one function's name.
    """
    findings: list[Finding] = []
    for dossier in premises.register.gates:
        for part in dossier.parts:
            if part.state not in SETTLING_PART_STATES:
                continue
            reference = _part_reference(dossier.id, part.id)
            closure = premises.closures.get(reference)
            if closure is not None:
                findings.extend(_part_settlement(premises, reference, part, closure.record))
            elif part.closure_record is None:
                findings.append(
                    Finding(
                        CLOSURE_COMPLETENESS,
                        f"{reference}: the part is recorded {part.state} and names no "
                        f"Closure_Record, so no element of the Decision_Content it settles is "
                        f"settled",
                    )
                )
            # A part naming a record that did not load is the premise failure load_premises
            # reported against the path. Reporting it again per element would bury it.
    return findings


def _recorded_signatures(reference: str, part: GatePart, record: ClosureRecord) -> list[Finding]:
    """Set equality between the owner set the dossier records and the signatures the record carries.

    Both directions, one finding per party, because the two halves are repaired differently: a
    missing owner is a signature to collect, and an unrecognised signatory is a signature to remove
    or an owner set to widen. Requirement 3 criterion 4 names each owner whose signature is
    required, which a message reporting that the two sets differ would leave a reviewer to work out
    from the register.

    The sign-off date Requirement 3 criterion 1 records alongside the signatures is checked here as
    well. The date on each signature row is the schema's -- it requires one per row, in a bounded
    shape -- so what is left to establish is that the record carries the one the review signs off
    on.
    """
    recorded = frozenset(part.signing_owners)
    signed = frozenset(signature.owner for signature in record.signatures)
    findings = [
        Finding(
            CLOSURE_SIGNATURES,
            f"{reference}: the Gate_Dossier records {owner} as an owner of the Decision_Content "
            f"the record settles, and the record carries no signature from {owner}, whose "
            f"signature is required before the part is accepted",
        )
        for owner in sorted(recorded - signed)
    ]
    findings.extend(
        Finding(
            CLOSURE_SIGNATURES,
            f"{reference}: the record carries a signature from {owner}, whom the Gate_Dossier does "
            f"not record as an owner of the Decision_Content the record settles. It records "
            f"{_owners(recorded)}",
        )
        for owner in sorted(signed - recorded)
    )
    if not record.sign_off_date.strip():
        findings.append(
            Finding(
                CLOSURE_SIGNATURES,
                f"{reference}: the record carries no sign-off date, which Requirement 3 criterion "
                f"1 records as part of the Closure_Record alongside the signature of each owner",
            )
        )
    return findings


def _trailer_parties(trailers: Iterable[str]) -> frozenset[str]:
    """The parties the ``Gate-Signed-By:`` trailers of one commit name.

    A trailer is a key, a colon and a value. Every other trailer a commit carries belongs to
    somebody else's convention and names no signing party.
    """
    parties: set[str] = set()
    for trailer in trailers:
        key, separator, party = trailer.partition(":")
        if separator and key.strip() == SIGNED_BY_KEY and party.strip():
            parties.add(party.strip())
    return frozenset(parties)


def _signature_trailers(
    reference: str, part: GatePart, closure: NamedClosureRecord
) -> list[Finding]:
    """The ``Gate-Signed-By:`` trailers of the commit that added the record, against the owner set.

    The rule runs over an `accepted` record and no other, which is Requirement 3 criterion 1 read
    with the schema. The record carries the commit at which it enters the repository, and that
    commit is `null` while the record is `submitted` -- so a submitted record is one the register
    itself says is not in yet. It is also the record the Gate_Review reads *before* anything
    accepts it, and the hook gates the very commit that would add it, so demanding a trailer on a
    commit the record records as absent would reject every record at the one moment the review
    exists to read it. Once the record is `accepted` the schema requires a 40-character hash, the
    record asserts it is in the repository, and git history has to be able to answer.

    The commit is the one the premises resolved, so this check and ``closure-entry-commit`` read
    one resolution per record and cannot disagree about which commit added it.

    A history that cannot answer is reported, never skipped -- by ``closure-entry-commit``, whose
    subject is that commit, and by this check not at all. One unanswerable history is one defect,
    and reporting it under both check names would read as two.

    The per-signature ``trailer`` the record may record is not read. The commit is the authority for
    its own trailers, and comparing the record against its own copy of them would compare a
    document with itself.
    """
    commit = closure.add_commit
    if closure.record.state != ACCEPTED_RECORD_STATE or commit is None or commit.sha is None:
        return []

    recorded = frozenset(part.signing_owners)
    named = _trailer_parties(commit.trailers)
    findings: list[Finding] = []
    missing = recorded - named
    if missing:
        findings.append(
            Finding(
                CLOSURE_SIGNATURES,
                f"{reference}: the commit {commit.sha} adds the record, its {SIGNED_BY_KEY} "
                f"trailers name {_owners(named)}, and no trailer there names {_owners(missing)}, "
                f"whom the Gate_Dossier records as authorised to sign the part. The comparison is "
                f"{RECORDED_IDENTITY}",
            )
        )
    unrecorded = named - recorded
    if unrecorded:
        findings.append(
            Finding(
                CLOSURE_SIGNATURES,
                f"{reference}: the commit {commit.sha} adds the record, and its {SIGNED_BY_KEY} "
                f"trailers name {_owners(unrecorded)}, whom the Gate_Dossier does not record as "
                f"authorised to sign the part. It records {_owners(recorded)}. The comparison is "
                f"{RECORDED_IDENTITY}",
            )
        )
    return findings


def check_closure_signatures(premises: Premises) -> list[Finding]:
    """Every owner the Gate_Dossier records for a part has signed it, and nobody else has.

    Requirement 3 criterion 4 is the governing rule, and it fails the same way in both directions:
    a record that omits the signature of an owner the dossier records, **or** carries a signature
    from a party the dossier does not record as an owner of that Decision_Content, holds the Gate
    at `OPEN`. Each direction is reported per party, because each names a different repair.

    A part recording an empty owner set is a named failure rather than a vacuous pass. Set equality
    against no owners is satisfied by a record carrying no signature, so the rule that should be
    protecting the part would pass over nothing. ``signingOwners`` admits an empty array on purpose
    -- Requirement 15 criterion 18 cannot name a part as carrying no recorded owner if the register
    cannot express one -- and the message says that an empty owner set authorises nobody rather than
    anybody. It runs over every part the dossier records rather than only the signed ones, because
    the criterion's trigger is the dossier recording no owner and not a record arriving.

    The trailer half compares the parties the adding commit's trailers name against the same
    recorded owner set, and says in its own output that what it establishes is recorded identity
    and no more. It reads the commit the premises resolved, and stays silent where that history
    could not answer: that reason is one defect, reported once, by ``closure-entry-commit``.

    Amendment signatures are not read here. Requirement 3 criterion 4 covers an amendment too, and
    an amendment settles one key carrying its own signatures and its own date, so the owner set
    those rows answer to is the key's rather than the part's. That scoping belongs with the
    amendment rule; reading an amendment's rows against the part's whole owner set here would be
    the conflation that rule exists to prevent.
    """
    findings: list[Finding] = []
    for dossier in premises.register.gates:
        for part in dossier.parts:
            reference = _part_reference(dossier.id, part.id)
            if not part.signing_owners:
                findings.append(
                    Finding(
                        CLOSURE_SIGNATURES,
                        f"{reference}: the Gate_Dossier records no owner authorised to sign the "
                        f"part, so nobody is authorised to sign it rather than anybody being. Set "
                        f"equality against an empty owner set is satisfied by a record carrying no "
                        f"signature, and Requirement 15 criterion 18 names the part as carrying no "
                        f"recorded owner instead of passing it",
                    )
                )
            closure = premises.closures.get(reference)
            if part.state not in SETTLING_PART_STATES or closure is None:
                # A part recorded `submitted` or `accepted` that names no record is
                # `closure-completeness`'s finding, reported once there. A part naming a record
                # that did not load is the premise failure load_premises reported against the path.
                continue
            findings.extend(_recorded_signatures(reference, part, closure.record))
            findings.extend(_signature_trailers(reference, part, closure))
    return findings


def _unsettled_commit(reference: str, closure: NamedClosureRecord) -> list[Finding]:
    """A record not recorded `accepted` carries no commit.

    A file cannot contain the hash of the commit that adds it, so ``entryCommit`` is `null` until
    the record is in and the review has accepted it. ``closure-record.schema.json`` already makes
    the other reading unrepresentable -- its ``else`` branch requires ``entryCommit`` to be null
    for every state other than `accepted`, so a record carrying one is rejected before this check
    sees it, and a commit recorded against a `submitted` part reaches here only from premises built
    without the schema. This guard is therefore the second line and not the first, and it is here
    because this is where the rule is read.
    """
    record = closure.record
    if record.state == ACCEPTED_RECORD_STATE or record.entry_commit is None:
        return []
    return [
        Finding(
            ENTRY_COMMIT,
            f"{reference}: the record at {closure.path} is recorded {record.state} and records "
            f"the entry commit {record.entry_commit}, and a record the register says is not in "
            f"the repository yet carries none: a file cannot contain the commit that adds it. "
            f"{CLOSURE_RECORD_SCHEMA_PATH} rejects it as well, so this is a record that reached "
            f"the check without being validated against the schema",
        )
    ]


def _recorded_commit(reference: str, closure: NamedClosureRecord) -> list[Finding]:
    """An `accepted` record's recorded ``entryCommit`` against the add-commit git history reports.

    The finding names the record, the recorded value and the resolved value. A reviewer has to see
    both to know which of the two is wrong: the record may carry the hash of some other commit, or
    the path may have entered the repository at a commit nobody recorded, and one value alone does
    not tell those apart.

    A history that cannot answer is reported here rather than skipped, naming the record and the
    reason git gave, which is the Error Handling row the design records for it: `pre-commit.yml`
    checks out with full history, so an unanswerable history is a real problem rather than an
    environment quirk. The four reasons are ``common.git``'s, not re-derived here -- git not
    runnable, a tree that is not a repository, unreadable history, and no commit adding the path,
    which probes shallowness.
    """
    record = closure.record
    if record.state != ACCEPTED_RECORD_STATE:
        return []

    commit = closure.add_commit
    if commit is None or commit.sha is None:
        # `None` and a reasonless resolution are the same defect: the premises carry no answer for
        # a record that says it is in the repository. Neither reaches here through load_premises,
        # which resolves one for every accepted record and takes its reason from `common.git`.
        reason = (
            "the premises resolved no add-commit for it"
            if commit is None or commit.reason is None
            else commit.reason
        )
        return [
            Finding(
                ENTRY_COMMIT,
                f"{reference}: the record at {closure.path} is recorded "
                f"{ACCEPTED_RECORD_STATE} and {reason}, so the commit it records could not be "
                f"compared against the commit that adds it. An unresolved premise is a failure "
                f"rather than a pass, and pre-commit.yml checks out full history, so a history "
                f"that cannot answer is a real problem rather than an environment quirk",
            )
        ]

    if record.entry_commit == commit.sha:
        return []
    carried = (
        "records no entry commit"
        if record.entry_commit is None
        else f"records the entry commit {record.entry_commit}"
    )
    return [
        Finding(
            ENTRY_COMMIT,
            f"{reference}: the record at {closure.path} is recorded {ACCEPTED_RECORD_STATE} and "
            f"{carried}, and the commit that adds that path is {commit.sha}",
        )
    ]


def check_closure_entry_commit(premises: Premises) -> list[Finding]:
    """Each Closure_Record's recorded ``entryCommit`` is the commit git history says added it.

    Requirement 3 criterion 1 records the commit at which the record enters the repository as part
    of the Closure_Record, which is the one field of it a file cannot know about itself. So
    ``entryCommit`` is a recorded field with a derived counterpart, like everything else in this
    register: git resolves the commit that added the path, and the two have to agree.

    Two rules and the reason the second can fail. An `accepted` record's recorded commit equals the
    resolved add-commit, and a record in any other state carries no commit at all. A history that
    cannot answer either question fails naming the record and the reason.

    The resolution is the premises', shared with ``closure-signatures``, which reads the trailers
    of the same commit. The reporting stays here, so the finding names the decision this check
    protects -- that the record entered the repository where it says it did.

    The rule runs over the records the premises loaded, in register order, as the other closure
    checks do. A part naming no record is ``closure-completeness``'s finding and a part naming one
    that did not load is the premise failure reported against its path, so neither is reported
    again here.
    """
    findings: list[Finding] = []
    for dossier in premises.register.gates:
        for part in dossier.parts:
            reference = _part_reference(dossier.id, part.id)
            closure = premises.closures.get(reference)
            if closure is None:
                continue
            findings.extend(_unsettled_commit(reference, closure))
            findings.extend(_recorded_commit(reference, closure))
    return findings


def _parts(gate: str, parts: Iterable[str]) -> str:
    """Render a set of one Gate's parts in the ``<GATE>:<part>`` notation the register uses.

    In the order the Gate_Dossier records the parts, which is the order the derivation reports
    them, so a finding reads down the dossier rather than alphabetically across it.
    """
    return ", ".join(_part_reference(gate, part) for part in parts)


def check_gate_status_derivation(premises: Premises) -> list[Finding]:
    """Each Gate's recorded ``status`` equals the derived one: `CLOSED` exactly when every part of
    it is accepted.

    The derivation is ``gate_open``'s, read from the premises rather than re-derived here, so this
    check and the release derivation cannot disagree about which Gates are open. Requirement 3
    criterion 7 is the governing rule in one direction and Requirement 3 criterion 6 supplies the
    quantifier: the part is the unit of acceptance, so a Gate is settled exactly when no part of it
    is unaccepted. There is no special case at one part.

    Both directions are findings, and each names the parts the verdict rests on.

    A Gate recorded `CLOSED` over an unaccepted part is the first, and the finding names **every**
    unaccepted part: Requirement 9 criterion 14, Requirement 10 criterion 15, Requirement 11
    criterion 15, Requirement 12 criterion 17 and Requirement 15 criterion 13 each require the part
    that is not accepted to be named, and a Gate recorded closed with two parts outstanding needs
    both.

    A Gate recorded `OPEN` with every part accepted is the second, and it is not cosmetic.
    Requirement 3 criterion 7 says the register *shall* record the Gate `CLOSED` once every element
    is settled, so a Gate left `OPEN` after its last part was accepted holds its Release_Set shut
    for no recorded reason. The screens stay unreleased either way -- ``released-derivation`` reads
    part acceptance and not this status -- which is exactly why the disagreement has to be reported
    here: nothing else would notice it.
    """
    findings: list[Finding] = []
    for dossier in premises.register.gates:
        openness = premises.derived.gate_open[dossier.id]
        derived = OPEN_STATUS if openness.is_open else CLOSED_STATUS
        if dossier.status == derived:
            continue
        if openness.is_open:
            findings.append(
                Finding(
                    GATE_STATUS_DERIVATION,
                    f"{dossier.id} is recorded {CLOSED_STATUS}, and the Gate_Dossier records "
                    f"{_parts(dossier.id, openness.unaccepted_parts)} as not accepted. A Gate is "
                    f"{CLOSED_STATUS} exactly when every part of it is accepted, and Requirement 3 "
                    f"criterion 7 records the Gate {CLOSED_STATUS} once an accepted "
                    f"Closure_Record settles every element of its Decision_Content",
                )
            )
            continue
        accepted = _parts(dossier.id, (part.id for part in dossier.parts))
        findings.append(
            Finding(
                GATE_STATUS_DERIVATION,
                f"{dossier.id} is recorded {OPEN_STATUS}, and the Gate_Dossier records every part "
                f"of it accepted: {accepted}. Requirement 3 criterion 7 records the Gate "
                f"{CLOSED_STATUS} once every element is settled, and a Gate left {OPEN_STATUS} "
                f"after its last part was accepted holds its Release_Set shut for no recorded "
                f"reason",
            )
        )
    return findings


def _verdicts(reference: str, part: GatePart, record: ClosureRecord) -> list[Finding]:
    """The Sign_Off_Conditions the dossier records for one part, against the verdicts the record
    carries for them.

    Set equality, reported per condition, plus the facts a verdict row has to carry. Each is a
    different repair: an unanswered condition is a verdict to reach, a verdict naming no reviewer
    is an attribution to record, and a verdict for a condition the dossier does not record answers
    a condition nobody was recorded as requiring.
    """
    recorded = {condition.id for condition in part.sign_off_conditions}
    answered = {verdict.id for verdict in record.sign_off_condition_verdicts}

    findings = [
        Finding(
            SIGNOFF_VERDICTS,
            f"{reference}: the Gate_Dossier records the Sign_Off_Condition {condition!r} for the "
            f"part, the part is accepted, and the record carries no verdict on it. A condition is "
            f"not satisfied by going unmentioned",
        )
        for condition in sorted(recorded - answered)
    ]
    findings.extend(
        Finding(
            SIGNOFF_VERDICTS,
            f"{reference}: the record carries a verdict on {condition!r}, and the Gate_Dossier "
            f"records no Sign_Off_Condition of that identifier for the part. It records "
            f"{', '.join(repr(item) for item in sorted(recorded)) or 'no Sign_Off_Condition'}",
        )
        for condition in sorted(answered - recorded)
    )
    for verdict in record.sign_off_condition_verdicts:
        if not verdict.reviewer.strip():
            findings.append(
                Finding(
                    SIGNOFF_VERDICTS,
                    f"{reference}: the verdict on the Sign_Off_Condition {verdict.id!r} names no "
                    f"reviewer. The verdict is a reviewer's judgement, so a verdict nobody is "
                    f"recorded as having reached attributes it to the record instead",
                )
            )
        if not verdict.satisfied:
            findings.append(
                Finding(
                    SIGNOFF_VERDICTS,
                    f"{reference}: the part is accepted, and the verdict {verdict.reviewer} "
                    f"recorded on the Sign_Off_Condition {verdict.id!r} records it unsatisfied. "
                    f"This check does not evaluate the condition -- the verdict is the reviewer's "
                    f"and is read as recorded -- and Requirement 3 criterion 5 records a "
                    f"Closure_Record as accepted when it satisfies every Sign_Off_Condition "
                    f"recorded for the Decision_Content it settles, so the two records disagree",
                )
            )
    return findings


def check_signoff_verdicts(premises: Premises) -> list[Finding]:
    """Every Sign_Off_Condition the dossier records for an accepted part carries a verdict, and
    every verdict names a reviewer.

    **The check establishes totality. It never evaluates a verdict.** Requirement 3 criterion 2
    makes the Gate_Review record, for each Sign_Off_Condition, whether the record satisfies it, and
    criterion 5 makes acceptance conditional on satisfying every one of them -- but *who judges* is
    the reviewer. ``satisfied`` is a boolean a reviewer set. A checker that read a false one as "not
    accepted" would be making the reviewer's call, and one that read a true one as evidence would
    be reading its own input as its own output. What no reviewer can be protected from is a
    condition that was never put to them, which is what this check is for: the set of conditions
    carrying a verdict equals the set the dossier records, or the missing one is named.

    The one place the recorded verdict is read is the inconsistency between two records, and it is
    reported as that rather than as an evaluation. A part the register records accepted whose record
    carries an unsatisfied verdict records both that the condition was not satisfied and that the
    part was accepted anyway, and criterion 5 does not admit both. The finding takes the verdict at
    face value, names the reviewer who reached it, and says in its own text that the condition was
    not evaluated here. The alternative -- staying silent -- would leave the register able to record
    an acceptance its own reviewer contradicted, with every other check passing.

    The rule runs over the parts the register records `accepted`, which is the state ``gate_open``
    reads and the state that closes a Gate and releases screens. A part recorded `submitted` is the
    review in progress: an unanswered condition is what a record in front of a reviewer looks like,
    and an unsatisfied verdict on one is a review doing its job. A part recorded `accepted` that
    names no Closure_Record is ``closure-completeness``'s finding, reported once there.
    """
    findings: list[Finding] = []
    for dossier in premises.register.gates:
        for part in dossier.parts:
            if part.state != ACCEPTED_PART_STATE:
                continue
            reference = _part_reference(dossier.id, part.id)
            closure = premises.closures.get(reference)
            if closure is None:
                continue
            findings.extend(_verdicts(reference, part, closure.record))
    return findings


def _amendment_key(reference: str, record: ClosureRecord, amendment: Amendment) -> list[Finding]:
    """The key an amendment settles is a key the record settles, exactly once.

    A key nothing settles is an amendment that amends nothing: Requirement 3 criterion 11 returns
    the Gate to `OPEN` over an element the record *does* record and leaves settled, so an amendment
    is always against something the record already carries.

    A key the record settles more than once -- in two elements, or twice in one -- is the other
    failure, and it is the one that would break the scoping: the amendment's signatures and date
    would attach to more than one settled entry, and nothing says which.
    """
    settling = [
        element.element
        for element in record.settled
        for entry in element.entries
        if entry.key == amendment.key
    ]
    if not settling:
        return [
            Finding(
                CLOSURE_AMENDMENTS,
                f"{reference}: the amendment settles the key {amendment.key!r}, and the record "
                f"settles no entry under that key, so the amendment amends nothing the record "
                f"records",
            )
        ]
    if len(settling) > 1:
        named = ", ".join(repr(element) for element in sorted(set(settling)))
        return [
            Finding(
                CLOSURE_AMENDMENTS,
                f"{reference}: the amendment settles the key {amendment.key!r}, and the record "
                f"settles that key {len(settling)} times, under {named}, so the amendment's "
                f"signatures and sign-off date would attach to more than one settled entry",
            )
        ]
    return []


def _amendment_signoff(reference: str, part: GatePart, amendment: Amendment) -> list[Finding]:
    """An amendment's signature set against the part's recorded owner set, and its own date.

    The signature halves are the set equality ``closure-signatures`` asserts over the record's own
    rows, in both directions and one finding per party, because Requirement 3 criterion 4 names an
    amendment alongside a submitted record and the owners authorised to sign the part are the owners
    authorised to amend it.

    A part recording no owner at all is ``closure-signatures``'s finding, reported once there. An
    amendment carrying a signature under that empty owner set is still reported here, because an
    empty owner set authorises nobody rather than anybody.

    The date is the amendment's own. Requirement 8 criterion 14 records an amendment's signatures
    *and* sign-off date against its key alone, so the record's date is the part's and says nothing
    about when this key was signed.
    """
    recorded = frozenset(part.signing_owners)
    signed = frozenset(signature.owner for signature in amendment.signatures)
    findings = [
        Finding(
            CLOSURE_AMENDMENTS,
            f"{reference}: the amendment to the key {amendment.key!r} carries no signature from "
            f"{owner}, whom the Gate_Dossier records as an owner of the Decision_Content the "
            f"record settles. An amendment is signed the way the record is, and the signatures it "
            f"carries attach to that key alone",
        )
        for owner in sorted(recorded - signed)
    ]
    findings.extend(
        Finding(
            CLOSURE_AMENDMENTS,
            f"{reference}: the amendment to the key {amendment.key!r} carries a signature from "
            f"{owner}, whom the Gate_Dossier does not record as an owner of the Decision_Content "
            f"the record settles. It records {_owners(recorded)}",
        )
        for owner in sorted(signed - recorded)
    )
    if not amendment.sign_off_date.strip():
        findings.append(
            Finding(
                CLOSURE_AMENDMENTS,
                f"{reference}: the amendment to the key {amendment.key!r} carries no sign-off "
                f"date, and Requirement 8 criterion 14 records an amendment's signatures and "
                f"sign-off date against that key alone, so the date the record carries is not it",
            )
        )
    return findings


def _amendments(reference: str, part: GatePart, record: ClosureRecord) -> list[Finding]:
    """Each amendment the record carries, read against its own key and nothing wider.

    Grouped by key first, so two amendments to one key are one finding naming the key rather than
    two readings of the same ambiguity. The per-amendment rules do not run over such a pair: the
    repair is to choose one of the two, and reporting its key three times would bury that.
    """
    grouped: dict[str, list[Amendment]] = {}
    for amendment in record.amendments or ():
        grouped.setdefault(amendment.key, []).append(amendment)

    findings: list[Finding] = []
    for key, group in grouped.items():
        if len(group) > 1:
            findings.append(
                Finding(
                    CLOSURE_AMENDMENTS,
                    f"{reference}: the record carries {len(group)} amendments to the key {key!r}, "
                    f"and an amendment settles one key: two of them carry two sets of signatures "
                    f"and two sign-off dates against that key, with nothing to choose between "
                    f"them",
                )
            )
            continue
        (amendment,) = group
        findings.extend(_amendment_key(reference, record, amendment))
        findings.extend(_amendment_signoff(reference, part, amendment))
    return findings


def check_closure_amendments(premises: Premises) -> list[Finding]:
    """An amendment settles one key the record settles, carrying its own signatures and sign-off
    date against that key alone.

    Requirement 3 criterion 11 is the governing rule, and its third clause is the load-bearing one:
    an accepted Closure_Record found to carry no settled entry for a recorded element returns the
    Gate to `OPEN`, names the omitted element and the amendment required, **and retains the accepted
    status of the elements the record settles.** Requirement 8 criteria 5 and 14 are the worked
    case: a freshness class `DG-TLM` names that an accepted `DG-FRS` record has not settled is
    amended in, and the amendment's signatures and sign-off date are recorded against that class
    alone while every class signed earlier keeps its recorded acceptance.

    So the whole of this check is scope. Every rule reads one amendment's own key: that the record
    settles it, that it settles it once, that the amendment's signature set equals the owner set the
    dossier records for the part, and that the amendment carries a sign-off date of its own. No rule
    reads another key, the record's own signature rows or the record's own date -- which is what
    makes applying an amendment unable to disturb an acceptance already recorded.

    An amendment's ``entryCommit`` is not compared against git history, and the reason is worth
    recording rather than leaving as an omission. The resolution the premises carry is an
    *add*-commit: ``git log --diff-filter=A`` reports the commit that added the record's path. An
    amendment is a change to a file already added, so it enters at a modify-commit that resolution
    by construction never reports, and comparing an amendment's commit against the record's
    add-commit would compare it against a commit that cannot be its own. What the schema does
    establish is the shape -- null, or forty hex characters -- and what would be needed beyond it is
    a modify-commit resolution per amendment, which ``common.git`` does not offer and this check
    does not invent. "Not resolved" is said here rather than read as "agrees".

    The rule runs over every Closure_Record the premises loaded, whatever its state. An amendment
    to a key the record does not settle is wrong wherever it is recorded, in the way a recorded
    observation that resolves against nothing is wrong whatever its row's status.
    """
    findings: list[Finding] = []
    for dossier in premises.register.gates:
        for part in dossier.parts:
            reference = _part_reference(dossier.id, part.id)
            closure = premises.closures.get(reference)
            if closure is None:
                continue
            findings.extend(_amendments(reference, part, closure.record))
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
    check_closure_completeness,
    check_closure_signatures,
    check_closure_entry_commit,
    check_gate_status_derivation,
    check_signoff_verdicts,
    check_closure_amendments,
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
    if premises.closures:
        # Whether the run passes or fails, because the passing case is where a reader is most
        # likely to take a signature for more than it is.
        print(SIGNATURE_NOTICE)
    if findings:
        return report(findings)

    # The run states what it established rather than that the register is valid, and every figure
    # in it is measured here and now: a remembered count is the one that goes stale.
    print(
        f"{REGISTER_PATH} validates against {REGISTER_SCHEMA_PATH} and agrees with "
        f"{premises.register.inventory.path}: {len(premises.register.gates)} Gate_Dossiers, "
        f"{len(premises.register.screens)} screen records, "
        f"{len(premises.closures)} Closure_Records, "
        f"{len(premises.derived.undelivered)} undelivered of {len(premises.inventory.rows)} rows"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

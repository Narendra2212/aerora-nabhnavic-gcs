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
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Any, Final

from common.gate_register import (
    DELIVERED_STATUSES,
    GATE_IDS,
    GateRegisterError,
    ScreenInventoryError,
    blocking_gates,
    load_gate_register,
    parse_screen_inventory,
    release_sets,
    released,
    two_gate_screens,
    undelivered,
)
from jsonschema import Draft202012Validator
from jsonschema.exceptions import SchemaError

if TYPE_CHECKING:
    from collections.abc import Iterable, Mapping, Sequence

    from common.gate_register import GateRegister, ScreenInventory, ScreenRelease

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


def _gates(gates: Iterable[str]) -> str:
    """Render a Gate set in the order :data:`GATE_IDS` declares it.

    A message therefore reads the same way however the register happened to order the entries that
    produced it, which is what lets a reviewer diff one run's findings against another's.
    """
    ordered = sorted(gates, key=lambda gate: (_GATE_ORDER.get(gate, len(GATE_IDS)), gate))
    return ", ".join(ordered)


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


# The derived checks, in the order the design's table lists them. Each reports findings carrying
# its own check name, so the registry needs no name of its own.
_CHECKS: Final = (
    check_partition,
    check_release_set_derivation,
    check_two_gate,
    check_released_derivation,
    check_delivered_outside,
    check_inventory_coverage,
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

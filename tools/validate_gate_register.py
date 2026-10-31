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
    GateRegisterError,
    ScreenInventoryError,
    load_gate_register,
    parse_screen_inventory,
)
from jsonschema import Draft202012Validator
from jsonschema.exceptions import SchemaError

if TYPE_CHECKING:
    from collections.abc import Sequence

    from common.gate_register import GateRegister, ScreenInventory

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
class Premises:
    """The documents every derived check reads, read and validated once each.

    ``root`` is how a check reaches a third file -- a test source, a guard declaration, git history
    -- without discovering the repository for itself.
    """

    root: Path
    register: GateRegister
    inventory: ScreenInventory


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

    return Premises(root=root, register=register, inventory=inventory)


def report(findings: Sequence[Finding]) -> int:
    """Print every finding and return the exit status, which is non-zero for any of them."""
    for finding in findings:
        print(finding)
    return int(bool(findings))


def main() -> int:
    premises = load_premises(ROOT)
    if isinstance(premises, list):
        return report(premises)

    # The run states what it established rather than that the register is valid, and every figure
    # in it is measured here and now: a remembered count is the one that goes stale.
    print(
        f"{REGISTER_PATH} validates against {REGISTER_SCHEMA_PATH}; "
        f"{len(premises.register.gates)} Gate_Dossiers and {len(premises.register.screens)} screen "
        f"records read, against {len(premises.inventory.rows)} screen rows in "
        f"{premises.register.inventory.path}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

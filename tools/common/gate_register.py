"""Typed model for the Aerora Gate_Register and its Closure_Records.

The two schemas under ``.kiro/specs/aerora-decision-gate-closure/`` are the authority for every
field name here. This module mirrors them so that a schema-valid document survives
``dump_gate_register(load_gate_register(document)) == document``, which the Gate_Review's two entry
points and the property tests over the derivations all rest on. It therefore stays a model: it reads
no file, discovers no path, runs no subprocess and reaches no network.

Two conventions carry the round trip:

- An optional key is ``None`` when the document does not carry it. No optional key in either schema
  admits an explicit null, so absent and null cannot be confused -- except
  ``settled[].entries[].value``, whose settled value is whatever an owner signs it as and may itself
  be null, so its absence is :data:`ABSENT` instead.
- A required nullable key -- ``closureRecord``, ``observation``, ``entryCommit`` -- is ``None`` for
  the null the schema requires while nothing has settled it, and is always emitted.

What the loader enforces is shape: every required key present, no unrecognised key, each value's
JSON type, and the closed vocabularies the derivations branch on. It does not re-implement the
schemas' string patterns, lengths or formats -- the schemas are the authority for those and the entry
point validates against them first -- and it reads no Decision_Content: a Gate's
``openStateReading.class`` and every settled value are an owner's record, not a vocabulary this
module knows.

:func:`parse_screen_inventory` is here on the same terms. The screen inventory is the other document
the Gate_Review reads, it is the authority for the screen set and for each screen's delivery status,
and the checks over it are predicates over its rows -- so the rows belong beside the model the
predicates read, and the parser takes the document's text rather than finding the document.
"""

from __future__ import annotations

import re
from collections.abc import Mapping
from dataclasses import dataclass
from enum import Enum
from typing import TYPE_CHECKING, Any, Final, TypeVar

from .markdown import parse_md_tables

if TYPE_CHECKING:
    from collections.abc import Callable, Collection

    from .markdown import MarkdownRow

__all__ = [
    "ABSENT",
    "CLASSIFICATIONS",
    "GATE_IDS",
    "GATE_STATUSES",
    "INVENTORY_COLUMNS",
    "NO_GATE",
    "OWNER_LABELS",
    "PART_STATES",
    "RECORD_STATES",
    "SETTLEMENT_SHAPES",
    "Absent",
    "Amendment",
    "BlockingEntry",
    "ClosureRecord",
    "ContractSurfaceElement",
    "DecisionContentElement",
    "Disagreement",
    "ExcludedAcceptanceCriterion",
    "ForbiddenUntil",
    "GateDossier",
    "GatePart",
    "GateRegister",
    "GateRegisterError",
    "Guard",
    "GuardPermission",
    "InventoryArea",
    "InventoryReference",
    "InventoryRow",
    "Observation",
    "OpenStateReading",
    "RationaleReference",
    "ReadOnlyException",
    "ReadOnlyPosture",
    "ReleaseSetEntry",
    "ScreenInventory",
    "ScreenInventoryError",
    "ScreenRecord",
    "SettledElement",
    "SettledEntry",
    "SignOffCondition",
    "SignOffConditionVerdict",
    "Signature",
    "dump_closure_record",
    "dump_gate_register",
    "load_closure_record",
    "load_gate_register",
    "parse_screen_inventory",
]

# The ten Gate identifiers of Requirement 1 criterion 1, in the order it names them. There is no
# eleventh, and the order is fixed so that a report over the ten reads the same way every run.
GATE_IDS: Final = (
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

# The programme owner labels requirements.md names as authorised to sign, to implement or to
# approve. These are not operator role names: those are Decision_Content of DG-ROL and nothing here
# names one.
OWNER_LABELS: Final = frozenset(
    {
        "Product",
        "Firmware",
        "Architecture",
        "operations",
        "engineering",
        "security architecture",
        "the external interface authority",
        "the process owner",
        "Tab 1",
        "Tab 2",
        "Tab 3",
        "Tab 4",
        "Tab 5",
        "Tab 6",
        "Tab 7",
        "Tab 8",
    }
)

GATE_STATUSES: Final = frozenset({"OPEN", "CLOSED"})
PART_STATES: Final = frozenset({"open", "submitted", "accepted"})
RECORD_STATES: Final = frozenset({"submitted", "accepted"})
CLASSIFICATIONS: Final = frozenset({"Gate", "Acceptance_Criterion"})
SETTLEMENT_SHAPES: Final = frozenset({"single", "keyed", "mapping"})

# What a contract-surface element records instead of a Gate when no Gate's Decision_Content would
# record it. The schema requires a reason alongside it.
NO_GATE: Final = "none"


class Absent(Enum):
    """Marker type for an optional key the document does not carry.

    An ``Enum`` member rather than a bare object so the marker stays a singleton through copying and
    compares by identity.
    """

    TOKEN = "absent"


ABSENT: Final = Absent.TOKEN


class GateRegisterError(ValueError):
    """Raised when a document does not match the shape of its schema."""


class ScreenInventoryError(GateRegisterError):
    """Raised when the screen inventory's area tables cannot be read.

    A subclass, so an entry point reading both documents catches one type. Where the register's
    reader names the key at fault, this one names the line.
    """


# ---------------------------------------------------------------------------
# Gate_Register
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class InventoryReference:
    """The screen inventory the register references and the facts that document owns."""

    path: str
    authoritative_for: list[str]


@dataclass(frozen=True)
class RationaleReference:
    """A reference to a Gate's narrative rather than a copy of it."""

    document: str
    anchor: str


@dataclass(frozen=True)
class DecisionContentElement:
    """One element of a Gate's Decision_Content, named rather than valued."""

    name: str
    source: str | None
    settlement_shape: str
    required_attributes: list[str] | None
    key_source: str | None
    excluded_source: str | None


@dataclass(frozen=True)
class SignOffCondition:
    """A condition a reviewer checks against a Closure_Record by inspection."""

    id: str
    requirement: str
    text: str
    verification: str | None


@dataclass(frozen=True)
class GatePart:
    """One part of a Gate's Decision_Content: the unit of signature, condition and acceptance."""

    id: str
    signing_owners: list[str]
    state: str
    closure_record: str | None
    elements: list[DecisionContentElement]
    sign_off_conditions: list[SignOffCondition]


@dataclass(frozen=True)
class ReleaseSetEntry:
    """A screen a Gate releases, with the half it releases where a screen is released in halves."""

    screen: str
    part: str | None


@dataclass(frozen=True)
class ExcludedAcceptanceCriterion:
    """An Acceptance_Criterion a Gate's Decision_Content explicitly does not settle."""

    id: str
    screen: str | None
    requirement: str | None


@dataclass(frozen=True)
class OpenStateReading:
    """The reading a Gate requires while it is OPEN.

    ``state_class`` mirrors the schema's ``class`` key, which Python cannot spell as an attribute.
    Its value is the Gate's own recorded content and this module enumerates no candidate for it.
    """

    state_class: str
    reports_gate_identifier: bool


@dataclass(frozen=True)
class GateDossier:
    """One Gate's dossier: its status, Decision_Content, owners, conditions and Release_Set."""

    id: str
    status: str
    implementing_owner: str
    rationale: RationaleReference
    parts: list[GatePart]
    release_set: list[ReleaseSetEntry]
    excluded_acceptance_criteria: list[ExcludedAcceptanceCriterion]
    guards: list[str]
    rests_on_review_alone: bool | None
    open_state_reading: OpenStateReading


@dataclass(frozen=True)
class Observation:
    """The recorded observation of a screen navigated to and drawn in a booted window."""

    suite: str
    slot: str


@dataclass(frozen=True)
class BlockingEntry:
    """One entry of a screen's blocking list, classified as a Gate or an Acceptance_Criterion."""

    id: str
    classification: str
    gate: str | None
    reclassified_from: str | None
    requirement: str | None
    inventory_text: str | None


@dataclass(frozen=True)
class ScreenRecord:
    """One screen's Released flag, its VERIFIED observation and its classified blocking entries."""

    id: str
    released: bool
    observation: Observation | None
    blocking_entries: list[BlockingEntry]


@dataclass(frozen=True)
class Disagreement:
    """A guard whose source value differs from its recorded one, with both values pinned."""

    observed_value: str
    against: list[str]
    owner: str
    remedies: list[str]
    hand_on_row: str


@dataclass(frozen=True)
class Guard:
    """One mechanical guard that holds an open Gate visible.

    ``holds_no_gate_open`` is the guard-side counterpart of a dossier's ``rests_on_review_alone``:
    Requirement 4 criterion 8's obligation runs in both directions, so an empty ``holds_open`` is
    accompanied by an explicit record rather than read off the emptiness.
    """

    symbol: str
    declared_in: str
    recorded_value: str
    asserted_in: list[str]
    holds_open: list[str]
    holds_no_gate_open: bool | None
    permitted_to_change_by: list[str]
    disagreement: Disagreement | None


@dataclass(frozen=True)
class ReadOnlyException:
    """The single recorded exception to read-only presentation, pinned to one control."""

    screen: str
    file: str
    control: str
    reason: str
    requirement: str | None


@dataclass(frozen=True)
class ForbiddenUntil:
    """An item held FORBIDDEN-UNTIL a Gate is recorded CLOSED."""

    id: str
    gate: str
    requirement: str | None


@dataclass(frozen=True)
class ReadOnlyPosture:
    """The read-only posture, which reads one Gate and nothing else."""

    gate: str
    exceptions: list[ReadOnlyException]
    forbidden_until: list[ForbiddenUntil]
    pre_approved_undeclared_seams: list[str]


@dataclass(frozen=True)
class ContractSurfaceElement:
    """One recorded contract-surface element, mapped to a Gate or to none with a reason."""

    symbol: str
    kind: str
    gate: str
    reason: str | None


@dataclass(frozen=True)
class GateRegister:
    """The single authoritative record of the ten Gates and the axes that read against them."""

    schema_ref: str | None
    inventory: InventoryReference
    gates: list[GateDossier]
    screens: list[ScreenRecord]
    guards: list[Guard]
    read_only: ReadOnlyPosture
    contract_surface: list[ContractSurfaceElement]


# ---------------------------------------------------------------------------
# Closure_Record
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Signature:
    """One signing owner's row. This is recorded identity, not cryptographic authorship."""

    owner: str
    date: str
    trailer: str | None
    signature_verified: bool | None


@dataclass(frozen=True)
class SignOffConditionVerdict:
    """A reviewer's verdict on one Sign_Off_Condition. Nothing here evaluates the verdict."""

    id: str
    satisfied: bool
    reviewer: str
    date: str | None
    note: str | None


@dataclass(frozen=True)
class GuardPermission:
    """Whether accepting this part permits one guard to change value. There is no default."""

    symbol: str
    permits: bool
    reason: str | None


@dataclass(frozen=True)
class SettledEntry:
    """One settled entry.

    ``value`` is :data:`ABSENT` when the document carries no ``value`` key, which an entry settled
    as an explicit ``exception`` does not, and ``None`` when it carries an explicit null.
    """

    key: str | None
    value: Any
    source: str | None
    exception: bool | None


@dataclass(frozen=True)
class SettledElement:
    """One Decision_Content element this record settles, in the shape the dossier records."""

    element: str
    shape: str
    entries: list[SettledEntry]


@dataclass(frozen=True)
class Amendment:
    """One key settled later, carrying its own signatures and date against that key alone."""

    key: str
    sign_off_date: str
    signatures: list[Signature]
    entry_commit: str | None


@dataclass(frozen=True)
class ClosureRecord:
    """One part of one Gate's Closure_Record. The unit of signature is the part."""

    schema_ref: str | None
    gate: str
    part: str
    state: str
    sign_off_date: str
    entry_commit: str | None
    signatures: list[Signature]
    sign_off_condition_verdicts: list[SignOffConditionVerdict]
    guard_permissions: list[GuardPermission]
    settled: list[SettledElement]
    amendments: list[Amendment] | None


# ---------------------------------------------------------------------------
# Reading
# ---------------------------------------------------------------------------

_T = TypeVar("_T")

_MISSING: Final = object()

_GATE_ID_DESCRIPTION: Final = "one of the ten Gate identifiers"
_OWNER_DESCRIPTION: Final = "a recorded owner label"


class _Reader:
    """Reads one JSON object, naming the key at fault in every rejection.

    Each accessor records the key it consumed so :meth:`done` can report an unrecognised key by
    name. Both schemas close ``additionalProperties``, and a model that dropped an extra key instead
    of reporting it would break the round trip while looking like it had succeeded.
    """

    def __init__(self, value: Any, path: str) -> None:
        if not isinstance(value, Mapping):
            raise GateRegisterError(f"{path} must be an object")
        self._document: Mapping[str, Any] = value
        self._path = path
        self._consumed: set[str] = set()

    def done(self) -> None:
        """Reject any key no accessor asked for."""
        unrecognised = sorted(key for key in self._document if key not in self._consumed)
        if unrecognised:
            raise GateRegisterError(
                f"{self._path} carries unrecognised keys: {', '.join(unrecognised)}"
            )

    def text(self, key: str) -> str:
        return self._text(self._required(key), key)

    def optional_text(self, key: str) -> str | None:
        value = self._present(key)
        return None if value is _MISSING else self._text(value, key)

    def nullable_text(self, key: str) -> str | None:
        value = self._required(key)
        return None if value is None else self._text(value, key)

    def member(self, key: str, vocabulary: Collection[str], described: str) -> str:
        return self._member(self.text(key), key, vocabulary, described)

    def optional_member(self, key: str, vocabulary: Collection[str], described: str) -> str | None:
        value = self.optional_text(key)
        return None if value is None else self._member(value, key, vocabulary, described)

    def flag(self, key: str) -> bool:
        return self._flag(self._required(key), key)

    def optional_flag(self, key: str) -> bool | None:
        value = self._present(key)
        return None if value is _MISSING else self._flag(value, key)

    def text_list(self, key: str) -> list[str]:
        return [
            self._text(item, f"{key}[{index}]")
            for index, item in enumerate(self._array(self._required(key), key))
        ]

    def optional_text_list(self, key: str) -> list[str] | None:
        value = self._present(key)
        if value is _MISSING:
            return None
        return [
            self._text(item, f"{key}[{index}]")
            for index, item in enumerate(self._array(value, key))
        ]

    def member_list(self, key: str, vocabulary: Collection[str], described: str) -> list[str]:
        return [
            self._member(item, f"{key}[{index}]", vocabulary, described)
            for index, item in enumerate(self.text_list(key))
        ]

    def child(self, key: str, load: Callable[[Any, str], _T]) -> _T:
        return load(self._required(key), self._child(key))

    def nullable_child(self, key: str, load: Callable[[Any, str], _T]) -> _T | None:
        value = self._required(key)
        return None if value is None else load(value, self._child(key))

    def optional_child(self, key: str, load: Callable[[Any, str], _T]) -> _T | None:
        value = self._present(key)
        return None if value is _MISSING else load(value, self._child(key))

    def children(self, key: str, load: Callable[[Any, str], _T]) -> list[_T]:
        return self._children(self._array(self._required(key), key), key, load)

    def optional_children(self, key: str, load: Callable[[Any, str], _T]) -> list[_T] | None:
        value = self._present(key)
        if value is _MISSING:
            return None
        return self._children(self._array(value, key), key, load)

    def value_or_absent(self, key: str) -> Any:
        """Return the settled value, or :data:`ABSENT` when the document carries no such key."""
        value = self._present(key)
        return ABSENT if value is _MISSING else value

    def _required(self, key: str) -> Any:
        self._consumed.add(key)
        if key not in self._document:
            raise GateRegisterError(f"{self._child(key)} is required")
        return self._document[key]

    def _present(self, key: str) -> Any:
        self._consumed.add(key)
        return self._document.get(key, _MISSING)

    def _child(self, key: str) -> str:
        return f"{self._path}.{key}"

    def _children(self, values: list[Any], key: str, load: Callable[[Any, str], _T]) -> list[_T]:
        base = self._child(key)
        return [load(item, f"{base}[{index}]") for index, item in enumerate(values)]

    def _text(self, value: Any, key: str) -> str:
        if not isinstance(value, str):
            raise GateRegisterError(f"{self._child(key)} must be a string")
        return value

    def _flag(self, value: Any, key: str) -> bool:
        if not isinstance(value, bool):
            raise GateRegisterError(f"{self._child(key)} must be a boolean")
        return value

    def _array(self, value: Any, key: str) -> list[Any]:
        if not isinstance(value, list):
            raise GateRegisterError(f"{self._child(key)} must be an array")
        return value

    def _member(self, value: str, key: str, vocabulary: Collection[str], described: str) -> str:
        if value not in vocabulary:
            raise GateRegisterError(f"{self._child(key)} must be {described}, not {value!r}")
        return value


def _read_inventory(value: Any, path: str) -> InventoryReference:
    reader = _Reader(value, path)
    inventory = InventoryReference(
        path=reader.text("path"),
        authoritative_for=reader.text_list("authoritativeFor"),
    )
    reader.done()
    return inventory


def _read_rationale(value: Any, path: str) -> RationaleReference:
    reader = _Reader(value, path)
    rationale = RationaleReference(
        document=reader.text("document"),
        anchor=reader.text("anchor"),
    )
    reader.done()
    return rationale


def _read_decision_content_element(value: Any, path: str) -> DecisionContentElement:
    reader = _Reader(value, path)
    element = DecisionContentElement(
        name=reader.text("name"),
        source=reader.optional_text("source"),
        settlement_shape=reader.member(
            "settlementShape", SETTLEMENT_SHAPES, "a recorded settlement shape"
        ),
        required_attributes=reader.optional_text_list("requiredAttributes"),
        key_source=reader.optional_text("keySource"),
        excluded_source=reader.optional_text("excludedSource"),
    )
    reader.done()
    return element


def _read_sign_off_condition(value: Any, path: str) -> SignOffCondition:
    reader = _Reader(value, path)
    condition = SignOffCondition(
        id=reader.text("id"),
        requirement=reader.text("requirement"),
        text=reader.text("text"),
        verification=reader.optional_text("verification"),
    )
    reader.done()
    return condition


def _read_gate_part(value: Any, path: str) -> GatePart:
    reader = _Reader(value, path)
    part = GatePart(
        id=reader.text("id"),
        signing_owners=reader.member_list("signingOwners", OWNER_LABELS, _OWNER_DESCRIPTION),
        state=reader.member("state", PART_STATES, "a recorded part state"),
        closure_record=reader.nullable_text("closureRecord"),
        elements=reader.children("elements", _read_decision_content_element),
        sign_off_conditions=reader.children("signOffConditions", _read_sign_off_condition),
    )
    reader.done()
    return part


def _read_release_set_entry(value: Any, path: str) -> ReleaseSetEntry:
    reader = _Reader(value, path)
    entry = ReleaseSetEntry(
        screen=reader.text("screen"),
        part=reader.optional_text("part"),
    )
    reader.done()
    return entry


def _read_excluded_acceptance_criterion(value: Any, path: str) -> ExcludedAcceptanceCriterion:
    reader = _Reader(value, path)
    criterion = ExcludedAcceptanceCriterion(
        id=reader.text("id"),
        screen=reader.optional_text("screen"),
        requirement=reader.optional_text("requirement"),
    )
    reader.done()
    return criterion


def _read_open_state_reading(value: Any, path: str) -> OpenStateReading:
    reader = _Reader(value, path)
    reading = OpenStateReading(
        state_class=reader.text("class"),
        reports_gate_identifier=reader.flag("reportsGateIdentifier"),
    )
    reader.done()
    return reading


def _read_gate_dossier(value: Any, path: str) -> GateDossier:
    reader = _Reader(value, path)
    dossier = GateDossier(
        id=reader.member("id", GATE_IDS, _GATE_ID_DESCRIPTION),
        status=reader.member("status", GATE_STATUSES, "a recorded Gate status"),
        implementing_owner=reader.member("implementingOwner", OWNER_LABELS, _OWNER_DESCRIPTION),
        rationale=reader.child("rationale", _read_rationale),
        parts=reader.children("parts", _read_gate_part),
        release_set=reader.children("releaseSet", _read_release_set_entry),
        excluded_acceptance_criteria=reader.children(
            "excludedAcceptanceCriteria", _read_excluded_acceptance_criterion
        ),
        guards=reader.text_list("guards"),
        rests_on_review_alone=reader.optional_flag("restsOnReviewAlone"),
        open_state_reading=reader.child("openStateReading", _read_open_state_reading),
    )
    reader.done()
    return dossier


def _read_observation(value: Any, path: str) -> Observation:
    reader = _Reader(value, path)
    observation = Observation(
        suite=reader.text("suite"),
        slot=reader.text("slot"),
    )
    reader.done()
    return observation


def _read_blocking_entry(value: Any, path: str) -> BlockingEntry:
    reader = _Reader(value, path)
    entry = BlockingEntry(
        id=reader.text("id"),
        classification=reader.member("classification", CLASSIFICATIONS, "a classification"),
        gate=reader.optional_member("gate", GATE_IDS, _GATE_ID_DESCRIPTION),
        reclassified_from=reader.optional_member(
            "reclassifiedFrom", CLASSIFICATIONS, "a classification"
        ),
        requirement=reader.optional_text("requirement"),
        inventory_text=reader.optional_text("inventoryText"),
    )
    reader.done()
    return entry


def _read_screen_record(value: Any, path: str) -> ScreenRecord:
    reader = _Reader(value, path)
    record = ScreenRecord(
        id=reader.text("id"),
        released=reader.flag("released"),
        observation=reader.nullable_child("observation", _read_observation),
        blocking_entries=reader.children("blockingEntries", _read_blocking_entry),
    )
    reader.done()
    return record


def _read_disagreement(value: Any, path: str) -> Disagreement:
    reader = _Reader(value, path)
    disagreement = Disagreement(
        observed_value=reader.text("observedValue"),
        against=reader.text_list("against"),
        owner=reader.member("owner", OWNER_LABELS, _OWNER_DESCRIPTION),
        remedies=reader.text_list("remedies"),
        hand_on_row=reader.text("handOnRow"),
    )
    reader.done()
    return disagreement


def _read_guard(value: Any, path: str) -> Guard:
    reader = _Reader(value, path)
    guard = Guard(
        symbol=reader.text("symbol"),
        declared_in=reader.text("declaredIn"),
        recorded_value=reader.text("recordedValue"),
        asserted_in=reader.text_list("assertedIn"),
        holds_open=reader.member_list("holdsOpen", GATE_IDS, _GATE_ID_DESCRIPTION),
        holds_no_gate_open=reader.optional_flag("holdsNoGateOpen"),
        permitted_to_change_by=reader.text_list("permittedToChangeBy"),
        disagreement=reader.optional_child("disagreement", _read_disagreement),
    )
    reader.done()
    return guard


def _read_read_only_exception(value: Any, path: str) -> ReadOnlyException:
    reader = _Reader(value, path)
    exception = ReadOnlyException(
        screen=reader.text("screen"),
        file=reader.text("file"),
        control=reader.text("control"),
        reason=reader.text("reason"),
        requirement=reader.optional_text("requirement"),
    )
    reader.done()
    return exception


def _read_forbidden_until(value: Any, path: str) -> ForbiddenUntil:
    reader = _Reader(value, path)
    item = ForbiddenUntil(
        id=reader.text("id"),
        gate=reader.member("gate", GATE_IDS, _GATE_ID_DESCRIPTION),
        requirement=reader.optional_text("requirement"),
    )
    reader.done()
    return item


def _read_read_only_posture(value: Any, path: str) -> ReadOnlyPosture:
    reader = _Reader(value, path)
    posture = ReadOnlyPosture(
        gate=reader.member("gate", GATE_IDS, _GATE_ID_DESCRIPTION),
        exceptions=reader.children("exceptions", _read_read_only_exception),
        forbidden_until=reader.children("forbiddenUntil", _read_forbidden_until),
        pre_approved_undeclared_seams=reader.text_list("preApprovedUndeclaredSeams"),
    )
    reader.done()
    return posture


def _read_contract_surface_element(value: Any, path: str) -> ContractSurfaceElement:
    reader = _Reader(value, path)
    element = ContractSurfaceElement(
        symbol=reader.text("symbol"),
        kind=reader.text("kind"),
        gate=reader.member("gate", (*GATE_IDS, NO_GATE), f"{_GATE_ID_DESCRIPTION} or {NO_GATE!r}"),
        reason=reader.optional_text("reason"),
    )
    reader.done()
    return element


def load_gate_register(document: Any, *, path: str = "gate-register") -> GateRegister:
    """Read a Gate_Register document into the model, naming the key at fault on rejection.

    *document* is already-parsed JSON, so this stays a model: callers read the file. *path* names
    the document in each message.
    """
    reader = _Reader(document, path)
    register = GateRegister(
        schema_ref=reader.optional_text("$schema"),
        inventory=reader.child("inventory", _read_inventory),
        gates=reader.children("gates", _read_gate_dossier),
        screens=reader.children("screens", _read_screen_record),
        guards=reader.children("guards", _read_guard),
        read_only=reader.child("readOnly", _read_read_only_posture),
        contract_surface=reader.children("contractSurface", _read_contract_surface_element),
    )
    reader.done()
    return register


def _read_signature(value: Any, path: str) -> Signature:
    reader = _Reader(value, path)
    signature = Signature(
        owner=reader.member("owner", OWNER_LABELS, _OWNER_DESCRIPTION),
        date=reader.text("date"),
        trailer=reader.optional_text("trailer"),
        signature_verified=reader.optional_flag("signatureVerified"),
    )
    reader.done()
    return signature


def _read_sign_off_condition_verdict(value: Any, path: str) -> SignOffConditionVerdict:
    reader = _Reader(value, path)
    verdict = SignOffConditionVerdict(
        id=reader.text("id"),
        satisfied=reader.flag("satisfied"),
        reviewer=reader.text("reviewer"),
        date=reader.optional_text("date"),
        note=reader.optional_text("note"),
    )
    reader.done()
    return verdict


def _read_guard_permission(value: Any, path: str) -> GuardPermission:
    reader = _Reader(value, path)
    permission = GuardPermission(
        symbol=reader.text("symbol"),
        permits=reader.flag("permits"),
        reason=reader.optional_text("reason"),
    )
    reader.done()
    return permission


def _read_settled_entry(value: Any, path: str) -> SettledEntry:
    reader = _Reader(value, path)
    entry = SettledEntry(
        key=reader.optional_text("key"),
        value=reader.value_or_absent("value"),
        source=reader.optional_text("source"),
        exception=reader.optional_flag("exception"),
    )
    reader.done()
    return entry


def _read_settled_element(value: Any, path: str) -> SettledElement:
    reader = _Reader(value, path)
    element = SettledElement(
        element=reader.text("element"),
        shape=reader.member("shape", SETTLEMENT_SHAPES, "a recorded settlement shape"),
        entries=reader.children("entries", _read_settled_entry),
    )
    reader.done()
    return element


def _read_amendment(value: Any, path: str) -> Amendment:
    reader = _Reader(value, path)
    amendment = Amendment(
        key=reader.text("key"),
        sign_off_date=reader.text("signOffDate"),
        signatures=reader.children("signatures", _read_signature),
        entry_commit=reader.nullable_text("entryCommit"),
    )
    reader.done()
    return amendment


def load_closure_record(document: Any, *, path: str = "closure-record") -> ClosureRecord:
    """Read one part's Closure_Record document into the model.

    *document* is already-parsed JSON, so this stays a model: callers read the file. *path* names
    the document in each message.
    """
    reader = _Reader(document, path)
    record = ClosureRecord(
        schema_ref=reader.optional_text("$schema"),
        gate=reader.member("gate", GATE_IDS, _GATE_ID_DESCRIPTION),
        part=reader.text("part"),
        state=reader.member("state", RECORD_STATES, "a recorded record state"),
        sign_off_date=reader.text("signOffDate"),
        entry_commit=reader.nullable_text("entryCommit"),
        signatures=reader.children("signatures", _read_signature),
        sign_off_condition_verdicts=reader.children(
            "signOffConditionVerdicts", _read_sign_off_condition_verdict
        ),
        guard_permissions=reader.children("guardPermissions", _read_guard_permission),
        settled=reader.children("settled", _read_settled_element),
        amendments=reader.optional_children("amendments", _read_amendment),
    )
    reader.done()
    return record


# ---------------------------------------------------------------------------
# Writing
# ---------------------------------------------------------------------------

_OMITTED: Final = object()


def _object(*fields: tuple[str, Any]) -> dict[str, Any]:
    """Build a JSON object from ordered pairs, dropping the pairs marked omitted.

    The pairs are in the order the schema declares the keys, so a document read and written back
    comes out as it went in.
    """
    return {key: value for key, value in fields if value is not _OMITTED}


def _or_omit(value: object | None) -> Any:
    """Return *value*, or the omission marker when the optional key is absent."""
    return _OMITTED if value is None else value


def _or_omit_texts(values: list[str] | None) -> Any:
    """Return a copy of the optional array, or the omission marker when it is absent."""
    return _OMITTED if values is None else list(values)


def _dump_inventory(inventory: InventoryReference) -> dict[str, Any]:
    return _object(
        ("path", inventory.path),
        ("authoritativeFor", list(inventory.authoritative_for)),
    )


def _dump_rationale(rationale: RationaleReference) -> dict[str, Any]:
    return _object(
        ("document", rationale.document),
        ("anchor", rationale.anchor),
    )


def _dump_decision_content_element(element: DecisionContentElement) -> dict[str, Any]:
    return _object(
        ("name", element.name),
        ("source", _or_omit(element.source)),
        ("settlementShape", element.settlement_shape),
        ("requiredAttributes", _or_omit_texts(element.required_attributes)),
        ("keySource", _or_omit(element.key_source)),
        ("excludedSource", _or_omit(element.excluded_source)),
    )


def _dump_sign_off_condition(condition: SignOffCondition) -> dict[str, Any]:
    return _object(
        ("id", condition.id),
        ("requirement", condition.requirement),
        ("text", condition.text),
        ("verification", _or_omit(condition.verification)),
    )


def _dump_gate_part(part: GatePart) -> dict[str, Any]:
    return _object(
        ("id", part.id),
        ("signingOwners", list(part.signing_owners)),
        ("state", part.state),
        ("closureRecord", part.closure_record),
        ("elements", [_dump_decision_content_element(item) for item in part.elements]),
        (
            "signOffConditions",
            [_dump_sign_off_condition(item) for item in part.sign_off_conditions],
        ),
    )


def _dump_release_set_entry(entry: ReleaseSetEntry) -> dict[str, Any]:
    return _object(
        ("screen", entry.screen),
        ("part", _or_omit(entry.part)),
    )


def _dump_excluded_acceptance_criterion(
    criterion: ExcludedAcceptanceCriterion,
) -> dict[str, Any]:
    return _object(
        ("id", criterion.id),
        ("screen", _or_omit(criterion.screen)),
        ("requirement", _or_omit(criterion.requirement)),
    )


def _dump_open_state_reading(reading: OpenStateReading) -> dict[str, Any]:
    return _object(
        ("class", reading.state_class),
        ("reportsGateIdentifier", reading.reports_gate_identifier),
    )


def _dump_gate_dossier(dossier: GateDossier) -> dict[str, Any]:
    return _object(
        ("id", dossier.id),
        ("status", dossier.status),
        ("implementingOwner", dossier.implementing_owner),
        ("rationale", _dump_rationale(dossier.rationale)),
        ("parts", [_dump_gate_part(item) for item in dossier.parts]),
        ("releaseSet", [_dump_release_set_entry(item) for item in dossier.release_set]),
        (
            "excludedAcceptanceCriteria",
            [
                _dump_excluded_acceptance_criterion(item)
                for item in dossier.excluded_acceptance_criteria
            ],
        ),
        ("guards", list(dossier.guards)),
        ("restsOnReviewAlone", _or_omit(dossier.rests_on_review_alone)),
        ("openStateReading", _dump_open_state_reading(dossier.open_state_reading)),
    )


def _dump_observation(observation: Observation) -> dict[str, Any]:
    return _object(
        ("suite", observation.suite),
        ("slot", observation.slot),
    )


def _dump_blocking_entry(entry: BlockingEntry) -> dict[str, Any]:
    return _object(
        ("id", entry.id),
        ("classification", entry.classification),
        ("gate", _or_omit(entry.gate)),
        ("reclassifiedFrom", _or_omit(entry.reclassified_from)),
        ("requirement", _or_omit(entry.requirement)),
        ("inventoryText", _or_omit(entry.inventory_text)),
    )


def _dump_screen_record(record: ScreenRecord) -> dict[str, Any]:
    observation = None if record.observation is None else _dump_observation(record.observation)
    return _object(
        ("id", record.id),
        ("released", record.released),
        ("observation", observation),
        ("blockingEntries", [_dump_blocking_entry(item) for item in record.blocking_entries]),
    )


def _dump_disagreement(disagreement: Disagreement) -> dict[str, Any]:
    return _object(
        ("observedValue", disagreement.observed_value),
        ("against", list(disagreement.against)),
        ("owner", disagreement.owner),
        ("remedies", list(disagreement.remedies)),
        ("handOnRow", disagreement.hand_on_row),
    )


def _dump_guard(guard: Guard) -> dict[str, Any]:
    disagreement = guard.disagreement
    return _object(
        ("symbol", guard.symbol),
        ("declaredIn", guard.declared_in),
        ("recordedValue", guard.recorded_value),
        ("assertedIn", list(guard.asserted_in)),
        ("holdsOpen", list(guard.holds_open)),
        ("holdsNoGateOpen", _or_omit(guard.holds_no_gate_open)),
        ("permittedToChangeBy", list(guard.permitted_to_change_by)),
        (
            "disagreement",
            _OMITTED if disagreement is None else _dump_disagreement(disagreement),
        ),
    )


def _dump_read_only_exception(exception: ReadOnlyException) -> dict[str, Any]:
    return _object(
        ("screen", exception.screen),
        ("file", exception.file),
        ("control", exception.control),
        ("reason", exception.reason),
        ("requirement", _or_omit(exception.requirement)),
    )


def _dump_forbidden_until(item: ForbiddenUntil) -> dict[str, Any]:
    return _object(
        ("id", item.id),
        ("gate", item.gate),
        ("requirement", _or_omit(item.requirement)),
    )


def _dump_read_only_posture(posture: ReadOnlyPosture) -> dict[str, Any]:
    return _object(
        ("gate", posture.gate),
        ("exceptions", [_dump_read_only_exception(item) for item in posture.exceptions]),
        ("forbiddenUntil", [_dump_forbidden_until(item) for item in posture.forbidden_until]),
        ("preApprovedUndeclaredSeams", list(posture.pre_approved_undeclared_seams)),
    )


def _dump_contract_surface_element(element: ContractSurfaceElement) -> dict[str, Any]:
    return _object(
        ("symbol", element.symbol),
        ("kind", element.kind),
        ("gate", element.gate),
        ("reason", _or_omit(element.reason)),
    )


def dump_gate_register(register: GateRegister) -> dict[str, Any]:
    """Write the model back as a JSON-ready object, in the key order the schema declares."""
    return _object(
        ("$schema", _or_omit(register.schema_ref)),
        ("inventory", _dump_inventory(register.inventory)),
        ("gates", [_dump_gate_dossier(item) for item in register.gates]),
        ("screens", [_dump_screen_record(item) for item in register.screens]),
        ("guards", [_dump_guard(item) for item in register.guards]),
        ("readOnly", _dump_read_only_posture(register.read_only)),
        (
            "contractSurface",
            [_dump_contract_surface_element(item) for item in register.contract_surface],
        ),
    )


def _dump_signature(signature: Signature) -> dict[str, Any]:
    return _object(
        ("owner", signature.owner),
        ("date", signature.date),
        ("trailer", _or_omit(signature.trailer)),
        ("signatureVerified", _or_omit(signature.signature_verified)),
    )


def _dump_sign_off_condition_verdict(verdict: SignOffConditionVerdict) -> dict[str, Any]:
    return _object(
        ("id", verdict.id),
        ("satisfied", verdict.satisfied),
        ("reviewer", verdict.reviewer),
        ("date", _or_omit(verdict.date)),
        ("note", _or_omit(verdict.note)),
    )


def _dump_guard_permission(permission: GuardPermission) -> dict[str, Any]:
    return _object(
        ("symbol", permission.symbol),
        ("permits", permission.permits),
        ("reason", _or_omit(permission.reason)),
    )


def _dump_settled_entry(entry: SettledEntry) -> dict[str, Any]:
    return _object(
        ("key", _or_omit(entry.key)),
        ("value", _OMITTED if entry.value is ABSENT else entry.value),
        ("source", _or_omit(entry.source)),
        ("exception", _or_omit(entry.exception)),
    )


def _dump_settled_element(element: SettledElement) -> dict[str, Any]:
    return _object(
        ("element", element.element),
        ("shape", element.shape),
        ("entries", [_dump_settled_entry(item) for item in element.entries]),
    )


def _dump_amendment(amendment: Amendment) -> dict[str, Any]:
    return _object(
        ("key", amendment.key),
        ("signOffDate", amendment.sign_off_date),
        ("signatures", [_dump_signature(item) for item in amendment.signatures]),
        ("entryCommit", amendment.entry_commit),
    )


def dump_closure_record(record: ClosureRecord) -> dict[str, Any]:
    """Write the model back as a JSON-ready object, in the key order the schema declares."""
    amendments = record.amendments
    return _object(
        ("$schema", _or_omit(record.schema_ref)),
        ("gate", record.gate),
        ("part", record.part),
        ("state", record.state),
        ("signOffDate", record.sign_off_date),
        ("entryCommit", record.entry_commit),
        ("signatures", [_dump_signature(item) for item in record.signatures]),
        (
            "signOffConditionVerdicts",
            [_dump_sign_off_condition_verdict(item) for item in record.sign_off_condition_verdicts],
        ),
        ("guardPermissions", [_dump_guard_permission(item) for item in record.guard_permissions]),
        ("settled", [_dump_settled_element(item) for item in record.settled]),
        (
            "amendments",
            _OMITTED if amendments is None else [_dump_amendment(item) for item in amendments],
        ),
    )


# ---------------------------------------------------------------------------
# The screen inventory
# ---------------------------------------------------------------------------

# The seven columns every area table carries. A table with any other header is not an area table,
# and that check is what stops a differently shaped table that happens to cite screen identifiers
# from being read as screens.
INVENTORY_COLUMNS: Final = ("ID", "Screen", "Class", "Seam", "Status", "Owner", "Blocked by")

# An area table announces itself and declares its own row count: "### Shell -- 4 screens". The
# parser anchors on the heading rather than on the shape of a row, because the document carries
# screen identifiers in two further tables -- the misread-blocker table and the totals table -- and
# a table added later with the same columns would otherwise be swept in silently.
_AREA_HEADING: Final = re.compile(r"^###\s+(?P<heading>.*?(?P<declared>\d+)\s+screens?)\s*$")

_CODE_SPAN: Final = re.compile(r"`([^`]+)`")

# A code span of upper-case segments ending in a numeric one: the shape of every decision,
# requirement, hand-off and screen identifier the inventory cites. The parser knows the shape; the
# identifiers stay the document's. The shape excludes the status words the same cells carry and the
# C++ and QML symbols they name, neither of which is a blocking entry.
_ENTRY_ID: Final = re.compile(r"[A-Z][A-Z0-9]*(?:-[A-Z0-9]+)*-[0-9]+")


@dataclass(frozen=True)
class InventoryRow:
    """One screen's row of an area table.

    ``blocking_tokens`` is the identifier set of the "Blocked by" cell, which is what a
    classification reads. ``blocked_by`` keeps the cell as written as well, because an entry that
    carries no identifier is matched on its text.
    """

    screen: str
    status: str
    blocked_by: str
    blocking_tokens: frozenset[str]


@dataclass(frozen=True)
class InventoryArea:
    """One area table, with the row count its own heading declares.

    ``declared_screens`` is recorded, not enforced. Whether a heading agrees with its own rows is
    the ``inventory-totals`` check's finding to report for the inventory's owner, and a parser that
    rejected the disagreement would leave the check with nothing to report it from.
    """

    heading: str
    declared_screens: int
    line: int
    rows: list[InventoryRow]


@dataclass(frozen=True)
class ScreenInventory:
    """The screen rows of the inventory's area tables, by area."""

    areas: list[InventoryArea]

    @property
    def rows(self) -> list[InventoryRow]:
        """Every screen row, in document order."""
        return [row for area in self.areas for row in area.rows]


def _entry_ids(cell: str) -> list[str]:
    """Return the identifier-shaped code spans of *cell*, in the order they appear."""
    return [span for span in _CODE_SPAN.findall(cell) if _ENTRY_ID.fullmatch(span)]


def _read_inventory_row(row: MarkdownRow, path: str) -> InventoryRow:
    if len(row.cells) != len(INVENTORY_COLUMNS):
        raise ScreenInventoryError(
            f"{path}:{row.line}: expected {len(INVENTORY_COLUMNS)} cells, found {len(row.cells)}"
        )
    cells = dict(zip(INVENTORY_COLUMNS, row.cells, strict=True))
    screens = _entry_ids(cells["ID"])
    if len(screens) != 1:
        raise ScreenInventoryError(
            f"{path}:{row.line}: the ID cell must carry one screen identifier, found {screens}"
        )
    # The delivery status is the cell's leading code span. What follows it is prose: the evidence a
    # VERIFIED row rests on, or the half an undelivered row has implemented.
    status = _CODE_SPAN.search(cells["Status"])
    if status is None:
        raise ScreenInventoryError(f"{path}:{row.line}: the Status cell carries no delivery status")
    return InventoryRow(
        screen=screens[0],
        status=status.group(1),
        blocked_by=cells["Blocked by"],
        blocking_tokens=frozenset(_entry_ids(cells["Blocked by"])),
    )


def parse_screen_inventory(text: str, *, path: str = "screen-inventory") -> ScreenInventory:
    """Read the screen inventory's area tables into rows, naming the line at fault on rejection.

    *text* is the document's text, so this stays a model: callers read the file. *path* names the
    document in each message, and every message carries the line the reader could not get past.

    The scope is the area tables alone. An area is a heading declaring a screen count, and its
    table is the first one under it; the table must carry :data:`INVENTORY_COLUMNS`. The
    misread-blocker table and the totals table cite screen identifiers in differently shaped
    tables, and neither is read as screen rows.

    Zero rows is a failure rather than an empty inventory. Every check over the inventory is a
    predicate over these rows, so all of them would pass over none of them.
    """
    lines = text.splitlines()
    headings = [
        (index + 1, match)
        for index, line in enumerate(lines)
        if (match := _AREA_HEADING.match(line))
    ]
    if not headings:
        raise ScreenInventoryError(
            f"{path} carries no area table heading matching {_AREA_HEADING.pattern!r}"
        )

    tables = parse_md_tables(text)
    areas: list[InventoryArea] = []
    for position, (line, heading) in enumerate(headings):
        end = headings[position + 1][0] if position + 1 < len(headings) else len(lines) + 1
        table = next((item for item in tables if line < item.line < end), None)
        if table is None:
            raise ScreenInventoryError(
                f"{path}:{line}: the area {heading['heading']!r} carries no table"
            )
        if table.headers != INVENTORY_COLUMNS:
            raise ScreenInventoryError(
                f"{path}:{table.line}: expected the columns {list(INVENTORY_COLUMNS)}, "
                f"found {list(table.headers)}"
            )
        areas.append(
            InventoryArea(
                heading=heading["heading"],
                declared_screens=int(heading["declared"]),
                line=line,
                rows=[_read_inventory_row(row, path) for row in table.rows],
            )
        )

    inventory = ScreenInventory(areas=areas)
    if not inventory.rows:
        raise ScreenInventoryError(f"{path}: the {len(areas)} area tables carry no screen row")
    return inventory

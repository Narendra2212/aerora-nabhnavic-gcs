"""Hypothesis strategies for the Gate_Register, its Closure_Records and the screen inventory.

Shared rather than per-file, because the properties are spread across two test modules that read
the same model: the derivations and the parser are `test_gate_register.py`'s, and the cross-document,
Closure_Record and guard-registry checks are `test_validate_gate_register.py`'s. One definition of
what a generated register *is* keeps the two halves testing the same thing.

**The strategies deliberately produce invalid registers, and that is the point.** Every property
here is two-sided: a check that accepted everything would pass a one-sided test just as well as a
correct one, so the generators have to reach the states the checks exist to reject. The edge cases
they are built to reach, each because some check's failure branch is otherwise unreachable:

* an empty signing-owner set, which Requirement 15 criterion 18 requires to be *named* rather than
  read as a vacuous pass;
* an empty blocking list, which is a screen Released with no Gate;
* a one-part Gate, so the derivation has no special case at one part;
* a screen with two Gate-classified entries naming the *same* Gate, which must count once;
* a `keyed` element with a duplicated key, and one with a key missing;
* a `mapping` relation total in one direction only;
* an accepted part with an unanswered Sign_Off_Condition;
* an accepted part carrying no permit verdict for a guard mapped to it;
* an inventory row with no blocking entry at all.

Identifiers are fictional on purpose. The real register exists now, and a generator drawing real
screen identifiers or real Release_Sets would be asserting remembered membership rather than the
equation that derives it — which is what `test_validate_gate_register.py`'s named-fact tests do
against the real document, deliberately and separately.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

from common.gate_register import (
    ACCEPTANCE_CRITERION_CLASSIFICATION,
    CHECK_GUARD,
    GATE_CLASSIFICATION,
    GATE_IDS,
    LITERAL_GUARD,
    OWNER_LABELS,
    PREDICATE_GUARD,
    SETTLEMENT_SHAPES,
    Amendment,
    BlockingEntry,
    ClosureRecord,
    DecisionContentElement,
    Disagreement,
    GateDossier,
    GatePart,
    GateRegister,
    Guard,
    GuardPermission,
    InventoryReference,
    LinkProfile,
    MavlinkIdentifierPosture,
    Observation,
    OpenStateReading,
    RationaleReference,
    ReadOnlyPosture,
    ReleaseSetEntry,
    ScreenRecord,
    SettledElement,
    SettledEntry,
    Signature,
    SignOffCondition,
    SignOffConditionVerdict,
    parse_screen_inventory,
)
from common.markdown import md_table
from hypothesis import strategies as st

if TYPE_CHECKING:
    from collections.abc import Sequence

    from common.gate_register import ScreenInventory

# The inventory's own status vocabulary, as the real document uses it. Two are delivered; the rest
# are stages of undelivered work. Spelled here rather than imported as one set, because
# `DELIVERED_STATUSES` is the model's and the undelivered spellings are the document's.
DELIVERED = ("VERIFIED", "SUPERSEDED")
UNDELIVERED = ("DESIGNED", "SPECIFIED-NOT-DESIGNED", "IMPLEMENTED-UNCOMPILED", "BLOCKED")
STATUSES = DELIVERED + UNDELIVERED

# The documents a generated register references. Fictional, and never read: no derivation opens the
# inventory path, and a generated register is compared against a generated inventory.
FIXTURE_INVENTORY = "a-fixture-inventory.md"

# How many examples every property test runs. Stated once so a property cannot be quietly weakened
# by lowering its own count: a test that needs fewer says so at its own decorator and says why.
MAX_EXAMPLES = 100

_SCREEN_AREAS = ("AAA", "BBB", "CCC", "DDD")


@st.composite
def screen_ids(draw: st.DrawFn, *, areas: Sequence[str] = _SCREEN_AREAS) -> str:
    """A screen identifier of the inventory's shape, from a small fictional area set.

    Small on purpose: the interesting registers are the ones where two screens collide on a Gate or
    a Gate's Release_Set overlaps another's, and a wide identifier space makes those vanishingly
    rare.
    """
    area = draw(st.sampled_from(list(areas)))
    number = draw(st.integers(min_value=1, max_value=9))
    return f"SCR-{area}-{number:02d}"


def part_slugs() -> st.SearchStrategy[str]:
    """A filename-safe part slug, which is what the schema constrains a part id to."""
    return st.sampled_from(["whole", "first-part", "second-part", "third-part", "fourth-part"])


def owner_sets() -> st.SearchStrategy[list[str]]:
    """A signing-owner set, **including the empty one**.

    Empty is reachable because Requirement 15 criterion 18 requires a part recording no owner to be
    named as such, and a check cannot be shown to name it if the register cannot express it.
    """
    return st.lists(st.sampled_from(sorted(OWNER_LABELS)), min_size=0, max_size=3, unique=True)


@st.composite
def decision_content_elements(
    draw: st.DrawFn, *, name: str | None = None
) -> DecisionContentElement:
    """One Decision_Content element, named and never valued.

    The shape is drawn from all three, so a part can carry a `keyed` element beside a `mapping` one
    and the settlement checks are exercised together rather than one shape per register.
    """
    shape = draw(st.sampled_from(sorted(SETTLEMENT_SHAPES)))
    attributes = draw(st.lists(st.sampled_from(["first", "second"]), max_size=2, unique=True))
    return DecisionContentElement(
        name=name or draw(st.sampled_from(["an element", "another element", "a third element"])),
        source=draw(st.sampled_from([None, "Q-000", "a fixture decision"])),
        settlement_shape=shape,
        required_attributes=attributes,
        key_source=draw(st.sampled_from([None, "a fixture key source"]))
        if shape == "keyed"
        else None,
        excluded_source=draw(st.sampled_from([None, "a fixture excluded source"])),
    )


@st.composite
def sign_off_conditions(draw: st.DrawFn, *, identifier: str | None = None) -> SignOffCondition:
    """One Sign_Off_Condition.

    `requirement` is always drawn, never omitted: the schema requires it and so does the model's
    reader, so a condition without one is a document that cannot be loaded rather than a register
    state worth generating. The round-trip property found this by failing on it.
    """
    return SignOffCondition(
        id=identifier or draw(st.sampled_from(["SOC-01", "SOC-02", "SOC-03"])),
        requirement=draw(st.sampled_from(["1.1", "15.18"])),
        text="a fixture condition",
        verification=draw(st.sampled_from([None, "review"])),
    )


@st.composite
def parts(draw: st.DrawFn, *, slug: str | None = None, state: str | None = None) -> GatePart:
    """One Gate part, in any of the three states.

    `submitted` is drawn as often as the other two, which matters: a submitted part is unaccepted
    for every derivation and carries no Closure_Record, and reading it as accepted is the mistake
    the Gate-open equation has to not make.
    """
    drawn_state = state or draw(st.sampled_from(sorted(["open", "submitted", "accepted"])))
    part_id = slug or draw(part_slugs())
    return GatePart(
        id=part_id,
        signing_owners=draw(owner_sets()),
        state=drawn_state,
        closure_record=f"closures/{part_id}.json" if drawn_state == "accepted" else None,
        elements=draw(st.lists(decision_content_elements(), max_size=2)),
        sign_off_conditions=draw(
            st.lists(sign_off_conditions(), max_size=2, unique_by=lambda c: c.id)
        ),
    )


@st.composite
def dossiers(
    draw: st.DrawFn,
    gate: str,
    *,
    release_set: Sequence[str] = (),
    status: str | None = None,
    guards: Sequence[str] = (),
) -> GateDossier:
    """One Gate_Dossier with one to four parts.

    One to four because that is the real spread — `DG-MIS` has one part and `DG-PAY` has four — and
    because a derivation that needed more than one would be a special case at one rather than a
    quantifier over parts.

    *status* is drawn independently of the parts unless given, so a register whose recorded status
    disagrees with its own part acceptance is reachable. That disagreement is
    `gate-status-derivation`'s finding and nothing else would notice it.
    """
    count = draw(st.integers(min_value=1, max_value=4))
    slugs = ["whole", "first-part", "second-part", "third-part"][:count]
    drawn = [draw(parts(slug=slug)) for slug in slugs]
    recorded = status or draw(st.sampled_from(["OPEN", "CLOSED"]))
    return GateDossier(
        id=gate,
        status=recorded,
        implementing_owner=draw(st.sampled_from(sorted(OWNER_LABELS))),
        rationale=RationaleReference(document=FIXTURE_INVENTORY, anchor="#an-anchor"),
        parts=drawn,
        release_set=[ReleaseSetEntry(screen=screen, part=None) for screen in release_set],
        excluded_acceptance_criteria=[],
        guards=list(guards),
        rests_on_review_alone=not guards,
        open_state_reading=OpenStateReading(
            state_class=draw(st.sampled_from(["Unknown", "a recorded class"])),
            reports_gate_identifier=draw(st.booleans()),
        ),
    )


@st.composite
def blocking_entries(draw: st.DrawFn, *, gate: str | None = None) -> BlockingEntry:
    """One blocking entry, classified as exactly one of the two.

    When *gate* is given the entry is Gate-classified and names it; otherwise the classification is
    drawn. A drawn Gate-classified entry still names a Gate, because an entry classified as a Gate
    and naming none is the data half's finding and is built explicitly where a test wants it.
    """
    if gate is not None:
        classification = GATE_CLASSIFICATION
        named: str | None = gate
    else:
        classification = draw(
            st.sampled_from([GATE_CLASSIFICATION, ACCEPTANCE_CRITERION_CLASSIFICATION])
        )
        named = (
            draw(st.sampled_from(list(GATE_IDS))) if classification == GATE_CLASSIFICATION else None
        )
    return BlockingEntry(
        id=draw(st.sampled_from(["P7-AAA-001", "P7-BBB-002", "Q-001", "Q-002"])),
        classification=classification,
        gate=named,
        reclassified_from=draw(
            st.sampled_from([None, GATE_CLASSIFICATION, ACCEPTANCE_CRITERION_CLASSIFICATION])
        ),
        requirement=None,
        inventory_text=None,
    )


@st.composite
def screen_records(draw: st.DrawFn, *, screen: str | None = None) -> ScreenRecord:
    """One screen record, whose blocking list may be empty and may repeat a Gate.

    Both of those are deliberate. An empty list is a screen Released with no Gate, and two entries
    naming one Gate must count once: the Two_Gate_Screen rule is about *decisions* and not rows, so
    a generator that never repeated a Gate could not show the difference.
    """
    entries = draw(st.lists(blocking_entries(), max_size=3))
    return ScreenRecord(
        id=screen or draw(screen_ids()),
        released=draw(st.booleans()),
        observation=draw(
            st.sampled_from([None, Observation(suite="AFixtureTest", slot="_aFixtureSlot")])
        ),
        blocking_entries=entries,
    )


@st.composite
def guards(draw: st.DrawFn, *, symbol: str | None = None, holds_open: Sequence[str] = ()) -> Guard:
    """One registry entry, in any of the three kinds.

    Each kind carries only the fields the schema admits on it, so a generated registry is one a
    document could hold: a literal has a value and no expression, a predicate has both, and a check
    has a rule name and neither.
    """
    kind = draw(st.sampled_from(sorted([LITERAL_GUARD, PREDICATE_GUARD, CHECK_GUARD])))
    sites = (
        []
        if kind == CHECK_GUARD
        else draw(st.lists(st.just("aerora/test/AFixtureTest.cc"), max_size=1))
    )
    disagreement = (
        draw(
            st.sampled_from(
                [
                    None,
                    Disagreement(
                        observed_value="true",
                        against=["1.1"],
                        owner="Tab 1",
                        remedies=["a fixture remedy"],
                        hand_on_row="a fixture row",
                    ),
                ]
            )
        )
        if kind == LITERAL_GUARD
        else None
    )
    return Guard(
        symbol=symbol or draw(st.sampled_from(["kAFixtureGuard", "kAnotherFixtureGuard"])),
        kind=kind,
        declared_in="aerora/src/AFixtureGuard.h",
        recorded_value=None if kind == CHECK_GUARD else "false",
        declared_expression="return aFixture();" if kind == PREDICATE_GUARD else None,
        held_by="a-fixture-rule" if kind == CHECK_GUARD else None,
        asserted_in=sites,
        asserted_nowhere=None if sites or kind == CHECK_GUARD else True,
        remedy=None if sites or kind == CHECK_GUARD else "a fixture remedy",
        holds_open=list(holds_open),
        holds_no_gate_open=not holds_open,
        permitted_to_change_by=[],
        disagreement=disagreement,
    )


# Fictional link classes and parameter names. Naming a real parameter would settle part of what
# `Q-015` holds, and this module's identifiers are fictional for that reason throughout.
_LINK_CLASSES = ("a-fixture-link-class", "another-fixture-link-class")
_LINK_PARAMETERS = ("aFixtureParameter", "anotherFixtureParameter")


@st.composite
def link_profiles(draw: st.DrawFn) -> list[LinkProfile] | None:
    """A recorded Link_Profile set, or no profiles at all.

    `None` is drawn as often as a list, because the register carries no `linkProfiles` key today and
    the round trip has to hold over the absent key as well as the present one.

    Parameter sets are drawn both empty and populated. Empty is the state every profile is in until
    `Q-015` closes, and it is the state a reader is most likely to mishandle by treating an empty
    mapping as a missing one.
    """
    if draw(st.booleans()):
        return None
    classes = draw(st.lists(st.sampled_from(_LINK_CLASSES), min_size=1, max_size=2, unique=True))
    return [
        LinkProfile(
            link_class=name,
            parameters=draw(
                st.dictionaries(
                    st.sampled_from(_LINK_PARAMETERS),
                    st.integers(min_value=0, max_value=9),
                    max_size=2,
                )
            ),
            held_by="Q-015",
            revision=draw(st.sampled_from(["a-fixture-revision", "another-fixture-revision"])),
        )
        for name in classes
    ]


@st.composite
def registers(
    draw: st.DrawFn,
    *,
    gate_count: int = len(GATE_IDS),
    screens: Sequence[ScreenRecord] | None = None,
) -> GateRegister:
    """A register of *gate_count* Gates, each with one to four parts.

    Ten by default, which is the schema's rule, so a generated register is shaped like a real one.
    A property about the equations can take fewer: no derivation reads the count, and showing that
    is part of what these properties are for.
    """
    gate_ids = list(GATE_IDS[:gate_count])
    drawn_screens = (
        list(screens)
        if screens is not None
        else draw(st.lists(screen_records(), max_size=4, unique_by=lambda record: record.id))
    )
    return GateRegister(
        schema_ref=None,
        inventory=InventoryReference(path=FIXTURE_INVENTORY, authoritative_for=["the screen set"]),
        gates=[draw(dossiers(gate)) for gate in gate_ids],
        screens=drawn_screens,
        guards=draw(st.lists(guards(), max_size=2, unique_by=lambda guard: guard.symbol)),
        read_only=ReadOnlyPosture(
            gate="DG-ROL",
            exceptions=[],
            actionable_control_sites=[],
            forbidden_until=[],
            pre_approved_undeclared_seams=[],
        ),
        mavlink_identifiers=MavlinkIdentifierPosture(gate="DG-MIS", allow_list=[]),
        contract_surface=[],
        link_profiles=draw(link_profiles()),
    )


# ---------------------------------------------------------------------------
# The screen inventory
# ---------------------------------------------------------------------------


def inventory_row(screen: str, status: str, blocked_by: str = "-") -> list[str]:
    """One inventory row, in the column order the real document declares."""
    return [f"`{screen}`", "A screen", "O", "a seam", f"`{status}`", "7", blocked_by]


def inventory_document(rows: Sequence[list[str]]) -> str:
    """The rows as a document the parser reads: one area heading with its own row count."""
    from common.gate_register import INVENTORY_COLUMNS

    return "\n".join(
        [
            "# A fixture inventory",
            "",
            f"### An area -- {len(rows)} screens",
            "",
            md_table(INVENTORY_COLUMNS, list(rows)),
            "",
        ]
    )


# The screen identifiers an inventory draws its rows from. A finite pool rather than a generator,
# because the rows have to be unique and a retry loop over a composite burns entropy until
# Hypothesis gives up on generating anything at all — which is how the first version of
# `inventories()` failed its own health check.
SCREEN_POOL = tuple(f"SCR-{area}-{number:02d}" for area in _SCREEN_AREAS for number in range(1, 10))


@st.composite
def inventories(draw: st.DrawFn, *, min_rows: int = 1, max_rows: int = 6) -> ScreenInventory:
    """An inventory whose statuses come from the document's own vocabulary.

    At least one row, because zero rows is a parse *failure* rather than an empty result, and the
    parser asserts that separately: a strategy that produced it would be generating a document the
    parser refuses rather than an inventory to derive over.

    Rows carry no blocking entry by default, which is itself one of the edge cases: an undelivered
    screen with an empty "Blocked by" cell is a screen the partition has to place.
    """
    chosen = draw(
        st.lists(
            st.sampled_from(SCREEN_POOL),
            min_size=min_rows,
            max_size=max_rows,
            unique=True,
        )
    )
    rows = [
        inventory_row(screen, draw(st.sampled_from(list(STATUSES)))) for screen in sorted(chosen)
    ]
    return parse_screen_inventory(inventory_document(rows))


# ---------------------------------------------------------------------------
# Closure_Records
# ---------------------------------------------------------------------------


@st.composite
def settled_entries(draw: st.DrawFn, *, key: str | None = None) -> SettledEntry:
    return SettledEntry(
        key=key if key is not None else draw(st.sampled_from([None, "first-key", "second-key"])),
        value=dict.fromkeys(
            draw(st.lists(st.sampled_from(["first", "second"]), max_size=2, unique=True)),
            "a fixture value",
        ),
        source=draw(st.sampled_from([None, "a fixture source", "a fixture excluded source"])),
        # A boolean, not a reason: the entry records *that* a key is excepted and the Gate_Dossier
        # carries why. A `keyed` element settles a key either with an entry or with an exception,
        # so this is what makes the second branch of that rule reachable.
        exception=draw(st.sampled_from([None, True, False])),
    )


@st.composite
def settled_elements(draw: st.DrawFn, element: DecisionContentElement) -> SettledElement:
    """The settled form of *element*, which may or may not actually settle it.

    Entry counts are drawn from zero to three so each shape's failure branch is reachable: a
    `single` settled by none or by two, a `keyed` with a key missing or duplicated, and a `mapping`
    total in one direction only.
    """
    count = draw(st.integers(min_value=0, max_value=3))
    keys = ["first-key", "second-key", "first-key"][:count]
    entries = [
        draw(settled_entries(key=keys[index] if element.settlement_shape != "single" else None))
        for index in range(count)
    ]
    return SettledElement(element=element.name, shape=element.settlement_shape, entries=entries)


@st.composite
def closure_records(draw: st.DrawFn, gate: str, part: GatePart) -> ClosureRecord:
    """A Closure_Record for *part*, which may be incomplete in any of the ways the checks reject.

    The signature set, the verdict set and the guard-permission set are each drawn independently of
    what the dossier records, so set equality is tested in both directions rather than only where
    the record happens to agree.
    """
    state = draw(st.sampled_from(sorted(["submitted", "accepted"])))
    owners = draw(st.lists(st.sampled_from(sorted(OWNER_LABELS)), max_size=3, unique=True))
    answered = draw(
        st.lists(st.sampled_from(["SOC-01", "SOC-02", "SOC-03"]), max_size=3, unique=True)
    )
    return ClosureRecord(
        schema_ref=None,
        gate=gate,
        part=part.id,
        state=state,
        # Always present and never blank. `closure-signatures` reads whether a *date* is recorded,
        # and the schema requires one, so an absent date is a document that cannot be loaded rather
        # than a record state worth generating — the same lesson the Sign_Off_Condition's
        # requirement field taught the round-trip property.
        sign_off_date="2026-01-01",
        entry_commit=("0" * 40) if state == "accepted" else None,
        signatures=[
            Signature(owner=owner, date="2026-01-01", trailer=owner, signature_verified=None)
            for owner in owners
        ],
        sign_off_condition_verdicts=[
            SignOffConditionVerdict(
                id=identifier,
                satisfied=draw(st.booleans()),
                reviewer=draw(st.sampled_from(["a reviewer", ""])),
                date="2026-01-01",
                note=None,
            )
            for identifier in answered
        ],
        guard_permissions=[
            GuardPermission(symbol=symbol, permits=draw(st.booleans()), reason="a fixture reason")
            for symbol in draw(
                st.lists(
                    st.sampled_from(["kAFixtureGuard", "kAnotherFixtureGuard"]),
                    max_size=2,
                    unique=True,
                )
            )
        ],
        settled=[draw(settled_elements(element)) for element in part.elements],
        amendments=draw(
            st.lists(
                st.builds(
                    Amendment,
                    key=st.sampled_from(["first-key", "second-key"]),
                    sign_off_date=st.just("2026-02-01"),
                    signatures=st.just([]),
                    entry_commit=st.just(None),
                ),
                max_size=1,
            )
        ),
    )

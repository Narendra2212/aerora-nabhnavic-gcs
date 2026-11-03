#!/usr/bin/env python3
"""Check the Aerora source tree against what the Gate_Register records about it.

The source half of the Gate_Review. The data half -- ``tools/validate_gate_register.py`` -- reads
the register against the documents it names; this reads it against the files it names, so that a
guard holding an open Gate visible cannot change value, and a Contract_Value no Gate has settled
cannot enter the repository, without the rejection saying which decision is being protected.

**It inspects text, never a build.** Every rule here reads a file as characters: the authoring host
has no compiler and no Qt, so nothing here resolves a symbol, instantiates a type or runs a test.
That bound is deliberate rather than regrettable. Source inspection is the gate, because it fires
before any build and rejects the declaration; the C++ suites that assert the same guards at the
composition root are the thing that is actually true.

**It never writes.** The data half promises the same, for the same reason: the only thing an
auto-fix could write here is a guard value, an allow-list entry or a surface element that no owner
signed. ``fix`` is accepted because the dispatcher's signature carries it, and discarded.

**The register is the authority.** Every rule compares a file against something an owner recorded
there, so the register is established before any rule reads a file -- through the data half's
loader, which owns the register path, the schema path and the error contract, so the two halves
cannot disagree about what a readable register is. A premise that loader cannot establish is
reported here as itself and names all ten Gates, because a register that cannot be read leaves
every guard unverified: an unresolved premise is a failure, never a pass.

**The two guard rules read the files the register names, not the files the run selected.**
``guard-value`` reads each recorded guard's declaration in its recorded ``declaredIn`` file and
compares the literal it carries against the recorded one. ``guard-assertion`` reads each recorded
assertion site and rejects a guard no site under ``aerora/test/**`` still names, because
Requirement 4 criterion 9 makes removing the assertion as serious as removing the guard. Neither
reads the selected files, so a commit touching one guard is reviewed against all of them. Neither
reads the freedom derivation either: criterion 6 lets a *recorded* value move once every part
permitted to change a guard has permitted it, and both before and after that these rules compare
source against the recorded value. The recorded-value side of criterion 6 is ``guard-registry``'s,
in the data half.

**``mavlink-identifier`` is the one rule that walks a tree, and the one that reads comments.** It
has to find a token in a file nobody named, so the overlay tree is its input rather than a path the
register records -- but the selection still decides nothing, and the allow-list it reads is recorded
by site and reason, never by identifier. It reads the allow-listed files and takes whatever
identifiers they hold as the current baseline, so the permitted set lives in the tree and the
register carries none of its own, and it rejects a token in any file the allow-list does not carry.
Comments are left in, because a MAVLink identifier written into a comment is the identifier.

**``adr-016-pair`` is the one rule that reads core, and it reads the permission rather than the
value.** Requirement 7 criterion 17 is about two edits -- a friend declaration in
``src/Vehicle/Vehicle.h`` and a compile definition in ``aerora/CMakeLists.txt`` -- which no guard's
declaration carries and ``guard-value`` never opens, so the two rules divide by subject: the value
the guard holds is ``guard-value``'s and the two edits are this rule's. It reads the change
permission on the same registry entry, through the freedom derivation, because Requirement 7
criterion 18 records exactly that as the condition under which the core edit's authorisation lets
the guard move. Both files are read as characters and neither is written.

**``contract-surface`` answers a decidable question in place of an undecidable one, and says so.**
"Reject a change that introduces a Contract_Value" cannot be checked: no rule can look at a new
enumerator and know whether it is a value some Gate settles. So it compares the enumerators,
declared constants and registry string-literal keys under ``aerora/src/**`` against the inventory
the register records, in both directions, and a reviewer classifies each element once. It
establishes that the surface did not change -- not that no value was invented -- and it cannot
classify. It is also the one rule here that is not conditional on a single Gate: it reads the Gate
*each element maps to*, so a closing Gate frees its own elements and nothing else.

**``actionable-control`` compares two sets rather than asserting an absence.** Requirement 5
criterion 1 forbids a control whose activation *reaches* a forbidden path, which no text rule can
decide, so the rule decides a control by its type and the register carries the judgement: the
actionable controls the Aerora QML documents declare must be exactly the controls the read-only
posture records. The single exception to read-only presentation is one of them and the other
recorded sites are not exceptions -- each carries an owner's statement that the control reaches none
of the paths criterion 1 names, and the handler that statement was made about. Set equality is what
makes "exactly one in the exception's document", "none in any other" and "no record for a control
that has gone" one rule instead of three.

**The four absence rules assert that something is not there, and each stands down when its Gate
closes, and so does the fifth.** ``populated-file-override``, ``action-seam``,
``freshness-threshold`` and
``payload-command-sink`` read criteria written ``WHILE <gate> is OPEN``, so each reads the *derived*
openness of the Gate it waits on -- ``premises.derived.gate_open``, which the data half evaluates
once and ``gate-status-derivation`` owns the disagreement between recorded and derived. A rule whose
Gate is closed reports nothing, and the run's summary states how many stood down, so a rule that
stopped looking is a number rather than a silence. Each of the four distinguishes its subject from a
legitimate mention of the same name -- the entry list rather than the file, a call rather than a
declaration, a derivation rather than a pointer member -- because a rule that fired on this tree
would be removed rather than obeyed. They add no path of their own: Requirement 5 criterion 2 counts
an outbound path as the thing to reject, and four rules that reject one introduce none.

**Reported is not passed.** A guard carrying a ``disagreement`` the source actually holds is the one
concession the design makes to being gating: the finding is printed on every run and the run still
exits zero, because a hook that blocks every commit until an unrelated authorisation lands gets
removed rather than obeyed. :attr:`Finding.blocking` carries that distinction per finding,
``AnalysisResult.issues`` counts every finding and ``AnalysisResult.passed`` reports only whether
one of them blocks -- so a matched disagreement neither makes the run look clean nor fails the hook.
A third value, a guard whose declaration cannot be found, and a ``disagreement`` naming no handed-on
row all block: the record excuses one known state, not the guard.

Usage:
    python3 gate_guard_check.py [files or directories...]
    python3 gate_guard_check.py aerora/      # a directory is expanded to the C++ files under it
    python3 gate_guard_check.py              # read the paths from stdin (for pre-commit)

Exit codes:
    0 - No blocking finding, which includes a run reporting a matched disagreement
    1 - A blocking finding
"""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import TYPE_CHECKING, ClassVar, Final

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from _bootstrap import ensure_tools_dir

ensure_tools_dir(__file__)

from common.analyzer import AnalysisResult, AnalyzerBase
from common.file_traversal import DEFAULT_SKIP_DIRS, find_cpp_files, should_skip_path
from common.gate_register import (
    GATE_IDS,
    NO_GATE,
    GateRegisterError,
    guard_freedom,
    part_reference,
)
from validate_gate_register import (
    FACT_GROUP_REGISTRAR_GUARD,
    GUARD_REGISTRY,
    MAVLINK_IDENTIFIER_GUARD,
    OPEN_STATUS,
    OVERLAY_TREE,
    PRE_APPROVED_SEAMS,
    READ_ONLY_GATE,
    REGISTER_PATH,
    TEST_TREE,
    THRESHOLD_COUNT_GUARD,
    load_premises,
)

if TYPE_CHECKING:
    from collections.abc import Mapping, Sequence

    from common.gate_register import (
        Disagreement,
        GateRegister,
        Guard,
        GuardFreedom,
        MavlinkIdentifierPosture,
    )
    from validate_gate_register import Finding as PremiseFinding
    from validate_gate_register import Premises

# The repository this run reviews, resolved from this file rather than from the working directory,
# so a hook invoking the script from anywhere reads the same register. `run` takes its root from
# the dispatcher instead, which is what lets a test review a tree it laid out itself.
ROOT: Final = Path(__file__).resolve().parents[2]

# The name a finding and the dispatcher's status line carry, which is the hook id as well.
NAME: Final = "gate-guards"

# The rules this module carries, named as the design's table names them. `mavlink-identifier` is
# spelled by the registry entry rather than again: Requirement 4 criterion 5's ninth guard *is*
# this check, so the guard's symbol and the check's name are one string and a second spelling of it
# would be a second thing to keep in step.
GUARD_VALUE: Final = "guard-value"
GUARD_ASSERTION: Final = "guard-assertion"
ADR_016_PAIR: Final = "adr-016-pair"
MAVLINK_IDENTIFIER: Final = MAVLINK_IDENTIFIER_GUARD
POPULATED_FILE_OVERRIDE: Final = "populated-file-override"
ACTION_SEAM: Final = "action-seam"
FRESHNESS_THRESHOLD: Final = "freshness-threshold"
PAYLOAD_COMMAND_SINK: Final = "payload-command-sink"
ACTIONABLE_CONTROL: Final = "actionable-control"
CONTRACT_SURFACE: Final = "contract-surface"

# How a finding names the Gate set where no Gate_Dossier records the element it rejects, which is
# the second half of Requirement 4 criterion 2: name the element as unassigned Decision_Content.
# Spelled out rather than left blank, so that case reads as a sentence instead of a silence.
UNASSIGNED: Final = "unassigned Decision_Content"

# How a finding that does not block says so, at the front of the line rather than at the end of the
# message, so a reader scanning a run's output can tell which line stopped the commit.
REPORTED: Final = "reported, not blocking"


@dataclass(frozen=True)
class Finding:
    """One rejection, naming the check that made it and the Gate it is protecting.

    ``check`` belongs to the finding for the reason it does in the data half: the run prints
    findings in one stream, and the reader has to be able to tell which rule rejected which file.

    ``gates`` carries no default, so a rule cannot report a rejection without deciding which
    decision it is protecting. Requirement 4 criterion 7 requires the guard *and* the Gate to be
    named, and a check that rejects without naming the Gate leaves the reviewer to work out which
    decision is being held open. The empty tuple is the one case Requirement 4 criterion 2 admits
    -- an element no Gate_Dossier records -- and it renders as :data:`UNASSIGNED`.

    The Gates are rendered in the order they are given, which for a guard is the order the register
    records them in, so one run's findings diff against another's.

    ``blocking`` defaults to true, so a rule has to say that a finding does not block rather than
    forget to say that it does. False is the design's one concession: a guard standing in the state
    its recorded ``disagreement`` pins, which is reported on every run and does not fail the hook.
    """

    check: str
    gates: tuple[str, ...]
    message: str
    blocking: bool = True

    def __str__(self) -> str:
        marked = self.check if self.blocking else f"{self.check} ({REPORTED})"
        return f"{marked}: {', '.join(self.gates) or UNASSIGNED}: {self.message}"


def premise_findings(findings: Sequence[PremiseFinding]) -> list[Finding]:
    """The data half's premise failures, reported here as themselves.

    Each keeps the check name the data half rejected it under, because the repair is to the
    register or to a document the register names and never to a source file, and each names all ten
    Gates, because a register that cannot be read leaves every guard unverified.
    """
    return [Finding(finding.check, GATE_IDS, finding.message) for finding in findings]


# ---------------------------------------------------------------------------
# Reading the files the register names
# ---------------------------------------------------------------------------

_COMMENT: Final = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)

# A string literal, with its escapes, so a path inside one is not read as code. Only the rules that
# read a brace-delimited span mask these: a declaration and the literal beside it carry none, and
# `mavlink-identifier` reads a token wherever it is written.
_STRING: Final = re.compile(r'"(?:[^"\\\n]|\\.)*"')
_NOT_NEWLINE: Final = re.compile(r"[^\n]")

# The literal a definition's body returns, read from the first `return` after its opening brace.
# A body whose value depends on a preprocessor condition therefore reads as its first branch, which
# is what `AeroraVehicleFactGroupRegistrar::isRegistrationAvailable()` is: a text comparison cannot
# evaluate `#ifdef`, and the rule that reads the define is `adr-016-pair`.
_RETURN: Final = re.compile(r"\breturn\s+(?P<value>[^;]+);")


def _blanked(match: re.Match[str]) -> str:
    """*match* as the same span of spaces, with its newlines kept."""
    return _NOT_NEWLINE.sub(" ", match.group())


def _code(text: str) -> str:
    """*text* with its comments blanked, so a comment naming a guard does not declare one.

    The overlay documents every one of these guards in a doc comment beside it, and
    `AeroraSecurityStatus.h` carries ``kRemainingEnergyEstimationImplemented = false`` in prose, so
    a rule that read comments would read a sentence *about* a guard as the guard itself. The same
    holds for every subject the four absence rules read: the override allow-list's own comment block
    names `Tier::PopulatedFile` and both pre-approved seams in prose, and
    `AeroraPayloadModel.h` names the payload command sink in three doc comments.

    Blanked rather than deleted, so a line number or an offset read off the result is the line
    number in the file. A finding that named the wrong line would send a reviewer to the wrong
    place, and the rules that walk a tree have no other way to say where they found a subject.

    The removal is textual and knows nothing about string literals, which is sound for what it reads
    here -- a declaration and the literal beside it, and spans the brace-reading rules mask
    separately.
    """
    return _COMMENT.sub(_blanked, text)


def _without_strings(text: str) -> str:
    """*text* with its string literals blanked, on the same terms :func:`_code` blanks comments.

    What this keeps out is a resource path read as code: the override allow-list carries each entry
    as a quoted path beside its tier, so a path that happened to carry a tier's name would be read
    as the tier. Length and newlines are preserved for the reason :func:`_code` preserves them.
    """
    return _STRING.sub(_blanked, text)


def _line_of(text: str, offset: int) -> int:
    """The one-based line of *text* that *offset* falls on."""
    return text.count("\n", 0, offset) + 1


def _read(root: Path, relative: str) -> tuple[str | None, str]:
    """Read a repository file as UTF-8, naming the reason on failure.

    The reasons are the data half's, worded the same way, because a finding about a file the
    register names reads the same whichever half of the Gate_Review reported it.
    """
    try:
        return (root / relative).read_text(encoding="utf-8"), ""
    except FileNotFoundError:
        return None, "does not exist"
    except OSError as error:
        return None, f"could not be read: {error.strerror}"
    except UnicodeDecodeError as error:
        return None, f"is not valid UTF-8: {error.reason}"


def _declared_name(symbol: str) -> str:
    """The name a file declares *symbol* under: its last ``::`` component, without a trailing call.

    Requirement 4 criterion 5 names its nine guards as an owner refers to them -- two qualified by
    their class, one of them a call, and six bare -- while a declaration carries the member name
    alone and an assertion site carries whichever form the suite wrote. Matching on the last
    component reads both forms and still rejects a rename, because a renamed declaration carries a
    different last component.
    """
    return symbol.rpartition("::")[2].removesuffix("()")


@dataclass(frozen=True)
class _Declaration:
    """What a guard's recorded ``declaredIn`` file says about its declaration.

    Four states rather than two, because each is a different repair: the file could not be read,
    the file names no such symbol, the file names it and carries no literal this check can read,
    and the file carries a literal. "Not found" is not "unchanged" in any of the first three.
    """

    unreadable: str
    named: bool
    value: str | None


def _declaration(root: Path, guard: Guard) -> _Declaration:
    """Read *guard*'s declaration out of the file the register records it in.

    Three declaration shapes, which are the three the overlay uses: an initialiser
    (``static constexpr bool kFoo = false;``), a brace initialiser, and a definition whose body
    returns a literal (``bool Class::isFoo() { return false; }``). A ``==`` comparison is not an
    initialiser, so an assertion written beside a declaration is not read as one.
    """
    text, unreadable = _read(root, guard.declared_in)
    if text is None:
        return _Declaration(unreadable=unreadable, named=False, value=None)

    code = _code(text)
    name = _declared_name(guard.symbol)
    if re.search(rf"\b{re.escape(name)}\b", code) is None:
        return _Declaration(unreadable="", named=False, value=None)

    initialiser = re.search(
        rf"\b{re.escape(name)}\b\s*"
        rf"(?:=(?!=)\s*(?P<assigned>[^;,{{}}\n]+)|\{{\s*(?P<braced>[^}};\n]*?)\s*\}})",
        code,
    )
    if initialiser is not None:
        assigned = initialiser["assigned"]
        literal = (assigned if assigned is not None else initialiser["braced"]).strip()
        if literal:
            return _Declaration(unreadable="", named=True, value=literal)

    definition = re.search(rf"\b{re.escape(name)}\s*\([^)]*\)[^;{{}}]*\{{", code)
    if definition is None:
        return _Declaration(unreadable="", named=True, value=None)

    returned = _RETURN.search(code, definition.end())
    value = None if returned is None else returned["value"].strip()
    return _Declaration(unreadable="", named=True, value=value or None)


def _registry(premises: Premises) -> list[Guard]:
    """The registry entries these two rules read, which is every entry but one.

    Requirement 4 criterion 5's ninth guard is the static MAVLink-identifier check rather than a
    symbol holding a literal: no file declares it and no site asserts a value for it, so what holds
    it is `mavlink-identifier`'s own rule over the allow-list the register records by site. The data
    half reads its recorded value for nothing for the same reason, and the registry names it by that
    check so the row reads like the other eight without pretending a declaring file holds a value.
    """
    return [guard for guard in premises.register.guards if guard.symbol != MAVLINK_IDENTIFIER_GUARD]


def _excusing(guard: Guard) -> Disagreement | None:
    """The recorded disagreement this check will excuse a value with, where *guard* carries one.

    A disagreement naming no handed-on row excuses nothing, because what keeps the record from
    becoming the place inconvenient failures go is that it points at the row tracking it.
    """
    disagreement = guard.disagreement
    if disagreement is None or not disagreement.hand_on_row.strip():
        return None
    return disagreement


def _matched(guard: Guard, declaration: _Declaration) -> bool:
    """Whether *guard* stands in exactly the state its recorded disagreement pins."""
    excusing = _excusing(guard)
    return excusing is not None and declaration.value == excusing.observed_value


# ---------------------------------------------------------------------------
# guard-value
# ---------------------------------------------------------------------------


def _hand_on_row(guard: Guard) -> list[Finding]:
    """Requirement 4's escape hatch cannot be opened without naming who is tracking it.

    ``gate-register.schema.json`` requires ``handOnRow`` and requires it non-empty, so a register
    carrying one that is absent or blank is rejected before any rule reads it. This is therefore the
    second line, for a model built in code and for the day the schema stops being applied to a
    document, and it is here because this is where the rule is read.
    """
    if guard.disagreement is None or _excusing(guard) is not None:
        return []
    return [
        Finding(
            GUARD_VALUE,
            tuple(guard.holds_open),
            f"{guard.symbol}: the recorded disagreement names no handed-on row, so it excuses "
            f"nothing. A disagreement cannot be added without pointing at the row that tracks it "
            f'in "Open items this design hands on" -- whether the row says the right thing is a '
            f"reviewer's call, whether one was named is not",
        )
    ]


def _value(guard: Guard, declaration: _Declaration) -> list[Finding]:
    """Compare what *guard*'s declaring file carries against what the register records.

    Three outcomes for a literal that is read: the recorded one, the one a recorded disagreement
    observes, and anything else. Everything before them is a declaration this check could not read,
    which is a failure rather than a pass -- treating "not found" as "unchanged" is how a renamed
    guard stops being guarded.
    """
    gates = tuple(guard.holds_open)
    recorded = guard.recorded_value

    if declaration.unreadable:
        return [
            Finding(
                GUARD_VALUE,
                gates,
                f"{guard.symbol}: {guard.declared_in} {declaration.unreadable}, so the declaration "
                f"the register records at {recorded!r} was compared against nothing. A declaration "
                f'this check cannot read is a failure rather than a pass: "not found" is not '
                f'"unchanged"',
            )
        ]
    if not declaration.named:
        return [
            Finding(
                GUARD_VALUE,
                gates,
                f"{guard.symbol}: {guard.declared_in} declares no symbol of that name, so the "
                f"recorded {recorded!r} was compared against nothing. A renamed, moved or deleted "
                f'declaration is how a guard stops being guarded, so "not found" is not '
                f'"unchanged" here either',
            )
        ]
    if declaration.value is None:
        return [
            Finding(
                GUARD_VALUE,
                gates,
                f"{guard.symbol}: {guard.declared_in} names the symbol and carries no literal this "
                f"check can read beside it, so the recorded {recorded!r} was compared against "
                f"nothing. The register names the file the value is declared in, and a declaration "
                f"without a literal leaves the guard unverified",
            )
        ]

    value = declaration.value
    if value == recorded:
        return []

    disagreement = guard.disagreement
    if disagreement is None:
        return [
            Finding(
                GUARD_VALUE,
                gates,
                f"{guard.symbol}: the register records {recorded!r} and {guard.declared_in} "
                f"carries {value!r}, with no recorded disagreement. Requirement 4 criterion 7 "
                f"rejects the change and holds the Gate at {OPEN_STATUS}",
            )
        ]
    if _matched(guard, declaration):
        return [
            Finding(
                GUARD_VALUE,
                gates,
                f"{guard.symbol}: the register records {recorded!r}, {guard.declared_in} carries "
                f"{value!r}, and the recorded disagreement observes exactly that. Owner: "
                f"{disagreement.owner}. Remedies: {'; '.join(disagreement.remedies)}. Against: "
                f"{', '.join(disagreement.against)}. Tracked at {disagreement.hand_on_row}. Both "
                f"values are pinned and this prints on every run, so the gap is recorded rather "
                f"than silenced, and it does not block",
                blocking=False,
            )
        ]
    return [
        Finding(
            GUARD_VALUE,
            gates,
            f"{guard.symbol}: the register records {recorded!r}, the recorded disagreement "
            f"observes {disagreement.observed_value!r}, and {guard.declared_in} carries {value!r}. "
            f"The record excuses one known state, not the guard, so Requirement 4 criterion 7 "
            f"rejects the change and holds the Gate at {OPEN_STATUS}",
        )
    ]


def check_guard_value(premises: Premises) -> list[Finding]:
    """Each recorded guard's declaration carries its recorded literal, or its recorded exception."""
    findings: list[Finding] = []
    for guard in _registry(premises):
        findings.extend(_hand_on_row(guard))
        findings.extend(_value(guard, _declaration(premises.root, guard)))
    return findings


# ---------------------------------------------------------------------------
# guard-assertion
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class _Site:
    """One recorded assertion site, with whether it still names the guard and why not."""

    site: str
    names: bool
    reason: str


def _site(root: Path, guard: Guard, site: str) -> _Site:
    """Read one recorded assertion site.

    A site outside ``aerora/test/**`` names nothing this rule counts, for the reason the data half's
    `verified-observation` resolves a suite there: the assertion that matters is the one the
    overlay's own suites make. Comments are removed first, because a comment naming a guard asserts
    nothing about it.
    """
    if not PurePosixPath(site).is_relative_to(TEST_TREE):
        return _Site(site, names=False, reason=f"is not under {TEST_TREE}/")

    text, unreadable = _read(root, site)
    if text is None:
        return _Site(site, names=False, reason=unreadable)

    name = _declared_name(guard.symbol)
    if re.search(rf"\b{re.escape(name)}\b", _code(text)) is None:
        return _Site(site, names=False, reason="no longer names the symbol")
    return _Site(site, names=True, reason="names the symbol")


def check_guard_assertion(premises: Premises) -> list[Finding]:
    """Each recorded guard still appears in at least one recorded assertion site under the tree.

    "At least one" is Requirement 4 criterion 9 read as it is written: what it rejects is removing
    *the* check that asserts the guard's recorded value, so a guard two suites assert is still
    asserted when one of them consolidates into the other. The finding names every recorded site and
    why each one did not count, because a reviewer repairing this has to know whether the site moved,
    the symbol was renamed, or the assertion was dropped.

    A guard standing in the state its recorded disagreement pins is the one case this reports without
    blocking. The design records that the C++ assertion for such a guard is deliberately not written
    -- asserting the recorded value would fail CI and asserting the observed one would launder an
    unauthorised edit into a passing test -- and that this check carries the finding instead.
    """
    findings: list[Finding] = []
    for guard in _registry(premises):
        sites = [_site(premises.root, guard, site) for site in guard.asserted_in]
        if any(site.names for site in sites):
            continue

        read = (
            "; ".join(f"{site.site} {site.reason}" for site in sites)
            or "the register records no assertion site for it"
        )
        blocking = not _matched(guard, _declaration(premises.root, guard))
        standing = (
            ""
            if blocking
            else ". The register records a disagreement this source matches, and while that state "
            "stands this finding is the assertion the design records as not written"
        )
        findings.append(
            Finding(
                GUARD_ASSERTION,
                tuple(guard.holds_open),
                f"{guard.symbol}: no recorded assertion site under {TEST_TREE}/ names the symbol, "
                f"so nothing asserts its recorded {guard.recorded_value!r}: {read}. Requirement 4 "
                f"criterion 9 makes removing the check that asserts a guard's value as serious as "
                f"removing the guard{standing}",
                blocking=blocking,
            )
        )
    return findings


# ---------------------------------------------------------------------------
# adr-016-pair
# ---------------------------------------------------------------------------

# The two halves of the `ADR-016` core edit, each as the file that carries it and the text that is
# it. Requirement 7 criteria 2 and 17 name the declaration, the define and `src/Vehicle/Vehicle.h`
# verbatim, and the design's Finding 1 names `CUSTOM_DEFINITIONS` in `aerora/CMakeLists.txt` as
# where the define is written -- which is also where the second of the two recorded remedies would
# withdraw it. Neither a path nor a macro name is a Contract_Value: requirements.md carries both.
CORE_HEADER: Final = "src/Vehicle/Vehicle.h"
FRIEND_DECLARATION: Final = "friend class AeroraVehicleFactGroupRegistrar"
OVERLAY_MANIFEST: Final = "aerora/CMakeLists.txt"
FRIENDSHIP_DEFINITION: Final = "AERORA_VEHICLE_FACTGROUP_FRIENDSHIP"
CUSTOM_DEFINITIONS: Final = "CUSTOM_DEFINITIONS"

# The Gate Requirement 7 criterion 17 protects, read only where the registry records no entry for
# the guard and so records no mapping to read. Every other finding here names the entry's
# `holdsOpen`, because the registry is the authority for that mapping and comparing it against the
# criteria is `guard-registry`'s.
CORE_EDIT_GATE: Final = "DG-TLM"

_FRIEND: Final = re.compile(rf"\b{re.escape(FRIEND_DECLARATION)}\b\s*;")
_CMAKE_COMMENT: Final = re.compile(r"#[^\n]*")
_DEFINITIONS_LIST: Final = re.compile(
    rf"\bset\s*\(\s*{re.escape(CUSTOM_DEFINITIONS)}\b(?P<body>[^)]*)\)"
)


@dataclass(frozen=True)
class _Half:
    """One half of the pair: where it is written, whether it is applied, and why that is unknown.

    ``applied`` is ``None`` for a file this check could not read, or could not find the definition
    list in. That is a third state rather than "not applied": a renamed file and a withdrawn define
    are different repairs, and reading the first as the second is how the pair stops being watched.
    """

    file: str
    text: str
    applied: bool | None
    reason: str


def _friend_half(root: Path) -> _Half:
    """Whether ``src/Vehicle/Vehicle.h`` declares the registrar a friend of ``Vehicle``.

    Comments are removed first, because the Aerora-added block beside the declaration names the
    class in prose and reading one of those mentions as the declaration would report the half
    applied in a tree that had withdrawn it.

    **This reads the file and never writes it.** That is the one promise this rule makes about
    core: the edit is already applied, and the only thing a write here could do is launder it.
    """
    text, unreadable = _read(root, CORE_HEADER)
    if text is None:
        return _Half(CORE_HEADER, FRIEND_DECLARATION, None, unreadable)
    return _Half(CORE_HEADER, FRIEND_DECLARATION, _FRIEND.search(_code(text)) is not None, "")


def _definition_half(root: Path) -> _Half:
    """Whether ``aerora/CMakeLists.txt`` defines the macro in its ``CUSTOM_DEFINITIONS`` list.

    Scoped to that list, because that is where the define is written and where the recorded remedy
    would withdraw it: the comment above the list names the macro and the manifest could name it
    again in prose, so a whole-file search would read the explanation as the definition. CMake
    comments are removed for that reason, with the textual bound :func:`_code` carries.
    """
    text, unreadable = _read(root, OVERLAY_MANIFEST)
    if text is None:
        return _Half(OVERLAY_MANIFEST, FRIENDSHIP_DEFINITION, None, unreadable)

    listed = _DEFINITIONS_LIST.search(_CMAKE_COMMENT.sub("", text))
    if listed is None:
        return _Half(
            OVERLAY_MANIFEST, FRIENDSHIP_DEFINITION, None, f"sets no {CUSTOM_DEFINITIONS} list"
        )

    defined = re.search(rf"\b{re.escape(FRIENDSHIP_DEFINITION)}\b", listed["body"]) is not None
    return _Half(OVERLAY_MANIFEST, FRIENDSHIP_DEFINITION, defined, "")


def _authorising_guard(premises: Premises) -> Guard | None:
    """The registry entry whose change permission is the core edit's recorded authorisation.

    Requirement 7 criterion 18 records this guard as the one holding `DG-TLM` visible and records
    the acceptance of both parts as the condition under which Requirement 4 permits it to change
    value. So the entry's ``permittedToChangeBy`` *is* the authorisation state this rule reads, and
    the rule asks the freedom derivation rather than reading a part state of its own.
    """
    return next(
        (guard for guard in premises.register.guards if guard.symbol == FACT_GROUP_REGISTRAR_GUARD),
        None,
    )


def _pinning(freedom: GuardFreedom) -> tuple[str, ...]:
    """The parts that resolve and leave the guard pinned, which are the parts with owners to name.

    ``unresolved_parts`` is left out: a permission naming a part the register does not record
    resolves to no owner, and that it resolves to nothing at all is ``guard-registry``'s finding.
    """
    return (
        *freedom.unaccepted_parts,
        *freedom.unrecorded_parts,
        *freedom.unanswered_parts,
        *freedom.withholding_parts,
    )


def _owners(register: GateRegister, guard: Guard, freedom: GuardFreedom) -> tuple[str, ...]:
    """The owner labels the register records as owing the signature the core edit requires.

    Requirement 7 criterion 17 names Tab 1, and this reads that label out of the register rather
    than carrying it: a check whose expectation was a constant would stop naming the right owner
    the day an owner label changed, and the label is the Gate_Dossier's to record. Two sources, in
    this order -- the ``disagreement``'s own ``owner``, which is the register's answer to who
    resolves this gap, and the ``signingOwners`` of each part whose acceptance would free the
    guard, which resolves whether or not a disagreement is recorded.

    A part recording no owner at all is ``closure-signatures``'s named failure in the data half.
    Here it leaves the set empty, and the finding says that the register named no owner rather than
    naming one.
    """
    named: list[str] = [] if guard.disagreement is None else [guard.disagreement.owner]
    parts = {
        part_reference(dossier.id, part.id): part
        for dossier in register.gates
        for part in dossier.parts
    }
    for reference in _pinning(freedom):
        part = parts.get(reference)
        if part is not None:
            named.extend(part.signing_owners)
    return tuple(dict.fromkeys(named))


def _pinned_by(freedom: GuardFreedom) -> str:
    """Why the register records no acceptance permitting the guard, in the derivation's own terms.

    Each state names the parts in it, because each is a different repair and a reviewer reading
    "unauthorised" has to know whether a signature is missing, a record is missing, or an
    acceptance answered nothing about this guard.
    """
    stated = [
        f"{', '.join(references)}: {state}"
        for references, state in (
            (freedom.unaccepted_parts, "recorded unaccepted"),
            (freedom.unrecorded_parts, "accepted and naming no Closure_Record"),
            (freedom.unanswered_parts, "accepted with no verdict on the guard"),
            (freedom.withholding_parts, "accepted with the verdict withheld"),
            (freedom.unresolved_parts, "named as permitting it and recorded nowhere"),
        )
        if references
    ]
    return "; ".join(stated) or (
        "the registry records no part whose acceptance would permit the guard to change value"
    )


def check_adr_016_pair(premises: Premises) -> list[Finding]:
    """The `ADR-016` core edit's two halves against the authorisation the register records.

    Requirement 7 criterion 17 rejects a change that adds
    ``friend class AeroraVehicleFactGroupRegistrar`` to ``src/Vehicle/Vehicle.h`` or defines
    ``AERORA_VEHICLE_FACTGROUP_FRIENDSHIP`` while the `ADR-016` core-edit part holds no accepted
    Closure_Record, and requires the rejection to name the owner whose signature it needs.

    **The recorded authorisation state is the change permission on the guard.** Requirement 7
    criterion 18 records the acceptance of both parts of `DG-TLM` as the condition under which
    Requirement 4 permits ``AeroraVehicleFactGroupRegistrar::isRegistrationAvailable()`` to change
    value, so :func:`guard_freedom` already answers whether the authorisation has been given and
    this rule asks it. What the pair must match is therefore:

    - the guard free -- every part permitted to change it accepted, and each of those acceptances
      permitting it -- with both halves applied, or with neither. An authorisation is a permission
      rather than an instruction, so a signed authorisation nobody acted on is not a finding here;
      whether the guard's value then followed the edit is ``guard-value``'s.
    - the guard pinned, with neither half applied: the state criterion 17 protects.
    - the guard pinned with both halves applied, which is the state this tree stands in. Reported
      with the owner named, and blocking unless the register records a ``disagreement`` for the
      guard -- the one known state the design pins. The alternative is the cliff the design refuses:
      a rule rejecting the applied pair on every commit until an unrelated authorisation landed
      would be removed rather than obeyed.
    - exactly one half applied, under either state: rejected. Criterion 2 records the core edit as
      comprising both halves, the friend declaration alone leaves the registrar inert and the define
      alone does not compile, so one half matches no recorded state and no record pins it.

    **It divides with ``guard-value`` by subject rather than by silence.** ``guard-value`` reads the
    guard's declaration under ``aerora/src/**`` and owns everything about the *value*: the observed
    literal, whether the recorded disagreement still observes it, the remedies, the handed-on row,
    and a record that has stopped matching. This rule reads neither that file nor ``recordedValue``
    and reports none of them. It owns the two *edits*, in two files ``guard-value`` never opens. So
    the tree in which the declaration is applied and the define is not is reported here and nowhere
    else, which is the gap a rule that went quiet whenever ``guard-value`` spoke would leave:
    ``isRegistrationAvailable()`` returns the recorded ``false`` in that tree and the value rule
    passes over it.

    Two premises belong to the data half and are reported here as a pair read against nothing
    rather than passed over: that the registry covers the guard at all, and that a change permission
    resolves to a part. Both are ``guard-registry``'s findings, and an unresolved premise is a
    failure, never a pass.

    **Read-only.** Both files are read as characters and neither is written.
    """
    guard = _authorising_guard(premises)
    if guard is None:
        return [
            Finding(
                ADR_016_PAIR,
                (CORE_EDIT_GATE,),
                f"{FACT_GROUP_REGISTRAR_GUARD}: the guard registry records no entry for the guard "
                f"Requirement 7 criterion 18 records as holding the Gate visible, so {CORE_HEADER} "
                f"and {OVERLAY_MANIFEST} were compared against no recorded authorisation state. "
                f"That the registry covers the guard is {GUARD_REGISTRY}'s finding in the data "
                f"half; what is reported here is the pair read against nothing",
            )
        ]

    gates = tuple(guard.holds_open) or (CORE_EDIT_GATE,)
    try:
        freedom = guard_freedom(
            premises.register,
            {reference: closure.record for reference, closure in premises.closures.items()},
        )[guard.symbol]
    except GateRegisterError as error:
        return [
            Finding(
                ADR_016_PAIR,
                gates,
                f"{guard.symbol}: {error}, so the authorisation the register records for the "
                f"ADR-016 core edit has two readings and the pair was compared against neither",
            )
        ]

    halves = (_friend_half(premises.root), _definition_half(premises.root))
    unanswerable = [half for half in halves if half.applied is None]
    if unanswerable:
        return [
            Finding(
                ADR_016_PAIR,
                gates,
                f"{half.text}: {half.file} {half.reason}, so the half of the ADR-016 core edit it "
                f"carries was compared against nothing. A file this check cannot read is a failure "
                f'rather than a pass: "not found" is not "not applied"',
            )
            for half in unanswerable
        ]

    applied = [half for half in halves if half.applied]
    absent = [half for half in halves if not half.applied]
    owners = _owners(premises.register, guard, freedom)
    requires = (
        f" Requirement 7 criterion 17 names the owner whose signature the core edit requires, "
        f"which the register records as {', '.join(owners)}"
        if owners
        else " Requirement 7 criterion 17 requires the rejection to name the owner whose signature "
        "the core edit requires, and the register records none for it"
    )

    if applied and absent:
        return [
            Finding(
                ADR_016_PAIR,
                gates,
                f"the ADR-016 core edit is one edit in two files, and this tree carries "
                f"{applied[0].text} in {applied[0].file} and not {absent[0].text} in "
                f"{absent[0].file}. Requirement 7 criterion 2 records the core edit as comprising "
                f"both halves, so one half alone matches no recorded authorisation state: the "
                f"friend declaration alone leaves the registrar inert, and the define alone does "
                f"not compile.{requires}",
            )
        ]
    if not applied or freedom.is_free:
        return []

    standing = (
        f"this tree carries {FRIEND_DECLARATION} in {CORE_HEADER} and {FRIENDSHIP_DEFINITION} in "
        f"{OVERLAY_MANIFEST} {CUSTOM_DEFINITIONS}, and the register records no acceptance "
        f"permitting it -- {_pinned_by(freedom)}"
    )
    excusing = _excusing(guard)
    if excusing is None:
        return [
            Finding(
                ADR_016_PAIR,
                gates,
                f"{standing} -- and no disagreement that excuses the applied pair. Requirement 7 "
                f"criterion 17 rejects the change and holds the Gate at {OPEN_STATUS}.{requires}",
            )
        ]
    return [
        Finding(
            ADR_016_PAIR,
            gates,
            f"{standing}. The recorded disagreement for {guard.symbol} pins this state and tracks "
            f"it at {excusing.hand_on_row}, so this prints on every run and does not block; the "
            f"guard's own value, the remedies and whether the record still observes the source are "
            f"{GUARD_VALUE}'s finding, reported once there.{requires}",
            blocking=False,
        )
    ]


# ---------------------------------------------------------------------------
# mavlink-identifier
# ---------------------------------------------------------------------------

# The Gate whose Decision_Content this rule holds visible. Read as a constant rather than out of
# `mavlinkIdentifiers.gate`, for the reason the data half records `READ_ONLY_GATE` as one: the
# posture is part of the document under check, and the schema pins that key to this Gate as a
# `const` so the two cannot disagree.
MISSION_GATE: Final = "DG-MIS"

# The four token classes Requirement 6 criterion 14 is enforced through, which are the four the
# task and the design's table name. A prefix names a class of identifiers and is not one: no
# dialect issues `MAV_CMD_`, requirements.md and the design carry all four already, and the Glossary
# counts the message, command and enumeration *identifier* as the Contract_Value. So the prefixes
# are constants here while no identifier is -- not in this module, not in the register, not in a
# fixture -- and no finding prints the token it found.
IDENTIFIER_PREFIXES: Final = ("MAV_CMD_", "MAVLINK_MSG_ID_", "MAV_COMP_ID_", "MAV_FRAME_")

# A token of any of the four classes: a prefix and at least one character after it, which is what
# makes it an identifier rather than a mention of the class. That bound is the same one that lets
# the prefixes be constants in this module -- the Glossary counts the *identifier* as the
# Contract_Value -- so a sentence naming a class is prose and a sentence naming a member of it is
# not. Deliberately not left-anchored on a word boundary, because the rule this one takes over from
# matches the prefix as a substring and the authoritative copy must not be the narrower of the two.
_IDENTIFIER: Final = re.compile(
    "|".join(rf"{re.escape(prefix)}\w+" for prefix in IDENTIFIER_PREFIXES)
)

# What the walk does not descend into: build output and tool caches, which hold generated copies of
# upstream headers rather than anything the overlay authored. Taken from the shared list minus the
# one directory it drops that this rule needs -- `find_cpp_files` skips `test/` by default, and the
# allow-list's third category is the Aerora suites' own fixtures, which live there.
_SKIP_DIRS: Final = DEFAULT_SKIP_DIRS - {"test"}


@dataclass(frozen=True)
class _Baseline:
    """What the allow-listed sites hold, which this rule reads rather than records.

    ``identifiers`` is the distinct tokens the readable sites carry. Nothing is compared against
    it -- a token in a file the allow-list does not carry is rejected whether or not an allow-listed
    site holds the same one -- so what it is for is the measurement the run states: that the
    permitted set came from the tree, and that an allow-list whose sites hold none has gone stale.

    ``allowed`` carries every allow-listed path, readable or not, so a site the tree lacks is one
    finding rather than two.
    """

    allowed: frozenset[str]
    identifiers: frozenset[str]
    unreadable: tuple[tuple[str, str], ...]
    reasonless: tuple[str, ...]


def _searchable(root: Path, relative: str) -> tuple[str | None, str]:
    """A file's text as this rule searches it, with the reason it could not be read.

    Decoded leniently, which :func:`_read` is not, because the two answer different questions.
    ``_read`` opens a file the register names and the document's encoding is part of the premise;
    this opens whatever the overlay tree holds, and an undecodable byte somewhere in a file must
    not stop the ASCII token elsewhere in it from being found. A file whose bytes are not text at
    all decodes to no token, which is the right answer rather than a skipped file.
    """
    try:
        data = (root / relative).read_bytes()
    except FileNotFoundError:
        return None, "does not exist"
    except OSError as error:
        return None, f"could not be read: {error.strerror}"
    return data.decode("utf-8", errors="replace"), ""


def _overlay_files(root: Path) -> list[str]:
    """Every file under the overlay tree, as repository-relative posix paths in path order."""
    found: list[str] = []
    for path in (root / OVERLAY_TREE).rglob("*"):
        relative = path.relative_to(root)
        if path.is_file() and not should_skip_path(relative, _SKIP_DIRS):
            found.append(relative.as_posix())
    return sorted(found)


def _baseline(root: Path, posture: MavlinkIdentifierPosture) -> _Baseline:
    """Read the allow-listed sites, which is where the permitted identifiers come from."""
    identifiers: set[str] = set()
    unreadable: list[tuple[str, str]] = []
    reasonless: list[str] = []
    for site in posture.allow_list:
        if not site.reason.strip():
            reasonless.append(site.path)
        text, cannot_read = _searchable(root, site.path)
        if text is None:
            unreadable.append((site.path, cannot_read))
            continue
        identifiers.update(_IDENTIFIER.findall(text))
    return _Baseline(
        allowed=frozenset(site.path for site in posture.allow_list),
        identifiers=frozenset(identifiers),
        unreadable=tuple(unreadable),
        reasonless=tuple(reasonless),
    )


def _classes(tokens: Sequence[str]) -> str:
    """The token classes *tokens* fall into, in the order :data:`IDENTIFIER_PREFIXES` names them.

    The classes and never the tokens: a finding says which class of identifier a file carries and
    where, and a reviewer reads the file for the rest. Printing the token would make this rule's
    own output the place a Contract_Value is written down.
    """
    carried = [
        prefix
        for prefix in IDENTIFIER_PREFIXES
        if any(token.startswith(prefix) for token in tokens)
    ]
    return ", ".join(carried)


def _counted(tokens: Sequence[str]) -> str:
    """How many tokens a file holds, worded for one as well as for many."""
    return f"{len(tokens)} MAVLink identifier token{'' if len(tokens) == 1 else 's'}"


def _matching_lines(text: str) -> list[tuple[int, tuple[str, ...]]]:
    """Each line that holds a token, as its one-based number and the tokens on it.

    Comments are not removed, which is the one place this rule parts company with its neighbours.
    ``guard-value`` strips them because a sentence *about* a guard is not a declaration of one; a
    MAVLink identifier written into a comment is the identifier, and the tokens in the tree today
    include a documentation file carrying them in prose.
    """
    return [
        (number, tuple(found))
        for number, line in enumerate(text.splitlines(), start=1)
        if (found := _IDENTIFIER.findall(line))
    ]


def _unlisted(relative: str, text: str) -> list[Finding]:
    """Reject *relative* where it holds a token and the allow-list does not carry the file."""
    matched = _matching_lines(text)
    if not matched:
        return []

    first, _ = matched[0]
    tokens = [token for _, found in matched for token in found]
    return [
        Finding(
            MAVLINK_IDENTIFIER,
            (MISSION_GATE,),
            f"{relative}: holds {_counted(tokens)} of {_classes(tokens)}, first at line {first}, "
            f"and the register's allow-list does not carry the file. Requirement 6 criterion 14 "
            f"rejects the change before it enters the repository. The repair is to take the "
            f"identifier out, or to allow-list the file with the reason it reads one it does not "
            f"author -- a site and a reason, never the identifier itself",
        )
    ]


def check_mavlink_identifier(premises: Premises) -> list[Finding]:
    """No MAVLink identifier appears under the overlay tree outside the allow-listed sites.

    Requirement 4 criterion 2 and Requirement 6 criterion 14 read against the one class of
    Contract_Value a text check can recognise without classifying anything: the Glossary counts a
    MAVLink message, command or enumeration identifier as a Contract_Value, and `DG-MIS` is the Gate
    whose Decision_Content records the mission commands, so a token appearing where no owner
    allow-listed one is rejected naming the file and that Gate.

    **The allow-list is recorded by site and reason, never by identifier, and this rule is what
    makes that affordable.** It reads the allow-listed files and takes whatever identifiers they
    hold as the current baseline, so the permitted set lives in the tree and the register holds not
    one identifier of its own. That is a ratchet rather than a freeze in both directions: a token
    added to an allow-listed file is accepted without the register being edited, and a token in any
    other file is rejected whether or not an allow-listed site already holds the same one. It is
    also not a cliff -- the tokens standing in the tree today are accepted by recording their sites,
    which is the only thing an owner signs here.

    **It walks the tree rather than the files the register names, and it is the only rule here that
    does.** The other three read a file because the register or a criterion names it; this one has
    to find a token in a file nobody named, so the overlay tree is its input and the selected file
    list still decides nothing. The walk covers every file under ``aerora/`` and not only the C++
    ones: ``find_cpp_files`` yields C++ alone and its default skip list drops ``test/``, while the
    tokens in the tree today include a documentation file carrying them in prose and the Aerora
    suites' own fixtures, so either bound would let a Contract_Value land somewhere this rule had
    stopped looking.

    **This is the one rule moved rather than duplicated.**
    ``.kiro/tools/aerora-ui-static-check.ps1`` keeps working where it is and this is the
    authoritative copy, which is the wider of the two: that one reads three of the four classes,
    only the two UI directories, only four file extensions and no allow-list at all. So the two are
    not equivalent checks and neither answer substitutes for the other.

    Three premise failures, each a finding rather than a quiet pass: an allow-listed site the tree
    does not carry, because a baseline read from a file that is not there is not a baseline; an
    allow-list entry whose reason is blank, which is the second line behind a schema that requires
    one, for the reason :func:`_hand_on_row` is the second line behind ``handOnRow``; and an overlay
    tree this rule cannot walk, because a run that searched nothing must not read as a run that
    found nothing.
    """
    posture = premises.register.mavlink_identifiers
    baseline = _baseline(premises.root, posture)
    findings = [
        Finding(
            MAVLINK_IDENTIFIER,
            (MISSION_GATE,),
            f"{path}: the allow-list records the site and no reason for it, so nothing says why "
            f"the file reads an identifier it does not author. The reason is the entry: a site "
            f"recorded without one is an exemption no reviewer can re-read",
        )
        for path in baseline.reasonless
    ]
    findings.extend(
        Finding(
            MAVLINK_IDENTIFIER,
            (MISSION_GATE,),
            f"{path}: the allow-listed site {reason}, so the baseline was read from nothing. The "
            f"identifiers this rule permits are the ones its allow-listed files hold, so a site "
            f'the tree does not carry permits nothing and hides that it permits nothing: "not '
            f'found" is not "unchanged" here either',
        )
        for path, reason in baseline.unreadable
    )

    if not (premises.root / OVERLAY_TREE).is_dir():
        findings.append(
            Finding(
                MAVLINK_IDENTIFIER,
                (MISSION_GATE,),
                f"{OVERLAY_TREE}/ is not a directory this rule could walk, so no file was searched "
                f"for a MAVLink identifier. A run that searched nothing is a failure rather than a "
                f"pass: it is indistinguishable from a run that found nothing",
            )
        )
        return findings

    for relative in _overlay_files(premises.root):
        if relative in baseline.allowed:
            continue
        text, cannot_read = _searchable(premises.root, relative)
        if text is None:
            findings.append(
                Finding(
                    MAVLINK_IDENTIFIER,
                    (MISSION_GATE,),
                    f"{relative}: the file {cannot_read}, so it was searched for no MAVLink "
                    f"identifier. A file under {OVERLAY_TREE}/ that this rule cannot read is a "
                    f"failure rather than a pass",
                )
            )
            continue
        findings.extend(_unlisted(relative, text))
    return findings


# ---------------------------------------------------------------------------
# The four absence rules
# ---------------------------------------------------------------------------

# The tree the four absence rules read. The overlay's own source, without its suites: every subject
# here is something the *product* must not carry, and `aerora/test/**` is where the instrument that
# proves the prohibition lives -- the one implementation of the payload command sink in the tree is
# a test double whose own comment says so, and every threshold call site is a suite configuring a
# fixture. A rule scoped to the overlay tree would reject the evidence for the prohibition it
# enforces. Recorded here rather than read out of the register, for the reason the data half records
# `TEST_TREE`: the design's table names the tree, and a directory name is not a Contract_Value.
SOURCE_TREE: Final = f"{OVERLAY_TREE}/src"

# What an implementation can be written in. A `.md` naming a type in prose and a `.cmake` listing a
# header are neither a declaration nor a call, and `aerora/src/Payload/README.md` and
# `aerora/src/PayloadPower.cmake` carry the payload command sink's name for exactly those reasons.
_SOURCE_SUFFIXES: Final = (".h", ".cc")

# The Gate each absence rule waits on, which is the one place that mapping is written: `_standing`
# reads it to decide whether a rule is in force and `_established` reads it to say how many stood
# down. Requirement 14 criterion 13 names `DG-VID`, Requirement 5 criterion 4 names the read-only
# Gate, Requirement 8 criteria 13 and 16 name `DG-FRS`, and Requirement 10 criterion 18 names
# `DG-PAY` *and* the read-only Gate and requires each of the two that is open to be named.
#
# The read-only Gate is the data half's constant, imported rather than respelled, because the
# posture that records both seams and every `FORBIDDEN-UNTIL` row reads that one Gate. The other
# three are constants here on the same terms `MISSION_GATE` is: requirements.md names each verbatim
# against the criterion the rule discharges.
VIDEO_GATE: Final = "DG-VID"
FRESHNESS_GATE: Final = "DG-FRS"
PAYLOAD_GATE: Final = "DG-PAY"

# Every rule whose criterion is written `WHILE <gate> is OPEN`, which is the four absence rules and
# `actionable-control`. The fifth is not an absence rule -- it compares two sets rather than
# asserting that something is missing -- but its conditional is the same one, and `_standing` and the
# run's summary read this mapping for the stand-down reasoning that applies identically to all five.
CONDITIONAL_GATES: Final[Mapping[str, tuple[str, ...]]] = {
    POPULATED_FILE_OVERRIDE: (VIDEO_GATE,),
    ACTION_SEAM: (READ_ONLY_GATE,),
    FRESHNESS_THRESHOLD: (FRESHNESS_GATE,),
    PAYLOAD_COMMAND_SINK: (PAYLOAD_GATE, READ_ONLY_GATE),
    ACTIONABLE_CONTROL: (READ_ONLY_GATE,),
}


def _standing(premises: Premises, check: str) -> tuple[tuple[str, ...], list[Finding]]:
    """The Gates *check* waits on that are open, or the premise it could not read them against.

    **The derived openness, never the recorded status.** Each of these four criteria is written
    ``WHILE <gate> is OPEN``, and Requirement 3 criterion 7 derives that from whether every part is
    accepted. Reading ``dossier.status`` would let a Gate recorded `CLOSED` over an unsigned part
    switch four rules off, which is the disagreement `gate-status-derivation` exists to report in
    the data half -- so this reads the derivation and that check reports the gap.

    An empty tuple with no finding is a rule standing down, which is the criterion's own
    conditional rather than a rule going quiet: once the Gate holds an accepted Closure_Record, the
    thing the rule rejects is the thing an owner signed for.

    A Gate the register records no Gate_Dossier for leaves the conditional unanswerable, so the
    rule reports that and does not run. That the register carries one dossier per Gate is the
    schema's to reject -- ten items, each `id` an enum of the ten identifiers -- and what is
    reported here is the rule read against nothing.
    """
    openness = premises.derived.gate_open
    gates = CONDITIONAL_GATES[check]
    unrecorded = tuple(gate for gate in gates if gate not in openness)
    if unrecorded:
        return (), [
            Finding(
                check,
                (gate,),
                "the register records no Gate_Dossier for the Gate this rule waits on, so whether "
                "the rule is in force could not be derived and the subject it rejects was looked "
                "for nowhere. That the register carries a Gate_Dossier per Gate is the schema's "
                'to reject; what is reported here is the rule read against nothing: "not found" '
                'is not "not open"',
            )
            for gate in unrecorded
        ]
    return tuple(gate for gate in gates if openness[gate].is_open), []


def _in_force(premises: Premises) -> int:
    """How many Gate-conditional rules the derivation leaves in force, for the run's own summary."""
    openness = premises.derived.gate_open
    return sum(
        1
        for gates in CONDITIONAL_GATES.values()
        if any(gate in openness and openness[gate].is_open for gate in gates)
    )


@dataclass(frozen=True)
class _SourceTree:
    """The overlay source as the absence rules read it: whether it was walked, and what it holds.

    ``walked`` is false for a tree that is not a directory, which is the state a run must not report
    as an absence: a rule that searched nothing is indistinguishable from a rule that found nothing.

    ``unreadable`` carries a file and the reason, so a file whose subject could be hidden behind an
    I/O error is a finding rather than a gap. Each rule reports it under its own name and its own
    Gate, the way ``guard-value`` and ``guard-assertion`` each report the declaring file they could
    not read: one defect, but a different decision left unprotected in each line.
    """

    walked: bool
    sources: tuple[tuple[str, str], ...]
    unreadable: tuple[tuple[str, str], ...]


def _source_tree(root: Path) -> _SourceTree:
    """Read every overlay source file an absence rule searches, in path order.

    Decoded leniently, for the reason :func:`_searchable` is: an undecodable byte somewhere in a
    file must not stop the subject elsewhere in it from being found, and a file whose bytes are not
    text at all carries no declaration and no call.
    """
    tree = root / SOURCE_TREE
    if not tree.is_dir():
        return _SourceTree(walked=False, sources=(), unreadable=())

    sources: list[tuple[str, str]] = []
    unreadable: list[tuple[str, str]] = []
    for path in sorted(tree.rglob("*")):
        relative = path.relative_to(root)
        if path.suffix not in _SOURCE_SUFFIXES or not path.is_file():
            continue
        if should_skip_path(relative, _SKIP_DIRS):
            continue
        text, cannot_read = _searchable(root, relative.as_posix())
        if text is None:
            unreadable.append((relative.as_posix(), cannot_read))
            continue
        sources.append((relative.as_posix(), text))
    return _SourceTree(walked=True, sources=tuple(sources), unreadable=tuple(unreadable))


def _unsearched(
    tree: _SourceTree, check: str, gates: tuple[str, ...], subject: str
) -> list[Finding]:
    """What *check* could not search, which is a failure rather than an absence."""
    if not tree.walked:
        return [
            Finding(
                check,
                gates,
                f"{SOURCE_TREE}/ is not a directory this rule could walk, so no file was searched "
                f"for {subject}. A run that searched nothing is a failure rather than a pass: it "
                f"is indistinguishable from a run that found nothing",
            )
        ]
    return [
        Finding(
            check,
            gates,
            f"{path}: the file {reason}, so it was searched for {subject} nowhere. A file under "
            f"{SOURCE_TREE}/ that this rule cannot read is a failure rather than a pass",
        )
        for path, reason in tree.unreadable
    ]


def _balanced(text: str, opening: int) -> int | None:
    """The index of the brace closing the one at *opening*, or ``None`` where none closes it.

    Comments and string literals are blanked before this runs, so a brace inside either is gone and
    the depth count is over code. An unbalanced span is ``None`` rather than the end of the file:
    reading a truncated list as the whole of it would read an absence off half a document.
    """
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return index
    return None


# ---------------------------------------------------------------------------
# populated-file-override and action-seam
# ---------------------------------------------------------------------------

# The allow-list both rules read, and the tier one of them rejects. Requirement 5 criterion 4 and
# Requirement 14 criterion 13 name the function verbatim and neither names a file, so the rules find
# the definition by walking rather than against a path the register does not record -- which is also
# what keeps them working if the file moves, and what makes a function nobody defines a finding
# instead of a silence.
OVERRIDE_ENTRIES: Final = "AeroraQmlOverrides::overrideEntries()"
POPULATED_FILE_TIER: Final = "Tier::PopulatedFile"

_OVERRIDE_ENTRIES_DEFINITION: Final = re.compile(
    r"\bAeroraQmlOverrides\s*::\s*overrideEntries\s*\(\s*\)\s*\{"
)

# The list the definition returns. Only whitespace is admitted between the signature and the body's
# brace, so a *call* to the function -- which `populatedFileOverrideCount()` makes inside a `for`
# header -- is not read as a definition and its loop body is not read as an entry list.
_RETURNED_LIST: Final = re.compile(r"\breturn\b[^;{}]*\{")

# The tier, matched without its qualifier because the span this runs over is the entry list alone:
# `using enum Tier` would write the member bare, and nothing else inside a list of paths and tiers
# is named `PopulatedFile`. The paths are blanked first, so a file called `PopulatedFile.qml` is a
# path rather than a tier.
_POPULATED_FILE: Final = re.compile(r"\bPopulatedFile\b")


@dataclass(frozen=True)
class _AllowList:
    """One definition of the override allow-list: where it is, and the entries it returns.

    The span is the returned list rather than the file, which is what makes these two rules usable.
    Three things in `aerora/src/UI/AeroraQmlOverrides.cc` name a subject of theirs and are not one:
    the comment block inside the function names `Tier::PopulatedFile` and both pre-approved seams in
    prose, `populatedFileOverrideCount()` compares an entry's tier against `Tier::PopulatedFile` in
    code, and the header declares the enumerator. A rule that read the file would reject the tree it
    exists to protect.

    Two readings of that one span, because the two rules read opposite halves of an entry.
    ``entries`` keeps the quoted paths, which is what ``action-seam`` matches a seam in. ``tiers``
    blanks them, which is what keeps a path named after a tier from reading as one. Both are the
    same length as the span they came from, so an index into either is an index into the other and
    into the file.

    ``line`` is the file line the list opens on, so a finding can say where without re-reading.
    """

    file: str
    entries: str
    tiers: str
    line: int


def _entry_line(allow_list: _AllowList, index: int) -> int:
    """The file line the character at *index* of the entry list falls on."""
    return allow_list.line + allow_list.entries.count("\n", 0, index)


def _allow_lists(tree: _SourceTree) -> tuple[list[_AllowList], list[str]]:
    """Every override allow-list the tree defines, and each file whose list could not be read.

    More than one definition is read as more than one allow-list rather than as an error: two
    definitions are two lists, and a rule that rejected the second would reject a fixture double
    instead of reading it. A file defining the function whose returned list this cannot bracket is
    the other half -- "not found" is not "empty", because an entry list read as empty is an absence
    nobody established.
    """
    found: list[_AllowList] = []
    unreadable: list[str] = []
    for path, text in tree.sources:
        # Scanned with the paths blanked as well, so a brace or a `return` inside a quoted path
        # cannot move where the list is read to end. Sliced from the text that keeps them, at the
        # same indices, because blanking preserves every offset.
        masked = _code(text)
        scanned = _without_strings(masked)
        for definition in _OVERRIDE_ENTRIES_DEFINITION.finditer(scanned):
            body = _balanced(scanned, definition.end() - 1)
            ends = len(scanned) if body is None else body
            returned = _RETURNED_LIST.search(scanned, definition.end(), ends)
            opening = None if returned is None else _balanced(scanned, returned.end() - 1)
            if returned is None or opening is None:
                unreadable.append(path)
                continue
            span = slice(returned.end() - 1, opening + 1)
            found.append(
                _AllowList(
                    file=path,
                    entries=masked[span],
                    tiers=scanned[span],
                    line=_line_of(masked, span.start),
                )
            )
    return found, unreadable


def _no_allow_list(check: str, gates: tuple[str, ...], subject: str) -> Finding:
    """No file under the overlay source tree defines the allow-list both rules read."""
    return Finding(
        check,
        gates,
        f"no file under {SOURCE_TREE}/ defines {OVERRIDE_ENTRIES}, so the override allow-list was "
        f"searched for {subject} nowhere. The criterion names the function rather than a file, so "
        f"this rule finds the definition by walking -- and a function that has been renamed, moved "
        f'out of the tree or deleted is a failure rather than a pass: "not found" is not "absent"',
    )


def _unreadable_allow_list(check: str, gates: tuple[str, ...], path: str, subject: str) -> Finding:
    """A file defining the allow-list whose returned entry list could not be bracketed."""
    return Finding(
        check,
        gates,
        f"{path}: defines {OVERRIDE_ENTRIES} and returns no entry list this check can bracket, so "
        f"the list was searched for {subject} nowhere. The rules read the returned list rather than "
        f"the file, because the file names every subject of both of them in prose beside it",
    )


def check_populated_file_override(premises: Premises) -> list[Finding]:
    """No `Tier::PopulatedFile` entry is in the override allow-list while the Gate is open.

    Requirement 14 criterion 13 rejects a change adding such an entry while `DG-VID` holds no
    accepted Closure_Record, because that entry is the silent fork the Gate exists to attach a fork
    point, an owner and a drift-review cadence to. The entry *is* the fork, so the rule is the
    absence of one.

    **It reads the returned entry list, not the file.** The three legitimate mentions in the real
    file are the reason: the function's own comment block, the count that compares an entry's tier
    against the tier, and the enumerator's declaration in the header. Comments are blanked and the
    span is the list, so each of the three is left where it is.

    The guard the acceptance would release is `AeroraQmlOverrides::populatedFileOverrideCount()`,
    whose value is ``guard-value``'s to read against the register and whose change permission is
    ``guard-registry``'s to derive. This rule reads neither: it reads the list the count counts, in
    the state the count reports, so a list that gained an entry is rejected here before the count
    could report it and whether the recorded value followed is reported there.
    """
    gates, findings = _standing(premises, POPULATED_FILE_OVERRIDE)
    if findings or not gates:
        return findings

    subject = f"a {POPULATED_FILE_TIER} entry"
    tree = _source_tree(premises.root)
    findings = _unsearched(tree, POPULATED_FILE_OVERRIDE, gates, subject)
    lists, unreadable = _allow_lists(tree)
    findings.extend(
        _unreadable_allow_list(POPULATED_FILE_OVERRIDE, gates, path, subject) for path in unreadable
    )
    if tree.walked and not lists and not unreadable:
        findings.append(_no_allow_list(POPULATED_FILE_OVERRIDE, gates, subject))

    for allow_list in lists:
        entry = _POPULATED_FILE.search(allow_list.tiers)
        if entry is None:
            continue
        findings.append(
            Finding(
                POPULATED_FILE_OVERRIDE,
                gates,
                f"{allow_list.file}: the {OVERRIDE_ENTRIES} list carries a {POPULATED_FILE_TIER} "
                f"entry at line {_entry_line(allow_list, entry.start())}, and the register holds "
                f"no accepted Closure_Record for the Gate. Requirement 14 criterion 13 rejects the "
                f"change before it enters the repository. Each such entry forks a populated "
                f"upstream file silently, so it waits on the fork point, the named owner and the "
                f"drift-review cadence the Closure_Record settles rather than on a review of this "
                f"file",
            )
        )
    return findings


def check_action_seam(premises: Premises) -> list[Finding]:
    """Neither pre-approved seam is in the override allow-list while the read-only Gate is open.

    Requirement 5 criterion 4 rejects the addition of either seam, names the Gate the operator
    actions each seam introduces wait on, and retains both as pre-approved and undeclared. Both
    exist to introduce an *action*, which is why declaring one is rejected while nothing evaluates
    an authorisation: the seam would put an action surface in front of an operator with no decision
    behind it.

    **The two seams are the criterion's, read from the data half's constant.** The register's
    ``readOnly.preApprovedUndeclaredSeams`` is part of the document under check -- ``forbidden-until``
    compares that list against this same constant, so reading the posture here would let a seam
    dropped from the posture stop being watched by the rule *and* by the check that notices. This is
    the division ``adr-016-pair`` draws: the subject it rejects is the text Requirement 7 criterion
    17 names, and what it reads out of the register is the *owner label*, which is a Gate_Dossier's
    to record and no criterion's to spell.

    A seam is matched by its file name wherever it appears in the list, because the allow-list
    carries a resource path and the criterion names a file: either spelling is the seam. The one
    entry this build declares is a third seam the criterion does not name, so the rule reads a list
    with an entry in it rather than an empty one.
    """
    gates, findings = _standing(premises, ACTION_SEAM)
    if findings or not gates:
        return findings

    subject = "either pre-approved seam"
    tree = _source_tree(premises.root)
    findings = _unsearched(tree, ACTION_SEAM, gates, subject)
    lists, unreadable = _allow_lists(tree)
    findings.extend(
        _unreadable_allow_list(ACTION_SEAM, gates, path, subject) for path in unreadable
    )
    if tree.walked and not lists and not unreadable:
        findings.append(_no_allow_list(ACTION_SEAM, gates, subject))

    for allow_list in lists:
        for seam in PRE_APPROVED_SEAMS:
            declared = allow_list.entries.find(seam)
            if declared < 0:
                continue
            findings.append(
                Finding(
                    ACTION_SEAM,
                    gates,
                    f"{allow_list.file}: the {OVERRIDE_ENTRIES} list carries {seam} at line "
                    f"{_entry_line(allow_list, declared)}, and the register holds no accepted "
                    f"Closure_Record for the Gate. Requirement 5 criterion 4 rejects the addition "
                    f"of the seam to the override allow-list and retains both seams as "
                    f"pre-approved and undeclared, because the operator actions each one "
                    f"introduces wait on this decision",
                )
            )
    return findings


# ---------------------------------------------------------------------------
# freshness-threshold
# ---------------------------------------------------------------------------

# The setter a call site calls. Requirement 8 criterion 13 rejects a change introducing a threshold
# *value*, and a call is where a value is passed; the name is not a Contract_Value and no finding
# here prints an argument, because a threshold value is one.
THRESHOLD_SETTER: Final = "setThreshold"

_THRESHOLD_CALL: Final = re.compile(rf"\b{THRESHOLD_SETTER}\s*\(")

# What tells a call from a declaration, read backwards from the name over the statement it is in.
# A member access reaches the setter through an object, a qualifier may precede either, a declarator
# head ends in a type, and an expression keyword ends in neither.
_MEMBER_ACCESS: Final = re.compile(r"(?:\.|->)\s*$")
_QUALIFIER: Final = re.compile(r"(?:\b\w+\s*::\s*)+$")
_DECLARATOR: Final = re.compile(r"[\w>&*\]]\s*$")
_LAST_WORD: Final = re.compile(r"\w+\s*$")

# Where the statement the name sits in begins, as far as this classification needs: a statement end,
# a block brace, or the start of an argument or an initialiser list.
_STATEMENT_START: Final = ";{}(,"

# The words that may precede an expression and end in an identifier, which a declarator head also
# ends in. Without them `return setThreshold(x);` would read as a declaration of a function whose
# return type is `return`.
_EXPRESSION_KEYWORDS: Final = frozenset(
    {
        "return",
        "co_return",
        "co_await",
        "co_yield",
        "throw",
        "new",
        "delete",
        "sizeof",
        "else",
        "do",
        "case",
        "default",
        "and",
        "or",
        "not",
    }
)


def _is_call(code: str, start: int) -> bool:
    """Whether the setter named at *start* of *code* is called there rather than declared.

    **This is the whole difficulty of the rule.** The setter is declared and defined under the
    overlay source tree today -- a declaration in `AeroraFreshness.h` and an out-of-line definition
    in `AeroraFreshness.cc` -- and every call in the tree is in a suite. A rule that searched for
    the name, or for the name and its parenthesis, would reject both of those and fail on a clean
    tree, and a gate that fails on a clean tree is removed rather than obeyed.

    The classification reads the statement backwards from the name:

    - a member access (``.`` or ``->``) immediately before it is a call through an object, which is
      how every call in the tree today is written;
    - otherwise a qualifier (``Class::``) is dropped, because an out-of-line definition and a static
      call both carry one and what divides them is what precedes it;
    - what remains is a declarator head if it ends in a type -- an identifier, ``>``, ``&``, ``*``
      or the ``]`` of an attribute -- which a declaration and an out-of-line definition do and an
      expression does not;
    - an expression keyword ending in an identifier is read as an expression rather than as a type.

    It is a text classification and not a parse, and the bound is worth stating: an expression whose
    last operator is ``*`` or ``&`` immediately before the name would read as a declarator, and a
    macro expanding to a call would read as whatever it is spelled as. What the rule is for is the
    call site a commit adds, which is written the way the tree writes one.
    """
    line = code[code.rfind("\n", 0, start) + 1 : start]
    begins = max((line.rfind(char) for char in _STATEMENT_START), default=-1)
    prefix = line[begins + 1 :]
    if _MEMBER_ACCESS.search(prefix):
        return True

    head = _QUALIFIER.sub("", prefix)
    keyword = _LAST_WORD.search(head)
    if keyword is not None and keyword.group().strip() in _EXPRESSION_KEYWORDS:
        return True
    return _DECLARATOR.search(head) is None


def _deferred_literal(gates: tuple[str, ...], premises: Premises) -> list[Finding]:
    """The registry covers the guard whose literal this rule hands to ``guard-value``.

    The task's two halves divide by subject rather than by duplication, which is the discipline
    this spec has settled repeatedly -- ``check_partition`` leaves inventory coverage to
    ``inventory-coverage``, ``closure-signatures`` leaves an amendment's own signatures to
    ``closure-amendments``, and ``adr-016-pair`` leaves the guard's value to ``guard-value``.
    `AeroraPowerModel::kDefinedThresholdCount` is a recorded guard, so ``guard-value`` already reads
    its declaration against the recorded literal, in the one file the register names, with the
    disagreement, the three-value case and the renamed declaration all reported there. Reading it
    again here would be a second answer to one question, and the day the two disagreed a reviewer
    would have to work out which rule owned it.

    What a deferral owes is that it lands somewhere, so the one premise this rule does read is that
    the registry records the guard at all. Without that entry ``guard-value`` reads nothing and the
    deferral would be a silence.
    """
    if any(guard.symbol == THRESHOLD_COUNT_GUARD for guard in premises.register.guards):
        return []
    return [
        Finding(
            FRESHNESS_THRESHOLD,
            gates,
            f"{THRESHOLD_COUNT_GUARD}: the guard registry records no entry for it, so the literal "
            f"this rule defers to {GUARD_VALUE} is compared against nothing. That the registry "
            f"covers the guard is {GUARD_REGISTRY}'s finding in the data half; what is reported "
            f"here is the deferral landing nowhere, because a count nothing pins is a threshold "
            f"that can be defined without the Gate noticing",
        )
    ]


def check_freshness_threshold(premises: Premises) -> list[Finding]:
    """No threshold call site is under the overlay source tree while the Gate is open.

    Requirement 8 criterion 13 rejects a change introducing a threshold value for a freshness class
    while `DG-FRS` holds no accepted Closure_Record for it, and criterion 16 holds the Gate at
    `OPEN`. A call to the setter is where a value is introduced: criterion 7 resolves every
    evaluation to the unevaluated outcome while the Gate is open, which holds exactly while no class
    carries a threshold.

    The value `kDefinedThresholdCount` holds is ``guard-value``'s, deferred rather than re-read --
    see :func:`_deferred_literal` for why, and for the one premise the deferral owes.

    **No finding prints an argument.** The class a call site names and the threshold it passes are
    both Decision_Content, and a threshold value is a Contract_Value outright, so a finding names
    the file and the line and a reviewer reads the call. That is the bound ``mavlink-identifier``
    works under for the same reason: a rule that printed what it found would make its own output the
    place a Contract_Value is written down.
    """
    gates, findings = _standing(premises, FRESHNESS_THRESHOLD)
    if findings or not gates:
        return findings

    subject = f"a {THRESHOLD_SETTER}( call site"
    tree = _source_tree(premises.root)
    findings = _deferred_literal(gates, premises)
    findings.extend(_unsearched(tree, FRESHNESS_THRESHOLD, gates, subject))
    for path, text in tree.sources:
        code = _code(text)
        called = [
            found.start()
            for found in _THRESHOLD_CALL.finditer(code)
            if _is_call(code, found.start())
        ]
        if not called:
            continue
        further = "" if len(called) == 1 else f" and at {len(called) - 1} further site(s)"
        findings.append(
            Finding(
                FRESHNESS_THRESHOLD,
                gates,
                f"{path}: calls {THRESHOLD_SETTER}() at line {_line_of(code, called[0])}"
                f"{further}, so a "
                f"threshold value is introduced where the register holds no accepted "
                f"Closure_Record for the class. Requirement 8 criterion 13 rejects the change "
                f"before it enters the repository and criterion 16 holds the Gate at "
                f"{OPEN_STATUS}. The declaration and the out-of-line definition of the setter are "
                f"not call sites and are not reported; the suites that configure a fixture are "
                f"outside {SOURCE_TREE}/",
            )
        )
    return findings


# ---------------------------------------------------------------------------
# payload-command-sink
# ---------------------------------------------------------------------------

# The interface whose implementation is the subject, and what an implementation is: a class deriving
# from it. The type name is not a Contract_Value -- requirements.md names it, and what an
# implementation would carry is the payload Decision_Content the Gate withholds.
PAYLOAD_COMMAND_SINK_TYPE: Final = "AeroraPayloadCommandSink"

# A class head whose base-class list names the type. Neither the brace nor a semicolon may fall
# between the `class` keyword and the name, which is what keeps the four legitimate mentions under
# the overlay source tree out: the interface's own declaration carries no base clause, the pointer
# member and the setter parameter are inside a class body the brace has already opened, and a
# forward declaration ends at its semicolon. The doc comments naming the type are blanked first.
_SINK_DERIVATION: Final = re.compile(
    rf"\b(?:class|struct)\b[^;{{}}]*:\s*[^;{{}}]*\b{PAYLOAD_COMMAND_SINK_TYPE}\b"
)


def check_payload_command_sink(premises: Premises) -> list[Finding]:
    """No implementation of the payload command sink is under the overlay source tree.

    Requirement 10 criterion 18 rejects a change implementing it while `DG-PAY` or the read-only
    Gate is open, names each of those two Gates that is open, and holds each named Gate at `OPEN`.
    Two decisions hold it: the payload Decision_Content a request would have to carry, and the
    authorisation scope a command path would have to be evaluated under. So the rule is in force
    while either is open and the finding names the ones that are, which is why its Gates are read
    as a set rather than as one Gate.

    **An implementation is a derivation.** The interface is declared under the tree deliberately --
    unimplemented is its recorded state, not an omission -- and the overlay holds the type's name in
    a pointer member, a setter parameter, three doc comments, a module manifest and a README. None
    of those is an implementation, and a rule that searched for the name would reject the interface
    the Gate exists to leave unimplemented. The one implementation in the tree is a recording double
    in `aerora/test/Payload/AeroraPayloadModelTest.cc`, whose own comment records it as the
    instrument the prohibition tests need, and it is outside this rule's tree for that reason.

    The other half of criterion 18 -- a change altering a guard criterion 17 records -- is
    ``guard-value``'s, which reads each of those five guards against its recorded literal, and
    ``guard-registry``'s, which derives whether an acceptance permitted one to move. This rule reads
    no guard.
    """
    gates, findings = _standing(premises, PAYLOAD_COMMAND_SINK)
    if findings or not gates:
        return findings

    subject = f"an implementation of {PAYLOAD_COMMAND_SINK_TYPE}"
    tree = _source_tree(premises.root)
    findings = _unsearched(tree, PAYLOAD_COMMAND_SINK, gates, subject)
    for path, text in tree.sources:
        code = _code(text)
        findings.extend(
            Finding(
                PAYLOAD_COMMAND_SINK,
                gates,
                f"{path}: derives from {PAYLOAD_COMMAND_SINK_TYPE} at line "
                f"{_line_of(code, derived.start())}, which implements the one outbound payload "
                f"seam while the register holds no accepted Closure_Record for the Gates named "
                f"here. Requirement 10 criterion 18 rejects the change before it enters the "
                f"repository and holds each named Gate at {OPEN_STATUS}. The interface's own "
                f"declaration, a pointer member, a parameter and a doc comment are not "
                f"implementations and are not reported",
            )
            # One finding per derivation rather than per file: two classes deriving from the
            # interface are two implementations, and a reviewer withdrawing them needs both lines.
            for derived in _SINK_DERIVATION.finditer(code)
        )
    return findings


# ---------------------------------------------------------------------------
# actionable-control
# ---------------------------------------------------------------------------

_QML_SUFFIX: Final = ".qml"

# A QML object declaration: a capitalised type name and the brace that opens its body. Comments and
# string literals are blanked before this runs, so a type named in prose or inside a quoted path is
# not a declaration.
#
# The brace has to belong to the name. `Component.onCompleted: {`, `function f() {` and
# `onClicked: {` all put a character between the identifier and the brace that is not whitespace, so
# none of them matches, and a lower-case first letter excludes every property and signal handler.
_QML_DECLARATION: Final = re.compile(r"\b(?P<type>[A-Z][A-Za-z0-9_]*)\s*\{")

# What makes a declared type actionable, matched by the shape of its *name* rather than against a
# list of types. The runtime walk in `AeroraUiFoundationTest` asks `QObject::inherits()` for
# `QQuickAbstractButton`, `QQuickMouseArea` and `QQuickPointerHandler`; none of those three has a
# QML spelling, and a text rule cannot resolve a QML type to its C++ base. So this matches the
# naming convention the three families share, which is what lets the rule see a type nobody here
# thought to name -- `QGCButton`, `Button`, `ToolButton` and `SwitchDelegate` all end in a token
# below, and so does a control added next year.
#
# A hand-maintained list of types is the failure mode this repository keeps recording:
# `aerora-ui-static-check.ps1` skipped a file for a whole pass because its target list was written
# out, and the design's own note on the runtime walk gives naming the types as the reason the
# `inherits()` approach was kept. Suffixes are that approach in text.
#
# `Flickable` is deliberately absent. `QGCFlickable` carries the hub's report and upstream's own
# `QQuickFlickable` is not one of the three bases the runtime walk names, so a rule that rejected it
# would reject scrolling rather than action.
_ACTIONABLE_SUFFIXES: Final = (
    "Button",
    "CheckBox",
    "ComboBox",
    "Delegate",
    "Handler",
    "MouseArea",
    "Slider",
    "SpinBox",
    "Switch",
    "TextArea",
    "TextField",
)

# The `objectName` a declaration sets, which is the identity a recorded site pins. Read at the depth
# of the declaration's own body, so a control takes its own name rather than a child's.
_OBJECT_NAME: Final = re.compile(r'\bobjectName\s*:\s*"(?P<name>[^"\n]*)"')

# How a finding refers to a control that sets no `objectName`. Such a control cannot be recorded --
# a site pins an objectName, so there is nothing for the register to hold it by -- and that is the
# reason it is rejected rather than a technicality.
_UNNAMED: Final = "<no objectName>"


@dataclass(frozen=True)
class _Control:
    """One actionable control a QML document declares: what it is, where, and what it handles."""

    file: str
    control: str
    type_name: str
    line: int
    body: str


@dataclass(frozen=True)
class _QmlTree:
    """The Aerora QML documents as this rule reads them, on :class:`_SourceTree`'s own terms."""

    walked: bool
    documents: tuple[tuple[str, str], ...]
    unreadable: tuple[tuple[str, str], ...]


def _qml_tree(root: Path) -> _QmlTree:
    """Read every Aerora QML document, in path order.

    Scoped to the overlay source rather than the whole overlay, for the reason
    :data:`SOURCE_TREE` is: `aerora/test/**` is where the instrument proving a prohibition lives, so
    a fixture document written to show that the runtime walk catches a control must not be rejected
    by the rule it exercises.
    """
    tree = root / SOURCE_TREE
    if not tree.is_dir():
        return _QmlTree(walked=False, documents=(), unreadable=())

    documents: list[tuple[str, str]] = []
    unreadable: list[tuple[str, str]] = []
    for path in sorted(tree.rglob(f"*{_QML_SUFFIX}")):
        relative = path.relative_to(root)
        if not path.is_file() or should_skip_path(relative, _SKIP_DIRS):
            continue
        text, cannot_read = _searchable(root, relative.as_posix())
        if text is None:
            unreadable.append((relative.as_posix(), cannot_read))
            continue
        documents.append((relative.as_posix(), text))
    return _QmlTree(walked=True, documents=tuple(documents), unreadable=tuple(unreadable))


def _is_actionable(type_name: str) -> bool:
    """Whether *type_name* names one of the three families the runtime walk asks `inherits()` for."""
    return type_name.endswith(_ACTIONABLE_SUFFIXES)


def _own_object_name(body: str) -> str:
    """The `objectName` *body* sets on itself, or :data:`_UNNAMED` where it sets none.

    *body* starts at the declaration's opening brace. A match is its own only at depth one: a child
    declaration opens another brace, and the first `objectName` inside that child is the child's.
    Without the depth test a panel containing a named label would report the label's name, and a
    recorded site would pin the wrong object.
    """
    depth = 0
    for index, character in enumerate(body):
        if character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
            if depth == 0:
                break
        elif depth == 1 and body.startswith("objectName", index):
            named = _OBJECT_NAME.match(body, index)
            if named is not None:
                return named.group("name")
    return _UNNAMED


def _controls(tree: _QmlTree) -> list[_Control]:
    """Every actionable control the documents declare, in document and then declaration order."""
    found: list[_Control] = []
    for path, text in tree.documents:
        # Comments out and strings out, on the terms the brace-reading rules use them: a type named
        # in a doc comment is not a declaration, and `objectName` is read back off the text that
        # keeps its literals.
        code = _code(text)
        scanned = _without_strings(code)
        for declared in _QML_DECLARATION.finditer(scanned):
            type_name = declared.group("type")
            if not _is_actionable(type_name):
                continue
            opening = declared.end() - 1
            closing = _balanced(scanned, opening)
            body = code[opening : len(code) if closing is None else closing + 1]
            found.append(
                _Control(
                    file=path,
                    control=_own_object_name(body),
                    type_name=type_name,
                    line=_line_of(code, declared.start()),
                    body=body,
                )
            )
    return found


def check_actionable_control(premises: Premises) -> list[Finding]:
    """The actionable controls the Aerora QML documents declare are the ones the register records.

    Requirement 5 criterion 2 rejects a change adding a control criterion 1 forbids and names the
    read-only Gate. Criterion 1 forbids a control "whose activation reaches an outbound command
    path, a mission upload, start or resume path, or a call to `AeroraMissionObserver::verify()`",
    and records the clipboard copy on `SCR-HLP-03` as the single exception to read-only
    presentation.

    **Reachability is not decidable from QML text, so the rule decides by type and the register
    carries the judgement.** That is the move the design makes for the contract surface and the one
    `AeroraUiSafetyTest` makes for the QML-reachable method set: an undecidable question becomes the
    decidable one of whether the surface changed, and a reviewer classifies once. Here the decidable
    question is set equality -- the actionable controls in source against the controls the register
    records -- and every consequence the design asks for falls out of it rather than needing a rule
    of its own: a new control anywhere is unrecorded; a second control in the exception's own
    document is unrecorded, so "exactly one" needs no count; a removed control is recorded and
    absent; and an `objectName` that drifts is both at once.

    **Two recorded lists, and only one of them is an exception.** `readOnly.exceptions` is the single
    exception to read-only presentation, which the schema pins to one entry on one screen and which
    criterion 11 protects. `readOnly.actionableControlSites` is every other document permitted to
    declare a control, each carrying an owner's statement that this control reaches none of the paths
    criterion 1 names. The second list is not a way to add an exception: a control that *does* reach
    a forbidden path cannot be recorded in it, because `reachesNoForbiddenPath` is `const true` in
    the schema and is rejected here as well, the way `guard-value` rejects a disagreement carrying no
    handed-on row -- a register reaching a rule as a model has not passed the schema.

    Without that second list the rule would be a cliff rather than a ratchet. The tree carries two
    actionable controls today: the clipboard copy the exception records, and
    `aeroraHub_vehicleConfigButton`, whose handler is `mainWindow.showVehicleConfig()` -- a local
    view switch through `MainWindow.qml`, reaching no command path, no mission path and no
    `verify()`. A rule admitting only the exception would reject the tree on every commit, and the
    only ways to satisfy it would be to delete a control that `SCR-SHL-01`'s `VERIFIED` status rests
    on or to record a second exception the schema cannot hold.

    **A recorded site pins the handler the owner reviewed.** `reachesNoForbiddenPath` is a claim
    about an expression, so the expression is held still: a control whose handler stops matching has
    not been reviewed in its new form and is rejected naming both. That is what stops the list
    becoming the escape hatch -- recording a navigation button does not license turning it into a
    command path later.

    A control setting no `objectName` is rejected whether or not its document is recorded, because a
    site pins an objectName and there is nothing to record such a control by.

    The rule walks for controls. It adds none, and it reads every document and writes none.
    """
    gates, findings = _standing(premises, ACTIONABLE_CONTROL)
    if findings or not gates:
        return findings

    posture = premises.register.read_only
    tree = _qml_tree(premises.root)
    subject = "an actionable control"
    findings = []
    if not tree.walked:
        return [
            Finding(
                ACTIONABLE_CONTROL,
                gates,
                f"{SOURCE_TREE}/ is not a directory this rule could walk, so no document was "
                f"searched for {subject}. A run that searched nothing is a failure rather than a "
                f"pass: it is indistinguishable from a run that found nothing",
            )
        ]
    findings.extend(
        Finding(
            ACTIONABLE_CONTROL,
            gates,
            f"{path}: the document {reason}, so it was searched for {subject} nowhere. A document "
            f"under {SOURCE_TREE}/ that this rule cannot read is a failure rather than a pass",
        )
        for path, reason in tree.unreadable
    )

    # The exception and the other recorded sites, keyed alike, because set equality is over controls
    # and a control is a document and an objectName. Which list an entry came from decides what else
    # is checked about it and how a finding describes it, never whether it is permitted.
    recorded = {(site.file, site.control): site for site in posture.actionable_control_sites}
    excepted = {(item.file, item.control): item for item in posture.exceptions}
    found = _controls(tree)

    for control in found:
        key = (control.file, control.control)
        if control.control == _UNNAMED:
            findings.append(
                Finding(
                    ACTIONABLE_CONTROL,
                    gates,
                    f"{control.file}: declares {control.type_name} at line {control.line} setting "
                    f"no objectName. A recorded site pins a control by its objectName, so a control "
                    f"without one cannot be recorded and cannot be told apart from another in the "
                    f"same document. Requirement 5 criterion 2 rejects the change before it enters "
                    f"the repository",
                )
            )
            continue
        if key in excepted:
            continue
        site = recorded.get(key)
        if site is None:
            findings.append(
                Finding(
                    ACTIONABLE_CONTROL,
                    gates,
                    f"{control.file}: declares {control.type_name} {control.control} at line "
                    f"{control.line}, and the register records neither the single exception to "
                    f"read-only presentation nor a site for it. Requirement 5 criterion 2 rejects "
                    f"the change before it enters the repository. Either the control reaches a path "
                    f"criterion 1 forbids, in which case it waits on this Gate, or it reaches none, "
                    f"in which case an owner records it with the handler they reviewed and the "
                    f"reason it reaches none",
                )
            )
            continue
        if not site.reaches_no_forbidden_path:
            findings.append(
                Finding(
                    ACTIONABLE_CONTROL,
                    gates,
                    f"{control.file}: the recorded site for {control.control} does not state that "
                    f"the control reaches no path criterion 1 forbids. The schema writes that claim "
                    f"as a constant, so a register carrying it unset has not passed the schema; a "
                    f"control that does reach one is not recordable here at all",
                )
            )
        if site.handler not in control.body:
            findings.append(
                Finding(
                    ACTIONABLE_CONTROL,
                    gates,
                    f"{control.file}: {control.control} at line {control.line} no longer carries "
                    f"the handler the register records for it, {site.handler}. The recorded reason "
                    f'"{site.reason}" was an owner\'s statement about that expression, so a control '
                    f"whose handler has changed has not been reviewed in its new form. Requirement "
                    f"5 criterion 2 rejects the change before it enters the repository",
                )
            )

    present = {(control.file, control.control) for control in found}
    findings.extend(
        Finding(
            ACTIONABLE_CONTROL,
            gates,
            f"{file}: the register records {control} as {described}, and the document declares no "
            f"actionable control of that name. A record naming a control the tree does not carry is "
            f'stale rather than satisfied: "not found" is not "withdrawn", and a reviewer reading '
            f"the register would be told a control is accounted for that nothing accounts for",
        )
        for (file, control), described in sorted(
            [(key, "the single exception to read-only presentation") for key in excepted]
            + [(key, "a site reaching no forbidden path") for key in recorded]
        )
        if (file, control) not in present
    )
    return findings


# ---------------------------------------------------------------------------
# contract-surface
# ---------------------------------------------------------------------------

# The three element categories the design records, which are the `kind` values the register carries.
# The schema leaves `kind` a free string, so these are the vocabulary: a recorded element whose kind
# is none of them matches nothing in source and is reported as a removal, which is the right answer
# for a kind nothing extracts.
ENUMERATOR_KIND: Final = "enumerator"
CONSTANT_KIND: Final = "constant"
REGISTRY_KEY_KIND: Final = "registryKey"

# A class, struct or namespace body, which is what qualifies a symbol. Four constants in this tree
# are called `kServiceName` and they are four different elements, so an unqualified inventory could
# not tell them apart and a rule reading one would report three removals.
_SCOPE: Final = re.compile(
    r"\b(?:class|struct|namespace)\s+([A-Za-z_]\w*)(?:\s+final)?\s*(?::[^{;]*?)?\{"
)

# The `enum` keyword immediately before a scope match, which is how `enum class X {` is kept from
# reading as a class body. Written as a search over the preceding text rather than as a lookbehind,
# because the keyword and the name are separated by a run of whitespace of no fixed width.
_ENUM_PREFIX: Final = re.compile(r"\benum\s+$")

# An enum body, scoped or unscoped. The name is optional because an unscoped anonymous enum is legal
# and its enumerators land in the enclosing scope, which is where this then qualifies them.
_ENUM: Final = re.compile(
    r"\benum\s+(?:class|struct)\s+([A-Za-z_]\w*)[^{;]*?\{|\benum\s+([A-Za-z_]\w*)?\s*(?::[^{;]*)?\{"
)

# A `constexpr` or `static constexpr` declaration with an initialiser. The declarator may carry no
# parenthesis, which is what keeps a `constexpr` *function* out: a function's parameter list falls
# between the keyword and the `=` or brace, and a value's initialiser falls after it.
_CONSTANT: Final = re.compile(
    r"\b(?:static\s+)?constexpr\b([^;{}()=]*?)\b([A-Za-z_]\w*)\s*(?:=|\{)"
)

# An enumerator, once its `= value` tail is taken off. Anything else in a comma-separated enum body
# -- a trailing comma's empty tail, an initialiser naming another enumerator -- is not a declaration.
_ENUMERATOR: Final = re.compile(r"[A-Za-z_]\w*\Z")

# How far past a declaration's `=` the initialiser is looked for. A declaration and its literal sit
# on one line or wrap once, and reading to end of file would let an unrelated literal downstream
# decide a constant's kind.
_INITIALISER_WINDOW: Final = 120


@dataclass(frozen=True)
class _Element:
    """One contract-surface element as source declares it: its kind, its name, and where."""

    kind: str
    symbol: str
    file: str
    line: int


def _scopes(scanned: str) -> list[tuple[int, int, str]]:
    """Every named class, struct or namespace body in *scanned*, as a span and a name."""
    found: list[tuple[int, int, str]] = []
    for scope in _SCOPE.finditer(scanned):
        if _ENUM_PREFIX.search(scanned[: scope.start()]):
            continue
        closing = _balanced(scanned, scope.end() - 1)
        found.append((scope.end(), len(scanned) if closing is None else closing, scope.group(1)))
    return found


def _qualified(scopes: Sequence[tuple[int, int, str]], offset: int, *names: str) -> str:
    """*names* qualified by every scope of *scopes* enclosing *offset*, innermost last.

    A file-scope or anonymous-namespace element qualifies to its bare name, which is the name C++
    gives it. Two files declaring the same bare name are two elements one inventory entry cannot
    tell apart, so :func:`_surface` reports that rather than merging them.
    """
    enclosing = [name for start, end, name in scopes if start <= offset < end]
    return "::".join([*enclosing, *(name for name in names if name)])


def _surface(tree: _SourceTree) -> tuple[list[_Element], list[_Element]]:
    """Every contract-surface element the tree declares, and those declared more than once.

    An element is identified by its kind and its qualified symbol, which is what the register
    records. A second declaration of the same pair is returned separately because the inventory has
    one entry for it: reporting it as a collision is honest, where silently merging would let a
    second declaration of a recorded symbol enter unnoticed.
    """
    found: dict[tuple[str, str], _Element] = {}
    collisions: list[_Element] = []

    def record(element: _Element) -> None:
        key = (element.kind, element.symbol)
        if key in found:
            collisions.append(element)
            return
        found[key] = element

    for path, text in tree.sources:
        code = _code(text)
        scanned = _without_strings(code)
        scopes = _scopes(scanned)
        for enum in _ENUM.finditer(scanned):
            # Only a *scoped* enum's name qualifies its enumerators. An unscoped enum's members land
            # in the enclosing scope -- `AFixtureHolder::UnscopedOnly`, not
            # `AFixtureHolder::Unscoped::UnscopedOnly` -- which is what C++ does and therefore what
            # an inventory recording C++ qualified names has to record. Group one is the scoped
            # alternative's name; group two is the unscoped alternative's and is a scope of nothing.
            name = enum.group(1) or ""
            closing = _balanced(scanned, enum.end() - 1)
            body = scanned[enum.end() : len(scanned) if closing is None else closing]
            line = _line_of(code, enum.start())
            for item in body.split(","):
                declared = _ENUMERATOR.search(item.split("=")[0].strip())
                if declared is None:
                    continue
                symbol = _qualified(scopes, enum.start(), name, declared.group())
                record(_Element(ENUMERATOR_KIND, symbol, path, line))
        for constant in _CONSTANT.finditer(scanned):
            declarator, name = constant.group(1), constant.group(2)
            # A char-pointer constant whose initialiser is a string literal is a registry key, which
            # is read off the text that kept its literals. That is the one shape the five real key
            # sites share and the shape no log string has: a message is a `QStringLiteral` at its
            # call site, never a named `constexpr` declaration.
            initialiser = code[constant.end() :][:_INITIALISER_WINDOW].lstrip()
            key = "*" in declarator and initialiser.startswith('"')
            record(
                _Element(
                    REGISTRY_KEY_KIND if key else CONSTANT_KIND,
                    _qualified(scopes, constant.start(), name),
                    path,
                    _line_of(code, constant.start()),
                )
            )
    return list(found.values()), collisions


def check_contract_surface(premises: Premises) -> list[Finding]:
    """The contract surface in source and in the register agree in both directions.

    **What this honestly does, and what it does not.** "Reject a change that introduces a
    Contract_Value" is Requirement 3 criterion 12, Requirement 4 criterion 2 and one criterion per
    Gate, and as written it is undecidable: no checker can look at a new enumerator or a new registry
    key and know whether it is an Aerora value some Gate settles or an identifier with no contract
    behind it. So this rule answers the decidable question instead -- *did the surface change* -- and
    a reviewer classifies each element once, in the register.

    Three limitations, stated rather than papered over:

    * it establishes that the surface did not change, **not** that no value was invented;
    * it cannot see a *value* moving inside an element it already knows, because the register
      records a symbol and a kind and no literal;
    * it cannot classify. An element arriving unmapped is reported as unassigned Decision_Content
      for an owner to map; the rule has no opinion on which Gate.

    That is the move `AeroraUiSafetyTest` makes for the QML-reachable method set: it cannot judge
    whether a new method is safe, so it fails on *any* change and makes a human record the judgement
    once. "Not looked at" fails rather than passes.

    **Three findings, which are the three ways the two sets can disagree.** An element in source and
    absent from the inventory fails naming the symbol, with no Gate named because none is recorded --
    that is Requirement 4 criterion 2's "name the element as unassigned Decision_Content", and it
    renders as :data:`UNASSIGNED`. An element mapped to a Gate the derivation reports `OPEN` fails
    naming both, because an element of a Gate's Decision_Content standing in source is the value the
    Gate exists to withhold. An element in the inventory and absent from source fails as a removal,
    so the surface cannot shrink silently either.

    **This rule is not Gate-conditional.** The other five read one Gate's openness and stand down
    when it closes; this one reads the Gate *each element maps to*, so it runs whatever the register
    records and a closing Gate frees its own elements and nothing else.

    Symbols are qualified by their enclosing class, struct and namespace scopes. Four constants in
    this tree are called `kServiceName`, so an unqualified inventory could not tell them apart and a
    rule reading one of them would report the other three as removals.
    """
    openness = premises.derived.gate_open
    tree = _source_tree(premises.root)
    subject = "a contract-surface element"
    if not tree.walked:
        return [
            Finding(
                CONTRACT_SURFACE,
                GATE_IDS,
                f"{SOURCE_TREE}/ is not a directory this rule could walk, so no file was searched "
                f"for {subject}. A run that searched nothing is a failure rather than a pass: an "
                f"inventory compared against nothing reports every element as removed",
            )
        ]

    findings = [
        Finding(
            CONTRACT_SURFACE,
            GATE_IDS,
            f"{path}: the file {reason}, so it was searched for {subject} nowhere. A file under "
            f"{SOURCE_TREE}/ that this rule cannot read is a failure rather than a pass",
        )
        for path, reason in tree.unreadable
    ]

    declared, collisions = _surface(tree)
    findings.extend(
        Finding(
            CONTRACT_SURFACE,
            (),
            f"{element.file}: declares {element.kind} {element.symbol} at line {element.line}, "
            f"which is already declared elsewhere under {SOURCE_TREE}/. The register records one "
            f"entry per kind and symbol, so two declarations of one pair are two elements the "
            f"inventory cannot tell apart. Qualify one of them, or record them as distinct symbols",
        )
        for element in collisions
    )

    recorded = {
        (element.kind, element.symbol): element for element in premises.register.contract_surface
    }
    present = {(element.kind, element.symbol): element for element in declared}

    for key, element in sorted(present.items()):
        mapped = recorded.get(key)
        if mapped is None:
            findings.append(
                Finding(
                    CONTRACT_SURFACE,
                    (),
                    f"{element.file}: declares {element.kind} {element.symbol} at line "
                    f"{element.line}, and the register's contract surface records no entry for it. "
                    f"Requirement 4 criterion 2 names such an element as unassigned "
                    f"Decision_Content: a reviewer maps it to the Gate whose Decision_Content would "
                    f"record it, or to {NO_GATE} with the reason no Gate does. This rule cannot "
                    f"classify it and does not guess",
                )
            )
            continue
        if mapped.gate == NO_GATE:
            if not mapped.reason:
                findings.append(
                    Finding(
                        CONTRACT_SURFACE,
                        (),
                        f"the register maps {mapped.kind} {mapped.symbol} to {NO_GATE} and records "
                        f"no reason. The schema requires one beside it, so a register carrying none "
                        f"has not passed the schema: an element mapped to no Gate without a reason "
                        f"is an element nobody classified rather than one somebody cleared",
                    )
                )
            continue
        if mapped.gate not in openness:
            findings.append(
                Finding(
                    CONTRACT_SURFACE,
                    (mapped.gate,),
                    f"the register maps {mapped.kind} {mapped.symbol} to a Gate it records no "
                    f"Gate_Dossier for, so whether that Gate is open could not be derived and the "
                    f'element was checked against nothing: "not found" is not "closed"',
                )
            )
            continue
        if openness[mapped.gate].is_open:
            findings.append(
                Finding(
                    CONTRACT_SURFACE,
                    (mapped.gate,),
                    f"{element.file}: declares {element.kind} {element.symbol} at line "
                    f"{element.line}, and the register maps it to a Gate holding no accepted "
                    f"Closure_Record. An element of a Gate's Decision_Content standing in source is "
                    f"the value the Gate exists to withhold, so it waits on the signature rather "
                    f"than on a review of this file",
                )
            )

    findings.extend(
        Finding(
            CONTRACT_SURFACE,
            () if mapped.gate == NO_GATE else (mapped.gate,),
            f"the register's contract surface records {mapped.kind} {mapped.symbol}, and no file "
            f"under {SOURCE_TREE}/ declares it. A recorded element the tree no longer carries is a "
            f"removal rather than a satisfied entry, so the surface cannot shrink silently either: "
            f"withdraw the entry in the change that withdraws the declaration",
        )
        for key, mapped in sorted(recorded.items())
        if key not in present
    )
    return findings


# ---------------------------------------------------------------------------
# The run
# ---------------------------------------------------------------------------

# The rules, in the order the design's table lists them. Each reports findings carrying its own
# check name, so the registry needs no name of its own.
_CHECKS: Final = (
    check_guard_value,
    check_guard_assertion,
    check_adr_016_pair,
    check_mavlink_identifier,
    check_populated_file_override,
    check_action_seam,
    check_freshness_threshold,
    check_payload_command_sink,
    check_actionable_control,
    check_contract_surface,
)


def run_checks(premises: Premises) -> list[Finding]:
    """Run every rule over established premises."""
    return [finding for check in _CHECKS for finding in check(premises)]


def _established(premises: Premises, files: int) -> str:
    """What the run established, measured here and now rather than remembered.

    It states the premise and what was compared against it, and says that the selection is not what
    decided any of it: the guard rules read the files the register names, `adr-016-pair` reads the
    two files Requirement 7 criterion 17 names, and `mavlink-identifier` walks the overlay tree, so
    a run claiming to have reviewed the selection would be claiming the wrong thing -- including the
    run that selects one of those two files, which is read because the criterion names it and not
    because it was given.

    The baseline is reported as a measurement for the same reason: the identifiers the allow-listed
    sites hold are read out of the tree on every run and recorded nowhere, so an allow-list whose
    sites have stopped holding any is visible as a number rather than as a silence.

    The Gate-conditional rules are counted for that reason too, and it is the sharper case. Each is
    conditional on a Gate the register records, so a rule that is not in force reports nothing --
    and a run in which five rules reported nothing because their Gates closed must not read as a run
    in which five rules looked and found nothing.

    The recorded control surface is stated beside them, because `actionable-control` compares two
    sets and a run that found none on either side is a different answer from a run over a register
    recording none: the first is a tree with no control in it, the second is a register that has
    stopped accounting for one. The contract surface is stated on both sides for the same reason,
    and because its two numbers are the one measurement that says the ratchet is loaded: an
    inventory compared against an extractor that has stopped extracting reports every element as
    removed, and the pair of counts is where that is visible.
    """
    guards = _registry(premises)
    sites = sum(len(guard.asserted_in) for guard in guards)
    posture = premises.register.mavlink_identifiers
    baseline = _baseline(premises.root, posture)
    in_force = _in_force(premises)
    read_only = premises.register.read_only
    controls = len(read_only.exceptions) + len(read_only.actionable_control_sites)
    declared, _ = _surface(_source_tree(premises.root))
    surface = len(declared)
    return (
        f"{REGISTER_PATH} records {len(premises.register.guards)} guards over "
        f"{len(premises.register.gates)} Gate_Dossiers; {len(guards)} guard values and {sites} "
        f"assertion sites compared against the files the register names, and the ADR-016 pair "
        f"against {CORE_HEADER} and {OVERLAY_MANIFEST}; every file under {OVERLAY_TREE}/ searched "
        f"against {len(posture.allow_list)} allow-listed sites holding "
        f"{len(baseline.identifiers)} identifiers; {in_force} of the "
        f"{len(CONDITIONAL_GATES)} Gate-conditional rules in force over {SOURCE_TREE}/ and "
        f"{len(CONDITIONAL_GATES) - in_force} standing down on a Gate the derivation records "
        f"closed, with {controls} actionable controls recorded; {surface} contract-surface elements "
        f"declared under {SOURCE_TREE}/ against {len(premises.register.contract_surface)} recorded; "
        f"no rule read a file because it was among the {files} selected source files\n"
    )


def _result(findings: Sequence[Finding], *, files: int, established: str = "") -> AnalysisResult:
    """The findings in the result shape the dispatcher reads.

    ``passed`` is the exit status and nothing else: it says that no finding blocks, not that none
    was found. ``issues`` is every finding, blocking or reported. Keeping them apart is what lets a
    matched disagreement print on every run and still exit zero without the run looking clean --
    ``passed`` with a non-zero ``issues`` is exactly that state, and the marked line says which
    finding it was.
    """
    blocking = [finding for finding in findings if finding.blocking]
    return AnalysisResult(
        tool=NAME,
        passed=not blocking,
        issues=len(findings),
        output="".join(f"{finding}\n" for finding in findings) + established,
        files_checked=files,
    )


def review(root: Path, files: Sequence[Path]) -> AnalysisResult:
    """Review *files* against the register at *root*, in the result shape the dispatcher reads.

    The premise comes first and a failure stops the run there, which is the order the data half
    establishes its premises in and for the same reason: a rule applied to a register that could
    not be read reports noise that hides the real error, and reports it against the wrong document.
    """
    premises = load_premises(root)
    if isinstance(premises, list):
        return _result(premise_findings(premises), files=len(files))

    return _result(
        run_checks(premises), files=len(files), established=_established(premises, len(files))
    )


class GateGuardCheckAnalyzer(AnalyzerBase):
    """Check the Aerora source tree against the guards and the surface the register records."""

    name: ClassVar[str] = NAME
    install_hint: ClassVar[str] = ""

    def run(self, files: list[Path], fix: bool = False) -> AnalysisResult:
        # There is no auto-fix here, so the discard is explicit rather than implied: the only thing
        # to "fix" would be a guard value, an allow-list entry or a surface element that no owner
        # signed, and writing one would be this analyzer forging the signature it exists to demand.
        del fix
        return review(self.repo_root, files)


def main() -> int:
    """Review the paths given as arguments, or the paths read from stdin."""
    args = sys.argv[1:]
    if args:
        paths = [Path(arg) for arg in args]
    else:
        paths = [Path(line.strip()) for line in sys.stdin if line.strip()]

    if not paths:
        # No path is a run with nothing in front of it, which is what `pass_filenames: true` hands
        # the hook when no file it watches changed.
        print("Usage: gate_guard_check.py [files or directories...]", file=sys.stderr)
        return 0

    result = review(ROOT, list(find_cpp_files(paths)))
    print(result.output, end="")
    return 0 if result.passed else 1


if __name__ == "__main__":
    sys.exit(main())

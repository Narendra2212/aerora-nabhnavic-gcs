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
from common.file_traversal import find_cpp_files
from common.gate_register import GATE_IDS
from validate_gate_register import (
    MAVLINK_IDENTIFIER_GUARD,
    OPEN_STATUS,
    REGISTER_PATH,
    TEST_TREE,
    load_premises,
)

if TYPE_CHECKING:
    from collections.abc import Sequence

    from common.gate_register import Disagreement, Guard
    from validate_gate_register import Finding as PremiseFinding
    from validate_gate_register import Premises

# The repository this run reviews, resolved from this file rather than from the working directory,
# so a hook invoking the script from anywhere reads the same register. `run` takes its root from
# the dispatcher instead, which is what lets a test review a tree it laid out itself.
ROOT: Final = Path(__file__).resolve().parents[2]

# The name a finding and the dispatcher's status line carry, which is the hook id as well.
NAME: Final = "gate-guards"

# The two rules this module carries, named as the design's table names them.
GUARD_VALUE: Final = "guard-value"
GUARD_ASSERTION: Final = "guard-assertion"

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

_BLOCK_COMMENT: Final = re.compile(r"/\*.*?\*/", re.S)
_LINE_COMMENT: Final = re.compile(r"//[^\n]*")

# The literal a definition's body returns, read from the first `return` after its opening brace.
# A body whose value depends on a preprocessor condition therefore reads as its first branch, which
# is what `AeroraVehicleFactGroupRegistrar::isRegistrationAvailable()` is: a text comparison cannot
# evaluate `#ifdef`, and the rule that reads the define is `adr-016-pair`.
_RETURN: Final = re.compile(r"\breturn\s+(?P<value>[^;]+);")


def _code(text: str) -> str:
    """*text* with its comments removed, so a comment naming a guard does not declare one.

    The overlay documents every one of these guards in a doc comment beside it, and
    `AeroraSecurityStatus.h` carries ``kRemainingEnergyEstimationImplemented = false`` in prose, so
    a rule that read comments would read a sentence *about* a guard as the guard itself. The removal
    is textual and knows nothing about string literals, which is sound for what it reads here -- a
    declaration and the literal beside it.
    """
    return _LINE_COMMENT.sub("", _BLOCK_COMMENT.sub("", text))


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
# The run
# ---------------------------------------------------------------------------

# The rules, in the order the design's table lists them. Each reports findings carrying its own
# check name, so the registry needs no name of its own.
_CHECKS: Final = (
    check_guard_value,
    check_guard_assertion,
)


def run_checks(premises: Premises) -> list[Finding]:
    """Run every rule over established premises."""
    return [finding for check in _CHECKS for finding in check(premises)]


def _established(premises: Premises, files: int) -> str:
    """What the run established, measured here and now rather than remembered.

    It states the premise and what was compared against it, and says that the selected files are not
    what the guard rules read: both read the files the register names, so a run claiming to have
    reviewed the selection would be claiming the wrong thing.
    """
    guards = _registry(premises)
    sites = sum(len(guard.asserted_in) for guard in guards)
    return (
        f"{REGISTER_PATH} records {len(premises.register.guards)} guards over "
        f"{len(premises.register.gates)} Gate_Dossiers; {len(guards)} guard values and {sites} "
        f"assertion sites compared against the files the register names, and none of the {files} "
        f"selected source files has been read by a rule yet\n"
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

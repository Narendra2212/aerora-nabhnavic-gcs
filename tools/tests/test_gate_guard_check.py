#!/usr/bin/env python3
"""Tests for the shell and the three source-half rules in tools/analyzers/gate_guard_check.py.

Two things are under test, and they are tested differently.

The shell: that it is an `AnalyzerBase` the dispatcher can run, that `fix` is accepted and writes
nothing, that arguments and stdin reach the review as the file list a rule will read, that a
directory is expanded by `find_cpp_files` rather than by something local to the analyzer, and that
the register premise is established before anything else and reported as itself when it cannot be.
The last of those is the behaviour the repository exercises today, because the register does not
exist yet: task 9 writes it, and until it does, every run of the source half fails naming the path.
That is the spec's governing rule rather than an interim inconvenience -- an unresolved premise is a
failure, never a pass -- so it is asserted here as the shell's answer in that state.

The rules: `guard-value`, which reads each recorded guard's declaration in its recorded `declaredIn`
file; `guard-assertion`, which reads each recorded assertion site under `aerora/test/**`;
`adr-016-pair`, which reads the two files Requirement 7 criterion 17 names against the change
permission the registry records on the fact-group registrar's guard; and `mavlink-identifier`, which
walks the overlay tree against an allow-list the register records by site and reason. None of them
reads the files the run selected, so the fixture that decides what a rule sees is not the file list
but the tree.

`adr-016-pair` is tested over both recorded authorisation states and all four states of the pair,
because each combination is a different answer: the applied pair under an unsigned authorisation is
the state this repository stands in and is reported without blocking, the same pair with nothing
pinning it blocks, one half alone blocks under either authorisation, and an authorisation nobody has
acted on is no finding at all. Two of its cases reach the register as a model rather than as a
document, for the reason `_in_code` does: an authorised core edit needs an accepted part whose
Closure_Record records a permitting verdict, which `_accepting` builds in code. One case reads the
repository's own `src/Vehicle/Vehicle.h` and `aerora/CMakeLists.txt`, which is the only evidence
that the two patterns match the files they exist to watch rather than only the fixtures, and it
asserts that both files come back byte for byte -- the rule reads core and never writes it.

The fixture register is the data half's, imported rather than rewritten, and the fixture tree is
derived from it: each guard's declaration carries the literal the register records, and each
recorded site names the guards recorded there. Both halves read one register through one loader, so
a second fixture register here would be a second definition of a register the Gate_Review accepts,
free to drift from the first and to let one half's tests pass over a document the other half
rejects. A tree written out by hand would drift the same way, one document further out -- it would
be a second definition of what the recorded declaration says -- so each rejecting case lays out the
agreeing tree and rewrites the one file whose finding it asserts.

Each rejecting case asserts the finding *and* whether it blocks, because the result shape is where
the design's one concession to being gating lives. `Finding.blocking` defaults to true,
`AnalysisResult.issues` counts every finding and `AnalysisResult.passed` reports only whether one of
them blocks, so a guard standing in its recorded disagreement is `passed` with a non-zero `issues`
and a line carrying the reported marker. A change that quietly made that case block, or made it
vanish, breaks a test here.
"""

from __future__ import annotations

import io
import re
import subprocess
import sys
from dataclasses import replace
from typing import TYPE_CHECKING

import pytest
from analyzers.gate_guard_check import (
    ADR_016_PAIR,
    CORE_HEADER,
    CUSTOM_DEFINITIONS,
    FRIEND_DECLARATION,
    FRIENDSHIP_DEFINITION,
    GUARD_ASSERTION,
    GUARD_VALUE,
    IDENTIFIER_PREFIXES,
    MAVLINK_IDENTIFIER,
    NAME,
    OVERLAY_MANIFEST,
    REPORTED,
    UNASSIGNED,
    Finding,
    GateGuardCheckAnalyzer,
    check_adr_016_pair,
    check_guard_value,
    check_mavlink_identifier,
    main,
    premise_findings,
    review,
)
from common.analyzer import AnalyzerBase
from common.file_traversal import find_cpp_files
from common.gate_register import (
    GATE_IDS,
    Disagreement,
    MavlinkIdentifierSite,
    dump_gate_register,
    part_reference,
)
from validate_gate_register import (
    FACT_GROUP_REGISTRAR_GUARD,
    GUARD_REGISTRY,
    MAVLINK_IDENTIFIER_GUARD,
    MECHANICAL_GUARDS,
    OPEN_STATUS,
    OVERLAY_TREE,
    PREMISES,
    REGISTER_PATH,
    TEST_TREE,
    load_premises,
)

from ._helpers import REPO_ROOT, TOOLS_DIR
from .test_validate_gate_register import (
    ACTUATOR_PATH_GUARD,
    BASELINE_GUARD_GATES,
    BASELINE_PART,
    GUARD_DECLARATION,
    MISSION_GATE,
    PAYLOAD_GATE,
    REPLAY_GUARD,
    ROLE_GATE,
    SINGLE_GATE,
    TELEMETRY_GUARD,
    _accepting,
    _baseline_rows,
    _blocked_by,
    _guard,
    _inventory_text,
    _permissions,
    _premises,
    _register,
    _repository,
    _with_allow_list,
    _with_guard,
    _write,
)

if TYPE_CHECKING:
    from collections.abc import Sequence
    from pathlib import Path

    from common.analyzer import AnalysisResult
    from common.gate_register import GateRegister, Guard
    from validate_gate_register import Premises

ANALYZER = TOOLS_DIR / "analyzers" / "gate_guard_check.py"

# The script name a direct invocation puts in argv[0], which the usage line names.
SCRIPT = ANALYZER.name

# How the register premise reads when the tree lacks the register, which is the one premise failure
# the repository can reach today and the message the data half reports it with.
MISSING_REGISTER = f"{REGISTER_PATH} does not exist"

# Where a test puts the guard whose evidence it rewrites: a declaring file and an assertion site of
# its own, which no other registry entry records. The other seven guards keep the shared pair the
# data half's fixture records, so they keep agreeing and the finding a test asserts is the one it
# wrote.
SOLO_DECLARATION = "aerora/src/ASoloFixtureGuard.h"
SOLO_SITE = f"{TEST_TREE}/ASoloFixtureGuardTest.cc"

# A second recorded site for the guard, which is what "at least one" is read against.
SECOND_SITE = f"{TEST_TREE}/ASecondFixtureGuardTest.cc"

# A file that exists and declares nothing, and a site that exists and names no guard. Both are the
# state a removal leaves behind, which is the state "not found" must not read as "unchanged" in.
DECLARES_NOTHING = "#pragma once\n\n// A fixture header declaring no guard.\n"
ASSERTS_NOTHING = "// A fixture suite naming no guard.\n"

# The name a renamed declaration carries, which shares no prefix with any recorded symbol.
RENAMED_GUARD = "kAFixtureRenamedGuard"

# A literal no registry entry records: the value a recorded disagreement observes, and the value a
# guard carrying no disagreement moved to. A boolean, because Requirement 4 criterion 5 names the
# booleans these guards hold already.
ANOTHER_VALUE = "true"

# A third literal, which is neither the recorded value nor the one a disagreement observes. A
# fixture symbol rather than a value of the product's: the three-value case needs a literal the
# record does not excuse, and it must not introduce a Contract_Value to get one.
THIRD_VALUE = "kAFixtureGuardLiteral"

# The disagreement the design's Finding 1 records for the fact-group registrar: the register records
# `false`, the tree carries `true`, Tab 1 owns the authorisation that would settle it, and two
# remedies exist. The owner and the criteria are the design's; the remedies are fixtures, because
# writing the remedy an owner signs is task 9.3's and not a test's.
DISAGREEMENT_OWNER = "Tab 1"
DISAGREEMENT_AGAINST = ("7.17", "7.18")
DISAGREEMENT_REMEDIES = ("a fixture remedy", "another fixture remedy")
HAND_ON_ROW = "a fixture row of the handed-on items"


def _disagreement(*, observed: str = ANOTHER_VALUE, hand_on_row: str = HAND_ON_ROW) -> Disagreement:
    """The record that excuses one known state, in the shape the schema requires it in."""
    return Disagreement(
        observed_value=observed,
        against=list(DISAGREEMENT_AGAINST),
        owner=DISAGREEMENT_OWNER,
        remedies=list(DISAGREEMENT_REMEDIES),
        hand_on_row=hand_on_row,
    )


def _member(symbol: str) -> str:
    """The name a file declares *symbol* under: its last `::` component, without a trailing call.

    Derived here rather than imported from the analyzer, so the fixture spells a declaration the way
    C++ does and the rule's own name matching is compared against something written independently of
    it.
    """
    return symbol.rpartition("::")[2].removesuffix("()")


def _declares(symbol: str, value: str) -> str:
    """One declaration of *symbol* carrying *value*, in the shapes the overlay declares guards in.

    A symbol the register records as a call is written as a definition returning the literal and
    everything else as an initialiser, which are the two shapes the nine recorded guards are written
    in.
    """
    if symbol.endswith("()"):
        return f"bool {symbol}\n{{\n    return {value};\n}}\n"
    return f"inline constexpr auto {_member(symbol)} = {value};\n"


def _declaring(guards: Sequence[Guard]) -> str:
    """The header the register names as declaring *guards*, each carrying its recorded literal."""
    declarations = "\n".join(_declares(guard.symbol, guard.recorded_value) for guard in guards)
    return f"#pragma once\n\n{declarations}"


def _asserting(guards: Sequence[Guard]) -> str:
    """The suite the register names as asserting *guards* against their recorded literals.

    `guard-assertion` reads whether a site still names the guard and never what it asserts about it
    -- the value is `guard-value`'s to read, out of the declaration -- so what makes this a site is
    the symbol, and `ASSERTS_NOTHING` is the same file with the symbols gone.
    """
    body = "".join(f"    QCOMPARE({guard.symbol}, {guard.recorded_value});\n" for guard in guards)
    return (
        "// A fixture suite standing where the overlay asserts each guard's recorded value.\n"
        "void AFixtureGuardTest::_guardsHoldTheirRecordedValues()\n"
        "{\n"
        f"{body}"
        "}\n"
    )


def _guard_sources(root: Path, register: GateRegister) -> Path:
    """Lay out the files *register*'s registry names, as the agreeing case has them.

    Keyed by the recorded path, so guards recorded in one file share one document the way the
    overlay's headers and suites do.

    `mavlink-identifier` is left out, because no file declares it and no site asserts a value for
    it: Requirement 4 criterion 5 names it as a check rather than as a symbol, and what holds it is
    that check's own rule over an allow-list recorded by site.
    """
    declarations: dict[str, list[Guard]] = {}
    sites: dict[str, list[Guard]] = {}
    for guard in register.guards:
        if guard.symbol == MAVLINK_IDENTIFIER_GUARD:
            continue
        declarations.setdefault(guard.declared_in, []).append(guard)
        for site in guard.asserted_in:
            sites.setdefault(site, []).append(guard)

    for path, declared in declarations.items():
        _write(root / path, _declaring(declared))
    for path, asserted in sites.items():
        _write(root / path, _asserting(asserted))
    return root


# A compile definition the fixture manifest's list always carries, so a manifest that does not
# define the Aerora half still sets a list to find. A list that is absent is a different finding
# from a list that does not carry the define, and the fixture has to be able to write each.
ANOTHER_DEFINITION = "A_FIXTURE_DEFINITION"


def _core_header(*, friend: bool) -> str:
    """`src/Vehicle/Vehicle.h` as the fixture tree carries it, with its half applied or not.

    The comment block names the class and the define whether or not the declaration is there, which
    is the shape the real header has: the Aerora-added block beside the declaration explains both
    halves in prose. So the absent case is a file naming the declaration in a comment and nowhere
    else, which is what a rule reading comments would read as applied.
    """
    declared = f"    {FRIEND_DECLARATION};\n" if friend else ""
    return (
        "#pragma once\n\n"
        "class Vehicle\n"
        "{\n"
        "    friend class AFixtureUpstreamFriend;    // A friend upstream grants\n"
        f"    // A fixture comment naming `{FRIEND_DECLARATION}` and the\n"
        f"    // {FRIENDSHIP_DEFINITION} it is gated on, the way the overlay's block does.\n"
        f"{declared}"
        "};\n"
    )


def _overlay_manifest(*, defined: bool) -> str:
    """`aerora/CMakeLists.txt` as the fixture tree carries it, with its half defined or not.

    The comment above the list names the macro either way, for the reason `_core_header`'s does:
    the real manifest explains the pair in a comment directly above `CUSTOM_DEFINITIONS`, so a
    whole-file search would read the explanation as the definition.
    """
    define = f"    {FRIENDSHIP_DEFINITION}\n" if defined else ""
    return (
        f"# A fixture comment naming {FRIENDSHIP_DEFINITION} and the\n"
        f"# `{FRIEND_DECLARATION};` it pairs with.\n"
        f"set({CUSTOM_DEFINITIONS}\n"
        f"    {ANOTHER_DEFINITION}\n"
        f"{define}"
        "    PARENT_SCOPE\n"
        ")\n"
    )


def _pair_sources(root: Path, *, friend: bool, defined: bool) -> Path:
    """Lay out the two files Requirement 7 criterion 17 names, each half applied or not."""
    _write(root / CORE_HEADER, _core_header(friend=friend))
    _write(root / OVERLAY_MANIFEST, _overlay_manifest(defined=defined))
    return root


def _tree(root: Path, register: GateRegister | None = None, *, applied: bool = False) -> Path:
    """A repository root carrying *register*, the inventory it names and the sources it names.

    Every guard agrees with its record, so the baseline passes all three rules and a test asserting
    a finding writes that finding itself.

    *applied* is whether the `ADR-016` pair is written into the tree. It defaults to neither half,
    because every register reachable through a document here records the authorisation unsigned --
    no fixture part is accepted -- and neither half applied is the state Requirement 7 criterion 17
    protects. A test wanting the applied pair says so.
    """
    recorded = _register() if register is None else register
    return _guard_sources(
        _pair_sources(
            _repository(
                root,
                register=dump_gate_register(recorded),
                inventory=_inventory_text(_baseline_rows(recorded), _blocked_by(recorded)),
            ),
            friend=applied,
            defined=applied,
        ),
        recorded,
    )


def _without_register(root: Path) -> Path:
    """A repository root carrying the schemas and no register, which is the tree today."""
    return _repository(root, register=None, inventory=None)


def _in_code(root: Path, register: GateRegister) -> Premises:
    """Premises built in code over *register*, rooted at the guard sources at *root*.

    The schema rejects part of what `guard-value` rejects -- it requires `handOnRow` and requires it
    non-empty -- so a test of that rule hands the register over as a model rather than as a
    document, which is the state the rule's own docstring says it is the second line for.
    """
    return replace(_premises(register), root=_guard_sources(root, register))


def _alone(register: GateRegister, symbol: str, **changes: object) -> GateRegister:
    """*register* with *symbol* recorded in a declaring file and an assertion site of its own."""
    recorded: dict[str, object] = {
        "declared_in": SOLO_DECLARATION,
        "asserted_in": [SOLO_SITE],
        **changes,
    }
    return _with_guard(register, symbol, **recorded)


def _recorded(register: GateRegister, symbol: str) -> Guard:
    """The one registry entry *register* records for *symbol*."""
    (guard,) = [guard for guard in register.guards if guard.symbol == symbol]
    return guard


def _sources(root: Path) -> Path:
    """A directory laid out so that the expansion of it is observable.

    `find_cpp_files` yields the header and the source, drops the text file by suffix and drops
    everything under `build/` by directory name, so a review of exactly two files is evidence that
    it did the expansion rather than something written here.
    """
    (root / "build").mkdir(parents=True)
    (root / "AFixture.h").write_text("// a fixture header\n", encoding="utf-8")
    (root / "AFixture.cc").write_text("// a fixture source\n", encoding="utf-8")
    (root / "notes.txt").write_text("not a C++ file\n", encoding="utf-8")
    (root / "build" / "Generated.cc").write_text("// a build artefact\n", encoding="utf-8")
    return root


def _analyzer(root: Path) -> GateGuardCheckAnalyzer:
    """The analyzer against *root*.

    The base class takes a build directory and this analyzer never reads it: it inspects text, so
    there is nothing in a build for it.
    """
    return GateGuardCheckAnalyzer(root, root / "build")


def _review(root: Path) -> AnalysisResult:
    """The review of *root* selecting no file, because neither rule reads the selection."""
    return review(root, [])


def _established(register: GateRegister, *, files: int = 0, baseline: int = 0) -> str:
    """The summary line a run ends with, spelled out rather than matched loosely.

    What it claims is the point: the guard rules read the files *register* names, `adr-016-pair`
    reads the two files the criterion names and `mavlink-identifier` walks the overlay tree, so the
    line reports the guard values, the assertion sites, the pair and the allow-list it compared, and
    states that the selection decided none of it. Taking it off a run's output by equality is what
    makes every test below assert that the claim still reads that way.

    *baseline* is how many identifiers the allow-listed sites held, which defaults to none because
    no fixture tree carries a MAVLink identifier until a test writes one. A test that writes one
    says how many, which is how the ratchet case asserts that the baseline is read from the tree
    rather than recorded anywhere.
    """
    guards = [guard for guard in register.guards if guard.symbol != MAVLINK_IDENTIFIER_GUARD]
    sites = sum(len(guard.asserted_in) for guard in guards)
    allow_listed = len(register.mavlink_identifiers.allow_list)
    return (
        f"{REGISTER_PATH} records {len(register.guards)} guards over {len(register.gates)} "
        f"Gate_Dossiers; {len(guards)} guard values and {sites} assertion sites compared against "
        f"the files the register names, and the ADR-016 pair against {CORE_HEADER} and "
        f"{OVERLAY_MANIFEST}; every file under {OVERLAY_TREE}/ searched against {allow_listed} "
        f"allow-listed sites holding {baseline} identifiers; no rule read a file because it was "
        f"among the {files} selected source files\n"
    )


def _lines(
    result: AnalysisResult, register: GateRegister, *, files: int = 0, baseline: int = 0
) -> list[str]:
    """The finding lines *result* printed, with the summary the run ends with taken off."""
    return result.output.removesuffix(
        _established(register, files=files, baseline=baseline)
    ).splitlines()


def _snapshot(*trees: Path) -> dict[str, str]:
    """Every file under *trees* with its text, so a test can assert that nothing was written."""
    return {
        str(path): path.read_text(encoding="utf-8")
        for tree in trees
        for path in sorted(tree.rglob("*"))
        if path.is_file()
    }


@pytest.fixture
def reviewed(monkeypatch: pytest.MonkeyPatch) -> list[tuple[Path, list[Path]]]:
    """Every root and file list `main` hands the review, with the real review still running.

    `main`'s whole job is to turn arguments or stdin into the file list a rule will read, so the
    list it hands over is what a test about argument handling has to see.
    """
    calls: list[tuple[Path, list[Path]]] = []

    def _record(root: Path, files: Sequence[Path]) -> AnalysisResult:
        calls.append((root, list(files)))
        return review(root, files)

    monkeypatch.setattr("analyzers.gate_guard_check.review", _record)
    return calls


def test_the_analyzer_is_an_analyzer_base_subclass_exposing_name_and_run(tmp_path: Path) -> None:
    analyzer = _analyzer(_tree(tmp_path / "repo"))

    assert isinstance(analyzer, AnalyzerBase)
    assert analyzer.name == NAME
    assert analyzer.run([]).tool == NAME


def test_fix_is_accepted_and_changes_nothing(tmp_path: Path) -> None:
    """`fix` is discarded, so the result is the same and neither tree is written.

    There is no auto-fix here: the only thing to "fix" would be a recorded value no owner signed.
    """
    root = _tree(tmp_path / "repo")
    sources = _sources(tmp_path / "sources")
    files = sorted(find_cpp_files([sources]))
    before = _snapshot(root, sources)

    fixed = _analyzer(root).run(files, fix=True)

    assert fixed == _analyzer(root).run(files, fix=False)
    assert _snapshot(root, sources) == before


def test_a_premise_the_register_loader_cannot_establish_is_reported_as_itself(
    tmp_path: Path,
) -> None:
    """The data half's premise failure, carrying its check name and every Gate it leaves open."""
    premises = load_premises(_without_register(tmp_path / "repo"))

    assert isinstance(premises, list), premises

    (finding,) = premise_findings(premises)

    assert finding.check == PREMISES
    assert finding.gates == GATE_IDS
    assert finding.message == MISSING_REGISTER
    assert str(finding) == f"{PREMISES}: {', '.join(GATE_IDS)}: {MISSING_REGISTER}"


def test_a_finding_naming_no_gate_renders_as_unassigned_decision_content() -> None:
    """Requirement 4 criterion 2's other half: an element no Gate_Dossier records."""
    finding = Finding("a-fixture-check", (), "a fixture message")

    assert str(finding) == f"a-fixture-check: {UNASSIGNED}: a fixture message"


def test_run_reports_the_premise_failure_and_counts_the_files_it_was_given(tmp_path: Path) -> None:
    root = _without_register(tmp_path / "repo")
    files = sorted(find_cpp_files([_sources(tmp_path / "sources")]))

    result = _analyzer(root).run(files)

    assert not result.passed
    assert result.issues == 1
    assert result.files_checked == len(files)
    assert result.output == f"{PREMISES}: {', '.join(GATE_IDS)}: {MISSING_REGISTER}\n"


def test_run_states_the_premise_it_established_and_claims_nothing_about_the_files(
    tmp_path: Path,
) -> None:
    register = _register()
    files = sorted(find_cpp_files([_sources(tmp_path / "sources")]))

    result = _analyzer(_tree(tmp_path / "repo")).run(files)

    assert result.passed
    assert result.issues == 0
    assert result.output == _established(register, files=len(files))


def test_main_reviews_the_file_a_path_argument_names(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    reviewed: list[tuple[Path, list[Path]]],
) -> None:
    root = _tree(tmp_path / "repo")
    source = _sources(tmp_path / "sources") / "AFixture.cc"
    monkeypatch.setattr("analyzers.gate_guard_check.ROOT", root)
    monkeypatch.setattr(sys, "argv", [SCRIPT, str(source)])

    assert main() == 0

    assert reviewed == [(root, [source])]


def test_main_expands_a_directory_argument_with_find_cpp_files(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    reviewed: list[tuple[Path, list[Path]]],
) -> None:
    root = _tree(tmp_path / "repo")
    sources = _sources(tmp_path / "sources")
    monkeypatch.setattr("analyzers.gate_guard_check.ROOT", root)
    monkeypatch.setattr(sys, "argv", [SCRIPT, str(sources)])

    assert main() == 0

    ((_, files),) = reviewed
    assert sorted(file.name for file in files) == ["AFixture.cc", "AFixture.h"]


def test_main_reads_the_paths_from_stdin(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    reviewed: list[tuple[Path, list[Path]]],
) -> None:
    root = _tree(tmp_path / "repo")
    source = _sources(tmp_path / "sources") / "AFixture.cc"
    monkeypatch.setattr("analyzers.gate_guard_check.ROOT", root)
    monkeypatch.setattr(sys, "argv", [SCRIPT])
    monkeypatch.setattr(sys, "stdin", io.StringIO(f"{source}\n\n"))

    assert main() == 0

    assert reviewed == [(root, [source])]


def test_main_with_no_path_and_empty_stdin_reviews_nothing(
    monkeypatch: pytest.MonkeyPatch,
    reviewed: list[tuple[Path, list[Path]]],
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A run with nothing in front of it, which is what the hook hands over when no watched file
    changed."""
    monkeypatch.setattr(sys, "argv", [SCRIPT])
    monkeypatch.setattr(sys, "stdin", io.StringIO(""))

    assert main() == 0

    assert reviewed == []
    assert capsys.readouterr().err.startswith(f"Usage: {SCRIPT}")


def test_main_exits_one_naming_the_register_the_tree_lacks(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    root = _without_register(tmp_path / "repo")
    source = _sources(tmp_path / "sources") / "AFixture.cc"
    monkeypatch.setattr("analyzers.gate_guard_check.ROOT", root)
    monkeypatch.setattr(sys, "argv", [SCRIPT, str(source)])

    assert main() == 1

    assert capsys.readouterr().out == f"{PREMISES}: {', '.join(GATE_IDS)}: {MISSING_REGISTER}\n"


def test_the_module_runs_as_a_script_from_any_directory(tmp_path: Path) -> None:
    """The bootstrap preamble's own test: the module imports `common` and the data half when it is
    run as a script rather than imported as `analyzers.gate_guard_check`.

    It reviews the repository's own register, so the assertions are the ones that hold whether or
    not that register exists yet: it reports rather than crashes, and it names the register either
    way -- as the path it could not read, or as the premise it established.
    """
    source = _sources(tmp_path / "sources") / "AFixture.cc"

    result = subprocess.run(
        [sys.executable, str(ANALYZER), str(source)],
        capture_output=True,
        text=True,
        cwd=tmp_path,
        check=False,
    )

    assert "Traceback" not in result.stderr
    assert result.returncode in {0, 1}
    assert REGISTER_PATH in result.stdout


# ---------------------------------------------------------------------------
# guard-value
# ---------------------------------------------------------------------------


def test_the_agreeing_guard_sources_produce_no_finding_from_either_rule(tmp_path: Path) -> None:
    """Every recorded guard declared at its recorded literal and named by its recorded site.

    A rule that rejected the agreeing case would be as broken as one that accepted the disagreeing
    case, and this is the premise every case below mutates one axis of. The literal asserted is
    Requirement 4 criterion 5's own, so the tree the rules pass over is the tree the criterion
    describes rather than one written to agree with them.
    """
    register = _register()

    root = _tree(tmp_path / "repo", register)
    result = _review(root)

    assert result.passed
    assert result.issues == 0
    assert _lines(result, register) == []
    declared = (root / _recorded(register, TELEMETRY_GUARD).declared_in).read_text(encoding="utf-8")
    assert f"return {MECHANICAL_GUARDS[TELEMETRY_GUARD]};" in declared


def test_guard_value_names_the_guard_both_values_and_every_gate_it_holds_open(
    tmp_path: Path,
) -> None:
    """Requirement 4 criterion 7: name the guard and the Gate, and hold the Gate at `OPEN`.

    The guard chosen holds two Gates open, because the finding has to name each of them: a reviewer
    reading one Gate would repair one decision and leave the other unprotected.
    """
    register = _alone(_register(), ACTUATOR_PATH_GUARD)
    root = _tree(tmp_path / "repo", register)
    _write(root / SOLO_DECLARATION, _declares(ACTUATOR_PATH_GUARD, ANOTHER_VALUE))

    result = _review(root)

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{GUARD_VALUE}: {PAYLOAD_GATE}, {ROLE_GATE}: {ACTUATOR_PATH_GUARD}: ")
    assert repr(_recorded(register, ACTUATOR_PATH_GUARD).recorded_value) in line
    assert repr(ANOTHER_VALUE) in line
    assert SOLO_DECLARATION in line


def test_guard_value_names_a_declaring_file_the_tree_does_not_carry(tmp_path: Path) -> None:
    """A declaration this check cannot read is a failure rather than a pass."""
    register = _alone(_register(), REPLAY_GUARD)
    root = _tree(tmp_path / "repo", register)
    (root / SOLO_DECLARATION).unlink()

    result = _review(root)

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{GUARD_VALUE}: {MISSION_GATE}: {REPLAY_GUARD}: ")
    assert f"{SOLO_DECLARATION} does not exist" in line
    assert '"not found" is not' in line


def test_guard_value_names_a_declaration_absent_from_a_file_that_exists(tmp_path: Path) -> None:
    """The file the register names is readable and declares no symbol of that name."""
    register = _alone(_register(), REPLAY_GUARD)
    root = _tree(tmp_path / "repo", register)
    _write(root / SOLO_DECLARATION, DECLARES_NOTHING)

    result = _review(root)

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{GUARD_VALUE}: {MISSION_GATE}: {REPLAY_GUARD}: ")
    assert f"{SOLO_DECLARATION} declares no symbol of that name" in line
    assert '"not found" is not' in line


def test_guard_value_rejects_a_renamed_guard_whose_declaration_keeps_the_recorded_literal(
    tmp_path: Path,
) -> None:
    """The sharpest case the rule has: the literal is still there and the guard is not.

    A check that searched the declaring file for the recorded value would pass this, which is how a
    renamed guard stops being guarded, so the fixture keeps the literal and renames the symbol and
    the run has to fail anyway.
    """
    register = _alone(_register(), REPLAY_GUARD)
    recorded = _recorded(register, REPLAY_GUARD).recorded_value
    root = _tree(tmp_path / "repo", register)
    _write(root / SOLO_DECLARATION, _declares(RENAMED_GUARD, recorded))

    result = _review(root)

    (line,) = _lines(result, register)
    assert recorded in (root / SOLO_DECLARATION).read_text(encoding="utf-8")
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{GUARD_VALUE}: {MISSION_GATE}: {REPLAY_GUARD}: ")
    assert f"{SOLO_DECLARATION} declares no symbol of that name" in line


# ---------------------------------------------------------------------------
# guard-assertion
# ---------------------------------------------------------------------------


def test_guard_assertion_names_a_recorded_site_that_no_longer_names_the_symbol(
    tmp_path: Path,
) -> None:
    """Requirement 4 criterion 9: removing the check is as serious as removing the guard."""
    register = _alone(_register(), REPLAY_GUARD)
    root = _tree(tmp_path / "repo", register)
    _write(root / SOLO_SITE, ASSERTS_NOTHING)

    result = _review(root)

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{GUARD_ASSERTION}: {MISSION_GATE}: {REPLAY_GUARD}: ")
    assert f"{SOLO_SITE} no longer names the symbol" in line
    assert "Requirement 4 criterion 9" in line


def test_guard_assertion_accepts_a_guard_one_of_whose_recorded_sites_still_names_it(
    tmp_path: Path,
) -> None:
    """Criterion 9 read as it is written: what it rejects is removing *the* check.

    A guard two suites assert is still asserted when one of them consolidates into the other, so the
    rule reads every recorded site and rejects only the guard no site names.
    """
    register = _alone(_register(), REPLAY_GUARD, asserted_in=[SOLO_SITE, SECOND_SITE])
    root = _tree(tmp_path / "repo", register)
    _write(root / SECOND_SITE, ASSERTS_NOTHING)

    result = _review(root)

    assert result.passed
    assert result.issues == 0
    assert _lines(result, register) == []


# ---------------------------------------------------------------------------
# The recorded disagreement
# ---------------------------------------------------------------------------


def test_guard_value_reports_a_guard_standing_in_its_recorded_disagreement_without_blocking(
    tmp_path: Path,
) -> None:
    """The design's one concession to being gating, asserted as the result shape it chose.

    `passed` with a non-zero `issues` is that state: the run does not fail the hook and does not
    look clean either, and the marker on the line says which finding did not stop the commit. The
    owner and the remedies print with it, because a record that excuses a state without naming who
    resolves it is the suppression list the design refuses to build.
    """
    register = _alone(_register(), TELEMETRY_GUARD, disagreement=_disagreement())
    root = _tree(tmp_path / "repo", register)
    _write(root / SOLO_DECLARATION, _declares(TELEMETRY_GUARD, ANOTHER_VALUE))

    result = _review(root)

    (line,) = _lines(result, register)
    assert result.passed
    assert result.issues == 1
    assert line.startswith(f"{GUARD_VALUE} ({REPORTED}): {SINGLE_GATE}: {TELEMETRY_GUARD}: ")
    assert repr(_recorded(register, TELEMETRY_GUARD).recorded_value) in line
    assert repr(ANOTHER_VALUE) in line
    assert DISAGREEMENT_OWNER in line
    assert all(remedy in line for remedy in DISAGREEMENT_REMEDIES)
    assert HAND_ON_ROW in line


def test_main_exits_zero_on_a_guard_standing_in_its_recorded_disagreement(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """The exit status the design asks for: the finding prints on every run and the hook passes."""
    register = _alone(_register(), TELEMETRY_GUARD, disagreement=_disagreement())
    root = _tree(tmp_path / "repo", register)
    _write(root / SOLO_DECLARATION, _declares(TELEMETRY_GUARD, ANOTHER_VALUE))
    source = _sources(tmp_path / "sources") / "AFixture.cc"
    monkeypatch.setattr("analyzers.gate_guard_check.ROOT", root)
    monkeypatch.setattr(sys, "argv", [SCRIPT, str(source)])

    assert main() == 0

    assert f"{GUARD_VALUE} ({REPORTED}): " in capsys.readouterr().out


def test_guard_assertion_reports_the_assertion_the_design_records_as_not_written(
    tmp_path: Path,
) -> None:
    """A guard standing in its recorded disagreement carries no C++ assertion, by design.

    Asserting the recorded value would fail CI and asserting the observed one would launder an
    unauthorised edit into a passing test, so the design records that the assertion is not written
    and that this check carries the finding instead. Both findings print, neither blocks, and this
    is the one state in which a guard no site names does not fail the hook.
    """
    register = _alone(_register(), TELEMETRY_GUARD, disagreement=_disagreement())
    root = _tree(tmp_path / "repo", register)
    _write(root / SOLO_DECLARATION, _declares(TELEMETRY_GUARD, ANOTHER_VALUE))
    _write(root / SOLO_SITE, ASSERTS_NOTHING)

    result = _review(root)

    value, assertion = _lines(result, register)
    assert result.passed
    assert result.issues == 2
    assert value.startswith(f"{GUARD_VALUE} ({REPORTED}): {SINGLE_GATE}: {TELEMETRY_GUARD}: ")
    assert assertion.startswith(
        f"{GUARD_ASSERTION} ({REPORTED}): {SINGLE_GATE}: {TELEMETRY_GUARD}: "
    )
    assert "the assertion the design records as not written" in assertion


def test_guard_value_rejects_a_third_value_naming_all_three(tmp_path: Path) -> None:
    """The record pins both values, so it excuses one known state rather than the guard."""
    register = _alone(_register(), TELEMETRY_GUARD, disagreement=_disagreement())
    root = _tree(tmp_path / "repo", register)
    _write(root / SOLO_DECLARATION, _declares(TELEMETRY_GUARD, THIRD_VALUE))

    result = _review(root)

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{GUARD_VALUE}: {SINGLE_GATE}: {TELEMETRY_GUARD}: ")
    assert repr(_recorded(register, TELEMETRY_GUARD).recorded_value) in line
    assert repr(ANOTHER_VALUE) in line
    assert repr(THIRD_VALUE) in line


def test_guard_value_rejects_a_disagreement_naming_no_handed_on_row(tmp_path: Path) -> None:
    """The escape hatch cannot be opened without pointing at the row that tracks it.

    The register reaches the rule as a model rather than as a document, because the schema requires
    a non-empty `handOnRow` and would reject this one first. A blank row is the case the rule reads:
    the field is present, and what it names is nothing.
    """
    register = _alone(_register(), TELEMETRY_GUARD, disagreement=_disagreement(hand_on_row=" "))

    (finding,) = check_guard_value(_in_code(tmp_path / "repo", register))

    assert finding.check == GUARD_VALUE
    assert finding.gates == (SINGLE_GATE,)
    assert finding.blocking
    assert finding.message.startswith(f"{TELEMETRY_GUARD}: ")
    assert "names no handed-on row" in finding.message


# ---------------------------------------------------------------------------
# adr-016-pair
#
# Named with the `adr` prefix, which is the selector these reach under: `-k adr` runs this group and
# nothing else, and the task's own command is a direct invocation of the script rather than a test
# selection.
#
# Two documents again, read against each other, and the division with `guard-value` is the thing
# most of these assert. `guard-value` owns the value `AeroraVehicleFactGroupRegistrar::
# isRegistrationAvailable()` holds and everything recorded about that value; this rule owns the two
# edits, in the two files `guard-value` never opens, against the change permission recorded on the
# same registry entry. The sharpest case is the half-applied tree: the guard holds its recorded
# `false` there, so the value rule passes and this is the only finding -- a rule that went quiet
# whenever `guard-value` spoke would let that tree through.
# ---------------------------------------------------------------------------


def _signing_owner(register: GateRegister, gate: str) -> str:
    """The owner *register* records as authorised to sign *gate*'s one part.

    Read off the register the test hands over rather than written here, because what the finding
    has to name is the label the register records and not the label this file happens to know.
    """
    (dossier,) = [item for item in register.gates if item.id == gate]
    (owner,) = dossier.parts[0].signing_owners
    return owner


def _authorised() -> Premises:
    """Premises in which the register records the core edit's authorisation as given.

    The part permitted to change the guard accepted, with a Closure_Record recording a permitting
    verdict for it, which is what the freedom derivation reads as freedom. Requirement 7 criterion
    18 records the acceptance of both parts of the Gate as that condition and the fixture Gate has
    one part, so this is that condition met.

    A model rather than a document, for the reason `_in_code` hands one over: the document route
    would need a Closure_Record file in the tree and a register naming it, and what is under test
    here is the rule's reading of the permission.
    """
    return _accepting(_register(), {SINGLE_GATE: _permissions(SINGLE_GATE)})


def _pair_premises(root: Path, premises: Premises, *, friend: bool, defined: bool) -> Premises:
    """*premises* rooted at a tree carrying the `ADR-016` pair in the state the test wants."""
    return replace(premises, root=_pair_sources(root, friend=friend, defined=defined))


def test_adr_016_pair_accepts_a_tree_naming_both_halves_in_comments_and_applying_neither(
    tmp_path: Path,
) -> None:
    """The state Requirement 7 criterion 17 protects, with both halves named in prose.

    The fixture files mention the declaration and the define in a comment the way the real two
    files do, so a rule that searched the text would report an applied pair in the tree that
    applied neither half. Nothing is reported, and the mentions are asserted to be there, so the
    pass is evidence that comments were removed rather than that the fixture was quiet.
    """
    register = _register()

    root = _tree(tmp_path / "repo", register)
    result = _review(root)

    assert FRIEND_DECLARATION in (root / CORE_HEADER).read_text(encoding="utf-8")
    assert FRIENDSHIP_DEFINITION in (root / OVERLAY_MANIFEST).read_text(encoding="utf-8")
    assert result.passed
    assert result.issues == 0
    assert _lines(result, register) == []


def test_adr_016_pair_reports_the_applied_pair_the_register_pins_without_blocking(
    tmp_path: Path,
) -> None:
    """The state this repository stands in: both halves applied, unsigned, disagreement recorded.

    Reported and not blocking, which is the design's one concession carried over from
    `guard-value`: a rule rejecting the applied pair on every commit until an unrelated
    authorisation landed would be removed rather than obeyed. The finding names both halves, the
    part whose acceptance would authorise them, and the owner.
    """
    register = _with_guard(_register(), TELEMETRY_GUARD, disagreement=_disagreement())

    result = _review(_tree(tmp_path / "repo", register, applied=True))

    (line,) = _lines(result, register)
    assert result.passed
    assert result.issues == 1
    assert line.startswith(f"{ADR_016_PAIR} ({REPORTED}): {SINGLE_GATE}: ")
    assert f"{FRIEND_DECLARATION} in {CORE_HEADER}" in line
    assert f"{FRIENDSHIP_DEFINITION} in {OVERLAY_MANIFEST} {CUSTOM_DEFINITIONS}" in line
    assert f"{part_reference(SINGLE_GATE, BASELINE_PART)}: recorded unaccepted" in line
    assert HAND_ON_ROW in line


def test_adr_016_pair_names_the_owner_the_register_records_rather_than_a_label_of_its_own(
    tmp_path: Path,
) -> None:
    """Requirement 7 criterion 17's second clause: name the owner whose signature it requires.

    Both sources the register offers are read, and they differ in this fixture: the disagreement's
    own owner, which is the register's answer to who resolves this gap, and the signing owner of
    the part whose acceptance would free the guard. A check carrying one label as a constant would
    stop naming the right owner the day that label changed.
    """
    register = _with_guard(_register(), TELEMETRY_GUARD, disagreement=_disagreement())

    result = _review(_tree(tmp_path / "repo", register, applied=True))

    (line,) = _lines(result, register)
    assert _signing_owner(register, SINGLE_GATE) != DISAGREEMENT_OWNER
    assert DISAGREEMENT_OWNER in line
    assert _signing_owner(register, SINGLE_GATE) in line
    assert "names the owner whose signature the core edit requires" in line


def test_adr_016_pair_and_guard_value_report_one_gap_as_two_findings_that_do_not_overlap(
    tmp_path: Path,
) -> None:
    """The division, asserted as output: two findings, two subjects, neither blocking.

    `guard-value` names the observed literal, the remedies and the handed-on row; this rule names
    the two edits and the owner and reports none of the value's evidence. Reporting the same defect
    twice would be noise, and the test fails if either line grows the other's content.
    """
    register = _alone(_register(), TELEMETRY_GUARD, disagreement=_disagreement())
    root = _tree(tmp_path / "repo", register, applied=True)
    _write(root / SOLO_DECLARATION, _declares(TELEMETRY_GUARD, ANOTHER_VALUE))

    result = _review(root)

    value, pair = _lines(result, register)
    assert result.passed
    assert result.issues == 2
    assert value.startswith(f"{GUARD_VALUE} ({REPORTED}): {SINGLE_GATE}: {TELEMETRY_GUARD}: ")
    assert pair.startswith(f"{ADR_016_PAIR} ({REPORTED}): {SINGLE_GATE}: ")
    assert repr(ANOTHER_VALUE) in value
    assert repr(ANOTHER_VALUE) not in pair
    assert all(remedy in value for remedy in DISAGREEMENT_REMEDIES)
    assert all(remedy not in pair for remedy in DISAGREEMENT_REMEDIES)
    assert CORE_HEADER not in value
    assert CORE_HEADER in pair


def test_adr_016_pair_rejects_the_applied_pair_that_no_recorded_disagreement_pins(
    tmp_path: Path,
) -> None:
    """The record excuses one known state, not the pair.

    The baseline records no disagreement for the guard, so nothing in the register says this state
    is known and criterion 17's rejection stands. The owner comes from the part's signing owners,
    which is the source that resolves whether or not a disagreement is recorded.
    """
    register = _register()

    result = _review(_tree(tmp_path / "repo", register, applied=True))

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{ADR_016_PAIR}: {SINGLE_GATE}: ")
    assert "no disagreement that excuses the applied pair" in line
    assert _signing_owner(register, SINGLE_GATE) in line
    assert f"holds the Gate at {OPEN_STATUS}" in line


def test_adr_016_pair_rejects_the_half_the_guard_value_rule_cannot_see(tmp_path: Path) -> None:
    """The friend declaration applied and the define withdrawn, under an unsigned authorisation.

    This is the gap that makes the rule worth having. `isRegistrationAvailable()` returns the
    recorded `false` in such a tree, so `guard-value` passes over it and this is the only finding --
    and criterion 17 rejects *either* half, not only the pair.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(root / CORE_HEADER, _core_header(friend=True))

    result = _review(root)

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{ADR_016_PAIR}: {SINGLE_GATE}: ")
    assert f"{FRIEND_DECLARATION} in {CORE_HEADER} and not {FRIENDSHIP_DEFINITION} in " in line
    assert "one edit in two files" in line
    assert _signing_owner(register, SINGLE_GATE) in line


def test_adr_016_pair_rejects_a_half_applied_pair_the_register_records_as_authorised(
    tmp_path: Path,
) -> None:
    """A signed authorisation covers an edit in two files, and this tree carries one of them.

    The define alone is the direction that does not compile, which is why one half is a third
    state rather than a weaker form of either: it matches neither the authorised pair nor the
    absence criterion 17 protects.
    """
    premises = _pair_premises(tmp_path / "repo", _authorised(), friend=False, defined=True)

    (finding,) = check_adr_016_pair(premises)

    assert finding.check == ADR_016_PAIR
    assert finding.gates == (SINGLE_GATE,)
    assert finding.blocking
    assert f"{FRIENDSHIP_DEFINITION} in {OVERLAY_MANIFEST}" in finding.message
    assert f"not {FRIEND_DECLARATION} in {CORE_HEADER}" in finding.message


def test_adr_016_pair_accepts_the_applied_pair_once_the_register_records_the_authorisation(
    tmp_path: Path,
) -> None:
    """The guard free and both halves applied, which is the pair matching the recorded state."""
    premises = _pair_premises(tmp_path / "repo", _authorised(), friend=True, defined=True)

    assert check_adr_016_pair(premises) == []


def test_adr_016_pair_accepts_an_authorisation_that_nobody_has_acted_on(tmp_path: Path) -> None:
    """An authorisation is a permission rather than an instruction.

    A signed core-edit authorisation over a tree carrying neither half is no finding here. Whether
    the guard's value followed the edit is `guard-value`'s, which reads the declaration against the
    value the register records for it.
    """
    premises = _pair_premises(tmp_path / "repo", _authorised(), friend=False, defined=False)

    assert check_adr_016_pair(premises) == []


def test_adr_016_pair_names_a_core_header_the_tree_does_not_carry(tmp_path: Path) -> None:
    """A file this rule cannot read is a failure rather than a pass."""
    register = _register()
    root = _tree(tmp_path / "repo", register)
    (root / CORE_HEADER).unlink()

    result = _review(root)

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{ADR_016_PAIR}: {SINGLE_GATE}: {FRIEND_DECLARATION}: ")
    assert f"{CORE_HEADER} does not exist" in line
    assert '"not found" is not "not applied"' in line


def test_adr_016_pair_names_a_manifest_that_sets_no_custom_definitions_list(
    tmp_path: Path,
) -> None:
    """A manifest that changed shape is a half this rule could not read, not a withdrawn define.

    The recorded remedy withdraws the define *from* the list, so a manifest setting no list at all
    is a different repair and reading it as the remedy would retire the rule by accident.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(root / OVERLAY_MANIFEST, "# A fixture manifest setting no definitions list.\n")

    result = _review(root)

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{ADR_016_PAIR}: {SINGLE_GATE}: {FRIENDSHIP_DEFINITION}: ")
    assert f"{OVERLAY_MANIFEST} sets no {CUSTOM_DEFINITIONS} list" in line
    assert '"not found" is not "not applied"' in line


def test_adr_016_pair_rejects_a_register_recording_no_entry_for_the_guard(tmp_path: Path) -> None:
    """The premise is the data half's and the silence would be this rule's.

    That the registry covers the guard is `guard-registry`'s finding, so this does not restate it
    -- what it reports is that the pair was read against no recorded authorisation state. Going
    quiet here would leave the only rule that reads the fork surface switched off by an omission in
    the register.
    """
    register = _register(
        guards=[_guard(symbol) for symbol in BASELINE_GUARD_GATES if symbol != TELEMETRY_GUARD]
    )

    result = _review(_tree(tmp_path / "repo", register))

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{ADR_016_PAIR}: {SINGLE_GATE}: {FACT_GROUP_REGISTRAR_GUARD}: ")
    assert "records no entry" in line
    assert f"is {GUARD_REGISTRY}'s finding in the data half" in line


def test_adr_016_pair_reads_the_real_core_header_and_overlay_manifest_without_writing_them() -> (
    None
):
    """The one case whose evidence is the repository rather than a fixture.

    Both patterns are matched against the two files they exist to watch, with a fixture register
    recording what the design's Finding 1 records: the authorisation unsigned, and a disagreement
    standing on the guard. Both halves are applied at this tree, so the rule reports exactly the
    state task 1.1 established by hand, and reports it without blocking.

    The files come back byte for byte, which is the read-only promise asserted against the files it
    is about rather than against a fixture.
    """
    register = _with_guard(_register(), TELEMETRY_GUARD, disagreement=_disagreement())
    pair = (CORE_HEADER, OVERLAY_MANIFEST)
    before = {path: (REPO_ROOT / path).read_text(encoding="utf-8") for path in pair}

    (finding,) = check_adr_016_pair(_premises(register))

    assert finding.check == ADR_016_PAIR
    assert finding.gates == (SINGLE_GATE,)
    assert not finding.blocking
    assert f"{FRIEND_DECLARATION} in {CORE_HEADER}" in finding.message
    assert f"{FRIENDSHIP_DEFINITION} in {OVERLAY_MANIFEST} {CUSTOM_DEFINITIONS}" in finding.message
    assert "records no acceptance permitting it" in finding.message
    assert DISAGREEMENT_OWNER in finding.message
    assert {path: (REPO_ROOT / path).read_text(encoding="utf-8") for path in pair} == before


# ---------------------------------------------------------------------------
# mavlink-identifier
#
# Named with the `mavlink` token, which is the selector these reach under: `-k mavlink` runs this
# group and nothing else, and it is the task's own command.
#
# The rule under test is the only one here that walks a tree rather than reading the files the
# register names, so the fixture that decides what it sees is the overlay tree the premises are
# rooted at -- and the register's part of it is an allow-list of *sites*, never of identifiers. The
# fixtures are held to the same rule: a token here is one of the four prefixes the task names
# followed by a suffix no dialect issues, so nothing written in this file is a Contract_Value. Every
# case that writes a token also asserts that the finding names the token's *class* and not the
# token, because a rule that printed what it found would make its own output the place a MAVLink
# identifier is written down.
# ---------------------------------------------------------------------------

# Where these tests write a file carrying a token: a source the register allow-lists, a source it
# does not, and a document it does not. All three under the overlay tree, because that is the tree
# the rule walks.
ALLOW_LISTED_SOURCE = f"{OVERLAY_TREE}/src/AFixtureAllowListedSource.cc"
UNLISTED_SOURCE = f"{OVERLAY_TREE}/src/AFixtureUnlistedSource.cc"
UNLISTED_DOCUMENT = f"{OVERLAY_TREE}/src/AFixtureUnlistedNotes.md"

# The reason an allow-list entry carries. A role rather than an identifier, which is the sentence
# the schema's own description asks the next person to be able to repeat about their own entry.
SITE_REASON = "a fixture site reading values it does not author"

# Two suffixes that are obviously fixtures, so a prefix plus either is a token of the right shape
# and an identifier of no dialect. The second is what a new token appearing in an already
# allow-listed file is written as.
FIXTURE_SUFFIX = "A_FIXTURE_IDENTIFIER"
ANOTHER_FIXTURE_SUFFIX = "ANOTHER_FIXTURE_IDENTIFIER"

# A token of any of the four classes, spelled here rather than imported from the analyzer so that
# what each case asserts about a finding's text is written independently of the pattern that
# produced it. A prefix and at least one character after it, which is the shape that makes a token
# an identifier: a finding naming a bare prefix as a class is naming the class, not printing a
# token, and this has to tell the two apart.
_NAMED_IDENTIFIER = re.compile("|".join(rf"{prefix}\w+" for prefix in IDENTIFIER_PREFIXES))


def _token(prefix: str, suffix: str = FIXTURE_SUFFIX) -> str:
    """One token of *prefix*'s class, which is the prefix and a fixture suffix and nothing real."""
    return f"{prefix}{suffix}"


def _carrying(*tokens: str) -> str:
    """A fixture source naming *tokens*, in the shape a file reading an upstream value has one."""
    read = "".join(f"    (void){token};\n" for token in tokens)
    return (
        "// A fixture source standing where the overlay reads a value it does not author.\n"
        "void aFixtureFunction()\n"
        "{\n"
        f"{read}"
        "}\n"
    )


def _documenting(token: str) -> str:
    """A fixture document naming *token* in prose, which is how three files in the tree carry one.

    Markdown rather than C++, because that is the shape the walk's scope turns on: a rule reading
    only the C++ files would pass over this, and `aerora/src/Vehicle/README.md`,
    `aerora/src/Mission/README.md` and `aerora/src/Mission/coverage-mission-contract.md` all
    describe upstream commands and frames in prose, naming them.
    """
    return f"# A fixture note\n\nThe prose names `{token}` as an upstream value.\n"


def _allowing(register: GateRegister, *paths: str, reason: str = SITE_REASON) -> GateRegister:
    """*register* allow-listing each of *paths* with *reason*, which is all an entry carries."""
    return _with_allow_list(
        register, *(MavlinkIdentifierSite(path=path, reason=reason) for path in paths)
    )


def test_mavlink_identifier_accepts_every_token_an_allow_listed_site_holds(tmp_path: Path) -> None:
    """The baseline is whatever the allow-listed files hold, read out of the tree on every run.

    Two classes in one site, so the acceptance is of the identifiers the file carries rather than
    of one shape of them, and the run's own summary states the baseline it read.
    """
    register = _allowing(_register(), ALLOW_LISTED_SOURCE)
    root = _tree(tmp_path / "repo", register)
    _write(
        root / ALLOW_LISTED_SOURCE,
        _carrying(_token(IDENTIFIER_PREFIXES[0]), _token(IDENTIFIER_PREFIXES[3])),
    )

    result = _review(root)

    assert result.passed
    assert result.issues == 0
    assert _lines(result, register, baseline=2) == []


def test_mavlink_identifier_accepts_a_new_token_in_an_already_allow_listed_site(
    tmp_path: Path,
) -> None:
    """A ratchet rather than a freeze, and this is the half that makes it one.

    The permitted set is read from the allow-listed file on every run and recorded nowhere, so a
    token added to a site an owner has already signed is accepted without the register being
    edited -- which is the only reason the allow-list can be recorded by site at all. The baseline
    the run states grows with the file, which is the evidence that it was read rather than
    remembered.
    """
    register = _allowing(_register(), ALLOW_LISTED_SOURCE)
    prefix = IDENTIFIER_PREFIXES[0]
    root = _tree(tmp_path / "repo", register)
    _write(root / ALLOW_LISTED_SOURCE, _carrying(_token(prefix)))

    before = _review(root)
    _write(
        root / ALLOW_LISTED_SOURCE,
        _carrying(_token(prefix), _token(prefix, ANOTHER_FIXTURE_SUFFIX)),
    )
    after = _review(root)

    assert before.passed
    assert after.passed
    assert _lines(before, register, baseline=1) == []
    assert _lines(after, register, baseline=2) == []


def test_mavlink_identifier_names_the_file_and_the_gate_of_a_token_it_does_not_allow_list(
    tmp_path: Path,
) -> None:
    """Requirement 6 criterion 14: reject the change, naming the file and `DG-MIS`.

    The class is named and the token is not, which is the constraint the allow-list exists to keep:
    a finding carrying the identifier would put a Contract_Value in the output of the rule that
    exists to stop one entering the repository.
    """
    register = _register()
    prefix = IDENTIFIER_PREFIXES[0]
    root = _tree(tmp_path / "repo", register)
    _write(root / UNLISTED_SOURCE, _carrying(_token(prefix)))

    result = _review(root)

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{MAVLINK_IDENTIFIER}: {MISSION_GATE}: {UNLISTED_SOURCE}: ")
    assert "the register's allow-list does not carry the file" in line
    assert "Requirement 6 criterion 14" in line
    assert "a site and a reason, never the identifier itself" in line
    assert prefix in line
    assert not _NAMED_IDENTIFIER.search(line)


@pytest.mark.parametrize("prefix", IDENTIFIER_PREFIXES)
def test_mavlink_identifier_detects_a_token_of_each_class(tmp_path: Path, prefix: str) -> None:
    """All four classes, because the rule this one takes over from reads three of them.

    `.kiro/tools/aerora-ui-static-check.ps1` matches `MAV_CMD_`, `MAVLINK_MSG_ID_` and
    `MAV_COMP_ID_` and leaves `MAV_FRAME_` out, so the two are not equivalent checks and the
    authoritative copy must not be the narrower one. Ten files under the overlay tree carry a
    `MAV_FRAME_` token today, so the omitted class is not a hypothetical one.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(root / UNLISTED_SOURCE, _carrying(_token(prefix)))

    result = _review(root)

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{MAVLINK_IDENTIFIER}: {MISSION_GATE}: {UNLISTED_SOURCE}: ")
    assert f"1 MAVLink identifier token of {prefix}," in line
    assert not _NAMED_IDENTIFIER.search(line)


def test_mavlink_identifier_reads_a_prefix_named_in_prose_as_a_class_and_not_as_a_token(
    tmp_path: Path,
) -> None:
    """A sentence naming a class is prose; a sentence naming a member of it is an identifier.

    That bound is the same one that lets the four prefixes be constants in the analyzer and in this
    file: the Glossary counts the message, command or enumeration *identifier* as the
    Contract_Value, so a file under the overlay tree may say which class of identifier it must not
    carry without carrying one. A rule that read the bare prefix would reject its own
    documentation, which is the false rejection that gets a gate removed rather than obeyed.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(root / UNLISTED_DOCUMENT, _documenting(IDENTIFIER_PREFIXES[0]))

    result = _review(root)

    assert IDENTIFIER_PREFIXES[0] in (root / UNLISTED_DOCUMENT).read_text(encoding="utf-8")
    assert result.passed
    assert result.issues == 0
    assert _lines(result, register) == []


def test_mavlink_identifier_reads_a_token_written_into_a_document(tmp_path: Path) -> None:
    """The walk covers every file under the overlay tree, not only the C++ ones.

    `find_cpp_files` yields C++ alone and its default skip list drops `test/`, and the tokens in
    the tree today include a documentation file carrying them in prose, so a walk bounded either
    way would let a Contract_Value land somewhere this rule had stopped looking. The line the
    finding names is the prose line, which is what tells a reviewer it is a document and not a
    declaration.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(root / UNLISTED_DOCUMENT, _documenting(_token(IDENTIFIER_PREFIXES[1])))

    result = _review(root)

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{MAVLINK_IDENTIFIER}: {MISSION_GATE}: {UNLISTED_DOCUMENT}: ")
    assert "first at line 3" in line
    assert not _NAMED_IDENTIFIER.search(line)


def test_mavlink_identifier_names_an_allow_listed_site_the_tree_does_not_carry(
    tmp_path: Path,
) -> None:
    """A baseline read from a file that is not there is not a baseline.

    The site permits nothing once the file is gone, and what makes that a finding rather than a
    quiet pass is the same rule `guard-value` reads a missing declaration under: "not found" is not
    "unchanged".
    """
    register = _allowing(_register(), ALLOW_LISTED_SOURCE)

    result = _review(_tree(tmp_path / "repo", register))

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{MAVLINK_IDENTIFIER}: {MISSION_GATE}: {ALLOW_LISTED_SOURCE}: ")
    assert "the allow-listed site does not exist" in line
    assert '"not found" is not' in line


def test_mavlink_identifier_rejects_an_allow_list_entry_recording_no_reason(
    tmp_path: Path,
) -> None:
    """An exemption no reviewer can re-read, which the reason is the whole of.

    The register reaches the rule as a model rather than as a document, for the reason the
    handed-on row case hands one over: the schema requires a non-empty `reason` and would reject
    this entry first. A blank one is the case the rule reads -- the field is present, and what it
    says is nothing.
    """
    register = _allowing(_register(), GUARD_DECLARATION, reason=" ")

    (finding,) = check_mavlink_identifier(_in_code(tmp_path / "repo", register))

    assert finding.check == MAVLINK_IDENTIFIER
    assert finding.gates == (MISSION_GATE,)
    assert finding.blocking
    assert finding.message.startswith(f"{GUARD_DECLARATION}: ")
    assert "no reason for it" in finding.message


def test_mavlink_identifier_rejects_an_overlay_tree_it_could_not_walk(tmp_path: Path) -> None:
    """A run that searched nothing must not read as a run that found nothing.

    The premises are rooted at a tree that carries no overlay at all, which is the one state this
    rule cannot answer in: every other rule here reads a file the register or a criterion names and
    reports the one it could not read, and the file this rule reads is every file there is.
    """
    premises = replace(_premises(_register()), root=tmp_path / "repo")

    (finding,) = check_mavlink_identifier(premises)

    assert finding.check == MAVLINK_IDENTIFIER
    assert finding.gates == (MISSION_GATE,)
    assert finding.blocking
    assert finding.message.startswith(f"{OVERLAY_TREE}/ is not a directory")
    assert "indistinguishable from a run that found nothing" in finding.message


def test_mavlink_identifier_reports_the_real_overlay_tree_against_an_empty_allow_list() -> None:
    """The one case whose evidence is the repository rather than a fixture.

    The real tree carries tokens today, so a rule walking it against an allow-list recording no
    site has to report them -- and which files it names is what task 9.4 has to allow-list. The
    assertions are on the categories the design names rather than on a path, because a path moves
    and the categories are the reason an entry exists: the mission files, the calibration
    authorization gate, the Aerora suites' own fixtures, and a document carrying them in prose.

    No finding carries an identifier, which is asserted over the real tokens rather than over
    fixture ones, and the tree comes back byte for byte: the rule reads the overlay and never
    writes it.
    """
    register = _register()
    before = _snapshot(REPO_ROOT / OVERLAY_TREE)

    findings = check_mavlink_identifier(_premises(register))

    reported = [finding.message.split(":")[0] for finding in findings]
    assert findings
    assert {finding.check for finding in findings} == {MAVLINK_IDENTIFIER}
    assert {finding.gates for finding in findings} == {(MISSION_GATE,)}
    assert all(finding.blocking for finding in findings)
    assert [path for path in reported if path.endswith(".md")]
    assert [path for path in reported if path.startswith(f"{TEST_TREE}/")]
    assert [path for path in reported if path.startswith(f"{OVERLAY_TREE}/src/")]
    assert not any(_NAMED_IDENTIFIER.search(finding.message) for finding in findings)
    assert _snapshot(REPO_ROOT / OVERLAY_TREE) == before

#!/usr/bin/env python3
"""Tests for the shell and the two guard rules in tools/analyzers/gate_guard_check.py.

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
file, and `guard-assertion`, which reads each recorded assertion site under `aerora/test/**`.
Neither reads the files the run selected, so the fixture that decides what a rule sees is not the
file list but the tree.

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
import subprocess
import sys
from dataclasses import replace
from typing import TYPE_CHECKING

import pytest
from analyzers.gate_guard_check import (
    GUARD_ASSERTION,
    GUARD_VALUE,
    NAME,
    REPORTED,
    UNASSIGNED,
    Finding,
    GateGuardCheckAnalyzer,
    check_guard_value,
    main,
    premise_findings,
    review,
)
from common.analyzer import AnalyzerBase
from common.file_traversal import find_cpp_files
from common.gate_register import GATE_IDS, Disagreement, dump_gate_register
from validate_gate_register import (
    MAVLINK_IDENTIFIER_GUARD,
    MECHANICAL_GUARDS,
    PREMISES,
    REGISTER_PATH,
    TEST_TREE,
    load_premises,
)

from ._helpers import TOOLS_DIR
from .test_validate_gate_register import (
    ACTUATOR_PATH_GUARD,
    MISSION_GATE,
    PAYLOAD_GATE,
    REPLAY_GUARD,
    ROLE_GATE,
    SINGLE_GATE,
    TELEMETRY_GUARD,
    _baseline_rows,
    _blocked_by,
    _inventory_text,
    _premises,
    _register,
    _repository,
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


def _tree(root: Path, register: GateRegister | None = None) -> Path:
    """A repository root carrying *register*, the inventory it names and the guard sources it names.

    Every guard agrees with its record, so the baseline passes both rules and a test asserting a
    finding writes that finding itself.
    """
    recorded = _register() if register is None else register
    return _guard_sources(
        _repository(
            root,
            register=dump_gate_register(recorded),
            inventory=_inventory_text(_baseline_rows(recorded), _blocked_by(recorded)),
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


def _established(register: GateRegister, *, files: int = 0) -> str:
    """The summary line a run ends with, spelled out rather than matched loosely.

    What it claims is the point: both rules read the files *register* names, so the line reports the
    guard values and the assertion sites it compared and states that the selected files have not
    been read by a rule. Taking it off a run's output by equality is what makes every test below
    assert that the claim still reads that way.
    """
    guards = [guard for guard in register.guards if guard.symbol != MAVLINK_IDENTIFIER_GUARD]
    sites = sum(len(guard.asserted_in) for guard in guards)
    return (
        f"{REGISTER_PATH} records {len(register.guards)} guards over {len(register.gates)} "
        f"Gate_Dossiers; {len(guards)} guard values and {sites} assertion sites compared against "
        f"the files the register names, and none of the {files} selected source files has been "
        f"read by a rule yet\n"
    )


def _lines(result: AnalysisResult, register: GateRegister, *, files: int = 0) -> list[str]:
    """The finding lines *result* printed, with the summary the run ends with taken off."""
    return result.output.removesuffix(_established(register, files=files)).splitlines()


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

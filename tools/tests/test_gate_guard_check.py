#!/usr/bin/env python3
"""Tests for the analyzer shell in tools/analyzers/gate_guard_check.py.

The source half of the Gate_Review carries no rule yet -- tasks 7.2 to 7.7 add the ten the design's
table names -- so what there is to test is the shell: that it is an `AnalyzerBase` the dispatcher
can run, that `fix` is accepted and writes nothing, that arguments and stdin reach the review as the
file list a rule will read, that a directory is expanded by `find_cpp_files` rather than by
something local to the analyzer, and that the register premise is established before anything else
and reported as itself when it cannot be.

The last of those is the behaviour the repository exercises today, because the register does not
exist yet: task 9 writes it, and until it does, every run of the source half fails naming the path.
That is the spec's governing rule rather than an interim inconvenience -- an unresolved premise is a
failure, never a pass -- so it is asserted here as the shell's answer in that state.

The fixture register is the data half's, imported rather than rewritten. Both halves read one
register through one loader, so a second fixture register here would be a second definition of a
register the Gate_Review accepts, free to drift from the first and to let one half's tests pass over
a document the other half rejects.
"""

from __future__ import annotations

import io
import subprocess
import sys
from typing import TYPE_CHECKING

import pytest
from analyzers.gate_guard_check import (
    NAME,
    UNASSIGNED,
    Finding,
    GateGuardCheckAnalyzer,
    main,
    premise_findings,
    review,
)
from common.analyzer import AnalyzerBase
from common.file_traversal import find_cpp_files
from common.gate_register import GATE_IDS, dump_gate_register
from validate_gate_register import PREMISES, REGISTER_PATH, load_premises

from ._helpers import TOOLS_DIR
from .test_validate_gate_register import (
    _baseline_rows,
    _blocked_by,
    _inventory_text,
    _register,
    _repository,
)

if TYPE_CHECKING:
    from collections.abc import Sequence
    from pathlib import Path

    from common.analyzer import AnalysisResult

ANALYZER = TOOLS_DIR / "analyzers" / "gate_guard_check.py"

# The script name a direct invocation puts in argv[0], which the usage line names.
SCRIPT = ANALYZER.name

# How the register premise reads when the tree lacks the register, which is the one premise failure
# the repository can reach today and the message the data half reports it with.
MISSING_REGISTER = f"{REGISTER_PATH} does not exist"


def _tree(root: Path) -> Path:
    """A repository root carrying the data half's baseline register and the inventory it names."""
    register = _register()
    return _repository(
        root,
        register=dump_gate_register(register),
        inventory=_inventory_text(_baseline_rows(register), _blocked_by(register)),
    )


def _without_register(root: Path) -> Path:
    """A repository root carrying the schemas and no register, which is the tree today."""
    return _repository(root, register=None, inventory=None)


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

    result = _analyzer(_tree(tmp_path / "repo")).run([])

    assert result.passed
    assert result.issues == 0
    assert REGISTER_PATH in result.output
    assert f"{len(register.guards)} guards" in result.output
    assert f"{len(register.gates)} Gate_Dossiers" in result.output


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

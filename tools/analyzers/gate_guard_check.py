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

Usage:
    python3 gate_guard_check.py [files or directories...]
    python3 gate_guard_check.py aerora/      # a directory is expanded to the C++ files under it
    python3 gate_guard_check.py              # read the paths from stdin (for pre-commit)

Exit codes:
    0 - No findings
    1 - Findings
"""

from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, ClassVar, Final

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from _bootstrap import ensure_tools_dir

ensure_tools_dir(__file__)

from common.analyzer import AnalysisResult, AnalyzerBase
from common.file_traversal import find_cpp_files
from common.gate_register import GATE_IDS
from validate_gate_register import REGISTER_PATH, load_premises

if TYPE_CHECKING:
    from collections.abc import Sequence

    from validate_gate_register import Finding as PremiseFinding
    from validate_gate_register import Premises

# The repository this run reviews, resolved from this file rather than from the working directory,
# so a hook invoking the script from anywhere reads the same register. `run` takes its root from
# the dispatcher instead, which is what lets a test review a tree it laid out itself.
ROOT: Final = Path(__file__).resolve().parents[2]

# The name a finding and the dispatcher's status line carry, which is the hook id as well.
NAME: Final = "gate-guards"

# How a finding names the Gate set where no Gate_Dossier records the element it rejects, which is
# the second half of Requirement 4 criterion 2: name the element as unassigned Decision_Content.
# Spelled out rather than left blank, so that case reads as a sentence instead of a silence.
UNASSIGNED: Final = "unassigned Decision_Content"


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
    """

    check: str
    gates: tuple[str, ...]
    message: str

    def __str__(self) -> str:
        return f"{self.check}: {', '.join(self.gates) or UNASSIGNED}: {self.message}"


def premise_findings(findings: Sequence[PremiseFinding]) -> list[Finding]:
    """The data half's premise failures, reported here as themselves.

    Each keeps the check name the data half rejected it under, because the repair is to the
    register or to a document the register names and never to a source file, and each names all ten
    Gates, because a register that cannot be read leaves every guard unverified.
    """
    return [Finding(finding.check, GATE_IDS, finding.message) for finding in findings]


def _established(premises: Premises, files: int) -> str:
    """What the run established, measured here and now rather than remembered.

    It states the premise and stops there, claiming nothing about the files it selected, because
    nothing has compared one against the register yet.
    """
    return (
        f"{REGISTER_PATH} records {len(premises.register.guards)} guards over "
        f"{len(premises.register.gates)} Gate_Dossiers; {files} source files selected, "
        f"and no rule has read one\n"
    )


def review(root: Path, files: Sequence[Path]) -> AnalysisResult:
    """Review *files* against the register at *root*, in the result shape the dispatcher reads.

    The premise comes first and a failure stops the run there, which is the order the data half
    establishes its premises in and for the same reason: a rule applied to a register that could
    not be read reports noise that hides the real error, and reports it against the wrong document.
    """
    premises = load_premises(root)
    if isinstance(premises, list):
        findings = premise_findings(premises)
        return AnalysisResult(
            tool=NAME,
            passed=False,
            issues=len(findings),
            output="".join(f"{finding}\n" for finding in findings),
            files_checked=len(files),
        )

    return AnalysisResult(
        tool=NAME,
        passed=True,
        output=_established(premises, len(files)),
        files_checked=len(files),
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

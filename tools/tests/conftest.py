"""Shared pytest setup for tools tests."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

# Ensure the tools directory is importable (for common.*, setup.*, etc.)
tools_dir = str(Path(__file__).resolve().parent.parent)
if tools_dir not in sys.path:
    sys.path.insert(0, tools_dir)

# The three Gate_Register suites read the real spec tree and the real overlay tree: the register
# under .kiro/specs/, the screen inventory beside it, and every source file under aerora/. Two CI
# jobs collect this directory and only one of them checks those trees out. ci-scripts.yml uses a
# sparse checkout listing tools/ and .github/ and not .kiro/ or aerora/, because it exists to test
# the tooling rather than the overlay, so without this hook those suites fail there with
# FileNotFoundError on a path the job was never going to have. That reads as a broken test rather
# than as a job that was not asked to run them, which is the opposite of what a failure should say.
#
# Keyed on the trees themselves rather than on a job name or an environment variable, so a checkout
# that does carry them runs the suites wherever it happens: aerora-lint.yml takes a full checkout
# with no sparse filter and runs them for real. A new suite reading the spec tree is added to the
# tuple below and nothing else has to change.
_SPEC_TREE_SUITES = (
    "test_gate_register.py",
    "test_validate_gate_register.py",
    "test_gate_guard_check.py",
)

_REPO_ROOT = Path(__file__).resolve().parent.parent.parent

# One path from each tree, so a checkout carrying one and not the other is reported as incomplete
# rather than quietly running half the suites against a tree that is missing the other half.
_SPEC_TREE_PATHS = (
    _REPO_ROOT / ".kiro" / "specs" / "aerora-decision-gate-closure" / "gate-register.json",
    _REPO_ROOT / "aerora" / "src",
)


def pytest_collection_modifyitems(config: pytest.Config, items: list[pytest.Item]) -> None:
    """Skip the Gate_Register suites when the trees they read are not checked out.

    A skip and not a failure: the suites are correct and the checkout is narrow, and reporting the
    narrow checkout as a test failure would hide the one thing worth knowing, which is that these
    suites did not run here.
    """
    del config
    missing = [path for path in _SPEC_TREE_PATHS if not path.exists()]
    if not missing:
        return
    skip = pytest.mark.skip(
        reason=(
            "the Gate_Register suites read trees this checkout does not carry: "
            + ", ".join(str(path.relative_to(_REPO_ROOT)) for path in missing)
        )
    )
    for item in items:
        if item.path.name in _SPEC_TREE_SUITES:
            item.add_marker(skip)

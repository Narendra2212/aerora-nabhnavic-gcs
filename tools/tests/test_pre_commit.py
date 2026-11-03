"""Tests for tools/pre_commit.py."""

from __future__ import annotations

import os
import re
from unittest.mock import patch

import pytest
import yaml
from pre_commit import build_precommit_args, extract_hook_lines, parse_args, summarize_output

from ._helpers import REPO_ROOT


def test_summarize_output_counts_states() -> None:
    passed, failed, skipped = summarize_output(
        "hook-a........................Passed\nhook-b........................Failed\nhook-c........................Skipped\n"
    )
    assert (passed, failed, skipped) == (1, 1, 1)


def test_extract_hook_lines_strips_ansi() -> None:
    lines = extract_hook_lines("\x1b[31mhook-a........................Failed\x1b[0m\n")
    assert lines == ["hook-a........................Failed"]


def test_build_precommit_args_all_files() -> None:
    args = parse_args([])
    with patch("pre_commit.get_default_branch_ref", return_value="master"):
        assert build_precommit_args(args)[-1] == "--all-files"


def test_build_precommit_args_changed_mode() -> None:
    args = parse_args(["--changed"])
    with patch("pre_commit.get_default_branch_ref", return_value="master"):
        built = build_precommit_args(args)
    assert built[-4:] == ["--from-ref", "master", "--to-ref", "HEAD"]


def test_build_precommit_args_changed_main_branch() -> None:
    args = parse_args(["--changed"])
    with patch("pre_commit.get_default_branch_ref", return_value="main"):
        built = build_precommit_args(args)
    assert built[-4:] == ["--from-ref", "main", "--to-ref", "HEAD"]


def test_build_precommit_args_changed_no_ref() -> None:
    args = parse_args(["--changed"])
    with patch("pre_commit.get_default_branch_ref", return_value=None):
        built = build_precommit_args(args)
    assert built[-1] == "--all-files"


def test_changed_files_use_pr_head_instead_of_merge_checkout() -> None:
    with patch.dict(os.environ, {"PR_BASE_SHA": "base-sha", "PR_HEAD_SHA": "head-sha"}):
        built = build_precommit_args(parse_args(["--changed"]))
    assert built[-4:] == ["--from-ref", "base-sha", "--to-ref", "head-sha"]


def test_summary_counts_colored_hook_results_and_ignores_diagnostics():
    output = (
        "format...........\x1b[41mFailed\x1b[0m\n"
        "python...........\x1b[42mPassed\x1b[0m\n"
        "qml..............(no files)\x1b[46mSkipped\x1b[0m\n"
        "Failed to load a file\n"
    )
    assert summarize_output(output) == (1, 1, 1)


# ---------------------------------------------------------------------------
# The three Aerora Gate_Review hooks
#
# A hook whose `files` pattern misses the register is a hook that never runs, and a hook carrying
# `stages: [manual]` rejects nothing. Both are silent: the config stays valid, `pre-commit run`
# reports no failure, and the gate is simply not there. So the wiring is asserted rather than
# reviewed.
# ---------------------------------------------------------------------------

GATE_HOOK_IDS = ("gate-register-schema", "gate-register", "gate-guards")

REGISTER_PATH = ".kiro/specs/aerora-decision-gate-closure/gate-register.json"
INVENTORY_PATH = ".kiro/specs/05-product-completion/03-gui-information-architecture-and-screens.md"


def _gate_hooks() -> dict[str, dict[str, object]]:
    """The three Gate_Review hooks, keyed by the id or alias each is reached under.

    Read out of the real `.pre-commit-config.yaml` rather than a fixture: what these tests are about
    is whether the repository's own config wires the gate, so a fixture would assert the fixture.
    """
    config = yaml.safe_load((REPO_ROOT / ".pre-commit-config.yaml").read_text(encoding="utf-8"))
    found: dict[str, dict[str, object]] = {}
    for repo in config["repos"]:
        for hook in repo.get("hooks", []):
            reached = hook.get("alias") or hook["id"]
            if reached in GATE_HOOK_IDS:
                found[reached] = hook
    return found


def test_every_gate_review_hook_is_configured() -> None:
    assert sorted(_gate_hooks()) == sorted(GATE_HOOK_IDS)


@pytest.mark.parametrize("hook_id", GATE_HOOK_IDS)
def test_a_gate_review_hook_is_gating_rather_than_manual(hook_id: str) -> None:
    """`stages: [manual]` would leave the hook configured and never run on a commit.

    Requirement 4 criterion 2 asks for the change to be rejected *before* it enters the repository,
    and a hook that only runs when asked for by name rejects nothing.
    """
    hook = _gate_hooks()[hook_id]
    args = hook.get("args")

    assert "stages" not in hook
    assert not isinstance(args, list) or "--advisory" not in args


@pytest.mark.parametrize(
    ("hook_id", "path"),
    [
        ("gate-register-schema", REGISTER_PATH),
        ("gate-register", REGISTER_PATH),
        ("gate-register", INVENTORY_PATH),
        ("gate-register", "tools/validate_gate_register.py"),
        ("gate-register", "tools/common/gate_register.py"),
        ("gate-guards", REGISTER_PATH),
        ("gate-guards", "aerora/src/UI/Screens/AeroraOperationsHub.qml"),
        ("gate-guards", "aerora/src/Power/AeroraPowerModel.h"),
        ("gate-guards", "aerora/test/Mission/AeroraMissionTest.cc"),
        ("gate-guards", "aerora/CMakeLists.txt"),
        ("gate-guards", "src/Vehicle/Vehicle.h"),
    ],
)
def test_a_gate_review_hook_watches_the_path_its_rules_read(hook_id: str, path: str) -> None:
    """A pattern that misses a file the rules read is a gate with a hole in it.

    `src/Vehicle/Vehicle.h` is the sharpest of these: it is the one file outside the overlay any of
    these rules opens, `adr-016-pair` reads it, and a pattern scoped to `aerora/` alone would have
    looked correct while leaving the core edit unwatched.
    """
    pattern = _gate_hooks()[hook_id]["files"]
    assert isinstance(pattern, str)

    assert re.search(pattern, path), f"{hook_id} does not watch {path}"


@pytest.mark.parametrize(
    ("hook_id", "expected"),
    [("gate-register-schema", False), ("gate-register", False), ("gate-guards", True)],
)
def test_a_gate_review_hook_passes_filenames_only_where_its_rules_select(
    hook_id: str, expected: bool
) -> None:
    """The two whole-document checks read the register and not a selection; the source half does.

    Recorded per hook because getting it backwards is quiet in both directions: a
    `pass_filenames: true` on the register check would hand it paths it ignores, and a `false` on
    the guard check would hand it nothing to count as reviewed.
    """
    assert _gate_hooks()[hook_id].get("pass_filenames") is expected

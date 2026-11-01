"""Git helpers for QGC tooling.

Centralises the ``["git", ...]`` subprocess shelling used by CI scripts and
dev tools. Provides default-branch discovery and add-commit resolution plus a
generic ``run_git`` wrapper backed by :mod:`common.proc`.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING, Final

from .proc import run_captured

if TYPE_CHECKING:
    import subprocess
    from pathlib import Path

__all__ = ["AddCommit", "get_default_branch_ref", "resolve_add_commit", "run_git"]

_FALLBACK_REFS: tuple[str, ...] = ("master", "main", "origin/master", "origin/main")

# The hash and the trailers in one invocation: the first output line is the commit and every line
# after it is one trailer. ``only`` drops the non-trailer lines of the message, and ``unfold`` puts
# a folded multi-line trailer back on a single line so a caller splits on nothing.
_ADD_COMMIT_FORMAT: Final = "%H%n%(trailers:only,unfold)"


def run_git(
    *args: str,
    cwd: Path | str | None = None,
    check: bool = False,
    timeout: float | None = None,
) -> subprocess.CompletedProcess[str]:
    """Run ``git *args`` capturing stdout/stderr as text. Thin wrapper over run_captured."""
    return run_captured(["git", *args], cwd=cwd, check=check, timeout=timeout)


def get_default_branch_ref(repo_root: Path | None = None) -> str | None:
    """Return the local name of the default branch, or None if undiscoverable.

    Tries ``refs/remotes/origin/HEAD`` first (post-clone canonical), then
    probes the usual ``master``/``main`` variants. ``repo_root`` selects the
    git directory; ``None`` uses the caller's CWD.
    """
    head = run_git("symbolic-ref", "refs/remotes/origin/HEAD", "--short", cwd=repo_root)
    if head.returncode == 0:
        return head.stdout.strip().removeprefix("origin/")

    for ref in _FALLBACK_REFS:
        probe = run_git("rev-parse", "--verify", ref, cwd=repo_root)
        if probe.returncode == 0:
            return ref
    return None


@dataclass(frozen=True)
class AddCommit:
    """What git history says about the commit that added a path.

    Either ``sha`` names that commit and ``trailers`` carries the trailer lines it holds, or ``sha``
    is ``None`` and ``reason`` says why history could not answer. A caller therefore reports the
    reason rather than reading an unanswerable history as an answer.
    """

    sha: str | None
    trailers: tuple[str, ...]
    reason: str | None


def _first_line(text: str) -> str:
    """The first non-empty line of *text*, which is where git writes its reason."""
    return next((line.strip() for line in text.splitlines() if line.strip()), "git gave no reason")


def resolve_add_commit(path: str, *, repo_root: Path | str | None = None) -> AddCommit:
    """Resolve the commit that added *path*, together with the trailers that commit carries.

    ``git log`` reports the newest commit first, so the resolved commit is the one at which the
    file as it stands entered: a path added, deleted and added again entered at the latest add.

    *path* is a pathspec relative to *repo_root*, which selects the git directory; ``None`` uses
    the caller's CWD. Every branch that cannot resolve a commit names its own reason, so a caller
    reporting it says which of them it hit.
    """
    try:
        repository = run_git("rev-parse", "--git-dir", cwd=repo_root)
    except OSError as error:
        return AddCommit(sha=None, trailers=(), reason=f"git could not be run: {error.strerror}")
    if repository.returncode != 0:
        return AddCommit(sha=None, trailers=(), reason="the tree is not a git repository")

    log = run_git(
        "log",
        "-1",
        "--diff-filter=A",
        f"--format={_ADD_COMMIT_FORMAT}",
        "--",
        path,
        cwd=repo_root,
    )
    if log.returncode != 0:
        return AddCommit(
            sha=None,
            trailers=(),
            reason=f"git could not read this repository's history: {_first_line(log.stderr)}",
        )

    lines = log.stdout.splitlines()
    if not lines:
        shallow = run_git("rev-parse", "--is-shallow-repository", cwd=repo_root)
        if shallow.stdout.strip() == "true":
            return AddCommit(
                sha=None,
                trailers=(),
                reason=(
                    "the repository is a shallow clone, so its history does not reach the commit "
                    "that added it"
                ),
            )
        return AddCommit(
            sha=None, trailers=(), reason="no commit in this repository's history adds it"
        )

    sha, *trailers = lines
    return AddCommit(
        sha=sha.strip(),
        trailers=tuple(line.strip() for line in trailers if line.strip()),
        reason=None,
    )

#!/usr/bin/env python3
"""Tests for the qmllint analyzer's source-to-built-module path mapping."""

from __future__ import annotations

from typing import TYPE_CHECKING

from analyzers.qmllint import QmlLintAnalyzer

if TYPE_CHECKING:
    from pathlib import Path


def make_analyzer(tmp_path: Path, build_aware: bool = True) -> QmlLintAnalyzer:
    analyzer = QmlLintAnalyzer(tmp_path, tmp_path / "build")
    analyzer.build_aware = build_aware
    return analyzer


def write_qml(directory: Path, *names: str) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    for name in names:
        (directory / name).touch()


class TestBuiltModuleCopy:
    def test_flattened_module_is_linted_from_the_build_tree(self, tmp_path: Path) -> None:
        """A module split across source directories must be linted as the build flattens it."""
        screens = tmp_path / "aerora" / "src" / "UI" / "Screens"
        write_qml(screens, "AeroraCapabilityPage.qml")
        write_qml(tmp_path / "aerora" / "src" / "UI" / "Controls", "AeroraSelfReportRow.qml")

        module = tmp_path / "build" / "qml" / "Aerora" / "Controls"
        write_qml(module, "AeroraCapabilityPage.qml", "AeroraSelfReportRow.qml")

        analyzer = make_analyzer(tmp_path)
        built = analyzer.built_module_copy(
            screens / "AeroraCapabilityPage.qml", tmp_path / "build" / "qml"
        )

        assert built == module / "AeroraCapabilityPage.qml"

    def test_matching_layout_keeps_the_source_path(self, tmp_path: Path) -> None:
        """When source and module layouts agree there is nothing to correct."""
        source = tmp_path / "src" / "QmlControls"
        write_qml(source, "QGCLabel.qml", "QGCButton.qml")

        module = tmp_path / "build" / "qml" / "QGroundControl" / "Controls"
        write_qml(module, "QGCLabel.qml", "QGCButton.qml")

        analyzer = make_analyzer(tmp_path)
        assert (
            analyzer.built_module_copy(source / "QGCLabel.qml", tmp_path / "build" / "qml") is None
        )

    def test_ambiguous_basename_keeps_the_source_path(self, tmp_path: Path) -> None:
        """The same file name in two modules says nothing about which one owns this file."""
        screens = tmp_path / "aerora" / "src" / "UI" / "Screens"
        write_qml(screens, "Page.qml")

        write_qml(tmp_path / "build" / "qml" / "Aerora" / "Controls", "Page.qml", "Row.qml")
        write_qml(tmp_path / "build" / "qml" / "Other" / "Module", "Page.qml", "Row.qml")

        analyzer = make_analyzer(tmp_path)
        assert analyzer.built_module_copy(screens / "Page.qml", tmp_path / "build" / "qml") is None

    def test_absent_build_tree_keeps_the_source_path(self, tmp_path: Path) -> None:
        """Nothing to map against, and no exception for the missing directory."""
        screens = tmp_path / "aerora" / "src" / "UI" / "Screens"
        write_qml(screens, "Page.qml")

        analyzer = make_analyzer(tmp_path)
        assert analyzer.built_module_copy(screens / "Page.qml", tmp_path / "build" / "qml") is None


class TestLintTargets:
    def test_source_only_mode_never_remaps(self, tmp_path: Path) -> None:
        """Without --qml-build there is no build tree to read, so the source is the target."""
        screens = tmp_path / "aerora" / "src" / "UI" / "Screens"
        write_qml(screens, "Page.qml")
        write_qml(tmp_path / "build" / "qml" / "Aerora" / "Controls", "Page.qml", "Row.qml")

        analyzer = make_analyzer(tmp_path, build_aware=False)
        page = screens / "Page.qml"

        assert analyzer._lint_targets([page]) == [(page, page)]

    def test_build_aware_mode_pairs_source_with_built_copy(self, tmp_path: Path) -> None:
        """The source path is kept alongside the target so findings can name it."""
        screens = tmp_path / "aerora" / "src" / "UI" / "Screens"
        write_qml(screens, "Page.qml")

        module = tmp_path / "build" / "qml" / "Aerora" / "Controls"
        write_qml(module, "Page.qml", "Row.qml")

        analyzer = make_analyzer(tmp_path)
        page = screens / "Page.qml"

        assert analyzer._lint_targets([page]) == [(page, module / "Page.qml")]

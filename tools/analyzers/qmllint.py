"""qmllint QML linter."""

from __future__ import annotations

import re
import shutil
from typing import TYPE_CHECKING, ClassVar

from common.analyzer import AnalysisResult, AnalyzerBase
from common.logging import log_info, log_warn
from common.proc import run_captured

if TYPE_CHECKING:
    from pathlib import Path


class QmlLintAnalyzer(AnalyzerBase):
    """QML file linter."""

    build_aware: bool = False
    name: ClassVar[str] = "qmllint"
    install_hint: ClassVar[str] = (
        "Install with: Qt SDK or 'sudo apt install qt6-declarative-dev-tools'"
    )

    def built_module_copy(self, file: Path, module_root: Path) -> Path | None:
        """Return the built copy of `file` to lint instead of the source, or None to lint source.

        A QML module member resolves its sibling types through the implicit import of its own
        directory, so qmllint sees what the application sees only when the directory it reads is
        the module as built. For most modules those are the same directory and nothing here
        applies.

        Aerora's module is this repository's exception. ``qgc_set_qt_resource_alias()`` flattens
        ``Controls/``, ``Indicators/`` and ``Screens/`` into one module directory
        (``aerora/src/UI/Ui.cmake``), so a file in ``Screens/`` has no sibling
        ``AeroraSelfReportRow.qml`` on disk and every cross-directory reference is reported as a
        missing type - the tool reading a layout the application never loads.

        Two conditions, both narrowing:

        * the built directory must offer QML files the source directory does not, which is what
          makes implicit-import resolution differ in the first place;
        * the basename must be unambiguous under the module root, because the same file name in
          two modules says nothing about which one this file belongs to.
        """
        if not module_root.is_dir():
            return None

        candidates = [path for path in module_root.rglob(file.name) if path.is_file()]
        if len(candidates) != 1:
            return None

        built = candidates[0]
        source_siblings = {path.name for path in file.parent.glob("*.qml")}
        built_siblings = {path.name for path in built.parent.glob("*.qml")}
        if built_siblings <= source_siblings:
            return None

        return built

    def _lint_targets(self, files: list[Path]) -> list[tuple[Path, Path]]:
        """Pair each collected source file with the path to hand qmllint."""
        if not self.build_aware:
            return [(file, file) for file in files]

        module_root = self.build_dir / "qml"
        targets = [(file, self.built_module_copy(file, module_root) or file) for file in files]

        remapped = sum(1 for source, target in targets if source != target)
        log_info(
            f"{remapped} of {len(files)} file(s) linted from the built module tree, "
            "where a module's layout differs from the source tree"
        )

        return targets

    def run(self, files: list[Path], fix: bool = False) -> AnalysisResult:
        if shutil.which("qmllint") is None:
            log_warn("qmllint not found")
            log_info(self.install_hint)
            return AnalysisResult(
                tool=self.name, passed=False, execution_error=True, output="qmllint not found"
            )

        if not files:
            log_info("No QML files to lint")
            return AnalysisResult(tool=self.name, passed=True)

        extra_args: list[str] = ["--bare"]
        if self.build_aware:
            imports = self.build_dir / "qml"
            if not any(imports.rglob("*.qmltypes")):
                return AnalysisResult(
                    tool=self.name,
                    passed=False,
                    execution_error=True,
                    output="Build QGC first to generate QML module type information",
                )
            extra_args = [
                "-I",
                str(imports),
                "--import",
                "error",
                "--missing-property",
                "error",
                "--missing-type",
                "error",
            ]

        log_info(f"Running qmllint on {len(files)} files...")

        issues_found = False
        error_findings = False
        files_with_issues: list[str] = []
        all_output: list[str] = []

        for source, target in self._lint_targets(files):
            result = run_captured(["qmllint", *extra_args, str(target)])

            output = result.stdout + result.stderr
            if target != source:
                # Findings have to name the file a developer edits rather than its build-tree
                # copy. The copy is the same file, so line and column numbers carry over.
                output = output.replace(str(target), str(source))

            error_findings |= bool(re.search(r"^Error:", output, re.MULTILINE))
            if output.strip():
                print(output)
                all_output.append(output)

            if result.returncode != 0:
                issues_found = True
                files_with_issues.append(self.relative_path(source))

        if issues_found:
            log_warn("QML lint issues found")

        return AnalysisResult(
            tool=self.name,
            error_findings=error_findings,
            passed=not issues_found,
            issues=len(files_with_issues),
            output="\n".join(all_output),
            files_checked=len(files),
            files_with_issues=files_with_issues,
        )

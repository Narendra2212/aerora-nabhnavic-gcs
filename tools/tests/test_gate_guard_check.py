#!/usr/bin/env python3
"""Tests for the shell and the source-half rules in tools/analyzers/gate_guard_check.py.

Two things are under test, and they are tested differently.

The shell: that it is an `AnalyzerBase` the dispatcher can run, that `fix` is accepted and writes
nothing, that arguments and stdin reach the review as the file list a rule will read, that a
directory is expanded by `find_cpp_files` rather than by something local to the analyzer, and that
the register premise is established before anything else and reported as itself when it cannot be.
That last case is asserted over a tree laid out without a register rather than over the repository,
which now carries one: the spec's governing rule is that an unresolved premise is a failure and
never a pass, so the shell's answer in that state is worth pinning whether or not the real register
is present.

The rules: `guard-value`, which reads each recorded guard's declaration in its recorded `declaredIn`
file; `guard-assertion`, which reads each recorded assertion site under `aerora/test/**`;
`adr-016-pair`, which reads the two files Requirement 7 criterion 17 names against the change
permission the registry records on the fact-group registrar's guard; `mavlink-identifier`, which
walks the overlay tree against an allow-list the register records by site and reason; the four
absence rules, which walk `aerora/src/**` for a subject that must not be there; and
`actionable-control`, which walks the Aerora QML documents and compares the controls they declare
against the controls the read-only posture records. None of them reads the files the run selected, so
the fixture that decides what a rule sees is not the file list but the tree.

The four absence rules are tested on three axes each, because each is a different answer: the
subject absent, which is the state the tree stands in; the subject present, which is the rejection;
and the Gate the rule waits on recorded closed, in which the subject is present and the rule is
silent because what it rejects is then the thing an owner signed for. The Gate-closed cases reach
the register as a model rather than as a document, for the reason `_authorised` does -- a closed
Gate needs an accepted part with a Closure_Record, which `_accepting` builds in code.

What most of these cases actually assert is the *negative*: that a legitimate mention of a subject's
own name is not the subject. The fixture tree carries every one the real tree does -- the override
allow-list's comment block naming the forbidden tier and both pre-approved seams in prose, the count
that compares an entry's tier against the tier in code, the threshold setter's declaration and its
out-of-line definition, the payload command sink's own declaration, a pointer member of that type, a
setter taking one, and the one implementation in the tree, which is a test double under
`aerora/test/**`. A rule that fired on any of them would fail on a clean tree and be removed rather
than obeyed, so the fixture is written to look like the tree rather than to agree with the rule, and
each accepting case asserts that the mention is there before asserting that nothing was reported.

One case per rule reads the **real** tree with a fixture register, which is the only evidence that
the four read the overlay rather than the fixtures: it is what fails the day `AeroraFreshness.cc`'s
out-of-line definition starts reading as a call site.

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

The last section is the two rules that answer per registry entry rather than per tree, and it is
parametrised over every `literal` guard the register records instead of over one of them:
`guard-value` resolves a declaration per entry and `guard-assertion` a site per entry, so a guard
whose file either of them stopped finding would go quiet on its own while the rest kept passing.
`contract-surface` is quantified rather than parametrised, because what it asserts is a set equality
and the interesting cases are the combinations of declared and recorded across several elements at
once.
"""

from __future__ import annotations

import io
import re
import subprocess
import sys
from collections import Counter
from dataclasses import replace
from typing import TYPE_CHECKING

import pytest
from analyzers.gate_guard_check import (
    ACTION_SEAM,
    ACTIONABLE_CONTROL,
    ADR_016_PAIR,
    AUTONOMY_SOURCE,
    CONSTANT_KIND,
    CONTRACT_SURFACE,
    CORE_HEADER,
    CUSTOM_DEFINITIONS,
    ENUMERATOR_KIND,
    FRESHNESS_THRESHOLD,
    FRIEND_DECLARATION,
    FRIENDSHIP_DEFINITION,
    GUARD_ASSERTION,
    GUARD_VALUE,
    IDENTIFIER_PREFIXES,
    MAVLINK_IDENTIFIER,
    NAME,
    OVERLAY_ISOLATION,
    OVERLAY_MANIFEST,
    OVERRIDE_ENTRIES,
    PAYLOAD_COMMAND_SINK,
    PAYLOAD_COMMAND_SINK_TYPE,
    POPULATED_FILE_OVERRIDE,
    POPULATED_FILE_TIER,
    REGISTRY_KEY_KIND,
    REPORTED,
    SIMULATED_SOURCE,
    SOURCE_TREE,
    THRESHOLD_SETTER,
    UNASSIGNED,
    Finding,
    GateGuardCheckAnalyzer,
    _isolation_candidates,
    _source_tree,
    _surface,
    check_action_seam,
    check_actionable_control,
    check_adr_016_pair,
    check_autonomy_source,
    check_contract_surface,
    check_freshness_threshold,
    check_guard_assertion,
    check_guard_value,
    check_mavlink_identifier,
    check_overlay_isolation,
    check_payload_command_sink,
    check_populated_file_override,
    check_simulated_source,
    main,
    premise_findings,
    review,
)
from common.analyzer import AnalyzerBase
from common.file_traversal import find_cpp_files
from common.gate_register import (
    CHECK_GUARD,
    GATE_IDS,
    LITERAL_GUARD,
    NO_GATE,
    PREDICATE_GUARD,
    ActionableControlSite,
    ContractSurfaceElement,
    Disagreement,
    MavlinkIdentifierSite,
    dump_gate_register,
    part_reference,
)
from hypothesis import given, settings
from hypothesis import strategies as st
from validate_gate_register import (
    FACT_GROUP_REGISTRAR_GUARD,
    GUARD_REGISTRY,
    MAVLINK_IDENTIFIER_GUARD,
    MECHANICAL_GUARDS,
    OPEN_STATUS,
    OVERLAY_TREE,
    PRE_APPROVED_SEAMS,
    PREMISES,
    REGISTER_PATH,
    TEST_TREE,
    load_premises,
)

from ._helpers import REPO_ROOT, TOOLS_DIR
from ._strategies import MAX_EXAMPLES
from .test_validate_gate_register import (
    ACTUATOR_PATH_GUARD,
    ANOTHER_OUTSIDE_PATH,
    ASSERTION_REMEDY,
    BASELINE_GUARD_GATES,
    BASELINE_PART,
    EXCEPTION_CONTROL,
    EXCEPTION_DOCUMENT,
    EXCEPTION_SCREEN,
    GUARD_DECLARATION,
    MISSION_GATE,
    OUTSIDE_PATH,
    PAYLOAD_GATE,
    POWER_GUARD,
    PREDICATE_EXPRESSION,
    REPLAY_GUARD,
    ROLE_GATE,
    SEAM_DIRECTORY,
    SINGLE_GATE,
    SITE_CONTROL,
    SITE_DOCUMENT,
    SITE_HANDLER,
    SITE_SCREEN,
    TELEMETRY_GUARD,
    _accepting,
    _baseline_rows,
    _blocked_by,
    _edit,
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
    """The header the register names as declaring *guards*, each carrying its recorded literal.

    A guard recording no value declares nothing here, which is what a `check`-kind guard is: it is
    held by a rule, so no file declares it and there is no literal to write. `_guard_sources` leaves
    it out for the same reason, stated from the other side.
    """
    declarations = "\n".join(
        _declares(guard.symbol, guard.recorded_value)
        for guard in guards
        if guard.recorded_value is not None
    )
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


# ---------------------------------------------------------------------------
# The four absent subjects
# ---------------------------------------------------------------------------

# How many Gate-conditional rules the run's summary counts -- the four absence rules,
# `actionable-control`, and the two rules holding a guard whose content is an absence -- and the
# Gates the rest of this file does not already carry. Spelled here rather than imported from the
# analyzer: a test that read the rule's own table of Gates, or its own count of itself, would agree
# with it by construction. `ROLE_GATE` and `PAYLOAD_GATE` the data half's fixtures already name.
CONDITIONAL_RULES = 7
VIDEO_GATE = "DG-VID"
FRESHNESS_GATE = "DG-FRS"
AUTONOMY_GATE = "DG-AUT"
SIMULATION_GATE = "DG-SIM"

# Where the fixture tree carries each subject, under the tree the four rules walk. Fixture names,
# because no rule reads a recorded path for any of them: the two allow-list rules find the
# definition by walking, and the other two search every source file. The real-tree cases below are
# what prove the rules find the real files.
OVERRIDES_SOURCE = f"{SOURCE_TREE}/UI/AFixtureQmlOverrides.cc"
FRESHNESS_HEADER = f"{SOURCE_TREE}/Telemetry/AFixtureFreshness.h"
FRESHNESS_SOURCE = f"{SOURCE_TREE}/Telemetry/AFixtureFreshness.cc"
SINK_INTERFACE = f"{SOURCE_TREE}/Payload/AFixturePayloadCommandSink.h"
SINK_HOLDER = f"{SOURCE_TREE}/Payload/AFixturePayloadModel.h"
SINK_IMPLEMENTATION = f"{TEST_TREE}/Payload/AFixturePayloadModelTest.cc"

# How a fixture source includes the interface, which is the file name of the header above.
SINK_HEADER_NAME = SINK_INTERFACE.rpartition("/")[2]

# Where a rejecting case writes the subject it asserts a finding for: a file the baseline does not
# carry, so the tree under review is the agreeing one plus the one file the test wrote.
ADDED_SOURCE = f"{SOURCE_TREE}/Payload/AFixtureAddedSource.cc"
ADDED_TEST_SOURCE = f"{TEST_TREE}/Payload/AFixtureAddedTest.cc"

# The four subjects where the repository actually holds them, read by the real-tree case of each
# rule. Spelled here rather than discovered, because what those cases are evidence of is that the
# rules read *these* files: the day `AeroraFreshness.cc`'s out-of-line definition starts reading as
# a call site, the case that fails has to be the one naming it.
REAL_OVERRIDES = f"{SOURCE_TREE}/UI/AeroraQmlOverrides.cc"
REAL_FRESHNESS_HEADER = f"{SOURCE_TREE}/Telemetry/AeroraFreshness.h"
REAL_FRESHNESS_SOURCE = f"{SOURCE_TREE}/Telemetry/AeroraFreshness.cc"
REAL_SINK_INTERFACE = f"{SOURCE_TREE}/Payload/AeroraPayloadCommandSink.h"
REAL_SINK_HOLDER = f"{SOURCE_TREE}/Payload/AeroraPayloadModel.h"
REAL_SINK_IMPLEMENTATION = f"{TEST_TREE}/Payload/AeroraPayloadModelTest.cc"

# The one entry this build declares and the tier it carries. `FlyViewCustomLayer.qml` is the third
# pre-approved seam and is legitimately in the allow-list -- it is an empty upstream seam carrying
# no action -- so the baseline declares it and the two rules passing over it is the baseline rather
# than a case of its own. A rule that confused the three seams, or that read any declared entry as
# a populated-file fork, would reject the tree it exists to protect.
DECLARED_SEAM = f"{SEAM_DIRECTORY}/FlyViewCustomLayer.qml"
PURPOSE_BUILT_TIER = "Tier::PurposeBuiltSeam"

# The entry a rejecting case adds: a fork of a populated upstream file, which is a fixture path
# rather than a file upstream carries. What makes it the subject is the tier beside it.
FORKED_FILE = f"{SEAM_DIRECTORY}/AFixtureForkedFile.qml"

# The type the fixture documents declare their control as, and the handler the excepted one carries.
# `QGCButton` is the type both real controls use, and it is the one spelling that proves the rule
# matches a QGC control rather than only a bare Qt Quick `Button`.
CONTROL_TYPE = "QGCButton"
COPY = "AFixtureGlobal.copyToClipboard(root._report)"

# A document recorded nowhere and declaring no actionable control, which is the state of eighteen of
# the twenty real documents.
QUIET_DOCUMENT = f"{SOURCE_TREE}/UI/Screens/AFixtureQuietPage.qml"

# Where a rejecting case writes a document the posture records nothing for.
ADDED_DOCUMENT = f"{SOURCE_TREE}/UI/Screens/AFixtureAddedPage.qml"

# The two real documents that carry the tree's only two actionable controls, and the control each
# carries. Spelled here rather than discovered, because what the real-tree case is evidence of is
# that the rule finds *these* two: a rule that silently stopped matching `QGCButton` would pass a
# discovered-set assertion and fail this one.
REAL_EXCEPTION_DOCUMENT = f"{SOURCE_TREE}/UI/Screens/AeroraErrorReportPage.qml"
REAL_EXCEPTION_CONTROL = "aeroraErrorReport_copyButton"
REAL_SITE_DOCUMENT = f"{SOURCE_TREE}/UI/Screens/AeroraOperationsHub.qml"
REAL_SITE_CONTROL = "aeroraHub_vehicleConfigButton"
REAL_SITE_HANDLER = "mainWindow.showVehicleConfig()"


def _override_entries(*entries: tuple[str, str]) -> str:
    """The override allow-list as the overlay writes it, returning *entries* as path and tier.

    Every legitimate mention `aerora/src/UI/AeroraQmlOverrides.cc` carries is here: the comment
    block inside the function names the forbidden tier and both pre-approved seams in prose, and
    `populatedFileOverrideCount()` compares an entry's tier against the forbidden tier in code. So
    a rule reading the file rather than the returned list reports a finding against this fixture,
    and the accepting cases below are evidence that the span is the list.
    """
    declared = "".join(f'        {{QStringLiteral("{path}"), {tier}}},\n' for path, tier in entries)
    undeclared = "".join(f"    //       {seam}\n" for seam in PRE_APPROVED_SEAMS)
    return (
        '#include "AFixtureQmlOverrides.h"\n'
        "\n"
        f"QList<AeroraQmlOverrides::OverrideEntry> {OVERRIDE_ENTRIES}\n"
        "{\n"
        "    // A fixture comment block standing where the overlay's own does.\n"
        "    //\n"
        "    // The other two pre-approved seams stay UNDECLARED, because both exist to introduce\n"
        "    // operator actions:\n"
        f"{undeclared}"
        "    //\n"
        f"    // A {POPULATED_FILE_TIER} entry additionally requires a recorded reason, a\n"
        "    // fork-point commit and a named owner before it is added.\n"
        "    return {\n"
        f"{declared}"
        "    };\n"
        "}\n"
        "\n"
        "int AeroraQmlOverrides::populatedFileOverrideCount()\n"
        "{\n"
        "    int count = 0;\n"
        "\n"
        "    for (const OverrideEntry &entry : overrideEntries()) {\n"
        f"        if (entry.tier == {POPULATED_FILE_TIER}) {{\n"
        "            ++count;\n"
        "        }\n"
        "    }\n"
        "\n"
        "    return count;\n"
        "}\n"
    )


def _freshness_declaration() -> str:
    """The setter declared in a header, which is a declaration and never a call site."""
    return (
        "#pragma once\n"
        "\n"
        "class AFixtureFreshness\n"
        "{\n"
        "public:\n"
        f"    /// A fixture doc comment naming {THRESHOLD_SETTER}() in prose.\n"
        f"    void {THRESHOLD_SETTER}(const QString &freshnessClass, "
        "std::chrono::microseconds threshold);\n"
        "};\n"
    )


def _freshness_definition() -> str:
    """The setter defined out of line, which is a definition and never a call site."""
    return (
        '#include "AFixtureFreshness.h"\n'
        "\n"
        f"void AFixtureFreshness::{THRESHOLD_SETTER}(const QString &freshnessClass, "
        "std::chrono::microseconds threshold)\n"
        "{\n"
        "    _thresholds.insert(freshnessClass, threshold);\n"
        "}\n"
    )


def _threshold_call(reached_by: str) -> str:
    """A fixture source calling the setter, reaching it the way *reached_by* spells.

    The arguments are fixture calls rather than values: the class a call names and the budget it
    passes are Decision_Content, and a threshold value is a Contract_Value outright, so nothing
    here -- and nothing in a finding about it -- writes one.
    """
    return (
        '#include "AFixtureFreshness.h"\n'
        "\n"
        "void aFixtureFunction(AFixtureFreshness *freshness)\n"
        "{\n"
        f"    {reached_by}{THRESHOLD_SETTER}(aFixtureClass(), aFixtureBudget());\n"
        "}\n"
    )


def _sink_interface() -> str:
    """The interface itself: declared, and deliberately carrying no implementation."""
    return (
        "#pragma once\n"
        "\n"
        f"/// A fixture doc comment naming {PAYLOAD_COMMAND_SINK_TYPE} in prose.\n"
        f"class {PAYLOAD_COMMAND_SINK_TYPE}\n"
        "{\n"
        "public:\n"
        f"    virtual ~{PAYLOAD_COMMAND_SINK_TYPE}() = default;\n"
        "\n"
        "    [[nodiscard]] virtual bool submitPayloadRequest(const AFixtureRequest &r) = 0;\n"
        "};\n"
    )


def _sink_holder() -> str:
    """A class holding the sink as a pointer member and taking one as a parameter.

    Both are the shape `aerora/src/Payload/AeroraPayloadModel.h` has, and neither is an
    implementation: the member is a pointer the overlay never assigns, and the parameter belongs to
    the setter that would assign it. The class derives from something else, so the base-class list
    the rule reads is there and names another type.
    """
    return (
        "#pragma once\n"
        "\n"
        f'#include "{SINK_HEADER_NAME}"\n'
        "\n"
        f"/// Nothing in the fixture tree implements {PAYLOAD_COMMAND_SINK_TYPE}, so the sink is\n"
        "/// null for the lifetime of this object.\n"
        "class AFixturePayloadModel : public AFixtureBase\n"
        "{\n"
        "public:\n"
        f"    void setCommandSink({PAYLOAD_COMMAND_SINK_TYPE} *sink);\n"
        "\n"
        "private:\n"
        f"    {PAYLOAD_COMMAND_SINK_TYPE} *_commandSink = nullptr;\n"
        "};\n"
    )


def _sink_implementation(head: str) -> str:
    """A class whose head is *head*, which derives from the interface and so implements it."""
    return (
        f'#include "{SINK_HEADER_NAME}"\n'
        "\n"
        "/// A fixture double recording what it was handed.\n"
        f"{head}\n"
        "{\n"
        "public:\n"
        "    bool submitPayloadRequest(const AFixtureRequest &r) override\n"
        "    {\n"
        "        _requests.append(r);\n"
        "        return true;\n"
        "    }\n"
        "};\n"
    )


# The derivation the fixture test double is written with, which is the shape
# `aerora/test/Payload/AeroraPayloadModelTest.cc` uses for the one implementation in the tree.
RECORDING_SINK_HEAD = f"class ARecordingFixtureSink : public {PAYLOAD_COMMAND_SINK_TYPE}"


def _document(*body: str) -> str:
    """An Aerora QML document, with every legitimate namesake of the rule's subject in it.

    What the rule has to do is tell a declared control from a mention of one, so the fixture carries
    the mentions the real documents carry: `AeroraErrorReportPage.qml` and `AeroraOperationsHub.qml`
    both describe their own control in a comment beside it and both state in a `qsTr()` string what
    the screen does not carry. A rule that read either would reject the tree it protects.

    `QGCFlickable` and `QGCLabel` come with it because the real documents root on them, and because
    a rule matching the wrong name shape would read `QGCFlickable` as a control: the hub's report
    scrolls, and scrolling is not an operator action.
    """
    return "\n".join(
        [
            "import QtQuick",
            "import QGroundControl.Controls",
            "",
            "// A fixture document. It carries no QGCButton outside the one declared below, and",
            "// nothing on it reaches a MouseArea or a TapHandler.",
            "QGCFlickable {",
            '    objectName: "aFixtureFlickable"',
            "",
            "    QGCLabel {",
            '        objectName: "aFixtureLabel"',
            '        text: qsTr("This screen carries no button and no mouse area.")',
            "    }",
            *body,
            "}",
            "",
        ]
    )


def _control(type_name: str, control: str, handler: str) -> list[str]:
    """One actionable control, declared the way the real documents declare theirs."""
    return [
        "",
        f"    {type_name} {{",
        f'        objectName: "{control}"',
        '        text: qsTr("A fixture control")',
        "        // What this control does, described beside it the way the real two are.",
        f"        onClicked: {handler}",
        "    }",
    ]


def _qml_sources(root: Path) -> Path:
    """Lay out the Aerora QML documents the baseline posture records, and one that records nothing.

    Three documents, because the rule compares two sets and each side needs a member: the
    exception's document carrying exactly the control the posture excepts, the recorded site's
    document carrying exactly the control and handler the posture records, and a third carrying no
    actionable control at all -- which is every real document but two, and the state a rule matching
    too broad a name shape would reject.
    """
    _write(root / EXCEPTION_DOCUMENT, _document(*_control(CONTROL_TYPE, EXCEPTION_CONTROL, COPY)))
    _write(root / SITE_DOCUMENT, _document(*_control(CONTROL_TYPE, SITE_CONTROL, SITE_HANDLER)))
    _write(root / QUIET_DOCUMENT, _document())
    return root


def _absence_sources(root: Path) -> Path:
    """Lay out the four subjects of the absence rules, each in the state the real tree holds it.

    Absent is not empty. Each subject's legitimate namesakes are written here, because telling the
    subject from a mention of its own name is what the four rules have to do: the allow-list
    declares an entry and names the forbidden tier and both pre-approved seams in prose; the
    threshold setter is declared and defined and never called; and the payload command sink is
    declared, held as a pointer member, taken as a parameter, named in doc comments and implemented
    exactly once, under the test tree, which is where the prohibition's own instrument lives.
    """
    _write(root / OVERRIDES_SOURCE, _override_entries((DECLARED_SEAM, PURPOSE_BUILT_TIER)))
    _write(root / FRESHNESS_HEADER, _freshness_declaration())
    _write(root / FRESHNESS_SOURCE, _freshness_definition())
    _write(root / SINK_INTERFACE, _sink_interface())
    _write(root / SINK_HOLDER, _sink_holder())
    _write(root / SINK_IMPLEMENTATION, _sink_implementation(RECORDING_SINK_HEAD))
    return _qml_sources(root)


def _tree(root: Path, register: GateRegister | None = None, *, applied: bool = False) -> Path:
    """A repository root carrying *register*, the inventory it names and the sources it names.

    Every guard agrees with its record, so the baseline passes all three rules and a test asserting
    a finding writes that finding itself.

    *applied* is whether the `ADR-016` pair is written into the tree. It defaults to neither half,
    because every register reachable through a document here records the authorisation unsigned --
    no fixture part is accepted -- and neither half applied is the state Requirement 7 criterion 17
    protects. A test wanting the applied pair says so.

    The four subjects of the absence rules come with it, each in its absent state, so the baseline
    passes every rule in this module and a test asserting a finding writes that finding itself.
    """
    recorded = _register() if register is None else register
    return _absence_sources(
        _guard_sources(
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


def _literal(register: GateRegister, symbol: str) -> str:
    """The value *register* records for *symbol*, which has to be a literal to have one.

    Asserted rather than coerced: only a `literal` carries a value, so a case reading one is a case
    about a literal, and a registry entry that stopped being one should fail here rather than hand a
    `None` to a fixture writer.
    """
    recorded = _recorded(register, symbol).recorded_value
    assert recorded is not None
    return recorded


def _resurfaced(register: GateRegister, *, drops: str = "", adds: str = "") -> GateRegister:
    """*register* with the surface entry for *drops* withdrawn and one for *adds* recorded.

    A guard's declaration *is* a contract-surface element, so a test that removes or renames one
    changes two things at once. Moving the inventory with it is what the real change would do, and
    it keeps each case below asserting the one finding it is about rather than that finding plus a
    correct report of the surface it moved.
    """
    withdrawn = _member(drops)
    kept = [element for element in register.contract_surface if element.symbol != withdrawn]
    if adds:
        kept.append(
            ContractSurfaceElement(
                symbol=adds, kind=CONSTANT_KIND, gate=NO_GATE, reason="a fixture reason"
            )
        )
    return replace(register, contract_surface=kept)


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


def _established(
    register: GateRegister,
    *,
    files: int = 0,
    baseline: int = 0,
    in_force: int = CONDITIONAL_RULES,
    surface: int | None = None,
    examined: int = 2,
    carrying: int = 1,
) -> str:
    """The summary line a run ends with, spelled out rather than matched loosely.

    What it claims is the point: the guard rules read the files *register* names, `adr-016-pair`
    reads the two files the criterion names, `mavlink-identifier` walks the overlay tree and the
    four absence rules walk the overlay source, so the line reports the guard values, the assertion
    sites, the pair, the allow-list and the rules in force, and states that the selection decided
    none of it. Taking it off a run's output by equality is what makes every test below assert that
    the claim still reads that way.

    *baseline* is how many identifiers the allow-listed sites held, which defaults to none because
    no fixture tree carries a MAVLink identifier until a test writes one. A test that writes one
    says how many, which is how the ratchet case asserts that the baseline is read from the tree
    rather than recorded anywhere.

    *in_force* is how many of the Gate-conditional rules the derivation leaves in force, which
    defaults to all of them because every Gate of every register reachable through a document here
    is open. It is in the summary rather than left implicit because a rule standing down reports
    nothing: a run in which five rules stood down must not read as a run in which five rules found
    nothing. The recorded control count rides with them for the same reason, because
    `actionable-control` compares two sets and an empty register is not an empty tree.

    *surface* is how many contract-surface elements the tree declares, which defaults to however
    many *register* records because the baseline agrees in both directions. A test that writes a
    declaration says so, which is what makes the two numbers a measurement of the tree against the
    register rather than one number restated.

    *examined* and *carrying* are the isolation walk reach and result over the fixture repository,
    which carries two files outside the overlay and an Aerora identifier in one of them. A test
    writing another outside file says so. They default to the shape of the fixture rather than of
    the repository, which is what makes them the one pair in this line a tree-based test can move.
    """
    guards = [guard for guard in register.guards if guard.symbol != MAVLINK_IDENTIFIER_GUARD]
    sites = sum(len(guard.asserted_in) for guard in guards)
    allow_listed = len(register.mavlink_identifiers.allow_list)
    read_only = register.read_only
    controls = len(read_only.exceptions) + len(read_only.actionable_control_sites)
    declared = len(register.contract_surface) if surface is None else surface
    return (
        f"{REGISTER_PATH} records {len(register.guards)} guards over {len(register.gates)} "
        f"Gate_Dossiers; {len(guards)} guard values and {sites} assertion sites compared against "
        f"the files the register names, and the ADR-016 pair against {CORE_HEADER} and "
        f"{OVERLAY_MANIFEST}; every file under {OVERLAY_TREE}/ searched against {allow_listed} "
        f"allow-listed sites holding {baseline} identifiers; {in_force} of the "
        f"{CONDITIONAL_RULES} Gate-conditional rules in force over {SOURCE_TREE}/ and "
        f"{CONDITIONAL_RULES - in_force} standing down on a Gate the derivation records closed, "
        f"with {controls} actionable controls recorded; {declared} contract-surface elements "
        f"declared under {SOURCE_TREE}/ against {len(register.contract_surface)} recorded; "
        f"{carrying} of {examined} files outside {OVERLAY_TREE}/ carry an Aerora "
        f"identifier against {len(register.overlay_edits or ())} recorded; "
        f"no rule read a file because it was among the {files} selected source files\n"
    )


def _lines(
    result: AnalysisResult,
    register: GateRegister,
    *,
    files: int = 0,
    baseline: int = 0,
    in_force: int = CONDITIONAL_RULES,
    surface: int | None = None,
    examined: int = 2,
    carrying: int = 1,
) -> list[str]:
    """The finding lines *result* printed, with the summary the run ends with taken off."""
    return result.output.removesuffix(
        _established(
            register,
            files=files,
            baseline=baseline,
            in_force=in_force,
            surface=surface,
            examined=examined,
            carrying=carrying,
        )
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
    register = _resurfaced(_alone(_register(), REPLAY_GUARD), drops=REPLAY_GUARD)
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
    register = _resurfaced(_alone(_register(), REPLAY_GUARD), drops=REPLAY_GUARD)
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
    register = _resurfaced(
        _alone(_register(), REPLAY_GUARD), drops=REPLAY_GUARD, adds=RENAMED_GUARD
    )
    recorded = _literal(register, REPLAY_GUARD)
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
    """A file this rule cannot read is a failure rather than a pass.

    The baseline reach is withdrawn from the register first. Deleting the core header deletes the
    one file the fixture repository reaches into, so a register still recording it would be
    reported by `overlay-isolation` too -- correctly, and about a different subject than this one.
    """
    register = replace(_register(), overlay_edits=[])
    root = _tree(tmp_path / "repo", register)
    (root / CORE_HEADER).unlink()

    result = _review(root)

    (line,) = _lines(result, register, examined=1, carrying=0)
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


# ---------------------------------------------------------------------------
# populated-file-override
#
# Named with the `override` token, which is the selector this group reaches under: `-k override`
# runs it and nothing else.
#
# The subject is one entry in one list, and everything hard about the rule is that the file holding
# that list names the subject twice more -- in the function's own comment block and in the count
# that counts it. So the accepting cases assert the mentions are present before asserting that
# nothing was reported, because a quiet fixture would prove nothing.
# ---------------------------------------------------------------------------


def _closing(root: Path, *gates: str) -> Premises:
    """Premises in which each of *gates* is accepted, rooted at the four absent subjects.

    The route every Gate-closed case takes, for the reason `_authorised` takes it: a closed Gate
    needs an accepted part naming a Closure_Record, which `_accepting` builds in code, so the
    register cannot reach the rule as a document.
    """
    premises = _accepting(_register(), {gate: _permissions(gate) for gate in gates})
    return replace(premises, root=_absence_sources(root))


def _line_holding(root: Path, path: str, text: str) -> int:
    """The one-based line of *path* under *root* that holds *text*, and it must hold it once.

    Counted here rather than taken off the finding, so the line a rule reports is compared against
    a number this file worked out from the fixture it wrote.
    """
    lines = (root / path).read_text(encoding="utf-8").splitlines()
    (found,) = [number for number, line in enumerate(lines, 1) if text in line]
    return found


def test_populated_file_override_accepts_a_list_naming_the_tier_only_in_prose_and_in_the_count(
    tmp_path: Path,
) -> None:
    """The state the tree stands in, with both legitimate mentions of the tier in the file.

    The comment block inside the function names the forbidden tier, and
    `populatedFileOverrideCount()` compares an entry's tier against it in code. A rule that read the
    file would reject both; a rule that read the function body would reject the comment. Nothing is
    reported, and the mentions are asserted present, so the pass is evidence that the span read is
    the returned list and that comments came out.
    """
    register = _register()

    root = _tree(tmp_path / "repo", register)
    result = _review(root)

    declared = (root / OVERRIDES_SOURCE).read_text(encoding="utf-8")
    assert declared.count(POPULATED_FILE_TIER) == 2
    assert f'{{QStringLiteral("{DECLARED_SEAM}"), {PURPOSE_BUILT_TIER}}}' in declared
    assert result.passed
    assert result.issues == 0
    assert _lines(result, register) == []


def test_populated_file_override_rejects_an_entry_carrying_the_forbidden_tier(
    tmp_path: Path,
) -> None:
    """Requirement 14 criterion 13: reject the change before it enters the repository.

    The entry is added beside the one the build declares, so what the rule rejects is the entry
    rather than a list it could not read, and the line it names is the entry's own.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(
        root / OVERRIDES_SOURCE,
        _override_entries(
            (DECLARED_SEAM, PURPOSE_BUILT_TIER),
            (FORKED_FILE, POPULATED_FILE_TIER),
        ),
    )

    result = _review(root)

    (line,) = _lines(result, register)
    entry = _line_holding(root, OVERRIDES_SOURCE, f'QStringLiteral("{FORKED_FILE}")')
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{POPULATED_FILE_OVERRIDE}: {VIDEO_GATE}: {OVERRIDES_SOURCE}: ")
    assert f"carries a {POPULATED_FILE_TIER} entry at line {entry}" in line
    assert "Requirement 14 criterion 13" in line


def test_populated_file_override_reads_an_entry_path_naming_the_tier_as_a_path(
    tmp_path: Path,
) -> None:
    """A quoted path is not code, so a file named after the tier is not an entry carrying it.

    The false rejection this avoids is the kind that gets a gate removed rather than obeyed: the
    rule matches the tier without its qualifier, because the span is a list of paths and tiers, and
    what makes that sound is that the paths are blanked before the match.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(
        root / OVERRIDES_SOURCE,
        _override_entries(
            ("/qml/QGroundControl/FlyView/PopulatedFile.qml", PURPOSE_BUILT_TIER),
        ),
    )

    result = _review(root)

    assert "PopulatedFile" in (root / OVERRIDES_SOURCE).read_text(encoding="utf-8")
    assert result.passed
    assert result.issues == 0
    assert _lines(result, register) == []


def test_populated_file_override_stands_down_once_the_gate_records_an_accepted_closure(
    tmp_path: Path,
) -> None:
    """The criterion is written `WHILE DG-VID holds no accepted Closure_Record`.

    The entry is in the list and the rule says nothing, because once the Gate is closed the fork
    the entry makes is the one an owner signed a fork point, an owner and a drift-review cadence
    for. A rule that kept rejecting it would be rejecting the decision.
    """
    premises = _closing(tmp_path / "repo", VIDEO_GATE)
    _write(
        premises.root / OVERRIDES_SOURCE,
        _override_entries((FORKED_FILE, POPULATED_FILE_TIER)),
    )

    assert not premises.derived.gate_open[VIDEO_GATE].is_open
    assert check_populated_file_override(premises) == []


def test_populated_file_override_and_action_seam_each_name_a_tree_defining_no_allow_list(
    tmp_path: Path,
) -> None:
    """The allow-list is read against nothing, reported once per decision left unprotected.

    Two findings for one defect, deliberately: the two rules protect two Gates, and a reviewer
    repairing one of them has to see that the other is unguarded too. The criteria name the
    function rather than a file, so a function nobody defines is a failure rather than an absence --
    renaming it would otherwise switch both rules off silently.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    (root / OVERRIDES_SOURCE).unlink()

    result = _review(root)

    override, seam = _lines(result, register)
    assert not result.passed
    assert result.issues == 2
    assert override.startswith(f"{POPULATED_FILE_OVERRIDE}: {VIDEO_GATE}: ")
    assert seam.startswith(f"{ACTION_SEAM}: {ROLE_GATE}: ")
    assert all(
        f"no file under {SOURCE_TREE}/ defines {OVERRIDE_ENTRIES}" in line
        for line in (override, seam)
    )
    assert all('"not found" is not "absent"' in line for line in (override, seam))


def test_populated_file_override_reads_the_real_allow_list_without_writing_it() -> None:
    """The one case for this rule whose evidence is the repository rather than a fixture.

    The real file names the forbidden tier in its comment block and in the count, and declares one
    purpose-built seam, so a rule that read the file or the whole function body would reject this
    tree. Nothing is reported, the mentions are asserted present, and the file comes back byte for
    byte: the rule reads the overlay and never writes it.
    """
    before = (REPO_ROOT / REAL_OVERRIDES).read_text(encoding="utf-8")

    assert POPULATED_FILE_TIER in before
    assert check_populated_file_override(_premises(_register())) == []
    assert (REPO_ROOT / REAL_OVERRIDES).read_text(encoding="utf-8") == before


# ---------------------------------------------------------------------------
# action-seam
#
# Named with the `seam` token, which is the selector this group reaches under.
#
# Three seams exist and one of them is declared, so the rule's job is to tell the two the criterion
# retains from the one it does not mention. The baseline declares `FlyViewCustomLayer.qml`, so every
# accepting case here is a list with an entry in it rather than an empty one.
# ---------------------------------------------------------------------------


def test_action_seam_accepts_the_one_seam_the_allow_list_declares(tmp_path: Path) -> None:
    """A declared seam the criterion does not name is not a declared action seam.

    `FlyViewCustomLayer.qml` is an empty upstream seam carrying a read-only overlay, and both seams
    the criterion retains are named in the same file's comment block as deliberately undeclared. A
    rule that matched any seam, or that read the comment, would reject the tree it protects.
    """
    register = _register()

    root = _tree(tmp_path / "repo", register)
    result = _review(root)

    declared = (root / OVERRIDES_SOURCE).read_text(encoding="utf-8")
    assert DECLARED_SEAM in declared
    assert all(seam in declared for seam in PRE_APPROVED_SEAMS)
    assert result.passed
    assert result.issues == 0
    assert _lines(result, register) == []


@pytest.mark.parametrize("seam", PRE_APPROVED_SEAMS)
def test_action_seam_rejects_either_pre_approved_seam_added_to_the_allow_list(
    tmp_path: Path, seam: str
) -> None:
    """Requirement 5 criterion 4: reject the addition, and retain both seams as undeclared.

    Both seams, because the criterion names both and a rule watching one would let the other in.
    The seam is added as the resource path the allow-list carries, and the finding names it by the
    file name the criterion names it by.
    """
    register = _register()
    declared = f"{SEAM_DIRECTORY}/{seam}"
    root = _tree(tmp_path / "repo", register)
    _write(
        root / OVERRIDES_SOURCE,
        _override_entries((DECLARED_SEAM, PURPOSE_BUILT_TIER), (declared, PURPOSE_BUILT_TIER)),
    )

    result = _review(root)

    (line,) = _lines(result, register)
    entry = _line_holding(root, OVERRIDES_SOURCE, f'QStringLiteral("{declared}")')
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{ACTION_SEAM}: {ROLE_GATE}: {OVERRIDES_SOURCE}: ")
    assert f"carries {seam} at line {entry}" in line
    assert "Requirement 5 criterion 4" in line
    assert "retains both seams as pre-approved and undeclared" in line


def test_action_seam_stands_down_once_the_gate_records_an_accepted_closure(tmp_path: Path) -> None:
    """The criterion is written `WHILE DG-ROL is OPEN`.

    Both seams are declared and the rule says nothing, because the operator actions each one
    introduces are then inside the authorisation scope the acceptance settled.
    """
    premises = _closing(tmp_path / "repo", ROLE_GATE)
    _write(
        premises.root / OVERRIDES_SOURCE,
        _override_entries(
            *((f"{SEAM_DIRECTORY}/{seam}", PURPOSE_BUILT_TIER) for seam in PRE_APPROVED_SEAMS)
        ),
    )

    assert not premises.derived.gate_open[ROLE_GATE].is_open
    assert check_action_seam(premises) == []


def test_action_seam_reads_the_real_allow_list_and_finds_neither_seam() -> None:
    """The one case for this rule whose evidence is the repository rather than a fixture.

    The real file names both seams in prose, as the two it deliberately leaves undeclared, so a
    rule that read the file rather than the returned list would reject exactly the posture the
    criterion requires.
    """
    before = (REPO_ROOT / REAL_OVERRIDES).read_text(encoding="utf-8")

    assert all(seam in before for seam in PRE_APPROVED_SEAMS)
    assert check_action_seam(_premises(_register())) == []
    assert (REPO_ROOT / REAL_OVERRIDES).read_text(encoding="utf-8") == before


# ---------------------------------------------------------------------------
# freshness-threshold
#
# Named with the `threshold` token, which is the selector this group reaches under.
#
# The sharp edge: the setter is declared and defined under the overlay source tree today, and every
# call in the repository is in a suite. A rule searching for the name, or for the name and its
# parenthesis, would reject the declaration and the definition and fail on a clean tree.
# ---------------------------------------------------------------------------


def test_freshness_threshold_accepts_a_declaration_and_an_out_of_line_definition(
    tmp_path: Path,
) -> None:
    """Neither a declaration nor a definition introduces a value, so neither is a call site."""
    register = _register()

    root = _tree(tmp_path / "repo", register)
    result = _review(root)

    assert f"void {THRESHOLD_SETTER}(" in (root / FRESHNESS_HEADER).read_text(encoding="utf-8")
    assert f"::{THRESHOLD_SETTER}(" in (root / FRESHNESS_SOURCE).read_text(encoding="utf-8")
    assert result.passed
    assert result.issues == 0
    assert _lines(result, register) == []


@pytest.mark.parametrize(
    "reached_by",
    ["freshness->", "freshness.", "", "AFixtureFreshness::", "return freshness->"],
    ids=["arrow", "dot", "implicit-this", "qualified", "returned"],
)
def test_freshness_threshold_rejects_a_call_site_however_it_reaches_the_setter(
    tmp_path: Path, reached_by: str
) -> None:
    """Requirement 8 criterion 13: a call is where a threshold value is introduced.

    Five spellings, because the classification reads the statement backwards from the name and each
    spelling exercises a different branch of it: a member access through a pointer and through a
    value, an implicit `this`, a qualified call whose qualifier an out-of-line definition also
    carries, and an expression keyword that ends in an identifier the way a return type does.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(root / ADDED_SOURCE, _threshold_call(reached_by))

    result = _review(root)

    (line,) = _lines(result, register)
    called = _line_holding(root, ADDED_SOURCE, f"{THRESHOLD_SETTER}(")
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{FRESHNESS_THRESHOLD}: {FRESHNESS_GATE}: {ADDED_SOURCE}: ")
    assert f"calls {THRESHOLD_SETTER}() at line {called}" in line
    assert "Requirement 8 criterion 13" in line
    assert f"holds the Gate at {OPEN_STATUS}" in line


def test_freshness_threshold_leaves_a_call_site_under_the_test_tree_alone(tmp_path: Path) -> None:
    """Every call in the repository is a suite configuring a fixture, which is the evidence.

    The suites assert the three-outcome evaluation criterion 4 requires, and they cannot assert it
    without registering a threshold. A rule scoped to the overlay tree would reject the tests that
    prove the behaviour the Gate holds open.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(root / ADDED_TEST_SOURCE, _threshold_call("freshness->"))

    result = _review(root)

    assert f"{THRESHOLD_SETTER}(" in (root / ADDED_TEST_SOURCE).read_text(encoding="utf-8")
    assert result.passed
    assert result.issues == 0
    assert _lines(result, register) == []


def test_freshness_threshold_stands_down_once_the_gate_records_an_accepted_closure(
    tmp_path: Path,
) -> None:
    """The criterion is written `WHILE DG-FRS holds no accepted Closure_Record for a class`.

    The call site is there and the rule says nothing, because the value it would pass is then a
    value an owner signed with its unit and its source.
    """
    premises = _closing(tmp_path / "repo", FRESHNESS_GATE)
    _write(premises.root / ADDED_SOURCE, _threshold_call("freshness->"))

    assert not premises.derived.gate_open[FRESHNESS_GATE].is_open
    assert check_freshness_threshold(premises) == []


def test_freshness_threshold_leaves_the_recorded_literal_to_guard_value(tmp_path: Path) -> None:
    """The division of one subject between two rules, asserted as output.

    `AeroraPowerModel::kDefinedThresholdCount` is a recorded guard, so the literal it holds is
    `guard-value`'s to read against the register -- with the disagreement, the three-value case and
    the renamed declaration all reported there. A declaration carrying another literal is one
    finding from that rule and none from this one, so the two never answer the same question twice.
    """
    register = _alone(_register(), POWER_GUARD)
    root = _tree(tmp_path / "repo", register)
    _write(root / SOLO_DECLARATION, _declares(POWER_GUARD, ANOTHER_VALUE))

    result = _review(root)

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{GUARD_VALUE}: {PAYLOAD_GATE}: {POWER_GUARD}: ")
    assert FRESHNESS_THRESHOLD not in result.output


def test_freshness_threshold_names_the_registry_recording_no_entry_for_the_count(
    tmp_path: Path,
) -> None:
    """A deferral has to land somewhere, and this is the case in which it would not.

    Without a registry entry, `guard-value` reads nothing for the count and the literal is pinned by
    neither rule. That the registry covers the guard is `guard-registry`'s finding in the data half;
    what is reported here is the deferral landing nowhere.
    """
    register = _register(
        guards=[_guard(symbol) for symbol in BASELINE_GUARD_GATES if symbol != POWER_GUARD]
    )
    premises = replace(_premises(register), root=_absence_sources(tmp_path / "repo"))

    (finding,) = check_freshness_threshold(premises)

    assert finding.check == FRESHNESS_THRESHOLD
    assert finding.gates == (FRESHNESS_GATE,)
    assert finding.blocking
    assert finding.message.startswith(f"{POWER_GUARD}: ")
    assert "records no entry for it" in finding.message
    assert f"defers to {GUARD_VALUE}" in finding.message
    assert f"is {GUARD_REGISTRY}'s finding in the data half" in finding.message


def test_freshness_threshold_reads_the_real_source_tree_and_finds_no_call_site() -> None:
    """The one case for this rule whose evidence is the repository rather than a fixture.

    `AeroraFreshness.h` declares the setter and `AeroraFreshness.cc` defines it out of line, and
    both are asserted present here, so a rule that read either as a call site would reject a clean
    tree -- and this is the test that fails the day one of them starts reading as one. The tree
    comes back byte for byte.
    """
    pair = (REAL_FRESHNESS_HEADER, REAL_FRESHNESS_SOURCE)
    before = {path: (REPO_ROOT / path).read_text(encoding="utf-8") for path in pair}

    assert f"void {THRESHOLD_SETTER}(" in before[REAL_FRESHNESS_HEADER]
    assert f"::{THRESHOLD_SETTER}(" in before[REAL_FRESHNESS_SOURCE]
    assert check_freshness_threshold(_premises(_register())) == []
    assert {path: (REPO_ROOT / path).read_text(encoding="utf-8") for path in pair} == before


# ---------------------------------------------------------------------------
# payload-command-sink
#
# Named with the `sink` token, which is the selector this group reaches under.
#
# The other sharp edge: the interface is declared under the overlay source tree deliberately, and
# the overlay holds its name in a pointer member, a setter parameter and three doc comments. The one
# implementation in the repository is a recording double under `aerora/test/**`, which the
# prohibition's own tests need.
# ---------------------------------------------------------------------------


def test_payload_command_sink_accepts_the_interface_a_pointer_member_and_a_parameter(
    tmp_path: Path,
) -> None:
    """Declaring the interface is the Gate's recorded state; holding one is not implementing one."""
    register = _register()

    root = _tree(tmp_path / "repo", register)
    result = _review(root)

    assert f"class {PAYLOAD_COMMAND_SINK_TYPE}" in (root / SINK_INTERFACE).read_text(
        encoding="utf-8"
    )
    holder = (root / SINK_HOLDER).read_text(encoding="utf-8")
    assert f"{PAYLOAD_COMMAND_SINK_TYPE} *_commandSink" in holder
    assert f"setCommandSink({PAYLOAD_COMMAND_SINK_TYPE} *sink)" in holder
    assert result.passed
    assert result.issues == 0
    assert _lines(result, register) == []


def test_payload_command_sink_leaves_the_implementation_under_the_test_tree_alone(
    tmp_path: Path,
) -> None:
    """The one implementation in the repository is the instrument the prohibition tests need.

    It records what it was handed so a suite can assert that nothing was handed to it, which is how
    the absence of an outbound path is asserted rather than reviewed. A rule scoped to the overlay
    tree would reject the evidence for the prohibition it enforces.
    """
    register = _register()

    root = _tree(tmp_path / "repo", register)
    result = _review(root)

    assert RECORDING_SINK_HEAD in (root / SINK_IMPLEMENTATION).read_text(encoding="utf-8")
    assert result.passed
    assert result.issues == 0
    assert _lines(result, register) == []


@pytest.mark.parametrize(
    "head",
    [
        f"class AFixtureSink : public {PAYLOAD_COMMAND_SINK_TYPE}",
        f"struct AFixtureSink : {PAYLOAD_COMMAND_SINK_TYPE}",
        f"class AFixtureSink final : public {PAYLOAD_COMMAND_SINK_TYPE}",
        f"class AFixtureSink : public AFixtureBase, public {PAYLOAD_COMMAND_SINK_TYPE}",
    ],
    ids=["public", "implicit-struct", "final", "second-base"],
)
def test_payload_command_sink_rejects_an_implementation_under_the_source_tree(
    tmp_path: Path, head: str
) -> None:
    """Requirement 10 criterion 18: reject the change and name each of the two Gates that is open.

    Four class heads, because a derivation is what an implementation is and each of these is one:
    the access specifier is optional, `final` sits between the name and the base clause, and the
    interface may be the second base rather than the first.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(root / ADDED_SOURCE, _sink_implementation(head))

    result = _review(root)

    (line,) = _lines(result, register)
    derived = _line_holding(root, ADDED_SOURCE, head)
    assert not result.passed
    assert result.issues == 1
    assert line.startswith(f"{PAYLOAD_COMMAND_SINK}: {PAYLOAD_GATE}, {ROLE_GATE}: {ADDED_SOURCE}: ")
    assert f"derives from {PAYLOAD_COMMAND_SINK_TYPE} at line {derived}" in line
    assert "Requirement 10 criterion 18" in line


def test_payload_command_sink_names_only_the_gate_that_is_still_open(tmp_path: Path) -> None:
    """Criterion 18 is a disjunction over two Gates, and the finding names the ones that are open.

    With the payload Gate accepted, the implementation is still rejected and the read-only Gate is
    the only one named: the payload Decision_Content exists, and nothing yet evaluates an
    authorisation for the command path it would reach.
    """
    premises = _closing(tmp_path / "repo", PAYLOAD_GATE)
    _write(premises.root / ADDED_SOURCE, _sink_implementation(RECORDING_SINK_HEAD))

    (finding,) = check_payload_command_sink(premises)

    assert not premises.derived.gate_open[PAYLOAD_GATE].is_open
    assert premises.derived.gate_open[ROLE_GATE].is_open
    assert finding.check == PAYLOAD_COMMAND_SINK
    assert finding.gates == (ROLE_GATE,)
    assert finding.blocking


def test_payload_command_sink_stands_down_once_both_gates_record_an_accepted_closure(
    tmp_path: Path,
) -> None:
    """Both halves of the disjunction closed, so the rule has nothing left to say."""
    premises = _closing(tmp_path / "repo", PAYLOAD_GATE, ROLE_GATE)
    _write(premises.root / ADDED_SOURCE, _sink_implementation(RECORDING_SINK_HEAD))

    assert not premises.derived.gate_open[PAYLOAD_GATE].is_open
    assert not premises.derived.gate_open[ROLE_GATE].is_open
    assert check_payload_command_sink(premises) == []


def test_payload_command_sink_rejects_a_source_tree_it_could_not_walk(tmp_path: Path) -> None:
    """A run that searched nothing must not read as a run that found nothing.

    The premises are rooted at a tree carrying no overlay source at all, which is the state none of
    the four absence rules can answer in: an absence read off a tree that was never walked is an
    absence nobody established.
    """
    premises = replace(_premises(_register()), root=tmp_path / "repo")

    (finding,) = check_payload_command_sink(premises)

    assert finding.check == PAYLOAD_COMMAND_SINK
    assert finding.gates == (PAYLOAD_GATE, ROLE_GATE)
    assert finding.blocking
    assert finding.message.startswith(f"{SOURCE_TREE}/ is not a directory")
    assert "indistinguishable from a run that found nothing" in finding.message


def test_payload_command_sink_reads_the_real_source_tree_and_finds_no_implementation() -> None:
    """The one case for this rule whose evidence is the repository rather than a fixture.

    The interface's own declaration, a pointer member of that type and a setter taking one are all
    asserted present, and the one implementation in the repository is asserted to be the test
    double under `aerora/test/**`. So a rule that searched for the name would reject the interface
    the Gate exists to leave unimplemented, and this is the test that notices.
    """
    read = (REAL_SINK_INTERFACE, REAL_SINK_HOLDER, REAL_SINK_IMPLEMENTATION)
    before = {path: (REPO_ROOT / path).read_text(encoding="utf-8") for path in read}

    assert f"class {PAYLOAD_COMMAND_SINK_TYPE}" in before[REAL_SINK_INTERFACE]
    assert f"{PAYLOAD_COMMAND_SINK_TYPE} *_commandSink" in before[REAL_SINK_HOLDER]
    assert f": public {PAYLOAD_COMMAND_SINK_TYPE}" in before[REAL_SINK_IMPLEMENTATION]
    assert check_payload_command_sink(_premises(_register())) == []
    assert {path: (REPO_ROOT / path).read_text(encoding="utf-8") for path in read} == before


# ---------------------------------------------------------------------------
# actionable-control
#
# Named with the `qml` token, which is the selector this group reaches under: `-k qml` runs it and
# nothing else.
#
# The rule compares two sets, so every case is about one side disagreeing with the other, and the
# accepting cases assert the mentions are present first. The fixture documents carry what the real
# ones carry -- a comment describing the control beside it, and a `qsTr()` string saying the screen
# holds no button -- because a rule that read either would reject the tree it protects.
#
# The real-tree case is the one with the sharpest evidence here. The repository carries exactly two
# actionable controls and the design accounted for one, so that case asserts both: the clipboard
# copy the exception records, and the hub's navigation button, whose handler reaches no path
# Requirement 5 criterion 1 names.
# ---------------------------------------------------------------------------


def _site(**changes: object) -> ActionableControlSite:
    """The recorded site the baseline posture carries, with *changes* applied."""
    recorded: dict[str, object] = {
        "screen": SITE_SCREEN,
        "file": SITE_DOCUMENT,
        "control": SITE_CONTROL,
        "handler": SITE_HANDLER,
        "reaches_no_forbidden_path": True,
        "reason": "a fixture reason",
        "requirement": None,
        **changes,
    }
    return ActionableControlSite(**recorded)  # type: ignore[arg-type]


def _posture(register: GateRegister, *sites: ActionableControlSite) -> GateRegister:
    """*register* with its recorded actionable-control sites replaced by *sites*."""
    return replace(
        register, read_only=replace(register.read_only, actionable_control_sites=list(sites))
    )


def _over(register: GateRegister, root: Path) -> Premises:
    """Premises over *register*, rooted at the QML tree at *root* rather than at the repository."""
    return replace(_premises(register), root=root)


def _premises_at(register: GateRegister, root: Path) -> Premises:
    """Premises over *register*, rooted at *root*.

    The same shape `_over` has, named for the general case because the guard-kind and absent-subject
    cases below read source trees rather than QML documents.
    """
    return replace(_premises(register), root=root)


def test_actionable_control_accepts_the_qml_documents_the_recorded_posture_matches(
    tmp_path: Path,
) -> None:
    """The baseline: two recorded controls, two declared, and a third document carrying none.

    The mentions are asserted present before the pass, so this is evidence that a type named in a
    comment or in a translated string is not read as a declaration, and that `QGCFlickable` and
    `QGCLabel` are not read as controls.
    """
    register = _register()

    root = _tree(tmp_path / "repo", register)
    result = _review(root)

    excepted = (root / EXCEPTION_DOCUMENT).read_text(encoding="utf-8")
    assert f"// {CONTROL_TYPE}" not in excepted
    assert CONTROL_TYPE in excepted
    assert "carries no button and no mouse area" in excepted
    assert "QGCFlickable {" in (root / QUIET_DOCUMENT).read_text(encoding="utf-8")
    assert result.passed
    assert result.issues == 0
    assert _lines(result, register) == []


def test_actionable_control_rejects_a_qml_control_the_posture_records_nowhere(
    tmp_path: Path,
) -> None:
    """Requirement 5 criterion 2: reject the change before it enters the repository.

    The finding names the Gate and states the owner's two ways out, because a reviewer reading it
    has to be able to tell which decision is being protected and what settles it.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(
        root / ADDED_DOCUMENT,
        _document(*_control(CONTROL_TYPE, "aFixtureAddedControl", "vehicle.aFixtureAction()")),
    )

    (finding,) = check_actionable_control(_over(register, root))

    assert finding.check == ACTIONABLE_CONTROL
    assert finding.gates == (ROLE_GATE,)
    assert finding.blocking
    assert ADDED_DOCUMENT in finding.message
    assert "aFixtureAddedControl" in finding.message
    assert "records neither the single exception" in finding.message


def test_actionable_control_rejects_a_second_qml_control_in_the_excepted_document(
    tmp_path: Path,
) -> None:
    """Requirement 5 criterion 11, and the reason set equality needs no count beside it.

    The design asks the excepted document to carry *exactly one* control. Nothing here counts: the
    second control is simply a control the posture records nothing for, so it is rejected by the
    same line that rejects one in any other document.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(
        root / EXCEPTION_DOCUMENT,
        _document(
            *_control(CONTROL_TYPE, EXCEPTION_CONTROL, COPY),
            *_control(CONTROL_TYPE, "aFixtureSecondControl", "vehicle.aFixtureAction()"),
        ),
    )

    (finding,) = check_actionable_control(_over(register, root))

    assert finding.check == ACTIONABLE_CONTROL
    assert finding.gates == (ROLE_GATE,)
    assert finding.blocking
    assert EXCEPTION_DOCUMENT in finding.message
    assert "aFixtureSecondControl" in finding.message


def test_actionable_control_rejects_a_qml_control_whose_object_name_drifted(
    tmp_path: Path,
) -> None:
    """An objectName that moves is both halves of the set disagreeing, and both are reported.

    This is what pins the exception to a control rather than to a file: the recorded name is gone
    and an unrecorded one is there, so renaming the control cannot carry the exception with it.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(
        root / EXCEPTION_DOCUMENT,
        _document(*_control(CONTROL_TYPE, "aFixtureRenamedControl", COPY)),
    )

    unrecorded, stale = check_actionable_control(_over(register, root))

    assert "aFixtureRenamedControl" in unrecorded.message
    assert "records neither the single exception" in unrecorded.message
    assert EXCEPTION_CONTROL in stale.message
    assert "the single exception to read-only presentation" in stale.message
    assert "is stale rather than satisfied" in stale.message
    assert unrecorded.blocking
    assert stale.blocking


def test_actionable_control_rejects_a_recorded_qml_control_the_tree_no_longer_carries(
    tmp_path: Path,
) -> None:
    """A record naming a control the tree lost is stale rather than satisfied.

    A register still accounting for a control nothing carries tells a reviewer the opposite of the
    truth, which is the reasoning the contract-surface ratchet applies in the other direction.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(root / SITE_DOCUMENT, _document())

    (finding,) = check_actionable_control(_over(register, root))

    assert finding.check == ACTIONABLE_CONTROL
    assert finding.gates == (ROLE_GATE,)
    assert finding.blocking
    assert SITE_DOCUMENT in finding.message
    assert "a site reaching no forbidden path" in finding.message


def test_actionable_control_rejects_a_qml_control_whose_recorded_handler_changed(
    tmp_path: Path,
) -> None:
    """The pin that stops the site list becoming an escape hatch.

    `reachesNoForbiddenPath` is a claim about an expression, so recording a navigation button does
    not license turning it into a command path later: the handler the owner reviewed is gone, and
    the finding names it and the reason it was recorded under.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(
        root / SITE_DOCUMENT,
        _document(*_control(CONTROL_TYPE, SITE_CONTROL, "vehicle.sendAFixtureCommand()")),
    )

    (finding,) = check_actionable_control(_over(register, root))

    assert finding.check == ACTIONABLE_CONTROL
    assert finding.gates == (ROLE_GATE,)
    assert finding.blocking
    assert SITE_HANDLER in finding.message
    assert "has not been reviewed in its new form" in finding.message


def test_actionable_control_rejects_a_qml_site_not_claiming_it_reaches_no_forbidden_path(
    tmp_path: Path,
) -> None:
    """The schema writes the claim as a constant, so a model carrying it unset has bypassed it.

    Checked here as well for the reason `guard-value` checks a disagreement's handed-on row: a
    register reaching a rule as a model has not passed the schema, and the rule is the second line.
    """
    register = _posture(_register(), _site(reaches_no_forbidden_path=False))
    root = _tree(tmp_path / "repo", register)

    (finding,) = check_actionable_control(_over(register, root))

    assert finding.check == ACTIONABLE_CONTROL
    assert finding.gates == (ROLE_GATE,)
    assert finding.blocking
    assert "does not state that the control reaches no path" in finding.message


def test_actionable_control_rejects_a_qml_control_setting_no_object_name(tmp_path: Path) -> None:
    """A control with no objectName cannot be recorded, so it cannot be permitted.

    A site pins a control by its objectName. Without one there is nothing for the register to hold
    the control by and nothing to tell it apart from another in the same document, so it is rejected
    whether or not its document is recorded -- here the excepted document, which is the harder case.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(
        root / EXCEPTION_DOCUMENT,
        _document(
            *_control(CONTROL_TYPE, EXCEPTION_CONTROL, COPY),
            "",
            f"    {CONTROL_TYPE} {{",
            '        text: qsTr("A fixture control with no objectName")',
            "        onClicked: vehicle.aFixtureAction()",
            "    }",
        ),
    )

    (finding,) = check_actionable_control(_over(register, root))

    assert finding.check == ACTIONABLE_CONTROL
    assert finding.gates == (ROLE_GATE,)
    assert finding.blocking
    assert "setting no objectName" in finding.message
    assert "cannot be recorded" in finding.message


@pytest.mark.parametrize(
    "type_name",
    ["Button", "QGCCheckBox", "MouseArea", "TapHandler", "DragHandler", "ItemDelegate"],
)
def test_actionable_control_finds_each_actionable_qml_type_family(
    tmp_path: Path, type_name: str
) -> None:
    """Every family the runtime walk asks `inherits()` for, matched by the shape of its name.

    `QQuickAbstractButton`, `QQuickMouseArea` and `QQuickPointerHandler` have no QML spelling, so
    the rule matches the convention the three share. A type nobody here thought to name is caught by
    the same token a named one is, which is what a written-out list of types would not do.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(
        root / ADDED_DOCUMENT,
        _document(*_control(type_name, "aFixtureAddedControl", "vehicle.aFixtureAction()")),
    )

    (finding,) = check_actionable_control(_over(register, root))

    assert finding.check == ACTIONABLE_CONTROL
    assert f"declares {type_name} aFixtureAddedControl" in finding.message
    assert finding.blocking


def test_actionable_control_reads_a_scrolling_qml_type_as_not_a_control(tmp_path: Path) -> None:
    """`QGCFlickable` is draggable and is not an operator action.

    It is not one of the three bases the runtime walk names, and the hub's own report scrolls inside
    one, so a rule that rejected it would reject scrolling on the screen it protects.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(
        root / ADDED_DOCUMENT,
        _document(
            "",
            "    QGCFlickable {",
            '        objectName: "aFixtureInnerFlickable"',
            "    }",
        ),
    )

    assert check_actionable_control(_over(register, root)) == []


def test_actionable_control_reads_a_nested_object_name_as_the_childs_qml_name(
    tmp_path: Path,
) -> None:
    """A control takes its own objectName rather than a descendant's.

    Without the depth test the first `objectName` inside the control would be read as the control's,
    so a recorded site would pin the wrong object and a renamed child would read as a renamed
    control.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(
        root / SITE_DOCUMENT,
        _document(
            "",
            f"    {CONTROL_TYPE} {{",
            f'        objectName: "{SITE_CONTROL}"',
            "        QGCLabel {",
            '            objectName: "aFixtureInnerLabel"',
            "        }",
            f"        onClicked: {SITE_HANDLER}",
            "    }",
        ),
    )

    assert check_actionable_control(_over(register, root)) == []


def test_actionable_control_stands_down_once_the_gate_records_an_accepted_closure(
    tmp_path: Path,
) -> None:
    """Once the Gate holds an accepted Closure_Record, a control is what an owner signed for."""
    premises = _closing(tmp_path / "repo", ROLE_GATE)
    _write(
        premises.root / ADDED_DOCUMENT,
        _document(*_control(CONTROL_TYPE, "aFixtureAddedControl", "vehicle.aFixtureAction()")),
    )

    assert not premises.derived.gate_open[ROLE_GATE].is_open
    assert check_actionable_control(premises) == []


def test_actionable_control_rejects_a_qml_tree_it_could_not_walk(tmp_path: Path) -> None:
    """A run that searched no document must not read as a run that found no control."""
    premises = replace(_premises(_register()), root=tmp_path / "repo")

    (finding,) = check_actionable_control(premises)

    assert finding.check == ACTIONABLE_CONTROL
    assert finding.gates == (ROLE_GATE,)
    assert finding.blocking
    assert finding.message.startswith(f"{SOURCE_TREE}/ is not a directory")
    assert "indistinguishable from a run that found nothing" in finding.message


def test_actionable_control_reads_the_real_qml_tree_and_finds_exactly_two_controls() -> None:
    """The one case for this rule whose evidence is the repository rather than a fixture.

    It is also the case that records the finding this rule was written around. The design states
    that the clipboard copy on `SCR-HLP-03` is the only actionable control on the Aerora surface and
    that every other document carries none; the tree carries a second, the hub's navigation button,
    whose handler is a local view switch through `MainWindow.qml` and reaches no outbound command
    path, no mission path and no `AeroraMissionObserver::verify()`.

    So the posture that makes the real tree pass records one exception and one site, and a rule
    admitting only the exception would reject the repository on every commit. Both documents are
    asserted to carry their control and the tree is asserted to carry no third, which is what fails
    the day a third arrives.
    """
    register = _posture(
        _register(),
        _site(
            screen=SITE_SCREEN,
            file=REAL_SITE_DOCUMENT,
            control=REAL_SITE_CONTROL,
            handler=REAL_SITE_HANDLER,
        ),
    )
    register = replace(
        register,
        read_only=replace(
            register.read_only,
            exceptions=[
                replace(
                    register.read_only.exceptions[0],
                    screen=EXCEPTION_SCREEN,
                    file=REAL_EXCEPTION_DOCUMENT,
                    control=REAL_EXCEPTION_CONTROL,
                )
            ],
        ),
    )
    read = (REAL_EXCEPTION_DOCUMENT, REAL_SITE_DOCUMENT)
    before = {path: (REPO_ROOT / path).read_text(encoding="utf-8") for path in read}

    assert f'objectName: "{REAL_EXCEPTION_CONTROL}"' in before[REAL_EXCEPTION_DOCUMENT]
    assert f'objectName: "{REAL_SITE_CONTROL}"' in before[REAL_SITE_DOCUMENT]
    assert REAL_SITE_HANDLER in before[REAL_SITE_DOCUMENT]
    assert check_actionable_control(_premises(register)) == []
    assert {path: (REPO_ROOT / path).read_text(encoding="utf-8") for path in read} == before


# ---------------------------------------------------------------------------
# contract-surface
#
# Named with the `contract_surface` token, which is the selector this group reaches under:
# `-k contract_surface` runs it and nothing else.
#
# Two halves, tested differently, because they fail differently.
#
# The **comparison** half is the three ways the two sets disagree. The baseline register records the
# surface the guard fixture header declares -- derived from the registry, which is where the header
# itself comes from -- so a test asserting a finding rewrites one side and nothing else moves.
#
# The **extraction** half is tested against symbols written out by hand, because deriving the
# expectation with the rule's own extractor would agree with it by construction. Those cases write a
# source file whose declarations are known, record nothing for them, and assert the exact qualified
# symbols the rule reports as unassigned. That is what fails the day the scope prefix stops being
# applied, or a `constexpr` function starts reading as a constant.
# ---------------------------------------------------------------------------

# Where the extraction cases write the declarations they assert over: a file the baseline does not
# carry, so the surface under review is the agreeing one plus the elements the test wrote.
SURFACE_SOURCE = f"{SOURCE_TREE}/AFixtureSurface.h"

# One recorded element the baseline does not carry, for the cases about a recorded entry rather than
# a declared one.
UNDECLARED_SYMBOL = "AFixtureNamespace::kAFixtureUndeclared"


def _surface_header(*body: str) -> str:
    """A header declaring *body*, with the namesakes the extraction must not read as declarations.

    The comment names an enumerator and a constant in prose and the `constexpr` function is a
    function, so a rule that read comments or counted every `constexpr` would find three elements
    here that are not declarations.
    """
    return "\n".join(
        [
            "#pragma once",
            "",
            "// A fixture header. It mentions kAFixtureMentioned and AFixtureMentioned::Member in",
            "// prose, and neither is a declaration.",
            "",
            "constexpr int aFixtureFunction(int value) { return value; }",
            "",
            *body,
            "",
        ]
    )


def _element(
    symbol: str, kind: str, gate: str = NO_GATE, reason: str | None = "why"
) -> ContractSurfaceElement:
    """One recorded contract-surface entry."""
    return ContractSurfaceElement(symbol=symbol, kind=kind, gate=gate, reason=reason)


def _with_surface(register: GateRegister, *elements: ContractSurfaceElement) -> GateRegister:
    """*register* with *elements* added to the surface the baseline already records."""
    return replace(register, contract_surface=[*register.contract_surface, *elements])


def _unassigned(findings: Sequence[Finding]) -> list[str]:
    """The symbols *findings* reports as unassigned Decision_Content, in report order."""
    return [
        finding.message.split(" at line")[0].split("declares ")[1]
        for finding in findings
        if "records no entry for it" in finding.message
    ]


def test_contract_surface_accepts_the_baseline_the_register_records(tmp_path: Path) -> None:
    """The recorded surface and the declared surface agree, so the whole run passes.

    The recorded count is asserted non-zero first: a baseline recording nothing would pass this by
    comparing two empty sets, which is the one way this rule can look clean while checking nothing.
    """
    register = _register()

    root = _tree(tmp_path / "repo", register)
    result = _review(root)

    assert len(register.contract_surface) == 6
    assert result.passed
    assert result.issues == 0
    assert _lines(result, register) == []


def test_contract_surface_names_an_element_no_inventory_entry_records(tmp_path: Path) -> None:
    """Requirement 4 criterion 2: name the element as unassigned Decision_Content.

    The finding names no Gate, because none is recorded, so it renders as `UNASSIGNED`. That is the
    one case `Finding`'s empty-tuple contract admits, and the rule declines to guess a mapping.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(root / SURFACE_SOURCE, _surface_header("constexpr int kAFixtureAdded = 1;"))

    (finding,) = check_contract_surface(_over(register, root))

    assert finding.check == CONTRACT_SURFACE
    assert finding.gates == ()
    assert UNASSIGNED in str(finding)
    assert finding.blocking
    assert f"declares {CONSTANT_KIND} kAFixtureAdded" in finding.message
    assert "cannot classify it and does not guess" in finding.message


def test_contract_surface_names_an_element_mapped_to_an_open_gate(tmp_path: Path) -> None:
    """An element of an open Gate's Decision_Content standing in source is the withheld value.

    This is the rejection the rule exists for, and it is the one that needs the Gate named: the
    element waits on a signature rather than on a review of the file it stands in.
    """
    register = _with_surface(
        _register(), _element("kAFixtureAdded", CONSTANT_KIND, ROLE_GATE, None)
    )
    root = _tree(tmp_path / "repo", register)
    _write(root / SURFACE_SOURCE, _surface_header("constexpr int kAFixtureAdded = 1;"))

    premises = _over(register, root)
    (finding,) = check_contract_surface(premises)

    assert premises.derived.gate_open[ROLE_GATE].is_open
    assert finding.check == CONTRACT_SURFACE
    assert finding.gates == (ROLE_GATE,)
    assert finding.blocking
    assert "kAFixtureAdded" in finding.message
    assert "waits on the signature" in finding.message


def test_contract_surface_accepts_an_element_whose_gate_records_an_accepted_closure(
    tmp_path: Path,
) -> None:
    """A closing Gate frees its own elements, which is why this rule is not Gate-conditional.

    The other five rules stand down wholesale when their one Gate closes. This one reads the Gate
    each element maps to, so the same run reports nothing for an element of the closed Gate and
    still rejects one mapped to an open Gate.
    """
    register = _with_surface(
        _register(),
        _element("kAFixtureClosed", CONSTANT_KIND, ROLE_GATE, None),
        _element("kAFixtureOpen", CONSTANT_KIND, PAYLOAD_GATE, None),
    )
    premises = _accepting(register, {ROLE_GATE: _permissions(ROLE_GATE)})
    root = _tree(tmp_path / "repo", register)
    _write(
        root / SURFACE_SOURCE,
        _surface_header("constexpr int kAFixtureClosed = 1;", "constexpr int kAFixtureOpen = 2;"),
    )

    (finding,) = check_contract_surface(replace(premises, root=root))

    assert not premises.derived.gate_open[ROLE_GATE].is_open
    assert premises.derived.gate_open[PAYLOAD_GATE].is_open
    assert finding.gates == (PAYLOAD_GATE,)
    assert "kAFixtureOpen" in finding.message


def test_contract_surface_names_a_recorded_element_the_tree_no_longer_declares(
    tmp_path: Path,
) -> None:
    """The surface cannot shrink silently either, so a removal is a finding.

    A recorded entry nothing declares tells a reviewer the surface still carries an element it does
    not, which is the same defect as an unrecorded declaration read from the other side.
    """
    register = _with_surface(_register(), _element(UNDECLARED_SYMBOL, CONSTANT_KIND))
    root = _tree(tmp_path / "repo", register)

    (finding,) = check_contract_surface(_over(register, root))

    assert finding.check == CONTRACT_SURFACE
    assert finding.gates == ()
    assert finding.blocking
    assert UNDECLARED_SYMBOL in finding.message
    assert "is a removal rather than a satisfied entry" in finding.message


def test_contract_surface_names_a_removal_of_an_element_mapped_to_a_gate(tmp_path: Path) -> None:
    """A removal names the Gate when one is mapped, so the reader knows which decision it touched."""
    register = _with_surface(
        _register(), _element(UNDECLARED_SYMBOL, CONSTANT_KIND, ROLE_GATE, None)
    )
    root = _tree(tmp_path / "repo", register)

    (finding,) = check_contract_surface(_over(register, root))

    assert finding.gates == (ROLE_GATE,)
    assert "is a removal rather than a satisfied entry" in finding.message


def test_contract_surface_rejects_an_element_mapped_to_no_gate_with_no_reason(
    tmp_path: Path,
) -> None:
    """The schema requires a reason beside `none`, so a model carrying none has bypassed it.

    An element mapped to no Gate without a reason is an element nobody classified rather than one
    somebody cleared, which is the distinction the whole inventory turns on.
    """
    register = _with_surface(_register(), _element("kAFixtureAdded", CONSTANT_KIND, reason=None))
    root = _tree(tmp_path / "repo", register)
    _write(root / SURFACE_SOURCE, _surface_header("constexpr int kAFixtureAdded = 1;"))

    (finding,) = check_contract_surface(_over(register, root))

    assert finding.check == CONTRACT_SURFACE
    assert finding.blocking
    assert "records no reason" in finding.message
    assert "nobody classified" in finding.message


def test_contract_surface_rejects_an_element_mapped_to_a_gate_with_no_dossier(
    tmp_path: Path,
) -> None:
    """A mapping that resolves to no Gate_Dossier is a failure rather than a pass.

    Unreachable through a document and reached here by taking the Gate out of the derivation: the
    schema makes `gate` an enum of the ten identifiers and requires a dossier for each, so only a
    register handed over as a model can carry a mapping that resolves to nothing. That is the state
    this branch is the second line for, the way `guard-value` re-checks a handed-on row the schema
    already requires.
    """
    register = _with_surface(_register(), _element("kAFixtureAdded", CONSTANT_KIND, SINGLE_GATE))
    root = _tree(tmp_path / "repo", register)
    _write(root / SURFACE_SOURCE, _surface_header("constexpr int kAFixtureAdded = 1;"))
    premises = _over(register, root)
    unresolvable = {
        gate: openness
        for gate, openness in premises.derived.gate_open.items()
        if gate != SINGLE_GATE
    }

    findings = check_contract_surface(
        replace(premises, derived=replace(premises.derived, gate_open=unresolvable))
    )
    (finding,) = [f for f in findings if "records no Gate_Dossier for" in f.message]

    assert finding.gates == (SINGLE_GATE,)
    assert finding.blocking


def test_contract_surface_extracts_enumerators_qualified_through_class_and_enum(
    tmp_path: Path,
) -> None:
    """The symbols are written out here, because deriving them would agree with the rule.

    A scoped enum's name is a scope of its own and an unscoped enum's members land in the enclosing
    scope, which is what C++ does and what the inventory has to record: four constants in the real
    tree are called `kServiceName`, so an unqualified symbol could not tell two elements apart.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(
        root / SURFACE_SOURCE,
        _surface_header(
            "class AFixtureHolder",
            "{",
            "    enum class Scoped { ScopedFirst, ScopedSecond = 3 };",
            "    enum Unscoped { UnscopedOnly };",
            "};",
        ),
    )

    findings = check_contract_surface(_over(register, root))

    assert _unassigned(findings) == [
        f"{ENUMERATOR_KIND} AFixtureHolder::Scoped::ScopedFirst",
        f"{ENUMERATOR_KIND} AFixtureHolder::Scoped::ScopedSecond",
        f"{ENUMERATOR_KIND} AFixtureHolder::UnscopedOnly",
    ]


def test_contract_surface_tells_a_registry_key_from_a_declared_constant(tmp_path: Path) -> None:
    """A char-pointer constant initialised with a string literal is a key; nothing else is.

    That is the one shape the real key sites share and the shape no log message has: a message is a
    `QStringLiteral` at its call site, never a named `constexpr` declaration. So the 145
    `QStringLiteral` occurrences under the overlay are not keys and are not reported.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(
        root / SURFACE_SOURCE,
        _surface_header(
            'constexpr const char *kAFixtureKey = "aFixtureService";',
            "constexpr int kAFixtureCount = 7;",
            'constexpr auto kAFixtureMessage = QStringLiteral("not a declaration of a key");',
        ),
    )

    findings = check_contract_surface(_over(register, root))

    assert _unassigned(findings) == [
        f"{CONSTANT_KIND} kAFixtureCount",
        f"{CONSTANT_KIND} kAFixtureMessage",
        f"{REGISTRY_KEY_KIND} kAFixtureKey",
    ]


def test_contract_surface_reads_a_constexpr_function_as_not_a_constant(tmp_path: Path) -> None:
    """A function's parameter list falls between the keyword and the brace, so it is not a value.

    The fixture header declares one in every case above, and this is the case that asserts it was
    never counted: a rule matching every `constexpr` would report it in all of them.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(root / SURFACE_SOURCE, _surface_header())

    declared = (root / SURFACE_SOURCE).read_text(encoding="utf-8")

    assert "constexpr int aFixtureFunction" in declared
    assert "kAFixtureMentioned" in declared
    assert check_contract_surface(_over(register, root)) == []


def test_contract_surface_rejects_one_kind_and_symbol_declared_twice(tmp_path: Path) -> None:
    """The register records one entry per kind and symbol, so two declarations are a collision.

    Reported rather than merged: merging would let a second declaration of a recorded symbol enter
    unnoticed, which is the opposite of what a surface ratchet is for.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(root / SURFACE_SOURCE, _surface_header("constexpr int kAFixtureTwice = 1;"))
    _write(
        root / f"{SOURCE_TREE}/AFixtureSurfaceAgain.h",
        _surface_header("constexpr int kAFixtureTwice = 2;"),
    )

    findings = check_contract_surface(_over(register, root))
    (collision,) = [f for f in findings if "already declared elsewhere" in f.message]

    assert collision.check == CONTRACT_SURFACE
    assert collision.gates == ()
    assert collision.blocking
    assert "kAFixtureTwice" in collision.message


def test_contract_surface_rejects_a_source_tree_it_could_not_walk(tmp_path: Path) -> None:
    """An inventory compared against nothing reports every element as removed, so this fails.

    It names all ten Gates, because a surface that could not be read leaves every Gate's
    Decision_Content unchecked rather than one Gate's.
    """
    premises = replace(_premises(_register()), root=tmp_path / "repo")

    (finding,) = check_contract_surface(premises)

    assert finding.check == CONTRACT_SURFACE
    assert finding.gates == GATE_IDS
    assert finding.blocking
    assert finding.message.startswith(f"{SOURCE_TREE}/ is not a directory")
    assert "reports every element as removed" in finding.message


def test_contract_surface_reads_the_real_source_tree_and_qualifies_every_symbol() -> None:
    """The one case for this rule whose evidence is the repository rather than a fixture.

    The repository declares four constants called `kServiceName`, in four modules, and they are four
    elements. This asserts the rule separates them by scope, because an unqualified extractor would
    report one element and three removals against a correct inventory -- and it would do so while
    looking like it was working.

    The element count is asserted as a measurement rather than a target. It is what task 9.4 has to
    record, and a change that moved it is a change to the contract surface, which is exactly what
    this rule exists to make visible.
    """
    declared, collisions = _surface(_source_tree(REPO_ROOT))
    symbols = {(element.kind, element.symbol) for element in declared}
    kinds = Counter(element.kind for element in declared)

    assert collisions == []
    assert len(declared) == 197
    assert kinds == {ENUMERATOR_KIND: 139, REGISTRY_KEY_KIND: 37, CONSTANT_KIND: 21}
    for module in (
        "AeroraTelemetrySet",
        "AeroraPayloadModel",
        "AeroraPowerModel",
        "AeroraMissionObserver",
    ):
        assert (REGISTRY_KEY_KIND, f"{module}::kServiceName") in symbols


# ---------------------------------------------------------------------------
# The three guard kinds
#
# Named with the `guard` token, so `-k guard` runs them beside the two value rules they change.
#
# Requirement 4 criterion 5 names nine guards in three shapes and does not distinguish them. Six are
# `static constexpr` values, two are functions holding no literal at all, and the ninth is a check
# rather than a symbol. These cases are what stops the register being forced to record a value the
# source does not hold: a `predicate` pins the expression a reviewer read, and a `check` is held by a
# rule whose existence is resolved rather than trusted.
# ---------------------------------------------------------------------------

# A second expression, which is what a changed body carries. The recorded one is the data half's
# `PREDICATE_EXPRESSION`, imported rather than respelled so both halves pin one string.
ANOTHER_EXPRESSION = "return aFixtureDelegate()->isSomethingElse();"


def _predicate(register: GateRegister, symbol: str, expression: str) -> GateRegister:
    """*register* with *symbol* recorded as a predicate pinning *expression*.

    The recorded *value* stays whatever criterion 5 names for the symbol, because that is the field
    the data half reads and the predicate shape does not change it. Only the expression moves.
    """
    return _with_guard(
        register,
        symbol,
        kind=PREDICATE_GUARD,
        declared_expression=expression,
        declared_in=SOLO_DECLARATION,
        asserted_in=[SOLO_SITE],
    )


def _defines(symbol: str, body: str) -> str:
    """A definition of *symbol* whose body is *body*, with the expression named in a comment too.

    The comment matters: a predicate is pinned by text, so a rule that read comments would pass on a
    file that only *described* the expression it no longer carries.
    """
    return (
        "#pragma once\n\n"
        f"// A fixture predicate. Its reviewed expression is {PREDICATE_EXPRESSION}\n"
        f"bool {_member(symbol)}()\n"
        "{\n"
        f"    {body}\n"
        "}\n"
    )


def test_guard_value_accepts_a_predicate_whose_declaration_carries_its_expression(
    tmp_path: Path,
) -> None:
    """A predicate guard holds no literal, so what is pinned is the expression a reviewer read."""
    register = _predicate(_register(), REPLAY_GUARD, PREDICATE_EXPRESSION)
    root = _tree(tmp_path / "repo", register)
    _write(root / SOLO_DECLARATION, _defines(REPLAY_GUARD, PREDICATE_EXPRESSION))

    assert check_guard_value(_premises_at(register, root)) == []


def test_guard_value_rejects_a_predicate_guard_whose_expression_changed(tmp_path: Path) -> None:
    """A change to the expression has not been reviewed, so it is rejected naming both.

    This is the case that would otherwise be unreachable. `AeroraCorePlugin::isAuthorizationEnforced()`
    delegates to the authorizer singleton, so recording it as a literal `false` would compare a
    delegation against the word "false" and reject a correct tree on every run.
    """
    register = _predicate(_register(), REPLAY_GUARD, PREDICATE_EXPRESSION)
    root = _tree(tmp_path / "repo", register)
    _write(root / SOLO_DECLARATION, _defines(REPLAY_GUARD, ANOTHER_EXPRESSION))

    (finding,) = check_guard_value(_premises_at(register, root))

    assert finding.check == GUARD_VALUE
    assert finding.gates == (MISSION_GATE,)
    assert finding.blocking
    assert PREDICATE_EXPRESSION in finding.message
    assert "holds no literal" in finding.message


def test_guard_value_rejects_a_predicate_guard_whose_symbol_was_withdrawn(tmp_path: Path) -> None:
    """ "Not found" is not "unchanged" for a predicate either."""
    register = _predicate(_register(), REPLAY_GUARD, PREDICATE_EXPRESSION)
    root = _tree(tmp_path / "repo", register)
    _write(root / SOLO_DECLARATION, DECLARES_NOTHING)

    findings = check_guard_value(_premises_at(register, root))
    (finding,) = [f for f in findings if f.check == GUARD_VALUE]

    assert finding.blocking
    assert "declares no symbol of that name" in finding.message


def test_guard_value_reads_a_predicate_expression_past_a_comment_quoting_it(
    tmp_path: Path,
) -> None:
    """Comments are blanked first, so a file that only describes the expression does not pass.

    The fixture's comment carries the recorded expression verbatim, so a rule reading raw text would
    find it and pass over a body that no longer holds it.
    """
    register = _predicate(_register(), REPLAY_GUARD, PREDICATE_EXPRESSION)
    root = _tree(tmp_path / "repo", register)
    _write(root / SOLO_DECLARATION, _defines(REPLAY_GUARD, ANOTHER_EXPRESSION))

    declared = (root / SOLO_DECLARATION).read_text(encoding="utf-8")

    assert declared.count(PREDICATE_EXPRESSION) == 1
    assert check_guard_value(_premises_at(register, root)) != []


def test_guard_value_and_guard_assertion_skip_a_guard_held_by_a_rule(tmp_path: Path) -> None:
    """A check-kind guard has no declaration and no assertion site, so neither rule reads it.

    The baseline registry carries exactly one: Requirement 4 criterion 5's ninth guard. Selected on
    the recorded kind rather than by naming it, which is what stops the next guard of this shape
    being a second name in an exclusion list.
    """
    register = _register()
    root = _tree(tmp_path / "repo", register)

    held_by_a_rule = [guard for guard in register.guards if guard.kind == CHECK_GUARD]
    premises = _premises_at(register, root)

    assert [guard.symbol for guard in held_by_a_rule] == [MAVLINK_IDENTIFIER_GUARD]
    assert held_by_a_rule[0].asserted_in == []
    assert check_guard_value(premises) == []
    assert check_guard_assertion(premises) == []


def test_guard_value_rejects_a_guard_held_by_a_rule_that_does_not_exist(tmp_path: Path) -> None:
    """A guard held by a rule is only held if the rule exists.

    Recording a name that resolves to nothing would leave the guard unenforced while reading as
    covered, which is the failure mode the whole module is written against.
    """
    register = _with_guard(
        _register(),
        REPLAY_GUARD,
        kind=CHECK_GUARD,
        recorded_value=None,
        held_by="a-rule-this-module-does-not-carry",
        asserted_in=[],
        asserted_nowhere=True,
        remedy=ASSERTION_REMEDY,
    )
    root = _tree(tmp_path / "repo", register)

    (finding,) = check_guard_value(_premises_at(register, root))

    assert finding.check == GUARD_VALUE
    assert finding.blocking
    assert "carries no rule of that name" in finding.message
    assert "reading as covered" in finding.message


def test_guard_assertion_reports_a_guard_the_register_records_as_asserted_nowhere(
    tmp_path: Path,
) -> None:
    """Two of the nine are asserted by nothing today, and the register says so with its remedy.

    An empty list read as satisfied would hide it; read as a blocking failure it would reject a
    correct register on every run. So the absence is recorded, prints every run, and does not block.
    """
    register = _with_guard(
        _register(),
        REPLAY_GUARD,
        asserted_in=[],
        asserted_nowhere=True,
        remedy=ASSERTION_REMEDY,
    )
    root = _tree(tmp_path / "repo", register)

    (finding,) = check_guard_assertion(_premises_at(register, root))

    assert finding.check == GUARD_ASSERTION
    assert finding.gates == (MISSION_GATE,)
    assert not finding.blocking
    assert ASSERTION_REMEDY in finding.message
    assert "no suite asserts this guard" in finding.message


def test_guard_assertion_blocks_on_an_empty_site_list_recording_no_absence(
    tmp_path: Path,
) -> None:
    """Without the explicit record it blocks, which keeps the recorded absence from being a default.

    The schema requires `assertedNowhere` and a remedy together, so this state is reachable only
    through a model. That is what makes the recorded absence a decision rather than an omission.
    """
    register = _with_guard(
        _register(), REPLAY_GUARD, asserted_in=[], asserted_nowhere=None, remedy=None
    )
    root = _tree(tmp_path / "repo", register)

    (finding,) = check_guard_assertion(_premises_at(register, root))

    assert finding.blocking
    assert "does not record that none exists" in finding.message


# ---------------------------------------------------------------------------
# autonomy-source and simulated-source
#
# The two guards whose content is an absence. `-k autonomy` and `-k simulated` reach them.
#
# Both exist because the thing guarded does not: nothing under `aerora/` declares an autonomy-source
# type, and `AeroraTelemetrySource` carries no simulated/real axis at all. So the real-tree cases are
# the sharpest evidence here, and the fixture cases assert that a declaration is rejected while a
# name written into prose is not.
# ---------------------------------------------------------------------------

# Where a rejecting case writes the declaration it asserts over.
ABSENT_SUBJECT_SOURCE = f"{SOURCE_TREE}/Telemetry/AFixtureAbsentSubject.h"


def _naming_in_prose(subject: str) -> str:
    """A header naming *subject* in a comment and declaring nothing.

    The real tree does exactly this: `AeroraFactGroupBase.h:19` lists `aeroraAutonomy` as one of six
    planned fact groups and `Telemetry/README.md` records it as CONTRACT-ONLY. Both must stay
    readable, so a rule that matched a mention would reject the tree it protects.
    """
    return f"#pragma once\n\n// A fixture header naming {subject} in prose and declaring nothing.\n"


@pytest.mark.parametrize(
    ("check", "gate", "declaration", "declared"),
    [
        (
            AUTONOMY_SOURCE,
            AUTONOMY_GATE,
            "class AFixtureAutonomySource\n{\n};\n",
            "AFixtureAutonomySource",
        ),
        (
            AUTONOMY_SOURCE,
            AUTONOMY_GATE,
            "struct AFixtureAutonomyState\n{\n};\n",
            "AFixtureAutonomyState",
        ),
        (
            SIMULATED_SOURCE,
            SIMULATION_GATE,
            "class AFixtureSimulatedFlag\n{\n};\n",
            "AFixtureSimulatedFlag",
        ),
        (
            SIMULATED_SOURCE,
            SIMULATION_GATE,
            "enum class AFixtureSimulatedKind\n{\n};\n",
            "AFixtureSimulatedKind",
        ),
    ],
)
def test_the_absent_subject_rules_reject_a_declaration_naming_the_gate(
    tmp_path: Path, check: str, gate: str, declaration: str, declared: str
) -> None:
    """The guard is that nothing declares the subject, so a declaration is the thing it rejects."""
    register = _register()
    root = _tree(tmp_path / "repo", register)
    _write(root / ABSENT_SUBJECT_SOURCE, f"#pragma once\n\n{declaration}")

    rule = check_autonomy_source if check == AUTONOMY_SOURCE else check_simulated_source
    (finding,) = rule(_premises_at(register, root))

    assert finding.check == check
    assert finding.gates == (gate,)
    assert finding.blocking
    assert declared in finding.message
    assert ABSENT_SUBJECT_SOURCE in finding.message


@pytest.mark.parametrize("check", [AUTONOMY_SOURCE, SIMULATED_SOURCE])
def test_the_absent_subject_rules_read_a_name_in_prose_as_not_a_declaration(
    tmp_path: Path, check: str
) -> None:
    """`aeroraAutonomy` is named in two real documents as a planned group, and both must stay."""
    register = _register()
    root = _tree(tmp_path / "repo", register)
    subject = "aeroraAutonomy and a simulated source" if check == AUTONOMY_SOURCE else "isSimulated"
    _write(root / ABSENT_SUBJECT_SOURCE, _naming_in_prose(subject))

    rule = check_autonomy_source if check == AUTONOMY_SOURCE else check_simulated_source

    assert subject in (root / ABSENT_SUBJECT_SOURCE).read_text(encoding="utf-8")
    assert rule(_premises_at(register, root)) == []


@pytest.mark.parametrize(
    ("check", "gate"), [(AUTONOMY_SOURCE, AUTONOMY_GATE), (SIMULATED_SOURCE, SIMULATION_GATE)]
)
def test_the_absent_subject_rules_stand_down_once_the_gate_records_an_accepted_closure(
    tmp_path: Path, check: str, gate: str
) -> None:
    """Once the Gate holds an accepted Closure_Record, the subject is what an owner signed for."""
    premises = _closing(tmp_path / "repo", gate)
    _write(
        premises.root / ABSENT_SUBJECT_SOURCE,
        "#pragma once\n\nclass AFixtureAutonomySimulated\n{\n};\n",
    )

    rule = check_autonomy_source if check == AUTONOMY_SOURCE else check_simulated_source

    assert not premises.derived.gate_open[gate].is_open
    assert rule(premises) == []


@pytest.mark.parametrize("check", [AUTONOMY_SOURCE, SIMULATED_SOURCE])
def test_the_absent_subject_rules_reject_a_source_tree_they_could_not_walk(
    tmp_path: Path, check: str
) -> None:
    """A run that searched nothing must not read as a run that found nothing."""
    premises = replace(_premises(_register()), root=tmp_path / "repo")

    rule = check_autonomy_source if check == AUTONOMY_SOURCE else check_simulated_source
    (finding,) = rule(premises)

    assert finding.check == check
    assert finding.blocking
    assert finding.message.startswith(f"{SOURCE_TREE}/ is not a directory")


def test_autonomy_source_reads_the_real_tree_and_finds_no_declaration() -> None:
    """The evidence for DG-AUT's guard, which is that nothing in the repository declares one.

    The planned fact group is named in prose in two places and both are asserted present, so the
    pass is evidence that a mention is not a declaration rather than that the rule looked nowhere.
    """
    planned = (REPO_ROOT / SOURCE_TREE / "Telemetry" / "AeroraFactGroupBase.h").read_text(
        encoding="utf-8"
    )

    assert "aeroraAutonomy" in planned
    assert check_autonomy_source(_premises(_register())) == []


def test_simulated_source_reads_the_real_tree_and_finds_no_declaration() -> None:
    """The evidence for DG-SIM's guard: `AeroraTelemetrySource` carries no simulated axis.

    Its only enumeration is `Attribution`, whose members are about which component sent a message.
    Asserted present so the pass is evidence the rule read the file that would carry the flag.
    """
    source = (REPO_ROOT / SOURCE_TREE / "Telemetry" / "AeroraTelemetrySource.h").read_text(
        encoding="utf-8"
    )

    assert "enum Attribution" in source
    assert check_simulated_source(_premises(_register())) == []


# ---------------------------------------------------------------------------
# overlay-isolation
#
# Requirement 10 criteria 10 and 11. The fixtures are purpose-built rather than `_tree()` trees,
# because this is the only rule in the module that walks the whole repository rather than the
# overlay: a `_tree()` fixture carries every other rule's subject, and every one of them would
# land inside this rule's reach and have to be recorded to keep the baseline passing.
#
# `_edit` and the two fictional outside paths come from the data half's tests on purpose. Both
# halves of this ratchet have to agree on what a recorded entry looks like, so importing the
# constructor is what makes a change to its shape fail both halves rather than one.
# ---------------------------------------------------------------------------

REACHING_SOURCE = "class AeroraFixtureReach;"
MACRO_SOURCE = "#define AERORA_FIXTURE_REACH 1"
PROSE_SOURCE = "// Aerora is named here in prose, and aerora again in lower case."


def _reaching(root: Path, *files: tuple[str, str]) -> Path:
    """*root* carrying *files* as path and content, and nothing else."""
    for path, text in files:
        _write(root / path, text)
    return root


def _with_edits(register: GateRegister, *paths: str) -> GateRegister:
    """*register* recording one overlay edit per path in *paths*."""
    return replace(register, overlay_edits=[_edit(path) for path in paths])


def test_overlay_isolation_accepts_a_tree_whose_reaches_the_register_records(
    tmp_path: Path,
) -> None:
    """Two reaches, both recorded, and a third file carrying none so the walk is not empty.

    The examined count is asserted beside the carrying list for the reason the walk returns both:
    two files agreeing with two records produces the same empty finding list as a walk that read
    nothing at all, and the counts are the only thing that tells those two runs apart.
    """
    register = _with_edits(_register(), OUTSIDE_PATH, ANOTHER_OUTSIDE_PATH)
    root = _reaching(
        tmp_path / "repo",
        (OUTSIDE_PATH, REACHING_SOURCE),
        (ANOTHER_OUTSIDE_PATH, MACRO_SOURCE),
        ("src/AFixtureQuiet.h", PROSE_SOURCE),
    )

    examined, carrying, unreadable = _isolation_candidates(root)

    assert check_overlay_isolation(_premises_at(register, root)) == []
    assert unreadable == []
    assert carrying == [OUTSIDE_PATH, ANOTHER_OUTSIDE_PATH]
    assert len(examined) == 3


def test_overlay_isolation_names_a_file_the_register_does_not_record(tmp_path: Path) -> None:
    """An unrecorded reach, named as unassigned Decision_Content against no Gate.

    It renders `UNASSIGNED` for the reason `contract-surface` findings do. Overlay isolation is
    unconditional, so there is no Gate whose closure would make a reach acceptable -- what makes
    one acceptable is an owner deciding to spend the separability and recording that they did.
    """
    root = _reaching(tmp_path / "repo", (OUTSIDE_PATH, REACHING_SOURCE))

    (finding,) = check_overlay_isolation(_premises_at(_with_edits(_register()), root))

    assert finding.check == OVERLAY_ISOLATION
    assert finding.gates == ()
    assert UNASSIGNED in str(finding)
    assert finding.blocking
    assert finding.message.startswith(f"{OUTSIDE_PATH}: ")
    assert f"lies outside {OVERLAY_TREE}/" in finding.message
    assert "Requirement 10 criterion 11" in finding.message


def test_overlay_isolation_names_a_recorded_file_that_no_longer_reaches(tmp_path: Path) -> None:
    """The other direction, which a containment test either way round would pass over.

    A record outliving its subject is not harmless. The recorded list is read as a count -- it is
    the number a reviewer weighs the next reach against -- so a stale entry makes the overlay look
    less separable than it is, and makes the next reach look cheaper than it is.
    """
    register = _with_edits(_register(), OUTSIDE_PATH)
    root = _reaching(tmp_path / "repo", (OUTSIDE_PATH, PROSE_SOURCE))

    (finding,) = check_overlay_isolation(_premises_at(register, root))

    assert finding.check == OVERLAY_ISOLATION
    assert finding.blocking
    assert finding.message.startswith(f"{OUTSIDE_PATH}: ")
    assert "is a removal rather than a satisfied entry" in finding.message


def test_overlay_isolation_reports_both_directions_as_separate_findings(tmp_path: Path) -> None:
    """One unrecorded reach and one stale record are two findings and never one summary.

    Each has its own repair -- record the reach, or withdraw the entry -- so a rule that reported
    the two sets as differing would be naming nothing a reviewer could act on.
    """
    register = _with_edits(_register(), OUTSIDE_PATH)
    root = _reaching(
        tmp_path / "repo",
        (OUTSIDE_PATH, PROSE_SOURCE),
        (ANOTHER_OUTSIDE_PATH, REACHING_SOURCE),
    )

    unrecorded, removed = check_overlay_isolation(_premises_at(register, root))

    assert unrecorded.message.startswith(f"{ANOTHER_OUTSIDE_PATH}: ")
    assert "records no overlay edit for it" in unrecorded.message
    assert removed.message.startswith(f"{OUTSIDE_PATH}: ")
    assert "is a removal rather than a satisfied entry" in removed.message


def test_overlay_isolation_rejects_a_tree_it_could_not_walk(tmp_path: Path) -> None:
    """A walk that examined nothing is a failure, and it is the only finding the run reports.

    The register records a reach here, so a rule that went on to compare the two sets would report
    that reach as removed: a correct entry named as stale, against a tree it never read. Returning
    before the comparison is what keeps the real failure the only thing on the page.
    """
    register = _with_edits(_register(), OUTSIDE_PATH)
    premises = replace(_premises(register), root=tmp_path / "absent")

    (finding,) = check_overlay_isolation(premises)

    assert finding.check == OVERLAY_ISOLATION
    assert finding.gates == ()
    assert finding.blocking
    assert finding.message.startswith(f"no file outside {OVERLAY_TREE}/ was examined")
    assert "is a failure rather than a pass" in finding.message


def test_overlay_isolation_does_not_walk_the_trees_that_are_aerora_by_construction(
    tmp_path: Path,
) -> None:
    """The overlay, the tools and the specs name Aerora in every file, and none of it is a reach.

    A rule that read them would report hundreds of findings and be deleted rather than obeyed. The
    sibling directory pins the exclusion to a whole path segment: `toolsmith/` is not `tools/`, and
    an exclusion written as a bare prefix test would silence a real reach.
    """
    root = _reaching(
        tmp_path / "repo",
        (f"{OVERLAY_TREE}/src/AeroraFixture.h", REACHING_SOURCE),
        ("tools/analyzers/AFixture.h", REACHING_SOURCE),
        (".kiro/specs/AFixture.txt", REACHING_SOURCE),
        ("toolsmith/AFixtureReach.h", REACHING_SOURCE),
    )

    examined, carrying, _ = _isolation_candidates(root)

    assert examined == ["toolsmith/AFixtureReach.h"]
    assert carrying == ["toolsmith/AFixtureReach.h"]


def test_overlay_isolation_reads_an_identifier_and_not_the_word(tmp_path: Path) -> None:
    """Prose naming Aerora is not a reach into the tree, and the distinction is load-bearing.

    `.github/workflows/aerora-lint.yml` is why it has to be. That file is Aerora-owned CI that
    names no Aerora suite, and it is correctly absent from the recorded set while its two sibling
    legs are present -- those two name suites and it names none. A rule matching the word would
    record a workflow file as a core edit, and the recorded count would stop meaning anything.

    The prose file is asserted to contain the word before the rule declines it, so the pass is
    evidence the rule read the file and judged it rather than evidence of an empty fixture.
    """
    root = _reaching(
        tmp_path / "repo",
        ("src/AFixtureProse.h", PROSE_SOURCE),
        ("src/AFixtureMacro.h", MACRO_SOURCE),
    )

    examined, carrying, _ = _isolation_candidates(root)

    assert "Aerora" in PROSE_SOURCE
    assert len(examined) == 2
    assert carrying == ["src/AFixtureMacro.h"]


def test_overlay_isolation_passes_over_a_suffix_that_carries_no_declaration(
    tmp_path: Path,
) -> None:
    """A mention in documentation costs no separability, so the walk does not read it.

    The same content under a header suffix is a reach, which is what makes this a statement about
    the suffix rather than about the content.
    """
    root = _reaching(
        tmp_path / "repo",
        ("docs/AFixtureReach.md", REACHING_SOURCE),
        ("src/AFixtureReach.h", REACHING_SOURCE),
    )

    examined, carrying, _ = _isolation_candidates(root)

    assert examined == ["src/AFixtureReach.h"]
    assert carrying == ["src/AFixtureReach.h"]


def test_overlay_isolation_reads_the_real_tree_and_finds_the_six_reaches() -> None:
    """The one case whose evidence is the repository, and the surface task 2.4 has to record.

    The six are asserted as a list rather than a count because each is a different kind of reach
    -- a core header, a core test, a build file, two CI legs and a packaging leg -- so a change
    that moved the count without moving the list would be a different surface at the same size.

    The packaging leg is the one that proves the pin earns its keep. It was added without a
    recorded overlay edit and sat unreported for two commits, because aerora-lint.yml triggers on
    itself rather than on the workflow directory, so adding a sibling workflow ran no lint leg.
    A count would have caught it here just as well; the list says which reach appeared.

    The examined count is a floor rather than an equality. This is a fork that merges upstream, so
    the number of core files moves for reasons that have nothing to do with the overlay, and a
    pinned figure would fail on somebody else's commit. A floor still catches what the empty-walk
    guard cannot: that guard fails a walk that stopped, and this one fails a walk that narrowed.
    """
    examined, carrying, unreadable = _isolation_candidates(REPO_ROOT)

    assert unreadable == []
    assert carrying == [
        ".github/workflows/aerora-linux.yml",
        ".github/workflows/aerora-package.yml",
        ".github/workflows/aerora-windows.yml",
        "cmake/QGCTest.cmake",
        CORE_HEADER,
        "test/Vehicle/MavCommandAuthorizationTest.cc",
    ]
    assert len(examined) > 2000
    assert "CMakeLists.txt" in examined
    assert not any(path.startswith((f"{OVERLAY_TREE}/", "tools/", ".kiro/")) for path in examined)


# ---------------------------------------------------------------------------
# Properties
#
# Quantified over mutations of the agreeing tree, for the reason the data half's properties are
# quantified over mutations of the agreeing register: every rule here is a predicate relating a
# source tree to a recorded inventory, and a tree drawn from nothing disagrees with the register on
# a dozen axes at once, so a finding would prove nothing about which rule caught it.
# ---------------------------------------------------------------------------

# Property 23: The contract surface in source and in the register agree in both directions.
# Validates: Requirements 3.12, 4.2, 6.14, 7.16, 8.13, 9.15, 10.16, 11.17, 12.19, 13.12, 15.14.
#
# Three fixture symbols, each independently in one of five states. The states are what the rule has
# to tell apart: declared and unrecorded, recorded and undeclared, declared and recorded against an
# open Gate, declared and recorded as cleared, and neither. Three of the five are findings and two
# are not, so a rule that reported everything and a rule that reported nothing each fail half of
# this.
_SURFACE_ELEMENTS = ("kAFixtureAlpha", "kAFixtureBeta", "kAFixtureGamma")
_DECLARED_ONLY = "declared-and-unrecorded"
_RECORDED_ONLY = "recorded-and-undeclared"
_MAPPED_OPEN = "declared-and-mapped-to-an-open-gate"
_CLEARED = "declared-and-cleared"
_NEITHER = "neither"
_SURFACE_STATES = (_DECLARED_ONLY, _RECORDED_ONLY, _MAPPED_OPEN, _CLEARED, _NEITHER)
_RECORDED_STATES = frozenset({_RECORDED_ONLY, _MAPPED_OPEN, _CLEARED})
_DECLARED_STATES = frozenset({_DECLARED_ONLY, _MAPPED_OPEN, _CLEARED})


@settings(max_examples=MAX_EXAMPLES, deadline=None)
@given(
    st.dictionaries(
        st.sampled_from(_SURFACE_ELEMENTS), st.sampled_from(_SURFACE_STATES), min_size=1
    )
)
def test_the_contract_surface_in_source_and_in_the_register_agree_in_both_directions(
    tmp_path_factory: pytest.TempPathFactory, states: dict[str, str]
) -> None:
    """Set equality, which is what lets the surface grow and shrink only deliberately.

    A containment either way would be half a ratchet. Reading source against the inventory alone
    lets a recorded element be deleted from the tree without anybody noticing the register still
    carries it; reading the inventory against source alone lets a new Contract_Value be declared
    and go unassigned. So the rule is an equality, and each direction has its own message because
    each has its own repair.

    The finding count is asserted alongside the memberships. Three disagreeing elements are three
    findings and never one summary, and a rule that reported the two sets as differing would
    satisfy every membership assertion here while naming nothing a reviewer could act on.
    """
    recorded = [
        _element(
            symbol,
            CONSTANT_KIND,
            ROLE_GATE if state == _MAPPED_OPEN else NO_GATE,
            None if state == _MAPPED_OPEN else "a fixture reason",
        )
        for symbol, state in sorted(states.items())
        if state in _RECORDED_STATES
    ]
    declared = sorted(symbol for symbol, state in states.items() if state in _DECLARED_STATES)
    register = _with_surface(_register(), *recorded)
    root = _tree(tmp_path_factory.mktemp("surface") / "repo", register)
    _write(
        root / SURFACE_SOURCE,
        _surface_header(*(f"constexpr int {symbol} = 1;" for symbol in declared)),
    )

    findings = check_contract_surface(_over(register, root))

    unassigned = sorted(name for name, state in states.items() if state == _DECLARED_ONLY)
    withheld = sorted(name for name, state in states.items() if state == _MAPPED_OPEN)
    removed = sorted(name for name, state in states.items() if state == _RECORDED_ONLY)
    assert len(findings) == len(unassigned) + len(withheld) + len(removed)
    assert _unassigned(findings) == [f"{CONSTANT_KIND} {symbol}" for symbol in unassigned]
    for symbol in withheld:
        assert any(
            symbol in finding.message
            and "waits on the signature" in finding.message
            and finding.gates == (ROLE_GATE,)
            for finding in findings
        )
    for symbol in removed:
        assert any(
            symbol in finding.message
            and "is a removal rather than a satisfied entry" in finding.message
            for finding in findings
        )
    assert all(finding.blocking for finding in findings)
    assert all(finding.check == CONTRACT_SURFACE for finding in findings)


# Every recorded guard, flipped in its own declaration.
#
# Task 7.9 asks for a header flipping *each* recorded guard rather than one of them, and the reason
# is the shape of the rule: it resolves a declaration per registry entry, so a guard whose
# declaration it stopped finding would go quiet on its own while the other seven kept passing. The
# parametrisation is over the registry's `literal` entries, because those are the ones that carry a
# value to flip -- a `predicate` entry carries an expression and a `check` entry carries the name of
# the rule that holds it, and both have cases of their own above.
_LITERAL_GUARDS = tuple(
    sorted(guard.symbol for guard in _register().guards if guard.kind == LITERAL_GUARD)
)


@pytest.mark.parametrize("symbol", _LITERAL_GUARDS)
def test_guard_value_rejects_each_recorded_guard_flipped_in_its_declaration(
    tmp_path: Path, symbol: str
) -> None:
    """One case per guard, each naming the guard, both values and every Gate it holds open.

    The Gates are compared against `BASELINE_GUARD_GATES`, which is requirements.md's mapping read
    out by hand, so what this asserts is that the finding names the decisions Requirement 4
    criterion 5 maps the guard to rather than whatever the register happens to record.
    """
    register = _alone(_register(), symbol)
    root = _tree(tmp_path / "repo", register)
    _write(root / SOLO_DECLARATION, _declares(symbol, ANOTHER_VALUE))

    result = _review(root)

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    gates = ", ".join(BASELINE_GUARD_GATES[symbol])
    assert line.startswith(f"{GUARD_VALUE}: {gates}: {symbol}: ")
    assert repr(_literal(register, symbol)) in line
    assert repr(ANOTHER_VALUE) in line
    assert SOLO_DECLARATION in line


@pytest.mark.parametrize("symbol", _LITERAL_GUARDS)
def test_guard_assertion_rejects_each_recorded_guard_whose_site_stopped_naming_it(
    tmp_path: Path, symbol: str
) -> None:
    """And one per guard from the assertion side, because removing the check is the other half.

    Requirement 4 criterion 9 treats removing the assertion as seriously as moving the value, and
    the two rules resolve different files -- a declaration under `aerora/src/**` and a site under
    `aerora/test/**` -- so a guard covered by one is not covered by the other.
    """
    register = _alone(_register(), symbol)
    root = _tree(tmp_path / "repo", register)
    _write(root / SOLO_SITE, ASSERTS_NOTHING)

    result = _review(root)

    (line,) = _lines(result, register)
    assert not result.passed
    assert result.issues == 1
    gates = ", ".join(BASELINE_GUARD_GATES[symbol])
    assert line.startswith(f"{GUARD_ASSERTION}: {gates}: {symbol}: ")
    assert f"{SOLO_SITE} no longer names the symbol" in line
    assert "Requirement 4 criterion 9" in line


# Property 9: Aerora logic stays inside the overlay.
# Validates: Requirements 10.10, 10.11.
#
# Three fixture paths, each independently in one of four states. The states are what the rule has
# to tell apart: reaching and recorded, reaching and unrecorded, recorded and no longer reaching,
# and neither. Two of the four are findings and two are not, so a rule that reported everything and
# a rule that reported nothing each fail half of this.
#
# Quantified over the *tree* as well as the register, which is what makes it a property of the set
# equality rather than of one direction. The containment tests pass under a rule that only looks
# one way; this does not.
_ISOLATION_PATHS = (OUTSIDE_PATH, ANOTHER_OUTSIDE_PATH, "cmake/AFixture.cmake")
_REACHING_RECORDED = "reaching-and-recorded"
_REACHING_UNRECORDED = "reaching-and-unrecorded"
_ISOLATION_RECORDED_ONLY = "recorded-and-no-longer-reaching"
_ISOLATION_NEITHER = "neither"
_ISOLATION_STATES = (
    _REACHING_RECORDED,
    _REACHING_UNRECORDED,
    _ISOLATION_RECORDED_ONLY,
    _ISOLATION_NEITHER,
)
_REACHING = frozenset({_REACHING_RECORDED, _REACHING_UNRECORDED})
_RECORDED = frozenset({_REACHING_RECORDED, _ISOLATION_RECORDED_ONLY})


@settings(max_examples=MAX_EXAMPLES, deadline=None)
@given(
    st.dictionaries(
        st.sampled_from(_ISOLATION_PATHS), st.sampled_from(_ISOLATION_STATES), min_size=1
    )
)
def test_aerora_logic_stays_inside_the_overlay(
    tmp_path_factory: pytest.TempPathFactory, states: dict[str, str]
) -> None:
    """Set equality in both directions, over a generated tree and a generated record.

    A reach nobody recorded spends the separability of the overlay without anybody deciding to. A
    record whose file no longer reaches has outlived its subject, and that matters because the list
    is read as a count -- it is the number a reviewer weighs the next reach against, so a stale
    entry makes the next one look cheaper than it is.

    Every file is written either way, carrying an identifier or carrying only prose, so the walk
    examines the same set in every example and a finding is never the result of a file being absent.
    """
    register = _with_edits(
        _register(), *sorted(path for path, state in states.items() if state in _RECORDED)
    )
    root = _reaching(
        tmp_path_factory.mktemp("isolation") / "repo",
        *(
            (path, REACHING_SOURCE if state in _REACHING else PROSE_SOURCE)
            for path, state in sorted(states.items())
        ),
    )

    examined, carrying, unreadable = _isolation_candidates(root)
    findings = check_overlay_isolation(_premises_at(register, root))

    unrecorded = sorted(p for p, s in states.items() if s == _REACHING_UNRECORDED)
    removed = sorted(p for p, s in states.items() if s == _ISOLATION_RECORDED_ONLY)
    assert unreadable == []
    assert len(examined) == len(states)
    assert carrying == sorted(p for p, s in states.items() if s in _REACHING)
    assert len(findings) == len(unrecorded) + len(removed)
    for path in unrecorded:
        assert any(
            f.message.startswith(f"{path}: ") and "records no overlay edit for it" in f.message
            for f in findings
        )
    for path in removed:
        assert any(
            f.message.startswith(f"{path}: ")
            and "is a removal rather than a satisfied entry" in f.message
            for f in findings
        )
    assert all(f.check == OVERLAY_ISOLATION and f.blocking and f.gates == () for f in findings)

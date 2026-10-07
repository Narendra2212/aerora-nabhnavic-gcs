# ============================================================================
# Aerora Payload (M-Payload) and Power (M-Power) - source manifest
#
# Variables only. This file defines no targets, calls no add_test(), and sets nothing in
# PARENT_SCOPE, so including it has no side effects beyond defining the variables below.
#
# Matches the manifest convention established by aerora/src/Telemetry/Telemetry.cmake and
# aerora/src/Vehicle/Vehicle.cmake: the aggregation into flat CUSTOM_* variables belongs to
# aerora/CMakeLists.txt (M-Plugin), so this file keeps the payload and power file lists with their
# modules and makes integrating them a small, reviewable change there rather than an edit by this
# workstream.
#
# Two modules share one manifest because they share one file - src/Supervisory/AeroraReportedValue
# - and splitting the manifest would mean either duplicating that entry or making one module depend
# on the other. See "What Supervisory/ is" below.
#
# ----------------------------------------------------------------------------
# Integration, in aerora/CMakeLists.txt
# ----------------------------------------------------------------------------
#
#   include(${CMAKE_CURRENT_SOURCE_DIR}/src/PayloadPower.cmake)
#
#   list(APPEND CUSTOM_SOURCES ${AERORA_PAYLOAD_POWER_SOURCES})
#   list(APPEND CUSTOM_INCLUDE_DIRECTORIES ${AERORA_PAYLOAD_POWER_INCLUDE_DIRECTORIES})
#   list(APPEND CUSTOM_LIBRARIES ${AERORA_PAYLOAD_POWER_LIBRARIES})
#
#   if(QGC_BUILD_TESTING)
#       list(APPEND CUSTOM_SOURCES ${AERORA_PAYLOAD_POWER_TEST_SOURCES})
#       list(APPEND CUSTOM_INCLUDE_DIRECTORIES ${AERORA_PAYLOAD_POWER_TEST_INCLUDE_DIRECTORIES})
#       include(QGCTest)
#       add_qgc_test(AeroraReportedValueTest LABELS Unit)
#       add_qgc_test(AeroraPayloadModelTest LABELS Unit)
#       add_qgc_test(AeroraPowerModelTest LABELS Unit)
#   endif()
#
# The list(APPEND ...) calls must come before the existing set(CUSTOM_SOURCES ... PARENT_SCOPE), or
# the PARENT_SCOPE set must be repeated afterwards - a plain list(APPEND) does not propagate to the
# parent scope on its own.
#
# ----------------------------------------------------------------------------
# ORDERING REQUIREMENT - this manifest is not self-sufficient
# ----------------------------------------------------------------------------
#
# These modules consume two other workstreams' headers and do not vendor copies of them:
#
#   * M-Telemetry (Tab 4) - AeroraTelemetryState.h, AeroraStateValue.h, AeroraTelemetrySource.h and
#     AeroraFreshness.h. AeroraReportedValue delegates every state-class decision to
#     AeroraTelemetryState::resolve() and every freshness decision to AeroraFreshness::evaluate().
#   * M-Vehicle (Tab 3) - AeroraVehicleCapabilities.h. Both models read the capability inventory to
#     gate their surfaces.
#
# So aerora/src/Telemetry/Telemetry.cmake and aerora/src/Vehicle/Vehicle.cmake must be included and
# aggregated as well, and their include directories must be on the target. This manifest
# deliberately does not include them itself: a manifest that pulled in its dependencies would make
# the aggregation order invisible at the point where it is decided.
#
# The include directories this manifest exports cover its own three directories only. The telemetry
# and vehicle directories arrive via AERORA_TELEMETRY_INCLUDE_DIRECTORIES and
# AERORA_VEHICLE_INCLUDE_DIRECTORIES.
#
# ----------------------------------------------------------------------------
# What Supervisory/ is, and what it is not
# ----------------------------------------------------------------------------
#
# NOT a new module. src/Supervisory/AeroraReportedValue is the one type M-Payload and M-Power share,
# placed in its own directory so that neither module has to depend on the other for it. The module
# architecture in 02-module-architecture.md is unchanged by its existence.
#
# It carries its own direct unit test at test/Supervisory/AeroraReportedValueTest, because the
# staleness-verdict latch is a property of the primitive rather than of either consumer, and a
# regression there should be attributed to the primitive instead of surfacing twice downstream.
#
# It is a reasonable candidate for absorption into M-Telemetry, which is where the state binding
# otherwise lives. That is Tab 1's call, and the move is a directory rename plus one line here - no
# API change - because the type owns no state semantics of its own.
#
# ----------------------------------------------------------------------------
# One note on include(QGCTest) in the snippet above
# ----------------------------------------------------------------------------
#
# QGCTest.cmake carries an include_guard(GLOBAL) and defines the check / check-unit / ... custom
# targets. The overlay is configured before test/, so including it from the overlay moves those
# convenience targets into the overlay directory. They are global by name, so `ninja check` still
# works. The deferred alternative, if that side effect is unwanted:
#
#   cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}"
#                  CALL add_qgc_test AeroraPayloadModelTest LABELS Unit)
#
# Either works; the choice belongs to M-Plugin, and consistency with the telemetry and vehicle
# manifests is the better reason to pick one.
#
# Note also that test/CMakeLists.txt escalates the sanitizer timeouts after the overlay has been
# configured, so a test registered from the overlay does not pick that escalation up. Pass an
# explicit TIMEOUT to add_qgc_test if a sanitizer lane needs one. Both tests here are pure in-memory
# state tests driven by a manual clock, with no vehicle, link, file or network I/O, so the default
# 60s Unit timeout has a very large margin even instrumented.
# ============================================================================

set(AERORA_PAYLOAD_POWER_DIR ${CMAKE_CURRENT_LIST_DIR})

# ----------------------------------------------------------------------------
# Production sources
#
# No Aerora FactGroup appears here, and none is registered through FirmwarePlugin::factGroups().
# AeroraPayloadFactGroup and AeroraPowerFactGroup remain CONTRACT-ONLY for two independent reasons,
# both source-verified rather than assumed:
#
#   * No Aerora dialect exists, so no handleMessage body can be written, and no Aerora fact name,
#     unit, range or enumeration member is specified anywhere (Q-023, Q-024). Declaring facts now
#     would mean inventing them.
#   * A Fact is unusable without metadata. Fact::setRawValue is a no-op that warns when _metaData is
#     null (src/FactSystem/Fact.cc:135-161), and the metadata resource would have to carry the fact
#     names that do not exist yet.
#
# The attachment seam itself is also unresolved: FirmwarePlugin::factGroups() hands every vehicle of
# a firmware class the same FactGroup objects because the plugin is a shared singleton - Q4-002 /
# R4-001. That decision is the orchestrator's.
# ----------------------------------------------------------------------------

set(AERORA_PAYLOAD_POWER_SOURCES
    ${AERORA_PAYLOAD_POWER_DIR}/Supervisory/AeroraReportedValue.cc
    ${AERORA_PAYLOAD_POWER_DIR}/Supervisory/AeroraReportedValue.h
    ${AERORA_PAYLOAD_POWER_DIR}/Payload/AeroraPayloadCommandSink.h
    ${AERORA_PAYLOAD_POWER_DIR}/Payload/AeroraPayloadModel.cc
    ${AERORA_PAYLOAD_POWER_DIR}/Payload/AeroraPayloadModel.h
    ${AERORA_PAYLOAD_POWER_DIR}/Power/AeroraPowerModel.cc
    ${AERORA_PAYLOAD_POWER_DIR}/Power/AeroraPowerModel.h
)

# AeroraPayloadCommandSink.h is listed deliberately even though it has no translation unit: it
# declares AeroraPayloadRequest, which is a Q_GADGET, so AUTOMOC has to see the header.

set(AERORA_PAYLOAD_POWER_INCLUDE_DIRECTORIES
    ${AERORA_PAYLOAD_POWER_DIR}/Supervisory
    ${AERORA_PAYLOAD_POWER_DIR}/Payload
    ${AERORA_PAYLOAD_POWER_DIR}/Power
)

# QGC::Timing carries RuntimeScheduler.h and ScheduledTask.h as a PUBLIC header file set, and links
# QGC::Clock PUBLIC for MonotonicClock.h. AeroraReportedValue schedules its own staleness-expiry
# callback through ScheduledTask, so the dependency is listed explicitly rather than relying on the
# application target picking it up transitively from src/Vehicle or from M-Telemetry.
set(AERORA_PAYLOAD_POWER_LIBRARIES QGC::Timing)

# ----------------------------------------------------------------------------
# Mock and test sources
#
# AeroraPayloadMock and AeroraPowerMock are MOCK / TEST ONLY. They live in the test tree and are
# listed only here, and each header fails the build with #error if it is ever compiled without
# QGC_UNITTEST_BUILD, so both guards have to be defeated together for either to reach a shipped
# binary.
#
# Other workstreams that want the mocks need AERORA_PAYLOAD_POWER_TEST_INCLUDE_DIRECTORIES on the
# target. Neither mock needs a fixture, a link, a Vehicle or hardware; both need a RuntimeScheduler,
# and tests should pass ManualScheduler so the result is deterministic.
# ----------------------------------------------------------------------------

set(AERORA_PAYLOAD_POWER_TEST_DIR ${AERORA_PAYLOAD_POWER_DIR}/../test)
cmake_path(NORMAL_PATH AERORA_PAYLOAD_POWER_TEST_DIR)

set(AERORA_PAYLOAD_POWER_TEST_SOURCES
    ${AERORA_PAYLOAD_POWER_TEST_DIR}/Supervisory/AeroraReportedValueTest.cc
    ${AERORA_PAYLOAD_POWER_TEST_DIR}/Supervisory/AeroraReportedValueTest.h
    ${AERORA_PAYLOAD_POWER_TEST_DIR}/Payload/AeroraPayloadMock.cc
    ${AERORA_PAYLOAD_POWER_TEST_DIR}/Payload/AeroraPayloadMock.h
    ${AERORA_PAYLOAD_POWER_TEST_DIR}/Payload/AeroraPayloadModelTest.cc
    ${AERORA_PAYLOAD_POWER_TEST_DIR}/Payload/AeroraPayloadModelTest.h
    ${AERORA_PAYLOAD_POWER_TEST_DIR}/Power/AeroraPowerMock.cc
    ${AERORA_PAYLOAD_POWER_TEST_DIR}/Power/AeroraPowerMock.h
    ${AERORA_PAYLOAD_POWER_TEST_DIR}/Power/AeroraPowerModelTest.cc
    ${AERORA_PAYLOAD_POWER_TEST_DIR}/Power/AeroraPowerModelTest.h
)

set(AERORA_PAYLOAD_POWER_TEST_INCLUDE_DIRECTORIES
    ${AERORA_PAYLOAD_POWER_TEST_DIR}/Supervisory
    ${AERORA_PAYLOAD_POWER_TEST_DIR}/Payload
    ${AERORA_PAYLOAD_POWER_TEST_DIR}/Power
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/test/UnitTestFramework
    ${CMAKE_SOURCE_DIR}/test/Utilities/Timing/Support
)

# CTest names, so the aggregation point can register them without reading this file.
#
# AeroraReportedValueTest is new. It needs one line added to aerora/CMakeLists.txt, which this
# workstream does not own:
#
#     add_qgc_test(AeroraReportedValueTest LABELS Unit)
#
# alongside the two existing entries. Nothing else in the aggregation changes: the test sources and
# the new Supervisory test include directory arrive through the variables above, which
# aerora/CMakeLists.txt already appends wholesale.
set(AERORA_PAYLOAD_POWER_TEST_NAMES
    AeroraReportedValueTest
    AeroraPayloadModelTest
    AeroraPowerModelTest
)

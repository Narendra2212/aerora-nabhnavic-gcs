# ============================================================================
# Aerora Telemetry module (M-Telemetry) - source manifest
#
# Variables only. This file defines no targets, calls no add_test(), and sets nothing in
# PARENT_SCOPE, so including it has no side effects beyond defining the variables below.
#
# The overlay build contract aggregates everything into flat CUSTOM_* variables in
# aerora/CMakeLists.txt (consumed by src/CMakeLists.txt, which calls target_sources /
# target_include_directories / target_link_libraries on the application target). That aggregation
# file belongs to M-Plugin, so this manifest keeps the telemetry file list with the telemetry module
# and makes integrating it a small, reviewable change there rather than an edit by this workstream.
#
# ----------------------------------------------------------------------------
# Integration, in aerora/CMakeLists.txt
# ----------------------------------------------------------------------------
#
#   include(${CMAKE_CURRENT_SOURCE_DIR}/src/Telemetry/Telemetry.cmake)
#
#   list(APPEND CUSTOM_SOURCES ${AERORA_TELEMETRY_SOURCES})
#   list(APPEND CUSTOM_INCLUDE_DIRECTORIES ${AERORA_TELEMETRY_INCLUDE_DIRECTORIES})
#   list(APPEND CUSTOM_LIBRARIES ${AERORA_TELEMETRY_LIBRARIES})
#
#   if(QGC_BUILD_TESTING)
#       list(APPEND CUSTOM_SOURCES ${AERORA_TELEMETRY_TEST_SOURCES})
#       list(APPEND CUSTOM_INCLUDE_DIRECTORIES ${AERORA_TELEMETRY_TEST_INCLUDE_DIRECTORIES})
#       include(QGCTest)
#       add_qgc_test(AeroraTelemetryStateTest LABELS Unit)
#       add_qgc_test(AeroraTelemetryAttachmentTest LABELS Unit)
#   endif()
#
# The list(APPEND ...) calls must come before the existing set(CUSTOM_SOURCES ... PARENT_SCOPE),
# or the PARENT_SCOPE set must be repeated afterwards - a plain list(APPEND) does not propagate to
# the parent scope on its own.
#
# Two notes on the test wiring, both verified by reading the build files rather than assumed:
#
#   * No core edit is required. test/CMakeLists.txt has no overlay hook, but it does not need one:
#     overlay test sources compile into the same application target, which already links Qt6::Test
#     and carries the test include directories when QGC_BUILD_TESTING is on. UT_REGISTER_TEST
#     self-registers at static initialisation and add_qgc_test() only adds a CTest entry that runs
#     the application with --unittest:<name>.
#   * The overlay is configured before test/ (root CMakeLists.txt adds the custom directory at :403
#     and test/ at :445), so the sanitizer timeout escalation in test/CMakeLists.txt is applied after
#     this test is registered and will not apply to it. Pass an explicit TIMEOUT to add_qgc_test if a
#     sanitizer lane needs one.
# ============================================================================

set(AERORA_TELEMETRY_DIR ${CMAKE_CURRENT_LIST_DIR})

# ----------------------------------------------------------------------------
# Production sources
#
# No concrete fact group appears here. All six Aerora groups are CONTRACT-ONLY because no Aerora
# dialect exists to parse; AeroraFactGroupBase is the part that is buildable and testable today.
# ----------------------------------------------------------------------------

set(AERORA_TELEMETRY_SOURCES
    ${AERORA_TELEMETRY_DIR}/AeroraFactGroupBase.cc
    ${AERORA_TELEMETRY_DIR}/AeroraFactGroupBase.h
    ${AERORA_TELEMETRY_DIR}/AeroraFactGroupRegistrar.cc
    ${AERORA_TELEMETRY_DIR}/AeroraFactGroupRegistrar.h
    ${AERORA_TELEMETRY_DIR}/AeroraFreshness.cc
    ${AERORA_TELEMETRY_DIR}/AeroraFreshness.h
    ${AERORA_TELEMETRY_DIR}/AeroraStateValue.cc
    ${AERORA_TELEMETRY_DIR}/AeroraStateValue.h
    ${AERORA_TELEMETRY_DIR}/AeroraTelemetrySet.cc
    ${AERORA_TELEMETRY_DIR}/AeroraTelemetrySet.h
    ${AERORA_TELEMETRY_DIR}/AeroraTelemetrySource.cc
    ${AERORA_TELEMETRY_DIR}/AeroraTelemetrySource.h
    ${AERORA_TELEMETRY_DIR}/AeroraTelemetryState.cc
    ${AERORA_TELEMETRY_DIR}/AeroraTelemetryState.h
    ${AERORA_TELEMETRY_DIR}/AeroraVehicleFactGroupRegistrar.cc
    ${AERORA_TELEMETRY_DIR}/AeroraVehicleFactGroupRegistrar.h
)

set(AERORA_TELEMETRY_INCLUDE_DIRECTORIES ${AERORA_TELEMETRY_DIR})

# QGC::Timing carries RuntimeScheduler.h, QtRuntimeScheduler.h and ScheduledTask.h as a PUBLIC
# header file set, and links QGC::Clock PUBLIC for MonotonicClock.h. Listing it makes this module's
# clock dependency explicit rather than relying on the application target picking it up transitively
# from src/Vehicle.
set(AERORA_TELEMETRY_LIBRARIES QGC::Timing)

# ----------------------------------------------------------------------------
# Test sources
#
# Reuses the upstream deterministic scheduler (test/Utilities/Timing/Support/ManualScheduler.h)
# rather than introducing a second fake clock.
# ----------------------------------------------------------------------------

set(AERORA_TELEMETRY_TEST_DIR ${AERORA_TELEMETRY_DIR}/../../test/Telemetry)
cmake_path(NORMAL_PATH AERORA_TELEMETRY_TEST_DIR)

set(AERORA_TELEMETRY_TEST_SOURCES
    ${AERORA_TELEMETRY_TEST_DIR}/AeroraTelemetryAttachmentTest.cc
    ${AERORA_TELEMETRY_TEST_DIR}/AeroraTelemetryAttachmentTest.h
    ${AERORA_TELEMETRY_TEST_DIR}/AeroraTelemetryStateTest.cc
    ${AERORA_TELEMETRY_TEST_DIR}/AeroraTelemetryStateTest.h
)

set(AERORA_TELEMETRY_TEST_INCLUDE_DIRECTORIES
    ${AERORA_TELEMETRY_TEST_DIR}
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/test/UnitTestFramework
    ${CMAKE_SOURCE_DIR}/test/Utilities/Timing/Support
)

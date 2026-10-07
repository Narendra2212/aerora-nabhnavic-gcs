# ============================================================================
# Aerora Vehicle Integration (M-Vehicle) - source manifest
#
# Variables only. This file defines no targets, calls no add_test(), and sets nothing in
# PARENT_SCOPE, so including it has no side effects beyond defining the variables below.
#
# Matches the manifest convention established by aerora/src/Telemetry/Telemetry.cmake: the
# aggregation into flat CUSTOM_* variables belongs to aerora/CMakeLists.txt (M-Plugin), so this
# file keeps the vehicle file list with the vehicle module and makes integrating it a small,
# reviewable change there rather than an edit by this workstream.
#
# ----------------------------------------------------------------------------
# Integration, in aerora/CMakeLists.txt
# ----------------------------------------------------------------------------
#
#   include(${CMAKE_CURRENT_SOURCE_DIR}/src/Vehicle/Vehicle.cmake)
#
#   list(APPEND CUSTOM_SOURCES ${AERORA_VEHICLE_SOURCES})
#   list(APPEND CUSTOM_INCLUDE_DIRECTORIES ${AERORA_VEHICLE_INCLUDE_DIRECTORIES})
#
#   if(QGC_BUILD_TESTING)
#       list(APPEND CUSTOM_SOURCES ${AERORA_VEHICLE_TEST_SOURCES})
#       list(APPEND CUSTOM_INCLUDE_DIRECTORIES ${AERORA_VEHICLE_TEST_INCLUDE_DIRECTORIES})
#       include(QGCTest)
#       add_qgc_test(AeroraVehicleIntegrationTest LABELS Unit)
#       add_qgc_test(AeroraVehicleLifecycleTest LABELS Integration Vehicle)
#   endif()
#
# The list(APPEND ...) calls must come before the existing set(CUSTOM_SOURCES ... PARENT_SCOPE),
# or the PARENT_SCOPE set must be repeated afterwards - a plain list(APPEND) does not propagate to
# the parent scope on its own.
#
# This module is NOT standalone. It must be aggregated together with the modules listed in
# AERORA_VEHICLE_REQUIRED_MODULES, and the telemetry module's AERORA_TELEMETRY_LIBRARIES must be
# linked. See "Cross-module dependency" below for why and for what happens if they are not.
#
# ----------------------------------------------------------------------------
# AERORA_VEHICLE_SOURCES must reach the application target, not a static library
# ----------------------------------------------------------------------------
#
# AeroraFirmwarePluginFactory registers itself through a non-local static object, and a linker is
# free to discard one that no symbol references. The failure is silent: the build succeeds, the
# application starts, the Aerora plugin is never registered, and the operator sees a generic
# vehicle. CUSTOM_SOURCES is compiled directly into the application target by src/CMakeLists.txt,
# which is why this manifest exports sources rather than building a library of its own.
#
# ----------------------------------------------------------------------------
# One note on include(QGCTest) in the snippet above
# ----------------------------------------------------------------------------
#
# QGCTest.cmake carries an include_guard(GLOBAL) and defines the check / check-unit / ... custom
# targets. The overlay is configured before test/ (root CMakeLists.txt adds the custom directory
# at :403 and test/ at :445), so including it from the overlay moves those convenience targets out
# of test/ and into the overlay directory. They are global by name, so `ninja check` still works.
#
# The alternative, if that side effect is unwanted, is to defer the registration instead:
#
#   cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}"
#                  CALL add_qgc_test AeroraVehicleIntegrationTest LABELS Unit)
#
# which runs after test/ has been processed and needs no include. Either works; the choice belongs
# to M-Plugin, and consistency with the telemetry manifest is the better reason to pick one.
#
# Also note that test/CMakeLists.txt escalates the sanitizer timeouts after the overlay has been
# configured, so a test registered from the overlay does not pick that escalation up. Pass an
# explicit TIMEOUT to add_qgc_test if a sanitizer lane needs one.
# ============================================================================

set(AERORA_VEHICLE_DIR ${CMAKE_CURRENT_LIST_DIR})

# ----------------------------------------------------------------------------
# Production sources
#
# No Aerora FactGroup appears here, and none is registered through
# FirmwarePlugin::factGroups(). That seam hands every vehicle of a firmware class the same
# FactGroup objects, because the plugin is a shared singleton - Q4-002. The decision is the
# orchestrator's; see README.md.
# ----------------------------------------------------------------------------

set(AERORA_VEHICLE_SOURCES
    ${AERORA_VEHICLE_DIR}/AeroraFirmwareClaim.h
    ${AERORA_VEHICLE_DIR}/AeroraFirmwarePlugin.cc
    ${AERORA_VEHICLE_DIR}/AeroraFirmwarePlugin.h
    ${AERORA_VEHICLE_DIR}/AeroraFirmwarePluginFactory.cc
    ${AERORA_VEHICLE_DIR}/AeroraFirmwarePluginFactory.h
    ${AERORA_VEHICLE_DIR}/AeroraProductProfile.cc
    ${AERORA_VEHICLE_DIR}/AeroraProductProfile.h
    ${AERORA_VEHICLE_DIR}/AeroraVehicleCapabilities.cc
    ${AERORA_VEHICLE_DIR}/AeroraVehicleCapabilities.h
    ${AERORA_VEHICLE_DIR}/AeroraVehicleIdentity.cc
    ${AERORA_VEHICLE_DIR}/AeroraVehicleIdentity.h
    ${AERORA_VEHICLE_DIR}/AeroraVehicleLog.cc
    ${AERORA_VEHICLE_DIR}/AeroraVehicleLog.h
    ${AERORA_VEHICLE_DIR}/AeroraVehicleServices.cc
    ${AERORA_VEHICLE_DIR}/AeroraVehicleServices.h
)

# ----------------------------------------------------------------------------
# Cross-module dependency
#
# AeroraFirmwarePlugin::initializeVehicle() is the per-vehicle composition root, so this module now
# references four sibling modules by header and by symbol:
#
#   M-Telemetry      AeroraTelemetrySet, AeroraFreshness
#   M-Payload        AeroraPayloadModel
#   M-Power          AeroraPowerModel
#   M-Mission        AeroraMissionBridge
#
# Consequence for the aggregation point: AERORA_VEHICLE_SOURCES no longer links standalone. The
# overlay must aggregate those four modules' sources as well, and must link the telemetry module's
# AERORA_TELEMETRY_LIBRARIES (QGC::Timing), which carries RuntimeScheduler.h for the freshness
# evaluator. Composing a vehicle out of modules that are not in the build is a link error, which is
# the correct failure: silently attaching three of four services would be worse.
#
# The include directories below are listed so this module's translation units compile wherever the
# aggregation puts them, rather than depending on another module's manifest being included first.
# They add -I paths only; they declare no source and build no target.
# ----------------------------------------------------------------------------

set(AERORA_VEHICLE_SRC_ROOT ${AERORA_VEHICLE_DIR}/..)
cmake_path(NORMAL_PATH AERORA_VEHICLE_SRC_ROOT)

set(AERORA_VEHICLE_INCLUDE_DIRECTORIES
    ${AERORA_VEHICLE_DIR}
    ${AERORA_VEHICLE_SRC_ROOT}/Telemetry
    ${AERORA_VEHICLE_SRC_ROOT}/Payload
    ${AERORA_VEHICLE_SRC_ROOT}/Power
    ${AERORA_VEHICLE_SRC_ROOT}/Mission
    ${AERORA_VEHICLE_SRC_ROOT}/Supervisory
)

# The modules this one must be aggregated with. Declared so the aggregation point can assert the
# set rather than discover it from a link error.
set(AERORA_VEHICLE_REQUIRED_MODULES Telemetry Payload Power Mission Supervisory)

# ----------------------------------------------------------------------------
# Mock and test sources
#
# AeroraVehicleMock is MOCK / TEST ONLY. It lives in the test tree and is listed only here, and
# its header also fails the build with #error if it is ever compiled without QGC_UNITTEST_BUILD,
# so both guards have to be defeated together for it to reach a shipped binary.
#
# Other workstreams that want the mock need AERORA_VEHICLE_TEST_INCLUDE_DIRECTORIES on the target;
# the mock needs no fixture, no link and no Vehicle.
# ----------------------------------------------------------------------------

set(AERORA_VEHICLE_TEST_DIR ${AERORA_VEHICLE_DIR}/../../test/Vehicle)
cmake_path(NORMAL_PATH AERORA_VEHICLE_TEST_DIR)

set(AERORA_VEHICLE_TEST_SOURCES
    ${AERORA_VEHICLE_TEST_DIR}/AeroraVehicleMock.cc
    ${AERORA_VEHICLE_TEST_DIR}/AeroraVehicleMock.h
    ${AERORA_VEHICLE_TEST_DIR}/AeroraVehicleIntegrationTest.cc
    ${AERORA_VEHICLE_TEST_DIR}/AeroraVehicleIntegrationTest.h
    ${AERORA_VEHICLE_TEST_DIR}/AeroraVehicleLifecycleTest.cc
    ${AERORA_VEHICLE_TEST_DIR}/AeroraVehicleLifecycleTest.h
)

set(AERORA_VEHICLE_TEST_INCLUDE_DIRECTORIES
    ${AERORA_VEHICLE_TEST_DIR}
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/test/UnitTestFramework
)

# CTest names, so the aggregation point can register them without reading this file.
set(AERORA_VEHICLE_TEST_NAMES AeroraVehicleIntegrationTest AeroraVehicleLifecycleTest)

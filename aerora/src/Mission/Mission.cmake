# ============================================================================
# Aerora Mission module (M-Mission) - source manifest
#
# Variables only. This file defines no targets, calls no add_test(), and sets nothing in
# PARENT_SCOPE, so including it has no side effects beyond defining the variables below.
#
# Follows the manifest convention established by aerora/src/Telemetry/Telemetry.cmake and
# aerora/src/Vehicle/Vehicle.cmake: the aggregation into flat CUSTOM_* variables belongs to
# aerora/CMakeLists.txt (M-Plugin), so this file keeps the mission file list with the mission
# module and makes integrating it a small, reviewable change there rather than an edit by this
# workstream.
#
# ----------------------------------------------------------------------------
# Integration, in aerora/CMakeLists.txt
# ----------------------------------------------------------------------------
#
#   include(${CMAKE_CURRENT_SOURCE_DIR}/src/Vehicle/Vehicle.cmake)   # see the dependency below
#   include(${CMAKE_CURRENT_SOURCE_DIR}/src/Mission/Mission.cmake)
#
#   list(APPEND CUSTOM_SOURCES ${AERORA_MISSION_SOURCES})
#   list(APPEND CUSTOM_INCLUDE_DIRECTORIES ${AERORA_MISSION_INCLUDE_DIRECTORIES})
#
#   if(QGC_BUILD_TESTING)
#       list(APPEND CUSTOM_SOURCES ${AERORA_MISSION_TEST_SOURCES})
#       list(APPEND CUSTOM_INCLUDE_DIRECTORIES ${AERORA_MISSION_TEST_INCLUDE_DIRECTORIES})
#       include(QGCTest)
#       add_qgc_test(AeroraMissionTest LABELS Unit MissionManager)
#       add_qgc_test(AeroraMissionSyncTest LABELS Integration Vehicle MissionManager)
#   endif()
#
# The list(APPEND ...) calls must come before the existing set(CUSTOM_SOURCES ... PARENT_SCOPE),
# or the PARENT_SCOPE set must be repeated afterwards - a plain list(APPEND) does not propagate to
# the parent scope on its own.
#
# ----------------------------------------------------------------------------
# HARD DEPENDENCY on the Aerora vehicle module
# ----------------------------------------------------------------------------
#
# AeroraMissionBridge.cc includes "AeroraVehicleServices.h", because the per-vehicle attachment
# seam belongs to the vehicle integration layer and this module consumes it rather than inventing
# a second one. Two consequences for the aggregation point:
#
#   * AERORA_VEHICLE_INCLUDE_DIRECTORIES must be on the target. This manifest deliberately does
#     not add it to AERORA_MISSION_INCLUDE_DIRECTORIES: re-exporting another module's include
#     directory would hide the dependency and let the vehicle module be dropped without a build
#     failure pointing at the real cause.
#   * Mission observation only attaches to a vehicle that already has AeroraVehicleServices, which
#     the Aerora firmware plugin attaches from FirmwarePlugin::initializeVehicle(). On a vehicle
#     that resolved to a different firmware plugin, AeroraMissionBridge::attachTo() declines and
#     logs. That is intended - creating the services object from here would take a lifecycle
#     decision owned by the vehicle workstream - but it means Mission without Vehicle is not a
#     supported configuration.
#
# If the aggregation point chooses to wire Mission without Vehicle, the build fails on the missing
# header rather than silently producing a mission module that never attaches.
#
# ----------------------------------------------------------------------------
# Nothing here registers itself
# ----------------------------------------------------------------------------
#
# Unlike AERORA_VEHICLE_SOURCES, this module contains no self-registering non-local static, so it
# has no linker-discard hazard of its own. AeroraMissionExtensionRegistry uses explicit
# registration precisely to avoid one, and nothing currently registers an extension because no
# Aerora complex item or mission command set is evidenced (Q-021, Q-023).
#
# The one call the application must make is AeroraMissionBridge::attachTo(vehicle), which has no
# caller yet. Reporting that integration point rather than wiring it is deliberate: every seam that
# could invoke it (the Aerora firmware plugin's initializeVehicle, the Aerora core plugin, or
# MultiVehicleManager::vehicleAdded) lives in a file owned by another workstream. See README.md.
#
# ----------------------------------------------------------------------------
# One note on include(QGCTest) in the snippet above
# ----------------------------------------------------------------------------
#
# QGCTest.cmake carries an include_guard(GLOBAL) and defines the check / check-unit / ... custom
# targets. The overlay is configured before test/, so including it from the overlay moves those
# convenience targets out of test/ and into the overlay directory. They are global by name, so
# `ninja check` still works. The alternative is to defer registration instead:
#
#   cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}"
#                  CALL add_qgc_test AeroraMissionTest LABELS Unit MissionManager)
#
# Either works; the choice belongs to M-Plugin, and consistency with the telemetry and vehicle
# manifests is the better reason to pick one.
#
# Also note that test/CMakeLists.txt escalates the sanitizer timeouts after the overlay has been
# configured, so a test registered from the overlay does not pick that escalation up. Pass an
# explicit TIMEOUT to add_qgc_test if a sanitizer lane needs one.
# ============================================================================

set(AERORA_MISSION_DIR ${CMAKE_CURRENT_LIST_DIR})

# ----------------------------------------------------------------------------
# Production sources
#
# AeroraMissionBridge is the only file here that includes Vehicle.h or MissionManager.h. Everything
# else is free of QGroundControl mission dependencies, which is what lets the observer, the
# snapshot comparison and the resume boundary be unit-tested without a vehicle, a link or an event
# loop.
#
# No QGC core file is modified by this module, and no mission command identifier is defined by it.
# ----------------------------------------------------------------------------

set(AERORA_MISSION_SOURCES
    ${AERORA_MISSION_DIR}/AeroraMissionBridge.cc
    ${AERORA_MISSION_DIR}/AeroraMissionBridge.h
    ${AERORA_MISSION_DIR}/AeroraMissionExtension.cc
    ${AERORA_MISSION_DIR}/AeroraMissionExtension.h
    ${AERORA_MISSION_DIR}/AeroraMissionIndex.cc
    ${AERORA_MISSION_DIR}/AeroraMissionIndex.h
    ${AERORA_MISSION_DIR}/AeroraMissionLog.cc
    ${AERORA_MISSION_DIR}/AeroraMissionLog.h
    ${AERORA_MISSION_DIR}/AeroraMissionObserver.cc
    ${AERORA_MISSION_DIR}/AeroraMissionObserver.h
    ${AERORA_MISSION_DIR}/AeroraMissionResumeBoundary.cc
    ${AERORA_MISSION_DIR}/AeroraMissionResumeBoundary.h
    ${AERORA_MISSION_DIR}/AeroraMissionSnapshot.cc
    ${AERORA_MISSION_DIR}/AeroraMissionSnapshot.h
)

set(AERORA_MISSION_INCLUDE_DIRECTORIES ${AERORA_MISSION_DIR})

# No extra library. The mission module uses only QtCore, QtQml integration macros, and the
# QGroundControl headers the application target already carries.
set(AERORA_MISSION_LIBRARIES "")

# ----------------------------------------------------------------------------
# Mock and test sources
#
# AeroraMissionMock is MOCK / TEST ONLY. It lives in the test tree and is listed only here, and its
# header also fails the build with #error if it is ever compiled without QGC_UNITTEST_BUILD, so
# both guards have to be defeated together for it to reach a shipped binary.
#
# Other workstreams that want the mock need AERORA_MISSION_TEST_INCLUDE_DIRECTORIES on the target;
# the mock needs no fixture, no link, no Vehicle and no MissionManager.
# ----------------------------------------------------------------------------

set(AERORA_MISSION_TEST_DIR ${AERORA_MISSION_DIR}/../../test/Mission)
cmake_path(NORMAL_PATH AERORA_MISSION_TEST_DIR)

set(AERORA_MISSION_TEST_SOURCES
    ${AERORA_MISSION_TEST_DIR}/AeroraMissionMock.cc
    ${AERORA_MISSION_TEST_DIR}/AeroraMissionMock.h
    ${AERORA_MISSION_TEST_DIR}/AeroraMissionSyncTest.cc
    ${AERORA_MISSION_TEST_DIR}/AeroraMissionSyncTest.h
    ${AERORA_MISSION_TEST_DIR}/AeroraMissionTest.cc
    ${AERORA_MISSION_TEST_DIR}/AeroraMissionTest.h
)

set(AERORA_MISSION_TEST_INCLUDE_DIRECTORIES
    ${AERORA_MISSION_TEST_DIR}
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/test/UnitTestFramework
)

# CTest names, so the aggregation point can register them without reading this file.
set(AERORA_MISSION_TEST_NAMES AeroraMissionTest AeroraMissionSyncTest)

# ============================================================================
# Aerora Persistence module (M-Store, WP1-C) - source manifest
#
# Variables only. This file defines no targets, calls no add_test(), and sets nothing in
# PARENT_SCOPE, so including it has no side effects beyond defining the variables below. It follows
# the manifest convention established by aerora/src/Telemetry/Telemetry.cmake, so integrating the
# module is a small reviewable edit in the aggregation file rather than an edit made here.
# aerora/CMakeLists.txt belongs to M-Plugin and is not modified by this manifest.
#
# ----------------------------------------------------------------------------
# Integration, in aerora/CMakeLists.txt
# ----------------------------------------------------------------------------
#
#   include(${CMAKE_CURRENT_SOURCE_DIR}/src/Persistence/Persistence.cmake)
#
#   list(APPEND CUSTOM_SOURCES ${AERORA_PERSISTENCE_SOURCES})
#   list(APPEND CUSTOM_INCLUDE_DIRECTORIES ${AERORA_PERSISTENCE_INCLUDE_DIRECTORIES})
#
#   if(QGC_BUILD_TESTING)
#       list(APPEND CUSTOM_SOURCES ${AERORA_PERSISTENCE_TEST_SOURCES})
#       list(APPEND CUSTOM_INCLUDE_DIRECTORIES ${AERORA_PERSISTENCE_TEST_INCLUDE_DIRECTORIES})
#       include(QGCTest)
#       add_qgc_test(AeroraLocalStoreTest LABELS Unit)
#   endif()
#
# No addition to CUSTOM_QT_COMPONENTS is required. AeroraLocalStore uses Qt Sql, and the application
# target already links it: Sql is in the root Qt component list and src/Utilities/Database (QGCSqlHelper)
# links Qt6::Sql, which this module reuses rather than reimplementing. Overlay sources compile into
# the application target, so they reach QGCSqlHelper and Qt Sql without a components change - the
# same reasoning aerora/src/Security/Security.cmake records for Qt Qml.
# ============================================================================

set(AERORA_PERSISTENCE_DIR ${CMAKE_CURRENT_LIST_DIR})

# ----------------------------------------------------------------------------
# Production sources
#
# AeroraLocalStore owns the database file, the six-table schema and its versioning, and failure
# accounting (design §13). It declares no enumerator and one constant (kSchemaVersion); the register
# records that constant under contractSurface so the gate check accounts for it.
# ----------------------------------------------------------------------------

set(AERORA_PERSISTENCE_SOURCES ${AERORA_PERSISTENCE_DIR}/AeroraLocalStore.cc
                               ${AERORA_PERSISTENCE_DIR}/AeroraLocalStore.h
)

set(AERORA_PERSISTENCE_INCLUDE_DIRECTORIES ${AERORA_PERSISTENCE_DIR})

# ----------------------------------------------------------------------------
# Test sources
# ----------------------------------------------------------------------------

set(AERORA_PERSISTENCE_TEST_DIR ${AERORA_PERSISTENCE_DIR}/../../test/Persistence)
cmake_path(NORMAL_PATH AERORA_PERSISTENCE_TEST_DIR)

set(AERORA_PERSISTENCE_TEST_SOURCES ${AERORA_PERSISTENCE_TEST_DIR}/AeroraLocalStoreTest.cc
                                    ${AERORA_PERSISTENCE_TEST_DIR}/AeroraLocalStoreTest.h
)

set(AERORA_PERSISTENCE_TEST_INCLUDE_DIRECTORIES ${AERORA_PERSISTENCE_TEST_DIR} ${CMAKE_SOURCE_DIR}/test
                                                ${CMAKE_SOURCE_DIR}/test/UnitTestFramework
)

# ----------------------------------------------------------------------------
# CTest names
#
# Declared here so .kiro/tools/aerora-manifest-check.ps1 can cross-check them against the
# add_qgc_test() calls in aerora/CMakeLists.txt. Per the Phase 7 convention a module exports its
# test names and Tab 1 adds the registration line, so this list is the request and the aggregation
# file is the authority.
# ----------------------------------------------------------------------------

set(AERORA_PERSISTENCE_TEST_NAMES AeroraLocalStoreTest)

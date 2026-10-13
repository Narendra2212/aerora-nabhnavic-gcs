# ============================================================================
# Aerora Security module (M-Security, M-Authz) - source manifest
#
# Variables only. This file defines no targets, calls no add_test(), and sets nothing in
# PARENT_SCOPE, so including it has no side effects beyond defining the variables below.
#
# Follows the manifest convention established by aerora/src/Telemetry/Telemetry.cmake, so that
# integrating a module is a small reviewable edit in the aggregation file rather than an edit made
# by this workstream. aerora/CMakeLists.txt belongs to M-Plugin and is not modified here.
#
# ----------------------------------------------------------------------------
# Integration, in aerora/CMakeLists.txt
# ----------------------------------------------------------------------------
#
#   include(${CMAKE_CURRENT_SOURCE_DIR}/src/Security/Security.cmake)
#
#   list(APPEND CUSTOM_SOURCES ${AERORA_SECURITY_SOURCES})
#   list(APPEND CUSTOM_INCLUDE_DIRECTORIES ${AERORA_SECURITY_INCLUDE_DIRECTORIES})
#
#   if(QGC_BUILD_TESTING)
#       list(APPEND CUSTOM_SOURCES ${AERORA_SECURITY_TEST_SOURCES})
#       list(APPEND CUSTOM_INCLUDE_DIRECTORIES ${AERORA_SECURITY_TEST_INCLUDE_DIRECTORIES})
#       include(QGCTest)
#       add_qgc_test(AeroraAuditSinkTest LABELS Unit)
#       add_qgc_test(AeroraCalibrationAuthorizationTest LABELS Unit)
#       add_qgc_test(AeroraSecurityStatusTest LABELS Unit)
#       add_qgc_test(AeroraSigningKeyStoreTest LABELS Unit)
#       add_qgc_test(AeroraCalibrationEnforcementTest LABELS Integration Vehicle)
#   endif()
#
# Three of the five are registered in aerora/CMakeLists.txt today. AeroraAuditSinkTest and
# AeroraSecurityStatusTest are new with WP2-A/WP2-B and are pending HO-TAB1-01: both compile and
# self-register via UT_REGISTER_TEST, so they are reachable with --unittest:<name>, but CTest will
# not run them until Tab 1 adds the two lines.
#
# IMPORTANT - this manifest SUPERSEDES the seven ${CMAKE_CURRENT_SOURCE_DIR}/src/Security/... lines
# currently written inline in the existing set(CUSTOM_SOURCES ...) block, and the
# ${CMAKE_CURRENT_SOURCE_DIR}/src/Security entry in CUSTOM_INCLUDE_DIRECTORIES. Remove those when
# adopting it, or the Security sources will be listed twice.
#
# The src/AeroraCorePlugin.{cc,h} entries are NOT part of this manifest: the core plugin is
# M-Plugin's, not this workstream's, and stays in the aggregation file.
#
# As with the telemetry manifest, no core edit is needed for the test: overlay test sources compile
# into the same application target, which already links Qt6::Test and carries the test include
# directories when QGC_BUILD_TESTING is on. UT_REGISTER_TEST self-registers at static
# initialisation, and add_qgc_test() only adds a CTest entry that runs the application with
# --unittest:<name>.
# ============================================================================

set(AERORA_SECURITY_DIR ${CMAKE_CURRENT_LIST_DIR})

# ----------------------------------------------------------------------------
# Production sources
#
# No authorization policy appears here, and no role or permission name appears in any of these
# files. Q-029 is OPEN; the policy interface is injected and no implementation is shipped.
#
# No platform key-protection backend appears here either. D-002 approved the abstraction;
# AeroraPlatformKeyProtector::createForCurrentPlatform() returns nullptr on every platform while
# D-006 leaves the host unable to compile or test one.
# ----------------------------------------------------------------------------

set(AERORA_SECURITY_SOURCES
    ${AERORA_SECURITY_DIR}/AeroraAuditSink.cc
    ${AERORA_SECURITY_DIR}/AeroraAuditSink.h
    ${AERORA_SECURITY_DIR}/AeroraAuthorization.h
    ${AERORA_SECURITY_DIR}/AeroraCalibrationAuthorization.cc
    ${AERORA_SECURITY_DIR}/AeroraCalibrationAuthorization.h
    ${AERORA_SECURITY_DIR}/AeroraCommandAuthorizer.cc
    ${AERORA_SECURITY_DIR}/AeroraCommandAuthorizer.h
    ${AERORA_SECURITY_DIR}/AeroraPlatformKeyProtector.h
    ${AERORA_SECURITY_DIR}/AeroraSecurityStatus.cc
    ${AERORA_SECURITY_DIR}/AeroraSecurityStatus.h
    ${AERORA_SECURITY_DIR}/AeroraSigningKeyStore.cc
    ${AERORA_SECURITY_DIR}/AeroraSigningKeyStore.h
)

# ----------------------------------------------------------------------------
# Qt components
#
# AeroraSecurityStatus is the one type in this module that carries Q_OBJECT and QML macros, so it
# includes <QtQmlIntegration/QtQmlIntegration> and needs moc. No addition to CUSTOM_QT_COMPONENTS
# is required and none is requested: the application target already links Qt6::Qml,
# Qt6::QmlIntegration and Qt6::Quick (src/CMakeLists.txt:115-117), and overlay sources compile into
# that target rather than into a library of their own. Adding Qml to CUSTOM_QT_COMPONENTS would
# make the dependency explicit but is a Tab 1 edit to aerora/CMakeLists.txt, and the build does not
# need it. Same reasoning as aerora/src/UI/Ui.cmake records for AeroraQmlOverrides.
#
# AeroraSecurityStatus is a deliberate, single exception to "no security type is QML-visible"
# (P7-SEC-004): it is a read-only projection that declares no data member and reaches no security
# object. 04-subsystem-and-interface-completion-matrix.md specifies exactly this - the security
# screens need "a deliberate minimal accessor (D-UI-01) rather than a type exposure". Every other
# type in this module stays free of Q_OBJECT, Q_GADGET and QML macros.
# ----------------------------------------------------------------------------

set(AERORA_SECURITY_INCLUDE_DIRECTORIES ${AERORA_SECURITY_DIR})

# ----------------------------------------------------------------------------
# Test sources
# ----------------------------------------------------------------------------

set(AERORA_SECURITY_TEST_DIR ${AERORA_SECURITY_DIR}/../../test/Security)
cmake_path(NORMAL_PATH AERORA_SECURITY_TEST_DIR)

set(AERORA_SECURITY_TEST_SOURCES
    ${AERORA_SECURITY_TEST_DIR}/AeroraAuditSinkTest.cc
    ${AERORA_SECURITY_TEST_DIR}/AeroraAuditSinkTest.h
    ${AERORA_SECURITY_TEST_DIR}/AeroraCalibrationAuthorizationTest.cc
    ${AERORA_SECURITY_TEST_DIR}/AeroraCalibrationAuthorizationTest.h
    ${AERORA_SECURITY_TEST_DIR}/AeroraCalibrationEnforcementTest.cc
    ${AERORA_SECURITY_TEST_DIR}/AeroraCalibrationEnforcementTest.h
    ${AERORA_SECURITY_TEST_DIR}/AeroraSecurityStatusTest.cc
    ${AERORA_SECURITY_TEST_DIR}/AeroraSecurityStatusTest.h
    ${AERORA_SECURITY_TEST_DIR}/AeroraSecurityTestDoubles.h
    ${AERORA_SECURITY_TEST_DIR}/AeroraSigningKeyStoreTest.cc
    ${AERORA_SECURITY_TEST_DIR}/AeroraSigningKeyStoreTest.h
)

set(AERORA_SECURITY_TEST_INCLUDE_DIRECTORIES
    ${AERORA_SECURITY_TEST_DIR}
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/test/UnitTestFramework
)

# ----------------------------------------------------------------------------
# CTest names
#
# Declared here so .kiro/tools/aerora-manifest-check.ps1 can cross-check them against the
# add_qgc_test() calls in aerora/CMakeLists.txt. Per the Phase 7 convention a module exports its
# test names and Tab 1 adds the registration line, so this list is the request and the aggregation
# file is the authority.
#
# AeroraAuditSinkTest and AeroraSecurityStatusTest are NOT yet registered in aerora/CMakeLists.txt.
# The manifest check will report both as NOT REGISTERED until Tab 1 applies HO-TAB1-01, and that
# report is the intended signal rather than a defect in this manifest - a test that self-registers
# through UT_REGISTER_TEST but has no CTest entry compiles and is reachable with
# --unittest:<name>, while `ctest -L Unit` will not see it. See
# .kiro/specs/03-implementation-specification/34-audit-sink-and-security-status-contract.md.
# ----------------------------------------------------------------------------

set(AERORA_SECURITY_TEST_NAMES
    AeroraAuditSinkTest
    AeroraCalibrationAuthorizationTest
    AeroraCalibrationEnforcementTest
    AeroraSecurityStatusTest
    AeroraSigningKeyStoreTest
)

# ============================================================================
# Aerora operator interface module (M-Ui) - source manifest
#
# Variables only. This file defines no targets, calls no add_test(), and sets nothing in
# PARENT_SCOPE, so including it has no side effects beyond defining the variables below. The
# convention is Telemetry.cmake's and Vehicle.cmake's, adopted unchanged.
#
# ----------------------------------------------------------------------------
# Integration, in aerora/CMakeLists.txt  (M-Plugin / Tab 1 owns that file)
# ----------------------------------------------------------------------------
#
# Include LAST of the domain manifests. M-Ui consumes Telemetry, Vehicle, Mission and
# Payload/Power headers and deliberately does not re-export their include directories, so the
# dependency stays visible at the aggregation point rather than hidden in this file:
#
#   include(${CMAKE_CURRENT_SOURCE_DIR}/src/UI/Ui.cmake)
#
#   list(APPEND _aerora_sources ${AERORA_UI_SOURCES})
#   list(APPEND _aerora_include_directories ${AERORA_UI_INCLUDE_DIRECTORIES})
#
#   if(QGC_BUILD_TESTING)
#       list(APPEND _aerora_sources ${AERORA_UI_TEST_SOURCES})
#       list(APPEND _aerora_include_directories ${AERORA_UI_TEST_INCLUDE_DIRECTORIES})
#       add_qgc_test(AeroraScreenMetricsTest LABELS Unit)
#       add_qgc_test(AeroraSceneHarnessProbeTest LABELS Integration NoSanitizer SERIAL TIMEOUT 600)
#       add_qgc_test(AeroraUiFoundationTest LABELS Unit)
#       add_qgc_test(AeroraUiSafetyTest LABELS Unit)
#       add_qgc_test(AeroraMissionProgressUITest LABELS Integration Vehicle)
#       # Withheld on Windows: QmlUITestBase::startUI() aborts in the offscreen scene boot there.
#       # The reason, and the evidence it is the harness rather than this test, is recorded at the
#       # registration site in aerora/CMakeLists.txt.
#       if(NOT WIN32)
#           add_qgc_test(AeroraSceneAttachmentUITest LABELS Integration NoSanitizer TIMEOUT 180)
#           add_qgc_test(AeroraVehicleConfigUITest LABELS Integration Vehicle NoSanitizer TIMEOUT 300)
#           add_qgc_test(AeroraFlyViewOverlayUITest LABELS Integration Vehicle NoSanitizer TIMEOUT 300)
#       endif()
#   endif()
#
# ----------------------------------------------------------------------------
# The QML module is a target, so it cannot live in this manifest
# ----------------------------------------------------------------------------
#
# A QML module needs qt_add_library() + qt_add_qml_module(), both targets, which the manifest
# convention excludes. The block below belongs in aerora/CMakeLists.txt. It is written out in full
# so integrating it is a copy rather than a design decision:
#
#   qt_add_library(AeroraControlsModule STATIC)
#   qgc_configure_target(AeroraControlsModule)
#
#   qgc_set_qt_resource_alias(${AERORA_UI_QML_FILES})
#
#   qt_add_qml_module(AeroraControlsModule
#       URI ${AERORA_UI_QML_MODULE_URI}
#       VERSION 1.0
#       RESOURCE_PREFIX /qml
#       QML_FILES ${AERORA_UI_QML_FILES}
#       IMPORTS
#           QGC
#           QGroundControl
#           QGroundControl.Controls
#       NO_PLUGIN
#   )
#
#   list(APPEND _aerora_libraries AeroraControlsModule)
#
# Four notes, each checked against the build files rather than assumed:
#
#   * qgc_set_qt_resource_alias() (cmake/Helpers.cmake:12, available in a custom subdirectory
#     because the root CMakeLists.txt does include(Helpers) at :21) strips the directory prefix so
#     each file is aliased to its bare name. Without it the QML_FILES paths would nest under the
#     URI and the resource would land at qrc:/qml/Aerora/Controls/src/UI/Controls/<Name>.qml.
#     With it the module resolves at qrc:/qml/Aerora/Controls/<Name>.qml.
#
#   * IMPORTS QGC is the load-bearing line. Aerora's QML_ELEMENT C++ types - including the
#     AeroraTelemetryState singleton the controls read state classes from, and the AeroraUi
#     singleton - register into the application target's QML module, whose URI is QGC
#     (root CMakeLists.txt:422-428). This mirrors src/CMakeLists.txt:15-26, where
#     QGroundControlModule declares IMPORTS QGC for exactly the same reason.
#
#   * The library goes into CUSTOM_LIBRARIES; the C++ stays in CUSTOM_SOURCES. Inverting those
#     fails quietly in one direction - a self-registering static dropped from an archive - and
#     loudly in the other.
#
#   * No addImportPath() call is needed. The base QGCCorePlugin::createQmlApplicationEngine()
#     already adds qrc:/qml (src/API/QGCCorePlugin.cc:303), and a module at
#     qrc:/qml/Aerora/Controls/ resolves from that path. The reference overlay's extra
#     addImportPath("qrc:/qml/Custom/Widgets") calls are redundant under this prefix and are
#     deliberately not copied.
#
# ----------------------------------------------------------------------------
# The core plugin override is also outside this manifest
# ----------------------------------------------------------------------------
#
# AeroraQmlOverrides must be installed on the QML engine, which means overriding two public
# virtuals on AeroraCorePlugin - a file M-Plugin owns. The exact change is recorded in
# .kiro/specs/04-ui-implementation/01-ui-foundation-closure.md as handoff HO-TAB7-02. Until it is
# applied, no Aerora QML override takes effect; the Aerora QML *module* still loads, because a
# module import does not go through the interceptor.
#
# ----------------------------------------------------------------------------
# Qt components
# ----------------------------------------------------------------------------
#
# AeroraQmlOverrides.cc includes <QtQml/QQmlApplicationEngine> and
# <QtQml/QQmlAbstractUrlInterceptor>. No addition to CUSTOM_QT_COMPONENTS is required: the
# application target already links Qt6::Qml, Qt6::QmlIntegration and Qt6::Quick
# (src/CMakeLists.txt:115-117), and overlay sources compile into that target. Adding Qml to
# CUSTOM_QT_COMPONENTS would make the dependency explicit and is harmless, but it is a Tab 1 edit
# and is not required for the build to succeed.
# ============================================================================

set(AERORA_UI_DIR ${CMAKE_CURRENT_LIST_DIR})

# ----------------------------------------------------------------------------
# Production sources
#
# Three files, two seams. No panel and no product surface appears here: the backend has no Aerora
# value to populate one with (no dialect, every fact group CONTRACT-ONLY), so a panel would be
# shape with nothing behind it. The one indicator that does exist is QML reading a C++ singleton,
# so it is declared in AERORA_UI_QML_FILES below and needs no source here.
# ----------------------------------------------------------------------------

set(AERORA_UI_SOURCES
    ${AERORA_UI_DIR}/AeroraQmlOverrides.cc
    ${AERORA_UI_DIR}/AeroraQmlOverrides.h
    ${AERORA_UI_DIR}/AeroraUiLog.cc
    ${AERORA_UI_DIR}/AeroraUiLog.h
    ${AERORA_UI_DIR}/AeroraVehicleUiContext.cc
    ${AERORA_UI_DIR}/AeroraVehicleUiContext.h
)

set(AERORA_UI_INCLUDE_DIRECTORIES ${AERORA_UI_DIR})

# M-Ui links no library of its own. The QML module target the aggregation point creates is appended
# to CUSTOM_LIBRARIES there, not here, because this manifest declares no targets.
set(AERORA_UI_LIBRARIES "")

# ----------------------------------------------------------------------------
# QML module
#
# One module. Splitting it into fly view / plan / setup modules before any of those surfaces exists
# would be four empty namespaces instead of one populated one.
# ----------------------------------------------------------------------------

set(AERORA_UI_QML_MODULE_URI "Aerora.Controls")

set(AERORA_UI_QML_FILES
    ${AERORA_UI_DIR}/Controls/AeroraAgeLabel.qml
    ${AERORA_UI_DIR}/Controls/AeroraLinkStatePanel.qml
    ${AERORA_UI_DIR}/Controls/AeroraMissionProgressPanel.qml
    ${AERORA_UI_DIR}/Controls/AeroraOverlayValueRow.qml
    ${AERORA_UI_DIR}/Controls/AeroraSelfReportRow.qml
    ${AERORA_UI_DIR}/Controls/AeroraStateText.qml
    ${AERORA_UI_DIR}/Controls/AeroraTokenDisplay.qml
    ${AERORA_UI_DIR}/Controls/AeroraValueDisplay.qml
    ${AERORA_UI_DIR}/Indicators/AeroraEnforcementIndicator.qml
    ${AERORA_UI_DIR}/Screens/AeroraCapabilityPage.qml
    ${AERORA_UI_DIR}/Screens/AeroraErrorReportPage.qml
    ${AERORA_UI_DIR}/Screens/AeroraHealthPage.qml
    ${AERORA_UI_DIR}/Screens/AeroraIdentityPage.qml
    ${AERORA_UI_DIR}/Screens/AeroraMissionProgressPage.qml
    ${AERORA_UI_DIR}/Screens/AeroraOperationsHub.qml
    ${AERORA_UI_DIR}/Screens/AeroraSecurityPosturePage.qml
    ${AERORA_UI_DIR}/Screens/AeroraServiceDiagnosticsPage.qml
    ${AERORA_UI_DIR}/Screens/AeroraStateVocabularyPage.qml
)

# ----------------------------------------------------------------------------
# Source layout versus resource layout
#
# Controls/, Indicators/ and Screens/ are a source-tree distinction only.
# qgc_set_qt_resource_alias() aliases every entry above to its bare name, so all of them land flat
# at qrc:/qml/Aerora/Controls/<Name>.qml in one module, Aerora.Controls. Two consequences worth
# stating because neither is guessable from this list:
#
#   * a file here must not import Aerora.Controls - that would be the module importing itself.
#     Types in one QML module are mutually visible, which is how AeroraOperationsHub.qml reaches
#     AeroraSelfReportRow without an import.
#
#   * the URL to register a screen or an indicator with is qrc:/qml/Aerora/Controls/<Name>.qml
#     regardless of its source directory. For the hub that is
#     qrc:/qml/Aerora/Controls/AeroraOperationsHub.qml, which is the string HO-TAB7-05 asks Tab 1
#     to put in analyzePages(). For the enforcement strip it is
#     qrc:/qml/Aerora/Controls/AeroraEnforcementIndicator.qml, which is the string HO-TAB7-06
#     asks Tab 1 to put in toolBarIndicators() - note that seam takes bare QUrls, not
#     QmlComponentInfo objects.
#
# Splitting the module per surface was considered and declined: the URI is the module boundary and
# a second URI would need its own qt_add_qml_module target in Tab 1's file, for one screen.
# ----------------------------------------------------------------------------

# ----------------------------------------------------------------------------
# Override mirrors
#
# These are NOT part of the Aerora.Controls module and must not be added to AERORA_UI_QML_FILES.
# A module member is aliased to its bare name under RESOURCE_PREFIX /qml; an override mirror has to
# keep the stock nested path under the /Aerora prefix, because AeroraQmlOverrides derives the
# mirrored path mechanically:
#
#   mirroredResourcePath("/qml/QGroundControl/FlyView/FlyViewCustomLayer.qml")
#     == ":/Aerora/qml/QGroundControl/FlyView/FlyViewCustomLayer.qml"
#
# So the aggregation point sets each mirror's resource alias to its stock path explicitly, which is
# the shape custom-example/CMakeLists.txt:80-96 uses for its own /Custom/qml mirrors. Each mirror is
# therefore exported under its own variable as well as in the list: the list drives
# qt_add_resources(), the named variable is what the alias is set on.
#
# A mirror is inert until AeroraQmlOverrides::overrideEntries() declares its stock path. That is the
# whole point of the allow-list: Aerora keys interception on declaration, not on resource existence,
# so a mirror present in the build and absent from the list is never loaded.
#
# These files are outside the module directory, so unlike the Aerora screens they must
# `import Aerora.Controls` to reach the overlay's own controls.
# ----------------------------------------------------------------------------

set(AERORA_UI_OVERRIDE_PREFIX "/Aerora/qml")

# SCR-LIV-01. Stock path: /qml/QGroundControl/FlyView/FlyViewCustomLayer.qml
set(AERORA_UI_OVERRIDE_FLY_VIEW_CUSTOM_LAYER ${AERORA_UI_DIR}/Overrides/FlyViewCustomLayer.qml)

set(AERORA_UI_OVERRIDE_QML_FILES
    ${AERORA_UI_OVERRIDE_FLY_VIEW_CUSTOM_LAYER}
)

# ----------------------------------------------------------------------------
# Test sources
#
# Both suites are pure in-memory logic: no vehicle, no link, no QML engine, no clock, no file or
# network I/O. Neither needs ManualScheduler, because neither reads a clock - the one timing-shaped
# thing in the module, AeroraAgeLabel's refresh cadence, is QML and is covered by the Qt Quick Test
# target recorded as Q7-004 rather than here.
#
# The default 60s Unit timeout has a very large margin even instrumented, so no explicit TIMEOUT is
# requested.
# ----------------------------------------------------------------------------

set(AERORA_UI_TEST_DIR ${AERORA_UI_DIR}/../../test/UI)
cmake_path(NORMAL_PATH AERORA_UI_TEST_DIR)

set(AERORA_UI_TEST_SOURCES
    ${AERORA_UI_TEST_DIR}/AeroraFlyViewOverlayUITest.cc
    ${AERORA_UI_TEST_DIR}/AeroraFlyViewOverlayUITest.h
    ${AERORA_UI_TEST_DIR}/AeroraSceneBootOnlyTest.cc
    ${AERORA_UI_TEST_DIR}/AeroraSceneBootOnlyTest.h
    ${AERORA_UI_TEST_DIR}/AeroraSceneHarnessProbeTest.cc
    ${AERORA_UI_TEST_DIR}/AeroraSceneHarnessProbeTest.h
    ${AERORA_UI_TEST_DIR}/AeroraScreenMetricsTest.cc
    ${AERORA_UI_TEST_DIR}/AeroraScreenMetricsTest.h
    ${AERORA_UI_TEST_DIR}/AeroraMissionProgressUITest.cc
    ${AERORA_UI_TEST_DIR}/AeroraMissionProgressUITest.h
    ${AERORA_UI_TEST_DIR}/AeroraSceneAttachmentUITest.cc
    ${AERORA_UI_TEST_DIR}/AeroraSceneAttachmentUITest.h
    ${AERORA_UI_TEST_DIR}/AeroraUiFoundationTest.cc
    ${AERORA_UI_TEST_DIR}/AeroraUiFoundationTest.h
    ${AERORA_UI_TEST_DIR}/AeroraUiSafetyTest.cc
    ${AERORA_UI_TEST_DIR}/AeroraUiSafetyTest.h
    ${AERORA_UI_TEST_DIR}/AeroraVehicleConfigUITest.cc
    ${AERORA_UI_TEST_DIR}/AeroraVehicleConfigUITest.h
)

# test/QmlUITests carries QmlUITestBase, which AeroraSceneAttachmentUITest derives from to boot a
# real application scene, and VehicleConfigUITestBase, which AeroraVehicleConfigUITest derives from
# to drive the Configure view. Both .cc files are already compiled into the application target by
# test/QmlUITests/CMakeLists.txt, so only the header path is needed here - the overlay builds no
# second copy of the harness and modifies no core file to reach it.
set(AERORA_UI_TEST_INCLUDE_DIRECTORIES
    ${AERORA_UI_TEST_DIR}
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/test/UnitTestFramework
    ${CMAKE_SOURCE_DIR}/test/QmlUITests
)

set(AERORA_UI_TEST_NAMES
    AeroraFlyViewOverlayUITest
    AeroraMissionProgressUITest
    AeroraSceneBootOnlyTest
    AeroraSceneHarnessProbeTest
    AeroraScreenMetricsTest
    AeroraSceneAttachmentUITest
    AeroraUiFoundationTest
    AeroraUiSafetyTest
    AeroraVehicleConfigUITest
)

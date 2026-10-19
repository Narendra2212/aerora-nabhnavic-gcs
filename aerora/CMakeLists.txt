# ============================================================================
# Aerora Universal GCS - overlay build aggregation (M-Plugin / Tab 1)
#
# This file is the single aggregation point for the Aerora overlay. Domain modules export
# variables from their own <Module>.cmake manifests and this file flattens them into the
# CUSTOM_* variables that src/CMakeLists.txt consumes (verified at src/CMakeLists.txt:203-222:
# CUSTOM_QT_COMPONENTS, CUSTOM_SOURCES, CUSTOM_LIBRARIES, CUSTOM_INCLUDE_DIRECTORIES and
# CUSTOM_DEFINITIONS, each guarded by if(QGC_CUSTOM_BUILD)).
#
# Ownership: domain modules own their manifests; this file owns only the aggregation. Adding a
# module here must stay a small, reviewable change - see each manifest's integration comment.
#
# Integration convention, established independently by aerora/src/Telemetry/Telemetry.cmake and
# aerora/src/Vehicle/Vehicle.cmake and adopted unchanged:
#
#   - a manifest sets variables only: no targets, no add_test(), nothing in PARENT_SCOPE
#   - it exports AERORA_<MODULE>_SOURCES / _INCLUDE_DIRECTORIES / _LIBRARIES
#   - and AERORA_<MODULE>_TEST_SOURCES / _TEST_INCLUDE_DIRECTORIES / _TEST_NAMES
#   - this file includes it, appends to local lists, and performs one PARENT_SCOPE set at the end
#
# The local-list-then-single-set shape is deliberate: both manifests warn that a plain
# list(APPEND) does not propagate to the parent scope, so every append happens before the
# PARENT_SCOPE sets at the bottom of this file.
#
# Sources reach the application target directly rather than a static library. That is required,
# not incidental: AeroraFirmwarePluginFactory registers itself through a non-local static object,
# and a linker may discard such an object from a static archive if no symbol references it. The
# failure mode is silent - the build succeeds, the application starts, and no Aerora plugin is
# ever registered.
#
# All five domain manifests are aggregated. src/Supervisory/ is covered by PayloadPower.cmake: it
# is not a sixth module but the single type M-Payload and M-Power share, held in its own directory
# so neither depends on the other for it. Whether it is absorbed into M-Telemetry is recorded OPEN
# and assigned to Tab 1 - see 29-integration-status.md.
# ============================================================================

# ----------------------------------------------------------------------------
# Domain manifests
#
# Vehicle is included before Mission because the mission module has a hard dependency on it:
# AeroraMissionBridge.cc includes "AeroraVehicleServices.h". Mission.cmake deliberately does not
# re-export the vehicle include directory, so that dropping the vehicle module fails the build at
# the missing header rather than silently yielding a mission module that never attaches.
# ----------------------------------------------------------------------------

# PayloadPower is included after Telemetry and Vehicle because it consumes their headers
# (AeroraTelemetryState/StateValue/Source/Freshness and AeroraVehicleCapabilities) and deliberately
# does not re-export their include directories, so the aggregation order stays visible here.
include(${CMAKE_CURRENT_SOURCE_DIR}/src/Security/Security.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/Telemetry/Telemetry.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/Vehicle/Vehicle.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/Mission/Mission.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/PayloadPower.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/UI/Ui.cmake)

# ----------------------------------------------------------------------------
# Cross-module dependency assertion
#
# AeroraFirmwarePlugin::initializeVehicle() is the per-vehicle composition root and references four
# sibling modules by header and by symbol, so AERORA_VEHICLE_SOURCES no longer links standalone.
# Vehicle.cmake declares AERORA_VEHICLE_REQUIRED_MODULES precisely so this point can assert the set
# instead of discovering an omission from a link error.
#
# Module names do not map one-to-one onto manifest variable prefixes: Payload, Power and
# Supervisory are all carried by src/PayloadPower.cmake, because Payload and Power share
# AeroraReportedValue and splitting the manifest would duplicate it. The mapping is therefore
# explicit rather than derived from the name.
# ----------------------------------------------------------------------------

set(_aerora_module_sentinel_Telemetry   AERORA_TELEMETRY_SOURCES)
set(_aerora_module_sentinel_Mission     AERORA_MISSION_SOURCES)
set(_aerora_module_sentinel_Payload     AERORA_PAYLOAD_POWER_SOURCES)
set(_aerora_module_sentinel_Power       AERORA_PAYLOAD_POWER_SOURCES)
set(_aerora_module_sentinel_Supervisory AERORA_PAYLOAD_POWER_SOURCES)

foreach(_required_module IN LISTS AERORA_VEHICLE_REQUIRED_MODULES)
    set(_sentinel_name "${_aerora_module_sentinel_${_required_module}}")

    if(NOT _sentinel_name)
        message(FATAL_ERROR
            "Aerora overlay: M-Vehicle requires module '${_required_module}', but this aggregation "
            "point has no sentinel mapping for it. Add one above, or correct "
            "AERORA_VEHICLE_REQUIRED_MODULES in aerora/src/Vehicle/Vehicle.cmake.")
    endif()

    if(NOT ${_sentinel_name})
        message(FATAL_ERROR
            "Aerora overlay: M-Vehicle requires module '${_required_module}', whose manifest was "
            "not included or exported no sources (expected ${_sentinel_name} to be set). "
            "AeroraFirmwarePlugin::initializeVehicle() composes that module into every vehicle, so "
            "building without it would either fail to link or silently attach an incomplete set of "
            "per-vehicle services.")
    endif()
endforeach()

unset(_required_module)
unset(_sentinel_name)
unset(_aerora_module_sentinel_Telemetry)
unset(_aerora_module_sentinel_Mission)
unset(_aerora_module_sentinel_Payload)
unset(_aerora_module_sentinel_Power)
unset(_aerora_module_sentinel_Supervisory)

# ----------------------------------------------------------------------------
# Aerora plugin sources
#
# M-Plugin owns only AeroraCorePlugin.*, which binds the Aerora security foundation to QGC's
# extension points. Every domain module, security included, now exports its own manifest, so
# nothing else is listed inline here.
# ----------------------------------------------------------------------------

set(_aerora_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AeroraCorePlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AeroraCorePlugin.h
)

set(_aerora_include_directories
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(_aerora_libraries "")

# ----------------------------------------------------------------------------
# Aggregate the domain modules
# ----------------------------------------------------------------------------

list(APPEND _aerora_sources
    ${AERORA_SECURITY_SOURCES}
    ${AERORA_TELEMETRY_SOURCES}
    ${AERORA_VEHICLE_SOURCES}
    ${AERORA_MISSION_SOURCES}
    ${AERORA_PAYLOAD_POWER_SOURCES}
    ${AERORA_UI_SOURCES}
)

list(APPEND _aerora_include_directories
    ${AERORA_SECURITY_INCLUDE_DIRECTORIES}
    ${AERORA_TELEMETRY_INCLUDE_DIRECTORIES}
    ${AERORA_VEHICLE_INCLUDE_DIRECTORIES}
    ${AERORA_MISSION_INCLUDE_DIRECTORIES}
    ${AERORA_PAYLOAD_POWER_INCLUDE_DIRECTORIES}
    ${AERORA_UI_INCLUDE_DIRECTORIES}
)

# ----------------------------------------------------------------------------
# Aerora QML module (B2)
#
# M-Ui exports the URI and the file list; creating the target belongs here because a manifest
# declares no targets. Follows the custom-example pattern verbatim
# (custom-example/CMakeLists.txt:143-172): qt_add_library STATIC, qgc_configure_target, then
# qt_add_qml_module with NO_PLUGIN, and the resulting target appended to CUSTOM_LIBRARIES.
#
# Two lines in the block below are load-bearing and were missing from the first pass. Both are
# specified in aerora/src/UI/Ui.cmake, which is Tab 7's manifest and the authority on this module:
#
#   * qgc_set_qt_resource_alias() (cmake/Helpers.cmake:13, in scope because the root CMakeLists.txt
#     does include(Helpers) at :21, before it adds this directory at :403) aliases each file to its
#     bare name. AERORA_UI_QML_FILES holds absolute paths, so without the alias the resource lands
#     at qrc:/qml/Aerora/Controls/src/UI/Controls/<Name>.qml and the module does not resolve. It is
#     called from this file rather than the manifest because source file properties are
#     directory-scoped and must be set where the target is declared.
#
#     The first pass omitted it on the grounds that aliases exist to shadow stock QGroundControl
#     files by basename and Aerora.Controls shadows nothing. That is true of shadowing and
#     irrelevant here: the alias is doing path flattening, which this module needs either way.
#
#   * IMPORTS QGC is how Aerora's QML_ELEMENT C++ types resolve inside the module. They register
#     into the application target's QML module, whose URI is QGC (root CMakeLists.txt:422), so a
#     module that does not import it cannot see AeroraUi or AeroraTelemetryState. This mirrors
#     src/CMakeLists.txt:15-26, where QGroundControlModule does exactly the same thing.
#
#   * IMPORTS QGroundControl.AnalyzeView is what makes AnalyzePage resolvable. AeroraOperationsHub.qml
#     roots on AnalyzePage, which upstream declares in AnalyzeViewModule (src/AnalyzeView/CMakeLists.txt:19-24,
#     URI QGroundControl.AnalyzeView). Upstream's own analyze pages import nothing for it because they
#     are members of that same module (MAVLinkInspector/MAVLinkInspectorPage.qml is listed at :29) and
#     types within a module are mutually visible. The hub is a member of Aerora.Controls instead, so it
#     gets no such visibility and would fail to load with "AnalyzePage is not a type" - at runtime only,
#     with the resource present and the page registered. Declared here rather than as a file-level import
#     in the hub because this block is where this module's dependencies are stated, and because
#     aerora/CMakeLists.txt is M-Plugin's file while the hub is M-Ui's.
#
# No addImportPath() call is needed. The base QGCCorePlugin::createQmlApplicationEngine() already
# adds qrc:/qml (src/API/QGCCorePlugin.cc:303) and a module at qrc:/qml/Aerora/Controls/ resolves
# from it.
#
# The URI comes from AERORA_UI_QML_MODULE_URI - Tab 7 owns it and it is not restated here.
# ----------------------------------------------------------------------------

if(AERORA_UI_QML_FILES)
    qt_add_library(AeroraControlsModule STATIC)
    qgc_configure_target(AeroraControlsModule)

    qgc_set_qt_resource_alias(${AERORA_UI_QML_FILES})

    qt_add_qml_module(AeroraControlsModule
        URI ${AERORA_UI_QML_MODULE_URI}
        VERSION 1.0
        RESOURCE_PREFIX /qml
        QML_FILES ${AERORA_UI_QML_FILES}
        IMPORTS
            QGC
            QGroundControl
            QGroundControl.Controls
            QGroundControl.AnalyzeView
        NO_PLUGIN
    )

    list(APPEND _aerora_libraries AeroraControlsModule)
endif()

# ----------------------------------------------------------------------------
# QML override mirrors
#
# Registered on the application target rather than on AeroraControlsModule: these are documents the
# QML engine loads by URL through the AeroraQmlOverrides interceptor, not importable types, so they
# want a plain resource at a fixed path rather than module membership. See the "Override mirrors"
# section of src/UI/Ui.cmake for why the alias has to preserve the stock nested path.
#
# The alias list is index-matched with the file list, so a mismatched length is a configure error
# rather than a mirror that lands at the wrong path and silently never loads.
# ----------------------------------------------------------------------------

if(AERORA_UI_OVERRIDE_QML_FILES)
    # One explicit alias per mirror, as custom-example/CMakeLists.txt:80-96 does. The alias is the
    # stock path relative to the prefix, so the mirror lands exactly where
    # AeroraQmlOverrides::mirroredResourcePath() will look for it.
    set_source_files_properties(${AERORA_UI_OVERRIDE_FLY_VIEW_CUSTOM_LAYER}
        PROPERTIES QT_RESOURCE_ALIAS "QGroundControl/FlyView/FlyViewCustomLayer.qml")

    qt_add_resources(${CMAKE_PROJECT_NAME} aerora_qml_overrides
        PREFIX "${AERORA_UI_OVERRIDE_PREFIX}"
        FILES ${AERORA_UI_OVERRIDE_QML_FILES}
    )
endif()

list(APPEND _aerora_libraries
    ${AERORA_TELEMETRY_LIBRARIES}
    ${AERORA_MISSION_LIBRARIES}
    ${AERORA_PAYLOAD_POWER_LIBRARIES}
)

# ----------------------------------------------------------------------------
# Test sources and CTest registration
#
# Gated on QGC_BUILD_TESTING so no mock or test translation unit can reach a shipped binary
# through the overlay. AeroraVehicleMock additionally fails the build with #error unless
# QGC_UNITTEST_BUILD is defined, so both guards must be defeated together.
#
# include(QGCTest) is used rather than cmake_language(DEFER ...) for consistency with the
# snippet both manifests document. Two consequences, both known and accepted:
#   - QGCTest.cmake carries include_guard(GLOBAL) and defines the check / check-unit targets, so
#     including it here moves those convenience targets into this directory. They are global by
#     name, so `ninja check` still resolves.
#   - the overlay is configured before test/ (root CMakeLists.txt adds the custom directory at
#     :403 and test/ at :445), so the sanitizer timeout escalation applied inside
#     test/CMakeLists.txt does not reach tests registered here. Pass an explicit TIMEOUT to
#     add_qgc_test if a sanitizer lane needs one.
# ----------------------------------------------------------------------------

if(QGC_BUILD_TESTING)
    list(APPEND _aerora_sources
        ${AERORA_SECURITY_TEST_SOURCES}
        ${AERORA_TELEMETRY_TEST_SOURCES}
        ${AERORA_VEHICLE_TEST_SOURCES}
        ${AERORA_MISSION_TEST_SOURCES}
        ${AERORA_PAYLOAD_POWER_TEST_SOURCES}
        ${AERORA_UI_TEST_SOURCES}
    )

    list(APPEND _aerora_include_directories
        ${AERORA_SECURITY_TEST_INCLUDE_DIRECTORIES}
        ${AERORA_TELEMETRY_TEST_INCLUDE_DIRECTORIES}
        ${AERORA_VEHICLE_TEST_INCLUDE_DIRECTORIES}
        ${AERORA_MISSION_TEST_INCLUDE_DIRECTORIES}
        ${AERORA_PAYLOAD_POWER_TEST_INCLUDE_DIRECTORIES}
        ${AERORA_UI_TEST_INCLUDE_DIRECTORIES}
    )

    include(QGCTest)

    add_qgc_test(AeroraAuditSinkTest LABELS Unit)
    add_qgc_test(AeroraCalibrationAuthorizationTest LABELS Unit)
    add_qgc_test(AeroraCalibrationEnforcementTest LABELS Integration Vehicle)
    add_qgc_test(AeroraReportedValueTest LABELS Unit)
    add_qgc_test(AeroraSecurityStatusTest LABELS Unit)
    add_qgc_test(AeroraSigningKeyStoreTest LABELS Unit)
    add_qgc_test(AeroraTelemetryAttachmentTest LABELS Unit)
    add_qgc_test(AeroraScreenMetricsTest LABELS Unit)
    # Boots the production scene in a child process, once per candidate headless platform, so an
    # abort is recorded rather than fatal. Registered on every platform: Linux is what proves the
    # probe itself works, because there the answer is already known. NoSanitizer because the child
    # boots a QML scene; SERIAL because two instances of the application fight over settings.
    #
    # Its payload, AeroraSceneBootOnlyTest, is deliberately NOT registered here. It is the probe's
    # instrument rather than a suite: it boots the scene and asserts nothing else, so its exit code
    # means "the scene booted" and cannot be moved by an unrelated assertion. UT_REGISTER_TEST is
    # what makes it reachable as --unittest:AeroraSceneBootOnlyTest; a CTest entry would run it a
    # second time for no added coverage.
    add_qgc_test(AeroraSceneHarnessProbeTest LABELS Integration NoSanitizer SERIAL TIMEOUT 600)
    add_qgc_test(AeroraUiFoundationTest LABELS Unit)
    add_qgc_test(AeroraUiSafetyTest LABELS Unit)
    # Loads the mission progress page on a real application engine, so it needs a connected
    # vehicle but no window - see the \section nowindow note on the test class.
    add_qgc_test(AeroraMissionProgressUITest LABELS Integration Vehicle)
    # Boots a full application scene via QmlUITestBase, so it carries the same labels and timeout
    # the core QML UI tests use (test/QmlUITests/CMakeLists.txt).
    #
    # Not registered on Windows, and the reason is a property of the harness rather than of this
    # test. QmlUITestBase::startUI() aborts partway through the scene boot on the Windows offscreen
    # platform:
    #
    #   ASSERT: "value < maximalPlusOne" in qnumeric.h, line 523   ->   exit 0xc0000409
    #
    # That path has never run on Windows in this project: windows.yml runs `include-labels:
    # Portable` only, so none of the ~25 QmlUITestBase tests in test/QmlUITests is exercised there,
    # and this overlay's Windows leg is the only one that runs `^Aerora` Integration tests at all.
    # An empty offscreen font database was ruled out as the cause (see the QT_QPA_FONTDIR block in
    # cmake/QGCTest.cmake). Fixing it means fixing the shared harness, which is a core change on
    # evidence this overlay cannot gather from a Windows-only leg.
    #
    # The source stays in AERORA_UI_TEST_SOURCES and keeps compiling, so it cannot rot, and the
    # test registers and runs as soon as a non-Windows Aerora leg exists. Registering it on Windows
    # today reports a harness defect as an overlay failure on every push.
    # AeroraVehicleConfigUITest shares the constraint: it derives from VehicleConfigUITestBase,
    # which derives from QmlUITestBase. Its longer timeout covers two scene boots plus a MockLink
    # parameter load each.
    if(NOT WIN32)
        add_qgc_test(AeroraSceneAttachmentUITest LABELS Integration NoSanitizer TIMEOUT 180)
        add_qgc_test(AeroraVehicleConfigUITest LABELS Integration Vehicle NoSanitizer TIMEOUT 300)
        add_qgc_test(AeroraFlyViewOverlayUITest LABELS Integration Vehicle NoSanitizer TIMEOUT 300)
    endif()

    # ----------------------------------------------------------------------------------------
    # Windows: no scene suite is registered, and the reason is now specific.
    #
    # AeroraSceneHarnessProbeTest boots the production scene in a child process once per
    # candidate headless plugin. Both are unusable on Windows, for *different* reasons:
    #
    #   offscreen  aborts during scene construction - qnumeric.h:523 qt_saturate, exit
    #              0xc0000409, ~8s in (run 39265133278)
    #   minimal    constructs the scene but lays nothing out: the window's root item comes up
    #              zero-sized, so every geometry assertion fails and anything that reads a size
    #              takes a fatal error (run 39272899314)
    #
    # AeroraFlyViewOverlayUITest was registered here under `minimal` on the strength of the
    # earlier run, where that suite reported "3 passed, 1 failed". That reading was too
    # generous: the slots that passed only look up objects, and none of them checks a size. The
    # registration is withdrawn until a plugin produces a laid-out scene.
    #
    # The suites stay compiled on Windows, so none of them rots, and none is reported as
    # passing.
    # ----------------------------------------------------------------------------------------
    add_qgc_test(AeroraTelemetryStateTest LABELS Unit)
    add_qgc_test(AeroraVehicleIntegrationTest LABELS Unit)
    add_qgc_test(AeroraVehicleSetupTest LABELS Unit)
    add_qgc_test(AeroraProfileRegistryTest LABELS Unit)
    add_qgc_test(AeroraVehicleLifecycleTest LABELS Integration Vehicle)
    add_qgc_test(AeroraMissionTest LABELS Unit MissionManager)
    add_qgc_test(AeroraMissionSyncTest LABELS Integration Vehicle MissionManager)
    add_qgc_test(AeroraPlanBridgeTest LABELS Integration Vehicle MissionManager)
    add_qgc_test(AeroraPayloadModelTest LABELS Unit)
    add_qgc_test(AeroraPowerModelTest LABELS Unit)
endif()

# A source listed twice would be compiled twice; de-duplicate after aggregation rather than
# relying on every manifest being disjoint.
list(REMOVE_DUPLICATES _aerora_sources)
list(REMOVE_DUPLICATES _aerora_include_directories)
if(_aerora_libraries)
    list(REMOVE_DUPLICATES _aerora_libraries)
endif()

# ----------------------------------------------------------------------------
# Export to the overlay build contract
#
# Single PARENT_SCOPE set per variable, after every append above.
# ----------------------------------------------------------------------------

set(CUSTOM_SOURCES ${_aerora_sources} PARENT_SCOPE)
set(CUSTOM_INCLUDE_DIRECTORIES ${_aerora_include_directories} PARENT_SCOPE)
set(CUSTOM_LIBRARIES ${_aerora_libraries} PARENT_SCOPE)

# ----------------------------------------------------------------------------
# Custom build identification
#
# QGC_CUSTOM_BUILD routes QGCCorePlugin::instance() through CUSTOMCLASS::instance()
# (src/API/QGCCorePlugin.cc), which is how the Aerora authorization gate is bound to the core
# send path. Without it the overlay compiles but the gate is never consulted. See ADR-012.
# ----------------------------------------------------------------------------

# AERORA_VEHICLE_FACTGROUP_FRIENDSHIP enables the single privileged statement in
# AeroraVehicleFactGroupRegistrar, which calls the protected FactGroup::_addFactGroup() on a
# Vehicle. It is a compile definition, not a runtime feature flag, and it pairs with
# `friend class AeroraVehicleFactGroupRegistrar;` in src/Vehicle/Vehicle.h. Both halves are now
# applied; defining this without the friend declaration would be a compile error. See ADR-016.
set(CUSTOM_DEFINITIONS
    QGC_CUSTOM_BUILD
    CUSTOMHEADER="AeroraCorePlugin.h"
    CUSTOMCLASS=AeroraCorePlugin
    AERORA_VEHICLE_FACTGROUP_FRIENDSHIP
    PARENT_SCOPE
)

set(CUSTOM_QT_COMPONENTS
    Core
    PARENT_SCOPE
)

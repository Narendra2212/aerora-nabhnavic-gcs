import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

// SetupPage lives in QGroundControl.Controls (src/QmlControls/SetupPage.qml), which the import
// above reaches. AeroraSelfReportRow needs no import: the resource alias puts every
// AERORA_UI_QML_FILES entry in one flat module directory.
//
// The active vehicle is reached through QGroundControl.multiVehicleManager rather than through
// `globals`. Upstream setup pages use `globals.activeVehicle`, and that works for them, but
// `globals` is a QtObject with an *id* inside MainWindow.qml (`:74`) rather than a context
// property - it resolves only because a Loader hands its own document's context down the chain. A
// page loaded through a different route would find nothing there. MainWindow.qml:77 defines
// globals.activeVehicle as exactly the expression below, so this is the same value with no
// dependency on how the page was loaded.

/// `SCR-VEH-04` - reported sensor and health state, and nothing where nothing has been reported.
///
/// \section what What this page is
///
/// A vehicle setup page, reached from the Vehicle Configuration view. `AeroraVehicleComponent`
/// publishes it the moment this file exists: the descriptor for `SCR-VEH-04` already names
/// `AeroraHealthPage.qml`, so no C++ change was needed, and its name and description come from that
/// descriptor rather than from here.
///
/// \section source Where the readings come from, and why they are not Aerora's
///
/// `SYS_STATUS`, by way of upstream's own parser: `Vehicle.sysStatusSensorInfo` publishes two
/// parallel `QStringList`s - the sensors the aircraft said are present, and what it said about each
/// one. No Aerora health model exists and none is invented here; `P7-VEH-004` asks for *reported*
/// state, and this is the aircraft's own report.
///
/// Each sensor's status string is rendered **verbatim**. Upstream produces it with `tr()`
/// (`SysStatusSensorInfo.cc:92-103`), so matching against "Error" or "Normal" to decide a colour
/// would be a comparison against a translated string - the defect class that upstream issue #14929
/// records and that this overlay has already declined twice. The aircraft's word for the state is
/// the aircraft's word for it.
///
/// \section unreported The reading this page exists to withhold
///
/// `Vehicle::allSensorsHealthy()` is initialised to **true** (`Vehicle.h:969`) and only moves when a
/// `SYS_STATUS` arrives and disagrees. So a vehicle that has reported nothing at all reports *all
/// sensors healthy*, which is the single most dangerous reading a health screen can show: an
/// unreported sensor presented as a working one.
///
/// This page therefore never shows a verdict that is not backed by a report. `_anyReported` is the
/// gate - the aircraft has named at least one sensor - and the aggregate row is `unchecked` until it
/// is true. Only then is `allSensorsHealthy` meaningful, because only then has something set it.
///
/// \section readonly Read-only
///
/// Nothing here writes a parameter, sends a command, or asks the aircraft for anything. Every
/// binding is a read of an upstream `Q_PROPERTY`. `requiresSetup()` is false on this component
/// because there is no action on the page to require.
SetupPage {
    id: root

    // Test hook, following the convention upstream uses for `vehicleConfig_comp_*`: without a name
    // on the root a UI test cannot confirm the setup panel actually loaded rather than silently
    // loading nothing.
    objectName: "aeroraHealthPage"

    // pageDescription is deliberately not set: SetupPage binds it to vehicleComponent.description,
    // so AeroraVehicleComponent is the single source of the page's name and description.
    pageComponent: pageComponent

    // Load-bearing, and not for this file's own use. SetupPage.qml binds `qgcPal.warningText` on
    // its "Disabled while the vehicle is ..." label and declares no QGCPalette of its own, so a
    // derived page has to supply one or that binding raises a ReferenceError on every load.
    QGCPalette {
        id: qgcPal
        colorGroupEnabled: enabled
    }

    /// The active vehicle, or null. Null is a normal state here: the Configure view can be open
    /// while the connection drops, and a page that dereferenced it would take the application down.
    readonly property var _vehicle: QGroundControl.multiVehicleManager.activeVehicle
                                    ? QGroundControl.multiVehicleManager.activeVehicle
                                    : null

    /// Upstream's `SYS_STATUS` sensor model, or null before one exists.
    readonly property var _sensorInfo: root._vehicle && root._vehicle.sysStatusSensorInfo
                                       ? root._vehicle.sysStatusSensorInfo
                                       : null

    /// The sensors the aircraft named, and what it said about each. Parallel by construction:
    /// upstream builds both lists from one map in one order (unhealthy, then healthy, then
    /// disabled), so index i of one describes index i of the other.
    readonly property var _sensorNames: root._sensorInfo ? root._sensorInfo.sensorNames : []
    readonly property var _sensorStatus: root._sensorInfo ? root._sensorInfo.sensorStatus : []

    /// Whether the aircraft has reported anything at all. The gate on every verdict below - see
    /// \section unreported.
    readonly property bool _anyReported: root._sensorNames.length > 0

    /// Upstream's aggregate, read **only** when something has been reported. Outside that gate it
    /// is a default rather than a finding.
    readonly property bool _allHealthy: root._anyReported && !!root._vehicle && root._vehicle.allSensorsHealthy

    Component {
        id: pageComponent

        // A plain ColumnLayout, not a Flickable: SetupPage already wraps the loaded page in a
        // QGCFlickable whose contentHeight tracks this item's height, so the report scrolls on a
        // short screen without a second flickable competing for the same drag gesture.
        ColumnLayout {
            width: Math.max(root.availableWidth, ScreenTools.defaultFontPixelWidth * 60)
            spacing: ScreenTools.defaultFontPixelHeight

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("Sensor state is what the aircraft reported, in the aircraft's own words. A sensor "
                           + "the aircraft has not mentioned is not shown as working, and no overall verdict "
                           + "appears until at least one report has arrived.")
                color: qgcPal.colorGrey
                font.pointSize: ScreenTools.smallFontPointSize
                wrapMode: Text.WordWrap
            }

            // ---------------------------------------------------------------- Aggregate

            QGCLabel {
                text: qsTr("Overall")
                font.bold: true
            }

            AeroraSelfReportRow {
                Layout.fillWidth: true
                label: qsTr("Sensor health")
                // Unchecked until a report exists, which is the whole point: see \section
                // unreported on why the underlying property cannot be trusted before then.
                unchecked: !root._anyReported
                warning: root._anyReported && !root._allHealthy
                statusText: {
                    if (!root._anyReported) {
                        return ""
                    }
                    return root._allHealthy
                           ? qsTr("Every sensor the aircraft enabled reported healthy.")
                           : qsTr("At least one enabled sensor reported a fault. The rows below say which.")
                }
                basis: root._anyReported
                       ? qsTr("Vehicle.allSensorsHealthy, read only because a SYS_STATUS has arrived. It "
                              + "compares the aircraft's enabled-sensor set against its healthy-sensor set.")
                       : qsTr("No SYS_STATUS sensor report has been received, so there is no health reading. "
                              + "The underlying property defaults to healthy and is deliberately not shown "
                              + "here until a report sets it.")
            }

            AeroraSelfReportRow {
                Layout.fillWidth: true
                label: qsTr("Sensors reported")
                warning: !root._anyReported
                statusText: root._anyReported
                            ? qsTr("%1 reported by the aircraft.").arg(root._sensorNames.length)
                            : qsTr("None. The aircraft has not named a single sensor.")
                basis: qsTr("Vehicle.sysStatusSensorInfo, built from the SYS_STATUS present, enabled and "
                            + "health bitmasks. A sensor absent from the present mask is not listed at all, "
                            + "which is different from one listed as disabled.")
            }

            // ---------------------------------------------------------------- Per sensor

            QGCLabel {
                text: qsTr("Reported sensors")
                font.bold: true
                visible: root._anyReported
            }

            Repeater {
                model: root._sensorNames

                // One row per reported sensor. required properties, so a model change cannot leave
                // a delegate bound to a stale index.
                AeroraSelfReportRow {
                    id: sensorRow

                    required property string modelData
                    required property int index

                    Layout.fillWidth: true
                    label: sensorRow.modelData
                    // No warning derived from the status text: it is translated upstream, so a
                    // comparison here would be a comparison against a locale. The aggregate row
                    // above carries the fault signal, from a boolean.
                    statusText: root._sensorStatus.length > sensorRow.index
                                ? root._sensorStatus[sensorRow.index]
                                : qsTr("No status reported for this sensor.")
                    basis: qsTr("The aircraft's own word for this sensor's state, carried verbatim and not "
                                + "reinterpreted. Sensors the aircraft reported as faulty are listed first.")
                }
            }

            // ---------------------------------------------------------------- Scope of this page

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("This page reports what the aircraft has said about its own sensors. It carries no "
                           + "control: nothing on it calibrates, enables, disables or resets a sensor, writes "
                           + "a parameter or sends anything to the aircraft.")
                color: qgcPal.colorGrey
                font.pointSize: ScreenTools.smallFontPointSize
                wrapMode: Text.WordWrap
            }
        }
    }
}

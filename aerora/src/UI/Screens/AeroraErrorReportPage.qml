import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.AnalyzeView

// AnalyzePage is imported at file level rather than left to the module's IMPORTS list, for the
// reason recorded on AeroraOperationsHub.qml: Qt documents IMPORTS in the consumer direction only.
//
// AeroraUi and AeroraSecurity need no import of their own - they register into the application
// target's module (URI QGC) and `import QGroundControl` reaches it. AeroraSelfReportRow needs none
// either: the resource alias puts every AERORA_UI_QML_FILES entry in one flat module directory.

/// `SCR-HLP-03` - an error report the operator assembles, reads, and sends if and when they choose.
///
/// \section rule The requirement this screen is shaped by
///
/// `P7-HELP-002`: a report is assembled **locally** and transmitted **only on explicit operator
/// action**. This screen satisfies the second half the strongest way available - it has no transmit
/// path at all. Nothing here opens a socket, writes a file, or calls anything that could. The one
/// action on the screen puts the text on the station's clipboard, which leaves the machine only if
/// a person pastes it somewhere.
///
/// That is deliberately not an "upload" button that does nothing. `P7-LOG-003` leaves export with
/// provenance unresolved, and a button offering to send a report to a destination that does not
/// exist would be exactly the failure `P7-FLY-004` argues against.
///
/// \section content What goes in the report, and what cannot
///
/// Only values the station already displays somewhere else. Assembling a report must not become a
/// way to reach state the rest of the interface is not allowed to read: everything below comes from
/// `QGroundControl`'s own build properties, `AeroraSecurity`'s projection and `AeroraUi`'s typed
/// read-only accessors, all of which other Aerora screens already bind.
///
/// Two absences are stated in the report rather than omitted from it, because a reader who does not
/// find them would otherwise not know they were missing:
///
/// \li **no upstream base commit is recorded.** The report carries this build's own commit, branch
///     and working-tree state, which `SCR-SHL-04` added. What is still absent is the upstream
///     QGroundControl commit the fork diverged from - nothing in the build records it - so the
///     report names the field as not recorded rather than offering the fork's own hash in its
///     place.
/// \li **no durable log is attached.** There is no local store (`Q-037`, `P7-DAT-002`), so the
///     report is a snapshot of now and carries no history. A report that silently omitted history
///     would read as a report that found none.
///
/// \section readonly Read-only
///
/// Nothing here commands the aircraft, changes a policy, writes a setting or alters what is
/// recorded. The clipboard copy is a station-local action requiring no authority, which is why it
/// is a button on a screen that otherwise has none.
AnalyzePage {
    id: root

    pageComponent: pageComponent
    pageDescription: qsTr("A snapshot of what the station can say about itself, assembled here and sent "
                          + "nowhere. Copy it to include in a report.")
    allowPopout: true

    // Test hook. Without a name on the root a scene test cannot confirm the Analyze entry loaded
    // this page rather than silently loading nothing.
    objectName: "aeroraErrorReportPage"

    readonly property bool _servicesAvailable: !!AeroraUi && AeroraUi.available

    /// The active vehicle, or null. Null is a normal state: the report is useful with no vehicle
    /// connected, and says so.
    ///
    /// Reached through the `QGroundControl` singleton rather than through `globals`, which is an id
    /// on a QtObject inside MainWindow.qml rather than a context property - see the note on
    /// AeroraHealthPage.qml for why that distinction matters to a freshly loaded page.
    readonly property var _vehicle: QGroundControl.multiVehicleManager.activeVehicle
                                    ? QGroundControl.multiVehicleManager.activeVehicle
                                    : null

    /// See the enum note on AeroraSecurityPosturePage: matched against the enforced value and
    /// negated, so a resolution failure reports *not enforced* rather than claiming enforcement.
    readonly property bool _notEnforced: AeroraSecurity.enforcement !== AeroraSecurity.EnforcedAtCommandGate

    /// The report, as plain text.
    ///
    /// Built as one string rather than scraped from the rows below, so what the operator copies is
    /// what the operator read. Every line is a value this interface already displays elsewhere.
    readonly property string _report: root._buildReport()

    function _yesNo(value) {
        return value ? qsTr("yes") : qsTr("no")
    }

    function _buildReport() {
        var lines = []

        lines.push(qsTr("Aerora station report"))
        lines.push("")

        lines.push(qsTr("Station"))
        lines.push(qsTr("  Application version: %1").arg(QGroundControl.qgcVersion))
        lines.push(qsTr("  Build date: %1").arg(QGroundControl.qgcAppDate))
        lines.push(qsTr("  Daily build: %1").arg(root._yesNo(QGroundControl.qgcDailyBuild)))
        // One hash for both the base and the overlay: they are the same git tree. SCR-SHL-04 is
        // where this is laid out; the report carries it because a report without a build identity
        // cannot be acted on.
        lines.push(qsTr("  Commit: %1").arg(AeroraProvenance.commitKnown
                                            ? AeroraProvenance.commit
                                            : qsTr("not recorded - built outside a checkout")))
        lines.push(qsTr("  Branch: %1").arg(AeroraProvenance.branch === ""
                                            ? qsTr("not recorded")
                                            : AeroraProvenance.branch))
        lines.push(qsTr("  Working tree at build: %1").arg(AeroraProvenance.workingTreeDirty
                                                           ? qsTr("had uncommitted changes")
                                                           : qsTr("clean")))
        lines.push(qsTr("  Version: %1").arg(AeroraProvenance.version))
        // Stated, not omitted. See \section content.
        lines.push(qsTr("  Upstream base commit: not recorded in this build"))
        lines.push("")

        lines.push(qsTr("Command authorization"))
        lines.push(qsTr("  Enforced: %1").arg(root._notEnforced ? qsTr("no - no policy installed")
                                                                : qsTr("yes - at the queued MAV_CMD gate")))
        lines.push(qsTr("  Observed refusing a command: %1").arg(root._yesNo(AeroraSecurity.enforcementVerified)))
        lines.push(qsTr("  Paths not covered: PARAM_SET, mission writes, MANUAL_CONTROL, RC override"))
        lines.push("")

        lines.push(qsTr("Audit trail"))
        lines.push(qsTr("  Sink installed: %1").arg(root._yesNo(AeroraSecurity.auditSinkInstalled)))
        lines.push(qsTr("  Survives restart: %1").arg(root._yesNo(AeroraSecurity.auditDurable)))
        lines.push(qsTr("  Tamper-evident: %1").arg(root._yesNo(AeroraSecurity.auditTamperEvident)))
        lines.push(qsTr("  Records lost this session: %1").arg(AeroraSecurity.auditRecordsLost))
        lines.push(qsTr("  Durable history attached: no - the station has no local store"))
        lines.push("")

        lines.push(qsTr("Aerora services"))
        if (!root._servicesAvailable) {
            lines.push(qsTr("  Not attached. Either no vehicle is connected, or the connected vehicle is not "
                            + "an Aerora composition."))
        } else {
            lines.push(qsTr("  Identity: %1").arg(root._yesNo(!!AeroraUi.identity)))
            lines.push(qsTr("  Capabilities: %1").arg(root._yesNo(!!AeroraUi.capabilities)))
            lines.push(qsTr("  Payload: %1").arg(root._yesNo(!!AeroraUi.payload)))
            lines.push(qsTr("  Power: %1").arg(root._yesNo(!!AeroraUi.power)))
            lines.push(qsTr("  Mission observer: %1").arg(root._yesNo(!!AeroraUi.mission)))
            lines.push(qsTr("  Product profile: %1").arg(AeroraUi.productProfileId === ""
                                                         ? qsTr("none resolved")
                                                         : AeroraUi.productProfileId))
        }
        lines.push("")

        lines.push(qsTr("Active vehicle"))
        if (!root._vehicle) {
            lines.push(qsTr("  None connected."))
        } else {
            if (root._servicesAvailable && AeroraUi.identity) {
                lines.push(qsTr("  MAVLink system ID: %1").arg(AeroraUi.identity.mavlinkSystemId))
                lines.push(qsTr("  MAVLink component ID: %1").arg(AeroraUi.identity.mavlinkComponentId))
                lines.push(qsTr("  Aerora fleet identity: %1").arg(AeroraUi.identity.fleetIdentityKnown
                                                                   ? AeroraUi.identity.fleetIdentity
                                                                   : qsTr("not reported")))
            }
            lines.push(qsTr("  Firmware version: %1.%2.%3 %4")
                       .arg(root._vehicle.firmwareMajorVersion)
                       .arg(root._vehicle.firmwareMinorVersion)
                       .arg(root._vehicle.firmwarePatchVersion)
                       .arg(root._vehicle.firmwareVersionTypeString))
            lines.push(qsTr("  Firmware git hash: %1").arg(root._vehicle.gitHash === ""
                                                           ? qsTr("not reported")
                                                           : root._vehicle.gitHash))
            lines.push(qsTr("  Sensors reported: %1").arg(root._sensorCount))
        }

        return lines.join("\n")
    }

    /// How many sensors the aircraft has named. Zero is a reading, not a gap - it means the
    /// aircraft has reported nothing, which is what `SCR-VEH-04` exists to say out loud.
    readonly property int _sensorCount: (root._vehicle && root._vehicle.sysStatusSensorInfo)
                                        ? root._vehicle.sysStatusSensorInfo.sensorNames.length
                                        : 0

    Component {
        id: pageComponent

        QGCFlickable {
            objectName: "aeroraErrorReport_flickable"

            width: root.availableWidth
            height: root.availableHeight
            contentHeight: reportColumn.height
            flickableDirection: Flickable.VerticalFlick

            ColumnLayout {
                id: reportColumn

                width: root.availableWidth
                spacing: ScreenTools.defaultFontPixelHeight

                // ------------------------------------------------------------ What this is

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Transmission")
                    statusText: qsTr("Nothing is sent. This screen has no network or file path of any kind.")
                    basis: qsTr("P7-HELP-002 requires a report be transmitted only on explicit operator "
                                + "action. There is no transmit path here at all, so the requirement holds "
                                + "structurally rather than by policy.")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("History included")
                    // A warning, because it is a real limitation of the report rather than a
                    // property of this screen: a reader must not take a snapshot for a log.
                    warning: true
                    statusText: qsTr("None. The report describes the station as it is now and carries no "
                                     + "record of earlier sessions.")
                    basis: qsTr("The station has no durable local store (Q-037, P7-DAT-002), so there is no "
                                + "history to attach. Stated in the report as well, so a reader does not "
                                + "mistake its absence for a clean history.")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Build provenance")
                    warning: true
                    statusText: qsTr("Version and build date only. Neither the base commit nor the overlay "
                                     + "commit is recorded in this build.")
                    basis: qsTr("P7-HELP-004 asks the station to state both. Nothing embeds them, so the "
                                + "report names them as not recorded rather than omitting the fields.")
                }

                // ------------------------------------------------------------ The report

                QGCLabel {
                    text: qsTr("Report")
                    font.bold: true
                }

                QGCLabel {
                    objectName: "aeroraErrorReport_text"
                    Layout.fillWidth: true
                    text: root._report
                    color: qgcPal.text
                    font.pointSize: ScreenTools.smallFontPointSize
                    font.family: ScreenTools.fixedFontFamily
                    wrapMode: Text.WordWrap
                }

                QGCButton {
                    objectName: "aeroraErrorReport_copyButton"
                    text: qsTr("Copy report to clipboard")
                    // The only action on any Aerora screen, and it acts on the station's clipboard
                    // rather than on the aircraft: no command, no authorization, no transmission.
                    // QGroundControlQmlGlobal::copyToClipboard() is upstream's own invokable.
                    onClicked: QGroundControl.copyToClipboard(root._report)
                }

                // ------------------------------------------------------------ Scope of this screen

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("This screen assembles a description of the station. Apart from the clipboard "
                               + "copy above it carries no control: nothing on it commands the aircraft, "
                               + "changes a policy, writes a setting or alters what is recorded.")
                    color: qgcPal.colorGrey
                    font.pointSize: ScreenTools.smallFontPointSize
                    wrapMode: Text.WordWrap
                }

                QGCPalette {
                    id: qgcPal
                    colorGroupEnabled: enabled
                }
            }
        }
    }
}

import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

// AeroraSelfReportRow needs no import: the resource alias puts every AERORA_UI_QML_FILES entry
// under one module, Aerora.Controls, and types within a module are mutually visible. Importing it
// here would be a module importing itself.

/// `SCR-SHL-01` - the Aerora Operations Hub.
///
/// \section what What this screen is
///
/// The overlay's honest self-report. It answers one question: *what is actually in force right
/// now?* - which services attached to the active vehicle, whether command authorization is being
/// evaluated, and what the audit trail is and is not.
///
/// It is **not** a dashboard of green tiles. That is the recorded anti-requirement for this screen
/// and the reason every row goes through `AeroraSelfReportRow`, which cannot render a verdict
/// without also rendering what the verdict rests on, and which distinguishes "checked and absent"
/// from "never checked".
///
/// \section enforcement The rule this screen exists to not break
///
/// `AeroraSecurity.enforcement` is `NotEnforced` in every build shipped so far: the decision point
/// is wired into the command send path and no policy is installed, because `Q-029` has not
/// produced an operator role set for one to be written against. This screen must never read as
/// though authorization is in force, and three separate things keep it from doing so:
///
/// \li the enforcement row is a `warning` whenever `enforcement === AeroraSecurity.NotEnforced`,
///     and its text says *not enforced* in those words;
/// \li `AeroraSecurity.enforcementVerified` is reported as its own row, because a configured
///     policy is not an observed one. It is false on every build that has not been compiled and
///     exercised (`D-006`), so the satisfied case still carries the caveat;
/// \li the enforced wording names its scope - the queued `MAV_CMD` gate - rather than claiming
///     commands in general. `PARAM_SET`, mission writes, `MANUAL_CONTROL` and RC override are
///     **not** covered, Tab 9 owns that boundary (`P7-AUTH-003`), and this screen asserts nothing
///     about them.
///
/// \section scroll Why the content scrolls
///
/// `AnalyzePage` clips its content area and provides no scrolling of its own. Fifteen report rows
/// do not fit a short screen, and a clipped row is a row the operator cannot see - on the one
/// screen whose whole purpose is that a missing subsystem cannot hide. So the report goes inside a
/// `QGCFlickable`, which is the pattern the upstream analyze pages already use for content taller
/// than the view.
///
/// \section nav Why there are no navigation buttons yet
///
/// `WP7-B` specifies the hub as the navigation root, reaching the other Aerora screens through
/// `mainWindow.showTool()`. None of those screens exists: `SCR-VEH-*`, `SCR-LIV-*`, `SCR-SEC-*`
/// and the rest are `DESIGNED` or `SPECIFIED-NOT-DESIGNED` under `WP7-C`. A button that opens
/// nothing is worse than an absent one - the same reasoning `P7-FLY-004` applies to action
/// controls - so the navigation section arrives with its first destination rather than ahead of
/// it.
///
/// \section reg How this screen is reached
///
/// Through `QGCCorePlugin::analyzePages()`, which `AeroraCorePlugin` must override.
/// `aerora/src/AeroraCorePlugin.*` is Tab 1's file, so that override is a handoff, not an edit
/// made here. The resource URL to register is
/// `qrc:/qml/Aerora/Controls/AeroraOperationsHub.qml` - flat under the module prefix because
/// `qgc_set_qt_resource_alias()` aliases every `AERORA_UI_QML_FILES` entry to its bare name,
/// which is why this file's source directory does not appear in its URL.
AnalyzePage {
    id: root

    pageComponent: pageComponent
    pageDescription: qsTr("What the Aerora overlay has actually attached, authorized and recorded. "
                          + "Every row states what was checked.")
    allowPopout: true

    readonly property bool _servicesAvailable: AeroraUi.available

    // Read once into locals so every row below is derived from the same observation rather than
    // re-reading a live property per row, which could render a self-inconsistent screen if the
    // active vehicle changed mid-layout.
    readonly property bool _notEnforced: AeroraSecurity.enforcement === AeroraSecurity.NotEnforced

    /// The basis string shared by every per-vehicle service row.
    ///
    /// The distinction it carries is the whole point of the screen: with no Aerora vehicle
    /// connected the service map was never consulted, so each service row is `unchecked` rather
    /// than reported absent.
    readonly property string _serviceBasis: root._servicesAvailable
                                            ? qsTr("Checked against the active vehicle's Aerora service map.")
                                            : qsTr("No Aerora vehicle is connected, so the service map "
                                                   + "was not consulted.")

    Component {
        id: pageComponent

        QGCFlickable {
            width: root.availableWidth
            height: root.availableHeight
            contentHeight: reportColumn.height
            flickableDirection: Flickable.VerticalFlick

            ColumnLayout {
                id: reportColumn

                width: root.availableWidth
                spacing: ScreenTools.defaultFontPixelHeight

                // ------------------------------------------------------------ Command authorization

                QGCLabel {
                    text: qsTr("Command authorization")
                    font.bold: true
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Authorization enforcement")
                    warning: root._notEnforced
                    statusText: root._notEnforced
                                ? qsTr("Not enforced. No authorization policy is installed, so queued commands "
                                       + "are not evaluated.")
                                : qsTr("Enforced at the queued MAV_CMD gate.")
                    basis: root._notEnforced
                           ? qsTr("AeroraSecurity.enforcement is NotEnforced. The decision point exists in the "
                                  + "command send path; the operator role set (Q-029) is unresolved, so there is "
                                  + "no policy for it to consult.")
                           : qsTr("AeroraSecurity.enforcement reports an installed identity provider, role "
                                  + "resolver and configured policy. Scope is the queued MAV_CMD gate only.")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Enforcement verified")
                    // A false here is a warning even when a policy is configured, because
                    // "configured" and "observed to refuse a command" are different claims and only
                    // the second one is evidence.
                    warning: !AeroraSecurity.enforcementVerified
                    statusText: AeroraSecurity.enforcementVerified
                                ? qsTr("Verified against an executed test.")
                                : qsTr("Not verified. No build of this overlay has been compiled and exercised.")
                    basis: qsTr("AeroraSecurity.enforcementVerified. Blocked on a build host (D-006).")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Paths not covered")
                    // Not a verdict about those paths - an explicit statement that this screen makes
                    // none. Reported as a warning so it is not read as reassurance.
                    warning: true
                    statusText: qsTr("PARAM_SET, mission writes, MANUAL_CONTROL and RC override are outside "
                                     + "the queued MAV_CMD gate.")
                    basis: qsTr("Stated, not measured. The parameter write path is assessed separately "
                                + "(P7-AUTH-003); this screen asserts nothing about it.")
                }

                // ------------------------------------------------------------ Audit trail

                QGCLabel {
                    text: qsTr("Audit trail")
                    font.bold: true
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Audit destination")
                    warning: !AeroraSecurity.auditSinkInstalled
                    statusText: AeroraSecurity.auditSinkInstalled
                                ? qsTr("A sink is installed.")
                                : qsTr("No sink is installed. Audit records have nowhere to go and are counted "
                                       + "as lost.")
                    basis: qsTr("AeroraSecurity.auditSinkInstalled.")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Record durability")
                    warning: !AeroraSecurity.auditDurable
                    statusText: AeroraSecurity.auditDurable
                                ? qsTr("Records are reported durable across restart.")
                                : qsTr("Not durable. Nothing written survives a restart.")
                    basis: qsTr("AeroraSecurity.auditDurable, which is the sink's own report. The local store "
                                + "design exists; no implementation does (Q-037).")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Tamper evidence")
                    warning: !AeroraSecurity.auditTamperEvident
                    statusText: AeroraSecurity.auditTamperEvident
                                ? qsTr("Records are reported tamper-evident.")
                                : qsTr("Not tamper-evident. Modification of a stored record would not be "
                                       + "detectable.")
                    basis: qsTr("AeroraSecurity.auditTamperEvident. Blocked on a protected anchor, which is "
                                + "blocked on platform key protection (Q-037, D-006).")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Records lost")
                    warning: AeroraSecurity.auditRecordsLost > 0
                    statusText: AeroraSecurity.auditRecordsLost > 0
                                ? qsTr("%1 record(s) could not be written.").arg(AeroraSecurity.auditRecordsLost)
                                : qsTr("None lost since this session started.")
                    // The counter counts losses only. It is deliberately not phrased as a number of
                    // records stored, because nothing stores them.
                    basis: qsTr("AeroraSecurity.auditRecordsLost, a monotonic count of records this process "
                                + "failed to write. It is not a count of records retained.")
                }

                // ------------------------------------------------------------ Per-vehicle services

                QGCLabel {
                    text: qsTr("Active vehicle services")
                    font.bold: true
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Aerora services")
                    warning: !root._servicesAvailable
                    statusText: root._servicesAvailable
                                ? qsTr("Attached to the active vehicle.")
                                : qsTr("Not attached. No connected vehicle carries an Aerora service object.")
                    basis: qsTr("AeroraUi.available. A vehicle that is not an Aerora composition shows the "
                                + "stock QGroundControl interface, which is correct rather than degraded.")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Vehicle identity")
                    unchecked: !root._servicesAvailable
                    warning: root._servicesAvailable && !AeroraUi.identity
                    statusText: AeroraUi.identity ? qsTr("Attached.") : qsTr("Not attached.")
                    basis: root._serviceBasis
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Capabilities")
                    unchecked: !root._servicesAvailable
                    warning: root._servicesAvailable && !AeroraUi.capabilities
                    statusText: AeroraUi.capabilities ? qsTr("Attached.") : qsTr("Not attached.")
                    basis: root._serviceBasis
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Payload model")
                    unchecked: !root._servicesAvailable
                    warning: root._servicesAvailable && !AeroraUi.payload
                    statusText: AeroraUi.payload ? qsTr("Attached.") : qsTr("Not attached.")
                    basis: root._serviceBasis
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Power model")
                    unchecked: !root._servicesAvailable
                    warning: root._servicesAvailable && !AeroraUi.power
                    statusText: AeroraUi.power ? qsTr("Attached.") : qsTr("Not attached.")
                    basis: root._serviceBasis
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Mission observer")
                    unchecked: !root._servicesAvailable
                    warning: root._servicesAvailable && !AeroraUi.mission
                    statusText: AeroraUi.mission ? qsTr("Attached.") : qsTr("Not attached.")
                    basis: root._serviceBasis
                }

                // ------------------------------------------------------------ Identification

                QGCLabel {
                    text: qsTr("Vehicle identification")
                    font.bold: true
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Aerora hardware")
                    unchecked: !root._servicesAvailable
                    // SelectedBySeam is deliberately a warning, not a satisfied state. It means
                    // QGroundControl picked the Aerora firmware plugin, which is an outcome of how
                    // this build was configured and says nothing about the aircraft.
                    warning: root._servicesAvailable
                             && AeroraUi.classification !== AeroraVehicleServices.CorroboratedAerora
                    statusText: {
                        switch (AeroraUi.classification) {
                        case AeroraVehicleServices.CorroboratedAerora:
                            return qsTr("Corroborated by an Aerora evidence source.")
                        case AeroraVehicleServices.SelectedBySeam:
                            return qsTr("Not identified. The Aerora firmware plugin was selected by build "
                                        + "configuration, which is not evidence about the aircraft.")
                        case AeroraVehicleServices.NotAerora:
                            return qsTr("Positively identified as not Aerora hardware.")
                        default:
                            return qsTr("No classification evidence has been seen.")
                        }
                    }
                    basis: root._servicesAvailable
                           ? qsTr("AeroraUi.classification. No Aerora evidence source exists yet, so "
                                  + "CorroboratedAerora is not currently reachable.")
                           : root._serviceBasis
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Product profile")
                    unchecked: !root._servicesAvailable
                    warning: root._servicesAvailable && AeroraUi.productProfileId === ""
                    statusText: AeroraUi.productProfileId === ""
                                ? qsTr("No profile resolved.")
                                : AeroraUi.productProfileId
                    basis: qsTr("AeroraUi.productProfileId, an opaque build-supplied identifier. It is not "
                                + "parsed and no product family is named here.")
                }

                // ------------------------------------------------------------ Scope of this screen

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("This screen reports the station's own configuration. It is observation and "
                               + "supervision only and carries no control: nothing on it commands the "
                               + "aircraft, changes a policy, or alters what is recorded.")
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

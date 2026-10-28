import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.AnalyzeView

// AnalyzePage is imported at file level rather than left to the module's IMPORTS list, for the
// reason recorded on AeroraOperationsHub.qml: Qt documents IMPORTS in the consumer direction only.
//
// AeroraSecurity needs no import of its own - it registers into the application target's module
// (URI QGC) and `import QGroundControl` reaches it. AeroraSelfReportRow needs none either: the
// resource alias puts every AERORA_UI_QML_FILES entry in one flat module directory.

/// `SCR-SEC-01` - whether command authorization is in force, and exactly what that covers.
///
/// \section why Why this is not the Operations Hub again
///
/// The hub (`SCR-SHL-01`) reports the station's whole posture in one row per fact, security
/// included. It answers *is authorization enforced?* in a sentence. This screen answers the
/// question an operator asks next and the hub has no room for: **enforced over what?**
///
/// That distinction is the reason `P7-SEC-005` names a security screen separately. An operator told
/// "authorization enforced" will reasonably assume the station cannot send an unauthorized
/// instruction to the aircraft, and that assumption is false: the gate sits on queued `MAV_CMD`
/// traffic and one direct calibration send, and four other outbound paths go nowhere near it. A
/// one-line status cannot carry that; a table can.
///
/// \section state What the station can actually say about itself
///
/// Everything here reads `AeroraSecurity`, which is a computed projection holding no security state
/// of its own - no identity, no role, no policy, no key, no sink (`P7-SEC-004`, `D-UI-01`). So this
/// screen can report *whether* the chain is configured and cannot report *which link* is missing,
/// because that detail is deliberately not exposed. The prose below names the chain; the rows
/// report only what the projection returns.
///
/// Three distinctions are kept apart rather than collapsed, because each is a different fact:
///
/// \li a mechanism existing is not a policy being configured - `enforcement`;
/// \li a policy being configured is not enforcement having been observed - `enforcementVerified`;
/// \li an audit sink existing is not a record surviving - `auditSinkInstalled` versus
///     `auditDurable` versus `auditTamperEvident`.
///
/// \section enum How the enum comparison is written
///
/// Against `EnforcedAtCommandGate` and negated, never against `NotEnforced`. If that enumerator
/// ever failed to resolve - a rename, a registration that did not happen - the comparison yields
/// `undefined === value`, `_enforced` is false, and this screen reports *not enforced*. Written the
/// other way round, the identical failure would make it claim enforcement that is not happening.
/// Only one of those two failure directions is acceptable on this screen.
///
/// \section readonly Read-only
///
/// Nothing here installs a policy, changes a role, authorizes a command or writes an audit record.
/// `AeroraSecurity` has no `Q_INVOKABLE` and no writable property, so there is nothing on it to
/// call - a property `AeroraSecurityStatusTest` holds at zero methods and zero writable properties.
AnalyzePage {
    id: root

    pageComponent: pageComponent
    pageDescription: qsTr("Whether the station evaluates command authorization, and which outbound paths "
                          + "that covers. Every row states what it was read from.")
    allowPopout: true

    // Test hook. Without a name on the root a scene test cannot confirm the Analyze entry loaded
    // this page rather than silently loading nothing.
    objectName: "aeroraSecurityPosturePage"

    /// See \section enum. `_notEnforced` is the negation, not a second comparison.
    readonly property bool _enforced: AeroraSecurity.enforcement === AeroraSecurity.EnforcedAtCommandGate
    readonly property bool _notEnforced: !root._enforced

    /// The outbound paths, and whether the authorization gate is on them.
    ///
    /// Listed as data rather than prose so the uncovered ones cannot be dropped by an edit that
    /// only touches the covered ones. `covered` is a property of where the gate is installed in the
    /// core send path, not of whether a policy is loaded - a covered path with no policy is
    /// evaluated against nothing, which is what the enforcement row above says.
    readonly property var _paths: [
        {
            name: qsTr("Queued MAV_CMD commands"),
            covered: true,
            detail: qsTr("MavCommandQueue::sendWorker() reaches QGCCorePlugin::authorizeMavCommand().")
        },
        {
            name: qsTr("Sensor calibration start"),
            covered: true,
            detail: qsTr("Vehicle::startCalibration() is the one direct send that is gated (ADR-015).")
        },
        {
            name: qsTr("Parameter writes (PARAM_SET)"),
            covered: false,
            detail: qsTr("A different send path. Assessed separately under P7-AUTH-003; this screen "
                         + "asserts nothing about it.")
        },
        {
            name: qsTr("Mission, geofence and rally writes"),
            covered: false,
            detail: qsTr("The mission protocol does not pass through the command gate. Aerora adds no "
                         + "mission write path of its own.")
        },
        {
            name: qsTr("Manual control (MANUAL_CONTROL)"),
            covered: false,
            detail: qsTr("Streamed stick input, not a command. Unevidenced.")
        },
        {
            name: qsTr("RC channel override"),
            covered: false,
            detail: qsTr("Unevidenced.")
        }
    ]

    Component {
        id: pageComponent

        QGCFlickable {
            objectName: "aeroraSecurity_flickable"

            width: root.availableWidth
            height: root.availableHeight
            contentHeight: securityColumn.height
            flickableDirection: Flickable.VerticalFlick

            ColumnLayout {
                id: securityColumn

                width: root.availableWidth
                spacing: ScreenTools.defaultFontPixelHeight

                // ------------------------------------------------------------ Enforcement

                QGCLabel {
                    text: qsTr("Command authorization")
                    font.bold: true
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Enforcement")
                    warning: root._notEnforced
                    statusText: root._notEnforced
                                ? qsTr("Not enforced. No authorization policy is installed, so commands that "
                                       + "reach the gate pass without being evaluated.")
                                : qsTr("Enforced at the queued MAV_CMD gate. Scope is the table below.")
                    basis: root._notEnforced
                           ? qsTr("AeroraSecurity.enforcement is NotEnforced. The decision point exists in "
                                  + "the command send path; an operator role set (Q-029) is unresolved, so "
                                  + "there is no policy for it to consult.")
                           : qsTr("AeroraSecurity.enforcement reports an installed identity provider, role "
                                  + "resolver and configured policy. The projection does not say which of "
                                  + "the three it checked, by design.")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Observed to refuse a command")
                    // A warning even when a policy is configured: "configured" and "seen to refuse"
                    // are different claims and only the second is evidence.
                    warning: !AeroraSecurity.enforcementVerified
                    statusText: AeroraSecurity.enforcementVerified
                                ? qsTr("Yes. The gate has been observed refusing a command.")
                                : qsTr("No. No build of this overlay has been exercised against the gate, so "
                                       + "enforcement is designed rather than demonstrated.")
                    basis: qsTr("AeroraSecurity.enforcementVerified, a compile-time value rather than a "
                                + "runtime reading. It is what stops a configured policy being presented as "
                                + "a working one.")
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("The chain is an operator identity, a role resolved for that identity, and a "
                               + "policy that decides. All three must be present before anything is "
                               + "evaluated. Which of them is missing is not reported here: the station "
                               + "keeps identity, role and policy out of the interface entirely, so there is "
                               + "nothing to read.")
                    color: qgcPal.colorGrey
                    font.pointSize: ScreenTools.smallFontPointSize
                    wrapMode: Text.WordWrap
                }

                // ------------------------------------------------------------ Scope

                QGCLabel {
                    text: qsTr("What the gate covers")
                    font.bold: true
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Enforcement is a property of a send path, not of the station. These are the "
                               + "outbound paths and whether the gate sits on them. An uncovered path is "
                               + "uncovered whether or not a policy is installed.")
                    color: qgcPal.colorGrey
                    font.pointSize: ScreenTools.smallFontPointSize
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: root._paths

                    // One row per path. required property, so a model change cannot leave a
                    // delegate bound to a stale index.
                    AeroraSelfReportRow {
                        id: pathRow

                        required property var modelData

                        Layout.fillWidth: true
                        label: pathRow.modelData.name
                        // An uncovered path is a warning regardless of enforcement state, because
                        // it is uncovered either way. A covered path is a warning while nothing is
                        // enforced, because the gate is there and evaluating nothing.
                        warning: !pathRow.modelData.covered || root._notEnforced
                        statusText: {
                            if (!pathRow.modelData.covered) {
                                return qsTr("Not covered by the authorization gate.")
                            }
                            return root._notEnforced
                                   ? qsTr("On the gate, which currently evaluates nothing.")
                                   : qsTr("On the gate, and evaluated.")
                        }
                        basis: pathRow.modelData.detail
                    }
                }

                // ------------------------------------------------------------ Audit

                QGCLabel {
                    text: qsTr("Audit trail")
                    font.bold: true
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("A record being written, surviving a restart, and being tamper-evident are "
                               + "three separate facts. They are reported separately because an absence in "
                               + "any one of them means something different for what the station can later "
                               + "show about a decision.")
                    color: qgcPal.colorGrey
                    font.pointSize: ScreenTools.smallFontPointSize
                    wrapMode: Text.WordWrap
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Destination")
                    warning: !AeroraSecurity.auditSinkInstalled
                    statusText: AeroraSecurity.auditSinkInstalled
                                ? qsTr("A sink is installed.")
                                : qsTr("No sink is installed. An audit record has nowhere to go and is "
                                       + "counted as lost rather than written.")
                    basis: qsTr("AeroraSecurity.auditSinkInstalled.")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Survives a restart")
                    warning: !AeroraSecurity.auditDurable
                    statusText: AeroraSecurity.auditDurable
                                ? qsTr("Reported durable.")
                                : qsTr("No. Nothing written survives this process ending, so there is no "
                                       + "record of a past session to examine.")
                    basis: qsTr("AeroraSecurity.auditDurable, the sink's own report. The local store is "
                                + "designed and unimplemented (Q-037, P7-DAT-002).")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Tamper-evident")
                    warning: !AeroraSecurity.auditTamperEvident
                    statusText: AeroraSecurity.auditTamperEvident
                                ? qsTr("Reported tamper-evident.")
                                : qsTr("No. Modification of a stored record would not be detectable, so a "
                                       + "record cannot be relied on as evidence.")
                    basis: qsTr("AeroraSecurity.auditTamperEvident. Blocked on a protected anchor, which is "
                                + "blocked on platform key protection (Q-037, D-006).")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Records lost this session")
                    warning: AeroraSecurity.auditRecordsLost > 0
                    statusText: AeroraSecurity.auditRecordsLost > 0
                                ? qsTr("%1.").arg(AeroraSecurity.auditRecordsLost)
                                : qsTr("None since this session started.")
                    // Deliberately not phrased as a number of records held: nothing holds them.
                    basis: qsTr("AeroraSecurity.auditRecordsLost, a count of records this process failed to "
                                + "write. IF-LOG-015 requires it be shown. It is not a count of records "
                                + "retained.")
                }

                // ------------------------------------------------------------ Scope of this screen

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("This screen reports the station's own authorization posture. It carries no "
                               + "control: nothing on it installs a policy, changes a role, authorizes a "
                               + "command or alters what is recorded.")
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

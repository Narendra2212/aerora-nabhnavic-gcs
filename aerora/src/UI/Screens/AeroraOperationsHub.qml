import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.AnalyzeView

// AnalyzePage is imported at file level, not left to this module's IMPORTS list. Qt documents
// IMPORTS in the consumer direction only - "if a QML file imports this module, it also imports all
// the modules listed under IMPORTS" - which is a guarantee for importers of Aerora.Controls, not
// for its own members. HO-TAB1-13.
//
// AeroraSecurity and AeroraUi need no entry of their own: they register into the application
// target's module (URI QGC), and the `import QGroundControl` above reaches it, because
// QGroundControlModule declares IMPORTS QGC (src/CMakeLists.txt:22-23). That is the consumer
// direction and is the documented one.
//
// AeroraSelfReportRow needs no import either: the resource alias puts every AERORA_UI_QML_FILES
// entry in one flat module directory, so the implicit import covers it. Importing Aerora.Controls
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
/// \li the enforcement row is a `warning` unless `enforcement` matches
///     `AeroraSecurity.EnforcedAtCommandGate`, and its text says *not enforced* in those words.
///     The comparison is written against the enforced value and negated, never against
///     `NotEnforced` — see `_enforced` below for why that direction is the fail-safe one;
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
/// \section nav Navigation, and what is deliberately not a button
///
/// `WP7-B` specifies the hub as the navigation root. It carried no navigation at first because no
/// destination existed; three now do, so the section arrived with them - a button that opens
/// nothing is worse than an absent one, which is the reasoning `P7-FLY-004` applies to action
/// controls.
///
/// Exactly one is a button, and only because the route is a real function:
/// `mainWindow.showVehicleConfig()` (`MainWindow.qml:161`), which is where `SCR-VEH-02` and
/// `SCR-VEH-06` appear in the sidebar. It is gated on a connected vehicle because
/// `VehicleConfigView` builds its whole sidebar from `_fullParameterVehicleAvailable`.
///
/// The other two destinations are **stated, not offered**, because no supported route exists:
///
/// \li `AnalyzeView` has no public API for selecting a sibling analyze page - `_loadPage()` is
///     private and the entries are a `Repeater` over `analyzePages()`. Matching an entry by its
///     translated title would break on the first locale change, which is the defect class upstream
///     issue #14929 records.
/// \li `SCR-LIV-01` is an overlay on the Fly View rather than a screen with an address, so there
///     is nothing to navigate *to*.
///
/// Deep-linking to a specific Aerora setup page is possible in principle -
/// `VehicleConfigView.showVehicleComponentPanel()` takes a component object - but reaching it means
/// matching a `VehicleComponent` by its translated name, and that is the same locale defect. The
/// sidebar already lists both pages once the view is open.
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

    /// Enforcement is derived by matching the **enforced** value, never the unenforced one, and
    /// `_notEnforced` is its negation.
    ///
    /// The asymmetry is deliberate and it is a fail-safe. If `AeroraSecurity.EnforcedAtCommandGate`
    /// ever failed to resolve - a renamed enumerator, a registration that did not happen, a typo -
    /// the comparison yields `undefined === value`, so `_enforced` is false and this screen reports
    /// *not enforced*. Written the other way round, matching `NotEnforced`, the identical failure
    /// would make `_notEnforced` false and the screen would claim enforcement that is not
    /// happening. Only one of those two failure directions is acceptable.
    readonly property bool _enforced: AeroraSecurity.enforcement === AeroraSecurity.EnforcedAtCommandGate
    readonly property bool _notEnforced: !root._enforced

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
            // Test hook. The navigation section is below fifteen report rows, so a scene test has
            // to scroll it into view before it can be clicked.
            objectName: "aeroraHub_reportFlickable"

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

                // ------------------------------------------------------------ Navigation

                QGCLabel {
                    text: qsTr("Aerora screens")
                    font.bold: true
                }

                QGCButton {
                    objectName: "aeroraHub_vehicleConfigButton"
                    text: qsTr("Open Vehicle Configuration")
                    // Gated on a vehicle, not on the Aerora pages: VehicleConfigView builds its
                    // whole sidebar from _fullParameterVehicleAvailable, so with no vehicle this
                    // opens a view with nothing in it. A button that opens an empty view is the
                    // thing P7-FLY-004 argues against, so it is disabled with the reason stated
                    // below rather than offered and useless.
                    enabled: root._servicesAvailable
                    onClicked: mainWindow.showVehicleConfig()
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: root._servicesAvailable
                          ? qsTr("Aerora Identity and Aerora Services appear in the configuration sidebar. Both "
                                 + "are read-only.")
                          : qsTr("Disabled: no Aerora vehicle is connected, so the configuration view would "
                                 + "have no pages to show.")
                    color: qgcPal.colorGrey
                    font.pointSize: ScreenTools.smallFontPointSize
                    wrapMode: Text.WordWrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    // Stated rather than offered as a button. AnalyzeView exposes no supported way
                    // to select a sibling analyze page - _loadPage() is private and the entries are
                    // a Repeater over analyzePages() - and matching one by its translated title
                    // would break the moment the locale changes. The operator is already in the
                    // list that contains it.
                    text: qsTr("Aerora Mission Progress is in the Analyze list alongside this screen. The Live "
                               + "Operations overlay appears in Fly View while an Aerora vehicle is connected.")
                    color: qgcPal.colorGrey
                    font.pointSize: ScreenTools.smallFontPointSize
                    wrapMode: Text.WordWrap
                }

                // ------------------------------------------------------------ Scope of this screen

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("This screen reports the station's own configuration. Apart from the navigation "
                               + "above it carries no control: nothing on it commands the aircraft, changes a "
                               + "policy, or alters what is recorded.")
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

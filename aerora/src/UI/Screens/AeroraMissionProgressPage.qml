import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.AnalyzeView

// AnalyzePage is imported at file level for the same reason AeroraOperationsHub.qml does it: this
// file belongs to Aerora.Controls, not to AnalyzeViewModule, so the module's own IMPORTS list is
// the wrong direction to rely on. The upstream precedent is GeoTagPage.qml:6, which belongs to
// GeoTagModule and carries the identical import.
//
// AeroraUi and AeroraMissionObserver need no import of their own: they register into the
// application target's module (URI QGC) and `import QGroundControl` reaches it, because
// QGroundControlModule declares IMPORTS QGC.
//
// AeroraSelfReportRow needs none either - the resource alias puts every AERORA_UI_QML_FILES entry
// in one flat module directory, so the implicit import covers it.

/// `SCR-LIV-06`, progress half - what the Aerora mission observer has actually seen.
///
/// \section what What this screen is
///
/// `AeroraMissionObserver` keeps three things apart that an operator interface usually conflates:
/// the ground station's plan, the mission the vehicle holds, and the vehicle's execution position.
/// This screen renders that separation rather than collapsing it into one "mission status" line,
/// and every row states what its verdict rests on.
///
/// It is observation only. The observer has no command API - no upload, download, clear, resume or
/// set-current - and nothing here acquires one. Mission commanding stays in `PlanMasterController`
/// / `MissionController` / `MissionManager`, which Aerora reuses unchanged.
///
/// \section notverified Why the agreement row is not a verification result
///
/// `AeroraMissionAgreement` is a `Q_GADGET` with no QML value-type registration, so
/// `AeroraMissionAgreement.Matches` does not resolve and the verdict cannot be compared by name.
/// Mirroring the enumerator as an integer literal here would put a silent "verified" one enum
/// reorder away, which is the one failure direction `AERORA-MISSION-REQ-007` forbids.
///
/// What is reachable without that registration is `isVerified()`, a `Q_INVOKABLE` on the gadget
/// that is true for `Verdict::Matches` alone. That is what the row reads, and it is the direction
/// `AERORA-MISSION-REQ-007` cares about: an upload nobody compared reads as not verified.
///
/// It still does not make the row a verification result, and the row says so. The gadget carries
/// the comparison depth *as part of* the verdict precisely because "matches" means something
/// materially different at each depth, and `Depth` is an enumerator this screen cannot name
/// either. A true reading is therefore reported as a match whose depth is unreadable, not as
/// verification. Registering the three mission gadgets as QML value types is the prerequisite for
/// the depth, and it is a Tab 5 change. Nothing in this build calls `verify()` and the plan side
/// is never captured, so `NotCompared` is the state in every session regardless.
///
/// \section unfed Rows that are structurally not evidenced
///
/// Both sides are now fed. `AeroraMissionBridge` feeds the vehicle side from real `MissionManager`
/// signals, so execution position, the on-board mission, transfer activity and the last outcome are
/// live. `AeroraPlanBridge` feeds the plan side from the two session `PlanMasterController`
/// instances: the Plan view's reports the ground-station plan and the editor selection, the Fly
/// view's reports the resume candidate.
///
/// A row can still read as not captured, and that is a reading rather than a structural gap: the
/// plan side reports nothing while no vehicle carries an Aerora mission observer, which covers a
/// disconnected station and the offline editing vehicle. The rows say which of the two it is
/// instead of rendering an empty plan as a captured one, because "absent" and "empty" are
/// different facts and `AeroraMissionSnapshot` keeps them apart.
///
/// \section enums How the enum comparisons are written
///
/// Every `AeroraMissionObserver` enum comparison matches the **positive** value and treats
/// anything else as not evidenced, never the reverse. If an enumerator stopped resolving, the
/// comparison yields `undefined === value`, every positive match is false, and the row reads "not
/// evidenced" - which is the honest direction. Written against the `NotEvidenced*` value instead,
/// the identical failure would make the row claim a transfer state nobody observed.
///
/// \section reg How this screen is reached
///
/// `QGCCorePlugin::analyzePages()`, which `AeroraCorePlugin` overrides. The resource URL is
/// `qrc:/qml/Aerora/Controls/AeroraMissionProgressPage.qml` - flat under the module prefix,
/// because `qgc_set_qt_resource_alias()` aliases every `AERORA_UI_QML_FILES` entry to its bare
/// name.
AnalyzePage {
    id: root

    pageComponent: pageComponent
    pageDescription: qsTr("What the Aerora mission observer has seen: the vehicle's execution position, the "
                          + "mission it holds, and the transfers in between. Every row states what was checked.")
    allowPopout: true

    readonly property var _mission: AeroraUi.mission

    /// Truthy test rather than a null comparison, so an unresolved property reads as unattached
    /// instead of attached.
    readonly property bool _attached: !!root._mission

    /// The basis string shared by every row that depends on the observer being attached.
    readonly property string _observerBasis: root._attached
                                             ? qsTr("Read from the active vehicle's Aerora mission observer.")
                                             : qsTr("No Aerora mission observer is attached, so nothing was "
                                                    + "consulted.")

    // Positive matches only. See \section enums above for why the negation is never the test.
    readonly property bool _transferInProgress: root._attached
                                                && root._mission.transferActivity === AeroraMissionObserver.InProgress
    readonly property bool _transferIdle: root._attached
                                          && root._mission.transferActivity === AeroraMissionObserver.Idle

    readonly property bool _lastSucceeded: root._attached
                                           && root._mission.lastOutcome === AeroraMissionObserver.Succeeded
    readonly property bool _lastFailed: root._attached
                                        && root._mission.lastOutcome === AeroraMissionObserver.Failed

    /// Positive match again, and the only agreement member reachable without QML value-type
    /// registration. See \section notverified for what it does and does not establish.
    ///
    /// Guarded with `typeof` rather than called straight. `AeroraMissionAgreement` is an
    /// unregistered `Q_GADGET`; an unguarded call on an invokable that did not resolve throws
    /// instead of returning, which would leave the row with no reading at all. Guarded, an
    /// unresolved invokable reads false - not verified, which is the safe direction.
    readonly property bool _agreementMatched: {
        if (!root._attached) {
            return false
        }
        const agreement = root._mission.agreement
        const readable = agreement && (typeof agreement.isVerified === "function")
        return readable ? agreement.isVerified() : false
    }

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

                // ------------------------------------------------------------ Observer attachment

                QGCLabel {
                    text: qsTr("Mission observation")
                    font.bold: true
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Mission observer")
                    warning: !root._attached
                    statusText: root._attached
                                ? qsTr("Attached to the active vehicle.")
                                : qsTr("Not attached. No connected vehicle carries an Aerora mission observer.")
                    basis: qsTr("AeroraUi.mission. The observer attaches only to vehicles routed to the Aerora "
                                + "firmware plugin; a stock vehicle shows the stock mission interface, which is "
                                + "correct rather than degraded.")
                }

                // ------------------------------------------------------------ Execution position

                QGCLabel {
                    text: qsTr("Vehicle execution position")
                    font.bold: true
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Execution position")
                    unchecked: !root._attached
                    // Attributed-but-unknown is the warning case, and it is the reason this row is
                    // not a number. A successful write resets the index before the vehicle has
                    // reported anything about the mission it now holds; rendering that as item 0
                    // would invent a position.
                    warning: root._attached
                             && root._mission.vehicleCurrentIndex.attributed
                             && !root._mission.vehicleCurrentIndex.known
                    statusText: {
                        if (!root._attached) {
                            return ""
                        }
                        const index = root._mission.vehicleCurrentIndex
                        if (!index.attributed) {
                            return qsTr("No position has been reported.")
                        }
                        if (!index.known) {
                            return qsTr("Reported, but no current item. The vehicle has not yet reported a "
                                        + "position for the mission it now holds.")
                        }
                        if (!index.vehicleAuthoritative) {
                            return qsTr("Attributed to a non-vehicle source, so it is not the aircraft's "
                                        + "position.")
                        }
                        return qsTr("Item %1.").arg(index.sequenceNumber)
                    }
                    basis: root._attached
                           ? qsTr("AeroraUi.mission.vehicleCurrentIndex. 'attributed' and 'known' are separate: an "
                                  + "attributed index carrying no sequence number is not item 0.")
                           : root._observerBasis
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Resume candidate")
                    unchecked: !root._attached
                    // Never a warning on its own: a resume candidate is a ground-station proposal,
                    // and its absence is the normal state rather than a gap.
                    statusText: {
                        if (!root._attached) {
                            return ""
                        }
                        const candidate = root._mission.resumeCandidate
                        return candidate.known
                               ? qsTr("Item %1 proposed. A ground-station proposal, never the aircraft's "
                                      + "position.").arg(candidate.sequenceNumber)
                               : qsTr("None proposed.")
                    }
                    basis: qsTr("AeroraUi.mission.resumeCandidate, reported by AeroraPlanBridge from the Fly "
                                + "view's plan controller. 'No candidate' and 'none computed' are both shown "
                                + "as no proposal: item 0 is never rendered as one, because upstream returns 0 "
                                + "for 'no resume available'.")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Plan editor selection")
                    unchecked: !root._attached
                    // Not a warning either. An editor selection is the operator's own cursor; its
                    // absence says nothing about the aircraft, and presenting it as a gap would
                    // imply this screen expected one.
                    statusText: {
                        if (!root._attached) {
                            return ""
                        }
                        const selection = root._mission.planViewSelection
                        return selection.known
                               ? qsTr("Item %1 selected in the plan editor. Never transmitted and never the "
                                      + "aircraft's position.").arg(selection.sequenceNumber)
                               : qsTr("No editor selection reported.")
                    }
                    basis: qsTr("AeroraUi.mission.planViewSelection, reported by AeroraPlanBridge from the Plan "
                                + "view's controller - the only one that maintains a selection. Reported only "
                                + "while a vehicle carries an Aerora mission observer.")
                }

                // ------------------------------------------------------------ Mission on board

                QGCLabel {
                    text: qsTr("Mission on board")
                    font.bold: true
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Vehicle mission")
                    unchecked: !root._attached
                    warning: root._attached && !root._mission.vehicleMission.captured
                    statusText: {
                        if (!root._attached) {
                            return ""
                        }
                        const mission = root._mission.vehicleMission
                        // Not captured is absence, not an empty mission. The snapshot keeps the
                        // two apart and so does this row.
                        return mission.captured
                               ? qsTr("%1 item(s), as the vehicle last reported.").arg(mission.itemCount)
                               : qsTr("Not captured. No mission has been downloaded or uploaded this session.")
                    }
                    basis: root._observerBasis
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Mission determinacy")
                    unchecked: !root._attached
                    warning: root._attached && root._mission.vehicleMissionIndeterminate
                    statusText: {
                        if (!root._attached) {
                            return ""
                        }
                        return root._mission.vehicleMissionIndeterminate
                               ? qsTr("Indeterminate. A failed upload left the vehicle in a state that no plan "
                                      + "describes.")
                               : qsTr("No failed upload has left the mission indeterminate.")
                    }
                    basis: qsTr("AeroraUi.mission.vehicleMissionIndeterminate. A failed upload is not rolled "
                                + "back and the vehicle may hold part of the new mission, so neither plan "
                                + "describes it. Cleared only by a completed download or a successful upload.")
                }

                // ------------------------------------------------------------ Transfers

                QGCLabel {
                    text: qsTr("Transfers")
                    font.bold: true
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Transfer activity")
                    unchecked: !root._attached
                    statusText: {
                        if (root._transferInProgress) {
                            // Upstream keeps the transaction kind in a protected member and
                            // exposes only a boolean, so the kind genuinely is not observable
                            // while it runs. Saying so beats guessing.
                            return qsTr("A transfer is in flight. Its kind is not observable while it runs.")
                        }
                        if (root._transferIdle) {
                            return qsTr("Idle. A transfer was observed to finish and none has started since.")
                        }
                        return qsTr("Not evidenced. No transfer has been observed to start or finish.")
                    }
                    basis: root._observerBasis
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Last operation")
                    unchecked: !root._attached
                    statusText: {
                        if (!root._attached) {
                            return ""
                        }
                        switch (root._mission.lastOperation) {
                        case AeroraMissionObserver.Download:
                            return qsTr("Download.")
                        case AeroraMissionObserver.Upload:
                            return qsTr("Upload.")
                        case AeroraMissionObserver.RemoveAll:
                            return qsTr("Remove all.")
                        default:
                            return qsTr("Not evidenced. No transfer has completed.")
                        }
                    }
                    basis: root._observerBasis
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Last outcome")
                    unchecked: !root._attached
                    warning: root._lastFailed
                    statusText: {
                        if (root._lastSucceeded) {
                            return qsTr("Succeeded.")
                        }
                        if (root._lastFailed) {
                            return qsTr("Failed.")
                        }
                        return qsTr("Not evidenced. No transfer has completed.")
                    }
                    // The download caveat is not a detail: upstream emits the same completion
                    // signal for a failed download and a genuinely empty mission, with no error
                    // flag, so the error correlation is the best evidence available.
                    basis: qsTr("AeroraUi.mission.lastOutcome. For a download, a failure is only distinguishable "
                                + "when an error was also reported during the transaction; otherwise an empty "
                                + "result is reported as success.")
                }

                // ------------------------------------------------------------ Ground-station plan

                QGCLabel {
                    text: qsTr("Ground-station plan")
                    font.bold: true
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Plan captured")
                    unchecked: !root._attached
                    warning: root._attached && !root._mission.gcsPlan.captured
                    statusText: {
                        if (!root._attached) {
                            return ""
                        }
                        const plan = root._mission.gcsPlan
                        return plan.captured
                               ? qsTr("%1 item(s), flattened as an upload would flatten them.").arg(plan.itemCount)
                               : qsTr("Not captured. The plan side has not been reported to the observer.")
                    }
                    basis: qsTr("AeroraUi.mission.gcsPlan, reported by AeroraPlanBridge from the Plan view's "
                                + "controller and flattened by the upload path's own converter, so it matches "
                                + "what an upload would send. Uncaptured means no vehicle carries an Aerora "
                                + "mission observer, not that the plan is empty.")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Plan/vehicle agreement")
                    unchecked: !root._attached
                    // A warning in both directions, which is the unusual part. False is plainly a
                    // gap. True is incomplete rather than reassuring: a match whose depth cannot
                    // be read is not verification, so neither reading may be presented as one.
                    warning: true
                    statusText: {
                        if (!root._attached) {
                            return ""
                        }
                        return root._agreementMatched
                               ? qsTr("A comparison matched, but the depth it matched at cannot be read here, so "
                                      + "this is not presented as verification.")
                               : qsTr("Not verified. No comparison has matched.")
                    }
                    basis: qsTr("AeroraUi.mission.agreement.isVerified(), the one member of AeroraMissionAgreement "
                                + "reachable without QML value-type registration. True for Matches alone, so "
                                + "NotCompared and Differs both read as not verified and cannot be told apart "
                                + "here. Nothing in this build calls verify() and the plan side is never "
                                + "captured, so NotCompared is the state in every session.")
                }

                // ------------------------------------------------------------ Scope of this screen

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("This screen observes mission state. It carries no control: nothing on it uploads, "
                               + "downloads, clears, resumes or sets a mission item, and mission commanding "
                               + "remains with the stock QGroundControl mission interface.")
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

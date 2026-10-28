import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.AnalyzeView

// AnalyzePage is imported at file level rather than left to the module's IMPORTS list, for the
// reason recorded on AeroraOperationsHub.qml: Qt documents IMPORTS in the consumer direction only.
//
// AeroraProvenance, AeroraSecurity and AeroraUi need no import of their own - they register into
// the application target's module (URI QGC) and `import QGroundControl` reaches it.
// AeroraSelfReportRow needs none either: the resource alias puts every AERORA_UI_QML_FILES entry in
// one flat module directory.

/// `SCR-SHL-04` - what this build is, stated by the build itself.
///
/// \section why Why a station has to be able to answer this
///
/// `P7-HELP-004` asks the station to state "base commit, overlay commit, profile set, enforcement
/// state". The reason is operational rather than tidy: when a report comes back from the field, the
/// first question is which build produced it, and a station that cannot answer makes every other
/// answer unverifiable. It is also the field `SCR-HLP-03`'s report used to carry as *not recorded*.
///
/// \section fields What each of the four fields is, and which one is absent
///
/// \li **Commit.** One hash, and in this repository it is both the base and the overlay commit: the
///     overlay lives in the same git tree as the fork of QGroundControl it extends. Reported with
///     the branch and with a dirty-working-tree qualifier, because a build made from uncommitted
///     changes is not described by its hash.
/// \li **Upstream base commit.** Genuinely absent. No upstream remote, merge-base or vendored
///     revision is recorded in the build, so there is nothing to read.
///     `AeroraProvenance.upstreamBaseCommitKnown` is a compile-time `false` and this screen renders
///     *not recorded* rather than offering the fork's own commit in its place - which is the one
///     mistake a provenance screen must not make.
/// \li **Profile set.** `AeroraUi.productProfileId`, an opaque build-supplied identifier. Empty is
///     the intended default and means no product surface is shown at all.
/// \li **Enforcement state.** `AeroraSecurity.enforcement`. Reported here as a one-line fact;
///     `SCR-SEC-01` is where its scope is laid out.
///
/// \section readonly Read-only
///
/// Every value is a preprocessor constant or a read of an existing projection. `AeroraProvenance`
/// has no `Q_INVOKABLE`, no public slot and no data member, so there is nothing on it to call;
/// `AeroraUiSafetyTest` holds its reviewed-method list at empty.
AnalyzePage {
    id: root

    pageComponent: pageComponent
    pageDescription: qsTr("Which build of the station this is. Every row states where the value came from, "
                          + "and the one field the build does not record says so.")
    allowPopout: true

    // Test hook. Without a name on the root a scene test cannot confirm the Analyze entry loaded
    // this page rather than silently loading nothing.
    objectName: "aeroraProvenancePage"

    readonly property bool _servicesAvailable: !!AeroraUi && AeroraUi.available

    /// See the enum note on AeroraSecurityPosturePage: matched against the enforced value and
    /// negated, so a resolution failure reports *not enforced* rather than claiming enforcement.
    readonly property bool _notEnforced: AeroraSecurity.enforcement !== AeroraSecurity.EnforcedAtCommandGate

    /// Mirrors `AeroraProvenance.commitKnown` so a scene test can compare the QML singleton against
    /// a C++ instance of the same type.
    ///
    /// That comparison is the only way to catch the failure that matters here: if the singleton
    /// registration did not happen, every binding on this page evaluates against `undefined`, the
    /// rows render as unchecked, and the page looks like a build with no recorded provenance rather
    /// than a page that cannot read it.
    readonly property bool _commitKnown: AeroraProvenance.commitKnown

    Component {
        id: pageComponent

        QGCFlickable {
            objectName: "aeroraProvenance_flickable"

            width: root.availableWidth
            height: root.availableHeight
            contentHeight: provenanceColumn.height
            flickableDirection: Flickable.VerticalFlick

            ColumnLayout {
                id: provenanceColumn

                width: root.availableWidth
                spacing: ScreenTools.defaultFontPixelHeight

                // ------------------------------------------------------------ Source

                QGCLabel {
                    text: qsTr("Source")
                    font.bold: true
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Commit")
                    // Unchecked rather than a warning when there is no hash: the build was made
                    // outside a checkout, which is a different fact from a build whose provenance
                    // is wrong.
                    unchecked: !AeroraProvenance.commitKnown
                    statusText: AeroraProvenance.commitKnown ? AeroraProvenance.commit : ""
                    basis: qsTr("Captured at configure time from the git checkout. One hash describes both "
                                + "the base and the overlay: they are the same tree.")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Working tree")
                    // A dirty tree is a warning, because the commit above then does not describe
                    // this binary - which is precisely the case where provenance matters.
                    warning: AeroraProvenance.workingTreeDirty
                    statusText: AeroraProvenance.workingTreeDirty
                                ? qsTr("Had uncommitted changes when this build was configured, so the commit "
                                       + "above does not fully describe it.")
                                : qsTr("Clean. The commit above describes this build.")
                    basis: qsTr("AeroraProvenance.workingTreeDirty, from the same configure-time check that "
                                + "the build summary prints.")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Branch")
                    unchecked: AeroraProvenance.branch === ""
                    statusText: AeroraProvenance.branch
                    basis: qsTr("The branch checked out when this build was configured. Informational: a "
                                + "branch name is not a version.")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Version")
                    statusText: AeroraProvenance.version
                    basis: qsTr("As git describe produced it: the last release tag, the commits since it, and "
                                + "the short hash.")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Upstream base commit")
                    // A warning rather than unchecked: this is a recorded gap in what the build
                    // captures, not a check that happened to find nothing.
                    warning: !AeroraProvenance.upstreamBaseCommitKnown
                    statusText: AeroraProvenance.upstreamBaseCommitKnown
                                ? qsTr("Recorded.")
                                : qsTr("Not recorded. The commit above is this fork's own and is NOT shown "
                                       + "here in its place.")
                    basis: qsTr("No upstream remote, merge-base or vendored revision is captured by the "
                                + "build, so there is nothing to read. P7-HELP-004 asks for it and this is "
                                + "the part that is still open.")
                }

                // ------------------------------------------------------------ Configuration

                QGCLabel {
                    text: qsTr("Configuration")
                    font.bold: true
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Product profile")
                    unchecked: !root._servicesAvailable
                    statusText: {
                        if (!root._servicesAvailable) {
                            return ""
                        }
                        return AeroraUi.productProfileId === ""
                               ? qsTr("None resolved. No product surface is shown, which is the intended "
                                      + "default rather than a fallback.")
                               : AeroraUi.productProfileId
                    }
                    basis: qsTr("AeroraUi.productProfileId, an opaque build-supplied identifier matched "
                                + "against a registry and never parsed.")
                }

                AeroraSelfReportRow {
                    Layout.fillWidth: true
                    label: qsTr("Command authorization")
                    warning: root._notEnforced
                    statusText: root._notEnforced
                                ? qsTr("Not enforced in this build.")
                                : qsTr("Enforced at the queued MAV_CMD gate.")
                    basis: qsTr("AeroraSecurity.enforcement. The Aerora Security screen is where the scope "
                                + "of that gate is set out, path by path.")
                }

                // ------------------------------------------------------------ Scope of this screen

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("This screen reports how the station was built. It carries no control: nothing "
                               + "on it changes a setting, a profile or a policy.")
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

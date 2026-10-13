import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// One line of the overlay's self-report: what was checked, and what the check found.
///
/// This control exists to make the `SCR-SHL-01` anti-requirement structural. A hub screen's
/// default shape is a grid of tiles that are green when a value is truthy, and the failure mode
/// that produces is a tile that reads as healthy because nothing was ever evaluated. So this row
/// cannot be constructed without saying what the verdict rests on, and it carries three outcomes
/// rather than two.
///
/// | Outcome      | Set by                    | Reads as                                      |
/// | ------------ | ------------------------- | --------------------------------------------- |
/// | satisfied    | neither flag              | Plain text. The check ran and found the thing  |
/// | `warning`    | `warning: true`           | Warning text. The check ran and found a gap    |
/// | `unchecked`  | `unchecked: true`         | Grey, and the word is "not checked"            |
///
/// `unchecked` is the one that earns the control its place. "No Aerora vehicle is connected, so
/// the per-vehicle service map was never consulted" is a different fact from "the service map was
/// consulted and the payload service is absent", and a two-state tile collapses them into the
/// same amber square. `unchecked` outranks `warning`: a check that did not run has no finding to
/// report, so it must not borrow one.
///
/// No colour or pixel literal appears here - `QGCPalette` roles and `ScreenTools` metrics only,
/// because the Aerora visual language is unresolved (`Q-040`).
RowLayout {
    id: root

    spacing: ScreenTools.defaultFontPixelWidth * 2

    /// What is being reported on. Rendered as the row's label.
    property string label: ""

    /// The finding, already rendered to text by the caller.
    property string statusText: ""

    /// What the verdict rests on. Required in practice: a row whose basis is empty is reporting a
    /// conclusion with no stated evidence, which is the thing this control is for.
    property string basis: ""

    /// The check ran and found a gap.
    property bool warning: false

    /// The check did not run. Outranks `warning`, and replaces `statusText` entirely - a check
    /// that did not run cannot also have a finding.
    property bool unchecked: false

    readonly property string _renderedStatus: root.unchecked ? qsTr("not checked") : root.statusText

    QGCLabel {
        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 28
        text: root.label
        color: qgcPal.text
        wrapMode: Text.WordWrap
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 0

        QGCLabel {
            Layout.fillWidth: true
            // Never an empty string: an empty status reads as "fine" to an operator scanning a
            // column, which is the same defect as a green tile.
            text: root._renderedStatus === "" ? qsTr("no status reported") : root._renderedStatus
            color: {
                if (root.unchecked) {
                    return qgcPal.colorGrey
                }
                if (root.warning) {
                    return qgcPal.warningText
                }
                return qgcPal.text
            }
            wrapMode: Text.WordWrap
        }

        QGCLabel {
            Layout.fillWidth: true
            visible: root.basis !== ""
            text: root.basis
            color: qgcPal.colorGrey
            font.pointSize: ScreenTools.smallFontPointSize
            wrapMode: Text.WordWrap
        }
    }

    QGCPalette {
        id: qgcPal
        colorGroupEnabled: enabled
    }
}

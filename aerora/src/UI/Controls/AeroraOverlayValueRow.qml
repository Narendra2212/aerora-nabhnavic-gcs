import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// One compact label-and-value line for a Fly View overlay panel.
///
/// \section why Why not AeroraSelfReportRow
///
/// `AeroraSelfReportRow` cannot be constructed without stating what its verdict rests on, which is
/// the right constraint for a screen and the wrong one for a flight overlay: three basis paragraphs
/// over a moving map are three paragraphs nobody reads. This control carries the readings only, and
/// the panel that uses it names where the basis lives.
///
/// What it does keep is the distinction that matters. `unknown` is a separate state from a value,
/// and it outranks `caution` - a reading the station does not have cannot also carry a finding. A
/// two-state row would render "nothing observed" with the same weight as an observed value, which
/// is the defect this control exists to prevent at a glance.
///
/// No colour or pixel literal appears here: `QGCPalette` roles and `ScreenTools` metrics only,
/// because the Aerora visual language is unresolved (`Q-040`).
RowLayout {
    id: root

    /// What is being reported. Rendered as the row's label.
    property string label: ""

    /// The reading, already rendered to text by the caller.
    property string valueText: ""

    /// The station does not have this reading. Outranks `caution`.
    property bool unknown: false

    /// The reading is present and warrants attention.
    property bool caution: false

    spacing: ScreenTools.defaultFontPixelWidth

    QGCLabel {
        text: root.label
        color: qgcPal.colorGrey
        font.pointSize: ScreenTools.smallFontPointSize
        Layout.alignment: Qt.AlignBaseline
    }

    QGCLabel {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignBaseline
        horizontalAlignment: Text.AlignRight
        // Never an empty string: an empty value reads as "fine" to an operator scanning a column.
        text: root.valueText === "" ? qsTr("Unknown") : root.valueText
        color: {
            if (root.unknown) {
                return qgcPal.colorGrey
            }
            if (root.caution) {
                return qgcPal.warningText
            }
            return qgcPal.text
        }
        elide: Text.ElideRight
    }

    QGCPalette {
        id: qgcPal
        colorGroupEnabled: enabled
    }
}

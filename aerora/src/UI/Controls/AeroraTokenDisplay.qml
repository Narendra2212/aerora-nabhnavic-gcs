import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// Displays one AeroraReportedValue: its name, its reported token, its class label and its age.
///
/// AeroraReportedValue is the primitive the payload and power models use for every item whose
/// value has no evidenced representation yet. The token is whatever the vehicle reported, carried
/// verbatim as text.
///
/// \section raw The token is rendered raw
///
/// It is not parsed, compared against a known set, mapped to a meaning, ranged or defaulted. The
/// payload and power content enumerations are `UNKNOWN` (Q-023, Q-024), so an unrecognised value
/// is shown as it arrived — which is the documented isolation strategy for an unknown enumeration,
/// and the only honest option while no dialect exists.
///
/// \section composition What this adds over AeroraStateText
///
/// The class label. A REQUESTED, PROPOSED or STALE value must be *labelled*, not merely coloured,
/// and this control is where the label is guaranteed to be rendered.
RowLayout {
    id: root

    /// The AeroraReportedValue to display. Null is a normal state: no vehicle, no Aerora services,
    /// or the service is not attached.
    property var reportedValue: null

    /// Operator-facing name for the item. Supplied by the caller, because
    /// AeroraReportedValue.itemId is a machine-readable identifier for logs and records and is
    /// explicitly not a label.
    property string label: ""

    spacing: ScreenTools.defaultFontPixelWidth

    // An absent value is an absent element, matching how UNAVAILABLE is treated. A caller that
    // wants "no payload attached" stated in words renders that itself; this control does not
    // invent the sentence.
    visible: root.reportedValue !== null

    QGCLabel {
        text: root.label
        visible: root.label !== ""
        color: qgcPal.colorGrey
        font.pointSize: ScreenTools.smallFontPointSize
        Layout.alignment: Qt.AlignBaseline
    }

    AeroraStateText {
        id: stateText
        stateClass: root.reportedValue ? root.reportedValue.stateClass : AeroraTelemetryState.Unknown
        valueText: root.reportedValue ? root.reportedValue.reportedToken : ""
        Layout.alignment: Qt.AlignBaseline
    }

    QGCLabel {
        text: stateText.classLabel
        visible: stateText.classLabel !== ""
        color: qgcPal.colorGrey
        font.pointSize: ScreenTools.smallFontPointSize
        Layout.alignment: Qt.AlignBaseline
    }

    AeroraAgeLabel {
        reportedValue: root.reportedValue
        Layout.alignment: Qt.AlignBaseline
    }

    QGCPalette {
        id: qgcPal
        colorGroupEnabled: enabled
    }
}

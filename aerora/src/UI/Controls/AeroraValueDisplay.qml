import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// Displays one AeroraStateValue: its Fact's value, its class label and its age.
///
/// The Fact-backed counterpart of AeroraTokenDisplay. Units, bounds and enumeration members come
/// from Fact metadata rather than from this file, which is why the value text is
/// `fact.enumOrValueString` rather than anything computed here.
///
/// \section nodata There is no Aerora Fact to bind yet
///
/// All six Aerora fact groups are `CONTRACT-ONLY` for want of a dialect, and
/// AeroraVehicleFactGroupRegistrar cannot place a group into a vehicle's map until the Q4-002 core
/// edit is applied. So no instance of AeroraStateValue exists in this build and this control has
/// nothing to show in the running application.
///
/// It is written now because it is the artefact that is expensive to retrofit: the state-class
/// contract has to be right before panels are built on it, and the control is testable against a
/// test double without a dialect, a vehicle or a link.
///
/// \section readonly Read-only
///
/// The Fact is read through `enumOrValueString` and `units` only. `Fact` does carry writable
/// `value` / `rawValue` properties — that is upstream QGroundControl, and how parameter editing
/// works — and nothing in this control or any Aerora UI control writes them.
RowLayout {
    id: root

    /// The AeroraStateValue to display. Null is a normal state.
    property var stateValue: null

    /// Operator-facing name for the item. Defaults to the Fact's own short description, which
    /// comes from metadata; a caller may override it.
    property string label: root._fact ? root._fact.shortDescription : ""

    /// Whether to append the Fact's metadata-supplied units.
    property bool showUnits: true

    readonly property var _fact: root.stateValue ? root.stateValue.fact : null

    spacing: ScreenTools.defaultFontPixelWidth

    visible: root.stateValue !== null

    QGCLabel {
        text: root.label
        visible: root.label !== ""
        color: qgcPal.colorGrey
        font.pointSize: ScreenTools.smallFontPointSize
        Layout.alignment: Qt.AlignBaseline
    }

    AeroraStateText {
        id: stateText
        stateClass: root.stateValue ? root.stateValue.stateClass : AeroraTelemetryState.Unknown
        valueText: root._fact ? root._fact.enumOrValueString : ""
        Layout.alignment: Qt.AlignBaseline
    }

    QGCLabel {
        text: root._fact ? root._fact.units : ""
        // Units belong to the value, so they are hidden whenever the value is not shown - an
        // UNKNOWN reading followed by a unit would read as a measurement.
        visible: root.showUnits && stateText.visible && (text !== "")
                 && AeroraTelemetryState.carriesValue(stateText.stateClass)
        color: qgcPal.colorGrey
        font.pointSize: ScreenTools.smallFontPointSize
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
        reportedValue: root.stateValue
        Layout.alignment: Qt.AlignBaseline
    }

    QGCPalette {
        id: qgcPal
        colorGroupEnabled: enabled
    }
}

import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// The in-flight mission readout for `SCR-LIV-01`: where the aircraft is in its mission, and what
/// the ground station is still unsure of.
///
/// \section why Why this is not the Analyze page
///
/// `AeroraMissionProgressPage` (`SCR-LIV-06`) renders the same observer with a stated basis under
/// every row, which is right for a screen an operator opens to answer a question. A flight overlay
/// is read at a glance while the aircraft is moving, so this control renders the readings and
/// nothing else, and names the Analyze page as the place the basis lives.
///
/// What it does **not** do is soften an unknown into a plausible number. Every reading has an
/// explicit unknown rendering, because the one failure that matters here is a panel that shows a
/// confident mission item the station never actually observed.
///
/// \section readonly Read-only
///
/// No control on this panel is clickable. The resume candidate is deliberately absent: it is an
/// observation, and a resume proposal rendered next to a live mission reads as an offer to act on
/// it. `verify()` is never called, so no agreement verdict appears here either.
QGCGroupBox {
    id: root

    /// The active vehicle's `AeroraMissionObserver`, or null. Null is a normal, displayable state:
    /// no vehicle, a vehicle that is not an Aerora composition, or the observer not attached.
    property var mission: null

    title: qsTr("Aerora mission")

    /// Positive match only, the rule every Aerora enum comparison follows. If the enumerator
    /// stopped resolving, `undefined === value` is false and the row reads as idle rather than
    /// asserting a transfer nobody observed.
    readonly property bool _transferInProgress: !!root.mission
                                                && root.mission.transferActivity === AeroraMissionObserver.InProgress

    /// The vehicle's own current item. `AeroraMissionIndex` carries `known` separately from
    /// `sequenceNumber` precisely so that "item 0" and "nothing observed" stay distinguishable.
    readonly property var _vehicleIndex: root.mission ? root.mission.vehicleCurrentIndex : null

    readonly property var _gcsPlan: root.mission ? root.mission.gcsPlan : null

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // The state that makes every row below meaningful or not. Stated in words rather than left
        // to be inferred from three unknowns in a row.
        QGCLabel {
            Layout.fillWidth: true
            visible: !root.mission
            text: qsTr("No mission observer attached.")
            color: qgcPal.warningText
            font.pointSize: ScreenTools.smallFontPointSize
            wrapMode: Text.WordWrap
        }

        AeroraOverlayValueRow {
            Layout.fillWidth: true
            visible: !!root.mission
            label: qsTr("Vehicle item")
            valueText: {
                if (!root._vehicleIndex || !root._vehicleIndex.known) {
                    return qsTr("Unknown")
                }
                return root._vehicleIndex.sequenceNumber.toString()
            }
            // The aircraft reported its own position in the mission, rather than the station
            // having inferred it. A reading the station derived must not read as the aircraft's.
            unknown: !root._vehicleIndex || !root._vehicleIndex.known
            caution: !!root._vehicleIndex && root._vehicleIndex.known
                     && !root._vehicleIndex.vehicleAuthoritative
        }

        AeroraOverlayValueRow {
            Layout.fillWidth: true
            visible: !!root.mission
            label: qsTr("Plan held")
            valueText: {
                if (!root._gcsPlan || !root._gcsPlan.captured) {
                    return qsTr("None captured")
                }
                return qsTr("%1 items").arg(root._gcsPlan.itemCount)
            }
            unknown: !root._gcsPlan || !root._gcsPlan.captured
        }

        AeroraOverlayValueRow {
            Layout.fillWidth: true
            visible: !!root.mission
            label: qsTr("Transfer")
            valueText: root._transferInProgress ? qsTr("In progress") : qsTr("Idle")
            caution: root._transferInProgress
        }

        QGCLabel {
            Layout.fillWidth: true
            Layout.topMargin: ScreenTools.defaultFontPixelHeight * 0.25
            visible: !!root.mission
            text: qsTr("Basis for each reading, and plan agreement, are on the Aerora Mission Progress page "
                       + "under Analyze.")
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

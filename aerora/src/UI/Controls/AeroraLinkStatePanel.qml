import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// `SCR-LIV-07` - one line per link, never one aggregated bar.
///
/// \section why The requirement this panel is shaped by
///
/// `P7-RAD-002`: link state, quality and freshness are displayed **per link, not aggregated into
/// one bar**. A single "link quality" indicator is the standard shape and it hides the thing an
/// operator needs during a handover or a dropout - *which* link is carrying the aircraft. So the
/// rows below are per link, from the vehicle's own link manager.
///
/// Where the station genuinely has no per-link figure, it says so rather than dividing an aggregate
/// by the link count. Message loss is counted per vehicle, not per link
/// (`Vehicle::mavlinkLossPercent()`), so it is labelled as covering all links together. Presenting
/// it against a single link would be inventing a measurement.
///
/// \section empty The upstream encoding that would otherwise read backwards
///
/// `VehicleLinkManager::linkStatuses()` returns `tr("Comm Lost")` for a link in loss and **an empty
/// string** for a healthy one (`VehicleLinkManager.cc:407-416`). Passed straight into
/// `AeroraOverlayValueRow`, an empty value renders as *Unknown* by that control's never-blank rule
/// - so a link that is working perfectly would be shown as a link the station knows nothing about.
///
/// The panel therefore reads the emptiness, not the words: an empty status means the link manager
/// is reporting and has no loss to report. That test is a comparison against `""` rather than
/// against "Comm Lost", so it does not break when the string is translated - the same reason
/// `AeroraHealthPage` renders its sensor states verbatim instead of matching them.
///
/// \section radio What this panel does not claim
///
/// No Aerora radio has been attached to anything (`D-004`), and `P7-RAD-003` makes every Aerora
/// radio capability designed rather than supported. These readings are QGroundControl's own link
/// layer, which is reused unchanged (`P7-RAD-001`); nothing here reports on an Aerora radio,
/// because there is nothing to report. `P7-RAD-004` asks that a designed-but-unverified interface
/// be labelled as such in the UI, and the footer is that label.
///
/// \section readonly Read-only
///
/// `VehicleLinkManager::primaryLinkName` has a `WRITE` accessor, so QML *could* switch the
/// aircraft's primary link. Nothing here does: the primary link is read to mark a row and is never
/// assigned. The overlay carries no actionable control at all, which
/// `AeroraFlyViewOverlayUITest::_overlayCarriesNoActionableControl` asserts structurally over the
/// whole subtree rather than by review of this file.
QGCGroupBox {
    id: root

    objectName: "aeroraLinkStatePanel"

    /// The active vehicle, or null. Null is a normal, displayable state for most of the station's
    /// life, and the panel says so rather than rendering empty rows.
    property var vehicle: null

    title: qsTr("Links")

    /// The vehicle's link manager, or null. Guarded separately from `vehicle` so a vehicle without
    /// one reads as unknown rather than throwing.
    readonly property var _links: root.vehicle && root.vehicle.vehicleLinkManager
                                  ? root.vehicle.vehicleLinkManager
                                  : null

    /// Parallel lists: index i of `_linkStatuses` describes index i of `_linkNames`. Upstream
    /// builds both from one vector in one pass.
    readonly property var _linkNames: root._links ? root._links.linkNames : []
    readonly property var _linkStatuses: root._links ? root._links.linkStatuses : []

    /// The link the vehicle is addressed through. Read to mark a row; never assigned.
    readonly property string _primaryLinkName: root._links ? root._links.primaryLinkName : ""

    /// Total loss of communication across every link, which is a different fact from one link
    /// being down and is reported separately for that reason.
    readonly property bool _communicationLost: !!root._links && root._links.communicationLost

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // The state that makes every row below meaningful or not. Stated in words rather than left
        // to be inferred from a column of unknowns.
        QGCLabel {
            Layout.fillWidth: true
            visible: !root._links
            text: qsTr("No vehicle link manager attached.")
            color: qgcPal.warningText
            font.pointSize: ScreenTools.smallFontPointSize
            wrapMode: Text.WordWrap
        }

        // A link manager with no links. Distinct from having no manager, and distinct from having
        // links that are all down.
        QGCLabel {
            Layout.fillWidth: true
            visible: !!root._links && (root._linkNames.length === 0)
            text: qsTr("No link to this vehicle.")
            color: qgcPal.warningText
            font.pointSize: ScreenTools.smallFontPointSize
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: root._linkNames

            // One row per link. required properties, so a model change cannot leave a delegate
            // bound to a stale index.
            AeroraOverlayValueRow {
                id: linkRow

                required property string modelData
                required property int index

                /// Upstream reports loss by putting a string here and health by leaving it empty.
                /// See \section empty on why this is read as emptiness rather than matched.
                readonly property string status: root._linkStatuses.length > linkRow.index
                                                 ? root._linkStatuses[linkRow.index]
                                                 : ""

                readonly property bool isPrimary: linkRow.modelData === root._primaryLinkName

                Layout.fillWidth: true
                // The primary link is marked rather than sorted first: the order is the vehicle's
                // own, and reordering would make a row move when the primary link changed.
                label: linkRow.isPrimary ? qsTr("%1 (primary)").arg(linkRow.modelData) : linkRow.modelData
                valueText: linkRow.status === "" ? qsTr("Connected") : linkRow.status
                caution: linkRow.status !== ""
            }
        }

        AeroraOverlayValueRow {
            Layout.fillWidth: true
            visible: !!root._links
            label: qsTr("All links")
            valueText: root._communicationLost ? qsTr("Communication lost") : qsTr("At least one carrying")
            caution: root._communicationLost
        }

        AeroraOverlayValueRow {
            Layout.fillWidth: true
            visible: !!root.vehicle
            // Named as covering every link, because it does. See \section why on why it is not
            // divided across the rows above.
            label: qsTr("Message loss, all links")
            valueText: qsTr("%1%").arg(root.vehicle ? root.vehicle.mavlinkLossPercent.toFixed(1) : "")
            // No caution threshold: no loss budget has been approved (Q-036 is the freshness
            // equivalent), and a threshold invented here would be a verdict the product has not
            // defined. The figure is shown; the judgement is not made.
        }

        QGCLabel {
            Layout.fillWidth: true
            text: qsTr("Link readings are QGroundControl's own. No Aerora radio is attached and none is "
                       + "supported; per-link quality is not reported by the link layer.")
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

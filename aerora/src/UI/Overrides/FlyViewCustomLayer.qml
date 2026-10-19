import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

import Aerora.Controls

// `import Aerora.Controls` is required here and is not required by the Aerora screens. Those live
// inside the module directory, so the implicit import covers them; this file is registered under
// the /Aerora override prefix with its stock nested path preserved, so it is outside that
// directory and must import the module by URI. The URI resolves because the base
// QGCCorePlugin::createQmlApplicationEngine() adds qrc:/qml as an import path and the module sits
// at qrc:/qml/Aerora/Controls.

/// `SCR-LIV-01` - the Aerora Live Operations overlay: the one Aerora surface an operator sees
/// while the aircraft is flying.
///
/// \section seam What this file replaces
///
/// This is the overlay's mirrored copy of `src/FlyView/FlyViewCustomLayer.qml`, reached through the
/// `AeroraQmlOverrides` allow-list. Upstream ships that file as an empty, purpose-built seam - four
/// properties and no content - and documents it as the override point for a custom overlay, so this
/// is a `Tier::PurposeBuiltSeam` entry and forks nothing populated.
///
/// The three seam properties keep their upstream names and shapes exactly, because `FlyView.qml`
/// binds them by name:
///
/// \code
/// FlyView.qml:145  FlyViewCustomLayer { occluders: _layerOccluders; mapControl: _mapControl }
/// FlyView.qml:49   _mapOccluders: _layerOccluders.all.concat(customOverlay.customOccluders)
/// \endcode
///
/// Renaming or dropping one would not fail to compile - it would silently stop the map from
/// keeping the vehicle clear of this overlay.
///
/// \section occluders How panel geometry reaches the map
///
/// `customOccluders` is published for the **container**, not per panel. One rect that grows with
/// its contents cannot fall out of step with the panels inside it, which is the failure a
/// per-panel list invites. It is published only while the container is visible and has a non-zero
/// size, matching how `FlyViewOccluders` filters its own rects, so a hidden overlay contributes no
/// keep-out area.
///
/// The rects are in this item's coordinates, which are the widget layer's: `FlyView.qml` anchors
/// this layer to `widgetLayer` and then offsets every rect by `widgetLayer.x/y` itself.
///
/// \section place Where the overlay sits, and what it stays clear of
///
/// Immediately right of the upstream tool strip, at the tool strip's top. That keeps it clear of
/// the top-right instrument panel, the bottom-right value column and pip view, the bottom-left
/// virtual joystick, and - because the width is bounded - the view centre, which the map recenters
/// on and which the upstream seam comment warns against covering.
///
/// A hidden tool strip collapses to `Qt.rect(0, 0, 0, 0)` (`FlyViewWidgetLayer.qml:42`), so the
/// overlay then sits at the layer origin, which is where the tool strip would have been.
///
/// \section degrade What happens with no vehicle
///
/// The overlay hides itself entirely. With no active vehicle, or a vehicle that is not an Aerora
/// composition, there is nothing Aerora to report and an empty frame over the map would be worse
/// than no frame: it would occupy keep-out area for no content. `customOccluders` is then empty and
/// the Fly View is pixel-identical to upstream.
///
/// \section readonly Read-only, structurally
///
/// Nothing here is clickable and nothing sends. Every binding is a read of `AeroraUi.mission`,
/// whose observer has no command API - nothing uploads, downloads, clears, resumes or sets a
/// mission item through it - and `AeroraUiSafetyTest` holds the writable-property set across every
/// QML-registered Aerora type at zero. The resume candidate is deliberately **not** shown: it is an
/// observation, and presenting it in a flight overlay would read as an offer to resume.
///
/// `verify()` is not called. The agreement verdict stays on the Analyze page (`SCR-LIV-06`), which
/// is where the basis for each reading is rendered in full; a flight overlay is the wrong place for
/// a verdict nothing has computed.
Item {
    id: root

    // ---------------------------------------------------------------------- Upstream seam contract

    /// FlyViewOccluders: where the upstream widgets are. Used for positioning only.
    property var occluders

    /// Rects of this overlay's controls which cover the map, so the map keeps the vehicle out from
    /// under them. See \section occluders.
    property var customOccluders: (panelColumn.visible && (panelColumn.width > 0) && (panelColumn.height > 0))
                                  ? [Qt.rect(panelColumn.x, panelColumn.y, panelColumn.width, panelColumn.height)]
                                  : []

    /// The Fly View map. Unused so far: no Aerora overlay draws on the map, and `CustomMapItems.qml`
    /// is on the override seam's forbidden list because `QGCCorePlugin::customMapItems()` is the
    /// supported route for a map item.
    property var mapControl

    // Test hook. Without a name on the root, a scene test cannot tell an overlay that loaded from
    // one that silently did not.
    objectName: "aeroraLiveOperationsOverlay"

    // ---------------------------------------------------------------------- State

    /// Truthy test rather than a null comparison, so an unresolved property reads as unavailable.
    readonly property bool _available: !!AeroraUi && AeroraUi.available

    /// The active vehicle's mission observer, or null. Null is the normal case for most of the
    /// station's life.
    readonly property var _mission: root._available && AeroraUi.mission ? AeroraUi.mission : null

    readonly property real _margin: ScreenTools.defaultFontPixelWidth

    QGCPalette {
        id: qgcPal
        colorGroupEnabled: enabled
    }

    // ---------------------------------------------------------------------- Panels

    ColumnLayout {
        id: panelColumn

        // Guarded: `occluders` is supplied by FlyView.qml, but an overlay loaded without it must
        // position itself rather than bind to undefined.
        readonly property rect _toolStrip: root.occluders ? root.occluders.toolStrip : Qt.rect(0, 0, 0, 0)

        x: panelColumn._toolStrip.x + panelColumn._toolStrip.width + root._margin
        y: panelColumn._toolStrip.y
        width: ScreenTools.defaultFontPixelWidth * 28
        spacing: root._margin

        // No Aerora vehicle means no Aerora content. See \section degrade.
        visible: root._available

        AeroraMissionProgressPanel {
            Layout.fillWidth: true
            mission: root._mission
        }

        // Extension point. Later SCR-LIV-* panels are appended here and need no change to the
        // occluder plumbing above: the container rect grows with them. Each one is gated on its own
        // unblocking - SCR-LIV-02 on the fact groups (Q4-002), SCR-LIV-03 on the payload reported-
        // value set (P7-PAY-004), SCR-LIV-04 on the tether item set (P7-PWR-006), SCR-LIV-05 on the
        // proposal model and the operator role set (P7-AI-001, Q-029).
    }
}

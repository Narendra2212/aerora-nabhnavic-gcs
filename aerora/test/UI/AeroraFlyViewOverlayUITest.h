#pragma once

#include "QmlUITestBase.h"

/// Runtime coverage of `SCR-LIV-01`, the Aerora Live Operations overlay, in the real Fly View.
///
/// \section proof What finding the overlay proves
///
/// Upstream's `src/FlyView/FlyViewCustomLayer.qml` is an empty seam with no `objectName`. The
/// Aerora mirror sets one. So locating `aeroraLiveOperationsOverlay` in a booted scene is the
/// end-to-end proof of the whole override mechanism: the allow-list declared the stock path, the
/// interceptor rewrote the URL the engine asked for, the mirrored resource was found at the
/// `/Aerora` prefix, and `FlyView.qml` instantiated the result. No unit test can show that - every
/// one of them stops at a pure function or a bare engine.
///
/// \section occluders Why the occluder assertions are shaped the way they are
///
/// `customOccluders` is the overlay's only outbound effect on upstream behaviour:
/// `FlyView.qml:49` concatenates it into `_mapOccluders`, which both map engines consume to keep
/// the vehicle out from under covering widgets. Getting it wrong is silent - the map just drifts
/// the vehicle under the panel, or reserves keep-out area for a panel that is not there.
///
/// The assertions are written against the *property* rather than the positioning formula: the
/// published rect must be non-empty, must lie inside the overlay, and must not contain the
/// overlay's centre, because that is where the map recenters and the upstream seam comment warns
/// against covering it. Reproducing the arithmetic here would assert that the test and the overlay
/// agree, which they would by construction.
///
/// \section degrade The state the station is in for most of its life
///
/// No vehicle. The overlay must then publish **no** occluder at all, so the Fly View's keep-out set
/// is exactly upstream's and the view behaves as though Aerora were not installed. That is asserted
/// in both directions - empty without a vehicle, one rect with one - so a panel stuck visible and a
/// panel that never appears are distinct failures.
///
/// \section readonly The read-only boundary, asserted structurally
///
/// The seam declaration was approved for read-only display only. `_overlayCarriesNoActionableControl()`
/// walks the overlay's item tree for any `QQuickAbstractButton` or `QQuickMouseArea` and requires
/// none. That is a stronger guarantee than reviewing the QML, because it fails on a control added
/// through any route - a new panel, a nested control, an upstream type that happens to be clickable.
class AeroraFlyViewOverlayUITest : public QmlUITestBase
{
    Q_OBJECT

public:
    AeroraFlyViewOverlayUITest() = default;

protected slots:
    void init() override;

private slots:
    // The overlay is in the scene at all, with the upstream property contract intact and bound.
    void _flyViewLoadsTheAeroraOverlayWithItsSeamContractBound();

    // No vehicle: present, no content, and no keep-out area taken from the map.
    void _overlayPublishesNoOccluderWithoutAnAeroraVehicle();

    // With a vehicle: one rect, inside the overlay, clear of the view centre - and back to none
    // when the vehicle goes away.
    void _overlayPublishesItsPanelBoundsAndFollowsVisibility();

    // The read-only boundary the seam declaration was approved under.
    void _overlayCarriesNoActionableControl();

private:
    /// Locates the overlay root in the booted scene. Includes hidden items: the root is present
    /// even when its panels are not, and that distinction is the subject of two of these tests.
    ///
    /// @return The overlay root, or nullptr after recording a failure.
    QQuickItem* _overlay();

    /// Reads `customOccluders` off @a overlay as a rect list.
    [[nodiscard]] static QList<QRectF> _publishedOccluders(QQuickItem* overlay);
};

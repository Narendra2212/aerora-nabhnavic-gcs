#include "AeroraFlyViewOverlayUITest.h"

#include <QtCore/QList>
#include <QtCore/QRectF>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtCore/QVariantList>
#include <QtQuick/QQuickItem>
#include <QtTest/QTest>

#include "MockLink.h"
#include "Vehicle.h"

UT_REGISTER_TEST(AeroraFlyViewOverlayUITest, TestLabel::Integration, TestLabel::Vehicle)

namespace {

/// The overlay root's objectName, set by the Aerora mirror. Upstream's seam sets none, so this
/// string is what distinguishes "the override took effect" from "the stock empty seam loaded".
constexpr const char* kOverlayObjectName = "aeroraLiveOperationsOverlay";

/// Returns true if @a item or any descendant is something an operator could press.
///
/// QQuickAbstractButton covers every Qt Quick Controls button, which is what QGCButton and friends
/// derive from; QQuickMouseArea covers a hand-rolled click target. Searched by class name through
/// QObject::inherits() so a control added through any route is caught, rather than only the types
/// this test thought to name.
bool hasActionableControl(QQuickItem* item)
{
    if (item->inherits("QQuickAbstractButton") || item->inherits("QQuickMouseArea")) {
        return true;
    }

    const QList<QQuickItem*> children = item->childItems();
    for (QQuickItem* const child : children) {
        if (hasActionableControl(child)) {
            return true;
        }
    }

    return false;
}

}  // namespace

void AeroraFlyViewOverlayUITest::init()
{
    QmlUITestBase::init();

    // Emitted once per process by AeroraCorePlugin::init() and latched there, so only whichever
    // slot runs first could observe them - expecting them would be expecting something absent for
    // every later slot. Same set and reasoning as AeroraSceneAttachmentUITest::init().
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("no platform key protector available")));
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("command authorization is NOT being enforced")));
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("allowing commands without authorization")));

    // The Generic MockLink publishes no component metadata, so the request state machine gives up
    // once per connection. Environment noise from the mock.
    ignoreLogMessage("ComponentInformation.RequestMetaDataTypeStateMachine", QtWarningMsg,
                     QRegularExpression(QStringLiteral("failed to load metadata")));
}

QQuickItem* AeroraFlyViewOverlayUITest::_overlay()
{
    QQuickItem* const overlay = findItem(_rootItem, QString::fromUtf8(kOverlayObjectName));
    if (!overlay) {
        QTest::qFail("the Fly View did not load the Aerora overlay; the override did not take effect", __FILE__,
                     __LINE__);
        return nullptr;
    }

    return overlay;
}

QList<QRectF> AeroraFlyViewOverlayUITest::_publishedOccluders(QQuickItem* overlay)
{
    QList<QRectF> rects;

    const QVariant published = overlay->property("customOccluders");
    const QVariantList entries = published.toList();
    rects.reserve(entries.count());

    for (const QVariant& entry : entries) {
        rects.append(entry.toRectF());
    }

    return rects;
}

void AeroraFlyViewOverlayUITest::_flyViewLoadsTheAeroraOverlayWithItsSeamContractBound()
{
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    QQuickItem* const overlay = _overlay();
    QVERIFY(overlay);

    // The three upstream-facing properties. FlyView.qml binds them by name, so a rename would not
    // fail to compile - it would silently stop the map keeping the vehicle clear of the overlay.
    // Checked for existence first: an invalid QVariant converts to a null/false value, which would
    // make the binding checks below pass vacuously.
    for (const char* seamProperty : {"occluders", "customOccluders", "mapControl"}) {
        QVERIFY2(overlay->property(seamProperty).isValid(),
                 qPrintable(QStringLiteral("the overlay must keep the upstream seam property %1")
                                .arg(QString::fromUtf8(seamProperty))));
    }

    // And two of them are actually bound by the Fly View, not merely declared. `occluders` is what
    // the overlay positions against, so an unbound one puts the panels at the layer origin on top
    // of the tool strip.
    QVERIFY2(!overlay->property("occluders").isNull(), "FlyView.qml must bind occluders on the custom layer");
    QVERIFY2(!overlay->property("mapControl").isNull(), "FlyView.qml must bind mapControl on the custom layer");

    // The overlay fills the widget layer, so a zero-sized root means it was instantiated outside
    // the layout and nothing it contains could be positioned.
    QVERIFY2((overlay->width() > 0) && (overlay->height() > 0),
             "the overlay must be laid out with a real size in the Fly View");
}

void AeroraFlyViewOverlayUITest::_overlayPublishesNoOccluderWithoutAnAeroraVehicle()
{
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    QQuickItem* const overlay = _overlay();
    QVERIFY(overlay);

    // The state the station is in for most of its life. No active vehicle means no Aerora
    // services, so there is nothing to report and an empty frame over the map would be worse than
    // none: it would hold keep-out area for no content.
    QVERIFY2(_publishedOccluders(overlay).isEmpty(),
             "with no vehicle the overlay must take no keep-out area from the map");

    // Asserted as well as the rect list, because an empty list could also come from a visible
    // panel whose publication is broken. Both must say the same thing.
    QQuickItem* const panels = findItem(overlay, QStringLiteral("aeroraLiveOperationsPanels"));
    QVERIFY2(panels, "the overlay must contain its panel container even when hidden");
    QVERIFY2(!panels->isVisible(), "with no vehicle the overlay must render no panel");
}

void AeroraFlyViewOverlayUITest::_overlayPublishesItsPanelBoundsAndFollowsVisibility()
{
    runWithMockLink(
        [] { return MockLink::startGenericMockLink(); },
        [this](QPointer<MockLink> mockLink, Vehicle* vehicle) {
            QVERIFY(vehicle);

            QQuickItem* const overlay = _overlay();
            QVERIFY(overlay);

            QQuickItem* const panels = findItem(overlay, QStringLiteral("aeroraLiveOperationsPanels"));
            QVERIFY(panels);

            // Polled: the services attach from the firmware plugin during the vehicle
            // construction, but the binding chain through AeroraUi to this panel settles on a
            // later frame.
            QVERIFY2(UnitTest::waitForCondition([this, overlay]() { return _publishedOccluders(overlay).count() == 1; },
                                                TestTimeout::mediumMs(),
                                                QStringLiteral("the overlay to publish its bounds")),
                     "with an Aerora vehicle connected the overlay must publish exactly one rect");

            QVERIFY2(panels->isVisible(), "with an Aerora vehicle the overlay must render its panel");

            const QRectF rect = _publishedOccluders(overlay).first();

            // Non-empty, or the map is handed a degenerate keep-out area that reserves nothing.
            QVERIFY2((rect.width() > 0) && (rect.height() > 0),
                     qPrintable(QStringLiteral("published rect must be non-empty, got %1x%2")
                                    .arg(rect.width())
                                    .arg(rect.height())));

            // In the overlay's own coordinates, which are the widget layer's - FlyView.qml offsets
            // by widgetLayer.x/y itself, so a rect published in screen coordinates would be
            // double-offset and keep the vehicle out of the wrong region.
            const QRectF overlayBounds(0, 0, overlay->width(), overlay->height());
            QVERIFY2(overlayBounds.contains(rect),
                     qPrintable(QStringLiteral("published rect %1,%2 %3x%4 must lie inside the overlay %5x%6")
                                    .arg(rect.x())
                                    .arg(rect.y())
                                    .arg(rect.width())
                                    .arg(rect.height())
                                    .arg(overlay->width())
                                    .arg(overlay->height())));

            // The requirement the upstream seam comment states: the map recenters on the view
            // centre, so a control covering it fights the map for the vehicle's position.
            QVERIFY2(!rect.contains(overlayBounds.center()),
                     "the overlay must stay clear of the view centre, which the map recenters on");

            // And it follows visibility. Disconnecting takes the services away, so the panel hides
            // and the keep-out area must go with it - a rect left behind would reserve map area for
            // a panel that is gone.
            disconnectMockLink(mockLink);
            if (QTest::currentTestFailed()) {
                return;
            }

            QVERIFY2(UnitTest::waitForCondition([this, overlay]() { return _publishedOccluders(overlay).isEmpty(); },
                                                TestTimeout::mediumMs(),
                                                QStringLiteral("the overlay to withdraw its bounds")),
                     "the overlay must publish no rect once the vehicle is gone");
        });
}

void AeroraFlyViewOverlayUITest::_overlayCarriesNoActionableControl()
{
    runWithMockLink(
        [] { return MockLink::startGenericMockLink(); },
        [this](QPointer<MockLink>, Vehicle* vehicle) {
            QVERIFY(vehicle);

            QQuickItem* const overlay = _overlay();
            QVERIFY(overlay);

            // With the vehicle connected so the panels are actually instantiated: an overlay that
            // rendered nothing would pass this vacuously.
            QQuickItem* const panels = findItem(overlay, QStringLiteral("aeroraLiveOperationsPanels"));
            QVERIFY(panels);
            QVERIFY2(UnitTest::waitForCondition([panels]() { return panels->isVisible(); }, TestTimeout::mediumMs(),
                                                QStringLiteral("the overlay panels to render")),
                     "this assertion is only meaningful once the panels exist");

            // The boundary the seam declaration was approved under: read-only display. Asserted
            // structurally rather than by review, so a control added through any route fails here.
            QVERIFY2(!hasActionableControl(overlay),
                     "the Live Operations overlay must carry no button and no mouse area");
        });
}

void AeroraFlyViewOverlayUITest::_linkStatePanelReportsOneRowPerLink()
{
    runWithMockLink(
        [] { return MockLink::startGenericMockLink(); },
        [this](QPointer<MockLink>, Vehicle* vehicle) {
            QVERIFY(vehicle);

            QQuickItem* const overlay = _overlay();
            QVERIFY(overlay);

            QQuickItem* const panel = findItem(overlay, QStringLiteral("aeroraLinkStatePanel"));
            QVERIFY2(panel, "the overlay must contain the link state panel");

            // Polled for the same reason as the occluder publication: the services attach during
            // vehicle construction but the binding chain settles a frame or two later.
            QVERIFY2(UnitTest::waitForCondition([panel]() { return panel->isVisible(); }, TestTimeout::mediumMs(),
                                                QStringLiteral("the link panel to render")),
                     "with an Aerora vehicle connected the link panel must render");

            // Read the same source the panel reads, so the comparison is between the panel and the
            // vehicle rather than between the panel and this test's idea of the vehicle.
            QObject* const linkManager = vehicle->property("vehicleLinkManager").value<QObject*>();
            QVERIFY2(linkManager, "the vehicle must publish its link manager");

            const QStringList vehicleLinkNames = linkManager->property("linkNames").toStringList();
            QVERIFY2(!vehicleLinkNames.isEmpty(), "a connected vehicle must have at least one link to report");

            // One row per link and none invented. P7-RAD-002 asks for per-link display rather than
            // an aggregate, and a panel that collapsed them would fail here rather than on review.
            QCOMPARE(panel->property("_linkNames").toStringList(), vehicleLinkNames);

            // Upstream's parallel-list invariant, which the panel indexes into. If the two lists
            // ever differed in length the panel would describe one link with another's status, and
            // the per-link display would be worse than an aggregate rather than better.
            QCOMPARE(panel->property("_linkStatuses").toStringList().size(), qsizetype(vehicleLinkNames.size()));

            // The link the vehicle is actually addressed through. Read to mark a row; a panel that
            // left it empty would mark none, which on a multi-link handover is the one thing the
            // operator needs.
            QVERIFY2(!panel->property("_primaryLinkName").toString().isEmpty(),
                     "a connected vehicle must have a primary link for the panel to mark");
        });
}

void AeroraFlyViewOverlayUITest::_enforcementIndicatorReachesTheToolbarAndReportsNotEnforced()
{
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    // No vehicle needed and deliberately none connected: the enforcement strip is a non-vehicle
    // indicator, which is the whole point of registering it through QGCCorePlugin rather than
    // through FirmwarePlugin. It must be readable before anything is connected.
    QQuickItem* const indicator =
        findVisibleItem(_rootItem, QStringLiteral("aeroraEnforcementIndicator"), TestTimeout::mediumMs());
    QVERIFY2(indicator,
             "the Aerora enforcement indicator must reach the toolbar through toolBarIndicators(); a missing one "
             "is the silent-registration failure this overlay has hit before");

    // The claim the strip exists to make. No authorization policy is installed in any build
    // shipped so far, so the honest reading is "not enforced" - and the derivation is written
    // against the enforced enumerator so a resolution failure lands here too rather than
    // presenting enforcement that is not happening.
    QVERIFY2(!indicator->property("_enforced").toBool(),
             "with no authorization policy installed the strip must not report enforcement");

    // Asserted because the binding FlyView's toolbar uses is `visible: item.showIndicator`, which
    // is unguarded: an indicator that lost this property would be laid out with an undefined
    // visibility rather than failing.
    QVERIFY2(indicator->property("showIndicator").toBool(),
             "the toolbar binds visible to showIndicator, so the indicator must keep it true");
}

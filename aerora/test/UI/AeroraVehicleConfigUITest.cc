#include "AeroraVehicleConfigUITest.h"

#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtQuick/QQuickItem>
#include <QtTest/QTest>

#include "MockLink.h"
#include "Vehicle.h"

UT_REGISTER_TEST(AeroraVehicleConfigUITest, TestLabel::Integration, TestLabel::Vehicle)

namespace {

/// The sidebar entries, as VehicleConfigView builds them: "vehicleConfig_comp_" plus the
/// component's name with spaces removed. The names come from AeroraVehicleComponent::name(), so a
/// rename there breaks these and should - the operator-facing label and the test's handle on it
/// are the same string.
constexpr const char* kIdentitySidebarButton = "vehicleConfig_comp_AeroraIdentity";
constexpr const char* kServicesSidebarButton = "vehicleConfig_comp_AeroraServices";

/// The page roots. Finding these proves the click loaded the intended page rather than leaving the
/// previous one up, which a content check alone cannot distinguish.
constexpr const char* kIdentityPageObject = "aeroraIdentityPage";
constexpr const char* kServicesPageObject = "aeroraServiceDiagnosticsPage";

}  // namespace

void AeroraVehicleConfigUITest::init()
{
    VehicleConfigUITestBase::init();

    // Emitted once per process by AeroraCorePlugin::init() and latched there, so only whichever
    // slot runs first could observe them - expecting them would be expecting something absent for
    // every later slot. Same set and same reasoning as AeroraSceneAttachmentUITest::init().
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("no platform key protector available")));
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("command authorization is NOT being enforced")));
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("allowing commands without authorization")));

    // The Generic MockLink publishes no component metadata, so the request state machine gives up
    // once per connection. Environment noise from the mock, not a property of the overlay.
    ignoreLogMessage("ComponentInformation.RequestMetaDataTypeStateMachine", QtWarningMsg,
                     QRegularExpression(QStringLiteral("failed to load metadata")));
}

QQuickItem* AeroraVehicleConfigUITest::_openAeroraPage(const QString& sidebarObjectName, const QString& pageObjectName)
{
    clickSidebarButton(sidebarObjectName);
    if (QTest::currentTestFailed()) {
        return nullptr;
    }

    QQuickItem* const page = findVisibleItem(_rootItem, pageObjectName, TestTimeout::mediumMs());
    if (!page) {
        QTest::qFail(
            qPrintable(QStringLiteral("sidebar entry %1 did not load %2").arg(sidebarObjectName, pageObjectName)),
            __FILE__, __LINE__);
        return nullptr;
    }

    // The rendering claim. See the \section rendering note on the class: this descends into
    // setupPage_contentLoader and requires a laid-out leaf with a real size, so a page that loads
    // and draws nothing fails here rather than passing as "the loader has an item".
    verifyPanelContentVisible(pageObjectName);
    if (QTest::currentTestFailed()) {
        return nullptr;
    }

    return page;
}

void AeroraVehicleConfigUITest::_aeroraSetupPagesRenderInTheConfigureView()
{
    runWithMockLink(
        [] { return MockLink::startGenericMockLink(); },
        [this](QPointer<MockLink>, Vehicle* vehicle) {
            QVERIFY(vehicle);

            navigateToConfigureView();
            if (QTest::currentTestFailed()) {
                return;
            }

            // Both published pages, in catalogue order. Opening them in one slot is deliberate:
            // the second click also proves the view switches *between* Aerora pages, which a
            // single-page test cannot show and which is where a shared objectName or a stuck
            // loader would surface.
            QVERIFY(_openAeroraPage(QString::fromUtf8(kIdentitySidebarButton), QString::fromUtf8(kIdentityPageObject)));
            QVERIFY(_openAeroraPage(QString::fromUtf8(kServicesSidebarButton), QString::fromUtf8(kServicesPageObject)));

            // And the identity page is gone, so the panel was replaced rather than stacked.
            // Polled rather than checked once: the Loader destroys the old item, and that can
            // land a frame later.
            QVERIFY(verifyVisibility(QString::fromUtf8(kIdentityPageObject), false,
                                     QStringLiteral("after switching to the services page")));

            waitForParamRefreshQuiet(vehicle);
        });
}

void AeroraVehicleConfigUITest::_identityPageReadsTheLiveVehicleAndWithholdsAFleetIdentity()
{
    runWithMockLink(
        [] { return MockLink::startGenericMockLink(); },
        [this](QPointer<MockLink>, Vehicle* vehicle) {
            QVERIFY(vehicle);

            navigateToConfigureView();
            if (QTest::currentTestFailed()) {
                return;
            }

            QQuickItem* const page =
                _openAeroraPage(QString::fromUtf8(kIdentitySidebarButton), QString::fromUtf8(kIdentityPageObject));
            QVERIFY(page);

            // The reading, not the route. These are the strings the two protocol rows render, so a
            // page that bound the right object and rendered the wrong field off it fails here -
            // which is the half AeroraVehicleLifecycleTest's check of the service cannot see.
            QCOMPARE(page->property("_systemIdText").toString(), QString::number(vehicle->id()));
            QCOMPARE(page->property("_componentIdText").toString(), QString::number(vehicle->defaultComponentId()));

            // The boundary this page exists to hold. A connected vehicle has a known system id and
            // still has no fleet identity, because no evidence source reports one and the system
            // id is not a substitute. A true reading here would mean the page had invented one.
            const QVariant fleetKnown = page->property("_fleetKnown");
            QVERIFY2(fleetKnown.isValid(), "the page must expose its fleet-identity reading");
            QVERIFY2(!fleetKnown.toBool(), "no source reports a fleet identity, so the page must report none");

            // The precondition for all of the above, asserted so a page that read every row as
            // "not checked" could not satisfy the fleet-identity check vacuously.
            QVERIFY2(page->property("_available").toBool(),
                     "an Aerora vehicle is connected, so the page must report its services attached");

            waitForParamRefreshQuiet(vehicle);
        });
}

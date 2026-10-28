#include "AeroraVehicleConfigUITest.h"

#include <QtCore/QList>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QVariant>
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
constexpr const char* kCapabilitySidebarButton = "vehicleConfig_comp_AeroraCapabilities";
constexpr const char* kHealthSidebarButton = "vehicleConfig_comp_AeroraHealth";
constexpr const char* kServicesSidebarButton = "vehicleConfig_comp_AeroraServices";

/// The page roots. Finding these proves the click loaded the intended page rather than leaving the
/// previous one up, which a content check alone cannot distinguish.
constexpr const char* kIdentityPageObject = "aeroraIdentityPage";
constexpr const char* kCapabilityPageObject = "aeroraCapabilityPage";
constexpr const char* kHealthPageObject = "aeroraHealthPage";
constexpr const char* kServicesPageObject = "aeroraServiceDiagnosticsPage";

/// Every enumerator of AeroraVehicleCapabilities::Category. The capability page must publish one
/// section per category; AeroraUiFoundationTest holds this count against the meta-object.
constexpr int kCapabilityCategoryCount = 5;

/// The Operations Hub's Analyze entry, as AnalyzeView builds it: "analyzeButton_" plus the title
/// from AeroraCorePlugin::analyzePages(). A rename there breaks this and should - it is the string
/// the operator reads.
constexpr const char* kHubAnalyzeEntryButton = "analyzeButton_Aerora Operations";

/// The hub's navigation button and the flickable it sits inside. The navigation section is below
/// fifteen report rows, so it has to be scrolled into view before it can be clicked.
constexpr const char* kHubVehicleConfigButton = "aeroraHub_vehicleConfigButton";
constexpr const char* kHubReportFlickable = "aeroraHub_reportFlickable";

/// The Configure view's root, which is the destination the hub button claims to open.
constexpr const char* kVehicleConfigRoot = "vehicleConfig_root";

/// The Analyze view's own page-list flickable. Aerora entries are appended after the upstream ones,
/// so the list outgrows an offscreen window and an entry has to be scrolled into view before it can
/// be clicked. Clicked through the scrolling helper so this stays independent of how many entries
/// precede the hub.
constexpr const char* kAnalyzeListFlickable = "analyzeView_buttonScroll";

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

            // Every published page, in catalogue order. Opening them in one slot is deliberate:
            // each later click also proves the view switches *between* Aerora pages, which a
            // single-page test cannot show and which is where a shared objectName or a stuck
            // loader would surface.
            QVERIFY(_openAeroraPage(QString::fromUtf8(kIdentitySidebarButton), QString::fromUtf8(kIdentityPageObject)));
            QVERIFY(
                _openAeroraPage(QString::fromUtf8(kCapabilitySidebarButton), QString::fromUtf8(kCapabilityPageObject)));
            QVERIFY(_openAeroraPage(QString::fromUtf8(kHealthSidebarButton), QString::fromUtf8(kHealthPageObject)));
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

void AeroraVehicleConfigUITest::_vehicleConfigurationIsReachableFromTheOperationsHub()
{
    runWithMockLink(
        [] { return MockLink::startGenericMockLink(); },
        [this](QPointer<MockLink>, Vehicle* vehicle) {
            QVERIFY(vehicle);

            // The operator's route in, through the Analyze list rather than by loading the hub
            // directly: whether the hub's entry becomes a button a person can reach is a property
            // of the Analyze view consuming analyzePages(), which the registry cannot report.
            QVERIFY2(clickToolSelectDropdownButton(QStringLiteral("toolbar_viewAnalyze")),
                     "the Analyze view must be reachable from the tool-select dropdown");
            QVERIFY2(clickButtonScrolled(QString::fromUtf8(kHubAnalyzeEntryButton),
                                         QString::fromUtf8(kAnalyzeListFlickable)),
                     "the Aerora Operations entry must appear in the Analyze page list");

            // Found including hidden items, then scrolled: the navigation section is at the bottom
            // of the hub's report and starts below the fold on an offscreen-sized window.
            QQuickItem* const navButton = findItem(_rootItem, QString::fromUtf8(kHubVehicleConfigButton));
            QVERIFY2(navButton, "the hub must carry its Vehicle Configuration navigation button");
            QVERIFY(scrollIntoView(navButton, QString::fromUtf8(kHubReportFlickable)));

            // Enabled because a vehicle is connected. The gate is the point of the button's
            // `enabled` binding: with no vehicle the Configure view has no sidebar to show.
            QVERIFY(verifyEnabled(QString::fromUtf8(kHubVehicleConfigButton), true,
                                  QStringLiteral("with an Aerora vehicle connected")));

            QVERIFY2(clickButton(QString::fromUtf8(kHubVehicleConfigButton)),
                     "the hub's Vehicle Configuration button must be clickable");

            // Clicking is not navigating. Without this the test would pass against a button whose
            // handler called a function that does not exist - which is a runtime ReferenceError in
            // QML, not a build failure.
            QVERIFY2(findVisibleItem(_rootItem, QString::fromUtf8(kVehicleConfigRoot), TestTimeout::mediumMs()),
                     "the hub button must open the Vehicle Configuration view");

            // And the destination is the one the hub's text promises: every published Aerora page is
            // in the sidebar it just opened.
            QVERIFY2(findVisibleItem(_rootItem, QString::fromUtf8(kIdentitySidebarButton), TestTimeout::mediumMs()),
                     "the opened view must list the Aerora Identity page");
            QVERIFY2(findVisibleItem(_rootItem, QString::fromUtf8(kCapabilitySidebarButton), TestTimeout::mediumMs()),
                     "the opened view must list the Aerora Capabilities page");
            QVERIFY2(findVisibleItem(_rootItem, QString::fromUtf8(kHealthSidebarButton), TestTimeout::mediumMs()),
                     "the opened view must list the Aerora Health page");
            QVERIFY2(findVisibleItem(_rootItem, QString::fromUtf8(kServicesSidebarButton), TestTimeout::mediumMs()),
                     "the opened view must list the Aerora Services page");

            waitForParamRefreshQuiet(vehicle);
        });
}

void AeroraVehicleConfigUITest::_capabilityPageReportsAnEmptyInventoryWithoutClaimingAbsence()
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
                _openAeroraPage(QString::fromUtf8(kCapabilitySidebarButton), QString::fromUtf8(kCapabilityPageObject));
            QVERIFY(page);

            // The precondition, asserted first so nothing below can be satisfied vacuously by a
            // page that read every row as "not checked".
            QVERIFY2(page->property("_available").toBool(),
                     "an Aerora vehicle is connected, so the page must report its services attached");
            QVERIFY2(!page->property("_capabilities").isNull(),
                     "the connected Aerora vehicle must have a capability service for the page to read");

            // One entry per category, derived from the Q_ENUM rather than written out as sections.
            // A page that rendered a heading and no inventory would still look like a working
            // screen, which is the failure this count exists to catch.
            const QVariantList reportedKeys = page->property("_reportedKeys").toList();
            QCOMPARE(reportedKeys.size(), qsizetype(kCapabilityCategoryCount));

            // Every category empty, which is the Q3-009 reading: no Aerora evidence source exists
            // in this build, so nothing has reported a capability. When one does, this is the
            // assertion that should fail and be updated - the page needs no change for it.
            for (qsizetype i = 0; i < reportedKeys.size(); ++i) {
                QVERIFY2(reportedKeys.at(i).toList().isEmpty(),
                         qPrintable(QStringLiteral("category %1 reported a capability key; no evidence source "
                                                   "exists in this build (Q3-009)")
                                        .arg(i)));
            }

            waitForParamRefreshQuiet(vehicle);
        });
}

void AeroraVehicleConfigUITest::_healthPageWithholdsAVerdictUntilTheAircraftReportsOne()
{
    runWithMockLink([] { return MockLink::startGenericMockLink(); },
                    [this](QPointer<MockLink>, Vehicle* vehicle) {
                        QVERIFY(vehicle);

                        navigateToConfigureView();
                        if (QTest::currentTestFailed()) {
                            return;
                        }

                        QQuickItem* const page = _openAeroraPage(QString::fromUtf8(kHealthSidebarButton),
                                                                 QString::fromUtf8(kHealthPageObject));
                        QVERIFY(page);

                        // Read the same source the page reads, through the same property, so the comparison is
                        // between the page and the vehicle rather than between the page and this test's idea of
                        // the vehicle.
                        QObject* const sensorInfo = vehicle->property("sysStatusSensorInfo").value<QObject*>();
                        QVERIFY2(sensorInfo, "Vehicle must publish its SYS_STATUS sensor model");

                        const QStringList vehicleSensorNames = sensorInfo->property("sensorNames").toStringList();
                        const bool anythingReported = !vehicleSensorNames.isEmpty();

                        // The page renders one row per reported sensor and invents none.
                        QCOMPARE(page->property("_sensorNames").toList().size(), qsizetype(vehicleSensorNames.size()));
                        QCOMPARE(page->property("_anyReported").toBool(), anythingReported);

                        // The assertion this slot exists for. Vehicle::allSensorsHealthy() starts true
                        // (Vehicle.h:969) and only moves when a SYS_STATUS disagrees, so before any report it
                        // says every sensor is healthy for an aircraft that has named none. The page's reading
                        // must be gated on a report having arrived - which means that when nothing has been
                        // reported it must be false even while the vehicle's own property is true.
                        const bool vehicleClaimsHealthy = vehicle->property("allSensorsHealthy").toBool();
                        const bool pageClaimsHealthy = page->property("_allHealthy").toBool();

                        if (anythingReported) {
                            QCOMPARE(pageClaimsHealthy, vehicleClaimsHealthy);
                        } else {
                            QVERIFY2(!pageClaimsHealthy,
                                     "nothing has been reported, so the page must show no health verdict even though "
                                     "Vehicle::allSensorsHealthy() defaults to true");
                        }

                        waitForParamRefreshQuiet(vehicle);
                    });
}

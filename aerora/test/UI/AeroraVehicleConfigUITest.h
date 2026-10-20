#pragma once

#include "VehicleConfigUITestBase.h"

/// Runtime coverage of the Aerora vehicle setup pages in the view that actually publishes them.
///
/// Every other test of these pages stops short of the thing that can break. `AeroraVehicleSetupTest`
/// asserts the catalogue publishes a descriptor; `AeroraVehicleLifecycleTest` asserts the plugin
/// builds a component from it; `AeroraUiFoundationTest` asserts the QML document compiles. None of
/// them loads a page, because `SetupPage` reads `globals` and `vehicleComponent` from context that
/// only `VehicleConfigView`'s `Loader` supplies - so a page that compiles and renders nothing would
/// pass all three.
///
/// This suite closes that by booting the real scene and taking the operator's route:
///
/// \code
/// tool-select dropdown -> Configure view -> Aerora sidebar entry -> panelLoader
/// \endcode
///
/// \section rendering Why this is rendering evidence and the others are not
///
/// `verifyPanelContentVisible()` walks the loaded panel for a visible leaf - a `QQuickText`,
/// `QQuickControl` or `QQuickImageBase` with non-zero width *and* height - and for a
/// `SetupPage` it descends into `setupPage_contentLoader` first, so the page's own content is
/// checked rather than the title and description chrome that render regardless. In a booted window
/// under xvfb with a real font database, that is a laid-out item with a real size, not an
/// instantiated object. It is the strongest claim this harness supports; pixel appearance and the
/// Aerora visual language (`Q-040`) remain outside it.
///
/// \section generic Why a Generic MockLink
///
/// `AeroraFirmwarePluginFactory` claims exactly `FirmwareClassGeneric`
/// (`AeroraFirmwareClaim::kFirmwareClass`), and `FirmwarePluginManager` matches the raw
/// `MAV_AUTOPILOT` value without normalising it. A PX4- or APM-flavoured MockLink therefore does
/// not reach the Aerora plugin at all and would assert nothing about Aerora. `startGenericMockLink()`
/// also has its own parameter file (`:/MockLink/GenericMockLink.params`), which matters because
/// `VehicleConfigView` gates its whole component list on `_fullParameterVehicleAvailable`: a
/// vehicle whose parameters never finish loading shows an empty sidebar and no Aerora entry.
class AeroraVehicleConfigUITest : public VehicleConfigUITestBase
{
    Q_OBJECT

public:
    AeroraVehicleConfigUITest() = default;

protected slots:
    void init() override;

private slots:
    // The navigation and rendering contract: both published pages are reachable through the real
    // sidebar and draw content once loaded.
    void _aeroraSetupPagesRenderInTheConfigureView();

    // The state contract: the identity page's readings are the connected vehicle's own, and the
    // fleet identity is withheld rather than substituted.
    void _identityPageReadsTheLiveVehicleAndWithholdsAFleetIdentity();

    // SCR-SHL-01's navigation section: the operator's route from the Operations Hub to the view
    // that hosts the Aerora setup pages, clicked rather than asserted on the registry.
    void _vehicleConfigurationIsReachableFromTheOperationsHub();

    // SCR-VEH-03's claim: the inventory is derived from the category Q_ENUM, and an empty one is
    // reported as unreported rather than as a vehicle with no capabilities.
    void _capabilityPageReportsAnEmptyInventoryWithoutClaimingAbsence();

private:
    /// Clicks the sidebar entry named @a sidebarObjectName, waits for @a pageObjectName to appear,
    /// and asserts the loaded panel renders visible content.
    ///
    /// Uses QVERIFY internally - check QTest::currentTestFailed() on return.
    ///
    /// @return The page root item, or nullptr after recording a failure.
    QQuickItem* _openAeroraPage(const QString& sidebarObjectName, const QString& pageObjectName);
};

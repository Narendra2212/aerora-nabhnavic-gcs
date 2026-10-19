#pragma once

#include <QtCore/QString>

#include "UnitTest.h"

/// Covers the Aerora vehicle setup-page catalogue (U4) and the per-vehicle tool indicator seam
/// (U5), at the level that needs no `Vehicle`.
///
/// Everything asserted here is a static function or a plugin method that tolerates a null vehicle,
/// so these run as `Unit` with no link and no event loop. The same seams driven through a real
/// MockLink-connected vehicle are covered by AeroraVehicleLifecycleTest.
///
/// The property these tests exist to protect is that neither seam offers something it cannot
/// deliver: a setup page is published only when its QML is in the build, and an indicator is
/// offered only when the capability it reports on was reported **available**.
///
/// That gate fails in two directions and both are asserted. A page whose QML is absent must be
/// withheld, which the matched-pair checks cover generically; and a page whose QML is *present*
/// must actually publish, which no generic check can cover - a broken resource alias withholds
/// every page, and a test written only as a matched pair would still pass with the whole Aerora
/// setup surface silently gone. `_serviceDiagnosticsIsPublishedAndTheRemainingPagesAreWithheld()`
/// is the assertion that names the expected state outright for that reason.
class AeroraVehicleSetupTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _pageCatalogueCoversTheProductScreensAndWithholdsCalibration();

    void _resourceResolutionRejectsAbsentAndNonResourcePaths_data();
    void _resourceResolutionRejectsAbsentAndNonResourcePaths();

    void _aPageIsPublishableOnlyWhenEveryDeclaredResourceExists();
    void _pagePublishabilityFollowsItsQmlResource();
    void _publishedPagesAreNamedAndTheRestAreWithheld();

    void _indicatorGateRequiresCapabilityReportedAvailable_data();
    void _indicatorGateRequiresCapabilityReportedAvailable();
    void _indicatorUrlsFollowDeclarationOrderAndSkipWithheldEntries();
    void _noIndicatorIsDeclaredWhileNoneIsEvidenced();

    void _autopilotPluginRefusesANullVehicle();
    void _toolIndicatorsLeaveTheBaseListUnchangedWhileNoneIsDeclared();

private:
    /// A Qt resource path that this build actually contains, for exercising the positive half of
    /// the existence gate without hard-coding an upstream file path that could move.
    ///
    /// @return The path, or an empty string when the resource system holds no QML at all.
    [[nodiscard]] QString _anyExistingQmlResource();

    QString _existingQmlResource;
    bool _existingQmlResourceProbed = false;
};

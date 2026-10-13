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
class AeroraVehicleSetupTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _pageCatalogueCoversTheProductScreensAndWithholdsCalibration();

    void _resourceResolutionRejectsAbsentAndNonResourcePaths_data();
    void _resourceResolutionRejectsAbsentAndNonResourcePaths();

    void _aPageIsPublishableOnlyWhenEveryDeclaredResourceExists();
    void _declaredPagesAreWithheldWhileTheirQmlIsAbsent();

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

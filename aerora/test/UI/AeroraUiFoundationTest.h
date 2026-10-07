#pragma once

#include "UnitTest.h"

/// \brief Verifies the two UI foundation seams: the allow-listed QML override interceptor
/// (Q7-003) and the typed read-only route from QML to the active vehicle's Aerora services
/// (Q7-002).
///
/// Every test here is pure in-memory logic. None needs a vehicle, a link, a QML engine, a clock or
/// a dialect, which is deliberate: the UI foundation has to be testable before any Aerora telemetry
/// exists, because the state-class contract is the part that is expensive to retrofit.
///
/// Four of these are prohibition tests and they name the regression they catch:
///
/// \li an override declared by accident, which would silently fork a populated upstream file;
/// \li an override of `CustomMapItems.qml`, which reads as a purpose-built seam and is not one;
/// \li interception of a URL Aerora never declared, which would silently replace upstream
///     behaviour with no error of any kind;
/// \li a string-keyed service lookup appearing on the QML surface, which would let the interface
///     reach any service attached later with no review of its QML surface.
class AeroraUiFoundationTest : public UnitTest
{
    Q_OBJECT

private slots:
    // Q7-003 - the override allow-list, and the tier discipline it enforces.
    void _noOverrideIsDeclared();
    void _customMapItemsIsForbiddenAndUndeclared();
    void _mirroredResourcePathMirrorsTheStockPath_data();
    void _mirroredResourcePathMirrorsTheStockPath();

    // Q7-003 - the interception rule. The security constraint rests on these three.
    void _nonResourceUrlPassesThroughUnchanged_data();
    void _nonResourceUrlPassesThroughUnchanged();
    void _undeclaredResourceUrlIsNeverIntercepted_data();
    void _undeclaredResourceUrlIsNeverIntercepted();
    void _selfCheckPassesWhenNothingIsDeclared();

    // Q7-002 - the QML route to per-vehicle services, with no vehicle connected. This is the state
    // the interface is in for most of its life and every accessor must survive it.
    void _uiContextIsUnavailableWithNoActiveVehicle();
    void _uiContextDoesNotExposeTheVehicleOrTheServiceMap();
};

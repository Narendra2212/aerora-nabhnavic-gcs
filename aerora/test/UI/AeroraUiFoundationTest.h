#pragma once

#include "UnitTest.h"

/// \brief Verifies the two UI foundation seams: the allow-listed QML override interceptor
/// (Q7-003) and the typed read-only route from QML to the active vehicle's Aerora services
/// (Q7-002).
///
/// Most tests here are pure in-memory logic. None needs a vehicle, a link, a clock or a dialect,
/// which is deliberate: the UI foundation has to be testable before any Aerora telemetry exists,
/// because the state-class contract is the part that is expensive to retrofit.
///
/// The installation-state group is the one exception: `installOn()` calls
/// `QQmlApplicationEngine::addUrlInterceptor()`, so those tests construct a bare engine. They never
/// call `load()`, so no window and no scene graph is created. This is safe in a unit-test run
/// because `QGCApplication::init()` takes neither the boot-test nor the normal-boot branch when
/// `_runningUnitTests` is set (`src/QGCApplication.cc:244-251`), so no engine exists and no
/// interceptor is installed when these tests start.
///
/// \note The interceptor is tracked in file-static state in `AeroraQmlOverrides.cc`, shared by the
///       whole process. Every test below releases what it installs, so test order cannot leak an
///       installed interceptor into `AeroraUiSafetyTest` or into a later run of this fixture.
///
/// Four of these are prohibition tests and they name the regression they catch:
///
/// \li an override declared by accident, which would silently fork a populated upstream file;
/// \li an override of `CustomMapItems.qml`, which reads as a purpose-built seam and is not one;
/// \li interception of a URL Aerora never declared, which would silently replace upstream
///     behaviour with no error of any kind;
/// \li a string-keyed service lookup appearing on the QML surface, which would let the interface
///     reach any service attached later with no review of its QML surface.
///
/// One test covers a third seam: the names `AeroraEnforcementIndicator.qml` binds to on the
/// `AeroraSecurity` singleton. The strip itself is QML and cannot be loaded here - that needs the
/// Qt Quick Test target, `HO-TAB7-04` - so what is asserted is the C++ half of its contract, which
/// is also the half whose breakage is invisible: the indicator fails safe to "not enforced", so a
/// renamed property or enumerator would leave it permanently correct-looking and permanently stuck.
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

    // U1 / HO-TAB7-02 - the installation state machine. isInstalled() is what the IF-UIS-015
    // startup self-check reads, so an interface quietly missing its Aerora layer is detectable
    // rather than merely wrong. These are the tests that fail if the AeroraCorePlugin overrides
    // are ever removed and the interceptor stops being installed.
    void _interceptorIsNotInstalledBeforeInstall();
    void _installOnNullEngineDoesNotInstall();
    void _installThenReleaseTracksInstallationState();
    void _secondInstallIsRefusedAndLeavesTheFirstInstalled();
    void _releaseFromAnUnrelatedEngineIsANoOp();
    void _releaseWithNothingInstalledIsANoOp();

    // Q7-002 - the QML route to per-vehicle services, with no vehicle connected. This is the state
    // the interface is in for most of its life and every accessor must survive it.
    void _uiContextIsUnavailableWithNoActiveVehicle();
    void _uiContextDoesNotExposeTheVehicleOrTheServiceMap();

    // HO-TAB1-10 - the names AeroraEnforcementIndicator.qml binds to on the AeroraSecurity
    // singleton. The strip's own rendering is QML and is not reachable from this harness; this
    // covers only the part that is, which is the half that fails silently.
    void _enforcementIndicatorBindingNamesResolve();

    // SCR-LIV-06 - the names AeroraMissionProgressPage.qml binds to. Same reason as the row above
    // and a sharper case of it: that page matches every enumerator positively and renders anything
    // unmatched as "not evidenced", so a renamed enumerator degrades to a plausible-looking screen
    // rather than to an error.
    void _missionProgressEnumeratorsResolve_data();
    void _missionProgressEnumeratorsResolve();
    void _missionProgressPropertiesResolve_data();
    void _missionProgressPropertiesResolve();

    // Gadget value access through a real QML engine, not through the meta-object.
    //
    // This is the mechanism AeroraMissionProgressPage.qml actually depends on, and the one a name
    // lookup cannot prove. AeroraMissionIndex and AeroraMissionSnapshot carry no QML value-type
    // registration, so whether a binding resolves `.known` or `.itemCount` is a question about
    // QQmlValueTypeWrapper rather than about the headers. Qt documents Q_GADGET attributes as
    // accessible from QML; this asserts it for the specific types the page reads, so the page's
    // reliance on it stops being an assumption.
    //
    // The enum half is deliberately absent: an unregistered gadget's Q_ENUM is not reachable by
    // type name from QML at all, which is why the page names no enumerator of AeroraMissionIndex,
    // AeroraMissionSnapshot or AeroraMissionAgreement.
    void _missionGadgetValuesAreReadableFromQml_data();
    void _missionGadgetValuesAreReadableFromQml();

    // The agreement verdict, read the one way the page can read it.
    //
    // AeroraMissionAgreement::isVerified() is a Q_INVOKABLE on an unregistered Q_GADGET, so
    // whether a QML binding can call it at all is the open question - and it is the whole
    // verification half of SCR-LIV-06, because Verdict::Matches cannot be named from QML. Both
    // directions are asserted from one fixture: NotCompared must read false before a comparison,
    // and the same expression must read true after one matched. Without the second half the first
    // would pass for a call that never resolved.
    void _missionAgreementVerdictIsReadableFromQml();
};

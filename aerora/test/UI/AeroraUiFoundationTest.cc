#include "AeroraUiFoundationTest.h"

#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QMetaClassInfo>
#include <QtCore/QMetaEnum>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtCore/QVariant>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlExpression>

#include "AeroraMissionIndex.h"
#include "AeroraMissionObserver.h"
#include "AeroraMissionSnapshot.h"
#include "AeroraQmlOverrides.h"
#include "AeroraSecurityStatus.h"
#include "AeroraVehicleServices.h"
#include "AeroraVehicleUiContext.h"
#include "QGCCorePlugin.h"

namespace {

/// The stock resource path of the upstream file that looks like a purpose-built override seam and
/// is not one. Spelled out here rather than read from the production list so a silent change to
/// that list fails this test.
constexpr const char *kCustomMapItemsPath = "/qml/QGroundControl/FlightMap/MapItems/CustomMapItems.qml";

/// A stock seam path Aerora has pre-approved but has not declared. Used to assert that approval is
/// not the same thing as declaration: an undeclared path is never intercepted.
constexpr const char *kUndeclaredSeamPath = "/qml/QGroundControl/FlyView/FlyViewCustomLayer.qml";

/// Exact logging category of AeroraQmlOverrides, from AeroraUiLog.cc. expectLogMessage() matches
/// the category string literally, so a mismatch here is a silent test failure rather than a
/// compile error - which is why it is a named constant checked against the production definition.
constexpr const char *kUiLogCategory = "Aerora.Ui";

/// The SCR-VEH-06 setup page, as the engine URL and as the resource path behind it. Flat under the
/// module prefix because qgc_set_qt_resource_alias() aliases every AERORA_UI_QML_FILES entry to its
/// bare name. Spelled out rather than read from AeroraVehicleComponent::pageDescriptors(), so that
/// the catalogue and the engine's view of the same file cannot drift in step.
constexpr const char* kServiceDiagnosticsPageUrl = "qrc:/qml/Aerora/Controls/AeroraServiceDiagnosticsPage.qml";
constexpr const char* kServiceDiagnosticsResource = ":/qml/Aerora/Controls/AeroraServiceDiagnosticsPage.qml";

/// Resolves one of the three meta-objects AeroraMissionProgressPage.qml binds to, by class name.
///
/// A name lookup rather than a `const QMetaObject *` data column: the set is closed, and naming
/// the class in the data table is what makes a failure message say which binding broke.
const QMetaObject *missionMetaObjectFor(const QByteArray &className)
{
    if (className == "AeroraMissionObserver") {
        return &AeroraMissionObserver::staticMetaObject;
    }
    if (className == "AeroraMissionIndex") {
        return &AeroraMissionIndex::staticMetaObject;
    }
    if (className == "AeroraMissionSnapshot") {
        return &AeroraMissionSnapshot::staticMetaObject;
    }
    return nullptr;
}

} // namespace

void AeroraUiFoundationTest::_noOverrideIsDeclared()
{
    // The whole tier-3 obligation - a recorded fork point, a named owner, a drift review at every
    // upstream merge - attaches per populated-file override. Zero is the cheapest possible state
    // and this test is what keeps it from drifting upward unnoticed.
    QVERIFY(AeroraQmlOverrides::overrideEntries().isEmpty());
    QVERIFY(AeroraQmlOverrides::overridePaths().isEmpty());
    QCOMPARE(AeroraQmlOverrides::populatedFileOverrideCount(), 0);
}

void AeroraUiFoundationTest::_customMapItemsIsForbiddenAndUndeclared()
{
    const QString path = QString::fromUtf8(kCustomMapItemsPath);

    // Upstream populates CustomMapItems.qml with an Instantiator over
    // QGCCorePlugin::customMapItems(). Overriding it would fork a populated file and discard the
    // C++ seam that is the supported way to add a map item.
    QVERIFY2(AeroraQmlOverrides::forbiddenPaths().contains(path),
             "CustomMapItems.qml must stay on the forbidden list");
    QVERIFY2(!AeroraQmlOverrides::overridePaths().contains(path),
             "CustomMapItems.qml must never be declared as an override");
    QVERIFY(!AeroraQmlOverrides::isOverrideCandidate(path));
}

void AeroraUiFoundationTest::_mirroredResourcePathMirrorsTheStockPath_data()
{
    QTest::addColumn<QString>("stockPath");
    QTest::addColumn<QString>("expected");

    QTest::newRow("qml file") << QStringLiteral("/qml/QGroundControl/FlyView/FlyViewCustomLayer.qml")
                              << QStringLiteral(":/Aerora/qml/QGroundControl/FlyView/FlyViewCustomLayer.qml");
    QTest::newRow("nested path") << QStringLiteral("/qml/QGroundControl/FlightMap/MapItems/Thing.qml")
                                 << QStringLiteral(":/Aerora/qml/QGroundControl/FlightMap/MapItems/Thing.qml");
    QTest::newRow("empty") << QString() << QString();
}

void AeroraUiFoundationTest::_mirroredResourcePathMirrorsTheStockPath()
{
    QFETCH(QString, stockPath);
    QFETCH(QString, expected);

    // The mirrored path must be a mechanical transform of the stock path, not a per-file mapping.
    // A per-file mapping is a second place for the two to disagree.
    QCOMPARE(AeroraQmlOverrides::mirroredResourcePath(stockPath), expected);
}

void AeroraUiFoundationTest::_nonResourceUrlPassesThroughUnchanged_data()
{
    QTest::addColumn<QUrl>("url");

    QTest::newRow("file") << QUrl(QStringLiteral("file:///tmp/Thing.qml"));
    QTest::newRow("https") << QUrl(QStringLiteral("https://example.invalid/Thing.qml"));
    QTest::newRow("image provider") << QUrl(QStringLiteral("image://coloredsvg/thing.svg"));
    QTest::newRow("empty") << QUrl();
    QTest::newRow("relative, no scheme") << QUrl(QStringLiteral("Thing.qml"));
}

void AeroraUiFoundationTest::_nonResourceUrlPassesThroughUnchanged()
{
    QFETCH(QUrl, url);

    // The interceptor sees every URL the QML engine resolves. Anything that is not a qrc resource
    // must come back byte-identical: this is the half of the security constraint that keeps a UI
    // mechanism from reaching anything other than Aerora UI resources.
    QCOMPARE(AeroraQmlOverrides::interceptedUrlFor(url), url);
}

void AeroraUiFoundationTest::_undeclaredResourceUrlIsNeverIntercepted_data()
{
    QTest::addColumn<QUrl>("url");

    QTest::newRow("pre-approved but undeclared seam")
        << QUrl(QStringLiteral("qrc:%1").arg(QString::fromUtf8(kUndeclaredSeamPath)));
    QTest::newRow("forbidden path")
        << QUrl(QStringLiteral("qrc:%1").arg(QString::fromUtf8(kCustomMapItemsPath)));
    QTest::newRow("main window") << QUrl(QStringLiteral("qrc:/qml/QGroundControl/MainWindow.qml"));
    QTest::newRow("arbitrary core qml")
        << QUrl(QStringLiteral("qrc:/qml/QGroundControl/FlyView/GuidedActionsController.qml"));
    QTest::newRow("core resource, not qml") << QUrl(QStringLiteral("qrc:/res/qgroundcontrol.ico"));
    QTest::newRow("json resource") << QUrl(QStringLiteral("qrc:/json/USBBoardInfo.json"));
}

void AeroraUiFoundationTest::_undeclaredResourceUrlIsNeverIntercepted()
{
    QFETCH(QUrl, url);

    // QGroundControl's reference overlay keys interception on QFile::exists(":/Custom" + path), so
    // the set of overridden files is implicit and a resource mirrored by accident wins silently.
    // Aerora keys it on an explicit declaration instead. This test is that difference: a path
    // nobody declared is returned unchanged no matter what resources the build happens to contain.
    QCOMPARE(AeroraQmlOverrides::interceptedUrlFor(url), url);
}

void AeroraUiFoundationTest::_selfCheckPassesWhenNothingIsDeclared()
{
    const AeroraQmlOverrides::SelfCheckResult result = AeroraQmlOverrides::verify();

    // Passing trivially is the honest outcome, not a weak assertion: with nothing declared there is
    // nothing that could have failed to take effect. The assertion that earns its place is that the
    // check reports no missing resource while also reporting that it examined an empty set.
    QVERIFY(result.passed);
    QVERIFY(result.missingMirroredResources.isEmpty());
}

void AeroraUiFoundationTest::_uiContextIsUnavailableWithNoActiveVehicle()
{
    AeroraVehicleUiContext context;

    // No vehicle is connected in a unit-test run, which is also the state the interface is in for
    // most of its life. Every accessor must answer, and answer null, rather than assume a vehicle.
    QVERIFY(!context.isAvailable());
    QVERIFY(context.identity() == nullptr);
    QVERIFY(context.capabilities() == nullptr);
    QVERIFY(context.payload() == nullptr);
    QVERIFY(context.power() == nullptr);
    QVERIFY(context.mission() == nullptr);

    // Unknown rather than a default that reads as information: SelectedBySeam would wrongly imply
    // QGroundControl had chosen the Aerora plugin for something.
    QCOMPARE(context.classification(), AeroraVehicleServices::Classification::Unknown);

    // Empty rather than a placeholder profile id. An empty id shows no product surface at all,
    // which is what keeps painting from becoming the default.
    QVERIFY(context.productProfileId().isEmpty());
}

void AeroraUiFoundationTest::_uiContextDoesNotExposeTheVehicleOrTheServiceMap()
{
    const QMetaObject *const meta = &AeroraVehicleUiContext::staticMetaObject;

    // AeroraVehicleServices::service(QString) is a string-keyed QObject * lookup. Keeping it out of
    // the QML surface is the difference between "the interface can read the services it was
    // designed around" and "the interface can reach anything anyone attaches later". A typed
    // property fails to compile when its service changes shape; a string lookup returns null.
    QStringList declaredMethodNames;
    for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
        declaredMethodNames.append(QString::fromUtf8(meta->method(i).name()));
    }

    for (const char *forbidden : {"service", "serviceNames", "attachService"}) {
        const QString name = QString::fromUtf8(forbidden);
        QVERIFY2(!declaredMethodNames.contains(name), forbidden);
        QVERIFY2(meta->indexOfProperty(forbidden) == -1, forbidden);
    }

    // Vehicle is reachable from QML already, as QGroundControl.multiVehicleManager.activeVehicle.
    // Re-exposing it here would add a second route to the same writable object and widen the Aerora
    // surface for no gain.
    QCOMPARE(meta->indexOfProperty("vehicle"), -1);
    QCOMPARE(meta->indexOfProperty("services"), -1);

    // The surface that is intended, asserted positively so a property being dropped is a failure
    // rather than a silent loss of a binding.
    for (const char *expected :
         {"available", "identity", "capabilities", "payload", "power", "mission",
          "classification", "productProfileId"}) {
        QVERIFY2(meta->indexOfProperty(expected) != -1, expected);
    }
}

void AeroraUiFoundationTest::_interceptorIsNotInstalledBeforeInstall()
{
    // Asserted rather than assumed, because every test below depends on it and because it records
    // the real property: nothing installs the interceptor implicitly. It reaches an engine only
    // through AeroraCorePlugin::createQmlApplicationEngine(), which a unit-test run never calls.
    QVERIFY(!AeroraQmlOverrides::isInstalled());
}

void AeroraUiFoundationTest::_installOnNullEngineDoesNotInstall()
{
    expectLogMessage(kUiLogCategory, QtWarningMsg, QRegularExpression(QStringLiteral("null engine")));
    AeroraQmlOverrides::installOn(nullptr);
    verifyExpectedLogMessage();

    // The interesting half: a refused install must leave no interceptor behind, or the next
    // install would be refused as a duplicate and the real engine would never get one.
    QVERIFY(!AeroraQmlOverrides::isInstalled());
}

void AeroraUiFoundationTest::_installThenReleaseTracksInstallationState()
{
    QQmlApplicationEngine engine;

    AeroraQmlOverrides::installOn(&engine);
    QVERIFY2(AeroraQmlOverrides::isInstalled(), "install must be observable through isInstalled()");

    AeroraQmlOverrides::releaseFrom(&engine);
    QVERIFY2(!AeroraQmlOverrides::isInstalled(), "release must clear the installed state");

    // Install is not single-shot. The engine is created once per application run today, but the
    // hook is a virtual the base may call again, and a release that poisoned the state would turn
    // that into silently missing Aerora UI.
    AeroraQmlOverrides::installOn(&engine);
    QVERIFY(AeroraQmlOverrides::isInstalled());
    AeroraQmlOverrides::releaseFrom(&engine);
    QVERIFY(!AeroraQmlOverrides::isInstalled());
}

void AeroraUiFoundationTest::_secondInstallIsRefusedAndLeavesTheFirstInstalled()
{
    QQmlApplicationEngine firstEngine;
    QQmlApplicationEngine secondEngine;

    AeroraQmlOverrides::installOn(&firstEngine);
    QVERIFY(AeroraQmlOverrides::isInstalled());

    expectLogMessage(kUiLogCategory, QtWarningMsg, QRegularExpression(QStringLiteral("already installed")));
    AeroraQmlOverrides::installOn(&secondEngine);
    verifyExpectedLogMessage();

    // Refusing is only half the contract. The first engine must keep its interceptor, so a stray
    // second install cannot strip Aerora UI from the engine that is actually displaying it.
    QVERIFY(AeroraQmlOverrides::isInstalled());

    // And the refused engine must not have acquired ownership of it: releasing the engine that was
    // turned away is a no-op.
    AeroraQmlOverrides::releaseFrom(&secondEngine);
    QVERIFY2(AeroraQmlOverrides::isInstalled(),
             "releasing a refused engine must not release the first engine's interceptor");

    AeroraQmlOverrides::releaseFrom(&firstEngine);
    QVERIFY(!AeroraQmlOverrides::isInstalled());
}

void AeroraUiFoundationTest::_releaseFromAnUnrelatedEngineIsANoOp()
{
    QQmlApplicationEngine installedEngine;
    QQmlApplicationEngine unrelatedEngine;

    AeroraQmlOverrides::installOn(&installedEngine);

    // The out-of-order teardown property. QGCApplication routes destruction through the destroy
    // hook (src/QGCApplication.cc:705-710) precisely so parent-based teardown cannot bypass it; if
    // release matched on anything looser than engine identity, an unrelated engine's teardown
    // would delete this engine's interceptor while it was still in use.
    AeroraQmlOverrides::releaseFrom(&unrelatedEngine);
    QVERIFY(AeroraQmlOverrides::isInstalled());

    AeroraQmlOverrides::releaseFrom(&installedEngine);
    QVERIFY(!AeroraQmlOverrides::isInstalled());
}

void AeroraUiFoundationTest::_releaseWithNothingInstalledIsANoOp()
{
    QQmlApplicationEngine engine;

    // Silent, not warned: a release with nothing installed is the normal shutdown path for any
    // build where AERORA_UI_SOURCES is absent, and warning there would be noise rather than news.
    AeroraQmlOverrides::releaseFrom(&engine);
    AeroraQmlOverrides::releaseFrom(nullptr);

    QVERIFY(!AeroraQmlOverrides::isInstalled());
}

void AeroraUiFoundationTest::_enforcementIndicatorBindingNamesResolve()
{
    const QMetaObject *const meta = &AeroraSecurityStatus::staticMetaObject;

    // AeroraEnforcementIndicator.qml reads AeroraSecurity.enforcement, so the registered QML name
    // is part of the binding rather than a registration detail. QML_NAMED_ELEMENT expands to a
    // Q_CLASSINFO("QML.Element", …), which is the same public meta-object route AeroraUiSafetyTest
    // takes rather than the private QQmlMetaType.
    QString qmlName;
    for (int i = meta->classInfoOffset(); i < meta->classInfoCount(); ++i) {
        if (qstrcmp(meta->classInfo(i).name(), "QML.Element") == 0) {
            qmlName = QString::fromUtf8(meta->classInfo(i).value());
            break;
        }
    }

    QCOMPARE(qmlName, QStringLiteral("AeroraSecurity"));

    // Asserted positively, for the same reason the AeroraUi property set is: a rename must be a
    // failure here rather than a silently dropped binding.
    QVERIFY2(meta->indexOfProperty("enforcement") != -1,
             "AeroraEnforcementIndicator.qml reads AeroraSecurity.enforcement");

    const int enumIndex = meta->indexOfEnumerator("Enforcement");
    QVERIFY(enumIndex >= 0);

    // The indicator compares against the *enforced* enumerator and renders its negation as "not
    // enforced". If this name stopped resolving, the comparison would yield `undefined === value`
    // and the strip would report not enforced - the safe direction, and therefore a silent one.
    // That is precisely why the name is asserted: the fail-safe hides the breakage from the
    // operator, so a test has to be what notices.
    bool resolved = false;
    (void) meta->enumerator(enumIndex).keyToValue("EnforcedAtCommandGate", &resolved);

    QVERIFY2(resolved,
             "AeroraEnforcementIndicator.qml compares against AeroraSecurity.EnforcedAtCommandGate");
}

void AeroraUiFoundationTest::_missionProgressEnumeratorsResolve_data()
{
    QTest::addColumn<QByteArray>("enumName");
    QTest::addColumn<QByteArray>("enumeratorName");

    // Exactly the enumerators AeroraMissionProgressPage.qml names, and no others. The page reads
    // transferActivity, lastOperation and lastOutcome by matching the positive values; it never
    // compares against a NotEvidenced* enumerator, so none is asserted here.
    QTest::newRow("transfer in progress") << QByteArray("TransferActivity") << QByteArray("InProgress");
    QTest::newRow("transfer idle") << QByteArray("TransferActivity") << QByteArray("Idle");
    QTest::newRow("operation download") << QByteArray("LastOperation") << QByteArray("Download");
    QTest::newRow("operation upload") << QByteArray("LastOperation") << QByteArray("Upload");
    QTest::newRow("operation remove all") << QByteArray("LastOperation") << QByteArray("RemoveAll");
    QTest::newRow("outcome succeeded") << QByteArray("LastOutcome") << QByteArray("Succeeded");
    QTest::newRow("outcome failed") << QByteArray("LastOutcome") << QByteArray("Failed");
}

void AeroraUiFoundationTest::_missionProgressEnumeratorsResolve()
{
    QFETCH(QByteArray, enumName);
    QFETCH(QByteArray, enumeratorName);

    const QMetaObject *const meta = &AeroraMissionObserver::staticMetaObject;

    const int enumIndex = meta->indexOfEnumerator(enumName.constData());
    QVERIFY2(enumIndex >= 0, qPrintable(QStringLiteral("AeroraMissionObserver.%1 does not resolve")
                                            .arg(QString::fromUtf8(enumName))));

    // The page matches every one of these positively and renders anything unmatched as "not
    // evidenced". That direction is the honest one - a broken name cannot make the screen claim a
    // transfer state nobody observed - but it is therefore also silent, so this test is what
    // notices. Same reasoning as the enforcement strip above, with more names to lose.
    bool resolved = false;
    (void) meta->enumerator(enumIndex).keyToValue(enumeratorName.constData(), &resolved);

    QVERIFY2(resolved,
             qPrintable(QStringLiteral("AeroraMissionProgressPage.qml compares against "
                                       "AeroraMissionObserver.%1")
                            .arg(QString::fromUtf8(enumeratorName))));
}

void AeroraUiFoundationTest::_missionProgressPropertiesResolve_data()
{
    QTest::addColumn<QByteArray>("className");
    QTest::addColumn<QByteArray>("propertyName");

    // The observer's own properties, read through AeroraUi.mission.
    const QByteArray observer("AeroraMissionObserver");
    QTest::newRow("vehicleCurrentIndex") << observer << QByteArray("vehicleCurrentIndex");
    QTest::newRow("resumeCandidate") << observer << QByteArray("resumeCandidate");
    QTest::newRow("planViewSelection") << observer << QByteArray("planViewSelection");
    QTest::newRow("vehicleMission") << observer << QByteArray("vehicleMission");
    QTest::newRow("gcsPlan") << observer << QByteArray("gcsPlan");
    QTest::newRow("transferActivity") << observer << QByteArray("transferActivity");
    QTest::newRow("lastOperation") << observer << QByteArray("lastOperation");
    QTest::newRow("lastOutcome") << observer << QByteArray("lastOutcome");
    QTest::newRow("vehicleMissionIndeterminate") << observer << QByteArray("vehicleMissionIndeterminate");

    // The gadget properties reached one level further in. These are the page's substitute for the
    // gadget enumerators, which it cannot name: AeroraMissionIndex and AeroraMissionSnapshot carry
    // no QML value-type registration, so the bool convenience properties are the whole mechanism
    // by which the page distinguishes "nothing observed" from "observed and absent".
    const QByteArray index("AeroraMissionIndex");
    QTest::newRow("index attributed") << index << QByteArray("attributed");
    QTest::newRow("index known") << index << QByteArray("known");
    QTest::newRow("index vehicleAuthoritative") << index << QByteArray("vehicleAuthoritative");
    QTest::newRow("index sequenceNumber") << index << QByteArray("sequenceNumber");

    const QByteArray snapshot("AeroraMissionSnapshot");
    QTest::newRow("snapshot captured") << snapshot << QByteArray("captured");
    QTest::newRow("snapshot itemCount") << snapshot << QByteArray("itemCount");
}

void AeroraUiFoundationTest::_missionProgressPropertiesResolve()
{
    QFETCH(QByteArray, className);
    QFETCH(QByteArray, propertyName);

    const QMetaObject *const meta = missionMetaObjectFor(className);
    QVERIFY2(meta != nullptr, qPrintable(QStringLiteral("unmapped class name %1 in the data table")
                                             .arg(QString::fromUtf8(className))));

    // Asserted positively, like the AeroraUi property set: a renamed or dropped property makes a
    // QML binding evaluate to undefined, which the page renders as a benign-looking row rather
    // than as an error.
    QVERIFY2(meta->indexOfProperty(propertyName.constData()) != -1,
             qPrintable(QStringLiteral("AeroraMissionProgressPage.qml reads %1.%2")
                            .arg(QString::fromUtf8(className), QString::fromUtf8(propertyName))));
}

void AeroraUiFoundationTest::_serviceDiagnosticsRegisteredTypeNameMatchesThePageBinding()
{
    const QMetaObject* const meta = &AeroraVehicleServices::staticMetaObject;

    // AeroraServiceDiagnosticsPage.qml writes the type name out - `AeroraVehicleServices.
    // CorroboratedAerora` - so the registered QML name is part of the binding, not a registration
    // detail. A switch to QML_NAMED_ELEMENT would leave the C++ untouched, the enumerators intact
    // and every comparison on the page permanently false, which renders as "classification
    // unknown" on a vehicle that was classified.
    //
    // Read through the QML.Element class info rather than QQmlMetaType, which is private - the
    // same public route AeroraUiSafetyTest takes.
    QString qmlName;
    for (int i = meta->classInfoOffset(); i < meta->classInfoCount(); ++i) {
        if (qstrcmp(meta->classInfo(i).name(), "QML.Element") == 0) {
            qmlName = QString::fromUtf8(meta->classInfo(i).value());
            break;
        }
    }

    // "auto" is what bare QML_ELEMENT emits, and it means qmltyperegistrar registers the type
    // under its class name - which is the name the page writes. QML_NAMED_ELEMENT would put the
    // registered name here instead, so this comparison is what catches the switch.
    QCOMPARE(qmlName, QStringLiteral("auto"));

    // The class name is the other half of that: "auto" only says "use the class name", so the
    // class name itself has to be the string the page spells.
    QCOMPARE(QString::fromUtf8(meta->className()), QStringLiteral("AeroraVehicleServices"));
}

void AeroraUiFoundationTest::_serviceDiagnosticsEnumeratorsResolve_data()
{
    QTest::addColumn<QByteArray>("enumeratorName");

    // Exactly the three AeroraServiceDiagnosticsPage.qml names, and no others. The page matches
    // positively and renders anything unmatched as unknown, so Classification::Unknown is never
    // compared against and is not asserted here.
    QTest::newRow("corroborated") << QByteArray("CorroboratedAerora");
    QTest::newRow("selected by seam") << QByteArray("SelectedBySeam");
    QTest::newRow("not aerora") << QByteArray("NotAerora");
}

void AeroraUiFoundationTest::_serviceDiagnosticsEnumeratorsResolve()
{
    QFETCH(QByteArray, enumeratorName);

    const QMetaObject* const meta = &AeroraVehicleServices::staticMetaObject;

    const int enumIndex = meta->indexOfEnumerator("Classification");
    QVERIFY2(enumIndex >= 0, "AeroraVehicleServices.Classification does not resolve");

    bool resolved = false;
    (void) meta->enumerator(enumIndex).keyToValue(enumeratorName.constData(), &resolved);

    QVERIFY2(resolved, qPrintable(QStringLiteral("AeroraServiceDiagnosticsPage.qml compares against "
                                                 "AeroraVehicleServices.%1")
                                      .arg(QString::fromUtf8(enumeratorName))));
}

void AeroraUiFoundationTest::_serviceDiagnosticsPageDocumentCompiles()
{
    QVERIFY2(QFile::exists(QString::fromUtf8(kServiceDiagnosticsResource)),
             "the setup page must be compiled into the resource at the flat module path");

    // The production factory rather than a bare QQmlEngine: it is what adds the qrc:/qml import
    // path that `import QGroundControl` and the flat Aerora.Controls module resolve from, and it is
    // the AeroraCorePlugin override, so the engine is configured as the application's is.
    QQmlApplicationEngine* const engine = QGCCorePlugin::instance()->createQmlApplicationEngine(this);
    QVERIFY(engine);

    bool compiled = false;
    QString errors;

    // Scoped so the component is destroyed while its engine is still alive. QQmlComponent keeps a
    // back-pointer to the engine and reads it on destruction, so the reverse order is a
    // use-after-free rather than merely untidy.
    {
        QQmlComponent component(engine, QUrl(QString::fromUtf8(kServiceDiagnosticsPageUrl)));

        // Compilation, which is more than it sounds: Qt resolves every referenced type and every
        // property name on a QML-declared type at this point. So this covers the resource alias,
        // the import set, SetupPage coming from QGroundControl.Controls, the implicit reach to
        // AeroraSelfReportRow, and a mistyped property on any of them.
        compiled = !component.isError();
        errors = component.errorString();
    }

    // Released before the assertion, so a failing compile cannot leave the URL interceptor
    // installed for the rest of the fixture - the interceptor is process-wide file-static state
    // and a second install is refused.
    QGCCorePlugin::instance()->destroyQmlApplicationEngine(engine);

    QVERIFY2(compiled, qPrintable(errors));

    // What this does *not* cover, stated because the gap is easy to miss: no binding is evaluated,
    // because create() is never called. SetupPage.qml reads `globals` and `vehicleComponent` from
    // context that only VehicleConfigView's Loader supplies, so instantiating the page here would
    // report that absence as a failure of the page - and supplying a stand-in would assert the
    // stand-in. Binding evaluation and rendering are evidenced by neither this test nor CI.
}

void AeroraUiFoundationTest::_missionGadgetValuesAreReadableFromQml_data()
{
    QTest::addColumn<QByteArray>("expression");
    QTest::addColumn<QString>("expected");

    // Compared as strings so an int property arriving as a JavaScript number still matches. Each
    // row asserts that the value resolved and carried the right content, not which numeric type
    // happened to carry it.
    QTest::newRow("index attributed") << QByteArray("observer.vehicleCurrentIndex.attributed")
                                      << QStringLiteral("true");
    QTest::newRow("index known") << QByteArray("observer.vehicleCurrentIndex.known") << QStringLiteral("true");
    QTest::newRow("index sequence number")
        << QByteArray("observer.vehicleCurrentIndex.sequenceNumber") << QStringLiteral("7");
    QTest::newRow("index vehicle authoritative")
        << QByteArray("observer.vehicleCurrentIndex.vehicleAuthoritative") << QStringLiteral("true");

    // Never reported, so this is the page's "not evidenced" path. It must read false rather than
    // anything an operator could mistake for a proposal.
    QTest::newRow("resume candidate unattributed")
        << QByteArray("observer.resumeCandidate.attributed") << QStringLiteral("false");
    QTest::newRow("plan view selection unattributed")
        << QByteArray("observer.planViewSelection.attributed") << QStringLiteral("false");

    QTest::newRow("vehicle mission captured")
        << QByteArray("observer.vehicleMission.captured") << QStringLiteral("true");
    QTest::newRow("vehicle mission item count")
        << QByteArray("observer.vehicleMission.itemCount") << QStringLiteral("1");

    // The plan side has no production feed at all, so an uncaptured snapshot is the state the page
    // renders in every session. Absent must not read as an empty plan.
    QTest::newRow("gcs plan not captured") << QByteArray("observer.gcsPlan.captured") << QStringLiteral("false");

    QTest::newRow("determinacy") << QByteArray("observer.vehicleMissionIndeterminate") << QStringLiteral("false");

    // Negative control, so the rows above are not tautologies: a property that does not exist must
    // not come back looking like a value. A null expected string selects this branch.
    QTest::newRow("nonexistent property") << QByteArray("observer.vehicleCurrentIndex.noSuchProperty") << QString();
}

void AeroraUiFoundationTest::_missionGadgetValuesAreReadableFromQml()
{
    QFETCH(QByteArray, expression);
    QFETCH(QString, expected);

    // Only qCDebug fires on these paths, so strict log checking is not engaged. The qCWarning
    // paths - a transfer error, a wrong-origin plan, an append to an unattributed snapshot - are
    // all deliberately avoided here.
    AeroraMissionObserver observer;
    observer.reportVehicleCurrentIndex(7);

    AeroraMissionSnapshot vehicleMission = AeroraMissionSnapshot::empty(AeroraMissionSnapshot::VehicleStore);
    AeroraMissionItemFingerprint item;
    item.sequenceNumber = 0;
    item.command = MAV_CMD_NAV_WAYPOINT;
    vehicleMission.append(item);
    observer.reportDownloadComplete(vehicleMission);

    QQmlEngine engine;
    QQmlContext context(&engine);
    context.setContextProperty(QStringLiteral("observer"), &observer);

    QQmlExpression expr(&context, nullptr, QString::fromUtf8(expression));
    const QVariant actual = expr.evaluate();

    if (expected.isNull()) {
        QVERIFY2(actual.toString().isEmpty(),
                 qPrintable(QStringLiteral("a nonexistent gadget property returned %1").arg(actual.toString())));
        return;
    }

    QVERIFY2(!expr.hasError(), qPrintable(expr.error().toString()));
    QCOMPARE(actual.toString(), expected);
}

void AeroraUiFoundationTest::_missionAgreementVerdictIsReadableFromQml()
{
    AeroraMissionItemFingerprint item;
    item.sequenceNumber = 0;
    item.command = MAV_CMD_NAV_WAYPOINT;

    AeroraMissionObserver observer;

    AeroraMissionSnapshot vehicleMission = AeroraMissionSnapshot::empty(AeroraMissionSnapshot::VehicleStore);
    vehicleMission.append(item);
    observer.reportDownloadComplete(vehicleMission);

    QQmlEngine engine;
    QQmlContext context(&engine);
    context.setContextProperty(QStringLiteral("observer"), &observer);

    // The exact expression AeroraMissionProgressPage.qml evaluates, spelled out rather than
    // assembled, so a change to the page's reading has to be mirrored here deliberately.
    const QString reading = QStringLiteral("observer.agreement.isVerified()");

    QQmlExpression beforeComparison(&context, nullptr, reading);
    const QVariant unverified = beforeComparison.evaluate();
    QVERIFY2(!beforeComparison.hasError(), qPrintable(beforeComparison.error().toString()));
    QCOMPARE(unverified.toString(), QStringLiteral("false"));

    AeroraMissionSnapshot plan = AeroraMissionSnapshot::empty(AeroraMissionSnapshot::GcsPlan);
    plan.append(item);
    observer.reportGcsPlan(plan);

    // From C++, because the page must never call verify(): the comparison is the observer's, and
    // a screen that triggers it is a screen that decides when the answer is current.
    QCOMPARE(observer.verify(AeroraMissionAgreement::DepthItemCount).verdict(), AeroraMissionAgreement::Matches);

    QQmlExpression afterComparison(&context, nullptr, reading);
    const QVariant verified = afterComparison.evaluate();
    QVERIFY2(!afterComparison.hasError(), qPrintable(afterComparison.error().toString()));
    QCOMPARE(verified.toString(), QStringLiteral("true"));
}

UT_REGISTER_TEST(AeroraUiFoundationTest, TestLabel::Unit)

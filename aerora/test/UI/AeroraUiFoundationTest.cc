#include "AeroraUiFoundationTest.h"

#include <QtCore/QMetaMethod>
#include <QtCore/QMetaObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtQml/QQmlApplicationEngine>

#include "AeroraQmlOverrides.h"
#include "AeroraVehicleUiContext.h"

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

UT_REGISTER_TEST(AeroraUiFoundationTest, TestLabel::Unit)

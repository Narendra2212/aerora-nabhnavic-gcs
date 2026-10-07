#include "AeroraUiFoundationTest.h"

#include <QtCore/QMetaMethod>
#include <QtCore/QMetaObject>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

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

UT_REGISTER_TEST(AeroraUiFoundationTest, TestLabel::Unit)

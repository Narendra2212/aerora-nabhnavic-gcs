#include "AeroraVehicleSetupTest.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtTest/QTest>

#include "AeroraFirmwarePlugin.h"
#include "AeroraToolIndicators.h"
#include "AeroraVehicleCapabilities.h"
#include "AeroraVehicleComponent.h"

using Category = AeroraVehicleCapabilities::Category;
using IndicatorDescriptor = AeroraToolIndicators::IndicatorDescriptor;
using Page = AeroraVehicleComponent::Page;
using PageDescriptor = AeroraVehicleComponent::PageDescriptor;
using State = AeroraVehicleCapabilities::State;

namespace {

constexpr const char *kLogCategory = "Aerora.Vehicle";

/// A resource path that cannot exist, for the negative half of the existence gate.
constexpr const char *kAbsentResource = ":/qml/Aerora/Controls/ThisResourceIsNotInTheBuild.qml";

} // namespace

QString AeroraVehicleSetupTest::_anyExistingQmlResource()
{
    if (_existingQmlResourceProbed) {
        return _existingQmlResource;
    }
    _existingQmlResourceProbed = true;

    // Probed rather than named, so the test does not break when an upstream QML file is moved or
    // renamed. Any real resource proves the same property.
    QDirIterator iterator(QStringLiteral(":/qml"), {QStringLiteral("*.qml")}, QDir::Files,
                          QDirIterator::Subdirectories);
    if (iterator.hasNext()) {
        _existingQmlResource = iterator.next();
    }

    return _existingQmlResource;
}

void AeroraVehicleSetupTest::_pageCatalogueCoversTheProductScreensAndWithholdsCalibration()
{
    const QList<PageDescriptor> descriptors = AeroraVehicleComponent::pageDescriptors();

    QList<Page> declaredPages;
    QStringList declaredScreenIds;
    for (const PageDescriptor &descriptor : descriptors) {
        declaredPages.append(descriptor.page);
        declaredScreenIds.append(descriptor.screenId);

        // A descriptor with no panel resource could never be published, so it would be a silent
        // dead entry rather than a page.
        QVERIFY2(!descriptor.setupResource.isEmpty(),
                 qPrintable(QStringLiteral("page declares no setup resource: %1").arg(descriptor.screenId)));
    }

    // The four screens whose backing C++ exists. SCR-VEH-01 is this list itself, not a member.
    QCOMPARE(declaredScreenIds,
             QStringList({QStringLiteral("SCR-VEH-02"),
                          QStringLiteral("SCR-VEH-03"),
                          QStringLiteral("SCR-VEH-04"),
                          QStringLiteral("SCR-VEH-06")}));

    // SCR-VEH-05. Calibration is the one SCR-VEH-* screen that acts on the aircraft, and its
    // authorization gate permits everything while Q-029 is open. Publishing it would present a
    // gated operation whose gate evaluates nothing, so the absence is asserted rather than left to
    // whoever next reads the catalogue and notices a gap in the numbering.
    QVERIFY(!declaredPages.contains(Page::Calibration));
    QVERIFY(!AeroraVehicleComponent::withheldPageReason(Page::Calibration).isEmpty());

    // Every page that *is* declared must not also claim to be withheld.
    for (const Page page : declaredPages) {
        QVERIFY2(AeroraVehicleComponent::withheldPageReason(page).isEmpty(),
                 qPrintable(QStringLiteral("page is both declared and withheld: %1").arg(static_cast<int>(page))));
    }

    // SCR-VEH-04 is the JSON-driven candidate and carries no page definition yet: a
    // *.VehicleConfig.json names real vehicle parameters, and no Aerora parameter is evidenced.
    // This fails when a JSON is wired in, which is the point at which the parameter set has to
    // have been approved.
    for (const PageDescriptor &descriptor : descriptors) {
        QVERIFY2(descriptor.configJsonResource.isEmpty(),
                 qPrintable(QStringLiteral("page declares a VehicleConfig.json with no evidenced "
                                           "parameter set: %1")
                                .arg(descriptor.screenId)));
    }
}

void AeroraVehicleSetupTest::_resourceResolutionRejectsAbsentAndNonResourcePaths_data()
{
    QTest::addColumn<QString>("resourcePath");
    QTest::addColumn<bool>("expectResolved");

    QTest::newRow("empty") << QString() << false;
    QTest::newRow("absentResource") << QString::fromUtf8(kAbsentResource) << false;
    // A filesystem path must not be rewritten into a qrc URL: it would resolve to nothing and
    // report as a missing QML type rather than as a bad path.
    QTest::newRow("filesystemPath") << QStringLiteral("/tmp/NotAResource.qml") << false;
    // Nor may an already-schemed URL be accepted: prefixing it again yields "qrcqrc:/...".
    QTest::newRow("qrcSchemeRatherThanResourcePath")
        << QStringLiteral("qrc:/qml/QGroundControl/Toolbar/BatteryIndicator.qml") << false;

    // The positive row, so the expectation column distinguishes rather than restates. Probed
    // rather than named, for the reason given on _anyExistingQmlResource().
    const QString existing = _anyExistingQmlResource();
    if (!existing.isEmpty()) {
        QTest::newRow("presentResource") << existing << true;
    }
}

void AeroraVehicleSetupTest::_resourceResolutionRejectsAbsentAndNonResourcePaths()
{
    QFETCH(QString, resourcePath);
    QFETCH(bool, expectResolved);

    const QUrl resolved = AeroraVehicleComponent::resolvedQmlUrl(resourcePath);
    QCOMPARE(!resolved.isEmpty(), expectResolved);
}

void AeroraVehicleSetupTest::_aPageIsPublishableOnlyWhenEveryDeclaredResourceExists()
{
    const QString existing = _anyExistingQmlResource();
    if (existing.isEmpty()) {
        QSKIP("No QML resource in this build to exercise the positive half of the gate");
    }

    // The positive case, and the proof that resolution turns a resource path into a loadable URL.
    PageDescriptor publishable;
    publishable.page = Page::Identity;
    publishable.screenId = QStringLiteral("SCR-VEH-02");
    publishable.setupResource = existing;
    QVERIFY(AeroraVehicleComponent::isPublishable(publishable));

    // The transform the QML engine depends on: a ':'-rooted resource path becomes the same path
    // under the qrc scheme. Getting this wrong yields a URL that resolves to nothing and reports
    // as a missing QML type rather than as a bad path.
    const QUrl resolved = AeroraVehicleComponent::resolvedQmlUrl(existing);
    QCOMPARE(resolved.scheme(), QStringLiteral("qrc"));
    QCOMPARE(resolved.path(), existing.mid(1));

    // A missing panel withholds the page. This is the case that exists today.
    PageDescriptor absentPanel = publishable;
    absentPanel.setupResource = QString::fromUtf8(kAbsentResource);
    QVERIFY(!AeroraVehicleComponent::isPublishable(absentPanel));

    PageDescriptor noPanel = publishable;
    noPanel.setupResource = QString();
    QVERIFY(!AeroraVehicleComponent::isPublishable(noPanel));

    // An optional resource may be left undeclared; a declared-and-absent one withholds the page.
    // The distinction matters most for configJsonResource: the base class opens that path itself
    // and, on failure, warns once and derives no sections, so the page would render without its
    // sidebar and nothing on screen would say why.
    const auto publishableWith = [&publishable](QString PageDescriptor::*field, const QString &value) {
        PageDescriptor descriptor = publishable;
        descriptor.*field = value;
        return AeroraVehicleComponent::isPublishable(descriptor);
    };

    for (QString PageDescriptor::*field : {&PageDescriptor::summaryResource,
                                           &PageDescriptor::configJsonResource,
                                           &PageDescriptor::iconResource}) {
        QVERIFY(publishableWith(field, QString()));
        QVERIFY(publishableWith(field, existing));
        QVERIFY(!publishableWith(field, QString::fromUtf8(kAbsentResource)));
    }
}

void AeroraVehicleSetupTest::_pagePublishabilityFollowsItsQmlResource()
{
    // Written as a matched pair rather than as a count, so it holds as the Aerora setup QML lands
    // one file at a time and the reason stays attached: a page is unpublishable *because* its
    // resource is missing, and unresolvedPageResources() names which.
    //
    // A matched pair cannot say which pages exist, and must not try - that is the next test.
    const QStringList unresolved = AeroraVehicleComponent::unresolvedPageResources();

    for (const PageDescriptor &descriptor : AeroraVehicleComponent::pageDescriptors()) {
        const bool publishable = AeroraVehicleComponent::isPublishable(descriptor);
        const bool panelResolves = !AeroraVehicleComponent::resolvedQmlUrl(descriptor.setupResource).isEmpty();

        // The catalogue declares no optional resources, so publishability is exactly "the panel
        // resolves". A page that reports a loadable panel it would not publish, or the reverse,
        // means the gate and the URL the view loads have drifted apart.
        QVERIFY2(publishable == panelResolves,
                 qPrintable(QStringLiteral("page publishability disagrees with its panel URL: %1")
                                .arg(descriptor.screenId)));

        // And the reason is recorded: an unpublishable page names the resource it is waiting for.
        QVERIFY2(publishable == !unresolved.contains(descriptor.setupResource),
                 qPrintable(QStringLiteral("withheld page is not reported as unresolved: %1")
                                .arg(descriptor.screenId)));
    }
}

void AeroraVehicleSetupTest::_publishedPagesAreNamedAndTheRestAreWithheld()
{
    // The state of U4 in this build, named rather than derived. SCR-VEH-02, SCR-VEH-03 and
    // SCR-VEH-06 have their QML and publish; SCR-VEH-04 is declared and waiting for its own.
    //
    // Naming it is the point. Publishability is "does this resource exist", so the failure that
    // matters most here is the one where the resource moves - a changed RESOURCE_PREFIX, a dropped
    // qgc_set_qt_resource_alias() call, a file removed from AERORA_UI_QML_FILES. Any of those
    // withholds the page, and every other test in this fixture is written as a matched pair that
    // moves with it. This one does not move: the paths are spelled out, so the Aerora setup
    // surface cannot disappear from the Vehicle Configuration view without a test failing.
    // Screen ids as well as resources: the ids say which screen the operator gets, the resources
    // say which file the Loader opens, and only asserting both catches a descriptor that publishes
    // the right screen id against the wrong QML.
    QStringList publishedScreenIds;
    QStringList publishedResources;
    for (const PageDescriptor& descriptor : AeroraVehicleComponent::pageDescriptors()) {
        if (!AeroraVehicleComponent::isPublishable(descriptor)) {
            continue;
        }
        publishedScreenIds.append(descriptor.screenId);
        publishedResources.append(descriptor.setupResource);
    }

    QCOMPARE(publishedScreenIds,
             QStringList({QStringLiteral("SCR-VEH-02"), QStringLiteral("SCR-VEH-03"), QStringLiteral("SCR-VEH-06")}));
    QCOMPARE(publishedResources,
             QStringList({QStringLiteral(":/qml/Aerora/Controls/AeroraIdentityPage.qml"),
                          QStringLiteral(":/qml/Aerora/Controls/AeroraCapabilityPage.qml"),
                          QStringLiteral(":/qml/Aerora/Controls/AeroraServiceDiagnosticsPage.qml")}));

    // And the one that is still waiting says so, by resource, so this fails when it lands rather
    // than letting the published set grow unremarked.
    QCOMPARE(AeroraVehicleComponent::unresolvedPageResources(),
             QStringList({QStringLiteral(":/qml/Aerora/Controls/AeroraHealthPage.qml")}));
}

void AeroraVehicleSetupTest::_indicatorGateRequiresCapabilityReportedAvailable_data()
{
    QTest::addColumn<bool>("requireCapability");
    QTest::addColumn<bool>("useInventory");
    QTest::addColumn<bool>("doReport");
    QTest::addColumn<State>("reportState");
    QTest::addColumn<bool>("expectPublishable");

    QTest::newRow("unconditionalWithNoInventory") << false << false << false << State::NotEvidenced << true;
    QTest::newRow("unconditionalWithInventory") << false << true << false << State::NotEvidenced << true;
    QTest::newRow("requiredAndReportedAvailable") << true << true << true << State::Available << true;
    QTest::newRow("requiredAndReportedUnavailable") << true << true << true << State::Unavailable << false;
    // The case the gate exists for: the inventory object is attached and has reported nothing.
    // Existence of the object is not evidence of the capability.
    QTest::newRow("requiredAndNeverReported") << true << true << false << State::NotEvidenced << false;
    QTest::newRow("requiredWithNoInventory") << true << false << false << State::NotEvidenced << false;
}

void AeroraVehicleSetupTest::_indicatorGateRequiresCapabilityReportedAvailable()
{
    QFETCH(bool, requireCapability);
    QFETCH(bool, useInventory);
    QFETCH(bool, doReport);
    QFETCH(State, reportState);
    QFETCH(bool, expectPublishable);

    const QString existing = _anyExistingQmlResource();
    if (existing.isEmpty()) {
        QSKIP("No QML resource in this build to isolate the capability half of the gate");
    }

    // The key is opaque and test-local. No Aerora capability identifier is evidenced, so the test
    // must not introduce one that could be mistaken for a real key.
    const QString capabilityKey = QStringLiteral("test-local-opaque-capability-key");

    AeroraVehicleCapabilities inventory;
    if (doReport) {
        QVERIFY(inventory.report(Category::Vehicle, capabilityKey, reportState));
    }

    IndicatorDescriptor descriptor;
    descriptor.screenId = QStringLiteral("SCR-SHL-02");
    descriptor.qmlResource = existing;
    descriptor.capabilityCategory = Category::Vehicle;
    if (requireCapability) {
        descriptor.capabilityKey = capabilityKey;
    }

    const AeroraVehicleCapabilities *const capabilities = useInventory ? &inventory : nullptr;

    QCOMPARE(AeroraToolIndicators::isPublishable(descriptor, capabilities), expectPublishable);

    // urlsFor() must agree with the gate rather than apply a second, looser rule.
    QCOMPARE(AeroraToolIndicators::urlsFor({descriptor}, capabilities).count(), expectPublishable ? 1 : 0);
}

void AeroraVehicleSetupTest::_indicatorUrlsFollowDeclarationOrderAndSkipWithheldEntries()
{
    const QString existing = _anyExistingQmlResource();
    if (existing.isEmpty()) {
        QSKIP("No QML resource in this build to construct a publishable indicator");
    }

    IndicatorDescriptor first;
    first.screenId = QStringLiteral("first");
    first.qmlResource = existing;

    IndicatorDescriptor withheld;
    withheld.screenId = QStringLiteral("withheld");
    withheld.qmlResource = QString::fromUtf8(kAbsentResource);

    IndicatorDescriptor last;
    last.screenId = QStringLiteral("last");
    last.qmlResource = existing;

    const QVariantList urls = AeroraToolIndicators::urlsFor({first, withheld, last}, nullptr);

    // A withheld entry is skipped, not replaced by a placeholder: an empty QUrl in the list would
    // reach FlyViewToolBarIndicators.qml, which binds `visible: item.showIndicator` on the loaded
    // item with no null guard.
    QCOMPARE(urls.count(), 2);
    const QUrl expected = AeroraVehicleComponent::resolvedQmlUrl(existing);
    QVERIFY(!expected.isEmpty());
    QCOMPARE(urls.at(0).toUrl(), expected);
    QCOMPARE(urls.at(1).toUrl(), expected);
}

void AeroraVehicleSetupTest::_noIndicatorIsDeclaredWhileNoneIsEvidenced()
{
    // Two independent prerequisites are absent: there is no Aerora toolbar QML, and no Aerora
    // capability identifier is evidenced (Q3-009) to gate one on. Declaring an entry now would
    // mean inventing a capability key.
    //
    // Asserted so that adding the first indicator is a deliberate change to a test that states
    // why the set was empty, rather than an unremarked edit.
    QVERIFY(AeroraToolIndicators::declaredIndicators().isEmpty());

    // Vacuously true today. It becomes the real assertion the moment an entry is declared: a
    // declared indicator whose QML is missing is loaded and fails, so it must be caught here.
    QVERIFY(AeroraToolIndicators::unresolvedIndicatorResources().isEmpty());
}

void AeroraVehicleSetupTest::_autopilotPluginRefusesANullVehicle()
{
    AeroraFirmwarePlugin plugin;

    // AutoPilotPlugin's constructor reads vehicle->firmwarePlugin() unguarded, so the base
    // implementation would dereference null here. Vehicle always passes itself, which is why this
    // is reachable only from a direct call - and why it returns null rather than constructing.
    expectLogMessage(kLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("cannot create the Aerora autopilot plugin")));
    QCOMPARE(plugin.autopilotPlugin(nullptr), nullptr);
    verifyExpectedLogMessage();
}

void AeroraVehicleSetupTest::_toolIndicatorsLeaveTheBaseListUnchangedWhileNoneIsDeclared()
{
    AeroraFirmwarePlugin plugin;

    const QVariantList indicators = plugin.toolIndicators(nullptr);

    // The base set is preserved. An override that returned an empty or truncated list would remove
    // the GPS, battery and link indicators from every Aerora vehicle.
    QVERIFY(!indicators.isEmpty());

    // And nothing Aerora was appended, which is the honest consequence of declaring no indicator.
    // When the first one is declared this loop must be relaxed to allow its URL.
    for (const QVariant &indicator : indicators) {
        const QString url = indicator.toUrl().toString();
        QVERIFY2(url.startsWith(QStringLiteral("qrc:/qml/QGroundControl/")),
                 qPrintable(QStringLiteral("unexpected non-base tool indicator: %1").arg(url)));
    }
}

UT_REGISTER_TEST(AeroraVehicleSetupTest, TestLabel::Unit)

#include "AeroraVehicleLifecycleTest.h"

#include <QtCore/QList>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtTest/QTest>

#include "AeroraAutoPilotPlugin.h"
#include "AeroraFirmwarePlugin.h"
#include "AeroraMissionBridge.h"
#include "AeroraMissionObserver.h"
#include "AeroraPayloadModel.h"
#include "AeroraPowerModel.h"
#include "AeroraTelemetrySet.h"
#include "AeroraToolIndicators.h"
#include "AeroraVehicleCapabilities.h"
#include "AeroraVehicleComponent.h"
#include "AeroraVehicleIdentity.h"
#include "AeroraVehicleServices.h"
#include "JoystickComponent.h"
#include "MissionManager.h"
#include "Vehicle.h"
#include "VehicleComponent.h"

void AeroraVehicleLifecycleTest::init()
{
    VehicleTest::init();

    // Suppressed rather than expected, and not by preference. Both warnings are emitted by the
    // vehicle connect inside VehicleTest::init() above, which has already returned by the time
    // this line runs, and expectLogMessage() only matches messages captured *after* the call that
    // registers it - it records the captured-message count as its start index (UnitTest.h).
    // VehicleTest::init() calls UnitTest::init(), which clears the expectation lists, and connects
    // in the same step, so a subclass has no seam in which to register an expectation first.
    // ignoreLogMessage() carries no index and is applied in cleanup() to every captured message,
    // which is why it works from here. Without both, every function in this suite fails strict
    // log checking even though its assertions pass.

    // Unexpectable on its own terms as well: latched once per process by
    // AeroraCorePlugin::_warnedUnconfigured, so it fires on the first command the first connect
    // queues and never again. Only whichever function runs first observes it, so asserting it
    // would be asserting something absent for every later function.
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("allowing commands without authorization")));

    // Emitted on every connect - the generic MockLink serves no component metadata. Upstream
    // suppresses this same warning at all of its generic-autopilot call sites, including ones that
    // do have a seam, because the metadata request completes asynchronously with respect to
    // initialConnectComplete and so cannot be tied to a precise call site.
    ignoreLogMessage("ComponentInformation.RequestMetaDataTypeStateMachine", QtWarningMsg,
                     QRegularExpression(QStringLiteral("failed to load metadata")));
}

void AeroraVehicleLifecycleTest::_aeroraServicesAttachToADiscoveredVehicle()
{
    QVERIFY(vehicle());

    // The seam worked: the self-registering factory was not discarded by the linker and the
    // manager routed this heartbeat to the Aerora plugin rather than to the generic fallback.
    QVERIFY(qobject_cast<AeroraFirmwarePlugin *>(vehicle()->firmwarePlugin()));

    // initializeVehicle() ran and attached the per-vehicle services object.
    AeroraVehicleServices *const services = AeroraVehicleServices::forVehicle(vehicle());
    QVERIFY(services);

    // Parenting is the lifetime guarantee. Asserting the parent asserts that the services object,
    // and everything attached to it, is destroyed with the vehicle and needs no teardown code.
    QCOMPARE(services->parent(), static_cast<QObject *>(vehicle()));

    // Idempotent: a repeated initialize cannot produce a second competing instance.
    QCOMPARE(AeroraVehicleServices::attachTo(vehicle()), services);

    // Protocol identity is adopted from the vehicle.
    QCOMPARE(services->identity()->mavlinkSystemId(), vehicle()->id());
    QCOMPARE(services->identity()->mavlinkComponentId(), vehicle()->defaultComponentId());

    // Fleet identity is not. A connected vehicle with a known system id still has no fleet
    // identity, because no source has reported one and the system id is not a substitute.
    QVERIFY(!services->identity()->fleetIdentityKnown());
    QVERIFY(services->identity()->fleetIdentity().isEmpty());

    // Plugin selection is a build-configuration outcome. It must not be recorded as evidence that
    // this aircraft is Aerora hardware.
    QCOMPARE(services->classification(), AeroraVehicleServices::Classification::SelectedBySeam);

    // Nothing has reported a capability, so nothing is presented as one.
    QVERIFY(!services->capabilities()->hasAnyReport());

    // Q4-002 is closed in favour of per-vehicle registration (ADR-016), and the wiring is live.
    // The FORBIDDEN path is asserted absent here; the CORRECT path is asserted present in
    // _domainServicesAttachToADiscoveredVehicle.
    //
    // FORBIDDEN: FirmwarePlugin::factGroups() -> Vehicle::_commonInit() -> vehicle fact-group map.
    // The plugin is cached per firmware class, so a non-null return here would hand the same group
    // objects to every vehicle of that class and the aircraft would overwrite each other's
    // telemetry (R4-001). Asserting nullptr is the structural guard against that architecture
    // being wired back in, and it is why AeroraFirmwarePlugin does not override factGroups().
    QCOMPARE(vehicle()->firmwarePlugin()->factGroups(), nullptr);
}

void AeroraVehicleLifecycleTest::_domainServicesAttachToADiscoveredVehicle()
{
    QVERIFY(vehicle());

    AeroraVehicleServices *const services = AeroraVehicleServices::forVehicle(vehicle());
    QVERIFY(services);

    // All four domain services are composed by initializeVehicle(), in dependency order.
    auto *const telemetry = qobject_cast<AeroraTelemetrySet *>(
        services->service(QString::fromUtf8(AeroraTelemetrySet::kServiceName)));
    auto *const payload =
        qobject_cast<AeroraPayloadModel *>(services->service(QString::fromUtf8(AeroraPayloadModel::kServiceName)));
    auto *const power =
        qobject_cast<AeroraPowerModel *>(services->service(QString::fromUtf8(AeroraPowerModel::kServiceName)));

    QVERIFY(telemetry);
    QVERIFY(payload);
    QVERIFY(power);

    // The mission observer is reached through the mission module's own entry point rather than by
    // a service-name literal, so this asserts the published contract and not an implementation
    // detail of the registry.
    AeroraMissionObserver *const missionObserver = AeroraMissionBridge::observerFor(vehicle());
    QVERIFY(missionObserver);
    QCOMPARE(services->service(QString::fromUtf8(AeroraMissionObserver::kServiceName)),
             static_cast<QObject *>(missionObserver));

    // The bridge requires a MissionManager, which _commonInit() builds before initializeVehicle()
    // runs. That ordering is what makes mission attachment possible from this seam at all.
    QVERIFY(vehicle()->missionManager());

    // Everything is parented into the vehicle's services object, so vehicle destruction destroys
    // the whole graph and no module needs teardown code.
    QCOMPARE(telemetry->parent(), static_cast<QObject *>(services));
    QCOMPARE(payload->parent(), static_cast<QObject *>(services));
    QCOMPARE(power->parent(), static_cast<QObject *>(services));
    QCOMPARE(missionObserver->parent(), static_cast<QObject *>(services));

    // Each model received this vehicle's own freshness evaluator, not a process-wide one.
    QVERIFY(telemetry->freshness());

    // CORRECT path, and the assertion that closes the composition-root gap: the root invoked
    // AeroraTelemetrySet::publishTo(), which published through AeroraVehicleFactGroupRegistrar to
    // this vehicle's own fact-group map. isPublished() is set only by publishTo() succeeding, so
    // it cannot be true unless the root actually called it.
    QVERIFY(telemetry->isPublished());

    // Publication seals the set, which is what makes "Aerora adds no fact group dynamically"
    // checkable: registerGroup() refuses from here on, so the set the CSV header is written from is
    // the set that stays (R-027).
    QVERIFY(telemetry->isSealed());

    // FORBIDDEN path still absent. Published or not, groups must never arrive via the shared
    // plugin seam.
    QCOMPARE(vehicle()->firmwarePlugin()->factGroups(), nullptr);

    // What was published is nothing, and that is the honest state rather than a wiring failure: no
    // Aerora dialect exists, so AeroraTelemetrySet declares no concrete group. Publishing an empty
    // set succeeds and seals it.
    //
    // These two assertions are therefore a matched pair. The vehicle map carries no aerora* group
    // *because* the set is empty - not because publication was skipped, which isPublished() above
    // rules out. When a concrete group is introduced, groupNames() becomes non-empty and this loop
    // must be inverted to assert the group's presence in the map.
    QVERIFY(telemetry->groupNames().isEmpty());
    for (const QString &factGroupName : vehicle()->factGroupNames()) {
        QVERIFY2(!factGroupName.startsWith(QStringLiteral("aerora"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("unexpected Aerora fact group in the vehicle map: %1").arg(factGroupName)));
    }

    // Composition opened no outbound path on either supervisory model.
    QVERIFY(!payload->isCommandPathInstalled());
    QVERIFY(!payload->controlsPermitted());
    QVERIFY(!power->isExtendedSurfacePermitted());
}

void AeroraVehicleLifecycleTest::_missionBridgeIsAttachedByTheLifecycleAndOnlyOnce()
{
    QVERIFY(vehicle());

    AeroraVehicleServices *const services = AeroraVehicleServices::forVehicle(vehicle());
    QVERIFY(services);

    // AeroraMissionBridge::attachTo() is called from AeroraFirmwarePlugin::initializeVehicle(),
    // the one lifecycle seam that runs once per vehicle and receives the Vehicle*. Nothing else in
    // the overlay calls it, so a refactor that drops the call produces a vehicle with no mission
    // observation at all - and does so silently, because the observer's absence is a null return
    // rather than an error.
    AeroraMissionObserver *const observer = AeroraMissionBridge::observerFor(vehicle());
    QVERIFY(observer);

    // The observer alone does not prove the bridge exists. attachTo() has an explicit path where
    // an observer is attached and the bridge is not, and in that state nothing reports into the
    // observer: the mission signals are connected by the bridge, which parents itself to the
    // observer. So the bridge's presence is what makes "mission signals are observed" checkable.
    const QList<AeroraMissionBridge *> bridges = observer->findChildren<AeroraMissionBridge *>();
    QCOMPARE(bridges.count(), 1);

    const QStringList serviceNamesBefore = services->serviceNames();

    // Idempotent, per the published contract: a repeated attach returns the existing bridge rather
    // than connecting a second one to the same observer, which would double-report every mission
    // signal into one consumer.
    QCOMPARE(AeroraMissionBridge::attachTo(vehicle()), bridges.first());
    QCOMPARE(observer->findChildren<AeroraMissionBridge *>().count(), 1);
    QCOMPARE(AeroraMissionBridge::observerFor(vehicle()), observer);

    // And it attached no second service under a second name.
    QCOMPARE(services->serviceNames(), serviceNamesBefore);
}

void AeroraVehicleLifecycleTest::_autopilotPluginPublishesOnlyPagesThatExist()
{
    QVERIFY(vehicle());

    // The seam worked: AeroraFirmwarePlugin::autopilotPlugin() was consulted by the Vehicle
    // constructor, so the Vehicle Configuration view builds its tree from the Aerora page index
    // rather than from the generic fallback.
    auto *const autopilotPlugin = qobject_cast<AeroraAutoPilotPlugin *>(vehicle()->autopilotPlugin());
    QVERIFY(autopilotPlugin);

    // Owned by the vehicle, which re-parents whatever autopilotPlugin() returned and deletes it in
    // its own destructor. Asserting the parent asserts that the plugin and its components need no
    // teardown code.
    QCOMPARE(autopilotPlugin->parent(), static_cast<QObject *>(vehicle()));

    const QVariantList components = autopilotPlugin->vehicleComponents();

    // Built once. A second call that rebuilt would parent another full set of components onto the
    // plugin on every query, which the setup view makes on every rebind.
    QCOMPARE(autopilotPlugin->vehicleComponents().count(), components.count());

    int aeroraPages = 0;
    bool joystickPagePresent = false;
    for (const QVariant &componentVariant : components) {
        auto *const component = qobject_cast<VehicleComponent *>(qvariant_cast<QObject *>(componentVariant));
        QVERIFY(component);

        // A published page must have a loadable panel. VehicleConfigView loads setupSource() into
        // a Loader, so an empty or unresolvable URL is a blank panel with no error on screen.
        QVERIFY(!component->setupSource().isEmpty());

        // No published page gates another. These are displays, so there is no setup order.
        QVERIFY(autopilotPlugin->prerequisiteSetup(component).isEmpty());

        if (qobject_cast<AeroraVehicleComponent *>(component)) {
            aeroraPages++;

            // An Aerora page is read-only. requiresSetup() drives both the "setup required" badge
            // and parametersReadyPreChecks()'s "configuration tasks remain before this vehicle is
            // ready to fly" message, neither of which may be raised for a page that has no action
            // an operator could take.
            QVERIFY(!component->requiresSetup());
            QVERIFY(component->setupComplete());

            // No parameter trigger list: no Aerora parameter is evidenced, and a read-only page
            // has no completeness to re-evaluate.
            QVERIFY(component->setupCompleteChangedTriggerList().isEmpty());
        } else if (qobject_cast<JoystickComponent *>(component)) {
            joystickPagePresent = true;
        }
    }

    // The joystick page survives the override. FirmwarePlugin::autopilotPlugin() returns a
    // GenericAutoPilotPlugin by default and that page is its entire content, so an override which
    // did not reproduce it would remove joystick setup from every Aerora vehicle - a regression
    // with no error attached to it.
    QVERIFY(joystickPagePresent);

    // The published Aerora page count is exactly what the catalogue's gate says is publishable.
    // Written as a matched pair rather than as "zero" so the two cannot drift: when the Aerora
    // setup QML lands, both sides move together and this still holds.
    int publishablePages = 0;
    for (const AeroraVehicleComponent::PageDescriptor &descriptor : AeroraVehicleComponent::pageDescriptors()) {
        if (AeroraVehicleComponent::isPublishable(descriptor)) {
            publishablePages++;
        }
    }
    QCOMPARE(aeroraPages, publishablePages);
}

void AeroraVehicleLifecycleTest::_toolIndicatorsAreResolvedPerVehicle()
{
    QVERIFY(vehicle());

    const QVariantList indicators = vehicle()->toolIndicators();

    // The base set reaches the toolbar through the Aerora override. An override that replaced the
    // list instead of appending to it would remove the GPS, battery and link indicators.
    QVERIFY(!indicators.isEmpty());

    // Nothing Aerora is appended, because nothing is declared: there is no Aerora toolbar QML, and
    // no Aerora capability identifier is evidenced to gate one on. Asserted as a matched pair with
    // the declared set so that declaring the first indicator is what changes this, rather than an
    // unrelated edit.
    QVERIFY(AeroraToolIndicators::declaredIndicators().isEmpty());
    for (const QVariant &indicator : indicators) {
        const QString url = indicator.toUrl().toString();
        QVERIFY2(url.startsWith(QStringLiteral("qrc:/qml/QGroundControl/")),
                 qPrintable(QStringLiteral("unexpected non-base tool indicator: %1").arg(url)));
    }

    // The capability inventory is attached and has reported nothing, which is the state in which a
    // capability-gated indicator must stay withheld. The gate is exercised directly in
    // AeroraVehicleSetupTest; here the point is that a vehicle reaching the toolbar in this state
    // produces no Aerora entry from the existence of the inventory object alone.
    AeroraVehicleServices *const services = AeroraVehicleServices::forVehicle(vehicle());
    QVERIFY(services);
    QVERIFY(services->capabilities());
    QVERIFY(!services->capabilities()->hasAnyReport());
    QVERIFY(AeroraToolIndicators::urlsFor(services->capabilities()).isEmpty());
}

UT_REGISTER_TEST(AeroraVehicleLifecycleTest, TestLabel::Integration, TestLabel::Vehicle)

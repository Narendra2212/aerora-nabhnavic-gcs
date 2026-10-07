#include "AeroraVehicleLifecycleTest.h"

#include <QtTest/QTest>

#include "AeroraFirmwarePlugin.h"
#include "AeroraMissionBridge.h"
#include "AeroraMissionObserver.h"
#include "AeroraPayloadModel.h"
#include "AeroraPowerModel.h"
#include "AeroraTelemetrySet.h"
#include "AeroraVehicleCapabilities.h"
#include "AeroraVehicleIdentity.h"
#include "AeroraVehicleServices.h"
#include "MissionManager.h"
#include "Vehicle.h"

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

UT_REGISTER_TEST(AeroraVehicleLifecycleTest, TestLabel::Integration, TestLabel::Vehicle)

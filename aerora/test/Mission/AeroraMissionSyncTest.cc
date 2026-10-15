#include "AeroraMissionSyncTest.h"

#include <QtCore/QRegularExpression>
#include <QtCore/QScopeGuard>
#include <QtCore/QStringList>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "AeroraMissionBridge.h"
#include "AeroraMissionObserver.h"
#include "AeroraMissionSnapshot.h"
#include "AeroraVehicleServices.h"
#include "MissionItem.h"
#include "MissionManager.h"
#include "Vehicle.h"

void AeroraMissionSyncTest::init()
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

void AeroraMissionSyncTest::_missionObservationAttachesToARealVehicle()
{
    QVERIFY(vehicle());

    AeroraMissionBridge *const bridge = AeroraMissionBridge::attachTo(vehicle());
    QVERIFY(bridge);

    AeroraMissionObserver *const observer = bridge->observer();
    QVERIFY(observer);

    // Attached through the established per-vehicle seam, under the documented service name, rather
    // than through a mission-specific mechanism of its own.
    AeroraVehicleServices *const services = AeroraVehicleServices::forVehicle(vehicle());
    QVERIFY(services);
    QCOMPARE(services->service(QString::fromUtf8(AeroraMissionObserver::kServiceName)),
             static_cast<QObject *>(observer));

    // Parenting is the lifetime guarantee: Vehicle -> services -> observer -> bridge, so the whole
    // graph is destroyed with the vehicle and needs no teardown code.
    QCOMPARE(observer->parent(), static_cast<QObject *>(services));
    QCOMPARE(bridge->parent(), static_cast<QObject *>(observer));

    // Idempotent. A second attach must not connect a second bridge, which would double-report
    // every mission signal into one observer.
    QCOMPARE(AeroraMissionBridge::attachTo(vehicle()), bridge);
    QCOMPARE(AeroraMissionBridge::observerFor(vehicle()), observer);

    // The bridge feeds the vehicle side only. The plan side belongs to whoever holds the
    // PlanMasterController, so attaching to a vehicle must not fabricate a plan or a selection.
    QVERIFY(!observer->gcsPlan().isCaptured());
    QVERIFY(!observer->planViewSelection().isAttributed());
}

void AeroraMissionSyncTest::_aRealUploadReplacesTheVehicleMissionAndResetsTheCurrentIndex()
{
    QVERIFY(vehicle());

    MissionManager *const missionManager = vehicle()->missionManager();
    QVERIFY(missionManager);

    AeroraMissionBridge *const bridge = AeroraMissionBridge::attachTo(vehicle());
    QVERIFY(bridge);

    AeroraMissionObserver *const observer = bridge->observer();
    QVERIFY(observer);

    // Record every index the aircraft side reports, rather than asserting the state at the end.
    // MockLink may report a new current index at any point after the write completes, and the
    // contract under test is the reset itself, not what happens to follow it.
    QStringList reportedIndices;
    const QMetaObject::Connection reportedIndexConnection =
        connect(observer, &AeroraMissionObserver::vehicleCurrentIndexChanged, observer, [&]() {
            reportedIndices.append(observer->vehicleCurrentIndex().toString());
        });

    // The connection is owned by the observer, which the vehicle keeps alive into cleanup(), while
    // the lambda writes to a stack local. MockLink keeps reporting indices until the vehicle is
    // torn down, so a report arriving after this function returns would write into a dead frame.
    // A scope guard rather than a trailing disconnect, because every assertion below leaves this
    // function early on failure. Declared after the connection and holding its own copy of it, so
    // it runs before reportedIndices is destroyed and before the QVERIFY below can return.
    const auto reportedIndexGuard =
        qScopeGuard([reportedIndexConnection]() { QObject::disconnect(reportedIndexConnection); });

    // An invalid connection would leave reportedIndices empty and surface as a confusing failure
    // at the reset assertion far below, rather than here.
    QVERIFY(reportedIndexConnection);

    // writeMissionItems() is a no-op while a transaction is in progress, and the automatic plan
    // request at connect may still be running.
    QVERIFY_TRUE_WAIT(!missionManager->inProgress(), TestTimeout::longMs());

    // Three plain waypoints. No Aerora command, no complex item and no painting geometry: none is
    // evidenced, and this test is about the transfer contract rather than about mission content.
    QList<MissionItem *> items;
    for (int sequenceNumber = 0; sequenceNumber < 3; sequenceNumber++) {
        items.append(new MissionItem(sequenceNumber,
                                     MAV_CMD_NAV_WAYPOINT,
                                     MAV_FRAME_GLOBAL_RELATIVE_ALT,
                                     0.0,
                                     0.0,
                                     0.0,
                                     0.0,
                                     0.0,
                                     0.0,
                                     20.0,
                                     true /* autoContinue */,
                                     sequenceNumber == 0 /* isCurrentItem */));
    }

    QSignalSpy sendCompleteSpy(missionManager, &MissionManager::sendComplete);

    // PlanManager takes ownership of the items, so they must not be deleted here.
    missionManager->writeMissionItems(items);

    QVERIFY_SIGNAL_WAIT(sendCompleteSpy, TestTimeout::longMs());
    QCOMPARE(sendCompleteSpy.count(), 1);
    QVERIFY(!sendCompleteSpy.first().first().toBool());

    QCOMPARE(observer->lastOperation(), AeroraMissionObserver::Upload);
    QCOMPARE(observer->lastOutcome(), AeroraMissionObserver::Succeeded);
    QCOMPARE(observer->transferActivity(), AeroraMissionObserver::Idle);
    QVERIFY(!observer->isVehicleMissionIndeterminate());

    // The vehicle mission was captured from the vehicle side. Its item count is deliberately not
    // pinned to three: whether the leading item is consumed as a home position depends on
    // FirmwarePlugin::sendHomePositionToVehicle(), which is not what this test is about.
    QVERIFY(observer->vehicleMission().isCaptured());
    QCOMPARE(observer->vehicleMission().origin(), AeroraMissionSnapshot::VehicleStore);
    QVERIFY(observer->vehicleMission().itemCount() > 0);

    // Mission replacement resets the vehicle's current index. The reset is reported as attributed
    // to the vehicle but carrying no position, so it can be presented as neither item 0 nor a
    // stale previous index.
    QVERIFY2(reportedIndices.contains(QStringLiteral("vehicleReported:none")),
             qPrintable(QStringLiteral("expected a current-index reset, observed: %1")
                            .arg(reportedIndices.join(QStringLiteral(",")))));

    // A successful upload is not a verified upload, and the bridge never fed the plan side, so
    // there is nothing here that could have been compared against.
    QCOMPARE(observer->agreement().verdict(), AeroraMissionAgreement::NotCompared);
    QVERIFY(!observer->agreement().isVerified());
    QVERIFY(!observer->gcsPlan().isCaptured());
}

UT_REGISTER_TEST(AeroraMissionSyncTest, TestLabel::Integration, TestLabel::Vehicle, TestLabel::MissionManager)

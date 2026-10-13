#include "AeroraMissionTest.h"

#include <QtCore/QRegularExpression>
#include <QtTest/QTest>

#include "AeroraMissionIndex.h"
#include "AeroraMissionMock.h"
#include "AeroraMissionObserver.h"
#include "AeroraMissionResumeBoundary.h"
#include "AeroraMissionSnapshot.h"

namespace {

constexpr const char *kLogCategory = "Aerora.Mission";

/// Data columns carry command ids as `int` so the table relies only on built-in metatypes.
QList<MAV_CMD> toCommands(const QList<int> &commandIds)
{
    QList<MAV_CMD> commands;
    commands.reserve(commandIds.count());

    for (const int commandId : commandIds) {
        commands.append(static_cast<MAV_CMD>(commandId));
    }

    return commands;
}

QList<int> toCommandIds(const QList<MAV_CMD> &commands)
{
    QList<int> commandIds;
    commandIds.reserve(commands.count());

    for (const MAV_CMD command : commands) {
        commandIds.append(static_cast<int>(command));
    }

    return commandIds;
}

} // namespace

void AeroraMissionTest::cleanup()
{
    // The extension registry is process-global. Clearing it before the stubs are destroyed keeps
    // a dangling registration from leaking into any later test.
    AeroraMissionExtensionRegistry::instance()->clear();

    qDeleteAll(_stubExtensions);
    _stubExtensions.clear();

    UnitTest::cleanup();
}

void AeroraMissionTest::_missionIndexKeepsOriginsApart()
{
    // The same sequence number from three different sources must not be three equal values. This
    // is the whole reason the type exists: in QGroundControl all three are plain ints.
    const AeroraMissionIndex vehicleIndex = AeroraMissionIndex::fromVehicleReport(4);
    const AeroraMissionIndex selection = AeroraMissionIndex::fromPlanViewSelection(4);
    const AeroraMissionIndex resumeCandidate = AeroraMissionIndex::resumeCandidate(4);

    QVERIFY(vehicleIndex != selection);
    QVERIFY(vehicleIndex != resumeCandidate);
    QVERIFY(selection != resumeCandidate);

    // Only the aircraft's report has authority over where the aircraft is.
    QVERIFY(vehicleIndex.isVehicleAuthoritative());
    QVERIFY(!selection.isVehicleAuthoritative());
    QVERIFY(!resumeCandidate.isVehicleAuthoritative());

    // A default-constructed index is not position zero.
    const AeroraMissionIndex unattributed;
    QVERIFY(!unattributed.isAttributed());
    QVERIFY(!unattributed.isKnown());
    QCOMPARE(unattributed.sequenceNumber(), -1);

    // "No current index" is attributed but unknown. A successful mission write produces exactly
    // this state, and rendering it as item 0 would tell the operator the aircraft is at the start
    // of a mission it has not reported on yet.
    const AeroraMissionIndex noCurrentIndex = AeroraMissionIndex::fromVehicleReport(-1);
    QVERIFY(noCurrentIndex.isAttributed());
    QVERIFY(!noCurrentIndex.isKnown());
    QVERIFY(noCurrentIndex.isVehicleAuthoritative());
    QVERIFY(noCurrentIndex != unattributed);

    // Every negative report normalises, so isKnown() cannot depend on which negative arrived.
    QCOMPARE(AeroraMissionIndex::fromVehicleReport(-7).toString(), noCurrentIndex.toString());
}

void AeroraMissionTest::_snapshotComparisonRefusesWrongOriginsAndUncapturedSides()
{
    const AeroraMissionSnapshot plan =
        AeroraMissionMock::buildMission(AeroraMissionSnapshot::GcsPlan, {MAV_CMD_NAV_WAYPOINT});
    const AeroraMissionSnapshot vehicleMission =
        AeroraMissionMock::buildMission(AeroraMissionSnapshot::VehicleStore, {MAV_CMD_NAV_WAYPOINT});

    // The supported direction agrees.
    const AeroraMissionAgreement supportedDirection =
        AeroraMissionSnapshot::compare(plan, vehicleMission, AeroraMissionAgreement::DepthFullParameters);
    QCOMPARE(supportedDirection.verdict(), AeroraMissionAgreement::Matches);

    // Comparing a plan against a plan answers no question anyone asked. Returning Matches for it
    // would be the worst available failure, so it must refuse instead.
    const QRegularExpression wrongOrigins(QStringLiteral("refusing mission comparison with wrong origins"));

    expectLogMessage(kLogCategory, QtWarningMsg, wrongOrigins);
    QCOMPARE(AeroraMissionSnapshot::compare(plan, plan, AeroraMissionAgreement::DepthFullParameters).verdict(),
             AeroraMissionAgreement::NotCompared);
    verifyExpectedLogMessage();

    // Reversed arguments are also refused rather than silently compared.
    expectLogMessage(kLogCategory, QtWarningMsg, wrongOrigins);
    const AeroraMissionAgreement reversedArguments =
        AeroraMissionSnapshot::compare(vehicleMission, plan, AeroraMissionAgreement::DepthFullParameters);
    QCOMPARE(reversedArguments.verdict(), AeroraMissionAgreement::NotCompared);
    verifyExpectedLogMessage();

    // Absent is not empty. An uncaptured side cannot be shown to agree with anything.
    expectLogMessage(kLogCategory, QtWarningMsg, wrongOrigins);
    QCOMPARE(
        AeroraMissionSnapshot::compare(AeroraMissionSnapshot(), vehicleMission, AeroraMissionAgreement::DepthItemCount)
            .verdict(),
        AeroraMissionAgreement::NotCompared);
    verifyExpectedLogMessage();

    // An empty-but-captured side is a different thing, and compares normally.
    const AeroraMissionSnapshot emptyPlan = AeroraMissionSnapshot::empty(AeroraMissionSnapshot::GcsPlan);
    const AeroraMissionSnapshot emptyVehicle = AeroraMissionSnapshot::empty(AeroraMissionSnapshot::VehicleStore);
    QVERIFY(emptyPlan.isCaptured());
    QCOMPARE(emptyPlan.itemCount(), 0);
    QCOMPARE(
        AeroraMissionSnapshot::compare(emptyPlan, emptyVehicle, AeroraMissionAgreement::DepthFullParameters).verdict(),
        AeroraMissionAgreement::Matches);

    // A verdict carries the depth that produced it, so "matches" is never read as more than it is.
    const AeroraMissionAgreement shallow =
        AeroraMissionSnapshot::compare(plan, vehicleMission, AeroraMissionAgreement::DepthItemCount);
    QCOMPARE(shallow.depth(), AeroraMissionAgreement::DepthItemCount);
    QVERIFY(shallow.isVerified());

    // DepthNone performs nothing and must not claim agreement.
    QVERIFY(!AeroraMissionSnapshot::compare(plan, vehicleMission, AeroraMissionAgreement::DepthNone).isVerified());
}

void AeroraMissionTest::_agreementVerdicts_data()
{
    QTest::addColumn<QList<int>>("planCommands");
    QTest::addColumn<QList<int>>("vehicleCommands");
    QTest::addColumn<int>("depth");
    QTest::addColumn<int>("expectedVerdict");

    const QList<int> threeItems =
        toCommandIds({MAV_CMD_NAV_TAKEOFF, MAV_CMD_NAV_WAYPOINT, MAV_CMD_NAV_RETURN_TO_LAUNCH});
    const QList<int> truncated = toCommandIds({MAV_CMD_NAV_TAKEOFF, MAV_CMD_NAV_WAYPOINT});
    const QList<int> reordered =
        toCommandIds({MAV_CMD_NAV_TAKEOFF, MAV_CMD_NAV_RETURN_TO_LAUNCH, MAV_CMD_NAV_WAYPOINT});

    QTest::newRow("identical at count")
        << threeItems << threeItems << int(AeroraMissionAgreement::DepthItemCount)
        << int(AeroraMissionAgreement::Matches);
    QTest::newRow("identical at commands")
        << threeItems << threeItems << int(AeroraMissionAgreement::DepthCommandSequence)
        << int(AeroraMissionAgreement::Matches);
    QTest::newRow("identical at parameters")
        << threeItems << threeItems << int(AeroraMissionAgreement::DepthFullParameters)
        << int(AeroraMissionAgreement::Matches);

    // Truncation is the one thing even the shallowest depth catches.
    QTest::newRow("truncation at count")
        << threeItems << truncated << int(AeroraMissionAgreement::DepthItemCount)
        << int(AeroraMissionAgreement::Differs);

    // Reordering survives a count-only comparison and is caught one level down. This is exactly
    // why the depth is part of the verdict rather than something the caller remembers.
    QTest::newRow("reorder at count")
        << threeItems << reordered << int(AeroraMissionAgreement::DepthItemCount)
        << int(AeroraMissionAgreement::Matches);
    QTest::newRow("reorder at commands")
        << threeItems << reordered << int(AeroraMissionAgreement::DepthCommandSequence)
        << int(AeroraMissionAgreement::Differs);
}

void AeroraMissionTest::_agreementVerdicts()
{
    QFETCH(QList<int>, planCommands);
    QFETCH(QList<int>, vehicleCommands);
    QFETCH(int, depth);
    QFETCH(int, expectedVerdict);

    const auto requestedDepth = static_cast<AeroraMissionAgreement::Depth>(depth);
    const auto expected = static_cast<AeroraMissionAgreement::Verdict>(expectedVerdict);

    const AeroraMissionSnapshot plan =
        AeroraMissionMock::buildMission(AeroraMissionSnapshot::GcsPlan, toCommands(planCommands));
    const AeroraMissionSnapshot vehicleMission =
        AeroraMissionMock::buildMission(AeroraMissionSnapshot::VehicleStore, toCommands(vehicleCommands));

    const AeroraMissionAgreement agreement = AeroraMissionSnapshot::compare(plan, vehicleMission, requestedDepth);

    QCOMPARE(agreement.verdict(), expected);
    QCOMPARE(agreement.depth(), requestedDepth);
    QCOMPARE(agreement.isVerified(), expected == AeroraMissionAgreement::Matches);
}

void AeroraMissionTest::_agreementToleratesDifferencesBelowWirePrecision()
{
    // A plan parameter is a full-precision double. The same parameter returned from the vehicle
    // has been through MISSION_ITEM_INT, which truncates params 1-4 and 7 to float and quantises
    // params 5 and 6 to 1e-7 degrees. Comparing raw doubles would report a difference for almost
    // every mission that round-tripped correctly, which would make verification useless.
    AeroraMissionItemFingerprint planItem;
    planItem.sequenceNumber = 0;
    planItem.command = MAV_CMD_NAV_WAYPOINT;
    planItem.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
    planItem.autoContinue = true;
    planItem.param5 = 47.39774189999999;
    planItem.param6 = 8.54559380000001;
    planItem.param7 = 30.000000001;

    // The same item after a round trip: the coordinates land on the same 1e-7 quantum and the
    // altitude on the same float.
    AeroraMissionItemFingerprint vehicleItem = planItem;
    vehicleItem.param5 = 47.3977418;
    vehicleItem.param6 = 8.5455938;
    vehicleItem.param7 = 30.0;

    AeroraMissionSnapshot plan = AeroraMissionSnapshot::empty(AeroraMissionSnapshot::GcsPlan);
    plan.append(planItem);

    AeroraMissionSnapshot vehicleMission = AeroraMissionSnapshot::empty(AeroraMissionSnapshot::VehicleStore);
    vehicleMission.append(vehicleItem);

    const AeroraMissionAgreement roundTripped =
        AeroraMissionSnapshot::compare(plan, vehicleMission, AeroraMissionAgreement::DepthFullParameters);
    QCOMPARE(roundTripped.verdict(), AeroraMissionAgreement::Matches);

    // A difference the aircraft can actually see is still reported. A whole 1e-7 quantum of
    // latitude is roughly a centimetre, which is above wire precision rather than below it.
    AeroraMissionItemFingerprint shiftedItem = vehicleItem;
    shiftedItem.param5 = 47.3977419;

    AeroraMissionSnapshot shiftedMission = AeroraMissionSnapshot::empty(AeroraMissionSnapshot::VehicleStore);
    shiftedMission.append(shiftedItem);

    const AeroraMissionAgreement shifted =
        AeroraMissionSnapshot::compare(plan, shiftedMission, AeroraMissionAgreement::DepthFullParameters);
    QCOMPARE(shifted.verdict(), AeroraMissionAgreement::Differs);
    QCOMPARE(shifted.firstDifferenceIndex(), 0);

    // The same altered parameter is invisible one depth up, which is the cost of the cheaper
    // comparison and is why the depth travels with the verdict.
    QCOMPARE(
        AeroraMissionSnapshot::compare(plan, shiftedMission, AeroraMissionAgreement::DepthCommandSequence).verdict(),
        AeroraMissionAgreement::Matches);
}

void AeroraMissionTest::_resumeBoundaryMirrorsUpstreamAndCarriesNoPayloadCommand()
{
    const QList<MAV_CMD> &replayed = AeroraMissionResumeBoundary::replayedCommands();

    // Pinned count. A silent edit to the mirror fails here rather than passing unnoticed.
    QCOMPARE(replayed.count(), AeroraMissionResumeBoundary::kReplayedCommandCount);

    // The mirror is exactly upstream's set, in upstream's order.
    const QList<MAV_CMD> expected = {MAV_CMD_DO_CONTROL_VIDEO,
                                     MAV_CMD_DO_SET_ROI,
                                     MAV_CMD_DO_DIGICAM_CONFIGURE,
                                     MAV_CMD_DO_DIGICAM_CONTROL,
                                     MAV_CMD_DO_MOUNT_CONFIGURE,
                                     MAV_CMD_DO_MOUNT_CONTROL,
                                     MAV_CMD_DO_SET_CAM_TRIGG_DIST,
                                     MAV_CMD_DO_FENCE_ENABLE,
                                     MAV_CMD_IMAGE_START_CAPTURE,
                                     MAV_CMD_IMAGE_STOP_CAPTURE,
                                     MAV_CMD_VIDEO_START_CAPTURE,
                                     MAV_CMD_VIDEO_STOP_CAPTURE,
                                     MAV_CMD_DO_CHANGE_SPEED,
                                     MAV_CMD_SET_CAMERA_MODE};
    QCOMPARE(toCommandIds(replayed), toCommandIds(expected));

    // No actuator or payload-actuation command is replayed. Asserting the absence is the point:
    // it is the exposure that makes R-002 real, and a future edit that "helpfully" added one
    // would be a modification of upstream behaviour by proxy.
    const QList<MAV_CMD> mustNotBeReplayed = {MAV_CMD_DO_SET_SERVO,
                                              MAV_CMD_DO_REPEAT_SERVO,
                                              MAV_CMD_DO_SET_ACTUATOR,
                                              MAV_CMD_DO_GRIPPER};
    for (const MAV_CMD command : mustNotBeReplayed) {
        QVERIFY2(!AeroraMissionResumeBoundary::isReplayed(command),
                 qPrintable(QStringLiteral("command %1 must not be in the resume replay set")
                                .arg(static_cast<int>(command))));
    }

    // Only the legacy DO_SET_ROI is replayed; the modern variants are not. Worth pinning, because
    // a plan built with the modern commands loses its region of interest across a resume.
    QVERIFY(AeroraMissionResumeBoundary::isReplayed(MAV_CMD_DO_SET_ROI));
    QVERIFY(!AeroraMissionResumeBoundary::isReplayed(MAV_CMD_DO_SET_ROI_LOCATION));
    QVERIFY(!AeroraMissionResumeBoundary::isReplayed(MAV_CMD_DO_SET_ROI_WPNEXT_OFFSET));
    QVERIFY(!AeroraMissionResumeBoundary::isReplayed(MAV_CMD_DO_SET_ROI_NONE));

    // The two prohibitions, as assertions rather than inspections. Both are expected to stay
    // false until actual evidence changes them.
    QVERIFY(!AeroraMissionResumeBoundary::kGcsPayloadStateReplayImplemented);
    QVERIFY(!AeroraMissionResumeBoundary::kPayloadStatePersistenceEvidenced);
}

void AeroraMissionTest::_resumeReportsItemsNotCarriedForwardBeforeResumeIndex()
{
    // seq 0 home, 1 takeoff, 2 a replayed camera command, 3 an unlisted actuator command,
    // 4 an already-flown waypoint, 5 the resume point, 6 an actuator command after it.
    const AeroraMissionSnapshot plan = AeroraMissionMock::buildMission(AeroraMissionSnapshot::GcsPlan,
                                                                       {MAV_CMD_NAV_WAYPOINT,
                                                                        MAV_CMD_NAV_TAKEOFF,
                                                                        MAV_CMD_DO_SET_CAM_TRIGG_DIST,
                                                                        MAV_CMD_DO_SET_ACTUATOR,
                                                                        MAV_CMD_NAV_WAYPOINT,
                                                                        MAV_CMD_NAV_WAYPOINT,
                                                                        MAV_CMD_DO_SET_ACTUATOR});

    const QList<AeroraMissionResumeOmission> omissions =
        AeroraMissionResumeBoundary::notCarriedForwardBeforeResumeIndex(plan, 5, {MAV_CMD_NAV_TAKEOFF});

    // The home item, the takeoff, the replayed camera command, and everything from the resume
    // index onward all survive. What is left is the actuator command and the already-flown
    // waypoint. Both are genuinely not carried forward; only the consumer can say which of them
    // matters, which is why this function classifies neither.
    QCOMPARE(omissions.count(), 2);
    QCOMPARE(omissions.at(0).sequenceNumber, 3);
    QCOMPARE(static_cast<int>(omissions.at(0).command), static_cast<int>(MAV_CMD_DO_SET_ACTUATOR));
    QCOMPARE(omissions.at(1).sequenceNumber, 4);
    QCOMPARE(static_cast<int>(omissions.at(1).command), static_cast<int>(MAV_CMD_NAV_WAYPOINT));

    // Without takeoff classification the analysis over-reports rather than under-reports, which
    // is the safe direction for a warning an operator acts on.
    QCOMPARE(AeroraMissionResumeBoundary::notCarriedForwardBeforeResumeIndex(plan, 5).count(), 3);

    // On a firmware that takes no home item, index 0 is an ordinary mission item and is filtered
    // like any other, so it appears too.
    QCOMPARE(AeroraMissionResumeBoundary::notCarriedForwardBeforeResumeIndex(plan,
                                                                             5,
                                                                             {MAV_CMD_NAV_TAKEOFF},
                                                                             false /* firmwareTakesHomeItem */)
                 .count(),
             3);

    // Resuming at the start omits nothing, because nothing precedes the resume index.
    QVERIFY(AeroraMissionResumeBoundary::notCarriedForwardBeforeResumeIndex(plan, 0).isEmpty());

    // A negative resume index is not a resume at the end of the mission.
    QVERIFY(AeroraMissionResumeBoundary::notCarriedForwardBeforeResumeIndex(plan, -1).isEmpty());
}

void AeroraMissionTest::_missionExtensionRegistryRefusesAmbiguity()
{
    AeroraMissionExtensionRegistry *const registry = AeroraMissionExtensionRegistry::instance();

    auto *const first = new StubExtension(QStringLiteral("stub.one"), {QStringLiteral("StubPattern")}, {});
    _stubExtensions.append(first);
    QVERIFY(registry->registerExtension(first));
    QCOMPARE(registry->extensionIds(), QStringList{QStringLiteral("stub.one")});

    // A duplicate id is a build configuration defect, not a last-one-wins override.
    auto *const duplicateId = new StubExtension(QStringLiteral("stub.one"), {QStringLiteral("OtherPattern")}, {});
    _stubExtensions.append(duplicateId);
    expectLogMessage(kLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("rejecting duplicate mission extension id")));
    QVERIFY(!registry->registerExtension(duplicateId));
    verifyExpectedLogMessage();

    // Two products claiming one complex item name is refused at registration, so resolution never
    // depends on link order.
    auto *const duplicateName = new StubExtension(QStringLiteral("stub.two"), {QStringLiteral("StubPattern")}, {});
    _stubExtensions.append(duplicateName);
    expectLogMessage(kLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("already-claimed complex item name")));
    QVERIFY(!registry->registerExtension(duplicateName));
    verifyExpectedLogMessage();

    expectLogMessage(kLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("rejecting null mission extension")));
    QVERIFY(!registry->registerExtension(nullptr));
    verifyExpectedLogMessage();

    auto *const emptyId = new StubExtension(QString(), {QStringLiteral("AnotherPattern")}, {});
    _stubExtensions.append(emptyId);
    expectLogMessage(kLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("empty id")));
    QVERIFY(!registry->registerExtension(emptyId));
    verifyExpectedLogMessage();

    // Only the one accepted registration is present.
    QCOMPARE(registry->extensionIds().count(), 1);
    QCOMPARE(registry->complexMissionItemNames(), QStringList{QStringLiteral("StubPattern")});

    // An unclaimed name creates nothing, and no extension is asked to answer for another's name.
    QVERIFY(!registry->createComplexMissionItem(QStringLiteral("UnclaimedPattern"), nullptr, false));
    QCOMPARE(first->createCallCount, 0);

    // A claimed name routes to its owner. The stub creates nothing, because no Aerora complex
    // item is evidenced, so the warning about that is expected.
    expectLogMessage(kLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("claimed a complex item name but created nothing")));
    QVERIFY(!registry->createComplexMissionItem(QStringLiteral("StubPattern"), nullptr, false));
    verifyExpectedLogMessage();
    QCOMPARE(first->createCallCount, 1);
}

void AeroraMissionTest::_missionCommandSetIsNotDeclared()
{
    AeroraMissionExtensionRegistry *const registry = AeroraMissionExtensionRegistry::instance();

    // This build carries no mission extension, because the Aerora mission command set is
    // OPEN / NOT-EVIDENCED under Q-023. The assertion pins the restraint: no command identifier
    // has been invented, and no placeholder set has been shipped.
    QVERIFY(registry->extensionIds().isEmpty());
    QVERIFY(!registry->isMissionCommandSetDeclared());
    QVERIFY(registry->declaredMissionCommandSet().isEmpty());

    // An extension that declares nothing must not flip the declared flag. The distinction matters
    // because FirmwarePlugin::supportedMissionCommands() reads an empty list as "all commands
    // supported", so a caller that confused the two would offer the operator every command in
    // MAVLink.
    auto *const declaresNothing = new StubExtension(QStringLiteral("stub.none"), {}, {});
    _stubExtensions.append(declaresNothing);
    QVERIFY(registry->registerExtension(declaresNothing));
    QVERIFY(!registry->isMissionCommandSetDeclared());
    QVERIFY(registry->declaredMissionCommandSet().isEmpty());

    // One that does declare a set flips it, and the union is de-duplicated.
    auto *const declaresSome =
        new StubExtension(QStringLiteral("stub.some"), {}, {MAV_CMD_NAV_WAYPOINT, MAV_CMD_NAV_WAYPOINT});
    _stubExtensions.append(declaresSome);
    QVERIFY(registry->registerExtension(declaresSome));
    QVERIFY(registry->isMissionCommandSetDeclared());

    const QList<MAV_CMD> declared = registry->declaredMissionCommandSet();
    QCOMPARE(declared.count(), 1);
    QVERIFY(declared.contains(MAV_CMD_NAV_WAYPOINT));
}

void AeroraMissionTest::_observerPresentsAnUncheckedUploadAsUnverified()
{
    AeroraMissionMock mock;
    AeroraMissionObserver *const observer = mock.observer();

    // Nothing observed yet: no plan, no vehicle mission, no position, nothing verified.
    QVERIFY(!observer->gcsPlan().isCaptured());
    QVERIFY(!observer->vehicleMission().isCaptured());
    QVERIFY(!observer->vehicleCurrentIndex().isAttributed());
    QCOMPARE(observer->transferActivity(), AeroraMissionObserver::NotEvidencedActivity);
    QVERIFY(!observer->agreement().isVerified());

    const QList<MAV_CMD> missionCommands = {MAV_CMD_NAV_TAKEOFF, MAV_CMD_NAV_WAYPOINT, MAV_CMD_NAV_RETURN_TO_LAUNCH};
    mock.loadPlan(AeroraMissionMock::buildMission(AeroraMissionSnapshot::GcsPlan, missionCommands));

    // The aircraft is partway through the previous mission.
    mock.reportCurrentIndex(2);
    QVERIFY(observer->vehicleCurrentIndex().isKnown());
    QCOMPARE(observer->vehicleCurrentIndex().sequenceNumber(), 2);

    mock.beginUpload(AeroraMissionMock::buildMission(AeroraMissionSnapshot::VehicleStore, missionCommands));
    QCOMPARE(observer->transferActivity(), AeroraMissionObserver::InProgress);

    mock.acceptUpload();

    QCOMPARE(observer->transferActivity(), AeroraMissionObserver::Idle);
    QCOMPARE(observer->lastOperation(), AeroraMissionObserver::Upload);
    QCOMPARE(observer->lastOutcome(), AeroraMissionObserver::Succeeded);
    QVERIFY(!observer->isVehicleMissionIndeterminate());

    // Mission replacement resets the current index. The aircraft was at item 2 and is now at no
    // reported index at all, which must read as neither item 0 nor the stale 2.
    QVERIFY(observer->vehicleCurrentIndex().isAttributed());
    QVERIFY(!observer->vehicleCurrentIndex().isKnown());

    // A successful upload is not a verified upload. AERORA-MISSION-REQ-007 requires exactly this.
    QCOMPARE(observer->agreement().verdict(), AeroraMissionAgreement::NotCompared);
    QVERIFY(!observer->agreement().isVerified());

    // Verification is explicit, and then it holds.
    QVERIFY(observer->verify(AeroraMissionAgreement::DepthFullParameters).isVerified());
    QVERIFY(observer->agreement().isVerified());

    // A later change to either side invalidates the verdict rather than leaving a verified claim
    // standing over a mission it no longer describes.
    mock.beginDownload();
    mock.completeDownload(
        AeroraMissionMock::buildMission(AeroraMissionSnapshot::VehicleStore, {MAV_CMD_NAV_WAYPOINT}));
    QCOMPARE(observer->agreement().verdict(), AeroraMissionAgreement::NotCompared);

    // And a real mismatch is reported as one.
    QCOMPARE(observer->verify(AeroraMissionAgreement::DepthItemCount).verdict(), AeroraMissionAgreement::Differs);

    // The editor selection is independent throughout: it never became the aircraft's position.
    mock.selectPlanViewItem(1);
    QCOMPARE(observer->planViewSelection().sequenceNumber(), 1);
    QVERIFY(!observer->planViewSelection().isVehicleAuthoritative());
    QVERIFY(!observer->vehicleCurrentIndex().isKnown());
}

void AeroraMissionTest::_observerMarksTheVehicleMissionIndeterminateAfterAFailedUpload()
{
    AeroraMissionMock mock;
    AeroraMissionObserver *const observer = mock.observer();

    // Start from a known vehicle mission.
    mock.beginDownload();
    mock.completeDownload(AeroraMissionMock::buildMission(AeroraMissionSnapshot::VehicleStore,
                                                          {MAV_CMD_NAV_TAKEOFF, MAV_CMD_NAV_WAYPOINT}));
    QVERIFY(observer->vehicleMission().isCaptured());
    QVERIFY(!observer->isVehicleMissionIndeterminate());

    mock.beginUpload(AeroraMissionMock::buildMission(AeroraMissionSnapshot::VehicleStore, {MAV_CMD_NAV_WAYPOINT}));

    expectLogMessage(kLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("mission transfer error reported")));
    mock.rejectUpload(QStringLiteral("vehicle rejected item 1"));
    verifyExpectedLogMessage();

    QCOMPARE(observer->lastOperation(), AeroraMissionObserver::Upload);
    QCOMPARE(observer->lastOutcome(), AeroraMissionObserver::Failed);

    // The write list is discarded with no rollback and the vehicle may already have accepted part
    // of the new mission, so neither mission describes what is on board. The previous snapshot
    // must not be left standing as if it still did.
    QVERIFY(observer->isVehicleMissionIndeterminate());
    QVERIFY(!observer->vehicleMission().isCaptured());

    // Nothing can be verified against an indeterminate store, and the attempt is refused rather
    // than compared against the stale snapshot.
    expectLogMessage(kLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("refusing mission comparison with wrong origins")));
    QVERIFY(!observer->verify(AeroraMissionAgreement::DepthItemCount).isVerified());
    verifyExpectedLogMessage();

    // A completed download resolves the indeterminacy, because it describes the vehicle.
    mock.beginDownload();
    mock.completeDownload(AeroraMissionMock::buildMission(AeroraMissionSnapshot::VehicleStore, {MAV_CMD_NAV_WAYPOINT}));
    QVERIFY(!observer->isVehicleMissionIndeterminate());
    QVERIFY(observer->vehicleMission().isCaptured());
}

void AeroraMissionTest::_observerDistinguishesAFailedDownloadFromAnEmptyMission()
{
    AeroraMissionMock mock;
    AeroraMissionObserver *const observer = mock.observer();

    // An empty mission is a successful observation of a vehicle holding nothing.
    mock.beginDownload();
    mock.completeDownload(AeroraMissionSnapshot::empty(AeroraMissionSnapshot::VehicleStore));

    QCOMPARE(observer->lastOperation(), AeroraMissionObserver::Download);
    QCOMPARE(observer->lastOutcome(), AeroraMissionObserver::Succeeded);
    QVERIFY(observer->vehicleMission().isCaptured());
    QCOMPARE(observer->vehicleMission().itemCount(), 0);

    // A failed download looks identical on the completion signal, which carries no error flag and
    // arrives with an emptied item list. The error reported during the transaction is the only
    // evidence that separates the two, and it has to survive until completion.
    mock.beginDownload();

    expectLogMessage(kLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("mission transfer error reported")));
    mock.failDownload(QStringLiteral("ack timeout"));
    verifyExpectedLogMessage();

    QCOMPARE(observer->lastOperation(), AeroraMissionObserver::Download);
    QCOMPARE(observer->lastOutcome(), AeroraMissionObserver::Failed);

    // A failed download told us nothing about the vehicle, so it must not overwrite what the
    // earlier successful one did tell us.
    QVERIFY(observer->vehicleMission().isCaptured());
    QCOMPARE(observer->vehicleMission().itemCount(), 0);

    // The error latch is per transaction: the next download is not tainted by the previous one.
    mock.beginDownload();
    mock.completeDownload(AeroraMissionMock::buildMission(AeroraMissionSnapshot::VehicleStore, {MAV_CMD_NAV_WAYPOINT}));
    QCOMPARE(observer->lastOutcome(), AeroraMissionObserver::Succeeded);
    QCOMPARE(observer->vehicleMission().itemCount(), 1);
}

void AeroraMissionTest::_mockIsDeterministicAndSimulatesNoAircraft()
{
    // The same call sequence produces the same observable state, with no timers and no randomness
    // anywhere in the path.
    const auto driveScenario = [](AeroraMissionMock &mock) {
        const QList<MAV_CMD> missionCommands = {MAV_CMD_NAV_TAKEOFF, MAV_CMD_NAV_WAYPOINT};
        mock.loadPlan(AeroraMissionMock::buildMission(AeroraMissionSnapshot::GcsPlan, missionCommands));
        mock.beginUpload(AeroraMissionMock::buildMission(AeroraMissionSnapshot::VehicleStore, missionCommands));
        mock.acceptUpload();
        mock.reportCurrentIndex(1);
        mock.reportResumeCandidate(1);
        mock.selectPlanViewItem(0);
    };

    AeroraMissionMock first;
    AeroraMissionMock second;
    driveScenario(first);
    driveScenario(second);

    QCOMPARE(first.observer()->vehicleCurrentIndex().toString(), second.observer()->vehicleCurrentIndex().toString());
    QCOMPARE(first.observer()->resumeCandidate().toString(), second.observer()->resumeCandidate().toString());
    QCOMPARE(first.observer()->planViewSelection().toString(), second.observer()->planViewSelection().toString());
    QCOMPARE(first.observer()->vehicleMission().itemCount(), second.observer()->vehicleMission().itemCount());
    QCOMPARE(first.observer()->lastOutcome(), second.observer()->lastOutcome());

    // The resume candidate stays a proposal even when it names the same item the aircraft reports.
    QCOMPARE(first.observer()->resumeCandidate().sequenceNumber(),
             first.observer()->vehicleCurrentIndex().sequenceNumber());
    QVERIFY(first.observer()->vehicleCurrentIndex().isVehicleAuthoritative());
    QVERIFY(!first.observer()->resumeCandidate().isVehicleAuthoritative());

    // The mock fabricates no mission geometry: every parameter is zero, so no coordinate,
    // altitude or speed has been invented anywhere in this suite.
    const AeroraMissionSnapshot built =
        AeroraMissionMock::buildMission(AeroraMissionSnapshot::GcsPlan, {MAV_CMD_NAV_WAYPOINT});
    QCOMPARE(built.itemCount(), 1);

    const AeroraMissionItemFingerprint &item = built.items().first();
    QCOMPARE(item.param1, 0.0);
    QCOMPARE(item.param5, 0.0);
    QCOMPARE(item.param6, 0.0);
    QCOMPARE(item.param7, 0.0);
}

UT_REGISTER_TEST(AeroraMissionTest, TestLabel::Unit, TestLabel::MissionManager)

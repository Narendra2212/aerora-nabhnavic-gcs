#include "AeroraPayloadModelTest.h"

#include <QtCore/QRegularExpression>
#include <QtTest/QSignalSpy>

#include "AeroraFreshness.h"
#include "AeroraPayloadCommandSink.h"
#include "AeroraPayloadMock.h"
#include "AeroraPayloadModel.h"
#include "AeroraReportedValue.h"
#include "AeroraStateValue.h"
#include "AeroraTelemetrySet.h"
#include "AeroraTelemetrySource.h"
#include "AeroraTelemetryState.h"
#include "AeroraVehicleCapabilities.h"
#include "ManualScheduler.h"

namespace {

constexpr const char *kLogCategory = "aerora.payload";

/// M-Telemetry's category. AeroraFreshness warns on it, not on this module's category, the first
/// time it evaluates a class with no configured threshold.
constexpr const char *kTelemetryLogCategory = "aerora.telemetry";

// Every token and key below is chosen by this test and means nothing. Payload state, health and
// safety-state enumeration members are OPEN (Q-023) and no capability identifier is evidenced, so
// the test supplies obviously-synthetic strings rather than anything that could be mistaken for a
// specification.
constexpr const char *kCapabilityKey = "test.capability.key";
constexpr const char *kToken = "test-token";
constexpr const char *kOtherToken = "test-other-token";
constexpr const char *kRequestToken = "test-request-token";

/// The freshness class the production model declares. Named here so the composition tests can
/// assert that nothing in the build registers a threshold for it.
constexpr const char *kPayloadFreshnessClass = "aeroraPayload";

/// Records what it is handed and does nothing else.
///
/// This is the instrument the prohibition tests need: it is the only implementation of
/// \c AeroraPayloadCommandSink in the tree, it exists only in this translation unit, and a
/// production change that routed a reported condition to a command would show up here as a non-zero
/// \c callCount.
class RecordingCommandSink : public AeroraPayloadCommandSink
{
public:
    bool submitPayloadRequest(const AeroraPayloadRequest &request) override
    {
        ++callCount;
        lastRequest = request;

        return accept;
    }

    int callCount = 0;
    bool accept = true;
    AeroraPayloadRequest lastRequest;
};

/// The five item ids, in declaration order. Kept here rather than exported from the model so a
/// silent rename or an added sixth item fails a test instead of passing unnoticed.
QStringList expectedItemIds()
{
    return QStringList{QStringLiteral("payloadIdentity"),
                       QStringLiteral("payloadState"),
                       QStringLiteral("payloadHealth"),
                       QStringLiteral("payloadSafetyState"),
                       QStringLiteral("payloadEmergencyStop")};
}

} // namespace

void AeroraPayloadModelTest::_consumeUnconfiguredThresholdWarning(AeroraReportedValue *value)
{
    QVERIFY(value);
    QVERIFY2(value->isObserved(), "the item must be observed, or the evaluator is never reached");

    expectLogMessage(kTelemetryLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("No freshness threshold configured")));
    (void) value->stateClass();
    verifyExpectedLogMessage();
}

void AeroraPayloadModelTest::_itemsStartUnknownAndAreSeparate()
{
    ManualScheduler scheduler;
    AeroraPayloadMock mock(&scheduler);
    AeroraPayloadModel *const model = mock.model();

    // Exactly five items, and exactly these five. A sixth would most likely be a field relating the
    // emergency stop to a watchdog event or an arming change, and no such relationship is reported
    // by anything - that is the absence this assertion protects.
    QStringList actualIds;
    for (const AeroraReportedValue *const value : model->reportedValues()) {
        actualIds << value->itemId();
    }
    QCOMPARE(actualIds, expectedItemIds());

    for (const AeroraReportedValue *const value : model->reportedValues()) {
        // UNKNOWN, not UNAVAILABLE, and carrying nothing. Health in particular must not read as
        // healthy before anything has been reported: that is the dangerous default IF-PAY-004
        // names, and it is avoided structurally rather than by a check somewhere.
        QCOMPARE(value->stateClass(), AeroraTelemetryState::Unknown);
        QVERIFY(!value->isObserved());
        QVERIFY(value->reportedToken().isEmpty());
        QCOMPARE(value->receiveTimeUs(), quint64(0));
        QVERIFY(!value->source().isIdentified());
        QCOMPARE(value->ageMs(), -1);
        QVERIFY(!AeroraTelemetryState::carriesValue(value->stateClass()));

        // Every payload item is reported by the payload controller. None is effective, so none can
        // ever be read as authoritative aircraft state.
        QCOMPARE(value->declaredKind(), AeroraTelemetryState::KindReported);
        QVERIFY(!AeroraTelemetryState::isAuthoritative(value->stateClass()));
    }
}

void AeroraPayloadModelTest::_reportsDoNotLeakBetweenItems_data()
{
    QTest::addColumn<QString>("reportedItemId");

    const QStringList ids = expectedItemIds();
    for (const QString &id : ids) {
        QTest::newRow(id.toUtf8().constData()) << id;
    }
}

void AeroraPayloadModelTest::_reportsDoNotLeakBetweenItems()
{
    QFETCH(QString, reportedItemId);

    ManualScheduler scheduler;
    AeroraPayloadMock mock(&scheduler);
    AeroraPayloadModel *const model = mock.model();

    mock.setReportingSource(1, 1);

    AeroraReportedValue *const reported = model->reportedValue(reportedItemId);
    QVERIFY(reported != nullptr);

    const quint64 stampUs = mock.nowUs();
    QSignalSpy reportedSpy(reported, &AeroraReportedValue::reportChanged);

    reported->recordReport(QString::fromLatin1(kToken), stampUs, AeroraTelemetrySource::fromSendingComponent(1, 1));

    _consumeUnconfiguredThresholdWarning(reported);

    QCOMPARE(reportedSpy.count(), 1);
    QCOMPARE(reported->reportedToken(), QString::fromLatin1(kToken));
    QCOMPARE(reported->receiveTimeUs(), stampUs);
    QCOMPARE(reported->stateClass(), AeroraTelemetryState::Reported);
    QCOMPARE(reported->source().componentId(), 1);
    QCOMPARE(reported->source().attribution(), AeroraTelemetrySource::SendingComponent);

    // No threshold is configured for the payload freshness class anywhere in this build (Q-036),
    // so an observed item has no freshness verdict and a consumer must surface its age.
    QVERIFY(!reported->freshnessEvaluated());

    for (const AeroraReportedValue *const other : model->reportedValues()) {
        if (other == reported) {
            continue;
        }

        QVERIFY2(!other->isObserved(),
                 qPrintable(QStringLiteral("reporting %1 moved %2").arg(reportedItemId, other->itemId())));
        QCOMPARE(other->stateClass(), AeroraTelemetryState::Unknown);
        QVERIFY(other->reportedToken().isEmpty());
    }
}

void AeroraPayloadModelTest::_unknownAndUnavailableStayDistinct()
{
    ManualScheduler scheduler;
    AeroraPayloadMock mock(&scheduler);
    AeroraPayloadModel *const model = mock.model();

    AeroraReportedValue *const health = model->health();

    QCOMPARE(health->stateClass(), AeroraTelemetryState::Unknown);

    // A payload capability reported absent does NOT make an item unavailable: no mapping from a
    // capability key to an item is evidenced (Q-023), so the model performs none rather than
    // guessing one.
    QVERIFY(mock.reportCapabilityUnavailable(QString::fromLatin1(kCapabilityKey)));
    QCOMPARE(health->stateClass(), AeroraTelemetryState::Unknown);

    // Only a reported capability absence on the item itself produces UNAVAILABLE, and it outranks
    // an observation that has already arrived.
    mock.reportHealth(QString::fromLatin1(kToken));
    _consumeUnconfiguredThresholdWarning(health);
    QCOMPARE(health->stateClass(), AeroraTelemetryState::Reported);

    QVERIFY(mock.markItemCapabilityAbsent(health->itemId()));
    QCOMPARE(health->stateClass(), AeroraTelemetryState::Unavailable);

    // Both are non-values and both carry nothing, yet they remain different answers: "the payload
    // has not said" versus "the payload says it does not have this". IF-UIS-005 requires an
    // operator be able to tell them apart.
    QVERIFY(AeroraTelemetryState::isNonValue(AeroraTelemetryState::Unknown));
    QVERIFY(AeroraTelemetryState::isNonValue(AeroraTelemetryState::Unavailable));
    static_assert(AeroraTelemetryState::Unknown != AeroraTelemetryState::Unavailable,
                  "IF-UIS-005: unknown and unavailable must not collapse into each other");

    QVERIFY(mock.setItemCapability(health->itemId(), AeroraStateValue::CapabilityUnknown));
    QCOMPARE(health->stateClass(), AeroraTelemetryState::Reported);
}

void AeroraPayloadModelTest::_capabilityGatesControls()
{
    ManualScheduler scheduler;
    AeroraPayloadMock mock(&scheduler);
    AeroraPayloadModel *const model = mock.model();

    QSignalSpy permittedSpy(model, &AeroraPayloadModel::controlsPermittedChanged);

    // IF-PAY-002: no capability set means no payload controls at all. "We have not been told" is
    // not permission.
    QVERIFY(!model->controlsPermitted());
    QVERIFY(model->reportedCapabilityKeys().isEmpty());

    // A capability reported absent is still not permission.
    QVERIFY(mock.reportCapabilityUnavailable(QString::fromLatin1(kCapabilityKey)));
    QVERIFY(!model->controlsPermitted());
    QCOMPARE(permittedSpy.count(), 0);

    // Rule 6 of the capability model: an unrecognised identifier is retained and recorded rather
    // than discarded, so the key is visible even though nothing here knows what it means.
    QCOMPARE(model->reportedCapabilityKeys(), QStringList{QString::fromLatin1(kCapabilityKey)});

    QVERIFY(mock.reportCapabilityAvailable(QString::fromLatin1(kCapabilityKey)));
    QVERIFY(model->controlsPermitted());
    QCOMPARE(permittedSpy.count(), 1);

    // A reconnection re-establishes capability from scratch; it is never carried across.
    mock.clearCapabilityReports();
    QVERIFY(!model->controlsPermitted());
    QCOMPARE(permittedSpy.count(), 2);
}

void AeroraPayloadModelTest::_acceptedRequestDoesNotBecomePayloadState()
{
    ManualScheduler scheduler;
    AeroraPayloadMock mock(&scheduler);
    AeroraPayloadModel *const model = mock.model();

    RecordingCommandSink sink;
    model->setCommandSink(&sink);
    QVERIFY(mock.reportCapabilityAvailable(QString::fromLatin1(kCapabilityKey)));

    // Give the payload state a prior reported value, so the test can tell "unchanged" from
    // "never set" after the request is accepted.
    mock.reportPayloadState(QString::fromLatin1(kToken));
    _consumeUnconfiguredThresholdWarning(model->payloadState());
    const quint64 stateStampUs = model->payloadState()->receiveTimeUs();

    const AeroraPayloadRequest request(QString::fromLatin1(kRequestToken), mock.nowUs());
    QSignalSpy outcomeSpy(model, &AeroraPayloadModel::requestOutcome);

    QCOMPARE(model->submitRequest(request), AeroraPayloadModel::RequestOutcome::Submitted);

    // The request reached the command path, which is all "Submitted" claims.
    QCOMPARE(sink.callCount, 1);
    QCOMPARE(sink.lastRequest.requestToken(), QString::fromLatin1(kRequestToken));
    QCOMPARE(model->requestCount(), 1);
    QCOMPARE(outcomeSpy.count(), 1);

    // AERORA-PAYLOAD-REQ-011 and IF-PAY-003: an accepted command is not an effect. Payload state
    // keeps the value and the receive time the payload reported, and nothing acquires the request's
    // token. This is the regression a future "optimistically update the display" change would
    // introduce.
    QCOMPARE(model->payloadState()->reportedToken(), QString::fromLatin1(kToken));
    QCOMPARE(model->payloadState()->receiveTimeUs(), stateStampUs);
    QCOMPARE(model->payloadState()->stateClass(), AeroraTelemetryState::Reported);

    for (const AeroraReportedValue *const value : model->reportedValues()) {
        QVERIFY(value->reportedToken() != QString::fromLatin1(kRequestToken));
        QVERIFY(!AeroraTelemetryState::isAuthoritative(value->stateClass()));
    }

    // Only a report from the payload controller moves the state, and it moves it to what the
    // payload said rather than to what was asked for.
    mock.reportPayloadState(QString::fromLatin1(kOtherToken));
    QCOMPARE(model->payloadState()->reportedToken(), QString::fromLatin1(kOtherToken));
}

void AeroraPayloadModelTest::_requestsFailClosed_data()
{
    QTest::addColumn<bool>("capabilityReported");
    QTest::addColumn<bool>("commandPathInstalled");
    QTest::addColumn<bool>("commandPathAccepts");
    QTest::addColumn<bool>("validRequest");
    QTest::addColumn<AeroraPayloadModel::RequestOutcome>("expectedOutcome");

    QTest::newRow("no capability, no command path")
        << false << false << true << true << AeroraPayloadModel::RequestOutcome::RefusedNoCapabilityReported;
    QTest::newRow("no capability, command path installed")
        << false << true << true << true << AeroraPayloadModel::RequestOutcome::RefusedNoCapabilityReported;
    QTest::newRow("capability, no command path")
        << true << false << true << true << AeroraPayloadModel::RequestOutcome::RefusedNoCommandPath;
    QTest::newRow("capability, command path declines")
        << true << true << false << true << AeroraPayloadModel::RequestOutcome::RefusedByCommandPath;
    QTest::newRow("tokenless request")
        << true << true << true << false << AeroraPayloadModel::RequestOutcome::RefusedInvalidRequest;
}

void AeroraPayloadModelTest::_requestsFailClosed()
{
    QFETCH(bool, capabilityReported);
    QFETCH(bool, commandPathInstalled);
    QFETCH(bool, commandPathAccepts);
    QFETCH(bool, validRequest);
    QFETCH(AeroraPayloadModel::RequestOutcome, expectedOutcome);

    ManualScheduler scheduler;
    AeroraPayloadMock mock(&scheduler);
    AeroraPayloadModel *const model = mock.model();

    RecordingCommandSink sink;
    sink.accept = commandPathAccepts;

    if (commandPathInstalled) {
        model->setCommandSink(&sink);
    }
    if (capabilityReported) {
        QVERIFY(mock.reportCapabilityAvailable(QString::fromLatin1(kCapabilityKey)));
    }

    const AeroraPayloadRequest request = validRequest
        ? AeroraPayloadRequest(QString::fromLatin1(kRequestToken), mock.nowUs())
        : AeroraPayloadRequest();

    if (expectedOutcome == AeroraPayloadModel::RequestOutcome::RefusedInvalidRequest) {
        // The one refusal that is a caller defect rather than this build behaving as designed, and
        // therefore the one that warns.
        expectLogMessage(kLogCategory,
                         QtWarningMsg,
                         QRegularExpression(QStringLiteral("carried no token")));
    }

    QCOMPARE(model->submitRequest(request), expectedOutcome);

    if (expectedOutcome == AeroraPayloadModel::RequestOutcome::RefusedInvalidRequest) {
        verifyExpectedLogMessage();
    }

    // A refused request never reaches the payload, and never reaches any item either.
    const int expectedSinkCalls = (expectedOutcome == AeroraPayloadModel::RequestOutcome::RefusedByCommandPath) ? 1 : 0;
    QCOMPARE(sink.callCount, expectedSinkCalls);

    for (const AeroraReportedValue *const value : model->reportedValues()) {
        QVERIFY(!value->isObserved());
        QCOMPARE(value->stateClass(), AeroraTelemetryState::Unknown);
    }
}

void AeroraPayloadModelTest::_noReportedConditionProducesARequest()
{
    ManualScheduler scheduler;
    AeroraPayloadMock mock(&scheduler);
    AeroraPayloadModel *const model = mock.model();

    // Every gate open, so a refusal cannot be what keeps the sink quiet. If any production code
    // ever derives a payload command from a reported condition, this is where it surfaces.
    RecordingCommandSink sink;
    model->setCommandSink(&sink);
    QVERIFY(mock.reportCapabilityAvailable(QString::fromLatin1(kCapabilityKey)));
    mock.setReportingSource(1, 1);

    // The conditions 06-payload-icd.md and the directive name: a fault, an emergency stop, a
    // safety-state change, an identity change. Tokens are synthetic; what matters is that the model
    // treats all of them as values to display and none as a trigger.
    mock.reportHealth(QString::fromLatin1(kToken));
    mock.reportSafetyState(QString::fromLatin1(kToken));
    mock.reportEmergencyStop(QString::fromLatin1(kToken));
    mock.reportPayloadState(QString::fromLatin1(kOtherToken));
    mock.reportIdentity(QString::fromLatin1(kToken));

    // AERORA-PAYLOAD-REQ-010: the ground station is not the payload safety controller. No stop, no
    // valve close, no pump stop, no disarm, no land, no return-to-launch.
    QCOMPARE(sink.callCount, 0);
    QCOMPARE(model->requestCount(), 0);

    // The emergency stop is reported alongside the payload state, not instead of it: the debrief
    // can still say what state the payload was in when the stop fired.
    QCOMPARE(model->emergencyStop()->reportedToken(), QString::fromLatin1(kToken));
    QCOMPARE(model->payloadState()->reportedToken(), QString::fromLatin1(kOtherToken));
    QVERIFY(model->emergencyStop()->receiveTimeUs() != 0);
    QVERIFY(model->payloadState()->receiveTimeUs() != 0);
}

void AeroraPayloadModelTest::_noActuatorPathIsInstalledByDefault()
{
    ManualScheduler scheduler;
    AeroraPayloadMock mock(&scheduler);
    AeroraPayloadModel *const model = mock.model();

    // IF-PAY-015 and IF-PAY-013, as build properties rather than as inspection findings.
    static_assert(!AeroraPayloadModel::kGroundStationActuatorPathImplemented,
                  "IF-PAY-015 is CONFIRMED as prohibited: the ground station has no path to a payload actuator");
    static_assert(!AeroraPayloadModel::kAutonomousPayloadActionImplemented,
                  "AERORA-PAYLOAD-REQ-010: the ground station originates no payload action of its own");

    // No sink is installed by anything in the overlay, and there is no implementation of the
    // interface outside this test's own recorder. So a payload request cannot leave this build.
    QVERIFY(!model->isCommandPathInstalled());

    QVERIFY(mock.reportCapabilityAvailable(QString::fromLatin1(kCapabilityKey)));
    const AeroraPayloadRequest request(QString::fromLatin1(kRequestToken), mock.nowUs());
    QCOMPARE(model->submitRequest(request), AeroraPayloadModel::RequestOutcome::RefusedNoCommandPath);
}

void AeroraPayloadModelTest::_disconnectClearsReportsAndRestoresNothing()
{
    ManualScheduler scheduler;
    AeroraPayloadMock mock(&scheduler);
    AeroraPayloadModel *const model = mock.model();

    mock.setReportingSource(1, 1);
    QVERIFY(mock.reportCapabilityAvailable(QString::fromLatin1(kCapabilityKey)));
    mock.reportIdentity(QString::fromLatin1(kToken));
    mock.reportPayloadState(QString::fromLatin1(kToken));
    mock.reportHealth(QString::fromLatin1(kToken));
    mock.reportSafetyState(QString::fromLatin1(kToken));
    mock.reportEmergencyStop(QString::fromLatin1(kToken));

    for (const AeroraReportedValue *const value : model->reportedValues()) {
        QVERIFY(value->isObserved());
    }

    mock.disconnectPayload();

    for (const AeroraReportedValue *const value : model->reportedValues()) {
        QVERIFY(!value->isObserved());
        QCOMPARE(value->stateClass(), AeroraTelemetryState::Unknown);
        QVERIFY(value->reportedToken().isEmpty());
        QVERIFY(!value->source().isIdentified());
    }
    QVERIFY(!model->controlsPermitted());

    // IF-PAY-014: payload state retention across an interruption is NOT-EVIDENCED, and
    // ground-station replay is rejected by decision. A reconnection therefore starts from nothing -
    // reporting the capability again restores no value, because nothing was kept to restore from.
    // This is the regression a future "remember the last payload state" change would introduce.
    QVERIFY(mock.reportCapabilityAvailable(QString::fromLatin1(kCapabilityKey)));
    QVERIFY(model->controlsPermitted());

    for (const AeroraReportedValue *const value : model->reportedValues()) {
        QVERIFY2(!value->isObserved(),
                 qPrintable(QStringLiteral("%1 was restored after a reconnection").arg(value->itemId())));
        QVERIFY(value->reportedToken().isEmpty());
    }
}

void AeroraPayloadModelTest::_compositionBorrowsFreshnessAndOwnsItsItems()
{
    // The wiring AeroraFirmwarePlugin::attachDomainServices() performs, reproduced with the real
    // types rather than the mock: a telemetry set that owns the freshness evaluator, and a
    // capability inventory that starts empty.
    //
    // Deliberately narrow. M-Vehicle's AeroraVehicleIntegrationTest already asserts the composition
    // itself - attachment order, per-vehicle isolation, parenting onto the services object,
    // idempotency - and re-asserts this module's prohibition constants, the absent command path and
    // both gates at the composition root. None of that is repeated here. What is left, and what
    // only this module can state, is the ownership contract inside the model.
    ManualScheduler scheduler;
    AeroraTelemetrySet telemetrySet(&scheduler);
    AeroraVehicleCapabilities capabilities;

    AeroraFreshness *const freshness = telemetrySet.freshness();
    QVERIFY(freshness);

    AeroraPayloadModel model(freshness, &capabilities);

    // The evaluator is borrowed, never adopted. It stays a child of the telemetry set, so
    // reparenting the model onto a services object cannot move it, and destroying the model cannot
    // destroy it out from under the power model that shares it.
    QCOMPARE(freshness->parent(), static_cast<QObject *>(&telemetrySet));

    // The items are children of the model, which is what ties their lifetime to the vehicle once
    // the model itself is reparented. No item needs teardown code anywhere.
    for (AeroraReportedValue *const value : model.reportedValues()) {
        QCOMPARE(value->parent(), static_cast<QObject *>(&model));
    }

    // The composition registers no threshold, so no payload item can be presented with a freshness
    // verdict behind it. Asserted against the composed evaluator rather than the mock's, because
    // this is the instance a vehicle actually gets.
    QVERIFY(!freshness->hasThreshold(QString::fromLatin1(kPayloadFreshnessClass)));
    QVERIFY(freshness->configuredClasses().isEmpty());
}

void AeroraPayloadModelTest::_outlivingTheFreshnessEvaluatorIsSafe()
{
    ManualScheduler scheduler;
    AeroraVehicleCapabilities capabilities;

    // Production destruction order, reproduced. AeroraVehicleServices attaches the telemetry set
    // before the payload model, and Qt deletes children in attachment order, so the freshness
    // evaluator - and the scheduler it owns - is destroyed while the payload model is still alive.
    // The composition root relies on the models staying correct through that window; this is the
    // assertion behind that claim, and it is this module's to make.
    auto *const telemetrySet = new AeroraTelemetrySet(&scheduler);
    AeroraPayloadModel model(telemetrySet->freshness(), &capabilities);

    model.identity()->recordReport(QString::fromLatin1(kToken),
                                   scheduler.nowUs(),
                                   AeroraTelemetrySource::fromSendingComponent(1, 1));
    _consumeUnconfiguredThresholdWarning(model.identity());
    QCOMPARE(model.identity()->stateClass(), AeroraTelemetryState::Reported);
    QVERIFY(model.identity()->ageMs() >= 0);

    delete telemetrySet;

    // Every accessor must still answer, and answer honestly: the observation is real, its
    // freshness is now unknowable, and claiming either verdict would be a fabrication.
    QCOMPARE(model.identity()->reportedToken(), QString::fromLatin1(kToken));
    QCOMPARE(model.identity()->stateClass(), AeroraTelemetryState::Reported);
    QVERIFY(!model.identity()->freshnessEvaluated());
    QCOMPARE(model.identity()->ageMs(), -1);

    // The mutating paths must not reach through the dead pointer either. recordReport reschedules
    // an expiry task and clearReport cancels one; both are guarded in AeroraReportedValue and
    // again in ScheduledTask::cancel(), whose scheduler is also a QPointer.
    model.payloadState()->recordReport(QString::fromLatin1(kOtherToken), 1, AeroraTelemetrySource());
    QCOMPARE(model.payloadState()->reportedToken(), QString::fromLatin1(kOtherToken));
    QCOMPARE(model.payloadState()->stateClass(), AeroraTelemetryState::Reported);

    model.clearReports();
    for (const AeroraReportedValue *const value : model.reportedValues()) {
        QCOMPARE(value->stateClass(), AeroraTelemetryState::Unknown);
    }
}

UT_REGISTER_TEST(AeroraPayloadModelTest, TestLabel::Unit)

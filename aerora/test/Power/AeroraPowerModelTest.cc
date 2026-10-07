#include "AeroraPowerModelTest.h"

#include <chrono>

#include <QtTest/QSignalSpy>

#include "AeroraFreshness.h"
#include "AeroraPayloadModel.h"
#include "AeroraPowerMock.h"
#include "AeroraPowerModel.h"
#include "AeroraReportedValue.h"
#include "AeroraStateValue.h"
#include "AeroraTelemetrySet.h"
#include "AeroraTelemetrySource.h"
#include "AeroraTelemetryState.h"
#include "AeroraVehicleCapabilities.h"
#include "ManualScheduler.h"

namespace {

using namespace std::chrono_literals;

// Every token and key below is chosen by this test and means nothing. Power state, fault and
// source-transition enumeration members are UNKNOWN (Q-024) and no capability identifier is
// evidenced, so the test supplies obviously-synthetic strings rather than anything that could be
// mistaken for a specification. In particular there is no "tether connected" or "on battery"
// string anywhere here.
constexpr const char *kCapabilityKey = "test.capability.key";
constexpr const char *kToken = "test-token";
constexpr const char *kOtherToken = "test-other-token";

/// The freshness class the production model declares. Named here so the test can assert the build
/// registers no threshold for it, and can register one itself to drive the staleness transition.
constexpr const char *kPowerFreshnessClass = "aeroraPower";

/// M-Telemetry's category. AeroraFreshness warns on it, not on this module's category, the first
/// time it evaluates a class with no configured threshold.
constexpr const char *kTelemetryLogCategory = "aerora.telemetry";

/// The seven item ids, in declaration order. Kept here rather than exported from the model so a
/// silent rename or an added eighth item fails a test instead of passing unnoticed.
QStringList expectedItemIds()
{
    return QStringList{QStringLiteral("powerTetherConfiguration"),
                       QStringLiteral("powerBatteryConfiguration"),
                       QStringLiteral("powerEffectiveState"),
                       QStringLiteral("powerReportedState"),
                       QStringLiteral("powerSourceTransition"),
                       QStringLiteral("powerFault"),
                       QStringLiteral("powerWatchdogEvent")};
}

} // namespace

void AeroraPowerModelTest::_consumeUnconfiguredThresholdWarning(AeroraReportedValue *value)
{
    QVERIFY(value);
    QVERIFY2(value->isObserved(), "the item must be observed, or the evaluator is never reached");

    expectLogMessage(kTelemetryLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("No freshness threshold configured")));
    (void) value->stateClass();
    verifyExpectedLogMessage();
}

void AeroraPowerModelTest::_itemsStartUnknownAndAreSeparate()
{
    ManualScheduler scheduler;
    AeroraPowerMock mock(&scheduler);
    AeroraPowerModel *const model = mock.model();

    // Exactly seven items, and exactly these seven. An eighth would most likely be a field relating
    // the watchdog event to a power state or a converter change, and none of those relationships is
    // evidenced in either direction — that is the absence this assertion protects.
    QStringList actualIds;
    for (const AeroraReportedValue *const value : model->reportedValues()) {
        actualIds << value->itemId();
    }
    QCOMPARE(actualIds, expectedItemIds());

    for (const AeroraReportedValue *const value : model->reportedValues()) {
        // UNKNOWN, not UNAVAILABLE, and carrying nothing. Never zero and never a default: an
        // aircraft that has not reported its power configuration must not read as one that reported
        // no tether, no battery and no fault.
        QCOMPARE(value->stateClass(), AeroraTelemetryState::Unknown);
        QVERIFY(!value->isObserved());
        QVERIFY(value->reportedToken().isEmpty());
        QCOMPARE(value->receiveTimeUs(), quint64(0));
        QVERIFY(!value->source().isIdentified());
        QCOMPARE(value->ageMs(), -1);
        QVERIFY(!AeroraTelemetryState::carriesValue(value->stateClass()));
        QVERIFY(!AeroraTelemetryState::isCurrent(value->stateClass()));
    }

    // Exactly one authoritative item. If a second ever acquires KindEffective, two values both
    // claim to be what the aircraft is doing, and the second architectural principle has nothing
    // left to prefer.
    QCOMPARE(model->effectiveState()->declaredKind(), AeroraTelemetryState::KindEffective);
    for (const AeroraReportedValue *const value : model->reportedValues()) {
        if (value == model->effectiveState()) {
            continue;
        }
        QVERIFY2(value->declaredKind() == AeroraTelemetryState::KindReported,
                 qPrintable(QStringLiteral("%1 is not KindReported").arg(value->itemId())));
    }

    // The configuration accessor addresses the two confirmed physical configurations and nothing
    // else.
    QCOMPARE(model->configuration(AeroraPowerModel::Configuration::Tether), model->tetherConfiguration());
    QCOMPARE(model->configuration(AeroraPowerModel::Configuration::Battery), model->batteryConfiguration());
}

void AeroraPowerModelTest::_reportsDoNotLeakBetweenItems_data()
{
    QTest::addColumn<QString>("reportedItemId");

    const QStringList ids = expectedItemIds();
    for (const QString &id : ids) {
        QTest::newRow(id.toUtf8().constData()) << id;
    }
}

void AeroraPowerModelTest::_reportsDoNotLeakBetweenItems()
{
    QFETCH(QString, reportedItemId);

    ManualScheduler scheduler;
    AeroraPowerMock mock(&scheduler);
    AeroraPowerModel *const model = mock.model();

    AeroraReportedValue *const reported = model->reportedValue(reportedItemId);
    QVERIFY(reported != nullptr);

    const quint64 stampUs = mock.nowUs();
    QSignalSpy reportedSpy(reported, &AeroraReportedValue::reportChanged);

    reported->recordReport(QString::fromLatin1(kToken), stampUs, AeroraTelemetrySource::fromSendingComponent(1, 1));

    _consumeUnconfiguredThresholdWarning(reported);

    QCOMPARE(reportedSpy.count(), 1);
    QCOMPARE(reported->reportedToken(), QString::fromLatin1(kToken));
    QCOMPARE(reported->receiveTimeUs(), stampUs);

    // No threshold is configured for the power freshness class anywhere in this build (Q-036), so
    // an observed item reaches its declared kind with no freshness verdict behind it.
    QVERIFY(!reported->freshnessEvaluated());

    // Nothing else moved. This is the assertion that keeps the tether configuration independent of
    // the battery configuration, the watchdog event independent of the power state and the fault,
    // and the fault independent of everything.
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

void AeroraPowerModelTest::_unknownAndUnavailableStayDistinct()
{
    ManualScheduler scheduler;
    AeroraPowerMock mock(&scheduler);
    AeroraPowerModel *const model = mock.model();

    AeroraReportedValue *const tether = model->tetherConfiguration();

    QCOMPARE(tether->stateClass(), AeroraTelemetryState::Unknown);

    // A vehicle capability reported absent does not make an item unavailable: no capability
    // identifier is evidenced, so the model maps none onto an item.
    QVERIFY(mock.reportCapabilityUnavailable(QString::fromLatin1(kCapabilityKey)));
    QCOMPARE(tether->stateClass(), AeroraTelemetryState::Unknown);

    mock.reportTetherConfiguration(QString::fromLatin1(kToken));
    _consumeUnconfiguredThresholdWarning(tether);
    QCOMPARE(tether->stateClass(), AeroraTelemetryState::Reported);

    // Only a reported capability absence on the item produces UNAVAILABLE, and it outranks an
    // observation that already arrived.
    QVERIFY(mock.markItemCapabilityAbsent(tether->itemId()));
    QCOMPARE(tether->stateClass(), AeroraTelemetryState::Unavailable);

    static_assert(AeroraTelemetryState::Unknown != AeroraTelemetryState::Unavailable,
                  "IF-UIS-005: unknown and unavailable must not collapse into each other");

    // IF-PWR-003's failure behaviour: with the active source unknown, both configurations are shown
    // as possible rather than one assumed. Returning the item to CapabilityUnknown must restore the
    // honest answer rather than latch the absence.
    QVERIFY(mock.setItemCapability(tether->itemId(), AeroraStateValue::CapabilityUnknown));
    QCOMPARE(tether->stateClass(), AeroraTelemetryState::Reported);
}

void AeroraPowerModelTest::_noTransitionIsInferredFromAConfigurationChange()
{
    ManualScheduler scheduler;
    AeroraPowerMock mock(&scheduler);
    AeroraPowerModel *const model = mock.model();

    QSignalSpy transitionSpy(model->sourceTransition(), &AeroraReportedValue::reportChanged);

    // The sequence a transition would look like if the ground station were inferring one: the
    // tether configuration reports one thing, then the battery configuration reports another, then
    // the tether reports something else again. Three reports, two configurations, zero transitions.
    mock.reportTetherConfiguration(QString::fromLatin1(kToken));
    scheduler.advanceBy(1s);
    mock.reportBatteryConfiguration(QString::fromLatin1(kToken));
    scheduler.advanceBy(1s);
    mock.reportTetherConfiguration(QString::fromLatin1(kOtherToken));

    // Baseline 49: the two configurations are "not yet a claim of automatic in-flight source
    // switching". Nothing here derives a transition, and nothing derives an effective state either.
    QCOMPARE(transitionSpy.count(), 0);
    QVERIFY(!model->sourceTransition()->isObserved());
    QCOMPARE(model->sourceTransition()->stateClass(), AeroraTelemetryState::Unknown);
    QVERIFY(!model->effectiveState()->isObserved());
    QCOMPARE(model->effectiveState()->stateClass(), AeroraTelemetryState::Unknown);

    // A transition becomes observable only when the vehicle reports one. Q-026 is therefore not
    // blocking: if transitions exist the item populates, and if they do not it stays unknown.
    mock.reportSourceTransition(QString::fromLatin1(kToken));

    _consumeUnconfiguredThresholdWarning(model->sourceTransition());

    QCOMPARE(transitionSpy.count(), 1);
    QCOMPARE(model->sourceTransition()->stateClass(), AeroraTelemetryState::Reported);

    // Even a reported transition is not an effective state, and does not become one.
    QVERIFY(!model->effectiveState()->isObserved());
    QVERIFY(!AeroraTelemetryState::isAuthoritative(model->sourceTransition()->stateClass()));
}

void AeroraPowerModelTest::_staleEffectiveStateIsNeverReplacedByAReport()
{
    ManualScheduler scheduler;
    AeroraPowerMock mock(&scheduler);
    AeroraPowerModel *const model = mock.model();

    // The production build registers no threshold, so the test registers one to drive the
    // transition. This is the mechanism under test, not a value being proposed: Q-036 owns the
    // number and this is not it.
    mock.freshness()->setThreshold(QString::fromLatin1(kPowerFreshnessClass), 5s);

    mock.reportEffectiveState(QString::fromLatin1(kToken));
    mock.reportReportedState(QString::fromLatin1(kToken));

    QCOMPARE(model->effectiveState()->stateClass(), AeroraTelemetryState::Effective);
    QCOMPARE(model->reportedState()->stateClass(), AeroraTelemetryState::Reported);
    QVERIFY(AeroraTelemetryState::isAuthoritative(model->effectiveState()->stateClass()));
    QVERIFY(!AeroraTelemetryState::isAuthoritative(model->reportedState()->stateClass()));

    scheduler.advanceBy(6s);

    // An aged-out effective value is STALE, shown with its age, and is no longer current. It does
    // not keep claiming to be what the aircraft is doing.
    QCOMPARE(model->effectiveState()->stateClass(), AeroraTelemetryState::Stale);
    QVERIFY(!AeroraTelemetryState::isCurrent(model->effectiveState()->stateClass()));
    QVERIFY(model->effectiveState()->freshnessEvaluated());
    QVERIFY(model->effectiveState()->ageMs() >= 6000);

    // A fresh ordinary report arrives while the effective value is stale. This is the moment the
    // divergence matters, and the regression to catch: a fresh REPORTED value must not be promoted
    // into the gap the stale EFFECTIVE value left.
    mock.reportReportedState(QString::fromLatin1(kOtherToken));

    QCOMPARE(model->reportedState()->stateClass(), AeroraTelemetryState::Reported);
    QVERIFY(!AeroraTelemetryState::isAuthoritative(model->reportedState()->stateClass()));
    QCOMPARE(model->effectiveState()->stateClass(), AeroraTelemetryState::Stale);
    QCOMPARE(model->effectiveState()->reportedToken(), QString::fromLatin1(kToken));

    // Only the vehicle reporting the effective state again restores it.
    mock.reportEffectiveState(QString::fromLatin1(kOtherToken));
    QCOMPARE(model->effectiveState()->stateClass(), AeroraTelemetryState::Effective);
}

void AeroraPowerModelTest::_capabilityGatesExtendedSurface()
{
    ManualScheduler scheduler;
    AeroraPowerMock mock(&scheduler);
    AeroraPowerModel *const model = mock.model();

    QSignalSpy permittedSpy(model, &AeroraPowerModel::extendedSurfacePermittedChanged);

    // IF-PWR-012: no power capability reported means a consumer offers the generic battery model
    // only. That is correct behaviour for an aircraft that has said nothing, not degraded
    // behaviour.
    QVERIFY(!model->isExtendedSurfacePermitted());
    QVERIFY(model->reportedCapabilityKeys().isEmpty());

    QVERIFY(mock.reportCapabilityUnavailable(QString::fromLatin1(kCapabilityKey)));
    QVERIFY(!model->isExtendedSurfacePermitted());
    QCOMPARE(permittedSpy.count(), 0);

    // Rule 6 of the capability model: an unrecognised identifier is retained and recorded.
    QCOMPARE(model->reportedCapabilityKeys(), QStringList{QString::fromLatin1(kCapabilityKey)});

    QVERIFY(mock.reportCapabilityAvailable(QString::fromLatin1(kCapabilityKey)));
    QVERIFY(model->isExtendedSurfacePermitted());
    QCOMPARE(permittedSpy.count(), 1);

    mock.clearCapabilityReports();
    QVERIFY(!model->isExtendedSurfacePermitted());
    QCOMPARE(permittedSpy.count(), 2);
}

void AeroraPowerModelTest::_prohibitionsAndThresholdsArePinned()
{
    ManualScheduler scheduler;
    AeroraPowerMock mock(&scheduler);

    // The seven refusals in 11-power-implementation.md, as build properties rather than as
    // inspection findings. Each static_assert fails the build if the position is ever reversed
    // without the reviewer decision the baselines require.
    static_assert(!AeroraPowerModel::kStationInitiatedPowerActionImplemented,
                  "AERORA-POWER-REQ-008: the ground station initiates no power transition");
    static_assert(!AeroraPowerModel::kPowerArbitrationImplemented,
                  "Source arbitration is a vehicle-side electrical function, not a ground-station one");
    static_assert(!AeroraPowerModel::kAutomaticSourceTransitionImplemented,
                  "Q-026: transition behaviour requires separate engineering and verification");
    static_assert(!AeroraPowerModel::kRemainingEnergyEstimationImplemented,
                  "IF-PWR-008: remaining energy is presented only where the vehicle validates it");
    static_assert(!AeroraPowerModel::kWatchdogToPowerRelationshipEvidenced,
                  "No watchdog-to-power relationship is evidenced in either direction");

    // No voltage, current, power, temperature or timing threshold is defined anywhere in the
    // module. Baseline 13 places high-voltage isolation, converter protection, grounding, connector
    // interlock and tether fault protection outside the ground station entirely, so a threshold
    // here would be a limit nobody specified being enforced by the one component with no authority
    // to enforce it.
    static_assert(AeroraPowerModel::kDefinedThresholdCount == 0,
                  "No power threshold is evidenced; the ground station defines none");

    // And none is registered at runtime either. The staleness mechanism is complete and
    // deliberately unparameterised until Q-036 supplies the numbers.
    QVERIFY(!mock.freshness()->hasThreshold(QString::fromLatin1(kPowerFreshnessClass)));
    QVERIFY(mock.freshness()->configuredClasses().isEmpty());
}

void AeroraPowerModelTest::_disconnectClearsReportsAndRestoresNothing()
{
    ManualScheduler scheduler;
    AeroraPowerMock mock(&scheduler);
    AeroraPowerModel *const model = mock.model();

    mock.setReportingSource(1, 1);
    QVERIFY(mock.reportCapabilityAvailable(QString::fromLatin1(kCapabilityKey)));
    mock.reportTetherConfiguration(QString::fromLatin1(kToken));
    mock.reportBatteryConfiguration(QString::fromLatin1(kToken));
    mock.reportEffectiveState(QString::fromLatin1(kToken));
    mock.reportReportedState(QString::fromLatin1(kToken));
    mock.reportSourceTransition(QString::fromLatin1(kToken));
    mock.reportFault(QString::fromLatin1(kToken));
    mock.reportWatchdogEvent(QString::fromLatin1(kToken));

    for (const AeroraReportedValue *const value : model->reportedValues()) {
        QVERIFY(value->isObserved());
    }

    mock.disconnectPower();

    for (const AeroraReportedValue *const value : model->reportedValues()) {
        QVERIFY(!value->isObserved());
        QCOMPARE(value->stateClass(), AeroraTelemetryState::Unknown);
        QVERIFY(value->reportedToken().isEmpty());
        QVERIFY(!value->source().isIdentified());
    }
    QVERIFY(!model->isExtendedSurfacePermitted());

    // A reconnection re-establishes everything from the new connection's reports. A fault, a
    // configuration or a watchdog event from the previous aircraft is not restored onto the next
    // one.
    QVERIFY(mock.reportCapabilityAvailable(QString::fromLatin1(kCapabilityKey)));
    QVERIFY(model->isExtendedSurfacePermitted());

    for (const AeroraReportedValue *const value : model->reportedValues()) {
        QVERIFY2(!value->isObserved(),
                 qPrintable(QStringLiteral("%1 was restored after a reconnection").arg(value->itemId())));
        QVERIFY(value->reportedToken().isEmpty());
    }
}

void AeroraPowerModelTest::_twoModelsShareOneEvaluatorAndNeitherOwnsIt()
{
    // The composition hands the *same* AeroraFreshness to the payload model and to this one. That
    // is the intended architecture - staleness thresholds are per class, not per model, so one
    // evaluator per vehicle keeps a class's threshold identical wherever it is consumed - and it is
    // also the one place a double-ownership defect could hide. If either model adopted the
    // evaluator, destroying that model would take the evaluator out from under the other, and the
    // survivor's QPointer would silently go null mid-flight.
    //
    // M-Vehicle's AeroraVehicleIntegrationTest asserts that two *vehicles* get two evaluators. This
    // asserts the complement: within one vehicle, two models get one evaluator and neither owns it.
    ManualScheduler scheduler;
    AeroraTelemetrySet telemetrySet(&scheduler);
    AeroraVehicleCapabilities capabilities;

    AeroraFreshness *const freshness = telemetrySet.freshness();
    QVERIFY(freshness);

    {
        AeroraPowerModel power(freshness, &capabilities);
        AeroraPayloadModel payload(freshness, &capabilities);

        // One evaluator, owned by neither model and by the telemetry set alone.
        QCOMPARE(freshness->parent(), static_cast<QObject *>(&telemetrySet));
        QVERIFY(freshness->parent() != static_cast<QObject *>(&power));
        QVERIFY(freshness->parent() != static_cast<QObject *>(&payload));

        // Items are children of their own model, which ties their lifetime to the vehicle once the
        // model is reparented onto the services object.
        for (AeroraReportedValue *const value : power.reportedValues()) {
            QCOMPARE(value->parent(), static_cast<QObject *>(&power));
        }

        // A threshold registered once reaches both models, because the class is the key rather than
        // the model. This is the behaviour the shared evaluator exists to produce.
        freshness->setThreshold(QString::fromLatin1(kPowerFreshnessClass), 5s);

        power.effectiveState()->recordReport(QString::fromLatin1(kToken),
                                             scheduler.nowUs(),
                                             AeroraTelemetrySource::fromSendingComponent(1, 1));
        QCOMPARE(power.effectiveState()->stateClass(), AeroraTelemetryState::Effective);
        QVERIFY(power.effectiveState()->freshnessEvaluated());

        scheduler.advanceBy(6s);
        QCOMPARE(power.effectiveState()->stateClass(), AeroraTelemetryState::Stale);

        // The payload model is on its own freshness class, so the power threshold does not reach
        // it. Classes are independent keys, not a global setting.
        QVERIFY(!freshness->hasThreshold(QStringLiteral("aeroraPayload")));
    }

    // Both models destroyed; the evaluator and its threshold survive, because neither model ever
    // owned it. A double-free or a premature destruction would surface here.
    QVERIFY(freshness->hasThreshold(QString::fromLatin1(kPowerFreshnessClass)));
    QCOMPARE(telemetrySet.freshness(), freshness);
}

void AeroraPowerModelTest::_destroyedEvaluatorMustNotRefreshAStaleObservation()
{
    // Permanent regression guard for HO-TAB6-01, now fixed in AeroraReportedValue's staleness
    // latch. This is the model-level half of the invariant; the primitive is covered directly by
    // AeroraReportedValueTest.
    //
    // The defect was: _observation() returned ObservedFreshnessUnevaluated once its evaluator
    // QPointer went null, and resolve() maps that to the item's declared kind - so an item already
    // aged out to STALE resolved back to EFFECTIVE the moment the evaluator was destroyed, and
    // isCurrent() answered true for a reading known to be old. That contradicted DEG-04.
    //
    // It matters most on this item specifically: powerEffectiveState is the only KindEffective item
    // in the overlay, so it is the value a consumer is most likely to read through isCurrent()
    // before treating it as what the aircraft is doing now. The window is not exotic either - the
    // telemetry set that owns the evaluator is attached first, so Qt destroys it before this model
    // on every vehicle teardown.
    ManualScheduler scheduler;
    AeroraVehicleCapabilities capabilities;

    auto *const telemetrySet = new AeroraTelemetrySet(&scheduler);
    telemetrySet->freshness()->setThreshold(QString::fromLatin1(kPowerFreshnessClass), 5s);

    AeroraPowerModel model(telemetrySet->freshness(), &capabilities);
    AeroraReportedValue *const effective = model.effectiveState();

    effective->recordReport(QString::fromLatin1(kToken),
                            scheduler.nowUs(),
                            AeroraTelemetrySource::fromSendingComponent(1, 1));
    scheduler.advanceBy(6s);

    // Aged out against a configured threshold: STALE, and explicitly not current.
    QCOMPARE(effective->stateClass(), AeroraTelemetryState::Stale);
    QVERIFY(!AeroraTelemetryState::isCurrent(effective->stateClass()));
    QVERIFY(effective->freshnessEvaluated());

    delete telemetrySet;

    // Losing the evaluator makes freshness unknowable. It must not make a known-stale reading
    // current again: staleness already established is a fact about the observation, not a fact
    // about the evaluator.
    QCOMPARE(effective->stateClass(), AeroraTelemetryState::Stale);
    QVERIFY(!AeroraTelemetryState::isCurrent(effective->stateClass()));
    QVERIFY(!AeroraTelemetryState::isAuthoritative(effective->stateClass()));

    // The verdict is retained rather than withdrawn, so a consumer is still told that a freshness
    // verdict exists - it is just no longer able to age the value, because the clock is gone.
    QVERIFY(effective->freshnessEvaluated());
    QCOMPARE(effective->ageMs(), -1);

    // The datum and its provenance survive the degradation, so nothing is lost by it.
    QCOMPARE(effective->reportedToken(), QString::fromLatin1(kToken));
    QVERIFY(effective->isObserved());
    QCOMPARE(effective->source().attribution(), AeroraTelemetrySource::SendingComponent);
}

UT_REGISTER_TEST(AeroraPowerModelTest, TestLabel::Unit)

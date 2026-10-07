#include "AeroraReportedValueTest.h"

#include <chrono>

#include <QtCore/QRegularExpression>
#include <QtTest/QSignalSpy>

#include "AeroraFreshness.h"
#include "AeroraReportedValue.h"
#include "AeroraStateValue.h"
#include "AeroraTelemetrySource.h"
#include "AeroraTelemetryState.h"
#include "ManualScheduler.h"

namespace {

using namespace std::chrono_literals;

/// M-Telemetry's category. AeroraFreshness warns on it, not on the supervisory category.
constexpr const char *kTelemetryLogCategory = "aerora.telemetry";

// Chosen by this test and meaning nothing. No Aerora item id, freshness class or reported value is
// specified anywhere, so nothing here may look like a specification.
constexpr const char *kItemId = "test.item";
constexpr const char *kFreshnessClass = "test.class";
constexpr const char *kToken = "test-token";
constexpr const char *kOtherToken = "test-other-token";

/// The threshold these tests drive the transition with. A test fixture value, not a proposal:
/// Q-036 owns the real numbers and no production threshold exists.
constexpr std::chrono::microseconds kTestThreshold = 5s;

void addKindRows()
{
    QTest::addColumn<int>("declaredKind");
    QTest::addColumn<int>("expectedFreshClass");

    QTest::newRow("reported") << static_cast<int>(AeroraTelemetryState::KindReported)
                              << static_cast<int>(AeroraTelemetryState::Reported);
    QTest::newRow("effective") << static_cast<int>(AeroraTelemetryState::KindEffective)
                               << static_cast<int>(AeroraTelemetryState::Effective);
    QTest::newRow("requested") << static_cast<int>(AeroraTelemetryState::KindRequested)
                               << static_cast<int>(AeroraTelemetryState::Requested);
    QTest::newRow("proposed") << static_cast<int>(AeroraTelemetryState::KindProposed)
                              << static_cast<int>(AeroraTelemetryState::Proposed);
}

AeroraTelemetrySource testSource()
{
    return AeroraTelemetrySource::fromSendingComponent(1, 1);
}

} // namespace

void AeroraReportedValueTest::_staleSurvivesEvaluatorDestruction_data()
{
    addKindRows();
}

void AeroraReportedValueTest::_staleSurvivesEvaluatorDestruction()
{
    QFETCH(int, declaredKind);

    const auto kind = static_cast<AeroraTelemetryState::Kind>(declaredKind);

    ManualScheduler scheduler;

    // Owned by this test rather than by a telemetry set, so the evaluator can be destroyed
    // independently of the item - which is what a vehicle teardown does, since the set that owns
    // the evaluator is attached first and Qt deletes children in attachment order.
    auto *const freshness = new AeroraFreshness(&scheduler);
    freshness->setThreshold(QString::fromLatin1(kFreshnessClass), kTestThreshold);

    AeroraReportedValue value(QString::fromLatin1(kItemId),
                              kind,
                              QString::fromLatin1(kFreshnessClass),
                              freshness);

    value.recordReport(QString::fromLatin1(kToken), scheduler.nowUs(), testSource());

    // Inside the window: the item's declared class, with a real freshness verdict behind it.
    QVERIFY(value.freshnessEvaluated());
    QVERIFY(value.stateClass() != AeroraTelemetryState::Stale);

    scheduler.advanceBy(6s);

    // Aged out against a configured threshold.
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Stale);
    QVERIFY(!AeroraTelemetryState::isCurrent(value.stateClass()));
    QVERIFY(!AeroraTelemetryState::isAuthoritative(value.stateClass()));
    QVERIFY(value.freshnessEvaluated());

    delete freshness;

    // HO-TAB6-01, the regression this test exists for. Losing the evaluator makes freshness
    // unknowable; it must not make a known-stale reading current again. Staleness already
    // established is a fact about the observation, not about the evaluator.
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Stale);
    QVERIFY(!AeroraTelemetryState::isCurrent(value.stateClass()));
    QVERIFY(!AeroraTelemetryState::isAuthoritative(value.stateClass()));

    // The datum and its provenance survive the degradation; only the age becomes unavailable,
    // because there is no clock left to measure it against.
    QCOMPARE(value.reportedToken(), QString::fromLatin1(kToken));
    QVERIFY(value.isObserved());
    QCOMPARE(value.source().attribution(), AeroraTelemetrySource::SendingComponent);
    QCOMPARE(value.ageMs(), -1);
}

void AeroraReportedValueTest::_staleSurvivesThresholdRemoval()
{
    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);
    freshness.setThreshold(QString::fromLatin1(kFreshnessClass), kTestThreshold);

    AeroraReportedValue value(QString::fromLatin1(kItemId),
                              AeroraTelemetryState::KindEffective,
                              QString::fromLatin1(kFreshnessClass),
                              &freshness);

    value.recordReport(QString::fromLatin1(kToken), scheduler.nowUs(), testSource());
    scheduler.advanceBy(6s);

    QCOMPARE(value.stateClass(), AeroraTelemetryState::Stale);

    // Removing a threshold is a configuration change, not evidence that the aircraft reported
    // anything new. It must not withdraw a verdict already reached.
    freshness.clearThreshold(QString::fromLatin1(kFreshnessClass));

    // The class now has no threshold, so the evaluator warns once that it cannot reach a verdict.
    expectLogMessage(kTelemetryLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("No freshness threshold configured")));
    const AeroraTelemetryState::Class afterRemoval = value.stateClass();
    verifyExpectedLogMessage();

    QCOMPARE(afterRemoval, AeroraTelemetryState::Stale);
    QVERIFY(!AeroraTelemetryState::isCurrent(afterRemoval));
}

void AeroraReportedValueTest::_expiryLatchesTheVerdictWithoutAConsumerRead()
{
    ManualScheduler scheduler;

    auto *const freshness = new AeroraFreshness(&scheduler);
    freshness->setThreshold(QString::fromLatin1(kFreshnessClass), kTestThreshold);

    AeroraReportedValue value(QString::fromLatin1(kItemId),
                              AeroraTelemetryState::KindEffective,
                              QString::fromLatin1(kFreshnessClass),
                              freshness);

    QSignalSpy changedSpy(&value, &AeroraReportedValue::reportChanged);

    value.recordReport(QString::fromLatin1(kToken), scheduler.nowUs(), testSource());

    // Crossing the threshold fires the scheduled expiry callback, which announces the transition.
    scheduler.advanceBy(6s);
    QVERIFY(changedSpy.count() >= 2);

    // Nothing has read stateClass() since the transition. The verdict must already be latched,
    // because the expiry callback evaluates before it signals. If the latch depended on a consumer
    // having queried, destroying the evaluator here would take the verdict with it - and a stale
    // value would come back as current for anyone who only read after teardown.
    delete freshness;

    QCOMPARE(value.stateClass(), AeroraTelemetryState::Stale);
    QVERIFY(!AeroraTelemetryState::isCurrent(value.stateClass()));
}

void AeroraReportedValueTest::_newReportWithdrawsTheStaleVerdict()
{
    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);
    freshness.setThreshold(QString::fromLatin1(kFreshnessClass), kTestThreshold);

    AeroraReportedValue value(QString::fromLatin1(kItemId),
                              AeroraTelemetryState::KindEffective,
                              QString::fromLatin1(kFreshnessClass),
                              &freshness);

    value.recordReport(QString::fromLatin1(kToken), scheduler.nowUs(), testSource());
    scheduler.advanceBy(6s);
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Stale);

    // A new report opens a new freshness epoch: the verdict described a reading this one replaces.
    value.recordReport(QString::fromLatin1(kOtherToken), scheduler.nowUs(), testSource());

    QCOMPARE(value.stateClass(), AeroraTelemetryState::Effective);
    QVERIFY(AeroraTelemetryState::isCurrent(value.stateClass()));
    QCOMPARE(value.reportedToken(), QString::fromLatin1(kOtherToken));

    // Withdrawn, not merely masked: time passing inside the new window keeps the value current,
    // which it could not do if the previous epoch's verdict were still latched underneath.
    scheduler.advanceBy(1s);
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Effective);

    // And the new epoch ages out on its own schedule, measured from the new receive time.
    scheduler.advanceBy(6s);
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Stale);
}

void AeroraReportedValueTest::_clearingReportProducesUnknownNotStale()
{
    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);
    freshness.setThreshold(QString::fromLatin1(kFreshnessClass), kTestThreshold);

    AeroraReportedValue value(QString::fromLatin1(kItemId),
                              AeroraTelemetryState::KindReported,
                              QString::fromLatin1(kFreshnessClass),
                              &freshness);

    value.recordReport(QString::fromLatin1(kToken), scheduler.nowUs(), testSource());
    scheduler.advanceBy(6s);
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Stale);

    value.clearReport();

    // With nothing observed there is no reading for a staleness verdict to describe, so the result
    // is UNKNOWN. A retained latch would report STALE about a reading that no longer exists.
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Unknown);
    QVERIFY(!value.isObserved());
    QVERIFY(value.reportedToken().isEmpty());
    QVERIFY(!value.freshnessEvaluated());
    QCOMPARE(value.ageMs(), -1);

    // A clear is not a teardown of the item: a later report starts cleanly rather than inheriting
    // the verdict from before the clear.
    value.recordReport(QString::fromLatin1(kOtherToken), scheduler.nowUs(), testSource());
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Reported);
}

void AeroraReportedValueTest::_evaluatorLossWithoutAPriorVerdictIsNotStale()
{
    ManualScheduler scheduler;

    // No threshold configured, which is the state of every Aerora class in this build (Q-036). The
    // evaluator can therefore never reach a staleness verdict.
    auto *const freshness = new AeroraFreshness(&scheduler);

    AeroraReportedValue value(QString::fromLatin1(kItemId),
                              AeroraTelemetryState::KindReported,
                              QString::fromLatin1(kFreshnessClass),
                              freshness);

    value.recordReport(QString::fromLatin1(kToken), scheduler.nowUs(), testSource());

    expectLogMessage(kTelemetryLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("No freshness threshold configured")));
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Reported);
    verifyExpectedLogMessage();

    QVERIFY(!value.freshnessEvaluated());

    scheduler.advanceBy(1h);
    delete freshness;

    // Evaluator absence on its own must NOT mean stale. The latch preserves a verdict that was
    // reached; it never manufactures one. A value that was never judged stale stays at its declared
    // class with no freshness verdict, which is the honest answer and the pre-existing behaviour.
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Reported);
    QVERIFY(!value.freshnessEvaluated());
    QCOMPARE(value.ageMs(), -1);
}

void AeroraReportedValueTest::_noObservationIsUnknown()
{
    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);
    freshness.setThreshold(QString::fromLatin1(kFreshnessClass), kTestThreshold);

    AeroraReportedValue value(QString::fromLatin1(kItemId),
                              AeroraTelemetryState::KindEffective,
                              QString::fromLatin1(kFreshnessClass),
                              &freshness);

    // Never observed: UNKNOWN regardless of declared kind, and never zero, empty or a default.
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Unknown);
    QVERIFY(!value.isObserved());
    QVERIFY(value.reportedToken().isEmpty());
    QCOMPARE(value.receiveTimeUs(), quint64(0));
    QVERIFY(!value.source().isIdentified());
    QCOMPARE(value.ageMs(), -1);
    QVERIFY(!value.freshnessEvaluated());

    // Time passing cannot make an unobserved item stale: there is nothing to expire.
    scheduler.advanceBy(1h);
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Unknown);
}

void AeroraReportedValueTest::_capabilityAbsentIsUnavailable()
{
    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);
    freshness.setThreshold(QString::fromLatin1(kFreshnessClass), kTestThreshold);

    AeroraReportedValue value(QString::fromLatin1(kItemId),
                              AeroraTelemetryState::KindReported,
                              QString::fromLatin1(kFreshnessClass),
                              &freshness);

    value.recordReport(QString::fromLatin1(kToken), scheduler.nowUs(), testSource());
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Reported);

    // A reported capability absence outranks any observation, including a stale one.
    value.setCapability(AeroraStateValue::CapabilityAbsent);
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Unavailable);

    scheduler.advanceBy(6s);
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Unavailable);

    // UNKNOWN and UNAVAILABLE stay distinct: the first is "nothing reported", the second is "the
    // vehicle reported it does not have this". Collapsing them would manufacture an UNAVAILABLE the
    // vehicle never reported.
    static_assert(AeroraTelemetryState::Unknown != AeroraTelemetryState::Unavailable,
                  "IF-UIS-005: unknown and unavailable must not collapse into each other");
    QVERIFY(AeroraTelemetryState::isNonValue(AeroraTelemetryState::Unknown));
    QVERIFY(AeroraTelemetryState::isNonValue(AeroraTelemetryState::Unavailable));

    // Returning to CapabilityUnknown reveals the underlying observation, which aged out meanwhile.
    value.setCapability(AeroraStateValue::CapabilityUnknown);
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Stale);
}

void AeroraReportedValueTest::_freshObservationResolvesToDeclaredKind_data()
{
    addKindRows();
}

void AeroraReportedValueTest::_freshObservationResolvesToDeclaredKind()
{
    QFETCH(int, declaredKind);
    QFETCH(int, expectedFreshClass);

    const auto kind = static_cast<AeroraTelemetryState::Kind>(declaredKind);
    const auto expected = static_cast<AeroraTelemetryState::Class>(expectedFreshClass);

    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);
    freshness.setThreshold(QString::fromLatin1(kFreshnessClass), kTestThreshold);

    AeroraReportedValue value(QString::fromLatin1(kItemId),
                              kind,
                              QString::fromLatin1(kFreshnessClass),
                              &freshness);

    value.recordReport(QString::fromLatin1(kToken), scheduler.nowUs(), testSource());

    // The declared kind is const and there is no setter, so a fresh observation resolves to exactly
    // the item's own class and nothing promotes it. REQUESTED and PROPOSED in particular never
    // become EFFECTIVE, which is the promotion the state model exists to make impossible.
    QCOMPARE(value.stateClass(), expected);
    QCOMPARE(value.declaredKind(), kind);

    const bool shouldBeAuthoritative = (kind == AeroraTelemetryState::KindEffective);
    QCOMPARE(AeroraTelemetryState::isAuthoritative(value.stateClass()), shouldBeAuthoritative);

    const bool shouldBeSubordinate =
        (kind == AeroraTelemetryState::KindRequested) || (kind == AeroraTelemetryState::KindProposed);
    QCOMPARE(AeroraTelemetryState::isSubordinate(value.stateClass()), shouldBeSubordinate);
}

void AeroraReportedValueTest::_staleIsNeverCurrentForAnyKind_data()
{
    addKindRows();
}

void AeroraReportedValueTest::_staleIsNeverCurrentForAnyKind()
{
    QFETCH(int, declaredKind);

    const auto kind = static_cast<AeroraTelemetryState::Kind>(declaredKind);

    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);
    freshness.setThreshold(QString::fromLatin1(kFreshnessClass), kTestThreshold);

    AeroraReportedValue value(QString::fromLatin1(kItemId),
                              kind,
                              QString::fromLatin1(kFreshnessClass),
                              &freshness);

    value.recordReport(QString::fromLatin1(kToken), scheduler.nowUs(), testSource());
    scheduler.advanceBy(6s);

    // Staleness outranks the declared kind for every kind, including EFFECTIVE. An aged-out
    // authoritative value is not authoritative any more.
    QCOMPARE(value.stateClass(), AeroraTelemetryState::Stale);
    QVERIFY(!AeroraTelemetryState::isCurrent(value.stateClass()));
    QVERIFY(!AeroraTelemetryState::isAuthoritative(value.stateClass()));

    // STALE still carries its datum, so the last reading remains available with its age rather than
    // being discarded - DEG-03.
    QVERIFY(AeroraTelemetryState::carriesValue(value.stateClass()));
    QCOMPARE(value.reportedToken(), QString::fromLatin1(kToken));
    QVERIFY(value.ageMs() >= 6000);
}

UT_REGISTER_TEST(AeroraReportedValueTest, TestLabel::Unit)

#include "AeroraTelemetryAttachmentTest.h"

#include <chrono>
#include <memory>

#include <QtCore/QList>
#include <QtCore/QPair>

#include "AeroraFactGroupBase.h"
#include "AeroraFactGroupRegistrar.h"
#include "AeroraFreshness.h"
#include "AeroraStateValue.h"
#include "AeroraTelemetrySet.h"
#include "AeroraTelemetrySource.h"
#include "AeroraTelemetryState.h"
#include "AeroraVehicleFactGroupRegistrar.h"
#include "Fact.h"
#include "FactMetaData.h"
#include "MAVLinkLib.h"
#include "ManualScheduler.h"

namespace {

using namespace std::chrono_literals;

constexpr const char *kFreshnessClass = "test.class";
constexpr const char *kLogCategory = "aerora.telemetry";

/// Minimal concrete group. Borrows an upstream fact name so the Fact gets real metadata without
/// this test inventing an Aerora fact name, which the dialect owns.
class TestFactGroup : public AeroraFactGroupBase
{
public:
    explicit TestFactGroup(AeroraFreshness *freshness, AeroraTelemetryState::Kind kind, QObject *parent = nullptr)
        : AeroraFactGroupBase(QStringLiteral(":/json/Vehicle/GPSFact.json"),
                              QString::fromLatin1(kFreshnessClass),
                              freshness,
                              parent)
    {
        _value = _addStateFact(&_latFact, kind);
    }

    [[nodiscard]] AeroraStateValue *value() const { return _value; }

    void applyReading(double reading)
    {
        mavlink_message_t message{};
        message.sysid = 1;
        message.compid = 1;

        const quint64 receiveTimeUs = _stampNowUs();
        _recordObservation(_value, message, receiveTimeUs);
        _latFact.setRawValue(reading);
    }

private:
    Fact _latFact = Fact(0, QStringLiteral("lat"), FactMetaData::valueTypeDouble);
    AeroraStateValue *_value = nullptr;
};

/// Stands in for the authorised registrar. Records what it was asked to register, in order, and can
/// refuse a chosen name so the caller's failure reporting is exercised.
class TestRegistrar final : public AeroraFactGroupRegistrar
{
public:
    [[nodiscard]] bool registerFactGroup(const QString &name, FactGroup *factGroup) override
    {
        registered.append(qMakePair(name, factGroup));

        return name != refuseName;
    }

    QList<QPair<QString, FactGroup *>> registered;
    QString refuseName;
};

QStringList registeredNames(const TestRegistrar &registrar)
{
    QStringList names;
    for (const auto &entry : registrar.registered) {
        names << entry.first;
    }

    return names;
}

} // namespace

void AeroraTelemetryAttachmentTest::_serviceNameIsOwnedByTheTelemetryType()
{
    // The module that owns the type publishes the registry key, as the mission, payload and power
    // models already do. The composition root should read this rather than declare its own.
    QCOMPARE(QString::fromUtf8(AeroraTelemetrySet::kServiceName), QStringLiteral("telemetry"));
}

void AeroraTelemetryAttachmentTest::_publishHandsEveryGroupToTheRegistrar()
{
    ManualScheduler scheduler;
    AeroraTelemetrySet set(&scheduler);

    auto *const first = new TestFactGroup(set.freshness(), AeroraTelemetryState::KindReported, &set);
    auto *const second = new TestFactGroup(set.freshness(), AeroraTelemetryState::KindEffective, &set);
    QVERIFY(set.registerGroup(QStringLiteral("aeroraAlpha"), first));
    QVERIFY(set.registerGroup(QStringLiteral("aeroraBravo"), second));

    QVERIFY(!set.isPublished());

    TestRegistrar registrar;
    QVERIFY(set.publishTo(registrar));

    // Every registered group reached the registrar, under its registered name, and the pointers are
    // the groups the set owns rather than copies.
    QCOMPARE(registeredNames(registrar), QStringList({QStringLiteral("aeroraAlpha"), QStringLiteral("aeroraBravo")}));
    QCOMPARE(registrar.registered.at(0).second, static_cast<FactGroup *>(first));
    QCOMPARE(registrar.registered.at(1).second, static_cast<FactGroup *>(second));

    QVERIFY(set.isPublished());
    QVERIFY(set.isSealed());
}

void AeroraTelemetryAttachmentTest::_publishSealsAgainstDynamicGroups()
{
    ManualScheduler scheduler;
    AeroraTelemetrySet set(&scheduler);

    auto *const group = new TestFactGroup(set.freshness(), AeroraTelemetryState::KindReported, &set);
    QVERIFY(set.registerGroup(QStringLiteral("aeroraAlpha"), group));

    TestRegistrar registrar;
    QVERIFY(set.publishTo(registrar));

    // R-027 / AERORA-GCS-REQ-022. The telemetry CSV header is enumerated once; a group arriving
    // after publication would shift every later column, so it is refused rather than accepted and
    // silently corrupting the record.
    auto *const late = new TestFactGroup(set.freshness(), AeroraTelemetryState::KindReported, &set);
    expectLogMessage(kLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("after the set was sealed")));
    QVERIFY(!set.registerGroup(QStringLiteral("aeroraLate"), late));
    verifyExpectedLogMessage();

    QCOMPARE(registrar.registered.size(), qsizetype(1));
    QVERIFY(!set.groupNames().contains(QStringLiteral("aeroraLate")));
}

void AeroraTelemetryAttachmentTest::_publishIsRefusedTwice()
{
    ManualScheduler scheduler;
    AeroraTelemetrySet set(&scheduler);

    auto *const group = new TestFactGroup(set.freshness(), AeroraTelemetryState::KindReported, &set);
    QVERIFY(set.registerGroup(QStringLiteral("aeroraAlpha"), group));

    TestRegistrar registrar;
    QVERIFY(set.publishTo(registrar));

    // A repeated initialize must not re-register names the vehicle already has: the refusals would
    // read as a registration defect rather than as a duplicated call.
    expectLogMessage(kLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("publish a telemetry set twice")));
    QVERIFY(!set.publishTo(registrar));
    verifyExpectedLogMessage();

    QCOMPARE(registrar.registered.size(), qsizetype(1));
}

void AeroraTelemetryAttachmentTest::_refusedGroupIsReportedNotSwallowed()
{
    ManualScheduler scheduler;
    AeroraTelemetrySet set(&scheduler);

    auto *const first = new TestFactGroup(set.freshness(), AeroraTelemetryState::KindReported, &set);
    auto *const second = new TestFactGroup(set.freshness(), AeroraTelemetryState::KindReported, &set);
    QVERIFY(set.registerGroup(QStringLiteral("aeroraAlpha"), first));
    QVERIFY(set.registerGroup(QStringLiteral("aeroraBravo"), second));

    TestRegistrar registrar;
    registrar.refuseName = QStringLiteral("aeroraBravo");

    // A group the vehicle refuses is not present, and a caller that treated publication as
    // successful would show an Aerora panel backed by a group no consumer can reach.
    expectLogMessage(kLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("refused by the registrar")));
    QVERIFY(!set.publishTo(registrar));
    verifyExpectedLogMessage();

    // Publication still attempted every group rather than stopping at the first refusal.
    QCOMPARE(registeredNames(registrar), QStringList({QStringLiteral("aeroraAlpha"), QStringLiteral("aeroraBravo")}));
    QVERIFY(set.isPublished());
}

void AeroraTelemetryAttachmentTest::_concreteRegistrarRefusesWithoutAVehicle()
{
    expectLogMessage(kLogCategory, QtWarningMsg, QRegularExpression(QStringLiteral("Constructed with no vehicle")));
    AeroraVehicleFactGroupRegistrar registrar(nullptr);
    verifyExpectedLogMessage();

    QCOMPARE(registrar.vehicle(), nullptr);

    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);
    TestFactGroup group(&freshness, AeroraTelemetryState::KindReported);

    // No vehicle means no map to register into. The refusal is a return value, not a crash and not
    // a silent no-op: a group that never reached a vehicle must not look like one that did.
    expectLogMessage(kLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("Refused fact group registration")));
    QVERIFY(!registrar.registerFactGroup(QStringLiteral("aeroraAlpha"), &group));
    verifyExpectedLogMessage();

    // The refusal is observable, and the name arrived verbatim.
    QCOMPARE(registrar.refusedNames(), QStringList{QStringLiteral("aeroraAlpha")});
    QVERIFY(registrar.registeredNames().isEmpty());

    // An empty name and a null group are refused before the vehicle is even consulted.
    expectLogMessage(kLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("Refused fact group registration")));
    QVERIFY(!registrar.registerFactGroup(QString(), &group));
    verifyExpectedLogMessage();

    expectLogMessage(kLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("Refused fact group registration")));
    QVERIFY(!registrar.registerFactGroup(QStringLiteral("aeroraBravo"), nullptr));
    verifyExpectedLogMessage();

    QCOMPARE(registrar.refusedNames().size(), qsizetype(3));
}

void AeroraTelemetryAttachmentTest::_concreteRegistrarHoldsNoSharedState()
{
    expectLogMessage(kLogCategory, QtWarningMsg, QRegularExpression(QStringLiteral("Constructed with no vehicle")));
    AeroraVehicleFactGroupRegistrar first(nullptr);
    verifyExpectedLogMessage();

    expectLogMessage(kLogCategory, QtWarningMsg, QRegularExpression(QStringLiteral("Constructed with no vehicle")));
    AeroraVehicleFactGroupRegistrar second(nullptr);
    verifyExpectedLogMessage();

    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);
    TestFactGroup group(&freshness, AeroraTelemetryState::KindReported);

    expectLogMessage(kLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("Refused fact group registration")));
    QVERIFY(!first.registerFactGroup(QStringLiteral("aeroraAlpha"), &group));
    verifyExpectedLogMessage();

    // Two registrars keep separate books. Nothing here is static, cached or process-wide, which is
    // what makes one registrar per vehicle meaningful.
    QCOMPARE(first.refusedNames(), QStringList{QStringLiteral("aeroraAlpha")});
    QVERIFY(second.refusedNames().isEmpty());
    QVERIFY(second.registeredNames().isEmpty());
}

void AeroraTelemetryAttachmentTest::_publishFailureThroughConcreteRegistrarIsObservable()
{
    ManualScheduler scheduler;
    AeroraTelemetrySet set(&scheduler);

    auto *const group = new TestFactGroup(set.freshness(), AeroraTelemetryState::KindReported, &set);
    QVERIFY(set.registerGroup(QStringLiteral("aeroraAlpha"), group));

    expectLogMessage(kLogCategory, QtWarningMsg, QRegularExpression(QStringLiteral("Constructed with no vehicle")));
    AeroraVehicleFactGroupRegistrar registrar(nullptr);
    verifyExpectedLogMessage();

    // Two warnings, in order: the registrar's own refusal, then publishTo() reporting that a group
    // it published was not accepted. Both are required - a caller must not be able to treat a failed
    // publication as a successful one.
    expectLogMessage(kLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("Refused fact group registration")));
    expectLogMessage(kLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("refused by the registrar")));
    QVERIFY(!set.publishTo(registrar));
    verifyExpectedLogMessage();
    verifyExpectedLogMessage();

    QCOMPARE(registrar.refusedNames(), QStringList{QStringLiteral("aeroraAlpha")});
    QVERIFY(registrar.registeredNames().isEmpty());

    // Publication is one-shot even when it fails. Retrying would re-register names on a vehicle that
    // may have accepted some of them, so the set stays sealed and a retry is refused.
    QVERIFY(set.isPublished());
    QVERIFY(set.isSealed());
}

void AeroraTelemetryAttachmentTest::_twoSetsShareNoGroupsOrEvaluator()
{
    ManualScheduler scheduler;
    AeroraTelemetrySet setA(&scheduler);
    AeroraTelemetrySet setB(&scheduler);

    auto *const groupA = new TestFactGroup(setA.freshness(), AeroraTelemetryState::KindEffective, &setA);
    auto *const groupB = new TestFactGroup(setB.freshness(), AeroraTelemetryState::KindEffective, &setB);
    QVERIFY(setA.registerGroup(QStringLiteral("aeroraAlpha"), groupA));
    QVERIFY(setB.registerGroup(QStringLiteral("aeroraAlpha"), groupB));

    // Same group name on two vehicles, two distinct evaluators and two distinct group objects. This
    // is the property the shared-plugin route could not provide.
    QVERIFY(setA.freshness() != setB.freshness());
    QVERIFY(setA.group(QStringLiteral("aeroraAlpha")) != setB.group(QStringLiteral("aeroraAlpha")));

    TestRegistrar registrarA;
    TestRegistrar registrarB;
    QVERIFY(setA.publishTo(registrarA));
    QVERIFY(setB.publishTo(registrarB));
    QVERIFY(registrarA.registered.at(0).second != registrarB.registered.at(0).second);

    // A report into one vehicle leaves the other untouched.
    groupA->applyReading(1.0);

    QVERIFY(groupA->value()->isObserved());
    QVERIFY(!groupB->value()->isObserved());
    QCOMPARE(groupB->value()->stateClass(), AeroraTelemetryState::Unknown);

    QVERIFY(!setA.sharedAcrossVehiclesDetected());
    QVERIFY(!setB.sharedAcrossVehiclesDetected());
}

void AeroraTelemetryAttachmentTest::_staleVerdictSurvivesEvaluatorDestruction()
{
    ManualScheduler scheduler;

    auto freshness = std::make_unique<AeroraFreshness>(&scheduler);
    freshness->setThreshold(QString::fromLatin1(kFreshnessClass), 5s);

    TestFactGroup group(freshness.get(), AeroraTelemetryState::KindEffective);
    AeroraStateValue *const value = group.value();
    QVERIFY(value);

    group.applyReading(42.0);
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Effective);

    scheduler.advanceBy(6s);
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Stale);

    // Destroying the evaluator removes the ability to reach a verdict. It must not withdraw one
    // already reached: an aged value returning to EFFECTIVE is a stale value silently presented as
    // current, which is the failure the whole state model exists to prevent.
    freshness.reset();

    QCOMPARE(value->stateClass(), AeroraTelemetryState::Stale);
    QVERIFY(!AeroraTelemetryState::isCurrent(value->stateClass()));

    // The age is no longer computable and is reported as absent rather than fabricated.
    QCOMPARE(value->ageMs(), qint64(-1));
}

void AeroraTelemetryAttachmentTest::_staleVerdictSurvivesThresholdRemoval()
{
    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);
    freshness.setThreshold(QString::fromLatin1(kFreshnessClass), 5s);

    TestFactGroup group(&freshness, AeroraTelemetryState::KindEffective);
    AeroraStateValue *const value = group.value();
    QVERIFY(value);

    group.applyReading(42.0);
    scheduler.advanceBy(6s);
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Stale);

    // Clearing a threshold removes the rule, not the history. Reconfiguration is not evidence that
    // a value became current again.
    freshness.clearThreshold(QString::fromLatin1(kFreshnessClass));

    // The class now has no threshold, so the next evaluation warns once — the Q-036 condition
    // becoming visible, exactly as it does on a fresh build.
    expectLogMessage(kLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("No freshness threshold configured")));
    const AeroraTelemetryState::Class afterRemoval = value->stateClass();
    verifyExpectedLogMessage();

    QCOMPARE(afterRemoval, AeroraTelemetryState::Stale);
    QVERIFY(!AeroraTelemetryState::isCurrent(afterRemoval));
}

void AeroraTelemetryAttachmentTest::_newReportWithdrawsTheStaleVerdict()
{
    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);
    freshness.setThreshold(QString::fromLatin1(kFreshnessClass), 5s);

    TestFactGroup group(&freshness, AeroraTelemetryState::KindEffective);
    AeroraStateValue *const value = group.value();
    QVERIFY(value);

    group.applyReading(42.0);
    scheduler.advanceBy(6s);
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Stale);

    // A new observation is the only thing that withdraws the verdict, and it does so because the
    // vehicle reported again — not because the ground station decided the value was current.
    group.applyReading(43.0);
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Effective);
    QVERIFY(value->freshnessEvaluated());

    // And it can go stale again on the new stamp, so the latch is not sticky.
    scheduler.advanceBy(6s);
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Stale);

    // Clearing the observation returns the item to never-observed, not to a latched staleness.
    value->clearObservation();
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Unknown);
}

UT_REGISTER_TEST(AeroraTelemetryAttachmentTest, TestLabel::Unit)

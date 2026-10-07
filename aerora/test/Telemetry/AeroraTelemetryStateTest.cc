#include "AeroraTelemetryStateTest.h"

#include <chrono>

#include <QtTest/QSignalSpy>

#include "AeroraFactGroupBase.h"
#include "AeroraFreshness.h"
#include "AeroraStateValue.h"
#include "AeroraTelemetrySet.h"
#include "AeroraTelemetrySource.h"
#include "AeroraTelemetryState.h"
#include "Fact.h"
#include "FactMetaData.h"
#include "MAVLinkLib.h"
#include "ManualScheduler.h"

namespace {

using namespace std::chrono_literals;

constexpr const char *kFreshnessClass = "test.class";
constexpr const char *kLogCategory = "aerora.telemetry";

/// Minimal concrete group. The six production groups are CONTRACT-ONLY because no Aerora dialect
/// exists to parse, so the test supplies the only thing missing: a body that stamps and stores.
///
/// The fact name is borrowed from the upstream GPS metadata resource so the fact gets real metadata
/// (type, decimal places) without this test inventing an Aerora fact name - 06-factgroup-implementation.md
/// leaves Aerora fact names to the dialect, and a Fact with no metadata warns on every write.
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
    [[nodiscard]] Fact *valueFact() { return &_latFact; }

    /// Drives the group as a vehicle dispatch would, without needing a Vehicle.
    void dispatch(const mavlink_message_t &message, const void *vehicleIdentity)
    {
        _noteDispatchIdentity(vehicleIdentity);
        _handleAeroraMessage(nullptr, message);
    }

    /// Records the observation and then the value, which is the order the contract requires.
    void applyReading(const mavlink_message_t &message, double reading)
    {
        const quint64 receiveTimeUs = _stampNowUs();
        _recordObservation(_value, message, receiveTimeUs);
        _latFact.setRawValue(reading);
    }

private:
    Fact _latFact = Fact(0, QStringLiteral("lat"), FactMetaData::valueTypeDouble);
    AeroraStateValue *_value = nullptr;
};

mavlink_message_t messageFrom(uint8_t systemId, uint8_t componentId)
{
    // Only the header identifiers are read by the telemetry layer. No message id is chosen, because
    // no Aerora message exists to choose.
    mavlink_message_t message{};
    message.sysid = systemId;
    message.compid = componentId;

    return message;
}

}  // namespace

void AeroraTelemetryStateTest::_resolutionTable_data()
{
    QTest::addColumn<int>("declaredKind");
    QTest::addColumn<int>("observation");
    QTest::addColumn<int>("expectedClass");

    // Observation outranks kind for the three conditions a producer may not declare: an absent
    // capability, nothing observed, and an observation that has aged out.
    const struct
    {
        const char *tag;
        AeroraTelemetryState::Kind kind;
    } kinds[] = {
        {"reported", AeroraTelemetryState::KindReported},
        {"effective", AeroraTelemetryState::KindEffective},
        {"requested", AeroraTelemetryState::KindRequested},
        {"proposed", AeroraTelemetryState::KindProposed},
    };

    for (const auto &kind : kinds) {
        QTest::addRow("%s/unavailable", kind.tag)
            << static_cast<int>(kind.kind) << static_cast<int>(AeroraTelemetryState::SourceUnavailable)
            << static_cast<int>(AeroraTelemetryState::Unavailable);
        QTest::addRow("%s/not-observed", kind.tag)
            << static_cast<int>(kind.kind) << static_cast<int>(AeroraTelemetryState::NotObserved)
            << static_cast<int>(AeroraTelemetryState::Unknown);
        QTest::addRow("%s/stale", kind.tag)
            << static_cast<int>(kind.kind) << static_cast<int>(AeroraTelemetryState::ObservedStale)
            << static_cast<int>(AeroraTelemetryState::Stale);
    }

    // A fresh observation, and an observation whose freshness has not been evaluated, both resolve to
    // the declared kind. The rows that matter are requested and proposed: fresh, and still not
    // effective.
    const struct
    {
        const char *tag;
        AeroraTelemetryState::Kind kind;
        AeroraTelemetryState::Class expected;
    } declared[] = {
        {"reported", AeroraTelemetryState::KindReported, AeroraTelemetryState::Reported},
        {"effective", AeroraTelemetryState::KindEffective, AeroraTelemetryState::Effective},
        {"requested", AeroraTelemetryState::KindRequested, AeroraTelemetryState::Requested},
        {"proposed", AeroraTelemetryState::KindProposed, AeroraTelemetryState::Proposed},
    };

    for (const auto &row : declared) {
        QTest::addRow("%s/fresh", row.tag)
            << static_cast<int>(row.kind) << static_cast<int>(AeroraTelemetryState::ObservedFresh)
            << static_cast<int>(row.expected);
        QTest::addRow("%s/unevaluated", row.tag)
            << static_cast<int>(row.kind)
            << static_cast<int>(AeroraTelemetryState::ObservedFreshnessUnevaluated)
            << static_cast<int>(row.expected);
    }
}

void AeroraTelemetryStateTest::_resolutionTable()
{
    QFETCH(int, declaredKind);
    QFETCH(int, observation);
    QFETCH(int, expectedClass);

    const auto kind = static_cast<AeroraTelemetryState::Kind>(declaredKind);
    const auto observed = static_cast<AeroraTelemetryState::Observation>(observation);
    const AeroraTelemetryState::Class resolved = AeroraTelemetryState::resolve(kind, observed);

    QCOMPARE(static_cast<int>(resolved), expectedClass);

    // Whatever the observation, only an effective declaration can ever resolve to EFFECTIVE.
    if (kind != AeroraTelemetryState::KindEffective) {
        QVERIFY(resolved != AeroraTelemetryState::Effective);
    }
}

void AeroraTelemetryStateTest::_classPredicates_data()
{
    QTest::addColumn<int>("stateClass");
    QTest::addColumn<bool>("authoritative");
    QTest::addColumn<bool>("current");
    QTest::addColumn<bool>("subordinate");
    QTest::addColumn<bool>("carriesValue");

    QTest::newRow("unknown") << static_cast<int>(AeroraTelemetryState::Unknown) << false << false << false << false;
    QTest::newRow("unavailable") << static_cast<int>(AeroraTelemetryState::Unavailable) << false << false << false
                                 << false;
    QTest::newRow("reported") << static_cast<int>(AeroraTelemetryState::Reported) << false << true << false << true;
    QTest::newRow("effective") << static_cast<int>(AeroraTelemetryState::Effective) << true << true << false << true;
    QTest::newRow("requested") << static_cast<int>(AeroraTelemetryState::Requested) << false << false << true << true;
    QTest::newRow("proposed") << static_cast<int>(AeroraTelemetryState::Proposed) << false << false << true << true;

    // Stale is the row that must carry a value and not be current: the last reading is kept and shown
    // with its age, never presented as describing the aircraft now.
    QTest::newRow("stale") << static_cast<int>(AeroraTelemetryState::Stale) << false << false << false << true;
}

void AeroraTelemetryStateTest::_classPredicates()
{
    QFETCH(int, stateClass);
    QFETCH(bool, authoritative);
    QFETCH(bool, current);
    QFETCH(bool, subordinate);
    QFETCH(bool, carriesValue);

    const auto resolved = static_cast<AeroraTelemetryState::Class>(stateClass);

    QCOMPARE(AeroraTelemetryState::isAuthoritative(resolved), authoritative);
    QCOMPARE(AeroraTelemetryState::isCurrent(resolved), current);
    QCOMPARE(AeroraTelemetryState::isSubordinate(resolved), subordinate);
    QCOMPARE(AeroraTelemetryState::carriesValue(resolved), carriesValue);
    QCOMPARE(AeroraTelemetryState::isNonValue(resolved), !carriesValue);

    // The two non-values both answer isNonValue and are still not interchangeable.
    QVERIFY(AeroraTelemetryState::classToString(AeroraTelemetryState::Unknown)
            != AeroraTelemetryState::classToString(AeroraTelemetryState::Unavailable));
}

void AeroraTelemetryStateTest::_nonAuthoritativeKindsAreNeverEffective_data()
{
    QTest::addColumn<int>("declaredKind");
    QTest::addColumn<int>("expectedClass");

    // A companion proposal and a ground-station request are the two inputs the architecture must
    // never let become aircraft state. Both arrive, both are fresh, neither is effective.
    QTest::newRow("companion proposal") << static_cast<int>(AeroraTelemetryState::KindProposed)
                                        << static_cast<int>(AeroraTelemetryState::Proposed);
    QTest::newRow("gcs request") << static_cast<int>(AeroraTelemetryState::KindRequested)
                                 << static_cast<int>(AeroraTelemetryState::Requested);
}

void AeroraTelemetryStateTest::_nonAuthoritativeKindsAreNeverEffective()
{
    QFETCH(int, declaredKind);
    QFETCH(int, expectedClass);

    const auto kind = static_cast<AeroraTelemetryState::Kind>(declaredKind);

    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);
    freshness.setThreshold(QString::fromLatin1(kFreshnessClass), 5s);

    TestFactGroup group(&freshness, kind);
    AeroraStateValue *const value = group.value();
    QVERIFY(value);

    group.applyReading(messageFrom(1, 1), 12.5);

    // The observation is as fresh as it can be, and the class is still subordinate.
    QCOMPARE(static_cast<int>(value->stateClass()), expectedClass);
    QVERIFY(value->freshnessEvaluated());
    QVERIFY(!AeroraTelemetryState::isAuthoritative(value->stateClass()));
    QVERIFY(!AeroraTelemetryState::isCurrent(value->stateClass()));
    QVERIFY(AeroraTelemetryState::isSubordinate(value->stateClass()));

    // The declared kind is fixed for the item's life, so repeated fresh reports cannot promote it.
    // There is no API that would: the kind is const and has no setter.
    group.applyReading(messageFrom(1, 1), 13.5);
    QCOMPARE(static_cast<int>(value->declaredKind()), declaredKind);
    QCOMPARE(static_cast<int>(value->stateClass()), expectedClass);
}

void AeroraTelemetryStateTest::_freshnessWithoutThresholdIsNotAVerdict()
{
    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);

    // Q-036 is open, so no threshold is configured for any class and none is invented.
    QVERIFY(!freshness.hasThreshold(QString::fromLatin1(kFreshnessClass)));
    QVERIFY(freshness.configuredClasses().isEmpty());

    TestFactGroup group(&freshness, AeroraTelemetryState::KindEffective);
    AeroraStateValue *const value = group.value();
    QVERIFY(value);

    group.applyReading(messageFrom(1, 1), 1.0);

    // Evaluating an unconfigured class warns once, which is how the open threshold stays visible
    // rather than becoming an accidental default.
    expectLogMessage(kLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("No freshness threshold configured")));
    const AeroraTelemetryState::Class initialClass = value->stateClass();
    verifyExpectedLogMessage();

    // The value is observed, the age is available, and no freshness verdict has been reached.
    QVERIFY(value->isObserved());
    QVERIFY(!value->freshnessEvaluated());
    QVERIFY(value->ageMs() >= 0);

    // Crucially: not claimed fresh, and not claimed stale.
    QCOMPARE(initialClass, AeroraTelemetryState::Effective);
    QVERIFY(initialClass != AeroraTelemetryState::Stale);

    // Time passing cannot produce staleness when there is nothing to expire against. Guessing a
    // threshold here is what would turn a slow link into false staleness.
    scheduler.advanceBy(1h);
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Effective);
    QVERIFY(!value->freshnessEvaluated());
    QVERIFY(value->ageMs() >= 3600000);

    // A zero stamp is the encoding of "never observed" and must not erase a real observation.
    const quint64 observedAt = value->receiveTimeUs();
    expectLogMessage(kLogCategory, QtWarningMsg, QRegularExpression(QStringLiteral("zero receive time")));
    value->recordObservation(0, AeroraTelemetrySource::fromSendingComponent(1, 1));
    verifyExpectedLogMessage();
    QCOMPARE(value->receiveTimeUs(), observedAt);
}

void AeroraTelemetryStateTest::_freshnessTransitionsAcrossTheThreshold()
{
    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);
    freshness.setThreshold(QString::fromLatin1(kFreshnessClass), 5s);

    TestFactGroup group(&freshness, AeroraTelemetryState::KindEffective);
    AeroraStateValue *const value = group.value();
    QVERIFY(value);

    QSignalSpy stateSpy(value, &AeroraStateValue::stateChanged);

    group.applyReading(messageFrom(1, 1), 42.0);
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Effective);
    QVERIFY(value->freshnessEvaluated());

    // Just inside the threshold: still current.
    scheduler.advanceBy(4s);
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Effective);
    QVERIFY(AeroraTelemetryState::isCurrent(value->stateClass()));

    const int signalsBeforeExpiry = stateSpy.count();

    // Past the threshold: stale, and the transition is announced so a binding updates without
    // polling. DEG-04 requires the value be marked and aged rather than hidden or shown plain, so the
    // last reading is still there and the age keeps growing.
    scheduler.advanceBy(2s);
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Stale);
    QVERIFY(!AeroraTelemetryState::isCurrent(value->stateClass()));
    QVERIFY(AeroraTelemetryState::carriesValue(value->stateClass()));
    QCOMPARE(group.valueFact()->rawValue().toDouble(), 42.0);
    QVERIFY(value->ageMs() >= 6000);
    QVERIFY(stateSpy.count() > signalsBeforeExpiry);

    // A new report replaces the stamp wholly and the value is current again. Recovery comes from a
    // fresh observation, never from staleness being re-evaluated or from the GCS assuming anything.
    group.applyReading(messageFrom(1, 1), 43.0);
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Effective);
    QVERIFY(value->ageMs() < 5000);
}

void AeroraTelemetryStateTest::_unknownAndUnavailableStayDistinct()
{
    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);
    freshness.setThreshold(QString::fromLatin1(kFreshnessClass), 5s);

    TestFactGroup group(&freshness, AeroraTelemetryState::KindReported);
    AeroraStateValue *const value = group.value();
    QVERIFY(value);

    // Nothing observed and nothing said about the capability: UNKNOWN, not UNAVAILABLE.
    QCOMPARE(value->capability(), AeroraStateValue::CapabilityUnknown);
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Unknown);
    QVERIFY(!value->isObserved());
    QCOMPARE(value->ageMs(), qint64(-1));

    // An unreported capability must not be read as an absent one. Collapsing those is the derivation
    // AERORA-GCS-REQ-029 prohibits, and the distinction is what F3.1-05 leaves open.
    QVERIFY(value->stateClass() != AeroraTelemetryState::Unavailable);

    // A capability reported present does not by itself produce a value.
    value->setCapability(AeroraStateValue::CapabilityPresent);
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Unknown);

    // Only a reported absence produces UNAVAILABLE, and it outranks any observation.
    value->setCapability(AeroraStateValue::CapabilityAbsent);
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Unavailable);
    group.applyReading(messageFrom(1, 1), 7.0);
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Unavailable);

    // Both are non-values, and they are still not the same state.
    QVERIFY(AeroraTelemetryState::isNonValue(AeroraTelemetryState::Unknown));
    QVERIFY(AeroraTelemetryState::isNonValue(AeroraTelemetryState::Unavailable));
    QVERIFY(AeroraTelemetryState::Unknown != AeroraTelemetryState::Unavailable);
}

void AeroraTelemetryStateTest::_reportingSourceIsAttributedToTheSender()
{
    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);
    freshness.setThreshold(QString::fromLatin1(kFreshnessClass), 5s);

    TestFactGroup group(&freshness, AeroraTelemetryState::KindReported);
    AeroraStateValue *const value = group.value();
    QVERIFY(value);

    // Before anything arrives there is no source. An unobserved value is not a value from an
    // anonymous source.
    QVERIFY(!value->source().isIdentified());
    QCOMPARE(value->source().attribution(), AeroraTelemetrySource::Unidentified);

    group.applyReading(messageFrom(7, 42), 1.0);

    // The identifiers are carried through unmodified - observed from the message header, not inferred.
    QCOMPARE(value->source().systemId(), 7);
    QCOMPARE(value->source().componentId(), 42);
    QVERIFY(value->source().isIdentified());

    // They are attributed to the sending component, never to an originating subsystem. For a relayed
    // payload or power value the sender is the flight controller; distinguishing the two is
    // IF-MAV-022 and is OPEN, so nothing here claims to have done it.
    QCOMPARE(value->source().attribution(), AeroraTelemetrySource::SendingComponent);
    QVERIFY(value->source().attribution() != AeroraTelemetrySource::OriginatingComponent);
    QCOMPARE(value->source().toString(), QStringLiteral("sending:7.42"));

    // A different sender replaces the attribution rather than merging with it.
    group.applyReading(messageFrom(7, 1), 2.0);
    QCOMPARE(value->source().componentId(), 1);

    // Clearing an observation drops the source too, so a reconnection cannot inherit the previous
    // aircraft's attribution. The fact keeps its last reading, per DEG-03's requirement that
    // last-known values be retained rather than discarded.
    value->clearObservation();
    QVERIFY(!value->source().isIdentified());
    QCOMPARE(value->stateClass(), AeroraTelemetryState::Unknown);
    QCOMPARE(group.valueFact()->rawValue().toDouble(), 2.0);
}

void AeroraTelemetryStateTest::_receiveTimeIsStampedBeforeTheValueIsVisible()
{
    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);
    freshness.setThreshold(QString::fromLatin1(kFreshnessClass), 5s);

    TestFactGroup group(&freshness, AeroraTelemetryState::KindEffective);
    AeroraStateValue *const value = group.value();
    QVERIFY(value);

    // IF-TLM-010: a value reaching a consumer without a stamp is a defect, and this is the test that
    // detects it. The group's update rate is zero, so rawValueChanged fires synchronously inside
    // setRawValue - which means the stamp must already be in place when this handler runs.
    bool observedDuringValueChange = false;
    quint64 stampDuringValueChange = 0;
    AeroraTelemetryState::Class classDuringValueChange = AeroraTelemetryState::Unknown;

    (void) connect(group.valueFact(), &Fact::rawValueChanged, this, [&](const QVariant &) {
        observedDuringValueChange = value->isObserved();
        stampDuringValueChange = value->receiveTimeUs();
        classDuringValueChange = value->stateClass();
    });

    group.applyReading(messageFrom(1, 1), 99.0);

    QVERIFY(observedDuringValueChange);
    QVERIFY(stampDuringValueChange != 0);
    QCOMPARE(stampDuringValueChange, value->receiveTimeUs());

    // The consumer saw the value already classified, not classified a moment later.
    QCOMPARE(classDuringValueChange, AeroraTelemetryState::Effective);
}

void AeroraTelemetryStateTest::_groupSharedBySecondVehicleIsDetected()
{
    ManualScheduler scheduler;
    AeroraFreshness freshness(&scheduler);

    TestFactGroup group(&freshness, AeroraTelemetryState::KindReported);
    QVERIFY(!group.sharedAcrossVehiclesDetected());

    // Two distinct dispatch identities stand in for two vehicles. The guard compares identity only
    // and never dereferences, so no Vehicle is needed - and no Vehicle pointer is fabricated.
    int firstVehicle = 0;
    int secondVehicle = 0;
    const mavlink_message_t message = messageFrom(1, 1);

    group.dispatch(message, &firstVehicle);
    group.dispatch(message, &firstVehicle);
    QVERIFY(!group.sharedAcrossVehiclesDetected());

    // This is the Q4-002 / R4-001 hazard occurring: one fact group reached by two aircraft, because
    // FirmwarePluginManager caches one plugin per firmware class. It cannot be fixed at this layer,
    // but it must not be silent.
    expectLogMessage(kLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("reached by a second vehicle")));
    group.dispatch(message, &secondVehicle);
    verifyExpectedLogMessage();

    QVERIFY(group.sharedAcrossVehiclesDetected());
}

void AeroraTelemetryStateTest::_setRefusesGroupsAfterSealing()
{
    ManualScheduler scheduler;
    AeroraTelemetrySet set(&scheduler);

    QVERIFY(set.freshness());
    QVERIFY(!set.isSealed());
    QVERIFY(set.groupNames().isEmpty());

    auto *const group = new TestFactGroup(set.freshness(), AeroraTelemetryState::KindReported, &set);
    QVERIFY(set.registerGroup(QStringLiteral("aeroraTest"), group));
    QCOMPARE(set.groupNames(), QStringList{QStringLiteral("aeroraTest")});
    QCOMPARE(set.group(QStringLiteral("aeroraTest")), group);

    // The map handed to FirmwarePlugin::factGroups() is never null, so the upstream attachment loop
    // needs no special case for an Aerora build whose groups are all still contract-only.
    QMap<QString, FactGroup *> *const factGroups = set.factGroups();
    QVERIFY(factGroups);
    QCOMPARE(factGroups->size(), qsizetype(1));
    QVERIFY(set.isSealed());

    // R-027 / AERORA-GCS-REQ-022: the telemetry CSV header is enumerated once at file open, so a
    // group appearing afterwards would shift every later column. Late registration is refused rather
    // than accepted and silently corrupting the record.
    auto *const lateGroup = new TestFactGroup(set.freshness(), AeroraTelemetryState::KindReported, &set);
    expectLogMessage(kLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("after the set was sealed")));
    QVERIFY(!set.registerGroup(QStringLiteral("aeroraLate"), lateGroup));
    verifyExpectedLogMessage();

    QCOMPARE(factGroups->size(), qsizetype(1));
    QVERIFY(!set.groupNames().contains(QStringLiteral("aeroraLate")));
}

UT_REGISTER_TEST(AeroraTelemetryStateTest, TestLabel::Unit)

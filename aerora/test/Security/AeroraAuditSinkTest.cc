#include "AeroraAuditSinkTest.h"

#include <memory>

#include <QtCore/QList>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>
#include <QtTest/QTest>

#include "AeroraAuditSink.h"
#include "AeroraAuthorization.h"
#include "AeroraCommandAuthorizer.h"
#include "AeroraSecurityTestDoubles.h"
#include "MAVLinkLib.h"

using namespace AeroraSecurityTestDoubles;

namespace {

constexpr const char* kAuthorizationLogCategory = "Aerora.Authorization";

/// Records emitted by one `authorize()` call that reaches no policy: `CommandRequested`,
/// `AuthorizationDecision`, `CommandDenied`. `OperatorIdentityResolved` is absent because the
/// decision is reached before the identity stage runs.
///
/// Asserted as an exact number rather than "more than zero" so that deleting an `_audit()` call -
/// which would silently shrink the audit trail without changing any authorization behaviour -
/// fails here.
constexpr quint64 kRecordsPerUnconfiguredAuthorize = 3;

/// Records emitted by one `authorize()` call that is allowed: `CommandRequested`,
/// `OperatorIdentityResolved`, `AuthorizationDecision`. No denial record.
constexpr quint64 kRecordsPerAllowedAuthorize = 3;

QRegularExpression denialWarningPattern()
{
    return QRegularExpression(QStringLiteral("denied mavCommand"));
}

QRegularExpression auditFailureWarningPattern()
{
    return QRegularExpression(QStringLiteral("audit record was not accepted"));
}

AeroraGatedOperation armOperation()
{
    AeroraGatedOperation operation;
    operation.mavCommand = MAV_CMD_COMPONENT_ARM_DISARM;
    operation.targetCompId = MAV_COMP_ID_AUTOPILOT1;
    operation.param1 = 1.0f;
    operation.vehicle.mavlinkSystemId = 1;
    operation.vehicle.resolved = true;
    return operation;
}

} // namespace

void AeroraAuditSinkTest::cleanup()
{
    clearChain();

    UnitTest::cleanup();
}

void AeroraAuditSinkTest::_sequenceIsMonotonicAndNeverZero()
{
    // Zero is reserved for "unstamped", so a default-constructed record must not look ordered or
    // timed.
    const AeroraAuditEvent unstamped;
    QCOMPARE(unstamped.sequence, static_cast<quint64>(0));
    QCOMPARE(unstamped.monotonicUs, static_cast<quint64>(0));
    QVERIFY(!unstamped.utc.isValid());

    // Absolute values are not asserted: the counter is process-wide and earlier tests in the same
    // run will have consumed some. The properties that matter are that it never yields zero and
    // never repeats.
    const quint64 first = AeroraAuditEvent::nextSequence();
    const quint64 second = AeroraAuditEvent::nextSequence();
    const quint64 third = AeroraAuditEvent::nextSequence();

    QVERIFY(first > 0);
    QVERIFY2(second > first, "sequence must be strictly increasing, or two records tie");
    QVERIFY(third > second);
}

void AeroraAuditSinkTest::_stampNowSetsAllThreeTimeFields()
{
    AeroraAuditEvent event;
    event.stampNow();

    // All three, because a record with a wall-clock stamp and no monotonic stamp is unorderable
    // and the omission would be invisible until someone tried to reconstruct a trail.
    QVERIFY(event.utc.isValid());
    QVERIFY(event.monotonicUs > 0);
    QVERIFY(event.sequence > 0);

    AeroraAuditEvent later;
    later.stampNow();

    QVERIFY2(later.sequence > event.sequence, "stampNow must advance the strict order");

    // Non-decreasing, not strictly increasing: two reads of a monotonic clock may return the same
    // value, which is exactly why `sequence` exists alongside it.
    QVERIFY(later.monotonicUs >= event.monotonicUs);
    QVERIFY(later.utc >= event.utc);
}

void AeroraAuditSinkTest::_recordsOfOneCommandAreOrderableBySequence()
{
    auto sink = std::make_shared<RecordingSink>();
    installAllowingChain(sink);

    QVERIFY(AeroraCommandAuthorizer::instance()->isAuthorized(armOperation()));

    QCOMPARE(static_cast<quint64>(sink->events.count()), kRecordsPerAllowedAuthorize);

    // The documented order, which is meaning-bearing: an attempt is recorded before evaluation so
    // that an attempt is auditable even when evaluation cannot proceed.
    QCOMPARE(static_cast<int>(sink->events.at(0).type),
             static_cast<int>(AeroraAuditEventType::CommandRequested));
    QCOMPARE(static_cast<int>(sink->events.at(1).type),
             static_cast<int>(AeroraAuditEventType::OperatorIdentityResolved));
    QCOMPARE(static_cast<int>(sink->events.at(2).type),
             static_cast<int>(AeroraAuditEventType::AuthorizationDecision));

    // The reason sequence exists. These three records are composed microseconds apart, so their
    // QDateTime stamps routinely share a millisecond and cannot order them. utc and monotonicUs
    // are only asserted non-decreasing, because that is all a clock guarantees; sequence is the
    // one field that carries a strict order.
    for (int i = 1; i < sink->events.count(); ++i) {
        const AeroraAuditEvent& previous = sink->events.at(i - 1);
        const AeroraAuditEvent& current = sink->events.at(i);

        QVERIFY2(current.sequence > previous.sequence,
                 "records of one command must be strictly orderable by sequence");
        QVERIFY(previous.sequence > 0);
        QVERIFY(previous.monotonicUs > 0);
        QVERIFY(current.utc.isValid());
        QVERIFY(current.utc >= previous.utc);
        QVERIFY(current.monotonicUs >= previous.monotonicUs);
    }
}

void AeroraAuditSinkTest::_recordCapturesTheEnforcementPostureAtCompositionTime()
{
    // Unconfigured: every record must say so, because P7-AUTH-002 requires the station report when
    // it is not enforcing, and a trail that omits it reads as though the decisions were evaluated.
    auto unenforcedSink = std::make_shared<RecordingSink>();
    installSinkOnly(unenforcedSink);

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    (void) AeroraCommandAuthorizer::instance()->authorize(armOperation());
    verifyExpectedLogMessage();

    QCOMPARE(static_cast<quint64>(unenforcedSink->events.count()),
             kRecordsPerUnconfiguredAuthorize);
    for (const AeroraAuditEvent& event : unenforcedSink->events) {
        QVERIFY2(!event.authorizationEnforced,
                 "a record composed with no policy installed must not claim enforcement");
    }

    // Configured: the positive control. Without it the field could be hard-coded false.
    auto enforcedSink = std::make_shared<RecordingSink>();
    installAllowingChain(enforcedSink);

    QVERIFY(AeroraCommandAuthorizer::instance()->isAuthorized(armOperation()));

    QCOMPARE(static_cast<quint64>(enforcedSink->events.count()), kRecordsPerAllowedAuthorize);
    for (const AeroraAuditEvent& event : enforcedSink->events) {
        QVERIFY(event.authorizationEnforced);
    }
}

void AeroraAuditSinkTest::_recordCarriesTheProducersLossCountAtCompositionTime()
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();

    // Phase 1: lose some records with no sink installed.
    installSinkOnly(nullptr);
    const quint64 lostBeforePhase1 = authorizer->lostAuditRecordCount();

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    (void) authorizer->authorize(armOperation());
    verifyExpectedLogMessage();

    const quint64 lostAfterPhase1 = authorizer->lostAuditRecordCount();
    QCOMPARE(lostAfterPhase1 - lostBeforePhase1, kRecordsPerUnconfiguredAuthorize);

    // Phase 2: install a sink. Every record it now receives must carry the loss count as it stood
    // when the record was composed - IF-LOG-015's requirement that the drop count be written into
    // the record, so an exported set reveals the gap without the live counter.
    auto sink = std::make_shared<RecordingSink>();
    installAllowingChain(sink);

    QVERIFY(authorizer->isAuthorized(armOperation()));
    QCOMPARE(static_cast<quint64>(sink->events.count()), kRecordsPerAllowedAuthorize);

    for (const AeroraAuditEvent& event : sink->events) {
        QCOMPARE(event.recordsLostBeforeThis, lostAfterPhase1);
    }
}

void AeroraAuditSinkTest::_outcomeToStringDistinguishesEveryOutcome()
{
    const QList<AeroraAuditWriteOutcome> outcomes = {
        AeroraAuditWriteOutcome::Accepted,
        AeroraAuditWriteOutcome::DroppedQueueFull,
        AeroraAuditWriteOutcome::FailedStorage,
        AeroraAuditWriteOutcome::SinkUnavailable,
    };

    QStringList rendered;
    for (const AeroraAuditWriteOutcome outcome : outcomes) {
        const QString text = AeroraAuditSink::outcomeToString(outcome);
        QVERIFY(!text.isEmpty());
        QVERIFY2(!text.contains(QStringLiteral("Unknown")),
                 qPrintable(QStringLiteral("unhandled outcome rendered as: %1").arg(text)));
        rendered.append(text);
    }

    // A shared string would make two distinct causes indistinguishable in a log or a record, which
    // is the whole reason the outcomes are separate values.
    QStringList distinct = rendered;
    distinct.removeDuplicates();
    QCOMPARE(distinct.count(), rendered.count());
}

void AeroraAuditSinkTest::_onlyAcceptedIsNotALoss()
{
    QVERIFY(!AeroraAuditSink::isLoss(AeroraAuditWriteOutcome::Accepted));
    QVERIFY(AeroraAuditSink::isLoss(AeroraAuditWriteOutcome::DroppedQueueFull));
    QVERIFY(AeroraAuditSink::isLoss(AeroraAuditWriteOutcome::FailedStorage));
    QVERIFY(AeroraAuditSink::isLoss(AeroraAuditWriteOutcome::SinkUnavailable));
}

void AeroraAuditSinkTest::_noSinkCountsADropAndStillDecides()
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();
    installSinkOnly(nullptr);

    const quint64 droppedBefore = authorizer->droppedAuditRecordCount();
    const quint64 failedBefore = authorizer->failedAuditWriteCount();

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    const AeroraAuthorizationDecision decision = authorizer->authorize(armOperation());
    verifyExpectedLogMessage();

    // A decision is still reached. An absent sink degrades the record; it does not stop the
    // station from deciding.
    QCOMPARE(static_cast<int>(decision.outcome),
             static_cast<int>(AeroraAuthorizationOutcome::DeniedNotConfigured));

    QCOMPARE(authorizer->droppedAuditRecordCount() - droppedBefore,
             kRecordsPerUnconfiguredAuthorize);

    // An absent sink is a configuration state, not a write failure. Attributing it to the wrong
    // counter would report a station that was never configured to record as one whose disk failed.
    QCOMPARE(authorizer->failedAuditWriteCount(), failedBefore);
}

void AeroraAuditSinkTest::_failingSinkCountsAFailedWriteAndDoesNotChangeTheDecision()
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();

    auto sink = std::make_shared<FailingSink>(AeroraAuditWriteOutcome::FailedStorage);
    installAllowingChain(sink);

    const quint64 failedBefore = authorizer->failedAuditWriteCount();
    const quint64 droppedBefore = authorizer->droppedAuditRecordCount();

    // Warned once per sink installation, not once per record: a storage outage would otherwise
    // emit a line for every record of every command and bury the condition it reports.
    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, auditFailureWarningPattern());
    const AeroraAuthorizationDecision decision = authorizer->authorize(armOperation());
    verifyExpectedLogMessage();

    // The assertion WP2-A exists to pin: an unauditable **allow** stays an allow. Whether it
    // should instead be denied is P7-AUDIT-003, an OPEN product decision. Changing this
    // expectation requires that decision to be made first, not a test edit.
    QVERIFY2(decision.allowed(),
             "an audit write failure must not turn an allow into a denial - P7-AUDIT-003 is OPEN");

    QCOMPARE(authorizer->failedAuditWriteCount() - failedBefore, kRecordsPerAllowedAuthorize);
    QCOMPARE(authorizer->droppedAuditRecordCount(), droppedBefore);

    // The sink was actually asked. Without this the counter could be rising because the producer
    // skipped the sink rather than because the sink refused.
    QCOMPARE(static_cast<quint64>(sink->callCount), kRecordsPerAllowedAuthorize);
}

void AeroraAuditSinkTest::_eachFailureOutcomeIsCountedAsALoss_data()
{
    // Carried as int: AeroraAuditWriteOutcome has no metatype registration, and adding one purely
    // for a test column would be test machinery leaking into the interface.
    QTest::addColumn<int>("outcome");

    QTest::newRow("queue full")
        << static_cast<int>(AeroraAuditWriteOutcome::DroppedQueueFull);
    QTest::newRow("storage failure")
        << static_cast<int>(AeroraAuditWriteOutcome::FailedStorage);
    QTest::newRow("sink unavailable")
        << static_cast<int>(AeroraAuditWriteOutcome::SinkUnavailable);
}

void AeroraAuditSinkTest::_eachFailureOutcomeIsCountedAsALoss()
{
    QFETCH(int, outcome);

    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();

    auto sink = std::make_shared<FailingSink>(static_cast<AeroraAuditWriteOutcome>(outcome));
    installAllowingChain(sink);

    const quint64 lostBefore = authorizer->lostAuditRecordCount();

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, auditFailureWarningPattern());
    QVERIFY(authorizer->isAuthorized(armOperation()));
    verifyExpectedLogMessage();

    // Every non-accepted outcome is a loss. A new failure outcome added without extending
    // isLoss() would leave records unaccounted, which is what this row set guards.
    QCOMPARE(authorizer->lostAuditRecordCount() - lostBefore, kRecordsPerAllowedAuthorize);
}

void AeroraAuditSinkTest::_acceptedWritesAreNotCounted()
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();

    auto sink = std::make_shared<RecordingSink>();
    installAllowingChain(sink);

    const quint64 lostBefore = authorizer->lostAuditRecordCount();

    QVERIFY(authorizer->isAuthorized(armOperation()));

    // The positive control. Without it every counter assertion above could pass because the
    // counters increment unconditionally.
    QCOMPARE(authorizer->lostAuditRecordCount(), lostBefore);
    QCOMPARE(static_cast<quint64>(sink->events.count()), kRecordsPerAllowedAuthorize);
}

void AeroraAuditSinkTest::_lossCountersAreMonotonicAcrossSinkReplacement()
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();

    auto failing = std::make_shared<FailingSink>(AeroraAuditWriteOutcome::FailedStorage);
    installAllowingChain(failing);

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, auditFailureWarningPattern());
    QVERIFY(authorizer->isAuthorized(armOperation()));
    verifyExpectedLogMessage();

    const quint64 lostAfterFailure = authorizer->lostAuditRecordCount();
    QVERIFY(lostAfterFailure > 0);

    // Installing a working sink must not clear the count. Records lost under the previous
    // configuration were still lost, and resetting here would let a station erase the evidence of
    // its own audit gap by installing a sink late.
    auto working = std::make_shared<RecordingSink>();
    authorizer->setAuditSink(working);
    QCOMPARE(authorizer->lostAuditRecordCount(), lostAfterFailure);

    QVERIFY(authorizer->isAuthorized(armOperation()));
    QCOMPARE(authorizer->lostAuditRecordCount(), lostAfterFailure);

    // And removing the sink entirely resumes counting from where it was, rather than restarting.
    authorizer->setAuditSink(nullptr);
    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    (void) AeroraCommandAuthorizer::instance()->authorize(armOperation());
    verifyExpectedLogMessage();

    QVERIFY(authorizer->lostAuditRecordCount() > lostAfterFailure);
}

void AeroraAuditSinkTest::_dropAndFailureAreCountedSeparately()
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();

    const quint64 droppedStart = authorizer->droppedAuditRecordCount();
    const quint64 failedStart = authorizer->failedAuditWriteCount();

    // One command with no sink: drops only.
    installSinkOnly(nullptr);
    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    (void) authorizer->authorize(armOperation());
    verifyExpectedLogMessage();

    // One command with a refusing sink: failures only.
    auto failing = std::make_shared<FailingSink>(AeroraAuditWriteOutcome::SinkUnavailable);
    installAllowingChain(failing);
    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, auditFailureWarningPattern());
    QVERIFY(authorizer->isAuthorized(armOperation()));
    verifyExpectedLogMessage();

    const quint64 dropped = authorizer->droppedAuditRecordCount() - droppedStart;
    const quint64 failed = authorizer->failedAuditWriteCount() - failedStart;

    QCOMPARE(dropped, kRecordsPerUnconfiguredAuthorize);
    QCOMPARE(failed, kRecordsPerAllowedAuthorize);

    // The total is the sum, so a consumer showing one number to the operator - IF-LOG-015 - does
    // not have to add the causes itself and cannot miss one.
    QCOMPARE(authorizer->lostAuditRecordCount(),
             authorizer->droppedAuditRecordCount() + authorizer->failedAuditWriteCount());
}

void AeroraAuditSinkTest::_defaultSinkReportsNeitherDurableNorTamperEvident()
{
    const MinimalSink minimal;

    // The honesty default. An implementor who writes the two required methods and nothing else
    // inherits "not evidential", so an in-memory or log-only sink cannot be mistaken for an
    // audit store. Both stay false until Q-037 / Q4-003 resolves with an implemented, tested
    // mechanism.
    QVERIFY2(!minimal.isDurable(), "a sink must not inherit a durability claim");
    QVERIFY2(!minimal.isTamperEvident(), "a sink must not inherit a tamper-evidence claim");
    QVERIFY(!minimal.sinkName().isEmpty());

    // And the same reported through the authorizer, which is the only path a status consumer has.
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();

    installSinkOnly(nullptr);
    QVERIFY(!authorizer->hasAuditSink());
    QVERIFY(authorizer->auditSinkName().isEmpty());
    QVERIFY2(!authorizer->auditIsDurable(), "no sink must report not durable, never unknown-as-true");
    QVERIFY(!authorizer->auditIsTamperEvident());

    installSinkOnly(std::make_shared<MinimalSink>());
    QVERIFY(authorizer->hasAuditSink());
    QCOMPARE(authorizer->auditSinkName(), QStringLiteral("Minimal"));
    QVERIFY2(!authorizer->auditIsDurable(),
             "an installed sink must not make the station report durable audit");
    QVERIFY(!authorizer->auditIsTamperEvident());
}

UT_REGISTER_TEST(AeroraAuditSinkTest, TestLabel::Unit)

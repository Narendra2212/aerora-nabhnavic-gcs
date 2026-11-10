#include "AeroraAuditQueueTest.h"

#include <optional>

#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtTest/QTest>

#include "AeroraAuditQueue.h"
#include "AeroraAuditSink.h"

namespace {

/// An event tagged by its sequence number, which is what the FIFO assertions key on. The other
/// fields are populated so a round-trip proves the whole record survives, not just the tag.
AeroraAuditEvent eventWithSequence(quint64 sequence)
{
    AeroraAuditEvent event;
    event.type = AeroraAuditEventType::AuthorizationDecision;
    event.sequence = sequence;
    event.monotonicUs = sequence * 1000;
    event.operatorId = QStringLiteral("opaque-operator-%1").arg(sequence);
    event.reasonCode = QStringLiteral("reason-%1").arg(sequence);
    event.vehicleMavlinkSystemId = static_cast<int>(sequence);
    event.vehicleResolved = true;
    event.detail = QVariantMap{{QStringLiteral("n"), static_cast<qulonglong>(sequence)}};
    return event;
}

}  // namespace

void AeroraAuditQueueTest::_aFreshQueueIsEmpty()
{
    const AeroraAuditQueue queue(4);

    QVERIFY(queue.isEmpty());
    QVERIFY(!queue.isFull());
    QCOMPARE(queue.size(), qsizetype(0));
}

void AeroraAuditQueueTest::_capacityIsReported()
{
    QCOMPARE(AeroraAuditQueue(4).capacity(), qsizetype(4));
    QCOMPARE(AeroraAuditQueue(1).capacity(), qsizetype(1));
}

void AeroraAuditQueueTest::_anInvalidCapacityIsClampedToOne()
{
    // A zero or negative capacity would refuse every record, which is a worse failure than a tiny
    // queue: it loses the whole trail silently. Clamped defensively rather than asserted.
    QCOMPARE(AeroraAuditQueue(0).capacity(), qsizetype(1));
    QCOMPARE(AeroraAuditQueue(-5).capacity(), qsizetype(1));

    AeroraAuditQueue queue(0);
    QCOMPARE(queue.enqueue(eventWithSequence(1)), AeroraAuditWriteOutcome::Accepted);
    QVERIFY(queue.isFull());
}

void AeroraAuditQueueTest::_enqueueIsAcceptedWhileThereIsRoom()
{
    AeroraAuditQueue queue(3);

    QCOMPARE(queue.enqueue(eventWithSequence(1)), AeroraAuditWriteOutcome::Accepted);
    QCOMPARE(queue.size(), qsizetype(1));
    QVERIFY(!queue.isFull());

    QCOMPARE(queue.enqueue(eventWithSequence(2)), AeroraAuditWriteOutcome::Accepted);
    QCOMPARE(queue.enqueue(eventWithSequence(3)), AeroraAuditWriteOutcome::Accepted);
    QCOMPARE(queue.size(), qsizetype(3));
    QVERIFY(queue.isFull());
}

void AeroraAuditQueueTest::_enqueueIsRefusedWhenFull()
{
    AeroraAuditQueue queue(2);
    QCOMPARE(queue.enqueue(eventWithSequence(1)), AeroraAuditWriteOutcome::Accepted);
    QCOMPARE(queue.enqueue(eventWithSequence(2)), AeroraAuditWriteOutcome::Accepted);

    // Full. The overflow is refused for fullness, and specifically not reported as a storage fault.
    const AeroraAuditWriteOutcome outcome = queue.enqueue(eventWithSequence(3));
    QCOMPARE(outcome, AeroraAuditWriteOutcome::DroppedQueueFull);
    QVERIFY(outcome != AeroraAuditWriteOutcome::FailedStorage);
    QVERIFY(outcome != AeroraAuditWriteOutcome::SinkUnavailable);
}

void AeroraAuditQueueTest::_refusalDoesNotDisturbWhatIsHeld()
{
    AeroraAuditQueue queue(2);
    static_cast<void>(queue.enqueue(eventWithSequence(1)));
    static_cast<void>(queue.enqueue(eventWithSequence(2)));

    static_cast<void>(queue.enqueue(eventWithSequence(99)));  // refused

    // The two accepted records are intact and in order; the refused one left nothing behind.
    QCOMPARE(queue.size(), qsizetype(2));
    QCOMPARE(queue.dequeue().value().sequence, quint64(1));
    QCOMPARE(queue.dequeue().value().sequence, quint64(2));
    QVERIFY(!queue.dequeue().has_value());
}

void AeroraAuditQueueTest::_repeatedOverflowKeepsRefusing()
{
    AeroraAuditQueue queue(1);
    QCOMPARE(queue.enqueue(eventWithSequence(1)), AeroraAuditWriteOutcome::Accepted);

    // A queue that refused once must keep refusing while full rather than, say, quietly overwriting.
    for (int attempt = 0; attempt < 5; ++attempt) {
        QCOMPARE(queue.enqueue(eventWithSequence(100 + attempt)), AeroraAuditWriteOutcome::DroppedQueueFull);
    }
    QCOMPARE(queue.size(), qsizetype(1));
    QCOMPARE(queue.dequeue().value().sequence, quint64(1));
}

void AeroraAuditQueueTest::_dequeueReturnsRecordsInInsertionOrder()
{
    AeroraAuditQueue queue(5);
    for (quint64 sequence = 1; sequence <= 5; ++sequence) {
        static_cast<void>(queue.enqueue(eventWithSequence(sequence)));
    }

    for (quint64 expected = 1; expected <= 5; ++expected) {
        const std::optional<AeroraAuditEvent> record = queue.dequeue();
        QVERIFY(record.has_value());
        QCOMPARE(record.value().sequence, expected);
    }
    QVERIFY(queue.isEmpty());
}

void AeroraAuditQueueTest::_dequeueFromEmptyReturnsNothing()
{
    AeroraAuditQueue queue(2);

    QVERIFY(!queue.dequeue().has_value());

    static_cast<void>(queue.enqueue(eventWithSequence(1)));
    QVERIFY(queue.dequeue().has_value());
    QVERIFY(!queue.dequeue().has_value());  // emptied again
}

void AeroraAuditQueueTest::_dequeueFreesExactlyOneSlot()
{
    AeroraAuditQueue queue(2);
    static_cast<void>(queue.enqueue(eventWithSequence(1)));
    static_cast<void>(queue.enqueue(eventWithSequence(2)));
    QVERIFY(queue.isFull());

    static_cast<void>(queue.dequeue());

    // One slot freed, and exactly one: the next enqueue is accepted, and the one after it refused.
    QVERIFY(!queue.isFull());
    QCOMPARE(queue.enqueue(eventWithSequence(3)), AeroraAuditWriteOutcome::Accepted);
    QCOMPARE(queue.enqueue(eventWithSequence(4)), AeroraAuditWriteOutcome::DroppedQueueFull);
}

void AeroraAuditQueueTest::_interleavedEnqueueAndDequeueStayFifo()
{
    // The order has to hold when the queue is drained as it fills, which is how it runs in practice.
    AeroraAuditQueue queue(3);

    static_cast<void>(queue.enqueue(eventWithSequence(1)));
    static_cast<void>(queue.enqueue(eventWithSequence(2)));
    QCOMPARE(queue.dequeue().value().sequence, quint64(1));

    static_cast<void>(queue.enqueue(eventWithSequence(3)));
    static_cast<void>(queue.enqueue(eventWithSequence(4)));
    QCOMPARE(queue.dequeue().value().sequence, quint64(2));
    QCOMPARE(queue.dequeue().value().sequence, quint64(3));
    QCOMPARE(queue.dequeue().value().sequence, quint64(4));
    QVERIFY(queue.isEmpty());
}

void AeroraAuditQueueTest::_theDequeuedRecordIsTheOneEnqueued()
{
    // Round-trip fidelity: the whole record survives the queue, not just the tag the other cases
    // key on. A queue that dropped a field would be invisible to a sequence-only assertion.
    AeroraAuditQueue queue(2);
    const AeroraAuditEvent original = eventWithSequence(7);
    static_cast<void>(queue.enqueue(original));

    const std::optional<AeroraAuditEvent> record = queue.dequeue();
    QVERIFY(record.has_value());

    const AeroraAuditEvent& out = record.value();
    QCOMPARE(out.type, original.type);
    QCOMPARE(out.sequence, original.sequence);
    QCOMPARE(out.monotonicUs, original.monotonicUs);
    QCOMPARE(out.operatorId, original.operatorId);
    QCOMPARE(out.reasonCode, original.reasonCode);
    QCOMPARE(out.vehicleMavlinkSystemId, original.vehicleMavlinkSystemId);
    QCOMPARE(out.vehicleResolved, original.vehicleResolved);
    QCOMPARE(out.detail, original.detail);
}

UT_REGISTER_TEST(AeroraAuditQueueTest, TestLabel::Unit)

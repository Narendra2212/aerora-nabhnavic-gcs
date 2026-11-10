#pragma once

#include "UnitTest.h"

/// \brief Asserts `AeroraAuditQueue`: that it is a strict bounded FIFO, and that it refuses by
/// fullness alone.
///
/// \section why What is worth testing
///
/// The queue is the component the audit loss accounting rests on, so three things carry the weight:
///
/// \li **The taken-or-refused verdict.** `Accepted` while there is room, `DroppedQueueFull` once
///     there is not, and never anything else - the store's `FailedStorage` and `SinkUnavailable`
///     are not a queue's to report. A verdict that drifted would miscount `lostAuditRecordCount()`.
/// \li **Strict FIFO.** A reader reconstructs order from each record's `sequence`, so a queue that
///     reordered would hand the store an insertion order that disagreed with the sequence - a
///     defect to debug rather than a property to trust. Asserted by sequence identity, including
///     across interleaved enqueue and dequeue.
/// \li **The bound holds.** The queue never grows past capacity, a full queue refuses without
///     losing what it holds, and a dequeue frees exactly one slot.
///
/// \section nothread What this suite does not touch
///
/// Concurrency. The queue holds no lock and this suite is single-threaded, which is the whole point
/// of keeping the buffer a pure data structure: the thread that will drain it is a separate,
/// deliberate choice and is not this type's concern.
class AeroraAuditQueueTest : public UnitTest
{
    Q_OBJECT

private slots:
    // Construction and bounds.
    void _aFreshQueueIsEmpty();
    void _capacityIsReported();
    void _anInvalidCapacityIsClampedToOne();

    // The verdict.
    void _enqueueIsAcceptedWhileThereIsRoom();
    void _enqueueIsRefusedWhenFull();
    void _refusalDoesNotDisturbWhatIsHeld();
    void _repeatedOverflowKeepsRefusing();

    // FIFO.
    void _dequeueReturnsRecordsInInsertionOrder();
    void _dequeueFromEmptyReturnsNothing();
    void _dequeueFreesExactlyOneSlot();
    void _interleavedEnqueueAndDequeueStayFifo();
    void _theDequeuedRecordIsTheOneEnqueued();
};

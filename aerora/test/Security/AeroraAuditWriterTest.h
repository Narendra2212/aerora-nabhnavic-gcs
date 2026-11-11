#pragma once

#include "UnitTest.h"

/// \brief Asserts `AeroraAuditWriter`: that it persists queued records through the durable sink on
/// its worker thread in the order they were handed over, flushes the queue on `stop()`, refuses a
/// record without blocking once the bounded queue is full, buffers records enqueued before the
/// worker starts, and projects the durable sink's posture.
///
/// \section why What carries the weight
///
/// The writer is the non-blocking front installed on the live command gate, so the properties worth
/// proving are the ones a producer and a reviewer rely on:
///
/// \li **A queued record reaches the durable store.** `record()` only enqueues; the worker opens
///     the thread-affine sink on itself and writes. If the connection were opened on the wrong
///     thread the write would fail, so a persisted row is also evidence the affinity is right.
/// \li **Order is preserved.** Records persist in the order they were enqueued.
/// \li **`stop()` flushes.** Every record accepted before `stop()` is written before the connection
///     closes, which is what makes a clean shutdown lossless.
/// \li **A full queue drops without blocking.** The bounded queue refuses the overflow and reports
///     `DroppedQueueFull`, never blocking the caller - verified with the worker stopped so the
///     verdict is deterministic.
/// \li **The posture is projected honestly.** `isDurable()` is false until the worker opens the
///     sink and false again after `stop()`; `isTamperEvident()` stays false.
///
/// Persisted rows are read through an independent connection, so the assertions see what the worker
/// committed. Synchronisation is `QTRY_*_WITH_TIMEOUT` on the writer's counters, never a fixed wait.
class AeroraAuditWriterTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _recordedEventsArePersistedInOrder();
    void _stopFlushesPendingRecords();
    void _recordsBufferedBeforeStartAreDrained();
    void _queueFullIsDroppedWithoutBlocking();
    void _durabilityAndNameAreProjected();
};

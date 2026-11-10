#pragma once

#include <optional>

#include <QtCore/QList>

#include "AeroraAuditSink.h"

/// \brief The bounded, ordered buffer an asynchronous audit writer drains. IF-LOG-014's queue,
///        without its thread.
///
/// \section what What this is
///
/// `AeroraAuditSink.h` records the designed layering as producer -> `AeroraAuditSink::record` ->
/// `AeroraAuditWriter` (IF-LOG-014) -> `AeroraAuditStore`. The writer's job is to decouple the
/// synchronous producer call, which runs on the command-gate thread and must return promptly, from
/// the slow store write. This type is the buffer at the centre of that: a producer enqueues and
/// the drain happens elsewhere.
///
/// It decides the one thing the loss accounting hinges on - whether a record was taken or refused.
/// `enqueue()` returns `Accepted` while there is room and `DroppedQueueFull` once there is not,
/// which is exactly the verdict `AeroraCommandAuthorizer` counts into
/// `lostAuditRecordCount()` (`AERORA-GCS-REQ-025`, `IF-LOG-015`). A bounded queue is the mechanism
/// `IF-LOG-014` requires in preference to blocking: a full disk degrades the record rather than
/// freezing the operator interface.
///
/// \section nothread Why there is no thread here, and why that is deliberate
///
/// IF-LOG-014 is "the bounded writer queue **and its thread**." This is the queue. The thread is
/// left out on purpose, not forgotten:
///
/// \li The overlay has no other threaded component. Introducing its first one is a concurrency-model
///     decision that should match how QGC already runs work - the vehicle thread, a `QThread`
///     worker, a thread pool - rather than being improvised here as a bare `std::thread`.
/// \li The drain target does not exist. `AeroraAuditStore` is `P7-DAT-002`, unbuilt, so a worker
///     would have nothing to hand records to.
/// \li The hard part of IF-LOG-014 is the bounded-buffer logic - capacity, order, the
///     taken-or-refused verdict - and that is pure and deterministic. The thread is thin mechanism
///     over it. Building the deterministic half first is the same split the audit hash chain used:
///     `AeroraAuditChain` is the keyless half, and the anchor that needs a key store waits.
///
/// So this type is **not** an `AeroraAuditSink`. A sink that enqueued and never drained would fill
/// once and then drop every record, which is useless in production and would be dishonest to
/// present as the writer. The writer owns one of these; this is the buffer, not the writer.
///
/// \section order Order is preserved, and that matters even though records carry their own
///
/// Dequeue is strict FIFO. `AeroraAuditSink.h` requires a sink to "preserve the order in which
/// records were handed to it," and although each record carries `sequence` and `monotonicUs` so a
/// reader can reconstruct order, a queue that reordered would hand the store rows whose insertion
/// order disagreed with their sequence - which is a defect to debug rather than a property to rely
/// on.
///
/// \section notthreadsafe Not thread-safe on its own
///
/// This holds no lock. The writer that owns it provides the mutex across the producer and drain
/// threads, so the queue stays a pure data structure with no opinion about concurrency - which is
/// what lets the thread be chosen later without rewriting the logic. Used from one thread, as in
/// its tests, it needs no lock at all.
class AeroraAuditQueue
{
public:
    /// \a capacity is the most records the queue holds before it refuses. Clamped to at least one:
    /// a zero or negative capacity would refuse every record, which is a misconfiguration that
    /// should degrade to a tiny queue rather than to a silent total loss. Defensive rather than an
    /// assert, per the coding style.
    explicit AeroraAuditQueue(qsizetype capacity);

    /// Appends \a event if there is room.
    ///
    /// \return `Accepted` when the record was taken, `DroppedQueueFull` when the queue was at
    ///         capacity. Never blocks, never grows past capacity, never throws. The two failure
    ///         outcomes a sink can report - `FailedStorage`, `SinkUnavailable` - are the store's to
    ///         return later; a queue only ever refuses for fullness.
    [[nodiscard]] AeroraAuditWriteOutcome enqueue(const AeroraAuditEvent& event);

    /// Removes and returns the oldest record, or `std::nullopt` when empty. The drain step.
    [[nodiscard]] std::optional<AeroraAuditEvent> dequeue();

    [[nodiscard]] qsizetype size() const { return _records.size(); }

    [[nodiscard]] qsizetype capacity() const { return _capacity; }

    [[nodiscard]] bool isEmpty() const { return _records.isEmpty(); }

    [[nodiscard]] bool isFull() const { return _records.size() >= _capacity; }

private:
    qsizetype _capacity;
    QList<AeroraAuditEvent> _records;
};

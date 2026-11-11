#pragma once

#include <atomic>
#include <memory>

#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>

#include "AeroraAuditQueue.h"
#include "AeroraAuditSink.h"

class AeroraLocalStore;
class AeroraSqliteAuditSink;
class QThread;

/// \brief The asynchronous audit front: `IF-LOG-014`'s bounded writer queue **and its thread**,
/// the non-blocking `AeroraAuditSink` installed on the live command gate. `P7-DAT-002`, design
/// 09-local-persistence-architecture.md §6.
///
/// \section what What this completes
///
/// `AeroraAuditQueue` built the deterministic half of `IF-LOG-014` - the bounded buffer and its
/// taken-or-refused verdict - and deliberately left out the thread, because the overlay had no
/// threaded component and no drain target. Both now exist: `AeroraSqliteAuditSink` is the durable,
/// `synchronous=FULL` writer, and this is the worker that drains the queue into it off the caller's
/// thread. So this is the writer the sink's "notlive" section names: the thin front whose
/// `record()` never blocks, driving the synchronous durable sink from its own thread.
///
/// \section nonblocking Why the front and the durable write are split across a thread
///
/// `AeroraAuditSink.h` and `IF-LOG-014` require the sink on the command gate to **never block** the
/// caller, because `record()` runs on the vehicle thread. A `synchronous=FULL` `fsync` per record
/// cannot run there. So `record()` here only enqueues under a short lock and wakes the worker - it
/// does no I/O and returns the queue's verdict (`Accepted`, or `DroppedQueueFull` when the bounded
/// queue is full, which is exactly the loss `AeroraCommandAuthorizer` counts). The worker thread
/// dequeues and performs the durable write. `Accepted` therefore means *the record was queued*, not
/// *persisted* - the distinction `AeroraAuditSink.h` draws, and why `isDurable()` is a property of
/// the mechanism rather than a per-record promise: a record still in the queue when the host loses
/// power is lost, and a clean `stop()` is what flushes the queue before the connection closes.
///
/// \section affinity The durable sink lives entirely on the worker thread
///
/// A `QSqlDatabase` connection is thread-affine: it must be opened, used and closed on one thread.
/// So this owns the `AeroraSqliteAuditSink` but does not open it; the worker opens it as its first
/// act after the thread starts, uses it for every drained record, and closes it as its last, all on
/// the one thread. A write the worker cannot persist - a sink that would not open, a storage
/// failure - is counted in `storageFailureCount()` rather than lost silently, which is the
/// drain-side half of the loss accounting the authorizer keeps for the enqueue side.
///
/// \section lifetime Start, stop, and teardown
///
/// `start()` launches the worker; records enqueued before it are buffered and drained once it runs.
/// `stop()` signals the worker, wakes it, drains what remains, waits for it to finish and joins it -
/// so every queued record is written before the connection closes. Both are idempotent, and the
/// destructor calls `stop()`, so a writer torn down without an explicit stop still flushes. The
/// drain loop and teardown follow `QGCFileWriter`, the overlay-adjacent precedent for a
/// queue-draining worker.
///
/// \section notthreadsafe Thread-safety boundary
///
/// `record()` is called from the producer thread; `start()`/`stop()` from the owning thread; the
/// drain runs on the worker. This class provides the mutex and condition across them, which is what
/// lets `AeroraAuditQueue` stay a lock-free data structure. Its own getters read atomics.
class AeroraAuditWriter : public AeroraAuditSink
{
public:
    /// \a store supplies the durable sink its database path and active session id, and must outlive
    /// this writer. \a queueCapacity bounds the in-memory buffer; it is passed in rather than
    /// defaulted here so no queue-depth constant enters the overlay's contract surface.
    AeroraAuditWriter(AeroraLocalStore& store, qsizetype queueCapacity);
    ~AeroraAuditWriter() override;

    AeroraAuditWriter(const AeroraAuditWriter&) = delete;
    AeroraAuditWriter& operator=(const AeroraAuditWriter&) = delete;
    AeroraAuditWriter(AeroraAuditWriter&&) = delete;
    AeroraAuditWriter& operator=(AeroraAuditWriter&&) = delete;

    /// Launches the worker thread, which opens the durable sink on itself. Idempotent; a second
    /// call while running does nothing.
    void start();

    /// Signals the worker to stop, drains the remaining queue to the sink, closes the connection and
    /// joins the thread. Idempotent and safe to call when not running.
    void stop();

    [[nodiscard]] bool isRunning() const;

    /// Enqueues \a event for the worker to persist and returns without blocking. `Accepted` when the
    /// record was queued, `DroppedQueueFull` when the bounded queue was full. Never performs I/O,
    /// never blocks on the write.
    [[nodiscard]] AeroraAuditWriteOutcome record(const AeroraAuditEvent& event) override;

    /// True while the worker is running with the durable sink open. The mechanism is durable; a
    /// record not yet drained from the queue is not - see the non-blocking section.
    [[nodiscard]] bool isDurable() const override;

    /// False: the durable sink's hash chain is integrity checking, never tamper-evidence, and
    /// fronting it asynchronously does not change that.
    [[nodiscard]] bool isTamperEvident() const override { return false; }

    [[nodiscard]] QString sinkName() const override;

    /// Records the worker has written to the durable sink. Updated on the worker thread, read
    /// anywhere; the observable that a drain completed.
    [[nodiscard]] quint64 persistedCount() const { return _persisted.load(std::memory_order_relaxed); }

    /// Records the worker dequeued but could not persist - a sink that would not open, or a storage
    /// failure. The drain-side loss count, distinct from the enqueue-side drops the queue reports.
    [[nodiscard]] quint64 storageFailureCount() const { return _storageFailures.load(std::memory_order_relaxed); }

private:
    void _workerLoop();
    void _drainAll();

    mutable QMutex _mutex;
    QWaitCondition _condition;
    AeroraAuditQueue _queue;
    std::unique_ptr<AeroraSqliteAuditSink> _sink;

    QThread* _thread = nullptr;
    std::atomic<bool> _quit{false};
    std::atomic<bool> _sinkOpen{false};
    std::atomic<quint64> _persisted{0};
    std::atomic<quint64> _storageFailures{0};
};

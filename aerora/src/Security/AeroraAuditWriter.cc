#include "AeroraAuditWriter.h"

#include <optional>

#include <QtCore/QThread>

#include "AeroraSqliteAuditSink.h"
#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(AeroraAuditWriterLog, "aerora.security.audit.writer")

AeroraAuditWriter::AeroraAuditWriter(AeroraLocalStore& store, qsizetype queueCapacity)
    : _queue(queueCapacity)
    , _sink(std::make_unique<AeroraSqliteAuditSink>(store))
{}

AeroraAuditWriter::~AeroraAuditWriter()
{
    stop();
}

void AeroraAuditWriter::start()
{
    const QMutexLocker locker(&_mutex);
    if (_thread) {
        return;
    }

    _quit.store(false, std::memory_order_relaxed);
    _thread = QThread::create([this]() { _workerLoop(); });
    _thread->setObjectName(QStringLiteral("AeroraAuditWriter"));
    _thread->start();
}

void AeroraAuditWriter::stop()
{
    {
        const QMutexLocker locker(&_mutex);
        if (!_thread) {
            return;
        }
        _quit.store(true, std::memory_order_relaxed);
    }

    // Wake the worker so it leaves its wait, drains the remaining queue and returns; then join and
    // delete it. wait() guarantees the worker has finished before the thread object is destroyed,
    // which is the documented teardown for a QThread::create worker.
    _condition.wakeOne();
    _thread->wait();
    delete _thread;

    const QMutexLocker locker(&_mutex);
    _thread = nullptr;
}

bool AeroraAuditWriter::isRunning() const
{
    const QMutexLocker locker(&_mutex);
    return _thread != nullptr;
}

AeroraAuditWriteOutcome AeroraAuditWriter::record(const AeroraAuditEvent& event)
{
    AeroraAuditWriteOutcome outcome = AeroraAuditWriteOutcome::DroppedQueueFull;
    {
        const QMutexLocker locker(&_mutex);
        outcome = _queue.enqueue(event);
    }

    // Wake the worker only when the record was taken; a refused record added nothing to drain.
    if (outcome == AeroraAuditWriteOutcome::Accepted) {
        _condition.wakeOne();
    }
    return outcome;
}

bool AeroraAuditWriter::isDurable() const
{
    return _sinkOpen.load(std::memory_order_relaxed);
}

QString AeroraAuditWriter::sinkName() const
{
    return QStringLiteral("AsyncSqliteAudit");
}

void AeroraAuditWriter::_workerLoop()
{
    // Open the thread-affine durable sink here, on the worker thread, so its connection is created,
    // used and closed on one thread. A failure is not fatal to the loop: every drained record is
    // then counted as a storage failure rather than silently lost, and isDurable() stays false.
    const bool opened = _sink->open();
    _sinkOpen.store(opened, std::memory_order_relaxed);
    qCDebug(AeroraAuditWriterLog) << "audit writer worker started; durable sink open:" << opened;

    while (!_quit.load(std::memory_order_relaxed)) {
        {
            QMutexLocker locker(&_mutex);
            while (_queue.isEmpty() && !_quit.load(std::memory_order_relaxed)) {
                _condition.wait(&_mutex);
            }
        }
        _drainAll();
    }

    // A final drain catches records enqueued during the last write cycle, so stop() flushes
    // everything that was accepted before the connection closes.
    _drainAll();

    if (opened) {
        _sink->close();
    }
    _sinkOpen.store(false, std::memory_order_relaxed);
    qCDebug(AeroraAuditWriterLog) << "audit writer worker stopped; persisted:"
                                  << _persisted.load(std::memory_order_relaxed)
                                  << "storage failures:" << _storageFailures.load(std::memory_order_relaxed);
}

void AeroraAuditWriter::_drainAll()
{
    for (;;) {
        AeroraAuditEvent event;
        {
            QMutexLocker locker(&_mutex);
            std::optional<AeroraAuditEvent> dequeued = _queue.dequeue();
            if (!dequeued.has_value()) {
                return;
            }
            event = std::move(*dequeued);
        }

        // The durable write runs without the lock held, so record() on the producer thread never
        // waits on I/O. A closed sink returns SinkUnavailable, which counts as a storage failure.
        const AeroraAuditWriteOutcome outcome = _sink->record(event);
        if (outcome == AeroraAuditWriteOutcome::Accepted) {
            _persisted.fetch_add(1, std::memory_order_relaxed);
        } else {
            _storageFailures.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

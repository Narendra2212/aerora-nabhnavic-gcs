#include "AeroraAuditQueue.h"

AeroraAuditQueue::AeroraAuditQueue(qsizetype capacity)
    : _capacity(capacity < 1 ? 1 : capacity)
{}

AeroraAuditWriteOutcome AeroraAuditQueue::enqueue(const AeroraAuditEvent& event)
{
    // Refuse for fullness only. A queue never reports FailedStorage or SinkUnavailable: those are
    // the store's outcomes, reported later on the drain, and conflating them here would tell a
    // producer the record was lost to a storage fault when it was lost to a full buffer - different
    // causes with different remedies.
    if (isFull()) {
        return AeroraAuditWriteOutcome::DroppedQueueFull;
    }

    _records.append(event);
    return AeroraAuditWriteOutcome::Accepted;
}

std::optional<AeroraAuditEvent> AeroraAuditQueue::dequeue()
{
    if (_records.isEmpty()) {
        return std::nullopt;
    }

    return _records.takeFirst();
}

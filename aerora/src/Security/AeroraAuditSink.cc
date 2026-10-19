#include "AeroraAuditSink.h"

#include <QtCore/QAtomicInteger>
#include <QtCore/QElapsedTimer>

QString AeroraAuditEvent::typeToString(AeroraAuditEventType type)
{
    switch (type) {
    case AeroraAuditEventType::OperatorIdentityResolved:
        return QStringLiteral("OperatorIdentityResolved");
    case AeroraAuditEventType::AuthorizationDecision:
        return QStringLiteral("AuthorizationDecision");
    case AeroraAuditEventType::CommandRequested:
        return QStringLiteral("CommandRequested");
    case AeroraAuditEventType::CommandDenied:
        return QStringLiteral("CommandDenied");
    case AeroraAuditEventType::CommandTransmitted:
        return QStringLiteral("CommandTransmitted");
    case AeroraAuditEventType::VehicleResponse:
        return QStringLiteral("VehicleResponse");
    case AeroraAuditEventType::SecurityEvent:
        return QStringLiteral("SecurityEvent");
    case AeroraAuditEventType::SystemEvent:
        return QStringLiteral("SystemEvent");
    }
    return QStringLiteral("Unknown");
}

quint64 AeroraAuditEvent::nextSequence()
{
    // Atomic rather than a plain counter: the gate is reached from the vehicle's thread today, but
    // the interface does not promise a single producer thread, and a torn sequence would silently
    // produce duplicate order keys - which is worse than the tie `sequence` exists to break.
    //
    // Returns counter + 1, so the first record is 1 and zero stays available as "unstamped".
    static QAtomicInteger<quint64> counter(0);
    return counter.fetchAndAddOrdered(1) + 1;
}

quint64 AeroraAuditEvent::nowMonotonicUs()
{
    // QElapsedTimer is Qt's monotonic source - QueryPerformanceCounter on Windows,
    // CLOCK_MONOTONIC where available - so it does not step on an NTP correction, a GNSS time fix
    // or an operator clock change. That immunity is the entire reason ordering does not use `utc`.
    //
    // Started once, on first use, rather than at static-initialisation time: the domain only has
    // to be consistent within the process, and a function-local static has a defined
    // initialisation point whereas a file-scope one would not relative to other translation units.
    static const QElapsedTimer processTimer = [] {
        QElapsedTimer timer;
        timer.start();
        return timer;
    }();

    // nsecsElapsed() rather than elapsed(): elapsed() is milliseconds, which is the resolution
    // `utc` already has and too coarse to order records composed within one authorize() call.
    //
    // Plus one, for the same reason nextSequence() above adds one: zero is the documented
    // "unstamped" sentinel. The timer starts on first use and the division truncates, so the first
    // call in a process - which starts and reads the timer within the same microsecond - would
    // otherwise stamp a real record with the sentinel and make it indistinguishable from an
    // unstamped one. A constant offset cancels in every difference and leaves the sequence
    // non-decreasing, so ordering is unaffected.
    return static_cast<quint64>(processTimer.nsecsElapsed() / 1000) + 1;
}

void AeroraAuditEvent::stampNow()
{
    utc = QDateTime::currentDateTimeUtc();
    monotonicUs = nowMonotonicUs();
    sequence = nextSequence();
}

QString AeroraAuditSink::outcomeToString(AeroraAuditWriteOutcome outcome)
{
    switch (outcome) {
    case AeroraAuditWriteOutcome::Accepted:
        return QStringLiteral("Accepted");
    case AeroraAuditWriteOutcome::DroppedQueueFull:
        return QStringLiteral("Dropped: Queue Full");
    case AeroraAuditWriteOutcome::FailedStorage:
        return QStringLiteral("Failed: Storage");
    case AeroraAuditWriteOutcome::SinkUnavailable:
        return QStringLiteral("Failed: Sink Unavailable");
    }
    return QStringLiteral("Failed: Unknown");
}

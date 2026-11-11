#pragma once

#include "UnitTest.h"

/// \brief Asserts `AeroraStoreRetention`: that the age prunes delete exactly the rows older than a
/// cutoff and no others, that an event prune cascades its `EventValues` and completes across more
/// than one batch, that an empty-session prune keeps any session a record references and the active
/// one, and that every prune refuses before the connection is open.
///
/// \section why What carries the weight
///
/// Retention deletes records that are not reconstructible, so the properties worth proving are the
/// ones that bound what it may remove:
///
/// \li **An age prune is exact.** `pruneEventsOlderThan` and `pruneWriteFailuresOlderThan` remove
///     the rows strictly older than the cutoff and leave the rest, returning the count removed.
/// \li **An event prune takes its values with it.** `EventValues` go by `ON DELETE CASCADE`, so a
///     pruned event leaves no orphaned value rows.
/// \li **It completes across batches.** A backlog larger than one batch is fully removed and
///     counted, so the bounded-batch loop terminates rather than stopping after the first batch.
/// \li **Audit and the active session are never swept.** A session an `AuditRecords` or `Events`
///     row references is kept, and so is the store's current session even when it is empty.
/// \li **A prune before open is a no-op that reports failure.** Each prune returns -1 rather than
///     touching a database it has no connection to.
///
/// Verified through independent SQLite connections to the same file, so each assertion reads what
/// was persisted rather than what the class reported in memory.
class AeroraStoreRetentionTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _pruneEventsRemovesOldAndCascadesValues();
    void _pruneEventsSpansMultipleBatches();
    void _pruneWriteFailuresRemovesOldRows();
    void _pruneEmptySessionsKeepsReferencedAndActive();
    void _pruneBeforeOpenReturnsMinusOne();
    void _openFailsWhenStoreIsNotOpen();
};

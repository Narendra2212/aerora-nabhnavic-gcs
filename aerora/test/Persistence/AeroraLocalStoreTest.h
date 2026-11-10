#pragma once

#include "UnitTest.h"

/// \brief Asserts `AeroraLocalStore`: that it opens and versions its schema, refuses a version it
/// does not recognise without destroying the file, honours disabled persistence, and accounts for
/// write failures rather than losing them.
///
/// \section why What carries the weight
///
/// The store is the foundation every other persistence writer builds on (design §13), so the
/// properties worth proving are the ones a later writer trusts without re-checking:
///
/// \li **The schema is created and versioned.** A fresh file gets all six tables, the five named
///     indices and `user_version = kSchemaVersion`, so a reopen is a version match rather than a
///     second creation.
/// \li **An unrecognised version is refused, not reset.** A newer version and a legacy unversioned
///     file both leave the store closed with the file preserved - the divergence from the tile
///     cache that design §5 makes, because these records are not reconstructible.
/// \li **Disabled persistence opens nothing.** `P7-DAT-005`: no file is created, so the UI reports
///     persistence as off rather than showing an empty timeline.
/// \li **Failures are counted.** A failure a session can own lands in `WriteFailures`; one it
///     cannot - no store, or no session yet - is counted in memory, so neither is silently lost
///     (`P7-AUDIT-003`, design §11).
///
/// Verified through a second SQLite connection to the same file, so the assertions read what was
/// persisted rather than what the store reported in memory.
class AeroraLocalStoreTest : public UnitTest
{
    Q_OBJECT

private slots:
    // Open outcomes.
    void _disabledPersistenceOpensNothing();
    void _openingAFreshFileCreatesSchemaAndSetsVersion();
    void _schemaCarriesEveryTableAndIndex();
    void _reopeningAnExistingStoreProceeds();
    void _newerSchemaVersionIsRefusedAndPreserved();
    void _legacyUnversionedDatabaseIsRefusedAndPreserved();
    void _databasePathIsReported();

    // Session lifecycle.
    void _beginSessionPersistsARow();
    void _endSessionStampsTheEndTime();

    // Failure accounting.
    void _recordedWriteFailureIsPersistedAndCounted();
    void _writeFailureWithoutASessionIsUnrecorded();
    void _writeFailureWithoutAStoreIsUnrecorded();
};

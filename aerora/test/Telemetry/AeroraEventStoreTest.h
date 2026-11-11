#pragma once

#include "UnitTest.h"

/// \brief Asserts `AeroraEventStore`: that it persists `Events` and their `EventValues` in one
/// transaction, stores the category as its integer, keeps the nullable columns null, allows an
/// event with no values, and counts a write it cannot complete.
class AeroraEventStoreTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _recordEventPersistsEventAndValues();
    void _nullableFieldsArePersistedAsNull();
    void _categoryIsStoredAsItsInteger();
    void _anEventMayCarryNoValues();
    void _recordWithoutAnActiveSessionFails();
    void _recordBeforeOpenFails();
    void _openFailsWhenStoreIsNotOpen();
};

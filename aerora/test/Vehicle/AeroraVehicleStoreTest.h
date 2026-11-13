#pragma once

#include "UnitTest.h"

/// \brief Asserts `AeroraVehicleStore`: that it mints a canonical-form identity for a board seen
/// for the first time, recalls the same identity on every later lookup rather than re-minting,
/// keeps distinct boards apart, stamps `lastSeenUtcMs` on every call while touching
/// `firstSeenUtcMs` only at mint, and refuses an empty board identifier rather than minting
/// against one.
///
/// Also asserts `AeroraCorePlugin::boardUidFromAutopilotVersion()`, the pure extraction rule
/// `DG-FLT-SOC-02` names: `uid2` preferred when it carries evidence, `uid` as the documented
/// fallback, empty when neither does. Tested here because it serves this class's one caller and
/// takes the two already-decoded fields rather than a MAVLink type, so no packed message or
/// MAVLink struct appears in this suite - the same reasoning that puts
/// `AeroraCorePlugin::classifyOperationBoundary()`'s tests in the Security suite its decision
/// serves, applied to the Vehicle domain this one serves.
///
/// \section why What carries the weight
///
/// This is the authority `DG-FLT`'s identity-format decision names, so the properties worth
/// proving are the ones the decision itself states:
///
/// \li **A fresh board gets a canonical identity.** Lowercase, hyphenated, no braces.
/// \li **The same board never gets a second identity.** Recall, not re-mint, and never on a
///     different board's record.
/// \li **Only `lastSeenUtcMs` moves on a recall.** `firstSeenUtcMs` is written once.
/// \li **No evidence is not acted on.** An empty board identifier mints nothing.
/// \li **The extraction prefers `uid2`, falls back to `uid`, and admits absence.** The one rule
///     `DG-FLT-SOC-02` records as what was reviewed.
///
/// Verified through an independent SQLite connection to the same file, so the assertions read
/// what was persisted rather than what the class reported in memory.
class AeroraVehicleStoreTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _mintsACanonicalIdentityForAnUnknownBoard();
    void _recallsTheSameIdentityOnASecondLookup();
    void _distinctBoardsGetDistinctIdentities();
    void _lastSeenIsTouchedButFirstSeenIsNot();
    void _emptyBoardUidIsRefusedWithoutMinting();
    void _resolveBeforeOpenReturnsEmpty();
    void _openFailsWhenStoreIsNotOpen();

    void _boardUidPrefersUid2WhenItCarriesEvidence();
    void _boardUidFallsBackToUidWhenUid2IsAllZero();
    void _boardUidIsEmptyWhenNeitherCarriesEvidence();
    void _boardUidHexEncodingIsLowercase();
};

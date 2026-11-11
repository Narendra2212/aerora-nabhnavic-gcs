#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QString>
#include <QtCore/QTemporaryDir>

#include "AeroraLocalStore.h"
#include "AeroraSqliteAuditSink.h"
#include "UnitTest.h"

// AeroraLocalStore, AeroraSqliteAuditSink and QTemporaryDir are included in full rather than
// forward-declared: the QScopedPointer members below need a complete type for their deleter, which
// moc instantiates in the generated metatype where only this header is visible.

/// \brief Asserts `AeroraSqliteAuditSink`: that it persists `AuditRecords` rows through
/// `AeroraLocalStore`, chains them with `AeroraAuditChain`, classifies the outcome as the owner
/// approved, and is honest about durability and tamper-evidence.
///
/// \section why What carries the weight
///
/// \li **The translation is faithful.** Each column is the one `AeroraAuditSink.h` maps: an empty
///     operator becomes the literal `unresolved` in the writer, the action is `typeToString`, the
///     outcome is the approved classification, and the detail round-trips.
/// \li **The chain is correct and continuous.** The first record chains onto genesis, each record
///     chains onto the previous, and the chain continues across a close/open rather than restarting
///     at genesis - which is what makes a reopened store one trail and not two.
/// \li **Integrity checking works, and only to the extent claimed.** The stored chain, verified
///     against the original events, detects an edited hash and a removed record. It is not
///     tamper-evidence, and the sink never reports it as such.
/// \li **Failure and readiness are reported, not hidden.** A record with no active session is a
///     storage failure; a record before `open()` is sink-unavailable; durability is true only while
///     open.
///
/// Persistence is checked through an independent connection, so the assertions read what reached the
/// database rather than what the sink reported in memory.
class AeroraSqliteAuditSinkTest : public UnitTest
{
    Q_OBJECT

private slots:
    void cleanup();

    // Faithful translation onto the columns.
    void _recordIsAcceptedAndPersisted();
    void _emptyOperatorBecomesUnresolved();
    void _classifyOutcomeMatchesTheApprovedTable();
    void _deniedAuthorizationDecisionIsPersistedAsDenied();
    void _detailRoundTripsThroughJson();

    // The hash chain.
    void _firstRecordChainsOntoGenesisAndRecordsLink();
    void _chainContinuesAcrossReopen();
    void _storedChainVerifiesWithOriginalEvents();
    void _editingAStoredHashBreaksVerification();
    void _deletingARecordBreaksVerification();

    // Failure, readiness and honesty.
    void _recordWithoutAnActiveSessionFailsStorage();
    void _recordBeforeOpenIsSinkUnavailable();
    void _isDurableOnlyWhenOpenAndNeverTamperEvident();
    void _openFailsWhenStoreIsNotOpen();

private:
    void _openStore();
    void _beginSession();
    void _openSink();
    void _openCommonFixture();

    QScopedPointer<QTemporaryDir> _dir;
    QScopedPointer<AeroraLocalStore> _store;
    QScopedPointer<AeroraSqliteAuditSink> _sink;
    QString _dbPath;
    qint64 _sessionId = -1;
};

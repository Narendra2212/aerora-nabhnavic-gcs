#pragma once

#include "UnitTest.h"

/// \brief Asserts the audit hash chain: that it detects every mutation it is specified to detect,
/// and that it claims nothing it has not earned.
///
/// \section why What this suite is for
///
/// The accepted `DG-STO:tamper-evidence` record requires that "editing any field or removing any
/// row breaks the chain at that point and the break is localised to the exact record". Those are
/// two separate claims and a digest satisfies only the first by accident - a construction that
/// detected a change but could not say where would pass a naive test and fail the requirement. So
/// the mutation cases below each assert the index as well as the verdict.
///
/// \section fields Why every field gets its own case
///
/// A canonical encoding that silently omits a field produces a digest that cannot see edits to it,
/// and nothing about that failure is visible: the chain still verifies, still looks like integrity
/// checking, and no longer is. The only way to know a field is covered is to change it and require
/// a break. The data-driven case does exactly that, once per field the record carries, so a field
/// added to `AeroraAuditEvent` and forgotten in `canonicalForm()` fails here.
///
/// \section notevidence What this suite deliberately does not assert
///
/// That any honesty flag moved. The chain is the keyless half of the construction; the accepted
/// record settles a protected anchor as the other half, and no platform has a key-protector
/// backend. `AeroraAuditSink.h` is explicit that implementing the chain "does **not** entitle a
/// sink to return true from `isTamperEvident()`", and nothing here does.
class AeroraAuditChainTest : public UnitTest
{
    Q_OBJECT

private slots:
    // The construction itself.
    void _theGenesisDigestIsStableAndNotEmpty();
    void _anEmptyChainVerifiesAndItsHeadIsGenesis();
    void _appendingAdvancesTheHeadAndChainsOntoThePrevious();
    void _identicalRecordsAtDifferentPositionsGetDifferentDigests();
    void _theCanonicalFormIsStableAcrossCalls();

    // Detection, with the break localised.
    void _anIntactChainVerifies();
    void _anEditedFieldBreaksTheChainAtThatRecord_data();
    void _anEditedFieldBreaksTheChainAtThatRecord();
    void _aRemovedRecordBreaksTheChainWhereItWasRemoved();
    void _areorderedPairBreaksTheChain();
    void _aForgedFirstRecordDoesNotChainOntoGenesis();
    void _anEditedRecordWithARecomputedDigestStillBreaksTheLink();

    // The distinctions the encoding has to keep.
    void _anUnstampedTimeIsNotTheEpoch();
    void _anUnresolvedVehicleIsNotSystemIdZero();
    void _aNestedDetailChangeIsCovered();
};

#pragma once

#include "UnitTest.h"

/// \brief Asserts the `AeroraAuditSink` contract (WP2-A) and the loss accounting that stands in for
/// the durability mechanism `Q-037` has not chosen.
///
/// The contract is specified in `AeroraAuditSink.h` and in
/// `.kiro/specs/03-implementation-specification/34-audit-sink-and-security-status-contract.md`.
/// Every test here maps to one acceptance criterion in that document.
///
/// \section what What is assertable without a store
///
/// No sink implementation exists, and none may be written until `Q-037` and `P7-DAT-002` resolve.
/// That bounds this suite but does not empty it, because the properties at risk are properties of
/// the *interface* and of the *producer*, not of a store:
///
/// \li A sink that fails is observable at all. Before WP2-A, `record()` returned `void`, so a
///     storage failure was indistinguishable from a successful write at every call site.
/// \li A failed audit write does not change an authorization outcome (`P7-AUDIT-003`).
/// \li Losses are counted by cause, and the counters never go backwards or reset.
/// \li Records carry an orderable key, because wall-clock time alone cannot order them.
/// \li No sink reports itself durable or tamper-evident.
///
/// \section not What this suite deliberately does not assert
///
/// Durability, crash survival, tamper-evidence, retention and export. There is no mechanism behind
/// any of them, and a test that passed against a stub would convert an open decision into an
/// apparently-met requirement - the `R7-11` failure mode. The stub sinks here are test doubles that
/// report outcomes on command; none of them stores anything.
class AeroraAuditSinkTest : public UnitTest
{
    Q_OBJECT

protected slots:
    void cleanup() override;

private slots:
    // The record: the ordering keys, the enforcement posture, and the loss evidence the record
    // carries itself.
    void _sequenceIsMonotonicAndNeverZero();
    void _stampNowSetsAllThreeTimeFields();
    void _recordsOfOneCommandAreOrderableBySequence();
    void _recordCapturesTheEnforcementPostureAtCompositionTime();
    void _recordCarriesTheProducersLossCountAtCompositionTime();

    // Outcome reporting. The gap WP2-A closes: before it, `record()` returned void.
    void _outcomeToStringDistinguishesEveryOutcome();
    void _onlyAcceptedIsNotALoss();

    // Producer behaviour when a record cannot be written.
    void _noSinkCountsADropAndStillDecides();
    void _failingSinkCountsAFailedWriteAndDoesNotChangeTheDecision();
    void _eachFailureOutcomeIsCountedAsALoss_data();
    void _eachFailureOutcomeIsCountedAsALoss();
    void _acceptedWritesAreNotCounted();
    void _lossCountersAreMonotonicAcrossSinkReplacement();
    void _dropAndFailureAreCountedSeparately();

    // Honesty defaults. Neither may be inherited as true.
    void _defaultSinkReportsNeitherDurableNorTamperEvident();
};

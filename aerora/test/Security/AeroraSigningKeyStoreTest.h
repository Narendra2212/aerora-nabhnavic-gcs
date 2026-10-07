#pragma once

#include "UnitTest.h"

/// Covers the Aerora signing-key backend (`ADR-013`): the half of `D-002` that is implemented.
///
/// The abstraction and this store exist; **no platform key protector exists on any platform**, so
/// the production posture is that `AeroraSigningKeyStore` is never installed and keys stay in the
/// stock unprotected backend. These tests therefore assert two distinct things, and the second
/// matters more than the first:
///
///   1. With a protector present, the store wraps before writing and unwraps on read.
///   2. With no usable protector, the store **refuses** rather than quietly writing plaintext.
///
/// The protector used here is a reversible test double, not cryptography. It exists to prove the
/// store calls the seam and never persists the bytes it was handed; it says nothing about the
/// strength of any future real backend.
class AeroraSigningKeyStoreTest : public UnitTest
{
    Q_OBJECT

protected slots:
    void cleanup() override;

private slots:
    void _testUnusableProtectorRefuses_data();
    void _testUnusableProtectorRefuses();
    void _testUnusableProtectorStillAllowsRemoval();
    void _testRoundTripThroughProtector();
    void _testStoredBlobIsNotThePlaintextKey();
    void _testProtectFailureWritesNothing();
    void _testUnprotectFailureReportsCorruptData();
    void _testEmptyNameIsRejected();
    void _testAeroraPathIsSeparateFromStockPath();
    void _testNoPlatformProtectorIsShipped();
};

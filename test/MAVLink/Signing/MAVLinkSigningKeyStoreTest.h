#pragma once

#include "UnitTest.h"

/// Covers the signing-key storage backend seam introduced by ADR-013: the default
/// QSettings backend's behaviour, and that MAVLinkSigningKeys actually routes key
/// material through an injected backend rather than writing it directly.
class MAVLinkSigningKeyStoreTest : public UnitTest
{
    Q_OBJECT

protected slots:
    void cleanup() override;

private slots:
    void _testDefaultBackendRoundTrip();
    void _testDefaultBackendReportsNotFound();
    void _testDefaultBackendRejectsEmptyName();
    void _testDefaultBackendRemoveKey();
    void _testDefaultBackendDoesNotClaimProtection();
    void _testResultToStringIsNeverUnknown();
    void _testKeyStoreDefaultsToQSettings();
    void _testInjectedBackendReceivesKeyMaterial();
    void _testCorruptStoredMaterialCannotBecomeAKey_data();
    void _testCorruptStoredMaterialCannotBecomeAKey();
    void _testFailingBackendIsNotListedInManifest();
};

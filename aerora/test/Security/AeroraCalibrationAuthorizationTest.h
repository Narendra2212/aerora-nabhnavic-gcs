#pragma once

#include "UnitTest.h"

/// Covers the calibration authorization policy (ADR-017). The enforcement site itself is ADR-015,
/// implemented in `Vehicle::startCalibration()`.
///
/// Two groups, split by what they can actually assert today.
///
/// **Classification and chain behaviour** — asserted here. These exercise the structural classifier
/// and the Aerora authorization chain directly, with stub collaborators standing in for the identity
/// provider, role resolver and policy. They need no vehicle and no link, because fail-closed is a
/// property of the chain.
///
/// **End-to-end enforcement on the direct send path** — *not* asserted here. The gate now exists, so
/// "a denied calibration puts no bytes on the wire" has become writable, but it needs a
/// `VehicleTest` fixture and an overlay build lane with `QGC_BUILD_TESTING=ON` (`Q-035`). The
/// assertion is specified in ADR-017 rather than written as a test that would pass for the wrong
/// reason.
///
/// One limit worth knowing before extending this suite: the landed gate reports a refusal only as an
/// app message, not as a `mavCommandResult` failure code, so a wire-level test can assert the
/// absence of the packet and the presence of the message but cannot assert a failure code. See the
/// divergence recorded in ADR-017.
class AeroraCalibrationAuthorizationTest : public UnitTest
{
    Q_OBJECT

protected slots:
    void cleanup() override;

private slots:
    // Structural classification. No identity, roles or policy involved.
    void _testClassification_data();
    void _testClassification();
    void _testNonCalibrationCommandIsNotClassified();
    void _testCancelIsFlaggedAsBlockingCancel();
    void _testParamIndexDoesNotIdentifyMotorRelevance();
    void _testCalibrationIsInTheDeclaredGatedSet();
    void _testDescribeDistinguishesStartFromCancel();

    // Authorization chain, exercised with a calibration operation.
    void _testAuthorizedCalibrationIsAllowed();
    void _testDeniedCalibrationIsDeniedWithReason();
    void _testUnconfiguredPolicyDeniesCalibration();
    void _testAuditIdentifiesWhichCalibrationWasRefused();
    void _testAuditDistinguishesCancelFromStart();
};

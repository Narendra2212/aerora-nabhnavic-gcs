#pragma once

#include "UnitTest.h"

/// \brief Asserts the addressing axis and the capability axis of a gated operation, and that
/// neither reaches the other.
///
/// \section atomic What authorization being atomic means here
///
/// Requirement 3 criterion 3 applies a Fleet_Operation to every addressed aircraft or to none, and
/// criterion 7 says in as many words that transmission cannot be atomic because a link can fail
/// after the decision. So the decision and the transmission are two values, and the cases below
/// assert they stay two: a partial delivery must never be representable as a partial
/// authorization.
///
/// \section exclusion Why exclusion is not refusal
///
/// An aircraft reporting no satisfying capability is removed from the addressed set before
/// authorization runs, per Requirement 4 criterion 3. It was not refused - it was never addressed -
/// and an operator told otherwise would go looking for a policy that denied it.
///
/// \section unresolvable Why unresolvable is not empty
///
/// While `DG-PAY:capability` holds no accepted Closure_Record there is no vocabulary to match
/// against, and Requirement 4 criterion 7 requires a Capability_Address to report unresolvable
/// rather than to address every aircraft. The difference is not cosmetic: resolving an unsettled
/// vocabulary to everything would send a hover command to a fixed wing. These cases assert that
/// an unresolvable address excludes nobody *and* resolves nothing, because excluding the whole
/// fleet would be indistinguishable from resolving it.
class AeroraFleetAuthorizationTest : public UnitTest
{
    Q_OBJECT

private slots:
    // The addressing axis.
    void _anAddressCarryingOneVehicleYieldsThatVehicle();
    void _anAddressCarryingNoVehicleYieldsAnUnresolvedContext();
    void _anAddressCarryingSeveralVehiclesYieldsNoSoleVehicle();

    // The subject axis, and that it does not read the address.
    void _theSubjectAndTheAddressVaryIndependently();

    // Transmission is a separate value from the decision.
    void _anEmptyTransmissionIsNotReportedAsDeliveredToEvery();
    void _aTransmissionIsCompleteOnlyWhenEveryMemberWasDelivered();
    void _aNotAttemptedMemberIsDistinctFromAFailedOne();

    // The capability axis.
    void _anAddressNamingNoCapabilityIsNotResolvedByCapability();
    void _aCapabilityAddressIsUnresolvableWhileTheVocabularyIsUnaccepted();
    void _anUnresolvableCapabilityAddressExcludesNobody();

    // The guard the Gate_Register pins.
    void _theCapabilityVocabularyIsRecordedUnaccepted();
};

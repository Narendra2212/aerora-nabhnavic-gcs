#pragma once

#include "UnitTest.h"

/// \brief Asserts `AeroraCredentialDocument`: that a credential signed by the build's issuer is
/// accepted with its operator, window and role table intact, and that everything else is refused.
///
/// \section why What carries the weight
///
/// \li **An edited payload breaks the signature.** The escalation case specifically — adding
///     `parameter` to the flying role — because that is why the role table rides *inside* the
///     signed payload rather than beside it.
/// \li **No verifier refuses a valid document.** The state a build with no configured issuer
///     reaches, and the reason the shipped station stays fail-closed.
/// \li **Another issuer is refused**, which is forgery rather than corruption.
/// \li **An expired credential still parses.** Deliberate: presence and freshness are separate
///     answers, so an operator stays identified after their window closes and a command already in
///     flight is not cut off. The gate consults `isFreshAt()`; this class does not.
/// \li **The signature covers the file bytes.** Asserted with a payload whose keys are reordered
///     and whitespaced differently — it verifies, because nothing re-serialises it.
/// \li **Field checks are enforced on correctly signed payloads**, so a refusal there is the
///     field rule and not the signature.
///
/// Fixtures are signed with the linked library from a fixed seed, so the suite exercises the real
/// verification path end to end and a failure is reproducible. No key material is shared with
/// anything that ships.
class AeroraCredentialDocumentTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _aSignedDocumentIsAccepted();
    void _noVerifierRefusesEvenAValidDocument();
    void _anotherIssuerIsRefused();
    void _anEditedPayloadBreaksTheSignature();
    void _anExpiredCredentialStillParses();
    void _malformedDocumentsAreRefused();
    void _incompletePayloadsAreRefused();
    void _theSignatureCoversTheFileBytesNotAReserialisation();
};

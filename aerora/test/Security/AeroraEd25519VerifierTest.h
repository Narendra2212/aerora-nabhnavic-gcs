#pragma once

#include "UnitTest.h"

/// \brief Asserts `AeroraEd25519Verifier` against the RFC 8032 §7.1 test vectors.
///
/// \section why What carries the weight
///
/// This suite exists for one reason above the others: **it proves which of Monocypher's two
/// signature APIs got linked.** `crypto_eddsa_check` is BLAKE2b-based and interoperable with no
/// standard signer; `crypto_ed25519_check` is RFC 8032. The two have identical signatures, so
/// linking the wrong one compiles cleanly, passes any self-consistency test that signs and then
/// verifies with the same library, and fails only when a deployment tries to issue a credential
/// with `openssl` or `ssh-keygen`. A published vector cannot be satisfied by the wrong
/// construction, so verifying one is positive evidence rather than an absence of errors.
///
/// The rest is the behaviour the credential mechanism rests on:
///
/// \li **A good signature verifies**, including over an empty message, which is the RFC's own
///     first vector and must not be refused as malformed.
/// \li **Tampering is refused** — a changed message, a flipped signature bit, and a well-formed
///     signature from a different issuer, which is forgery rather than corruption.
/// \li **Malformed input is refused rather than reported** — a credential that cannot be verified
///     and one that fails verification are the same outcome to every caller.
/// \li **An unusable key refuses everything**, and `fromHex()` returns null for input it cannot
///     decode, so a build with no configured issuer installs nothing and the station stays
///     fail-closed by construction rather than by a caller remembering to check.
/// \li **The name carries no key material**, because it reaches a status surface and the audit
///     trail.
///
/// No key is generated here and nothing is signed: the suite holds only public keys and published
/// signatures, so it needs no secret and proves verification rather than a round trip.
class AeroraEd25519VerifierTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _rfc8032Test1Verifies();
    void _rfc8032Test2Verifies();
    void _theStandardVariantIsTheOneLinked();

    void _aTamperedMessageIsRefused();
    void _aTamperedSignatureIsRefused();
    void _anotherIssuersKeyIsRefused();
    void _aMalformedSignatureLengthIsRefused();
    void _anUnusableKeyRefusesEverything();

    void _fromHexAcceptsAWellFormedKey();
    void _fromHexRejectsAnythingElse();
    void _theVerifierNamesItsPrimitiveAndNotItsKey();
};

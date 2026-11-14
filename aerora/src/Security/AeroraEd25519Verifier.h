#pragma once

#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include "AeroraCredentialVerifier.h"

/// \brief An `AeroraCredentialVerifier` over RFC 8032 Ed25519, holding only a public key.
///
/// \section standard Why RFC 8032 specifically
///
/// So a deployment can issue credentials with tools it already has. The signature is verified with
/// Monocypher's `crypto_ed25519_check`, which its own header describes as "EdDSA with curve25519 +
/// SHA-512" - the standard construction, interoperable with `openssl pkeyutl -sign`, `ssh-keygen -Y
/// sign` and Python `cryptography`. The library's *other* signature API, `crypto_eddsa_check`, is
/// BLAKE2b-based and interoperable with nothing, so choosing between them is a correctness decision
/// rather than a preference; `AeroraEd25519VerifierTest` pins it with the RFC 8032 §7.1 vectors.
///
/// \section key The key is a build input, not a file beside the credential
///
/// The public key reaches this class from the caller, and the caller takes it from a value fixed at
/// build time. That placement is the point. A key sitting next to `credential.json` in the save
/// directory would be writable by anyone who can write the credential, so an attacker would simply
/// replace both and the signature would establish nothing. A key fixed in the binary cannot be
/// swapped without replacing the application, which is a different and larger act.
///
/// The cost is honest and accepted: rotating the issuer key needs a rebuild. That is the trade a
/// station with no protected key store can actually make, and it is recorded rather than hidden.
///
/// \section unusable An unusable verifier refuses everything
///
/// Constructed with a key of the wrong length - including the empty key a build with no configured
/// issuer produces - every `verify()` returns false. It does not throw and does not report itself
/// usable, so a caller that installs one anyway rejects every credential instead of accepting
/// whatever arrives. `fromHex()` returns null for input it cannot decode, which is the path a
/// caller should prefer because it distinguishes "no issuer configured" from "issuer configured
/// wrongly" before anything is installed.
class AeroraEd25519Verifier : public AeroraCredentialVerifier
{
public:
    /// \a publicKey is the raw 32-byte Ed25519 public key. A key of any other length, empty
    /// included, yields a verifier that refuses everything - see \ref unusable.
    explicit AeroraEd25519Verifier(QByteArray publicKey);

    /// Decodes \a hexPublicKey - 64 hex characters, case-insensitive - and returns a verifier, or
    /// null when it is empty or not a well-formed 32-byte key. Null is the "do not install
    /// anything" answer and is how a build with no configured issuer reaches a fail-closed station.
    [[nodiscard]] static std::unique_ptr<AeroraEd25519Verifier> fromHex(const QString& hexPublicKey);

    /// Whether this verifier holds a well-formed key and can reach a true verdict at all.
    [[nodiscard]] bool isUsable() const;

    [[nodiscard]] bool verify(const QByteArray& signedPayload, const QByteArray& signature) const override;

    [[nodiscard]] QString verifierName() const override;

private:
    QByteArray _publicKey;
};

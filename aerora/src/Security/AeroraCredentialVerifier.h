#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

/// \brief Verifies that a provisioned credential was issued by the party the build trusts.
///
/// \section why Why this is an interface
///
/// The credential mechanism and the signature primitive are separate decisions, and only the first
/// is settled. The owner-approved shape is an administrator-provisioned file carrying an asymmetric
/// signature the station can check but never produce, and `AeroraEd25519Verifier` is that. The seam
/// exists so the *source* of a credential can later become the cloud reconciliation `IF-SEC-009`
/// describes without the policy, the role resolver or the command gate changing at all - they
/// consume a verified credential and never learn where it came from.
///
/// \section closed No verifier means no credential
///
/// A consumer that holds no verifier must reject every credential rather than accept an unverified
/// one. That keeps the shipped posture fail-closed: a build configured with no issuer key installs
/// no verifier, accepts no credential, installs no identity, and therefore enforces nothing - which
/// is exactly the state the station reports today, reached deliberately rather than by accident.
///
/// \section notsecret A verifier holds no secret
///
/// An implementation verifies and never signs, so it needs only public material. That is the whole
/// reason this shape was chosen: the absent protected key store (`D-DAT-05`, `F3.1-01` - no backend
/// on any of the five platforms) blocks keeping a secret at rest, and blocks nothing here. An actor
/// who replaces the public material can cause a credential to be *rejected*; they cannot forge one
/// the station accepts.
class AeroraCredentialVerifier
{
public:
    virtual ~AeroraCredentialVerifier() = default;

    /// Whether \a signature is a valid signature by the trusted issuer over \a signedPayload, which
    /// is the exact byte sequence the issuer signed rather than a re-serialisation of it.
    ///
    /// Must be constant-time with respect to the signature where the primitive allows, must not
    /// throw, and must return false for any malformed input rather than reporting an error
    /// separately - a credential that cannot be verified and one that fails verification are the
    /// same outcome to every caller, which is refusal.
    [[nodiscard]] virtual bool verify(const QByteArray& signedPayload, const QByteArray& signature) const = 0;

    /// The primitive's name, for the status surface and the audit trail. Never the key.
    [[nodiscard]] virtual QString verifierName() const = 0;
};

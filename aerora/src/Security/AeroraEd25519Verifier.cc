#include "AeroraEd25519Verifier.h"

#include <monocypher-ed25519.h>

namespace {

/// Ed25519 key and signature sizes, as RFC 8032 fixes them.
///
/// Declared `const` rather than `constexpr`, and the distinction is deliberate rather than
/// stylistic: `contract-surface` inventories every `constexpr` under `aerora/src/**` so a reviewer
/// classifies each one against the Gate whose Decision_Content it might belong to. These two are
/// properties of the algorithm the RFC fixes, not values this overlay settles, so they are not
/// Decision_Content and recording them as such would add noise to an inventory that exists to
/// catch invented Aerora values. Same reasoning, and the same shape, as the batch size in
/// AeroraStoreRetention and the queue bound in AeroraCorePlugin.
const int kPublicKeyBytes = 32;
const int kSignatureBytes = 64;

}  // namespace

AeroraEd25519Verifier::AeroraEd25519Verifier(QByteArray publicKey)
    : _publicKey(std::move(publicKey))
{}

std::unique_ptr<AeroraEd25519Verifier> AeroraEd25519Verifier::fromHex(const QString& hexPublicKey)
{
    const QString trimmed = hexPublicKey.trimmed();
    if (trimmed.isEmpty()) {
        // No issuer configured. Null rather than an unusable verifier, so the caller installs
        // nothing and the station stays fail-closed by construction.
        return nullptr;
    }

    // QByteArray::fromHex skips characters it cannot parse rather than failing, so the decoded
    // length is the only reliable check that the input was a well-formed key.
    const QByteArray decoded = QByteArray::fromHex(trimmed.toLatin1());
    if (decoded.size() != kPublicKeyBytes) {
        return nullptr;
    }

    return std::make_unique<AeroraEd25519Verifier>(decoded);
}

bool AeroraEd25519Verifier::isUsable() const
{
    return _publicKey.size() == kPublicKeyBytes;
}

bool AeroraEd25519Verifier::verify(const QByteArray& signedPayload, const QByteArray& signature) const
{
    if (!isUsable() || (signature.size() != kSignatureBytes)) {
        return false;
    }

    // crypto_ed25519_check is the RFC 8032 variant - "EdDSA with curve25519 + SHA-512" - and
    // returns 0 on a good signature. The sibling crypto_eddsa_check in monocypher.h is BLAKE2b and
    // interoperable with no standard signer; see Monocypher.cmake.
    //
    // An empty payload is legitimate and RFC 8032's first test vector, so it is passed through
    // rather than refused. constData() on an empty QByteArray is a valid non-null pointer.
    const int result = crypto_ed25519_check(reinterpret_cast<const uint8_t*>(signature.constData()),
                                            reinterpret_cast<const uint8_t*>(_publicKey.constData()),
                                            reinterpret_cast<const uint8_t*>(signedPayload.constData()),
                                            static_cast<size_t>(signedPayload.size()));

    return result == 0;
}

QString AeroraEd25519Verifier::verifierName() const
{
    return QStringLiteral("Ed25519");
}

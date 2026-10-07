#pragma once

#include <memory>

#include <QtCore/QLatin1StringView>

#include "AeroraPlatformKeyProtector.h"
#include "MAVLinkSigningKeyStore.h"

/// \brief Aerora signing-key backend: persists only platform-protected key material.
///
/// Implements the core MAVLinkSigningKeyStore seam introduced by ADR-013. Key bytes are
/// wrapped by an AeroraPlatformKeyProtector before they are written, and are stored under
/// an Aerora-specific settings path so protected blobs are never confused with the
/// stock plaintext entries.
///
/// If no protector is available this store refuses to write and reports
/// ProtectionUnavailable. It does not fall back to plaintext. That is the difference
/// between a security boundary and a security-shaped comment.
///
class AeroraSigningKeyStore : public MAVLinkSigningKeyStore
{
public:
    explicit AeroraSigningKeyStore(std::unique_ptr<AeroraPlatformKeyProtector> protector);

    MAVLinkSigningKeyStoreResult storeKey(const QString& name, const QByteArray& keyBytes) override;
    MAVLinkSigningKeyStoreResult loadKey(const QString& name, QByteArray& outKeyBytes) override;
    MAVLinkSigningKeyStoreResult removeKey(const QString& name) override;
    MAVLinkSigningKeyStoreResult flush() override;

    [[nodiscard]] bool protectsAtRest() const override;
    [[nodiscard]] QString backendName() const override;

    /// True when a protector is present and currently usable. When false every store
    /// operation fails closed.
    [[nodiscard]] bool isUsable() const;

private:
    /// Aerora-specific subgroup. Separate from the stock "keys" path so a protected blob is
    /// never read back as raw key bytes, or the reverse.
    static constexpr QLatin1StringView kProtectedKeySubgroup = QLatin1StringView("aeroraProtectedKeys");

    std::unique_ptr<AeroraPlatformKeyProtector> _protector;
};

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QLatin1StringView>
#include <QtCore/QString>

/// \brief Outcome of a signing-key store operation.
///
/// A backend that cannot protect key material must report it rather than silently
/// persisting unprotected material. The decision to accept unprotected storage
/// belongs to the policy installing the backend, not to the backend itself.
///
enum class MAVLinkSigningKeyStoreResult {
    Success,                ///< Operation completed
    NotFound,               ///< No material stored under that name
    Unavailable,            ///< Backend cannot operate in this configuration at all
    ProtectionUnavailable,  ///< Backend operates but cannot protect material at rest
    PermissionDenied,       ///< Platform refused access to the store
    CorruptData,            ///< Stored material was present but unusable
    Failure,                ///< Any other failure
};

/// \brief Persistence backend for MAVLink signing key material.
///
/// Only secret key bytes pass through this interface. The key-name manifest and the
/// per-key signing timestamps are deliberately not part of it: neither contains
/// secrets, and keeping them in their existing location keeps the core change minimal.
///
/// Implementations must secureZero any internal copies of key material they make.
///
class MAVLinkSigningKeyStore
{
public:
    /// Settings group shared with MAVLinkSigningKeys, which keeps the non-secret key-name manifest
    /// and the per-key signing timestamps alongside. Declared here so both have one source of truth.
    static constexpr QLatin1StringView kSettingsGroup = QLatin1StringView("MAVLinkSigningKeys");

    virtual ~MAVLinkSigningKeyStore() = default;

    /// Persist \a keyBytes under \a name, replacing any existing entry.
    virtual MAVLinkSigningKeyStoreResult storeKey(const QString& name, const QByteArray& keyBytes) = 0;

    /// Read key material for \a name into \a outKeyBytes.
    virtual MAVLinkSigningKeyStoreResult loadKey(const QString& name, QByteArray& outKeyBytes) = 0;

    /// Remove stored material for \a name. Removing an absent entry is Success.
    virtual MAVLinkSigningKeyStoreResult removeKey(const QString& name) = 0;

    /// Commit any pending writes. Called once after a batch of storeKey/removeKey calls so
    /// backends are not forced to flush per key. Backends with no deferred state may ignore it.
    virtual MAVLinkSigningKeyStoreResult flush() { return MAVLinkSigningKeyStoreResult::Success; }

    /// Whether this backend protects material at rest. Reported explicitly so a platform
    /// without protection can never be mistaken for one with it.
    [[nodiscard]] virtual bool protectsAtRest() const = 0;

    /// Short identifier used in logging and diagnostics.
    [[nodiscard]] virtual QString backendName() const = 0;

    static QString resultToString(MAVLinkSigningKeyStoreResult result);
};

/// \brief Default backend: stores raw key bytes in QSettings.
///
/// Reproduces the behaviour that existed before the backend abstraction was introduced,
/// byte-for-byte and at the same settings paths, so stock QGroundControl is neither
/// regressed nor silently changed. It does not protect material at rest and says so.
///
class MAVLinkSigningKeyStoreQSettings : public MAVLinkSigningKeyStore
{
public:
    MAVLinkSigningKeyStoreResult storeKey(const QString& name, const QByteArray& keyBytes) override;
    MAVLinkSigningKeyStoreResult loadKey(const QString& name, QByteArray& outKeyBytes) override;
    MAVLinkSigningKeyStoreResult removeKey(const QString& name) override;

    MAVLinkSigningKeyStoreResult flush() override;

    [[nodiscard]] bool protectsAtRest() const override { return false; }
    [[nodiscard]] QString backendName() const override { return QStringLiteral("QSettings"); }

private:
    /// <kSettingsGroup>/keys/<name> = raw key bytes. Owned here because only this backend reads it.
    static constexpr QLatin1StringView kKeySubgroup = QLatin1StringView("keys");
};

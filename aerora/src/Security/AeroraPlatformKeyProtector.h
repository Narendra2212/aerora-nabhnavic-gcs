#pragma once

#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QString>

/// \file AeroraPlatformKeyProtector.h
/// Platform abstraction for protecting signing-key material at rest.
///
/// NO PLATFORM BACKEND IS IMPLEMENTED. D-002 approved the abstraction; the build host is
/// NO-GO under D-006, so a cryptographic backend could be neither compiled nor tested, and
/// an untested key-protection backend is worse than none. \c createForCurrentPlatform()
/// therefore returns nullptr on every platform today.
///
/// No platform API is named, assumed or invented here. Each backend, when implemented,
/// must be accompanied by a record of:
///
///   - storage mechanism
///   - encryption / protection applied
///   - availability conditions
///   - failure mode
///   - key export implications
///   - backup implications
///   - deletion behaviour
///   - test strategy
///
/// Required platform coverage, from the QGC target set (Windows, Linux, macOS, Android, iOS):
/// a backend is required for each, and any platform that cannot provide equivalent
/// protection must have that posture recorded explicitly rather than silently downgraded.

/// \brief Wraps and unwraps key material using a platform protection mechanism.
///
/// Implementations must not fall back to returning unprotected material. If protection is
/// unavailable, \c isAvailable() must report false and the wrap calls must fail; the decision
/// to accept unprotected storage belongs to the installing policy, never to the backend.
///
class AeroraPlatformKeyProtector
{
public:
    virtual ~AeroraPlatformKeyProtector() = default;

    /// Wrap \a plaintext for storage. Returns false on failure; must not emit plaintext on failure.
    [[nodiscard]] virtual bool protect(const QByteArray& plaintext, QByteArray& outProtected) = 0;

    /// Unwrap \a protectedBlob. Returns false on failure.
    [[nodiscard]] virtual bool unprotect(const QByteArray& protectedBlob, QByteArray& outPlaintext) = 0;

    /// Whether this protector can operate right now in this configuration.
    [[nodiscard]] virtual bool isAvailable() const = 0;

    /// Whether material wrapped by this protector is genuinely protected at rest.
    [[nodiscard]] virtual bool protectsAtRest() const = 0;

    [[nodiscard]] virtual QString platformName() const = 0;

    /// Returns nullptr on every platform today. See the file comment: this is a deliberate,
    /// recorded gap tied to D-006, not an oversight to be patched over with a plaintext stub.
    static std::unique_ptr<AeroraPlatformKeyProtector> createForCurrentPlatform();
};

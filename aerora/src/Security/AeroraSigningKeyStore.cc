#include "AeroraSigningKeyStore.h"

#include <QtCore/QSettings>

#include "QGCLoggingCategory.h"
#include "SecureMemory.h"

QGC_LOGGING_CATEGORY(AeroraSigningKeyStoreLog, "Aerora.SigningKeyStore")

AeroraSigningKeyStore::AeroraSigningKeyStore(std::unique_ptr<AeroraPlatformKeyProtector> protector)
    : _protector(std::move(protector))
{
    if (!isUsable()) {
        qCWarning(AeroraSigningKeyStoreLog)
            << "no usable platform key protector; this store will refuse all writes."
            << "protector:" << (_protector ? _protector->platformName() : QStringLiteral("none"));
    }
}

bool AeroraSigningKeyStore::isUsable() const
{
    return _protector && _protector->isAvailable();
}

bool AeroraSigningKeyStore::protectsAtRest() const
{
    return isUsable() && _protector->protectsAtRest();
}

QString AeroraSigningKeyStore::backendName() const
{
    if (!_protector) {
        return QStringLiteral("Aerora(no-protector)");
    }
    return QStringLiteral("Aerora(%1)").arg(_protector->platformName());
}

MAVLinkSigningKeyStoreResult AeroraSigningKeyStore::storeKey(const QString& name, const QByteArray& keyBytes)
{
    if (name.isEmpty()) {
        return MAVLinkSigningKeyStoreResult::Failure;
    }
    if (!isUsable()) {
        // Deliberate refusal. Writing plaintext here would defeat the entire purpose of D-002.
        return MAVLinkSigningKeyStoreResult::ProtectionUnavailable;
    }

    QByteArray wrapped;
    const bool ok = _protector->protect(keyBytes, wrapped);
    if (!ok || wrapped.isEmpty()) {
        QGC::secureZero(wrapped);
        qCWarning(AeroraSigningKeyStoreLog) << "protect() failed for key:" << name;
        return MAVLinkSigningKeyStoreResult::Failure;
    }

    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(QString("%1/%2").arg(kProtectedKeySubgroup, name), wrapped);
    settings.endGroup();

    QGC::secureZero(wrapped);
    return MAVLinkSigningKeyStoreResult::Success;
}

MAVLinkSigningKeyStoreResult AeroraSigningKeyStore::loadKey(const QString& name, QByteArray& outKeyBytes)
{
    if (name.isEmpty()) {
        return MAVLinkSigningKeyStoreResult::Failure;
    }
    if (!isUsable()) {
        return MAVLinkSigningKeyStoreResult::ProtectionUnavailable;
    }

    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    const QString path = QString("%1/%2").arg(kProtectedKeySubgroup, name);
    const bool present = settings.contains(path);
    QByteArray wrapped = present ? settings.value(path).toByteArray() : QByteArray();
    settings.endGroup();

    if (!present) {
        return MAVLinkSigningKeyStoreResult::NotFound;
    }

    const bool ok = _protector->unprotect(wrapped, outKeyBytes);
    QGC::secureZero(wrapped);

    if (!ok) {
        QGC::secureZero(outKeyBytes);
        outKeyBytes.clear();
        qCWarning(AeroraSigningKeyStoreLog) << "unprotect() failed for key:" << name;
        return MAVLinkSigningKeyStoreResult::CorruptData;
    }

    return MAVLinkSigningKeyStoreResult::Success;
}

MAVLinkSigningKeyStoreResult AeroraSigningKeyStore::removeKey(const QString& name)
{
    if (name.isEmpty()) {
        return MAVLinkSigningKeyStoreResult::Failure;
    }

    // Removal is permitted even without a protector: deleting material must never be blocked
    // by the inability to read it.
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.remove(QString("%1/%2").arg(kProtectedKeySubgroup, name));
    settings.endGroup();

    return MAVLinkSigningKeyStoreResult::Success;
}

MAVLinkSigningKeyStoreResult AeroraSigningKeyStore::flush()
{
    QSettings settings;
    settings.sync();
    return MAVLinkSigningKeyStoreResult::Success;
}

std::unique_ptr<AeroraPlatformKeyProtector> AeroraPlatformKeyProtector::createForCurrentPlatform()
{
    // Intentionally nullptr on every platform. See AeroraPlatformKeyProtector.h: the backends are
    // approved in principle by D-002 but cannot be written responsibly while D-006 leaves the host
    // unable to compile or test them. Returning a plaintext stub here would be a silent downgrade.
    return nullptr;
}

#include "MAVLinkSigningKeyStore.h"

#include <QtCore/QSettings>

#include "SecureMemory.h"

QString MAVLinkSigningKeyStore::resultToString(MAVLinkSigningKeyStoreResult result)
{
    switch (result) {
    case MAVLinkSigningKeyStoreResult::Success:
        return QStringLiteral("Success");
    case MAVLinkSigningKeyStoreResult::NotFound:
        return QStringLiteral("Not Found");
    case MAVLinkSigningKeyStoreResult::Unavailable:
        return QStringLiteral("Backend Unavailable");
    case MAVLinkSigningKeyStoreResult::ProtectionUnavailable:
        return QStringLiteral("Protection Unavailable");
    case MAVLinkSigningKeyStoreResult::PermissionDenied:
        return QStringLiteral("Permission Denied");
    case MAVLinkSigningKeyStoreResult::CorruptData:
        return QStringLiteral("Corrupt Data");
    case MAVLinkSigningKeyStoreResult::Failure:
        return QStringLiteral("Failure");
    }
    return QStringLiteral("Unknown");
}

MAVLinkSigningKeyStoreResult MAVLinkSigningKeyStoreQSettings::storeKey(const QString& name, const QByteArray& keyBytes)
{
    if (name.isEmpty()) {
        return MAVLinkSigningKeyStoreResult::Failure;
    }

    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    // Local copy is zeroed after the write; the caller still owns its own buffer.
    QByteArray serialized = keyBytes;
    settings.setValue(QString("%1/%2").arg(kKeySubgroup, name), serialized);
    QGC::secureZero(serialized);
    settings.endGroup();

    return MAVLinkSigningKeyStoreResult::Success;
}

MAVLinkSigningKeyStoreResult MAVLinkSigningKeyStoreQSettings::loadKey(const QString& name, QByteArray& outKeyBytes)
{
    if (name.isEmpty()) {
        return MAVLinkSigningKeyStoreResult::Failure;
    }

    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    const QString path = QString("%1/%2").arg(kKeySubgroup, name);
    const bool present = settings.contains(path);
    if (present) {
        outKeyBytes = settings.value(path).toByteArray();
    }
    settings.endGroup();

    return present ? MAVLinkSigningKeyStoreResult::Success : MAVLinkSigningKeyStoreResult::NotFound;
}

MAVLinkSigningKeyStoreResult MAVLinkSigningKeyStoreQSettings::removeKey(const QString& name)
{
    if (name.isEmpty()) {
        return MAVLinkSigningKeyStoreResult::Failure;
    }

    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.remove(QString("%1/%2").arg(kKeySubgroup, name));
    settings.endGroup();

    return MAVLinkSigningKeyStoreResult::Success;
}

MAVLinkSigningKeyStoreResult MAVLinkSigningKeyStoreQSettings::flush()
{
    QSettings settings;
    // Explicit sync: persist before a crash between here and QSettings destruction.
    settings.sync();
    return MAVLinkSigningKeyStoreResult::Success;
}

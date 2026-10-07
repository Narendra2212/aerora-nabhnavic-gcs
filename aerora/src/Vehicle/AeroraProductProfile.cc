#include "AeroraProductProfile.h"

#include <QtCore/QGlobalStatic>

#include "AeroraVehicleLog.h"

Q_GLOBAL_STATIC(AeroraProductProfileRegistry, _aeroraProductProfileRegistryInstance)

AeroraProductProfileRegistry *AeroraProductProfileRegistry::instance()
{
    return _aeroraProductProfileRegistryInstance();
}

bool AeroraProductProfileRegistry::registerProfile(AeroraProductProfile *profile)
{
    if (!profile) {
        qCWarning(AeroraVehicleLog) << "Product profile registration rejected, null profile";
        return false;
    }

    const QString profileId = profile->profileId();
    if (profileId.isEmpty()) {
        qCWarning(AeroraVehicleLog) << "Product profile registration rejected, empty profile id";
        return false;
    }

    if (profileIds().contains(profileId)) {
        qCWarning(AeroraVehicleLog) << "Product profile registration rejected, duplicate profile id"
                                    << "profileId:" << profileId;
        return false;
    }

    _profiles.append(profile);

    qCDebug(AeroraVehicleLog) << "registered product profile"
                              << "profileId:" << profileId;

    return true;
}

QStringList AeroraProductProfileRegistry::profileIds() const
{
    QStringList ids;
    ids.reserve(_profiles.count());
    for (const AeroraProductProfile *profile : _profiles) {
        ids.append(profile->profileId());
    }

    return ids;
}

AeroraProductProfile *AeroraProductProfileRegistry::resolve(const AeroraVehicleCapabilities &capabilities) const
{
    AeroraProductProfile *match = nullptr;

    for (AeroraProductProfile *profile : _profiles) {
        if (!profile->matchesCapabilities(capabilities)) {
            continue;
        }

        if (match) {
            qCWarning(AeroraVehicleLog) << "Product profile resolution is ambiguous, no profile selected"
                                        << "first:" << match->profileId()
                                        << "second:" << profile->profileId();
            return nullptr;
        }

        match = profile;
    }

    return match;
}

#ifdef QGC_UNITTEST_BUILD
void AeroraProductProfileRegistry::clear()
{
    _profiles.clear();
}
#endif

#include "AeroraMissionExtension.h"

#include <QtCore/QApplicationStatic>

#include "AeroraMissionLog.h"

Q_APPLICATION_STATIC(AeroraMissionExtensionRegistry, _aeroraMissionExtensionRegistryInstance);

AeroraMissionExtensionRegistry *AeroraMissionExtensionRegistry::instance()
{
    return _aeroraMissionExtensionRegistryInstance();
}

bool AeroraMissionExtensionRegistry::registerExtension(AeroraMissionExtension *extension)
{
    if (!extension) {
        qCWarning(AeroraMissionLog) << "rejecting null mission extension";
        return false;
    }

    const QString extensionId = extension->extensionId();
    if (extensionId.isEmpty()) {
        qCWarning(AeroraMissionLog) << "rejecting mission extension with an empty id";
        return false;
    }

    const QStringList claimedNames = extension->complexMissionItemNames();

    for (const QString &claimedName : claimedNames) {
        if (claimedName.isEmpty()) {
            qCWarning(AeroraMissionLog) << "rejecting mission extension with an empty complex item name. id:"
                                        << extensionId;
            return false;
        }
    }

    for (const AeroraMissionExtension *const registered : _extensions) {
        if (registered->extensionId() == extensionId) {
            qCWarning(AeroraMissionLog) << "rejecting duplicate mission extension id:" << extensionId;
            return false;
        }

        const QStringList registeredNames = registered->complexMissionItemNames();
        for (const QString &claimedName : claimedNames) {
            if (registeredNames.contains(claimedName)) {
                // A build configuration defect. Resolving it by registration order would make the
                // outcome depend on link order and hide the defect.
                qCWarning(AeroraMissionLog)
                    << "rejecting mission extension claiming an already-claimed complex item name. id:" << extensionId
                    << "name:" << claimedName << "alreadyClaimedBy:" << registered->extensionId();
                return false;
            }
        }
    }

    _extensions.append(extension);

    qCDebug(AeroraMissionLog) << "registered mission extension id:" << extensionId
                              << "complexMissionItemNames:" << claimedNames;

    return true;
}

QStringList AeroraMissionExtensionRegistry::extensionIds() const
{
    QStringList ids;
    ids.reserve(_extensions.count());

    for (const AeroraMissionExtension *const extension : _extensions) {
        ids.append(extension->extensionId());
    }

    return ids;
}

QStringList AeroraMissionExtensionRegistry::complexMissionItemNames() const
{
    QStringList names;

    for (const AeroraMissionExtension *const extension : _extensions) {
        names.append(extension->complexMissionItemNames());
    }

    return names;
}

ComplexMissionItem *AeroraMissionExtensionRegistry::createComplexMissionItem(const QString &canonicalName,
                                                                             PlanMasterController *masterController,
                                                                             bool flyView)
{
    if (canonicalName.isEmpty()) {
        return nullptr;
    }

    for (AeroraMissionExtension *const extension : _extensions) {
        if (!extension->complexMissionItemNames().contains(canonicalName)) {
            continue;
        }

        ComplexMissionItem *const item = extension->createComplexMissionItem(canonicalName, masterController, flyView);
        if (!item) {
            qCWarning(AeroraMissionLog) << "mission extension claimed a complex item name but created nothing. id:"
                                        << extension->extensionId() << "name:" << canonicalName;
        }

        return item;
    }

    qCDebug(AeroraMissionLog) << "no mission extension claims complex item name:" << canonicalName;

    return nullptr;
}

bool AeroraMissionExtensionRegistry::isMissionCommandSetDeclared() const
{
    for (const AeroraMissionExtension *const extension : _extensions) {
        if (!extension->missionCommandSet().isEmpty()) {
            return true;
        }
    }

    return false;
}

QList<MAV_CMD> AeroraMissionExtensionRegistry::declaredMissionCommandSet() const
{
    QList<MAV_CMD> commands;

    for (const AeroraMissionExtension *const extension : _extensions) {
        for (const MAV_CMD command : extension->missionCommandSet()) {
            if (!commands.contains(command)) {
                commands.append(command);
            }
        }
    }

    return commands;
}

#ifdef QGC_UNITTEST_BUILD
void AeroraMissionExtensionRegistry::clear()
{
    // Registration does not transfer ownership, so the registry must not delete the extensions.
    _extensions.clear();
}
#endif

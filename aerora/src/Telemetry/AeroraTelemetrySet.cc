#include "AeroraTelemetrySet.h"

#include "AeroraFactGroupBase.h"
#include "AeroraFactGroupRegistrar.h"
#include "AeroraFreshness.h"

AeroraTelemetrySet::AeroraTelemetrySet(RuntimeScheduler *scheduler, QObject *parent)
    : QObject(parent)
    , _freshness(new AeroraFreshness(scheduler, this))
{
}

AeroraTelemetrySet::~AeroraTelemetrySet() = default;

bool AeroraTelemetrySet::registerGroup(const QString &name, AeroraFactGroupBase *group)
{
    if (name.isEmpty()) {
        qCWarning(AeroraTelemetryLog) << "Refusing to register a fact group with an empty name";
        return false;
    }

    if (!group) {
        qCWarning(AeroraTelemetryLog) << "Refusing to register a null fact group" << name;
        return false;
    }

    if (_sealed) {
        // R-027: the telemetry CSV header is enumerated once at file open. A group appearing later
        // shifts every subsequent column, so a late registration is refused rather than accepted
        // and silently corrupting the record.
        qCWarning(AeroraTelemetryLog)
            << "Refusing to register a fact group after the set was sealed; Aerora adds no group dynamically"
            << "name:" << name;
        return false;
    }

    if (_nameToFactGroupMap.contains(name)) {
        qCWarning(AeroraTelemetryLog) << "Duplicate fact group" << name;
        return false;
    }

    _nameToFactGroupMap.insert(name, group);
    _nameToAeroraGroupMap.insert(name, group);

    qCDebug(AeroraTelemetryLog) << "Registered fact group" << name;

    return true;
}

bool AeroraTelemetrySet::publishTo(AeroraFactGroupRegistrar &registrar)
{
    if (_published) {
        // A second publication would attempt to register names the vehicle already has, and the
        // refusals would read as a registration defect rather than as a repeated call.
        qCWarning(AeroraTelemetryLog) << "Refusing to publish a telemetry set twice";
        return false;
    }

    // Seal before registering, not after. A registrar implementation is core-adjacent code that may
    // call back; sealing first means no group can slip into the set part-way through publication and
    // reach the vehicle's map without the CSV header knowing about it.
    _published = true;
    _sealed = true;

    bool allAccepted = true;
    for (auto it = _nameToFactGroupMap.cbegin(); it != _nameToFactGroupMap.cend(); ++it) {
        if (!registrar.registerFactGroup(it.key(), it.value())) {
            allAccepted = false;
            qCWarning(AeroraTelemetryLog) << "Fact group was refused by the registrar"
                                         << "name:" << it.key();
        }
    }

    qCDebug(AeroraTelemetryLog) << "Published fact groups:" << _nameToFactGroupMap.size()
                                << "allAccepted:" << allAccepted;

    return allAccepted;
}

QMap<QString, FactGroup *> *AeroraTelemetrySet::factGroups()
{
    _sealed = true;

    return &_nameToFactGroupMap;
}

QStringList AeroraTelemetrySet::groupNames() const
{
    return _nameToAeroraGroupMap.keys();
}

AeroraFactGroupBase *AeroraTelemetrySet::group(const QString &name) const
{
    return _nameToAeroraGroupMap.value(name, nullptr);
}

void AeroraTelemetrySet::clearObservations()
{
    for (AeroraFactGroupBase *const factGroup : _nameToAeroraGroupMap) {
        factGroup->clearObservations();
    }
}

bool AeroraTelemetrySet::sharedAcrossVehiclesDetected() const
{
    for (const AeroraFactGroupBase *const factGroup : _nameToAeroraGroupMap) {
        if (factGroup->sharedAcrossVehiclesDetected()) {
            return true;
        }
    }

    return false;
}

#include "AeroraFactGroupBase.h"

#include "AeroraFreshness.h"
#include "Fact.h"

AeroraFactGroupBase::AeroraFactGroupBase(const QString &metaDataFile,
                                        const QString &freshnessClass,
                                        AeroraFreshness *freshness,
                                        QObject *parent)
    // Update rate zero: immediate valueChanged, so a value's apparent receive time is never moved by
    // signal coalescing. See the class documentation.
    : FactGroup(0, metaDataFile, parent)
    , _freshnessClass(freshnessClass)
    , _freshness(freshness)
{
    if (!freshness) {
        qCWarning(AeroraTelemetryLog) << "Constructed with no freshness evaluator"
                                     << "freshnessClass:" << freshnessClass;
    }
}

AeroraFactGroupBase::~AeroraFactGroupBase() = default;

void AeroraFactGroupBase::handleMessage(Vehicle *vehicle, const mavlink_message_t &message)
{
    _noteDispatchIdentity(static_cast<const void *>(vehicle));

    _handleAeroraMessage(vehicle, message);
}

void AeroraFactGroupBase::_noteDispatchIdentity(const void *vehicleIdentity)
{
    if (!vehicleIdentity) {
        return;
    }

    // Q4-002 guard. A fact group reached by two vehicles is being shared through the cached
    // FirmwarePlugin singleton, and the two aircraft's values are overwriting each other. Nothing
    // here can separate them; the point is that the condition stops being invisible.
    if (!_boundVehicle) {
        _boundVehicle = vehicleIdentity;
        return;
    }

    if ((_boundVehicle == vehicleIdentity) || _sharedAcrossVehiclesDetected) {
        return;
    }

    _sharedAcrossVehiclesDetected = true;
    qCWarning(AeroraTelemetryLog)
        << "Fact group reached by a second vehicle; values from two aircraft are being merged."
        << "This is the Q4-002 / R4-001 FirmwarePlugin::factGroups() sharing hazard."
        << "freshnessClass:" << _freshnessClass;
}

void AeroraFactGroupBase::_handleAeroraMessage(Vehicle *vehicle, const mavlink_message_t &message)
{
    Q_UNUSED(vehicle);
    Q_UNUSED(message);
}

AeroraStateValue *AeroraFactGroupBase::_addStateFact(Fact *fact,
                                                    AeroraTelemetryState::Kind declaredKind,
                                                    const QString &itemFreshnessClass)
{
    if (!fact) {
        qCWarning(AeroraTelemetryLog) << "Ignoring a null fact registration";
        return nullptr;
    }

    const QString factName = fact->name();
    if (_nameToStateValueMap.contains(factName)) {
        qCWarning(AeroraTelemetryLog) << "Duplicate state fact" << factName;
        return _nameToStateValueMap.value(factName);
    }

    _addFact(fact);

    const QString resolvedClass = itemFreshnessClass.isEmpty() ? _freshnessClass : itemFreshnessClass;
    AeroraStateValue *const value = new AeroraStateValue(fact, declaredKind, resolvedClass, _freshness, this);

    _stateValues.append(value);
    _nameToStateValueMap.insert(factName, value);

    return value;
}

AeroraStateValue *AeroraFactGroupBase::stateValue(const QString &factName) const
{
    return _nameToStateValueMap.value(factName, nullptr);
}

QStringList AeroraFactGroupBase::stateValueNames() const
{
    return _nameToStateValueMap.keys();
}

void AeroraFactGroupBase::clearObservations()
{
    for (AeroraStateValue *const item : _stateValues) {
        item->clearObservation();
    }
}

quint64 AeroraFactGroupBase::_stampNowUs() const
{
    return _freshness ? _freshness->nowUs() : 0;
}

void AeroraFactGroupBase::_recordObservation(AeroraStateValue *value,
                                             const mavlink_message_t &message,
                                             quint64 receiveTimeUs)
{
    if (!value) {
        qCWarning(AeroraTelemetryLog) << "Ignoring an observation for a null state value";
        return;
    }

    value->recordObservation(receiveTimeUs, _sourceOf(message));
}

AeroraTelemetrySource AeroraFactGroupBase::_sourceOf(const mavlink_message_t &message)
{
    return AeroraTelemetrySource::fromSendingComponent(message.sysid, message.compid);
}

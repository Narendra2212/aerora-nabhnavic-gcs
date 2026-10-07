#include "AeroraPayloadMock.h"

#include "AeroraFreshness.h"
#include "AeroraPayloadModel.h"
#include "AeroraReportedValue.h"
#include "AeroraVehicleCapabilities.h"

AeroraPayloadMock::AeroraPayloadMock(RuntimeScheduler *scheduler, QObject *parent)
    : QObject(parent)
    , _freshness(new AeroraFreshness(scheduler, this))
    , _capabilities(new AeroraVehicleCapabilities(this))
    , _model(new AeroraPayloadModel(_freshness, _capabilities, this))
{
}

AeroraPayloadMock::~AeroraPayloadMock() = default;

void AeroraPayloadMock::setReportingSource(int systemId, int componentId)
{
    _source = AeroraTelemetrySource::fromSendingComponent(systemId, componentId);
}

void AeroraPayloadMock::clearReportingSource()
{
    _source = AeroraTelemetrySource();
}

bool AeroraPayloadMock::reportCapabilityAvailable(const QString &key)
{
    return _capabilities->report(AeroraVehicleCapabilities::Category::Payload,
                                 key,
                                 AeroraVehicleCapabilities::State::Available);
}

bool AeroraPayloadMock::reportCapabilityUnavailable(const QString &key)
{
    return _capabilities->report(AeroraVehicleCapabilities::Category::Payload,
                                 key,
                                 AeroraVehicleCapabilities::State::Unavailable);
}

void AeroraPayloadMock::clearCapabilityReports()
{
    _capabilities->clearReports();
}

void AeroraPayloadMock::reportIdentity(const QString &reportedToken)
{
    _model->identity()->recordReport(reportedToken, nowUs(), _source);
}

void AeroraPayloadMock::reportPayloadState(const QString &reportedToken)
{
    _model->payloadState()->recordReport(reportedToken, nowUs(), _source);
}

void AeroraPayloadMock::reportHealth(const QString &reportedToken)
{
    _model->health()->recordReport(reportedToken, nowUs(), _source);
}

void AeroraPayloadMock::reportSafetyState(const QString &reportedToken)
{
    _model->safetyState()->recordReport(reportedToken, nowUs(), _source);
}

void AeroraPayloadMock::reportEmergencyStop(const QString &reportedToken)
{
    // Touches the emergency-stop item and nothing else. That is the whole scenario: the stop is an
    // independent cut, so a test can assert payload state is unchanged and still whatever it was
    // when the stop fired.
    _model->emergencyStop()->recordReport(reportedToken, nowUs(), _source);
}

bool AeroraPayloadMock::markItemCapabilityAbsent(const QString &itemId)
{
    return setItemCapability(itemId, AeroraStateValue::CapabilityAbsent);
}

bool AeroraPayloadMock::setItemCapability(const QString &itemId, AeroraStateValue::Capability capability)
{
    AeroraReportedValue *const value = _model->reportedValue(itemId);
    if (!value) {
        return false;
    }

    value->setCapability(capability);

    return true;
}

void AeroraPayloadMock::disconnectPayload()
{
    _model->clearReports();
    _capabilities->clearReports();
    clearReportingSource();
}

quint64 AeroraPayloadMock::nowUs() const
{
    return _freshness->nowUs();
}

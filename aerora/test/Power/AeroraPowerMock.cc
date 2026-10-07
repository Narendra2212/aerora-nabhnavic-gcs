#include "AeroraPowerMock.h"

#include "AeroraFreshness.h"
#include "AeroraReportedValue.h"
#include "AeroraVehicleCapabilities.h"

AeroraPowerMock::AeroraPowerMock(RuntimeScheduler *scheduler, QObject *parent)
    : QObject(parent)
    , _freshness(new AeroraFreshness(scheduler, this))
    , _capabilities(new AeroraVehicleCapabilities(this))
    , _model(new AeroraPowerModel(_freshness, _capabilities, this))
{
}

AeroraPowerMock::~AeroraPowerMock() = default;

void AeroraPowerMock::setReportingSource(int systemId, int componentId)
{
    _source = AeroraTelemetrySource::fromSendingComponent(systemId, componentId);
}

void AeroraPowerMock::clearReportingSource()
{
    _source = AeroraTelemetrySource();
}

bool AeroraPowerMock::reportCapabilityAvailable(const QString &key)
{
    return _capabilities->report(AeroraVehicleCapabilities::Category::Vehicle,
                                 key,
                                 AeroraVehicleCapabilities::State::Available);
}

bool AeroraPowerMock::reportCapabilityUnavailable(const QString &key)
{
    return _capabilities->report(AeroraVehicleCapabilities::Category::Vehicle,
                                 key,
                                 AeroraVehicleCapabilities::State::Unavailable);
}

void AeroraPowerMock::clearCapabilityReports()
{
    _capabilities->clearReports();
}

void AeroraPowerMock::reportTetherConfiguration(const QString &reportedToken)
{
    _model->tetherConfiguration()->recordReport(reportedToken, nowUs(), _source);
}

void AeroraPowerMock::reportBatteryConfiguration(const QString &reportedToken)
{
    _model->batteryConfiguration()->recordReport(reportedToken, nowUs(), _source);
}

void AeroraPowerMock::reportConfiguration(AeroraPowerModel::Configuration configuration,
                                          const QString &reportedToken)
{
    AeroraReportedValue *const value = _model->configuration(configuration);
    if (!value) {
        return;
    }

    value->recordReport(reportedToken, nowUs(), _source);
}

void AeroraPowerMock::reportEffectiveState(const QString &reportedToken)
{
    _model->effectiveState()->recordReport(reportedToken, nowUs(), _source);
}

void AeroraPowerMock::reportReportedState(const QString &reportedToken)
{
    _model->reportedState()->recordReport(reportedToken, nowUs(), _source);
}

void AeroraPowerMock::reportSourceTransition(const QString &reportedToken)
{
    // The only path to an observed transition, and it exists because IF-PWR-004 is a real reported
    // item. Nothing derives a transition from the configuration items changing.
    _model->sourceTransition()->recordReport(reportedToken, nowUs(), _source);
}

void AeroraPowerMock::reportFault(const QString &reportedToken)
{
    _model->fault()->recordReport(reportedToken, nowUs(), _source);
}

void AeroraPowerMock::reportWatchdogEvent(const QString &reportedToken)
{
    // Touches the watchdog item and nothing else, so a test can assert that no power state, fault
    // or configuration moved with it.
    _model->watchdogEvent()->recordReport(reportedToken, nowUs(), _source);
}

bool AeroraPowerMock::markItemCapabilityAbsent(const QString &itemId)
{
    return setItemCapability(itemId, AeroraStateValue::CapabilityAbsent);
}

bool AeroraPowerMock::setItemCapability(const QString &itemId, AeroraStateValue::Capability capability)
{
    AeroraReportedValue *const value = _model->reportedValue(itemId);
    if (!value) {
        return false;
    }

    value->setCapability(capability);

    return true;
}

void AeroraPowerMock::disconnectPower()
{
    _model->clearReports();
    _capabilities->clearReports();
    clearReportingSource();
}

quint64 AeroraPowerMock::nowUs() const
{
    return _freshness->nowUs();
}

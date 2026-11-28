#include "AeroraDualPowerConfiguration.h"

#include "Fact.h"
#include "ParameterManager.h"
#include "QGCLoggingCategory.h"
#include "Vehicle.h"

QGC_LOGGING_CATEGORY(AeroraDualPowerLog, "aerora.power.dualpower")

namespace {

/// The component whose parameters are read. -1 is QGroundControl's "default component" selector,
/// the same value CODING_STYLE.md's own Fact example uses, and it resolves to whichever component
/// the vehicle reports as its autopilot. The component identifier itself is deliberately not named
/// here: this module reads a parameter by name and asserts nothing about addressing.
constexpr int kDefaultComponentId = -1;

}  // namespace

AeroraDualPowerConfiguration::AeroraDualPowerConfiguration(Vehicle* vehicle, QObject* parent)
    : QObject(parent)
    , _vehicle(vehicle)
{
    _connectParameterSignals();
    refresh();
}

AeroraDualPowerConfiguration::~AeroraDualPowerConfiguration() = default;

bool AeroraDualPowerConfiguration::isInterlockConsistent() const
{
    if (!isInterlockKnown()) {
        return false;
    }
    return interlockConsistentFrom(_tetherPowerConfigured, _tetherFenceConfigured);
}

bool AeroraDualPowerConfiguration::isReserveCapacityUnknown() const
{
    return reserveCapacityUnknownFrom(_powerSourceKnown, _tetherPowerConfigured);
}

void AeroraDualPowerConfiguration::refresh()
{
    const bool wasPowerSourceKnown = _powerSourceKnown;
    const bool wasTetherPower = _tetherPowerConfigured;
    const bool wasFenceModeKnown = _fenceModeKnown;
    const bool wasTetherFence = _tetherFenceConfigured;
    const bool wasRadiusKnown = _fenceRadiusKnown;
    const float wasRadius = _tetherFenceRadiusMetres;

    _tetherPowerConfigured = _readBool(kTetheredPowerParameter, _powerSourceKnown);
    _tetherFenceConfigured = _readBool(kTetherFenceModeParameter, _fenceModeKnown);
    _tetherFenceRadiusMetres = _readFloat(kTetherFenceRadiusParameter, _fenceRadiusKnown);

    const bool moved = (wasPowerSourceKnown != _powerSourceKnown) || (wasTetherPower != _tetherPowerConfigured) ||
                       (wasFenceModeKnown != _fenceModeKnown) || (wasTetherFence != _tetherFenceConfigured) ||
                       (wasRadiusKnown != _fenceRadiusKnown) ||
                       !qFuzzyCompare(1.0F + wasRadius, 1.0F + _tetherFenceRadiusMetres);

    if (!moved) {
        return;
    }

    // A warning rather than a debug line: this is a misconfiguration on the aircraft that no
    // component rejects, and the station reading the two parameters is the only place it becomes
    // visible. See the interlock note in the header.
    if (isInterlockKnown() && !isInterlockConsistent()) {
        qCWarning(AeroraDualPowerLog) << "tether interlock inconsistent:" << kTetheredPowerParameter << "="
                                      << _tetherPowerConfigured << kTetherFenceModeParameter << "="
                                      << _tetherFenceConfigured;
    }

    emit configurationChanged();
}

bool AeroraDualPowerConfiguration::_readBool(const char* name, bool& known) const
{
    bool reachable = false;
    const float value = _readFloat(name, reachable);
    known = reachable;
    if (!reachable) {
        return false;
    }
    // The firmware declares both interlock parameters as bool with range 0..1. Anything non-zero is
    // treated as set rather than compared against 1 exactly, so a value the aircraft accepted but
    // this code did not anticipate still reads as "tethered" - the direction that reports a
    // constraint rather than silently dropping one.
    return value != 0.0F;
}

float AeroraDualPowerConfiguration::_readFloat(const char* name, bool& known) const
{
    known = false;

    Vehicle* vehicle = _vehicle;
    if (!vehicle) {
        return 0.0F;
    }

    ParameterManager* parameters = vehicle->parameterManager();
    if (!parameters) {
        return 0.0F;
    }

    const QString parameterName = QString::fromLatin1(name);
    if (!parameters->parameterExists(kDefaultComponentId, parameterName)) {
        return 0.0F;
    }

    Fact* fact = parameters->getParameter(kDefaultComponentId, parameterName);
    if (!fact) {
        return 0.0F;
    }

    known = true;
    return fact->rawValue().toFloat();
}

void AeroraDualPowerConfiguration::_connectParameterSignals()
{
    Vehicle* vehicle = _vehicle;
    if (!vehicle) {
        return;
    }

    ParameterManager* parameters = vehicle->parameterManager();
    if (!parameters) {
        return;
    }

    // Re-read when the parameter set becomes available. Before that point every reading is unknown,
    // which is the correct state rather than a gap to be filled with defaults.
    connect(parameters, &ParameterManager::parametersReadyChanged, this, &AeroraDualPowerConfiguration::refresh);
}

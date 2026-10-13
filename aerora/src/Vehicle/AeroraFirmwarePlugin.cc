#include "AeroraFirmwarePlugin.h"

#include <QtCore/QString>

#include "AeroraAutoPilotPlugin.h"
#include "AeroraFirmwareClaim.h"
#include "AeroraMissionBridge.h"
#include "AeroraPayloadModel.h"
#include "AeroraPowerModel.h"
#include "AeroraTelemetrySet.h"
#include "AeroraToolIndicators.h"
#include "AeroraVehicleCapabilities.h"
#include "AeroraVehicleFactGroupRegistrar.h"
#include "AeroraVehicleLog.h"
#include "AeroraVehicleServices.h"
#include "Vehicle.h"

namespace {

/// Attaches @a service under @a name, deleting it if the registry refuses.
///
/// attachService() only reparents on success, so a refused service would otherwise be left
/// unparented and leak. Mirrors the failure handling in AeroraMissionBridge::attachTo().
bool attachOrDelete(AeroraVehicleServices *services, const char *name, QObject *service)
{
    if (services->attachService(QString::fromUtf8(name), service)) {
        return true;
    }

    // attachService() has already warned with the reason. This only records the disposal.
    qCDebug(AeroraVehicleLog) << "discarding unattached service"
                              << "name:" << name;
    delete service;

    return false;
}

/// Publishes @a telemetrySet's registered fact groups into @a vehicle's own fact-group map.
///
/// This is the Q4-002 path made operational (ADR-016). The chain is
/// `AeroraTelemetrySet::publishTo()` → `AeroraVehicleFactGroupRegistrar` →
/// `Vehicle::_addFactGroup()`, and it is deliberately **not**
/// `FirmwarePlugin::factGroups()`: that seam takes no `Vehicle *` and the plugin behind it is
/// cached per firmware class, so groups returned through it would be shared by every vehicle of
/// that class.
///
/// The registrar is stack-local, and that is the whole of its required lifetime. The contract on
/// `AeroraFactGroupRegistrar::registerFactGroup()` forbids retaining the groups, and
/// `FactGroup::_addFactGroup()` stores the `FactGroup` pointers rather than the registrar, so the
/// registrar has no work left once `publishTo()` returns. Ownership of the groups stays with the
/// `AeroraTelemetrySet`, which is a child of the vehicle's services object. Keeping the registrar
/// alive would mean putting telemetry-specific state on the generic services hub for diagnostics
/// the set already exposes through `isPublished()`.
void publishTelemetry(Vehicle *vehicle, AeroraTelemetrySet *telemetrySet)
{
    // One registrar per vehicle, built for this vehicle and holding nothing else. No static, no
    // cache, no shared instance.
    AeroraVehicleFactGroupRegistrar registrar(vehicle);

    if (telemetrySet->publishTo(registrar)) {
        qCDebug(AeroraVehicleLog) << "published Aerora telemetry"
                                  << "mavlinkSystemId:" << vehicle->id()
                                  << "groups:" << registrar.registeredNames();
        return;
    }

    // A refusal is a real failure rather than a quiet degradation: a consumer would otherwise look
    // for Aerora values in the telemetry value picker and the CSV record and not find them. The
    // registrar also refuses everything when AERORA_VEHICLE_FACTGROUP_FRIENDSHIP is absent, so the
    // build-level availability is reported alongside the names to tell the two cases apart.
    qCWarning(AeroraVehicleLog) << "Aerora telemetry publication incomplete"
                                << "mavlinkSystemId:" << vehicle->id()
                                << "refused:" << registrar.refusedNames()
                                << "registrationAvailable:"
                                << AeroraVehicleFactGroupRegistrar::isRegistrationAvailable();
}

} // namespace

AeroraFirmwarePlugin::AeroraFirmwarePlugin(QObject *parent)
    : FirmwarePlugin(parent)
{
}

AeroraFirmwarePlugin::~AeroraFirmwarePlugin() = default;

void AeroraFirmwarePlugin::initializeVehicle(Vehicle *vehicle)
{
    if (!vehicle) {
        qCWarning(AeroraVehicleLog) << "initializeVehicle called with no vehicle";
        return;
    }

    FirmwarePlugin::initializeVehicle(vehicle);

    AeroraVehicleServices *const services = AeroraVehicleServices::attachTo(vehicle);
    if (!services) {
        return;
    }

    // Resolves to nothing while no product profile is registered, which is the current state.
    // A later capability report retries resolution through AeroraVehicleServices.
    services->resolveProductProfile();

    // Before the offline-editing return below, because the capability inventory is attached on
    // that path too and the connection costs nothing when there is no indicator to re-offer.
    _installToolIndicatorInvalidation(services->capabilities());

    if (vehicle->isOfflineEditingVehicle()) {
        // The offline-editing vehicle exists so a plan can be built with no aircraft present. It
        // has no link, reports nothing, and its firmware class follows a settings value rather
        // than a heartbeat. Composing payload, power and telemetry onto it would create models
        // that can only ever read UNKNOWN, plus a freshness evaluator and its scheduler, for a
        // vehicle that is not one. Identity and capabilities stay attached because plan editing
        // legitimately reads them.
        qCDebug(AeroraVehicleLog) << "offline editing vehicle, domain services not composed";
        return;
    }

    attachDomainServices(services);
}

void AeroraFirmwarePlugin::attachDomainServices(AeroraVehicleServices *services)
{
    if (!services) {
        qCWarning(AeroraVehicleLog) << "attachDomainServices called with no services object";
        return;
    }

    if (services->service(QString::fromUtf8(AeroraTelemetrySet::kServiceName))) {
        qCDebug(AeroraVehicleLog) << "domain services already composed";
        return;
    }

    // Telemetry first, for two reasons: it owns the AeroraFreshness evaluator the two models below
    // require, and it is the set that gets published to the vehicle at the end of this function.
    //
    // AeroraTelemetrySet::factGroups() is never called here. That seam hands out the raw map and
    // seals the set without registering anything, and reaching it from
    // FirmwarePlugin::factGroups() is what would reintroduce the shared-plugin hazard. Publication
    // goes through AeroraVehicleFactGroupRegistrar instead - see publishTelemetry() below.
    auto *const telemetrySet = new AeroraTelemetrySet();
    if (!attachOrDelete(services, AeroraTelemetrySet::kServiceName, telemetrySet)) {
        return;
    }

    // Not owned by the models. AeroraReportedValue holds it as a QPointer and null-checks every
    // use, so the models stay correct whichever order the services are destroyed in.
    AeroraFreshness *const freshness = telemetrySet->freshness();

    // Null until a capability is reported, which gates both models' optional surfaces off rather
    // than on. Absence of evidence is not permission.
    AeroraVehicleCapabilities *const capabilities = services->capabilities();

    (void) attachOrDelete(services,
                          AeroraPayloadModel::kServiceName,
                          new AeroraPayloadModel(freshness, capabilities));

    (void) attachOrDelete(services,
                          AeroraPowerModel::kServiceName,
                          new AeroraPowerModel(freshness, capabilities));

    // The remaining two steps need the vehicle itself. On the test path there is none, so there is
    // no MissionManager to observe and no vehicle to register fact groups on - a legitimate
    // absence rather than a failure.
    if (Vehicle *const vehicle = services->vehicle()) {
        // The bridge attaches its own observer under its own service name and parents itself to
        // that observer, so nothing here owns it.
        (void) AeroraMissionBridge::attachTo(vehicle);

        // Published last so every module composed above has already had its chance to register a
        // group. Publication seals the set, and sealing is one-way.
        publishTelemetry(vehicle, telemetrySet);
    }
}

AutoPilotPlugin *AeroraFirmwarePlugin::autopilotPlugin(Vehicle *vehicle) const
{
    if (!vehicle) {
        qCWarning(AeroraVehicleLog) << "cannot create the Aerora autopilot plugin, no vehicle";
        return nullptr;
    }

    // Parented to the vehicle, matching the base. Vehicle re-parents it to itself anyway and
    // deletes it in its own destructor (Vehicle.cc:145-146, :407).
    return new AeroraAutoPilotPlugin(vehicle, vehicle);
}

const QVariantList &AeroraFirmwarePlugin::toolIndicators(const Vehicle *vehicle)
{
    const QVariantList &baseIndicators = FirmwarePlugin::toolIndicators(vehicle);

    const QList<AeroraToolIndicators::IndicatorDescriptor> declared = AeroraToolIndicators::declaredIndicators();
    if (declared.isEmpty()) {
        // Aerora declares no indicator, so the base list is returned untouched rather than copied
        // into this object's member. Nothing is added and nothing is removed.
        return baseIndicators;
    }

    if (!vehicle) {
        // Without a vehicle there is no capability inventory, and a capability-gated indicator
        // must not be offered on no evidence.
        qCWarning(AeroraVehicleLog) << "no vehicle, Aerora tool indicators withheld";
        return baseIndicators;
    }

    const AeroraVehicleServices *const services = AeroraVehicleServices::forVehicle(vehicle);

    const AeroraVehicleCapabilities *const capabilities = services ? services->capabilities() : nullptr;

    _aeroraToolIndicators = baseIndicators;
    _aeroraToolIndicators.append(AeroraToolIndicators::urlsFor(declared, capabilities));

    return _aeroraToolIndicators;
}

void AeroraFirmwarePlugin::_installToolIndicatorInvalidation(AeroraVehicleCapabilities *capabilities)
{
    if (!capabilities || AeroraToolIndicators::declaredIndicators().isEmpty()) {
        return;
    }

    (void) connect(capabilities, &AeroraVehicleCapabilities::capabilityChanged,
                   this, &AeroraFirmwarePlugin::toolIndicatorsChanged);
    (void) connect(capabilities, &AeroraVehicleCapabilities::reportsCleared,
                   this, &AeroraFirmwarePlugin::toolIndicatorsChanged);
}

MAV_AUTOPILOT AeroraFirmwarePlugin::_autopilotType() const
{
    return QGCMAVLink::firmwareClassToAutopilot(AeroraFirmwareClaim::kFirmwareClass);
}

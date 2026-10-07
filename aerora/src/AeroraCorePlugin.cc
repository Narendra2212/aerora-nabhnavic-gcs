#include "AeroraCorePlugin.h"

#include <QtCore/QApplicationStatic>

#include "AeroraCommandAuthorizer.h"
#include "AeroraPlatformKeyProtector.h"
#include "AeroraSigningKeyStore.h"
#include "MAVLinkSigningKeys.h"
#include "QGCLoggingCategory.h"
#include "Vehicle.h"

QGC_LOGGING_CATEGORY(AeroraCorePluginLog, "Aerora.CorePlugin")

Q_APPLICATION_STATIC(AeroraCorePlugin, _aeroraCorePluginInstance);

AeroraCorePlugin *AeroraCorePlugin::instance()
{
    return _aeroraCorePluginInstance();
}

AeroraCorePlugin::AeroraCorePlugin(QObject *parent)
    : QGCCorePlugin(parent)
{
    qCDebug(AeroraCorePluginLog) << this;
}

AeroraCorePlugin::~AeroraCorePlugin()
{
    qCDebug(AeroraCorePluginLog) << this;
}

void AeroraCorePlugin::init()
{
    QGCCorePlugin::init();

    _installSigningKeyStore();

    if (!AeroraCommandAuthorizer::instance()->isFullyConfigured()) {
        qCWarning(AeroraCorePluginLog)
            << "command authorization is NOT being enforced: no authorization policy is installed."
            << "The enforcement point is present in the command send path, but no decisions are made"
            << "until an approved operator role policy exists (Q-029).";
    }
}

void AeroraCorePlugin::_installSigningKeyStore()
{
    auto protector = AeroraPlatformKeyProtector::createForCurrentPlatform();
    if (!protector || !protector->isAvailable()) {
        // Leave the core default (QSettings, unprotected) in place rather than installing a store
        // that would refuse every write and break signing. The posture is unchanged from stock QGC
        // and is stated here so it cannot be mistaken for a protected configuration.
        qCWarning(AeroraCorePluginLog)
            << "no platform key protector available: MAVLink signing keys remain stored WITHOUT"
            << "protection at rest, as in stock QGroundControl. See D-002, F3.1-01, R-005.";
        return;
    }

    auto store = std::make_unique<AeroraSigningKeyStore>(std::move(protector));
    const bool protects = store->protectsAtRest();
    const QString backend = store->backendName();

    MAVLinkSigningKeys::setKeyStore(std::move(store));

    qCDebug(AeroraCorePluginLog) << "signing key store installed. backend:" << backend
                                 << "protectsAtRest:" << protects;
}

bool AeroraCorePlugin::isAuthorizationEnforced()
{
    return AeroraCommandAuthorizer::instance()->isFullyConfigured();
}

bool AeroraCorePlugin::authorizeMavCommand(Vehicle *vehicle, int targetCompId, int command, bool commandInt,
                                           float param1, float param2, float param3, float param4,
                                           double param5, double param6, float param7)
{
    auto *authorizer = AeroraCommandAuthorizer::instance();

    if (!authorizer->isFullyConfigured()) {
        if (!_warnedUnconfigured) {
            _warnedUnconfigured = true;
            qCWarning(AeroraCorePluginLog)
                << "allowing commands without authorization evaluation: no policy installed (Q-029).";
        }
        return true;
    }

    AeroraGatedOperation operation;
    operation.mavCommand = command;
    operation.targetCompId = targetCompId;
    operation.commandInt = commandInt;
    // The gate is reached on paths that may hold no vehicle. An unresolved vehicle is recorded
    // as unresolved rather than defaulted, so a denial never names a vehicle it did not concern.
    if (vehicle) {
        operation.vehicle.mavlinkSystemId = vehicle->id();
        operation.vehicle.resolved = true;
    }
    operation.param1 = param1;
    operation.param2 = param2;
    operation.param3 = param3;
    operation.param4 = param4;
    operation.param5 = param5;
    operation.param6 = param6;
    operation.param7 = param7;

    return authorizer->isAuthorized(operation);
}

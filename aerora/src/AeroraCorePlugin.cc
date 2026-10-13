#include "AeroraCorePlugin.h"

#include <QtCore/QApplicationStatic>
#include <QtCore/QUrl>
#include <QtCore/QVariant>
#include <QtQml/QQmlApplicationEngine>

#include "AeroraCommandAuthorizer.h"
#include "AeroraPlatformKeyProtector.h"
#include "AeroraQmlOverrides.h"
#include "AeroraSigningKeyStore.h"
#include "MAVLinkSigningKeys.h"
#include "QGCLoggingCategory.h"
#include "QmlComponentInfo.h"
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

QQmlApplicationEngine *AeroraCorePlugin::createQmlApplicationEngine(QObject *parent)
{
    QQmlApplicationEngine *const qmlEngine = QGCCorePlugin::createQmlApplicationEngine(parent);

    // Returned unchanged rather than warning: a null engine is the base's failure to report, and
    // installOn() would only restate it. Nothing downstream here can proceed either way.
    if (!qmlEngine) {
        return qmlEngine;
    }

    AeroraQmlOverrides::installOn(qmlEngine);

    // The allow-list is empty in this build, so this passes trivially. It is called anyway because
    // the condition it catches - a declared override whose mirrored resource is absent - is
    // otherwise silent, and the first populated entry is exactly when nobody will remember to add
    // the call.
    const AeroraQmlOverrides::SelfCheckResult selfCheck = AeroraQmlOverrides::verify();
    if (!selfCheck.passed) {
        qCWarning(AeroraCorePluginLog)
            << "QML override self-check failed: declared overrides have no mirrored resource and"
            << "silently did not take effect. missing:" << selfCheck.missingMirroredResources;
    }

    return qmlEngine;
}

void AeroraCorePlugin::destroyQmlApplicationEngine(QQmlApplicationEngine *qmlEngine)
{
    // Before the base call, which deletes the engine. releaseFrom() is a no-op for an engine this
    // class did not install on, so an out-of-order teardown cannot delete another engine's
    // interceptor.
    AeroraQmlOverrides::releaseFrom(qmlEngine);

    QGCCorePlugin::destroyQmlApplicationEngine(qmlEngine);
}

const QVariantList &AeroraCorePlugin::analyzePages()
{
    // Built once on first call and held in this override's own static, because the base returns a
    // reference to its own function-local static list: appending to that would mutate the base's
    // list, and returning a reference to a local would dangle.
    static const QVariantList pages = [this]() {
        QVariantList list = QGCCorePlugin::analyzePages();

        // No icon argument - QmlComponentInfo defaults it to an empty QUrl. Aerora has no icon
        // asset and Q-040 leaves the visual language unresolved, so none is invented here.
        //
        // requiresVehicle stays false. The hub's purpose is to report what is and is not attached,
        // which includes "no vehicle is connected"; gating it on a vehicle would hide the screen in
        // exactly the state an operator most needs to read it.
        //
        // The URL carries no Screens/ segment: qgc_set_qt_resource_alias() aliases every
        // AERORA_UI_QML_FILES entry to its bare name, so the whole module lands flat under
        // qrc:/qml/Aerora/Controls/.
        list.append(QVariant::fromValue(new QmlComponentInfo(
            tr("Aerora Operations"),
            QUrl::fromUserInput(QStringLiteral("qrc:/qml/Aerora/Controls/AeroraOperationsHub.qml")))));

        return list;
    }();

    return pages;
}

const QVariantList &AeroraCorePlugin::toolBarIndicators()
{
    // Same shape and same reason as analyzePages() above: the base returns a reference to its own
    // function-local static, so this override keeps its own.
    // Captures this because QGCCorePlugin::toolBarIndicators() is a non-static member; a
    // non-capturing lambda would not compile.
    static const QVariantList indicators = [this]() {
        QVariantList list = QGCCorePlugin::toolBarIndicators();

        // A bare QUrl, not a QmlComponentInfo - this registry and analyzePages() differ in element
        // type, and the consumer loads this one directly as Loader.source.
        //
        // No Indicators/ segment in the URL: qgc_set_qt_resource_alias() aliases every
        // AERORA_UI_QML_FILES entry to its bare name, so the module lands flat under
        // qrc:/qml/Aerora/Controls/ regardless of the source subdirectory.
        list.append(QVariant::fromValue(
            QUrl::fromUserInput(QStringLiteral("qrc:/qml/Aerora/Controls/AeroraEnforcementIndicator.qml"))));

        return list;
    }();

    return indicators;
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

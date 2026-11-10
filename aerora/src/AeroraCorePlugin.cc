#include "AeroraCorePlugin.h"

#include <QtCore/QApplicationStatic>
#include <QtCore/QUrl>
#include <QtCore/QVariant>
#include <QtQml/QQmlApplicationEngine>

#include "AeroraCommandAuthorizer.h"
#include "AeroraMissionExtension.h"
#include "AeroraPlanBridge.h"
#include "AeroraPlatformKeyProtector.h"
#include "AeroraQmlOverrides.h"
#include "AeroraSigningKeyStore.h"
#include "MAVLinkLib.h"
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

    // Plan observation is attached from here because PlanMasterController is QML_ELEMENT with no
    // C++ factory, registry or singleton: a pointer to one exists only inside the QML object tree.
    // objectCreated is the first moment the Plan view's and Fly view's controllers exist. The
    // connection is not single-shot - the engine may load more than one root - and attachTo() is
    // idempotent per controller, so a second root cannot double-report.
    //
    // Deliberately not wired to planCreators(): that hook is a factory for plan creators, it is
    // handed only the Plan view's controller, and capturing a pointer from it would be a side
    // effect in a function whose contract is to return a list.
    connect(qmlEngine, &QQmlApplicationEngine::objectCreated, this, [](QObject* rootObject, const QUrl& url) {
        if (!rootObject) {
            // The engine reports a load failure as a null object. It has already logged the
            // reason, so restating it here would be noise.
            return;
        }

        qCDebug(AeroraCorePluginLog) << "attaching plan observation to QML scene:" << url;
        (void) AeroraPlanBridge::attachToQmlScene(rootObject);
    });

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

        // Also vehicle-independent, and for the same reason: the page reports whether a mission
        // observer is attached at all, so "no vehicle is connected" is one of its readings.
        list.append(QVariant::fromValue(new QmlComponentInfo(
            tr("Aerora Mission Progress"),
            QUrl::fromUserInput(QStringLiteral("qrc:/qml/Aerora/Controls/AeroraMissionProgressPage.qml")))));

        // SCR-HLP-02. Vehicle-independent in the strongest sense of the three: it is reference
        // material derived from AeroraTelemetryState's own enumeration and predicates, and reads no
        // vehicle at all. Gating it on a connection would withhold the vocabulary from an operator
        // reading it to prepare for one.
        list.append(QVariant::fromValue(new QmlComponentInfo(
            tr("Aerora State Vocabulary"),
            QUrl::fromUserInput(QStringLiteral("qrc:/qml/Aerora/Controls/AeroraStateVocabularyPage.qml")))));

        // SCR-SEC-01. Vehicle-independent for the reason that matters most on this screen: the
        // authorization posture is a property of this station, not of whatever is connected to it,
        // and an operator checking whether commands are evaluated should not have to connect an
        // aircraft to find out.
        list.append(QVariant::fromValue(new QmlComponentInfo(
            tr("Aerora Security"),
            QUrl::fromUserInput(QStringLiteral("qrc:/qml/Aerora/Controls/AeroraSecurityPosturePage.qml")))));

        // SCR-HLP-03. Vehicle-independent because the state most worth reporting is often the state
        // where nothing is connected - a report that could only be assembled from a healthy station
        // would be useless in the case it exists for.
        list.append(QVariant::fromValue(new QmlComponentInfo(
            tr("Aerora Error Report"),
            QUrl::fromUserInput(QStringLiteral("qrc:/qml/Aerora/Controls/AeroraErrorReportPage.qml")))));

        // SCR-SHL-04. Every value on it is a build-time constant or a station setting, so a vehicle
        // gate would withhold the answer to "which build is this" from an operator who has not
        // connected anything - which is when the question is usually asked.
        list.append(QVariant::fromValue(new QmlComponentInfo(
            tr("Aerora Build"),
            QUrl::fromUserInput(QStringLiteral("qrc:/qml/Aerora/Controls/AeroraProvenancePage.qml")))));

        return list;
    }();

    return pages;
}

QVariantList AeroraCorePlugin::complexMissionItemNames(Vehicle *vehicle)
{
    // Null-checked rather than passed through: QGCCorePlugin::complexMissionItemNames()
    // dereferences the vehicle to decide whether StructureScan applies to the frame. Skipping the
    // base call costs the upstream patterns in a state that should not occur; passing a null would
    // crash inside upstream code.
    QVariantList items;
    if (vehicle != nullptr) {
        items = QGCCorePlugin::complexMissionItemNames(vehicle);
    } else {
        qCWarning(AeroraCorePluginLog)
            << "complexMissionItemNames called with no vehicle: the upstream patterns are omitted"
            << "because the base implementation dereferences it.";
    }

    // Appends rather than replaces, for the reason analyzePages() appends: the upstream patterns
    // are not Aerora to remove. The list is empty today because no extension is registered, so
    // this is behaviourally identical to the base until one is.
    const QStringList registered =
        AeroraMissionExtensionRegistry::instance()->complexMissionItemNames();
    for (const QString &name : registered) {
        QVariantMap entry;
        entry[QStringLiteral("canonicalName")] = name;
        entry[QStringLiteral("translatedName")] = name;
        items.append(entry);
    }

    return items;
}

ComplexMissionItem *AeroraCorePlugin::createComplexMissionItem(const QString &complexItemType,
                                                               PlanMasterController *masterController,
                                                               bool flyView,
                                                               const QString &kmlOrShpFile)
{
    // Registry first. The base logs an Unknown complex item type warning before returning nullptr,
    // so asking it first would put a false warning on the normal path for every Aerora item.
    if (ComplexMissionItem *const item =
            AeroraMissionExtensionRegistry::instance()->createComplexMissionItem(complexItemType,
                                                                                masterController,
                                                                                flyView)) {
        return item;
    }

    return QGCCorePlugin::createComplexMissionItem(complexItemType, masterController, flyView,
                                                   kmlOrShpFile);
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

    AeroraCommandSubject subject;
    subject.mavCommand = command;
    subject.targetCompId = targetCompId;
    subject.commandInt = commandInt;
    subject.param1 = param1;
    subject.param2 = param2;
    subject.param3 = param3;
    subject.param4 = param4;
    subject.param5 = param5;
    subject.param6 = param6;
    subject.param7 = param7;

    AeroraGatedOperation operation;
    operation.command = subject;

    // The gate is reached on paths that may hold no vehicle. An address carrying none is recorded
    // as carrying none rather than as carrying a defaulted vehicle, so a denial never names a
    // vehicle it did not concern.
    if (vehicle) {
        AeroraVehicleContext addressed;
        addressed.mavlinkSystemId = vehicle->id();
        addressed.resolved = true;
        operation.address.vehicles.append(addressed);
    }

    // Layer the operation-entry credential gate over role authorization. The classification is the
    // only thing read from the command id here; the gate and the role chain take it from there.
    bool entersOperation = false;
    bool exitsOperation = false;
    classifyOperationBoundary(command, param1, entersOperation, exitsOperation);

    return authorizer->authorizeThroughOperationGate(operation, entersOperation, exitsOperation);
}

void AeroraCorePlugin::classifyOperationBoundary(int command, float param1, bool& entersOperation, bool& exitsOperation)
{
    // Arming is the entry to an operation and disarming is the exit, per the owner's answers
    // A1 and A3 (product-completion 15). Both are MAV_CMD_COMPONENT_ARM_DISARM, distinguished by
    // param1: 1 arms, 0 disarms. A force-arm carries its magic in param2, so param1 still decides
    // here. Any other command is neither, so it is never credential-gated - only role-gated.
    const bool isArmDisarm = (command == MAV_CMD_COMPONENT_ARM_DISARM);
    entersOperation = isArmDisarm && (param1 == 1.0f);
    exitsOperation = isArmDisarm && (param1 == 0.0f);
}

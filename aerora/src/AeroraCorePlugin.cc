#include "AeroraCorePlugin.h"

#include <algorithm>

#include <QtCore/QApplicationStatic>
#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QIODevice>
#include <QtCore/QUrl>
#include <QtCore/QVariant>
#include <QtQml/QQmlApplicationEngine>

#include "AeroraAssignedRoleResolver.h"
#include "AeroraAuditWriter.h"
#include "AeroraCachedCredentialIdentityProvider.h"
#include "AeroraCommandAuthorizer.h"
#include "AeroraCredentialDocument.h"
#include "AeroraEd25519Verifier.h"
#include "AeroraLocalStore.h"
#include "AeroraMissionExtension.h"
#include "AeroraPlanBridge.h"
#include "AeroraPlatformKeyProtector.h"
#include "AeroraProvenance.h"
#include "AeroraQmlOverrides.h"
#include "AeroraRolePolicy.h"
#include "AeroraSigningKeyStore.h"
#include "AeroraStorePaths.h"
#include "AeroraVehicleIdentity.h"
#include "AeroraVehicleServices.h"
#include "AeroraVehicleStore.h"
#include "MAVLinkLib.h"
#include "MAVLinkSigningKeys.h"
#include "QGCLoggingCategory.h"
#include "QmlComponentInfo.h"
#include "Vehicle.h"
#include "aerora_credential_issuer.h"

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
    _installPersistence();
    _installAuthorizationChain();

    if (!AeroraCommandAuthorizer::instance()->isFullyConfigured()) {
        qCWarning(AeroraCorePluginLog)
            << "command authorization is NOT being enforced: no authorization policy is installed."
            << "The enforcement point is present in the command send path, but no decisions are made"
            << "until an approved operator role policy exists (Q-029).";
    }
}

void AeroraCorePlugin::cleanup()
{
    _teardownPersistence();

    QGCCorePlugin::cleanup();
}

bool AeroraCorePlugin::mavlinkMessage(Vehicle* vehicle, LinkInterface* link, const mavlink_message_t& message)
{
    if (message.msgid == MAVLINK_MSG_ID_AUTOPILOT_VERSION) {
        _resolveFleetIdentity(vehicle, message);
    }

    return QGCCorePlugin::mavlinkMessage(vehicle, link, message);
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

void AeroraCorePlugin::_installAuthorizationChain()
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();
    if (authorizer->isFullyConfigured()) {
        // Idempotent: QmlUITestBase::startUI() calls init() once per scene boot.
        return;
    }

    std::unique_ptr<AeroraEd25519Verifier> verifier =
        AeroraEd25519Verifier::fromHex(QString::fromLatin1(AERORA_CREDENTIAL_ISSUER_PUBLIC_KEY));
    if (!verifier) {
        // The shipped default. Stated at debug rather than warning level because it is the
        // configured posture of a build nobody provisioned, and init() already warns once that
        // authorization is not being enforced.
        qCDebug(AeroraCorePluginLog) << "no credential issuer key is configured in this build, so no credential can be"
                                     << "verified and command authorization stays unenforced.";
        return;
    }

    const QString credentialPath = AeroraStorePaths::credentialPath();
    QFile credentialFile(credentialPath);
    if (credentialPath.isEmpty() || !credentialFile.exists()) {
        qCDebug(AeroraCorePluginLog) << "no operator credential is provisioned at" << credentialPath
                                     << "so command authorization stays unenforced.";
        return;
    }
    if (!credentialFile.open(QIODevice::ReadOnly)) {
        qCWarning(AeroraCorePluginLog) << "could not read the operator credential at" << credentialPath
                                       << "so command authorization stays unenforced:" << credentialFile.errorString();
        return;
    }

    const AeroraCredentialDocument::Result credential =
        AeroraCredentialDocument::parseAndVerify(credentialFile.readAll(), verifier.get());
    credentialFile.close();

    if (!credential.accepted) {
        // Warning rather than debug: a credential is present and was refused, which is a
        // provisioning fault an operator can act on, unlike the absence of one.
        qCWarning(AeroraCorePluginLog) << "the provisioned operator credential was refused, so command"
                                       << "authorization stays unenforced:" << credential.detail;
        return;
    }

    // The assignment carries exactly the operator this credential names, holding exactly the roles
    // the issuer bound to them. One credential provisions one operator; a station serving several
    // needs the cloud reconciliation IF-SEC-009 describes, which replaces this source and leaves
    // everything below it untouched.
    AeroraAssignedRoleResolver::Assignment assignment;
    assignment.insert(credential.credential.operatorId, credential.roleIds);

    // The command class is the one seam ADR-014 Option C records as built: MavCommandQueue through
    // QGCCorePlugin::authorizeMavCommand(), with Vehicle::startCalibration() under ADR-015. The
    // parameter, mission, manual-control and payload classes have no seam here, so no policy is
    // installed for them and nothing claims to gate them.
    auto policy = std::make_unique<AeroraRolePolicy>(QStringLiteral("command"), credential.roleGrants);
    const bool policyConfigured = policy->isConfigured();

    authorizer->setIdentityProvider(std::make_unique<AeroraCachedCredentialIdentityProvider>(credential.credential));
    authorizer->setRoleResolver(std::make_unique<AeroraAssignedRoleResolver>(assignment));
    authorizer->setPolicy(std::move(policy));

    qCDebug(AeroraCorePluginLog) << "installed the authorization chain from a verified credential."
                                 << "issuer:" << verifier->verifierName() << "roles:" << credential.roleIds
                                 << "policy configured:" << policyConfigured
                                 << "enforcing:" << authorizer->isFullyConfigured();
}

void AeroraCorePlugin::_installPersistence()
{
    // Idempotent: QmlUITestBase::startUI() calls init() once per scene boot, so a UI-test process
    // can reach here more than once. A store already open is left as it is.
    if (_store) {
        return;
    }

    if (AeroraStorePaths::isPersistenceDisabled()) {
        // P7-DAT-005: nothing is opened and no file is created, so the station reports durable
        // audit as off and resolves no fleet identity, rather than opening a file and discarding
        // writes.
        qCDebug(AeroraCorePluginLog) << "persistence is disabled; durable audit and fleet identity are off "
                                        "(P7-DAT-005).";
        return;
    }

    AeroraStorePaths::ensureDirectories();

    auto store = std::make_unique<AeroraLocalStore>();
    const QString databasePath = AeroraStorePaths::databasePath();
    const AeroraLocalStore::OpenResult result = store->open(databasePath, false);
    if (!result.opened) {
        // Fail closed: no store, so no sink is installed and the authorizer keeps counting what it
        // cannot write. The store refused a file it did not recognise rather than resetting it, so
        // this path also covers an unreadable existing store (design §5).
        qCWarning(AeroraCorePluginLog) << "persistence store did not open; durable audit and fleet identity are off:"
                                       << result.detail;
        return;
    }

    const AeroraProvenance provenance;
    AeroraLocalStore::SessionRecord session;
    session.startedUtcMs = QDateTime::currentMSecsSinceEpoch();
    // Base and overlay are one commit in this repository (AeroraProvenance documents why), so the
    // one hash describes both columns. Empty when the build was not made from a checkout.
    session.baseCommit = provenance.commit();
    session.overlayCommit = provenance.commit();
#ifdef QGC_CUSTOM_BUILD
    session.customBuild = true;
#else
    session.customBuild = false;
#endif
    session.authorizationEnforced = isAuthorizationEnforced();

    const qint64 sessionId = store->beginSession(session);
    if (sessionId < 0) {
        qCWarning(AeroraCorePluginLog) << "could not begin a session; durable audit and fleet identity are off.";
        return;  // the store closes as the unique_ptr leaves scope
    }

    _store = std::move(store);
    _sessionId = sessionId;

    // DG-FLT's authority: opened unconditionally alongside the store, since every vehicle connect
    // can carry AUTOPILOT_VERSION evidence regardless of whether the audit path is healthy.
    auto vehicleStore = std::make_unique<AeroraVehicleStore>(*_store);
    if (vehicleStore->open()) {
        _vehicleStore = std::move(vehicleStore);
    } else {
        qCWarning(AeroraCorePluginLog) << "vehicle store did not open; fleet identity resolution is off.";
    }

    // The queue bound, a function-local const rather than a named constexpr so no queue-depth value
    // enters the overlay's contract surface. Generous for the few records a command emits, bounded
    // so a storage stall degrades to dropped records rather than unbounded memory.
    const qsizetype auditQueueCapacity = 1024;
    _auditWriter = std::make_shared<AeroraAuditWriter>(*_store, auditQueueCapacity);
    _auditWriter->start();
    AeroraCommandAuthorizer::instance()->setAuditSink(_auditWriter);

    qCDebug(AeroraCorePluginLog) << "persistence installed at" << databasePath << "session" << sessionId
                                 << "fleet identity resolution:" << (_vehicleStore != nullptr);
}

void AeroraCorePlugin::_teardownPersistence()
{
    // Clear the authorizer's reference first. It is a function-local static that outlives this
    // Q_APPLICATION_STATIC plugin, and its sink holds the store by reference, so the reference is
    // dropped here while this still owns the lifetime. By cleanup() the event loop has stopped and
    // vehicles are gone, so no record() and no mavlinkMessage() is in flight.
    AeroraCommandAuthorizer::instance()->setAuditSink(nullptr);

    if (_auditWriter) {
        // Joins the worker after it has drained the queue and closed the connection, so every
        // record accepted before shutdown is written before the session is ended.
        _auditWriter->stop();
        _auditWriter.reset();
    }

    if (_vehicleStore) {
        _vehicleStore->close();
        _vehicleStore.reset();
    }

    if (_store) {
        if (_sessionId >= 0) {
            // An unstamped session is how an unclean exit reads, so an orderly shutdown stamps it.
            _store->endSession(_sessionId, QDateTime::currentMSecsSinceEpoch());
            _sessionId = -1;
        }
        _store->close();
        _store.reset();
    }
}

void AeroraCorePlugin::_resolveFleetIdentity(Vehicle* vehicle, const mavlink_message_t& message)
{
    if (!vehicle || !_vehicleStore || !_vehicleStore->isOpen()) {
        return;
    }

    AeroraVehicleServices* const services = AeroraVehicleServices::forVehicle(vehicle);
    if (!services || !services->identity()) {
        // initializeVehicle() attaches services before the initial-connect state machine's
        // AUTOPILOT_VERSION reply can arrive (it only starts the async request), so this guards a
        // vehicle this class does not otherwise expect rather than a real race.
        return;
    }

    (void) reportFleetIdentityFrom(*_vehicleStore, *services->identity(), message, QDateTime::currentMSecsSinceEpoch());
}

QString AeroraCorePlugin::reportFleetIdentityFrom(AeroraVehicleStore& vehicleStore, AeroraVehicleIdentity& identity,
                                                  const mavlink_message_t& message, qint64 nowUtcMs)
{
    mavlink_autopilot_version_t autopilotVersion;
    mavlink_msg_autopilot_version_decode(&message, &autopilotVersion);

    const QByteArray uid2(reinterpret_cast<const char*>(autopilotVersion.uid2), sizeof(autopilotVersion.uid2));
    const QString boardUid = boardUidFromAutopilotVersion(autopilotVersion.uid, uid2);
    if (boardUid.isEmpty()) {
        // Neither field carried evidence - MockLink's simulated reply is exactly this state. No
        // board identifier is no contact, not a reason to invent one; the vehicle stays a first
        // contact (fleetIdentityKnown() false) rather than being minted against a sentinel.
        return QString();
    }

    const QString fleetIdentity = vehicleStore.resolveFleetIdentity(boardUid, nowUtcMs);
    if (fleetIdentity.isEmpty()) {
        return QString();
    }

    (void) identity.reportFleetIdentity(fleetIdentity);
    return fleetIdentity;
}

QString AeroraCorePlugin::boardUidFromAutopilotVersion(quint64 uid, const QByteArray& uid2)
{
    const bool uid2Present = std::any_of(uid2.cbegin(), uid2.cend(), [](char byte) { return byte != 0; });
    if (uid2Present) {
        return QString::fromLatin1(uid2.toHex());
    }

    if (uid != 0) {
        return QString::number(uid, 16);
    }

    return QString();
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

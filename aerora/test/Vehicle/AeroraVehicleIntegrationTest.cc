#include "AeroraVehicleIntegrationTest.h"

#include <QtCore/QRegularExpression>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "AeroraFirmwareClaim.h"
#include "AeroraFirmwarePlugin.h"
#include "AeroraFirmwarePluginFactory.h"
#include "AeroraFreshness.h"
#include "AeroraPayloadModel.h"
#include "AeroraPowerModel.h"
#include "AeroraTelemetrySet.h"
#include "AeroraVehicleCapabilities.h"
#include "AeroraVehicleIdentity.h"
#include "AeroraVehicleMock.h"
#include "AeroraVehicleServices.h"
#include "FirmwarePluginFactory.h"
#include "QGCMAVLink.h"

using Category = AeroraVehicleCapabilities::Category;
using State = AeroraVehicleCapabilities::State;

namespace {

constexpr const char *kLogCategory = "Aerora.Vehicle";

/// Every optional capability QGroundControl can ask a firmware plugin about.
const QList<FirmwarePlugin::FirmwareCapabilities> &allFirmwareCapabilities()
{
    static const QList<FirmwarePlugin::FirmwareCapabilities> capabilities = {
        FirmwarePlugin::SetFlightModeCapability,
        FirmwarePlugin::PauseVehicleCapability,
        FirmwarePlugin::GuidedModeCapability,
        FirmwarePlugin::OrbitModeCapability,
        FirmwarePlugin::TakeoffVehicleCapability,
        FirmwarePlugin::ROIModeCapability,
        FirmwarePlugin::ChangeHeadingCapability,
        FirmwarePlugin::GuidedTakeoffCapability,
        FirmwarePlugin::VTOLMulticopterTakeoffCapability,
    };
    return capabilities;
}

} // namespace

void AeroraVehicleIntegrationTest::cleanup()
{
    // The profile registry is process-global. Clearing it before the stubs are destroyed keeps a
    // dangling registration from leaking into any later test that constructs a vehicle.
    AeroraProductProfileRegistry::instance()->clear();
    qDeleteAll(_stubProfiles);
    _stubProfiles.clear();

    UnitTest::cleanup();
}

void AeroraVehicleIntegrationTest::_identityKeepsMavlinkAndFleetIdentityApart()
{
    AeroraVehicleIdentity identity(7, 1);

    QCOMPARE(identity.mavlinkSystemId(), 7);
    QCOMPARE(identity.mavlinkComponentId(), 1);

    // A known MAVLink identity must not imply a known fleet identity, and the unknown fleet
    // identity must be empty rather than falling back to "7".
    QVERIFY(!identity.fleetIdentityKnown());
    QVERIFY(identity.fleetIdentity().isEmpty());

    expectLogMessage(kLogCategory, QtWarningMsg, QRegularExpression(QStringLiteral("empty token")));
    QVERIFY(!identity.reportFleetIdentity(QString()));
    verifyExpectedLogMessage();
    QVERIFY(!identity.fleetIdentityKnown());

    QSignalSpy changedSpy(&identity, &AeroraVehicleIdentity::fleetIdentityChanged);

    // The token is opaque: no format is evidenced, so it is stored verbatim.
    const QString token = QStringLiteral("opaque-token-supplied-by-an-evidence-source");
    QVERIFY(identity.reportFleetIdentity(token));
    QVERIFY(identity.fleetIdentityKnown());
    QCOMPARE(identity.fleetIdentity(), token);
    QCOMPARE(changedSpy.count(), 1);

    // Reporting a fleet identity must not disturb the protocol identity.
    QCOMPARE(identity.mavlinkSystemId(), 7);

    identity.clearFleetIdentity();
    QVERIFY(!identity.fleetIdentityKnown());
    QVERIFY(identity.fleetIdentity().isEmpty());
    QCOMPARE(changedSpy.count(), 2);
}

void AeroraVehicleIntegrationTest::_capabilityEvidenceStates_data()
{
    QTest::addColumn<bool>("doReport");
    QTest::addColumn<State>("reportState");
    QTest::addColumn<bool>("expectAccepted");
    QTest::addColumn<State>("expectState");
    QTest::addColumn<bool>("expectAvailable");

    QTest::newRow("unreported") << false << State::NotEvidenced << false << State::NotEvidenced << false;
    QTest::newRow("available") << true << State::Available << true << State::Available << true;
    QTest::newRow("unavailable") << true << State::Unavailable << true << State::Unavailable << false;
    QTest::newRow("notEvidencedIsNotReportable")
        << true << State::NotEvidenced << false << State::NotEvidenced << false;
}

void AeroraVehicleIntegrationTest::_capabilityEvidenceStates()
{
    QFETCH(bool, doReport);
    QFETCH(State, reportState);
    QFETCH(bool, expectAccepted);
    QFETCH(State, expectState);
    QFETCH(bool, expectAvailable);

    AeroraVehicleCapabilities capabilities;
    const QString key = QStringLiteral("opaque-capability-key");

    if (doReport) {
        if (!expectAccepted) {
            expectLogMessage(kLogCategory,
                             QtWarningMsg,
                             QRegularExpression(QStringLiteral("NotEvidenced is not reportable")));
        }
        QCOMPARE(capabilities.report(Category::Vehicle, key, reportState), expectAccepted);
        if (!expectAccepted) {
            verifyExpectedLogMessage();
        }
    }

    QCOMPARE(capabilities.state(Category::Vehicle, key), expectState);
    QCOMPARE(capabilities.isAvailable(Category::Vehicle, key), expectAvailable);

    // A report in one category must not leak into another.
    QCOMPARE(capabilities.state(Category::Payload, key), State::NotEvidenced);
    QVERIFY(!capabilities.isAvailable(Category::Payload, key));

    // An unrelated key stays unreported regardless of what happened to this one.
    QCOMPARE(capabilities.state(Category::Vehicle, QStringLiteral("other-key")), State::NotEvidenced);
}

void AeroraVehicleIntegrationTest::_capabilityClearReturnsToNotEvidenced()
{
    AeroraVehicleCapabilities capabilities;
    const QString first = QStringLiteral("first-key");
    const QString second = QStringLiteral("second-key");

    QVERIFY(capabilities.report(Category::Vehicle, first, State::Available));
    QVERIFY(capabilities.report(Category::Vehicle, second, State::Unavailable));
    QVERIFY(capabilities.report(Category::Payload, first, State::Available));

    QCOMPARE(capabilities.reportedKeys(Category::Vehicle), QStringList({first, second}));
    QVERIFY(capabilities.hasAnyReport());

    QSignalSpy clearedSpy(&capabilities, &AeroraVehicleCapabilities::reportsCleared);
    capabilities.clearReports();
    QCOMPARE(clearedSpy.count(), 1);

    // Clearing returns to "nothing reported", not to "reported absent". Collapsing the two would
    // make a disconnected vehicle look like a vehicle that answered.
    QCOMPARE(capabilities.state(Category::Vehicle, first), State::NotEvidenced);
    QCOMPARE(capabilities.state(Category::Vehicle, second), State::NotEvidenced);
    QCOMPARE(capabilities.state(Category::Payload, first), State::NotEvidenced);
    QVERIFY(capabilities.reportedKeys(Category::Vehicle).isEmpty());
    QVERIFY(!capabilities.hasAnyReport());

    // Clearing an already-clear inventory is silent.
    capabilities.clearReports();
    QCOMPARE(clearedSpy.count(), 1);
}

void AeroraVehicleIntegrationTest::_classificationIsNeverInferredFromCapabilities()
{
    AeroraVehicleServices services(nullptr);

    // No vehicle, so not even plugin selection has happened.
    QCOMPARE(services.classification(), AeroraVehicleServices::Classification::Unknown);

    QSignalSpy classificationSpy(&services, &AeroraVehicleServices::classificationChanged);

    QVERIFY(services.capabilities()->report(Category::Vehicle, QStringLiteral("a"), State::Available));
    QVERIFY(services.capabilities()->report(Category::Payload, QStringLiteral("b"), State::Available));
    QVERIFY(services.capabilities()->report(Category::Command, QStringLiteral("c"), State::Available));

    // Capability reports say what a vehicle can do. They are not evidence of whose vehicle it is,
    // so no number of them may promote the classification.
    QCOMPARE(services.classification(), AeroraVehicleServices::Classification::Unknown);
    QCOMPARE(classificationSpy.count(), 0);

    // Only an explicit corroboration from an evidence source moves it.
    services.reportAeroraCorroboration();
    QCOMPARE(services.classification(), AeroraVehicleServices::Classification::CorroboratedAerora);
    QCOMPARE(classificationSpy.count(), 1);

    expectLogMessage(kLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("contradicts an earlier Aerora corroboration")));
    services.reportNotAerora();
    verifyExpectedLogMessage();
    QCOMPARE(services.classification(), AeroraVehicleServices::Classification::NotAerora);
}

void AeroraVehicleIntegrationTest::_firmwarePluginKeepsBaseClassSafeDefaults()
{
    AeroraFirmwarePlugin plugin;

    // The guard against the FORBIDDEN fact-group architecture, and the reason this assertion is
    // load-bearing rather than incidental.
    //
    // Aerora fact groups reach a vehicle through AeroraTelemetrySet::publishTo() ->
    // AeroraVehicleFactGroupRegistrar -> Vehicle::_addFactGroup(), one registrar per vehicle
    // (ADR-016). FirmwarePlugin::factGroups() is the competing route and must stay unused: the
    // plugin is cached per firmware class, so anything returned here would be handed to every
    // vehicle of that class and the aircraft would overwrite each other's telemetry (R4-001).
    //
    // A future change that overrides factGroups() to return a set fails here, which is the point.
    QCOMPARE(plugin.factGroups(), nullptr);

    // No optional capability is claimed without evidence.
    for (const FirmwarePlugin::FirmwareCapabilities capability : allFirmwareCapabilities()) {
        QVERIFY2(!plugin.isCapable(nullptr, capability),
                 qPrintable(QStringLiteral("capability claimed without evidence: %1")
                                .arg(static_cast<int>(capability))));
    }

    // An empty flight-mode name makes QGroundControl hide the dependent guided action. Hiding is
    // the safe failure; a guessed Aerora mode name would wire a button to the wrong behaviour.
    QVERIFY(plugin.pauseFlightMode().isEmpty());
    QVERIFY(plugin.missionFlightMode().isEmpty());
    QVERIFY(plugin.rtlFlightMode().isEmpty());
    QVERIFY(plugin.smartRTLFlightMode().isEmpty());
    QVERIFY(plugin.landFlightMode().isEmpty());
    QVERIFY(plugin.takeOffFlightMode().isEmpty());
    QVERIFY(plugin.motorDetectionFlightMode().isEmpty());
    QVERIFY(plugin.stabilizedFlightMode().isEmpty());
    QVERIFY(plugin.takeControlFlightMode().isEmpty());
    QVERIFY(plugin.gotoFlightMode().isEmpty());
    QVERIFY(plugin.followFlightMode().isEmpty());
    QVERIFY(!plugin.supportsSmartRTL());
    QVERIFY(plugin.flightModes(nullptr).isEmpty());
    QVERIFY(!plugin.isGuidedMode(nullptr));

    // The plugin observes traffic; it never vetoes. A veto here would silently break
    // ParameterManager and FTP for every vehicle of this firmware class.
    mavlink_message_t message{};
    QVERIFY(plugin.adjustIncomingMavlinkMessage(nullptr, &message));

    // No mission command set is asserted until a dialect exists; empty means "all supported".
    QVERIFY(plugin.supportedMissionCommands(QGCMAVLink::VehicleClassMultiRotor).isEmpty());

    // Attaching services to nothing is a defensive no-op, not a crash.
    expectLogMessage(kLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("initializeVehicle called with no vehicle")));
    plugin.initializeVehicle(nullptr);
    verifyExpectedLogMessage();
}

void AeroraVehicleIntegrationTest::_factoryRegistersItselfAndClaimsOneFirmwareClass()
{
    // Deliberately found through the registry rather than by naming AeroraFirmwarePluginFactoryImp.
    // Referencing the global would force its translation unit to link and so would hide the exact
    // failure this test exists to catch: a self-registering static discarded by the linker, which
    // produces no diagnostic and leaves the operator looking at a generic vehicle.
    AeroraFirmwarePluginFactory *aeroraFactory = nullptr;
    int aeroraFactoryCount = 0;
    const QList<FirmwarePluginFactory *> factories = FirmwarePluginFactoryRegister::instance()->pluginFactories();
    for (FirmwarePluginFactory *const factory : factories) {
        if (AeroraFirmwarePluginFactory *const candidate = qobject_cast<AeroraFirmwarePluginFactory *>(factory)) {
            aeroraFactory = candidate;
            aeroraFactoryCount++;
        }
    }

    QCOMPARE(aeroraFactoryCount, 1);
    QVERIFY(aeroraFactory);

    QCOMPARE(aeroraFactory->supportedFirmwareClasses(),
             QList<QGCMAVLink::FirmwareClass_t>({AeroraFirmwareClaim::kFirmwareClass}));

    // Every vehicle class stays supported, so a future product family is not rejected at the
    // factory by a guess about its airframe.
    QCOMPARE(aeroraFactory->supportedVehicleClasses(), QGCMAVLink::allVehicleClasses());

    const MAV_AUTOPILOT claimed =
        QGCMAVLink::firmwareClassToAutopilot(AeroraFirmwareClaim::kFirmwareClass);
    FirmwarePlugin *const plugin = aeroraFactory->firmwarePluginForAutopilot(claimed, MAV_TYPE_HEXAROTOR);
    QVERIFY(qobject_cast<AeroraFirmwarePlugin *>(plugin));

    // One shared instance per firmware class. This is the property Q4-002 is about.
    QCOMPARE(aeroraFactory->firmwarePluginForAutopilot(claimed, MAV_TYPE_QUADROTOR), plugin);

    // A vendor autopilot is never served the Aerora plugin, whatever the build configuration.
    QCOMPARE(aeroraFactory->firmwarePluginForAutopilot(MAV_AUTOPILOT_PX4, MAV_TYPE_HEXAROTOR), nullptr);
    QCOMPARE(aeroraFactory->firmwarePluginForAutopilot(MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_TYPE_HEXAROTOR),
             nullptr);
}

void AeroraVehicleIntegrationTest::_productProfileResolutionRefusesAmbiguity()
{
    AeroraProductProfileRegistry *const registry = AeroraProductProfileRegistry::instance();
    registry->clear();

    AeroraVehicleServices services(nullptr);

    // An empty registry resolves to nothing, and that is not an error: a build may carry no
    // profile at all, which is the current state.
    services.resolveProductProfile();
    QCOMPARE(services.productProfile(), nullptr);
    QVERIFY(services.productProfileId().isEmpty());

    StubProfile *const nonMatching = new StubProfile(QStringLiteral("non-matching"), false);
    _stubProfiles.append(nonMatching);
    QVERIFY(registry->registerProfile(nonMatching));

    expectLogMessage(kLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("duplicate profile id")));
    QVERIFY(!registry->registerProfile(nonMatching));
    verifyExpectedLogMessage();

    services.resolveProductProfile();
    QCOMPARE(services.productProfile(), nullptr);
    QCOMPARE(nonMatching->attachCount, 0);

    StubProfile *const matching = new StubProfile(QStringLiteral("matching"), true);
    _stubProfiles.append(matching);
    QVERIFY(registry->registerProfile(matching));

    QSignalSpy profileSpy(&services, &AeroraVehicleServices::productProfileChanged);
    services.resolveProductProfile();
    QCOMPARE(services.productProfile(), static_cast<AeroraProductProfile *>(matching));
    QCOMPARE(services.productProfileId(), QStringLiteral("matching"));
    QCOMPARE(matching->attachCount, 1);
    QCOMPARE(profileSpy.count(), 1);

    // Resolution happens once. A profile has already attached its services by now, so swapping it
    // on a live vehicle would leave those services behind.
    services.resolveProductProfile();
    QCOMPARE(matching->attachCount, 1);
    QCOMPARE(profileSpy.count(), 1);

    // Two matching profiles is a build configuration defect. Resolving it by registration order
    // would make the outcome depend on link order and hide the defect.
    StubProfile *const alsoMatching = new StubProfile(QStringLiteral("also-matching"), true);
    _stubProfiles.append(alsoMatching);
    QVERIFY(registry->registerProfile(alsoMatching));

    AeroraVehicleServices ambiguousServices(nullptr);
    expectLogMessage(kLogCategory,
                     QtWarningMsg,
                     QRegularExpression(QStringLiteral("ambiguous")));
    ambiguousServices.resolveProductProfile();
    verifyExpectedLogMessage();
    QCOMPARE(ambiguousServices.productProfile(), nullptr);
    QCOMPARE(alsoMatching->attachCount, 0);
}

void AeroraVehicleIntegrationTest::_serviceAttachmentIsTheSingleMechanism()
{
    AeroraVehicleServices services(nullptr);

    QObject *const telemetry = new QObject();
    QObject *const power = new QObject();
    QSignalSpy attachedSpy(&services, &AeroraVehicleServices::serviceAttached);

    QVERIFY(services.attachService(QStringLiteral("telemetry"), telemetry));
    QVERIFY(services.attachService(QStringLiteral("power"), power));
    QCOMPARE(attachedSpy.count(), 2);

    QCOMPARE(services.service(QStringLiteral("telemetry")), telemetry);
    QCOMPARE(services.service(QStringLiteral("power")), power);
    QCOMPARE(services.serviceNames(), QStringList({QStringLiteral("telemetry"), QStringLiteral("power")}));
    QCOMPARE(services.service(QStringLiteral("absent")), nullptr);

    // Reparenting is the lifetime guarantee: an attached service dies with the vehicle, so no
    // workstream needs teardown code of its own.
    QCOMPARE(telemetry->parent(), static_cast<QObject *>(&services));

    expectLogMessage(kLogCategory, QtWarningMsg, QRegularExpression(QStringLiteral("already attached")));
    QVERIFY(!services.attachService(QStringLiteral("telemetry"), new QObject(&services)));
    verifyExpectedLogMessage();

    expectLogMessage(kLogCategory, QtWarningMsg, QRegularExpression(QStringLiteral("empty name")));
    QVERIFY(!services.attachService(QString(), new QObject(&services)));
    verifyExpectedLogMessage();

    expectLogMessage(kLogCategory, QtWarningMsg, QRegularExpression(QStringLiteral("null service")));
    QVERIFY(!services.attachService(QStringLiteral("nothing"), nullptr));
    verifyExpectedLogMessage();

    QCOMPARE(attachedSpy.count(), 2);
    QCOMPARE(services.serviceNames().count(), 2);
}

void AeroraVehicleIntegrationTest::_mockIsDeterministicAndFabricatesNoVehicle()
{
    AeroraVehicleMock mock;
    QSignalSpy discoveredSpy(&mock, &AeroraVehicleMock::vehicleDiscovered);
    QSignalSpy connectedSpy(&mock, &AeroraVehicleMock::vehicleConnected);
    QSignalSpy disconnectedSpy(&mock, &AeroraVehicleMock::vehicleDisconnected);
    QSignalSpy forgottenSpy(&mock, &AeroraVehicleMock::vehicleForgotten);

    QVERIFY(!mock.isDiscovered());
    QCOMPARE(mock.services(), nullptr);

    expectLogMessage(kLogCategory, QtWarningMsg, QRegularExpression(QStringLiteral("nothing discovered")));
    mock.connectVehicle();
    verifyExpectedLogMessage();
    QCOMPARE(connectedSpy.count(), 0);

    mock.discover();
    QVERIFY(mock.isDiscovered());
    QVERIFY(mock.services());

    // The mock stands in for the integration surface; it does not impersonate QGroundControl.
    QCOMPARE(mock.services()->vehicle(), nullptr);

    // Repeated calls are idempotent, so a test's observable sequence depends only on its own
    // distinct transitions.
    mock.discover();
    mock.connectVehicle();
    mock.connectVehicle();
    QCOMPARE(discoveredSpy.count(), 1);
    QCOMPARE(connectedSpy.count(), 1);
    QVERIFY(mock.isConnected());

    const QString key = QStringLiteral("opaque-capability-key");
    QVERIFY(mock.reportCapabilityAvailable(Category::Payload, key));
    QVERIFY(mock.services()->capabilities()->isAvailable(Category::Payload, key));
    QVERIFY(mock.reportCapabilityUnavailable(Category::Payload, key));
    QCOMPARE(mock.services()->capabilities()->state(Category::Payload, key), State::Unavailable);

    mock.disconnectVehicle();
    mock.disconnectVehicle();
    QCOMPARE(disconnectedSpy.count(), 1);
    QVERIFY(!mock.isConnected());

    // Evidence belongs to the connection that produced it.
    QCOMPARE(mock.services()->capabilities()->state(Category::Payload, key), State::NotEvidenced);

    mock.forget();
    QCOMPARE(forgottenSpy.count(), 1);
    QVERIFY(!mock.isDiscovered());
    QCOMPARE(mock.services(), nullptr);
}

void AeroraVehicleIntegrationTest::_domainServicesAreComposedPerVehicle()
{
    // Two services objects stand in for two vehicles. The composition is driven directly rather
    // than through two MockLink connections, because what is being asserted is that nothing is
    // shared — and that is a property of the composition, not of the link.
    AeroraVehicleServices servicesA(nullptr);
    AeroraVehicleServices servicesB(nullptr);

    AeroraFirmwarePlugin::attachDomainServices(&servicesA);
    AeroraFirmwarePlugin::attachDomainServices(&servicesB);

    const QString telemetryName = QString::fromUtf8(AeroraTelemetrySet::kServiceName);
    const QString payloadName = QString::fromUtf8(AeroraPayloadModel::kServiceName);
    const QString powerName = QString::fromUtf8(AeroraPowerModel::kServiceName);

    // Attachment order is dependency order, and telemetry must come first because the payload and
    // power models take its freshness evaluator as a constructor argument.
    QCOMPARE(servicesA.serviceNames(), QStringList({telemetryName, payloadName, powerName}));

    auto *const telemetryA = qobject_cast<AeroraTelemetrySet *>(servicesA.service(telemetryName));
    auto *const telemetryB = qobject_cast<AeroraTelemetrySet *>(servicesB.service(telemetryName));
    auto *const payloadA = qobject_cast<AeroraPayloadModel *>(servicesA.service(payloadName));
    auto *const payloadB = qobject_cast<AeroraPayloadModel *>(servicesB.service(payloadName));
    auto *const powerA = qobject_cast<AeroraPowerModel *>(servicesA.service(powerName));
    auto *const powerB = qobject_cast<AeroraPowerModel *>(servicesB.service(powerName));

    QVERIFY(telemetryA);
    QVERIFY(telemetryB);
    QVERIFY(payloadA);
    QVERIFY(payloadB);
    QVERIFY(powerA);
    QVERIFY(powerB);

    // The whole point: no object and no evaluator is shared between the two vehicles.
    QVERIFY(telemetryA != telemetryB);
    QVERIFY(payloadA != payloadB);
    QVERIFY(powerA != powerB);
    QVERIFY(telemetryA->freshness());
    QVERIFY(telemetryA->freshness() != telemetryB->freshness());

    // Each service is reparented to its own vehicle's services object, which is what ties its
    // lifetime to that vehicle and means no teardown code is needed anywhere.
    QCOMPARE(telemetryA->parent(), static_cast<QObject *>(&servicesA));
    QCOMPARE(payloadA->parent(), static_cast<QObject *>(&servicesA));
    QCOMPARE(powerA->parent(), static_cast<QObject *>(&servicesA));
    QCOMPARE(powerB->parent(), static_cast<QObject *>(&servicesB));

    // Neither set is published, because neither services object has a vehicle and a registrar
    // built for no vehicle can register nothing. This pins the composition root's guard, not an
    // absence of wiring: _domainServicesAttachToADiscoveredVehicle asserts isPublished() on a real
    // vehicle. Unpublished therefore implies unsealed here.
    QVERIFY(!telemetryA->isPublished());
    QVERIFY(!telemetryA->isSealed());
    QVERIFY(telemetryA->groupNames().isEmpty());
    QVERIFY(!telemetryA->sharedAcrossVehiclesDetected());

    // No mission service: these services objects have no vehicle, so there is no MissionManager to
    // observe. An absent observer is the correct outcome, not a failure.
    QVERIFY(!servicesA.service(QStringLiteral("mission")));
}

void AeroraVehicleIntegrationTest::_domainServiceCompositionIsIdempotent()
{
    AeroraVehicleServices services(nullptr);

    AeroraFirmwarePlugin::attachDomainServices(&services);
    const QStringList afterFirst = services.serviceNames();
    QObject *const telemetry = services.service(QString::fromUtf8(AeroraTelemetrySet::kServiceName));
    QVERIFY(telemetry);

    QSignalSpy attachedSpy(&services, &AeroraVehicleServices::serviceAttached);

    // A repeated initialize must not produce a second set of models reporting into one vehicle.
    // The early return keeps this silent rather than emitting three rejections.
    AeroraFirmwarePlugin::attachDomainServices(&services);

    QCOMPARE(services.serviceNames(), afterFirst);
    QCOMPARE(attachedSpy.count(), 0);
    QCOMPARE(services.service(QString::fromUtf8(AeroraTelemetrySet::kServiceName)), telemetry);

    expectLogMessage(kLogCategory, QtWarningMsg, QRegularExpression(QStringLiteral("no services object")));
    AeroraFirmwarePlugin::attachDomainServices(nullptr);
    verifyExpectedLogMessage();
}

void AeroraVehicleIntegrationTest::_domainServicesOpenNoActuatorPath()
{
    AeroraVehicleServices services(nullptr);
    AeroraFirmwarePlugin::attachDomainServices(&services);

    auto *const payload =
        qobject_cast<AeroraPayloadModel *>(services.service(QString::fromUtf8(AeroraPayloadModel::kServiceName)));
    auto *const power =
        qobject_cast<AeroraPowerModel *>(services.service(QString::fromUtf8(AeroraPowerModel::kServiceName)));
    QVERIFY(payload);
    QVERIFY(power);

    // Composing these models onto a vehicle must not have opened an outbound path. The owning
    // modules pin these as constants precisely so attachment cannot quietly change them, and this
    // asserts the pins hold at the composition root rather than only in each module's own test.
    QVERIFY(!AeroraPayloadModel::kGroundStationActuatorPathImplemented);
    QVERIFY(!AeroraPayloadModel::kAutonomousPayloadActionImplemented);
    QVERIFY(!AeroraPowerModel::kStationInitiatedPowerActionImplemented);
    QVERIFY(!AeroraPowerModel::kPowerArbitrationImplemented);
    QVERIFY(!AeroraPowerModel::kAutomaticSourceTransitionImplemented);
    QVERIFY(!AeroraPowerModel::kRemainingEnergyEstimationImplemented);
    QCOMPARE(AeroraPowerModel::kDefinedThresholdCount, 0);

    // No payload command path is installed, so a request cannot leave the ground station.
    QVERIFY(!payload->isCommandPathInstalled());

    // Capabilities start at NotEvidenced, so both optional surfaces are gated off. "We have not
    // been told" is not permission.
    QVERIFY(!services.capabilities()->hasAnyReport());
    QVERIFY(!payload->controlsPermitted());
    QVERIFY(!power->isExtendedSurfacePermitted());
}

UT_REGISTER_TEST(AeroraVehicleIntegrationTest, TestLabel::Unit)

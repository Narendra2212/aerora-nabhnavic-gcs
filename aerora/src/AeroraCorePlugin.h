#pragma once

#include <memory>

#include <QtCore/QByteArray>

#include "QGCCorePlugin.h"

class AeroraAuditWriter;
class AeroraLocalStore;
class AeroraVehicleIdentity;
class AeroraVehicleStore;

/// \brief Aerora core plugin. Binds the Aerora security foundation to QGC's extension points.
///
/// Layer 1 scope: this overrides only what the security and command boundary needs. Branding,
/// UI, firmware factories and resource overrides are deliberately untouched - they belong to
/// the overlay foundation work, and the vendor firmware factories in particular must not be
/// disabled while Q4-004 is unresolved.
///
class AeroraCorePlugin : public QGCCorePlugin
{
    Q_OBJECT

public:
    static AeroraCorePlugin *instance();

    explicit AeroraCorePlugin(QObject *parent = nullptr);
    ~AeroraCorePlugin() override;

    void init() override;

    /// Tears down the durable audit path (`QGCApplication::shutdown()` calls this after the event
    /// loop has stopped and vehicles are gone, before any static is destroyed). It is the symmetric
    /// counterpart to `init()`'s store open, and the only safe place for the teardown: the command
    /// authorizer is a function-local static that outlives this plugin, so its sink reference must
    /// be cleared here while this still owns the store and writer.
    void cleanup() override;

    /// Observes `AUTOPILOT_VERSION` traffic to resolve `DG-FLT`'s fleet identity (see
    /// _resolveFleetIdentity()) and otherwise does nothing; every message, including the one it
    /// inspects, passes through to the base unexamined. This is an observer, never a gate: it must
    /// not and does not return false.
    bool mavlinkMessage(Vehicle* vehicle, LinkInterface* link, const mavlink_message_t& message) override;

    /// Installs the Aerora QML override interceptor on the engine the base implementation creates.
    ///
    /// The interceptor is the only reason an Aerora QML override file is ever loaded: without it an
    /// overlay override is compiled into a resource and never requested, with no error of any kind
    /// (R-019, IF-UIS-014). See aerora/src/UI/AeroraQmlOverrides.h for the seam and the allow-list.
    ///
    /// Installs *after* the base call, so the base keeps ownership of engine construction and its
    /// qrc:/qml import path is unchanged. No addImportPath() call is made here.
    QQmlApplicationEngine *createQmlApplicationEngine(QObject *parent) override;

    /// Releases the interceptor *before* the base implementation destroys the engine.
    ///
    /// Order is load-bearing in both directions: the interceptor is owned by this class, not by the
    /// engine, so it must be removed and deleted while the engine is still alive.
    void destroyQmlApplicationEngine(QQmlApplicationEngine *qmlEngine) override;

    /// Appends the Aerora Operations Hub to the Analyze page registry (HO-TAB7-05, SCR-SHL-01).
    ///
    /// Registration is what makes the screen reachable at all: without it the QML compiles into
    /// the resource, the module registers, and no menu entry opens it - with no error of any kind.
    /// That matters more than one menu entry here, because SCR-SHL-01 is the screen that reports
    /// authorization is *not* being enforced (P7-AUTH-002, P7-SEC-005).
    ///
    /// Appends to the base list rather than replacing it: the upstream Analyze pages are not
    /// Aerora's to remove. See aerora/src/UI/Screens/AeroraOperationsHub.qml.
    const QVariantList &analyzePages() override;

    /// Appends the Aerora enforcement-state strip to the non-vehicle toolbar indicator registry
    /// (HO-TAB7-06, SCR-SHL-03).
    ///
    /// This registry carries bare QUrl values, not QmlComponentInfo objects as analyzePages()
    /// does, and FlyViewToolBarIndicators.qml loads each one with
    /// `visible: item.showIndicator` and no null guard. So a registered URL must resolve to a
    /// root declaring that property; AeroraEnforcementIndicator.qml declares it.
    ///
    /// Non-vehicle is the correct registry for this strip: AeroraSecurity is a process singleton
    /// readable with no vehicle connected. Anything whose membership depended on reported
    /// capability would have to go through FirmwarePlugin::toolIndicators() instead, because this
    /// one is a CONSTANT property and is never re-read after the first bind.
    ///
    /// Appends to the base list rather than replacing it: upstream's RTK GPS indicator is not
    /// Aerora's to remove.
    const QVariantList &toolBarIndicators() override;

    /// Appends the registered complex mission item names to the base list (HO-TAB5-01).
    ///
    /// Required before the first extension registers rather than optional: MissionController
    /// reads this to build the Plan view pattern menu, and createComplexMissionItem() below is
    /// what a saved plan goes through on load. Without both, an extension item never appears in
    /// the editor and a plan containing one fails to load with only a warning.
    ///
    /// \a vehicle is null-checked rather than passed straight through. The base dereferences it
    /// to decide whether StructureScan applies to the frame, so a null would crash inside upstream
    /// code; when it is null the base is skipped and only the registered names are returned, which
    /// is a degraded list rather than a crash.
    ///
    /// Each entry carries `canonicalName` and `translatedName` as the base does, and for a
    /// registered name the two are the same string. An extension supplies its name at runtime, and
    /// a runtime string has no catalogue entry to look up, so calling tr() on it would imply a
    /// translation that does not exist. An extension needing a translated display name needs a
    /// second accessor on AeroraMissionExtension; that is recorded here rather than invented.
    QVariantList complexMissionItemNames(Vehicle *vehicle) override;

    /// Creates a registered complex item, or delegates to the base (HO-TAB5-01).
    ///
    /// The registry is consulted *first*. The base logs "Unknown complex item type" before
    /// returning nullptr, so delegating first would put a false warning on the normal path for
    /// every Aerora item: the type is known, just not to upstream.
    ///
    /// The consequence of that order is that an extension registering an upstream name would
    /// shadow it, and the registry cannot see the upstream set in order to refuse it. What
    /// prevents it is the plan-file key convention DG-MIS settles: an Aerora key carries the
    /// `Aerora` prefix, so it cannot collide with a present or future upstream pattern. A build
    /// that ignores the convention is a build defect this seam does not detect.
    ///
    /// \a kmlOrShpFile is not forwarded to the registry, because AeroraMissionExtension takes no
    /// such argument: an Aerora item is initialised from its own geometry rather than from a KML
    /// or SHP import. The argument still reaches the base for the upstream items.
    ComplexMissionItem *createComplexMissionItem(const QString &complexItemType,
                                                 PlanMasterController *masterController,
                                                 bool flyView,
                                                 const QString &kmlOrShpFile = QString()) override;

    /// Delegates to AeroraCommandAuthorizer when an authorization policy is configured.
    ///
    /// While no policy is configured (Q-029 is OPEN) this returns true and warns once. That is a
    /// recorded gap, not a design position: the enforcement point exists in the core send path and
    /// the chain is wired, but it cannot evaluate without an approved role policy. The two
    /// alternatives were rejected - denying every command makes the station unusable, and claiming
    /// a boundary that performs no evaluation would be dishonest.
    bool authorizeMavCommand(Vehicle *vehicle, int targetCompId, int command, bool commandInt,
                             float param1, float param2, float param3, float param4,
                             double param5, double param6, float param7) override;

    /// True when Aerora is actually evaluating command authorization. False means no policy is
    /// installed and no authorization decisions are being made.
    [[nodiscard]] static bool isAuthorizationEnforced();

    /// Classify a queued command against the operation boundary the credential gate needs. \a
    /// entersOperation is set for an arm, \a exitsOperation for a disarm, and both are cleared for
    /// any other command. Pure and static so it is unit-testable without a plugin instance; the
    /// one place the overlay reads a MAVLink command id, kept at this call-site boundary rather
    /// than in the authorization types, which carry \c mavCommand as an opaque int.
    static void classifyOperationBoundary(int command, float param1, bool& entersOperation, bool& exitsOperation);

    /// Extracts the board identifier `AUTOPILOT_VERSION` carries, as `DG-FLT`'s authority
    /// requires: \a uid2 when it carries evidence (at least one non-zero byte), \a uid as the
    /// fallback the MAVLink field itself documents ("If this is non-zero, use this field,
    /// otherwise use uid"). Hex-encoded, lower-case, so a binary or numeric board identifier
    /// becomes one `TEXT` lookup key. Empty when neither carries evidence - MockLink's simulated
    /// reply is exactly that state - which is reported rather than answered with an invented
    /// sentinel to mint against.
    ///
    /// Pure and static, and takes the two already-decoded fields rather than the MAVLink struct
    /// itself, so it is testable with no MAVLink type in this header and no packed message
    /// required in a test - the same reasoning that keeps classifyOperationBoundary() a call-site
    /// boundary over primitives rather than a MAVLink-typed one.
    [[nodiscard]] static QString boardUidFromAutopilotVersion(quint64 uid, const QByteArray& uid2);

    /// Decodes an `AUTOPILOT_VERSION` \a message, resolves its board identifier through \a
    /// vehicleStore, and reports the result into \a identity. Returns the fleet identity reported,
    /// or an empty string when the message carried no board evidence or the store resolved none.
    ///
    /// Static and taking its two collaborators by reference so the whole decode-resolve-report
    /// path is testable without a `Vehicle`, a plugin instance or `init()` - which a unit-test
    /// process never calls, so the member below could not otherwise be exercised at all. The
    /// member is the plumbing that finds these two from a vehicle; this is the behaviour.
    static QString reportFleetIdentityFrom(AeroraVehicleStore& vehicleStore, AeroraVehicleIdentity& identity,
                                           const mavlink_message_t& message, qint64 nowUtcMs);

private:
    void _installSigningKeyStore();

    /// Opens the Aerora local store, begins the audit session, opens the vehicle store, starts the
    /// asynchronous audit writer and installs it on the command authorizer. Honours
    /// `disableAllPersistence` and fails closed: when persistence is off or the store will not
    /// open, nothing is installed and the authorizer keeps counting what it cannot write, which is
    /// the shipped posture. Idempotent, so a second `init()` in a UI-test process neither reopens
    /// the store nor reinstalls the sink.
    /// Reads the administrator-provisioned credential, verifies it against the issuer key fixed at
    /// build time, and installs the three collaborators
    /// `AeroraCommandAuthorizer::isFullyConfigured()` requires: the identity provider, the role
    /// resolver and the role policy. After this, and only after it, the command gate evaluates.
    ///
    /// Every failure path leaves the chain uninstalled and warns: no issuer key configured in the
    /// build, no credential file present, a credential that does not verify, or one whose contents
    /// are incomplete. So the station either enforces with a credential the issuer signed, or
    /// enforces nothing and says which - it never enforces against something it could not verify.
    ///
    /// Independent of `disableAllPersistence` and of whether the store opened, because reading a
    /// credential records nothing: turning off the audit trail must not turn off authorization.
    void _installAuthorizationChain();

    void _installPersistence();

    /// The teardown `cleanup()` runs: clears the authorizer's sink reference, stops the audit
    /// writer (which flushes the queue and closes its connection), closes the vehicle store, ends
    /// the session and closes the store - in the one order that keeps the store alive until after
    /// both writers and the authorizer's reference are gone. Null-safe, so it is a no-op when
    /// nothing was opened.
    void _teardownPersistence();

    /// Decodes an `AUTOPILOT_VERSION` \a message for \a vehicle, resolves its board identifier
    /// through the vehicle store, and reports the result into \a vehicle's `AeroraVehicleIdentity`.
    /// A no-op when the vehicle store is not open, \a vehicle carries no attached Aerora services
    /// yet, or the message carries no board identifier at all (see boardUidFromAutopilotVersion()).
    void _resolveFleetIdentity(Vehicle* vehicle, const mavlink_message_t& message);

    bool _warnedUnconfigured = false;

    /// The durable persistence path, owned here for the process lifetime. The store must outlive
    /// both writers: the audit writer's sink and the vehicle store each hold it by reference. The
    /// audit writer is shared because the authorizer holds it as an `AeroraAuditSink`. All three
    /// are null until `_installPersistence()` succeeds and again after `_teardownPersistence()`.
    std::unique_ptr<AeroraLocalStore> _store;
    std::unique_ptr<AeroraVehicleStore> _vehicleStore;
    std::shared_ptr<AeroraAuditWriter> _auditWriter;
    qint64 _sessionId = -1;
};

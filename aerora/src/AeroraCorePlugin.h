#pragma once

#include "QGCCorePlugin.h"

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

private:
    void _installSigningKeyStore();

    bool _warnedUnconfigured = false;
};

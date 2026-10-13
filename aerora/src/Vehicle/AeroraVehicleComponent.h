#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

#include "AutoPilotPlugin.h"
#include "VehicleComponent.h"

class Vehicle;

/// One Aerora vehicle setup page, and the catalogue of pages Aerora declares.
///
/// `VehicleComponent` is QGroundControl's setup-page descriptor: the Vehicle Configuration view
/// builds its tree from `AutoPilotPlugin::vehicleComponents()` and loads each entry's
/// `setupSource()` into a panel. Aerora's four publishable pages differ only in which service they
/// read and which QML renders them, so they are one class driven by a descriptor rather than four
/// near-identical subclasses.
///
/// ### Every page is read-only, and that is why requiresSetup() is false
///
/// An Aerora page displays identity, reported capability, reported health or service attachment.
/// None of them is a configuration step an operator can complete, so `requiresSetup()` returns
/// false and `setupComplete()` returns true. Returning true from `requiresSetup()` would put a
/// "setup required" badge on a page with no action on it, and would make
/// `AutoPilotPlugin::parametersReadyPreChecks()` raise "Configuration tasks remain before this
/// vehicle is ready to fly" for a vehicle that has no outstanding task.
///
/// `setupCompleteChangedTriggerList()` is empty for the same reason, and because no Aerora
/// parameter name is evidenced — a trigger list is parameter names, and inventing one would be
/// inventing a parameter dialect.
///
/// ### A page publishes only when its QML actually exists
///
/// A descriptor names resources; it does not create them. The Aerora QML for these pages is
/// Tab 7's to write, and until it exists a published page would load a panel that renders nothing:
/// `VehicleConfigView.qml:209` calls `panelLoader.setSource(vehicleComponent.setupSource, ...)`
/// with no existence check of its own.
///
/// So resolution is gated on the resource being present. isPublishable() is the predicate,
/// setupSource() returns an empty `QUrl` for an absent resource, and
/// AeroraAutoPilotPlugin::vehicleComponents() omits the page entirely. The honest result today is
/// **no Aerora setup page**, and a page appears with no C++ change once its QML lands.
///
/// This is the same declared-intent-plus-existence shape as `AeroraQmlOverrides::verify()`: the
/// catalogue is the declaration of intent, and unresolvedPageResources() names what is declared
/// but absent, so a missing page is reportable rather than silent.
///
/// ### What is deliberately not in the catalogue
///
/// Calibration (`SCR-VEH-05`) is withheld, not forgotten. See withheldPageReason().
class AeroraVehicleComponent : public VehicleComponent
{
    Q_OBJECT

public:
    /// The Aerora vehicle setup screens. `Calibration` is declared here and deliberately absent
    /// from pageDescriptors(), so the omission is a value with a reason attached rather than a
    /// gap in a list.
    enum class Page {
        Identity,           ///< `SCR-VEH-02` — MAVLink identity, fleet identity, product profile
        Capabilities,       ///< `SCR-VEH-03` — the reported capability inventory
        SensorHealth,       ///< `SCR-VEH-04` — reported sensor and health state
        Calibration,        ///< `SCR-VEH-05` — withheld, see withheldPageReason()
        ServiceDiagnostics, ///< `SCR-VEH-06` — which Aerora services attached to this vehicle
    };
    Q_ENUM(Page)

    /// What a page is, and which resources it needs in order to be publishable.
    ///
    /// Resource paths are `:/`-rooted Qt resource paths rather than `qrc:` URLs, so existence and
    /// the loadable URL are derived from one field and cannot drift apart.
    struct PageDescriptor {
        Page page = Page::Identity;
        /// Screen id from the product definition, for diagnostics. Not shown to an operator.
        QString screenId;
        /// The setup panel QML. Required: a page with no panel has nothing to show.
        QString setupResource;
        /// Optional summary tile. Empty is supported upstream —
        /// `VehicleSummary.qml:71` hides a tile whose `summaryQmlSource` is empty.
        QString summaryResource;
        /// Optional `*.VehicleConfig.json` page definition. Empty today; see vehicleConfigJson().
        QString configJsonResource;
        /// Optional tree icon. Empty renders no icon — `VehicleConfigView.qml:432` binds
        /// `icon.source` directly, so an empty string is the no-icon case rather than a broken one.
        QString iconResource;
    };

    /// Resource root of the overlay's QML module, derived from `AERORA_UI_QML_MODULE_URI`
    /// (`Aerora.Controls`) and its `RESOURCE_PREFIX /qml` in `aerora/src/UI/Ui.cmake:133`.
    ///
    /// This is the only Aerora QML module in the build, so it is where an Aerora QML file lands
    /// today. If Tab 7 introduces a separate setup module the paths below change with it; the
    /// existence gate means a stale path fails closed, hiding the page, rather than publishing a
    /// panel that cannot load.
    static constexpr const char *kQmlModuleResourceRoot = ":/qml/Aerora/Controls";

    /// The complete set of Aerora setup pages, in display order.
    ///
    /// Declaring a page here is a statement of intent, not a claim that it works. Publication is
    /// isPublishable()'s decision.
    [[nodiscard]] static QList<PageDescriptor> pageDescriptors();

    /// @return Why @a page is withheld from pageDescriptors(), or an empty string when it is not
    ///         withheld.
    ///
    /// `Page::Calibration` is withheld. Calibration is the one `SCR-VEH-*` screen that acts on the
    /// aircraft, and its authorization gate (ADR-015) currently permits everything because the
    /// operator role set is undecided (`Q-029`). An Aerora calibration page would therefore
    /// present a gated operation whose gate evaluates nothing. Upstream calibration pages remain
    /// reachable through their own firmware plugins; Aerora adds none.
    [[nodiscard]] static QString withheldPageReason(Page page);

    /// @return The loadable `qrc:` URL for @a resourcePath, or an empty `QUrl` when
    ///         @a resourcePath is empty, is not a `:/`-rooted resource path, or does not exist.
    [[nodiscard]] static QUrl resolvedQmlUrl(const QString &resourcePath);

    /// @return true when every resource @a descriptor declares is present, so the page can be
    ///         published without producing a panel that fails to load.
    ///
    /// A declared-but-absent `configJsonResource` fails the check too: the base class opens that
    /// path with `QFile` and warns per page when it cannot
    /// (`VehicleComponent::_ensureSectionsCached()`).
    [[nodiscard]] static bool isPublishable(const PageDescriptor &descriptor);

    /// @return Resource paths that pageDescriptors() declares and the build does not contain, in
    ///         declaration order.
    ///
    /// The diagnostic that makes an absent page visible. Non-empty is the expected state while the
    /// Aerora setup QML is unwritten.
    [[nodiscard]] static QStringList unresolvedPageResources();

    /// Creates the component for @a descriptor.
    ///
    /// `AutoPilotPlugin::UnknownVehicleComponent` because no Aerora page is one of QGroundControl's
    /// known components. Claiming a known kind would make
    /// `AutoPilotPlugin::knownVehicleComponentAvailable()` report an Aerora page as, say, the
    /// vehicle's Safety or Power setup component.
    AeroraVehicleComponent(const PageDescriptor &descriptor,
                           Vehicle *vehicle,
                           AutoPilotPlugin *autopilot,
                           QObject *parent = nullptr);
    ~AeroraVehicleComponent() override;

    [[nodiscard]] Page page() const { return _descriptor.page; }

    /// Product-definition screen id, for diagnostics and tests.
    [[nodiscard]] QString screenId() const { return _descriptor.screenId; }

    QString name() const override;
    QString description() const override;
    QString iconResource() const override { return _descriptor.iconResource; }

    bool requiresSetup() const override { return false; }
    bool setupComplete() const override { return true; }

    QUrl setupSource() const override { return resolvedQmlUrl(_descriptor.setupResource); }
    QUrl summaryQmlSource() const override { return resolvedQmlUrl(_descriptor.summaryResource); }

    /// Resource path of this page's `*.VehicleConfig.json`, or an empty string when it has none.
    ///
    /// The JSON-driven path is the right mechanism for `SCR-VEH-04`: the base class then derives
    /// section ids, `repeat` expansion probed against live parameters, a search keyword index and
    /// per-section completeness, which is substantially less QML than a hand-written page.
    ///
    /// It is nonetheless **empty in this build**, and the seam rather than the content is what is
    /// implemented here. Two things are missing and neither is this layer's to supply:
    ///
    /// - A page definition's `param` fields name real vehicle parameters. No Aerora parameter is
    ///   evidenced, so a JSON written now would carry invented parameter names.
    /// - The JSON only becomes a page through a build-time generator
    ///   (`tools/generators/config_qml/page_generator.py`) plus a `_config_generated_qml_names`
    ///   entry in the owning CMake target. That wiring is build infrastructure, not vehicle code.
    ///
    /// Populating `configJsonResource` is therefore a descriptor change once a parameter set is
    /// approved and the generator is wired, with no change to this class.
    QString vehicleConfigJson() const override { return _descriptor.configJsonResource; }

    /// Empty: a trigger list is vehicle parameter names, and no Aerora parameter is evidenced.
    /// Every Aerora page is read-only, so there is no completeness to re-evaluate either.
    QStringList setupCompleteChangedTriggerList() const override { return QStringList(); }

private:
    const PageDescriptor _descriptor;
};

#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantList>

#include "AeroraVehicleCapabilities.h"

/// The Aerora per-vehicle toolbar indicator set, and the gate that decides membership.
///
/// `FirmwarePlugin::toolIndicators(const Vehicle *)` (`FirmwarePlugin.h:344`) is the **only**
/// indicator registry in QGroundControl that is per-vehicle and genuinely dynamic: it carries a
/// `toolIndicatorsChanged()` signal, where `QGCCorePlugin::toolBarIndicators` is `CONSTANT` and
/// never re-read after first bind. Anything whose membership depends on what a vehicle reported
/// therefore has to be here.
///
/// ### Membership is a different question from visibility
///
/// Each indicator QML root declares `property bool showIndicator`, which
/// `FlyViewToolBarIndicators.qml` binds to `visible` with no null guard. That property answers
/// "is there something to show right now" from live state.
///
/// This class answers a different question: "does this vehicle have the capability this indicator
/// reports on at all". A capability that was never reported must not produce an indicator that an
/// operator can see sitting at `UNKNOWN` and read as a capability the aircraft has. So membership
/// is decided here, in C++, against the capability inventory, and `showIndicator` still decides
/// visibility inside the component. The two are not duplicates.
///
/// ### The gate never reads presence from the existence of an object
///
/// isPublishable() requires `AeroraVehicleCapabilities::isAvailable()`, which is true only for
/// `State::Available`. `State::NotEvidenced` and `State::Unavailable` are both false, and a null
/// inventory is false as well: an attached-but-silent capabilities object is not evidence, and the
/// absence of one is not permission.
///
/// ### This is presentation only
///
/// Every entry is a QML URL for a read-only toolbar component. Nothing here actuates, switches
/// power, triggers a payload, or authorizes a command, and no entry may be added that does. An
/// indicator that needs an action is not an indicator.
///
/// ### The declared set is empty in this build
///
/// See declaredIndicators(). The gate and the per-vehicle plumbing are implemented and tested; the
/// content they would carry does not exist yet, and neither does the evidence that would let it be
/// written.
class AeroraToolIndicators
{
public:
    /// One declared indicator: the QML to load, and the capability that must be reported available
    /// before it is offered.
    struct IndicatorDescriptor {
        /// Screen id from the product definition, for diagnostics. Not shown to an operator.
        QString screenId;
        /// `:/`-rooted Qt resource path of the indicator QML. Resource path rather than `qrc:` URL
        /// so existence and the loadable URL come from one field and cannot drift apart.
        QString qmlResource;
        /// Capability category checked when @ref capabilityKey is set.
        AeroraVehicleCapabilities::Category capabilityCategory = AeroraVehicleCapabilities::Category::Vehicle;
        /// Capability that must be reported available. **Empty means unconditional** — the
        /// indicator reports on something that needs no capability, such as service attachment.
        QString capabilityKey;
    };

    /// The complete set of Aerora per-vehicle indicators. **Empty in this build.**
    ///
    /// Two independent things are missing, and inventing either is prohibited:
    ///
    /// - No Aerora indicator QML exists. `aerora/src/UI/Controls/` holds four generic controls and
    ///   no toolbar component, and that file is Tab 7's to write.
    /// - No Aerora capability identifier is evidenced (`Q3-009`). A capability-gated indicator
    ///   needs a key to gate on, and a key written here would be an invented capability
    ///   (`P7-NOINV-001`).
    ///
    /// Empty is therefore the correct state rather than an unfinished one, and it is what makes
    /// `toolIndicators()` return the base list unchanged today. Adding an entry is a change to
    /// this function alone.
    [[nodiscard]] static QList<IndicatorDescriptor> declaredIndicators();

    /// @return true when @a descriptor's QML exists in the build **and** its capability
    ///         requirement, if any, is reported available by @a capabilities.
    ///
    /// @a capabilities may be null, which is false for any descriptor carrying a capability
    /// requirement.
    ///
    /// The existence half is not cosmetic: `FlyViewToolBarIndicators.qml` loads each URL and binds
    /// `visible: item.showIndicator` with no null guard, so a URL with no resource behind it
    /// produces a binding error on a null `item` rather than an absent indicator.
    [[nodiscard]] static bool isPublishable(const IndicatorDescriptor &descriptor,
                                            const AeroraVehicleCapabilities *capabilities);

    /// @return Loadable `qrc:` URLs for the publishable members of @a descriptors, in declaration
    ///         order, as `QVariant`s ready to append to a `toolIndicators()` list.
    ///
    /// Takes the descriptor list explicitly so the gate is exercisable against a constructed set
    /// rather than only against the declared one, which is empty.
    [[nodiscard]] static QVariantList urlsFor(const QList<IndicatorDescriptor> &descriptors,
                                              const AeroraVehicleCapabilities *capabilities);

    /// @return Loadable URLs for the publishable members of declaredIndicators().
    [[nodiscard]] static QVariantList urlsFor(const AeroraVehicleCapabilities *capabilities);

    /// @return Resource paths that declaredIndicators() declares and the build does not contain.
    ///
    /// The diagnostic that keeps a declared-but-absent indicator from being indistinguishable from
    /// one that was never declared.
    [[nodiscard]] static QStringList unresolvedIndicatorResources();
};

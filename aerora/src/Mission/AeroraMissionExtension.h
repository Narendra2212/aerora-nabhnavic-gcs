#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "QGCMAVLink.h"

class ComplexMissionItem;
class PlanMasterController;

/// How a product family contributes mission content without the generic mission layer knowing it
/// exists.
///
/// The Aerora GCS has to serve several product families — painting, firefighting, agriculture,
/// inspection — and the mission architecture must not acquire a conditional per family. Expressing
/// the families as a branch inside shared mission code would make every new family a change to
/// shared code and would put painting behaviour on the path every other product runs. So the
/// generic layer knows only this interface, and a product's mission content is a new file.
///
/// There is deliberately **no enumeration of product families** in this header. Adding one would
/// reintroduce the conditional the interface exists to avoid.
///
/// \section nothing No extension is implemented
///
/// No Aerora mission extension exists in this build, and that is correct rather than incomplete:
///
/// \li Which complex items Aerora needs is `OPEN` (Q-021), and the painting pattern's geometry
///     and parameters depend on product definition and Q-023.
/// \li The Aerora mission command set is `OPEN / NOT-EVIDENCED` (Q-023). No Aerora dialect
///     exists, so there is no command identifier to declare. **No numeric command id is invented
///     here**, nor is a placeholder one.
///
/// The interface and the registry are therefore `CONTRACT-ONLY`: the seam exists, the content
/// does not.
///
/// \section flatten Flattening is one-way
///
/// A complex item is a plan-file construct. It flattens to primitive mission items once, on
/// upload, through `VisualMissionItem::appendMissionItems()`, and it is **not** reconstructed from
/// a downloaded mission. Only landing patterns have a scanner upstream; survey-style items
/// round-trip through the plan file alone. An Aerora scanner is rejected by decision: a flat item
/// sequence that happens to match a pattern is not evidence that the pattern is what the operator
/// planned (`AERORA-MISSION-REQ-002`).
class AeroraMissionExtension
{
public:
    virtual ~AeroraMissionExtension() = default;

    /// Opaque, build-supplied extension identifier, used for registration and diagnostics.
    ///
    /// Not a protocol value. It must not be transmitted, parsed, or matched against a MAVLink
    /// field.
    [[nodiscard]] virtual QString extensionId() const = 0;

    /// Canonical names of the complex mission items this extension provides.
    ///
    /// These are the keys `QGCCorePlugin::complexMissionItemNames()` and
    /// `createComplexMissionItem()` exchange, and they must match the item's
    /// `jsonComplexItemTypeValue` so a saved plan loads back. An empty list means this extension
    /// provides none.
    [[nodiscard]] virtual QStringList complexMissionItemNames() const = 0;

    /// Creates the complex item registered under @a canonicalName.
    ///
    /// @return The new item, or nullptr when @a canonicalName is not one of
    ///         complexMissionItemNames(). Returning nullptr for an unknown name is required, not
    ///         optional: the registry uses it to keep one extension from answering for another.
    [[nodiscard]] virtual ComplexMissionItem *createComplexMissionItem(const QString &canonicalName,
                                                                       PlanMasterController *masterController,
                                                                       bool flyView) = 0;

    /// The mission commands this extension's product is known to support.
    ///
    /// An empty list means **this extension declares no command set** — it does not mean "all
    /// commands". See AeroraMissionExtensionRegistry::isMissionCommandSetDeclared() for why the
    /// distinction is load-bearing.
    [[nodiscard]] virtual QList<MAV_CMD> missionCommandSet() const = 0;
};

/// Build-time registry of the mission extensions an Aerora build carries.
///
/// Registration is explicit rather than by static self-registration, matching the product profile
/// registry: a set assembled by static-initialisation order would be unordered and therefore
/// unreviewable, and any resolution ambiguity would depend on link order — a defect that
/// reproduces on one machine out of five.
///
/// \section ambiguity Ambiguity is refused at registration
///
/// Two extensions claiming the same complex item name is a build configuration defect. It is
/// rejected when the second one registers, rather than resolved later by registration order, so
/// the defect surfaces at startup instead of deciding which product's item an operator gets.
class AeroraMissionExtensionRegistry
{
public:
    static AeroraMissionExtensionRegistry *instance();

    /// Registers @a extension.
    ///
    /// Rejects a null extension, an empty extension id, a duplicate id, an empty complex item
    /// name, and any complex item name already claimed by another extension.
    ///
    /// @return true if the extension was registered.
    bool registerExtension(AeroraMissionExtension *extension);

    /// @return The registered extension ids, in registration order.
    [[nodiscard]] QStringList extensionIds() const;

    /// @return Every complex mission item name across all extensions, in registration order.
    ///         Empty when no extension provides any.
    [[nodiscard]] QStringList complexMissionItemNames() const;

    /// Creates the complex item registered under @a canonicalName.
    ///
    /// @return The new item, or nullptr when no extension claims the name. Name collisions cannot
    ///         occur because registerExtension() refuses them.
    [[nodiscard]] ComplexMissionItem *createComplexMissionItem(const QString &canonicalName,
                                                               PlanMasterController *masterController,
                                                               bool flyView);

    /// Whether any extension has declared a mission command set.
    ///
    /// This exists because of a trap in the seam it feeds: `FirmwarePlugin::supportedMissionCommands()`
    /// treats an **empty** returned list as "every command is supported", which would offer the
    /// operator commands the firmware may not implement. `AERORA-MISSION-REQ-005` requires Aerora
    /// to declare its set explicitly rather than rely on that default.
    ///
    /// So a caller must check this before using declaredMissionCommandSet(), and must **not** pass
    /// an empty set through to `supportedMissionCommands()`. While this is false — which it is in
    /// this build, because the Aerora command set is `OPEN / NOT-EVIDENCED` under Q-023 — the
    /// firmware plugin must leave upstream's behaviour in place rather than ship a guessed list.
    [[nodiscard]] bool isMissionCommandSetDeclared() const;

    /// @return The union of every extension's declared command set, in registration order with
    ///         duplicates removed. Empty when isMissionCommandSetDeclared() is false.
    [[nodiscard]] QList<MAV_CMD> declaredMissionCommandSet() const;

#ifdef QGC_UNITTEST_BUILD
    /// Test support: drops every registration so a test starts from a known empty registry.
    void clear();
#endif

private:
    QList<AeroraMissionExtension *> _extensions;
};

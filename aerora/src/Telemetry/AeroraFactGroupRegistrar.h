#pragma once

#include <QtCore/QString>

class FactGroup;

/// \brief The seam through which Aerora fact groups reach a vehicle's fact-group map.
///
/// This interface exists because of where the privilege lives, not because the indirection is
/// interesting. `FactGroup::_addFactGroup()` is `protected`, so only a friend of `Vehicle` can
/// register a group on it. Upstream grants that friendship to exactly two classes, each with the
/// same one-line comment:
///
/// \code
/// friend class FactGroupListModel;   // Allow call _addFactGroup
/// friend class GimbalController;     // Allow GimbalController to call _addFactGroup
/// \endcode
/// (`src/Vehicle/Vehicle.h:108, 115`)
///
/// `M-Telemetry` does not hold that privilege and must not grant it to itself. So the module
/// defines *what* registration means and leaves *who may perform it* to the single authorised
/// implementation, which is a QGroundControl core edit under Tab 1's authority (Q4-002).
///
/// \section why Why an interface rather than a direct call
///
/// Three properties follow from putting the boundary here:
///
/// \li The telemetry module compiles, links and is fully testable **today**, with no core edit and
///     no dead `#if` branch. The contract is exercised against a test registrar.
/// \li The authorised core edit shrinks to one friend declaration plus a short concrete registrar.
///     Nothing else in the module changes when it lands.
/// \li If Tab 1 instead pursues the upstream fix — a public per-vehicle registration API, or
///     `factGroups(Vehicle *)` — only the concrete registrar changes. The telemetry side is already
///     written against the abstraction.
///
/// \section notplugin What this is deliberately not
///
/// It is **not** `FirmwarePlugin::factGroups()`. That seam returns a map with no `Vehicle *`
/// argument from a plugin instance `FirmwarePluginManager` caches per firmware class, so groups
/// returned from it are shared by every vehicle of that class and the aircraft overwrite each
/// other's telemetry. A registrar is handed a vehicle by the caller that already has one, so the
/// sharing hazard is structurally absent rather than avoided by discipline.
///
/// \section ordering When registration must happen
///
/// During vehicle construction, which in practice means from
/// `FirmwarePlugin::initializeVehicle()`. That is called at `src/Vehicle/Vehicle.cc:161`, inside the
/// `Vehicle` constructor and before the CSV log timer is started at `:170`. Registering there is
/// therefore ahead of the telemetry CSV header, which is written once on first `_writeCsvLine()`
/// after arming — so no column shifts and R-027 holds.
///
/// Implementations must not retain the groups: `FactGroup::_addFactGroup()` stores a raw pointer and
/// does not reparent, so ownership stays with the `AeroraTelemetrySet` that created them.
class AeroraFactGroupRegistrar
{
public:
    virtual ~AeroraFactGroupRegistrar();

    AeroraFactGroupRegistrar(const AeroraFactGroupRegistrar &) = delete;
    AeroraFactGroupRegistrar &operator=(const AeroraFactGroupRegistrar &) = delete;

    /// Registers @a factGroup under @a name on whichever vehicle this registrar was built for.
    ///
    /// \param name      The name the vehicle will expose the group as. Conventionally prefixed
    ///                  `aerora` so a consumer can tell Aerora groups from upstream ones.
    /// \param factGroup The group. Borrowed, never owned, and must not be retained beyond the call.
    ///
    /// \return True when the group was registered. False is a real failure — a duplicate name, or
    ///         no vehicle — and the caller reports it rather than continuing as though the group
    ///         were present.
    [[nodiscard]] virtual bool registerFactGroup(const QString &name, FactGroup *factGroup) = 0;

protected:
    AeroraFactGroupRegistrar() = default;
};

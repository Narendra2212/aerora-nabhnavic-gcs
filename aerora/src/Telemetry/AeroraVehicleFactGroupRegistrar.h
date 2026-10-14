#pragma once

#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "AeroraFactGroupRegistrar.h"

class Vehicle;

/// \brief The one registrar that puts Aerora fact groups into a specific vehicle's fact-group map.
///
/// Holds the vehicle it was built for and nothing else. There is no static vehicle pointer, no
/// cached instance and no fact-group storage here — the groups stay owned by the
/// \c AeroraTelemetrySet that created them, and \c FactGroup::_addFactGroup() does not reparent, so
/// registration transfers no ownership.
///
/// This is the Q4-002 decision made concrete. See \c README.md for the decision record and
/// \c AeroraFactGroupRegistrar for why the boundary is an interface.
///
/// \section privilege The one privileged statement, and the build flag that gates it
///
/// `FactGroup::_addFactGroup()` is `protected`. Calling it on a `Vehicle` requires friendship, which
/// upstream grants to exactly two classes for exactly this purpose
/// (`src/Vehicle/Vehicle.h:108, 115`). Aerora does not hold it yet.
///
/// So the single privileged statement is compiled only when \c AERORA_VEHICLE_FACTGROUP_FRIENDSHIP
/// is defined. Without it this class validates, refuses, and says why — which is the honest
/// behaviour for a build whose core edit has not been applied, and it keeps the telemetry module
/// compiling and testable in the meantime.
///
/// **The two halves must be applied together.** Adding the friend declaration without defining the
/// flag leaves this class inert; defining the flag without the friend declaration is a compile
/// error, which is the safe direction. \c isRegistrationAvailable() exists so a startup self-check
/// can detect the inert case rather than discovering it as missing telemetry in flight.
///
/// \section notplugin What this is not
///
/// It is not `FirmwarePlugin::factGroups()` and must not be reached from it. That seam takes no
/// `Vehicle *` and the plugin behind it is cached per firmware class, so groups returned through it
/// are shared by every vehicle of that class. A registrar is handed one vehicle by a caller that
/// already has it, which is why cross-vehicle sharing is structurally absent here rather than
/// avoided by convention.
class AeroraVehicleFactGroupRegistrar final : public AeroraFactGroupRegistrar
{
public:
    /// \param vehicle The vehicle to register into. Held through a \c QPointer and never
    ///                dereferenced once null, so a vehicle destroyed mid-composition produces a
    ///                refusal rather than a crash. May be null, which is the unit-test path.
    explicit AeroraVehicleFactGroupRegistrar(Vehicle *vehicle);
    ~AeroraVehicleFactGroupRegistrar() override;

    /// True when this build can actually register a fact group — that is, when the Q4-002 core edit
    /// has been applied and \c AERORA_VEHICLE_FACTGROUP_FRIENDSHIP is defined.
    ///
    /// False means every call to \c registerFactGroup() will refuse. Nothing is silently dropped,
    /// but a consumer expecting Aerora values in the value picker or the CSV record will not find
    /// them, so the startup self-check should surface this.
    [[nodiscard]] static bool isRegistrationAvailable();

    /// \return The vehicle, or nullptr if it was never supplied or has been destroyed.
    ///
    /// Defined out of line so this header keeps \c Vehicle an opaque handle. The conversion from
    /// \c QPointer<Vehicle> instantiates \c static_cast<Vehicle *>(QObject *), which needs the
    /// complete type; inline, that would force every including translation unit to pull in
    /// \c Vehicle.h for an accessor this class never dereferences.
    [[nodiscard]] Vehicle *vehicle() const;

    /// Names accepted into the vehicle's map, in the order they were accepted.
    [[nodiscard]] QStringList registeredNames() const { return _registeredNames; }

    /// Names that were refused, in order. A refusal is always recorded and always warned, so a group
    /// that never reached the vehicle cannot be mistaken for one that did.
    [[nodiscard]] QStringList refusedNames() const { return _refusedNames; }

    /// Registers @a factGroup under @a name on this registrar's vehicle.
    ///
    /// The name and the object are passed through unchanged: the group the vehicle ends up holding
    /// is the one the \c AeroraTelemetrySet owns, under the name it registered.
    ///
    /// Refuses, records and warns when the name is empty, the group is null, the vehicle is absent,
    /// the vehicle already has a group under that name, or this build lacks the core edit.
    [[nodiscard]] bool registerFactGroup(const QString &name, FactGroup *factGroup) override;

private:
    bool _refuse(const QString &name, const char *reason);

    QPointer<Vehicle> _vehicle;
    QStringList _registeredNames;
    QStringList _refusedNames;
};

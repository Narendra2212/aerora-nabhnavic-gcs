#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

class AeroraVehicleCapabilities;
class AeroraVehicleServices;

/// How a product family attaches Aerora behaviour to a vehicle.
///
/// The Aerora GCS is intended to serve several product families. Expressing them as a conditional
/// in the vehicle layer would make every new family a change to shared code, so the vehicle layer
/// knows only about this interface and resolves a profile from the reported capability inventory.
///
/// No product family is implemented here, and no product-family identifier is evidenced. There is
/// deliberately no enumeration of families in this file: adding one would reintroduce the
/// conditional it exists to avoid.
class AeroraProductProfile
{
public:
    virtual ~AeroraProductProfile() = default;

    /// Opaque, build-supplied profile identifier, used for logging and diagnostics.
    ///
    /// This is not a protocol value and must not be transmitted, parsed or matched against a
    /// MAVLink field.
    virtual QString profileId() const = 0;

    /// @return true when @a capabilities identifies this profile's product.
    ///
    /// A profile that cannot decide from the inventory must return false. The vehicle layer never
    /// guesses a profile, so declining is always the correct answer under uncertainty.
    virtual bool matchesCapabilities(const AeroraVehicleCapabilities &capabilities) const = 0;

    /// Attaches the profile's own services to a vehicle. Called at most once per vehicle.
    ///
    /// A profile attaches through AeroraVehicleServices::attachService() and must not reach into
    /// the QGroundControl `Vehicle` directly.
    virtual void attachToVehicle(AeroraVehicleServices &services) = 0;
};

/// Build-time registry of the product profiles an Aerora build carries.
///
/// Registration is explicit rather than by static self-registration. The firmware plugin factory
/// has no choice about that (QGroundControl's seam requires a self-registering static), but a
/// profile set assembled by static-initialisation order would be unordered and therefore
/// unreviewable, and resolution ambiguity would depend on link order.
class AeroraProductProfileRegistry
{
public:
    static AeroraProductProfileRegistry *instance();

    /// Registers @a profile. Rejects a null profile, an empty profile id, and a duplicate id.
    /// @return true if the profile was registered.
    bool registerProfile(AeroraProductProfile *profile);

    /// @return The registered profile ids, in registration order.
    QStringList profileIds() const;

    /// Resolves the single profile matching @a capabilities.
    ///
    /// @return The matching profile, or nullptr when none matches **and** when more than one
    ///         matches. An ambiguous profile set is a build configuration defect; resolving it by
    ///         registration order would make the outcome depend on link order and hide the defect.
    AeroraProductProfile *resolve(const AeroraVehicleCapabilities &capabilities) const;

#ifdef QGC_UNITTEST_BUILD
    /// Test support: drops every registration so a test starts from a known empty registry.
    void clear();
#endif

private:
    QList<AeroraProductProfile *> _profiles;
};

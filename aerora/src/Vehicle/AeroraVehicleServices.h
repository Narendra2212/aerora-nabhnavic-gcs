#pragma once

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtQmlIntegration/QtQmlIntegration>

class AeroraProductProfile;
class AeroraVehicleCapabilities;
class AeroraVehicleIdentity;
class Vehicle;

/// The per-vehicle Aerora attachment point.
///
/// One instance exists per QGroundControl `Vehicle`, created from
/// AeroraFirmwarePlugin::initializeVehicle() and parented to that `Vehicle`, so its lifetime is
/// the vehicle's lifetime with no teardown code of its own. Other Aerora workstreams reach it
/// with forVehicle() and hang their own objects off it with attachService().
///
/// It exists because the alternatives are worse. `Vehicle` is already a very large class and
/// cannot absorb Aerora state; `FirmwarePlugin` is a singleton shared by every vehicle of a
/// firmware class, so per-vehicle state must not live there either. This class is the only
/// per-vehicle, Aerora-owned object in the design.
///
/// ### What this class is not
///
/// - **Not flight authority.** It issues no command, requests no mode change, and initiates no
///   failsafe action. The flight controller retains local authority.
/// - **Not a telemetry state engine.** The REPORTED / EFFECTIVE / REQUESTED / PROPOSED / STALE /
///   UNKNOWN / UNAVAILABLE distinction belongs to the telemetry workstream. This class holds no
///   telemetry value and performs no state translation, so it cannot promote `PROPOSED` to
///   `EFFECTIVE` or treat `STALE` as current. The telemetry engine attaches through
///   attachService().
/// - **Not an identity authority.** See AeroraVehicleIdentity for the MAVLink / fleet boundary.
class AeroraVehicleServices : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Attached to a vehicle by the Aerora vehicle integration layer")

    Q_MOC_INCLUDE("AeroraVehicleCapabilities.h")
    Q_MOC_INCLUDE("AeroraVehicleIdentity.h")
    Q_MOC_INCLUDE("Vehicle.h")

public:
    /// How much is known about whether this vehicle is Aerora hardware.
    ///
    /// The distinction between SelectedBySeam and CorroboratedAerora is the point of this
    /// enumeration. QGroundControl selects a firmware plugin from the `MAV_AUTOPILOT` value in
    /// the heartbeat, and the value Aerora firmware reports is not evidenced. An Aerora build
    /// therefore makes plugin selection deterministic by *exclusion* — it disables the upstream
    /// factories so only Aerora's remains — which means selection says what this build is
    /// configured to do and says nothing about what the aircraft is.
    enum class Classification {
        Unknown,            ///< No classification evidence has been seen
        SelectedBySeam,     ///< QGroundControl selected the Aerora firmware plugin for this
                            ///  vehicle. A build-configuration outcome, **not** evidence that the
                            ///  aircraft is Aerora hardware
        CorroboratedAerora, ///< An Aerora evidence source corroborated Aerora hardware
        NotAerora,          ///< Positively identified as not Aerora hardware
    };
    Q_ENUM(Classification)

    Q_PROPERTY(Vehicle *vehicle READ vehicle CONSTANT)
    Q_PROPERTY(AeroraVehicleIdentity *identity READ identity CONSTANT)
    Q_PROPERTY(AeroraVehicleCapabilities *capabilities READ capabilities CONSTANT)
    Q_PROPERTY(Classification classification READ classification NOTIFY classificationChanged)
    Q_PROPERTY(QString productProfileId READ productProfileId NOTIFY productProfileChanged)

    /// Creates the services object for @a vehicle.
    ///
    /// @a vehicle may be null, which is the mock and unit-test path. Every member tolerates a
    /// null vehicle rather than assuming one, so a test does not need a MockLink connection to
    /// exercise the integration surface.
    explicit AeroraVehicleServices(Vehicle *vehicle, QObject *parent = nullptr);
    ~AeroraVehicleServices() override;

    /// Attaches a services object to @a vehicle, parented to it.
    ///
    /// Idempotent: a vehicle that already has one keeps it and that instance is returned, so a
    /// repeated initialize cannot produce two competing instances.
    ///
    /// @return The services object, or nullptr when @a vehicle is null.
    static AeroraVehicleServices *attachTo(Vehicle *vehicle);

    /// @return The services object attached to @a vehicle, or nullptr if there is none. This is
    ///         the supported entry point for other Aerora workstreams.
    static AeroraVehicleServices *forVehicle(Vehicle *vehicle);

    /// Read-only overload, for the QGroundControl seams that hand out a `const Vehicle *` —
    /// `FirmwarePlugin::toolIndicators()` is the first. The lookup is a `findChild`, so it mutates
    /// neither the vehicle nor the services object; the overload exists so a read-only caller does
    /// not have to cast the constness away to perform it.
    static const AeroraVehicleServices *forVehicle(const Vehicle *vehicle);

    /// @return The vehicle these services belong to, or nullptr on the mock and test path.
    ///         Callers must null-check before dereferencing.
    Vehicle *vehicle() const { return _vehicle; }

    AeroraVehicleIdentity *identity() const { return _identity; }
    AeroraVehicleCapabilities *capabilities() const { return _capabilities; }

    Classification classification() const { return _classification; }

    /// Records corroboration that this vehicle is Aerora hardware.
    ///
    /// Only an Aerora evidence source may call this. Selection of the Aerora firmware plugin is
    /// not corroboration, and neither is the presence of capability reports, so nothing in this
    /// class calls it. No such evidence source exists yet.
    void reportAeroraCorroboration();

    /// Records that this vehicle is positively not Aerora hardware.
    void reportNotAerora();

    /// @return The resolved product profile, or nullptr when none has been resolved.
    AeroraProductProfile *productProfile() const { return _productProfile; }

    /// @return The resolved product profile's id, or an empty string when none is resolved.
    QString productProfileId() const;

    /// Asks the product profile registry to resolve a profile from the reported capability
    /// inventory, and attaches it if exactly one matches.
    ///
    /// Resolution happens at most once per vehicle. Swapping a profile on a live vehicle is not a
    /// supported transition: a profile has already attached its services by then, and detaching
    /// them mid-connection would be a far larger change than this layer should make.
    void resolveProductProfile();

    /// Attaches a workstream-owned object under @a name, reparenting it to this object so it is
    /// destroyed with the vehicle.
    ///
    /// This is the single attachment mechanism for every Aerora domain — telemetry, mission,
    /// payload, power, link health and any future one. A per-domain accessor on this class would
    /// make the vehicle layer depend on every domain, which is the coupling the design avoids.
    ///
    /// @a service must live on this object's thread. Rejects an empty name, a null service, and a
    /// name that is already attached.
    ///
    /// @return true if the service was attached.
    bool attachService(const QString &name, QObject *service);

    /// @return The service attached under @a name, or nullptr when there is none.
    QObject *service(const QString &name) const;

    /// @return Attached service names, in attachment order.
    QStringList serviceNames() const;

signals:
    void classificationChanged(Classification classification);
    void productProfileChanged();
    void serviceAttached(const QString &name);

private slots:
    void _onCapabilityReportChanged();

private:
    void _setClassification(Classification classification);

    Vehicle *const _vehicle = nullptr;
    AeroraVehicleIdentity *const _identity = nullptr;
    AeroraVehicleCapabilities *const _capabilities = nullptr;
    AeroraProductProfile *_productProfile = nullptr;
    Classification _classification = Classification::Unknown;
    QStringList _serviceOrder;
    QMap<QString, QObject *> _services;
};

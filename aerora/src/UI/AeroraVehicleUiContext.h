#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtQmlIntegration/QtQmlIntegration>

#include "AeroraVehicleServices.h"

class AeroraMissionObserver;
class AeroraPayloadModel;
class AeroraPowerModel;
class AeroraVehicleCapabilities;
class AeroraVehicleIdentity;
class Vehicle;

/// \brief The QML entry point to the active vehicle's Aerora services: typed, read-only.
///
/// This is the closure of Q7-002. Before it, QML had no route to any Aerora per-vehicle service at
/// all: `AeroraVehicleServices::forVehicle()`, `service()` and `serviceNames()` are plain C++, and
/// `Vehicle` carries no property pointing at the services object — adding one would be a core edit.
///
/// \code
/// QML:  AeroraUi.payload / .power / .mission / .identity / .capabilities
///          │
///          ▼  this class, in C++
///       MultiVehicleManager::activeVehicle()
///          │
///          ▼  AeroraVehicleServices::forVehicle()      public static, already available
///       AeroraVehicleServices
///          │
///          ▼  service(name) + qobject_cast             kept in C++, never exposed
///       AeroraPayloadModel / AeroraPowerModel / AeroraMissionObserver
/// \endcode
///
/// **No change outside M-Ui is required.** Everything this class needs from the vehicle layer is
/// already public C++, so the Q7-002 handoff to Tab 3 is withdrawn rather than filed.
///
/// \section typed Why typed accessors and not a service map
///
/// `AeroraVehicleServices::service(QString)` is a string-keyed `QObject *` lookup. Exposing it to
/// QML would hand the interface a way to reach any service, including ones added later with no
/// review of their QML surface, and would make every call site a stringly-typed lookup that fails
/// silently by returning null. The service registry is deliberately generic so the vehicle layer
/// need not depend on every domain; a *consumer* has no such constraint, and the UI legitimately
/// depends on exactly the domains it displays.
///
/// So the lookup stays in C++ and QML sees one property per service, of the service's own type.
///
/// \section readonly Read-only, and why that is structural rather than stated
///
/// Every property here is `READ`-only with no `WRITE`. Beyond that, the services reached through
/// them expose no actuating path to QML, which was checked against each type's meta-object rather
/// than assumed — see `AeroraUiSafetyTest`:
///
/// \li `AeroraPayloadModel::submitRequest()` and `setCommandSink()` are **not** `Q_INVOKABLE`, so
///     QML cannot submit a payload request. `IF-PAY-015` is "None — prohibited".
/// \li `AeroraPowerModel` has no outbound method of any kind. No source switching, no threshold,
///     no failover, no emergency action.
/// \li `AeroraMissionObserver` has no command API. Its only `Q_INVOKABLE` is `verify()`, a local
///     comparison of two snapshots that sends nothing.
/// \li Every `report*()` / `clear*()` mutator on every reached type is plain C++.
///
/// The one exception found was recorded rather than worked around, and is now closed:
/// `AeroraStateValue::capability` carried a `WRITE setCapability`, so QML could have manufactured
/// an `UNAVAILABLE` the vehicle never reported. U3 removed the `WRITE` (handoff HO-TAB7-01);
/// `setCapability()` stays public C++ for the capability model. `AeroraUiSafetyTest` now requires
/// the writable-property set across every QML-registered Aerora type to be **empty**, so the
/// ratchet fails on the first writable property to appear rather than on the second.
///
/// \section telemetry Why there is no telemetry property
///
/// `AeroraTelemetrySet` carries no QML registration macro, so it cannot be a `Q_PROPERTY` type, and
/// there is nothing to show through it: all six Aerora fact groups are `CONTRACT-ONLY` for want of
/// a dialect, and `AeroraVehicleFactGroupRegistrar` cannot register a group until the Q4-002 core
/// edit is applied. Adding the property now would be shape with no substance behind it.
///
/// \section null Null is the normal case
///
/// Every accessor returns null when there is no active vehicle, when the active vehicle has no
/// Aerora services (the offline-editing vehicle is deliberately not composed), or when the service
/// is not attached. QML must null-check, exactly as it already does for `activeVehicle`. Services
/// are held through `QPointer`, so a vehicle destroyed before this object is told reads as null
/// rather than dangling.
class AeroraVehicleUiContext : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(AeroraUi)
    QML_SINGLETON

    Q_PROPERTY(bool available READ isAvailable NOTIFY servicesChanged)
    Q_PROPERTY(AeroraVehicleIdentity *identity READ identity NOTIFY servicesChanged)
    Q_PROPERTY(AeroraVehicleCapabilities *capabilities READ capabilities NOTIFY servicesChanged)
    Q_PROPERTY(AeroraPayloadModel *payload READ payload NOTIFY servicesChanged)
    Q_PROPERTY(AeroraPowerModel *power READ power NOTIFY servicesChanged)
    Q_PROPERTY(AeroraMissionObserver *mission READ mission NOTIFY servicesChanged)

    Q_PROPERTY(AeroraVehicleServices::Classification classification READ classification NOTIFY classificationChanged)
    Q_PROPERTY(QString productProfileId READ productProfileId NOTIFY productProfileChanged)

public:
    explicit AeroraVehicleUiContext(QObject *parent = nullptr);
    ~AeroraVehicleUiContext() override;

    /// True when the active vehicle has an Aerora services object attached.
    ///
    /// False is a normal, displayable state: no vehicle is connected, or the connected vehicle is
    /// not an Aerora composition. A consumer shows the stock QGroundControl interface, which is
    /// correct behaviour rather than degraded behaviour.
    [[nodiscard]] bool isAvailable() const { return !_services.isNull(); }

    [[nodiscard]] AeroraVehicleIdentity *identity() const;
    [[nodiscard]] AeroraVehicleCapabilities *capabilities() const;

    /// The active vehicle's payload observation model, or nullptr.
    ///
    /// Read-only in every sense that matters: the model exposes reported state and the
    /// `controlsPermitted` gate, and no actuating method of it is reachable from QML.
    [[nodiscard]] AeroraPayloadModel *payload() const;

    /// The active vehicle's power observation model, or nullptr. The model has no outbound path.
    [[nodiscard]] AeroraPowerModel *power() const;

    /// The active vehicle's mission observer, or nullptr. The observer has no command API.
    [[nodiscard]] AeroraMissionObserver *mission() const;

    /// How much is known about whether the active vehicle is Aerora hardware.
    ///
    /// `SelectedBySeam` means QGroundControl chose the Aerora firmware plugin, which is a
    /// build-configuration outcome and **not** evidence about the aircraft. A consumer must not
    /// present it as identification.
    [[nodiscard]] AeroraVehicleServices::Classification classification() const;

    /// The resolved product profile id, or an empty string when none is resolved.
    ///
    /// An **opaque** build-supplied string. A consumer selects product UI by matching it against a
    /// registry; it must not be parsed, and no product family may be named in shared Aerora UI.
    /// An empty id shows no product surface rather than a default one.
    [[nodiscard]] QString productProfileId() const;

signals:
    /// Emitted when the active vehicle changes, or its Aerora services appear or go away. Every
    /// service property may have changed.
    void servicesChanged();

    void classificationChanged();
    void productProfileChanged();

private slots:
    void _onActiveVehicleChanged(Vehicle *vehicle);

private:
    /// Rebinds to @a vehicle's services, relaying their signals. Emits servicesChanged() plus the
    /// two per-service signals, because a rebind can change all of them at once.
    void _rebind(Vehicle *vehicle);

    /// Looks up an attached service by its published name and casts it. Stays private: the
    /// string-keyed lookup is not part of the QML surface.
    [[nodiscard]] QObject *_service(const char *serviceName) const;

    QPointer<AeroraVehicleServices> _services;
};

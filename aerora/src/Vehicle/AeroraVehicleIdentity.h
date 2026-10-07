#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtQmlIntegration/QtQmlIntegration>

/// Holds the two vehicle identities Aerora must keep apart.
///
/// MAVLink system identity is a protocol-level addressing value. It is assigned per connection,
/// is not guaranteed stable across a vehicle's lifetime, and is not guaranteed unique across a
/// fleet. It is therefore usable for addressing a connected vehicle and for nothing that outlives
/// the connection.
///
/// Aerora fleet identity is a product-level identifier. Its format is **OPEN / NOT-EVIDENCED**:
/// no fleet-identifier format, serial-number format, UUID scheme or registration-number format is
/// evidenced, so this class stores an opaque token reported by an evidence source and parses
/// nothing.
///
/// The two are kept apart structurally, not by convention:
///
/// - There is no API that derives a fleet identity from a MAVLink identity.
/// - reportFleetIdentity() takes a `QString`, never an integer, so a system id cannot be passed
///   to it by accident.
/// - fleetIdentityKnown() is false until a source reports one, and fleetIdentity() is empty in
///   that state rather than falling back to the system id.
class AeroraVehicleIdentity : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Attached to a vehicle by the Aerora vehicle integration layer")

    Q_PROPERTY(int mavlinkSystemId READ mavlinkSystemId CONSTANT)
    Q_PROPERTY(int mavlinkComponentId READ mavlinkComponentId CONSTANT)
    Q_PROPERTY(bool fleetIdentityKnown READ fleetIdentityKnown NOTIFY fleetIdentityChanged)
    Q_PROPERTY(QString fleetIdentity READ fleetIdentity NOTIFY fleetIdentityChanged)

public:
    AeroraVehicleIdentity(int mavlinkSystemId,
                          int mavlinkComponentId,
                          QObject *parent = nullptr);
    ~AeroraVehicleIdentity() override;

    /// MAVLink system id of the vehicle this identity belongs to. Protocol-level addressing only.
    int mavlinkSystemId() const { return _mavlinkSystemId; }

    /// MAVLink component id used as the vehicle's default target.
    int mavlinkComponentId() const { return _mavlinkComponentId; }

    /// @return true once an evidence source has reported an Aerora fleet identity.
    bool fleetIdentityKnown() const { return !_fleetIdentity.isEmpty(); }

    /// @return The reported Aerora fleet identity, or an empty string when none is known. Never
    ///         falls back to the MAVLink system id.
    QString fleetIdentity() const { return _fleetIdentity; }

    /// Records an Aerora fleet identity reported by an evidence source.
    ///
    /// The token is stored verbatim and is not parsed, validated against a format, or compared
    /// with the MAVLink identity, because no fleet-identifier format is evidenced. An empty token
    /// is rejected: discarding an identity is clearFleetIdentity().
    ///
    /// @return true if the report was accepted.
    bool reportFleetIdentity(const QString &fleetIdentity);

    /// Discards the reported fleet identity, returning to the unknown state.
    void clearFleetIdentity();

signals:
    void fleetIdentityChanged();

private:
    const int _mavlinkSystemId;
    const int _mavlinkComponentId;
    QString _fleetIdentity;
};

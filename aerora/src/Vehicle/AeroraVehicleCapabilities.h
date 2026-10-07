#pragma once

#include <QtCore/QHash>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtQmlIntegration/QtQmlIntegration>

/// Per-vehicle Aerora capability inventory.
///
/// Models the five capability *categories* the Aerora architecture defines. It deliberately does
/// not model capability identifiers or capability values: no Aerora capability identifier is
/// evidenced, so a key is an opaque string supplied by whichever evidence source reports it. This
/// class never originates a report.
///
/// The three-state model is the reason this class exists. `NotEvidenced` is not `Unavailable`:
/// the first means nothing has been reported, the second means a source reported absence.
/// Callers needing a boolean must use isAvailable(), which is true only for `Available`, so an
/// unreported capability can never present as a usable one.
class AeroraVehicleCapabilities : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Attached to a vehicle by the Aerora vehicle integration layer")

public:
    explicit AeroraVehicleCapabilities(QObject *parent = nullptr);
    ~AeroraVehicleCapabilities() override;

    /// Aerora capability categories. These are categories, not protocol enumerators: no MAVLink
    /// value, bit position or wire encoding is implied by any of them.
    enum class Category {
        Vehicle,
        Payload,
        Mission,
        Telemetry,
        Command,
    };
    Q_ENUM(Category)

    /// Evidence state of one capability within a category.
    enum class State {
        NotEvidenced, ///< Nothing has been reported. The default, and never a capability
        Unavailable,  ///< An evidence source reported the capability is absent
        Available,    ///< An evidence source reported the capability is present
    };
    Q_ENUM(State)

    /// @return The reported state, or State::NotEvidenced when nothing has been reported.
    Q_INVOKABLE State state(Category category, const QString &key) const;

    /// @return true only when the capability has been reported present. State::NotEvidenced and
    ///         State::Unavailable are both false, so an unreported capability is never usable.
    Q_INVOKABLE bool isAvailable(Category category, const QString &key) const;

    /// @return Keys reported for @a category, in the order they were first reported.
    Q_INVOKABLE QStringList reportedKeys(Category category) const;

    /// @return true when at least one key has been reported in any category.
    bool hasAnyReport() const;

    /// Records a report from an evidence source.
    ///
    /// State::NotEvidenced is not reportable: a source either reports presence or absence, and
    /// discarding a report is clearReports(). Passing it is rejected rather than stored, because
    /// storing it would make "reported as unknown" indistinguishable from "never reported".
    ///
    /// @return true if the report was accepted.
    bool report(Category category, const QString &key, State reportedState);

    /// Discards every report so the inventory returns to State::NotEvidenced.
    ///
    /// Called on vehicle disconnect: a reconnect must start from no evidence rather than from
    /// evidence that belonged to the previous connection.
    void clearReports();

signals:
    void capabilityChanged(Category category, const QString &key, State state);
    void reportsCleared();

private:
    struct CategoryReports {
        QStringList keyOrder;
        QHash<QString, State> states;
    };

    QMap<Category, CategoryReports> _reports;
};

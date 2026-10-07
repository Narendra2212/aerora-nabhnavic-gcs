#include "AeroraVehicleCapabilities.h"

#include "AeroraVehicleLog.h"

AeroraVehicleCapabilities::AeroraVehicleCapabilities(QObject *parent)
    : QObject(parent)
{
}

AeroraVehicleCapabilities::~AeroraVehicleCapabilities() = default;

AeroraVehicleCapabilities::State AeroraVehicleCapabilities::state(Category category, const QString &key) const
{
    const auto categoryIt = _reports.constFind(category);
    if (categoryIt == _reports.constEnd()) {
        return State::NotEvidenced;
    }

    return categoryIt->states.value(key, State::NotEvidenced);
}

bool AeroraVehicleCapabilities::isAvailable(Category category, const QString &key) const
{
    return state(category, key) == State::Available;
}

QStringList AeroraVehicleCapabilities::reportedKeys(Category category) const
{
    return _reports.value(category).keyOrder;
}

bool AeroraVehicleCapabilities::hasAnyReport() const
{
    for (const CategoryReports &reports : _reports) {
        if (!reports.keyOrder.isEmpty()) {
            return true;
        }
    }

    return false;
}

bool AeroraVehicleCapabilities::report(Category category, const QString &key, State reportedState)
{
    if (key.isEmpty()) {
        qCWarning(AeroraVehicleLog) << "Capability report rejected, empty key"
                                    << "category:" << category;
        return false;
    }

    if (reportedState == State::NotEvidenced) {
        qCWarning(AeroraVehicleLog) << "Capability report rejected, NotEvidenced is not reportable"
                                    << "category:" << category
                                    << "key:" << key;
        return false;
    }

    CategoryReports &reports = _reports[category];
    const State previousState = reports.states.value(key, State::NotEvidenced);
    if (previousState == reportedState) {
        return true;
    }

    if (previousState == State::NotEvidenced) {
        reports.keyOrder.append(key);
    }
    reports.states[key] = reportedState;

    emit capabilityChanged(category, key, reportedState);

    return true;
}

void AeroraVehicleCapabilities::clearReports()
{
    if (!hasAnyReport()) {
        return;
    }

    _reports.clear();

    emit reportsCleared();
}

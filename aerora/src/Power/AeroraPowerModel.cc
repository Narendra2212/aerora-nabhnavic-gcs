#include "AeroraPowerModel.h"

#include "AeroraFreshness.h"
#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(AeroraPowerLog, "aerora.power")

namespace {

/// The freshness class governing every power item.
///
/// An opaque producer-declared name, matching the fact group name
/// 06-factgroup-implementation.md assigns to the power group, so when that group becomes
/// implementable its items inherit the same threshold configuration key.
///
/// **No threshold is registered for it anywhere in this build** — Q-036 is open and no power
/// message rate is evidenced — so every power item resolves \c ObservedFreshnessUnevaluated once
/// observed, and consumers must surface the age rather than imply a freshness verdict.
constexpr const char *kPowerFreshnessClass = "aeroraPower";

// Stable machine-readable item identifiers, one per 07-power-telemetry-icd.md row. For logs,
// records and lookup only: none is a protocol value, a fact name or an operator-facing label, and
// none is ever sent, parsed or matched against a MAVLink field.
constexpr const char *kItemTetherConfiguration = "powerTetherConfiguration";   // IF-PWR-001
constexpr const char *kItemBatteryConfiguration = "powerBatteryConfiguration"; // IF-PWR-002
constexpr const char *kItemEffectiveState = "powerEffectiveState";             // IF-PWR-003
constexpr const char *kItemReportedState = "powerReportedState";               // IF-PWR-003, 009
constexpr const char *kItemSourceTransition = "powerSourceTransition";         // IF-PWR-004
constexpr const char *kItemFault = "powerFault";                               // IF-PWR-007
constexpr const char *kItemWatchdogEvent = "powerWatchdogEvent";               // IF-PWR-011

} // namespace

AeroraPowerModel::AeroraPowerModel(AeroraFreshness *freshness,
                                   AeroraVehicleCapabilities *capabilities,
                                   QObject *parent)
    : QObject(parent)
    , _freshness(freshness)
    , _capabilities(capabilities)
{
    if (!freshness) {
        qCWarning(AeroraPowerLog) << "Constructed with no freshness evaluator;"
                                  << "power items will never acquire a freshness verdict";
    }

    _tetherConfiguration = _addReportedValue(QString::fromLatin1(kItemTetherConfiguration),
                                             AeroraTelemetryState::KindReported);
    _batteryConfiguration = _addReportedValue(QString::fromLatin1(kItemBatteryConfiguration),
                                              AeroraTelemetryState::KindReported);

    // The only KindEffective item in the module. Keeping it distinct from the reported item is
    // what makes "a stale report does not become the effective value" structural rather than a
    // review rule: the declared kind is const, so no call sequence promotes one to the other.
    _effectiveState = _addReportedValue(QString::fromLatin1(kItemEffectiveState),
                                        AeroraTelemetryState::KindEffective);
    _reportedState = _addReportedValue(QString::fromLatin1(kItemReportedState),
                                       AeroraTelemetryState::KindReported);

    _sourceTransition = _addReportedValue(QString::fromLatin1(kItemSourceTransition),
                                          AeroraTelemetryState::KindReported);
    _fault = _addReportedValue(QString::fromLatin1(kItemFault), AeroraTelemetryState::KindReported);
    _watchdogEvent = _addReportedValue(QString::fromLatin1(kItemWatchdogEvent),
                                       AeroraTelemetryState::KindReported);

    if (!capabilities) {
        // Not a warning: a model with no inventory is the mock and test path, and the consequence
        // is that consumers show the standard battery model only, which is the correct default.
        qCDebug(AeroraPowerLog) << "Constructed with no capability inventory;"
                                << "the extended power surface stays gated off";
        return;
    }

    (void) connect(capabilities, &AeroraVehicleCapabilities::capabilityChanged,
                   this, &AeroraPowerModel::_onCapabilityReportsChanged);
    (void) connect(capabilities, &AeroraVehicleCapabilities::reportsCleared,
                   this, &AeroraPowerModel::_onCapabilityReportsChanged);

    _recomputeExtendedSurfacePermitted();
}

AeroraPowerModel::~AeroraPowerModel() = default;

AeroraReportedValue *AeroraPowerModel::_addReportedValue(const QString &itemId,
                                                         AeroraTelemetryState::Kind declaredKind)
{
    auto *const value = new AeroraReportedValue(itemId,
                                                declaredKind,
                                                QString::fromLatin1(kPowerFreshnessClass),
                                                _freshness,
                                                this);

    _reportedValues.append(value);
    _itemIdToValueMap.insert(itemId, value);

    return value;
}

AeroraReportedValue *AeroraPowerModel::configuration(Configuration configuration) const
{
    switch (configuration) {
    case Configuration::Tether:
        return _tetherConfiguration;
    case Configuration::Battery:
        return _batteryConfiguration;
    }

    // Unreachable for the two enumerators above. Returning the tether item rather than nullptr
    // would hand a caller the wrong aircraft configuration, so this returns nullptr and the
    // declared non-null contract is kept by the switch being exhaustive.
    return nullptr;
}

AeroraReportedValue *AeroraPowerModel::reportedValue(const QString &itemId) const
{
    return _itemIdToValueMap.value(itemId, nullptr);
}

QStringList AeroraPowerModel::reportedCapabilityKeys() const
{
    if (!_capabilities) {
        return QStringList();
    }

    return _capabilities->reportedKeys(AeroraVehicleCapabilities::Category::Vehicle);
}

void AeroraPowerModel::_onCapabilityReportsChanged()
{
    _recomputeExtendedSurfacePermitted();
}

void AeroraPowerModel::_recomputeExtendedSurfacePermitted()
{
    bool permitted = false;

    if (_capabilities) {
        // Category::Vehicle, because IF-PWR-012 is reported by the flight controller through
        // IF-FC-017 and 17-capability-model.md defines no Power category. See the note on
        // isExtendedSurfacePermitted().
        //
        // "Any vehicle capability reported available" is a coarse gate, and deliberately so: no
        // capability identifier is evidenced, so there is no key this module could match on without
        // inventing one. The gate errs towards showing the standard battery model, which is the
        // safe direction - it never presents an Aerora power surface for an aircraft that has
        // reported nothing.
        const QStringList keys = _capabilities->reportedKeys(AeroraVehicleCapabilities::Category::Vehicle);
        for (const QString &key : keys) {
            if (_capabilities->isAvailable(AeroraVehicleCapabilities::Category::Vehicle, key)) {
                permitted = true;
                break;
            }
        }
    }

    if (_extendedSurfacePermitted == permitted) {
        return;
    }

    _extendedSurfacePermitted = permitted;

    emit extendedSurfacePermittedChanged(_extendedSurfacePermitted);
}

void AeroraPowerModel::clearReports()
{
    for (AeroraReportedValue *const value : _reportedValues) {
        value->clearReport();
    }

    qCDebug(AeroraPowerLog) << "Power reports cleared; every item returns to UNKNOWN";
}

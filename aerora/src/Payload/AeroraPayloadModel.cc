#include "AeroraPayloadModel.h"

#include <QtCore/QMetaEnum>

#include "AeroraFreshness.h"
#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(AeroraPayloadLog, "aerora.payload")

namespace {

/// The freshness class governing every payload item.
///
/// An opaque producer-declared name, as \c M-Telemetry defines a freshness class. It matches the
/// fact group name 06-factgroup-implementation.md assigns to the payload group, so when that group
/// becomes implementable its items inherit the same threshold configuration key.
///
/// **No threshold is registered for it anywhere in this build** — Q-036 is open and no payload
/// message rate is evidenced — so every payload item resolves \c ObservedFreshnessUnevaluated once
/// observed, and consumers must surface the age rather than imply a freshness verdict.
constexpr const char *kPayloadFreshnessClass = "aeroraPayload";

// Stable machine-readable item identifiers, one per 06-payload-icd.md row. For logs, records and
// lookup only: none is a protocol value, a fact name or an operator-facing label, and none is ever
// sent, parsed or matched against a MAVLink field.
constexpr const char *kItemIdentity = "payloadIdentity";           // IF-PAY-001
constexpr const char *kItemPayloadState = "payloadState";           // IF-PAY-003
constexpr const char *kItemHealth = "payloadHealth";                // IF-PAY-004
constexpr const char *kItemSafetyState = "payloadSafetyState";      // IF-PAY-005
constexpr const char *kItemEmergencyStop = "payloadEmergencyStop";  // IF-PAY-006

} // namespace

AeroraPayloadModel::AeroraPayloadModel(AeroraFreshness *freshness,
                                       AeroraVehicleCapabilities *capabilities,
                                       QObject *parent)
    : QObject(parent)
    , _freshness(freshness)
    , _capabilities(capabilities)
{
    if (!freshness) {
        qCWarning(AeroraPayloadLog) << "Constructed with no freshness evaluator;"
                                    << "payload items will never acquire a freshness verdict";
    }

    // Every item is KindReported: the payload controller is the reporting authority for all five,
    // and none of them is a ground-station request or a companion proposal. No item is
    // KindEffective, because 06-payload-icd.md names the payload controller - not the flight
    // controller - as the deciding authority, and nothing evidenced reports a payload value "in
    // force" distinctly from reporting it.
    _identity = _addReportedValue(QString::fromLatin1(kItemIdentity), AeroraTelemetryState::KindReported);
    _payloadState = _addReportedValue(QString::fromLatin1(kItemPayloadState), AeroraTelemetryState::KindReported);
    _health = _addReportedValue(QString::fromLatin1(kItemHealth), AeroraTelemetryState::KindReported);
    _safetyState = _addReportedValue(QString::fromLatin1(kItemSafetyState), AeroraTelemetryState::KindReported);
    _emergencyStop = _addReportedValue(QString::fromLatin1(kItemEmergencyStop), AeroraTelemetryState::KindReported);

    if (!capabilities) {
        // Deliberately not a warning: a model with no inventory is the mock and test path, and the
        // consequence is simply that controls stay gated off, which is the correct default.
        qCDebug(AeroraPayloadLog) << "Constructed with no capability inventory; payload controls stay gated off";
        return;
    }

    (void) connect(capabilities, &AeroraVehicleCapabilities::capabilityChanged,
                   this, &AeroraPayloadModel::_onCapabilityReportsChanged);
    (void) connect(capabilities, &AeroraVehicleCapabilities::reportsCleared,
                   this, &AeroraPayloadModel::_onCapabilityReportsChanged);

    _recomputeControlsPermitted();
}

AeroraPayloadModel::~AeroraPayloadModel() = default;

AeroraReportedValue *AeroraPayloadModel::_addReportedValue(const QString &itemId,
                                                           AeroraTelemetryState::Kind declaredKind)
{
    auto *const value = new AeroraReportedValue(itemId,
                                                declaredKind,
                                                QString::fromLatin1(kPayloadFreshnessClass),
                                                _freshness,
                                                this);

    _reportedValues.append(value);
    _itemIdToValueMap.insert(itemId, value);

    return value;
}

AeroraReportedValue *AeroraPayloadModel::reportedValue(const QString &itemId) const
{
    return _itemIdToValueMap.value(itemId, nullptr);
}

bool AeroraPayloadModel::controlsPermitted() const
{
    return _controlsPermitted;
}

QStringList AeroraPayloadModel::reportedCapabilityKeys() const
{
    if (!_capabilities) {
        return QStringList();
    }

    return _capabilities->reportedKeys(AeroraVehicleCapabilities::Category::Payload);
}

void AeroraPayloadModel::_onCapabilityReportsChanged()
{
    _recomputeControlsPermitted();
}

void AeroraPayloadModel::_recomputeControlsPermitted()
{
    bool permitted = false;

    if (_capabilities) {
        const QStringList keys = _capabilities->reportedKeys(AeroraVehicleCapabilities::Category::Payload);
        for (const QString &key : keys) {
            if (_capabilities->isAvailable(AeroraVehicleCapabilities::Category::Payload, key)) {
                // One reported-available payload capability is enough to present a payload surface.
                // Which controls that surface contains depends on which capability it is, and that
                // mapping is Q-023; this gate answers only "any at all".
                permitted = true;
                break;
            }
        }
    }

    if (_controlsPermitted == permitted) {
        return;
    }

    _controlsPermitted = permitted;

    emit controlsPermittedChanged(_controlsPermitted);
}

void AeroraPayloadModel::setCommandSink(AeroraPayloadCommandSink *sink)
{
    if (_commandSink == sink) {
        return;
    }

    _commandSink = sink;

    qCDebug(AeroraPayloadLog) << "Payload command path installed:" << (sink != nullptr);

    emit commandPathChanged();
}

AeroraPayloadModel::RequestOutcome AeroraPayloadModel::submitRequest(const AeroraPayloadRequest &request)
{
    _lastRequest = request;

    const RequestOutcome outcome = [this, &request]() {
        if (!request.isValid()) {
            return RequestOutcome::RefusedInvalidRequest;
        }

        if (!controlsPermitted()) {
            // IF-PAY-002: with no payload capability reported, no payload control is presented and
            // none may be submitted. Enforced here rather than left to a caller's check.
            return RequestOutcome::RefusedNoCapabilityReported;
        }

        if (!_commandSink) {
            // The IF-PAY-015 default. No implementation of the sink exists in this build, so this
            // is the outcome every request receives.
            return RequestOutcome::RefusedNoCommandPath;
        }

        if (!_commandSink->submitPayloadRequest(request)) {
            return RequestOutcome::RefusedByCommandPath;
        }

        return RequestOutcome::Submitted;
    }();

    const char *const outcomeName = QMetaEnum::fromType<RequestOutcome>().valueToKey(static_cast<int>(outcome));

    if (outcome == RequestOutcome::RefusedInvalidRequest) {
        // A caller handed over a request identifying no operation. That is a defect in the caller,
        // not a gating outcome, so it is the one case worth a warning. Every other refusal is this
        // build behaving as designed: no capability reported, or no command path - IF-PAY-015.
        qCWarning(AeroraPayloadLog) << "Payload request carried no token and identifies no operation";
    } else {
        qCDebug(AeroraPayloadLog) << "requestToken:" << request.requestToken()
                                  << "outcome:" << outcomeName;
    }

    ++_requestCount;

    emit requestCountChanged(_requestCount);

    // No reported value is touched on any path above, and that is the point: IF-PAY-003 requires
    // payload state never be inferred from a command this station sent, and AERORA-PAYLOAD-REQ-011
    // requires effectiveness come from a reported state change. Submitting changes the request
    // record only.
    emit requestOutcome(request, outcome);

    return outcome;
}

void AeroraPayloadModel::clearReports()
{
    for (AeroraReportedValue *const value : _reportedValues) {
        value->clearReport();
    }

    qCDebug(AeroraPayloadLog) << "Payload reports cleared; every item returns to UNKNOWN";
}

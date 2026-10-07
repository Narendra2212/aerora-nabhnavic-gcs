#include "AeroraCommandAuthorizer.h"

#include <QtCore/QCoreApplication>

#include "AeroraCalibrationAuthorization.h"
#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(AeroraAuthorizationLog, "Aerora.Authorization")

QString AeroraAuthorizationDecision::outcomeToString(AeroraAuthorizationOutcome outcome)
{
    switch (outcome) {
    case AeroraAuthorizationOutcome::Allowed:
        return QStringLiteral("Allowed");
    case AeroraAuthorizationOutcome::DeniedNoIdentity:
        return QStringLiteral("Denied: No Identity");
    case AeroraAuthorizationOutcome::DeniedRoleUnresolved:
        return QStringLiteral("Denied: Role Unresolved");
    case AeroraAuthorizationOutcome::DeniedByPolicy:
        return QStringLiteral("Denied: By Policy");
    case AeroraAuthorizationOutcome::DeniedPolicyUnavailable:
        return QStringLiteral("Denied: Policy Unavailable");
    case AeroraAuthorizationOutcome::DeniedNotConfigured:
        return QStringLiteral("Denied: Not Configured");
    }
    return QStringLiteral("Denied: Unknown");
}

QString AeroraAuditEvent::typeToString(AeroraAuditEventType type)
{
    switch (type) {
    case AeroraAuditEventType::OperatorIdentityResolved:
        return QStringLiteral("OperatorIdentityResolved");
    case AeroraAuditEventType::AuthorizationDecision:
        return QStringLiteral("AuthorizationDecision");
    case AeroraAuditEventType::CommandRequested:
        return QStringLiteral("CommandRequested");
    case AeroraAuditEventType::CommandDenied:
        return QStringLiteral("CommandDenied");
    case AeroraAuditEventType::CommandTransmitted:
        return QStringLiteral("CommandTransmitted");
    case AeroraAuditEventType::VehicleResponse:
        return QStringLiteral("VehicleResponse");
    case AeroraAuditEventType::SecurityEvent:
        return QStringLiteral("SecurityEvent");
    case AeroraAuditEventType::SystemEvent:
        return QStringLiteral("SystemEvent");
    }
    return QStringLiteral("Unknown");
}

AeroraCommandAuthorizer* AeroraCommandAuthorizer::instance()
{
    static AeroraCommandAuthorizer instance;
    return &instance;
}

void AeroraCommandAuthorizer::setIdentityProvider(std::unique_ptr<AeroraIdentityProvider> provider)
{
    _identityProvider = std::move(provider);
}

void AeroraCommandAuthorizer::setRoleResolver(std::unique_ptr<AeroraRoleResolver> resolver)
{
    _roleResolver = std::move(resolver);
}

void AeroraCommandAuthorizer::setPolicy(std::unique_ptr<AeroraAuthorizationPolicy> policy)
{
    _policy = std::move(policy);
}

void AeroraCommandAuthorizer::setAuditSink(std::shared_ptr<AeroraAuditSink> sink)
{
    _auditSink = std::move(sink);
}

bool AeroraCommandAuthorizer::isFullyConfigured() const
{
    return _identityProvider && _roleResolver && _policy && _policy->isConfigured();
}

void AeroraCommandAuthorizer::_audit(AeroraAuditEventType type, const QString& operatorId,
                                     const QString& reasonCode, const AeroraGatedOperation& operation)
{
    if (!_auditSink) {
        ++_droppedAuditRecords;
        return;
    }

    AeroraAuditEvent event;
    event.type = type;
    event.utc = QDateTime::currentDateTimeUtc();
    event.operatorId = operatorId;
    event.reasonCode = reasonCode;
    event.vehicleMavlinkSystemId = operation.vehicle.mavlinkSystemId;
    event.vehicleResolved = operation.vehicle.resolved;
    event.detail.insert(QStringLiteral("mavCommand"), operation.mavCommand);
    event.detail.insert(QStringLiteral("targetCompId"), operation.targetCompId);
    event.detail.insert(QStringLiteral("commandInt"), operation.commandInt);

    // All seven, not just param1. For several commands the parameter vector *is* the operation:
    // MAV_CMD_PREFLIGHT_CALIBRATION start/cancel differ only in the parameters, and the ESC
    // variant is carried in param7 alone - so a record holding param1 could not identify what was
    // refused. ADR-017.
    event.detail.insert(QStringLiteral("param1"), operation.param1);
    event.detail.insert(QStringLiteral("param2"), operation.param2);
    event.detail.insert(QStringLiteral("param3"), operation.param3);
    event.detail.insert(QStringLiteral("param4"), operation.param4);
    event.detail.insert(QStringLiteral("param5"), operation.param5);
    event.detail.insert(QStringLiteral("param6"), operation.param6);
    event.detail.insert(QStringLiteral("param7"), operation.param7);

    _auditSink->record(event);
}

AeroraAuthorizationDecision AeroraCommandAuthorizer::authorize(const AeroraGatedOperation& operation)
{
    AeroraAuthorizationDecision decision;

    // Record the attempt before evaluating, so an attempt is auditable even if evaluation
    // cannot proceed. An unrecorded attempt is indistinguishable from no attempt.
    _audit(AeroraAuditEventType::CommandRequested, QString(), QString(), operation);

    // Stage 0: a policy must exist. Absence of policy is not permission.
    if (!_policy || !_policy->isConfigured()) {
        decision.outcome = AeroraAuthorizationOutcome::DeniedNotConfigured;
        decision.reasonCode = QStringLiteral("policy.absent");
    } else if (!_identityProvider) {
        decision.outcome = AeroraAuthorizationOutcome::DeniedNoIdentity;
        decision.reasonCode = QStringLiteral("identity.provider.absent");
    } else if (!_roleResolver) {
        decision.outcome = AeroraAuthorizationOutcome::DeniedRoleUnresolved;
        decision.reasonCode = QStringLiteral("role.resolver.absent");
    }

    QString operatorId;

    if (decision.reasonCode.isEmpty()) {
        // Stage 1: identity.
        const AeroraOperatorIdentity identity = _identityProvider->currentIdentity();
        operatorId = identity.opaqueId;

        if (!identity.isValid()) {
            decision.outcome = AeroraAuthorizationOutcome::DeniedNoIdentity;
            decision.reasonCode = QStringLiteral("identity.invalid");
        } else {
            _audit(AeroraAuditEventType::OperatorIdentityResolved, operatorId, QString(), operation);

            // Stage 2: role resolution. An unresolved role never falls back to a default.
            const AeroraRoleAssignment roles = _roleResolver->resolve(identity);
            if (!roles.resolved) {
                decision.outcome = AeroraAuthorizationOutcome::DeniedRoleUnresolved;
                decision.reasonCode = QStringLiteral("role.unresolved");
            } else {
                // Stage 3: policy. The only stage permitted to allow anything.
                decision = _policy->evaluate(roles, operation);
            }
        }
    }

    _audit(AeroraAuditEventType::AuthorizationDecision, operatorId, decision.reasonCode, operation);

    if (!decision.allowed()) {
        const QString outcomeText = AeroraAuthorizationDecision::outcomeToString(decision.outcome);

        // Calibration is the one command class whose id does not identify the operation: start and
        // cancel share MAV_CMD_PREFLIGHT_CALIBRATION and differ only in the parameter vector. The
        // descriptor is appended so a refusal is diagnosable from the log without re-deriving the
        // parameter mapping by hand. Other commands add nothing here. ADR-017.
        const AeroraCalibrationRequest calibration = AeroraCalibrationAuthorization::classify(operation);
        const QString descriptor = calibration.kind == AeroraCalibrationRequestKind::NotCalibration
                                       ? QString()
                                       : AeroraCalibrationAuthorization::describe(calibration);

        qCWarning(AeroraAuthorizationLog) << "denied mavCommand:" << operation.mavCommand
                                          << "operation:" << descriptor
                                          << "targetCompId:" << operation.targetCompId
                                          << "vehicleResolved:" << operation.vehicle.resolved
                                          << "mavlinkSystemId:" << operation.vehicle.mavlinkSystemId
                                          << "outcome:" << outcomeText
                                          << "reasonCode:" << decision.reasonCode;
        _audit(AeroraAuditEventType::CommandDenied, operatorId, decision.reasonCode, operation);
    }

    return decision;
}

bool AeroraCommandAuthorizer::isAuthorized(const AeroraGatedOperation& operation)
{
    return authorize(operation).allowed();
}

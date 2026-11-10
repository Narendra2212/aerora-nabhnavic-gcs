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

AeroraCommandAuthorizer* AeroraCommandAuthorizer::instance()
{
    static AeroraCommandAuthorizer instance;
    return &instance;
}

void AeroraCommandAuthorizer::setIdentityProvider(std::unique_ptr<AeroraIdentityProvider> provider)
{
    _identityProvider = std::move(provider);
    _notifyChanged();
}

void AeroraCommandAuthorizer::setRoleResolver(std::unique_ptr<AeroraRoleResolver> resolver)
{
    _roleResolver = std::move(resolver);
    _notifyChanged();
}

void AeroraCommandAuthorizer::setPolicy(std::unique_ptr<AeroraAuthorizationPolicy> policy)
{
    _policy = std::move(policy);
    _notifyChanged();
}

void AeroraCommandAuthorizer::setAuditSink(std::shared_ptr<AeroraAuditSink> sink)
{
    _auditSink = std::move(sink);

    // A new sink does not clear the loss counters. The records lost under the previous
    // configuration were still lost, and resetting on install would let a station erase the
    // evidence of its own audit gap by installing a sink late.
    _warnedAuditWriteFailure = false;

    _notifyChanged();
}

void AeroraCommandAuthorizer::setChangeNotifier(ChangeNotifier notifier)
{
    _changeNotifier = notifier;

    // Fires immediately, so an observer that registers after the chain was configured sees the
    // current state instead of waiting for a change that may never come - the shipped station
    // installs no policy, so without this the first notification would never arrive.
    _notifyChanged();
}

void AeroraCommandAuthorizer::_notifyChanged() const
{
    if (_changeNotifier) {
        _changeNotifier();
    }
}

bool AeroraCommandAuthorizer::isFullyConfigured() const
{
    return _identityProvider && _roleResolver && _policy && _policy->isConfigured();
}

QString AeroraCommandAuthorizer::auditSinkName() const
{
    return _auditSink ? _auditSink->sinkName() : QString();
}

bool AeroraCommandAuthorizer::auditIsDurable() const
{
    // No sink means no record survives anything, so the answer is false rather than unknown.
    return _auditSink && _auditSink->isDurable();
}

bool AeroraCommandAuthorizer::auditIsTamperEvident() const
{
    return _auditSink && _auditSink->isTamperEvident();
}

void AeroraCommandAuthorizer::_audit(AeroraAuditEventType type, const QString& operatorId,
                                     const QString& reasonCode, const AeroraGatedOperation& operation)
{
    if (!_auditSink) {
        ++_droppedAuditRecords;
        _notifyChanged();
        return;
    }

    AeroraAuditEvent event;
    event.type = type;

    // Stamped here, at composition, not by the sink. The time fields are the producer's statement
    // about when the event happened; a sink that stamped them would be recording when it got
    // around to the record instead. See the ordering section in AeroraAuditSink.h.
    event.stampNow();

    // Both captured at composition for the same reason. The enforcement posture can change between
    // composing a record and writing it, and the loss count is only evidence if it is the count
    // that applied to this record.
    event.authorizationEnforced = isFullyConfigured();
    event.recordsLostBeforeThis = lostAuditRecordCount();

    event.operatorId = operatorId;
    event.reasonCode = reasonCode;
    const AeroraVehicleContext addressed = operation.address.soleVehicle();
    event.vehicleMavlinkSystemId = addressed.mavlinkSystemId;
    event.vehicleResolved = addressed.resolved;
    event.detail.insert(QStringLiteral("mavCommand"), operation.command.mavCommand);
    event.detail.insert(QStringLiteral("targetCompId"), operation.command.targetCompId);
    event.detail.insert(QStringLiteral("commandInt"), operation.command.commandInt);

    // All seven, not just param1. For several commands the parameter vector *is* the operation:
    // MAV_CMD_PREFLIGHT_CALIBRATION start/cancel differ only in the parameters, and the ESC
    // variant is carried in param7 alone - so a record holding param1 could not identify what was
    // refused. ADR-017.
    event.detail.insert(QStringLiteral("param1"), operation.command.param1);
    event.detail.insert(QStringLiteral("param2"), operation.command.param2);
    event.detail.insert(QStringLiteral("param3"), operation.command.param3);
    event.detail.insert(QStringLiteral("param4"), operation.command.param4);
    event.detail.insert(QStringLiteral("param5"), operation.command.param5);
    event.detail.insert(QStringLiteral("param6"), operation.command.param6);
    event.detail.insert(QStringLiteral("param7"), operation.command.param7);

    const AeroraAuditWriteOutcome outcome = _auditSink->record(event);
    if (!AeroraAuditSink::isLoss(outcome)) {
        return;
    }

    ++_failedAuditWrites;

    // The command is not blocked and the decision is not changed. Whether an unauditable allow
    // must instead be denied is P7-AUDIT-003, which is an OPEN product decision; answering it
    // here would be inventing a security policy. The loss is counted and surfaced instead.
    if (!_warnedAuditWriteFailure) {
        _warnedAuditWriteFailure = true;
        qCWarning(AeroraAuthorizationLog)
            << "audit record was not accepted and the command was NOT blocked."
            << "sink:" << _auditSink->sinkName()
            << "outcome:" << AeroraAuditSink::outcomeToString(outcome)
            << "sequence:" << event.sequence
            << "- further failures are counted, not logged. See failedAuditWriteCount()"
            << "and P7-AUDIT-003.";
    }

    _notifyChanged();
}

bool AeroraCommandAuthorizer::isAuthenticationFresh() const
{
    // False with no provider: an unconfigured station is not fresh, which is the fail-closed
    // posture. A provider with no credential-expiry concept returns true by the base default.
    return _identityProvider && _identityProvider->isAuthenticationFresh();
}

bool AeroraCommandAuthorizer::authorizeThroughOperationGate(const AeroraGatedOperation& operation, bool entersOperation,
                                                            bool exitsOperation)
{
    // The phase is per vehicle: arming one aircraft must not exempt another's commands from the
    // freshness check. An operation with no resolved vehicle cannot be an entry - an arm targets a
    // vehicle - so it runs against a transient gate that persists nothing.
    const AeroraVehicleContext addressed = operation.address.soleVehicle();
    AeroraOperationGate transientGate;
    AeroraOperationGate& gate = addressed.resolved ? _operationGates[addressed.mavlinkSystemId] : transientGate;

    const AeroraOperationGate::Decision gateDecision =
        gate.evaluate(entersOperation, exitsOperation, isAuthenticationFresh());

    if (gateDecision.credentialBlocks) {
        // Refused at an entry for a stale credential. The role chain is never consulted - the
        // command does not reach it - so the refusal is audited here as its own cause. The operator
        // is still identified, because a stale credential still names its operator, so the record
        // says who was refused rather than leaving it blank.
        const QString operatorId = _identityProvider ? _identityProvider->currentIdentity().opaqueId : QString();
        _audit(AeroraAuditEventType::CommandRequested, operatorId, QString(), operation);
        _audit(AeroraAuditEventType::CommandDenied, operatorId, gateDecision.reasonCode, operation);
        return false;
    }

    // Freshness does not stand in the way. The command still faces role authorization, the one
    // stage that may permit it, and authorize() audits that decision as usual. The phase advances
    // only on an allowed command, so a role-refused arm does not enter an operation.
    const bool allowed = authorize(operation).allowed();
    if (allowed) {
        gate.applyAuthorized(gateDecision);
    }
    return allowed;
}

void AeroraCommandAuthorizer::resetOperationGate(int mavlinkSystemId)
{
    _operationGates.remove(mavlinkSystemId);
}

void AeroraCommandAuthorizer::resetAllOperationGates()
{
    _operationGates.clear();
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

        const AeroraVehicleContext denied = operation.address.soleVehicle();

        qCWarning(AeroraAuthorizationLog) << "denied mavCommand:" << operation.command.mavCommand
                                          << "operation:" << descriptor
                                          << "targetCompId:" << operation.command.targetCompId
                                          << "vehicleResolved:" << denied.resolved
                                          << "mavlinkSystemId:" << denied.mavlinkSystemId
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

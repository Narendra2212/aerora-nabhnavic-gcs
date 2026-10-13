#include "AeroraSecurityStatus.h"

#include <QtCore/QList>

#include "AeroraCommandAuthorizer.h"
#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(AeroraSecurityStatusLog, "Aerora.SecurityStatus")

namespace {

/// Every constructed instance.
///
/// A list rather than a single pointer because `QML_SINGLETON` is per-engine, not per-process:
/// `QGCApplication` can create and destroy an engine through
/// `QGCCorePlugin::createQmlApplicationEngine()` / `destroyQmlApplicationEngine()`, and a second
/// engine would construct a second instance. A single-pointer registry would let the second
/// instance silently take over notifications from the first, and the first's destruction would then
/// leave the second unnotified - the same class of defect `AeroraQmlOverrides` guards against by
/// matching on engine identity.
///
/// Mutated only from the constructor and destructor, and read only from `notifyChanged()`. All
/// three run on the thread that owns the instances: a QML engine is created and destroyed on the
/// main thread, and the two call sites that reach `notifyChanged()` - the command gate in
/// `MavCommandQueue::sendWorker()` and `Vehicle::startCalibration()` - run on the vehicle's thread,
/// which is the same thread. No locking, and no claim that this is thread-safe for a caller that
/// changes that.
QList<AeroraSecurityStatus*>& liveInstances()
{
    static QList<AeroraSecurityStatus*> instances;
    return instances;
}

} // namespace

AeroraSecurityStatus::AeroraSecurityStatus(QObject* parent)
    : QObject(parent)
{
    liveInstances().append(this);

    // Registering here rather than at startup keeps the dependency pointing one way: the
    // authorizer is handed a plain function pointer and never learns that a QML engine exists.
    // Re-registering the same pointer on a second instance is harmless, and setChangeNotifier()
    // fires once immediately so this instance starts from the current state rather than from a
    // default.
    AeroraCommandAuthorizer::instance()->setChangeNotifier(&AeroraSecurityStatus::notifyChanged);

    qCDebug(AeroraSecurityStatusLog)
        << "enforcement:" << static_cast<int>(enforcement())
        << "enforcementVerified:" << isEnforcementVerified()
        << "auditSinkInstalled:" << isAuditSinkInstalled()
        << "auditRecordsLost:" << auditRecordsLost();
}

AeroraSecurityStatus::~AeroraSecurityStatus()
{
    liveInstances().removeAll(this);

    // The notifier is deliberately left installed. It points at a static function that iterates a
    // now-shorter list, so it cannot dangle, and clearing it here would break a surviving
    // instance belonging to another engine.
}

void AeroraSecurityStatus::notifyChanged()
{
    // Copied before iterating: a slot reached by statusChanged() could in principle destroy an
    // engine and with it an instance, which would mutate the list mid-iteration.
    const QList<AeroraSecurityStatus*> instances = liveInstances();
    for (AeroraSecurityStatus* const instance : instances) {
        emit instance->statusChanged();
    }
}

AeroraSecurityStatus::Enforcement AeroraSecurityStatus::enforcement() const
{
    // isFullyConfigured() is the authorizer's own definition of "a decision can be reached":
    // identity provider, role resolver, and a policy that reports itself configured. Re-deriving
    // it here from the parts would create a second, divergent answer to the same question.
    return AeroraCommandAuthorizer::instance()->isFullyConfigured()
               ? Enforcement::EnforcedAtCommandGate
               : Enforcement::NotEnforced;
}

bool AeroraSecurityStatus::isAuditSinkInstalled() const
{
    return AeroraCommandAuthorizer::instance()->hasAuditSink();
}

bool AeroraSecurityStatus::isAuditDurable() const
{
    return AeroraCommandAuthorizer::instance()->auditIsDurable();
}

bool AeroraSecurityStatus::isAuditTamperEvident() const
{
    return AeroraCommandAuthorizer::instance()->auditIsTamperEvident();
}

qulonglong AeroraSecurityStatus::auditRecordsLost() const
{
    return AeroraCommandAuthorizer::instance()->lostAuditRecordCount();
}

#pragma once

#include <memory>

#include "AeroraAuditSink.h"
#include "AeroraAuthorization.h"

/// \brief Runs the authorization chain for one outgoing command and records the outcome.
///
/// Identity -> Role Resolution -> Authorization Policy -> decision
///
/// Every stage is injected and every missing stage denies. This class contains no role
/// names, no permission names and no policy content: it only sequences the collaborators
/// and guarantees the fail-closed property in one place, so no caller can reconstruct a
/// permissive path by assembling the pieces differently.
///
class AeroraCommandAuthorizer
{
public:
    static AeroraCommandAuthorizer* instance();

    AeroraCommandAuthorizer() = default;
    ~AeroraCommandAuthorizer() = default;

    AeroraCommandAuthorizer(const AeroraCommandAuthorizer&) = delete;
    AeroraCommandAuthorizer& operator=(const AeroraCommandAuthorizer&) = delete;

    void setIdentityProvider(std::unique_ptr<AeroraIdentityProvider> provider);
    void setRoleResolver(std::unique_ptr<AeroraRoleResolver> resolver);
    void setPolicy(std::unique_ptr<AeroraAuthorizationPolicy> policy);
    void setAuditSink(std::shared_ptr<AeroraAuditSink> sink);

    /// Evaluate \a operation and record the decision. Denies unless every stage is present
    /// and the policy explicitly allows.
    AeroraAuthorizationDecision authorize(const AeroraGatedOperation& operation);

    /// Convenience for the core hook: true only when \c authorize() allowed.
    bool isAuthorized(const AeroraGatedOperation& operation);

    /// True when identity provider, role resolver and a configured policy are all present.
    /// A false result means every command will be denied, which is the intended posture
    /// for an unconfigured station rather than an error to be worked around.
    [[nodiscard]] bool isFullyConfigured() const;

    /// Records that could not be written because no sink is installed. Monotonic.
    ///
    /// Counted rather than ignored: a gap with a count is usable evidence, a silent gap is
    /// not. Whether an unauditable *allow* must instead be denied is `P7-AUDIT-003`, an OPEN
    /// product decision, so this class counts the loss and does not invent a denial rule.
    /// See ADR-018.
    [[nodiscard]] quint64 droppedAuditRecordCount() const { return _droppedAuditRecords; }

    /// Records a sink was handed and did not accept - queue full, storage failure, or a sink
    /// that is installed but not accepting. Previously unobservable, because `record()` returned
    /// `void`.
    [[nodiscard]] quint64 failedAuditWriteCount() const { return _failedAuditWrites; }

    /// Every record this process failed to get recorded, by any cause. The single number
    /// `IF-LOG-015` asks be shown to the operator.
    ///
    /// Two counters rather than one behind it, because the causes have different meanings and
    /// different remedies: no sink installed is a *configuration* state, while a sink that
    /// refuses or fails a record is an *operational* fault. Collapsing them would report a
    /// station that was never configured to record and a station whose disk filled as the same
    /// condition.
    [[nodiscard]] quint64 lostAuditRecordCount() const
    {
        return _droppedAuditRecords + _failedAuditWrites;
    }

    /// \name Audit sink self-report
    ///
    /// Read-only projections of the installed sink, so a consumer can report the audit posture
    /// without being handed the sink. The sink stays private: it is a write path, and nothing
    /// outside this class may be given the ability to inject a record.
    ///@{

    [[nodiscard]] bool hasAuditSink() const { return _auditSink != nullptr; }

    /// The sink's own name, or an empty string when none is installed.
    [[nodiscard]] QString auditSinkName() const;

    /// False when no sink is installed, because nothing survives a restart in that case, and
    /// false for every sink that exists today. Never derived from the sink's presence.
    [[nodiscard]] bool auditIsDurable() const;

    /// False until Q-037/Q4-003 resolves and a mechanism is implemented and tested.
    [[nodiscard]] bool auditIsTamperEvident() const;

    ///@}

    /// A function called when the configuration or the loss counters change, so a status consumer
    /// can re-read without polling.
    ///
    /// A plain function pointer, not a `std::function`: there is no captured state to outlive its
    /// owner, so an observer registered once for the process lifetime cannot dangle. This class
    /// learns nothing about its observer - in particular nothing about QML - so the dependency
    /// runs one way, from the status projection to here.
    using ChangeNotifier = void (*)();
    void setChangeNotifier(ChangeNotifier notifier);

private:
    /// Composes a record and hands it to the sink, counting the loss when it is not accepted.
    /// Returns nothing: an audit outcome never changes an authorization outcome (`P7-AUDIT-003`).
    void _audit(AeroraAuditEventType type, const QString& operatorId, const QString& reasonCode,
                const AeroraGatedOperation& operation);

    void _notifyChanged() const;

    std::unique_ptr<AeroraIdentityProvider> _identityProvider;
    std::unique_ptr<AeroraRoleResolver> _roleResolver;
    std::unique_ptr<AeroraAuthorizationPolicy> _policy;
    std::shared_ptr<AeroraAuditSink> _auditSink;
    quint64 _droppedAuditRecords = 0;
    quint64 _failedAuditWrites = 0;

    /// Warned once rather than per record: a storage outage would otherwise emit a warning for
    /// every record of every command, burying the condition it reports. The counters stay exact;
    /// only the repeated log line is suppressed.
    bool _warnedAuditWriteFailure = false;

    ChangeNotifier _changeNotifier = nullptr;
};

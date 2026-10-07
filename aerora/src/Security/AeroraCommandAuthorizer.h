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
    /// not. Whether an unauditable *allow* must instead be denied is OPEN - it is a policy
    /// decision, so this class counts the loss and does not invent a denial rule.
    [[nodiscard]] quint64 droppedAuditRecordCount() const { return _droppedAuditRecords; }

private:
    void _audit(AeroraAuditEventType type, const QString& operatorId, const QString& reasonCode,
                const AeroraGatedOperation& operation);

    std::unique_ptr<AeroraIdentityProvider> _identityProvider;
    std::unique_ptr<AeroraRoleResolver> _roleResolver;
    std::unique_ptr<AeroraAuthorizationPolicy> _policy;
    std::shared_ptr<AeroraAuditSink> _auditSink;
    quint64 _droppedAuditRecords = 0;
};

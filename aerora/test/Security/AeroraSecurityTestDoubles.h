#pragma once

#include <memory>
#include <utility>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "AeroraAuditSink.h"
#include "AeroraAuthorization.h"
#include "AeroraCommandAuthorizer.h"

/// \file AeroraSecurityTestDoubles.h
/// Shared test doubles for the Phase 7 security suites.
///
/// Introduced with WP2-A/WP2-B because both new suites need the same authorization chain and the
/// same sink behaviours, and a second copy of each stub is a second place for them to drift.
///
/// The two Phase 6 calibration suites keep their own local copies deliberately: converting them
/// would be an unrelated edit to tests that are already written and reviewed, and the shape of
/// their stubs is tied to the calibration cases they exercise.
///
/// \warning No double here holds a role matrix, a permission set or a role name. There is none to
///          hold while `Q-029` is open, and the properties these suites assert - fail-closed
///          sequencing, loss accounting, status projection - are assertable without one. A stub
///          that enumerated roles would be the invented policy `WP2-C` prohibits.

namespace AeroraSecurityTestDoubles {

/// A valid, authenticated identity with an opaque id. The id carries no structure, matching
/// `AeroraOperatorIdentity`'s contract.
class StubIdentityProvider : public AeroraIdentityProvider
{
public:
    [[nodiscard]] AeroraOperatorIdentity currentIdentity() const override
    {
        AeroraOperatorIdentity identity;
        identity.opaqueId = QStringLiteral("opaque-operator-1");
        identity.authenticated = true;
        return identity;
    }
};

/// Resolves to one opaque role token. The token is not a role name and is never compared against
/// one.
class StubRoleResolver : public AeroraRoleResolver
{
public:
    [[nodiscard]] AeroraRoleAssignment resolve(const AeroraOperatorIdentity&) const override
    {
        AeroraRoleAssignment assignment;
        assignment.resolved = true;
        assignment.roleIds = QStringList{QStringLiteral("opaque-role-token")};
        return assignment;
    }
};

/// Reports itself configured and allows every operation.
///
/// Needed because the properties under test are what happens *around* an allow: that an
/// unauditable allow stays an allow (`P7-AUDIT-003`), and that a configured chain is reported as
/// enforcing. A denying variant is not provided here - the suites that need one have their own.
class AllowingPolicy : public AeroraAuthorizationPolicy
{
public:
    [[nodiscard]] bool isConfigured() const override { return true; }

    [[nodiscard]] AeroraAuthorizationDecision evaluate(const AeroraRoleAssignment&,
                                                       const AeroraGatedOperation&) const override
    {
        AeroraAuthorizationDecision decision;
        decision.outcome = AeroraAuthorizationOutcome::Allowed;
        return decision;
    }
};

/// Keeps every record and accepts all of them. Stores nothing beyond process memory and therefore
/// reports itself neither durable nor tamper-evident, by inheriting both defaults.
class RecordingSink : public AeroraAuditSink
{
public:
    AeroraAuditWriteOutcome record(const AeroraAuditEvent& event) override
    {
        events.append(event);
        return AeroraAuditWriteOutcome::Accepted;
    }

    [[nodiscard]] QString sinkName() const override { return QStringLiteral("Recording"); }

    QList<AeroraAuditEvent> events;
};

/// Reports a fixed failure for every record, and counts the calls so a suite can tell "the sink
/// was never asked" apart from "the sink was asked and refused".
class FailingSink : public AeroraAuditSink
{
public:
    explicit FailingSink(AeroraAuditWriteOutcome outcome)
        : _outcome(outcome)
    {
    }

    AeroraAuditWriteOutcome record(const AeroraAuditEvent&) override
    {
        ++callCount;
        return _outcome;
    }

    [[nodiscard]] QString sinkName() const override { return QStringLiteral("Failing"); }

    int callCount = 0;

private:
    AeroraAuditWriteOutcome _outcome;
};

/// Implements only what the interface requires, so the defaulted honesty predicates are exercised
/// exactly as a real implementor would inherit them.
class MinimalSink : public AeroraAuditSink
{
public:
    AeroraAuditWriteOutcome record(const AeroraAuditEvent&) override
    {
        return AeroraAuditWriteOutcome::Accepted;
    }

    [[nodiscard]] QString sinkName() const override { return QStringLiteral("Minimal"); }
};

/// Installs the four collaborators in one call.
///
/// Every suite using these doubles must clear all four in `cleanup()`. The authorizer is a
/// process-wide singleton, so a stub left installed changes the authorization posture - and a sink
/// left installed changes the audit posture - of every later test in the run.
inline void installChain(std::unique_ptr<AeroraIdentityProvider> identity,
                         std::unique_ptr<AeroraRoleResolver> resolver,
                         std::unique_ptr<AeroraAuthorizationPolicy> policy,
                         std::shared_ptr<AeroraAuditSink> sink)
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();
    authorizer->setIdentityProvider(std::move(identity));
    authorizer->setRoleResolver(std::move(resolver));
    authorizer->setPolicy(std::move(policy));
    authorizer->setAuditSink(std::move(sink));
}

/// A fully configured, allowing chain with \a sink installed.
inline void installAllowingChain(std::shared_ptr<AeroraAuditSink> sink)
{
    installChain(std::make_unique<StubIdentityProvider>(), std::make_unique<StubRoleResolver>(),
                 std::make_unique<AllowingPolicy>(), std::move(sink));
}

/// Installs only \a sink, leaving the chain unconfigured. Every command is then denied by the
/// chain, which is the shipped posture and needs no stubs.
inline void installSinkOnly(std::shared_ptr<AeroraAuditSink> sink)
{
    installChain(nullptr, nullptr, nullptr, std::move(sink));
}

/// Clears all four collaborators.
inline void clearChain()
{
    installChain(nullptr, nullptr, nullptr, nullptr);
}

} // namespace AeroraSecurityTestDoubles

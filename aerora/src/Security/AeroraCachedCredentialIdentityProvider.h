#pragma once

#include <QtCore/QString>

#include "AeroraAuthorization.h"

/// \brief A credential the station cached from a successful cloud login, with its own window.
///
/// The owner's model, recorded in
/// [product-completion
/// 15](../../../.kiro/specs/05-product-completion/15-operator-identity-and-the-single-role-deployment.md): an operator
/// authenticates against the Aeroradynamics cloud at **login**, which caches this credential; the arming gate then
/// reads the **cached** credential and contacts nothing. The credential is fresh for an offline flying window, 24 hours
/// by default.
///
/// \section noduration Why the 24 hours is not here
///
/// The window is owner-stated and is a Contract_Value, so it is not compiled in. The credential
/// carries `issuedUtcMs` and `expiresUtcMs`, and whatever issues it sets the gap - exactly as
/// `AeroraManualAuthorityGrant` carries its own issue and expiry rather than baking in a grant
/// duration. A deployment that wants a different window changes the issuer, not this type.
///
/// \section clock Why wall-clock
///
/// Freshness is measured on the wall clock, not the monotonic clock `AeroraManualAuthorityGrant`
/// uses. Two reasons: "24 hours since login" is a human duration, and a credential that is ever
/// persisted across a restart cannot use a monotonic clock, which resets. The cost is a wall clock
/// that steps on an NTP correction, a GNSS fix or an operator change, so a backward step extends a
/// window and a forward step shortens it. That exposure is `IF-SEC-008`'s already ("expiry is a
/// reported state, not an inference") and is not solved here; solving it needs a clock an attacker
/// cannot move, which is the trust-anchor concern of `F3.1-01`.
struct AeroraOperatorCredential
{
    /// Opaque operator identity from the cloud login. Carries no structure and is not parsed.
    QString operatorId;

    /// Wall-clock milliseconds at which the login succeeded. Zero means no credential is cached,
    /// which is distinct from a cached credential that has expired - one is an operator who has
    /// never logged in on this station, the other is one whose window has closed.
    qint64 issuedUtcMs = 0;

    /// Wall-clock milliseconds at which the window closes.
    qint64 expiresUtcMs = 0;

    /// Whether a credential is cached at all, fresh or not. A present-but-expired credential is
    /// still present, which is what keeps an operator identified - and therefore able to keep
    /// commanding an operation in force - after their window has closed.
    [[nodiscard]] bool isPresent() const { return issuedUtcMs != 0 && !operatorId.isEmpty(); }

    /// Whether the credential is present and within its window at \a nowUtcMs.
    ///
    /// Read from the clock on every call rather than latched, for the reason
    /// `AeroraManualAuthorityGrant::inForce()` gives: a latched flag needs something to clear it,
    /// and the window closing is exactly when nothing is guaranteed to run. Fresh *until* expiry,
    /// not through the expiry instant, which matches the grant's boundary.
    [[nodiscard]] bool isFreshAt(qint64 nowUtcMs) const
    {
        if (!isPresent()) {
            return false;
        }
        return nowUtcMs >= issuedUtcMs && nowUtcMs < expiresUtcMs;
    }
};

/// \brief An `AeroraIdentityProvider` that reports identity and freshness from a cached credential.
///
/// \section split Identity and freshness are two answers, not one
///
/// This is the change the owner's entry-not-continuation rule forces, and it is the whole point of
/// the type's shape:
///
/// \li **Identity** - `currentIdentity()` - answers *who is logged in*. It reports the operator as
///     authenticated whenever a credential is **present**, fresh or not. A credential that lapses
///     at the 24-hour mark does not strip the identity, so an operator stays identified and the
///     command path keeps resolving their role while an operation is in force.
/// \li **Freshness** - `isAuthenticationFresh()` - answers *is the login recent enough to enter a
///     new operation*. The operation gate consults it at an arm and nowhere else. A lapsed window
///     makes this false while identity stays true, which is exactly what blocks a new arm while
///     leaving a command in flight untouched.
///
/// Folding the two together - reporting unauthenticated once stale - is what would deny every
/// command after a mid-flight expiry, the continuation-blocking failure the owner's answer forbids.
/// Keeping them apart is what makes that failure unreachable.
///
/// \section notlogin Not the login
///
/// The cloud call that authenticates an operator and produces a credential is a separate seam that
/// needs the network, which `IF-CLD-002` forbids running synchronously on the main thread and which
/// does not exist. Something else performs that call and hands the result to `setCredential()`;
/// this type never reaches a network.
///
/// \section replace Why a new credential replaces the old one wholly
///
/// `setCredential()` overwrites. A re-login that produced a shorter window must not inherit the
/// previous credential's later expiry, which is the same whole-replacement rule `IF-SEC-003` states
/// for role resolution and `AeroraAssignedRoleResolver` implements for the assignment.
class AeroraCachedCredentialIdentityProvider : public AeroraIdentityProvider
{
public:
    AeroraCachedCredentialIdentityProvider() = default;
    explicit AeroraCachedCredentialIdentityProvider(const AeroraOperatorCredential& credential);

    /// Caches \a credential, replacing any held credential wholly.
    void setCredential(const AeroraOperatorCredential& credential);

    /// Drops the cached credential. The explicit sign-out, after which the operator is neither
    /// identified nor fresh.
    void clearCredential();

    /// Who is logged in. Authenticated whenever a credential is present, independent of freshness -
    /// see the split above. An absent credential yields an unauthenticated identity, which
    /// `AeroraOperatorIdentity::isValid()` rejects and the resolver renders as unresolved.
    [[nodiscard]] AeroraOperatorIdentity currentIdentity() const override;

    /// Whether the cached credential is within its window at \a nowUtcMs. The tested core of the
    /// freshness answer.
    [[nodiscard]] bool isCredentialFreshAt(qint64 nowUtcMs) const;

    /// Whether the cached credential is fresh now, on the wall clock. The operation gate reads this
    /// at an entry; a thin wrapper over `isCredentialFreshAt()`.
    [[nodiscard]] bool isAuthenticationFresh() const override;

    /// Whether a credential is cached, fresh or not. For a status surface that distinguishes
    /// "logged in, expired" from "never logged in".
    [[nodiscard]] bool hasCredential() const { return _credential.isPresent(); }

    /// The cached credential's expiry, or zero when none is cached. For a status surface, never for
    /// a decision - a consumer deciding freshness calls `isCredentialFreshAt()` so the clock is
    /// read in one place.
    [[nodiscard]] qint64 credentialExpiresAtUtcMs() const { return _credential.expiresUtcMs; }

private:
    AeroraOperatorCredential _credential;
};

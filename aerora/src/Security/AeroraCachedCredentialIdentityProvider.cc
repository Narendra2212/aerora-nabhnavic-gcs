#include "AeroraCachedCredentialIdentityProvider.h"

#include <QtCore/QDateTime>

AeroraCachedCredentialIdentityProvider::AeroraCachedCredentialIdentityProvider(
    const AeroraOperatorCredential& credential)
    : _credential(credential)
{}

void AeroraCachedCredentialIdentityProvider::setCredential(const AeroraOperatorCredential& credential)
{
    // Whole replacement. A re-login must not inherit a previous credential's later expiry, which is
    // the whole-replacement rule IF-SEC-003 states for role resolution.
    _credential = credential;
}

void AeroraCachedCredentialIdentityProvider::clearCredential()
{
    _credential = AeroraOperatorCredential{};
}

AeroraOperatorIdentity AeroraCachedCredentialIdentityProvider::currentIdentity() const
{
    AeroraOperatorIdentity identity;

    // Logged in means identified. Presence, not freshness: a lapsed credential still names its
    // operator, so the command path keeps resolving their role while an operation is in force. The
    // default unauthenticated identity is what an absent credential yields.
    if (_credential.isPresent()) {
        identity.opaqueId = _credential.operatorId;
        identity.authenticated = true;
    }

    return identity;
}

bool AeroraCachedCredentialIdentityProvider::isCredentialFreshAt(qint64 nowUtcMs) const
{
    return _credential.isFreshAt(nowUtcMs);
}

bool AeroraCachedCredentialIdentityProvider::isAuthenticationFresh() const
{
    return isCredentialFreshAt(QDateTime::currentMSecsSinceEpoch());
}

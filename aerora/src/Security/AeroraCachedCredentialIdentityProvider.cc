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

AeroraOperatorIdentity AeroraCachedCredentialIdentityProvider::currentIdentityAt(qint64 nowUtcMs) const
{
    AeroraOperatorIdentity identity;

    // Default is unauthenticated with an empty id, which AeroraOperatorIdentity::isValid() rejects
    // and the resolver renders as unresolved. Only a present, in-window credential populates it.
    if (_credential.isValidAt(nowUtcMs)) {
        identity.opaqueId = _credential.operatorId;
        identity.authenticated = true;
    }

    return identity;
}

AeroraOperatorIdentity AeroraCachedCredentialIdentityProvider::currentIdentity() const
{
    return currentIdentityAt(QDateTime::currentMSecsSinceEpoch());
}

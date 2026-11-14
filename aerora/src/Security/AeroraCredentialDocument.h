#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "AeroraCachedCredentialIdentityProvider.h"

class AeroraCredentialVerifier;

/// \brief Parses and verifies the administrator-provisioned credential file. The owner-approved
/// credential seam (`DG-ROL` installables), consumed by `AeroraCorePlugin` at startup.
///
/// \section format The file, and why the payload is base64
///
/// \code
/// { "payload": "<base64 of the credential JSON>", "signature": "<base64 of 64 signature bytes>" }
/// \endcode
///
/// The signature covers the **base64 payload characters exactly as they appear in the file**, which
/// removes canonicalisation from the problem entirely. Signing a re-serialised JSON object would
/// make verification depend on key order, whitespace and number formatting agreeing between
/// whatever produced the file and Qt's own writer - a mismatch that fails intermittently and is
/// miserable to diagnose. This is the same reason JWS encodes its payload rather than signing the
/// object, and it means a deployment can sign with standard tooling:
///
/// \code
/// printf '%s' "$PAYLOAD_B64" | openssl pkeyutl -sign -inkey issuer.key -rawin -out signature.bin
/// \endcode
///
/// The decoded payload carries the operator, the window, the roles that operator holds, and the
/// role-to-command-class table the policy evaluates against. The table rides inside the signed
/// payload deliberately: it is the deployment's authorisation policy, so editing it must break the
/// signature, and carrying it here is what `IF-SEC-009` means by credential and role data arriving
/// together.
///
/// \section notexpiry Parsing does not judge freshness
///
/// An expired credential parses and is accepted. That is not an oversight: `AeroraOperatorCredential`
/// keeps presence and freshness apart so an operator stays *identified* after their window closes,
/// which is what lets a command in flight continue while blocking a new operation entry. Freshness
/// is `isFreshAt()`'s answer and the operation gate's to consult; this class only establishes that
/// the issuer really said this.
///
/// \section closed Every failure is a refusal
///
/// A missing verifier, a malformed document, a bad signature, an absent field, an empty operator id
/// and a role granting nothing all produce the same outcome: not accepted, with an operator-facing
/// reason. Nothing is partially applied, so a caller cannot install an identity without also having
/// installed the roles the issuer bound to it.
class AeroraCredentialDocument
{
public:
    /// Opaque role identifier to the opaque command classes that role may authorise - the shape
    /// `AeroraRolePolicy::RoleGrants` takes, kept structurally identical so the policy is
    /// constructed from this without a translation step that could drop an entry.
    using RoleGrants = QHash<QString, QSet<QString>>;

    /// The outcome of reading one credential file. `accepted` false always carries a `detail`.
    struct Result
    {
        bool accepted = false;

        /// Operator-facing reason, set on every outcome including success.
        QString detail;

        /// The operator and the window, ready for `AeroraCachedCredentialIdentityProvider`.
        AeroraOperatorCredential credential;

        /// The roles this operator holds. Opaque strings, never interpreted here.
        QStringList roleIds;

        /// The deployment's role-to-class table, as the issuer signed it.
        RoleGrants roleGrants;
    };

    /// Parses \a fileBytes and verifies its signature with \a verifier.
    ///
    /// \a verifier may be null, which is the state a build with no configured issuer reaches; the
    /// result is then a refusal naming that, never an unverified acceptance. Static because this
    /// holds no state and must be testable without a store, a plugin or a file on disk.
    [[nodiscard]] static Result parseAndVerify(const QByteArray& fileBytes, const AeroraCredentialVerifier* verifier);
};

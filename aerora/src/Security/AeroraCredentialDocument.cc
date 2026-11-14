#include "AeroraCredentialDocument.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>
#include <QtCore/QJsonValue>

#include "AeroraCredentialVerifier.h"

namespace {

AeroraCredentialDocument::Result refused(const QString& detail)
{
    AeroraCredentialDocument::Result result;
    result.accepted = false;
    result.detail = detail;
    return result;
}

/// Decodes strict base64, returning nullopt-by-emptiness for input that is not valid base64.
/// QByteArray::fromBase64Encoding is used rather than fromBase64 because the latter silently
/// ignores malformed input, which would turn a corrupt file into an empty payload instead of a
/// refusal.
bool decodeBase64(const QString& text, QByteArray& out)
{
    const QByteArray::FromBase64Result decoded = QByteArray::fromBase64Encoding(
        text.toLatin1(), QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded) {
        return false;
    }
    out = *decoded;
    return true;
}

}  // namespace

AeroraCredentialDocument::Result AeroraCredentialDocument::parseAndVerify(const QByteArray& fileBytes,
                                                                          const AeroraCredentialVerifier* verifier)
{
    if (!verifier) {
        // A build with no configured issuer installs no verifier. Refusing here rather than
        // accepting an unverified document is what keeps that posture fail-closed.
        return refused(QStringLiteral("no credential verifier is configured in this build"));
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(fileBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return refused(QStringLiteral("the credential file is not valid JSON: %1").arg(parseError.errorString()));
    }
    if (!document.isObject()) {
        return refused(QStringLiteral("the credential file does not contain a JSON object"));
    }

    const QJsonObject root = document.object();
    const QJsonValue payloadValue = root.value(QStringLiteral("payload"));
    const QJsonValue signatureValue = root.value(QStringLiteral("signature"));
    if (!payloadValue.isString() || !signatureValue.isString()) {
        return refused(QStringLiteral("the credential file needs a string payload and a string signature"));
    }

    // The signature covers the payload characters verbatim, so they are taken from the file as
    // written rather than re-encoded from the decoded object. See the format note on the header.
    const QByteArray signedBytes = payloadValue.toString().toLatin1();

    QByteArray signature;
    if (!decodeBase64(signatureValue.toString(), signature)) {
        return refused(QStringLiteral("the credential signature is not valid base64"));
    }

    if (!verifier->verify(signedBytes, signature)) {
        // Deliberately one message for a forged signature, a corrupted one and one from the wrong
        // issuer: they are the same outcome, and distinguishing them tells an attacker which.
        return refused(QStringLiteral("the credential signature is not valid for this build's issuer"));
    }

    QByteArray payloadJson;
    if (!decodeBase64(payloadValue.toString(), payloadJson)) {
        return refused(QStringLiteral("the credential payload is not valid base64"));
    }

    QJsonParseError payloadError{};
    const QJsonDocument payloadDocument = QJsonDocument::fromJson(payloadJson, &payloadError);
    if ((payloadError.error != QJsonParseError::NoError) || !payloadDocument.isObject()) {
        return refused(QStringLiteral("the signed credential payload is not a JSON object"));
    }

    const QJsonObject payload = payloadDocument.object();

    const QString operatorId = payload.value(QStringLiteral("operatorId")).toString();
    if (operatorId.isEmpty()) {
        // AeroraOperatorCredential treats an empty id as no credential, so accepting one would
        // produce an identity that reports itself absent.
        return refused(QStringLiteral("the credential carries no operatorId"));
    }

    const QJsonValue issued = payload.value(QStringLiteral("issuedUtcMs"));
    const QJsonValue expires = payload.value(QStringLiteral("expiresUtcMs"));
    if (!issued.isDouble() || !expires.isDouble()) {
        return refused(QStringLiteral("the credential needs numeric issuedUtcMs and expiresUtcMs"));
    }

    Result result;
    result.credential.operatorId = operatorId;
    result.credential.issuedUtcMs = static_cast<qint64>(issued.toDouble());
    result.credential.expiresUtcMs = static_cast<qint64>(expires.toDouble());

    if (result.credential.expiresUtcMs <= result.credential.issuedUtcMs) {
        // A window that closes before it opens is never fresh, so it would install an identity
        // that could never enter an operation. Refused as malformed rather than accepted as expired.
        return refused(QStringLiteral("the credential window closes no later than it opens"));
    }

    const QJsonValue roleIdsValue = payload.value(QStringLiteral("roleIds"));
    if (!roleIdsValue.isArray()) {
        return refused(QStringLiteral("the credential needs a roleIds array"));
    }
    const QJsonArray roleIdsArray = roleIdsValue.toArray();
    for (const QJsonValue& roleId : roleIdsArray) {
        if (!roleId.isString() || roleId.toString().isEmpty()) {
            return refused(QStringLiteral("every entry in roleIds must be a non-empty string"));
        }
        result.roleIds.append(roleId.toString());
    }

    const QJsonValue grantsValue = payload.value(QStringLiteral("roleGrants"));
    if (!grantsValue.isObject()) {
        return refused(QStringLiteral("the credential needs a roleGrants object"));
    }
    const QJsonObject grantsObject = grantsValue.toObject();
    for (auto it = grantsObject.constBegin(); it != grantsObject.constEnd(); ++it) {
        if (!it.value().isArray()) {
            return refused(QStringLiteral("every roleGrants entry must be an array of command classes"));
        }
        QSet<QString> classes;
        const QJsonArray classArray = it.value().toArray();
        for (const QJsonValue& commandClass : classArray) {
            if (!commandClass.isString() || commandClass.toString().isEmpty()) {
                return refused(QStringLiteral("every command class must be a non-empty string"));
            }
            classes.insert(commandClass.toString());
        }
        if (classes.isEmpty()) {
            // The signed role set's own condition: a role authorising nothing is not a role.
            return refused(QStringLiteral("a role granting no command class is not a role"));
        }
        result.roleGrants.insert(it.key(), classes);
    }

    if (result.roleGrants.isEmpty()) {
        // AeroraRolePolicy reports itself unconfigured on an empty table, so this would install a
        // policy that denies everything while the station claimed to be enforcing.
        return refused(QStringLiteral("the credential carries an empty roleGrants table"));
    }

    result.accepted = true;
    result.detail =
        QStringLiteral("accepted a credential for an operator holding %1 role(s)").arg(result.roleIds.size());
    return result;
}

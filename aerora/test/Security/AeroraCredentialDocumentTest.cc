#include "AeroraCredentialDocumentTest.h"

#include <monocypher-ed25519.h>

#include <QtCore/QByteArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtTest/QTest>

#include "AeroraCredentialDocument.h"
#include "AeroraEd25519Verifier.h"

namespace {

/// A fixed-seed issuer keypair. Deterministic so a failure is reproducible, and test-local so no
/// key material is shared with anything that ships.
struct Issuer
{
    QByteArray publicKey;  ///< 32 bytes
    QByteArray secretKey;  ///< 64 bytes

    static Issuer fixed()
    {
        QByteArray seed(32, '\x2a');
        Issuer issuer;
        issuer.publicKey.resize(32);
        issuer.secretKey.resize(64);
        crypto_ed25519_key_pair(reinterpret_cast<uint8_t*>(issuer.secretKey.data()),
                                reinterpret_cast<uint8_t*>(issuer.publicKey.data()),
                                reinterpret_cast<uint8_t*>(seed.data()));
        return issuer;
    }

    /// Signs \a message exactly as `openssl pkeyutl -sign -rawin` would.
    [[nodiscard]] QByteArray sign(const QByteArray& message) const
    {
        QByteArray signature(64, '\0');
        crypto_ed25519_sign(reinterpret_cast<uint8_t*>(signature.data()),
                            reinterpret_cast<const uint8_t*>(secretKey.constData()),
                            reinterpret_cast<const uint8_t*>(message.constData()), static_cast<size_t>(message.size()));
        return signature;
    }
};

/// A well-formed payload. Role names come from the signed DG-ROL set, carried as opaque strings.
QByteArray samplePayload()
{
    return QByteArrayLiteral(
        R"({"operatorId":"opaque-operator-1","issuedUtcMs":1700000000000,"expiresUtcMs":1700086400000,)"
        R"("roleIds":["Remote Pilot"],)"
        R"("roleGrants":{"Remote Pilot":["command","mission","manual-control","payload"],)"
        R"("Maintenance Technician":["command","parameter"]}})");
}

/// Assembles a credential file from \a payload signed by \a issuer.
QByteArray documentFor(const Issuer& issuer, const QByteArray& payload)
{
    const QByteArray payloadB64 = payload.toBase64();
    QJsonObject root;
    root.insert(QStringLiteral("payload"), QString::fromLatin1(payloadB64));
    root.insert(QStringLiteral("signature"), QString::fromLatin1(issuer.sign(payloadB64).toBase64()));
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

}  // namespace

void AeroraCredentialDocumentTest::_aSignedDocumentIsAccepted()
{
    const Issuer issuer = Issuer::fixed();
    const AeroraEd25519Verifier verifier(issuer.publicKey);

    const AeroraCredentialDocument::Result result =
        AeroraCredentialDocument::parseAndVerify(documentFor(issuer, samplePayload()), &verifier);

    QVERIFY2(result.accepted, qPrintable(result.detail));
    QCOMPARE(result.credential.operatorId, QStringLiteral("opaque-operator-1"));
    QCOMPARE(result.credential.issuedUtcMs, 1700000000000LL);
    QCOMPARE(result.credential.expiresUtcMs, 1700086400000LL);
    QVERIFY(result.credential.isPresent());

    QCOMPARE(result.roleIds, QStringList{QStringLiteral("Remote Pilot")});

    // The role table arrives as the issuer signed it, in the shape AeroraRolePolicy takes.
    QCOMPARE(result.roleGrants.size(), 2);
    QVERIFY(result.roleGrants.value(QStringLiteral("Remote Pilot")).contains(QStringLiteral("manual-control")));
    QVERIFY(result.roleGrants.value(QStringLiteral("Maintenance Technician")).contains(QStringLiteral("parameter")));
    // Separation of duties, as the signed set records it: the flying role holds no parameter class.
    QVERIFY(!result.roleGrants.value(QStringLiteral("Remote Pilot")).contains(QStringLiteral("parameter")));
}

void AeroraCredentialDocumentTest::_noVerifierRefusesEvenAValidDocument()
{
    const Issuer issuer = Issuer::fixed();

    // The state a build with no configured issuer reaches. The document is perfectly good and is
    // still refused, which is what keeps the shipped station fail-closed.
    const AeroraCredentialDocument::Result result =
        AeroraCredentialDocument::parseAndVerify(documentFor(issuer, samplePayload()), nullptr);

    QVERIFY(!result.accepted);
    QVERIFY(!result.detail.isEmpty());
    QVERIFY(!result.credential.isPresent());
}

void AeroraCredentialDocumentTest::_anotherIssuerIsRefused()
{
    const Issuer issuer = Issuer::fixed();

    // A different, well-formed issuer: the forgery case.
    QByteArray otherSeed(32, '\x7b');
    Issuer other;
    other.publicKey.resize(32);
    other.secretKey.resize(64);
    crypto_ed25519_key_pair(reinterpret_cast<uint8_t*>(other.secretKey.data()),
                            reinterpret_cast<uint8_t*>(other.publicKey.data()),
                            reinterpret_cast<uint8_t*>(otherSeed.data()));

    const AeroraEd25519Verifier verifier(issuer.publicKey);
    const AeroraCredentialDocument::Result result =
        AeroraCredentialDocument::parseAndVerify(documentFor(other, samplePayload()), &verifier);

    QVERIFY(!result.accepted);
    QVERIFY(!result.credential.isPresent());
}

void AeroraCredentialDocumentTest::_anEditedPayloadBreaksTheSignature()
{
    const Issuer issuer = Issuer::fixed();
    const AeroraEd25519Verifier verifier(issuer.publicKey);

    // The property the whole mechanism rests on, and the reason roleGrants rides inside the signed
    // payload: granting yourself a class you were not issued invalidates the credential.
    QByteArray escalated = samplePayload();
    escalated.replace(R"("Remote Pilot":["command","mission","manual-control","payload"])",
                      R"("Remote Pilot":["command","mission","manual-control","payload","parameter"])");
    QVERIFY(escalated != samplePayload());

    const QByteArray tampered =
        documentFor(issuer, samplePayload()).replace(samplePayload().toBase64(), escalated.toBase64());

    const AeroraCredentialDocument::Result result = AeroraCredentialDocument::parseAndVerify(tampered, &verifier);

    QVERIFY(!result.accepted);
    QVERIFY(result.roleGrants.isEmpty());
}

void AeroraCredentialDocumentTest::_anExpiredCredentialStillParses()
{
    const Issuer issuer = Issuer::fixed();
    const AeroraEd25519Verifier verifier(issuer.publicKey);

    // Long past. Accepted deliberately: presence and freshness are separate answers, so an
    // operator stays identified after their window closes - which is what lets a command already
    // in flight continue while a new operation entry is refused. isFreshAt() is the gate's check,
    // not this one's.
    const QByteArray payload =
        QByteArrayLiteral(R"({"operatorId":"lapsed","issuedUtcMs":1000,"expiresUtcMs":2000,)"
                          R"("roleIds":["Remote Pilot"],"roleGrants":{"Remote Pilot":["command"]}})");

    const AeroraCredentialDocument::Result result =
        AeroraCredentialDocument::parseAndVerify(documentFor(issuer, payload), &verifier);

    QVERIFY2(result.accepted, qPrintable(result.detail));
    QVERIFY(result.credential.isPresent());
    QVERIFY(!result.credential.isFreshAt(1700000000000LL));
}

void AeroraCredentialDocumentTest::_malformedDocumentsAreRefused()
{
    const Issuer issuer = Issuer::fixed();
    const AeroraEd25519Verifier verifier(issuer.publicKey);

    const QList<QByteArray> rejected = {
        QByteArrayLiteral("not json at all"),
        QByteArrayLiteral("[]"),
        QByteArrayLiteral("{}"),
        QByteArrayLiteral(R"({"payload":"eyJ9","signature":42})"),
        QByteArrayLiteral(R"({"payload":"!!! not base64 !!!","signature":"!!!"})"),
    };

    for (const QByteArray& document : rejected) {
        const AeroraCredentialDocument::Result result = AeroraCredentialDocument::parseAndVerify(document, &verifier);
        QVERIFY2(!result.accepted, qPrintable(QString::fromUtf8(document)));
        QVERIFY(!result.detail.isEmpty());
    }
}

void AeroraCredentialDocumentTest::_incompletePayloadsAreRefused()
{
    const Issuer issuer = Issuer::fixed();
    const AeroraEd25519Verifier verifier(issuer.publicKey);

    // Each is correctly signed, so these assert the field checks rather than the signature.
    const QList<QByteArray> rejected = {
        // No operatorId: AeroraOperatorCredential would report itself absent.
        QByteArrayLiteral(
            R"({"operatorId":"","issuedUtcMs":1,"expiresUtcMs":2,"roleIds":[],"roleGrants":{"r":["command"]}})"),
        // Non-numeric window.
        QByteArrayLiteral(
            R"({"operatorId":"o","issuedUtcMs":"1","expiresUtcMs":2,"roleIds":[],"roleGrants":{"r":["command"]}})"),
        // A window that closes before it opens is never fresh.
        QByteArrayLiteral(
            R"({"operatorId":"o","issuedUtcMs":5,"expiresUtcMs":5,"roleIds":[],"roleGrants":{"r":["command"]}})"),
        // No roleIds array.
        QByteArrayLiteral(R"({"operatorId":"o","issuedUtcMs":1,"expiresUtcMs":2,"roleGrants":{"r":["command"]}})"),
        // A role granting nothing is not a role.
        QByteArrayLiteral(R"({"operatorId":"o","issuedUtcMs":1,"expiresUtcMs":2,"roleIds":[],"roleGrants":{"r":[]}})"),
        // An empty table would make AeroraRolePolicy report itself unconfigured.
        QByteArrayLiteral(R"({"operatorId":"o","issuedUtcMs":1,"expiresUtcMs":2,"roleIds":[],"roleGrants":{}})"),
    };

    for (const QByteArray& payload : rejected) {
        const AeroraCredentialDocument::Result result =
            AeroraCredentialDocument::parseAndVerify(documentFor(issuer, payload), &verifier);
        QVERIFY2(!result.accepted, qPrintable(QString::fromUtf8(payload)));
    }
}

void AeroraCredentialDocumentTest::_theSignatureCoversTheFileBytesNotAReserialisation()
{
    const Issuer issuer = Issuer::fixed();
    const AeroraEd25519Verifier verifier(issuer.publicKey);

    // The same payload with its keys in a different order and extra whitespace. Signed as written,
    // it verifies - which is the canonicalisation problem not existing, rather than being solved.
    const QByteArray reordered = QByteArrayLiteral(
        "{\n  \"expiresUtcMs\" : 1700086400000,\n  \"issuedUtcMs\" : 1700000000000,\n"
        "  \"operatorId\" : \"opaque-operator-1\",\n  \"roleGrants\" : { \"Remote Pilot\" : [ \"command\" ] },\n"
        "  \"roleIds\" : [ \"Remote Pilot\" ]\n}");

    const AeroraCredentialDocument::Result result =
        AeroraCredentialDocument::parseAndVerify(documentFor(issuer, reordered), &verifier);

    QVERIFY2(result.accepted, qPrintable(result.detail));
    QCOMPARE(result.credential.operatorId, QStringLiteral("opaque-operator-1"));
}

UT_REGISTER_TEST(AeroraCredentialDocumentTest, TestLabel::Unit)

#include "AeroraAuthorizationChainInstallTest.h"

#include <monocypher-ed25519.h>

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QIODevice>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include "AeroraCommandAuthorizer.h"
#include "AeroraCorePlugin.h"

namespace {

/// A fixed-seed issuer keypair. Deterministic so a failure is reproducible, and test-local so no
/// key material is shared with anything that ships.
struct Issuer
{
    QByteArray publicKey;  ///< 32 bytes
    QByteArray secretKey;  ///< 64 bytes

    static Issuer fromSeedByte(char seedByte)
    {
        QByteArray seed(32, seedByte);
        Issuer issuer;
        issuer.publicKey.resize(32);
        issuer.secretKey.resize(64);
        crypto_ed25519_key_pair(reinterpret_cast<uint8_t*>(issuer.secretKey.data()),
                                reinterpret_cast<uint8_t*>(issuer.publicKey.data()),
                                reinterpret_cast<uint8_t*>(seed.data()));
        return issuer;
    }

    /// The 64-hex form the build input carries, which is what the seam takes.
    [[nodiscard]] QString keyHex() const { return QString::fromLatin1(publicKey.toHex()); }

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

/// A payload whose window is open at \a nowUtcMs. Role names come from the signed DG-ROL set,
/// carried as opaque strings.
QByteArray payloadAround(qint64 nowUtcMs)
{
    return QByteArrayLiteral(R"({"operatorId":"opaque-operator-1","issuedUtcMs":)") +
           QByteArray::number(nowUtcMs - 3600000LL) + QByteArrayLiteral(R"(,"expiresUtcMs":)") +
           QByteArray::number(nowUtcMs + 3600000LL) +
           QByteArrayLiteral(R"(,"roleIds":["Remote Pilot"],)"
                             R"("roleGrants":{"Remote Pilot":["command","mission"]}})");
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

/// Writes \a bytes into \a dir as a credential file and returns its path.
QString writeCredential(const QTemporaryDir& dir, const QByteArray& bytes)
{
    const QString path = dir.filePath(QStringLiteral("credential.json"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    file.write(bytes);
    file.close();
    return path;
}

}  // namespace

void AeroraAuthorizationChainInstallTest::_aVerifiedCredentialInstallsTheWholeChain()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const Issuer issuer = Issuer::fromSeedByte('\x2a');
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QString path = writeCredential(dir, documentFor(issuer, payloadAround(now)));
    QVERIFY(!path.isEmpty());

    AeroraCommandAuthorizer authorizer;
    QVERIFY(!authorizer.isFullyConfigured());

    const AeroraCorePlugin::ChainInstallation result =
        AeroraCorePlugin::installAuthorizationChainInto(authorizer, issuer.keyHex(), path);

    QVERIFY2(result.installed, qPrintable(result.detail));
    QVERIFY(!result.detail.isEmpty());

    // The predicate the command send path consults. Until it is true no decision is made at all,
    // so this is the one assertion that distinguishes an installed chain from a wired one.
    QVERIFY(authorizer.isFullyConfigured());

    // And the identity installed is the credential's own, carrying the window the issuer signed
    // rather than a stand-in that reports itself fresh unconditionally.
    QVERIFY(authorizer.isAuthenticationFresh());
}

void AeroraAuthorizationChainInstallTest::_anExpiredCredentialInstallsAndReportsNotFresh()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const Issuer issuer = Issuer::fromSeedByte('\x2a');

    // Long past, and deliberately still installed: presence and freshness are separate answers, so
    // the operator stays identified after their window closes. Refusing to install here would cut
    // off a command already in flight, which is the opposite of the intended posture.
    const QByteArray payload =
        QByteArrayLiteral(R"({"operatorId":"lapsed","issuedUtcMs":1000,"expiresUtcMs":2000,)"
                          R"("roleIds":["Remote Pilot"],"roleGrants":{"Remote Pilot":["command"]}})");
    const QString path = writeCredential(dir, documentFor(issuer, payload));
    QVERIFY(!path.isEmpty());

    AeroraCommandAuthorizer authorizer;
    const AeroraCorePlugin::ChainInstallation result =
        AeroraCorePlugin::installAuthorizationChainInto(authorizer, issuer.keyHex(), path);

    QVERIFY2(result.installed, qPrintable(result.detail));
    QVERIFY(authorizer.isFullyConfigured());
    QVERIFY(!authorizer.isAuthenticationFresh());
}

void AeroraAuthorizationChainInstallTest::_anAlreadyInstalledChainIsNotReplaced()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const Issuer issuer = Issuer::fromSeedByte('\x2a');
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QString path = writeCredential(dir, documentFor(issuer, payloadAround(now)));
    QVERIFY(!path.isEmpty());

    AeroraCommandAuthorizer authorizer;
    QVERIFY(AeroraCorePlugin::installAuthorizationChainInto(authorizer, issuer.keyHex(), path).installed);
    QVERIFY(authorizer.isFullyConfigured());

    // A second attempt with a credential naming a different operator. The guard is what makes a
    // second init() in a UI-test process harmless, and it also means a credential appearing at the
    // path later cannot silently swap the operator the station is enforcing against.
    const QByteArray second =
        QByteArrayLiteral(R"({"operatorId":"someone-else","issuedUtcMs":1000,"expiresUtcMs":2000,)"
                          R"("roleIds":["Remote Pilot"],"roleGrants":{"Remote Pilot":["command"]}})");
    const QString secondPath = writeCredential(dir, documentFor(issuer, second));

    const AeroraCorePlugin::ChainInstallation result =
        AeroraCorePlugin::installAuthorizationChainInto(authorizer, issuer.keyHex(), secondPath);

    QVERIFY(!result.installed);
    QVERIFY(!result.detail.isEmpty());

    // Still the first credential's chain: configured, and fresh because the first window is open
    // while the second has long closed.
    QVERIFY(authorizer.isFullyConfigured());
    QVERIFY(authorizer.isAuthenticationFresh());
}

void AeroraAuthorizationChainInstallTest::_noIssuerKeyInstallsNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const Issuer issuer = Issuer::fromSeedByte('\x2a');
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QString path = writeCredential(dir, documentFor(issuer, payloadAround(now)));
    QVERIFY(!path.isEmpty());

    AeroraCommandAuthorizer authorizer;

    // The shipped default: AERORA_CREDENTIAL_ISSUER_PUBLIC_KEY is empty unless a deployment sets
    // it. The credential at the path is perfectly good and is still not installed, which is the
    // whole reason the key is a build input rather than a file an attacker could drop in.
    const AeroraCorePlugin::ChainInstallation result =
        AeroraCorePlugin::installAuthorizationChainInto(authorizer, QString(), path);

    QVERIFY(!result.installed);
    QVERIFY(!authorizer.isFullyConfigured());
    QVERIFY(!authorizer.isAuthenticationFresh());
}

void AeroraAuthorizationChainInstallTest::_anUnusableIssuerKeyInstallsNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const Issuer issuer = Issuer::fromSeedByte('\x2a');
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QString path = writeCredential(dir, documentFor(issuer, payloadAround(now)));
    QVERIFY(!path.isEmpty());

    // Each is something a misconfigured deployment could put in the build input. The odd-length
    // entry is the one that matters most: QByteArray::fromHex() pads it as though it had a leading
    // zero, so 63 characters decode to 32 bytes and would otherwise look like a usable key.
    const QStringList unusable = {
        QStringLiteral("not hex at all"),
        issuer.keyHex().left(63),
        issuer.keyHex().left(62),
        issuer.keyHex() + QStringLiteral("00"),
    };

    for (const QString& keyHex : unusable) {
        AeroraCommandAuthorizer authorizer;
        const AeroraCorePlugin::ChainInstallation result =
            AeroraCorePlugin::installAuthorizationChainInto(authorizer, keyHex, path);

        QVERIFY2(!result.installed, qPrintable(keyHex));
        QVERIFY2(!authorizer.isFullyConfigured(), qPrintable(keyHex));
    }
}

void AeroraAuthorizationChainInstallTest::_anEmptyCredentialPathInstallsNothing()
{
    const Issuer issuer = Issuer::fromSeedByte('\x2a');

    // AeroraStorePaths::credentialPath() returns an empty string when persistence resolves no save
    // path, so this is a state the plumbing really reaches rather than a hypothetical argument.
    AeroraCommandAuthorizer authorizer;
    const AeroraCorePlugin::ChainInstallation result =
        AeroraCorePlugin::installAuthorizationChainInto(authorizer, issuer.keyHex(), QString());

    QVERIFY(!result.installed);
    QVERIFY(!authorizer.isFullyConfigured());
}

void AeroraAuthorizationChainInstallTest::_aMissingCredentialFileInstallsNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const Issuer issuer = Issuer::fromSeedByte('\x2a');
    const QString path = dir.filePath(QStringLiteral("absent.json"));
    QVERIFY(!QFile::exists(path));

    AeroraCommandAuthorizer authorizer;
    const AeroraCorePlugin::ChainInstallation result =
        AeroraCorePlugin::installAuthorizationChainInto(authorizer, issuer.keyHex(), path);

    QVERIFY(!result.installed);
    QVERIFY(!authorizer.isFullyConfigured());
}

void AeroraAuthorizationChainInstallTest::_anUnreadableCredentialIsRefused()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const Issuer issuer = Issuer::fromSeedByte('\x2a');

    // A directory where the credential belongs: it exists, so the absence path does not catch it,
    // and opening it for reading fails on every platform. That reaches the open-failure branch
    // without needing file permissions, which behave differently on Windows.
    const QString path = dir.filePath(QStringLiteral("credential.json"));
    QVERIFY(QDir().mkpath(path));
    QVERIFY(QFile::exists(path));

    AeroraCommandAuthorizer authorizer;

    expectLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("could not read the operator credential")));
    const AeroraCorePlugin::ChainInstallation result =
        AeroraCorePlugin::installAuthorizationChainInto(authorizer, issuer.keyHex(), path);
    verifyExpectedLogMessage();

    QVERIFY(!result.installed);
    QVERIFY(!authorizer.isFullyConfigured());
}

void AeroraAuthorizationChainInstallTest::_aForgedCredentialIsRefused()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const Issuer configured = Issuer::fromSeedByte('\x2a');

    // A different, well-formed issuer signing a well-formed credential: forgery rather than
    // corruption, and the case the whole asymmetric mechanism exists to refuse.
    const Issuer other = Issuer::fromSeedByte('\x7b');
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QString path = writeCredential(dir, documentFor(other, payloadAround(now)));
    QVERIFY(!path.isEmpty());

    AeroraCommandAuthorizer authorizer;

    expectLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("operator credential was refused")));
    const AeroraCorePlugin::ChainInstallation result =
        AeroraCorePlugin::installAuthorizationChainInto(authorizer, configured.keyHex(), path);
    verifyExpectedLogMessage();

    QVERIFY(!result.installed);
    QVERIFY(!authorizer.isFullyConfigured());
    QVERIFY(!authorizer.isAuthenticationFresh());
}

void AeroraAuthorizationChainInstallTest::_aMalformedCredentialIsRefused()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const Issuer issuer = Issuer::fromSeedByte('\x2a');
    const QString path = writeCredential(dir, QByteArrayLiteral("not json at all"));
    QVERIFY(!path.isEmpty());

    AeroraCommandAuthorizer authorizer;

    expectLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("operator credential was refused")));
    const AeroraCorePlugin::ChainInstallation result =
        AeroraCorePlugin::installAuthorizationChainInto(authorizer, issuer.keyHex(), path);
    verifyExpectedLogMessage();

    QVERIFY(!result.installed);
    QVERIFY(!authorizer.isFullyConfigured());
}

UT_REGISTER_TEST(AeroraAuthorizationChainInstallTest, TestLabel::Unit)

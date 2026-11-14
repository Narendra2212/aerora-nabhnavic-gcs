#include "AeroraEd25519VerifierTest.h"

#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtTest/QTest>

#include "AeroraEd25519Verifier.h"

namespace {

QByteArray hex(const char* text)
{
    return QByteArray::fromHex(QByteArray(text));
}

// RFC 8032 §7.1, "Test 1": a 32-byte public key, an empty message, and its signature.
const char* kTest1PublicKey = "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a";
const char* kTest1Signature =
    "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b4"
    "6bd25bf5f0595bbe24655141438e7a100b";

// RFC 8032 §7.1, "Test 2": a one-byte message, 0x72.
const char* kTest2PublicKey = "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c";
const char* kTest2Message = "72";
const char* kTest2Signature =
    "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d"
    "8c387b2eaeb4302aeeb00d291612bb0c00";

}  // namespace

void AeroraEd25519VerifierTest::_rfc8032Test1Verifies()
{
    const AeroraEd25519Verifier verifier(hex(kTest1PublicKey));
    QVERIFY(verifier.isUsable());

    // An empty message is legitimate and is the RFC's own first vector, so it must verify rather
    // than be refused as malformed input.
    QVERIFY(verifier.verify(QByteArray(), hex(kTest1Signature)));
}

void AeroraEd25519VerifierTest::_rfc8032Test2Verifies()
{
    const AeroraEd25519Verifier verifier(hex(kTest2PublicKey));
    QVERIFY(verifier.isUsable());
    QVERIFY(verifier.verify(hex(kTest2Message), hex(kTest2Signature)));
}

void AeroraEd25519VerifierTest::_theStandardVariantIsTheOneLinked()
{
    // The assertion that matters most in this suite, and the reason the vectors above are here at
    // all. Monocypher ships two signature APIs: crypto_eddsa_check (BLAKE2b, interoperable with
    // nothing) and crypto_ed25519_check (SHA-512, RFC 8032). Both have identical signatures, so
    // linking the wrong one compiles cleanly and fails only against a real signer.
    //
    // An RFC 8032 vector verifying is positive proof the SHA-512 variant is what got linked: the
    // BLAKE2b construction cannot produce or accept this signature for this key and message.
    // A build that silently fell back would fail here rather than in the field, where the symptom
    // would be a deployment unable to sign a credential with any standard tool.
    const AeroraEd25519Verifier verifier(hex(kTest2PublicKey));
    QVERIFY(verifier.verify(hex(kTest2Message), hex(kTest2Signature)));
}

void AeroraEd25519VerifierTest::_aTamperedMessageIsRefused()
{
    const AeroraEd25519Verifier verifier(hex(kTest2PublicKey));

    // One bit different from the signed message. This is the property the whole mechanism rests
    // on: editing a provisioned credential invalidates its signature.
    QVERIFY(!verifier.verify(hex("73"), hex(kTest2Signature)));
}

void AeroraEd25519VerifierTest::_aTamperedSignatureIsRefused()
{
    const AeroraEd25519Verifier verifier(hex(kTest2PublicKey));

    QByteArray corrupted = hex(kTest2Signature);
    corrupted[0] = static_cast<char>(corrupted[0] ^ 0x01);

    QVERIFY(!verifier.verify(hex(kTest2Message), corrupted));
}

void AeroraEd25519VerifierTest::_anotherIssuersKeyIsRefused()
{
    // Test 2's signature against Test 1's key: a correctly formed signature by the wrong issuer,
    // which is the forgery case rather than the corruption case.
    const AeroraEd25519Verifier verifier(hex(kTest1PublicKey));

    QVERIFY(!verifier.verify(hex(kTest2Message), hex(kTest2Signature)));
}

void AeroraEd25519VerifierTest::_aMalformedSignatureLengthIsRefused()
{
    const AeroraEd25519Verifier verifier(hex(kTest2PublicKey));

    QVERIFY(!verifier.verify(hex(kTest2Message), QByteArray()));
    QVERIFY(!verifier.verify(hex(kTest2Message), hex(kTest2Signature).left(63)));
    QVERIFY(!verifier.verify(hex(kTest2Message), hex(kTest2Signature) + QByteArray(1, '\0')));
}

void AeroraEd25519VerifierTest::_anUnusableKeyRefusesEverything()
{
    // The state a build with no configured issuer would reach if a caller installed a verifier
    // anyway: it reports itself unusable and refuses a signature that is otherwise perfectly good.
    const AeroraEd25519Verifier empty(QByteArray());
    QVERIFY(!empty.isUsable());
    QVERIFY(!empty.verify(hex(kTest2Message), hex(kTest2Signature)));

    const AeroraEd25519Verifier tooShort(hex(kTest2PublicKey).left(31));
    QVERIFY(!tooShort.isUsable());
    QVERIFY(!tooShort.verify(hex(kTest2Message), hex(kTest2Signature)));
}

void AeroraEd25519VerifierTest::_fromHexAcceptsAWellFormedKey()
{
    const std::unique_ptr<AeroraEd25519Verifier> verifier =
        AeroraEd25519Verifier::fromHex(QString::fromLatin1(kTest2PublicKey));

    QVERIFY(verifier != nullptr);
    QVERIFY(verifier->isUsable());
    QVERIFY(verifier->verify(hex(kTest2Message), hex(kTest2Signature)));

    // Case-insensitive and whitespace-tolerant, because the key arrives as a build-time string a
    // human pasted.
    const std::unique_ptr<AeroraEd25519Verifier> upper =
        AeroraEd25519Verifier::fromHex(QStringLiteral("  %1  ").arg(QString::fromLatin1(kTest2PublicKey).toUpper()));
    QVERIFY(upper != nullptr);
    QVERIFY(upper->verify(hex(kTest2Message), hex(kTest2Signature)));
}

void AeroraEd25519VerifierTest::_fromHexRejectsAnythingElse()
{
    // Empty is the no-issuer-configured path, and null is what keeps the station fail-closed: the
    // caller installs nothing rather than installing something that refuses everything.
    QVERIFY(AeroraEd25519Verifier::fromHex(QString()) == nullptr);
    QVERIFY(AeroraEd25519Verifier::fromHex(QStringLiteral("   ")) == nullptr);

    // Wrong length, and short by one hex character so the decode drops a byte.
    QVERIFY(AeroraEd25519Verifier::fromHex(QStringLiteral("abcd")) == nullptr);
    QVERIFY(AeroraEd25519Verifier::fromHex(QString::fromLatin1(kTest2PublicKey).left(63)) == nullptr);

    // Not hex at all. QByteArray::fromHex skips what it cannot parse rather than failing, so this
    // asserts the length check is what catches it.
    QVERIFY(AeroraEd25519Verifier::fromHex(QStringLiteral("not a key at all, not even close")) == nullptr);
}

void AeroraEd25519VerifierTest::_theVerifierNamesItsPrimitiveAndNotItsKey()
{
    const AeroraEd25519Verifier verifier(hex(kTest2PublicKey));

    QCOMPARE(verifier.verifierName(), QStringLiteral("Ed25519"));
    // The name reaches a status surface and the audit trail, so it must not carry key material.
    QVERIFY(!verifier.verifierName().contains(QString::fromLatin1(kTest2PublicKey)));
}

UT_REGISTER_TEST(AeroraEd25519VerifierTest, TestLabel::Unit)

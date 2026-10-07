#include "AeroraSigningKeyStoreTest.h"

#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QRegularExpression>
#include <QtCore/QSettings>
#include <QtCore/QString>
#include <QtTest/QTest>

#include "AeroraPlatformKeyProtector.h"
#include "AeroraSigningKeyStore.h"
#include "MAVLinkSigningKeyStore.h"

namespace {

constexpr const char* kStoreLogCategory = "Aerora.SigningKeyStore";

/// Subgroup the Aerora store writes under. Duplicated from the private constant deliberately: the
/// on-disk path is part of the contract, so pinning it here is the point of the assertion.
const QString kProtectedSubgroup = QStringLiteral("aeroraProtectedKeys");

/// Subgroup the stock QSettings backend writes raw bytes under.
const QString kStockSubgroup = QStringLiteral("keys");

const QString kKeyName = QStringLiteral("aerora-test-key");

/// 32 bytes, the MAVLink signing key size.
QByteArray plaintextKey()
{
    return QByteArray(32, '\x5a');
}

/// Reversible test double. **Not cryptography.** A recognisable prefix plus a byte-wise XOR is
/// enough to prove the store wrapped before writing and unwrapped on read, without pretending to
/// protect anything.
class ReversibleTestProtector : public AeroraPlatformKeyProtector
{
public:
    static constexpr char kMagic = '\x7e';
    static constexpr char kXorMask = '\x33';

    ReversibleTestProtector(bool available, bool claimsProtection)
        : _available(available)
        , _claimsProtection(claimsProtection) {}

    bool protect(const QByteArray& plaintext, QByteArray& outProtected) override
    {
        if (failProtect) {
            outProtected.clear();
            return false;
        }
        outProtected.clear();
        outProtected.append(kMagic);
        for (const char byte : plaintext) {
            outProtected.append(static_cast<char>(byte ^ kXorMask));
        }
        ++protectCalls;
        return true;
    }

    bool unprotect(const QByteArray& protectedBlob, QByteArray& outPlaintext) override
    {
        if (failUnprotect || protectedBlob.isEmpty() || protectedBlob.at(0) != kMagic) {
            outPlaintext.clear();
            return false;
        }
        outPlaintext.clear();
        for (qsizetype i = 1; i < protectedBlob.size(); ++i) {
            outPlaintext.append(static_cast<char>(protectedBlob.at(i) ^ kXorMask));
        }
        ++unprotectCalls;
        return true;
    }

    [[nodiscard]] bool isAvailable() const override { return _available; }
    [[nodiscard]] bool protectsAtRest() const override { return _claimsProtection; }
    [[nodiscard]] QString platformName() const override { return QStringLiteral("TestDouble"); }

    bool failProtect = false;
    bool failUnprotect = false;
    int protectCalls = 0;
    int unprotectCalls = 0;

private:
    bool _available;
    bool _claimsProtection;
};

QString protectedPath(const QString& name)
{
    return QStringLiteral("%1/%2").arg(kProtectedSubgroup, name);
}

QByteArray readRawSetting(const QString& relativePath)
{
    QSettings settings;
    settings.beginGroup(MAVLinkSigningKeyStore::kSettingsGroup);
    const QByteArray value = settings.value(relativePath).toByteArray();
    settings.endGroup();
    return value;
}

bool settingExists(const QString& relativePath)
{
    QSettings settings;
    settings.beginGroup(MAVLinkSigningKeyStore::kSettingsGroup);
    const bool present = settings.contains(relativePath);
    settings.endGroup();
    return present;
}

void writeRawSetting(const QString& relativePath, const QByteArray& value)
{
    QSettings settings;
    settings.beginGroup(MAVLinkSigningKeyStore::kSettingsGroup);
    settings.setValue(relativePath, value);
    settings.endGroup();
}

} // namespace

void AeroraSigningKeyStoreTest::cleanup()
{
    QSettings settings;
    settings.beginGroup(MAVLinkSigningKeyStore::kSettingsGroup);
    settings.remove(kProtectedSubgroup);
    settings.remove(kStockSubgroup);
    settings.endGroup();
    settings.sync();

    UnitTest::cleanup();
}

void AeroraSigningKeyStoreTest::_testUnusableProtectorRefuses_data()
{
    QTest::addColumn<bool>("withProtector");

    // Both rows must refuse, and must refuse with the same result: "I cannot protect this" is a
    // different statement from "something went wrong", and the caller needs the former.
    QTest::newRow("no protector at all") << false;
    QTest::newRow("protector present but unavailable") << true;
}

void AeroraSigningKeyStoreTest::_testUnusableProtectorRefuses()
{
    QFETCH(bool, withProtector);

    std::unique_ptr<AeroraPlatformKeyProtector> protector;
    if (withProtector) {
        protector = std::make_unique<ReversibleTestProtector>(false /* available */, true);
    }

    // The constructor announces an unusable protector. Asserting it here also guarantees the
    // station cannot end up in this state silently.
    expectLogMessage(kStoreLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("no usable platform key protector")));
    AeroraSigningKeyStore store(std::move(protector));
    verifyExpectedLogMessage();

    QVERIFY(!store.isUsable());

    // It must not claim protection it cannot deliver.
    QVERIFY(!store.protectsAtRest());

    QCOMPARE(store.storeKey(kKeyName, plaintextKey()),
             MAVLinkSigningKeyStoreResult::ProtectionUnavailable);

    QByteArray readBack;
    QCOMPARE(store.loadKey(kKeyName, readBack), MAVLinkSigningKeyStoreResult::ProtectionUnavailable);
    QVERIFY(readBack.isEmpty());

    // The decisive assertion: refusing produced no stored material of any kind.
    QVERIFY(!settingExists(protectedPath(kKeyName)));
    QVERIFY(!settingExists(QStringLiteral("%1/%2").arg(kStockSubgroup, kKeyName)));
}

void AeroraSigningKeyStoreTest::_testUnusableProtectorStillAllowsRemoval()
{
    // Deleting key material must never be blocked by an inability to read it, or a station that
    // loses its protector could never clear a key it no longer wants to hold.
    writeRawSetting(protectedPath(kKeyName), QByteArray("stale-protected-blob"));
    QVERIFY(settingExists(protectedPath(kKeyName)));

    expectLogMessage(kStoreLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("no usable platform key protector")));
    AeroraSigningKeyStore store(nullptr);
    verifyExpectedLogMessage();

    QCOMPARE(store.removeKey(kKeyName), MAVLinkSigningKeyStoreResult::Success);
    QVERIFY(!settingExists(protectedPath(kKeyName)));

    // Removing an absent entry is success, matching the backend contract.
    QCOMPARE(store.removeKey(kKeyName), MAVLinkSigningKeyStoreResult::Success);
}

void AeroraSigningKeyStoreTest::_testRoundTripThroughProtector()
{
    auto protector = std::make_unique<ReversibleTestProtector>(true, true);
    ReversibleTestProtector* const observer = protector.get();
    AeroraSigningKeyStore store(std::move(protector));

    QVERIFY(store.isUsable());
    QVERIFY(store.protectsAtRest());
    QVERIFY(store.backendName().contains(QStringLiteral("TestDouble")));

    const QByteArray key = plaintextKey();
    QCOMPARE(store.storeKey(kKeyName, key), MAVLinkSigningKeyStoreResult::Success);
    QCOMPARE(observer->protectCalls, 1);

    QByteArray readBack;
    QCOMPARE(store.loadKey(kKeyName, readBack), MAVLinkSigningKeyStoreResult::Success);
    QCOMPARE(observer->unprotectCalls, 1);
    QCOMPARE(readBack, key);

    QCOMPARE(store.flush(), MAVLinkSigningKeyStoreResult::Success);

    // A name that was never stored is NotFound, not an error.
    QByteArray absent;
    QCOMPARE(store.loadKey(QStringLiteral("no-such-key"), absent),
             MAVLinkSigningKeyStoreResult::NotFound);
}

void AeroraSigningKeyStoreTest::_testStoredBlobIsNotThePlaintextKey()
{
    // The whole point of D-002. If this passes while the stored bytes equal the key, the store is
    // an expensive no-op.
    AeroraSigningKeyStore store(std::make_unique<ReversibleTestProtector>(true, true));

    const QByteArray key = plaintextKey();
    QCOMPARE(store.storeKey(kKeyName, key), MAVLinkSigningKeyStoreResult::Success);

    const QByteArray stored = readRawSetting(protectedPath(kKeyName));
    QVERIFY(!stored.isEmpty());
    QVERIFY(stored != key);
    QVERIFY(!stored.contains(key));
}

void AeroraSigningKeyStoreTest::_testProtectFailureWritesNothing()
{
    auto protector = std::make_unique<ReversibleTestProtector>(true, true);
    protector->failProtect = true;
    AeroraSigningKeyStore store(std::move(protector));

    expectLogMessage(kStoreLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("protect\\(\\) failed")));
    QCOMPARE(store.storeKey(kKeyName, plaintextKey()), MAVLinkSigningKeyStoreResult::Failure);
    verifyExpectedLogMessage();

    // A failed wrap must not leave a partial or unwrapped entry behind.
    QVERIFY(!settingExists(protectedPath(kKeyName)));
}

void AeroraSigningKeyStoreTest::_testUnprotectFailureReportsCorruptData()
{
    // Damaged or tampered stored material must be reported as corrupt, not returned as a key.
    writeRawSetting(protectedPath(kKeyName), QByteArray("not-a-valid-protected-blob"));

    auto protector = std::make_unique<ReversibleTestProtector>(true, true);
    protector->failUnprotect = true;
    AeroraSigningKeyStore store(std::move(protector));

    QByteArray readBack(16, '\x11');  // pre-filled, to prove the store clears it
    expectLogMessage(kStoreLogCategory, QtWarningMsg,
                     QRegularExpression(QStringLiteral("unprotect\\(\\) failed")));
    QCOMPARE(store.loadKey(kKeyName, readBack), MAVLinkSigningKeyStoreResult::CorruptData);
    verifyExpectedLogMessage();

    QVERIFY(readBack.isEmpty());
}

void AeroraSigningKeyStoreTest::_testEmptyNameIsRejected()
{
    AeroraSigningKeyStore store(std::make_unique<ReversibleTestProtector>(true, true));

    QByteArray readBack;
    QCOMPARE(store.storeKey(QString(), plaintextKey()), MAVLinkSigningKeyStoreResult::Failure);
    QCOMPARE(store.loadKey(QString(), readBack), MAVLinkSigningKeyStoreResult::Failure);
    QCOMPARE(store.removeKey(QString()), MAVLinkSigningKeyStoreResult::Failure);
}

void AeroraSigningKeyStoreTest::_testAeroraPathIsSeparateFromStockPath()
{
    // A protected blob must never be read back as raw key bytes, or the reverse. The two backends
    // therefore occupy different subgroups, and neither sees the other's entries.
    AeroraSigningKeyStore aeroraStore(std::make_unique<ReversibleTestProtector>(true, true));
    MAVLinkSigningKeyStoreQSettings stockStore;

    QCOMPARE(aeroraStore.storeKey(kKeyName, plaintextKey()), MAVLinkSigningKeyStoreResult::Success);

    QByteArray viaStock;
    QCOMPARE(stockStore.loadKey(kKeyName, viaStock), MAVLinkSigningKeyStoreResult::NotFound);

    QCOMPARE(stockStore.storeKey(QStringLiteral("stock-only"), plaintextKey()),
             MAVLinkSigningKeyStoreResult::Success);

    QByteArray viaAerora;
    QCOMPARE(aeroraStore.loadKey(QStringLiteral("stock-only"), viaAerora),
             MAVLinkSigningKeyStoreResult::NotFound);
}

void AeroraSigningKeyStoreTest::_testNoPlatformProtectorIsShipped()
{
    // Pins the recorded D-002 / D-006 posture: the abstraction exists, no backend does. If a
    // platform backend is ever added, this test must fail and be updated deliberately, together
    // with the protection-at-rest claims in ADR-013 and the security implementation note.
    QVERIFY(AeroraPlatformKeyProtector::createForCurrentPlatform() == nullptr);
}
UT_REGISTER_TEST(AeroraSigningKeyStoreTest, TestLabel::Unit)

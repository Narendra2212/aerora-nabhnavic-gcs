#include "MAVLinkSigningKeyStoreTest.h"

#include <QtCore/QHash>
#include <QtCore/QSettings>
#include <QtTest/QTest>
#include <memory>

#include "MAVLinkSigning.h"
#include "MAVLinkSigningKeyStore.h"
#include "MAVLinkSigningKeys.h"

namespace {

/// 32 bytes expressed as 64 hex characters, which is what addRawKey() requires.
const QString kTestKeyHex = QString(64, QLatin1Char('a'));
const QString kTestKeyName = QStringLiteral("unit-test-key");

/// In-memory backend that records what it was asked to persist.
class FakeKeyStore : public MAVLinkSigningKeyStore
{
public:
    explicit FakeKeyStore(bool failWrites = false)
        : _failWrites(failWrites) {}

    MAVLinkSigningKeyStoreResult storeKey(const QString& name, const QByteArray& keyBytes) override
    {
        ++storeCalls;
        if (_failWrites) {
            return MAVLinkSigningKeyStoreResult::ProtectionUnavailable;
        }
        keys.insert(name, keyBytes);
        return MAVLinkSigningKeyStoreResult::Success;
    }

    MAVLinkSigningKeyStoreResult loadKey(const QString& name, QByteArray& outKeyBytes) override
    {
        if (!keys.contains(name)) {
            return MAVLinkSigningKeyStoreResult::NotFound;
        }
        outKeyBytes = keys.value(name);
        return MAVLinkSigningKeyStoreResult::Success;
    }

    MAVLinkSigningKeyStoreResult removeKey(const QString& name) override
    {
        keys.remove(name);
        return MAVLinkSigningKeyStoreResult::Success;
    }

    [[nodiscard]] bool protectsAtRest() const override { return !_failWrites; }
    [[nodiscard]] QString backendName() const override { return QStringLiteral("Fake"); }

    QHash<QString, QByteArray> keys;
    int storeCalls = 0;

private:
    bool _failWrites = false;
};

QStringList readManifest()
{
    QSettings settings;
    settings.beginGroup(MAVLinkSigningKeyStore::kSettingsGroup);
    const QStringList manifest = settings.value(QStringLiteral("manifest")).toStringList();
    settings.endGroup();
    return manifest;
}

} // namespace

void MAVLinkSigningKeyStoreTest::cleanup()
{
    // Leave no key material or injected backend behind for the next test.
    MAVLinkSigningKeys::instance()->removeAllKeys();
    MAVLinkSigningKeys::setKeyStore(nullptr);

    UnitTest::cleanup();
}

void MAVLinkSigningKeyStoreTest::_testDefaultBackendRoundTrip()
{
    MAVLinkSigningKeyStoreQSettings store;

    const QByteArray written(32, '\x5a');
    QCOMPARE(store.storeKey(kTestKeyName, written), MAVLinkSigningKeyStoreResult::Success);

    QByteArray readBack;
    QCOMPARE(store.loadKey(kTestKeyName, readBack), MAVLinkSigningKeyStoreResult::Success);
    QCOMPARE(readBack, written);

    QCOMPARE(store.removeKey(kTestKeyName), MAVLinkSigningKeyStoreResult::Success);
}

void MAVLinkSigningKeyStoreTest::_testDefaultBackendReportsNotFound()
{
    MAVLinkSigningKeyStoreQSettings store;

    QByteArray readBack;
    QCOMPARE(store.loadKey(QStringLiteral("no-such-key-exists"), readBack),
             MAVLinkSigningKeyStoreResult::NotFound);
    QVERIFY(readBack.isEmpty());
}

void MAVLinkSigningKeyStoreTest::_testDefaultBackendRejectsEmptyName()
{
    MAVLinkSigningKeyStoreQSettings store;

    QByteArray readBack;
    QCOMPARE(store.storeKey(QString(), QByteArray(32, '\x01')), MAVLinkSigningKeyStoreResult::Failure);
    QCOMPARE(store.loadKey(QString(), readBack), MAVLinkSigningKeyStoreResult::Failure);
    QCOMPARE(store.removeKey(QString()), MAVLinkSigningKeyStoreResult::Failure);
}

void MAVLinkSigningKeyStoreTest::_testDefaultBackendRemoveKey()
{
    MAVLinkSigningKeyStoreQSettings store;

    QCOMPARE(store.storeKey(kTestKeyName, QByteArray(32, '\x7f')), MAVLinkSigningKeyStoreResult::Success);
    QCOMPARE(store.removeKey(kTestKeyName), MAVLinkSigningKeyStoreResult::Success);

    QByteArray readBack;
    QCOMPARE(store.loadKey(kTestKeyName, readBack), MAVLinkSigningKeyStoreResult::NotFound);

    // Removing an absent entry is success, not failure.
    QCOMPARE(store.removeKey(kTestKeyName), MAVLinkSigningKeyStoreResult::Success);
}

void MAVLinkSigningKeyStoreTest::_testDefaultBackendDoesNotClaimProtection()
{
    // The default backend stores raw bytes. It must never report that it protects them.
    MAVLinkSigningKeyStoreQSettings store;
    QVERIFY(!store.protectsAtRest());
}

void MAVLinkSigningKeyStoreTest::_testResultToStringIsNeverUnknown()
{
    const MAVLinkSigningKeyStoreResult all[] = {
        MAVLinkSigningKeyStoreResult::Success,
        MAVLinkSigningKeyStoreResult::NotFound,
        MAVLinkSigningKeyStoreResult::Unavailable,
        MAVLinkSigningKeyStoreResult::ProtectionUnavailable,
        MAVLinkSigningKeyStoreResult::PermissionDenied,
        MAVLinkSigningKeyStoreResult::CorruptData,
        MAVLinkSigningKeyStoreResult::Failure,
    };

    for (const MAVLinkSigningKeyStoreResult result : all) {
        const QString text = MAVLinkSigningKeyStore::resultToString(result);
        QVERIFY(!text.isEmpty());
        QVERIFY(text != QStringLiteral("Unknown"));
    }
}

void MAVLinkSigningKeyStoreTest::_testKeyStoreDefaultsToQSettings()
{
    MAVLinkSigningKeys::setKeyStore(nullptr);

    MAVLinkSigningKeyStore* const store = MAVLinkSigningKeys::keyStore();
    QVERIFY(store != nullptr);
    QCOMPARE(store->backendName(), QStringLiteral("QSettings"));
    QVERIFY(!store->protectsAtRest());
}

void MAVLinkSigningKeyStoreTest::_testInjectedBackendReceivesKeyMaterial()
{
    auto fake = std::make_unique<FakeKeyStore>();
    FakeKeyStore* const observer = fake.get();
    MAVLinkSigningKeys::setKeyStore(std::move(fake));

    MAVLinkSigningKeys* const keys = MAVLinkSigningKeys::instance();
    keys->removeAllKeys();

    QVERIFY(keys->addRawKey(kTestKeyName, kTestKeyHex));

    // The backend, not QSettings, received the secret material.
    QVERIFY(observer->storeCalls > 0);
    QVERIFY(observer->keys.contains(kTestKeyName));
    QCOMPARE(observer->keys.value(kTestKeyName).size(), 32);

    // The manifest still tracks the name, since the manifest holds no secrets.
    QVERIFY(readManifest().contains(kTestKeyName));

    keys->removeKey(kTestKeyName);
    QVERIFY(!observer->keys.contains(kTestKeyName));
}

void MAVLinkSigningKeyStoreTest::_testCorruptStoredMaterialCannotBecomeAKey_data()
{
    QTest::addColumn<QByteArray>("stored");

    // The backend is a byte store and does not validate size; makeSigningKey() is the gate.
    // These rows are the shapes a damaged or tampered settings entry can actually take.
    QTest::newRow("empty") << QByteArray();
    QTest::newRow("truncated") << QByteArray(16, '\x01');
    QTest::newRow("one byte short") << QByteArray(31, '\x01');
    QTest::newRow("one byte long") << QByteArray(33, '\x01');
    QTest::newRow("oversized") << QByteArray(4096, '\x01');
}

void MAVLinkSigningKeyStoreTest::_testCorruptStoredMaterialCannotBecomeAKey()
{
    QFETCH(QByteArray, stored);

    MAVLinkSigningKeyStoreQSettings store;

    // Corrupt material round-trips through the backend unchanged: the backend's contract is
    // storage, not validation. Reporting Success here is correct and is not a claim that the
    // bytes are a usable key.
    const QString name = QStringLiteral("corrupt-entry");
    if (!stored.isEmpty()) {
        QCOMPARE(store.storeKey(name, stored), MAVLinkSigningKeyStoreResult::Success);

        QByteArray readBack;
        QCOMPARE(store.loadKey(name, readBack), MAVLinkSigningKeyStoreResult::Success);
        QCOMPARE(readBack, stored);
    }

    // The property that matters: wrong-sized material can never become an installed key,
    // so a damaged settings entry degrades to a missing key rather than a weak one.
    QVERIFY(!MAVLinkSigning::makeSigningKey(stored).has_value());

    QCOMPARE(store.removeKey(name), MAVLinkSigningKeyStoreResult::Success);
}

void MAVLinkSigningKeyStoreTest::_testFailingBackendIsNotListedInManifest()
{
    auto fake = std::make_unique<FakeKeyStore>(true /* failWrites */);
    FakeKeyStore* const observer = fake.get();
    MAVLinkSigningKeys::setKeyStore(std::move(fake));

    MAVLinkSigningKeys* const keys = MAVLinkSigningKeys::instance();
    keys->removeAllKeys();

    keys->addRawKey(kTestKeyName, kTestKeyHex);

    // The backend refused, so nothing was persisted...
    QVERIFY(observer->keys.isEmpty());

    // ...and the manifest must not claim material that cannot be read back.
    QVERIFY(!readManifest().contains(kTestKeyName));
}
UT_REGISTER_TEST(MAVLinkSigningKeyStoreTest, TestLabel::Unit)

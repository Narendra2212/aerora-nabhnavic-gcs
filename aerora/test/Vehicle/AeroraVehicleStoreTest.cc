#include "AeroraVehicleStoreTest.h"

#include <QtCore/QByteArray>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QTemporaryDir>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtTest/QTest>

#include "AeroraCorePlugin.h"
#include "AeroraLocalStore.h"
#include "AeroraVehicleStore.h"
#include "QGCSqlHelper.h"

namespace {

/// The canonical form `DG-FLT` pins: lowercase hex, hyphenated into the five RFC 4122 groups, no
/// braces. The version/variant nibbles are deliberately not pinned here - `QUuid::createUuid()`'s
/// own documentation only guarantees UUIDv4 off Windows, so asserting the shape is what the
/// decision requires and asserting the nibbles would be asserting a platform guarantee Qt itself
/// does not make.
const QRegularExpression& canonicalUuidPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"));
    return pattern;
}

int rowCount(const QString& path, const QString& table)
{
    QGCSqlHelper::ScopedConnection connection(path);
    if (!connection.isValid()) {
        return -1;
    }
    QSqlQuery query(connection.database());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table)) || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

/// Reads `firstSeenUtcMs`/`lastSeenUtcMs` for \a boardUid, or (-1, -1) if the row does not exist.
QPair<qint64, qint64> seenTimes(const QString& path, const QString& boardUid)
{
    QGCSqlHelper::ScopedConnection connection(path);
    if (!connection.isValid()) {
        return {-1, -1};
    }
    QSqlQuery query(connection.database());
    query.prepare(QStringLiteral("SELECT firstSeenUtcMs, lastSeenUtcMs FROM Vehicles WHERE boardUid = ?"));
    query.addBindValue(boardUid);
    if (!query.exec() || !query.next()) {
        return {-1, -1};
    }
    return {query.value(0).toLongLong(), query.value(1).toLongLong()};
}

}  // namespace

void AeroraVehicleStoreTest::_mintsACanonicalIdentityForAnUnknownBoard()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    AeroraVehicleStore vehicles(store);
    QVERIFY(vehicles.open());

    const QString identity = vehicles.resolveFleetIdentity(QStringLiteral("board-aabbcc"), 1700000000000LL);

    QVERIFY(!identity.isEmpty());
    QVERIFY2(canonicalUuidPattern().match(identity).hasMatch(), qPrintable(identity));
    QCOMPARE(rowCount(path, QStringLiteral("Vehicles")), 1);
}

void AeroraVehicleStoreTest::_recallsTheSameIdentityOnASecondLookup()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    AeroraVehicleStore vehicles(store);
    QVERIFY(vehicles.open());

    const QString boardUid = QStringLiteral("board-recalled");
    const QString minted = vehicles.resolveFleetIdentity(boardUid, 1700000000000LL);
    QVERIFY(!minted.isEmpty());

    const QString recalled = vehicles.resolveFleetIdentity(boardUid, 1700000005000LL);

    QCOMPARE(recalled, minted);
    // One lookup and one recall of the same board is one row, never two.
    QCOMPARE(rowCount(path, QStringLiteral("Vehicles")), 1);
}

void AeroraVehicleStoreTest::_distinctBoardsGetDistinctIdentities()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    AeroraVehicleStore vehicles(store);
    QVERIFY(vehicles.open());

    const QString first = vehicles.resolveFleetIdentity(QStringLiteral("board-one"), 1700000000000LL);
    const QString second = vehicles.resolveFleetIdentity(QStringLiteral("board-two"), 1700000000000LL);

    QVERIFY(!first.isEmpty());
    QVERIFY(!second.isEmpty());
    QVERIFY(first != second);
    QCOMPARE(rowCount(path, QStringLiteral("Vehicles")), 2);
}

void AeroraVehicleStoreTest::_lastSeenIsTouchedButFirstSeenIsNot()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    AeroraVehicleStore vehicles(store);
    QVERIFY(vehicles.open());

    const QString boardUid = QStringLiteral("board-timestamps");
    const qint64 mintedAt = 1700000000000LL;
    const qint64 seenAgainAt = 1700003600000LL;  // an hour later

    QVERIFY(!vehicles.resolveFleetIdentity(boardUid, mintedAt).isEmpty());
    const QPair<qint64, qint64> afterMint = seenTimes(path, boardUid);
    QCOMPARE(afterMint.first, mintedAt);
    QCOMPARE(afterMint.second, mintedAt);

    QVERIFY(!vehicles.resolveFleetIdentity(boardUid, seenAgainAt).isEmpty());
    const QPair<qint64, qint64> afterRecall = seenTimes(path, boardUid);
    QCOMPARE(afterRecall.first, mintedAt);      // unchanged
    QCOMPARE(afterRecall.second, seenAgainAt);  // touched
}

void AeroraVehicleStoreTest::_emptyBoardUidIsRefusedWithoutMinting()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    AeroraVehicleStore vehicles(store);
    QVERIFY(vehicles.open());

    QVERIFY(vehicles.resolveFleetIdentity(QString(), 1700000000000LL).isEmpty());
    QCOMPARE(rowCount(path, QStringLiteral("Vehicles")), 0);
}

void AeroraVehicleStoreTest::_resolveBeforeOpenReturnsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    AeroraVehicleStore vehicles(store);  // deliberately not opened

    QVERIFY(vehicles.resolveFleetIdentity(QStringLiteral("board-unopened"), 1700000000000LL).isEmpty());
}

void AeroraVehicleStoreTest::_openFailsWhenStoreIsNotOpen()
{
    AeroraLocalStore store;  // deliberately not opened
    AeroraVehicleStore vehicles(store);

    expectLogMessage("Aerora.Vehicle", QtWarningMsg, QRegularExpression(QStringLiteral("store is not open")));
    QVERIFY(!vehicles.open());
    verifyExpectedLogMessage();

    QVERIFY(!vehicles.isOpen());
}

void AeroraVehicleStoreTest::_boardUidPrefersUid2WhenItCarriesEvidence()
{
    QByteArray uid2(18, '\0');
    uid2[11] = '\x2A';  // one non-zero byte is enough to be "present"

    const QString boardUid = AeroraCorePlugin::boardUidFromAutopilotVersion(0xABCDEF, uid2);

    QCOMPARE(boardUid, QString::fromLatin1(uid2.toHex()));
}

void AeroraVehicleStoreTest::_boardUidFallsBackToUidWhenUid2IsAllZero()
{
    const QByteArray uid2(18, '\0');
    const quint64 uid = 0x1234567890ABCDEFULL;

    const QString boardUid = AeroraCorePlugin::boardUidFromAutopilotVersion(uid, uid2);

    QCOMPARE(boardUid, QString::number(uid, 16));
}

void AeroraVehicleStoreTest::_boardUidIsEmptyWhenNeitherCarriesEvidence()
{
    // MockLink's simulated AUTOPILOT_VERSION reply is exactly this state: uid and uid2 both zero.
    const QByteArray uid2(18, '\0');

    QVERIFY(AeroraCorePlugin::boardUidFromAutopilotVersion(0, uid2).isEmpty());
}

void AeroraVehicleStoreTest::_boardUidHexEncodingIsLowercase()
{
    QByteArray uid2(18, '\0');
    uid2[0] = '\xAB';  // a byte whose hex digits would differ visibly by case

    const QString boardUid = AeroraCorePlugin::boardUidFromAutopilotVersion(0, uid2);

    QCOMPARE(boardUid, boardUid.toLower());
    QVERIFY(boardUid.startsWith(QStringLiteral("ab")));
}

UT_REGISTER_TEST(AeroraVehicleStoreTest, TestLabel::Unit)

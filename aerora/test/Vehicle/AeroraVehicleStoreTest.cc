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
#include "AeroraVehicleIdentity.h"
#include "AeroraVehicleStore.h"
#include "MAVLinkLib.h"
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

/// An 18-byte `uid2` whose first byte is \a marker and the rest zero - enough to count as present.
QByteArray boardUid2(char marker)
{
    QByteArray uid2(18, '\0');
    uid2[0] = marker;
    return uid2;
}

/// A real packed `AUTOPILOT_VERSION` carrying \a uid and \a uid2, so a test exercises the same
/// decode the live path does rather than a hand-built field.
mavlink_message_t autopilotVersionMessage(quint64 uid, const QByteArray& uid2)
{
    const QByteArray customVersion(8, '\0');
    mavlink_message_t message{};
    (void) mavlink_msg_autopilot_version_pack(1,  // system id
                                              1,  // component id
                                              &message,
                                              0,  // capabilities
                                              0,  // flight_sw_version
                                              0,  // middleware_sw_version
                                              0,  // os_sw_version
                                              0,  // board_version
                                              reinterpret_cast<const uint8_t*>(customVersion.constData()),
                                              reinterpret_cast<const uint8_t*>(customVersion.constData()),
                                              reinterpret_cast<const uint8_t*>(customVersion.constData()),
                                              0,  // vendor_id
                                              0,  // product_id
                                              uid, reinterpret_cast<const uint8_t*>(uid2.constData()));
    return message;
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

void AeroraVehicleStoreTest::_aRealMessageResolvesAnIdentityIntoTheVehicle()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    AeroraVehicleStore vehicles(store);
    QVERIFY(vehicles.open());

    AeroraVehicleIdentity identity(7, 1);
    QVERIFY(!identity.fleetIdentityKnown());

    // A real packed AUTOPILOT_VERSION, so the decode this exercises is the one that runs on every
    // vehicle connect - including the uid2 byte-array extraction, which a hand-built QByteArray
    // test cannot cover.
    const mavlink_message_t message = autopilotVersionMessage(0, boardUid2(0x5A));

    const QString reported = AeroraCorePlugin::reportFleetIdentityFrom(vehicles, identity, message, 1700000000000LL);

    QVERIFY(!reported.isEmpty());
    QVERIFY(identity.fleetIdentityKnown());
    QCOMPARE(identity.fleetIdentity(), reported);
    QVERIFY2(canonicalUuidPattern().match(reported).hasMatch(), qPrintable(reported));

    // The identity is bound to the board, so the same board on a later connect recalls it rather
    // than minting a second one - asserted here through the real decode path, not just the store.
    AeroraVehicleIdentity reconnected(9, 1);  // a different MAVLink system id, the same board
    const QString recalled = AeroraCorePlugin::reportFleetIdentityFrom(vehicles, reconnected, message, 1700000005000LL);

    QCOMPARE(recalled, reported);
    QCOMPARE(rowCount(path, QStringLiteral("Vehicles")), 1);
}

void AeroraVehicleStoreTest::_aMessageWithNoBoardEvidenceReportsNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    AeroraVehicleStore vehicles(store);
    QVERIFY(vehicles.open());

    AeroraVehicleIdentity identity(7, 1);

    // uid and uid2 both zero, which is exactly what MockLink's simulated reply carries. The
    // vehicle stays a first contact rather than being minted against a sentinel.
    const mavlink_message_t message = autopilotVersionMessage(0, QByteArray(18, '\0'));

    QVERIFY(AeroraCorePlugin::reportFleetIdentityFrom(vehicles, identity, message, 1700000000000LL).isEmpty());
    QVERIFY(!identity.fleetIdentityKnown());
    QVERIFY(identity.fleetIdentity().isEmpty());
    QCOMPARE(rowCount(path, QStringLiteral("Vehicles")), 0);
}

void AeroraVehicleStoreTest::_aLegacyUidOnlyMessageStillResolves()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(path, false).opened);

    AeroraVehicleStore vehicles(store);
    QVERIFY(vehicles.open());

    AeroraVehicleIdentity identity(7, 1);

    // An autopilot that fills only the legacy 8-byte uid: the documented fallback, and the case a
    // uid2-only implementation would silently fail to identify.
    const mavlink_message_t message = autopilotVersionMessage(0x1234567890ABCDEFULL, QByteArray(18, '\0'));

    const QString reported = AeroraCorePlugin::reportFleetIdentityFrom(vehicles, identity, message, 1700000000000LL);

    QVERIFY(!reported.isEmpty());
    QCOMPARE(identity.fleetIdentity(), reported);
    QCOMPARE(rowCount(path, QStringLiteral("Vehicles")), 1);
}

UT_REGISTER_TEST(AeroraVehicleStoreTest, TestLabel::Unit)

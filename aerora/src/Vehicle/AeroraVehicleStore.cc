#include "AeroraVehicleStore.h"

#include <QtCore/QUuid.h>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "AeroraLocalStore.h"
#include "AeroraVehicleLog.h"
#include "QGCSqlHelper.h"

AeroraVehicleStore::AeroraVehicleStore(AeroraLocalStore& store)
    : _store(store)
{}

AeroraVehicleStore::~AeroraVehicleStore() = default;

bool AeroraVehicleStore::open()
{
    if (_ready) {
        return true;
    }

    if (!_store.isOpen()) {
        qCWarning(AeroraVehicleLog) << "cannot open the vehicle store: the store is not open";
        return false;
    }

    const QString path = _store.databasePath();
    if (path.isEmpty()) {
        qCWarning(AeroraVehicleLog) << "cannot open the vehicle store: the store has no database path";
        return false;
    }

    _connection = std::make_unique<QGCSqlHelper::ScopedConnection>(path, false, QStringLiteral("AeroraVehicles"));
    if (!_connection->isValid()) {
        _connection.reset();
        qCWarning(AeroraVehicleLog) << "cannot open the vehicle store: the database did not open" << path;
        return false;
    }

    _ready = true;
    return true;
}

void AeroraVehicleStore::close()
{
    _connection.reset();
    _ready = false;
}

QString AeroraVehicleStore::resolveFleetIdentity(const QString& boardUid, qint64 nowUtcMs)
{
    if (!_ready) {
        return QString();
    }

    if (boardUid.isEmpty()) {
        // No board identifier is no evidence, not a reason to mint one: the decision's authority
        // acts "on first contact" with a board, and a caller with nothing to look up has had none.
        return QString();
    }

    QSqlDatabase db = _connection->database();
    QGCSqlHelper::Transaction transaction(db);
    if (!transaction.ok()) {
        _store.recordWriteFailure(QStringLiteral("Vehicles"), QStringLiteral("could not begin a transaction"));
        return QString();
    }

    QSqlQuery lookup(db);
    lookup.prepare(QStringLiteral("SELECT fleetIdentity FROM Vehicles WHERE boardUid = ?"));
    lookup.addBindValue(boardUid);
    if (!lookup.exec()) {
        _store.recordWriteFailure(QStringLiteral("Vehicles"), lookup.lastError().text());
        return QString();
    }

    if (lookup.next()) {
        // Already bound: recall it, and touch lastSeenUtcMs so the row reflects this contact.
        const QString existing = lookup.value(0).toString();

        QSqlQuery touch(db);
        touch.prepare(QStringLiteral("UPDATE Vehicles SET lastSeenUtcMs = ? WHERE boardUid = ?"));
        touch.addBindValue(nowUtcMs);
        touch.addBindValue(boardUid);
        if (!touch.exec()) {
            _store.recordWriteFailure(QStringLiteral("Vehicles"), touch.lastError().text());
            return QString();
        }

        if (!transaction.commit()) {
            _store.recordWriteFailure(QStringLiteral("Vehicles"), QStringLiteral("commit failed"));
            return QString();
        }

        return existing;
    }

    // Not yet known: mint and bind. Lower-cased explicitly rather than trusting a platform's
    // default casing, because the decision pins the canonical form as lowercase (design, §form).
    const QString minted = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();

    QSqlQuery insert(db);
    insert.prepare(
        QStringLiteral("INSERT INTO Vehicles (boardUid, fleetIdentity, profileId, firstSeenUtcMs, lastSeenUtcMs) "
                       "VALUES (?, ?, NULL, ?, ?)"));
    insert.addBindValue(boardUid);
    insert.addBindValue(minted);
    insert.addBindValue(nowUtcMs);
    insert.addBindValue(nowUtcMs);
    if (!insert.exec()) {
        _store.recordWriteFailure(QStringLiteral("Vehicles"), insert.lastError().text());
        return QString();
    }

    if (!transaction.commit()) {
        _store.recordWriteFailure(QStringLiteral("Vehicles"), QStringLiteral("commit failed"));
        return QString();
    }

    qCDebug(AeroraVehicleLog) << "minted a fleet identity for a board seen for the first time";

    return minted;
}

#include "AeroraLocalStore.h"

#include <optional>

#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "QGCFileHelper.h"
#include "QGCLoggingCategory.h"
#include "QGCSqlHelper.h"

QGC_LOGGING_CATEGORY(AeroraPersistenceLog, "aerora.persistence")

AeroraLocalStore::AeroraLocalStore() = default;

AeroraLocalStore::~AeroraLocalStore() = default;

AeroraLocalStore::OpenResult AeroraLocalStore::open(const QString& databasePath, bool persistenceDisabled)
{
    close();
    _databasePath = databasePath;

    OpenResult result;

    if (persistenceDisabled) {
        _persistenceDisabled = true;
        result.persistenceDisabled = true;
        result.detail = QStringLiteral("persistence is disabled; no database was opened");
        qCDebug(AeroraPersistenceLog) << result.detail;
        return result;
    }

    if (databasePath.isEmpty()) {
        result.detail = QStringLiteral("no database path was provided");
        qCWarning(AeroraPersistenceLog) << result.detail;
        return result;
    }

    if (!QGCFileHelper::ensureParentExists(databasePath)) {
        result.detail = QStringLiteral("could not create the directory for the database");
        qCWarning(AeroraPersistenceLog) << result.detail << databasePath;
        return result;
    }

    _connection = std::make_unique<QGCSqlHelper::ScopedConnection>(databasePath, false, QStringLiteral("AeroraStore"));
    if (!_connection->isValid()) {
        _connection.reset();
        result.detail = QStringLiteral("could not open the database file");
        qCWarning(AeroraPersistenceLog) << result.detail << databasePath;
        return result;
    }

    if (!_prepareSchema(result)) {
        // Close without deleting the file: a refused version or a failed open must preserve the
        // only copy of records that are not reconstructible (design §5, §11).
        _connection.reset();
        return result;
    }

    _open = true;
    return result;
}

void AeroraLocalStore::close()
{
    _connection.reset();
    _open = false;
    _persistenceDisabled = false;
    _currentSessionId = -1;
    _unrecordedWriteFailures = 0;
}

qint64 AeroraLocalStore::beginSession(const SessionRecord& record)
{
    if (!_open) {
        ++_unrecordedWriteFailures;
        qCWarning(AeroraPersistenceLog) << "beginSession called on a store that is not open";
        return -1;
    }

    QSqlDatabase db = _connection->database();
    QGCSqlHelper::Transaction transaction(db);
    if (!transaction.ok()) {
        ++_unrecordedWriteFailures;
        qCWarning(AeroraPersistenceLog) << "could not begin a transaction for a new session";
        return -1;
    }

    QSqlQuery query(db);
    query.prepare(
        QStringLiteral("INSERT INTO Sessions (startedUtcMs, endedUtcMs, baseCommit, overlayCommit, "
                       "customBuild, authzEnforced) VALUES (?, NULL, ?, ?, ?, ?)"));
    query.addBindValue(record.startedUtcMs);
    query.addBindValue(record.baseCommit);
    query.addBindValue(record.overlayCommit);
    query.addBindValue(record.customBuild ? 1 : 0);
    query.addBindValue(record.authorizationEnforced ? 1 : 0);
    if (!query.exec()) {
        // No session exists yet, so this cannot be attributed to one in WriteFailures; counted in
        // memory instead of silently lost.
        ++_unrecordedWriteFailures;
        qCWarning(AeroraPersistenceLog) << "session insert failed:" << query.lastError().text();
        return -1;
    }

    const qint64 sessionId = query.lastInsertId().toLongLong();
    if (!transaction.commit()) {
        ++_unrecordedWriteFailures;
        qCWarning(AeroraPersistenceLog) << "session commit failed:" << db.lastError().text();
        return -1;
    }

    _currentSessionId = sessionId;
    return sessionId;
}

bool AeroraLocalStore::endSession(qint64 sessionId, qint64 endedUtcMs)
{
    if (!_open) {
        ++_unrecordedWriteFailures;
        qCWarning(AeroraPersistenceLog) << "endSession called on a store that is not open";
        return false;
    }

    QSqlDatabase db = _connection->database();
    QGCSqlHelper::Transaction transaction(db);
    if (!transaction.ok()) {
        ++_unrecordedWriteFailures;
        return false;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral("UPDATE Sessions SET endedUtcMs = ? WHERE sessionId = ?"));
    query.addBindValue(endedUtcMs);
    query.addBindValue(sessionId);
    if (!query.exec()) {
        ++_unrecordedWriteFailures;
        qCWarning(AeroraPersistenceLog) << "session end update failed:" << query.lastError().text();
        return false;
    }

    if (!transaction.commit()) {
        ++_unrecordedWriteFailures;
        qCWarning(AeroraPersistenceLog) << "session end commit failed:" << db.lastError().text();
        return false;
    }

    return true;
}

void AeroraLocalStore::recordWriteFailure(const QString& targetTable, const QString& reason)
{
    if (_open && (_currentSessionId >= 0)) {
        QSqlDatabase db = _connection->database();
        QSqlQuery query(db);
        query.prepare(
            QStringLiteral("INSERT INTO WriteFailures (sessionId, occurredUtcMs, targetTable, reason) "
                           "VALUES (?, ?, ?, ?)"));
        query.addBindValue(_currentSessionId);
        query.addBindValue(QDateTime::currentMSecsSinceEpoch());
        query.addBindValue(targetTable);
        query.addBindValue(reason);
        if (query.exec()) {
            return;
        }
        // WriteFailures itself failed: fall through to the in-memory counter so the loss is still
        // visible rather than doubly swallowed (design §11).
        qCWarning(AeroraPersistenceLog) << "could not record a write failure:" << query.lastError().text();
    }
    ++_unrecordedWriteFailures;
}

int AeroraLocalStore::recordedWriteFailureCount() const
{
    if (!_open) {
        return 0;
    }

    QSqlDatabase db = _connection->database();
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM WriteFailures")) || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

bool AeroraLocalStore::_prepareSchema(OpenResult& result)
{
    QSqlDatabase db = _connection->database();
    const std::optional<int> version = QGCSqlHelper::userVersion(db);
    if (!version) {
        result.detail = QStringLiteral("could not read the database schema version");
        qCWarning(AeroraPersistenceLog) << result.detail << _databasePath;
        return false;
    }

    const int found = *version;
    if (found == kSchemaVersion) {
        result.opened = true;
        result.detail = QStringLiteral("opened an existing store at schema version %1").arg(found);
        return true;
    }

    if (found == 0) {
        if (_hasUserTable()) {
            // Version 0 with data is a legacy unversioned file. A cache would reset it; a record
            // store must not, so it is refused and preserved (design §5).
            result.versionUnsupported = true;
            result.schemaVersionFound = 0;
            result.detail = QStringLiteral(
                "refusing to open an unversioned database that already holds data; the file is preserved");
            qCWarning(AeroraPersistenceLog) << result.detail << _databasePath;
            return false;
        }

        if (!_createSchema()) {
            result.detail = QStringLiteral("could not create the store schema");
            qCWarning(AeroraPersistenceLog) << result.detail << _databasePath;
            return false;
        }

        result.opened = true;
        result.detail = QStringLiteral("created a new store at schema version %1").arg(kSchemaVersion);
        return true;
    }

    // Any other version refuses and preserves the file: a newer one this build cannot read, or an
    // older one with no forward migration (none exists yet, design §5).
    result.versionUnsupported = true;
    result.schemaVersionFound = found;
    result.detail = (found > kSchemaVersion)
                        ? QStringLiteral(
                              "refusing to open a store written by a newer build "
                              "(schema version %1 > %2); the file is preserved")
                              .arg(found)
                              .arg(kSchemaVersion)
                        : QStringLiteral(
                              "refusing to open schema version %1: no forward migration to %2 exists; "
                              "the file is preserved")
                              .arg(found)
                              .arg(kSchemaVersion);
    qCWarning(AeroraPersistenceLog) << result.detail << _databasePath;
    return false;
}

bool AeroraLocalStore::_createSchema()
{
    QSqlDatabase db = _connection->database();
    QGCSqlHelper::Transaction transaction(db);
    if (!transaction.ok()) {
        return false;
    }

    const QStringList statements = {
        QStringLiteral("CREATE TABLE Sessions ("
                       "sessionId     INTEGER PRIMARY KEY NOT NULL, "
                       "startedUtcMs  INTEGER NOT NULL, "
                       "endedUtcMs    INTEGER, "
                       "baseCommit    TEXT NOT NULL, "
                       "overlayCommit TEXT NOT NULL, "
                       "customBuild   INTEGER NOT NULL, "
                       "authzEnforced INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE Vehicles ("
                       "vehicleId      INTEGER PRIMARY KEY NOT NULL, "
                       "fleetIdentity  TEXT NOT NULL UNIQUE, "
                       "profileId      TEXT, "
                       "firstSeenUtcMs INTEGER NOT NULL, "
                       "lastSeenUtcMs  INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE Events ("
                       "eventId       INTEGER PRIMARY KEY NOT NULL, "
                       "sessionId     INTEGER NOT NULL REFERENCES Sessions(sessionId) ON DELETE CASCADE, "
                       "vehicleId     INTEGER REFERENCES Vehicles(vehicleId) ON DELETE SET NULL, "
                       "occurredUtcMs INTEGER NOT NULL, "
                       "monotonicUs   INTEGER NOT NULL, "
                       "category      INTEGER NOT NULL, "
                       "subject       TEXT NOT NULL, "
                       "stateClass    INTEGER, "
                       "detail        TEXT)"),
        QStringLiteral("CREATE TABLE EventValues ("
                       "eventId    INTEGER NOT NULL REFERENCES Events(eventId) ON DELETE CASCADE, "
                       "itemId     TEXT NOT NULL, "
                       "stateClass INTEGER NOT NULL, "
                       "derived    INTEGER NOT NULL, "
                       "valueText  TEXT, "
                       "ageMs      INTEGER)"),
        // AUTOINCREMENT must immediately follow PRIMARY KEY in SQLite, so the design's
        // "PRIMARY KEY NOT NULL AUTOINCREMENT" is written in its valid equivalent: INTEGER PRIMARY
        // KEY AUTOINCREMENT is implicitly NOT NULL and never reuses a rowid (design §4).
        QStringLiteral("CREATE TABLE AuditRecords ("
                       "recordId      INTEGER PRIMARY KEY AUTOINCREMENT, "
                       "sessionId     INTEGER NOT NULL REFERENCES Sessions(sessionId) ON DELETE RESTRICT, "
                       "occurredUtcMs INTEGER NOT NULL, "
                       "monotonicUs   INTEGER NOT NULL, "
                       "actor         TEXT NOT NULL, "
                       "action        TEXT NOT NULL, "
                       "subject       TEXT NOT NULL, "
                       "outcome       INTEGER NOT NULL, "
                       "enforced      INTEGER NOT NULL, "
                       "detail        TEXT, "
                       "prevHash      TEXT, "
                       "recordHash    TEXT)"),
        QStringLiteral("CREATE TABLE WriteFailures ("
                       "failureId     INTEGER PRIMARY KEY NOT NULL, "
                       "sessionId     INTEGER NOT NULL REFERENCES Sessions(sessionId) ON DELETE CASCADE, "
                       "occurredUtcMs INTEGER NOT NULL, "
                       "targetTable   TEXT NOT NULL, "
                       "reason        TEXT NOT NULL)"),
        QStringLiteral("CREATE INDEX idx_events_session_monotonic ON Events(sessionId, monotonicUs)"),
        QStringLiteral("CREATE INDEX idx_events_category ON Events(category, occurredUtcMs)"),
        QStringLiteral("CREATE INDEX idx_eventvalues_event ON EventValues(eventId)"),
        QStringLiteral("CREATE INDEX idx_audit_session_monotonic ON AuditRecords(sessionId, monotonicUs)"),
        QStringLiteral("CREATE UNIQUE INDEX idx_vehicles_identity ON Vehicles(fleetIdentity)"),
    };

    QSqlQuery query(db);
    for (const QString& statement : statements) {
        if (!query.exec(statement)) {
            qCWarning(AeroraPersistenceLog) << "schema statement failed:" << query.lastError().text();
            return false;  // transaction rolls back, leaving no half-built schema
        }
    }

    if (!QGCSqlHelper::setUserVersion(db, kSchemaVersion)) {
        qCWarning(AeroraPersistenceLog) << "could not set the schema version";
        return false;
    }

    return transaction.commit();
}

bool AeroraLocalStore::_hasUserTable() const
{
    QSqlDatabase db = _connection->database();
    QSqlQuery query(db);
    if (!query.exec(
            QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%'")) ||
        !query.next()) {
        return false;
    }
    return query.value(0).toInt() > 0;
}

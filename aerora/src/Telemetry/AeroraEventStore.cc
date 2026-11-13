#include "AeroraEventStore.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QVariant>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "AeroraLocalStore.h"
#include "QGCLoggingCategory.h"
#include "QGCSqlHelper.h"

QGC_LOGGING_CATEGORY(AeroraEventStoreLog, "aerora.telemetry.events")

namespace {

QString detailJson(const QVariantMap& detail)
{
    return QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(detail)).toJson(QJsonDocument::Compact));
}

/// The value bound for an optional integer column: the value, or a null QVariant for NULL.
QVariant optionalInt(const std::optional<qint64>& value)
{
    return value.has_value() ? QVariant(static_cast<qlonglong>(*value)) : QVariant();
}

}  // namespace

AeroraEventStore::AeroraEventStore(AeroraLocalStore& store)
    : _store(store)
{}

AeroraEventStore::~AeroraEventStore() = default;

bool AeroraEventStore::open()
{
    if (_ready) {
        return true;
    }

    if (!_store.isOpen()) {
        qCWarning(AeroraEventStoreLog) << "cannot open the event store: the store is not open";
        return false;
    }

    const QString path = _store.databasePath();
    if (path.isEmpty()) {
        qCWarning(AeroraEventStoreLog) << "cannot open the event store: the store has no database path";
        return false;
    }

    _connection = std::make_unique<QGCSqlHelper::ScopedConnection>(path, false, QStringLiteral("AeroraEvents"));
    if (!_connection->isValid()) {
        _connection.reset();
        qCWarning(AeroraEventStoreLog) << "cannot open the event store: the database did not open" << path;
        return false;
    }

    _ready = true;
    return true;
}

void AeroraEventStore::close()
{
    _connection.reset();
    _ready = false;
}

bool AeroraEventStore::recordEvent(const AeroraEventRecord& event, const QList<AeroraEventValue>& values)
{
    if (!_ready) {
        return false;
    }

    const qint64 sessionId = _store.currentSessionId();
    if (sessionId < 0) {
        _store.recordWriteFailure(QStringLiteral("Events"), QStringLiteral("no active session"));
        return false;
    }

    QSqlDatabase db = _connection->database();
    QGCSqlHelper::Transaction transaction(db);
    if (!transaction.ok()) {
        _store.recordWriteFailure(QStringLiteral("Events"), QStringLiteral("could not begin a transaction"));
        return false;
    }

    QSqlQuery eventQuery(db);
    // vehicleId is NULL: no caller resolves a Vehicles-table row id for an event yet, though
    // AeroraVehicleStore now exists and could supply one.
    eventQuery.prepare(
        QStringLiteral("INSERT INTO Events (sessionId, vehicleId, occurredUtcMs, monotonicUs, category, "
                       "subject, stateClass, detail) VALUES (?, NULL, ?, ?, ?, ?, ?, ?)"));
    eventQuery.addBindValue(sessionId);
    eventQuery.addBindValue(event.occurredUtcMs);
    eventQuery.addBindValue(event.monotonicUs);
    eventQuery.addBindValue(static_cast<int>(event.category));
    eventQuery.addBindValue(event.subject);
    eventQuery.addBindValue(event.stateClass.has_value() ? QVariant(static_cast<int>(*event.stateClass)) : QVariant());
    eventQuery.addBindValue(detailJson(event.detail));
    if (!eventQuery.exec()) {
        _store.recordWriteFailure(QStringLiteral("Events"), eventQuery.lastError().text());
        return false;
    }

    const qint64 eventId = eventQuery.lastInsertId().toLongLong();

    for (const AeroraEventValue& value : values) {
        QSqlQuery valueQuery(db);
        valueQuery.prepare(
            QStringLiteral("INSERT INTO EventValues (eventId, itemId, stateClass, derived, valueText, "
                           "ageMs) VALUES (?, ?, ?, ?, ?, ?)"));
        valueQuery.addBindValue(eventId);
        valueQuery.addBindValue(value.itemId);
        valueQuery.addBindValue(static_cast<int>(value.stateClass));
        valueQuery.addBindValue(value.derived ? 1 : 0);
        valueQuery.addBindValue(value.valueText.has_value() ? QVariant(*value.valueText) : QVariant());
        valueQuery.addBindValue(optionalInt(value.ageMs));
        if (!valueQuery.exec()) {
            _store.recordWriteFailure(QStringLiteral("EventValues"), valueQuery.lastError().text());
            return false;
        }
    }

    if (!transaction.commit()) {
        _store.recordWriteFailure(QStringLiteral("Events"), QStringLiteral("commit failed"));
        return false;
    }

    return true;
}

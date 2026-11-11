#include "AeroraSqliteAuditSink.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "AeroraAuditChain.h"
#include "AeroraLocalStore.h"
#include "QGCLoggingCategory.h"
#include "QGCSqlHelper.h"

QGC_LOGGING_CATEGORY(AeroraAuditSinkLog, "aerora.audit")

namespace {

/// The event detail as compact JSON for the `detail` TEXT column. An empty map encodes as `{}`
/// rather than NULL, so "no detail" and "column never written" stay distinct.
QString detailJson(const QVariantMap& detail)
{
    return QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(detail)).toJson(QJsonDocument::Compact));
}

/// The `subject` column: the vehicle the event concerned. An unresolved vehicle is an explicit gap
/// rather than a system id that was never involved (AeroraAuditSink.h). The request specifics live
/// in the `detail` column, not here.
QString subjectFor(const AeroraAuditEvent& event)
{
    return event.vehicleResolved ? QStringLiteral("vehicle %1").arg(event.vehicleMavlinkSystemId)
                                 : QStringLiteral("vehicle unresolved");
}

}  // namespace

AeroraSqliteAuditSink::AeroraSqliteAuditSink(AeroraLocalStore& store)
    : _store(store)
{}

AeroraSqliteAuditSink::~AeroraSqliteAuditSink() = default;

bool AeroraSqliteAuditSink::open()
{
    if (_ready) {
        return true;
    }

    if (!_store.isOpen()) {
        qCWarning(AeroraAuditSinkLog) << "cannot open the audit sink: the store is not open";
        return false;
    }

    const QString path = _store.databasePath();
    if (path.isEmpty()) {
        qCWarning(AeroraAuditSinkLog) << "cannot open the audit sink: the store has no database path";
        return false;
    }

    _connection = std::make_unique<QGCSqlHelper::ScopedConnection>(path, false, QStringLiteral("AeroraAudit"));
    if (!_connection->isValid()) {
        _connection.reset();
        qCWarning(AeroraAuditSinkLog) << "cannot open the audit sink: the database did not open" << path;
        return false;
    }

    QSqlDatabase db = _connection->database();

    // The §6 audit deviation: synchronous=FULL on top of the shared WAL pragmas ScopedConnection
    // already applied, so a committed audit row is fsync-durable rather than only crash-durable.
    QSqlQuery pragma(db);
    if (!pragma.exec(QStringLiteral("PRAGMA synchronous=FULL"))) {
        _connection.reset();
        qCWarning(AeroraAuditSinkLog) << "cannot open the audit sink: synchronous=FULL was refused";
        return false;
    }

    // Seed the chain head from the last stored record so the chain is continuous across restarts.
    _head = AeroraAuditChain::genesisDigest();
    QSqlQuery seed(db);
    if (seed.exec(QStringLiteral("SELECT recordHash FROM AuditRecords ORDER BY recordId DESC LIMIT 1")) &&
        seed.next()) {
        const QByteArray lastHex = seed.value(0).toString().toLatin1();
        if (!lastHex.isEmpty()) {
            _head = QByteArray::fromHex(lastHex);
        }
    }

    _ready = true;
    return true;
}

void AeroraSqliteAuditSink::close()
{
    _connection.reset();
    _head.clear();
    _ready = false;
}

AeroraAuditWriteOutcome AeroraSqliteAuditSink::record(const AeroraAuditEvent& event)
{
    if (!_ready) {
        return AeroraAuditWriteOutcome::SinkUnavailable;
    }

    const qint64 sessionId = _store.currentSessionId();
    if (sessionId < 0) {
        // Every AuditRecords row references a session (ON DELETE RESTRICT), and the producer has no
        // session identity, so a record composed before a session opened cannot be persisted.
        return AeroraAuditWriteOutcome::FailedStorage;
    }

    const QByteArray previousDigest = _head;
    const QByteArray recordDigest = AeroraAuditChain::digestFor(event, previousDigest);

    QSqlDatabase db = _connection->database();
    QGCSqlHelper::Transaction transaction(db);
    if (!transaction.ok()) {
        return AeroraAuditWriteOutcome::FailedStorage;
    }

    QSqlQuery query(db);
    query.prepare(
        QStringLiteral("INSERT INTO AuditRecords (sessionId, occurredUtcMs, monotonicUs, actor, action, "
                       "subject, outcome, enforced, detail, prevHash, recordHash) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(sessionId);
    query.addBindValue(event.utc.isValid() ? event.utc.toMSecsSinceEpoch() : qint64(0));
    query.addBindValue(static_cast<qint64>(event.monotonicUs));
    query.addBindValue(event.operatorId.isEmpty() ? QStringLiteral("unresolved") : event.operatorId);
    query.addBindValue(AeroraAuditEvent::typeToString(event.type));
    query.addBindValue(subjectFor(event));
    query.addBindValue(static_cast<int>(classifyOutcome(event)));
    query.addBindValue(event.authorizationEnforced ? 1 : 0);
    query.addBindValue(detailJson(event.detail));
    query.addBindValue(QString::fromLatin1(previousDigest.toHex()));
    query.addBindValue(QString::fromLatin1(recordDigest.toHex()));
    if (!query.exec()) {
        qCWarning(AeroraAuditSinkLog) << "audit record insert failed:" << query.lastError().text();
        return AeroraAuditWriteOutcome::FailedStorage;
    }

    if (!transaction.commit()) {
        qCWarning(AeroraAuditSinkLog) << "audit record commit failed:" << db.lastError().text();
        return AeroraAuditWriteOutcome::FailedStorage;
    }

    // Advance the head only after a durable commit, so a failed write does not leave the chain
    // pointing at a record that was rolled back.
    _head = recordDigest;
    return AeroraAuditWriteOutcome::Accepted;
}

QString AeroraSqliteAuditSink::sinkName() const
{
    return QStringLiteral("SqliteAudit");
}

AeroraAuditOutcome AeroraSqliteAuditSink::classifyOutcome(const AeroraAuditEvent& event)
{
    switch (event.type) {
        case AeroraAuditEventType::CommandDenied:
        case AeroraAuditEventType::ManualAuthorityRefused:
            return AeroraAuditOutcome::Denied;

        case AeroraAuditEventType::AuthorizationDecision:
            // An allow carries no reason; a denial carries why. This is the one type whose outcome the
            // reasonCode decides.
            return event.reasonCode.isEmpty() ? AeroraAuditOutcome::Allowed : AeroraAuditOutcome::Denied;

        case AeroraAuditEventType::CommandTransmitted:
        case AeroraAuditEventType::ManualAuthorityIssued:
        case AeroraAuditEventType::ManualAuthorityRefreshed:
            return AeroraAuditOutcome::Allowed;

        case AeroraAuditEventType::CommandRequested:
        case AeroraAuditEventType::OperatorIdentityResolved:
        case AeroraAuditEventType::VehicleResponse:
        case AeroraAuditEventType::SecurityEvent:
        case AeroraAuditEventType::SystemEvent:
        case AeroraAuditEventType::ManualAuthorityExpired:
            return AeroraAuditOutcome::Informational;
    }

    // A type added to the enumeration without a case here is informational until classified, which
    // records it rather than dropping it. The switch is unguarded by a default so -Wswitch flags
    // the omission at the same time.
    return AeroraAuditOutcome::Informational;
}

#include "AeroraStoreExportTest.h"

#include <optional>

#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTemporaryDir>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtTest/QTest>

#include "AeroraLocalStore.h"
#include "AeroraStoreExport.h"
#include "QGCSqlHelper.h"

namespace {

AeroraLocalStore::SessionRecord sampleSession()
{
    AeroraLocalStore::SessionRecord record;
    record.startedUtcMs = 1700000000000LL;
    record.baseCommit = QStringLiteral("base");
    record.overlayCommit = QStringLiteral("overlay");
    record.customBuild = true;
    record.authorizationEnforced = false;
    return record;
}

bool schemaObjectExists(const QString& path, const QString& type, const QString& name)
{
    QGCSqlHelper::ScopedConnection connection(path);
    if (!connection.isValid()) {
        return false;
    }
    QSqlQuery query(connection.database());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE type = ? AND name = ?"));
    query.addBindValue(type);
    query.addBindValue(name);
    if (!query.exec() || !query.next()) {
        return false;
    }
    return query.value(0).toInt() > 0;
}

int sessionCount(const QString& path)
{
    QGCSqlHelper::ScopedConnection connection(path);
    if (!connection.isValid()) {
        return -1;
    }
    QSqlQuery query(connection.database());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM Sessions")) || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

/// Opens a store at \a dbPath, records one session, and closes it, leaving a persisted single-session
/// database for an export test to read.
void seedOneSession(const QString& dbPath)
{
    AeroraLocalStore store;
    QVERIFY(store.open(dbPath, false).opened);
    QVERIFY(store.beginSession(sampleSession()) > 0);
}

}  // namespace

void AeroraStoreExportTest::_exportDatabaseProducesAReImportableCopy()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("aerora.db"));
    seedOneSession(dbPath);

    const QString dest = dir.filePath(QStringLiteral("export.db"));
    QVERIFY(AeroraStoreExport::exportDatabase(dbPath, dest));
    QVERIFY(QFile::exists(dest));

    // Re-importable means the copy carries the schema version, the full table set and the data.
    QGCSqlHelper::ScopedConnection connection(dest);
    QVERIFY(connection.isValid());
    QSqlDatabase db = connection.database();
    const std::optional<int> version = QGCSqlHelper::userVersion(db);
    QVERIFY(version.has_value());
    QCOMPARE(*version, AeroraLocalStore::kSchemaVersion);

    QCOMPARE(sessionCount(dest), 1);

    const QStringList tables = {QStringLiteral("Sessions"),     QStringLiteral("Vehicles"),
                                QStringLiteral("Events"),       QStringLiteral("EventValues"),
                                QStringLiteral("AuditRecords"), QStringLiteral("WriteFailures")};
    for (const QString& table : tables) {
        QVERIFY2(schemaObjectExists(dest, QStringLiteral("table"), table), qPrintable(table));
    }
}

void AeroraStoreExportTest::_exportDatabaseLeavesTheSourceIntact()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("aerora.db"));
    seedOneSession(dbPath);

    const QString dest = dir.filePath(QStringLiteral("export.db"));
    QVERIFY(AeroraStoreExport::exportDatabase(dbPath, dest));

    // The source still opens and still holds its session: the export read it and wrote only dest.
    QCOMPARE(sessionCount(dbPath), 1);
    QVERIFY(schemaObjectExists(dbPath, QStringLiteral("table"), QStringLiteral("AuditRecords")));
}

void AeroraStoreExportTest::_exportDatabaseRefusesAnExistingDestination()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("aerora.db"));
    seedOneSession(dbPath);

    const QString dest = dir.filePath(QStringLiteral("export.db"));
    QFile existing(dest);
    QVERIFY(existing.open(QIODevice::WriteOnly));
    existing.write("occupied");
    existing.close();

    // Overwriting a prior export silently would be worse than refusing; the refusal warns.
    expectLogMessage("aerora.persistence.export", QtWarningMsg,
                     QRegularExpression(QStringLiteral("destination already exists")));
    QVERIFY(!AeroraStoreExport::exportDatabase(dbPath, dest));
    verifyExpectedLogMessage();
}

void AeroraStoreExportTest::_exportRefusesEmptyPaths()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("aerora.db"));
    seedOneSession(dbPath);

    QVERIFY(!AeroraStoreExport::exportDatabase(QString(), dir.filePath(QStringLiteral("x.db"))));
    QVERIFY(!AeroraStoreExport::exportDatabase(dbPath, QString()));
    QVERIFY(!AeroraStoreExport::exportEventValuesCsv(QString(), dir.filePath(QStringLiteral("x.csv"))));
    QVERIFY(!AeroraStoreExport::exportEventValuesCsv(dbPath, QString()));
}

void AeroraStoreExportTest::_exportEventValuesCsvWritesProvenanceColumns()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("aerora.db"));

    qint64 sessionId = -1;
    {
        AeroraLocalStore store;
        QVERIFY(store.open(dbPath, false).opened);
        sessionId = store.beginSession(sampleSession());
        QVERIFY(sessionId > 0);
    }

    // The Events/EventValues writer is another tab's and does not exist yet, so seed one value row
    // directly to exercise the export. stateClass and derived are the provenance the CSV must carry.
    {
        QGCSqlHelper::ScopedConnection connection(dbPath);
        QVERIFY(connection.isValid());
        QSqlQuery event(connection.database());
        event.prepare(
            QStringLiteral("INSERT INTO Events (sessionId, occurredUtcMs, monotonicUs, category, subject) "
                           "VALUES (?, ?, ?, ?, ?)"));
        event.addBindValue(sessionId);
        event.addBindValue(1700000000000LL);
        event.addBindValue(1000);
        event.addBindValue(0);
        event.addBindValue(QStringLiteral("telemetry"));
        QVERIFY(event.exec());
        const qint64 eventId = event.lastInsertId().toLongLong();

        QSqlQuery value(connection.database());
        value.prepare(
            QStringLiteral("INSERT INTO EventValues (eventId, itemId, stateClass, derived, valueText, ageMs) "
                           "VALUES (?, ?, ?, ?, ?, ?)"));
        value.addBindValue(eventId);
        value.addBindValue(QStringLiteral("battery.voltage"));
        value.addBindValue(2);
        value.addBindValue(1);
        value.addBindValue(QStringLiteral("12.3"));
        value.addBindValue(500);
        QVERIFY(value.exec());
    }

    const QString csvPath = dir.filePath(QStringLiteral("events.csv"));
    QVERIFY(AeroraStoreExport::exportEventValuesCsv(dbPath, csvPath));

    QFile csv(csvPath);
    QVERIFY(csv.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString content = QString::fromUtf8(csv.readAll());

    QVERIFY2(content.startsWith(QStringLiteral("eventId,itemId,stateClass,derived,valueText,ageMs")),
             qPrintable(content));
    // itemId, then stateClass=2, derived=1, valueText=12.3, ageMs=500 as columns.
    QVERIFY2(content.contains(QStringLiteral("battery.voltage,2,1,12.3,500")), qPrintable(content));
}

void AeroraStoreExportTest::_exportEventValuesCsvRefusesAnExistingDestination()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("aerora.db"));
    seedOneSession(dbPath);

    const QString csvPath = dir.filePath(QStringLiteral("events.csv"));
    QFile existing(csvPath);
    QVERIFY(existing.open(QIODevice::WriteOnly));
    existing.write("occupied");
    existing.close();

    expectLogMessage("aerora.persistence.export", QtWarningMsg,
                     QRegularExpression(QStringLiteral("destination already exists")));
    QVERIFY(!AeroraStoreExport::exportEventValuesCsv(dbPath, csvPath));
    verifyExpectedLogMessage();
}

UT_REGISTER_TEST(AeroraStoreExportTest, TestLabel::Unit)

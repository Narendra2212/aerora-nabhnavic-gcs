#include "AeroraStoreExport.h"

#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "QGCLoggingCategory.h"
#include "QGCSqlHelper.h"

QGC_LOGGING_CATEGORY(AeroraStoreExportLog, "aerora.persistence.export")

namespace {

/// One CSV field, RFC 4180 escaped: wrapped in quotes and with internal quotes doubled when it
/// carries a comma, quote or newline. A null value is an empty field.
QString csvField(const QVariant& value)
{
    if (value.isNull()) {
        return QString();
    }
    const QString text = value.toString();
    if (text.contains(QLatin1Char(',')) || text.contains(QLatin1Char('"')) || text.contains(QLatin1Char('\n')) ||
        text.contains(QLatin1Char('\r'))) {
        QString escaped = text;
        escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + escaped + QLatin1Char('"');
    }
    return text;
}

}  // namespace

namespace AeroraStoreExport {

bool exportDatabase(const QString& sourcePath, const QString& destPath)
{
    if (sourcePath.isEmpty() || destPath.isEmpty()) {
        return false;
    }
    if (QFile::exists(destPath)) {
        // VACUUM INTO refuses an existing target, and overwriting an operator's prior export
        // silently would be worse than refusing.
        qCWarning(AeroraStoreExportLog) << "export destination already exists:" << destPath;
        return false;
    }

    QGCSqlHelper::ScopedConnection connection(sourcePath, false, QStringLiteral("AeroraExport"));
    if (!connection.isValid()) {
        qCWarning(AeroraStoreExportLog) << "could not open the store for export:" << sourcePath;
        return false;
    }

    QSqlDatabase db = connection.database();
    QString escapedDest = destPath;
    escapedDest.replace(QLatin1Char('\''), QStringLiteral("''"));

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("VACUUM INTO '%1'").arg(escapedDest))) {
        qCWarning(AeroraStoreExportLog) << "database export failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool exportEventValuesCsv(const QString& sourcePath, const QString& destPath)
{
    if (sourcePath.isEmpty() || destPath.isEmpty()) {
        return false;
    }
    if (QFile::exists(destPath)) {
        qCWarning(AeroraStoreExportLog) << "export destination already exists:" << destPath;
        return false;
    }

    QGCSqlHelper::ScopedConnection connection(sourcePath, false, QStringLiteral("AeroraExportCsv"));
    if (!connection.isValid()) {
        qCWarning(AeroraStoreExportLog) << "could not open the store for export:" << sourcePath;
        return false;
    }

    QSqlQuery query(connection.database());
    if (!query.exec(QStringLiteral("SELECT eventId, itemId, stateClass, derived, valueText, ageMs "
                                   "FROM EventValues ORDER BY eventId, itemId"))) {
        qCWarning(AeroraStoreExportLog) << "could not read EventValues for export:" << query.lastError().text();
        return false;
    }

    QFile file(destPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(AeroraStoreExportLog) << "could not open the CSV destination:" << destPath;
        return false;
    }

    QTextStream out(&file);
    out << "eventId,itemId,stateClass,derived,valueText,ageMs\n";
    while (query.next()) {
        out << csvField(query.value(0)) << QLatin1Char(',') << csvField(query.value(1)) << QLatin1Char(',')
            << csvField(query.value(2)) << QLatin1Char(',') << csvField(query.value(3)) << QLatin1Char(',')
            << csvField(query.value(4)) << QLatin1Char(',') << csvField(query.value(5)) << QLatin1Char('\n');
    }

    return true;
}

}  // namespace AeroraStoreExport

#include "AeroraStorePaths.h"

#include "AppSettings.h"
#include "Fact.h"
#include "QGCFileHelper.h"
#include "SettingsManager.h"

namespace AeroraStorePaths {

QString dataDirectoryFor(const QString& saveRoot)
{
    if (saveRoot.isEmpty()) {
        return QString();
    }
    return QGCFileHelper::joinPath(saveRoot, QStringLiteral("aerora"));
}

QString databasePathFor(const QString& saveRoot)
{
    const QString dataDir = dataDirectoryFor(saveRoot);
    if (dataDir.isEmpty()) {
        return QString();
    }
    return QGCFileHelper::joinPath(dataDir, QStringLiteral("aerora.db"));
}

QString exportDirectoryFor(const QString& saveRoot)
{
    const QString dataDir = dataDirectoryFor(saveRoot);
    if (dataDir.isEmpty()) {
        return QString();
    }
    return QGCFileHelper::joinPath(dataDir, QStringLiteral("export"));
}

bool ensureDirectoriesFor(const QString& saveRoot)
{
    const QString dataDir = dataDirectoryFor(saveRoot);
    if (dataDir.isEmpty()) {
        return false;
    }
    // The data directory first, then its export child, the same order and the same call
    // AppSettings::_checkSavePathDirectories() uses for the nine categories.
    const bool dataExists = QGCFileHelper::ensureDirectoryExists(dataDir);
    if (dataExists) {
        QGCFileHelper::ensureDirectoryExists(exportDirectoryFor(saveRoot));
    }
    return dataExists;
}

QString saveRoot()
{
    // Non-const: the generated savePath() accessor lazily creates its Fact, so it is not const.
    AppSettings* const appSettings = SettingsManager::instance()->appSettings();
    if (!appSettings) {
        return QString();
    }
    return appSettings->savePath()->rawValue().toString();
}

bool isPersistenceDisabled()
{
    AppSettings* const appSettings = SettingsManager::instance()->appSettings();
    if (!appSettings) {
        return false;
    }
    return appSettings->disableAllPersistence()->rawValue().toBool();
}

QString databasePath()
{
    return databasePathFor(saveRoot());
}

QString exportDirectory()
{
    return exportDirectoryFor(saveRoot());
}

bool ensureDirectories()
{
    return ensureDirectoriesFor(saveRoot());
}

}  // namespace AeroraStorePaths

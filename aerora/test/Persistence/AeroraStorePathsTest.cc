#include "AeroraStorePathsTest.h"

#include <QtCore/QDir>
#include <QtCore/QString>
#include <QtCore/QTemporaryDir>
#include <QtCore/QVariant>
#include <QtTest/QTest>

#include "AeroraStorePaths.h"
#include "AppSettings.h"
#include "Fact.h"
#include "QGCFileHelper.h"
#include "SettingsManager.h"

void AeroraStorePathsTest::_dataDirectoryIsAeroraUnderRoot()
{
    const QString root = QStringLiteral("/tmp/aerora-root");
    const QString dataDir = AeroraStorePaths::dataDirectoryFor(root);

    QCOMPARE(dataDir, QGCFileHelper::joinPath(root, QStringLiteral("aerora")));
    QVERIFY(dataDir.startsWith(root));
    QVERIFY2(dataDir.endsWith(QStringLiteral("aerora")), qPrintable(dataDir));
}

void AeroraStorePathsTest::_databasePathIsAeroraDbUnderDataDirectory()
{
    const QString root = QStringLiteral("/tmp/aerora-root");
    const QString path = AeroraStorePaths::databasePathFor(root);

    QCOMPARE(path, QGCFileHelper::joinPath(AeroraStorePaths::dataDirectoryFor(root), QStringLiteral("aerora.db")));
    QVERIFY2(path.endsWith(QStringLiteral("aerora/aerora.db")), qPrintable(path));
    QVERIFY(path.startsWith(root));
}

void AeroraStorePathsTest::_exportDirectoryIsUnderDataDirectory()
{
    const QString root = QStringLiteral("/tmp/aerora-root");
    const QString path = AeroraStorePaths::exportDirectoryFor(root);

    QCOMPARE(path, QGCFileHelper::joinPath(AeroraStorePaths::dataDirectoryFor(root), QStringLiteral("export")));
    QVERIFY2(path.endsWith(QStringLiteral("aerora/export")), qPrintable(path));
}

void AeroraStorePathsTest::_emptyRootYieldsEmptyPaths()
{
    // An empty root composes to nothing rather than to a bare "aerora" relative to the working
    // directory, so a caller passing an unresolved savePath opens no store instead of a stray file.
    QVERIFY(AeroraStorePaths::dataDirectoryFor(QString()).isEmpty());
    QVERIFY(AeroraStorePaths::databasePathFor(QString()).isEmpty());
    QVERIFY(AeroraStorePaths::exportDirectoryFor(QString()).isEmpty());
    QVERIFY(!AeroraStorePaths::ensureDirectoriesFor(QString()));
}

void AeroraStorePathsTest::_ensureDirectoriesCreatesDataAndExport()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = dir.path();

    QVERIFY(AeroraStorePaths::ensureDirectoriesFor(root));
    QVERIFY(QDir(AeroraStorePaths::dataDirectoryFor(root)).exists());
    QVERIFY(QDir(AeroraStorePaths::exportDirectoryFor(root)).exists());
}

void AeroraStorePathsTest::_liveFunctionsDelegateToTheComposers()
{
    // Holds whether or not the harness carries the settings toolbox: with no toolbox saveRoot() is
    // empty and both sides compose to empty; with one, both sides compose off the same root.
    const QString root = AeroraStorePaths::saveRoot();
    QCOMPARE(AeroraStorePaths::databasePath(), AeroraStorePaths::databasePathFor(root));
    QCOMPARE(AeroraStorePaths::exportDirectory(), AeroraStorePaths::exportDirectoryFor(root));
}

void AeroraStorePathsTest::_isPersistenceDisabledReflectsTheSetting()
{
    AppSettings* const appSettings = SettingsManager::instance()->appSettings();
    if (!appSettings) {
        QSKIP("this harness carries no SettingsManager toolbox, so disableAllPersistence is unreadable");
    }

    Fact* const fact = appSettings->disableAllPersistence();
    const QVariant original = fact->rawValue();

    fact->setRawValue(true);
    QVERIFY(AeroraStorePaths::isPersistenceDisabled());

    fact->setRawValue(false);
    QVERIFY(!AeroraStorePaths::isPersistenceDisabled());

    fact->setRawValue(original);  // Restore: the harness shares settings and does not reset them.
}

UT_REGISTER_TEST(AeroraStorePathsTest, TestLabel::Unit)

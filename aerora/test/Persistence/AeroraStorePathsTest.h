#pragma once

#include "UnitTest.h"

/// \brief Asserts `AeroraStorePaths`: that it composes `<savePath>/aerora/aerora.db` and the
/// `export/` child, creates them on demand, and reads `disableAllPersistence`.
///
/// The `...For(saveRoot)` functions are the composition logic and are tested against an explicit
/// root with no application dependency. The live, `AppSettings`-backed wrappers are tested for
/// delegation (which holds whether or not the test harness carries the settings toolbox) and, when
/// the toolbox is present, that the disabled read reflects the setting.
class AeroraStorePathsTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _dataDirectoryIsAeroraUnderRoot();
    void _databasePathIsAeroraDbUnderDataDirectory();
    void _exportDirectoryIsUnderDataDirectory();
    void _emptyRootYieldsEmptyPaths();
    void _ensureDirectoriesCreatesDataAndExport();
    void _liveFunctionsDelegateToTheComposers();
    void _isPersistenceDisabledReflectsTheSetting();
};

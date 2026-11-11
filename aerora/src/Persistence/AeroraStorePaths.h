#pragma once

#include <QtCore/QString>

/// \brief Resolves where the Aerora store lives under `AppSettings::savePath`, and whether
/// persistence is disabled. `P7-DAT-002`/`P7-DAT-005`, design 09-local-persistence-architecture.md §3.
///
/// \section seam The seam this is
///
/// `AeroraLocalStore` is a pure `(path, persistenceDisabled)` consumer and deliberately does not
/// read `AppSettings`, so it stays unit-testable against a temporary path. This namespace is the one
/// place that reads the setting and composes the path, so the store can be located in a real run
/// without taking a settings dependency. It is the first overlay reader of `AppSettings`.
///
/// \section where Where the data goes
///
/// A single `aerora/` subdirectory under `AppSettings::savePath`, a tenth sibling of the nine
/// categories core already manages (`parameter`, `telemetry`, `mission`, ...), holding `aerora.db`
/// and an `export/` child. The root is read from `savePath`, which already resolves under
/// `QStandardPaths::TempLocation` for unit tests and the simple-boot test, so a test inherits a
/// temporary root with no special handling here.
///
/// Core creates its nine category directories at startup in `AppSettings::_checkSavePathDirectories()`,
/// which hardcodes exactly those nine and offers no extension point. Rather than edit core, this
/// creates the `aerora/` directory lazily through `ensureDirectories()`, which a caller runs before
/// opening the store.
///
/// \section split Pure composition versus the live read
///
/// The `...For(saveRoot)` functions compose and create paths from an explicit root and take no
/// settings dependency, so they are unit-testable without the application. The no-argument forms
/// read `AppSettings::savePath` / `disableAllPersistence` and delegate to them.
namespace AeroraStorePaths {

// ── Pure composition over an explicit savePath root ────────────────────

/// `<saveRoot>/aerora`, or empty when \a saveRoot is empty.
[[nodiscard]] QString dataDirectoryFor(const QString& saveRoot);

/// `<saveRoot>/aerora/aerora.db`, or empty when \a saveRoot is empty.
[[nodiscard]] QString databasePathFor(const QString& saveRoot);

/// `<saveRoot>/aerora/export`, or empty when \a saveRoot is empty.
[[nodiscard]] QString exportDirectoryFor(const QString& saveRoot);

/// Creates the `aerora/` directory and its `export/` child under \a saveRoot. Returns whether the
/// data directory exists afterwards. A no-op returning false for an empty root.
bool ensureDirectoriesFor(const QString& saveRoot);

// ── AppSettings-backed ─────────────────────────────────────────────────

/// `AppSettings::savePath`, or empty when settings are not yet available.
[[nodiscard]] QString saveRoot();

/// `AppSettings::disableAllPersistence`. False when settings are not yet available, matching the
/// shipped default.
[[nodiscard]] bool isPersistenceDisabled();

/// `databasePathFor(saveRoot())` — the path to hand `AeroraLocalStore::open()`.
[[nodiscard]] QString databasePath();

/// `exportDirectoryFor(saveRoot())`.
[[nodiscard]] QString exportDirectory();

/// `ensureDirectoriesFor(saveRoot())`. Run before opening the store.
bool ensureDirectories();

}  // namespace AeroraStorePaths

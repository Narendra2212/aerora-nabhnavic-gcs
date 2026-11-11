#pragma once

#include <QtCore/QString>

/// \brief Operator-initiated, read-only export of the Aerora store. `P7-LOG-003`,
/// design 09-local-persistence-architecture.md §9.
///
/// \section forms Two forms, both read-only with respect to the source
///
/// \li **SQLite**, full fidelity and re-importable: a complete copy of the store, for archival and
///     for re-opening in another station.
/// \li **CSV**, one row per `EventValues` row with `stateClass` and `derived` as columns, for review
///     in external tools. Carrying both is mandatory (`P7-ARCH-004`): a station-derived estimate and
///     a vehicle-reported measurement must never present as the same kind of number, and in CSV that
///     means columns a spreadsheet filter cannot drop, not a header note.
///
/// Both open a second connection and never write to the source. A failed export leaves the store
/// untouched, and neither form transmits anywhere (`P7-ARCH-013`, `P7-DAT-004`): the destination is
/// a local file path.
///
/// \section fidelity Why VACUUM INTO rather than the tile cache's per-set copy
///
/// §9 points at `QGCTileCacheDatabase::exportSets()`, which copies a *selected subset* of tile sets
/// through a per-table transaction. Aerora exports the *whole* store, for which SQLite's
/// `VACUUM INTO` is the faithful single-statement form: it writes a fresh, defragmented, fully
/// self-contained database with the same schema, `user_version`, indices and `AUTOINCREMENT` state,
/// and reads the source without modifying it. It also avoids duplicating the store's schema DDL in a
/// second place, which a structured row-copy would require and which would drift from
/// `AeroraLocalStore`.
namespace AeroraStoreExport {

/// Writes a full-fidelity, re-importable SQLite copy of the store at \a sourcePath to \a destPath.
/// \a destPath must not already exist. Returns whether the export succeeded. The source is read,
/// never written.
bool exportDatabase(const QString& sourcePath, const QString& destPath);

/// Writes every `EventValues` row of the store at \a sourcePath to \a destPath as CSV, with
/// `stateClass` and `derived` as columns so provenance survives the export. \a destPath must not
/// already exist. Returns whether the export succeeded. The source is read, never written.
bool exportEventValuesCsv(const QString& sourcePath, const QString& destPath);

}  // namespace AeroraStoreExport

#pragma once

#include "UnitTest.h"

/// \brief Asserts `AeroraStoreExport`: that the SQLite export is a full-fidelity, re-importable copy,
/// that the CSV carries the provenance columns, and that both refuse to overwrite and never mutate
/// the source.
class AeroraStoreExportTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _exportDatabaseProducesAReImportableCopy();
    void _exportDatabaseLeavesTheSourceIntact();
    void _exportDatabaseRefusesAnExistingDestination();
    void _exportRefusesEmptyPaths();
    void _exportEventValuesCsvWritesProvenanceColumns();
    void _exportEventValuesCsvRefusesAnExistingDestination();
};

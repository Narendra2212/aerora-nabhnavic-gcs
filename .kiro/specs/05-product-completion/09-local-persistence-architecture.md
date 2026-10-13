# Local Persistence Architecture (WP1-C)

Owner: **Tab 1**. Design for `P7-DAT-002`, `P7-DAT-005`, and the Q-037 assessment.

Status: `DESIGNED`. No code exists. Every mechanism named below was verified against this checkout;
nothing is proposed that QGroundControl does not already do somewhere.

**The durability and tamper-evidence sections state what is *not* achievable as precisely as what
is.** `P7-NOINV-001` prohibits claiming a guarantee without evidence, and a persistence layer is
exactly where such a claim is easiest to make and hardest to check.

## Contents

- [1. What this unblocks](#1-what-this-unblocks)
- [2. Foundation: what already exists](#2-foundation-what-already-exists)
- [3. Storage locations](#3-storage-locations)
- [4. Schema](#4-schema)
- [5. Schema versioning](#5-schema-versioning)
- [6. Durability](#6-durability)
- [7. Q-037 assessment: can this carry audit?](#7-q-037-assessment-can-this-carry-audit)
- [8. Retention and pruning](#8-retention-and-pruning)
- [9. Export](#9-export)
- [10. No automatic outbound transmission](#10-no-automatic-outbound-transmission)
- [11. Failure behaviour](#11-failure-behaviour)
- [12. Data-integrity limitations](#12-data-integrity-limitations)
- [13. Implementation owners](#13-implementation-owners)
- [14. Open decisions](#14-open-decisions)

## 1. What this unblocks

| Screen / requirement | Needs |
| --- | --- |
| `SCR-TEL-02` event timeline | event records with state class |
| `SCR-FLT-01` fleet inventory | persistent per-vehicle records |
| `SCR-FLT-02` vehicle record detail | same |
| `SCR-LOG-01` session review | session + event records |
| `SCR-LOG-02` provenance export | export with per-value state class |
| `SCR-SEC-03` audit log viewer | audit records — **see [§7](#7-q-037-assessment-can-this-carry-audit)** |
| `SCR-AIS-03` approval audit view | same |

Seven screens. Five are blocked on this document and nothing else.

## 2. Foundation: what already exists

**No new dependency is introduced.** Qt Sql is already linked and SQLite is already deployed:

| Fact | Evidence |
| --- | --- |
| `Sql` is in the root Qt component list | `CMakeLists.txt:272` |
| The SQLite driver plugin is explicitly deployed | `CMakeLists.txt:538-539` — `sqldrivers`, `Qt6::QSQLiteDriverPlugin` |
| A dedicated database utility library exists | `src/Utilities/Database/`, links `Qt6::Sql` |

`QGCSqlHelper` (`src/Utilities/Database/QGCSqlHelper.h`) provides every primitive this design needs,
and Aerora uses it rather than reimplementing it:

| Primitive | Behaviour |
| --- | --- |
| `applySqlitePragmas(db)` | `journal_mode=WAL`, `synchronous=NORMAL`, `foreign_keys=ON` |
| `userVersion(db)` / `setUserVersion(db, v)` | Schema version via `PRAGMA user_version` |
| `ScopedConnection(path, readOnly, prefix)` | RAII connection, uniquely named, closed and removed on destruction |
| `Transaction(db)` | RAII; **rolls back on destruction unless `commit()` succeeded** |
| `placeholders(n)`, `escapeLikePattern(s)` | Parameterised-query helpers |

`Transaction`'s rollback-by-default shape is why this design uses it everywhere: a writer that
returns early on error cannot leave a half-written record behind.

The working reference implementation is `src/QtLocationPlugin/QGCTileCacheDatabase.cpp` — schema
creation, `user_version` checking, per-operation transactions, and a separate-file export built on
`ScopedConnection`. Aerora follows its structure and **deliberately diverges on two points**, both
in [§5](#5-schema-versioning) and [§6](#6-durability).

## 3. Storage locations

Aerora does not invent a path convention. It uses the existing `AppSettings::savePath` root and adds
one subdirectory, exactly as the nine existing categories do.

### The root

`AppSettings::savePath` (`src/Settings/AppSettings.cc:95-145`), resolved per platform:

| Platform | Root | Note |
| --- | --- | --- |
| Desktop (normal run) | `QStandardPaths::DocumentsLocation / <appName>` | Operator-visible and operator-changeable |
| Desktop (unit tests, `--simple-boot-test`) | `QStandardPaths::TempLocation / <appName>` | **Already handled upstream.** Tests never touch real operator data |
| iOS | `QStandardPaths::DocumentsLocation` | Exposed to the Files app by design |
| Android | SD card root if present, writable and permitted; otherwise `GenericDataLocation` | `savePath` is `setUserVisible(false)` on mobile |

### The Aerora subdirectory

A tenth sibling alongside `parameter`, `telemetry`, `mission`, `log`, `video`, `photo`, `crash`,
`mavlinkActions`, `settings`:

```text
<savePath>/
  aerora/
    aerora.db            the single Aerora store
    aerora.db-wal        WAL sidecar (SQLite-managed)
    aerora.db-shm        shared-memory index (SQLite-managed)
    export/              operator-initiated exports only
```

Created through `QGCFileHelper::ensureDirectoryExists()`, the same call
`AppSettings::_checkSavePathDirectories()` already makes for the other nine. Reached through an
`AppSettings`-style accessor so a `savePath` change relocates Aerora data with everything else.

### Why not the map cache location

The tile cache lives separately at `CacheLocation/QGCMapCache/qgcMapCache.db`
(`QGeoFileTileCacheQGC.cpp:173-190`), under a *cache* path because it is reconstructible by
re-downloading. **Aerora records are not reconstructible.** Putting them under a cache path invites
an OS or a user to clear them, so they belong under `savePath` with the other records.

### `disableAllPersistence` is honoured

`AppSettings::disableAllPersistence` already exists (`src/Settings/App.SettingsGroup.json`, default
`false`, labelled "Disable all data persistence", documented as "nothing will be saved to disk").
Upstream checks it before caching a tile (`QGeoFileTileCacheQGC.cpp:97`) and before telemetry
logging (`MAVLinkProtocol.cc:326`).

`P7-DAT-005` requirement: **Aerora checks it before opening the database, not per write.** When set,
no file is created, every writer is a no-op, and the UI reports persistence as disabled rather than
showing an empty timeline that reads as "nothing happened".

## 4. Schema

Six tables. Deliberately small: each exists because a named screen needs it.

```sql
-- One application run.
CREATE TABLE Sessions (
    sessionId     INTEGER PRIMARY KEY NOT NULL,
    startedUtcMs  INTEGER NOT NULL,
    endedUtcMs    INTEGER,              -- NULL while running, and after an unclean exit
    baseCommit    TEXT NOT NULL,        -- provenance, P7-HELP-004
    overlayCommit TEXT NOT NULL,
    customBuild   INTEGER NOT NULL,     -- was QGC_CUSTOM_BUILD actually on (ADR-012)
    authzEnforced INTEGER NOT NULL      -- isAuthorizationEnforced() at boot
);

-- Stable identity for a vehicle across sessions.
CREATE TABLE Vehicles (
    vehicleId     INTEGER PRIMARY KEY NOT NULL,
    fleetIdentity TEXT NOT NULL UNIQUE, -- format is OPEN; NOT the MAVLink sysid
    profileId     TEXT,
    firstSeenUtcMs INTEGER NOT NULL,
    lastSeenUtcMs  INTEGER NOT NULL
);

-- State transitions, operator actions and authorization outcomes on one axis (P7-TEL-004).
CREATE TABLE Events (
    eventId       INTEGER PRIMARY KEY NOT NULL,
    sessionId     INTEGER NOT NULL REFERENCES Sessions(sessionId) ON DELETE CASCADE,
    vehicleId     INTEGER REFERENCES Vehicles(vehicleId) ON DELETE SET NULL,
    occurredUtcMs INTEGER NOT NULL,
    monotonicUs   INTEGER NOT NULL,     -- ordering source of truth; see below
    category      INTEGER NOT NULL,
    subject       TEXT NOT NULL,
    stateClass    INTEGER,              -- AeroraTelemetryState::Class, NULL for non-value events
    detail        TEXT                  -- JSON; no schema claim is made about its contents
);

-- Per-value provenance, so an export can say what was reported and what was derived (P7-LOG-003).
CREATE TABLE EventValues (
    eventId       INTEGER NOT NULL REFERENCES Events(eventId) ON DELETE CASCADE,
    itemId        TEXT NOT NULL,
    stateClass    INTEGER NOT NULL,     -- never NULL: a value without a class is prohibited
    derived       INTEGER NOT NULL,     -- 1 = station-derived (P7-ARCH-004)
    valueText     TEXT,                 -- NULL is legitimate for UNKNOWN/UNAVAILABLE
    ageMs         INTEGER
);

-- Authorization decisions and operator approvals. See §7 before relying on this.
CREATE TABLE AuditRecords (
    recordId      INTEGER PRIMARY KEY NOT NULL AUTOINCREMENT,
    sessionId     INTEGER NOT NULL REFERENCES Sessions(sessionId) ON DELETE RESTRICT,
    occurredUtcMs INTEGER NOT NULL,
    monotonicUs   INTEGER NOT NULL,
    actor         TEXT NOT NULL,        -- operator identity, or the literal 'unresolved'
    action        TEXT NOT NULL,
    subject       TEXT NOT NULL,
    outcome       INTEGER NOT NULL,
    enforced      INTEGER NOT NULL,     -- was a policy evaluating at the time
    detail        TEXT,
    prevHash      TEXT,                 -- hash chain; see §7 for what it does NOT provide
    recordHash    TEXT
);

-- Writes the store could not complete. Counted rather than silently lost (P7-AUDIT-003).
CREATE TABLE WriteFailures (
    failureId     INTEGER PRIMARY KEY NOT NULL,
    sessionId     INTEGER NOT NULL REFERENCES Sessions(sessionId) ON DELETE CASCADE,
    occurredUtcMs INTEGER NOT NULL,
    targetTable   TEXT NOT NULL,
    reason        TEXT NOT NULL
);
```

### Four decisions worth stating

**`EventValues.stateClass` is `NOT NULL` while `valueText` is nullable.** The inversion is the point:
`UNKNOWN` and `UNAVAILABLE` are states with no value, and a record that stored the value but not the
class would make them indistinguishable from a null read. This is `P7-STATE-001` enforced by a
schema constraint rather than by a convention.

**`monotonicUs` alongside `occurredUtcMs`.** Wall-clock time can step backwards — NTP correction, a
GNSS time fix, an operator changing the clock. Ordering therefore comes from a monotonic source and
wall-clock is carried for human reading only. Without this, a clock step silently reorders a
timeline, which is worse than a visibly wrong timestamp.

**`AuditRecords` uses `ON DELETE RESTRICT`, every other table uses `CASCADE`.** Pruning a session
must not be able to delete audit records as a side effect. The constraint makes
[§8](#8-retention-and-pruning)'s "audit is not auto-prunable" rule a database property rather than a
rule the pruner must remember.

**`AUTOINCREMENT` on `AuditRecords.recordId` only.** Plain `INTEGER PRIMARY KEY` reuses rowids after
deletion; `AUTOINCREMENT` does not. A reused audit id would make a gap in the sequence
indistinguishable from a renumbering.

### Indices

```sql
CREATE INDEX idx_events_session_monotonic ON Events(sessionId, monotonicUs);
CREATE INDEX idx_events_category          ON Events(category, occurredUtcMs);
CREATE INDEX idx_eventvalues_event        ON EventValues(eventId);
CREATE INDEX idx_audit_session_monotonic  ON AuditRecords(sessionId, monotonicUs);
CREATE UNIQUE INDEX idx_vehicles_identity ON Vehicles(fleetIdentity);
```

Chosen for the two access patterns the screens actually have — "this session in order" and "this
category over time" — not speculatively. `Helios GCS`'s queryable-telemetry pattern
([benchmark §3.13](02-gcs-reference-benchmark.md#313-helios-gcs)) is the model, extended with the
state class that no reference product records.

## 5. Schema versioning

`PRAGMA user_version` via `QGCSqlHelper::userVersion()` / `setUserVersion()`, with a single
`kSchemaVersion` constant — the tile cache's mechanism.

### The deliberate divergence from the tile cache

`QGCTileCacheDatabase::_checkSchemaVersion()` (`QGCTileCacheDatabase.cpp:56-93`) drops every table
and rebuilds when it meets a version it does not recognise. **Correct for a cache. Prohibited here.**

| Version found | Tile cache | Aerora |
| --- | --- | --- |
| equal to `kSchemaVersion` | proceed | proceed |
| `0` with data (legacy) | drop and rebuild | **refuse to open**, preserve the file |
| older than current | drop and rebuild | run forward migrations; refuse if any step fails |
| newer than current | drop and rebuild | **refuse to open**, preserve the file |

A cache that resets loses nothing that cannot be re-downloaded. A record that resets loses the only
copy. On refusal Aerora leaves the file untouched, reports persistence as unavailable with the
version it found, and runs with no store — degraded and loud, rather than empty and quiet.

**No migration exists yet** because there is no version 2. The forward-migration slot is specified so
the first schema change is a migration rather than a reset; writing a migration framework before the
first migration would be speculative (`CODING_STYLE.md`: avoid overengineering).

## 6. Durability

This is where a persistence design most easily overclaims, so the mechanism and its limit are stated
together.

### What the shared pragmas give

`QGCSqlHelper::applySqlitePragmas()` sets `journal_mode=WAL` and `synchronous=NORMAL`.

| Property | Holds? | Why |
| --- | --- | --- |
| Database survives application crash | **Yes** | WAL replay. A committed transaction is in the WAL |
| Database survives OS crash / power loss without corruption | **Yes** | WAL is crash-safe by design |
| A transaction reported committed is still present after power loss | **No** | `synchronous=NORMAL` does not fsync the WAL on every commit. Recently committed transactions can be lost |
| Partial record written | **No** | `Transaction` rolls back on destruction unless `commit()` succeeded |

The third row is the one that matters and the one usually glossed. With `synchronous=NORMAL`,
"committed" means "durable against process death", not "durable against power loss".

### The deviation for audit

`AuditRecords` writes run on a connection with `synchronous=FULL` applied **after**
`applySqlitePragmas()`:

| | `NORMAL` | `FULL` |
| --- | --- | --- |
| Power-loss durability of last commit | **Not guaranteed** | Guaranteed, subject to the hardware honouring fsync |
| Cost | — | An fsync per commit |

Accepted deliberately: audit write volume is operator decisions and authorization outcomes, which is
orders of magnitude below telemetry, so the fsync cost is paid on a rare path. `Events` keeps
`NORMAL` — losing the last few timeline rows on power loss is a degradation, not a safety claim.

**The residual limit, stated rather than omitted:** `synchronous=FULL` is only as good as the storage
stack beneath it. A device or filesystem that acknowledges fsync without committing to media defeats
it, and Aerora cannot detect that. The claim is therefore "fsync is requested per audit commit", not
"audit records cannot be lost".

## 7. Q-037 assessment: can this carry audit?

The question WP1-C was asked to answer. **Partially — and the part it cannot carry must not be
claimed.**

`Q-037` asks for an audit mechanism that is **durable** and **tamper-evident**. These are different
problems with different answers.

### Durability — yes, with the limit in §6

| Requirement | Verdict |
| --- | --- |
| Records survive application crash | **Yes**, WAL |
| Records survive power loss | **Yes** for committed records, with `synchronous=FULL`, subject to the hardware honouring fsync |
| Records cannot be partially written | **Yes**, `Transaction` rollback-by-default |
| Records cannot be silently dropped on write failure | **Yes**, `WriteFailures` counts them (`P7-AUDIT-003`) |
| Records cannot be deleted by session pruning | **Yes**, `ON DELETE RESTRICT` |

This is a real improvement over the current `AeroraAuditSink`, which is `CONTRACT-ONLY` and reports
itself as neither durable nor tamper-evident.

### Tamper-evidence — no, and a hash chain does not change that

`prevHash` / `recordHash` form a chain: each record hashes its own fields plus its predecessor's
hash. What that buys, precisely:

| Property | Provided? |
| --- | --- |
| Detect modification of a record in place | **Yes** — the chain breaks |
| Detect deletion of a record from the middle | **Yes** — the chain breaks |
| Detect truncation of the tail | **No** — a shorter valid chain is still a valid chain |
| Detect wholesale re-computation after editing | **No** — an actor with write access recomputes every hash and the chain verifies |
| Prevent any of the above | **No** |

The third and fourth rows are the whole difficulty. A hash chain is tamper-evident only against an
actor who **cannot** recompute it, which requires the chain head to be anchored somewhere that actor
cannot write — a protected key, a hardware counter, an append-only remote, or a signed external
witness.

**Aerora has none of these.** `AeroraPlatformKeyProtector` is `CONTRACT-ONLY` with **no backend on
any of the five target platforms**, so there is no protected location for an anchor. A remote witness
is excluded by `P7-ARCH-010` and `P7-ARCH-013` — local-first, no automatic outbound transmission.

### The honest conclusion

| Claim | Supportable |
| --- | --- |
| "Audit records are durably stored" | **Yes**, with the §6 hardware caveat |
| "Audit records are ordered and the ordering is recoverable" | **Yes** — `monotonicUs` + `AUTOINCREMENT` |
| "Accidental corruption or partial write is detectable" | **Yes** — the chain detects it |
| "Audit records are tamper-evident" | **No. Must not be claimed** |
| "Audit records are tamper-proof" | **No** |

So: implement the chain, because corruption detection and ordering recovery are worth having and
cost almost nothing. **Describe it as integrity checking, never as tamper-evidence.** The field names
are `recordHash` and `prevHash` rather than anything containing "signature" or "seal" for exactly
this reason.

`Q-037` therefore moves from `OPEN` to **`PARTIALLY RESOLVED`**: durability is answerable with this
design; tamper-evidence stays `OPEN` and is **blocked on a protected anchor**, which is blocked on
`AeroraPlatformKeyProtector` having a backend, which is blocked on `D-006`. The dependency chain is
recorded rather than worked around.

## 8. Retention and pruning

### Operator-controlled, per category

`P7-DAT-005`. Limits are `SettingsFact`s in an Aerora settings group, mirroring how
`maxCacheDiskSize` drives tile pruning (`QGeoFileTileCacheQGC.cpp:84`).

| Category | Prunable | Limit |
| --- | --- | --- |
| `Events` + `EventValues` | **Yes**, automatically | age and total-size caps |
| `Sessions` | **Yes**, when no audit record references them | follows `Events` |
| `Vehicles` | **No** automatic prune | operator deletes a record explicitly |
| `AuditRecords` | **No automatic prune, ever** | operator action only, and only with an explicit confirmation |
| `WriteFailures` | **Yes** | follows `Events` |

### No default values are specified

The age and size caps are `OPEN`. `P7-NOINV-002`: a retention period is a policy decision with
regulatory weight in some domains, and inventing "30 days" or "500 MB" here would create a default
that outlives the decision it substituted for. The settings facts are specified; their defaults are
a product decision.

### Pruning is explicit, not a side effect

Runs on an operator action or at a declared point (session start), never opportunistically mid-flight
— a prune competing with a telemetry write for the same WAL is a latency source during an operation.
Deletes in bounded batches inside a `Transaction`, so an interrupted prune leaves a consistent
database.

Audit pruning being operator-only is enforced twice: by policy here, and by `ON DELETE RESTRICT` in
[§4](#4-schema). The second is the one that holds when someone writes a new pruner.

## 9. Export

### Mechanism

Two forms, both operator-initiated:

| Form | Use | Mechanism |
| --- | --- | --- |
| SQLite file | Full fidelity, re-importable | `ScopedConnection` to a new file + schema creation + per-table `Transaction` — exactly `QGCTileCacheDatabase::exportSets()` (`QGCTileCacheDatabase.cpp:1046-1200`) |
| CSV | Timeline review in external tools | One row per `EventValues` row, state class and `derived` as columns |

Written to `<savePath>/aerora/export/`. The SQLite form reuses the tile cache's proven shape;
nothing new is invented.

### Provenance is mandatory in both forms

`P7-LOG-003`. Every exported value carries its `stateClass` and its `derived` flag. An export that
dropped them would present a station-derived estimate and a vehicle-reported measurement as the same
kind of number, which is the single failure `P7-ARCH-004` exists to prevent. In CSV these are
columns, not a header note, so a spreadsheet filter cannot lose them.

Export is read-only with respect to the store: it opens a second connection and never mutates the
source. A failed export leaves the store untouched.

## 10. No automatic outbound transmission

`P7-ARCH-013`, `P7-DAT-004`. Structural rather than policy:

| Property | How |
| --- | --- |
| The store has no network code | It links Qt Sql and Qt Core. No `QNetworkAccessManager`, no socket, no URL |
| Export writes to a local path only | The destination is a file path from a file dialog |
| No scheduled or background upload exists | There is no scheduler and no upload path to disable |
| Cloud sync is not in this design | Deliberately out of scope. A future optional sync is a separate component reading the store through its public interface, and is subject to `P7-ARCH-011` — never an input to authorization |

A test asserts the store's translation units reference no network type, which is the
[`STATIC` verification class](06-requirements-to-test-traceability.md#a-note-on-the-static-class):
the prohibition is verified by showing the code does not exist.

## 11. Failure behaviour

Follows the precedent set by the two existing persistence paths, which agree with each other: **warn,
degrade, stay running. Never fail hard, never silently lose.**

| Upstream precedent | Behaviour |
| --- | --- |
| Tile cache open failure | `qCCritical` + `_failed = true`; feature degrades, app continues (`QGCTileCacheDatabase.cpp:127-137`) |
| Tile cache schema invalid | Delete and rebuild; app continues |
| No cache directory writable | Multi-step fallback, then `qCCritical` and empty path (`QGeoFileTileCacheQGC.cpp:173-195`) |
| Telemetry log open failure | `QGC::showAppMessage()` **user-visible**, suspend logging (`MAVLinkProtocol.cc:351-360`) |
| Telemetry log write failure | `showAppMessage()`, `cancelWriting()`, remove the partial file (`MAVLinkProtocol.cc:460-480`) |

### Aerora's behaviour

| Failure | Response |
| --- | --- |
| `disableAllPersistence` set | No file opened. Writers are no-ops. **UI reports persistence as disabled** |
| Directory not creatable | `qCWarning`, run with no store, UI reports unavailable |
| Database will not open | `qCWarning`, run with no store, UI reports unavailable. **File is not deleted** |
| Schema version unrecognised | Refuse to open, **preserve the file**, report the version found ([§5](#5-schema-versioning)) |
| A write fails | Record in `WriteFailures`, increment a counter, continue |
| An **audit** write fails | Same, plus `QGC::showAppMessage()` — user-visible, following the telemetry-log precedent for data loss |
| `WriteFailures` itself fails | Increment an in-memory counter only. The UI reports a non-zero unrecorded-failure count |
| Disk full | Surfaces as write failures. No pre-emptive space check: a check-then-write race would report success and then lose the row |

### The one deliberate asymmetry

An audit write failure is **user-visible**; an event write failure is not. An operator losing a
timeline row has lost convenience; an operator whose authorization decision was not recorded has lost
the evidence that the decision happened. Whether an unrecordable *allow* must be **denied** is a
different question, is `OPEN`, and belongs to product (`P7-AUDIT-003`). This design makes it
**visible**; it does not make it blocking.

## 12. Data-integrity limitations

Stated as a list so none of it has to be inferred from the absence of a claim.

| # | Limitation |
| --- | --- |
| 1 | **Not tamper-evident.** An actor with write access to the file can rewrite records and recompute the chain. [§7](#7-q-037-assessment-can-this-carry-audit) |
| 2 | **Tail truncation is undetectable.** A shorter valid chain verifies |
| 3 | **`synchronous=FULL` is only as strong as the hardware.** A device acknowledging fsync without committing to media defeats it, undetectably |
| 4 | **`Events` can lose recent rows on power loss** — `synchronous=NORMAL`, accepted |
| 5 | **No encryption at rest.** Records are readable by anyone who can read the file. Matches the current signing-key posture, which is also unprotected with no platform backend |
| 6 | **No multi-process locking strategy.** Two Aerora instances against one file is undefined; SQLite will serialise but Aerora asserts nothing about the result |
| 7 | **Wall-clock timestamps can be wrong.** Ordering is recoverable via `monotonicUs`; absolute times are as good as the host clock was |
| 8 | **`monotonicUs` does not survive a reboot.** It orders within a session; cross-session ordering falls back to wall clock |
| 9 | **`detail` is opaque JSON.** No schema is claimed or validated. A consumer must tolerate anything |
| 10 | **Nothing here is compiled.** `D-006`. No schema has been created, no transaction executed, no failure path exercised |

Items 1, 2 and 5 are the ones that must appear in any deployment document that mentions audit
(`P7-PKG-005`).

## 13. Implementation owners

The store is Tab 1's; the writers belong to the domains that generate the records. This keeps the
schema under one owner while avoiding Tab 1 writing other tabs' semantics.

| Component | Owner | Files | Depends on |
| --- | --- | --- | --- |
| `AeroraLocalStore` — open, schema, version check, transactions, failure accounting | **Tab 1** | new `aerora/src/Persistence/` + `Persistence.cmake` | `QGCSqlHelper`, `AppSettings` |
| Path accessor + Aerora settings group for retention | **Tab 1** | same, plus settings JSON | `AppSettings::savePath` |
| Provenance source (`Sessions` columns) | **Tab 1** | WP1-D `AeroraBuildInfo` | — |
| `Events` / `EventValues` writer and the timeline model | **Tab 4** | `aerora/src/Telemetry/` | store; `P7-TEL-004` |
| `AuditRecords` writer — the `AeroraAuditSink` implementation | **Tab 2** | `aerora/src/Security/` | store; `Q-037` |
| `Vehicles` writer and fleet records | **Tab 3** | `aerora/src/Vehicle/` | store; **fleet identity format `OPEN`** |
| Export (SQLite + CSV) | **Tab 1** store side, **Tab 7** UI | `aerora/src/Persistence/`, `aerora/src/UI/` | store |
| `SCR-TEL-02`, `SCR-LOG-01`, `SCR-LOG-02`, `SCR-SEC-03`, `SCR-FLT-01`, `SCR-FLT-02` | **Tab 7** | `aerora/src/UI/` | the writers above |

### Sequencing

1. Tab 1: `AeroraLocalStore` + schema + path + failure accounting. Nothing else can start
2. Tab 1: WP1-D provenance, which `Sessions` needs
3. Tab 4 and Tab 2 in parallel: their writers. Independent tables, no shared file
4. Tab 3: `Vehicles` writer — **gated on the fleet identity decision**, not on the store
5. Tab 7: the six screens

New module, so `aerora/CMakeLists.txt` needs the manifest include and a sentinel mapping if any
module declares a hard dependency on it. Both are Tab 1's file.

## 14. Open decisions

| ID | Decision | Owner | Blocks |
| --- | --- | --- | --- |
| `D-DAT-01` | Retention age and size caps for `Events` | **Product** | Default values only; the mechanism ships without them |
| `D-DAT-02` | Whether audit records may be deleted by an operator at all, or only exported | **Product + compliance** | The confirmation flow in `SCR-SEC-03` |
| `D-DAT-03` | Fleet identity format | **Product** | `Vehicles` table population, `SCR-FLT-01`, `SCR-FLT-02` |
| `D-DAT-04` | `Events.category` enumeration | Tab 4 | `Events` writer |
| `D-DAT-05` | Whether a protected chain anchor will ever exist, and on which platforms | Tab 2 + architecture | **Tamper-evidence. Blocked on `AeroraPlatformKeyProtector`, blocked on `D-006`** |
| `D-DAT-06` | Encryption at rest — required or not | Product + security | Limitation 5 |
| `D-DAT-07` | Multi-instance behaviour: prevent, or define | Tab 1 | Limitation 6 |

### Status changes this document justifies

| Item | From | To |
| --- | --- | --- |
| `P7-DAT-002` | `SPECIFIED-NOT-DESIGNED` | **`DESIGNED`** |
| `P7-DAT-005` | `SPECIFIED-NOT-DESIGNED` | **`DESIGNED`** |
| `Q-037` | `OPEN` | **`PARTIALLY RESOLVED`** — durability designed; tamper-evidence stays `OPEN` and is now explicitly blocked on `D-DAT-05` |
| `SCR-TEL-02`, `SCR-LOG-01`, `SCR-LOG-02`, `SCR-SEC-03` | `SPECIFIED-NOT-DESIGNED` | **`DESIGNED`**, pending their writers |
| `SCR-FLT-01`, `SCR-FLT-02` | `SPECIFIED-NOT-DESIGNED` | still blocked — on `D-DAT-03`, not on the store |

**No code was written by this document. Nothing here has been compiled or executed.**

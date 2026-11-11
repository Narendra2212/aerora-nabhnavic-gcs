#pragma once

#include <memory>
#include <optional>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QVariantMap>

#include "AeroraTelemetryState.h"

class AeroraLocalStore;

namespace QGCSqlHelper {
class ScopedConnection;
}

/// \brief The category of a timeline `Events` row. Owner-approved (`D-DAT-04`).
///
/// The coarse axis a reviewer filters a session timeline on. The authoritative, specific description
/// is the `subject` column and the per-value rows; this is only the kind. Integer values are pinned
/// so a persisted row keeps its meaning if the enumeration is reordered.
enum class AeroraEventCategory
{
    /// A vehicle or station state moved from one class to another.
    StateTransition = 0,
    /// An operator did something the timeline should carry.
    OperatorAction = 1,
    /// An authorization decision, denial or manual-authority transition (mirrors the audit trail
    /// on the one timeline, `P7-TEL-004`).
    AuthorizationOutcome = 2,
    /// A vehicle-reported event that is not itself a state-class transition.
    VehicleEvent = 3,
    /// A station or system event with no vehicle subject.
    SystemEvent = 4,
};

/// \brief One timeline event to persist. The producer composes it; this is not the mapping of
/// telemetry onto events, which is `M-Telemetry`'s and separate.
struct AeroraEventRecord
{
    AeroraEventCategory category = AeroraEventCategory::SystemEvent;
    QString subject;
    qint64 occurredUtcMs = 0;

    /// Ordering source of truth; wall-clock `occurredUtcMs` is for reading only (design §4).
    qint64 monotonicUs = 0;

    /// The event-level state class, absent for a non-value event. Stored NULL when absent.
    std::optional<AeroraTelemetryState::Class> stateClass;

    QVariantMap detail;

    // vehicleId is deliberately not a field: the Vehicles writer does not exist yet, so an event
    // references no vehicle row and is stored with a NULL vehicleId. Adding it once that writer
    // lands is a column already present in the schema.
};

/// \brief One per-value provenance row (`EventValues`).
struct AeroraEventValue
{
    QString itemId;

    /// Never absent: a value without a class is prohibited (`P7-STATE-001`), which the schema
    /// enforces with `NOT NULL`.
    AeroraTelemetryState::Class stateClass = AeroraTelemetryState::Unknown;

    /// 1 = station-derived rather than vehicle-reported (`P7-ARCH-004`). The producer supplies it;
    /// an export must keep it so a derived estimate never reads as a measurement.
    bool derived = false;

    /// Absent is legitimate for `Unknown`/`Unavailable` and is stored NULL, which is why the class
    /// is `NOT NULL` while the value is not: the two emptinesses stay distinguishable.
    std::optional<QString> valueText;

    std::optional<qint64> ageMs;
};

/// \brief Persists `Events` and their `EventValues` through `AeroraLocalStore`. `P7-TEL-004`,
/// design 09-local-persistence-architecture.md §4/§13.
///
/// \section what What this is
///
/// The timeline writer `M-Telemetry` owns. It is a persistence mechanism: a producer hands it a
/// composed event and its value rows, and it writes them in one transaction so a half-written event
/// never persists. The producer - the mapping of telemetry state onto events worth recording - is
/// separate and not built here.
///
/// \section durability Durability, and the deliberate asymmetry with audit
///
/// Events run on the store's shared `synchronous=NORMAL` connection, not the audit `FULL` one:
/// losing the last few timeline rows on power loss is a degradation, not a safety claim (design §6),
/// and a write failure is counted in `WriteFailures` but is **not** user-visible, unlike an audit
/// write failure (design §11). The asymmetry is intended.
///
/// \section connection Its own connection
///
/// Like the audit writer, it opens its own connection to the store's database rather than reusing
/// the store's, and takes the store only for the database path, the active session id, and failure
/// accounting. It writes no `AppSettings`, so it is unit-testable against a temporary store.
class AeroraEventStore
{
public:
    explicit AeroraEventStore(AeroraLocalStore& store);
    ~AeroraEventStore();

    AeroraEventStore(const AeroraEventStore&) = delete;
    AeroraEventStore& operator=(const AeroraEventStore&) = delete;
    AeroraEventStore(AeroraEventStore&&) = delete;
    AeroraEventStore& operator=(AeroraEventStore&&) = delete;

    /// Opens a connection to the store's database. Requires the store to be open. Idempotent;
    /// returns true when ready to record.
    bool open();
    void close();

    [[nodiscard]] bool isOpen() const { return _ready; }

    /// Persists \a event and its \a values in one transaction. Returns whether it succeeded; a
    /// failure is counted in the store's `WriteFailures` and is not surfaced to the operator
    /// (design §11). An event may carry no values.
    bool recordEvent(const AeroraEventRecord& event, const QList<AeroraEventValue>& values);

private:
    AeroraLocalStore& _store;
    std::unique_ptr<QGCSqlHelper::ScopedConnection> _connection;
    bool _ready = false;
};

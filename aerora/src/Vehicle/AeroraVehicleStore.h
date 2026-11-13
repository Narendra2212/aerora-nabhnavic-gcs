#pragma once

#include <memory>

#include <QtCore/QString>

class AeroraLocalStore;

namespace QGCSqlHelper {
class ScopedConnection;
}

/// \brief Mints or recalls the Aerora fleet identity for a board. Implements `DG-FLT`'s
/// identity-format decision. `P7-DAT-002`/`WP3-E`, design 09-local-persistence-architecture.md,
/// closure `.kiro/specs/aerora-decision-gate-closure/closures/DG-FLT.identity-format.json`.
///
/// \section what What this is
///
/// This class is the authority the closed decision names: "the station persistence store, on
/// first contact." Given a board identifier - the caller's already-resolved
/// `AUTOPILOT_VERSION.uid2`, falling back to `uid`, a MAVLink decode this class never performs
/// itself - it looks the value up in the `Vehicles` table and either returns the fleet identity
/// already bound to it, or mints a UUIDv4, binds it, and returns that instead. `lastSeenUtcMs` is
/// stamped on every call; `firstSeenUtcMs` is set once, at mint.
///
/// \section form The identity is an opaque, store-side convention
///
/// Minted as a UUID version 4, stored and compared as its canonical lowercase hyphenated string -
/// explicitly lower-cased here rather than trusting a platform's default casing, because the
/// decision pins that form. Nothing here parses an identity once minted; `AeroraVehicleIdentity`,
/// which carries the result, does not either.
///
/// \section scope Scope and lifetime, as settled
///
/// Unique within this one store; never claimed unique across stations (two stations meeting the
/// same aircraft independently mint different identifiers - a recorded limit, not a defect). The
/// board UID both stores record is what a future cross-store correlation would need, and nothing
/// here forecloses it. Permanent once minted: this class never deletes a `Vehicles` row and never
/// reassigns a `fleetIdentity` already bound to a board, so an identifier survives a MAVLink
/// system id change, a link change, a reconnection and a flight-control board swap.
///
/// \section notnewidentity Never derived from the MAVLink system id
///
/// This class has no parameter through which one could reach it: its only input is the board
/// identifier string the caller already resolved. `DG-FLT-SOC-02` names this explicitly as the
/// condition the closure verified.
///
/// \section connection Its own connection
///
/// Like the other per-table writers, it opens its own connection to the store's database and takes
/// the store only for its path and write-failure accounting. It keeps no session id: unlike
/// `Events` or `AuditRecords`, a `Vehicles` row is a standing fleet record and is not scoped to a
/// session.
class AeroraVehicleStore
{
public:
    explicit AeroraVehicleStore(AeroraLocalStore& store);
    ~AeroraVehicleStore();

    AeroraVehicleStore(const AeroraVehicleStore&) = delete;
    AeroraVehicleStore& operator=(const AeroraVehicleStore&) = delete;
    AeroraVehicleStore(AeroraVehicleStore&&) = delete;
    AeroraVehicleStore& operator=(AeroraVehicleStore&&) = delete;

    /// Opens a connection to the store's database. Requires the store to be open. Idempotent;
    /// returns true when ready to resolve.
    bool open();
    void close();

    [[nodiscard]] bool isOpen() const { return _ready; }

    /// Looks up \a boardUid in the `Vehicles` table and returns its bound fleet identity, minting
    /// and binding a new one if \a boardUid is not yet known. Stamps `lastSeenUtcMs` with \a
    /// nowUtcMs on every successful call, and `firstSeenUtcMs` only when the row is created.
    ///
    /// \a boardUid must be non-empty: an empty board identifier is not evidence of a board, and is
    /// refused rather than minted against - a first contact with nothing to look up is not a
    /// reason to invent an identity. Returns the bound fleet identity, or an empty string on
    /// refusal or failure; the two are not distinguished in the return value because neither
    /// yields an identity to report, and a write failure is already counted in the store's
    /// `WriteFailures`.
    [[nodiscard]] QString resolveFleetIdentity(const QString& boardUid, qint64 nowUtcMs);

private:
    AeroraLocalStore& _store;
    std::unique_ptr<QGCSqlHelper::ScopedConnection> _connection;
    bool _ready = false;
};

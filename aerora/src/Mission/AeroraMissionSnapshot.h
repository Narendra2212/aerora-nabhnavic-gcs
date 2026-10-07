#pragma once

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QString>

#include "QGCMAVLink.h"

class MissionItem;

/// One flattened mission item, reduced to the fields that actually cross the wire.
///
/// The field set is taken from what `MISSION_ITEM_INT` carries and QGroundControl actually packs
/// and unpacks, not from everything `MissionItem` holds. Two deliberate omissions:
///
/// \li **`isCurrentItem` is not here.** QGroundControl transmits the `current` flag as
///     `seq == 0` for every item regardless of what the item says, so the flag never survives a
///     round trip and comparing it would report a difference that does not exist on the vehicle.
/// \li **`doJumpId` is not here.** It is a plan-file construct with no wire representation.
struct AeroraMissionItemFingerprint
{
    int sequenceNumber = -1;
    MAV_CMD command = MAV_CMD_ENUM_END;
    MAV_FRAME frame = MAV_FRAME_ENUM_END;
    double param1 = 0.0;
    double param2 = 0.0;
    double param3 = 0.0;
    double param4 = 0.0;
    double param5 = 0.0;
    double param6 = 0.0;
    double param7 = 0.0;
    bool autoContinue = false;

    /// @return true when sequence number and command agree.
    [[nodiscard]] bool sameCommand(const AeroraMissionItemFingerprint &other) const;

    /// @return true when sameCommand() holds and every remaining field agrees **at wire
    ///         precision** — see AeroraMissionSnapshot for why that qualifier matters.
    [[nodiscard]] bool sameAtWirePrecision(const AeroraMissionItemFingerprint &other) const;
};

Q_DECLARE_METATYPE(AeroraMissionItemFingerprint)

/// The result of comparing a ground-station plan against a vehicle mission, with the depth that
/// produced it.
///
/// The depth is part of the verdict rather than an argument the caller remembers, because
/// "matches" means something materially different at each depth and the difference is exactly
/// what an operator needs in order to trust the answer. Verification depth is Q-041 and is
/// **not decided here**: this type reports the depth that was performed and refuses to imply
/// more.
///
/// Verdict::NotCompared is the initial state and is the honest answer for an upload that has not
/// been checked. `AERORA-MISSION-REQ-007` requires such an upload to be presented as unverified,
/// so NotCompared must never be rendered as agreement.
class AeroraMissionAgreement
{
    Q_GADGET
    Q_PROPERTY(Verdict verdict READ verdict CONSTANT)
    Q_PROPERTY(Depth depth READ depth CONSTANT)
    Q_PROPERTY(int firstDifferenceIndex READ firstDifferenceIndex CONSTANT)

public:
    enum Verdict {
        /// No comparison has been performed. An unverified upload, not an agreeing one.
        NotCompared = 0,
        /// The two agree, to the recorded depth and no further.
        Matches,
        /// They disagree. firstDifferenceIndex() locates the first disagreeing position.
        Differs,
    };
    Q_ENUM(Verdict)

    /// How deeply a comparison looked. Each level includes the ones above it.
    enum Depth {
        /// Nothing was compared.
        DepthNone = 0,
        /// Item counts only. Catches truncation.
        DepthItemCount,
        /// Counts plus the sequence of commands. Catches truncation and reordering.
        DepthCommandSequence,
        /// The above plus every transmitted parameter, at wire precision. Catches an altered
        /// parameter. This is the deepest depth available without a mission download, and it is
        /// the deepest this module implements — a read-back comparison costs a full download per
        /// upload and the decision to pay that is Q-041.
        DepthFullParameters,
    };
    Q_ENUM(Depth)

    AeroraMissionAgreement() = default;

    [[nodiscard]] static AeroraMissionAgreement matches(Depth depth);
    [[nodiscard]] static AeroraMissionAgreement differs(Depth depth, int firstDifferenceIndex, const QString &reason);

    [[nodiscard]] Verdict verdict() const { return _verdict; }
    [[nodiscard]] Depth depth() const { return _depth; }

    /// @return The first disagreeing position, or -1 when there is none or the disagreement is
    ///         not positional (a count mismatch, for instance).
    [[nodiscard]] int firstDifferenceIndex() const { return _firstDifferenceIndex; }

    /// Machine-readable description of the disagreement, for logs and records. Empty unless the
    /// verdict is Differs. Not an operator-facing label.
    [[nodiscard]] QString reason() const { return _reason; }

    /// @return true only for Verdict::Matches. False for NotCompared, so an unchecked upload is
    ///         never treated as verified.
    [[nodiscard]] Q_INVOKABLE bool isVerified() const { return _verdict == Matches; }

    [[nodiscard]] static QString verdictToString(Verdict verdict);
    [[nodiscard]] static QString depthToString(Depth depth);

private:
    Verdict _verdict = NotCompared;
    Depth _depth = DepthNone;
    int _firstDifferenceIndex = -1;
    QString _reason;
};

Q_DECLARE_METATYPE(AeroraMissionAgreement)

/// An immutable, flattened view of one mission, tagged with which side of the boundary it came
/// from.
///
/// This is the type that keeps `MISSION PLAN` and `VEHICLE MISSION` apart in Aerora code. Both
/// sides are a `QList<MissionItem*>` in QGroundControl — `MissionController` flattens the plan
/// into one on upload, and `PlanManager` holds the vehicle's in another — so the two are trivially
/// assignable to each other and nothing in the type system objects. Origin is recorded here and
/// compare() requires one of each, so a plan cannot be verified against itself.
///
/// \section wire Wire precision
///
/// A plan-side parameter is a full-precision `double`. The same parameter returned from the
/// vehicle has been through `MISSION_ITEM_INT`, which is lossy in two different ways, both read
/// from the pack/unpack sites in `PlanManager`:
///
/// \li `param1`–`param4` and `param7` travel as `float`.
/// \li `param5`/`param6` travel in `int32` fields. For every frame except `MAV_FRAME_MISSION`
///     they are scaled by 1e7 first, so they quantise to 1e-7 degrees. For `MAV_FRAME_MISSION`
///     they are placed in the integer fields unscaled, so they quantise to whole units.
///
/// Comparing raw doubles would therefore report a difference for almost every mission that
/// round-tripped correctly. compare() normalises both sides through the same transform before
/// comparing, which is why its result is qualified "at wire precision" rather than "equal".
/// A difference below wire precision is not observable on the vehicle and is not a difference.
class AeroraMissionSnapshot
{
    Q_GADGET
    Q_PROPERTY(Origin origin READ origin CONSTANT)
    Q_PROPERTY(int itemCount READ itemCount CONSTANT)
    Q_PROPERTY(bool captured READ isCaptured CONSTANT)

public:
    /// Which side of the plan/vehicle boundary a snapshot came from.
    enum Origin {
        /// Nothing has been captured. Not an empty mission — an absent one.
        NotCaptured = 0,
        /// The ground station's plan, flattened the way an upload would flatten it.
        GcsPlan,
        /// The mission as the vehicle reported it.
        VehicleStore,
    };
    Q_ENUM(Origin)

    AeroraMissionSnapshot() = default;

    /// An empty snapshot attributed to @a origin. Distinct from the default: this one records
    /// that a side was captured and found to contain no items.
    [[nodiscard]] static AeroraMissionSnapshot empty(Origin origin);

    /// Captures @a items, which may be either side's flattened list.
    ///
    /// Null entries are skipped and logged rather than dereferenced. @a origin must be stated by
    /// the caller because nothing about a `MissionItem` list reveals which side it belongs to.
    [[nodiscard]] static AeroraMissionSnapshot fromMissionItems(Origin origin, const QList<MissionItem *> &items);

    /// Appends one fingerprint. The builder used by tests and by the mock, so neither needs to
    /// construct `MissionItem` objects.
    void append(const AeroraMissionItemFingerprint &fingerprint);

    [[nodiscard]] Origin origin() const { return _origin; }

    /// @return true when some source captured this side, even if it contained no items.
    [[nodiscard]] bool isCaptured() const { return _origin != NotCaptured; }

    [[nodiscard]] int itemCount() const { return static_cast<int>(_items.count()); }

    [[nodiscard]] const QList<AeroraMissionItemFingerprint> &items() const { return _items; }

    /// Compares a plan against a vehicle mission at @a depth.
    ///
    /// Argument order is enforced by Origin: @a plan must be GcsPlan and @a vehicleMission must be
    /// VehicleStore. Anything else returns Verdict::NotCompared, because comparing two snapshots
    /// of the same side answers no question anyone asked and silently returning Matches for it
    /// would be the worst possible failure mode.
    ///
    /// An uncaptured snapshot on either side also returns NotCompared: absent is not empty, and
    /// an absent side cannot be shown to agree with anything.
    [[nodiscard]] static AeroraMissionAgreement compare(const AeroraMissionSnapshot &plan,
                                                        const AeroraMissionSnapshot &vehicleMission,
                                                        AeroraMissionAgreement::Depth depth);

    [[nodiscard]] static QString originToString(Origin origin);

private:
    Origin _origin = NotCaptured;
    QList<AeroraMissionItemFingerprint> _items;
};

Q_DECLARE_METATYPE(AeroraMissionSnapshot)

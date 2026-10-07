#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QString>

/// A mission sequence number carried together with where it came from.
///
/// Three different indices exist in a running ground station, they are all plain `int` in
/// QGroundControl, and conflating any two of them produces a display that is confidently wrong:
///
/// \li The **vehicle current index** is what the aircraft reports it is executing. It arrives in
///     `MISSION_CURRENT` (or `HIGH_LATENCY`/`HIGH_LATENCY2` `wp_num`) and is the only one of the
///     three with any authority over what the aircraft is doing.
/// \li The **plan-view selection** is which item the operator has selected in the editor. It is
///     never transmitted and says nothing about the aircraft. In QGroundControl it is
///     `MissionController::currentPlanViewSeqNum`, which is a different property from
///     `MissionController::currentMissionIndex` — a distinction that is easy to lose in a binding.
/// \li The **resume candidate** is a ground-station computation over the last index observed while
///     in mission flight mode. It is a proposal for where a resume would start, not a position.
///
/// This type exists so that confusion is a compile error rather than a review finding. There is
/// no constructor taking a bare sequence number, no API that changes an existing value's origin,
/// and no arithmetic, so a plan-view selection cannot become a vehicle report by assignment,
/// increment or copy. isVehicleAuthoritative() is the predicate a consumer must use before
/// presenting a number as the aircraft's position in the mission.
///
/// \section sentinel The -1 sentinel
///
/// QGroundControl uses -1 for "there is no current index", and sets it deliberately: a successful
/// mission write resets both the current and last-current index to -1 before the vehicle has
/// reported anything about the new mission. That is a distinct condition from "nothing has been
/// observed", so an attributed value with no sequence number is representable: isAttributed() is
/// true while isKnown() is false. A consumer must not render that as item 0.
class AeroraMissionIndex
{
    Q_GADGET
    Q_PROPERTY(int sequenceNumber READ sequenceNumber CONSTANT)
    Q_PROPERTY(Origin origin READ origin CONSTANT)
    Q_PROPERTY(bool attributed READ isAttributed CONSTANT)
    Q_PROPERTY(bool known READ isKnown CONSTANT)
    Q_PROPERTY(bool vehicleAuthoritative READ isVehicleAuthoritative CONSTANT)

public:
    /// Where a sequence number came from. There is no "any" or "generic" origin on purpose: a
    /// number whose provenance is unrecorded is Unattributed and carries no sequence number.
    enum Origin {
        /// Nothing has been attributed. The correct initial state, and never a position.
        Unattributed = 0,
        /// Reported by the aircraft. The only origin with authority over execution position.
        VehicleReported,
        /// Selected by the operator in the plan editor. Never transmitted, never a position.
        PlanViewSelection,
        /// A ground-station proposal for where a resume would start. Not a position.
        ResumeCandidate,
    };
    Q_ENUM(Origin)

    /// An unattributed index. A mission position that has not been reported is not position zero.
    AeroraMissionIndex() = default;

    /// An index as reported by the aircraft.
    ///
    /// @param sequenceNumber The reported sequence number, or -1 for "no current index".
    [[nodiscard]] static AeroraMissionIndex fromVehicleReport(int sequenceNumber);

    /// The operator's editor selection. Attributed, but never authoritative about the aircraft.
    [[nodiscard]] static AeroraMissionIndex fromPlanViewSelection(int sequenceNumber);

    /// A ground-station proposal for a resume start point.
    [[nodiscard]] static AeroraMissionIndex resumeCandidate(int sequenceNumber);

    /// @return The sequence number, or -1 when none is carried. Check isKnown() first.
    [[nodiscard]] int sequenceNumber() const { return _sequenceNumber; }

    [[nodiscard]] Origin origin() const { return _origin; }

    /// @return true when some source has been recorded, even if it carried no sequence number.
    [[nodiscard]] bool isAttributed() const { return _origin != Unattributed; }

    /// @return true only when a source is recorded **and** it carried a usable sequence number.
    [[nodiscard]] bool isKnown() const { return isAttributed() && (_sequenceNumber >= 0); }

    /// @return true only for Origin::VehicleReported. False for an editor selection and for a
    ///         resume candidate, so neither can be presented as the aircraft's position.
    [[nodiscard]] bool isVehicleAuthoritative() const { return _origin == VehicleReported; }

    /// Stable machine-readable identifier for logs and records, of the form
    /// `"<origin>:<seq>"`, or `"<origin>:none"` when no sequence number is carried.
    /// Not an operator-facing label.
    [[nodiscard]] Q_INVOKABLE QString toString() const;

    /// Stable machine-readable identifier for an origin. Not an operator-facing label.
    [[nodiscard]] static QString originToString(Origin origin);

    [[nodiscard]] bool operator==(const AeroraMissionIndex &other) const;
    [[nodiscard]] bool operator!=(const AeroraMissionIndex &other) const { return !(*this == other); }

private:
    int _sequenceNumber = -1;
    Origin _origin = Unattributed;
};

Q_DECLARE_METATYPE(AeroraMissionIndex)

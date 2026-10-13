#pragma once

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QString>

/// One mission check, its outcome, and — when it could not run — why not.
///
/// The three-outcome model is the whole point of this type. QGroundControl already has a mission
/// readiness gate, and it is a different shape: `VisualMissionItem::readyForSaveState()` returns
/// one of three values per item, and `MissionController::readyForSaveState()`
/// (`MissionController.cc:876-884`) returns the **first** non-ready item's state and stops. That
/// answers "may this be saved", which is a blocking gate. It cannot answer "what was checked",
/// "what passed", or — the one that matters — "what could not be checked at all".
///
/// `P7-CHK-002` requires the third category, and the reason it is a requirement rather than a
/// nicety is that the alternative is false assurance: a check that silently did not run looks
/// exactly like a check that passed, and an operator reading a clean report cannot tell them
/// apart.
///
/// ### NotPerformed is the default, deliberately
///
/// `Outcome::NotPerformed` is zero, so a default-constructed check is **not** passed. A check has
/// to be explicitly stated to have passed.
class AeroraMissionCheck
{
    Q_GADGET
    Q_PROPERTY(CheckId id READ id CONSTANT)
    Q_PROPERTY(Outcome outcome READ outcome CONSTANT)
    Q_PROPERTY(Unavailable unavailableReason READ unavailableReason CONSTANT)
    Q_PROPERTY(QString detail READ detail CONSTANT)
    Q_PROPERTY(bool performed READ isPerformed CONSTANT)

public:
    /// The checks this module can name. A check is listed here only when its semantics come from
    /// an existing contract or from QGroundControl source — never from a guessed product rule.
    ///
    /// Four of these are currently unperformable in every build, and they are listed anyway. That
    /// is the point: an absent check is invisible, and a named check that reports
    /// `NotPerformed` with a reason is not.
    enum CheckId {
        /// A mission was captured and carries at least one item.
        MissionPresence = 0,
        /// Sequence numbers are non-negative and strictly increasing.
        SequenceNumbering,
        /// Every item carries a frame QGroundControl can pack.
        FrameValidity,
        /// Items whose frame carries scaled latitude/longitude hold in-range, non-NaN values.
        CoordinateRange,
        /// The mission contains no command that makes an upstream resume refuse outright.
        ResumeCompatibility,
        /// Whether the items a resume would not carry forward can be told apart from the ones it
        /// is *supposed* not to carry forward.
        ResumeStateCarryForward,
        /// Every command in the mission is in the declared supported mission command set.
        MissionCommandSupport,
        /// The mission's commands agree with the capabilities the vehicle has reported.
        VehicleCapabilityAgreement,
        /// Mission geometry against approved operating limits.
        GeometryLimits,
    };
    Q_ENUM(CheckId)

    /// What a check concluded. `NotPerformed` is zero so it is the default.
    enum Outcome {
        /// The check did not run. **Not a pass.** unavailableReason() says what was missing.
        NotPerformed = 0,
        /// The check ran and the mission satisfied it.
        Passed,
        /// The check ran and the mission did not satisfy it.
        Failed,
    };
    Q_ENUM(Outcome)

    /// Why a check could not run. Every value names a specific missing input or open decision, so
    /// a report says what would have to change for the check to become possible.
    enum Unavailable {
        /// The check ran; nothing was missing.
        NothingMissing = 0,
        /// No mission was captured. Absent is not empty, and an absent mission cannot be checked.
        MissionNotCaptured,
        /// A mission was captured and carries no items, so a per-item check has nothing to
        /// examine. Distinct from MissionNotCaptured, and reported rather than passed: a check
        /// that is vacuously true over an empty list did not establish anything.
        MissionEmpty,
        /// The mission carries items, and none of them is one this check applies to. Same
        /// reasoning as MissionEmpty: a pass over an empty selection asserts nothing.
        NoApplicableItems,
        /// No mission command set is declared. `Q-023` — the Aerora mission command set is
        /// `OPEN / NOT-EVIDENCED`, so there is nothing to check a command against.
        NoDeclaredMissionCommandSet,
        /// No capability has been reported for the vehicle. `Q3-009`.
        NoReportedCapability,
        /// No operating limit has been approved. Spacing, altitude, geofence and turn-radius
        /// limits are product decisions; a check against an invented number would be worse than
        /// no check, because it would produce confident verdicts from nothing.
        NoApprovedOperatingLimits,
        /// The caller supplied no resume index, so there is no resume point to analyse.
        NoResumeIndexSupplied,
    };
    Q_ENUM(Unavailable)

    AeroraMissionCheck() = default;

    [[nodiscard]] static AeroraMissionCheck passed(CheckId id, const QString &detail = QString());
    [[nodiscard]] static AeroraMissionCheck failed(CheckId id, const QString &detail);
    [[nodiscard]] static AeroraMissionCheck notPerformed(CheckId id,
                                                         Unavailable reason,
                                                         const QString &detail = QString());

    [[nodiscard]] CheckId id() const { return _id; }
    [[nodiscard]] Outcome outcome() const { return _outcome; }

    /// @return Why the check did not run, or Unavailable::NothingMissing when it did.
    [[nodiscard]] Unavailable unavailableReason() const { return _unavailableReason; }

    /// Machine-readable description, for logs and records. Not an operator-facing label — a
    /// consumer renders the id and the outcome, and may show this as supporting text.
    [[nodiscard]] QString detail() const { return _detail; }

    [[nodiscard]] bool isPerformed() const { return _outcome != NotPerformed; }
    [[nodiscard]] bool isPassed() const { return _outcome == Passed; }
    [[nodiscard]] bool isFailed() const { return _outcome == Failed; }

    [[nodiscard]] static QString checkIdToString(CheckId id);
    [[nodiscard]] static QString outcomeToString(Outcome outcome);
    [[nodiscard]] static QString unavailableToString(Unavailable reason);

private:
    CheckId _id = MissionPresence;
    Outcome _outcome = NotPerformed;
    Unavailable _unavailableReason = MissionNotCaptured;
    QString _detail;
};

Q_DECLARE_METATYPE(AeroraMissionCheck)

/// The result of a validation pass: every check that was named, what it concluded, and the
/// aggregate that refuses to hide a gap.
///
/// ### There is deliberately no bool
///
/// This class exposes no `isValid()`. The aggregate is verdict(), which has a distinct value for
/// "everything that ran passed, and some checks could not run". A boolean would have to fold that
/// into either `true` or `false`, and both are wrong: `true` hides the gap, `false` reports a
/// failure that nothing found. `P7-CHK-002` exists because that fold is the defect.
///
/// ### A check that could not run blocks nothing on its own
///
/// `P7-CHK-004`. `Verdict::PassedWithGaps` is not a failure, and kNotPerformedBlocksOperation
/// pins that as an assertable constant rather than a convention. What an operator does about a
/// gap is an operator decision, recorded elsewhere; this type's job is to make the gap
/// impossible to miss, not to make it blocking.
class AeroraMissionValidationReport
{
    Q_GADGET
    Q_PROPERTY(Verdict verdict READ verdict CONSTANT)
    Q_PROPERTY(int checkCount READ checkCount CONSTANT)
    Q_PROPERTY(int passedCount READ passedCount CONSTANT)
    Q_PROPERTY(int failedCount READ failedCount CONSTANT)
    Q_PROPERTY(int notPerformedCount READ notPerformedCount CONSTANT)
    Q_PROPERTY(bool fullyChecked READ isFullyChecked CONSTANT)

public:
    /// The aggregate. Ordered by severity of what a consumer must say about it, not by numeric
    /// value.
    enum Verdict {
        /// No check ran at all. The honest state for a mission nobody validated, and distinct
        /// from passing.
        NoChecksPerformed = 0,
        /// At least one check failed.
        Failed,
        /// Every check that ran passed, and at least one could not run. **Not a clean result.**
        PassedWithGaps,
        /// Every named check ran and passed.
        Passed,
    };
    Q_ENUM(Verdict)

    /// A check that could not be evaluated does not, by itself, block the operation it was
    /// checking (`P7-CHK-004`).
    ///
    /// Pinned as a constant so the rule is assertable rather than a reading of verdict(). If this
    /// ever needs to become true, that is a product decision about operator authority, not a
    /// refactor of this class.
    static constexpr bool kNotPerformedBlocksOperation = false;

    AeroraMissionValidationReport() = default;

    void append(const AeroraMissionCheck &check);

    [[nodiscard]] const QList<AeroraMissionCheck> &checks() const { return _checks; }

    [[nodiscard]] int checkCount() const { return static_cast<int>(_checks.count()); }
    [[nodiscard]] int passedCount() const;
    [[nodiscard]] int failedCount() const;
    [[nodiscard]] int notPerformedCount() const;

    /// @return Checks whose outcome is @a outcome, in the order they were appended.
    [[nodiscard]] QList<AeroraMissionCheck> checksWithOutcome(AeroraMissionCheck::Outcome outcome) const;

    /// @return true when at least one named check ran and every one of them ran. False for an
    ///         empty report, so "nothing was checked" is never fully checked.
    [[nodiscard]] bool isFullyChecked() const { return (checkCount() > 0) && (notPerformedCount() == 0); }

    [[nodiscard]] bool hasFailure() const { return failedCount() > 0; }

    [[nodiscard]] Verdict verdict() const;

    /// A one-line machine-readable summary of the four counts, for logs and records. Not an
    /// operator-facing label.
    [[nodiscard]] Q_INVOKABLE QString toString() const;

    [[nodiscard]] static QString verdictToString(Verdict verdict);

private:
    QList<AeroraMissionCheck> _checks;
};

Q_DECLARE_METATYPE(AeroraMissionValidationReport)

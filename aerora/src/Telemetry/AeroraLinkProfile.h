#pragma once

#include <optional>
#include <utility>

#include <QtCore/QHash>
#include <QtCore/QString>

/// \file AeroraLinkProfile.h
/// The recorded parameter set of a Link_Class, and the figures the station derives from it.
///
/// Requirement 5 criteria 5 to 9 of the aerora-fleet-and-authority specification. The arithmetic is
/// **not** here: `Q-015` has not settled which parameters a Link_Class records, so a formula
/// written now would be written against a parameter set that does not exist. What is here is the
/// shape around it, which is the half that is easy to get wrong -- an optional return, nothing
/// stored, and the profile revision carried with every figure.
///
/// This mirrors the derivation the register tooling already carries in Python
/// (`tools/common/gate_register.py`), deliberately. The two halves must agree about what an absent
/// value means, and the Python half is where that agreement is tested against the register itself.

/// \brief One figure derived from a Link_Profile, and the revision of the values it came from.
///
/// Requirement 5 criterion 8: the station reports, with each derived figure, the Link_Profile it
/// was derived from. The revision travels inside the figure rather than beside it, because the two
/// are read together and a value separated from its provenance is one a reader assumes is current.
struct AeroraDerivedLinkFigure {
    double value = 0.0;
    QString revision;
};

/// \brief Whether a recorded freshness threshold can be met on a Link_Class.
///
/// Requirement 7 criterion 8. A threshold shorter than the update interval the link can actually
/// sustain would mark every value it carries `STALE` -- not because anything is wrong, but because
/// the threshold was written for a faster link. An operator shown a permanent staleness indicator
/// learns to ignore the indicator, which costs the one case where it was telling the truth. So the
/// station reports the threshold as unachievable on that class instead.
enum class AeroraThresholdAchievability {
    /// Either the profile yields no interval or no threshold is recorded, so no comparison was
    /// made. Distinct from `Achievable`: nothing was compared, rather than compared and passed.
    NotDeterminable,
    Achievable,
    Unachievable,
};

/// \brief Whether a Link_Class can sustain a Telemetry_Tier.
///
/// Requirement 6 criterion 9 forbids offering a tier a class cannot sustain. Three states rather
/// than a boolean, because not knowing must not read as yes: the tier memberships are settled by
/// `DG-TLM` and the profile values by `Q-015`, so today every answer is `NotDeterminable`, and a
/// boolean would have had to be false -- which reads as *this link cannot do it* rather than as
/// *nobody has said yet*.
enum class AeroraTierSupport {
    NotDeterminable,
    Sustainable,
    NotSustainable,
};

/// \brief The recorded parameter set of one Link_Class.
///
/// Nothing is stored beyond what the register recorded. Requirement 5 criterion 7 re-derives every
/// dependent figure when a profile changes, and the cheapest way to be certain no stale figure
/// survives the change is to leave nowhere for one to live, so every figure below is a function of
/// the members and this type holds no cache.
class AeroraLinkProfile
{
public:
    AeroraLinkProfile() = default;

    /// \param linkClass  The Link_Class this profile describes. Opaque; this type interprets none.
    /// \param parameters The recorded parameters and their values. May be empty, which is the
    ///                   recorded state of a Link_Class whose values `Q-015` has not settled.
    /// \param heldBy     What settles the values, so an empty parameter set reads as a recorded
    ///                   open decision rather than as an omission.
    /// \param revision   Identifies which profile a derived figure came from.
    AeroraLinkProfile(QString linkClass, QHash<QString, double> parameters, QString heldBy,
                      QString revision)
        : _linkClass(std::move(linkClass))
        , _parameters(std::move(parameters))
        , _heldBy(std::move(heldBy))
        , _revision(std::move(revision))
    {
    }

    [[nodiscard]] QString linkClass() const { return _linkClass; }
    [[nodiscard]] QString heldBy() const { return _heldBy; }
    [[nodiscard]] QString revision() const { return _revision; }

    /// Whether the profile carries any parameter value at all.
    ///
    /// False for every profile until `Q-015` closes. A caller that finds this false reports unknown
    /// and never a default: Requirement 5 criterion 9 is explicit, and a zero reaching an operator
    /// as a measured zero is worse than a blank because it is actionable and wrong.
    [[nodiscard]] bool carriesValues() const { return !_parameters.isEmpty(); }

    /// The sustained throughput the profile yields, or nothing where it yields none.
    ///
    /// The arithmetic is held by `Q-015`, so this returns nothing whenever the profile carries no
    /// values. It does not return nothing *because* the arithmetic is unwritten -- it returns
    /// nothing because there is no input, which is the same answer the finished version gives for
    /// an empty profile and is why the absent-value path is testable now.
    [[nodiscard]] std::optional<AeroraDerivedLinkFigure> sustainedThroughput() const
    {
        return _derived();
    }

    /// The update interval the profile yields for \a tier, or nothing where it yields none.
    [[nodiscard]] std::optional<AeroraDerivedLinkFigure> achievableUpdateInterval(
        const QString &tier) const
    {
        Q_UNUSED(tier)
        return _derived();
    }

    /// The aircraft count this class sustains at \a tier, or nothing where it yields none.
    [[nodiscard]] std::optional<AeroraDerivedLinkFigure> aircraftSustainedAtTier(
        const QString &tier) const
    {
        Q_UNUSED(tier)
        return _derived();
    }

    /// Whether this class can sustain \a tier.
    ///
    /// `NotDeterminable` while the profile carries no values, and `NotDeterminable` even once it
    /// does until `DG-TLM` records the tier memberships: knowing the interval a link sustains says
    /// nothing about what a tier requires until somebody records the membership.
    [[nodiscard]] AeroraTierSupport tierSupport(const QString &tier) const
    {
        Q_UNUSED(tier)
        return AeroraTierSupport::NotDeterminable;
    }

private:
    /// Every figure shares this path while `Q-015` is open, so the absent-value semantics are in
    /// one place rather than repeated per figure and able to drift between them.
    [[nodiscard]] std::optional<AeroraDerivedLinkFigure> _derived() const
    {
        return std::nullopt;
    }

    QString _linkClass;
    QHash<QString, double> _parameters;
    QString _heldBy;
    QString _revision;
};

/// Whether \a thresholdMs can be met on a link whose yielded update interval is \a intervalMs.
///
/// Requirement 7 criterion 8. Pure logic over two optionals, and fully implementable today even
/// though both inputs are absent: a threshold shorter than the interval is unachievable, and
/// either input missing means nothing was compared.
///
/// The boundary is achievable. A threshold equal to the interval is met exactly, and treating it as
/// unachievable would reject a threshold a reviewer deliberately set to the link rate.
[[nodiscard]] inline AeroraThresholdAchievability thresholdAchievability(
    std::optional<double> intervalMs, std::optional<double> thresholdMs)
{
    if (!intervalMs.has_value() || !thresholdMs.has_value()) {
        return AeroraThresholdAchievability::NotDeterminable;
    }
    return thresholdMs.value() < intervalMs.value() ? AeroraThresholdAchievability::Unachievable
                                                    : AeroraThresholdAchievability::Achievable;
}

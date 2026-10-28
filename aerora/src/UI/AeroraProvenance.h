#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtQmlIntegration/QtQmlIntegration>

/// \brief What this build of the station actually is, as a read-only projection for the interface.
///
/// This closes `P7-HELP-004` as far as the repository permits, and says so where it does not. The
/// requirement asks the station to state "base commit, overlay commit, profile set, enforcement
/// state". Three of those four are answerable; one is not, and the difference is reported rather
/// than papered over - see \ref upstream.
///
/// \section why Why a projection, and why it lives with the UI
///
/// Nothing in the overlay needed build provenance until a screen had to display it, and nothing
/// else consumes it now. Rather than add a build-facts module for one type, it sits beside the
/// other read-only QML projections (`AeroraVehicleUiContext`, `AeroraSecurityStatus`) and follows
/// their shape exactly: every property `READ`-only, no `Q_INVOKABLE`, no public slot, and **no data
/// member**. The values are preprocessor constants from a generated header, so there is nothing to
/// store and nothing that could go stale relative to the binary.
///
/// \section upstream The one field this cannot answer
///
/// `P7-HELP-004` distinguishes a *base* commit from an *overlay* commit. In this repository they
/// are the same commit: the overlay lives in the same git tree as the fork of QGroundControl it
/// extends, so one hash describes both. What is genuinely absent is the **upstream** commit the
/// fork diverged from - no upstream remote, merge-base or vendored revision is recorded anywhere in
/// the build, so there is nothing to read.
///
/// `upstreamBaseCommitKnown` is therefore a compile-time `false`, in the same spirit as
/// `AeroraSecurityStatus::kEnforcementVerified`: a prohibition expressed as a value a binding can
/// read, rather than as prose a screen author has to remember. A consumer renders *not recorded*
/// and cannot accidentally present the fork commit as the upstream one.
///
/// \section dirty Why a dirty working tree is reported
///
/// A build made from uncommitted changes is not described by its commit hash, and a provenance
/// screen that showed only the hash would be wrong in exactly the case where provenance matters -
/// an engineering build fielded by accident. `workingTreeDirty` is the qualifier that makes the
/// hash honest.
class AeroraProvenance : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(AeroraProvenance)
    QML_SINGLETON

    Q_PROPERTY(QString commit READ commit CONSTANT)
    Q_PROPERTY(bool commitKnown READ isCommitKnown CONSTANT)
    Q_PROPERTY(QString branch READ branch CONSTANT)
    Q_PROPERTY(bool workingTreeDirty READ isWorkingTreeDirty CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool upstreamBaseCommitKnown READ isUpstreamBaseCommitKnown CONSTANT)

public:
    explicit AeroraProvenance(QObject* parent = nullptr);
    ~AeroraProvenance() override;

    /// @return The short commit hash, or an empty string when the build was not made from a
    ///         checkout.
    ///
    /// Empty rather than the generator's `"0000000"` sentinel: a consumer that rendered the
    /// sentinel would be displaying something that looks like a commit and is not one.
    [[nodiscard]] QString commit() const;

    /// @return true when commit() carries a real hash. The gate every consumer should read before
    ///         presenting a commit.
    [[nodiscard]] bool isCommitKnown() const;

    /// @return The branch at configure time, or an empty string when unknown. Empty for the same
    ///         reason as commit().
    [[nodiscard]] QString branch() const;

    /// @return true when the working tree had uncommitted changes at configure time, so the commit
    ///         does not fully describe this binary. See \ref dirty.
    [[nodiscard]] bool isWorkingTreeDirty() const;

    /// @return The `git describe` version string. Always present: the generator substitutes a
    ///         fallback outside a checkout.
    [[nodiscard]] QString version() const;

    /// Whether the upstream QGroundControl commit this fork diverged from is recorded.
    ///
    /// Compile-time false. Nothing in the build records it, so a consumer must say *not recorded*
    /// rather than offering the fork's own commit in its place. See \ref upstream.
    [[nodiscard]] bool isUpstreamBaseCommitKnown() const { return kUpstreamBaseCommitKnown; }

    /// Set to false and asserted by test. See isUpstreamBaseCommitKnown().
    static constexpr bool kUpstreamBaseCommitKnown = false;

    /// The value the version generator substitutes for a commit when there is no checkout
    /// (`cmake/modules/Git.cmake:13`). Named so the comparison in commit() is not a bare literal.
    static constexpr const char* kAbsentCommitSentinel = "0000000";

    /// The value the version generator substitutes for a branch when there is no checkout.
    static constexpr const char* kAbsentBranchSentinel = "unknown";
};

#pragma once

#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QString>

#include "AeroraAuthorization.h"

/// \brief An \c AeroraAuthorizationPolicy that decides one outbound command class against a
///        role-to-class table supplied as configuration.
///
/// \section why Why this exists now
///
/// WP2-C prohibited implementing a policy, on the ground that "a policy over an unapproved role
/// set is an invented policy" while `Q-029` was open. `DG-ROL` is now `CLOSED`: the role set and
/// the command classes are settled and signed, and `DG-ROL:authorization-scope` selects ADR-014
/// Option C, a distinct seam per outbound class gated where that class converges. The
/// prohibition's premise is gone, so a policy over the approved set invents nothing.
///
/// \section table Why the table is configuration rather than code
///
/// No role name and no class name appears in this file. `AeroraRoleAssignment` records the reason:
/// a role name compiled in here could not be changed without a release, and the signed role set is
/// deployment configuration rather than a constant. This type holds opaque role identifiers mapped
/// to opaque class identifiers and compares strings it never interprets.
///
/// \section seam One instance per seam
///
/// Option C gates each class at its own convergence point, so an instance carries the single class
/// the seam it serves conveys and is told which rather than deducing it.
///
/// \section subject Why the subject is not inspected
///
/// \c evaluate() ignores the operation. Every operation reaching one seam carries that seam's class
/// by construction, and no accepted record distinguishes one MAV_CMD from another *within* a class.
/// A verdict that varied with \c mavCommand or \c param1 would therefore be deciding something no
/// owner has decided, which is what `P7-NOINV-001` prohibits. A per-command distinction belongs to
/// a later decision and to a different type.
///
/// \section closed Fail-closed
///
/// Every answer other than an explicit grant is a denial: an absent table, an unresolved
/// assignment, an empty assignment, a held role the table does not carry, and a held role carrying
/// other classes but not this one. \c AeroraCommandAuthorizer already denies when no policy is
/// installed; this type does not rely on that and denies on its own.
class AeroraRolePolicy : public AeroraAuthorizationPolicy
{
public:
    /// Opaque role identifier to the opaque class identifiers that role may authorise.
    using RoleGrants = QHash<QString, QSet<QString>>;

    AeroraRolePolicy(QString gatedClass, RoleGrants grants);

    /// True only with a non-empty gated class and a non-empty table.
    ///
    /// An instance carrying either empty denies everything, and reporting it configured would make
    /// \c AeroraSecurityStatus report enforcement for a station that grants nothing - which is the
    /// "a configured policy is not observed enforcement" distinction that class exists to keep.
    [[nodiscard]] bool isConfigured() const override;

    [[nodiscard]] AeroraAuthorizationDecision evaluate(const AeroraRoleAssignment& roles,
                                                       const AeroraGatedOperation& operation) const override;

    /// The one class this instance gates.
    [[nodiscard]] QString gatedClass() const { return _gatedClass; }

    /// How many roles the table carries. For a status projection, never for a decision.
    [[nodiscard]] qsizetype roleCount() const { return _grants.size(); }

private:
    QString _gatedClass;
    RoleGrants _grants;
};

#pragma once

#include <QtCore/QString>

/// \brief The operation-entry gate: decides when a stale operator credential blocks a command, and
///        tracks whether an operation is in force.
///
/// \section what The one thing it adds
///
/// `AeroraCommandAuthorizer` answers "may this operator's role do this command." This gate answers
/// a separate question layered over it: "is the operator's cloud credential fresh enough for what
/// this command does." The two are deliberately distinct. Role authorization applies to every
/// command always; credential freshness is checked only where the owner placed it.
///
/// \section rules The owner's answers, as a state machine
///
/// Four owner answers fix the behaviour (product-completion 15 § 15):
///
/// \li **A1 - entry is arming.** The armed period is "the operation." Entering it is where the
///     credential is checked for freshness.
/// \li **A2 - expiry does not end an operation.** While in operation, no command is blocked for a
///     stale credential - the operator keeps the full authority their role grants, including the
///     command to land. Only a *new* entry re-checks freshness.
/// \li **A3 - disarm ends the operation,** so the next entry re-checks the credential.
/// \li **B (answer 2 earlier) - the check sits at arming,** not at every ground command. A command
///     outside an operation that is not an entry - configuring, uploading a plan on the ground - is
///     role-gated but not freshness-gated, which keeps `AERORA-AWS-REQ-006` ground work possible
///     for an operator whose credential has just lapsed, while still refusing them the arm.
///
/// So freshness is consulted on exactly one transition: entering an operation from outside one.
/// Every other command - within an operation, a disarm, a pre-arm ground command, even an arm sent
/// while already armed - is never freshness-blocked. That single rule is the whole safety content,
/// and stating it as one condition is what makes the dangerous case - a continuation command
/// blocked mid-flight - unreachable by construction rather than avoided by remembering to.
///
/// \section notrole What it does not do
///
/// It does not authorize. A gate decision of "not blocked" means freshness does not stand in the
/// way; the command still faces role authorization in `AeroraCommandAuthorizer`, which can refuse
/// it. A gate that allowed would be a second thing that can permit a command, and there is exactly
/// one of those. The gate only ever *adds* a refusal reason, never removes one.
///
/// \section nomavlink Why the caller classifies the command
///
/// The gate is told `entersOperation` / `exitsOperation` rather than a `mavCommand`, because
/// recognising an arm is reading the standard dialect's arm-disarm command and its
/// `param1`, and the overlay keeps MAVLink identifiers at the call site rather than in the
/// authorization types - `AeroraCommandSubject` carries `mavCommand` as an int that no authorization
/// code interprets. The classification lives where the raw command already is.
///
/// \section notinstalled Not wired, and the shipped posture is unchanged
///
/// Nothing constructs or consults this gate yet. Installing it is part of making
/// `isAuthorizationEnforced()` true, which stays false in every shipped build until a deployment
/// installs the authorization chain deliberately. This type is the decision logic; the wiring into
/// `authorizeMavCommand`, and the freshness source it reads, are a separate step.
class AeroraOperationGate
{
public:
    /// What the gate decided about one command.
    struct Decision
    {
        /// True when the command is refused because the credential is not fresh at an entry. The
        /// only thing the gate blocks; everything else carries false.
        bool credentialBlocks = false;

        /// The operation phase after this command, if it proceeds. The caller commits it with
        /// `applyAuthorized()` only once the command has also passed role authorization and been
        /// sent, so the phase tracks what actually happened rather than what was attempted.
        bool nextInOperation = false;

        /// The audit cause when blocked, empty otherwise. Never a role name, matching the
        /// authorization decision's own rule for its reason code.
        QString reasonCode;
    };

    /// Whether an operation is currently in force on this station's view - entered by an authorized
    /// arm and not yet ended by a disarm.
    [[nodiscard]] bool inOperation() const { return _inOperation; }

    /// Decide one command. Pure: it reads the phase and returns a decision without changing
    /// anything, so a caller can evaluate before it knows whether the command will also pass role
    /// authorization.
    ///
    /// \a entersOperation and \a exitsOperation classify the command - an arm enters, a disarm
    /// exits, and a command that is neither carries both false. Both true is a caller error (a
    /// command cannot arm and disarm at once); entry takes precedence if it ever occurs.
    /// \a credentialFreshNow is the freshness verdict at this instant, consulted only at an entry.
    [[nodiscard]] Decision evaluate(bool entersOperation, bool exitsOperation, bool credentialFreshNow) const;

    /// Commit a decision's phase. Called only for a command that was authorized and sent, so a
    /// refused or dropped arm does not leave the gate believing an operation began.
    void applyAuthorized(const Decision& decision) { _inOperation = decision.nextInOperation; }

    /// Return to not-in-operation. The gate's owner calls this when the operation context is torn
    /// down - a vehicle disconnect - so a later arm re-enters through a fresh credential check
    /// rather than inheriting a phase from a connection that is gone.
    void reset() { _inOperation = false; }

private:
    bool _inOperation = false;
};

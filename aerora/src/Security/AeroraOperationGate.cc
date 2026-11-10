#include "AeroraOperationGate.h"

AeroraOperationGate::Decision AeroraOperationGate::evaluate(bool entersOperation, bool exitsOperation,
                                                            bool credentialFreshNow) const
{
    Decision decision;

    // Freshness is consulted on exactly one transition: entering an operation from outside one.
    // Writing it as this single condition is what makes a continuation command unreachable by the
    // block - while in operation, `!_inOperation` is false, so no command is ever freshness-blocked,
    // which is the owner's A2 rule that expiry does not end an operation.
    const bool consultFreshness = entersOperation && !_inOperation;

    if (consultFreshness && !credentialFreshNow) {
        decision.credentialBlocks = true;
        decision.nextInOperation = _inOperation;  // unchanged: a blocked arm does not enter
        decision.reasonCode = QStringLiteral("credential.not.fresh.at.entry");
        return decision;
    }

    // Not blocked. The resulting phase follows the boundary: an arm enters, a disarm exits, and a
    // command that is neither leaves the phase as it was. Entry is tested first so that a
    // nonsensical both-true classification resolves to entering rather than to an ambiguous state.
    decision.credentialBlocks = false;
    if (entersOperation) {
        decision.nextInOperation = true;
    } else if (exitsOperation) {
        decision.nextInOperation = false;
    } else {
        decision.nextInOperation = _inOperation;
    }
    return decision;
}

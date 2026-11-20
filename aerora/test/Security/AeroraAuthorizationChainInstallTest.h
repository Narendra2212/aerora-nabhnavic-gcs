#pragma once

#include "UnitTest.h"

/// \brief Asserts `AeroraCorePlugin::installAuthorizationChainInto()`: that a credential the
/// configured issuer signed installs the whole authorization chain, and that every other input
/// installs nothing at all.
///
/// \section why What carries the weight
///
/// This is the step between a verified credential and an enforcing command gate, and until this
/// suite existed it had no test of its own — the unit harness never calls `init()`, so the member
/// that used to hold this logic could not be reached. `AeroraCredentialDocumentTest` establishes
/// that a document verifies; nothing established that verifying one *installs* anything, or that a
/// refused one leaves the station unenforcing.
///
/// \li **A verified credential installs all three collaborators**, asserted through
///     `isFullyConfigured()` — the predicate the command path consults, and the one thing that
///     turns the gate on.
/// \li **Every refusal leaves the authorizer unconfigured.** No issuer key, an unusable key, an
///     empty path, an absent file, an unreadable file, a forged credential and a malformed one all
///     reach the same state, which is what "fail closed" has to mean here. Nothing is partially
///     applied, so no refusal can leave an identity installed without its roles.
/// \li **No issuer key is the shipped default**, and it must install nothing even when a perfectly
///     good credential is sitting at the path. That is the posture of every build nobody
///     provisioned.
/// \li **An already-installed chain is not replaced**, so a second `init()` in a UI-test process
///     cannot swap the operator — and a second credential appearing at the path cannot either.
/// \li **Presence and freshness stay separate across the install.** An expired credential installs
///     and reports not fresh, rather than failing to install: the operator stays identified after
///     their window closes, which is what lets a command already in flight continue while a new
///     operation entry is refused.
///
/// Credentials are signed with the linked library from a fixed seed, so the suite exercises the
/// real verification path end to end and a failure is reproducible. The issuer fixture is local to
/// this file rather than shared with `AeroraCredentialDocumentTest`, so neither suite's fixture can
/// break the other. No key material is shared with anything that ships.
class AeroraAuthorizationChainInstallTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _aVerifiedCredentialInstallsTheWholeChain();
    void _anExpiredCredentialInstallsAndReportsNotFresh();
    void _anAlreadyInstalledChainIsNotReplaced();

    void _noIssuerKeyInstallsNothing();
    void _anUnusableIssuerKeyInstallsNothing();
    void _anEmptyCredentialPathInstallsNothing();
    void _aMissingCredentialFileInstallsNothing();
    void _anUnreadableCredentialIsRefused();
    void _aForgedCredentialIsRefused();
    void _aMalformedCredentialIsRefused();
};

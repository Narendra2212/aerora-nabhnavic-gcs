#pragma once

#include "UnitTest.h"

/// \brief Asserts the QML-safe enforcement read path (WP2-B, `D-UI-01`): what it reports, and that
/// it cannot be used to change anything.
///
/// Two groups, and the second is the one that matters over time.
///
/// \section reports What it reports
///
/// `enforcement` must distinguish "a decision point exists but no policy is installed" from "a
/// policy is installed and evaluating", and must report the first for the shipped station
/// (`P7-AUTH-002`, `P7-SEC-005`). The audit properties must report a sink's absence and its
/// self-declared lack of durability without either being inferred from the other
/// (`P7-AUDIT-002`).
///
/// \section ratchet Why the meta-object is read here too
///
/// `AeroraUiSafetyTest` is the established ratchet over the QML-reachable Aerora surface: it reads
/// meta-objects because the meta-object is exactly what QML sees, and because acquiring
/// `Q_INVOKABLE`, a slot or a `WRITE` accessor is a one-line change that produces no compiler
/// warning and no visible UI diff.
///
/// `AeroraSecurityStatus` is a new QML-reachable type, so it needs that same guard. The guard is
/// duplicated here rather than added to `AeroraUiSafetyTest::auditedTypes()` because that file
/// belongs to another workstream and already carries one sanctioned cross-boundary edit this phase
/// (Tab 4's `U3`); adding this type to its audited set is filed as a handoff instead, and its
/// assertions are not modified. See
/// `.kiro/specs/03-implementation-specification/34-audit-sink-and-security-status-contract.md`.
///
/// The assertions below are stricter than that suite's, because this type has a stricter contract:
/// its entire QML-callable surface must be property getters, and it must declare no data member at
/// all - a member would mean the operator interface had acquired a cached copy of security state.
class AeroraSecurityStatusTest : public UnitTest
{
    Q_OBJECT

protected slots:
    void cleanup() override;

private slots:
    // What the interface reports.
    void _unconfiguredStationReportsNotEnforced();
    void _configuredChainReportsEnforcedAtTheCommandGate();
    void _enforcementIsNeverReportedAsVerified();
    void _auditPropertiesReportAbsenceAndNonDurabilitySeparately();
    void _auditRecordsLostIsVisibleAndMonotonic();
    void _statusChangedIsEmittedOnConfigurationAndOnLoss();

    // The QML-safety ratchet over this type.
    void _noPropertyIsQmlWritable();
    void _theTypeDeclaresNoQmlCallableMethod();
    void _noSecurityTypeIsReachableThroughAProperty();
    void _forbiddenSecurityNamesAreNotQmlReachable();
};

#pragma once

#include "UnitTest.h"

/// \brief Verifies that the dual-power configuration reader reports the aircraft's own parameters,
/// distinguishes unknown from false, and refuses every inference it could be tempted into.
///
/// Design record: 05-product-completion/16-dual-power-tether-and-battery-contract.md.
///
/// \section seams Why most of this needs no vehicle
///
/// The two decisions worth asserting - the interlock verdict and whether the safe-landing reserve's
/// capacity is unknown - are exposed as static pure functions, following the pattern established by
/// `boardUidFromAutopilotVersion()` and `reportFleetIdentityFrom()`. So the truth table is
/// exercised directly instead of through a `Vehicle`, a link and a loaded parameter set, and a
/// regression in the verdict is attributed to the verdict rather than to the harness.
///
/// The no-vehicle path is not merely a test convenience either: it is the shipped behaviour for any
/// aircraft that is not this firmware, and it must read as unknown rather than as battery power
/// with a consistent interlock.
///
/// \section prohibitions What the prohibition tests catch
///
/// Three named regressions. A future change that introduces a power threshold the station would
/// then be enforcing without authority; one that infers the live source from the `VIN_RAW` bus
/// voltage, which cannot separate a 48 V converter output from a 14S pack; and one that gives this
/// class a parameter write path, turning a report of the interlock disagreement into a correction
/// of it.
class AeroraDualPowerConfigurationTest : public UnitTest
{
    Q_OBJECT

private slots:
    // The interlock truth table, over both parameters.
    void _interlockIsConsistentOnlyWhenBothParametersAgree_data();
    void _interlockIsConsistentOnlyWhenBothParametersAgree();

    // The reserve rule, including the fail-closed direction.
    void _reserveCapacityIsUnknownWhenTetheredOrUnread_data();
    void _reserveCapacityIsUnknownWhenTetheredOrUnread();

    // Absence must not read as false.
    void _noVehicleLeavesEveryReadingUnknown();
    void _noVehicleStillReportsTheReserveAsUnknown();
    void _anUnknownInterlockIsNotReportedConsistent();

    // Prohibitions.
    void _prohibitionsAndThresholdsArePinned();

    // The parameter names are the firmware's, not this module's.
    void _parameterNamesMatchTheFirmwareTable();
};

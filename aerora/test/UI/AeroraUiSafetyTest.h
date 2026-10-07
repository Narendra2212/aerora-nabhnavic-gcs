#pragma once

#include "UnitTest.h"

/// \brief Turns the UI safety audit into a ratchet: enumerates the QML-reachable surface of every
/// Aerora type the operator interface can reach, and fails when it grows.
///
/// The interface is observation and supervision. It is not flight authority. That is asserted
/// everywhere in prose, and prose does not fail a build — so this test reads the meta-objects
/// instead, because the meta-object is exactly what QML sees.
///
/// \section why Why meta-object introspection rather than inspection
///
/// A method becomes QML-callable by acquiring `Q_INVOKABLE` or by being a slot, and a property
/// becomes QML-writable by acquiring a `WRITE` accessor. Each is a one-line change in a header
/// owned by another workstream, neither produces a compiler warning, and neither shows up in a UI
/// diff. Reviewing for them works until the once someone forgets.
///
/// So the reviewed surface is written down here as data. Adding an actuating path to a type the
/// interface can reach fails this test, in the UI workstream, naming the type and the method.
///
/// \section scope What is covered
///
/// The reachability closure from the `AeroraUi` QML singleton, plus the two state-binding types the
/// Aerora controls consume. Ten types. Signals are excluded: emitting one from QML re-evaluates
/// bindings and actuates nothing, whereas `Q_INVOKABLE` methods and slots are where actuation would
/// live.
///
/// \section known The one recorded deviation
///
/// `AeroraStateValue::capability` carries `WRITE setCapability`, so QML can set it. Capability
/// absence is the only input that produces `UNAVAILABLE`, so a QML write could manufacture an
/// `UNAVAILABLE` the vehicle never reported — the derivation `AERORA-GCS-REQ-029` prohibits. It is
/// an M-Telemetry property and is filed as handoff HO-TAB7-01 rather than worked around here. The
/// test asserts the deviation set is **exactly** that one entry, so a second writable property
/// cannot appear quietly while the first is outstanding.
class AeroraUiSafetyTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _everyQmlReachableMethodIsOnTheReviewedList_data();
    void _everyQmlReachableMethodIsOnTheReviewedList();

    void _onlyTheRecordedPropertyDeviationIsWritable();

    void _forbiddenMutatorNamesAreNotQmlReachable_data();
    void _forbiddenMutatorNamesAreNotQmlReachable();
};

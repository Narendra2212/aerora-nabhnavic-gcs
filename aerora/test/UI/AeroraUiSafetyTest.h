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
/// All twelve Aerora types that carry a QML registration macro. Signals are excluded: emitting one
/// from QML re-evaluates bindings and actuates nothing, whereas `Q_INVOKABLE` methods and slots are
/// where actuation would live.
///
/// Ten of the twelve were covered until `HO-TAB1-03`. `AeroraFreshness` had been registered and
/// unaudited since Phase 6 and `AeroraSecurityStatus` arrived with `WP2-B`, so the ratchet was
/// guarding most of the surface while reading as though it guarded all of it. Neither turned out to
/// expose anything — but "no defect found" is the result of the audit, not a reason to have skipped
/// it.
///
/// \section derivation How much of the audit is derived
///
/// A hand-maintained audit list fails the same way as the surface it guards, so two of the three
/// ways the list can be wrong are now mechanical:
///
/// \li `_everyAuditedTypeIsQmlRegistered` reads the `QML.Element` class info the registration
///     macros emit, so an entry that is not really QML-visible fails.
/// \li `_theQmlReachableClosureIsFullyAudited` walks properties and invokable return types out from
///     the `AeroraUi` singleton, so a new property handing QML a new Aerora object fails without
///     anyone remembering to extend the list.
///
/// What is **not** derived: a registered type that is unreachable from the root. Qt publishes no
/// API for enumerating the types registered into a QML module, and the alternative is
/// `QQmlMetaType`, which is private. That residue is precisely the shape of the two types above,
/// and `D-UI-07` records the two candidate closures and what each costs.
///
/// \section known Recorded deviations: none
///
/// `AeroraStateValue::capability` was the only one. It carried `WRITE setCapability`, so QML could
/// set the capability classification, and capability absence is the only input that produces
/// `UNAVAILABLE` — a QML write could have manufactured an `UNAVAILABLE` the vehicle never reported,
/// the derivation `AERORA-GCS-REQ-029` prohibits. Closed by U3 (handoff HO-TAB7-01): the `WRITE` is
/// gone and `setCapability()` remains public C++ for the capability model.
///
/// `recordedWritablePropertyDeviations()` is now empty, and the assertion is set equality, so the
/// suite requires **zero** writable properties across all twelve types — stricter than when it
/// permitted one. `_writablePropertyDetectionFindsAKnownWritableProperty` keeps that from passing
/// vacuously by pointing the same detector at `Fact`, which genuinely has writable properties.
class AeroraUiSafetyTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _everyQmlReachableMethodIsOnTheReviewedList_data();
    void _everyQmlReachableMethodIsOnTheReviewedList();

    void _onlyTheRecordedPropertyDeviationIsWritable();
    void _writablePropertyDetectionFindsAKnownWritableProperty();

    void _everyAuditedTypeIsQmlRegistered_data();
    void _everyAuditedTypeIsQmlRegistered();

    void _theQmlReachableClosureIsFullyAudited();

    void _forbiddenMutatorNamesAreNotQmlReachable_data();
    void _forbiddenMutatorNamesAreNotQmlReachable();
};

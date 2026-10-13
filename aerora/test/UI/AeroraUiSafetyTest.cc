#include "AeroraUiSafetyTest.h"

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaClassInfo>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QSet>
#include <QtCore/QStringList>

#include "AeroraFreshness.h"
#include "AeroraMissionObserver.h"
#include "AeroraPayloadModel.h"
#include "AeroraPowerModel.h"
#include "AeroraReportedValue.h"
#include "AeroraSecurityStatus.h"
#include "AeroraStateValue.h"
#include "AeroraTelemetryState.h"
#include "AeroraVehicleCapabilities.h"
#include "AeroraVehicleIdentity.h"
#include "AeroraVehicleServices.h"
#include "AeroraVehicleUiContext.h"
#include "Fact.h"

namespace {

/// Every Aerora type that carries a QML registration macro, which is the whole surface a QML
/// engine can reach.
///
/// \section derived What is checked mechanically, and what this list still carries
///
/// Two of the three ways this list could be wrong are now checked rather than reviewed:
///
/// \li an entry that is **not** actually QML-registered fails
///   `_everyAuditedTypeIsQmlRegistered`, which reads the `QML.Element` class info the
///   `QML_ELEMENT` / `QML_NAMED_ELEMENT` macros emit;
/// \li a type reachable from the `AeroraUi` root that is **missing** from this list fails
///   `_theQmlReachableClosureIsFullyAudited`, which walks properties and invokable return types
///   from the root singleton instead of trusting anyone to have added it here.
///
/// The third way remains open and is the reason this is still a list. A QML-registered type that
/// is **not** reachable from the root — an independent singleton, or a type only handed out by
/// C++ — cannot be discovered at runtime, because Qt exposes no public API for enumerating the
/// types registered into a QML module. `AeroraFreshness` and `AeroraSecurityStatus` are exactly
/// that shape and were both missing until `HO-TAB1-03`. `D-UI-07` records the two ways to close
/// it, each with a cost: `QQmlMetaType` is private Qt API, and a generated list is a build-system
/// dependency.
const QList<const QMetaObject *> &auditedTypes()
{
    static const QList<const QMetaObject *> types = {
        &AeroraVehicleUiContext::staticMetaObject,
        &AeroraVehicleServices::staticMetaObject,
        &AeroraVehicleIdentity::staticMetaObject,
        &AeroraVehicleCapabilities::staticMetaObject,
        &AeroraPayloadModel::staticMetaObject,
        &AeroraPowerModel::staticMetaObject,
        &AeroraMissionObserver::staticMetaObject,
        &AeroraReportedValue::staticMetaObject,
        &AeroraStateValue::staticMetaObject,
        &AeroraTelemetryState::staticMetaObject,
        // Registered but not reachable from the root, so no walk finds them. Added under
        // HO-TAB1-03: AeroraFreshness has been registered since Phase 6 and AeroraSecurityStatus
        // arrived with WP2-B, and neither was audited.
        &AeroraFreshness::staticMetaObject,
        &AeroraSecurityStatus::staticMetaObject,
    };
    return types;
}

/// Whether @a meta carries the class info the QML registration macros emit.
///
/// `QML_ELEMENT` and `QML_NAMED_ELEMENT` both expand to a `Q_CLASSINFO("QML.Element", …)`, which
/// is what `qmltyperegistrar` itself reads to decide what to register. Asserting on it keeps this
/// suite to public meta-object API: the alternative is `QQmlMetaType`, which is private.
///
/// Read with `classInfoOffset()` so an inherited registration does not count as this type's own.
bool declaresQmlElement(const QMetaObject *meta)
{
    for (int i = meta->classInfoOffset(); i < meta->classInfoCount(); ++i) {
        if (qstrcmp(meta->classInfo(i).name(), "QML.Element") == 0) {
            return true;
        }
    }

    return false;
}

/// Names of the properties @a meta declares itself that QML could assign to.
///
/// Both `WRITE` and `MEMBER` produce a writable property, and `QMetaProperty::isWritable()` is
/// true for either, so asking the meta-object is what catches both without enumerating the forms.
QStringList writablePropertyNames(const QMetaObject *meta)
{
    QStringList names;

    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
        const QMetaProperty property = meta->property(i);
        if (property.isWritable()) {
            names.append(QString::fromUtf8(property.name()));
        }
    }

    return names;
}

/// The audited types keyed by class name, for resolving a declared type name back to a meta-object.
QHash<QString, const QMetaObject *> auditedTypesByName()
{
    QHash<QString, const QMetaObject *> byName;

    for (const QMetaObject *const meta : auditedTypes()) {
        byName.insert(QString::fromUtf8(meta->className()), meta);
    }

    return byName;
}

/// The class name a declared **pointer** type points to, or an empty string for anything else.
///
/// Only a pointer is a handle QML can navigate through to another object. A value type
/// (`AeroraTelemetrySource`) or a nested enumeration (`AeroraTelemetryState::Class`,
/// `AeroraVehicleServices::Classification`) is data: it exposes no further surface of its own, and
/// treating either as a reached type would report plain enum and struct properties as unaudited
/// objects.
QString pointedToTypeName(const char *declaredTypeName)
{
    QString name = QString::fromUtf8(declaredTypeName).simplified();
    if (!name.endsWith(QLatin1Char('*'))) {
        return QString();
    }

    name.chop(1);
    name.remove(QStringLiteral("const "));

    return name.trimmed();
}

/// Every Aerora type name reachable from @a root by following declared property types and
/// invokable return types, transitively. Includes @a root's own class name.
///
/// This is the derived half of the audit. A new `Q_PROPERTY` or `Q_INVOKABLE` handing QML a new
/// Aerora object puts that type's name in here with no list to update, which is the regression a
/// hand-maintained closure cannot catch.
///
/// Deliberately keyed on the **declared type name** rather than on
/// `QMetaProperty::metaType().metaObject()`. `AeroraVehicleUiContext` forward-declares its five
/// service types, so without a `Q_MOC_INCLUDE` the generated meta-type for a pointer to an
/// incomplete class can carry no meta-object, and a meta-type walk would quietly find nothing and
/// pass. The declared name is present in the meta-object either way.
///
/// Recursion is only possible into types this suite already knows, which is sound because the
/// assertion is that there is nothing reachable it does not know: an unaudited type is reported
/// rather than traversed.
///
/// Signals are not followed - a signal argument is not a handle QML navigates a type through.
QStringList reachableAeroraTypeNames(const QMetaObject *root)
{
    const QHash<QString, const QMetaObject *> byName = auditedTypesByName();

    QStringList found;
    QList<const QMetaObject *> pending = {root};
    QSet<QString> visited;

    const auto record = [&](const QString &typeName) {
        if (!typeName.startsWith(QStringLiteral("Aerora"))) {
            return;
        }
        if (!found.contains(typeName)) {
            found.append(typeName);
        }
        if (const QMetaObject *const reached = byName.value(typeName, nullptr)) {
            pending.append(reached);
        }
    };

    while (!pending.isEmpty()) {
        const QMetaObject *const meta = pending.takeFirst();
        const QString className = QString::fromUtf8(meta->className());
        if (visited.contains(className)) {
            continue;
        }
        visited.insert(className);
        record(className);

        for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
            record(pointedToTypeName(meta->property(i).typeName()));
        }

        for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
            const QMetaMethod method = meta->method(i);
            if (method.methodType() == QMetaMethod::Signal) {
                continue;
            }
            record(pointedToTypeName(method.typeName()));
        }
    }

    found.sort();

    return found;
}

/// Names of every non-signal method each audited type declares, derived by reading the headers.
///
/// A name here means it was reviewed and is safe for QML to call: a read accessor, a pure
/// predicate, or an internal slot that recomputes a gate from a vehicle report. Nothing here
/// reaches a command, a link, a Fact write or an actuator.
QStringList reviewedMethodsFor(const QString &typeName)
{
    static const QHash<QString, QStringList> reviewed = {
        // Read-only facade. The slot rebinds to the active vehicle and emits; it actuates nothing.
        {QStringLiteral("AeroraVehicleUiContext"), {QStringLiteral("_onActiveVehicleChanged")}},
        // Recomputes classification from the capability inventory.
        {QStringLiteral("AeroraVehicleServices"), {QStringLiteral("_onCapabilityReportChanged")}},
        {QStringLiteral("AeroraVehicleIdentity"), {}},
        // Three pure inventory reads.
        {QStringLiteral("AeroraVehicleCapabilities"),
         {QStringLiteral("state"), QStringLiteral("isAvailable"), QStringLiteral("reportedKeys")}},
        // reportedValue() is a lookup. submitRequest() is deliberately absent - IF-PAY-015.
        {QStringLiteral("AeroraPayloadModel"),
         {QStringLiteral("reportedValue"), QStringLiteral("_onCapabilityReportsChanged")}},
        // configuration() and reportedValue() are lookups. There is no outbound method at all.
        {QStringLiteral("AeroraPowerModel"),
         {QStringLiteral("configuration"), QStringLiteral("reportedValue"),
          QStringLiteral("_onCapabilityReportsChanged")}},
        // verify() compares two local snapshots and sends nothing. There is no command API.
        {QStringLiteral("AeroraMissionObserver"), {QStringLiteral("verify")}},
        {QStringLiteral("AeroraReportedValue"), {QStringLiteral("ageMs")}},
        {QStringLiteral("AeroraStateValue"), {QStringLiteral("ageMs")}},
        // Eight pure functions over two enumerations.
        {QStringLiteral("AeroraTelemetryState"),
         {QStringLiteral("resolve"), QStringLiteral("isAuthoritative"), QStringLiteral("isCurrent"),
          QStringLiteral("isSubordinate"), QStringLiteral("carriesValue"), QStringLiteral("isNonValue"),
          QStringLiteral("classToString"), QStringLiteral("kindToString")}},
        // Nothing. setThreshold()/clearThreshold() are plain C++ and are on the forbidden list:
        // a QML-set threshold would be a station-side value acquiring safety meaning, which is
        // what Q-036 keeps open rather than defaulted.
        {QStringLiteral("AeroraFreshness"), {}},
        // Nothing. Every property is a computed read and the type holds no security state, so its
        // whole QML surface is property getters and one signal. See AeroraSecurityStatus.h.
        {QStringLiteral("AeroraSecurityStatus"), {}},
    };

    return reviewed.value(typeName);
}

/// The reviewed QML-writable properties across every audited type. Empty, and read as a result
/// rather than as an omission.
///
/// `AeroraStateValue.capability` was the one entry, recorded as HO-TAB7-01. U3 dropped its
/// `WRITE setCapability`, so the corrected property surface has no writable member on any audited
/// type and the assertion below is correspondingly stricter: it now fails on the first writable
/// property to appear, with no sanctioned exception to hide behind.
QStringList recordedWritablePropertyDeviations()
{
    return {};
}

/// Names that must never become QML-reachable on an audited type.
///
/// Each is a mutator or an outbound path that exists in C++ because some component legitimately
/// needs it, and that the operator interface must not be able to call. Grouped by why.
QStringList forbiddenNames()
{
    return {
        // Payload actuation - IF-PAY-015 is "None, prohibited".
        QStringLiteral("submitRequest"),
        QStringLiteral("setCommandSink"),
        // Manufacturing an UNAVAILABLE or a report the vehicle never sent.
        QStringLiteral("setCapability"),
        QStringLiteral("recordReport"),
        QStringLiteral("recordObservation"),
        QStringLiteral("report"),
        QStringLiteral("reportFleetIdentity"),
        // Erasing or acknowledging vehicle-reported state from the interface.
        QStringLiteral("clearReport"),
        QStringLiteral("clearReports"),
        QStringLiteral("clearObservation"),
        QStringLiteral("clearFleetIdentity"),
        // Vehicle composition and identity classification.
        QStringLiteral("attachService"),
        QStringLiteral("resolveProductProfile"),
        QStringLiteral("reportAeroraCorroboration"),
        QStringLiteral("reportNotAerora"),
        // Mission observation inputs. The interface observes; the bridge reports.
        QStringLiteral("reportGcsPlan"),
        QStringLiteral("reportPlanViewSelection"),
        QStringLiteral("reportVehicleCurrentIndex"),
        QStringLiteral("reportResumeCandidate"),
        QStringLiteral("reportDownloadComplete"),
        QStringLiteral("reportUploadComplete"),
        QStringLiteral("reportRemoveAllComplete"),
        QStringLiteral("reportTransferStarted"),
        QStringLiteral("reportTransferEnded"),
        QStringLiteral("reportTransferError"),
        // Fact-group registration and the string-keyed service map.
        QStringLiteral("registerGroup"),
        QStringLiteral("registerFactGroup"),
        QStringLiteral("factGroups"),
        QStringLiteral("service"),
        QStringLiteral("serviceNames"),
        // Freshness thresholds. Q-036 is open and no value is configured anywhere; a threshold
        // the interface could set or clear would decide when a value reads STALE, which is a
        // staleness verdict chosen by a screen rather than evaluated against configuration.
        QStringLiteral("setThreshold"),
        QStringLiteral("clearThreshold"),
    };
}

/// Every non-signal method @a meta declares itself, by name. Inherited methods are excluded: they
/// belong to the base type's own audit, and QObject's are not Aerora's to review.
QStringList declaredNonSignalMethodNames(const QMetaObject *meta)
{
    QStringList names;

    for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
        const QMetaMethod method = meta->method(i);
        if (method.methodType() == QMetaMethod::Signal) {
            continue;
        }
        names.append(QString::fromUtf8(method.name()));
    }

    names.sort();
    names.removeDuplicates();

    return names;
}

} // namespace

void AeroraUiSafetyTest::_everyQmlReachableMethodIsOnTheReviewedList_data()
{
    QTest::addColumn<QString>("typeName");

    for (const QMetaObject *const meta : auditedTypes()) {
        const QString typeName = QString::fromUtf8(meta->className());
        QTest::newRow(qPrintable(typeName)) << typeName;
    }
}

void AeroraUiSafetyTest::_everyQmlReachableMethodIsOnTheReviewedList()
{
    QFETCH(QString, typeName);

    const QMetaObject *meta = nullptr;
    for (const QMetaObject *const candidate : auditedTypes()) {
        if (QString::fromUtf8(candidate->className()) == typeName) {
            meta = candidate;
            break;
        }
    }
    QVERIFY(meta);

    QStringList expected = reviewedMethodsFor(typeName);
    expected.sort();
    expected.removeDuplicates();

    const QStringList actual = declaredNonSignalMethodNames(meta);

    // A method appearing here that is not on the reviewed list is not necessarily unsafe - but it
    // has not been looked at from the interface's side, and the whole point of the ratchet is that
    // "not looked at" fails rather than passes.
    QVERIFY2(actual == expected,
             qPrintable(QStringLiteral("%1 QML-reachable methods changed.\n  expected: %2\n  actual:   %3\n"
                                       "Add the new method to reviewedMethodsFor() only after "
                                       "confirming the interface may call it.")
                            .arg(typeName, expected.join(QStringLiteral(", ")),
                                 actual.join(QStringLiteral(", ")))));
}

void AeroraUiSafetyTest::_onlyTheRecordedPropertyDeviationIsWritable()
{
    QStringList writable;

    for (const QMetaObject *const meta : auditedTypes()) {
        const QString typeName = QString::fromUtf8(meta->className());

        for (const QString &propertyName : writablePropertyNames(meta)) {
            writable.append(QStringLiteral("%1.%2").arg(typeName, propertyName));
        }
    }

    writable.sort();

    QStringList expected = recordedWritablePropertyDeviations();
    expected.sort();

    // Read-only is the interface's posture, and the exception list is the audit. A new writable
    // property on a type the interface can reach means QML gained the ability to change Aerora
    // state, which is a decision requiring an interface contract behind it.
    QVERIFY2(writable == expected,
             qPrintable(QStringLiteral("QML-writable Aerora properties changed.\n  expected: %1\n  actual:   %2\n"
                                       "A new writable property must be justified against an "
                                       "interface contract before it is added here.")
                            .arg(expected.join(QStringLiteral(", ")), writable.join(QStringLiteral(", ")))));
}

void AeroraUiSafetyTest::_writablePropertyDetectionFindsAKnownWritableProperty()
{
    // The deviation test above passes by finding nothing, and a detector that silently stopped
    // detecting would pass it just as well. So the detector is pointed at a type that genuinely
    // has writable properties and must find them.
    //
    // Fact is the honest choice rather than a fixture invented here: it is upstream
    // QGroundControl, it is how parameter editing works, and 04 records it as the noted
    // non-issue - AeroraStateValue::fact returns one. A fixture declaring its own writable
    // Q_PROPERTY would also put `Q_PROPERTY(... WRITE ...)` into aerora/ and break the
    // repo-wide grep used to confirm no Aerora type has one.
    const QStringList factWritable = writablePropertyNames(&Fact::staticMetaObject);

    QVERIFY2(!factWritable.isEmpty(), "writable-property detection found nothing on Fact");
    for (const char *expected : {"value", "rawValue"}) {
        QVERIFY2(factWritable.contains(QString::fromUtf8(expected)),
                 qPrintable(QStringLiteral("Fact.%1 is writable and was not detected")
                                .arg(QString::fromUtf8(expected))));
    }

    // And the detector must not simply answer "yes". A type whose properties are all READ-only
    // has to come back empty, or the deviation test could never fail in the other direction.
    QVERIFY2(writablePropertyNames(&AeroraSecurityStatus::staticMetaObject).isEmpty(),
             "AeroraSecurityStatus declares no writable property and must detect as such");
}

void AeroraUiSafetyTest::_everyAuditedTypeIsQmlRegistered_data()
{
    QTest::addColumn<QString>("typeName");

    for (const QMetaObject *const meta : auditedTypes()) {
        const QString typeName = QString::fromUtf8(meta->className());
        QTest::newRow(qPrintable(typeName)) << typeName;
    }
}

void AeroraUiSafetyTest::_everyAuditedTypeIsQmlRegistered()
{
    QFETCH(QString, typeName);

    const QMetaObject *meta = nullptr;
    for (const QMetaObject *const candidate : auditedTypes()) {
        if (QString::fromUtf8(candidate->className()) == typeName) {
            meta = candidate;
            break;
        }
    }
    QVERIFY(meta);

    // The audit is only meaningful over types QML can actually see. An entry that lost its
    // QML_ELEMENT - or was never registered - makes the suite look like it covers more surface
    // than it does, which is the failure mode this whole file exists to prevent, one level up.
    QVERIFY2(declaresQmlElement(meta),
             qPrintable(QStringLiteral("%1 is audited but declares no QML.Element class info. "
                                       "Either restore its QML registration or remove it from "
                                       "auditedTypes().")
                            .arg(typeName)));
}

void AeroraUiSafetyTest::_theQmlReachableClosureIsFullyAudited()
{
    const QStringList reachable = reachableAeroraTypeNames(&AeroraVehicleUiContext::staticMetaObject);
    const QHash<QString, const QMetaObject *> audited = auditedTypesByName();

    // Derived, not listed. Everything QML can navigate to from the AeroraUi singleton - through a
    // property or an invokable's return type, transitively - must be audited. Adding a property
    // that hands QML a new Aerora object therefore fails here immediately, rather than whenever
    // somebody next happens to re-read auditedTypes().
    QStringList unaudited;
    for (const QString &typeName : reachable) {
        if (!audited.contains(typeName)) {
            unaudited.append(typeName);
        }
    }

    QVERIFY2(unaudited.isEmpty(),
             qPrintable(QStringLiteral("QML-reachable Aerora types are not in auditedTypes(): %1\n"
                                       "Add each to auditedTypes() and to reviewedMethodsFor() "
                                       "after reviewing what the interface may call on it.")
                            .arg(unaudited.join(QStringLiteral(", ")))));

    // The walk must actually traverse, and these five are AeroraUi's service properties. A walk
    // that reaches none of them is broken rather than reporting a shrunken surface - and a broken
    // walk satisfies the assertion above for entirely the wrong reason.
    for (const char *expected : {"AeroraVehicleIdentity", "AeroraVehicleCapabilities",
                                 "AeroraPayloadModel", "AeroraPowerModel", "AeroraMissionObserver"}) {
        QVERIFY2(reachable.contains(QString::fromUtf8(expected)),
                 qPrintable(QStringLiteral("reachability walk did not reach %1 from AeroraUi; "
                                           "either the property was removed or the walk is broken")
                                .arg(QString::fromUtf8(expected))));
    }
}

void AeroraUiSafetyTest::_forbiddenMutatorNamesAreNotQmlReachable_data()
{
    QTest::addColumn<QString>("typeName");

    for (const QMetaObject *const meta : auditedTypes()) {
        const QString typeName = QString::fromUtf8(meta->className());
        QTest::newRow(qPrintable(typeName)) << typeName;
    }
}

void AeroraUiSafetyTest::_forbiddenMutatorNamesAreNotQmlReachable()
{
    QFETCH(QString, typeName);

    const QMetaObject *meta = nullptr;
    for (const QMetaObject *const candidate : auditedTypes()) {
        if (QString::fromUtf8(candidate->className()) == typeName) {
            meta = candidate;
            break;
        }
    }
    QVERIFY(meta);

    const QStringList methods = declaredNonSignalMethodNames(meta);

    // The complement of the reviewed-list test. That one catches anything new; this one names the
    // specific paths whose appearance would be a safety regression rather than an oversight, so a
    // failure says what went wrong instead of only that something changed.
    for (const QString &forbidden : forbiddenNames()) {
        QVERIFY2(!methods.contains(forbidden),
                 qPrintable(QStringLiteral("%1 exposes forbidden method %2 to QML")
                                .arg(typeName, forbidden)));
        QVERIFY2(meta->indexOfProperty(forbidden.toUtf8().constData()) == -1,
                 qPrintable(QStringLiteral("%1 exposes forbidden property %2 to QML")
                                .arg(typeName, forbidden)));
    }
}

UT_REGISTER_TEST(AeroraUiSafetyTest, TestLabel::Unit)

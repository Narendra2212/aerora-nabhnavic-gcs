#include "AeroraUiSafetyTest.h"

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QStringList>

#include "AeroraMissionObserver.h"
#include "AeroraPayloadModel.h"
#include "AeroraPowerModel.h"
#include "AeroraReportedValue.h"
#include "AeroraStateValue.h"
#include "AeroraTelemetryState.h"
#include "AeroraVehicleCapabilities.h"
#include "AeroraVehicleIdentity.h"
#include "AeroraVehicleServices.h"
#include "AeroraVehicleUiContext.h"

namespace {

/// The reachability closure from the AeroraUi QML singleton, plus the two state-binding types the
/// Aerora controls consume directly.
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
    };
    return types;
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
    };

    return reviewed.value(typeName);
}

/// The one QML-writable property across every audited type. See HO-TAB7-01.
QStringList recordedWritablePropertyDeviations()
{
    return {QStringLiteral("AeroraStateValue.capability")};
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

        for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
            const QMetaProperty property = meta->property(i);
            if (property.isWritable()) {
                writable.append(QStringLiteral("%1.%2").arg(typeName, QString::fromUtf8(property.name())));
            }
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

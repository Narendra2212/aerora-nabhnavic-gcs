#include "AeroraSecurityStatusTest.h"

#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QMetaEnum>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QMetaType>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "AeroraAuditSink.h"
#include "AeroraCommandAuthorizer.h"
#include "AeroraSecurityStatus.h"
#include "AeroraSecurityTestDoubles.h"
#include "MAVLinkLib.h"

using namespace AeroraSecurityTestDoubles;

namespace {

constexpr const char* kAuthorizationLogCategory = "Aerora.Authorization";

QRegularExpression denialWarningPattern()
{
    return QRegularExpression(QStringLiteral("denied mavCommand"));
}

QRegularExpression auditFailureWarningPattern()
{
    return QRegularExpression(QStringLiteral("audit record was not accepted"));
}

AeroraGatedOperation armOperation()
{
    AeroraGatedOperation operation;
    operation.mavCommand = MAV_CMD_COMPONENT_ARM_DISARM;
    operation.targetCompId = MAV_COMP_ID_AUTOPILOT1;
    operation.param1 = 1.0f;
    operation.vehicle.mavlinkSystemId = 1;
    operation.vehicle.resolved = true;
    return operation;
}

/// Names that must never become reachable from QML on this type. Each is a write path, a
/// credential-bearing accessor, or the authorization decision itself.
QStringList forbiddenNames()
{
    return {
        // Installing or replacing a collaborator - the whole authorization chain.
        QStringLiteral("setIdentityProvider"),
        QStringLiteral("setRoleResolver"),
        QStringLiteral("setPolicy"),
        QStringLiteral("setAuditSink"),
        QStringLiteral("setChangeNotifier"),
        // Reaching a collaborator, and with it everything it can do.
        QStringLiteral("identityProvider"),
        QStringLiteral("roleResolver"),
        QStringLiteral("policy"),
        QStringLiteral("auditSink"),
        QStringLiteral("authorizer"),
        // Making or forcing an authorization decision from the interface.
        QStringLiteral("authorize"),
        QStringLiteral("isAuthorized"),
        QStringLiteral("evaluate"),
        // Injecting or reading an audit record. A status reader never writes the trail it reports.
        QStringLiteral("record"),
        QStringLiteral("events"),
        // Identity, roles and key material.
        QStringLiteral("currentIdentity"),
        QStringLiteral("opaqueId"),
        QStringLiteral("resolve"),
        QStringLiteral("roleIds"),
        QStringLiteral("storeKey"),
        QStringLiteral("loadKey"),
        QStringLiteral("removeKey"),
    };
}

/// Every non-signal method @a meta declares itself, by name. Inherited methods are excluded: they
/// belong to the base type's own audit, and QObject's are not Aerora's to review.
///
/// The same derivation `AeroraUiSafetyTest` uses, so the two ratchets measure the same thing.
QStringList declaredNonSignalMethodNames(const QMetaObject* meta)
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

void AeroraSecurityStatusTest::cleanup()
{
    clearChain();

    UnitTest::cleanup();
}

void AeroraSecurityStatusTest::_unconfiguredStationReportsNotEnforced()
{
    clearChain();

    const AeroraSecurityStatus status;

    // The shipped posture, and the one `P7-AUTH-002` and `P7-SEC-005` require be reportable: the
    // decision point exists in the command send path and evaluates nothing, because `Q-029` has
    // not produced a role set for a policy to be written against.
    QCOMPARE(status.enforcement(), AeroraSecurityStatus::Enforcement::NotEnforced);

    // Cross-checked against the chain's own answer, so this cannot pass by reporting a hard-coded
    // value that happens to match.
    QVERIFY(!AeroraCommandAuthorizer::instance()->isFullyConfigured());
}

void AeroraSecurityStatusTest::_configuredChainReportsEnforcedAtTheCommandGate()
{
    installAllowingChain(nullptr);

    const AeroraSecurityStatus status;

    // The positive control. Without it `_unconfiguredStationReportsNotEnforced` could pass because
    // the property is wired to a constant.
    QCOMPARE(status.enforcement(), AeroraSecurityStatus::Enforcement::EnforcedAtCommandGate);
    QVERIFY(AeroraCommandAuthorizer::instance()->isFullyConfigured());

    // The enumerator names its own scope. A consumer deriving a label from it cannot widen the
    // claim to every outbound path: `PARAM_SET` is Tab 9's (`P7-AUTH-003`), and mission writes,
    // `MANUAL_CONTROL` and RC override are unevidenced.
    const QMetaObject& meta = AeroraSecurityStatus::staticMetaObject;
    const int enumIndex = meta.indexOfEnumerator("Enforcement");
    QVERIFY(enumIndex >= 0);

    const QMetaEnum metaEnum = meta.enumerator(enumIndex);
    QCOMPARE(metaEnum.keyCount(), 2);
    QCOMPARE(QByteArray(metaEnum.valueToKey(
                 static_cast<int>(AeroraSecurityStatus::Enforcement::EnforcedAtCommandGate))),
             QByteArrayLiteral("EnforcedAtCommandGate"));
}

void AeroraSecurityStatusTest::_enforcementIsNeverReportedAsVerified()
{
    // Asserted with the chain fully configured, which is the only state in which a consumer would
    // be tempted to render "enforcement verified".
    installAllowingChain(std::make_shared<RecordingSink>());

    const AeroraSecurityStatus status;
    QCOMPARE(status.enforcement(), AeroraSecurityStatus::Enforcement::EnforcedAtCommandGate);

    // A configured policy is not observed enforcement. No overlay source has been compiled, no
    // test has been executed against a built binary and no aircraft has been flown, so there is no
    // evidence to report. `D-006` is what changes this, and flipping the constant without that
    // evidence is the honesty failure the Phase 7 status vocabulary exists to prevent.
    QVERIFY2(!status.isEnforcementVerified(),
             "enforcementVerified must stay false until the gate has been observed refusing a "
             "command on a real build - D-006");
    QVERIFY(!AeroraSecurityStatus::kEnforcementVerified);
}

void AeroraSecurityStatusTest::_auditPropertiesReportAbsenceAndNonDurabilitySeparately()
{
    const AeroraSecurityStatus status;

    // No sink: absent, and nothing survives anything. False rather than unknown, because the
    // answer is known.
    installSinkOnly(nullptr);
    QVERIFY(!status.isAuditSinkInstalled());
    QVERIFY(!status.isAuditDurable());
    QVERIFY(!status.isAuditTamperEvident());

    // A sink installed: present, and still neither durable nor tamper-evident. The three
    // properties are separate because they are separate facts, and this is the pair that must not
    // collapse - installing a sink is the moment a status display is most likely to start
    // claiming the records are evidence.
    installSinkOnly(std::make_shared<MinimalSink>());
    QVERIFY(status.isAuditSinkInstalled());
    QVERIFY2(!status.isAuditDurable(),
             "an installed sink must not be reported as durable - P7-AUDIT-002 / Q-037 is OPEN");
    QVERIFY2(!status.isAuditTamperEvident(),
             "an installed sink must not be reported as tamper-evident - Q-037 / Q4-003 is OPEN");
}

void AeroraSecurityStatusTest::_auditRecordsLostIsVisibleAndMonotonic()
{
    AeroraCommandAuthorizer* const authorizer = AeroraCommandAuthorizer::instance();
    const AeroraSecurityStatus status;

    installSinkOnly(nullptr);
    const qulonglong before = status.auditRecordsLost();

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, denialWarningPattern());
    (void) authorizer->authorize(armOperation());
    verifyExpectedLogMessage();

    // `IF-LOG-015` requires the drop count be shown to the operator, and this property is the only
    // path by which it can be: `SCR-SEC-03`, the audit log viewer, is blocked on `Q-037`.
    QVERIFY2(status.auditRecordsLost() > before, "a lost record must become visible to the operator");
    QCOMPARE(status.auditRecordsLost(), authorizer->lostAuditRecordCount());

    // The count reports loss, not persistence. There is deliberately no property reporting records
    // written or retained, because nothing stores them.
    const qulonglong afterDrops = status.auditRecordsLost();
    installAllowingChain(std::make_shared<RecordingSink>());
    QVERIFY(authorizer->isAuthorized(armOperation()));
    QCOMPARE(status.auditRecordsLost(), afterDrops);
}

void AeroraSecurityStatusTest::_statusChangedIsEmittedOnConfigurationAndOnLoss()
{
    clearChain();

    AeroraSecurityStatus status;
    QSignalSpy spy(&status, &AeroraSecurityStatus::statusChanged);
    QVERIFY(spy.isValid());

    // A configuration change. Without a notification the enforcement strip would bind once at
    // load and never update, so an operator could see "not enforced" after a policy was installed.
    installAllowingChain(nullptr);
    QVERIFY2(spy.count() > 0, "installing a policy must notify the status consumer");
    QCOMPARE(status.enforcement(), AeroraSecurityStatus::Enforcement::EnforcedAtCommandGate);

    // A counted audit loss. The loss count is a property, so it needs a notification for the same
    // reason - otherwise a binding would show the count as of screen load.
    spy.clear();
    AeroraCommandAuthorizer::instance()->setAuditSink(
        std::make_shared<FailingSink>(AeroraAuditWriteOutcome::FailedStorage));
    spy.clear();

    expectLogMessage(kAuthorizationLogCategory, QtWarningMsg, auditFailureWarningPattern());
    QVERIFY(AeroraCommandAuthorizer::instance()->isAuthorized(armOperation()));
    verifyExpectedLogMessage();

    QVERIFY2(spy.count() > 0, "a counted audit loss must notify the status consumer");
}

void AeroraSecurityStatusTest::_noPropertyIsQmlWritable()
{
    const QMetaObject& meta = AeroraSecurityStatus::staticMetaObject;

    QStringList writable;
    for (int i = meta.propertyOffset(); i < meta.propertyCount(); ++i) {
        const QMetaProperty property = meta.property(i);
        if (property.isWritable()) {
            writable.append(QString::fromUtf8(property.name()));
        }
    }

    // Exactly zero, with no recorded deviation. `AeroraUiSafetyTest` tolerates one documented
    // deviation because it audits types owned by other workstreams; this type is purpose-built for
    // the operator interface, so a writable property here would mean QML had been handed a way to
    // change what the station reports about its own enforcement - and that has no justification
    // available to it.
    QVERIFY2(writable.isEmpty(),
             qPrintable(QStringLiteral("AeroraSecurityStatus must expose no writable property. "
                                       "found: %1")
                            .arg(writable.join(QStringLiteral(", ")))));

    // The properties still have to exist, or the assertion above would pass on an empty type.
    QVERIFY(meta.propertyCount() > meta.propertyOffset());
}

void AeroraSecurityStatusTest::_theTypeDeclaresNoQmlCallableMethod()
{
    const QStringList declared = declaredNonSignalMethodNames(&AeroraSecurityStatus::staticMetaObject);

    // A method becomes QML-callable by acquiring `Q_INVOKABLE` or by being a slot, and only those
    // reach the meta-object. The property getters are plain public methods and correctly do not
    // appear here, so the expected set is empty - this type's entire QML-callable surface is its
    // property reads.
    //
    // That is stronger than a reviewed-list ratchet and is the right strength for this type:
    // there is no call a security status reader needs to offer QML.
    QVERIFY2(declared.isEmpty(),
             qPrintable(QStringLiteral("AeroraSecurityStatus must declare no invokable or slot. "
                                       "found: %1")
                            .arg(declared.join(QStringLiteral(", ")))));
}

void AeroraSecurityStatusTest::_noSecurityTypeIsReachableThroughAProperty()
{
    const QMetaObject& meta = AeroraSecurityStatus::staticMetaObject;

    for (int i = meta.propertyOffset(); i < meta.propertyCount(); ++i) {
        const QMetaProperty property = meta.property(i);
        const QMetaType type = property.metaType();

        // The structural guarantee behind `P7-SEC-004`. A pointer property is how a type exposure
        // would happen: one `Q_PROPERTY(AeroraAuthorizationPolicy *policy ...)` and QML reaches
        // the policy, the identity provider and everything they hold. Asserting on the metatype
        // rather than on a name list means a pointer to a type nobody has written yet is caught
        // too.
        QVERIFY2(!type.flags().testFlag(QMetaType::IsPointer),
                 qPrintable(QStringLiteral("property %1 is a pointer (%2): no security object may "
                                           "be reachable from QML")
                                .arg(QString::fromUtf8(property.name()),
                                     QString::fromUtf8(type.name()))));

        // Enumerations and primitives only. A QObject-derived or gadget value type would be a
        // second surface with its own members to audit.
        QVERIFY2(property.isEnumType() || type.flags().testFlag(QMetaType::IsEnumeration)
                     || type.id() == QMetaType::Bool || type.id() == QMetaType::ULongLong,
                 qPrintable(QStringLiteral("property %1 has unreviewed type %2")
                                .arg(QString::fromUtf8(property.name()),
                                     QString::fromUtf8(type.name()))));
    }
}

void AeroraSecurityStatusTest::_forbiddenSecurityNamesAreNotQmlReachable()
{
    const QMetaObject& meta = AeroraSecurityStatus::staticMetaObject;
    const QStringList methods = declaredNonSignalMethodNames(&meta);

    // The complement of the two assertions above. Those catch anything new; this one names the
    // specific paths whose appearance would be a safety regression rather than an oversight, so a
    // failure says what went wrong instead of only that something changed.
    for (const QString& forbidden : forbiddenNames()) {
        QVERIFY2(!methods.contains(forbidden),
                 qPrintable(QStringLiteral("AeroraSecurityStatus exposes forbidden method %1 to QML")
                                .arg(forbidden)));
        QVERIFY2(meta.indexOfProperty(forbidden.toUtf8().constData()) == -1,
                 qPrintable(QStringLiteral("AeroraSecurityStatus exposes forbidden property %1 to QML")
                                .arg(forbidden)));
    }
}

UT_REGISTER_TEST(AeroraSecurityStatusTest, TestLabel::Unit)

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtQmlIntegration/QtQmlIntegration>

/// \brief The read-only projection of Aerora's security posture for the operator interface.
///
/// This is the closure of `D-UI-01`. It exists so `SCR-SHL-03` (the enforcement-state strip) and
/// `SCR-SEC-01` (authorization and enforcement state) can report whether authorization is being
/// enforced, and `P7-SEC-005` can be met - a security screen must be able to say *not enforced*.
///
/// \section projection Why a projection and not a type exposure
///
/// 04-subsystem-and-interface-completion-matrix.md resolves this explicitly: the security screens
/// "need a deliberate minimal accessor (`D-UI-01`) **rather than a type exposure**". `P7-SEC-004`
/// requires that security state is not QML-visible or QML-writable, and the reason is concrete -
/// `AeroraIdentityProvider`, `AeroraRoleResolver`, `AeroraAuthorizationPolicy`,
/// `AeroraCommandAuthorizer`, `AeroraAuditSink` and `AeroraSigningKeyStore` are all either
/// credential-bearing, policy-bearing or write paths.
///
/// So this class carries **no security state of its own**. It declares no data member at all: it
/// holds no identity, no role, no policy, no key, no sink and no pointer to any of them. Every
/// property is computed on read from `AeroraCommandAuthorizer`'s read-only predicates, and none of
/// those predicates returns a security object. A member appearing here would mean the operator
/// interface had acquired a cached copy of security state, which is the thing to avoid.
///
/// \code
/// QML:  AeroraSecurity.enforcement / .auditSinkInstalled / .auditRecordsLost
///          |
///          v  this class, in C++ - booleans, one enum and one count
///       AeroraCommandAuthorizer::isFullyConfigured(), auditIsDurable(), lostAuditRecordCount()
///          |
///          v  stays in C++, never exposed
///       AeroraIdentityProvider / AeroraRoleResolver / AeroraAuthorizationPolicy / AeroraAuditSink
/// \endcode
///
/// \section readonly Read-only, structurally
///
/// Every property is `READ`-only with no `WRITE`. There is no `Q_INVOKABLE` and no public slot, so
/// the type's whole QML-callable surface is its property getters and its one signal. QML therefore
/// cannot change a role, a permission, a policy, an enforcement decision or any security setting
/// through this interface, because there is nothing here to call.
///
/// That is asserted rather than stated. `AeroraSecurityStatusTest` reads this class's meta-object
/// and fails if a writable property, an invokable or a slot appears - the same technique
/// `AeroraUiSafetyTest` applies to the ten types reachable from `AeroraUi`. The ratchet lives in
/// the Security suite because `AeroraUiSafetyTest.cc` belongs to another workstream and carries a
/// cross-boundary edit this phase; registering this type in its audited set is filed as a handoff.
///
/// \section honesty The three distinctions this type refuses to collapse
///
/// A status display is where overclaiming happens, so the properties are shaped to make the
/// honest answer the easy one.
///
/// \li **A mechanism existing** is not **a policy being configured.** The enforcement point is
///     implemented and wired into the core send path (`P7-SEC-001`), and it currently evaluates
///     nothing because no policy is installed (`Q-029`). `enforcement` reports
///     `NotEnforced` for that state - not "unknown", not "pending", not a blank.
/// \li **A policy being configured** is not **enforcement having been verified.**
///     `enforcementVerified` is a compile-time `false` and will stay false until a build exists
///     and the gate has been observed refusing a command. `D-006` blocks that, and the Phase 7
///     status vocabulary does not claim `VERIFIED` anywhere. The property is the same device as
///     `kRemainingEnergyEstimationImplemented = false` and
///     `kGroundStationActuatorPathImplemented = false`: a prohibition expressed as a value a
///     binding can read, rather than as prose a reviewer has to remember.
/// \li **A sink being installed** is not **a record being durable.** `auditSinkInstalled`,
///     `auditDurable` and `auditTamperEvident` are three properties because they are three facts.
///     All three are false in this build.
///
/// \section scope What `EnforcedAtCommandGate` does and does not mean
///
/// The scope is named inside the enumerator so a label derived from it cannot overstate it. The
/// enforcement point covers the `MAV_CMD` traffic that reaches
/// `QGCCorePlugin::authorizeMavCommand()` - `MavCommandQueue::sendWorker()` for queued commands
/// and `Vehicle::startCalibration()` for the one direct-send path that is gated (ADR-015).
///
/// It is **not** a statement that every outbound MAVLink path is authorized. `PARAM_SET` is a
/// different path and is owned by Tab 9 (`P7-AUTH-003`, risk `R7-15`); mission writes,
/// `MANUAL_CONTROL` and RC override are likewise unevidenced. No property here reports on those,
/// deliberately: inventing a coverage property would be asserting a result Tab 9's investigation
/// has not produced, and `ADR-014` has not settled the authorization scope that would define it.
///
/// \section notactuation What this type must never grow
///
/// No payload actuation, power switching, stabilization or failsafe action, and no command path of
/// any kind. It is a status reader. An outbound method appearing here would be an actuating path
/// in the one place the operator interface is allowed to read security state from.
class AeroraSecurityStatus : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(AeroraSecurity)
    QML_SINGLETON

public:
    /// \brief Whether command authorization is being evaluated.
    ///
    /// Two values, because two states are reachable. There is deliberately no `Unknown`: the
    /// authorization chain is a process singleton whose configuration is always readable, so an
    /// `Unknown` enumerator could never be produced, and an unreachable "honest" value is worse
    /// than its absence - it invites a consumer to render a state that never occurs.
    ///
    /// Before `AeroraCorePlugin::init()` runs, nothing is installed and `NotEnforced` is the
    /// accurate answer rather than a missing one.
    enum class Enforcement {
        /// The decision point exists in the command send path and **no policy is installed**, so
        /// commands pass without being evaluated. This is the shipped state while `Q-029` is open.
        NotEnforced,

        /// An identity provider, a role resolver and a configured policy are all present, so
        /// `MAV_CMD` traffic reaching the gate is evaluated. Scope is in the name - see the
        /// class-level note on what this does not cover.
        EnforcedAtCommandGate,
    };
    Q_ENUM(Enforcement)

    Q_PROPERTY(Enforcement enforcement READ enforcement NOTIFY statusChanged)
    Q_PROPERTY(bool enforcementVerified READ isEnforcementVerified CONSTANT)
    Q_PROPERTY(bool auditSinkInstalled READ isAuditSinkInstalled NOTIFY statusChanged)
    Q_PROPERTY(bool auditDurable READ isAuditDurable NOTIFY statusChanged)
    Q_PROPERTY(bool auditTamperEvident READ isAuditTamperEvident NOTIFY statusChanged)
    Q_PROPERTY(qulonglong auditRecordsLost READ auditRecordsLost NOTIFY statusChanged)

    explicit AeroraSecurityStatus(QObject* parent = nullptr);
    ~AeroraSecurityStatus() override;

    [[nodiscard]] Enforcement enforcement() const;

    /// Whether Aerora's command authorization has been *observed* to work.
    ///
    /// Compile-time false. No overlay source has been compiled, no test has been executed and no
    /// aircraft has been flown, so there is no evidence to report. A consumer must not present
    /// `EnforcedAtCommandGate` as verified enforcement, and this is the value that stops it.
    [[nodiscard]] bool isEnforcementVerified() const { return kEnforcementVerified; }

    [[nodiscard]] bool isAuditSinkInstalled() const;

    /// Whether audit records survive a process or host restart. False in this build: nothing
    /// constructs a sink, and no sink that exists reports itself durable. `P7-AUDIT-002` /
    /// `Q-037`.
    [[nodiscard]] bool isAuditDurable() const;

    /// Whether undetected modification of a stored record is prevented or detectable. False until
    /// `Q-037` / `Q4-003` resolves and a mechanism is implemented and tested.
    [[nodiscard]] bool isAuditTamperEvident() const;

    /// Records this process failed to get recorded, by any cause. `IF-LOG-015` requires the count
    /// be shown to the operator, and this is the only path by which it can be.
    ///
    /// `qulonglong` rather than `quint64` because `Q_PROPERTY` needs a type the meta-object system
    /// registers; they are the same width.
    [[nodiscard]] qulonglong auditRecordsLost() const;

    /// Set to false and asserted by test. See `isEnforcementVerified()`.
    static constexpr bool kEnforcementVerified = false;

    /// Emits `statusChanged()` on every live instance.
    ///
    /// Registered with `AeroraCommandAuthorizer` as its change notifier, so the chain can report a
    /// configuration change or a counted audit loss without this class polling and without the
    /// authorizer knowing what a QML engine is.
    ///
    /// A no-op when no instance exists, which is the normal case: the instance is created by a QML
    /// engine, and a unit-test run creates none.
    static void notifyChanged();

signals:
    /// Emitted when the authorization chain's configuration changes, or when the audit loss count
    /// changes. Every property above except `enforcementVerified` may have changed.
    void statusChanged();
};

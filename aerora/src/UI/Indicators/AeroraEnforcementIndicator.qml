import QtQuick

import QGroundControl
import QGroundControl.Controls

// AeroraSecurity needs no entry of its own. It is a QML_SINGLETON registered into the application
// target's module (URI QGC), and the `import QGroundControl` above reaches it, because
// QGroundControlModule declares IMPORTS QGC (src/CMakeLists.txt:22-23) and Qt documents IMPORTS as
// propagating to a module's importers. That is the same route AeroraStateText.qml reaches
// AeroraTelemetryState by.
//
// Not this module's own IMPORTS QGC: that is the member direction, which Qt does not document.
// See HO-TAB1-13.

/// `SCR-SHL-03` - the enforcement-state strip, as a non-vehicle toolbar indicator.
///
/// \section why Why this file exists
///
/// `P7-AUTH-002` and `P7-SEC-005` require that the station be able to say authorization is **not**
/// being enforced. Until this file existed the station held that state and never said so: the read
/// path was closed (`D-UI-01`, `AeroraSecurityStatus`) and both indicator seams were wired, but
/// there was no indicator QML for `HO-TAB7-06` to register. This is `HO-TAB1-10`.
///
/// \section contract The consumer contract, which is unforgiving
///
/// `QGCCorePlugin::toolBarIndicators()` carries bare `QUrl` values - not `QmlComponentInfo`
/// objects as `analyzePages()` does - and `src/Toolbar/FlyViewToolBarIndicators.qml:30` consumes
/// each one as `Loader { source: modelData; visible: item.showIndicator }`. That binding has
/// **no null guard**, so `showIndicator` below is load-bearing rather than conventional: a root
/// without it is a binding error on a null `item`, which is not the same thing as an absent
/// indicator.
///
/// The non-vehicle registry is also read once - its `Q_PROPERTY` is `CONSTANT`
/// (`QGCCorePlugin.h:55`) - so membership cannot depend on anything that changes. It does not:
/// `AeroraSecurity` is a process singleton reachable with no vehicle connected, which is what
/// makes the enforcement strip a non-vehicle indicator rather than a per-vehicle one. The
/// *contents* still update, because `enforcement` carries `NOTIFY statusChanged` and the chain
/// fires it on a configuration change.
///
/// \section always Why it is always shown
///
/// `showIndicator` is unconditionally true, which is a deliberate asymmetry with the upstream
/// indicators that hide themselves when they have nothing to report. This one always has
/// something to report: `AeroraSecurityStatus` has no `Unknown` enumerator because the
/// authorization chain's configuration is always readable, so there is no state in which this
/// strip would have to guess. An absent strip would read as "no information", and the single
/// state this build is actually in - not enforced - is the one that must never be silent.
///
/// \section failsafe Why the comparison matches the enforced value
///
/// `_enforced` tests against `EnforcedAtCommandGate` and the not-enforced rendering is its
/// negation. Written the other way round - matching `NotEnforced` - a failure to resolve the
/// enumerator would yield `undefined === value`, and this strip would claim enforcement that is
/// not happening. This direction fails to *not enforced*, which is the only acceptable failure
/// direction and the same derivation `AeroraOperationsHub.qml` was corrected to use.
///
/// \section scope What this strip does not claim
///
/// The enforced wording names the scope the enumerator names - the queued `MAV_CMD` gate - and
/// nothing wider. `PARAM_SET`, mission writes, `MANUAL_CONTROL` and RC override are outside it,
/// `ADR-014` has not settled the authorization scope and `Q-029` has not produced an operator
/// role set, so no role, permission, policy or coverage claim appears here. Nor does the word
/// "verified": `AeroraSecurity.enforcementVerified` is a compile-time false, and the hub reports
/// it as its own row rather than folding it into a one-line label.
///
/// There is no click handler and no detail drawer. The drill-in target, `SCR-SEC-01`, is
/// `SPECIFIED-NOT-DESIGNED`, and a control that opens nothing is worse than an absent one - the
/// same reasoning `P7-FLY-004` applies to action controls. This strip reads state and does
/// nothing else: no command path, no actuation, no policy write.
///
/// No colour or pixel literal appears here, only `QGCPalette` roles, because the Aerora visual
/// language is unresolved (`Q-040`).
QGCLabel {
    id: root

    /// Required by the unguarded `visible: item.showIndicator` binding described above. See
    /// \ref always for why it is constant.
    property bool showIndicator: true

    readonly property bool _enforced: AeroraSecurity.enforcement === AeroraSecurity.EnforcedAtCommandGate

    // The Loader anchors this root top-to-bottom in the toolbar's Row, so the text is centred
    // rather than sitting at the top of a stretched label.
    verticalAlignment: Text.AlignVCenter

    text: root._enforced ? qsTr("Command authorization: enforced at the queued MAV_CMD gate")
                         : qsTr("Command authorization: not enforced")

    // Warning treatment for the unenforced state, matching AeroraSelfReportRow. The enforced
    // state is plain text, not a positive colour: a configured policy has still not been observed
    // refusing a command.
    color: root._enforced ? qgcPal.text : qgcPal.warningText

    QGCPalette {
        id: qgcPal
        colorGroupEnabled: enabled
    }
}

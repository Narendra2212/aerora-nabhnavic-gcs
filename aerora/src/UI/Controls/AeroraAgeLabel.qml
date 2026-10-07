import QtQuick

import QGroundControl
import QGroundControl.Controls

/// Shows how old an Aerora observation is, and whether its currency was ever checked.
///
/// Required rather than decorative. No freshness threshold is configured anywhere in this build
/// (Q-036), so every observed Aerora item reports `freshnessEvaluated === false`: the receive time
/// is known and the threshold it would be compared against does not exist. A value presented
/// without its age in that state implies a currency check that never ran.
///
/// \section notathreshold refreshIntervalMs is not a freshness threshold
///
/// Age advances continuously with no signal to bind to, so `ageMs()` is a function and a consumer
/// displaying it must re-read it. refreshIntervalMs is how often this label re-reads — a
/// presentation cadence owned by M-Ui. It is **not** a staleness threshold, it does not decide any
/// state class, and changing it cannot make a value look fresh or stale.
QGCLabel {
    id: root

    /// An AeroraReportedValue or an AeroraStateValue. Duck-typed on `observed`,
    /// `freshnessEvaluated` and `ageMs()`, which both types expose identically.
    property var reportedValue: null

    /// How often the age is re-read. See the note above: this is not a freshness threshold.
    property int refreshIntervalMs: 1000

    readonly property bool _observed: root.reportedValue ? root.reportedValue.observed : false
    readonly property bool _evaluated: root.reportedValue ? root.reportedValue.freshnessEvaluated : false

    /// Last read age in milliseconds. -1 when never observed, or when the stamp cannot be aged
    /// because it and the evaluator disagree about their clock domain.
    property int _ageMs: -1

    function _readAge() {
        root._ageMs = root.reportedValue ? root.reportedValue.ageMs() : -1
    }

    // Nothing observed means there is no age, and no age is not "0 s ago". The companion
    // AeroraStateText renders the UNKNOWN state in that case.
    visible: root._observed

    text: {
        if (!root._observed) {
            return ""
        }
        if (root._ageMs < 0) {
            return qsTr("age unavailable")
        }
        const seconds = (root._ageMs / 1000).toFixed(1)
        // An unevaluated age is marked, because the value's currency has not been established and
        // a bare age reads as though it had been.
        return root._evaluated ? qsTr("%1 s ago").arg(seconds)
                               : qsTr("%1 s ago (unverified)").arg(seconds)
    }

    color: qgcPal.colorGrey
    font.pointSize: ScreenTools.smallFontPointSize

    QGCPalette {
        id: qgcPal
        colorGroupEnabled: enabled
    }

    Timer {
        interval: root.refreshIntervalMs
        running: root.visible
        repeat: true
        triggeredOnStart: true
        onTriggered: root._readAge()
    }

    // AeroraReportedValue emits reportChanged; AeroraStateValue emits stateChanged. Both are
    // handled so one control serves both types, and ignoreUnknownSignals keeps the handler the
    // target does not have from being an error.
    Connections {
        target: root.reportedValue
        ignoreUnknownSignals: true

        function onReportChanged() {
            root._readAge()
        }

        function onStateChanged() {
            root._readAge()
        }
    }

    onReportedValueChanged: root._readAge()

    Component.onCompleted: root._readAge()
}

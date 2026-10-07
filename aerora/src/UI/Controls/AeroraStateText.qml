import QtQuick

import QGroundControl
import QGroundControl.Controls

/// Renders one Aerora value's text according to its telemetry state class.
///
/// This control is the mechanism behind the rule that the state class is a property of the
/// binding and not a decision made per screen. A screen author supplies the value text and the
/// class; the treatment is decided here, so no screen can render a companion proposal as
/// effective aircraft state.
///
/// The treatment is deliberately structural rather than decorative. The Aerora visual design
/// language is unresolved (Q-040), so nothing here introduces a colour, an icon or a font family:
/// it uses existing QGCPalette roles and ScreenTools metrics only. What it does guarantee is the
/// set of distinctions 16-ui-state-icd.md requires survive to the screen.
///
/// | Class       | Treatment here                                                      |
/// | ----------- | ------------------------------------------------------------------- |
/// | EFFECTIVE   | Primary text                                                        |
/// | REPORTED    | Primary text                                                        |
/// | REQUESTED   | Subordinate text, and classLabel is non-empty so a caller labels it |
/// | PROPOSED    | Subordinate text, and classLabel is non-empty                       |
/// | STALE       | Warning text, and classLabel is non-empty. Keeps its last reading   |
/// | UNKNOWN     | The word "unknown". Never a zero, a dash or an empty string         |
/// | UNAVAILABLE | Not visible. The element is absent, not blank and not disabled      |
///
/// A caller must render classLabel somewhere adjacent. Colour alone does not satisfy the
/// requirement that a requested or proposed value be *labelled*, and a caller that drops the label
/// has produced a display this control cannot correct.
QGCLabel {
    id: root

    /// The resolved class, from AeroraStateValue.stateClass or AeroraReportedValue.stateClass.
    /// Defaults to UNKNOWN so an unbound control reads as unknown rather than as a value.
    property int stateClass: AeroraTelemetryState.Unknown

    /// The value, already rendered to text by the caller. Only displayed for a class that carries
    /// a datum.
    property string valueText: ""

    /// Non-empty for the classes that must be explicitly labelled. The caller renders it.
    readonly property string classLabel: {
        switch (root.stateClass) {
        case AeroraTelemetryState.Requested:
            return qsTr("requested")
        case AeroraTelemetryState.Proposed:
            return qsTr("proposed")
        case AeroraTelemetryState.Stale:
            return qsTr("stale")
        default:
            return ""
        }
    }

    readonly property bool _carriesValue: AeroraTelemetryState.carriesValue(root.stateClass)
    readonly property bool _subordinate: AeroraTelemetryState.isSubordinate(root.stateClass)
    readonly property bool _stale: root.stateClass === AeroraTelemetryState.Stale
    readonly property bool _unavailable: root.stateClass === AeroraTelemetryState.Unavailable

    // UNAVAILABLE is absence of the element. Not a blank, and explicitly not a disabled control:
    // a disabled control tells the operator the capability exists and is momentarily unavailable,
    // which is false for a capability the aircraft reported it does not have.
    visible: !root._unavailable

    text: {
        if (!root._carriesValue) {
            return qsTr("unknown")
        }
        // Reported with nothing in it, which is a different fact from never reported. Rendering it
        // as an empty string would read as a value.
        if (root.valueText === "") {
            return qsTr("no value reported")
        }
        return root.valueText
    }

    color: {
        if (root._stale) {
            return qgcPal.warningText
        }
        if (!root._carriesValue || root._subordinate) {
            return qgcPal.colorGrey
        }
        return qgcPal.text
    }

    font.pointSize: root._subordinate ? ScreenTools.smallFontPointSize : ScreenTools.defaultFontPointSize

    QGCPalette {
        id: qgcPal
        colorGroupEnabled: enabled
    }
}

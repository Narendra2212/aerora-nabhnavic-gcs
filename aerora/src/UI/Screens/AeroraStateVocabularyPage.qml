import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.AnalyzeView

// AnalyzePage is imported at file level rather than left to the module's IMPORTS list, for the
// reason recorded on AeroraOperationsHub.qml: Qt documents IMPORTS in the consumer direction only.
//
// AeroraTelemetryState needs no import of its own - it registers into the application target's
// module (URI QGC) and `import QGroundControl` reaches it. AeroraStateText needs none either: the
// resource alias puts every AERORA_UI_QML_FILES entry in one flat module directory.

/// `SCR-HLP-02` - the Aerora state vocabulary, as the station itself defines it.
///
/// \section why Why this screen earns its place
///
/// The seven-class vocabulary is the product's central claim: an Aerora display never shows a
/// requested value as though the aircraft were doing it, never shows a stale reading as current,
/// and never shows an unobserved item as a zero. All of that is worthless if the operator does not
/// know what the classes mean, and nothing in the station said so until now.
///
/// \section derived Nothing here is written down twice
///
/// Every row is derived from `AeroraTelemetryState` at runtime - the class list from the `Q_ENUM`,
/// each name from `classToString()`, each property from the four predicates, and the treatment from
/// a live `AeroraStateText` showing exactly what that class looks like on a real screen.
///
/// That is the difference between this and a help document. A class added, renamed or
/// reclassified in C++ changes this page with no edit here, so the page cannot drift from the
/// behaviour it describes. A hand-written table would be a second source of truth, and the first
/// time it disagreed it would be teaching the operator something false.
///
/// `UNAVAILABLE` is the proof that the derivation is real: `AeroraStateText` renders that class by
/// making itself **invisible**, so its sample cell is genuinely empty here. A hand-written table
/// would have put a dash or the word "absent" there and quietly contradicted the control.
///
/// \section readonly Read-only
///
/// Pure reference. No vehicle is read, no setting is written, nothing is sent. The predicates are
/// `Q_INVOKABLE static` pure functions on the state singleton.
AnalyzePage {
    id: root

    pageComponent: pageComponent
    pageDescription: qsTr("What each Aerora state class means, and how a value in that class is "
                          + "displayed. Derived from the station's own definitions, not a written copy "
                          + "of them.")
    allowPopout: true

    /// Every enumerator of `AeroraTelemetryState.Class`, in declaration order, which is the order
    /// the vocabulary is defined in. Listed by name because QML cannot enumerate a `Q_ENUM`'s keys;
    /// `AeroraUiFoundationTest` asserts this set matches the meta-object exactly, so a class added
    /// in C++ and missed here is a test failure rather than a silently incomplete reference.
    readonly property var _classes: [
        AeroraTelemetryState.Unknown,
        AeroraTelemetryState.Unavailable,
        AeroraTelemetryState.Reported,
        AeroraTelemetryState.Effective,
        AeroraTelemetryState.Requested,
        AeroraTelemetryState.Proposed,
        AeroraTelemetryState.Stale
    ]

    /// A value that is obviously a sample and could not be mistaken for a reading from an
    /// aircraft. Not a number: a plausible-looking figure on a reference page is exactly the kind
    /// of thing that gets screenshotted and read as telemetry.
    readonly property string _sampleValue: qsTr("sample")

    Component {
        id: pageComponent

        QGCFlickable {
            objectName: "aeroraVocabulary_flickable"

            width: root.availableWidth
            height: root.availableHeight
            contentHeight: vocabularyColumn.height
            flickableDirection: Flickable.VerticalFlick

            ColumnLayout {
                id: vocabularyColumn

                width: root.availableWidth
                spacing: ScreenTools.defaultFontPixelHeight

                QGCLabel {
                    text: qsTr("State classes")
                    font.bold: true
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Exactly one class applies to any value at any moment. The right-hand column is a "
                               + "live sample rendered by the same control every Aerora screen uses, so it shows "
                               + "the real treatment rather than a description of it.")
                    color: qgcPal.colorGrey
                    font.pointSize: ScreenTools.smallFontPointSize
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: root._classes

                    // One row per class. required property, so a model change cannot leave a
                    // delegate bound to a stale index.
                    RowLayout {
                        id: classRow

                        required property int modelData

                        readonly property int stateClass: classRow.modelData

                        Layout.fillWidth: true
                        spacing: ScreenTools.defaultFontPixelWidth * 2

                        QGCLabel {
                            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 14
                            Layout.alignment: Qt.AlignTop
                            // The station's own name for the class, not a label written here.
                            text: AeroraTelemetryState.classToString(classRow.stateClass)
                            color: qgcPal.text
                        }

                        QGCLabel {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            // The four predicates, which are what the rest of the overlay actually
                            // branches on. Stated as properties rather than prose so the operator
                            // reads the same distinctions the code makes.
                            text: {
                                var notes = []
                                if (AeroraTelemetryState.isAuthoritative(classRow.stateClass)) {
                                    notes.push(qsTr("authoritative - what the aircraft is doing"))
                                }
                                if (AeroraTelemetryState.isCurrent(classRow.stateClass)) {
                                    notes.push(qsTr("current"))
                                }
                                if (AeroraTelemetryState.isSubordinate(classRow.stateClass)) {
                                    notes.push(qsTr("subordinate - labelled, never promoted"))
                                }
                                if (AeroraTelemetryState.isNonValue(classRow.stateClass)) {
                                    notes.push(qsTr("carries no value"))
                                }
                                return notes.length > 0 ? notes.join(qsTr(", ")) : qsTr("no further property")
                            }
                            color: qgcPal.colorGrey
                            font.pointSize: ScreenTools.smallFontPointSize
                            wrapMode: Text.WordWrap
                        }

                        ColumnLayout {
                            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 18
                            Layout.alignment: Qt.AlignTop
                            spacing: 0

                            AeroraStateText {
                                id: sample
                                Layout.fillWidth: true
                                stateClass: classRow.stateClass
                                valueText: root._sampleValue
                            }

                            QGCLabel {
                                Layout.fillWidth: true
                                visible: sample.classLabel !== ""
                                text: sample.classLabel
                                color: qgcPal.colorGrey
                                font.pointSize: ScreenTools.smallFontPointSize
                            }

                            // UNAVAILABLE makes AeroraStateText invisible, which leaves this cell
                            // genuinely blank. Said in words so a blank cell reads as the defined
                            // treatment rather than as a rendering fault.
                            QGCLabel {
                                Layout.fillWidth: true
                                visible: !sample.visible
                                text: qsTr("(no element shown)")
                                color: qgcPal.colorGrey
                                font.pointSize: ScreenTools.smallFontPointSize
                            }
                        }
                    }
                }

                // ------------------------------------------------------------ Scope

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("This page is reference material derived from the station's own definitions. It "
                               + "reads no vehicle, writes no setting and sends nothing. The samples are "
                               + "samples: no value here came from an aircraft.")
                    color: qgcPal.colorGrey
                    font.pointSize: ScreenTools.smallFontPointSize
                    wrapMode: Text.WordWrap
                }

                QGCPalette {
                    id: qgcPal
                    colorGroupEnabled: enabled
                }
            }
        }
    }
}

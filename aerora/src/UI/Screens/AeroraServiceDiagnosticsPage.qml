import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

// SetupPage lives in QGroundControl.Controls (src/QmlControls/SetupPage.qml), which the import
// above reaches, so no extra import is needed for it - unlike AnalyzePage, which belongs to
// AnalyzeViewModule and the Analyze screens have to import explicitly.
//
// AeroraUi and AeroraVehicleServices need no import of their own: they register into the
// application target's module (URI QGC) and `import QGroundControl` reaches it, because
// QGroundControlModule declares IMPORTS QGC.
//
// AeroraSelfReportRow needs none either - the resource alias puts every AERORA_UI_QML_FILES entry
// in one flat module directory, so the implicit import covers it.

/// `SCR-VEH-06` - which Aerora services are attached to this vehicle, and what that does and does
/// not tell you about the aircraft.
///
/// \section what What this page is
///
/// A vehicle setup page, reached from the Vehicle Configuration view. `AeroraVehicleComponent`
/// publishes it the moment this file exists in the build: the descriptor for `SCR-VEH-06` names
/// `AeroraServiceDiagnosticsPage.qml`, and `isPublishable()` gates on the resource existing, so no
/// C++ change was needed to make the page appear.
///
/// It answers a question an operator otherwise cannot ask: *is this vehicle actually composed as
/// an Aerora vehicle, and which parts of it are present?* Every Aerora screen degrades to "not
/// attached" on a vehicle without services, and that reading is indistinguishable from a bug
/// unless something reports the composition itself. This is that something.
///
/// \section classification The one reading that is easy to over-read
///
/// `classification` is **not** hardware identification. `SelectedBySeam` means QGroundControl
/// chose the Aerora firmware plugin for this vehicle, which is a build-configuration outcome; it
/// says nothing about what the aircraft is. Only `CorroboratedAerora` rests on an Aerora evidence
/// source, and no such source exists in this build, so that value is unreachable today. The row
/// says so rather than letting a plugin selection read as an identified airframe.
///
/// \section readonly Read-only
///
/// Nothing here writes a parameter, sends a command, attaches or detaches a service, or resolves a
/// product profile. Every binding is a read of `AeroraUi`, whose whole surface is `READ`-only with
/// no `WRITE` - a property `AeroraUiSafetyTest` holds at zero writable properties across every
/// QML-registered Aerora type. `requiresSetup()` is false on this component for the same reason:
/// there is no action on the page to require.
///
/// \section enums How the enum comparisons are written
///
/// Positive matches only, the same rule the Aerora mission screen follows. If an enumerator
/// stopped resolving, the comparison yields `undefined === value`, every positive match is false,
/// and the row reads as unknown - the honest direction. Written against the negative value, the
/// identical failure would make the row assert a classification nobody established.
///
/// `AeroraVehicleServices` is `QML_ELEMENT`, so its `Q_ENUM` is reachable by type name. That is
/// what separates these comparisons from the Aerora mission screen's, which cannot name an
/// enumerator at all because its gadgets carry no QML registration.
SetupPage {
    id: root

    // Test hook, following the convention upstream uses for `analyzeButton_*` and
    // `settingsDivider_*`: without a name on the root a UI test cannot confirm the setup panel
    // actually loaded rather than silently loading nothing.
    objectName: "aeroraServiceDiagnosticsPage"

    // pageDescription is deliberately not set: SetupPage binds it to vehicleComponent.description,
    // so AeroraVehicleComponent is the single source of the page's name and description. Assigning
    // it here would break that binding and put the same text in two places.
    pageComponent: pageComponent

    // Load-bearing, and not for this file's own use. SetupPage.qml binds `qgcPal.warningText` on
    // its "Disabled while the vehicle is ..." label and declares no QGCPalette of its own, so the
    // derived page has to supply one or that binding raises a ReferenceError on every load. This
    // is why every upstream SetupPage carries the same declaration.
    QGCPalette {
        id: qgcPal
        colorGroupEnabled: enabled
    }

    /// Truthy test rather than a null comparison, so an unresolved property reads as unavailable
    /// instead of available.
    readonly property bool _available: !!AeroraUi && AeroraUi.available

    /// The basis shared by every per-service row.
    readonly property string _servicesBasis: root._available
                                             ? qsTr("Read from the active vehicle's Aerora service map.")
                                             : qsTr("No Aerora services are attached to the active vehicle, so "
                                                    + "the service map was never consulted.")

    // Positive matches only. See \section enums above.
    readonly property bool _corroborated: root._available
                                          && AeroraUi.classification === AeroraVehicleServices.CorroboratedAerora
    readonly property bool _selectedBySeam: root._available
                                            && AeroraUi.classification === AeroraVehicleServices.SelectedBySeam
    readonly property bool _notAerora: root._available
                                       && AeroraUi.classification === AeroraVehicleServices.NotAerora

    Component {
        id: pageComponent

        // A plain ColumnLayout, not a Flickable: SetupPage already wraps the loaded page in a
        // QGCFlickable whose contentHeight tracks this item's height, so the report scrolls on a
        // short screen without a second flickable competing for the same drag gesture.
        ColumnLayout {
            width: Math.max(root.availableWidth, ScreenTools.defaultFontPixelWidth * 60)
            spacing: ScreenTools.defaultFontPixelHeight

            // ---------------------------------------------------------------- Composition

            QGCLabel {
                text: qsTr("Aerora composition")
                font.bold: true
            }

            AeroraSelfReportRow {
                Layout.fillWidth: true
                label: qsTr("Aerora services")
                warning: !root._available
                statusText: root._available
                            ? qsTr("Attached to the active vehicle.")
                            : qsTr("Not attached. Either no vehicle is connected, or the connected vehicle is "
                                   + "not an Aerora composition.")
                basis: qsTr("AeroraUi.available. Services attach from the Aerora firmware plugin's "
                            + "initializeVehicle(), so a vehicle routed to a different plugin has none - "
                            + "which is correct rather than degraded. The offline editing vehicle is "
                            + "deliberately not composed either.")
            }

            AeroraSelfReportRow {
                Layout.fillWidth: true
                label: qsTr("Classification")
                unchecked: !root._available
                // A warning for SelectedBySeam, because that is the value most likely to be
                // mistaken for identification. Not a warning for NotAerora: a positively
                // identified non-Aerora vehicle is a correct answer, not a gap.
                warning: root._selectedBySeam
                statusText: {
                    if (!root._available) {
                        return ""
                    }
                    if (root._corroborated) {
                        return qsTr("Corroborated as Aerora hardware by an Aerora evidence source.")
                    }
                    if (root._selectedBySeam) {
                        return qsTr("QGroundControl selected the Aerora firmware plugin. This is a build "
                                    + "configuration outcome and is NOT evidence about the aircraft.")
                    }
                    if (root._notAerora) {
                        return qsTr("Positively identified as not Aerora hardware.")
                    }
                    return qsTr("Unknown. No classification evidence has been seen.")
                }
                basis: qsTr("AeroraUi.classification. CorroboratedAerora is unreachable in this build: no "
                            + "Aerora evidence source exists, so plugin selection is the only input and it "
                            + "cannot identify an airframe.")
            }

            AeroraSelfReportRow {
                Layout.fillWidth: true
                label: qsTr("Product profile")
                unchecked: !root._available
                statusText: {
                    if (!root._available) {
                        return ""
                    }
                    return AeroraUi.productProfileId === ""
                           ? qsTr("None resolved. No product surface is shown, which is the intended default "
                                  + "rather than a fallback.")
                           : qsTr("Resolved: %1. An opaque build-supplied identifier, matched against a "
                                  + "registry and never parsed.").arg(AeroraUi.productProfileId)
                }
                basis: qsTr("AeroraUi.productProfileId. An empty id shows no product surface at all, which "
                            + "is what keeps a product-specific interface from becoming the default.")
            }

            // ---------------------------------------------------------------- Per-service attachment

            QGCLabel {
                text: qsTr("Attached services")
                font.bold: true
            }

            AeroraSelfReportRow {
                Layout.fillWidth: true
                label: qsTr("Identity")
                unchecked: !root._available
                warning: root._available && !AeroraUi.identity
                statusText: {
                    if (!root._available) {
                        return ""
                    }
                    return AeroraUi.identity ? qsTr("Attached.") : qsTr("Not attached.")
                }
                basis: root._servicesBasis
            }

            AeroraSelfReportRow {
                Layout.fillWidth: true
                label: qsTr("Capabilities")
                unchecked: !root._available
                warning: root._available && !AeroraUi.capabilities
                statusText: {
                    if (!root._available) {
                        return ""
                    }
                    return AeroraUi.capabilities ? qsTr("Attached.") : qsTr("Not attached.")
                }
                basis: root._servicesBasis
            }

            AeroraSelfReportRow {
                Layout.fillWidth: true
                label: qsTr("Payload")
                unchecked: !root._available
                warning: root._available && !AeroraUi.payload
                statusText: {
                    if (!root._available) {
                        return ""
                    }
                    return AeroraUi.payload
                           ? qsTr("Attached. Observation only: no payload request can be submitted from the "
                                  + "interface.")
                           : qsTr("Not attached.")
                }
                basis: qsTr("AeroraUi.payload. IF-PAY-015 is 'None - prohibited': submitRequest() is not "
                            + "invokable from QML, so this page can report the payload and never act on it.")
            }

            AeroraSelfReportRow {
                Layout.fillWidth: true
                label: qsTr("Power")
                unchecked: !root._available
                warning: root._available && !AeroraUi.power
                statusText: {
                    if (!root._available) {
                        return ""
                    }
                    return AeroraUi.power
                           ? qsTr("Attached. The power model has no outbound path of any kind.")
                           : qsTr("Not attached.")
                }
                basis: root._servicesBasis
            }

            AeroraSelfReportRow {
                Layout.fillWidth: true
                label: qsTr("Mission observer")
                unchecked: !root._available
                warning: root._available && !AeroraUi.mission
                statusText: {
                    if (!root._available) {
                        return ""
                    }
                    return AeroraUi.mission
                           ? qsTr("Attached. Mission progress is reported on the Aerora Mission Progress "
                                  + "page under Analyze.")
                           : qsTr("Not attached.")
                }
                basis: qsTr("AeroraUi.mission. The observer has no command API: nothing uploads, downloads, "
                            + "clears, resumes or sets a mission item through it.")
            }

            // ---------------------------------------------------------------- Scope of this page

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("This page reports vehicle composition. It carries no control: nothing on it "
                           + "attaches or detaches a service, writes a parameter, resolves a product "
                           + "profile or sends anything to the aircraft.")
                color: qgcPal.colorGrey
                font.pointSize: ScreenTools.smallFontPointSize
                wrapMode: Text.WordWrap
            }
        }
    }
}

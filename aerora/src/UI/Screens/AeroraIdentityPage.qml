import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

// SetupPage lives in QGroundControl.Controls (src/QmlControls/SetupPage.qml), which the import
// above reaches. AeroraUi needs no import of its own: it registers into the application target's
// module (URI QGC) and `import QGroundControl` reaches it, because QGroundControlModule declares
// IMPORTS QGC. AeroraSelfReportRow needs none either - the resource alias puts every
// AERORA_UI_QML_FILES entry in one flat module directory, so the implicit import covers it.

/// `SCR-VEH-06`'s sibling: `SCR-VEH-02` - which vehicle this is, and which of the two answers to
/// that question the station actually has.
///
/// \section what What this page is
///
/// A vehicle setup page, reached from the Vehicle Configuration view. It is published by
/// `AeroraVehicleComponent` the moment this file exists in the build, because the descriptor for
/// `SCR-VEH-02` names `AeroraIdentityPage.qml` and `isPublishable()` gates on the resource
/// existing.
///
/// \section boundary The boundary this page exists to hold
///
/// Two identities, and conflating them is the defect this screen is built around.
///
/// \li **MAVLink system identity** is protocol-level addressing. It is assigned per connection, is
///     not guaranteed stable across a vehicle's lifetime, and is not guaranteed unique across a
///     fleet. It is usable for addressing a connected vehicle and for nothing that outlives the
///     connection - so it must not be read as "which aircraft this is".
/// \li **Aerora fleet identity** is a product-level identifier, and no evidence source reports one
///     in this build. Its format is open besides: no fleet-identifier, serial-number, UUID or
///     registration-number format is evidenced, so `AeroraVehicleIdentity` stores an opaque token
///     and parses nothing.
///
/// `AeroraVehicleIdentity` holds the two apart structurally - no API derives one from the other,
/// `reportFleetIdentity()` takes a `QString` so a system id cannot be passed by accident, and
/// `fleetIdentity()` is empty rather than falling back to the system id. This page is the operator
/// -facing half of that: the fleet row reads *not reported* in those words, and never borrows the
/// system id to fill the gap.
///
/// \section readonly Read-only
///
/// Nothing here writes a parameter, sends a command, reports an identity or resolves a profile.
/// Every binding is a read of `AeroraUi`, whose whole surface is `READ`-only with no `WRITE` - a
/// property `AeroraUiSafetyTest` holds at zero writable properties across every QML-registered
/// Aerora type. `reportFleetIdentity()` and `clearFleetIdentity()` are plain C++ and are not
/// reachable from here.
SetupPage {
    id: root

    // Test hook, following the convention upstream uses for `analyzeButton_*` and
    // `vehicleConfig_comp_*`: without a name on the root a UI test cannot confirm the setup panel
    // actually loaded rather than silently loading nothing.
    objectName: "aeroraIdentityPage"

    // pageDescription is deliberately not set: SetupPage binds it to vehicleComponent.description,
    // so AeroraVehicleComponent is the single source of the page's name and description.
    pageComponent: pageComponent

    // Load-bearing, and not for this file's own use. SetupPage.qml binds `qgcPal.warningText` on
    // its "Disabled while the vehicle is ..." label and declares no QGCPalette of its own, so a
    // derived page has to supply one or that binding raises a ReferenceError on every load.
    QGCPalette {
        id: qgcPal
        colorGroupEnabled: enabled
    }

    /// Truthy test rather than a null comparison, so an unresolved property reads as unavailable
    /// instead of available.
    readonly property bool _available: !!AeroraUi && AeroraUi.available

    /// The identity service, or null. Null is the normal case: no vehicle, a vehicle that is not
    /// an Aerora composition, or the service not attached.
    readonly property var _identity: root._available && AeroraUi.identity ? AeroraUi.identity : null

    /// Whether a fleet identity has been reported. False is the expected state in every build
    /// shipped so far, and the fleet row says so rather than leaving it to be inferred.
    readonly property bool _fleetKnown: !!root._identity && root._identity.fleetIdentityKnown

    /// The two protocol values as the rows render them, named rather than inlined so a UI test can
    /// assert the reading itself. Binding the right object and rendering the wrong field off it is
    /// the failure a test that only checked `_identity` would miss.
    readonly property string _systemIdText: root._identity ? root._identity.mavlinkSystemId.toString() : ""
    readonly property string _componentIdText: root._identity ? root._identity.mavlinkComponentId.toString() : ""

    Component {
        id: pageComponent

        // A plain ColumnLayout, not a Flickable: SetupPage already wraps the loaded page in a
        // QGCFlickable whose contentHeight tracks this item's height, so the report scrolls on a
        // short screen without a second flickable competing for the same drag gesture.
        ColumnLayout {
            width: Math.max(root.availableWidth, ScreenTools.defaultFontPixelWidth * 60)
            spacing: ScreenTools.defaultFontPixelHeight

            // ---------------------------------------------------------------- Protocol identity

            QGCLabel {
                text: qsTr("MAVLink identity")
                font.bold: true
            }

            AeroraSelfReportRow {
                Layout.fillWidth: true
                label: qsTr("System ID")
                unchecked: !root._identity
                statusText: root._systemIdText
                basis: qsTr("AeroraUi.identity.mavlinkSystemId, adopted from the connected vehicle. Protocol "
                            + "addressing only: it is assigned per connection, is not guaranteed stable across "
                            + "this aircraft's lifetime, and is not guaranteed unique across a fleet.")
            }

            AeroraSelfReportRow {
                Layout.fillWidth: true
                label: qsTr("Component ID")
                unchecked: !root._identity
                statusText: root._componentIdText
                basis: qsTr("AeroraUi.identity.mavlinkComponentId. The vehicle's default target component, "
                            + "used to address messages to it.")
            }

            // ---------------------------------------------------------------- Product identity

            QGCLabel {
                text: qsTr("Aerora fleet identity")
                font.bold: true
            }

            AeroraSelfReportRow {
                Layout.fillWidth: true
                label: qsTr("Fleet identity")
                unchecked: !root._identity
                // A warning, because this is a real gap rather than a benign absence: without a
                // fleet identity nothing the station records about this aircraft can be tied to it
                // across connections. Not a warning for a reported one, which is the good case.
                warning: !!root._identity && !root._fleetKnown
                statusText: {
                    if (!root._identity) {
                        return ""
                    }
                    return root._fleetKnown
                           ? root._identity.fleetIdentity
                           : qsTr("Not reported. The MAVLink system ID above is NOT a substitute and is not "
                                  + "shown here in its place.")
                }
                basis: qsTr("AeroraUi.identity.fleetIdentity, reported by an Aerora evidence source. No such "
                            + "source exists in this build, so this is unreported on every vehicle. The token "
                            + "is stored verbatim and never parsed: no fleet-identifier, serial-number, UUID "
                            + "or registration-number format is evidenced.")
            }

            // ---------------------------------------------------------------- Product profile

            QGCLabel {
                text: qsTr("Product profile")
                font.bold: true
            }

            AeroraSelfReportRow {
                Layout.fillWidth: true
                label: qsTr("Resolved profile")
                unchecked: !root._available
                statusText: {
                    if (!root._available) {
                        return ""
                    }
                    return AeroraUi.productProfileId === ""
                           ? qsTr("None resolved. No product surface is shown, which is the intended default "
                                  + "rather than a fallback.")
                           : AeroraUi.productProfileId
                }
                basis: qsTr("AeroraUi.productProfileId. An opaque build-supplied identifier, matched against a "
                            + "registry and never parsed. A resolved profile says which interface this build "
                            + "offers; like plugin selection, it is not evidence about the airframe.")
            }

            // ---------------------------------------------------------------- Scope of this page

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("This page reports identity. It carries no control: nothing on it writes a "
                           + "parameter, reports or clears a fleet identity, resolves a product profile or "
                           + "sends anything to the aircraft.")
                color: qgcPal.colorGrey
                font.pointSize: ScreenTools.smallFontPointSize
                wrapMode: Text.WordWrap
            }
        }
    }
}

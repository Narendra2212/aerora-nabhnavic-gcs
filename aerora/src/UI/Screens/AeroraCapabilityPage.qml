import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

// SetupPage lives in QGroundControl.Controls (src/QmlControls/SetupPage.qml), which the import
// above reaches. AeroraUi and AeroraVehicleCapabilities need no import of their own: they register
// into the application target's module (URI QGC) and `import QGroundControl` reaches it, because
// QGroundControlModule declares IMPORTS QGC. AeroraSelfReportRow needs none either - the resource
// alias puts every AERORA_UI_QML_FILES entry in one flat module directory.

/// `SCR-VEH-03` - the Aerora capability inventory, and the difference between a capability that was
/// reported absent and one that was never reported at all.
///
/// \section what What this page is
///
/// A vehicle setup page, reached from the Vehicle Configuration view. `AeroraVehicleComponent`
/// publishes it the moment this file exists in the build: the descriptor for `SCR-VEH-03` already
/// names `AeroraCapabilityPage.qml` and `isPublishable()` gates on the resource existing, so no C++
/// change was needed to make the page appear - and its name and description come from that
/// descriptor rather than from here.
///
/// \section why The reading this page exists to prevent
///
/// Nothing reports an Aerora capability in this build. `Q3-009` is open, no evidence source exists,
/// and so every category below is empty. That is not a defect, but it is dangerous to leave
/// unstated: the only place an operator could otherwise see the capability service mentioned is
/// `SCR-VEH-06`, which reports it as *attached* - and an attached service with an empty inventory
/// reads as "this aircraft has no capabilities" unless something says otherwise.
///
/// So each category states, in words, that nothing has been reported and that this is **not** a
/// statement about the aircraft. `AeroraVehicleCapabilities` holds the same distinction
/// structurally: `NotEvidenced` is not `Unavailable`, and `isAvailable()` is true only for
/// `Available`, so an unreported capability can never present as a usable one.
///
/// \section derived Nothing here is written down twice
///
/// The category list is the `Q_ENUM`, the keys are whatever `reportedKeys()` returns, and each
/// state comes from `state()`. No capability identifier is named in this file - none is evidenced,
/// and inventing one would be inventing a capability dialect. The day an evidence source reports a
/// key, it appears here with no edit to this page.
///
/// \section live Why a report counter appears in the bindings
///
/// `reportedKeys()` and `state()` are `Q_INVOKABLE` methods, not notifying properties. A binding
/// over them has nothing to depend on, so it is evaluated once when the panel loads and never
/// again - the page would keep showing the inventory as it stood then and would silently stop
/// agreeing with the vehicle, which is a failure mode this overlay has hit before.
///
/// `AeroraVehicleCapabilities` does emit `capabilityChanged()` and `reportsCleared()`.
/// `_reportRevision` counts them, and every expression that reads the inventory takes it as an
/// argument, so the dependency is visible at each call site rather than inferred. A vehicle change
/// needs no counter: `_capabilityService()` reads `AeroraUi.available` and `AeroraUi.capabilities`,
/// which do notify.
///
/// \section token Why not AeroraTokenDisplay
///
/// The information architecture pairs this screen with `AeroraTokenDisplay`, which renders an
/// `AeroraReportedValue` - a token plus a state class plus an age. A capability key is none of
/// those: it is an opaque `QString` with a three-state evidence value and no timestamp, so that
/// control has nothing to render it from. `AeroraSelfReportRow` is used instead, which is also the
/// control that carries the never-reported / reported-absent distinction this page is about.
///
/// \section readonly Read-only
///
/// Nothing here reports, clears or alters a capability. `report()` and `clearReports()` are plain
/// C++ and are not reachable from QML; the three methods this page calls are all const queries.
/// `requiresSetup()` is false on this component because there is no action on the page to require.
SetupPage {
    id: root

    // Test hook, following the convention upstream uses for `vehicleConfig_comp_*`: without a name
    // on the root a UI test cannot confirm the setup panel actually loaded rather than silently
    // loading nothing.
    objectName: "aeroraCapabilityPage"

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

    /// @return The active vehicle's capability service, or null.
    ///
    /// Null is the normal case: no vehicle, a vehicle that is not an Aerora composition, or the
    /// service not attached. One definition, read by the property below and by the inventory
    /// expressions, so "which object this page reads" is written once.
    function _capabilityService() {
        return (!!AeroraUi && AeroraUi.available && AeroraUi.capabilities) ? AeroraUi.capabilities : null
    }

    /// The same answer as a property, for the rows and the `Connections` target to read. Updates on
    /// `servicesChanged()`, because that is what the reads inside `_capabilityService()` notify on.
    readonly property var _capabilities: root._capabilityService()

    /// Every enumerator of `AeroraVehicleCapabilities.Category`, in declaration order. Listed by
    /// name because QML cannot enumerate a `Q_ENUM`'s keys; `AeroraUiFoundationTest` holds this set
    /// against the meta-object, so a category added in C++ and missed here is a test failure rather
    /// than a silently incomplete report.
    readonly property var _categories: [
        AeroraVehicleCapabilities.Vehicle,
        AeroraVehicleCapabilities.Payload,
        AeroraVehicleCapabilities.Mission,
        AeroraVehicleCapabilities.Telemetry,
        AeroraVehicleCapabilities.Command
    ]

    /// Bumped on every capability report, so `_reportedKeys` is re-read. See \section live.
    property int _reportRevision: 0

    Connections {
        target: root._capabilities

        // Declared with no parameters on purpose: the page re-reads the whole inventory rather
        // than trusting one notification's arguments to describe it.
        function onCapabilityChanged() {
            root._reportRevision++
        }

        function onReportsCleared() {
            root._reportRevision++
        }
    }

    /// The keys reported for each category, in the order `_categories` declares them.
    readonly property var _reportedKeys: root._readReportedKeys(root._reportRevision)

    /// @param revision Not read by the computation. It is this binding's explicit dependency:
    ///        `reportedKeys()` is a `Q_INVOKABLE` rather than a notifying property, so without it
    ///        the expression would be evaluated once when the panel loaded and never again. The
    ///        other dependency is implicit and sufficient on its own - `_capabilityService()` reads
    ///        `AeroraUi.available` and `AeroraUi.capabilities`, which notify, so a vehicle change
    ///        re-reads without the counter.
    ///
    /// Kept a binding rather than an assignment driven from `Component.onCompleted`: `SetupPage`
    /// declares a root-level `Component.onCompleted` of its own, and a derived page's handler is
    /// not guaranteed to run alongside it rather than in place of it.
    function _readReportedKeys(revision) {
        const service = root._capabilityService()

        var byCategory = []
        for (var i = 0; i < root._categories.length; ++i) {
            byCategory.push(service ? service.reportedKeys(root._categories[i]) : [])
        }

        return byCategory
    }

    /// Operator-facing name for a category. The `Q_ENUM` keys are identifiers, not labels, and
    /// `AeroraVehicleCapabilities` publishes no display name for them.
    function _categoryName(category) {
        switch (category) {
        case AeroraVehicleCapabilities.Vehicle:
            return qsTr("Vehicle")
        case AeroraVehicleCapabilities.Payload:
            return qsTr("Payload")
        case AeroraVehicleCapabilities.Mission:
            return qsTr("Mission")
        case AeroraVehicleCapabilities.Telemetry:
            return qsTr("Telemetry")
        case AeroraVehicleCapabilities.Command:
            return qsTr("Command")
        default:
            return qsTr("Unnamed category")
        }
    }

    /// What one reported key's state says. Positive matches only: if an enumerator stopped
    /// resolving, every comparison yields `undefined === value` and the key reads as unreported,
    /// which is the honest direction. Written against `Available` and negated, the same failure
    /// would present an unreported capability as present.
    ///
    /// @param revision Not read. The caller's dependency on the report counter - see \section live.
    function _stateText(category, key, revision) {
        const service = root._capabilityService()
        if (!service) {
            return ""
        }

        const state = service.state(category, key)
        if (state === AeroraVehicleCapabilities.Available) {
            return qsTr("Reported present by an evidence source.")
        }
        if (state === AeroraVehicleCapabilities.Unavailable) {
            return qsTr("Reported absent by an evidence source. This is a report, not an assumption.")
        }
        return qsTr("Nothing reported for this key.")
    }

    /// A key is a warning only when it is listed and yet carries no report, which should not
    /// happen - `report()` rejects `NotEvidenced`, so a listed key always has one. A capability
    /// reported absent is a correct answer rather than a gap, the same reading `SCR-VEH-06` gives
    /// `NotAerora`.
    ///
    /// @param revision Not read. The caller's dependency on the report counter - see \section live.
    function _stateIsWarning(category, key, revision) {
        const service = root._capabilityService()
        if (!service) {
            return false
        }

        const state = service.state(category, key)
        return (state !== AeroraVehicleCapabilities.Available)
               && (state !== AeroraVehicleCapabilities.Unavailable)
    }

    Component {
        id: pageComponent

        // A plain ColumnLayout, not a Flickable: SetupPage already wraps the loaded page in a
        // QGCFlickable whose contentHeight tracks this item's height, so the report scrolls on a
        // short screen without a second flickable competing for the same drag gesture.
        ColumnLayout {
            width: Math.max(root.availableWidth, ScreenTools.defaultFontPixelWidth * 60)
            spacing: ScreenTools.defaultFontPixelHeight

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("Aerora capabilities are reported by the aircraft, never assumed by the station. A "
                           + "capability that has not been reported is shown as unreported and is never "
                           + "treated as present.")
                color: qgcPal.colorGrey
                font.pointSize: ScreenTools.smallFontPointSize
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: root._categories

                // One section per category. required property, so a model change cannot leave a
                // delegate bound to a stale index.
                ColumnLayout {
                    id: categorySection

                    required property int modelData
                    required property int index

                    readonly property int category: categorySection.modelData
                    readonly property var keys: root._reportedKeys.length > categorySection.index
                                                ? root._reportedKeys[categorySection.index]
                                                : []

                    Layout.fillWidth: true
                    spacing: ScreenTools.defaultFontPixelHeight * 0.5

                    QGCLabel {
                        text: root._categoryName(categorySection.category)
                        font.bold: true
                    }

                    // The empty case, which is every category in every build so far. Stated as its
                    // own row rather than as a blank section: a heading with nothing under it is
                    // exactly the display that reads as "none of these apply".
                    AeroraSelfReportRow {
                        Layout.fillWidth: true
                        visible: categorySection.keys.length === 0
                        label: qsTr("Reported capabilities")
                        // not checked when there is no service to ask, a gap when there is one and
                        // it has nothing - and `unchecked` outranks `warning`, so the two cannot
                        // be confused.
                        unchecked: !root._capabilities
                        warning: !!root._capabilities
                        statusText: qsTr("None reported. This is NOT a statement that the aircraft lacks "
                                         + "these capabilities.")
                        basis: root._capabilities
                               ? qsTr("AeroraVehicleCapabilities.reportedKeys(), which is empty: no Aerora "
                                      + "evidence source exists in this build, so nothing has reported a "
                                      + "capability (Q3-009).")
                               : qsTr("No Aerora capability service is attached to the active vehicle, so the "
                                      + "inventory was never consulted.")
                    }

                    Repeater {
                        model: categorySection.keys

                        // One row per reported key. The key is the label: it is an opaque string
                        // from the evidence source and this page does not parse it, map it to a
                        // meaning or compare it against a known set.
                        AeroraSelfReportRow {
                            id: capabilityRow

                            required property string modelData

                            Layout.fillWidth: true
                            label: capabilityRow.modelData
                            warning: root._stateIsWarning(categorySection.category, capabilityRow.modelData,
                                                          root._reportRevision)
                            statusText: root._stateText(categorySection.category, capabilityRow.modelData,
                                                        root._reportRevision)
                            basis: qsTr("AeroraVehicleCapabilities.state(). The key is carried verbatim from "
                                        + "the evidence source and is not parsed: no Aerora capability "
                                        + "identifier is evidenced.")
                        }
                    }
                }
            }

            // ---------------------------------------------------------------- Scope of this page

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("This page reports what the aircraft has said about itself. It carries no control: "
                           + "nothing on it reports, clears or changes a capability, writes a parameter or "
                           + "sends anything to the aircraft.")
                color: qgcPal.colorGrey
                font.pointSize: ScreenTools.smallFontPointSize
                wrapMode: Text.WordWrap
            }
        }
    }
}

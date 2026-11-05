#pragma once

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>

#include "ComplexMissionItem.h"
#include "QGCMapPolygon.h"
#include "QmlObjectListModel.h"
#include "SettingsFact.h"

class AeroraSurfaceObstruction;

/// \brief The one Aerora complex mission item: coverage of a vertical or inclined surface.
///
/// Serves the product classes upstream genuinely lacks - painting, cleaning, facade inspection and
/// structural fire fighting. The other three mission classes need no Aerora code at all: waypoint
/// missions use SimpleMissionItem, ground area coverage uses SurveyComplexItem, and linear
/// coverage along a path uses CorridorScanComplexItem.
///
/// \section sibling A sibling of StructureScan, not a subclass
///
/// This derives from ComplexMissionItem directly, exactly as upstream StructureScanComplexItem
/// does. That is not a style preference: StructureScanComplexItem declares mapVisualQML,
/// patternName, commandName, commandDescription, abbreviation and load all `final`, sealing them
/// to StructureScanMapVisual.qml, to the name Structure Scan, to the abbreviation S and to the
/// StructureScan plan format. A subclass could carry none of the content DG-MIS settles.
///
/// What is reused is the StructureScan *shape* and not its code: a vertical extent, horizontal
/// passes stacked vertically, an entrance and a bottom altitude, and an interval along each pass.
/// Upstream reached the same conclusion first, deriving StructureScan from ComplexMissionItem
/// rather than from the transect machinery, which is the evidence that a layered vertical pattern
/// does not fit transects.
///
/// \section final Nothing here is final
///
/// Every override is `override` and none is `final`. The reason is the paragraph above: being
/// sealed is what made the nearest relative unusable as a base, and handing the same obstacle to
/// whoever extends this would repeat a defect this class exists because of.
///
/// \section limits Implementable now, unflyable until an applicator ICD exists
///
/// The parameter *set* is settled and no limit on any parameter is. The metadata resource carries
/// units and decimal places and no min, max or default, so every numeric parameter starts at zero.
/// A workingDistance of zero is not a usable standoff, and that is deliberate: inventing one would
/// produce a plan that looks flyable and is not. AeroraMissionValidator refuses a plan built on
/// zeros through its GeometryLimits check, which already reports itself unperformable for exactly
/// this reason and starts refusing once the limits arrive.
///
/// \section payload Payload actions are referenced, never commanded
///
/// capabilityRequired is an opaque token resolved by the payload controller. This class defines no
/// payload state and no payload command, so there is no type here that could hold one: the station
/// supervises and firmware actuates.
class AeroraSurfaceCoverageComplexItem : public ComplexMissionItem
{
    Q_OBJECT

public:
    /// \param masterController Plan master controller for the item.
    /// \param flyView          true when created for the Fly View, which is read-only.
    /// \param kmlOrSHPFile     Accepted for signature compatibility with the upstream factory and
    ///                         not read: a worked surface is drawn rather than imported.
    AeroraSurfaceCoverageComplexItem(PlanMasterController *masterController,
                                     bool flyView,
                                     const QString &kmlOrSHPFile = QString());
    ~AeroraSurfaceCoverageComplexItem() override;

    Q_PROPERTY(QGCMapPolygon *surfacePolygon READ surfacePolygon CONSTANT)
    Q_PROPERTY(QmlObjectListModel *obstructions READ obstructions CONSTANT)

    Q_PROPERTY(Fact *workingDistance READ workingDistance CONSTANT)
    Q_PROPERTY(Fact *passInterval READ passInterval CONSTANT)
    Q_PROPERTY(Fact *actionInterval READ actionInterval CONSTANT)
    Q_PROPERTY(Fact *surfaceHeight READ surfaceHeight CONSTANT)
    Q_PROPERTY(Fact *startAltitude READ startAltitude CONSTANT)
    Q_PROPERTY(Fact *entryAltitude READ entryAltitude CONSTANT)
    Q_PROPERTY(Fact *passSpeed READ passSpeed CONSTANT)
    Q_PROPERTY(Fact *passDirection READ passDirection CONSTANT)
    Q_PROPERTY(Fact *capabilityRequired READ capabilityRequired CONSTANT)

    /// The worked surface: a ground footprint extruded vertically by surfaceHeight.
    QGCMapPolygon *surfacePolygon() { return &_surfacePolygon; }

    /// Zero or more exclusions, each carrying its own height and clearance.
    ///
    /// A list rather than a flag because the product range needs several per surface, and because
    /// clearance is per obstruction: a cable tray and a window ledge do not warrant the same
    /// standoff.
    QmlObjectListModel *obstructions() { return &_obstructions; }

    Fact *workingDistance() { return &_workingDistanceFact; }
    Fact *passInterval() { return &_passIntervalFact; }
    Fact *actionInterval() { return &_actionIntervalFact; }
    Fact *surfaceHeight() { return &_surfaceHeightFact; }
    Fact *startAltitude() { return &_startAltitudeFact; }
    Fact *entryAltitude() { return &_entryAltitudeFact; }
    Fact *passSpeed() { return &_passSpeedFact; }
    Fact *passDirection() { return &_passDirectionFact; }
    Fact *capabilityRequired() { return &_capabilityRequiredFact; }

    // Overrides from ComplexMissionItem
    QString patternName() const override { return tr(canonicalName); }
    double minAMSLAltitude() const override;
    double maxAMSLAltitude() const override;
    double complexDistance() const override;
    bool load(const QJsonObject &complexObject, int sequenceNumber, QString &errorString) override;
    double greatestDistanceTo(const QGeoCoordinate &other) const override;

    // Overrides from VisualMissionItem
    bool dirty() const override { return _dirty; }
    bool isSimpleItem() const override { return false; }
    bool isStandaloneCoordinate() const override { return false; }
    bool specifiesCoordinate() const override { return true; }
    bool specifiesAltitudeOnly() const override { return false; }
    QString commandDescription() const override { return tr(canonicalName); }
    QString commandName() const override { return tr(canonicalName); }
    QString abbreviation() const override { return tr("SC"); }
    QGeoCoordinate coordinate() const override;
    QGeoCoordinate entryCoordinate() const override;
    QGeoCoordinate exitCoordinate() const override;
    double editableAlt() const override;
    double amslEntryAlt() const override;
    double amslExitAlt() const override;
    int sequenceNumber() const override { return _sequenceNumber; }
    double specifiedFlightSpeed() override;
    double specifiedGimbalYaw() override;
    double specifiedGimbalPitch() override;
    bool exitCoordinateSameAsEntry() const override { return false; }
    void setDirty(bool dirty) override;
    void setCoordinate(const QGeoCoordinate &coordinate) override;
    void setSequenceNumber(int sequenceNumber) override;
    int lastSequenceNumber() const override;
    void save(QJsonArray &missionItems) override;
    void appendMissionItems(QList<MissionItem *> &items, QObject *missionItemParent) override;
    void applyNewAltitude(double newAltitude) override;
    double additionalTimeDelay() const override;

    /// Both visual seams return a fully qualified qrc: URL, and both are implemented here.
    ///
    /// The URL must be fully qualified. Upstream returns bare filenames, which resolve relative to
    /// the document holding the Loader and work only because upstream visuals sit beside their
    /// loaders. An Aerora visual lives under qrc:/qml/Aerora/Controls/, so a bare filename would
    /// resolve against src/PlanView/ and find nothing.
    QString mapVisualQML() const override;

    /// \return The GeoMap-native visual, which no base class supplies.
    ///
    /// Recorded as an obligation because its absence is silent: GeoMapMissionItems.qml guards on a
    /// non-empty string, so an item without one is invisible with no error of any kind. Every
    /// upstream coverage pattern inherits the empty default, and two map engines exist, selected by
    /// flyViewSettings.useGeoMapEngine with a reboot to switch - so an item missing this works for
    /// one operator and is invisible to another.
    QString geoMapVisualQML() const override;

    /// Operator-visible pattern name, and what complexMissionItemNames() reports.
    static constexpr const char *canonicalName = QT_TR_NOOP("Surface Coverage");

    /// The plan-file type key, permanent from the first saved plan.
    ///
    /// createComplexMissionItem() returns nullptr for an unrecognised type and logs a warning, so
    /// changing this key silently orphans every saved plan: the operator sees a plan that loads
    /// without its pattern and no error saying why. The Aerora prefix is what keeps it from
    /// colliding with a present or future upstream pattern.
    static constexpr const char *jsonComplexItemTypeValue = "AeroraSurfaceCoverage";

    static constexpr const char *settingsGroup = "AeroraSurfaceCoverage";

    static constexpr const char *workingDistanceName = "WorkingDistance";
    static constexpr const char *passIntervalName = "PassInterval";
    static constexpr const char *actionIntervalName = "ActionInterval";
    static constexpr const char *surfaceHeightName = "SurfaceHeight";
    static constexpr const char *startAltitudeName = "StartAltitude";
    static constexpr const char *entryAltitudeName = "EntryAltitude";
    static constexpr const char *passSpeedName = "PassSpeed";
    static constexpr const char *passDirectionName = "PassDirection";
    static constexpr const char *capabilityRequiredName = "CapabilityRequired";

private:
    QMap<QString, FactMetaData *> _metaDataMap;

    int _sequenceNumber = 0;
    bool _dirty = false;

    QGCMapPolygon _surfacePolygon;
    QmlObjectListModel _obstructions;

    SettingsFact _workingDistanceFact;
    SettingsFact _passIntervalFact;
    SettingsFact _actionIntervalFact;
    SettingsFact _surfaceHeightFact;
    SettingsFact _startAltitudeFact;
    SettingsFact _entryAltitudeFact;
    SettingsFact _passSpeedFact;
    SettingsFact _passDirectionFact;
    SettingsFact _capabilityRequiredFact;

    static constexpr const char *_jsonSurfacePolygonKey = "surfacePolygon";
    static constexpr const char *_jsonObstructionsKey = "obstructions";

};

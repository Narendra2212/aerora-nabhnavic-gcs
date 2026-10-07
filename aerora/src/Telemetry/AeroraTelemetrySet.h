#pragma once

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "RuntimeScheduler.h"

class AeroraFactGroupBase;
class AeroraFactGroupRegistrar;
class AeroraFreshness;
class FactGroup;

/// \brief One vehicle's Aerora telemetry: the freshness evaluator and the fact groups bound to it.
///
/// The attachment boundary between \c M-Telemetry and \c M-Vehicle / \c M-Firmware. Vehicle
/// integration attaches this without knowing how state classes, freshness or stamping work, and
/// this module needs no access to the vehicle plugin, the vehicle factory, vehicle classification
/// or vehicle lifecycle.
///
/// \section ownership Per-vehicle ownership, which is settled
///
/// One instance per vehicle, attached under \c kServiceName from the composition root in
/// \c AeroraFirmwarePlugin::initializeVehicle(). It owns the \c AeroraFreshness evaluator, which is
/// what the payload and power models take as a constructor argument, and its lifetime is the
/// vehicle's because it is reparented into that vehicle's service graph.
///
/// Nothing here is static, cached or process-wide, so two vehicles share no telemetry state. That
/// half of the seam needs no core edit and is closed.
///
/// \section registration Fact-group registration, which needs one authorised core edit
///
/// Owning the groups is not the same as the vehicle knowing about them. Three upstream features key
/// off membership of the vehicle's own fact-group map, and none is reachable from outside it:
/// automatic MAVLink dispatch (\c Vehicle.cc:588-590), the operator's telemetry value picker
/// (\c InstrumentValueData::factGroupNames()), and the telemetry CSV record
/// (\c Vehicle::_initializeCsv / \c _writeCsvLine).
///
/// \c FactGroup::_addFactGroup() is \c protected, and upstream grants friendship to exactly two
/// classes so they can call it per vehicle — \c FactGroupListModel and \c GimbalController, both of
/// which register groups **dynamically, at message time**. So per-vehicle registration is an
/// established upstream pattern; what Aerora lacks is the privilege, not a mechanism.
///
/// \c publishTo() is this module's half. It walks the registered groups and hands each to an
/// \c AeroraFactGroupRegistrar, which is implemented once by the authorised core edit. See
/// \c AeroraFactGroupRegistrar for why the boundary is an interface and \c README.md for the Q4-002
/// decision record.
///
/// \section notplugin The route this type does not take
///
/// \c FirmwarePlugin::factGroups() is **not** used, and must not be wired to this type. It takes no
/// \c Vehicle *, and \c FirmwarePluginManager caches one plugin per firmware class, so groups
/// returned through it are shared by every vehicle of that class and the aircraft overwrite each
/// other's telemetry. \c ArduSubFirmwarePlugin has that defect upstream today. Routing registration
/// through a registrar handed a specific vehicle makes the hazard structurally absent instead of
/// merely avoided.
///
/// \section empty Why the set is empty today
///
/// No concrete Aerora fact group exists, because no Aerora dialect exists to parse, so
/// \c publishTo() currently publishes nothing. That is the degraded path described in
/// 11-telemetry-icd.md, and it is the **initial** path rather than an error path: with no Aerora
/// groups the interface is the standard QGroundControl set.
class AeroraTelemetrySet : public QObject
{
    Q_OBJECT

public:
    /// The service name this set is attached under, for \c AeroraVehicleServices::attachService()
    /// and \c service().
    ///
    /// Owned here rather than by the composition root so every Aerora domain follows one rule: the
    /// module that owns the type publishes the key. A service-registry key, not a protocol
    /// identifier — it is never sent, parsed or matched against a MAVLink field.
    static constexpr const char *kServiceName = "telemetry";

    /// \param scheduler The clock domain for stamping and staleness. Pass a deterministic scheduler
    ///                  in tests; when null the freshness evaluator creates and owns a
    ///                  \c QtRuntimeScheduler.
    /// \param parent    Owner. Construct one per vehicle.
    explicit AeroraTelemetrySet(RuntimeScheduler *scheduler = nullptr, QObject *parent = nullptr);
    ~AeroraTelemetrySet() override;

    /// The shared evaluator for every group in this set. Owned by this object, and outlived by
    /// nothing in it.
    ///
    /// Shared on purpose: staleness thresholds are per class, not per group, so one evaluator per
    /// vehicle keeps a class's threshold identical wherever it is consumed (\c IF-TLM-011).
    [[nodiscard]] AeroraFreshness *freshness() const { return _freshness; }

    /// Registers a group under the name the vehicle will expose it as.
    ///
    /// Must be called during vehicle construction, before \c publishTo(). A group registered after
    /// the telemetry CSV header is written shifts every later column (R-027,
    /// \c AERORA-GCS-REQ-022), so registration is refused once the set is sealed.
    ///
    /// \return True when the group was registered.
    bool registerGroup(const QString &name, AeroraFactGroupBase *group);

    /// Publishes every registered group to @a registrar, then seals the set.
    ///
    /// Called once per vehicle from the composition root, during vehicle construction. Sealing is
    /// what makes "Aerora adds no fact group dynamically" checkable rather than merely asserted:
    /// after publication \c registerGroup() refuses, so the set the CSV header was written from is
    /// the set that stays.
    ///
    /// Publication order is the set's name order, which is stable for a given group collection.
    ///
    /// \return True when the set was published and every group was accepted. False means either the
    ///         set was already published, or at least one group was refused — a refused group is
    ///         reported rather than silently treated as present. An empty set publishes
    ///         successfully; it has nothing to refuse.
    bool publishTo(AeroraFactGroupRegistrar &registrar);

    /// True once the set has been published or its group map handed out, after which it accepts no
    /// further groups.
    [[nodiscard]] bool isSealed() const { return _sealed; }

    /// True once \c publishTo() has succeeded. Distinct from \c isSealed(), which is also set by
    /// handing out the raw map for diagnostics.
    [[nodiscard]] bool isPublished() const { return _published; }

    [[nodiscard]] QStringList groupNames() const;

    /// \return The registered group, or nullptr when the set has none by that name.
    [[nodiscard]] AeroraFactGroupBase *group(const QString &name) const;

    /// The group map, for diagnostics and for a registrar that prefers to enumerate rather than be
    /// called back. Never null; the pointee is owned by this object.
    ///
    /// Handing it out seals the set, because a caller holding the map can observe a membership that
    /// later registration would contradict.
    ///
    /// **Not** for \c FirmwarePlugin::factGroups() — see the class documentation. The shape
    /// coincides; the lifetime and the sharing semantics do not.
    [[nodiscard]] QMap<QString, FactGroup *> *factGroups();

    /// Returns every item in every group to never-observed, for connection teardown. Facts keep
    /// their last values; only the observation metadata is cleared (\c DEG-03).
    void clearObservations();

    /// True when any group in this set has been reached by more than one vehicle — the cross-vehicle
    /// sharing hazard actually occurring. For the startup self-check and the isolation test.
    [[nodiscard]] bool sharedAcrossVehiclesDetected() const;

private:
    AeroraFreshness *_freshness = nullptr;
    QMap<QString, FactGroup *> _nameToFactGroupMap;
    QMap<QString, AeroraFactGroupBase *> _nameToAeroraGroupMap;
    bool _sealed = false;
    bool _published = false;
};

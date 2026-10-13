#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "AeroraMissionExtension.h"
#include "UnitTest.h"

/// Architectural-property tests for the Aerora mission module.
///
/// None of these need a `Vehicle`, a `MissionManager`, a link or an event loop, so they run as
/// `Unit`. That is a consequence of the module's shape rather than a convenience: every mission
/// type except AeroraMissionBridge is free of QGroundControl mission dependencies, so the
/// properties below can be asserted directly instead of through a simulated flight.
///
/// The properties chosen are the ones that are expensive to get wrong and invisible when they
/// regress: an editor selection never standing in for the aircraft's position, a plan never being
/// compared against itself, an unverified upload never presenting as verified, a wire-precision
/// difference never reported as a real one, and the resume replay set never acquiring an Aerora
/// or payload command.
///
/// Behaviour against a real vehicle is covered by AeroraMissionSyncTest.
class AeroraMissionTest : public UnitTest
{
    Q_OBJECT

private slots:
    void cleanup() override;

    void _missionIndexKeepsOriginsApart();
    void _snapshotComparisonRefusesWrongOriginsAndUncapturedSides();
    void _agreementVerdicts_data();
    void _agreementVerdicts();
    void _agreementToleratesDifferencesBelowWirePrecision();
    void _resumeBoundaryMirrorsUpstreamAndCarriesNoPayloadCommand();
    void _resumeReportsItemsNotCarriedForwardBeforeResumeIndex();
    void _missionExtensionRegistryRefusesAmbiguity();
    void _missionCommandSetIsNotDeclared();

    void _validationModelNeverTreatsAnUncheckedConditionAsPassed();
    void _validationVerdicts_data();
    void _validationVerdicts();
    void _validationNamesEveryCheckWhetherOrNotItRan();
    void _validationPassesWithGapsOnACleanMission();
    void _validationFailures_data();
    void _validationFailures();
    void _missionCommandSupportRunsOnlyWithADeclaredSet();
    void _resumeCarryForwardWithholdsItsVerdict();
    void _validationConsumesASnapshotBuiltFromRealMissionItems();
    void _observerPresentsAnUncheckedUploadAsUnverified();
    void _observerMarksTheVehicleMissionIndeterminateAfterAFailedUpload();
    void _observerDistinguishesAFailedDownloadFromAnEmptyMission();
    void _mockIsDeterministicAndSimulatesNoAircraft();

private:
    /// Minimal extension stand-in.
    ///
    /// It creates no complex item — `createComplexMissionItem()` always returns nullptr — because
    /// no Aerora complex item is evidenced (Q-021) and inventing one here would put a guessed
    /// mission structure into the test suite. What the registry tests actually need is the
    /// registration and name-claiming behaviour, which this provides.
    class StubExtension : public AeroraMissionExtension
    {
    public:
        StubExtension(const QString &id, const QStringList &itemNames, const QList<MAV_CMD> &commandSet)
            : _extensionId(id)
            , _itemNames(itemNames)
            , _commandSet(commandSet)
        {
        }

        QString extensionId() const override { return _extensionId; }
        QStringList complexMissionItemNames() const override { return _itemNames; }

        ComplexMissionItem *createComplexMissionItem(const QString &canonicalName,
                                                     PlanMasterController *masterController,
                                                     bool flyView) override
        {
            Q_UNUSED(canonicalName)
            Q_UNUSED(masterController)
            Q_UNUSED(flyView)
            createCallCount++;
            return nullptr;
        }

        QList<MAV_CMD> missionCommandSet() const override { return _commandSet; }

        int createCallCount = 0;

    private:
        const QString _extensionId;
        const QStringList _itemNames;
        const QList<MAV_CMD> _commandSet;
    };

    QList<StubExtension *> _stubExtensions;
};

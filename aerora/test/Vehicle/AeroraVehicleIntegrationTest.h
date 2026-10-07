#pragma once

#include <QtCore/QList>

#include "AeroraProductProfile.h"
#include "AeroraVehicleCapabilities.h"
#include "UnitTest.h"

/// Architectural-property tests for the Aerora vehicle integration layer.
///
/// None of these need a `Vehicle`, a link or an event loop, so they run as `Unit`. The properties
/// they assert are the ones that are expensive to get wrong and invisible when they regress:
/// MAVLink identity never standing in for fleet identity, unreported capabilities never
/// presenting as available, classification never being inferred, and the firmware plugin keeping
/// the base class's safe defaults.
///
/// Vehicle-attached behaviour is covered by AeroraVehicleLifecycleTest.
class AeroraVehicleIntegrationTest : public UnitTest
{
    Q_OBJECT

private slots:
    void cleanup() override;

    void _identityKeepsMavlinkAndFleetIdentityApart();
    void _capabilityEvidenceStates_data();
    void _capabilityEvidenceStates();
    void _capabilityClearReturnsToNotEvidenced();
    void _classificationIsNeverInferredFromCapabilities();
    void _firmwarePluginKeepsBaseClassSafeDefaults();
    void _factoryRegistersItselfAndClaimsOneFirmwareClass();
    void _productProfileResolutionRefusesAmbiguity();
    void _serviceAttachmentIsTheSingleMechanism();
    void _mockIsDeterministicAndFabricatesNoVehicle();
    void _domainServicesAreComposedPerVehicle();
    void _domainServiceCompositionIsIdempotent();
    void _domainServicesOpenNoActuatorPath();

private:
    /// Minimal profile stand-in. Matching is driven by the test rather than by capability content,
    /// because no capability identifier is evidenced and the test must not invent one.
    class StubProfile : public AeroraProductProfile
    {
    public:
        StubProfile(const QString &id, bool matches)
            : _profileId(id)
            , _matches(matches)
        {
        }

        QString profileId() const override { return _profileId; }
        bool matchesCapabilities(const AeroraVehicleCapabilities &capabilities) const override
        {
            Q_UNUSED(capabilities)
            return _matches;
        }
        void attachToVehicle(AeroraVehicleServices &services) override
        {
            Q_UNUSED(services)
            attachCount++;
        }

        int attachCount = 0;

    private:
        const QString _profileId;
        const bool _matches;
    };

    QList<StubProfile *> _stubProfiles;
};

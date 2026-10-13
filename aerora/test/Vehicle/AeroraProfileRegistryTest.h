#pragma once

#include <QtCore/QList>
#include <QtCore/QString>

#include "AeroraProductProfile.h"
#include "AeroraVehicleCapabilities.h"
#include "UnitTest.h"

/// Covers the product profile registry's selection contract.
///
/// Scoped to what AeroraVehicleIntegrationTest does not already assert. That test covers an empty
/// registry, a non-matching profile, duplicate-id rejection, a successful resolution and the
/// refusal to resolve an ambiguous set; those are not repeated here.
///
/// What is added is the part of the contract that is about *direction*: a profile is selected from
/// what the vehicle reported, and selecting one never adds to what the vehicle reported
/// (`P7-FLEET-001`, `P7-FLEET-002`, `P7-PROF-001`, `P7-PROF-004`).
///
/// ### One limit on what these tests can claim
///
/// "A profile never grants a capability" is asserted of the two pieces that can enforce it: the
/// registry, whose `resolve()` takes a `const AeroraVehicleCapabilities &`, and the vehicle layer,
/// which reports nothing on a profile's behalf. It is **not** structurally enforced against a
/// profile implementation: `AeroraProductProfile::attachToVehicle()` receives a mutable
/// `AeroraVehicleServices &`, whose `capabilities()` hands out a non-const pointer with a public
/// `report()`. A profile that called it would succeed. That gap is recorded rather than tested,
/// because a test asserting it would enshrine it.
class AeroraProfileRegistryTest : public UnitTest
{
    Q_OBJECT

private slots:
    void cleanup() override;

    void _registrationRejectsNullAndEmptyIdAndKeepsOrder();
    void _resolutionFollowsReportedCapabilityAndRunsAtMostOnce();
    void _selectingAProfileGrantsNoCapability();

private:
    /// A profile that matches on one reported capability, so selection is driven by inventory
    /// content rather than by a fixed answer the test chose.
    ///
    /// The key is opaque and test-local: no Aerora capability identifier is evidenced, so a test
    /// must not introduce one that could be mistaken for a real key.
    class CapabilityKeyedProfile : public AeroraProductProfile
    {
    public:
        CapabilityKeyedProfile(const QString &id,
                               AeroraVehicleCapabilities::Category category,
                               const QString &key)
            : _profileId(id)
            , _category(category)
            , _key(key)
        {
        }

        QString profileId() const override { return _profileId; }

        bool matchesCapabilities(const AeroraVehicleCapabilities &capabilities) const override
        {
            // Declining under uncertainty is the required answer: isAvailable() is true only for
            // State::Available, so neither an unreported nor an absent capability selects this
            // profile.
            return capabilities.isAvailable(_category, _key);
        }

        void attachToVehicle(AeroraVehicleServices &services) override
        {
            Q_UNUSED(services)
            attachCount++;
        }

        int attachCount = 0;

    private:
        const QString _profileId;
        const AeroraVehicleCapabilities::Category _category;
        const QString _key;
    };

    /// Registers a profile and keeps it alive for the duration of the test.
    CapabilityKeyedProfile *_registerProfile(const QString &id,
                                             AeroraVehicleCapabilities::Category category,
                                             const QString &key);

    QList<CapabilityKeyedProfile *> _profiles;
};

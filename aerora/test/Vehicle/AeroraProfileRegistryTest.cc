#include "AeroraProfileRegistryTest.h"

#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>
#include <QtTest/QTest>

#include "AeroraVehicleServices.h"

using Category = AeroraVehicleCapabilities::Category;
using State = AeroraVehicleCapabilities::State;

namespace {

constexpr const char *kLogCategory = "Aerora.Vehicle";

} // namespace

void AeroraProfileRegistryTest::cleanup()
{
    // The registry is process-global. Clearing it before the stubs are destroyed keeps a dangling
    // registration out of any later test that constructs a vehicle.
    AeroraProductProfileRegistry::instance()->clear();
    qDeleteAll(_profiles);
    _profiles.clear();

    UnitTest::cleanup();
}

AeroraProfileRegistryTest::CapabilityKeyedProfile *AeroraProfileRegistryTest::_registerProfile(
    const QString &id, Category category, const QString &key)
{
    auto *const profile = new CapabilityKeyedProfile(id, category, key);
    _profiles.append(profile);

    return profile;
}

void AeroraProfileRegistryTest::_registrationRejectsNullAndEmptyIdAndKeepsOrder()
{
    AeroraProductProfileRegistry *const registry = AeroraProductProfileRegistry::instance();
    registry->clear();

    expectLogMessage(kLogCategory, QtWarningMsg, QRegularExpression(QStringLiteral("null profile")));
    QVERIFY(!registry->registerProfile(nullptr));
    verifyExpectedLogMessage();

    // An empty id is rejected because the id is how a consumer selects product UI. A profile that
    // resolved under an empty id would be indistinguishable from no profile at all - and
    // AeroraVehicleServices::productProfileId() returns an empty string for exactly that case.
    CapabilityKeyedProfile *const unnamed =
        _registerProfile(QString(), Category::Vehicle, QStringLiteral("key"));
    expectLogMessage(kLogCategory, QtWarningMsg, QRegularExpression(QStringLiteral("empty profile id")));
    QVERIFY(!registry->registerProfile(unnamed));
    verifyExpectedLogMessage();

    QVERIFY(registry->profileIds().isEmpty());

    // Registration order is preserved and is the order resolve() scans. That matters because
    // ambiguity is detected by scanning every profile rather than stopping at the first match, so
    // the order must be stable enough to make an ambiguous set reproducible.
    QVERIFY(registry->registerProfile(
        _registerProfile(QStringLiteral("first"), Category::Vehicle, QStringLiteral("a"))));
    QVERIFY(registry->registerProfile(
        _registerProfile(QStringLiteral("second"), Category::Payload, QStringLiteral("b"))));
    QVERIFY(registry->registerProfile(
        _registerProfile(QStringLiteral("third"), Category::Mission, QStringLiteral("c"))));

    QCOMPARE(registry->profileIds(),
             QStringList({QStringLiteral("first"), QStringLiteral("second"), QStringLiteral("third")}));
}

void AeroraProfileRegistryTest::_resolutionFollowsReportedCapabilityAndRunsAtMostOnce()
{
    AeroraProductProfileRegistry *const registry = AeroraProductProfileRegistry::instance();
    registry->clear();

    const QString key = QStringLiteral("test-local-opaque-capability-key");
    CapabilityKeyedProfile *const profile = _registerProfile(QStringLiteral("keyed"), Category::Vehicle, key);
    QVERIFY(registry->registerProfile(profile));

    AeroraVehicleServices services(nullptr);

    // Nothing reported: no profile. P7-PROF-004's direction - the profile reads the inventory, so
    // with an empty inventory it declines rather than being selected by default.
    services.resolveProductProfile();
    QCOMPARE(services.productProfile(), nullptr);
    QCOMPARE(profile->attachCount, 0);

    // Reported *absent* is still not a match. This is the distinction the capability model exists
    // for: State::Unavailable is a report, and it is not availability.
    QVERIFY(services.capabilities()->report(Category::Vehicle, key, State::Unavailable));
    services.resolveProductProfile();
    QCOMPARE(services.productProfile(), nullptr);
    QCOMPARE(profile->attachCount, 0);

    // Reported available: it resolves and attaches exactly once. No explicit resolve call here on
    // purpose - AeroraVehicleServices retries resolution on every capability report, and that
    // retry is the only path by which a vehicle that reports its capabilities after connect ever
    // gets a profile.
    QVERIFY(services.capabilities()->report(Category::Vehicle, key, State::Available));
    QCOMPARE(services.productProfile(), static_cast<AeroraProductProfile *>(profile));
    QCOMPARE(profile->attachCount, 1);

    // Resolution is at most once per vehicle, and this is the assertion that holds it. A second
    // profile that also matches must not displace the first: by this point the resolved profile
    // has already attached its services, and swapping it would mean detaching them mid-connection.
    CapabilityKeyedProfile *const competitor =
        _registerProfile(QStringLiteral("competitor"), Category::Vehicle, key);
    QVERIFY(registry->registerProfile(competitor));

    services.resolveProductProfile();
    QCOMPARE(services.productProfile(), static_cast<AeroraProductProfile *>(profile));
    QCOMPARE(services.productProfileId(), QStringLiteral("keyed"));
    QCOMPARE(competitor->attachCount, 0);

    // Not even re-attached. A repeated resolve must not re-run attachment, which would attach a
    // profile's services twice.
    QCOMPARE(profile->attachCount, 1);
}

void AeroraProfileRegistryTest::_selectingAProfileGrantsNoCapability()
{
    AeroraProductProfileRegistry *const registry = AeroraProductProfileRegistry::instance();
    registry->clear();

    const QString reportedKey = QStringLiteral("reported-key");
    const QString expectedKey = QStringLiteral("expected-but-never-reported-key");

    // Reported before any profile is registered, so the report's own resolution retry finds an
    // empty registry and resolution happens at the explicit call below instead. That keeps the
    // before-and-after comparison around a known point.
    AeroraVehicleServices services(nullptr);
    QVERIFY(services.capabilities()->report(Category::Vehicle, reportedKey, State::Available));

    // A profile keyed on a capability the vehicle never reports: the mismatch case, where the
    // profile expects something the vehicle has not said it has.
    CapabilityKeyedProfile *const expectant =
        _registerProfile(QStringLiteral("expectant"), Category::Payload, expectedKey);
    QVERIFY(registry->registerProfile(expectant));

    CapabilityKeyedProfile *const matching =
        _registerProfile(QStringLiteral("matching"), Category::Vehicle, reportedKey);
    QVERIFY(registry->registerProfile(matching));

    const QStringList vehicleKeysBefore = services.capabilities()->reportedKeys(Category::Vehicle);
    QCOMPARE(services.productProfile(), nullptr);

    services.resolveProductProfile();
    QCOMPARE(services.productProfile(), static_cast<AeroraProductProfile *>(matching));

    // P7-FLEET-002. Selecting a profile changed nothing about what the vehicle reported: the same
    // keys, in the same order, and nothing appeared in the category the unselected profile
    // expected. The mismatch is left standing for a consumer to display rather than reconciled
    // into an inventory entry, which is what would make an expectation read as a report.
    QCOMPARE(services.capabilities()->reportedKeys(Category::Vehicle), vehicleKeysBefore);
    QVERIFY(services.capabilities()->reportedKeys(Category::Payload).isEmpty());
    QCOMPARE(services.capabilities()->state(Category::Payload, expectedKey), State::NotEvidenced);

    // And the unselected profile's expectation did not make it available anywhere.
    QVERIFY(!services.capabilities()->isAvailable(Category::Payload, expectedKey));
    QCOMPARE(expectant->attachCount, 0);

    // The registry itself cannot report: resolve() takes a const inventory. Asserted through the
    // observable consequence so the property survives a signature change.
    const AeroraVehicleCapabilities &inventory = *services.capabilities();
    QCOMPARE(registry->resolve(inventory), static_cast<AeroraProductProfile *>(matching));
    QCOMPARE(services.capabilities()->reportedKeys(Category::Vehicle), vehicleKeysBefore);
    QVERIFY(services.capabilities()->reportedKeys(Category::Payload).isEmpty());
}

UT_REGISTER_TEST(AeroraProfileRegistryTest, TestLabel::Unit)

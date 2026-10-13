#include "AeroraVehicleComponent.h"

#include <QtCore/QFile>
#include <QtCore/QLatin1Char>

namespace {

/// Builds a descriptor whose resources live in the overlay's QML module.
///
/// Only the setup panel is declared. A summary tile is left undeclared rather than declared and
/// absent, because isPublishable() requires every declared resource to exist and a page should not
/// be withheld for want of an optional tile.
AeroraVehicleComponent::PageDescriptor pageDescriptor(AeroraVehicleComponent::Page page,
                                                      const char *screenId,
                                                      const char *setupQmlFileName)
{
    AeroraVehicleComponent::PageDescriptor descriptor;
    descriptor.page = page;
    descriptor.screenId = QString::fromUtf8(screenId);
    descriptor.setupResource = QString::fromUtf8(AeroraVehicleComponent::kQmlModuleResourceRoot)
                               + QLatin1Char('/') + QString::fromUtf8(setupQmlFileName);

    return descriptor;
}

} // namespace

QList<AeroraVehicleComponent::PageDescriptor> AeroraVehicleComponent::pageDescriptors()
{
    // Page::Calibration is absent by decision, not by omission - see withheldPageReason().
    return {
        pageDescriptor(Page::Identity, "SCR-VEH-02", "AeroraIdentityPage.qml"),
        pageDescriptor(Page::Capabilities, "SCR-VEH-03", "AeroraCapabilityPage.qml"),
        pageDescriptor(Page::SensorHealth, "SCR-VEH-04", "AeroraHealthPage.qml"),
        pageDescriptor(Page::ServiceDiagnostics, "SCR-VEH-06", "AeroraServiceDiagnosticsPage.qml"),
    };
}

QString AeroraVehicleComponent::withheldPageReason(Page page)
{
    if (page == Page::Calibration) {
        // Not translated: this is a developer- and test-facing reason, not operator text.
        return QStringLiteral(
            "Calibration acts on the aircraft and its authorization gate (ADR-015) permits "
            "everything while the operator role set is undecided (Q-029). Aerora publishes no "
            "calibration page; upstream calibration remains reachable through its own firmware "
            "plugin.");
    }

    return QString();
}

QUrl AeroraVehicleComponent::resolvedQmlUrl(const QString &resourcePath)
{
    // Guarding the leading ':' keeps a filesystem path from being rewritten into a qrc URL, which
    // would resolve to nothing at runtime and report as a missing QML type rather than a bad path.
    if (resourcePath.isEmpty() || !resourcePath.startsWith(QLatin1Char(':'))) {
        return QUrl();
    }

    if (!QFile::exists(resourcePath)) {
        return QUrl();
    }

    return QUrl(QStringLiteral("qrc") + resourcePath);
}

bool AeroraVehicleComponent::isPublishable(const PageDescriptor &descriptor)
{
    // A page with no panel has nothing to show, so an undeclared setup resource is not
    // publishable - distinct from a declared one that is merely absent.
    if (descriptor.setupResource.isEmpty()) {
        return false;
    }

    for (const QString &resourcePath : {descriptor.setupResource,
                                        descriptor.summaryResource,
                                        descriptor.configJsonResource,
                                        descriptor.iconResource}) {
        if (!resourcePath.isEmpty() && !QFile::exists(resourcePath)) {
            return false;
        }
    }

    return true;
}

QStringList AeroraVehicleComponent::unresolvedPageResources()
{
    QStringList unresolved;

    for (const PageDescriptor &descriptor : pageDescriptors()) {
        for (const QString &resourcePath : {descriptor.setupResource,
                                            descriptor.summaryResource,
                                            descriptor.configJsonResource,
                                            descriptor.iconResource}) {
            if (!resourcePath.isEmpty() && !QFile::exists(resourcePath)) {
                unresolved.append(resourcePath);
            }
        }
    }

    return unresolved;
}

AeroraVehicleComponent::AeroraVehicleComponent(const PageDescriptor &descriptor,
                                               Vehicle *vehicle,
                                               AutoPilotPlugin *autopilot,
                                               QObject *parent)
    : VehicleComponent(vehicle, autopilot, AutoPilotPlugin::UnknownVehicleComponent, parent)
    , _descriptor(descriptor)
{
}

AeroraVehicleComponent::~AeroraVehicleComponent() = default;

QString AeroraVehicleComponent::name() const
{
    // Literal tr() per case so the strings are extractable. A descriptor-carried name would not be.
    switch (_descriptor.page) {
    case Page::Identity:
        return tr("Aerora Identity");
    case Page::Capabilities:
        return tr("Aerora Capabilities");
    case Page::SensorHealth:
        return tr("Aerora Health");
    case Page::Calibration:
        return tr("Aerora Calibration");
    case Page::ServiceDiagnostics:
        return tr("Aerora Services");
    }

    return QString();
}

QString AeroraVehicleComponent::description() const
{
    // Each description states that the page reports rather than configures, because that is the
    // property an operator cannot infer from a page that looks like every other setup page.
    switch (_descriptor.page) {
    case Page::Identity:
        return tr("Shows this vehicle's MAVLink identity, its Aerora fleet identity when one has "
                  "been reported, and the resolved product profile. Read-only.");
    case Page::Capabilities:
        return tr("Shows the Aerora capabilities this vehicle has reported, and distinguishes a "
                  "capability reported absent from one that was never reported. Read-only.");
    case Page::SensorHealth:
        return tr("Shows reported sensor and health state, and shows Unknown where nothing has "
                  "been reported. Read-only.");
    case Page::Calibration:
        return tr("Aerora publishes no calibration page.");
    case Page::ServiceDiagnostics:
        return tr("Shows which Aerora services attached to this vehicle and which did not. "
                  "Read-only.");
    }

    return QString();
}

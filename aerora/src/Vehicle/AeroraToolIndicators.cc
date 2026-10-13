#include "AeroraToolIndicators.h"

#include <QtCore/QUrl>

#include "AeroraVehicleComponent.h"

QList<AeroraToolIndicators::IndicatorDescriptor> AeroraToolIndicators::declaredIndicators()
{
    return {};
}

bool AeroraToolIndicators::isPublishable(const IndicatorDescriptor &descriptor,
                                         const AeroraVehicleCapabilities *capabilities)
{
    // resolvedQmlUrl() is shared with the setup-page gate, so "resource path to loadable URL" has
    // one definition and an absent resource yields an empty URL in both.
    if (AeroraVehicleComponent::resolvedQmlUrl(descriptor.qmlResource).isEmpty()) {
        return false;
    }

    if (descriptor.capabilityKey.isEmpty()) {
        return true;
    }

    // A null inventory is not permission, and an attached-but-silent one is not evidence.
    // isAvailable() is true only for State::Available, so both State::NotEvidenced and
    // State::Unavailable withhold the indicator.
    if (!capabilities) {
        return false;
    }

    return capabilities->isAvailable(descriptor.capabilityCategory, descriptor.capabilityKey);
}

QVariantList AeroraToolIndicators::urlsFor(const QList<IndicatorDescriptor> &descriptors,
                                           const AeroraVehicleCapabilities *capabilities)
{
    QVariantList urls;

    for (const IndicatorDescriptor &descriptor : descriptors) {
        if (!isPublishable(descriptor, capabilities)) {
            continue;
        }

        urls.append(QVariant::fromValue(AeroraVehicleComponent::resolvedQmlUrl(descriptor.qmlResource)));
    }

    return urls;
}

QVariantList AeroraToolIndicators::urlsFor(const AeroraVehicleCapabilities *capabilities)
{
    return urlsFor(declaredIndicators(), capabilities);
}

QStringList AeroraToolIndicators::unresolvedIndicatorResources()
{
    QStringList unresolved;

    for (const IndicatorDescriptor &descriptor : declaredIndicators()) {
        if (AeroraVehicleComponent::resolvedQmlUrl(descriptor.qmlResource).isEmpty()) {
            unresolved.append(descriptor.qmlResource);
        }
    }

    return unresolved;
}

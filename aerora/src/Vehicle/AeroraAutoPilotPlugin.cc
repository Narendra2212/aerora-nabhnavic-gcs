#include "AeroraAutoPilotPlugin.h"

#include "AeroraVehicleComponent.h"
#include "AeroraVehicleLog.h"
#include "JoystickComponent.h"

AeroraAutoPilotPlugin::AeroraAutoPilotPlugin(Vehicle *vehicle, QObject *parent)
    : AutoPilotPlugin(vehicle, parent)
{
}

AeroraAutoPilotPlugin::~AeroraAutoPilotPlugin() = default;

const QVariantList &AeroraAutoPilotPlugin::vehicleComponents()
{
    if (_componentsBuilt) {
        return _components;
    }

    _componentsBuilt = true;

    const QList<AeroraVehicleComponent::PageDescriptor> descriptors = AeroraVehicleComponent::pageDescriptors();

    int publishedPages = 0;
    for (const AeroraVehicleComponent::PageDescriptor &descriptor : descriptors) {
        if (!AeroraVehicleComponent::isPublishable(descriptor)) {
            continue;
        }

        auto *const component = new AeroraVehicleComponent(descriptor, _vehicle, this, this);
        component->setupTriggerSignals();
        _components.append(QVariant::fromValue(static_cast<VehicleComponent *>(component)));
        publishedPages++;
    }

    // Reproduced from GenericAutoPilotPlugin, which is what this plugin displaces. Omitting it
    // would remove joystick setup from every Aerora vehicle - see the class comment.
    auto *const joystickComponent = new JoystickComponent(_vehicle, this, this);
    joystickComponent->setupTriggerSignals();
    _components.append(QVariant::fromValue(static_cast<VehicleComponent *>(joystickComponent)));

    // Reported once, at build time, because a page that is declared and absent is otherwise
    // indistinguishable from a page that was never declared. An empty Aerora page set is the
    // expected state until the Aerora setup QML exists; it is not a wiring failure.
    const QStringList unresolved = AeroraVehicleComponent::unresolvedPageResources();
    if (!unresolved.isEmpty()) {
        qCDebug(AeroraVehicleLog) << "Aerora setup pages withheld, QML resource absent"
                                  << "published:" << publishedPages
                                  << "declared:" << descriptors.count()
                                  << "missing:" << unresolved;
    }

    return _components;
}

QString AeroraAutoPilotPlugin::prerequisiteSetup(VehicleComponent *component) const
{
    Q_UNUSED(component)

    return QString();
}

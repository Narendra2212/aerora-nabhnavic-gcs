#include "AeroraQmlOverrides.h"

#include <QtCore/QFile>
#include <QtQml/QQmlAbstractUrlInterceptor>
#include <QtQml/QQmlApplicationEngine>

#include "AeroraUiLog.h"

namespace {

/// Routes QML resource URL requests through AeroraQmlOverrides::interceptedUrlFor().
///
/// Deliberately thin: every decision is in the pure static function, so the rule is testable
/// without constructing an engine. Only `QmlFile` and `UrlString` are considered, matching the
/// reference overlay — the other data types address type information rather than loadable content.
class AeroraOverrideInterceptor final : public QQmlAbstractUrlInterceptor
{
public:
    AeroraOverrideInterceptor() = default;

    QUrl intercept(const QUrl &url, QQmlAbstractUrlInterceptor::DataType type) override
    {
        switch (type) {
        case QQmlAbstractUrlInterceptor::QmlFile:
        case QQmlAbstractUrlInterceptor::UrlString:
            return AeroraQmlOverrides::interceptedUrlFor(url);
        default:
            return url;
        }
    }
};

/// The installed interceptor and the engine it belongs to.
///
/// Main-thread only: a `QQmlApplicationEngine` is created on the main thread from
/// `QGCApplication::_initQmlRootWindow()` and destroyed there, so no synchronisation is needed and
/// none is implied.
AeroraOverrideInterceptor *s_interceptor = nullptr;
QQmlApplicationEngine *s_engine = nullptr;

} // namespace

QList<AeroraQmlOverrides::OverrideEntry> AeroraQmlOverrides::overrideEntries()
{
    // ------------------------------------------------------------------------------------------
    // EMPTY BY DESIGN. Aerora overrides no upstream QML file in this build.
    //
    // Adding an entry is a deliberate, reviewable act. The three upstream purpose-built seams are
    // listed below as the only pre-approved candidates, commented out because no Aerora content
    // exists to put behind them:
    //
    //   {QStringLiteral("/qml/QGroundControl/FlyView/FlyViewCustomLayer.qml"),
    //       Tier::PurposeBuiltSeam},               instantiated at src/FlyView/FlyView.qml:145
    //   {QStringLiteral("/qml/QGroundControl/FlyView/CustomGuidedActionsController.qml"),
    //       Tier::PurposeBuiltSeam},               instantiated at GuidedActionsController.qml:187
    //   {QStringLiteral("/qml/QGroundControl/FlyView/FlyViewAdditionalCustomActionsList.qml"),
    //       Tier::PurposeBuiltSeam},               FlyViewAdditionalActionsButton.qml:28
    //
    // The guided-actions and additional-actions seams must stay unused until an Aerora action has
    // an authorized command path: both exist to introduce operator actions, and no Aerora action
    // can be executed in this build.
    //
    // A Tier::PopulatedFile entry additionally requires a recorded reason, a fork-point commit and
    // a named owner before it is added (IF-UIS-012, AERORA-UI-REQ-005, Q-038).
    // ------------------------------------------------------------------------------------------
    return {};
}

QStringList AeroraQmlOverrides::overridePaths()
{
    QStringList paths;
    const QList<OverrideEntry> entries = overrideEntries();
    paths.reserve(entries.count());

    for (const OverrideEntry &entry : entries) {
        paths.append(entry.stockPath);
    }

    return paths;
}

int AeroraQmlOverrides::populatedFileOverrideCount()
{
    int count = 0;

    for (const OverrideEntry &entry : overrideEntries()) {
        if (entry.tier == Tier::PopulatedFile) {
            ++count;
        }
    }

    return count;
}

QStringList AeroraQmlOverrides::forbiddenPaths()
{
    // CustomMapItems.qml reads as a fourth purpose-built seam and is not one. Upstream populates it
    // with an Instantiator over QGCCorePlugin::customMapItems() (src/API/QGCCorePlugin.h:158) and
    // instantiates it at src/FlyView/FlyViewMap.qml:247. Overriding it would fork a populated file
    // *and* discard the C++ seam that is the supported way to add a map item.
    return {QStringLiteral("/qml/QGroundControl/FlightMap/MapItems/CustomMapItems.qml")};
}

bool AeroraQmlOverrides::isOverrideCandidate(const QString &stockPath)
{
    if (stockPath.isEmpty()) {
        return false;
    }

    // The allow-list is checked first and the common answer is a silent no. Every qrc URL the
    // engine loads reaches here, so warning about undeclared paths - or about the forbidden path,
    // which upstream legitimately loads on every fly view - would make normal operation noisy and
    // would fail unrelated tests under strict log checking.
    if (!overridePaths().contains(stockPath)) {
        return false;
    }

    if (forbiddenPaths().contains(stockPath)) {
        qCWarning(AeroraUiLog) << "declared override is on the forbidden list and is refused."
                               << "stockPath:" << stockPath
                               << "This is a build configuration defect, not a runtime condition.";
        return false;
    }

    return true;
}

QString AeroraQmlOverrides::mirroredResourcePath(const QString &stockPath)
{
    if (stockPath.isEmpty()) {
        return {};
    }

    return QLatin1String(kOverridePrefix) + stockPath;
}

QUrl AeroraQmlOverrides::interceptedUrlFor(const QUrl &url)
{
    if (url.scheme() != QLatin1String(kResourceScheme)) {
        return url;
    }

    const QString stockPath = url.path();
    if (!isOverrideCandidate(stockPath)) {
        return url;
    }

    const QString mirrored = mirroredResourcePath(stockPath);
    if (!QFile::exists(mirrored)) {
        // Declared but absent. The override silently did not take effect, which is exactly what
        // verify() exists to surface; warn here too so the condition is visible at load time
        // rather than only at startup.
        qCWarning(AeroraUiLog) << "declared override has no mirrored resource. stockPath:" << stockPath
                               << "expected:" << mirrored;
        return url;
    }

    QUrl result;
    result.setScheme(QLatin1String(kResourceScheme));
    result.setPath(mirrored.mid(1));

    qCDebug(AeroraUiLog) << "override resolved. stockPath:" << stockPath
                         << "to:" << result.toString();

    return result;
}

AeroraQmlOverrides::SelfCheckResult AeroraQmlOverrides::verify()
{
    SelfCheckResult result;

    for (const QString &stockPath : overridePaths()) {
        if (!QFile::exists(mirroredResourcePath(stockPath))) {
            result.missingMirroredResources.append(stockPath);
        }
    }

    result.passed = result.missingMirroredResources.isEmpty();

    return result;
}

void AeroraQmlOverrides::installOn(QQmlApplicationEngine *engine)
{
    if (!engine) {
        qCWarning(AeroraUiLog) << "cannot install the override interceptor on a null engine";
        return;
    }

    if (s_interceptor) {
        qCWarning(AeroraUiLog) << "override interceptor is already installed; refusing to install a second."
                               << "installedEngine:" << static_cast<void *>(s_engine)
                               << "requestedEngine:" << static_cast<void *>(engine);
        return;
    }

    s_interceptor = new AeroraOverrideInterceptor();
    s_engine = engine;
    engine->addUrlInterceptor(s_interceptor);

    qCDebug(AeroraUiLog) << "override interceptor installed. declaredOverrides:" << overridePaths().count()
                         << "populatedFileOverrides:" << populatedFileOverrideCount();
}

void AeroraQmlOverrides::releaseFrom(QQmlApplicationEngine *engine)
{
    if (!engine || (engine != s_engine) || !s_interceptor) {
        return;
    }

    engine->removeUrlInterceptor(s_interceptor);
    delete s_interceptor;
    s_interceptor = nullptr;
    s_engine = nullptr;

    qCDebug(AeroraUiLog) << "override interceptor released";
}

bool AeroraQmlOverrides::isInstalled()
{
    return s_interceptor != nullptr;
}

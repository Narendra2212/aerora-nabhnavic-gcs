#include "AeroraScreenMetricsTest.h"

#include <cstdio>
#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtCore/QVariant>
#include <QtCore/QtNumeric>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlComponent>

#include "QGCCorePlugin.h"

UT_REGISTER_TEST(AeroraScreenMetricsTest, TestLabel::Unit)

namespace {

/// One metric to probe, and what counts as usable for it.
struct Metric
{
    const char* name;
    /// Zero is a real possibility for some metrics and benign; for a size it is a broken layout.
    bool requirePositive;
};

/// The metrics every Aerora control's geometry is a multiple of, plus the two screen inputs they
/// are derived from.
///
/// `minTouchPixels` may legitimately be zero - ScreenTools only sets it from the physical pixel
/// density, and a density of zero yields zero without breaking a size - so it is required to be
/// finite rather than positive. Everything else multiplies into a width or a height.
const QList<Metric>& probedMetrics()
{
    static const QList<Metric> metrics = {
        {"screenWidth", true},           {"screenHeight", true},
        {"pixelDensity", true},          {"defaultFontPixelHeight", true},
        {"defaultFontPixelWidth", true}, {"toolbarHeight", true},
        {"implicitButtonHeight", true},  {"implicitButtonWidth", true},
        {"minTouchPixels", false},
    };

    return metrics;
}

/// A document rather than context properties: `Screen` is an attached property and `ScreenTools` a
/// QML singleton, so neither can be handed to a QQmlExpression. Reading them through a real
/// document is also the way every Aerora control reads them.
constexpr const char* kProbeQml = R"QML(
import QtQuick
import QtQuick.Window

import QGroundControl
import QGroundControl.Controls

Item {
    readonly property real screenWidth: Screen.width
    readonly property real screenHeight: Screen.height
    readonly property real pixelDensity: Screen.pixelDensity

    readonly property real defaultFontPixelHeight: ScreenTools.defaultFontPixelHeight
    readonly property real defaultFontPixelWidth: ScreenTools.defaultFontPixelWidth
    readonly property real toolbarHeight: ScreenTools.toolbarHeight
    readonly property real implicitButtonHeight: ScreenTools.implicitButtonHeight
    readonly property real implicitButtonWidth: ScreenTools.implicitButtonWidth
    readonly property real minTouchPixels: ScreenTools.minTouchPixels

    // Reported, not asserted. Both are derived by dividing by a screen metric, so a zero
    // dimension flips them - and isShortScreen in particular suppresses the description on every
    // SetupPage, which would be a silent layout change rather than a failure.
    readonly property bool isTinyScreen: ScreenTools.isTinyScreen
    readonly property bool isShortScreen: ScreenTools.isShortScreen
}
)QML";

}  // namespace

void AeroraScreenMetricsTest::_screenAndFontMetricsAreUsableNumbers()
{
    // The production factory: it is what adds the qrc:/qml import path the QGroundControl.Controls
    // module resolves from. No window is loaded - see the \section nonfatal note on the class.
    QQmlApplicationEngine* const engine = QGCCorePlugin::instance()->createQmlApplicationEngine(this);
    QVERIFY(engine);

    QStringList failures;
    QStringList readings;

    {
        QQmlComponent component(engine);
        component.setData(QByteArray(kProbeQml), QUrl(QStringLiteral("qrc:/AeroraScreenMetricsProbe.qml")));

        if (component.isError()) {
            failures.append(QStringLiteral("probe document did not compile: %1").arg(component.errorString()));
        } else {
            const std::unique_ptr<QObject> probe(component.create());
            if (!probe) {
                failures.append(QStringLiteral("probe document did not instantiate: %1").arg(component.errorString()));
            } else {
                for (const Metric& metric : probedMetrics()) {
                    const QVariant value = probe->property(metric.name);
                    if (!value.isValid()) {
                        failures.append(QStringLiteral("%1 does not resolve").arg(QString::fromUtf8(metric.name)));
                        continue;
                    }

                    const double reading = value.toDouble();
                    readings.append(QStringLiteral("%1=%2").arg(QString::fromUtf8(metric.name)).arg(reading));

                    // Infinity and NaN are the two shapes a divide-by-zero screen metric takes,
                    // and a NaN comparison is false, so ScreenTools' own fallback branch does not
                    // catch it. This is the check that names the value instead.
                    if (!qIsFinite(reading)) {
                        failures.append(
                            QStringLiteral("%1 is not finite (%2)").arg(QString::fromUtf8(metric.name)).arg(reading));
                        continue;
                    }

                    if (metric.requirePositive && (reading <= 0.0)) {
                        failures.append(QStringLiteral("%1 must be positive, got %2")
                                            .arg(QString::fromUtf8(metric.name))
                                            .arg(reading));
                    }
                }

                // Carried into the failure message rather than asserted: these are layout
                // consequences of the metrics above, and on a platform whose screen is synthesised
                // they are the fastest way to see that the synthesis is wrong.
                readings.append(QStringLiteral("isTinyScreen=%1").arg(probe->property("isTinyScreen").toBool()));
                readings.append(QStringLiteral("isShortScreen=%1").arg(probe->property("isShortScreen").toBool()));
            }
        }
    }

    // Released before asserting, so a failure cannot leave the URL interceptor installed for the
    // rest of the run - it is process-wide file-static state and a second install is refused.
    QGCCorePlugin::instance()->destroyQmlApplicationEngine(engine);

    // Emitted on success as well as failure, which is the whole point: a passing run previously
    // established only that the metrics were not degenerate, not what they actually were - and the
    // numbers are the diagnostic. Written with fprintf rather than a Qt logging macro on purpose:
    // Qt messages go through the strict-mode check and an unexpected one fails the test, while raw
    // stderr does not. CTest's --output-junit captures <system-out> for every test, pass or fail,
    // so this reaches the uploaded JUnit artifact. Kept to one short line because that capture is
    // truncated at 1024 bytes.
    fprintf(stderr, "AeroraScreenMetrics: %s\n", qPrintable(readings.join(QStringLiteral(" "))));
    fflush(stderr);

    QVERIFY2(failures.isEmpty(),
             qPrintable(QStringLiteral("degenerate UI metrics: %1 | all readings: %2")
                            .arg(failures.join(QStringLiteral("; ")), readings.join(QStringLiteral(" ")))));
}

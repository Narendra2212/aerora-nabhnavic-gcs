#pragma once

#include "UnitTest.h"

/// Probes the screen-derived metrics every Aerora surface is sized from, and is the diagnostic for
/// the Windows QML scene-test abort.
///
/// \section why Why this exists
///
/// `QmlUITestBase::startUI()` aborts partway through the offscreen scene boot on Windows:
///
/// \code
/// ASSERT: "value < maximalPlusOne" in qnumeric.h, line 523   ->   exit 0xc0000409
/// \endcode
///
/// `qnumeric.h` is `qt_saturate`, Qt's checked narrowing cast, so something converted a `qreal` to
/// an integer that could not hold it - an infinity, a NaN, or a value past `INT_MAX`. That happens
/// after the window is created and sized (the abort follows two `propagateSizeHints()` warnings),
/// which is where Qt turns computed item sizes into platform geometry.
///
/// The first hypothesis was an empty font database, and it was wrong: `QT_QPA_FONTDIR` removed the
/// `QFontDatabase` warning and the abort was unchanged (run `39215341862`). The remaining source of
/// a degenerate size is the other input `ScreenTools` divides by - the screen itself:
///
/// \code
/// ScreenTools.qml:162   minTouchPixels = Math.round(minTouchMillimeters * realPixelDensity)
/// ScreenTools.qml:163   if (minTouchPixels / Screen.height > 0.15) { ... }
/// ScreenTools.qml:97    isTinyScreen:  (Screen.width / realPixelDensity) < 120
/// ScreenTools.qml:98    isShortScreen: ((Screen.height / realPixelDensity) < 120) || ...
/// \endcode
///
/// `realPixelDensity` is `Screen.pixelDensity` on desktop. A zero screen dimension or pixel density
/// turns each of those into `Infinity` or `NaN`, and a `NaN` comparison is false, so the fallback
/// branch that exists for exactly this case does not run.
///
/// \section nonfatal Why this does not boot a window
///
/// Deliberately. A test that reproduced the abort would abort the process and take the Windows CI
/// leg with it, and an abort tells us only what we already know. This reads the inputs instead, on
/// an engine with no window, so it reports a degenerate metric as a test failure naming the value -
/// which is the same information without the crash.
///
/// Every metric is read in one slot and every failure is collected before reporting, so one CI run
/// names every degenerate value rather than only the first.
///
/// \section keep Why it is worth keeping once the abort is fixed
///
/// A user interface sized from a non-finite metric is a defect class on any platform, not a Windows
/// curiosity. Nothing else in the suite asserts that the numbers every Aerora control multiplies
/// are real numbers.
class AeroraScreenMetricsTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _screenAndFontMetricsAreUsableNumbers();
};

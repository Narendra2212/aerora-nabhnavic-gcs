#pragma once

#include "UnitTest.h"

/// Establishes whether *any* headless Qt platform plugin can boot the production QML scene on this
/// host, and names the exit status of each one that cannot.
///
/// \section why The question this answers
///
/// `QmlUITestBase::startUI()` aborts partway through the offscreen scene boot on Windows
/// (`ASSERT: "value < maximalPlusOne" in qnumeric.h`, exit `0xc0000409`), which is why
/// `AeroraSceneAttachmentUITest`, `AeroraVehicleConfigUITest` and `AeroraFlyViewOverlayUITest` are
/// registered off Windows. Two hypotheses have been tested and disproved: an empty font database
/// (`QT_QPA_FONTDIR` removed the warning, abort unchanged) and degenerate screen or font metrics
/// (`AeroraScreenMetricsTest` passes on Windows). The platform plugin's own window-geometry
/// handling is what remains, and `minimal` is the other headless plugin Qt ships.
///
/// \section child Why it runs the scene in a child process
///
/// Because the failure is an `abort()`. A test that booted the scene in-process would take the
/// whole test binary with it, so the CI leg would report one crash and no information - and it
/// would do that for every platform tried, including the one that might work.
///
/// Running each candidate as a child of the same executable makes the abort *data*: the parent
/// survives, records the exit status, and can try the next plugin. The child is this same binary
/// invoked the way CTest invokes it, so the scene it boots is the real one.
///
/// \section assertion What it asserts, and why that is the honest assertion
///
/// That at least one headless platform boots the scene. On Linux `offscreen` does, which is what
/// keeps this probe from being a tautology - it proves the mechanism works where the answer is
/// already known. On Windows the result is the finding: if `minimal` boots the scene the three
/// suites can be registered there, and if neither does, the failure message carries both exit
/// statuses and the child's last output, which is the bounded diagnostic this is for.
///
/// It is deliberately *not* asserted per platform. "offscreen aborts on Windows" is already known
/// and recorded; asserting it would turn a known gap into a permanently red test that tells nobody
/// anything new.
class AeroraSceneHarnessProbeTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _someHeadlessPlatformCanBootTheProductionScene();
};

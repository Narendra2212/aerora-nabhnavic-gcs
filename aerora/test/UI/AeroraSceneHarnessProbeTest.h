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
/// \section assertion What it asserts, and what it only reports
///
/// It asserts that it **measured**: every candidate started and terminated within its timeout. A
/// candidate that never started, or that hung, yields no reading, and a probe that reports nothing
/// is broken rather than informative.
///
/// Boot success is reported to stderr per candidate, not asserted. On Linux `offscreen` boots the
/// scene, so asserting it would pass and say nothing. On Windows neither plugin does - `offscreen`
/// aborts during construction, `minimal` constructs a scene it never lays out - and both are
/// already recorded at the registration site in `aerora/CMakeLists.txt`, so asserting it there
/// would be permanently red while restating a known gap.
///
/// Nothing is concealed by that: the readings are emitted every run, the three scene suites stay
/// unregistered on Windows, and none is reported as passing. When a plugin finally produces a
/// laid-out scene, this output is what will say so.
class AeroraSceneHarnessProbeTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _someHeadlessPlatformCanBootTheProductionScene();
};

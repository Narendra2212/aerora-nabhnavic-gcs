#pragma once

#include "QmlUITestBase.h"

/// Boots the production QML scene and tears it down. Nothing else.
///
/// \section payload This is an instrument, not a suite
///
/// It exists to be the payload `AeroraSceneHarnessProbeTest` runs in a child process, and it is
/// deliberately **not** registered with `add_qgc_test()` - no CTest entry, so no CI leg runs it
/// directly. `UT_REGISTER_TEST` is still required, because that is what makes it reachable as
/// `--unittest:AeroraSceneBootOnlyTest`.
///
/// The probe first pointed at `AeroraSceneAttachmentUITest` and that was a mistake: a child running
/// a real suite exits non-zero when any of its *assertions* fails, which is indistinguishable from
/// the scene having failed to boot. The first probe run proved the point - it reported `offscreen`
/// as failing on Linux, where offscreen demonstrably boots the scene, because an unrelated slot in
/// that suite was broken.
///
/// So the payload asserts only what the probe is measuring. Its exit code means "the scene booted"
/// or "it did not", and nothing else can move it.
class AeroraSceneBootOnlyTest : public QmlUITestBase
{
    Q_OBJECT

private slots:
    void _theProductionSceneBoots();
};

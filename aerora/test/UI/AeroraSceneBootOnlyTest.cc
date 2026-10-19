#include "AeroraSceneBootOnlyTest.h"

#include <QtQuick/QQuickItem>
#include <QtTest/QTest>

UT_REGISTER_TEST(AeroraSceneBootOnlyTest, TestLabel::Integration)

void AeroraSceneBootOnlyTest::_theProductionSceneBoots()
{
    // No log suppressions beyond what QmlUITestBase::startUI() installs itself. Adding any here
    // would risk this exiting zero on a host that logged something unexpected during the boot,
    // which is exactly the signal the probe reads this exit code for.
    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    // The three things that have to be true for a scene to be usable by any UI test. Anything
    // beyond this belongs in a real suite: a false negative here would be read as "this platform
    // cannot boot the scene" and would withdraw a platform that works.
    QVERIFY(_window);
    QVERIFY(_rootItem);
    QVERIFY2((_rootItem->width() > 0) && (_rootItem->height() > 0), "the scene booted but its root item has no size");

    stopUI();
}

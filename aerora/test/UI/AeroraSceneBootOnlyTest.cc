#include "AeroraSceneBootOnlyTest.h"

#include <QtCore/QRegularExpression>
#include <QtQuick/QQuickItem>
#include <QtTest/QTest>

UT_REGISTER_TEST(AeroraSceneBootOnlyTest, TestLabel::Integration)

void AeroraSceneBootOnlyTest::init()
{
    QmlUITestBase::init();

    // The same three suppressions every Aerora scene suite carries, and for the same reason:
    // AeroraCorePlugin::init() emits them once per process and latches them, and
    // QmlUITestBase::startUI() calls that init(). Without these the first probe run on Linux
    // exited 255 on both platforms - strict-mode log failures, not boot failures - which is
    // precisely the ambiguity this payload exists to remove.
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("no platform key protector available")));
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("command authorization is NOT being enforced")));
    ignoreLogMessage("Aerora.CorePlugin", QtWarningMsg,
                     QRegularExpression(QStringLiteral("allowing commands without authorization")));
}

void AeroraSceneBootOnlyTest::_theProductionSceneBoots()
{
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

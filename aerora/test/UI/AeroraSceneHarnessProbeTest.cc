#include "AeroraSceneHarnessProbeTest.h"

#include <cstdio>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

UT_REGISTER_TEST(AeroraSceneHarnessProbeTest, TestLabel::Integration)

namespace {

/// The payload the probe boots: a suite that boots the scene and asserts nothing else.
///
/// Pointing this at a real suite was the first version and it was wrong. A child running a real
/// suite exits non-zero when any of its assertions fails, which is indistinguishable from the scene
/// failing to boot - and the first probe run demonstrated it, reporting `offscreen` as failing on
/// Linux, where offscreen boots the scene perfectly well, because an unrelated slot in that suite
/// was broken at the time. The exit code has to mean one thing.
constexpr const char* kProbedSuite = "AeroraSceneBootOnlyTest";

/// The headless plugins Qt ships, in the order worth trying: the one the harness uses today, then
/// the alternative.
const QStringList& candidatePlatforms()
{
    static const QStringList platforms = {QStringLiteral("offscreen"), QStringLiteral("minimal")};
    return platforms;
}

/// What one candidate did.
struct ProbeResult
{
    QString platform;
    bool started = false;
    bool finished = false;
    int exitCode = -1;
    QProcess::ExitStatus exitStatus = QProcess::CrashExit;
    QString tail;

    [[nodiscard]] bool booted() const
    {
        return started && finished && (exitStatus == QProcess::NormalExit) && (exitCode == 0);
    }

    [[nodiscard]] QString summary() const
    {
        if (!started) {
            return QStringLiteral("%1=did-not-start").arg(platform);
        }
        if (!finished) {
            return QStringLiteral("%1=timed-out").arg(platform);
        }
        // Rendered unsigned as well as signed: the Windows abort surfaces as 0xc0000409, which a
        // signed int prints as a large negative number that looks like a different failure.
        return QStringLiteral("%1=exit:%2(0x%3)%4")
            .arg(platform)
            .arg(exitCode)
            .arg(static_cast<uint>(exitCode), 0, 16)
            .arg(exitStatus == QProcess::NormalExit ? QString() : QStringLiteral(" crashed"));
    }
};

/// Boots @a suite in a child process under @a platform and reports what happened.
///
/// The child is this same executable, invoked as CTest invokes it (`cmake/QGCTest.cmake:179`), so
/// the scene is the production one rather than a reconstruction.
ProbeResult probePlatform(const QString& platform, const QString& suite)
{
    ProbeResult result;
    result.platform = platform;

    // Its own settings and temp location, so the child cannot race the parent's settings file.
    // --allow-multiple is what lets a second instance run at all.
    QTemporaryDir childTmp;
    if (!childTmp.isValid()) {
        return result;
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), platform);
    env.insert(QStringLiteral("QT_QUICK_BACKEND"), QStringLiteral("software"));
    env.insert(QStringLiteral("LIBGL_ALWAYS_SOFTWARE"), QStringLiteral("1"));
    env.insert(QStringLiteral("QT_LOGGING_RULES"), QStringLiteral("*.debug=false"));
    // Without this a failing child on Windows reports nothing: QGroundControl links as a GUI
    // application, so a child with a piped stdout routes Qt output to OutputDebugStringA instead.
    env.insert(QStringLiteral("QT_FORCE_STDERR_LOGGING"), QStringLiteral("1"));
    env.insert(QStringLiteral("TMPDIR"), childTmp.path());
    env.insert(QStringLiteral("TMP"), childTmp.path());
    env.insert(QStringLiteral("TEMP"), childTmp.path());

    QProcess child;
    child.setProcessEnvironment(env);
    child.setProcessChannelMode(QProcess::MergedChannels);
    child.setProgram(QCoreApplication::applicationFilePath());
    child.setArguments({QStringLiteral("--unittest:%1").arg(suite), QStringLiteral("--allow-multiple")});

    child.start();
    result.started = child.waitForStarted(30000);
    if (!result.started) {
        return result;
    }

    // Generous: the Windows abort took ~13.6s to arrive and a healthy boot takes ~4s, so this is
    // sized to tell a slow boot apart from a hang rather than to be tight.
    result.finished = child.waitForFinished(180000);
    if (!result.finished) {
        child.kill();
        (void) child.waitForFinished(5000);
        return result;
    }

    result.exitCode = child.exitCode();
    result.exitStatus = child.exitStatus();

    // The tail rather than the whole log: the assertion message has to stay readable, and the last
    // lines are where the abort and the failing operation are.
    const QString output = QString::fromLocal8Bit(child.readAll());
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    result.tail = lines.mid(qMax(0, lines.count() - 6)).join(QStringLiteral(" | "));

    return result;
}

}  // namespace

void AeroraSceneHarnessProbeTest::_someHeadlessPlatformCanBootTheProductionScene()
{
    QStringList summaries;
    QStringList tails;
    bool anyBooted = false;

    for (const QString& platform : candidatePlatforms()) {
        const ProbeResult result = probePlatform(platform, QString::fromUtf8(kProbedSuite));

        summaries.append(result.summary());
        if (!result.booted() && !result.tail.isEmpty()) {
            tails.append(QStringLiteral("[%1] %2").arg(result.platform, result.tail));
        }
        if (result.booted()) {
            anyBooted = true;
        }

        // Emitted per candidate, and with fprintf rather than a Qt logging macro so the strict-mode
        // log check does not see it. CTest's --output-junit captures <system-out> whether the test
        // passes or fails, so a run that succeeds still reports which plugin worked - which is the
        // finding on the platform where the answer was unknown.
        fprintf(stderr, "AeroraSceneHarnessProbe: %s\n", qPrintable(result.summary()));
        fflush(stderr);
    }

    QVERIFY2(anyBooted,
             qPrintable(QStringLiteral("no headless platform booted the production scene. results: %1 || child "
                                       "output: %2")
                            .arg(summaries.join(QStringLiteral(" ")), tails.join(QStringLiteral(" ;; ")))));
}

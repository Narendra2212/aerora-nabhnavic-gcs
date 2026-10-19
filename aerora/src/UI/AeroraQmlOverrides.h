#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

class QQmlApplicationEngine;

/// \brief The Aerora QML override seam: an **allow-listed** URL interceptor and its self-check.
///
/// This is the closure of Q7-003. Without it an Aerora QML file added to the overlay is compiled
/// into a resource and never loaded, with no error of any kind — the silent failure R-019 and
/// `IF-UIS-014` describe.
///
/// \section seam The seam, verified in source
///
/// \code
/// QGCApplication::_initQmlRootWindow()                        src/QGCApplication.cc:268
///   └── QGCCorePlugin::createQmlApplicationEngine(this)       src/QGCApplication.cc:275
///         virtual, public                                     src/API/QGCCorePlugin.h:117
///         base impl                                           src/API/QGCCorePlugin.cc:300-307
///
/// QGCApplication shutdown                                     src/QGCApplication.cc:705-710
///   └── QGCCorePlugin::destroyQmlApplicationEngine(engine)
///         virtual, public                                     src/API/QGCCorePlugin.h:122
///         base impl                                           src/API/QGCCorePlugin.cc:308-311
/// \endcode
///
/// Both hooks are public virtuals on a class `AeroraCorePlugin` already derives from, so the whole
/// mechanism lives inside the overlay and **no QGroundControl core edit is required**.
///
/// \section allowlist Why an allow-list rather than existence
///
/// QGroundControl's reference overlay keys interception on `QFile::exists(":/Custom" + path)`
/// (`custom-example/src/CustomPlugin.cc:273-295`). Any resource that happens to be mirrored wins,
/// so the set of overridden files is implicit and unbounded. Two consequences Aerora does not
/// accept:
///
/// \li a resource mirrored by accident silently replaces upstream behaviour, and
/// \li the `IF-UIS-015` assertion "no stock URL is intercepted that Aerora did not intend to
///     override" cannot be written, because there is no declaration of intent to compare against.
///
/// Here the declaration of intent **is** the mechanism: \ref overrideEntries is the complete,
/// compile-time list, and a path outside it is never rewritten however many resources exist. That
/// makes "no unintended interception" a property of a data structure rather than of a review.
///
/// \section declared What is declared in this build
///
/// Exactly one entry: `FlyViewCustomLayer.qml`, the purpose-built Fly View overlay seam, carrying
/// `SCR-LIV-01`. Every other URL passes through unchanged. The populated-file count is **zero** —
/// no upstream file with content is forked — rather than the figure the Phase 2 inventory
/// anticipated.
///
/// \section safety What this must never do
///
/// It rewrites QML resource URLs and nothing else. It holds no vehicle, sends no message, reads no
/// Fact, and has no path to MAVLink, the mission manager, the Fact system, the authorization gate
/// or any command. A URL outside the allow-list is returned **identically**, so a bug here can lose
/// Aerora UI but cannot alter aircraft behaviour.
class AeroraQmlOverrides
{
public:
    /// Which upstream file an override replaces, and therefore what obligation it carries.
    enum class Tier {
        /// Upstream provides the file as an empty, purpose-built seam and documents it as an
        /// override point. Low cost: upstream has little reason to change it.
        PurposeBuiltSeam,
        /// Upstream file is populated. **A silent fork** — upstream bug fixes to that file are
        /// discarded with no warning, no lint error and no merge conflict. Each entry needs a
        /// recorded reason, a fork-point commit and a named owner (`IF-UIS-012`, Q-038).
        PopulatedFile,
    };

    /// One declared override.
    struct OverrideEntry {
        /// The stock resource path as the QML engine requests it, without the `qrc:` scheme —
        /// for example `/qml/QGroundControl/FlyView/FlyViewCustomLayer.qml`.
        QString stockPath;
        Tier tier = Tier::PurposeBuiltSeam;
    };

    /// Resource prefix the overlay's mirrored copies are registered under, matching the reference
    /// overlay's `/Custom` convention so the mirrored path is a mechanical transform of the stock
    /// path rather than a per-file mapping.
    static constexpr const char *kOverridePrefix = ":/Aerora";

    /// Scheme the QML engine uses for resource URLs. The only scheme this class acts on.
    static constexpr const char *kResourceScheme = "qrc";

    /// The complete set of declared overrides. **One purpose-built seam in this build** — see
    /// \ref declared.
    [[nodiscard]] static QList<OverrideEntry> overrideEntries();

    /// Stock paths of every declared override, for the startup self-check and for tests.
    [[nodiscard]] static QStringList overridePaths();

    /// How many declared overrides replace a **populated** upstream file.
    ///
    /// Zero, and a test asserts it. Each one is a silent fork, so the count is the thing to keep
    /// down; `AERORA-UI-REQ-005` attaches an owner and a fork point to every unit of it.
    [[nodiscard]] static int populatedFileOverrideCount();

    /// Paths this class refuses to override regardless of the allow-list.
    ///
    /// These are upstream files that **look** like override seams and are not. Refusing them here
    /// makes the mistake unrepresentable instead of reviewable.
    [[nodiscard]] static QStringList forbiddenPaths();

    /// Whether @a stockPath is declared as an override and not forbidden.
    ///
    /// Pure: no file system access, no engine, no resource. This is the predicate the security
    /// constraint rests on, so it is separately testable from resource existence.
    [[nodiscard]] static bool isOverrideCandidate(const QString &stockPath);

    /// The overlay resource path mirroring @a stockPath, whether or not it exists.
    [[nodiscard]] static QString mirroredResourcePath(const QString &stockPath);

    /// The URL the engine should load for @a url.
    ///
    /// Returns the overlay URL only when @a url is a resource URL, its path is an override
    /// candidate, **and** the mirrored resource actually exists. Returns @a url unchanged in every
    /// other case, including every URL this class knows nothing about.
    [[nodiscard]] static QUrl interceptedUrlFor(const QUrl &url);

    /// Result of \ref verify.
    struct SelfCheckResult {
        /// Declared overrides whose mirrored resource is absent, so the override silently did not
        /// take effect.
        QStringList missingMirroredResources;
        /// True when every declared override resolves to overlay content.
        bool passed = true;
    };

    /// Asserts that every declared override resolves to Aerora content.
    ///
    /// The `IF-UIS-015` assertion that catches the silent fork: a declared override whose mirrored
    /// resource is absent does not take effect, and nothing else in the build reports that.
    ///
    /// \note This covers QML resources only. A C++ `QFile(":/json/...")` read never passes through
    ///       a QML URL interceptor, so the `:/json` class of resource needs its own assertion and
    ///       is Q4-001.
    [[nodiscard]] static SelfCheckResult verify();

    /// Installs the interceptor on @a engine. Idempotent per engine; refuses a null engine and
    /// warns rather than installing a second interceptor.
    ///
    /// Called from `AeroraCorePlugin::createQmlApplicationEngine()` after the base implementation.
    static void installOn(QQmlApplicationEngine *engine);

    /// Removes and destroys the interceptor installed on @a engine.
    ///
    /// Called from `AeroraCorePlugin::destroyQmlApplicationEngine()` **before** the base
    /// implementation, which deletes the engine. Releasing an engine this class did not install on
    /// is a no-op, so an out-of-order teardown cannot delete another engine's interceptor.
    static void releaseFrom(QQmlApplicationEngine *engine);

    /// True when an interceptor is currently installed. For the startup self-check, so an
    /// interface quietly missing its Aerora layer is detectable rather than merely wrong.
    [[nodiscard]] static bool isInstalled();
};

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtQuick/QQuickItem>

/// \file
/// \brief The read-only ratchet's walk, shared by the two suites that assert it.
///
/// Header-only and in one place because two suites need the same answer about different trees:
/// `AeroraFlyViewOverlayUITest` walks the Live Operations overlay, and `AeroraUiFoundationTest`
/// walks every Aerora screen document. A second copy would be a second definition of what an
/// actionable control is, free to drift from the first, and the assertion that matters is that both
/// suites forbid the same thing.
///
/// \section families What counts as actionable, and why it is asked by class name
///
/// Three Qt base classes, asked through \c QObject::inherits() rather than by matching a type name:
///
/// \li \c QQuickAbstractButton - every Qt Quick Controls button, which is what \c QGCButton and
///     friends derive from;
/// \li \c QQuickMouseArea - a hand-rolled click target;
/// \li \c QQuickPointerHandler - a \c TapHandler, \c DragHandler or \c HoverHandler.
///
/// Asking the base class is what makes the walk catch a control added through *any* route - a new
/// panel, a nested control, a composed control, a \c Loader, or an upstream type that happens to be
/// clickable - rather than only the types this header thought to name. The source-text rule
/// `actionable-control` in `tools/analyzers/gate_guard_check.py` is the other half of the same
/// prohibition: it fires before any build and sees only types spelled out in a document, which is
/// why this walk exists beside it.
///
/// \section union Why the walk is over both trees
///
/// Neither tree subsumes the other, and the pointer-handler family is the reason this header was
/// written. A \c QQuickPointerHandler is **not** a \c QQuickItem: it is a plain \c QObject child of
/// the item it handles, so a \c QQuickItem::childItems() recursion never sees it. A tap handler on a
/// panel would therefore have passed the previous assertion while being exactly what Requirement 5
/// criterion 1 forbids. In the other direction, a visually reparented item keeps its \c QObject
/// parent elsewhere, so a \c QObject::children() walk alone would miss it.
///
/// So the walk is the union, deduplicated by pointer. Deduplication is load-bearing rather than
/// tidy: a control reachable through both trees would otherwise be counted twice, and the exception
/// assertion in `AeroraUiFoundationTest` counts.
namespace AeroraActionableControls {

/// Every object reachable from @a root through the visual tree, the object tree, or both.
///
/// Deduplicated by pointer and returned in discovery order, with @a root itself first. Null is an
/// empty list rather than a crash, so a caller that failed to resolve a root reports an empty walk
/// and the assertion it feeds reports *that* instead of aborting the suite.
inline QList<const QObject *> reachableObjects(QQuickItem *root)
{
    QList<const QObject *> found;
    if (!root) {
        return found;
    }

    QSet<const QObject *> seen;
    QList<QObject *> pending{root};

    while (!pending.isEmpty()) {
        QObject *const object = pending.takeFirst();
        if (!object || seen.contains(object)) {
            continue;
        }
        seen.insert(object);
        found.append(object);

        // The object tree. Covers a pointer handler, which is a QObject child and not a QQuickItem.
        const QList<QObject *> children = object->children();
        for (QObject *const child : children) {
            if (!seen.contains(child)) {
                pending.append(child);
            }
        }

        // The visual tree. Covers an item whose QObject parent is elsewhere.
        if (auto *const item = qobject_cast<QQuickItem *>(object)) {
            const QList<QQuickItem *> visualChildren = item->childItems();
            for (QQuickItem *const child : visualChildren) {
                if (!seen.contains(child)) {
                    pending.append(child);
                }
            }
        }
    }

    return found;
}

/// Whether @a object is something an operator could press.
inline bool isActionable(const QObject *object)
{
    if (!object) {
        return false;
    }

    return object->inherits("QQuickAbstractButton") || object->inherits("QQuickMouseArea")
           || object->inherits("QQuickPointerHandler");
}

/// Those of @a root's reachable objects an operator could press, named by \c objectName where one
/// is set and by class name otherwise.
///
/// Named rather than counted because the exception assertion has to say *exactly one, and this one*:
/// a count is satisfied by swapping one member for another, so the set is spelled out instead. A
/// control with no \c objectName reports its class name, which is enough for a reviewer to find it
/// and is why an unnamed control is still visible to the assertion rather than silently uncounted.
inline QStringList actionableControls(QQuickItem *root)
{
    QStringList found;

    const QList<const QObject *> reachable = reachableObjects(root);
    for (const QObject *const object : reachable) {
        if (!isActionable(object)) {
            continue;
        }
        found.append(object->objectName().isEmpty()
                         ? QString::fromUtf8(object->metaObject()->className())
                         : object->objectName());
    }

    return found;
}

}  // namespace AeroraActionableControls

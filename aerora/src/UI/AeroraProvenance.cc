#include "AeroraProvenance.h"

#include "AeroraUiLog.h"
#include "aerora_provenance.h"

AeroraProvenance::AeroraProvenance(QObject* parent)
    : QObject(parent)
{
    qCDebug(AeroraUiLog) << this;
}

AeroraProvenance::~AeroraProvenance()
{
    qCDebug(AeroraUiLog) << this;
}

QString AeroraProvenance::commit() const
{
    const QString hash = QString::fromUtf8(AERORA_BUILD_COMMIT);

    // The sentinel is not a commit, and returning it would hand a consumer seven zeros that look
    // like one. Empty is the honest answer, and isCommitKnown() is how a consumer asks first.
    if (hash == QString::fromUtf8(kAbsentCommitSentinel)) {
        return QString();
    }

    return hash;
}

bool AeroraProvenance::isCommitKnown() const
{
    return !commit().isEmpty();
}

QString AeroraProvenance::branch() const
{
    const QString name = QString::fromUtf8(AERORA_BUILD_BRANCH);

    if (name == QString::fromUtf8(kAbsentBranchSentinel)) {
        return QString();
    }

    return name;
}

bool AeroraProvenance::isWorkingTreeDirty() const
{
    // The generated macro is an integer literal rather than a CMake boolean, because TRUE and
    // FALSE are not C++ and a comparison against them would take the wrong branch silently.
    return AERORA_BUILD_WORKING_TREE_DIRTY != 0;
}

QString AeroraProvenance::version() const
{
    return QString::fromUtf8(AERORA_BUILD_VERSION);
}

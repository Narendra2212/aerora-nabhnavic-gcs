#include "AeroraAuditChain.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QString>

QByteArray AeroraAuditChain::genesisDigest()
{
    return QCryptographicHash::hash(QByteArray(), QCryptographicHash::Sha256);
}

QByteArray AeroraAuditChain::canonicalForm(const AeroraAuditEvent& event)
{
    QJsonObject canonical;

    // The action, as the string the store persists. AeroraAuditSink.h records that the store writes
    // typeToString() and not the integer, and that appending to the enumeration renumbers nothing -
    // so the digest covers the same representation the row does, and a reordered enumeration cannot
    // silently change a historical digest.
    canonical.insert(QStringLiteral("type"), AeroraAuditEvent::typeToString(event.type));

    // Milliseconds since epoch as a string. An invalid QDateTime is the unstamped state and is
    // encoded as empty rather than as a number, so "never stamped" and "stamped at the epoch" do
    // not collide.
    canonical.insert(QStringLiteral("occurredUtcMs"),
                     event.utc.isValid() ? QString::number(event.utc.toMSecsSinceEpoch()) : QString());

    // Strings, not numbers: both are quint64 and a JSON number is a double. See the encoding note
    // on the header.
    canonical.insert(QStringLiteral("monotonicUs"), QString::number(event.monotonicUs));
    canonical.insert(QStringLiteral("sequence"), QString::number(event.sequence));
    canonical.insert(QStringLiteral("recordsLostBeforeThis"), QString::number(event.recordsLostBeforeThis));

    canonical.insert(QStringLiteral("operatorId"), event.operatorId);
    canonical.insert(QStringLiteral("reasonCode"), event.reasonCode);

    // Both halves of the vehicle identity. The resolved flag is covered as well as the id, because
    // AeroraAuditSink.h records an unresolved vehicle as an explicit gap - and a digest that saw
    // only the id could not tell a real system id 0 from a gap.
    canonical.insert(QStringLiteral("vehicleMavlinkSystemId"), QString::number(event.vehicleMavlinkSystemId));
    canonical.insert(QStringLiteral("vehicleResolved"), event.vehicleResolved);

    canonical.insert(QStringLiteral("authorizationEnforced"), event.authorizationEnforced);

    // Through the same JSON encoding rather than through toString(). A lossy encoding here is a
    // field an editor could change without moving the digest, which is the whole failure this
    // construction exists to detect.
    canonical.insert(QStringLiteral("detail"), QJsonObject::fromVariantMap(event.detail));

    return QJsonDocument(canonical).toJson(QJsonDocument::Compact);
}

QByteArray AeroraAuditChain::digestFor(const AeroraAuditEvent& event, const QByteArray& previousDigest)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);

    // The predecessor first, then the record. Order is fixed so two implementations cannot both be
    // "correct" and disagree, and the predecessor is a fixed-width digest, so no separator is
    // needed between the two to keep the concatenation unambiguous.
    hash.addData(previousDigest);
    hash.addData(canonicalForm(event));

    return hash.result();
}

AeroraAuditChain::Verification AeroraAuditChain::verify(const QList<Entry>& entries)
{
    // An empty chain is intact. Nothing has been recorded, which is a different condition from a
    // chain that lost its records - and that one is detectable from the dense `sequence` field
    // rather than from the chain.
    Verification result;
    result.intact = true;

    QByteArray expectedPrevious = genesisDigest();

    for (qsizetype index = 0; index < entries.size(); ++index) {
        const Entry& entry = entries.at(index);

        if (entry.previousDigest != expectedPrevious) {
            result.intact = false;
            result.brokenAt = index;
            result.reason = (index == 0) ? QStringLiteral("the first record does not chain onto the genesis digest")
                                         : QStringLiteral(
                                               "this record's stored predecessor digest is not the previous "
                                               "record's digest, so a record was removed, inserted or reordered");
            return result;
        }

        if (entry.digest != digestFor(entry.event, entry.previousDigest)) {
            result.intact = false;
            result.brokenAt = index;
            result.reason = QStringLiteral(
                "this record's contents do not produce its stored digest, so a field "
                "was edited");
            return result;
        }

        expectedPrevious = entry.digest;
    }

    return result;
}

AeroraAuditChain::Entry AeroraAuditChain::append(const AeroraAuditEvent& event)
{
    Entry entry;
    entry.event = event;
    entry.previousDigest = _head;
    entry.digest = digestFor(event, _head);

    _head = entry.digest;
    ++_length;

    return entry;
}

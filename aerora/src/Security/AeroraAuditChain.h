#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QList>

#include "AeroraAuditSink.h"

/// \brief The per-record hash chain `DG-STO:tamper-evidence` settles, as a pure function.
///
/// \section settled What is implemented here, and on whose authority
///
/// The accepted `DG-STO:tamper-evidence` Closure_Record settles the construction: "a per-record
/// hash chain. Each audit record stores a SHA-256 digest over its own canonical fields together
/// with the digest of the record before it, so editing any field or removing any row breaks the
/// chain at that point and the break is localised to the exact record." It also records why that
/// was preferred to signing - the construction **needs no key**, so it is identical on all five
/// target platforms and cannot silently degrade on one of them.
///
/// Nothing here is invented. The field set is the one `AeroraAuditSink.h` maps onto the designed
/// `AuditRecords` columns, and the encoding is an implementation detail of this type rather than a
/// Contract_Value, so it is documented below instead of being chosen elsewhere.
///
/// \section notevidence Integrity checking, never tamper-evidence
///
/// This is the half that works without a key, and it is deliberately **not** sufficient for
/// `isTamperEvident()`. `AeroraAuditSink.h` states the reason: an actor with write access can
/// recompute the whole chain, so a chain alone detects editing by someone who did not rewrite it
/// and says nothing about substitution of the whole store. The accepted record settles the other
/// half - the chain head written to the platform key store at session close and verified at
/// startup - and that needs `AeroraPlatformKeyProtector` to have a backend, which no platform has.
///
/// So implementing this entitles no honesty flag to move. `isDurable()` stays false because no
/// store exists, and `isTamperEvident()` stays false because no anchor exists. Both guards are
/// recorded in the register as *permitted* to become true, which is not the same as earned.
///
/// \section encoding The canonical form, and why it is shaped this way
///
/// A digest over concatenated fields is ambiguous: two different records can produce one byte
/// string. So the canonical form is a compact JSON object with fixed keys, which is unambiguous by
/// construction, and three choices make it reproducible:
///
/// \li **Every numeric is encoded as a string.** `monotonicUs` and `sequence` are `quint64`, and a
///     JSON number is a double, so values above 2^53 would round. A digest that changes when a
///     value round-trips is worse than no digest.
/// \li **Key order is not chosen here.** `QJsonObject` holds keys sorted, so the serialised order
///     is a property of the keys rather than of the insertion sequence.
/// \li **`detail` goes through the same JSON encoding** rather than through `toString()`, which
///     would be lossy - and a field the digest cannot see is a field an editor can change.
///
/// \section scope What the digest covers, and what it cannot
///
/// The producer-supplied fields only. `recordId` and `sessionId` are assigned by the store on
/// insert, which `AeroraAuditSink.h` records as "not a producer concern", and this type runs before
/// a store exists to assign them.
///
/// That costs nothing for ordering, and the reason is worth stating because it looks like a gap:
/// row order is already covered, not by any field, but by the chain itself. Each digest consumes
/// the one before it, so moving a record changes every digest after it. Binding a store-assigned
/// row identity into the digest would add nothing the chain does not already carry.
class AeroraAuditChain
{
public:
    /// One stored link: the record, the digest it was chained onto, and its own digest.
    ///
    /// This is the shape the designed `AuditRecords` row persists as `prevHash` and `recordHash`.
    /// It exists here because verification reads what was *stored* rather than recomputing from a
    /// live chain - a verifier that trusted its own head would agree with itself.
    struct Entry
    {
        AeroraAuditEvent event;
        QByteArray previousDigest;
        QByteArray digest;
    };

    /// Where verification stopped.
    ///
    /// `brokenAt` is an index and not a boolean because the accepted record requires the break to
    /// be "localised to the exact record". A verifier that reported only intact-or-not would
    /// satisfy the hash and not the requirement.
    struct Verification
    {
        bool intact = false;

        /// Index of the first entry that does not verify, or -1 when `intact`.
        qsizetype brokenAt = -1;

        /// Why that entry failed, for an operator rather than for a parser. Empty when `intact`.
        QString reason;
    };

    /// The digest that precedes the first record: SHA-256 of no input.
    ///
    /// Derived rather than a written constant, so there is no magic value to transcribe wrongly,
    /// and the first record is chained onto something rather than onto an empty byte array that a
    /// later bug could also produce.
    [[nodiscard]] static QByteArray genesisDigest();

    /// The unambiguous byte encoding of the fields the digest covers. See \section encoding.
    [[nodiscard]] static QByteArray canonicalForm(const AeroraAuditEvent& event);

    /// SHA-256 over \a previousDigest followed by the canonical form of \a event.
    [[nodiscard]] static QByteArray digestFor(const AeroraAuditEvent& event, const QByteArray& previousDigest);

    /// Verifies \a entries as a chain, in order.
    ///
    /// Checks three things per entry, because they fail differently: that the stored digest is the
    /// one the record and its predecessor produce, that the stored predecessor digest is the
    /// previous entry's stored digest, and that the first entry chains onto the genesis digest.
    [[nodiscard]] static Verification verify(const QList<Entry>& entries);

    /// The digest the next appended record will chain onto. Genesis before any append.
    [[nodiscard]] QByteArray head() const { return _head; }

    /// How many records this instance has chained.
    [[nodiscard]] qsizetype length() const { return _length; }

    /// Chains \a event onto the current head, advances the head, and returns the completed link.
    Entry append(const AeroraAuditEvent& event);

private:
    QByteArray _head = genesisDigest();
    qsizetype _length = 0;
};

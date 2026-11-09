#include "AeroraAuditChainTest.h"

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtTest/QTest>

#include "AeroraAuditChain.h"
#include "AeroraAuditSink.h"

namespace {

/// A fully populated record. Every field carries a non-default value, because a field left at its
/// default cannot distinguish "covered by the digest" from "absent from the encoding".
AeroraAuditEvent populatedEvent(quint64 sequence)
{
    AeroraAuditEvent event;
    event.type = AeroraAuditEventType::AuthorizationDecision;
    event.utc = QDateTime::fromMSecsSinceEpoch(1762000000000);
    event.monotonicUs = 123456789;
    event.sequence = sequence;
    event.operatorId = QStringLiteral("opaque-operator-1");
    event.reasonCode = QStringLiteral("class-not-granted-to-held-roles");
    event.vehicleMavlinkSystemId = 1;
    event.vehicleResolved = true;
    event.authorizationEnforced = true;
    event.recordsLostBeforeThis = 2;
    event.detail = QVariantMap{{QStringLiteral("command"), 400}, {QStringLiteral("param1"), 1.0}};
    return event;
}

/// A chain of @a count populated records, each with its own sequence number.
QList<AeroraAuditChain::Entry> chainOf(int count)
{
    AeroraAuditChain chain;
    QList<AeroraAuditChain::Entry> entries;
    for (int index = 0; index < count; ++index) {
        entries.append(chain.append(populatedEvent(static_cast<quint64>(index) + 1)));
    }
    return entries;
}

}  // namespace

void AeroraAuditChainTest::_theGenesisDigestIsStableAndNotEmpty()
{
    const QByteArray genesis = AeroraAuditChain::genesisDigest();

    QCOMPARE(genesis.size(), 32);
    QCOMPARE(genesis, AeroraAuditChain::genesisDigest());
    QVERIFY(!genesis.isEmpty());
}

void AeroraAuditChainTest::_anEmptyChainVerifiesAndItsHeadIsGenesis()
{
    const AeroraAuditChain chain;

    QCOMPARE(chain.head(), AeroraAuditChain::genesisDigest());
    QCOMPARE(chain.length(), qsizetype(0));

    // An empty chain is intact rather than broken: nothing was recorded, which is a different
    // condition from records having been lost - and that one is evidenced by the dense sequence
    // field, not by the chain.
    const AeroraAuditChain::Verification result = AeroraAuditChain::verify({});
    QVERIFY(result.intact);
    QCOMPARE(result.brokenAt, qsizetype(-1));
}

void AeroraAuditChainTest::_appendingAdvancesTheHeadAndChainsOntoThePrevious()
{
    AeroraAuditChain chain;

    const AeroraAuditChain::Entry first = chain.append(populatedEvent(1));
    QCOMPARE(first.previousDigest, AeroraAuditChain::genesisDigest());
    QCOMPARE(chain.head(), first.digest);
    QCOMPARE(chain.length(), qsizetype(1));

    const AeroraAuditChain::Entry second = chain.append(populatedEvent(2));
    QCOMPARE(second.previousDigest, first.digest);
    QCOMPARE(chain.head(), second.digest);
    QCOMPARE(chain.length(), qsizetype(2));

    QVERIFY(first.digest != second.digest);
}

void AeroraAuditChainTest::_identicalRecordsAtDifferentPositionsGetDifferentDigests()
{
    // The property that makes position part of the digest. Two byte-identical records must not
    // produce the same digest at different points, or a record could be moved or duplicated
    // without breaking anything.
    AeroraAuditChain chain;
    const AeroraAuditEvent event = populatedEvent(1);

    const AeroraAuditChain::Entry first = chain.append(event);
    const AeroraAuditChain::Entry second = chain.append(event);

    QCOMPARE(AeroraAuditChain::canonicalForm(first.event), AeroraAuditChain::canonicalForm(second.event));
    QVERIFY(first.digest != second.digest);
}

void AeroraAuditChainTest::_theCanonicalFormIsStableAcrossCalls()
{
    // Determinism is the premise the whole construction rests on. If the encoding varied between
    // calls - an unordered map, a locale-dependent number - verification would fail at random and
    // read as tampering.
    const AeroraAuditEvent event = populatedEvent(7);

    const QByteArray first = AeroraAuditChain::canonicalForm(event);
    QCOMPARE(AeroraAuditChain::canonicalForm(event), first);
    QCOMPARE(AeroraAuditChain::canonicalForm(populatedEvent(7)), first);
    QVERIFY(!first.isEmpty());
}

void AeroraAuditChainTest::_anIntactChainVerifies()
{
    const AeroraAuditChain::Verification result = AeroraAuditChain::verify(chainOf(5));

    QVERIFY2(result.intact, qPrintable(result.reason));
    QCOMPARE(result.brokenAt, qsizetype(-1));
    QVERIFY(result.reason.isEmpty());
}

void AeroraAuditChainTest::_anEditedFieldBreaksTheChainAtThatRecord_data()
{
    QTest::addColumn<QString>("field");

    // One row per field the record carries. A field missing from canonicalForm() fails its row,
    // which is the only way to detect an encoding that silently stopped covering something.
    QTest::newRow("type") << QStringLiteral("type");
    QTest::newRow("utc") << QStringLiteral("utc");
    QTest::newRow("monotonicUs") << QStringLiteral("monotonicUs");
    QTest::newRow("sequence") << QStringLiteral("sequence");
    QTest::newRow("operatorId") << QStringLiteral("operatorId");
    QTest::newRow("reasonCode") << QStringLiteral("reasonCode");
    QTest::newRow("vehicleMavlinkSystemId") << QStringLiteral("vehicleMavlinkSystemId");
    QTest::newRow("vehicleResolved") << QStringLiteral("vehicleResolved");
    QTest::newRow("authorizationEnforced") << QStringLiteral("authorizationEnforced");
    QTest::newRow("recordsLostBeforeThis") << QStringLiteral("recordsLostBeforeThis");
    QTest::newRow("detail") << QStringLiteral("detail");
}

void AeroraAuditChainTest::_anEditedFieldBreaksTheChainAtThatRecord()
{
    QFETCH(QString, field);

    QList<AeroraAuditChain::Entry> entries = chainOf(4);

    // The middle of the chain, so a break is neither the first nor the last link and the reported
    // index has to be right rather than merely a boundary.
    const qsizetype edited = 2;
    AeroraAuditEvent& event = entries[edited].event;

    if (field == QStringLiteral("type")) {
        event.type = AeroraAuditEventType::CommandTransmitted;
    } else if (field == QStringLiteral("utc")) {
        event.utc = event.utc.addMSecs(1);
    } else if (field == QStringLiteral("monotonicUs")) {
        event.monotonicUs += 1;
    } else if (field == QStringLiteral("sequence")) {
        event.sequence += 1;
    } else if (field == QStringLiteral("operatorId")) {
        event.operatorId = QStringLiteral("someone-else");
    } else if (field == QStringLiteral("reasonCode")) {
        event.reasonCode = QStringLiteral("no-role-held");
    } else if (field == QStringLiteral("vehicleMavlinkSystemId")) {
        event.vehicleMavlinkSystemId += 1;
    } else if (field == QStringLiteral("vehicleResolved")) {
        event.vehicleResolved = !event.vehicleResolved;
    } else if (field == QStringLiteral("authorizationEnforced")) {
        event.authorizationEnforced = !event.authorizationEnforced;
    } else if (field == QStringLiteral("recordsLostBeforeThis")) {
        event.recordsLostBeforeThis += 1;
    } else if (field == QStringLiteral("detail")) {
        event.detail.insert(QStringLiteral("command"), 401);
    } else {
        QFAIL("unhandled field row");
    }

    const AeroraAuditChain::Verification result = AeroraAuditChain::verify(entries);

    QVERIFY2(!result.intact,
             qPrintable(QStringLiteral("editing %1 must break the chain, so the digest must cover it").arg(field)));

    // Localised to the exact record, which the accepted record requires in those words.
    QCOMPARE(result.brokenAt, edited);
    QVERIFY(!result.reason.isEmpty());
}

void AeroraAuditChainTest::_aRemovedRecordBreaksTheChainWhereItWasRemoved()
{
    QList<AeroraAuditChain::Entry> entries = chainOf(5);
    entries.removeAt(2);

    const AeroraAuditChain::Verification result = AeroraAuditChain::verify(entries);

    QVERIFY(!result.intact);

    // The break lands on the record that followed the removed one, because that is where the stored
    // predecessor digest stops matching. Asserted so the reported index cannot drift by one.
    QCOMPARE(result.brokenAt, qsizetype(2));
}

void AeroraAuditChainTest::_areorderedPairBreaksTheChain()
{
    QList<AeroraAuditChain::Entry> entries = chainOf(4);
    entries.swapItemsAt(1, 2);

    const AeroraAuditChain::Verification result = AeroraAuditChain::verify(entries);

    QVERIFY(!result.intact);
    QCOMPARE(result.brokenAt, qsizetype(1));
}

void AeroraAuditChainTest::_aForgedFirstRecordDoesNotChainOntoGenesis()
{
    // Truncating the head of the trail. Without the genesis check this verifies cleanly, because
    // every surviving link is internally consistent - which is how a trail loses its beginning
    // without anything noticing.
    QList<AeroraAuditChain::Entry> entries = chainOf(4);
    entries.removeFirst();

    const AeroraAuditChain::Verification result = AeroraAuditChain::verify(entries);

    QVERIFY2(!result.intact, "a chain whose first record does not chain onto genesis must not verify");
    QCOMPARE(result.brokenAt, qsizetype(0));
}

void AeroraAuditChainTest::_anEditedRecordWithARecomputedDigestStillBreaksTheLink()
{
    // The limit of a keyless chain, asserted rather than assumed. An editor who recomputes the
    // edited record's own digest is still caught, because the *next* record's stored predecessor
    // digest no longer matches. Only an editor who rewrites every subsequent digest succeeds, which
    // is exactly why AeroraAuditSink.h says this is integrity checking and not tamper-evidence, and
    // why the accepted record settles a protected anchor as the other half.
    QList<AeroraAuditChain::Entry> entries = chainOf(4);

    entries[1].event.operatorId = QStringLiteral("someone-else");
    entries[1].digest = AeroraAuditChain::digestFor(entries[1].event, entries[1].previousDigest);

    const AeroraAuditChain::Verification result = AeroraAuditChain::verify(entries);

    QVERIFY(!result.intact);
    QCOMPARE(result.brokenAt, qsizetype(2));
}

void AeroraAuditChainTest::_anUnstampedTimeIsNotTheEpoch()
{
    // AeroraAuditSink.h records an unstamped time as a real state. If the encoding rendered an
    // invalid QDateTime as 0 then a record stamped at the epoch and a record never stamped would
    // share a digest, and the gap would be unprovable from the trail.
    AeroraAuditEvent unstamped = populatedEvent(1);
    unstamped.utc = QDateTime();

    AeroraAuditEvent atEpoch = populatedEvent(1);
    atEpoch.utc = QDateTime::fromMSecsSinceEpoch(0);

    QVERIFY(AeroraAuditChain::canonicalForm(unstamped) != AeroraAuditChain::canonicalForm(atEpoch));
}

void AeroraAuditChainTest::_anUnresolvedVehicleIsNotSystemIdZero()
{
    // Same distinction on the vehicle identity. An unresolved vehicle is an explicit gap, and a
    // digest covering only the id could not tell it from a station that really saw system id 0.
    AeroraAuditEvent unresolved = populatedEvent(1);
    unresolved.vehicleResolved = false;
    unresolved.vehicleMavlinkSystemId = 0;

    AeroraAuditEvent resolvedZero = populatedEvent(1);
    resolvedZero.vehicleResolved = true;
    resolvedZero.vehicleMavlinkSystemId = 0;

    QVERIFY(AeroraAuditChain::canonicalForm(unresolved) != AeroraAuditChain::canonicalForm(resolvedZero));
}

void AeroraAuditChainTest::_aNestedDetailChangeIsCovered()
{
    // detail is a QVariantMap and can nest. Encoding it through toString() would flatten a nested
    // change away, so this is what holds the encoding to the JSON route.
    AeroraAuditEvent before = populatedEvent(1);
    before.detail = QVariantMap{{QStringLiteral("address"), QVariantMap{{QStringLiteral("vehicles"), 2}}}};

    AeroraAuditEvent after = populatedEvent(1);
    after.detail = QVariantMap{{QStringLiteral("address"), QVariantMap{{QStringLiteral("vehicles"), 3}}}};

    QVERIFY(AeroraAuditChain::canonicalForm(before) != AeroraAuditChain::canonicalForm(after));
}

UT_REGISTER_TEST(AeroraAuditChainTest, TestLabel::Unit)

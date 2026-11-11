#include "AeroraEventStoreTest.h"

#include <QtCore/QList>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QTemporaryDir>
#include <QtCore/QVariant>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtTest/QTest>

#include "AeroraEventStore.h"
#include "AeroraLocalStore.h"
#include "AeroraTelemetryState.h"
#include "QGCSqlHelper.h"

namespace {

AeroraLocalStore::SessionRecord sampleSession()
{
    AeroraLocalStore::SessionRecord record;
    record.startedUtcMs = 1700000000000LL;
    record.baseCommit = QStringLiteral("base");
    record.overlayCommit = QStringLiteral("overlay");
    record.customBuild = true;
    record.authorizationEnforced = false;
    return record;
}

AeroraEventRecord sampleEvent()
{
    AeroraEventRecord event;
    event.category = AeroraEventCategory::StateTransition;
    event.subject = QStringLiteral("flight.mode");
    event.occurredUtcMs = 1700000001000LL;
    event.monotonicUs = 2000;
    event.stateClass = AeroraTelemetryState::Reported;
    return event;
}

int rowCount(const QString& path, const QString& table)
{
    QGCSqlHelper::ScopedConnection connection(path);
    if (!connection.isValid()) {
        return -1;
    }
    QSqlQuery query(connection.database());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table)) || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

}  // namespace

void AeroraEventStoreTest::_recordEventPersistsEventAndValues()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(dbPath, false).opened);
    QVERIFY(store.beginSession(sampleSession()) > 0);

    AeroraEventStore events(store);
    QVERIFY(events.open());

    AeroraEventValue reported;
    reported.itemId = QStringLiteral("battery.voltage");
    reported.stateClass = AeroraTelemetryState::Reported;
    reported.derived = false;
    reported.valueText = QStringLiteral("12.3");
    reported.ageMs = 500;

    AeroraEventValue derived;
    derived.itemId = QStringLiteral("wind.estimate");
    derived.stateClass = AeroraTelemetryState::Proposed;
    derived.derived = true;
    derived.valueText = QStringLiteral("3.1");
    derived.ageMs = 100;

    QVERIFY(events.recordEvent(sampleEvent(), {reported, derived}));

    QCOMPARE(rowCount(dbPath, QStringLiteral("Events")), 1);
    QCOMPARE(rowCount(dbPath, QStringLiteral("EventValues")), 2);

    QGCSqlHelper::ScopedConnection connection(dbPath);
    QVERIFY(connection.isValid());

    QSqlQuery eventQuery(connection.database());
    QVERIFY(eventQuery.exec(
        QStringLiteral("SELECT category, subject, occurredUtcMs, monotonicUs, stateClass FROM Events")));
    QVERIFY(eventQuery.next());
    QCOMPARE(eventQuery.value(0).toInt(), static_cast<int>(AeroraEventCategory::StateTransition));
    QCOMPARE(eventQuery.value(1).toString(), QStringLiteral("flight.mode"));
    QCOMPARE(eventQuery.value(2).toLongLong(), 1700000001000LL);
    QCOMPARE(eventQuery.value(3).toLongLong(), qint64(2000));
    QCOMPARE(eventQuery.value(4).toInt(), static_cast<int>(AeroraTelemetryState::Reported));

    QSqlQuery valueQuery(connection.database());
    QVERIFY(
        valueQuery.exec(QStringLiteral("SELECT itemId, stateClass, derived, valueText, ageMs FROM EventValues "
                                       "ORDER BY itemId")));
    QVERIFY(valueQuery.next());
    QCOMPARE(valueQuery.value(0).toString(), QStringLiteral("battery.voltage"));
    QCOMPARE(valueQuery.value(1).toInt(), static_cast<int>(AeroraTelemetryState::Reported));
    QCOMPARE(valueQuery.value(2).toInt(), 0);
    QCOMPARE(valueQuery.value(3).toString(), QStringLiteral("12.3"));
    QCOMPARE(valueQuery.value(4).toLongLong(), qint64(500));
    QVERIFY(valueQuery.next());
    QCOMPARE(valueQuery.value(0).toString(), QStringLiteral("wind.estimate"));
    QCOMPARE(valueQuery.value(2).toInt(), 1);  // derived
}

void AeroraEventStoreTest::_nullableFieldsArePersistedAsNull()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(dbPath, false).opened);
    QVERIFY(store.beginSession(sampleSession()) > 0);

    AeroraEventStore events(store);
    QVERIFY(events.open());

    AeroraEventRecord event;
    event.category = AeroraEventCategory::SystemEvent;
    event.subject = QStringLiteral("boot");
    event.occurredUtcMs = 1700000002000LL;
    event.monotonicUs = 3000;
    // No stateClass: a non-value event.

    AeroraEventValue value;
    value.itemId = QStringLiteral("payload.state");
    value.stateClass = AeroraTelemetryState::Unavailable;
    value.derived = false;
    // No valueText, no ageMs: Unavailable carries no datum.

    QVERIFY(events.recordEvent(event, {value}));

    QGCSqlHelper::ScopedConnection connection(dbPath);
    QVERIFY(connection.isValid());

    QSqlQuery eventQuery(connection.database());
    QVERIFY(eventQuery.exec(QStringLiteral("SELECT vehicleId, stateClass FROM Events")));
    QVERIFY(eventQuery.next());
    QVERIFY2(eventQuery.value(0).isNull(), "vehicleId must be null until a Vehicles writer exists");
    QVERIFY2(eventQuery.value(1).isNull(), "a non-value event stores a null stateClass");

    QSqlQuery valueQuery(connection.database());
    QVERIFY(valueQuery.exec(QStringLiteral("SELECT stateClass, valueText, ageMs FROM EventValues")));
    QVERIFY(valueQuery.next());
    QCOMPARE(valueQuery.value(0).toInt(), static_cast<int>(AeroraTelemetryState::Unavailable));
    QVERIFY2(valueQuery.value(1).isNull(), "an Unavailable value stores a null valueText");
    QVERIFY2(valueQuery.value(2).isNull(), "an absent age stores null");
}

void AeroraEventStoreTest::_categoryIsStoredAsItsInteger()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(dbPath, false).opened);
    QVERIFY(store.beginSession(sampleSession()) > 0);

    AeroraEventStore events(store);
    QVERIFY(events.open());

    const QList<AeroraEventCategory> categories = {AeroraEventCategory::OperatorAction,
                                                   AeroraEventCategory::AuthorizationOutcome,
                                                   AeroraEventCategory::VehicleEvent};
    for (const AeroraEventCategory category : categories) {
        AeroraEventRecord event = sampleEvent();
        event.category = category;
        QVERIFY(events.recordEvent(event, {}));
    }

    QGCSqlHelper::ScopedConnection connection(dbPath);
    QVERIFY(connection.isValid());
    QSqlQuery query(connection.database());
    QVERIFY(query.exec(QStringLiteral("SELECT category FROM Events ORDER BY eventId")));
    for (const AeroraEventCategory category : categories) {
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), static_cast<int>(category));
    }
}

void AeroraEventStoreTest::_anEventMayCarryNoValues()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(dbPath, false).opened);
    QVERIFY(store.beginSession(sampleSession()) > 0);

    AeroraEventStore events(store);
    QVERIFY(events.open());

    // A state transition may be an event in itself, with no per-value rows.
    QVERIFY(events.recordEvent(sampleEvent(), {}));

    QCOMPARE(rowCount(dbPath, QStringLiteral("Events")), 1);
    QCOMPARE(rowCount(dbPath, QStringLiteral("EventValues")), 0);
}

void AeroraEventStoreTest::_recordWithoutAnActiveSessionFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(dbPath, false).opened);
    // No beginSession: every Event row needs a session.

    AeroraEventStore events(store);
    QVERIFY(events.open());

    QVERIFY(!events.recordEvent(sampleEvent(), {}));
    QCOMPARE(rowCount(dbPath, QStringLiteral("Events")), 0);
    // The failure is counted rather than lost; with no session it cannot be persisted, so it lands
    // in the in-memory counter.
    QCOMPARE(store.unrecordedWriteFailureCount(), 1);
}

void AeroraEventStoreTest::_recordBeforeOpenFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("aerora.db"));

    AeroraLocalStore store;
    QVERIFY(store.open(dbPath, false).opened);
    QVERIFY(store.beginSession(sampleSession()) > 0);

    AeroraEventStore events(store);  // deliberately not opened
    QVERIFY(!events.recordEvent(sampleEvent(), {}));
}

void AeroraEventStoreTest::_openFailsWhenStoreIsNotOpen()
{
    AeroraLocalStore store;  // deliberately not opened
    AeroraEventStore events(store);

    expectLogMessage("aerora.telemetry.events", QtWarningMsg, QRegularExpression(QStringLiteral("store is not open")));
    QVERIFY(!events.open());
    verifyExpectedLogMessage();

    QVERIFY(!events.isOpen());
}

UT_REGISTER_TEST(AeroraEventStoreTest, TestLabel::Unit)

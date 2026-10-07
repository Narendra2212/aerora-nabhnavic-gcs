#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

/// \brief The reporting source of an Aerora telemetry value: the third element of the value triple.
///
/// 11-telemetry-icd.md requires every Aerora value to carry its value, its receive time and its
/// reporting source, and requires the triple never to be reduced on its way to a display or a record
/// (\c AERORA-GCS-REQ-027). This type is that third element.
///
/// \section attribution Why there is no source-role enumeration
///
/// It is tempting to model the source as a role - flight controller, companion, payload controller,
/// power controller - because the ICD's rationale for carrying the source at all is that "a reading
/// from the flight controller and the same reading relayed by a companion are not equally
/// trustworthy". That rationale is correct and the enumeration would still be wrong today.
///
/// MAVLink identifies the component that *sent* a message. For a payload or power value relayed
/// through the flight controller, the sender is the flight controller, not the originating
/// subsystem. Nothing currently reported distinguishes the two; that is \c IF-MAV-022 and it is
/// \c OPEN. 11-telemetry-icd.md records the resulting conflict explicitly and resolves it in favour
/// of honesty:
///
/// > Until a message identifies the originator, the ground station records the sending component and
/// > must not present it as the originating subsystem.
///
/// So this type carries what is *observed* - the system and component identifiers out of the message
/// header - plus an \c Attribution that states which of the two the identifiers mean. No role is
/// inferred and no component identifier is invented. \c OriginatingComponent exists in the
/// enumeration so that consumers written today compile unchanged when \c IF-MAV-022 is answered; no
/// code produces it yet.
class AeroraTelemetrySource
{
    Q_GADGET
    Q_PROPERTY(int systemId READ systemId CONSTANT)
    Q_PROPERTY(int componentId READ componentId CONSTANT)
    Q_PROPERTY(Attribution attribution READ attribution CONSTANT)
    Q_PROPERTY(bool identified READ isIdentified CONSTANT)

public:
    /// What the carried identifiers actually denote.
    enum Attribution {
        /// No message has been observed, so there is no source. The identifiers are meaningless.
        Unidentified = 0,
        /// The identifiers denote the component that sent the message. This is the only attribution
        /// any Aerora code produces today, and it may differ from the originating subsystem.
        SendingComponent,
        /// The identifiers denote the component that originated the value. Reserved for
        /// \c IF-MAV-022; nothing produces this yet.
        OriginatingComponent,
    };
    Q_ENUM(Attribution)

    /// An unidentified source. The correct initial state for every item: a value with no source is
    /// not a value from an anonymous source, it is a value that has not arrived.
    AeroraTelemetrySource() = default;

    /// The source of an observed message, attributed to the sending component.
    ///
    /// \param systemId    The observed MAVLink system identifier. Transport addressing only - it is
    ///                    never the Aerora vehicle identity (\c AERORA-VEH-REQ-001).
    /// \param componentId The observed MAVLink component identifier of the \e sender.
    [[nodiscard]] static AeroraTelemetrySource fromSendingComponent(int systemId, int componentId);

    [[nodiscard]] int systemId() const { return _systemId; }
    [[nodiscard]] int componentId() const { return _componentId; }
    [[nodiscard]] Attribution attribution() const { return _attribution; }

    /// False until a message has been observed.
    [[nodiscard]] bool isIdentified() const { return _attribution != Unidentified; }

    /// Stable machine-readable identifier for records and logs, of the form
    /// \c "sending:<sysId>.<compId>" or \c "unidentified". Not an operator-facing label.
    [[nodiscard]] QString toString() const;

    [[nodiscard]] bool operator==(const AeroraTelemetrySource &other) const;
    [[nodiscard]] bool operator!=(const AeroraTelemetrySource &other) const { return !(*this == other); }

private:
    int _systemId = 0;
    int _componentId = 0;
    Attribution _attribution = Unidentified;
};

Q_DECLARE_METATYPE(AeroraTelemetrySource)

#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QString>

/// \brief An operator-originated payload request, recorded by the ground station.
///
/// \section opaque Why the request is an opaque token
///
/// \c IF-PAY-009 is \c NOT-EVIDENCED and \c OPEN: no payload command set exists, no MAVLink command
/// identifier for a payload operation is evidenced, and whether the payload controller is even a
/// distinct MAVLink component is unresolved (Q3-006). A typed command enumeration here would be an
/// invention that then propagated into a dialect, a metadata file, a log schema and an audit record
/// simultaneously — the failure 06-payload-icd.md names explicitly.
///
/// So the request carries a token the originator supplies and this layer does not interpret. It is
/// **not** a \c MAV_CMD, not a parameter block, and not a wire format. Mapping a token onto a
/// protocol message is the job of whatever implements \c AeroraPayloadCommandSink, and nothing in
/// this overlay does.
///
/// \section notstate What a request is not
///
/// A request is not payload state, and there is no member here that could become payload state.
/// \c AERORA-PAYLOAD-REQ-011 and \c IF-PAY-009 both require that a command reaching "accepted" is
/// still not an effect: effectiveness needs a **reported** state change from the payload
/// controller. The eight-state lifecycle a submitted request then travels through — \c REQUESTED,
/// \c SENT, \c RECEIVED, \c ACCEPTED, \c REJECTED, \c EFFECTIVE, \c STALE, \c FAILED — belongs to
/// \c M-Authority (\c AERORA-GCS-REQ-015), not here. This type deliberately carries no state field,
/// so \c M-Payload cannot acquire a second, divergent command tracker.
class AeroraPayloadRequest
{
    Q_GADGET
    Q_PROPERTY(QString requestToken READ requestToken CONSTANT)
    Q_PROPERTY(quint64 submitTimeUs READ submitTimeUs CONSTANT)
    Q_PROPERTY(bool valid READ isValid CONSTANT)

public:
    /// An empty request. Invalid, because it identifies no operation.
    AeroraPayloadRequest() = default;

    /// \param requestToken Opaque, originator-supplied identifier for the requested operation.
    ///                     Never interpreted, ranged, defaulted or mapped to a meaning by this
    ///                     module.
    /// \param submitTimeUs Submission time in the owning model's clock domain.
    AeroraPayloadRequest(const QString &requestToken, quint64 submitTimeUs)
        : _requestToken(requestToken)
        , _submitTimeUs(submitTimeUs)
    {
    }

    [[nodiscard]] QString requestToken() const { return _requestToken; }

    /// Submission time in the owning model's clock domain. Zero means unstamped.
    [[nodiscard]] quint64 submitTimeUs() const { return _submitTimeUs; }

    /// A request with no token identifies no operation and is refused rather than sent.
    [[nodiscard]] bool isValid() const { return !_requestToken.isEmpty(); }

private:
    QString _requestToken;
    quint64 _submitTimeUs = 0;
};

Q_DECLARE_METATYPE(AeroraPayloadRequest)

/// \brief The one outbound payload path, as an interface with no implementation.
///
/// \section why Why this is an interface and why nothing implements it
///
/// \c IF-PAY-015 — "Ground station to payload actuators" — is \c CONFIRMED with direction **None,
/// prohibited**, and \c IF-PAY-013 places the actuator interface entirely inside the vehicle, owned
/// by the payload controller. 10-payload-implementation.md records \c IF-PAY-015 as one of two
/// interfaces "implementable as assertions that nothing exists", and R3-007 names it a
/// highest-priority conversion from inspection to test.
///
/// Declaring the seam and leaving it empty is how that assertion becomes a property of the build
/// rather than a claim in a document:
///
/// \li There is **no implementation of this interface anywhere in the overlay.** The absence is the
///     deliverable.
/// \li \c AeroraPayloadModel's sink defaults to \c nullptr, so \c submitRequest() fails closed with
///     \c RefusedNoCommandPath. A payload request cannot leave the ground station in this build.
/// \li An implementation appearing is a reviewable event in a single, named place, not a diff
///     scattered across a model.
///
/// \section obligations What an implementation must do, when one is authorised
///
/// These are dependencies on other workstreams, recorded here so they are not rediscovered later.
/// \c M-Payload implements none of them and must not.
///
/// \li **Authorization.** Every request must pass \c M-Authz's command gate before reaching
///     MAVLink. The policy, the role set (Q-029) and the audit record are Tab 2's; see
///     \c aerora/src/Security/AeroraAuthorization.h. This module performs no authorization and
///     holds no policy, so it cannot weaken one.
/// \li **Command tracking.** The submitted request must be tracked through \c M-Authority's
///     eight-state model, including the accepted-but-not-effective case that
///     \c AERORA-GCS-REQ-016 requires be surfaced. A stale request must not be retried
///     automatically (\c AERORA-GCS-REQ-017).
/// \li **Protocol mapping.** Turning a token into a message needs the Aerora dialect. Until one
///     exists there is nothing to map onto, which is the concrete reason this interface is
///     unimplemented rather than merely unimplemented yet.
/// \li **No actuator addressing.** An implementation addresses the payload controller, which
///     decides. It never addresses an actuator, a valve, a pump, a nozzle or a gimbal axis
///     directly; that path is \c IF-PAY-015 and does not exist.
///
/// \section notsafety What no implementation may ever do
///
/// Originate a request that no operator asked for. The ground station is not the payload safety
/// controller (\c AERORA-PAYLOAD-REQ-010): a reported fault, overpressure, empty tank, jam,
/// temperature fault or communication loss is displayed and logged, and reaches no command.
/// 18-error-degradation-model.md records "none" in the authority column of every aircraft-reported
/// condition.
class AeroraPayloadCommandSink
{
public:
    virtual ~AeroraPayloadCommandSink() = default;

    /// Submits an operator-originated payload request.
    ///
    /// \return True when the request was accepted for transmission. **Never** an indication that
    ///         the payload acted: the stop is not the stop until the vehicle reports it
    ///         (\c IF-PAY-010), and the same holds for every other request.
    [[nodiscard]] virtual bool submitPayloadRequest(const AeroraPayloadRequest &request) = 0;
};

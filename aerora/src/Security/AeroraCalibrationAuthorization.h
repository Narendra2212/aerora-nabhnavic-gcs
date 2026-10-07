#pragma once

#include <QtCore/QString>

#include "AeroraAuthorization.h"

/// \file AeroraCalibrationAuthorization.h
/// Structural classification of calibration requests, for the authorization boundary.
///
/// This file contains **no policy**. It names no role, no permission and no allow-list, and it
/// decides nothing. Q-029 (the operator role set) is OPEN, so who may calibrate cannot be defined
/// here and is not defined here.
///
/// What it does provide is the part that is definable without the role model: a precise answer to
/// *what* was requested, so that a future policy can be written correctly and so that an audit
/// record of a calibration denial is identifiable.
///
/// ### Why calibration needs this at all
///
/// `Vehicle::startCalibration()` and `Vehicle::stopCalibration()` both send
/// `MAV_CMD_PREFLIGHT_CALIBRATION`. They are distinguished **only** by the parameter vector:
/// `stopCalibration()` sends all seven parameters zero (`src/Vehicle/Vehicle.cc:2408-2420`), while
/// `startCalibration()` sets one or more non-zero according to the requested calibration type
/// (`:2328-2405`).
///
/// A policy keyed on the command id alone therefore cannot tell "start calibrating the ESCs" from
/// "cancel the calibration that is running". Getting that wrong is not a cosmetic error: refusing
/// the all-zero form removes the operator's ability to stop a calibration in progress.

/// \brief What kind of calibration request a parameter vector represents.
enum class AeroraCalibrationRequestKind {
    /// Not `MAV_CMD_PREFLIGHT_CALIBRATION`. Nothing in this file applies.
    NotCalibration,

    /// All seven parameters are zero. Byte-identical to what `Vehicle::stopCalibration()` emits,
    /// and also what `Vehicle::startCalibration()` emits for `CalibrationNone` or any enumerator
    /// its switch does not handle. The two are indistinguishable on the wire and must be treated
    /// as one case.
    CancelOrNoOp,

    /// At least one parameter is non-zero: a request to begin a specific calibration.
    Start,
};

/// \brief Structural description of a calibration request. Carries no verdict.
///
/// \c setParamMask has bit *(n-1)* set when MAVLink `paramN` is non-zero, so a policy can see which
/// parameter carries the request without this file asserting what any parameter means.
///
/// The parameter-to-calibration-type mapping is deliberately **not** reproduced here. It lives in
/// `Vehicle::startCalibration()` and duplicating it would create a second copy that can drift from
/// the one that actually builds the packet.
///
/// ### A policy must NOT key on the parameter index alone
///
/// The index-to-type mapping is **not** one-to-one, and the collisions land in the worst place:
///
///   - `param6 = 1` is APM motor-interference calibration, which **spins the props**.
///     `param6 = 2` is PX4 airspeed calibration, which does not. Same index, different value,
///     completely different physical consequence.
///   - `param3 = 1` is emitted by three different calibration types with a byte-identical payload.
///   - `param5` carries 1, 2 and 4 for three different calibrations.
///   - The two motor-relevant calibrations sit in **different** parameters — `param6` and `param7` —
///     so there is no single "motor" parameter to test.
///
/// A policy that needs to distinguish those cases must therefore read the parameter **values** from
/// `AeroraGatedOperation`, which it already receives in full. This type intentionally exposes only
/// the structure; it is not sufficient on its own for a motor-relevance decision, and it does not
/// pretend to be.
///
/// Worse, the meaning is firmware-dependent: the upstream comment at
/// `src/Vehicle/Vehicle.cc:2366` records that `param6 = 1` is a deprecated-but-accepted PX4
/// airspeed value, while the same value is APM motor-interference calibration. Resolving that
/// needs the firmware type, which is **not** part of the authorization context today. Recorded as
/// constraint C4 in ADR-017 rather than guessed at here.
///
struct AeroraCalibrationRequest {
    AeroraCalibrationRequestKind kind = AeroraCalibrationRequestKind::NotCalibration;

    /// Bit *(n-1)* set when `paramN` is non-zero. Zero when \c kind is not \c Start.
    quint8 setParamMask = 0;

    /// 1-based index of the lowest non-zero parameter, or 0 when \c kind is not \c Start.
    int firstSetParamIndex = 0;

    /// True when more than one parameter is non-zero.
    ///
    /// This is reachable, not theoretical: `CalibrationAPMPreFlight` sets `param3`, and
    /// additionally `param1` on multirotor and rover (`src/Vehicle/Vehicle.cc:2378-2385`). A policy
    /// that assumes exactly one parameter is set will mishandle it, and will do so only on some
    /// vehicle types.
    bool multipleParamsSet = false;

    [[nodiscard]] bool isStart() const { return kind == AeroraCalibrationRequestKind::Start; }

    [[nodiscard]] bool isCancelOrNoOp() const
    {
        return kind == AeroraCalibrationRequestKind::CancelOrNoOp;
    }
};

namespace AeroraCalibrationAuthorization {

/// Classify \a operation structurally. Never consults identity, roles or policy.
///
/// A parameter is treated as set when it is not exactly zero. NaN therefore counts as set, which is
/// the conservative direction: a malformed request classifies as \c Start and stays inside the gated
/// set rather than falling through as a cancel.
[[nodiscard]] AeroraCalibrationRequest classify(const AeroraGatedOperation& operation);

/// Whether \a operation is a member of the declared gated-operation set for this workstream.
///
/// Declared explicitly, per `IF-SEC-005`: membership is never derived from UI visibility or from
/// whether a control happens to be reachable. Today this returns true only for
/// `MAV_CMD_PREFLIGHT_CALIBRATION`, because calibration is the only command this workstream has
/// been asked to add; it is not a claim that nothing else should ever be gated.
[[nodiscard]] bool isDeclaredGatedOperation(const AeroraGatedOperation& operation);

/// Whether denying \a request would remove the operator's ability to stop a calibration already in
/// progress.
///
/// True exactly for \c CancelOrNoOp. **This is a warning to the policy author, not an override.**
/// Nothing in this workstream overrides a policy verdict: doing so would be inventing policy, which
/// is precisely what Q-029 forbids until the role model exists. It is surfaced so that a policy
/// which denies the all-zero form does so deliberately and with the consequence recorded.
[[nodiscard]] bool denialWouldBlockCancel(const AeroraCalibrationRequest& request);

/// Short, stable description for audit detail and diagnostics. Contains no role or policy content.
[[nodiscard]] QString describe(const AeroraCalibrationRequest& request);

} // namespace AeroraCalibrationAuthorization

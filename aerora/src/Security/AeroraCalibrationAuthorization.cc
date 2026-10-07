#include "AeroraCalibrationAuthorization.h"

#include <array>

#include "MAVLinkLib.h"

namespace {

/// Exact-zero test. The parameter values that reach here are literal 0, 1 and 2 assigned in
/// `Vehicle::startCalibration()`, so an exact comparison is sufficient and avoids inventing a
/// tolerance that upstream does not use. NaN is not equal to zero and therefore counts as set.
bool paramIsSet(double value)
{
    return value != 0.0;
}

} // namespace

namespace AeroraCalibrationAuthorization {

AeroraCalibrationRequest classify(const AeroraGatedOperation& operation)
{
    AeroraCalibrationRequest request;

    if (operation.mavCommand != MAV_CMD_PREFLIGHT_CALIBRATION) {
        request.kind = AeroraCalibrationRequestKind::NotCalibration;
        return request;
    }

    const std::array<double, 7> params = {
        static_cast<double>(operation.param1),
        static_cast<double>(operation.param2),
        static_cast<double>(operation.param3),
        static_cast<double>(operation.param4),
        operation.param5,
        operation.param6,
        static_cast<double>(operation.param7),
    };

    int setCount = 0;
    for (std::size_t i = 0; i < params.size(); ++i) {
        if (!paramIsSet(params[i])) {
            continue;
        }
        const int oneBasedIndex = static_cast<int>(i) + 1;
        request.setParamMask |= static_cast<quint8>(1u << i);
        if (request.firstSetParamIndex == 0) {
            request.firstSetParamIndex = oneBasedIndex;
        }
        ++setCount;
    }

    request.multipleParamsSet = setCount > 1;
    request.kind = setCount == 0 ? AeroraCalibrationRequestKind::CancelOrNoOp
                                 : AeroraCalibrationRequestKind::Start;
    return request;
}

bool isDeclaredGatedOperation(const AeroraGatedOperation& operation)
{
    return operation.mavCommand == MAV_CMD_PREFLIGHT_CALIBRATION;
}

bool denialWouldBlockCancel(const AeroraCalibrationRequest& request)
{
    return request.isCancelOrNoOp();
}

QString describe(const AeroraCalibrationRequest& request)
{
    switch (request.kind) {
    case AeroraCalibrationRequestKind::NotCalibration:
        return QStringLiteral("not-calibration");
    case AeroraCalibrationRequestKind::CancelOrNoOp:
        return QStringLiteral("calibration:cancel-or-noop");
    case AeroraCalibrationRequestKind::Start:
        return QStringLiteral("calibration:start param%1%2")
            .arg(request.firstSetParamIndex)
            .arg(request.multipleParamsSet ? QStringLiteral("+") : QString());
    }
    return QStringLiteral("calibration:unknown");
}

} // namespace AeroraCalibrationAuthorization

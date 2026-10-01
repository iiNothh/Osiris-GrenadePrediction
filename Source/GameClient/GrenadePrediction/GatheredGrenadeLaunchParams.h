#pragma once

namespace gathered_grenade_launch_params
{
    constexpr float kDegreesToRadians{0.017453292f};
    constexpr float kBaseVelocity{750.0f};
    constexpr float kMinimumVelocity{15.0f};
    constexpr float kVelocityScale{0.9f};
    constexpr float kMinimumStrengthScale{0.3f};
    constexpr float kStrengthVelocityScale{0.7f};
    constexpr float kPitchWrap{360.0f};
    constexpr float kSourceZAdjustment{12.0f};
    constexpr float kEndpointTraceForward{16.0f};
    constexpr float kMovementVelocityScale{1.25f};
}

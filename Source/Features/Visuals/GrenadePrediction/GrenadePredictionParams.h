#pragma once

#include <cstdint>

namespace grenade_prediction_params
{
    constexpr float kSimDt = 1.0f / 64.0f;
    constexpr int kMovementSubsteps = 2;
    constexpr float kMovementSubstepDt = 1.0f / 128.0f;
    constexpr float kGravityScale = 0.40f;
    constexpr float kElasticity = 0.45f;
    constexpr float kStopSpeedSq = 400.0f;
    constexpr float kStopDisplacementSq = 1.42e-14f;
    constexpr float kMolotovSlope = 0.8660254f;
    constexpr float kClipPushOff = 0.03125f;

    constexpr std::uint64_t kInFlightTraceMask = 0x200003001ULL;
    constexpr std::uint8_t kInFlightTraceCollisionGroup = 16;
    constexpr std::uint8_t kInFlightTraceQueryByte = 0x0F;

    constexpr int kMaxTicks = 1154;
    constexpr int kMaxBounces = 20;
    constexpr float kClientTracerHorizonPadding = 0.125f;
    constexpr float kDetonateTimeHeFlash = 1.5f;
    constexpr float kDetonateTimeMolotov = 2.0f;
    constexpr float kDetonateTimeDecoy = 10.0f;
    constexpr float kDetonateTimeSmokeCap = 18.0f;
    constexpr int kTicksPerPoint = 2;
}

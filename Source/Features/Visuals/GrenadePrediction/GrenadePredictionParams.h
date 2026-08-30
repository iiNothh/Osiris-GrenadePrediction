#pragma once

#include <cstdint>

namespace grenade_prediction_params
{
    constexpr float kSimDt = 1.0f / 64.0f;
    constexpr int kMovementSubsteps = 2;
    constexpr float kMovementSubstepDt = kSimDt / kMovementSubsteps;
    constexpr float kDefaultServerGravity = 800.0f;
    constexpr float kGravityScale = 0.40f;
    constexpr float kElasticity = 0.45f;
    constexpr float kStopSpeedSq = 400.0f;
    constexpr float kStopDisplacementSq = 1.42e-14f;
    constexpr float kMolotovSlope = 0.8660254f;
    constexpr float kClipPushOff = 0.03125f;
    constexpr float kSteepFloorDampingNormalZ = 0.7f;
    constexpr float kSteepFloorDampingSpeedSq = 96000.0f;
    constexpr float kSteepFloorDampingDirectionDot = 0.5f;
    constexpr float kSteepFloorDampingScaleBase = 1.5f;

    constexpr int kMaxTicks = 1154;
    constexpr int kMaxBounces = 20;

    constexpr float kBaseThrowVelocity = 750.0f;
    constexpr float kPlayerVelocityScale = 1.25f;
    constexpr float kDefaultEyeHeight = 64.06f;
    constexpr float kSpawnTraceForward = 22.0f;
    constexpr float kSpawnPullBack = 6.0f;
    constexpr float kThrowZOffsetScale = 12.0f;
    constexpr float kClientTracerHorizonPadding = 0.125f;
    constexpr float kDetonateTimeHeFlash = 1.5f;
    constexpr float kDetonateTimeMolotov = 2.0f;
    constexpr float kDetonateTimeDecoy = 10.0f;
    constexpr float kDetonateTimeSmokeCap = 18.0f;
    constexpr int kTicksPerPoint = 2;
}

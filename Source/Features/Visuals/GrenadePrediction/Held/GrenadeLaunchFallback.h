#pragma once

#include <CS2/Classes/Vector.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionParams.h>
#include <GameClient/GrenadePrediction/GrenadeLaunchState.h>
#include <Utils/Optional.h>

template <typename PlayerPawn, typename Simulator>
[[nodiscard]] Optional<GrenadeLaunchState> computeHeldGrenadeLaunchFallback(PlayerPawn&& playerPawn, Simulator&& simulator, float throwStrength, void* skipEntity) noexcept
{
    const auto eyeAngles = playerPawn.eyeAngles();
    const auto origin = playerPawn.absOrigin();
    if (!eyeAngles.hasValue() || !origin.hasValue())
        return {};
    float eyeHeight = grenade_prediction_params::kDefaultEyeHeight;
    if (const auto viewOffset = playerPawn.baseEntity().viewOffset(); viewOffset.hasValue()
        && viewOffset.value().z > 30.0f && viewOffset.value().z < 70.0f)
        eyeHeight = viewOffset.value().z;
    const auto spawn = simulator.computeSpawnPosition(origin.value() + cs2::Vector{0.0f, 0.0f, eyeHeight}, eyeAngles.value(), throwStrength, skipEntity);
    if (!spawn.hasValue())
        return {};
    auto velocity = simulator.computeInitialVelocity(eyeAngles.value(), grenade_prediction_params::kBaseThrowVelocity, throwStrength);
    if (const auto playerVelocity = playerPawn.baseEntity().absVelocity(); playerVelocity.hasValue())
        velocity = velocity + playerVelocity.value() * grenade_prediction_params::kPlayerVelocityScale;
    return GrenadeLaunchState{spawn.value(), velocity};
}

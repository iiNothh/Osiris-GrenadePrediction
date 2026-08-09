#pragma once

#include <CS2/Classes/ConVarTypes.h>
#include <Utils/Math.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionParams.h>

namespace grenade_prediction
{

[[nodiscard]] inline float resolveServerGravity(auto&& cvarSystem) noexcept
{
    const auto gravity = cvarSystem.template getConVarValue<cs2::sv_gravity>();
    return gravity.has_value() && Math::isFinite(*gravity) && *gravity > 0.0f ? *gravity : grenade_prediction_params::kDefaultServerGravity;
}

}

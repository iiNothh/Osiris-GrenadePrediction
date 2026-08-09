#pragma once

#include <CS2/Classes/ConVarTypes.h>
#include <Utils/Math.h>
#include <Utils/Optional.h>

namespace grenade_prediction
{

[[nodiscard]] inline Optional<float> serverGravity(auto&& cvarSystem) noexcept
{
    const auto gravity = cvarSystem.template getConVarValue<cs2::sv_gravity>();
    if (!gravity.has_value() || !Math::isFinite(*gravity) || *gravity <= 0.0f)
        return {};
    return *gravity;
}

}

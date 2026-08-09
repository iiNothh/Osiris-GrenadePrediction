#pragma once

#include <cstdint>

#include <Config/ConfigVariable.h>
#include <Config/RangeConstrainedVariableParams.h>

namespace grenade_prediction_vars
{
    enum class LastTrajectoryVisibilityMode : std::uint8_t { Explode, Always, Off, Custom };

    [[nodiscard]] constexpr LastTrajectoryVisibilityMode normalizeLastTrajectoryVisibilityMode(std::uint8_t value) noexcept
    {
        return value <= static_cast<std::uint8_t>(LastTrajectoryVisibilityMode::Custom)
            ? static_cast<LastTrajectoryVisibilityMode>(value) : LastTrajectoryVisibilityMode::Explode;
    }

    [[nodiscard]] constexpr float normalizeCacheDuration(float value) noexcept
    {
        if (!(value >= 0.0f))
            return 0.0f;
        if (value >= 60.0f)
            return 60.0f;
        return static_cast<float>(static_cast<unsigned int>(value * 10.0f + 0.5f)) * 0.1f;
    }

    CONFIG_VARIABLE(Enabled, bool, false);
    constexpr HueVariableParams kTrajectoryHue{color::HueInteger{0}, color::HueInteger{359}, color::HueInteger{0}};
    constexpr HueVariableParams kBounceHue{color::HueInteger{0}, color::HueInteger{359}, color::HueInteger{120}};
    CONFIG_VARIABLE_HUE(TrajectoryHue, kTrajectoryHue);
    CONFIG_VARIABLE_HUE(BounceHue, kBounceHue);
    constexpr RangeConstrainedVariableParams<float> kCacheDuration{.min = 0.0f, .max = 60.0f, .def = 0.0f};
    CONFIG_VARIABLE_RANGE(CacheDuration, kCacheDuration);
    CONFIG_VARIABLE(LastTrajectoryVisibility, LastTrajectoryVisibilityMode, LastTrajectoryVisibilityMode::Explode);
}

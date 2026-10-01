#pragma once

#include <cstdint>

#include <Config/ConfigVariable.h>
#include <Config/RangeConstrainedVariableParams.h>

namespace grenade_prediction_vars
{
    enum class LastTrajectoryVisibilityMode : std::uint8_t { Explode, Always, Off, Custom };
    constexpr std::uint32_t kCacheDurationSliderScale = 100;
    constexpr auto kCacheDurationSliderIncrement = 1.0f / kCacheDurationSliderScale;
    constexpr auto kCacheDurationSliderDecimalPlaces = 2U;
    constexpr std::uint32_t kTrajectoryThicknessSliderScale = 100;
    constexpr auto kTrajectoryThicknessSliderIncrement = 0.01f;
    constexpr auto kTrajectoryThicknessSliderDecimalPlaces = 2U;
    constexpr RangeConstrainedVariableParams<float> kTrajectoryThickness{.min = 0.5f, .max = 3.0f, .def = 2.0f};

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

        const auto snappedUnits = static_cast<std::uint32_t>(static_cast<double>(value) * static_cast<double>(kCacheDurationSliderScale) + 0.5);
        const auto snappedValue = static_cast<float>(static_cast<double>(snappedUnits) / static_cast<double>(kCacheDurationSliderScale));
        if (snappedValue <= 0.0f)
            return 0.0f;
        if (snappedValue >= 60.0f)
            return 60.0f;
        return snappedValue;
    }

    [[nodiscard]] constexpr float normalizeTrajectoryThickness(float value) noexcept
    {
        if (!(value >= kTrajectoryThickness.min))
            return kTrajectoryThickness.min;
        if (value >= kTrajectoryThickness.max)
            return kTrajectoryThickness.max;

        const auto snappedUnits = static_cast<std::uint32_t>(static_cast<double>(value) * static_cast<double>(kTrajectoryThicknessSliderScale) + 0.5);
        const auto snappedValue = static_cast<float>(static_cast<double>(snappedUnits) / static_cast<double>(kTrajectoryThicknessSliderScale));
        if (snappedValue <= kTrajectoryThickness.min)
            return kTrajectoryThickness.min;
        if (snappedValue >= kTrajectoryThickness.max)
            return kTrajectoryThickness.max;
        return snappedValue;
    }

    CONFIG_VARIABLE(Enabled, bool, false);
    CONFIG_VARIABLE_RANGE(TrajectoryThickness, kTrajectoryThickness);
    constexpr HueVariableParams kTrajectoryHue{color::HueInteger{0}, color::HueInteger{359}, color::HueInteger{0}};
    constexpr HueVariableParams kBounceHue{color::HueInteger{0}, color::HueInteger{359}, color::HueInteger{120}};
    CONFIG_VARIABLE_HUE(TrajectoryHue, kTrajectoryHue);
    CONFIG_VARIABLE_HUE(BounceHue, kBounceHue);
    constexpr RangeConstrainedVariableParams<float> kCacheDuration{.min = 0.0f, .max = 60.0f, .def = 0.0f};
    CONFIG_VARIABLE_RANGE(CacheDuration, kCacheDuration);
    CONFIG_VARIABLE(LastTrajectoryVisibility, LastTrajectoryVisibilityMode, LastTrajectoryVisibilityMode::Explode);
}

#pragma once

#include <cstdint>

#include <GameClient/Panorama/PanoramaUiPanel.h>
#include <GameClient/Panorama/Slider.h>
#include <GameClient/Panorama/TextEntry.h>
#include <Utils/Math.h>

template <typename HookContext>
class FloatSlider {
public:
    FloatSlider(HookContext& hookContext, cs2::CUIPanel* panel, std::uint32_t decimalPlaces) noexcept
        : hookContext{hookContext}
        , panel_{panel}
        , decimalPrecision_{decimalPlaces}
    {
    }

    void updateSlider(float value) const noexcept
    {
        if (!Math::isFinite(value))
            return;

        panel().children()[0].clientPanel().template as<Slider>().setValue(value);
    }

    void updateTextEntry(float value) const noexcept
    {
        char text[32];
        if (!formatFixed(value, decimalPrecision_, text, sizeof(text)))
            (void)formatFixed(0.0f, decimalPrecision_, text, sizeof(text));

        panel().children()[1].clientPanel().template as<TextEntry>().setText(text);
    }

    [[nodiscard]] static bool formatFixed(float value, std::uint32_t decimalPrecision, char* output, std::uint32_t outputSize) noexcept
    {
        // Keep the conversion below inside the exactly representable signed int range.
        std::uint32_t scale = 1;
        for (auto i = 0U; i < decimalPrecision; ++i)
            scale *= 10;

        constexpr std::uint32_t kRequiredOutputSize = 21;
        const auto maximumValue = 214748300.0f / static_cast<float>(scale);
        if (!Math::isFinite(value) || value < -maximumValue || value > maximumValue || outputSize < kRequiredOutputSize)
            return false;

        const auto roundedValue = value * static_cast<float>(scale) + (value < 0.0f ? -0.5f : 0.5f);
        const auto scaledValue = static_cast<std::int32_t>(roundedValue);
        const auto negative = scaledValue < 0;
        const auto magnitude = negative
            ? static_cast<std::uint32_t>(-(scaledValue + 1)) + 1
            : static_cast<std::uint32_t>(scaledValue);

        const auto integerPart = magnitude / scale;
        std::uint32_t divisor = 1;
        while (integerPart / divisor >= 10)
            divisor *= 10;

        auto* position = output;
        if (negative)
            *position++ = '-';
        do {
            *position++ = static_cast<char>('0' + (integerPart / divisor) % 10);
            divisor /= 10;
        } while (divisor != 0);
        if (decimalPrecision != 0) {
            *position++ = '.';
            auto fractionalDivisor = scale / 10;
            while (fractionalDivisor != 0) {
                *position++ = static_cast<char>('0' + (magnitude / fractionalDivisor) % 10);
                fractionalDivisor /= 10;
            }
        }
        *position = '\0';
        return true;
    }

private:
    [[nodiscard]] decltype(auto) panel() const noexcept
    {
        return hookContext.template make<PanoramaUiPanel>(panel_);
    }

    HookContext& hookContext;
    cs2::CUIPanel* panel_;
    std::uint32_t decimalPrecision_;
};

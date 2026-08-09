#pragma once

#include <cstdint>

#include <GameClient/Panorama/PanoramaUiPanel.h>
#include <GameClient/Panorama/Slider.h>
#include <GameClient/Panorama/TextEntry.h>
#include <Utils/Math.h>

template <typename HookContext>
class FloatSlider {
public:
    FloatSlider(HookContext& hookContext, cs2::CUIPanel* panel) noexcept
        : hookContext{hookContext}
        , panel_{panel}
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
        if (!formatFixedOneDecimal(value, text, sizeof(text))) {
            panel().children()[1].clientPanel().template as<TextEntry>().setText("0.0");
            return;
        }

        panel().children()[1].clientPanel().template as<TextEntry>().setText(text);
    }

private:
    [[nodiscard]] static bool formatFixedOneDecimal(float value, char* output, std::uint32_t outputSize) noexcept
    {
        // Keep the conversion below inside the exactly representable signed int range.
        constexpr float kMaximumValue = 214748300.0f;
        constexpr std::uint32_t kRequiredOutputSize = 14;
        if (!Math::isFinite(value) || value < -kMaximumValue || value > kMaximumValue || outputSize < kRequiredOutputSize)
            return false;

        const auto roundedValue = value * 10.0f + (value < 0.0f ? -0.5f : 0.5f);
        const auto scaledValue = static_cast<std::int32_t>(roundedValue);
        const auto negative = scaledValue < 0;
        const auto magnitude = negative
            ? static_cast<std::uint32_t>(-(scaledValue + 1)) + 1
            : static_cast<std::uint32_t>(scaledValue);

        const auto integerPart = magnitude / 10;
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
        *position++ = '.';
        *position++ = static_cast<char>('0' + magnitude % 10);
        *position = '\0';
        return true;
    }

    [[nodiscard]] decltype(auto) panel() const noexcept
    {
        return hookContext.template make<PanoramaUiPanel>(panel_);
    }

    HookContext& hookContext;
    cs2::CUIPanel* panel_;
};

#pragma once

#include <charconv>
#include <limits>

#include <GameClient/Panorama/PanoramaUiPanel.h>
#include <GameClient/Panorama/Slider.h>
#include <GameClient/Panorama/TextEntry.h>

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
        panel().children()[0].clientPanel().template as<Slider>().setValue(value);
    }

    void updateTextEntry(float value) const noexcept
    {
        if (value != value || value > (std::numeric_limits<float>::max)() || value < -(std::numeric_limits<float>::max)()) {
            panel().children()[1].clientPanel().template as<TextEntry>().setText("0.0");
            return;
        }

        char text[64];
        const auto result = std::to_chars(text, text + sizeof(text) - 1, value, std::chars_format::fixed, 1);
        if (result.ec != std::errc{}) {
            panel().children()[1].clientPanel().template as<TextEntry>().setText("0.0");
            return;
        }

        *result.ptr = '\0';
        panel().children()[1].clientPanel().template as<TextEntry>().setText(text);
    }

private:
    [[nodiscard]] decltype(auto) panel() const noexcept
    {
        return hookContext.template make<PanoramaUiPanel>(panel_);
    }

    HookContext& hookContext;
    cs2::CUIPanel* panel_;
};

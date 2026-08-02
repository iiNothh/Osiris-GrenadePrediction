#pragma once

#include <cstddef>

#include <CS2/Classes/Color.h>
#include <CS2/Panorama/CUIPanel.h>
#include <Features/Visuals/GrenadePrediction/Simulation/GrenadeTrajectory.h>
#include <GameClient/Panorama/PanelHandle.h>
#include <GameClient/Panorama/PanoramaUiPanel.h>
#include <GameClient/WorldToScreen/WorldToClipSpaceConverter.h>
#include <Utils/Optional.h>

#include "GrenadePredictionRendererState.h"
#include "GrenadeTrajectoryLine.h"

namespace grenade_prediction
{

struct GrenadePredictionRenderStyle {
    cs2::Color lineColor{255, 255, 255, 220};
    cs2::Color bounceColor{255, 190, 60, 255};
    cs2::Color endColor{255, 80, 80, 255};
    float lineWidth{2.0f};
    float markerSize{8.0f};
};

template <typename HookContext>
class GrenadePredictionRenderer {
public:
    GrenadePredictionRenderer(HookContext& hookContext, GrenadePredictionRendererState& state) noexcept
        : hookContext{hookContext}
        , state{state}
    {
    }

    void render(cs2::CUIPanel* parent, const GrenadeTrajectory& trajectory, const GrenadePredictionRenderStyle& style, float aspectRatio) noexcept
    {
        hideUnused(state.activeLineCount, GrenadeTrajectory::kPointCapacity, state.linePanels);
        hideUnused(state.activeBounceCount, GrenadeTrajectory::kBounceCapacity, state.bouncePanels);
        hidePanel(state.endPanel);
        state.activeLineCount = 0;
        state.activeBounceCount = 0;
        state.activeEndMarker = false;
        if (!parent || !trajectory.valid)
            return;

        for (std::size_t i = 1; i < trajectory.pointCount; ++i) {
            const auto line = createLine(parent, trajectory.points[i - 1], trajectory.points[i], style, aspectRatio, state.linePanels[state.activeLineCount]);
            if (line)
                ++state.activeLineCount;
        }
        for (std::size_t i = 0; i < trajectory.bounceCount; ++i) {
            if (createMarker(parent, trajectory.bounces[i], style.bounceColor, style.markerSize, aspectRatio, state.bouncePanels[state.activeBounceCount]))
                ++state.activeBounceCount;
        }
        state.activeEndMarker = createMarker(parent, trajectory.endPosition, style.endColor, style.markerSize, aspectRatio, state.endPanel);
    }

    void clear() noexcept
    {
        hideUnused(0, GrenadeTrajectory::kPointCapacity, state.linePanels);
        hideUnused(0, GrenadeTrajectory::kBounceCapacity, state.bouncePanels);
        hidePanel(state.endPanel);
        state.activeLineCount = 0;
        state.activeBounceCount = 0;
        state.activeEndMarker = false;
    }

private:
    [[nodiscard]] bool createLine(cs2::CUIPanel* parent, const cs2::Vector& start, const cs2::Vector& end, const GrenadePredictionRenderStyle& style, float aspectRatio, cs2::PanelHandle& handle) noexcept
    {
        const auto projected = project(start, end, aspectRatio);
        if (!projected)
            return false;
        auto panel = getOrCreate(parent, handle);
        if (!panel)
            return false;
        panel.setBackgroundColor(style.lineColor);
        panel.setTransformOrigin(cs2::CUILength::percent(0.0f), cs2::CUILength::percent(50.0f));
        panel.setRotate2dCentered(projected->angleDegrees());
        const auto left = projected->start.x < projected->end.x ? projected->start.x : projected->end.x;
        const auto right = projected->start.x < projected->end.x ? projected->end.x : projected->start.x;
        panel.setPosition(cs2::CUILength::percent((left + 1.0f) * 50.0f), cs2::CUILength::percent((1.0f - projected->start.y) * 50.0f));
        panel.setWidth(cs2::CUILength::percent((right - left) * 50.0f));
        panel.setHeight(cs2::CUILength::pixels(style.lineWidth));
        panel.show();
        return true;
    }

    [[nodiscard]] bool createMarker(cs2::CUIPanel* parent, const cs2::Vector& position, cs2::Color color, float size, float aspectRatio, cs2::PanelHandle& handle) noexcept
    {
        const auto point = projectPoint(position, aspectRatio);
        if (!point)
            return false;
        auto panel = getOrCreate(parent, handle);
        if (!panel)
            return false;
        panel.setBackgroundColor(color);
        panel.setBorderRadius(cs2::CUILength::pixels(size * 0.5f));
        panel.setWidth(cs2::CUILength::pixels(size));
        panel.setHeight(cs2::CUILength::pixels(size));
        panel.setTransformOrigin(cs2::CUILength::percent(50.0f), cs2::CUILength::percent(50.0f));
        panel.setPosition(cs2::CUILength::percent((point->x + 1.0f) * 50.0f), cs2::CUILength::percent((1.0f - point->y) * 50.0f));
        panel.show();
        return true;
    }

    [[nodiscard]] auto project(const cs2::Vector& start, const cs2::Vector& end, float aspectRatio) const noexcept
    {
        return projectWith(start, end, [aspectRatio, this](const cs2::Vector& point) {
            auto clip = hookContext.template make<WorldToClipSpaceConverter>().toClipSpace(point);
            clip.x *= aspectRatio;
            return clip;
        });
    }

    [[nodiscard]] auto projectPoint(const cs2::Vector& point, float aspectRatio) const noexcept
    {
        const auto clip = hookContext.template make<WorldToClipSpaceConverter>().toClipSpace(point);
        if (!clip.onScreen())
            return Optional<GrenadeProjectedPoint>{};
        return Optional<GrenadeProjectedPoint>{GrenadeProjectedPoint{clip.x / clip.w * aspectRatio, clip.y / clip.w, clip.z / clip.w}};
    }

    template <typename Projector>
    [[nodiscard]] Optional<GrenadeTrajectoryLine> projectWith(const cs2::Vector& start, const cs2::Vector& end, Projector&& projector) const noexcept
    {
        GrenadeTrajectoryLine line{};
        if (!GrenadeTrajectoryLineClipper::project(start, end, projector, line))
            return {};
        return line;
    }

    [[nodiscard]] auto getOrCreate(cs2::CUIPanel* parent, cs2::PanelHandle& handle) const noexcept
    {
        auto panel = hookContext.template make<PanelHandle>(handle);
        if (!panel.panelExists()) {
            auto created = hookContext.panelFactory().createPanel(parent).uiPanel();
            handle = created.getHandle();
            return created;
        }
        return panel.get();
    }

    void hidePanel(cs2::PanelHandle handle) const noexcept
    {
        if (auto panel = hookContext.template make<PanelHandle>(handle); panel.panelExists())
            panel.get().hide();
    }

    void hideUnused(std::size_t active, std::size_t capacity, cs2::PanelHandle* handles) const noexcept
    {
        for (std::size_t i = active; i < capacity; ++i)
            hidePanel(handles[i]);
    }

    HookContext& hookContext;
    GrenadePredictionRendererState& state;
};

}

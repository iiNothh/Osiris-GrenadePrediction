#pragma once

#include <CS2/Panorama/CUIEngine.h>
#include <GameClient/Panorama/PanelAlignmentParams.h>
#include <GameClient/Panorama/PanelHandle.h>
#include <GameClient/Panorama/PanoramaTransformations.h>
#include <GameClient/WorldToScreen/ViewToProjectionMatrix.h>
#include <GameClient/WorldToScreen/WorldToClipSpaceConverter.h>
#include <Utils/ColorUtils.h>
#include <Utils/Lvalue.h>
#include <Utils/Math.h>

#include <Features/Visuals/GrenadePrediction/Rendering/TrajectoryLineSegment.h>
#include <Features/Visuals/GrenadePrediction/Rendering/TrajectoryRenderPlan.h>
#include <Features/Visuals/GrenadePrediction/GrenadeTrajectoryPresentationState.h>
#include <Features/Visuals/GrenadePrediction/Trajectory.h>

template <typename HookContext>
class GrenadeTrajectoryRenderer {
public:
    explicit GrenadeTrajectoryRenderer(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void hide(cs2::PanelHandle& containerPanelHandle) noexcept
    {
        auto&& panel = hookContext.template make<PanelHandle>(containerPanelHandle).get();
        if (panel)
            panel.setVisible(false);
    }

    void draw(const auto& trajectory, cs2::PanelHandle& containerPanelHandle, GrenadeTrajectoryPresentationState& presentationState,
        auto&& parentPanel, color::Hue trajectoryHue, color::Hue bounceHue) noexcept
    {
        if (!trajectory.valid || trajectory.pointsCount < 2 || trajectory.pointsCount > kMaxPoints
            || trajectory.markersCount < 0 || trajectory.markersCount > kMaxMarkers) {
            hide(containerPanelHandle);
            return;
        }

        auto&& containerPanel = getContainerPanel(containerPanelHandle, parentPanel);
        if (!containerPanel) {
            hide(containerPanelHandle);
            return;
        }
        containerPanel.setVisible(true);

        auto childrenProxy = containerPanel.children();
        int childCount = (childrenProxy.vector && childrenProxy.vector->memory) ? childrenProxy.vector->size : 0;
        TrajectoryRenderPlan renderPlan;
        renderPlan.build(trajectory);
        const int segmentCount = renderPlan.lineSegmentCount;
        const int neededPanels = segmentCount + trajectory.markersCount + (trajectory.validLanding ? 1 : 0);
        if (childCount < 0 || childCount > kMaxPanels || neededPanels > kMaxPanels) {
            containerPanel.setVisible(false);
            return;
        }

        bool panelsCreated = false;
        while (childCount < neededPanels) {
            auto&& panel = hookContext.panelFactory().createPanel(containerPanel).uiPanel();
            if (!panel) {
                containerPanel.setVisible(false);
                return;
            }
            panel.setTransformOrigin(cs2::CUILength::percent(50.0f), cs2::CUILength::percent(50.0f));
            panel.setVisible(false);
            ++childCount;
            panelsCreated = true;
        }

        auto children = containerPanel.children();
        if (!hasChildren(children, neededPanels)) {
            containerPanel.setVisible(false);
            return;
        }
        childCount = children.vector->size;
        auto converter = hookContext.template make<WorldToClipSpaceConverter>();
        const float aspectRatio = hookContext.template make<ViewToProjectionMatrix<HookContext>>().getAspectRatio();
        if (!Math::isFinite(aspectRatio) || aspectRatio <= kNearW) {
            containerPanel.setVisible(false);
            return;
        }

        const float trajectoryHueValue = static_cast<float>(trajectoryHue);
        const float bounceHueValue = static_cast<float>(bounceHue);
        const bool updateStyles = panelsCreated || !presentationState.panelStyle.initialized
            || presentationState.panelStyle.lineSegmentCount != segmentCount
            || presentationState.panelStyle.markersCount != trajectory.markersCount
            || presentationState.panelStyle.validLanding != trajectory.validLanding
            || presentationState.panelStyle.trajectoryHue != trajectoryHueValue
            || presentationState.panelStyle.bounceHue != bounceHueValue;

        const auto trajectoryColor = color::HSBtoRGB(trajectoryHue, color::Saturation{1.0f}, color::Brightness{1.0f}).setAlpha(255);
        const auto bounceColor = color::HSBtoRGB(bounceHue, color::Saturation{1.0f}, color::Brightness{1.0f}).setAlpha(255);
        const auto endMarkerColor = color::HSBtoRGB(color::Hue{30.0f / 360.0f}, color::Saturation{1.0f}, color::Brightness{1.0f}).setAlpha(255);

        if (updateStyles) {
            constexpr PanelAlignmentParams kCenteredAlignment{
                .horizontalAlignment = cs2::k_EHorizontalAlignmentCenter,
                .verticalAlignment = cs2::k_EVerticalAlignmentCenter};
            for (int i = 0; i < neededPanels; ++i)
                children[i].setAlign(kCenteredAlignment);
        }

        int index = 0;
        auto previousPoint = converter.toClipSpace(trajectory.points[renderPlan.pointIndex(0)]);
        for (int i = 0; i < segmentCount; ++i) {
            auto child = children[index++];
            if (updateStyles) {
                child.setHeight(cs2::CUILength::pixels(kTrajectoryLineThickness));
                child.setBackgroundColor(trajectoryColor);
            }
            TrajectoryLineSegment segment{};
            const auto nextPoint = converter.toClipSpace(trajectory.points[renderPlan.pointIndex(i + 1)]);
            if (TrajectoryLineSegment::fromClipSpace(previousPoint, nextPoint, aspectRatio, segment)) {
                child.setWidth(cs2::CUILength::percent(segment.width));
                child.setRotate2dCentered(segment.angleDegrees);
                child.setVisible(true);
                PanoramaTransformations{hookContext.panoramaTransformFactory().translate(
                    cs2::CUILength::percent(segment.midpointX - 50.0f), cs2::CUILength::percent(segment.midpointY - 50.0f))}.applyTo(child);
            } else {
                child.setVisible(false);
            }
            previousPoint = nextPoint;
        }

        for (int i = 0; i < trajectory.markersCount; ++i) {
            auto child = children[index++];
            if (updateStyles) {
                child.setWidth(cs2::CUILength::pixels(kBounceDotSize));
                child.setHeight(cs2::CUILength::pixels(kBounceDotSize));
                child.setRotate2dCentered(0.0f);
                child.setBackgroundColor(bounceColor);
            }
            const auto pointIndex = trajectory.markers[i].pointIndex;
            if (pointIndex < 0 || pointIndex >= trajectory.pointsCount) {
                child.setVisible(false);
                continue;
            }
            setMarkerVisibility(child, converter.toClipSpace(trajectory.points[pointIndex]));
        }

        if (trajectory.validLanding) {
            auto child = children[index++];
            if (updateStyles) {
                child.setWidth(cs2::CUILength::pixels(kEndMarkerSize));
                child.setHeight(cs2::CUILength::pixels(kEndMarkerSize));
                child.setRotate2dCentered(0.0f);
                child.setBackgroundColor(endMarkerColor);
            }
            setMarkerVisibility(child, converter.toClipSpace(trajectory.endPos));
        }

        while (index < presentationState.activePanelCount && index < childCount)
            children[index++].setVisible(false);
        presentationState.activePanelCount = neededPanels;
        presentationState.panelStyle = {segmentCount, trajectory.markersCount, trajectory.validLanding, trajectoryHueValue, bounceHueValue, true};
    }

private:
    static constexpr int kMaxPoints = Trajectory::kPointsCapacity;
    static constexpr int kMaxMarkers = Trajectory::kMarkersCapacity;
    static constexpr int kMaxPanels = TrajectoryRenderPlan::kPanelCapacity;
    static constexpr float kNearW = TrajectoryLineSegment::kNearW;
    static constexpr float kTrajectoryLineThickness = 2.0f;
    static constexpr float kBounceDotSize = 8.0f;
    static constexpr float kEndMarkerSize = 10.0f;

    [[nodiscard]] static bool hasChildren(const auto& children, int neededPanels) noexcept
    {
        return children.vector && children.vector->memory && children.vector->size >= neededPanels && children.vector->size <= kMaxPanels;
    }

    void setMarkerVisibility(auto&& panel, ClipSpaceCoordinates clipSpace) noexcept
    {
        if (!Math::isFinite(clipSpace.x) || !Math::isFinite(clipSpace.y) || !Math::isFinite(clipSpace.z) || !Math::isFinite(clipSpace.w)
            || clipSpace.w < kNearW || clipSpace.x < -clipSpace.w || clipSpace.x > clipSpace.w
            || clipSpace.y < -clipSpace.w || clipSpace.y > clipSpace.w) {
            panel.setVisible(false);
            return;
        }

        const float x = (clipSpace.x / clipSpace.w + 1.0f) * 50.0f;
        const float y = (1.0f - clipSpace.y / clipSpace.w) * 50.0f;
        if (!Math::isFinite(x) || !Math::isFinite(y)) {
            panel.setVisible(false);
            return;
        }

        panel.setVisible(true);
        PanoramaTransformations{hookContext.panoramaTransformFactory().translate(
            cs2::CUILength::percent(x - 50.0f), cs2::CUILength::percent(y - 50.0f))}.applyTo(panel);
    }

    [[nodiscard]] decltype(auto) getContainerPanel(cs2::PanelHandle& containerPanelHandle, auto&& parentPanel) noexcept
    {
        return hookContext.template make<PanelHandle>(containerPanelHandle).getOrInit([&]() -> decltype(auto) {
            auto&& panel = hookContext.panelFactory().createPanel(parentPanel).uiPanel();
            if (panel)
                panel.fitParent();
            return utils::lvalue<decltype(panel)>(panel);
        });
    }

    HookContext& hookContext;
};

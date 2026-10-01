#include <array>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include <CS2/Classes/Color.h>
#include <CS2/Classes/Vector.h>
#include <CS2/Panorama/CUILength.h>
#include <CS2/Panorama/PanelHandle.h>
#include <CS2/Panorama/Transform3D.h>
#include <Features/Visuals/GrenadePrediction/Rendering/GrenadeTrajectoryRenderer.h>
#include <GameClient/Panorama/PanelAlignmentParams.h>
#include <GameClient/WorldToScreen/ClipSpaceCoordinates.h>
#include <Utils/Optional.h>

namespace
{

enum class PanelMutation {
    SetAlign,
    SetHeight,
    SetWidth,
    SetBackgroundColor,
    SetScale2d
};

struct PanelMutationEvent {
    int panelIndex{};
    PanelMutation mutation{};
    cs2::CUILength length{};
    cs2::Color color{0, 0, 0};
    float scaleX{};
    float scaleY{};
};

struct PanelMutationRecorder {
    static constexpr int kCapacity = 64;

    void record(PanelMutationEvent event) noexcept
    {
        if (eventCount < kCapacity)
            events[eventCount++] = event;
    }

    [[nodiscard]] int mutationCount(PanelMutation mutation) const noexcept
    {
        int count = 0;
        for (int i = 0; i < eventCount; ++i) {
            if (events[i].mutation == mutation)
                ++count;
        }
        return count;
    }

    [[nodiscard]] std::optional<cs2::CUILength> lastLength(int panelIndex, PanelMutation mutation) const noexcept
    {
        for (int i = eventCount - 1; i >= 0; --i) {
            if (events[i].panelIndex == panelIndex && events[i].mutation == mutation)
                return events[i].length;
        }
        return {};
    }

    [[nodiscard]] std::optional<float> lastScaleY(int panelIndex) const noexcept
    {
        for (int i = eventCount - 1; i >= 0; --i) {
            if (events[i].panelIndex == panelIndex && events[i].mutation == PanelMutation::SetScale2d)
                return events[i].scaleY;
        }
        return {};
    }

    [[nodiscard]] std::optional<float> lastScaleX(int panelIndex) const noexcept
    {
        for (int i = eventCount - 1; i >= 0; --i) {
            if (events[i].panelIndex == panelIndex && events[i].mutation == PanelMutation::SetScale2d)
                return events[i].scaleX;
        }
        return {};
    }

    std::array<PanelMutationEvent, kCapacity> events{};
    int eventCount{};
};

enum class RenderEventKind {
    ContainerVisibility,
    ChildMutation
};

struct RenderEvent {
    RenderEventKind kind{};
    bool visible{};
};

struct RenderEventRecorder {
    static constexpr int kCapacity = 64;

    void record(RenderEvent event) noexcept
    {
        if (eventCount < kCapacity)
            events[eventCount++] = event;
    }

    std::array<RenderEvent, kCapacity> events{};
    int eventCount{};
};

struct ScriptedPanelState {
    bool visible{};
    int setVisibleCalls{};
    int setTransformOriginCalls{};
    int setAlignCalls{};
    int setHeightCalls{};
    int setWidthCalls{};
    int setBackgroundColorCalls{};
    int setScale2dCalls{};
    int actualLayoutHeightQueries{};
    int setRotate2dCalls{};
    float rotate2dDegrees{};
    int setTransform3DCalls{};
    int fitParentCalls{};
    cs2::CUILength height{};
    cs2::CUILength width{};
    cs2::Color backgroundColor{0, 0, 0};
    Optional<float> actualLayoutHeight{};
    float scaleX{1.0f};
    float scaleY{1.0f};
    bool scaleSubmissionSucceeds{true};
};

struct ScriptedPanel {
    struct ChildrenVector {
        int size{};
        ScriptedPanel** memory{};
    };

    struct Children {
        ChildrenVector* vector{};

        [[nodiscard]] ScriptedPanel& operator[](int index) const noexcept
        {
            return *vector->memory[index];
        }
    };

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return available;
    }

    void setVisible(bool newVisible) noexcept
    {
        sharedState->visible = newVisible;
        ++sharedState->setVisibleCalls;
        if (renderEventRecorder) {
            if (isContainer)
                renderEventRecorder->record({RenderEventKind::ContainerVisibility, newVisible});
            else
                renderEventRecorder->record({RenderEventKind::ChildMutation, false});
        }
    }

    void setTransformOrigin(cs2::CUILength, cs2::CUILength) noexcept
    {
        ++sharedState->setTransformOriginCalls;
    }

    void setAlign(const PanelAlignmentParams&) noexcept
    {
        ++sharedState->setAlignCalls;
        recordMutation(PanelMutation::SetAlign);
    }

    void setHeight(cs2::CUILength newHeight) noexcept
    {
        sharedState->height = newHeight;
        ++sharedState->setHeightCalls;
        recordMutation(PanelMutation::SetHeight, newHeight);
        recordChildMutation();
    }

    void setWidth(cs2::CUILength newWidth) noexcept
    {
        sharedState->width = newWidth;
        ++sharedState->setWidthCalls;
        recordMutation(PanelMutation::SetWidth, newWidth);
        recordChildMutation();
    }

    void setBackgroundColor(cs2::Color newBackgroundColor) noexcept
    {
        sharedState->backgroundColor = newBackgroundColor;
        ++sharedState->setBackgroundColorCalls;
        recordMutation(PanelMutation::SetBackgroundColor, {}, newBackgroundColor);
    }

    [[nodiscard]] bool setScale2dCentered(float x, float y) noexcept
    {
        ++sharedState->setScale2dCalls;
        sharedState->scaleX = x;
        sharedState->scaleY = y;
        recordMutation(PanelMutation::SetScale2d, {}, cs2::Color{0, 0, 0}, x, y);
        recordChildMutation();
        return sharedState->scaleSubmissionSucceeds;
    }

    [[nodiscard]] Optional<float> getActualLayoutHeight() const noexcept
    {
        ++sharedState->actualLayoutHeightQueries;
        return sharedState->actualLayoutHeight;
    }

    void setRotate2dCentered(float degrees) noexcept
    {
        ++sharedState->setRotate2dCalls;
        sharedState->rotate2dDegrees = degrees;
        recordChildMutation();
    }

    template <typename T>
    void setTransform3D(const T&) noexcept
    {
        ++sharedState->setTransform3DCalls;
        recordChildMutation();
    }

    void fitParent() noexcept
    {
        ++sharedState->fitParentCalls;
    }

    [[nodiscard]] cs2::PanelHandle getHandle() const noexcept
    {
        return {.panelIndex = 1, .serialNumber = 1};
    }

    [[nodiscard]] Children children() noexcept
    {
        return {.vector = childrenVector};
    }

    void recordMutation(PanelMutation mutation, cs2::CUILength length = {}, cs2::Color color = {0, 0, 0}, float scaleX = 0.0f,
        float scaleY = 0.0f) noexcept
    {
        if (mutationRecorder)
            mutationRecorder->record({.panelIndex = panelIndex, .mutation = mutation, .length = length, .color = color,
                .scaleX = scaleX, .scaleY = scaleY});
    }

    void recordChildMutation() noexcept
    {
        if (renderEventRecorder && !isContainer)
            renderEventRecorder->record({RenderEventKind::ChildMutation, false});
    }

    bool available{true};
    ScriptedPanelState state{};
    ScriptedPanelState* sharedState{&state};
    bool& visible{sharedState->visible};
    int& setVisibleCalls{sharedState->setVisibleCalls};
    int& setTransformOriginCalls{sharedState->setTransformOriginCalls};
    int& setAlignCalls{sharedState->setAlignCalls};
    int& setHeightCalls{sharedState->setHeightCalls};
    int& setWidthCalls{sharedState->setWidthCalls};
    int& setBackgroundColorCalls{sharedState->setBackgroundColorCalls};
    int& setScale2dCalls{sharedState->setScale2dCalls};
    int& actualLayoutHeightQueries{sharedState->actualLayoutHeightQueries};
    int& setRotate2dCalls{sharedState->setRotate2dCalls};
    float& rotate2dDegrees{sharedState->rotate2dDegrees};
    int& setTransform3DCalls{sharedState->setTransform3DCalls};
    int& fitParentCalls{sharedState->fitParentCalls};
    cs2::CUILength& height{sharedState->height};
    cs2::CUILength& width{sharedState->width};
    cs2::Color& backgroundColor{sharedState->backgroundColor};
    Optional<float>& actualLayoutHeight{sharedState->actualLayoutHeight};
    float& scaleX{sharedState->scaleX};
    float& scaleY{sharedState->scaleY};
    bool& scaleSubmissionSucceeds{sharedState->scaleSubmissionSucceeds};
    ChildrenVector* childrenVector{};
    PanelMutationRecorder* mutationRecorder{};
    RenderEventRecorder* renderEventRecorder{};
    int panelIndex{};
    bool isContainer{};
};

struct ScriptedClientPanel {
    ScriptedPanel* panel{};

    [[nodiscard]] ScriptedPanel& uiPanel() const noexcept
    {
        return *panel;
    }
};

struct ScriptedGrenadeTrajectoryContext;

struct ScriptedPanelHandle {
    ScriptedGrenadeTrajectoryContext& context;

    [[nodiscard]] ScriptedPanel& get() const noexcept;

    template <typename F>
    [[nodiscard]] ScriptedPanel& getOrInit(F&& createPanel) const noexcept;
};

struct ScriptedPanelFactory {
    ScriptedGrenadeTrajectoryContext& context;

    template <typename ParentPanel>
    [[nodiscard]] ScriptedClientPanel& createPanel(ParentPanel&) const noexcept;
};

struct ScriptedWorldToClipSpaceConverter {
    [[nodiscard]] ClipSpaceCoordinates toClipSpace(const cs2::Vector& point) const noexcept
    {
        return {.x = point.x, .y = point.y, .z = 0.0f, .w = 1.0f};
    }
};

struct ScriptedViewToProjectionMatrix {
    [[nodiscard]] float getAspectRatio() const noexcept
    {
        return 1.0f;
    }
};

struct ScriptedPanoramaTransformFactory {
    [[nodiscard]] cs2::CTransform3D* translate(cs2::CUILength x, cs2::CUILength y) noexcept
    {
        ++translateCalls;
        lastTranslationX = x;
        lastTranslationY = y;
        return &transform;
    }

    cs2::CTransform3D transform{};
    int translateCalls{};
    cs2::CUILength lastTranslationX{};
    cs2::CUILength lastTranslationY{};
};

struct ScriptedGrenadeTrajectoryContext {
    static constexpr int kPanelCapacity = 8;

    ScriptedGrenadeTrajectoryContext() noexcept
        : panelFactoryStorage{*this}
    {
        container.childrenVector = &children;
        container.isContainer = true;
        container.renderEventRecorder = &renderEventRecorder;
        for (int i = 0; i < kPanelCapacity; ++i) {
            childPointers[i] = &childPanels[i];
            childPanels[i].mutationRecorder = &panelMutationRecorder;
            childPanels[i].renderEventRecorder = &renderEventRecorder;
            childPanels[i].panelIndex = i;
        }
        children.memory = childPointers.data();
    }

    void makeContainerAvailable(int childCount = 0) noexcept
    {
        containerAvailable = true;
        children.size = childCount;
    }

    void setActualLayoutHeight(float height) noexcept
    {
        for (int i = 0; i < children.size; ++i)
            childPanels[i].actualLayoutHeight = height;
    }

    [[nodiscard]] ScriptedPanelFactory& panelFactory() noexcept
    {
        return panelFactoryStorage;
    }

    [[nodiscard]] ScriptedPanoramaTransformFactory& panoramaTransformFactory() noexcept
    {
        return panoramaTransformFactoryStorage;
    }

    template <typename T>
    [[nodiscard]] ScriptedViewToProjectionMatrix make() noexcept
    {
        static_assert(std::is_same_v<T, ViewToProjectionMatrix<ScriptedGrenadeTrajectoryContext>>);
        return {};
    }

    template <template <typename> typename T, typename... Args>
    [[nodiscard]] decltype(auto) make(Args&&...) noexcept
    {
        if constexpr (std::is_same_v<T<ScriptedGrenadeTrajectoryContext>, PanelHandle<ScriptedGrenadeTrajectoryContext>>) {
            return ScriptedPanelHandle{*this};
        } else if constexpr (std::is_same_v<T<ScriptedGrenadeTrajectoryContext>, WorldToClipSpaceConverter<ScriptedGrenadeTrajectoryContext>>) {
            return ScriptedWorldToClipSpaceConverter{};
        } else {
            static_assert(std::is_same_v<T<ScriptedGrenadeTrajectoryContext>, ViewToProjectionMatrix<ScriptedGrenadeTrajectoryContext>>);
            return ScriptedViewToProjectionMatrix{};
        }
    }

    ScriptedPanel parentPanel;
    ScriptedPanel container;
    ScriptedPanel unavailablePanel{.available = false};
    std::array<ScriptedPanel, kPanelCapacity> childPanels{};
    std::array<ScriptedPanel*, kPanelCapacity> childPointers{};
    ScriptedPanel::ChildrenVector children{};
    PanelMutationRecorder panelMutationRecorder;
    RenderEventRecorder renderEventRecorder;
    ScriptedClientPanel clientPanel{};
    ScriptedPanelFactory panelFactoryStorage;
    ScriptedPanoramaTransformFactory panoramaTransformFactoryStorage;
    bool containerAvailable{};
    bool failContainerCreation{};
    int createPanelCalls{};
};

[[nodiscard]] ScriptedPanel& ScriptedPanelHandle::get() const noexcept
{
    return context.containerAvailable ? context.container : context.unavailablePanel;
}

template <typename F>
[[nodiscard]] ScriptedPanel& ScriptedPanelHandle::getOrInit(F&& createPanel) const noexcept
{
    auto& panel = get();
    if (panel)
        return panel;
    return std::forward<F>(createPanel)();
}

template <typename ParentPanel>
[[nodiscard]] ScriptedClientPanel& ScriptedPanelFactory::createPanel(ParentPanel&) const noexcept
{
    ++context.createPanelCalls;
    if (!context.containerAvailable) {
        if (context.failContainerCreation)
            context.clientPanel.panel = &context.unavailablePanel;
        else {
            context.containerAvailable = true;
            context.clientPanel.panel = &context.container;
        }
        return context.clientPanel;
    }

    context.clientPanel.panel = &context.childPanels[context.children.size];
    ++context.children.size;
    return context.clientPanel;
}

class GrenadeTrajectoryRendererTest : public testing::Test {
protected:
    [[nodiscard]] Trajectory validTrajectory(int pointsCount = 2) const noexcept
    {
        Trajectory trajectory;
        trajectory.valid = true;
        trajectory.pointsCount = pointsCount;
        for (int i = 0; i < pointsCount; ++i)
            trajectory.points[i] = {.x = 0.1f * i, .y = 0.1f * i, .z = 0.0f};
        trajectory.endPos = {.x = 0.25f, .y = 0.25f, .z = 0.0f};
        return trajectory;
    }

    void draw(const Trajectory& trajectory, bool hideWhileUpdating = false, float trajectoryThickness = 2.0f) noexcept
    {
        renderer.draw(trajectory, containerPanelHandle, presentationState, context.parentPanel,
            color::Hue{0.25f}, color::Hue{0.5f}, trajectoryThickness, hideWhileUpdating);
    }

    void drawWithHues(const Trajectory& trajectory, float trajectoryThickness, color::Hue trajectoryHue, color::Hue bounceHue) noexcept
    {
        renderer.draw(trajectory, containerPanelHandle, presentationState, context.parentPanel,
            trajectoryHue, bounceHue, trajectoryThickness);
    }

    void setLayoutHeight(float height) noexcept
    {
        context.setActualLayoutHeight(height);
    }

    ScriptedGrenadeTrajectoryContext context;
    GrenadeTrajectoryRenderer<ScriptedGrenadeTrajectoryContext> renderer{context};
    cs2::PanelHandle containerPanelHandle{};
    GrenadeTrajectoryPresentationState presentationState;
};

TEST_F(GrenadeTrajectoryRendererTest, InvalidTrajectoryHidesExistingContainer)
{
    context.makeContainerAvailable();
    context.container.visible = true;
    auto trajectory = validTrajectory();
    trajectory.valid = false;

    draw(trajectory);

    EXPECT_FALSE(context.container.visible);
    EXPECT_EQ(context.container.setVisibleCalls, 1);
    EXPECT_EQ(context.createPanelCalls, 0);
}

TEST_F(GrenadeTrajectoryRendererTest, ContainerCreationFailureDoesNotCreateChildPanels)
{
    context.failContainerCreation = true;

    draw(validTrajectory());

    EXPECT_EQ(context.createPanelCalls, 1);
    EXPECT_EQ(context.children.size, 0);
    EXPECT_EQ(context.container.fitParentCalls, 0);
    EXPECT_EQ(presentationState.activePanelCount, 0);
}

TEST_F(GrenadeTrajectoryRendererTest, CreatesOnlyPanelsMissingFromExistingContainer)
{
    context.makeContainerAvailable(2);

    draw(validTrajectory(3));

    EXPECT_EQ(context.createPanelCalls, 1);
    EXPECT_EQ(context.children.size, 3);
    EXPECT_EQ(context.childPanels[0].setTransformOriginCalls, 0);
    EXPECT_EQ(context.childPanels[1].setTransformOriginCalls, 0);
    EXPECT_EQ(context.childPanels[2].setTransformOriginCalls, 1);
}

TEST_F(GrenadeTrajectoryRendererTest, ReusesCachedStylesWhenPresentationIsUnchanged)
{
    context.makeContainerAvailable();
    const auto trajectory = validTrajectory(3);

    draw(trajectory);

    const auto alignMutations = context.panelMutationRecorder.mutationCount(PanelMutation::SetAlign);
    const auto heightMutations = context.panelMutationRecorder.mutationCount(PanelMutation::SetHeight);
    const auto backgroundColorMutations = context.panelMutationRecorder.mutationCount(PanelMutation::SetBackgroundColor);
    EXPECT_EQ(alignMutations, 3);
    EXPECT_EQ(heightMutations, 3);
    EXPECT_EQ(backgroundColorMutations, 3);

    draw(trajectory);

    EXPECT_EQ(context.createPanelCalls, 3);
    EXPECT_EQ(context.panelMutationRecorder.mutationCount(PanelMutation::SetAlign), alignMutations);
    EXPECT_EQ(context.panelMutationRecorder.mutationCount(PanelMutation::SetHeight), heightMutations);
    EXPECT_EQ(context.panelMutationRecorder.mutationCount(PanelMutation::SetBackgroundColor), backgroundColorMutations);
}

class GrenadeTrajectoryThicknessRendererTest : public GrenadeTrajectoryRendererTest, public testing::WithParamInterface<bool> {};

TEST_P(GrenadeTrajectoryThicknessRendererTest, ReappliesScaleWhenOnlyThicknessChanges)
{
    context.makeContainerAvailable();
    auto trajectory = validTrajectory(3);
    trajectory.markersCount = 1;
    trajectory.markers[0] = {.pointIndex = 1};
    draw(trajectory, GetParam(), 2.0f);
    const auto lineWidth = context.panelMutationRecorder.lastLength(0, PanelMutation::SetWidth);
    ASSERT_TRUE(lineWidth.has_value());
    const auto heightMutations = context.panelMutationRecorder.mutationCount(PanelMutation::SetHeight);
    const auto colorMutations = context.panelMutationRecorder.mutationCount(PanelMutation::SetBackgroundColor);
    const auto createdPanels = context.createPanelCalls;
    const auto scaleMutations = context.panelMutationRecorder.mutationCount(PanelMutation::SetScale2d);

    draw(trajectory, GetParam(), 1.23f);

    EXPECT_EQ(context.createPanelCalls, createdPanels);
    EXPECT_EQ(context.panelMutationRecorder.mutationCount(PanelMutation::SetHeight), heightMutations);
    EXPECT_EQ(context.panelMutationRecorder.mutationCount(PanelMutation::SetBackgroundColor), colorMutations);
    EXPECT_EQ(context.panelMutationRecorder.lastLength(0, PanelMutation::SetWidth), lineWidth);
    EXPECT_EQ(context.panelMutationRecorder.lastLength(2, PanelMutation::SetHeight), cs2::CUILength::pixels(8.0f));
    EXPECT_EQ(context.panelMutationRecorder.lastLength(3, PanelMutation::SetHeight), cs2::CUILength::pixels(10.0f));
    EXPECT_EQ(context.panelMutationRecorder.mutationCount(PanelMutation::SetScale2d), scaleMutations + 4);
    EXPECT_FLOAT_EQ(presentationState.panelStyle.trajectoryThickness, 1.23f);
    EXPECT_TRUE(context.container.visible);
}

TEST_P(GrenadeTrajectoryThicknessRendererTest, ReusesCachedStylesWhenThicknessIsUnchanged)
{
    context.makeContainerAvailable();
    const auto trajectory = validTrajectory(3);
    draw(trajectory, GetParam(), 1.23f);
    const auto alignMutations = context.panelMutationRecorder.mutationCount(PanelMutation::SetAlign);
    const auto heightMutations = context.panelMutationRecorder.mutationCount(PanelMutation::SetHeight);
    const auto colorMutations = context.panelMutationRecorder.mutationCount(PanelMutation::SetBackgroundColor);
    const auto createdPanels = context.createPanelCalls;
    const auto scaleMutations = context.panelMutationRecorder.mutationCount(PanelMutation::SetScale2d);

    draw(trajectory, GetParam(), 1.23f);

    EXPECT_EQ(context.createPanelCalls, createdPanels);
    EXPECT_EQ(context.panelMutationRecorder.mutationCount(PanelMutation::SetAlign), alignMutations);
    EXPECT_EQ(context.panelMutationRecorder.mutationCount(PanelMutation::SetHeight), heightMutations);
    EXPECT_EQ(context.panelMutationRecorder.mutationCount(PanelMutation::SetBackgroundColor), colorMutations);
    EXPECT_EQ(context.panelMutationRecorder.mutationCount(PanelMutation::SetScale2d), scaleMutations + 3);
    EXPECT_EQ(context.panelMutationRecorder.lastLength(0, PanelMutation::SetHeight), cs2::CUILength::pixels(4.0f));
    EXPECT_FLOAT_EQ(presentationState.panelStyle.trajectoryThickness, 1.23f);
}

INSTANTIATE_TEST_SUITE_P(LiveAndCachedPresentation, GrenadeTrajectoryThicknessRendererTest, testing::Bool());

struct TrajectoryThicknessCase {
    float thickness;
    float actualHeight;
};

class GrenadeTrajectoryThicknessBoundsRendererTest : public GrenadeTrajectoryRendererTest,
    public testing::WithParamInterface<TrajectoryThicknessCase> {};

TEST_P(GrenadeTrajectoryThicknessBoundsRendererTest, ScalesFixedCssBaseToRequestedRenderThickness)
{
    const auto params = GetParam();
    context.makeContainerAvailable();
    draw(validTrajectory(3), false, params.thickness);

    EXPECT_EQ(context.panelMutationRecorder.lastLength(0, PanelMutation::SetHeight), cs2::CUILength::pixels(4.0f));
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(0).value(), 0.0f);

    setLayoutHeight(params.actualHeight);
    draw(validTrajectory(3), false, params.thickness);

    ASSERT_TRUE(context.panelMutationRecorder.lastScaleY(0).has_value());
    ASSERT_TRUE(context.panelMutationRecorder.lastScaleY(1).has_value());
    ASSERT_TRUE(context.panelMutationRecorder.lastScaleX(0).has_value());
    ASSERT_TRUE(context.panelMutationRecorder.lastScaleX(1).has_value());
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(0).value(), params.thickness / params.actualHeight);
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(1).value(), params.thickness / params.actualHeight);
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleX(0).value(), 1.0f);
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleX(1).value(), 1.0f);
    EXPECT_EQ(context.panelMutationRecorder.lastLength(0, PanelMutation::SetHeight), cs2::CUILength::pixels(4.0f));
    EXPECT_EQ(context.panelMutationRecorder.lastLength(1, PanelMutation::SetHeight), cs2::CUILength::pixels(4.0f));
    EXPECT_EQ(context.panelMutationRecorder.lastLength(2, PanelMutation::SetHeight), cs2::CUILength::pixels(10.0f));
    EXPECT_FLOAT_EQ(presentationState.panelStyle.trajectoryThickness, params.thickness);
}

INSTANTIATE_TEST_SUITE_P(BoundsAndDefault, GrenadeTrajectoryThicknessBoundsRendererTest, testing::Values(
    TrajectoryThicknessCase{0.01f, 6.0f}, TrajectoryThicknessCase{0.5f, 3.0f}, TrajectoryThicknessCase{0.6f, 5.0f},
    TrajectoryThicknessCase{0.75f, 6.0f}, TrajectoryThicknessCase{0.9f, 2.0f}, TrajectoryThicknessCase{0.99f, 5.0f},
    TrajectoryThicknessCase{1.0f, 6.0f}, TrajectoryThicknessCase{1.01f, 3.0f}, TrajectoryThicknessCase{2.0f, 6.0f},
    TrajectoryThicknessCase{3.0f, 5.0f}));

TEST_F(GrenadeTrajectoryRendererTest, ReappliesScaleWhenActualHeightChangesWithoutRestyling)
{
    context.makeContainerAvailable();
    draw(validTrajectory(3), false, 2.0f);
    setLayoutHeight(4.0f);
    draw(validTrajectory(3), false, 2.0f);
    const auto heightMutations = context.panelMutationRecorder.mutationCount(PanelMutation::SetHeight);
    const auto colorMutations = context.panelMutationRecorder.mutationCount(PanelMutation::SetBackgroundColor);
    const auto scaleMutations = context.panelMutationRecorder.mutationCount(PanelMutation::SetScale2d);

    setLayoutHeight(6.0f);
    draw(validTrajectory(3), false, 2.0f);

    EXPECT_EQ(context.panelMutationRecorder.mutationCount(PanelMutation::SetHeight), heightMutations);
    EXPECT_EQ(context.panelMutationRecorder.mutationCount(PanelMutation::SetBackgroundColor), colorMutations);
    EXPECT_EQ(context.panelMutationRecorder.mutationCount(PanelMutation::SetScale2d), scaleMutations + 3);
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(0).value(), 2.0f / 6.0f);
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(1).value(), 2.0f / 6.0f);
}

TEST_F(GrenadeTrajectoryRendererTest, PreparationIgnoresStaleHeightAndZeroHeightRecoversAfterLayout)
{
    context.makeContainerAvailable(2);
    context.childPanels[0].actualLayoutHeight = 5.0f;
    context.childPanels[1].actualLayoutHeight = 5.0f;

    draw(validTrajectory(3), false, 0.75f);

    EXPECT_EQ(context.childPanels[0].actualLayoutHeightQueries, 0);
    EXPECT_EQ(context.childPanels[1].actualLayoutHeightQueries, 0);
    EXPECT_EQ(context.panelMutationRecorder.lastLength(0, PanelMutation::SetHeight), cs2::CUILength::pixels(4.0f));
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(0).value(), 0.0f);
    EXPECT_TRUE(context.childPanels[0].visible);

    setLayoutHeight(0.0f);
    draw(validTrajectory(3), false, 0.75f);

    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(0).value(), 0.0f);
    EXPECT_TRUE(context.childPanels[0].visible);

    setLayoutHeight(6.0f);
    draw(validTrajectory(3), false, 0.75f);

    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(0).value(), 0.75f / 6.0f);
    EXPECT_TRUE(context.childPanels[0].visible);
}

TEST_F(GrenadeTrajectoryRendererTest, MissingAndInvalidHeightsUseVisibleZeroScaleAndRecover)
{
    context.makeContainerAvailable();
    draw(validTrajectory(3));
    const std::array invalidHeights{0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()};
    for (const float height : invalidHeights) {
        setLayoutHeight(height);
        draw(validTrajectory(3));
        ASSERT_TRUE(context.panelMutationRecorder.lastScaleY(0).has_value());
        EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(0).value(), 0.0f);
        EXPECT_TRUE(context.childPanels[0].visible);
    }

    for (int i = 0; i < context.children.size; ++i)
        context.childPanels[i].actualLayoutHeight = Optional<float>{};
    draw(validTrajectory(3));
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(0).value(), 0.0f);
    EXPECT_TRUE(context.childPanels[0].visible);

    setLayoutHeight(6.0f);
    draw(validTrajectory(3));
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(0).value(), 2.0f / 6.0f);
    EXPECT_TRUE(context.childPanels[0].visible);
}

TEST_F(GrenadeTrajectoryRendererTest, HidesLineWhenScaleSubmissionFailsAndRetriesOnNextDraw)
{
    context.makeContainerAvailable();
    draw(validTrajectory(3));
    setLayoutHeight(6.0f);
    context.childPanels[0].scaleSubmissionSucceeds = false;
    const int widthMutations = context.childPanels[0].setWidthCalls;

    draw(validTrajectory(3));

    EXPECT_FALSE(context.childPanels[0].visible);
    EXPECT_EQ(context.childPanels[0].setWidthCalls, widthMutations + 1);
    EXPECT_TRUE(context.container.visible);

    context.childPanels[0].scaleSubmissionSucceeds = true;
    draw(validTrajectory(3));

    EXPECT_TRUE(context.childPanels[0].visible);
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(0).value(), 2.0f / 6.0f);
}

TEST_F(GrenadeTrajectoryRendererTest, InvalidThicknessUsesZeroScaleWithoutHidingLine)
{
    context.makeContainerAvailable();
    draw(validTrajectory(3), false, 2.0f);
    setLayoutHeight(6.0f);

    draw(validTrajectory(3), false, std::numeric_limits<float>::infinity());

    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(0).value(), 0.0f);
    EXPECT_TRUE(context.childPanels[0].visible);
}

TEST_F(GrenadeTrajectoryRendererTest, ReusedPanelsResetScaleAcrossMarkerLineUnusedRoles)
{
    context.makeContainerAvailable();
    auto markerTrajectory = validTrajectory(2);
    markerTrajectory.validLanding = false;
    markerTrajectory.markersCount = 1;
    markerTrajectory.markers[0] = {.pointIndex = 1};
    draw(markerTrajectory);

    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(1).value(), 1.0f);
    EXPECT_EQ(context.childPanels[1].actualLayoutHeightQueries, 0);
    const int markerHeightMutations = context.childPanels[1].setHeightCalls;

    setLayoutHeight(6.0f);
    auto lineTrajectory = validTrajectory(3);
    lineTrajectory.validLanding = false;
    draw(lineTrajectory);

    EXPECT_EQ(context.childPanels[1].setHeightCalls, markerHeightMutations + 1);
    EXPECT_EQ(context.panelMutationRecorder.lastLength(1, PanelMutation::SetHeight), cs2::CUILength::pixels(4.0f));
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(1).value(), 0.0f);
    EXPECT_EQ(context.childPanels[1].actualLayoutHeightQueries, 0);

    auto singleLineTrajectory = validTrajectory(2);
    singleLineTrajectory.validLanding = false;
    draw(singleLineTrajectory);
    EXPECT_FALSE(context.childPanels[1].visible);

    draw(markerTrajectory);

    EXPECT_TRUE(context.childPanels[1].visible);
    EXPECT_FLOAT_EQ(context.childPanels[1].scaleX, 1.0f);
    EXPECT_FLOAT_EQ(context.childPanels[1].scaleY, 1.0f);
    EXPECT_EQ(context.childPanels[1].actualLayoutHeightQueries, 0);
    EXPECT_EQ(context.childPanels[1].width, cs2::CUILength::pixels(8.0f));
    EXPECT_EQ(context.childPanels[1].height, cs2::CUILength::pixels(8.0f));
    EXPECT_FLOAT_EQ(context.childPanels[1].rotate2dDegrees, 0.0f);
}

TEST_F(GrenadeTrajectoryRendererTest, AddingChildAndChangingThicknessOrHueDoesNotReprepareExistingLines)
{
    context.makeContainerAvailable(1);
    auto singleLineTrajectory = validTrajectory(2);
    singleLineTrajectory.validLanding = false;
    draw(singleLineTrajectory);
    const int existingLineHeightMutations = context.childPanels[0].setHeightCalls;
    setLayoutHeight(6.0f);
    draw(singleLineTrajectory);

    auto twoLineTrajectory = validTrajectory(3);
    twoLineTrajectory.validLanding = false;
    drawWithHues(twoLineTrajectory, 0.75f, color::Hue{0.8f}, color::Hue{0.1f});

    EXPECT_EQ(context.childPanels[0].setHeightCalls, existingLineHeightMutations);
    EXPECT_EQ(context.childPanels[1].setHeightCalls, 1);
    EXPECT_EQ(context.childPanels[1].height, cs2::CUILength::pixels(4.0f));
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(0).value(), 0.75f / 6.0f);

    setLayoutHeight(6.0f);
    const int firstLineHeightMutations = context.childPanels[0].setHeightCalls;
    const int secondLineHeightMutations = context.childPanels[1].setHeightCalls;
    drawWithHues(twoLineTrajectory, 1.25f, color::Hue{0.6f}, color::Hue{0.2f});

    EXPECT_EQ(context.childPanels[0].setHeightCalls, firstLineHeightMutations);
    EXPECT_EQ(context.childPanels[1].setHeightCalls, secondLineHeightMutations);
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(0).value(), 1.25f / 6.0f);
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.lastScaleY(1).value(), 1.25f / 6.0f);
    const auto trajectoryColor = color::HSBtoRGB(color::Hue{0.6f}, color::Saturation{1.0f}, color::Brightness{1.0f}).setAlpha(255);
    EXPECT_EQ(context.childPanels[0].backgroundColor, trajectoryColor);
}

TEST_F(GrenadeTrajectoryRendererTest, MarkerScaleFailureHidesMarkerUntilResetSucceeds)
{
    context.makeContainerAvailable();
    context.childPanels[1].scaleSubmissionSucceeds = false;
    auto trajectory = validTrajectory(2);
    trajectory.validLanding = false;
    trajectory.markersCount = 1;
    trajectory.markers[0] = {.pointIndex = 1};

    draw(trajectory);

    EXPECT_FALSE(context.childPanels[1].visible);
    EXPECT_EQ(context.childPanels[1].actualLayoutHeightQueries, 0);

    context.childPanels[1].scaleSubmissionSucceeds = true;
    draw(trajectory);

    EXPECT_TRUE(context.childPanels[1].visible);
    EXPECT_FLOAT_EQ(context.childPanels[1].scaleX, 1.0f);
    EXPECT_FLOAT_EQ(context.childPanels[1].scaleY, 1.0f);
    EXPECT_EQ(context.childPanels[1].actualLayoutHeightQueries, 0);
}

TEST_F(GrenadeTrajectoryRendererTest, KeepsSegmentWidthNinetyDegreeRotationAndMidpointTranslation)
{
    context.makeContainerAvailable();
    auto trajectory = validTrajectory();
    trajectory.validLanding = false;
    trajectory.points[0] = {.x = 0.0f, .y = 0.5f, .z = 0.0f};
    trajectory.points[1] = {.x = 0.0f, .y = -0.5f, .z = 0.0f};

    draw(trajectory, false, 0.75f);

    EXPECT_EQ(context.panelMutationRecorder.lastLength(0, PanelMutation::SetWidth), cs2::CUILength::percent(50.0f));
    EXPECT_EQ(context.childPanels[0].setRotate2dCalls, 1);
    EXPECT_FLOAT_EQ(context.childPanels[0].rotate2dDegrees, 90.0f);
    EXPECT_EQ(context.childPanels[0].setTransform3DCalls, 1);
    EXPECT_EQ(context.panoramaTransformFactoryStorage.translateCalls, 1);
    EXPECT_EQ(context.panoramaTransformFactoryStorage.lastTranslationX, cs2::CUILength::percent(0.0f));
    EXPECT_EQ(context.panoramaTransformFactoryStorage.lastTranslationY, cs2::CUILength::percent(0.0f));
}

TEST_F(GrenadeTrajectoryRendererTest, AssignsLineBounceAndLandingPanelsInOrder)
{
    context.makeContainerAvailable();
    auto trajectory = validTrajectory(3);
    trajectory.markersCount = 1;
    trajectory.markers[0] = {.pointIndex = 1};

    draw(trajectory);

    EXPECT_EQ(context.children.size, 4);
    constexpr std::array expectedMutations{
        PanelMutationEvent{.panelIndex = 0, .mutation = PanelMutation::SetAlign},
        PanelMutationEvent{.panelIndex = 1, .mutation = PanelMutation::SetAlign},
        PanelMutationEvent{.panelIndex = 2, .mutation = PanelMutation::SetAlign},
        PanelMutationEvent{.panelIndex = 3, .mutation = PanelMutation::SetAlign},
        PanelMutationEvent{.panelIndex = 0, .mutation = PanelMutation::SetHeight},
        PanelMutationEvent{.panelIndex = 0, .mutation = PanelMutation::SetBackgroundColor},
        PanelMutationEvent{.panelIndex = 0, .mutation = PanelMutation::SetScale2d},
        PanelMutationEvent{.panelIndex = 0, .mutation = PanelMutation::SetWidth},
        PanelMutationEvent{.panelIndex = 1, .mutation = PanelMutation::SetHeight},
        PanelMutationEvent{.panelIndex = 1, .mutation = PanelMutation::SetBackgroundColor},
        PanelMutationEvent{.panelIndex = 1, .mutation = PanelMutation::SetScale2d},
        PanelMutationEvent{.panelIndex = 1, .mutation = PanelMutation::SetWidth},
        PanelMutationEvent{.panelIndex = 2, .mutation = PanelMutation::SetScale2d},
        PanelMutationEvent{.panelIndex = 2, .mutation = PanelMutation::SetWidth},
        PanelMutationEvent{.panelIndex = 2, .mutation = PanelMutation::SetHeight},
        PanelMutationEvent{.panelIndex = 2, .mutation = PanelMutation::SetBackgroundColor},
        PanelMutationEvent{.panelIndex = 3, .mutation = PanelMutation::SetScale2d},
        PanelMutationEvent{.panelIndex = 3, .mutation = PanelMutation::SetWidth},
        PanelMutationEvent{.panelIndex = 3, .mutation = PanelMutation::SetHeight},
        PanelMutationEvent{.panelIndex = 3, .mutation = PanelMutation::SetBackgroundColor}};
    ASSERT_EQ(context.panelMutationRecorder.eventCount, static_cast<int>(expectedMutations.size()));
    for (int i = 0; i < context.panelMutationRecorder.eventCount; ++i) {
        EXPECT_EQ(context.panelMutationRecorder.events[i].panelIndex, expectedMutations[i].panelIndex);
        EXPECT_EQ(context.panelMutationRecorder.events[i].mutation, expectedMutations[i].mutation);
    }

    const auto trajectoryColor = color::HSBtoRGB(color::Hue{0.25f}, color::Saturation{1.0f}, color::Brightness{1.0f}).setAlpha(255);
    const auto bounceColor = color::HSBtoRGB(color::Hue{0.5f}, color::Saturation{1.0f}, color::Brightness{1.0f}).setAlpha(255);
    const auto landingColor = color::HSBtoRGB(color::Hue{30.0f / 360.0f}, color::Saturation{1.0f}, color::Brightness{1.0f}).setAlpha(255);
    EXPECT_EQ(context.panelMutationRecorder.events[4].length, cs2::CUILength::pixels(4.0f));
    EXPECT_EQ(context.panelMutationRecorder.events[5].color, trajectoryColor);
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.events[6].scaleY, 0.0f);
    EXPECT_EQ(context.panelMutationRecorder.events[8].length, cs2::CUILength::pixels(4.0f));
    EXPECT_EQ(context.panelMutationRecorder.events[9].color, trajectoryColor);
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.events[10].scaleY, 0.0f);
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.events[12].scaleY, 1.0f);
    EXPECT_EQ(context.panelMutationRecorder.events[14].length, cs2::CUILength::pixels(8.0f));
    EXPECT_EQ(context.panelMutationRecorder.events[15].color, bounceColor);
    EXPECT_FLOAT_EQ(context.panelMutationRecorder.events[16].scaleY, 1.0f);
    EXPECT_EQ(context.panelMutationRecorder.events[18].length, cs2::CUILength::pixels(10.0f));
    EXPECT_EQ(context.panelMutationRecorder.events[19].color, landingColor);
}

TEST_F(GrenadeTrajectoryRendererTest, HidesPanelsNoLongerUsedByTheTrajectory)
{
    context.makeContainerAvailable(4);
    presentationState.activePanelCount = 4;

    draw(validTrajectory());

    EXPECT_FALSE(context.childPanels[2].visible);
    EXPECT_FALSE(context.childPanels[3].visible);
    EXPECT_EQ(context.childPanels[2].setVisibleCalls, 1);
    EXPECT_EQ(context.childPanels[3].setVisibleCalls, 1);
    EXPECT_EQ(presentationState.activePanelCount, 2);
}

TEST_F(GrenadeTrajectoryRendererTest, HidesReusedContainerUntilReplacementPanelsAreUpdated)
{
    context.makeContainerAvailable(2);
    context.container.visible = true;
    context.childPanels[0].visible = true;
    context.childPanels[1].visible = true;

    draw(validTrajectory(3), true);

    const auto& events = context.renderEventRecorder;
    ASSERT_GE(events.eventCount, 3);
    EXPECT_EQ(events.events[0].kind, RenderEventKind::ContainerVisibility);
    EXPECT_FALSE(events.events[0].visible);
    EXPECT_EQ(events.events[events.eventCount - 1].kind, RenderEventKind::ContainerVisibility);
    EXPECT_TRUE(events.events[events.eventCount - 1].visible);
    EXPECT_TRUE(context.container.visible);
    bool childUpdatedWhileHidden = false;
    for (int i = 1; i < events.eventCount - 1; ++i) {
        if (events.events[i].kind == RenderEventKind::ChildMutation)
            childUpdatedWhileHidden = true;
        else
            EXPECT_FALSE(events.events[i].visible);
    }
    EXPECT_TRUE(childUpdatedWhileHidden);
}

}

#include <array>
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

namespace
{

enum class PanelMutation {
    SetAlign,
    SetHeight,
    SetWidth,
    SetBackgroundColor
};

struct PanelMutationEvent {
    int panelIndex{};
    PanelMutation mutation{};
    cs2::CUILength length{};
    cs2::Color color{0, 0, 0};
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

    std::array<PanelMutationEvent, kCapacity> events{};
    int eventCount{};
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
        visible = newVisible;
        ++setVisibleCalls;
    }

    void setTransformOrigin(cs2::CUILength, cs2::CUILength) noexcept
    {
        ++setTransformOriginCalls;
    }

    void setAlign(const PanelAlignmentParams&) noexcept
    {
        ++setAlignCalls;
        recordMutation(PanelMutation::SetAlign);
    }

    void setHeight(cs2::CUILength newHeight) noexcept
    {
        height = newHeight;
        ++setHeightCalls;
        recordMutation(PanelMutation::SetHeight, newHeight);
    }

    void setWidth(cs2::CUILength newWidth) noexcept
    {
        width = newWidth;
        ++setWidthCalls;
        recordMutation(PanelMutation::SetWidth, newWidth);
    }

    void setBackgroundColor(cs2::Color newBackgroundColor) noexcept
    {
        backgroundColor = newBackgroundColor;
        ++setBackgroundColorCalls;
        recordMutation(PanelMutation::SetBackgroundColor, {}, newBackgroundColor);
    }

    void setRotate2dCentered(float) noexcept
    {
        ++setRotate2dCalls;
    }

    template <typename T>
    void setTransform3D(const T&) noexcept
    {
        ++setTransform3DCalls;
    }

    void fitParent() noexcept
    {
        ++fitParentCalls;
    }

    [[nodiscard]] cs2::PanelHandle getHandle() const noexcept
    {
        return {.panelIndex = 1, .serialNumber = 1};
    }

    [[nodiscard]] Children children() noexcept
    {
        return {.vector = childrenVector};
    }

    void recordMutation(PanelMutation mutation, cs2::CUILength length = {}, cs2::Color color = {0, 0, 0}) noexcept
    {
        if (mutationRecorder)
            mutationRecorder->record({.panelIndex = panelIndex, .mutation = mutation, .length = length, .color = color});
    }

    bool available{true};
    bool visible{};
    int setVisibleCalls{};
    int setTransformOriginCalls{};
    int setAlignCalls{};
    int setHeightCalls{};
    int setWidthCalls{};
    int setBackgroundColorCalls{};
    int setRotate2dCalls{};
    int setTransform3DCalls{};
    int fitParentCalls{};
    cs2::CUILength height{};
    cs2::CUILength width{};
    cs2::Color backgroundColor{0, 0, 0};
    ChildrenVector* childrenVector{};
    PanelMutationRecorder* mutationRecorder{};
    int panelIndex{};
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
    [[nodiscard]] cs2::CTransform3D* translate(cs2::CUILength, cs2::CUILength) noexcept
    {
        ++translateCalls;
        return &transform;
    }

    cs2::CTransform3D transform{};
    int translateCalls{};
};

struct ScriptedGrenadeTrajectoryContext {
    static constexpr int kPanelCapacity = 8;

    ScriptedGrenadeTrajectoryContext() noexcept
        : panelFactoryStorage{*this}
    {
        container.childrenVector = &children;
        for (int i = 0; i < kPanelCapacity; ++i) {
            childPointers[i] = &childPanels[i];
            childPanels[i].mutationRecorder = &panelMutationRecorder;
            childPanels[i].panelIndex = i;
        }
        children.memory = childPointers.data();
    }

    void makeContainerAvailable(int childCount = 0) noexcept
    {
        containerAvailable = true;
        children.size = childCount;
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

    void draw(const Trajectory& trajectory) noexcept
    {
        renderer.draw(trajectory, containerPanelHandle, presentationState, context.parentPanel, color::Hue{0.25f}, color::Hue{0.5f});
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
        PanelMutationEvent{.panelIndex = 0, .mutation = PanelMutation::SetWidth},
        PanelMutationEvent{.panelIndex = 1, .mutation = PanelMutation::SetHeight},
        PanelMutationEvent{.panelIndex = 1, .mutation = PanelMutation::SetBackgroundColor},
        PanelMutationEvent{.panelIndex = 1, .mutation = PanelMutation::SetWidth},
        PanelMutationEvent{.panelIndex = 2, .mutation = PanelMutation::SetWidth},
        PanelMutationEvent{.panelIndex = 2, .mutation = PanelMutation::SetHeight},
        PanelMutationEvent{.panelIndex = 2, .mutation = PanelMutation::SetBackgroundColor},
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
    EXPECT_EQ(context.panelMutationRecorder.events[4].length, cs2::CUILength::pixels(2.0f));
    EXPECT_EQ(context.panelMutationRecorder.events[5].color, trajectoryColor);
    EXPECT_EQ(context.panelMutationRecorder.events[7].length, cs2::CUILength::pixels(2.0f));
    EXPECT_EQ(context.panelMutationRecorder.events[8].color, trajectoryColor);
    EXPECT_EQ(context.panelMutationRecorder.events[10].length, cs2::CUILength::pixels(8.0f));
    EXPECT_EQ(context.panelMutationRecorder.events[11].length, cs2::CUILength::pixels(8.0f));
    EXPECT_EQ(context.panelMutationRecorder.events[12].color, bounceColor);
    EXPECT_EQ(context.panelMutationRecorder.events[13].length, cs2::CUILength::pixels(10.0f));
    EXPECT_EQ(context.panelMutationRecorder.events[14].length, cs2::CUILength::pixels(10.0f));
    EXPECT_EQ(context.panelMutationRecorder.events[15].color, landingColor);
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

}

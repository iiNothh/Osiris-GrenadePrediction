#include <type_traits>

#include <gtest/gtest.h>

#include <CS2/Classes/EntitySystem/CEntityHandle.h>
#include <CS2/Panorama/PanelHandle.h>
#include <Features/FeaturesStates.h>
#include <GameClient/Entities/GrenadeKind.h>
#include <Features/Visuals/GrenadePrediction/GrenadePrediction.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionContext.h>
#include <Features/Visuals/GrenadePrediction/Rendering/GrenadeTrajectoryRenderer.h>
#include <GameClient/Panorama/PanoramaUiEngine.h>

namespace
{

struct GrenadePredictionUnloadRendererRecorder {
    cs2::PanelHandle* hiddenPanels[2]{};
    int hideCalls{};
};

struct GrenadePredictionUnloadRenderer {
    GrenadePredictionUnloadRendererRecorder& recorder;

    void hide(cs2::PanelHandle& panelHandle) noexcept
    {
        if (recorder.hideCalls < 2)
            recorder.hiddenPanels[recorder.hideCalls] = &panelHandle;
        ++recorder.hideCalls;
    }
};

struct GrenadePredictionUnloadUiEngine {
    cs2::PanelHandle deletedPanels[2]{};
    int deleteCalls{};

    void deletePanelByHandle(cs2::PanelHandle panelHandle) noexcept
    {
        if (deleteCalls < 2)
            deletedPanels[deleteCalls] = panelHandle;
        ++deleteCalls;
    }
};

struct GrenadePredictionUnloadTestContext {
    GrenadePredictionUnloadTestContext() noexcept
        : grenadePredictionContext{*this}
        , renderer{rendererRecorder}
    {
    }

    [[nodiscard]] FeaturesStates& featuresStates() noexcept { return featuresStatesStorage; }

    template <template <typename> typename T, typename... Args>
    [[nodiscard]] decltype(auto) make(Args&&...) noexcept
    {
        if constexpr (std::is_same_v<T<GrenadePredictionUnloadTestContext>, GrenadePredictionContext<GrenadePredictionUnloadTestContext>>) {
            return (grenadePredictionContext);
        } else if constexpr (std::is_same_v<T<GrenadePredictionUnloadTestContext>, GrenadeTrajectoryRenderer<GrenadePredictionUnloadTestContext>>) {
            return (renderer);
        } else {
            static_assert(std::is_same_v<T<GrenadePredictionUnloadTestContext>, PanoramaUiEngine<GrenadePredictionUnloadTestContext>>);
            return (uiEngine);
        }
    }

    FeaturesStates featuresStatesStorage{};
    GrenadePredictionContext<GrenadePredictionUnloadTestContext> grenadePredictionContext;
    GrenadePredictionUnloadRendererRecorder rendererRecorder;
    GrenadePredictionUnloadRenderer renderer;
    GrenadePredictionUnloadUiEngine uiEngine;
};

TEST(GrenadePredictionTest, OnUnloadClearsStateAndDeletesPanels)
{
    GrenadePredictionUnloadTestContext context;
    auto& state = context.featuresStatesStorage.visualFeaturesStates.grenadePredictionState;
    const cs2::CEntityHandle localPawn{1};
    const cs2::CEntityHandle projectile{2};
    const cs2::PanelHandle livePanel{.panelIndex = 1, .serialNumber = 2};
    const cs2::PanelHandle cachedPanel{.panelIndex = 3, .serialNumber = 4};
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.lastCommittedTrajectory.valid = true;
    state.lastCommittedTrajectory.pointsCount = 1;
    state.liveGrenadeTrajectoryScratch.valid = true;
    state.liveGrenadeTrajectoryScratch.pointsCount = 1;
    state.liveContainerPanelHandle = livePanel;
    state.lastCacheContainerPanelHandle = cachedPanel;
    state.livePresentationState.activePanelCount = 1;
    state.livePresentationState.panelStyle.initialized = true;
    state.lastCachePresentationState.activePanelCount = 1;
    state.lastCachePresentationState.panelStyle.initialized = true;
    ASSERT_TRUE(state.liveGrenadeCache.upsert({projectile, localPawn, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, GrenadeKind::HEGrenade}));

    GrenadePrediction<GrenadePredictionUnloadTestContext>{context}.onUnload();

    EXPECT_EQ(context.rendererRecorder.hideCalls, 2);
    EXPECT_EQ(context.rendererRecorder.hiddenPanels[0], &state.liveContainerPanelHandle);
    EXPECT_EQ(context.rendererRecorder.hiddenPanels[1], &state.lastCacheContainerPanelHandle);
    EXPECT_EQ(context.uiEngine.deleteCalls, 2);
    EXPECT_EQ(context.uiEngine.deletedPanels[0], livePanel);
    EXPECT_EQ(context.uiEngine.deletedPanels[1], cachedPanel);
    EXPECT_FALSE(state.tempTrajectory.valid);
    EXPECT_FALSE(state.lastCommittedTrajectory.valid);
    EXPECT_FALSE(state.liveGrenadeTrajectoryScratch.valid);
    EXPECT_FALSE(state.liveGrenadeCache.newestForThrower(localPawn).hasValue());
    EXPECT_FALSE(state.liveContainerPanelHandle.isValid());
    EXPECT_FALSE(state.lastCacheContainerPanelHandle.isValid());
    EXPECT_EQ(state.livePresentationState.activePanelCount, 0);
    EXPECT_FALSE(state.livePresentationState.panelStyle.initialized);
    EXPECT_EQ(state.lastCachePresentationState.activePanelCount, 0);
    EXPECT_FALSE(state.lastCachePresentationState.panelStyle.initialized);
}

}

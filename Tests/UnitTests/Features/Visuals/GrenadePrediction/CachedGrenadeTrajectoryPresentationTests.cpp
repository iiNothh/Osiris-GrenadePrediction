#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/CachedGrenadeTrajectoryPresentation.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>

namespace
{

TEST(CachedGrenadeTrajectoryPresentationTest, RollbackHidesBothPanelsAndClearsTheRollbackMarker)
{
    GrenadePredictionState state;
    state.lastCommittedTrajectory.valid = true;
    state.lastCommittedTrajectory.pointsCount = 1;
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.lastCommitCurtime = 10.0f;
    state.hasCommitCurtime = true;
    state.beginFrame();
    EXPECT_FALSE(state.observeTime(true, 12.0f));
    state.beginFrame();
    EXPECT_TRUE(state.observeTime(true, 11.0f));
    const auto decision = state.cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 5.0f, true, 11.0f, false);
    int hiddenLive{};
    int hiddenCached{};

    CachedGrenadeTrajectoryPresentation::apply(state, decision, [] {}, [&] { ++hiddenLive; }, [&] { ++hiddenCached; });

    EXPECT_EQ(decision, LastGrenadeCacheVisibility::Hide);
    EXPECT_FALSE(state.lastCommittedTrajectory.valid);
    EXPECT_FALSE(state.tempTrajectory.valid);
    EXPECT_FALSE(state.rollbackDetected);
    EXPECT_EQ(hiddenLive, 1);
    EXPECT_EQ(hiddenCached, 1);
}

TEST(CachedGrenadeTrajectoryPresentationTest, RendersTheCachedTrajectoryOnceAfterValidityUpdate)
{
    GrenadePredictionState state;
    state.lastCommittedTrajectory.valid = true;
    state.lastCommittedTrajectory.pointsCount = 1;
    int drawn{};
    int hidden{};

    const auto decision = state.cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode::Always, 0.0f, true, 10.0f, false);
    CachedGrenadeTrajectoryPresentation::apply(state, decision, [] {}, [] {}, [] {});
    CachedGrenadeTrajectoryPresentation::apply(state, decision, [&] { ++drawn; }, [] {}, [&] { ++hidden; });

    EXPECT_EQ(drawn, 1);
    EXPECT_EQ(hidden, 0);
}

}

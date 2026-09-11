#include <gtest/gtest.h>

#include <limits>

#include <GameClient/Entities/GrenadeKind.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadePrediction.h>

namespace
{

constexpr cs2::CEntityHandle localPawn{1};
constexpr cs2::CEntityHandle firstProjectile{2};
constexpr cs2::CEntityHandle secondProjectile{3};
constexpr cs2::CEntityHandle heldWeapon{4};

struct LiveGrenadePredictionTestContext {
};

[[nodiscard]] LiveGrenadeSnapshot snapshot(cs2::CEntityHandle handle) noexcept
{
    return {handle, localPawn, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, GrenadeKind::Flashbang};
}

TEST(LiveGrenadePredictionTest, CompletesScanBySimulatingAndAcceptingNewestLocalProjectile)
{
    GrenadePredictionState state;
    LiveGrenadePredictionTestContext context;
    auto liveGrenadePrediction = LiveGrenadePrediction<LiveGrenadePredictionTestContext>{context, state};
    Trajectory liveGrenadeTrajectoryScratch;
    state.liveGrenadeCache.beginScan();
    EXPECT_TRUE(state.liveGrenadeCache.upsert(snapshot(secondProjectile)));
    EXPECT_TRUE(state.liveGrenadeCache.upsert(snapshot(firstProjectile)));
    liveGrenadeTrajectoryScratch.valid = true;
    liveGrenadeTrajectoryScratch.pointsCount = 1;
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.tagTempTrajectory(heldWeapon, 1);
    state.liveGrenadeCache.endScan();

    cs2::CEntityHandle simulatedProjectile{};
    EXPECT_TRUE(liveGrenadePrediction.attemptNewestProjectileAdoption(liveGrenadeTrajectoryScratch, localPawn, 10.0f, [&](const auto& projectile) noexcept {
        simulatedProjectile = projectile.projectileHandle;
        return true;
    }));
    EXPECT_EQ(simulatedProjectile, firstProjectile);
    EXPECT_TRUE(state.lastCommittedTrajectory.valid);
    EXPECT_TRUE(state.liveGrenadeAuthority.hasAcceptedLiveProjectile());
    EXPECT_EQ(state.liveGrenadeAuthority.acceptedLiveProjectile().projectileHandle, firstProjectile);
    EXPECT_FALSE(state.tempTrajectory.valid);
}

TEST(LiveGrenadePredictionTest, EmptyAuthoritativeScanDoesNotAcceptLiveProjectile)
{
    GrenadePredictionState state;
    LiveGrenadePredictionTestContext context;
    auto liveGrenadePrediction = LiveGrenadePrediction<LiveGrenadePredictionTestContext>{context, state};
    Trajectory liveGrenadeTrajectoryScratch;
    state.liveGrenadeCache.beginScan();
    state.liveGrenadeCache.endScan();
    bool simulated{};

    EXPECT_FALSE(liveGrenadePrediction.attemptNewestProjectileAdoption(liveGrenadeTrajectoryScratch, localPawn, 10.0f, [&](const auto&) noexcept {
        simulated = true;
        return true;
    }));

    EXPECT_FALSE(simulated);
    EXPECT_FALSE(state.liveGrenadeAuthority.hasAcceptedLiveProjectile());
}

TEST(LiveGrenadePredictionTest, DoesNotUseNonFiniteTimeForAcceptedLiveProjectile)
{
    GrenadePredictionState state;
    LiveGrenadePredictionTestContext context;
    auto liveGrenadePrediction = LiveGrenadePrediction<LiveGrenadePredictionTestContext>{context, state};
    Trajectory liveGrenadeTrajectoryScratch;
    EXPECT_TRUE(state.liveGrenadeCache.upsert(snapshot(firstProjectile)));
    liveGrenadeTrajectoryScratch.valid = true;
    liveGrenadeTrajectoryScratch.pointsCount = 1;
    state.liveGrenadeCache.endScan();

    EXPECT_TRUE(liveGrenadePrediction.attemptNewestProjectileAdoption(
        liveGrenadeTrajectoryScratch, localPawn, std::numeric_limits<float>::infinity(), [](const auto&) noexcept { return true; }));
    EXPECT_FALSE(state.liveGrenadeAuthority.isFlashbangInEarlyHideWindow(100.0f));
}

}

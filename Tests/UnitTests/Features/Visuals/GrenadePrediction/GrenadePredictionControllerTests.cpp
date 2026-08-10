#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/GrenadePredictionController.h>

namespace
{

constexpr cs2::CEntityHandle localPawn{1};
constexpr cs2::CEntityHandle firstProjectile{2};
constexpr cs2::CEntityHandle secondProjectile{3};

struct Projectile {
    Optional<cs2::Vector> initialPosition() const noexcept { return cs2::Vector{1.0f, 2.0f, 3.0f}; }
    Optional<cs2::Vector> initialVelocity() const noexcept { return cs2::Vector{4.0f, 5.0f, 6.0f}; }
    Optional<cs2::CEntityHandle> thrower() const noexcept { return localPawn; }
};

struct Decoy {
    [[nodiscard]] int decoyShotTick() const noexcept { return 1; }
};

LiveGrenadeSnapshot snapshot(cs2::CEntityHandle handle) noexcept
{
    return {handle, localPawn, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, cs2::GrenadeKind::Flashbang};
}

TEST(GrenadePredictionControllerTest, CommitsHeldThrowOnlyAfterOwnedNativeExecution)
{
    GrenadePredictionState state;
    const int weapon{};
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    static_cast<void>(state.throwObservation.observeWeapon(&weapon));
    state.throwObservation.retainThrowStrength(0.5f);
    state.tagTempTrajectory(&weapon, state.throwObservation.pendingSequence());
    static_cast<void>(state.throwObservation.observeThrowTime(&weapon, 10.0f));

    EXPECT_FALSE(GrenadePredictionController::completeHeldThrow(state, &weapon, true, 10.0f));
    EXPECT_FALSE(state.lastCommittedTrajectory.valid);
    EXPECT_TRUE(GrenadePredictionController::completeHeldThrow(state, &weapon, true, 10.1f));
    EXPECT_TRUE(state.lastCommittedTrajectory.valid);
    EXPECT_TRUE(state.tempTrajectory.valid);
}

TEST(GrenadePredictionControllerTest, CompletesScanBySimulatingAndAcceptingNewestLocalProjectile)
{
    GrenadePredictionState state;
    EXPECT_TRUE(state.liveGrenadeCache.upsert(snapshot(secondProjectile)));
    EXPECT_TRUE(state.liveGrenadeCache.upsert(snapshot(firstProjectile)));
    state.liveGrenadeTrajectoryScratch.valid = true;
    state.liveGrenadeTrajectoryScratch.pointsCount = 1;

    cs2::CEntityHandle simulatedProjectile{};
    EXPECT_TRUE(GrenadePredictionController::completeLiveGrenadeScan(state, localPawn, 10.0f, [&](const auto& projectile) noexcept {
        simulatedProjectile = projectile.projectileHandle;
        return true;
    }));
    EXPECT_EQ(simulatedProjectile, firstProjectile);
    EXPECT_TRUE(state.lastCommittedTrajectory.valid);
    EXPECT_TRUE(state.liveGrenadeAuthority.hasAcceptedLiveProjectile());
    EXPECT_EQ(state.liveGrenadeAuthority.acceptedLiveProjectile().projectileHandle, firstProjectile);
}

TEST(GrenadePredictionControllerTest, PropagatesDecoyAccessorTickToLifecycleCacheRemoval)
{
    GrenadePredictionState state;
    state.liveGrenadeCache.beginScan();

    EXPECT_TRUE(GrenadePredictionController::updateDecoyLiveGrenade(state.liveGrenadeCache, Projectile{}, firstProjectile, Decoy{}));
    state.liveGrenadeCache.endScan();
    EXPECT_FALSE(state.liveGrenadeCache.newestForThrower(localPawn).hasValue());
}

}

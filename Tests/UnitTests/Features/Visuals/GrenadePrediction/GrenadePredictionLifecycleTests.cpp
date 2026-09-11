#include <gtest/gtest.h>

#include <cstdint>

#include <GameClient/Entities/GrenadeKind.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCacheUpdater.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadePrediction.h>

namespace
{

constexpr cs2::CEntityHandle localPawn{1};
constexpr cs2::CEntityHandle projectileHandle{2};

struct HEProjectile {
    [[nodiscard]] Optional<cs2::Vector> initialPosition() const noexcept { return cs2::Vector{1.0f, 2.0f, 3.0f}; }
    [[nodiscard]] Optional<cs2::Vector> initialVelocity() const noexcept { return cs2::Vector{4.0f, 5.0f, 6.0f}; }
    [[nodiscard]] Optional<cs2::CEntityHandle> thrower() const noexcept { return localPawn; }
    [[nodiscard]] Optional<std::int32_t> explodeEffectTickBegin() const noexcept { return 1; }
};

struct LiveGrenadePredictionTestContext {
};

TEST(GrenadePredictionLifecycleTest, KeepsHEGrenadeWhenExplodeEffectTickIsUnavailableOrZero)
{
    EXPECT_EQ(getLiveGrenadeLifecycle(GrenadeKind::HEGrenade, {}), LiveGrenadeLifecycle::Keep);
    EXPECT_EQ(getLiveGrenadeLifecycle(GrenadeKind::HEGrenade, {.heExplodeEffectTickBegin = 0}), LiveGrenadeLifecycle::Keep);
}

TEST(GrenadePredictionLifecycleTest, PositiveHEExplodeEffectTickInvalidatesAcceptedTrajectory)
{
    GrenadePredictionState state;
    LiveGrenadePredictionTestContext context;
    auto liveGrenadePrediction = LiveGrenadePrediction<LiveGrenadePredictionTestContext>{context, state};
    Trajectory liveGrenadeTrajectoryScratch{.pointsCount = 1, .valid = true};
    state.liveGrenadeCache.beginScan();
    EXPECT_TRUE(state.liveGrenadeCache.upsert({projectileHandle, localPawn, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, GrenadeKind::HEGrenade}));
    state.liveGrenadeCache.endScan();
    EXPECT_TRUE(liveGrenadePrediction.attemptNewestProjectileAdoption(
        liveGrenadeTrajectoryScratch, localPawn, 10.0f, [](const auto&) noexcept { return true; }));
    ASSERT_TRUE(state.liveGrenadeAuthority.hasAcceptedLiveProjectile());

    state.liveGrenadeCache.beginScan();
    EXPECT_TRUE(LiveGrenadeCacheUpdater{state.liveGrenadeCache}.update(HEProjectile{}, projectileHandle, GrenadeKind::HEGrenade,
        {.heExplodeEffectTickBegin = HEProjectile{}.explodeEffectTickBegin()}));
    state.liveGrenadeCache.endScan();
    EXPECT_FALSE(liveGrenadePrediction.attemptNewestProjectileAdoption(
        liveGrenadeTrajectoryScratch, localPawn, 10.1f, [](const auto&) noexcept { return true; }));
    EXPECT_FALSE(state.liveGrenadeAuthority.hasAcceptedLiveProjectile());
}

}

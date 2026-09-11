#include <gtest/gtest.h>

#include <cstdint>

#include <GameClient/Entities/GrenadeKind.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeAuthority.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCache.h>

namespace
{

constexpr cs2::CEntityHandle localPawn{1};
constexpr cs2::CEntityHandle firstProjectile{2};
constexpr cs2::CEntityHandle secondProjectile{3};

[[nodiscard]] LiveGrenadeSnapshot snapshot(cs2::CEntityHandle projectile, GrenadeKind kind = GrenadeKind::Flashbang) noexcept
{
    return {projectile, localPawn, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, kind};
}

[[nodiscard]] LiveGrenadeSnapshot snapshot(cs2::CEntityHandle projectile, std::uint32_t sequence, GrenadeKind kind = GrenadeKind::HEGrenade) noexcept
{
    return {projectile, localPawn, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, kind, sequence};
}

TEST(GrenadePredictionAuthorityParityTest, NewerFailedProjectileBlocksHeldAndOlderRetries)
{
    GrenadePredictionState state;
    constexpr cs2::CEntityHandle weapon{1};
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.tagTempTrajectory(weapon, 1);
    EXPECT_TRUE(state.liveGrenadeAuthority.observeForSimulation(snapshot({2}, 1)));
    EXPECT_FALSE(state.stageOwnedTempTrajectory(weapon, 1));
    EXPECT_TRUE(state.liveGrenadeAuthority.observeForSimulation(snapshot({3}, 2)));
    EXPECT_FALSE(state.liveGrenadeAuthority.observeForSimulation(snapshot({2}, 1)));
    EXPECT_TRUE(state.liveGrenadeAuthority.observeForSimulation(snapshot({3}, 2)));
}

TEST(GrenadePredictionLiveCacheTest, AdoptsNewestLocalProjectileByObservationOrder)
{
    LiveGrenadeCache cache;
    EXPECT_TRUE(cache.upsert(snapshot(secondProjectile)));
    EXPECT_TRUE(cache.upsert(snapshot(firstProjectile)));
    LiveGrenadeAuthority authority;
    authority.observeLocalPawn(localPawn);
    const auto newest = authority.newestLocalProjectile(cache);
    ASSERT_TRUE(newest.hasValue());
    EXPECT_EQ(newest.value().projectileHandle, firstProjectile);
    authority.accept(newest.value());
    EXPECT_TRUE(authority.hasAcceptedLiveProjectile());
}

}

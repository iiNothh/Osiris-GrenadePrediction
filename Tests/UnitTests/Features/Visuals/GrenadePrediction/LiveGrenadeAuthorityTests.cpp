#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

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

TEST(LiveGrenadeAuthorityTest, SchedulesExponentialSimulationRetries)
{
    LiveGrenadeAuthority authority;
    const auto projectile = snapshot(firstProjectile, 1);

    EXPECT_TRUE(authority.observeForSimulation(projectile));
    authority.recordSimulationFailure(projectile, 10);
    EXPECT_FALSE(authority.isSimulationRetryDue(projectile, 10));
    EXPECT_TRUE(authority.isSimulationRetryDue(projectile, 11));

    authority.recordSimulationFailure(projectile, 11);
    EXPECT_FALSE(authority.isSimulationRetryDue(projectile, 12));
    EXPECT_TRUE(authority.isSimulationRetryDue(projectile, 13));

    authority.recordSimulationFailure(projectile, 13);
    EXPECT_FALSE(authority.isSimulationRetryDue(projectile, 16));
    EXPECT_TRUE(authority.isSimulationRetryDue(projectile, 17));
}

TEST(LiveGrenadeAuthorityTest, ResetsRetryForNewObservation)
{
    LiveGrenadeAuthority authority;
    const auto first = snapshot(firstProjectile, 1);
    const auto second = snapshot(secondProjectile, 2);

    EXPECT_TRUE(authority.observeForSimulation(first));
    authority.recordSimulationFailure(first, 10);
    EXPECT_FALSE(authority.isSimulationRetryDue(first, 10));

    EXPECT_TRUE(authority.observeForSimulation(second));
    EXPECT_TRUE(authority.isSimulationRetryDue(second, 10));
    authority.recordSimulationFailure(second, 10);
    EXPECT_FALSE(authority.isSimulationRetryDue(second, 10));
    EXPECT_TRUE(authority.isSimulationRetryDue(second, 11));
}

TEST(LiveGrenadeAuthorityTest, TreatsWrappedFrameAsDue)
{
    LiveGrenadeAuthority authority;
    const auto projectile = snapshot(firstProjectile, 1);

    EXPECT_TRUE(authority.observeForSimulation(projectile));
    authority.recordSimulationFailure(projectile, std::numeric_limits<std::uint32_t>::max());
    EXPECT_TRUE(authority.isSimulationRetryDue(projectile, 0));
}

TEST(LiveGrenadeAuthorityTest, AcceptClearsRetryState)
{
    LiveGrenadeAuthority authority;
    const auto projectile = snapshot(firstProjectile, 1);

    EXPECT_TRUE(authority.observeForSimulation(projectile));
    authority.recordSimulationFailure(projectile, 10);
    EXPECT_FALSE(authority.isSimulationRetryDue(projectile, 10));

    authority.accept(projectile);
    EXPECT_TRUE(authority.isSimulationRetryDue(projectile, 10));
}

}

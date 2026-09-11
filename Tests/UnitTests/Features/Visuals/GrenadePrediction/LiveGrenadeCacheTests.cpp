#include <gtest/gtest.h>

#include <cstdint>

#include <GameClient/Entities/GrenadeKind.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCache.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCacheUpdater.h>

namespace
{

constexpr cs2::CEntityHandle localPawn{1};
constexpr cs2::CEntityHandle firstProjectile{2};
constexpr cs2::CEntityHandle secondProjectile{3};

[[nodiscard]] LiveGrenadeSnapshot snapshot(cs2::CEntityHandle projectile, std::uint32_t sequence = 0, GrenadeKind kind = GrenadeKind::Flashbang) noexcept
{
    return {projectile, localPawn, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, kind, sequence};
}

struct Projectile {
    Optional<cs2::Vector> position{cs2::Vector{1.0f, 2.0f, 3.0f}};
    Optional<cs2::Vector> velocity{cs2::Vector{4.0f, 5.0f, 6.0f}};
    Optional<cs2::CEntityHandle> owner{localPawn};

    [[nodiscard]] Optional<cs2::Vector> initialPosition() const noexcept { return position; }
    [[nodiscard]] Optional<cs2::Vector> initialVelocity() const noexcept { return velocity; }
    [[nodiscard]] Optional<cs2::CEntityHandle> thrower() const noexcept { return owner; }
};

struct Decoy {
    [[nodiscard]] int decoyShotTick() const noexcept { return 1; }
};

TEST(GrenadePredictionAuthorityParityTest, CacheUsesSequenceAndFullAcceptedIdentity)
{
    LiveGrenadeCache cache;
    ASSERT_TRUE(cache.upsert(snapshot({0x00020002}, 0)));
    ASSERT_TRUE(cache.upsert(snapshot({0x00010002}, 0)));
    const auto newest = cache.newestForThrower(localPawn);
    ASSERT_TRUE(newest.hasValue());
    EXPECT_EQ(newest.value().projectileHandle, (cs2::CEntityHandle{0x00010002}));
    const auto accepted = newest.value();
    ASSERT_TRUE(cache.upsert({accepted.projectileHandle, {8}, accepted.initialPosition, accepted.initialVelocity, accepted.kind}));
    EXPECT_FALSE(cache.contains(accepted));
}

TEST(LiveGrenadeCacheUpdaterTest, PropagatesDecoyAccessorTickToLifecycleCacheRemoval)
{
    GrenadePredictionState state;
    state.liveGrenadeCache.beginScan();

    EXPECT_TRUE(LiveGrenadeCacheUpdater{state.liveGrenadeCache}.update(Projectile{}, firstProjectile, GrenadeKind::Decoy, {.decoyShotTick = Decoy{}.decoyShotTick()}));
    state.liveGrenadeCache.endScan();
    EXPECT_FALSE(state.liveGrenadeCache.newestForThrower(localPawn).hasValue());
}

TEST(GrenadePredictionLiveCacheTest, RemovesSmokeAndDecoyWhenLifecycleEnds)
{
    LiveGrenadeCache cache;
    LiveGrenadeCacheUpdater updater{cache};
    Projectile projectile{cs2::Vector{1.0f, 2.0f, 3.0f}, cs2::Vector{4.0f, 5.0f, 6.0f}, localPawn};
    EXPECT_TRUE(updater.update(projectile, firstProjectile, GrenadeKind::SmokeGrenade, {.smokeEffectStarted = true}));
    EXPECT_TRUE(updater.update(projectile, secondProjectile, GrenadeKind::Decoy, {.decoyShotTick = 1}));
    cache.endScan();
    EXPECT_FALSE(cache.newestForThrower(localPawn).hasValue());
}

}

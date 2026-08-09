#include <gtest/gtest.h>

#include <Features/Visuals/GrenadePrediction/GrenadeLaunchSelection.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionController.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadeThrowObservation.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeAuthority.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCacheUpdater.h>

namespace
{

constexpr cs2::CEntityHandle localPawn{1};
constexpr cs2::CEntityHandle firstProjectile{2};
constexpr cs2::CEntityHandle secondProjectile{3};

LiveGrenadeSnapshot snapshot(cs2::CEntityHandle projectile, cs2::GrenadeKind kind = cs2::GrenadeKind::Flashbang) noexcept
{
    return {projectile, localPawn, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, kind};
}

struct Projectile {
    Optional<cs2::Vector> position;
    Optional<cs2::Vector> velocity;
    Optional<cs2::CEntityHandle> owner;

    [[nodiscard]] Optional<cs2::Vector> initialPosition() const noexcept { return position; }
    [[nodiscard]] Optional<cs2::Vector> initialVelocity() const noexcept { return velocity; }
    [[nodiscard]] Optional<cs2::CEntityHandle> thrower() const noexcept { return owner; }
};

TEST(GrenadePredictionThrowObservationTest, CommitsOnlyTheOwnedCompletedSequence)
{
    GrenadeThrowObservation observation;
    const int weapon{};
    static_cast<void>(observation.observeWeapon(&weapon));
    EXPECT_FALSE(observation.observePinState(&weapon, true));
    EXPECT_TRUE(observation.observePinState(&weapon, false));
    EXPECT_TRUE(observation.observeThrowTime(&weapon, 10.0f));
    EXPECT_FALSE(observation.consumeActualExecution(true, 10.0f));
    EXPECT_TRUE(observation.consumeActualExecution(true, 10.1f));
    EXPECT_TRUE(observation.isFinalized());
}

TEST(GrenadePredictionThrowObservationTest, UsesDefaultStrengthBeforePinIsPulled)
{
    GrenadeThrowObservation observation;
    const int weapon{};

    EXPECT_TRUE(observation.observeWeapon(&weapon));
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);
}

TEST(GrenadePredictionControllerTest, CapturesRealStrengthAfterObservingPinPull)
{
    GrenadeThrowObservation observation;
    const int weapon{};
    static_cast<void>(observation.observeWeapon(&weapon));

    GrenadePredictionController::observeHeldThrow(observation, &weapon, false, 0.25f);
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);

    GrenadePredictionController::observeHeldThrow(observation, &weapon, true, 0.25f);
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.25f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(GrenadePredictionLaunchTest, RejectsUnavailableNativeLaunch)
{
    const auto unavailable = prepareGrenadeLaunch(false, true, false, false, []() noexcept { return Optional<GrenadeLaunchState>{}; });
    EXPECT_EQ(unavailable.status, GrenadeLaunchPreparationStatus::Unavailable);
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

TEST(GrenadePredictionLiveCacheTest, RemovesSmokeAndDecoyWhenLifecycleEnds)
{
    LiveGrenadeCache cache;
    LiveGrenadeCacheUpdater updater{cache};
    Projectile projectile{cs2::Vector{1.0f, 2.0f, 3.0f}, cs2::Vector{4.0f, 5.0f, 6.0f}, localPawn};
    EXPECT_TRUE(updater.update(projectile, firstProjectile, cs2::GrenadeKind::SmokeGrenade, {.smokeEffectStarted = true}));
    EXPECT_TRUE(updater.update(projectile, secondProjectile, cs2::GrenadeKind::Decoy, {.decoyShotTick = 1}));
    cache.endScan();
    EXPECT_FALSE(cache.newestForThrower(localPawn).hasValue());
}

TEST(GrenadePredictionStateTest, HonorsVisibilityModesAndFlashExpiry)
{
    GrenadePredictionState state;
    state.lastCommittedTrajectory.valid = true;
    state.lastCommittedTrajectory.pointsCount = 1;
    EXPECT_EQ(state.cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode::Always, 0.0f, true, 2.0f, false), LastGrenadeCacheVisibility::Show);
    EXPECT_EQ(state.cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode::Off, 0.0f, true, 2.0f, true), LastGrenadeCacheVisibility::Invalidate);
    state.lastCommitCurtime = 1.0f;
    state.hasCommitCurtime = true;
    EXPECT_EQ(state.cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 2.0f, true, 2.0f, false), LastGrenadeCacheVisibility::Show);

    LiveGrenadeAuthority authority;
    authority.observeLocalPawn(localPawn);
    authority.accept(snapshot(firstProjectile), 1.0f);
    LiveGrenadeCache cache;
    EXPECT_TRUE(cache.upsert(snapshot(firstProjectile)));
    authority.update(cache, 1.0f + LiveGrenadeAuthority::flashHorizon);
    EXPECT_FALSE(authority.hasAcceptedLiveProjectile());
}

}

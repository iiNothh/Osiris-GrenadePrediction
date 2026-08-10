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

struct FlashbangWeapon {};
struct HEGrenadeWeapon {};
struct SmokeGrenadeWeapon {};
struct MolotovWeapon {};
struct IncendiaryWeapon {};
struct DecoyWeapon {};

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

    GrenadePredictionController::observeHeldThrow(observation, &weapon, false, []() noexcept { return Optional<float>{0.25f}; });
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);

    GrenadePredictionController::observeHeldThrow(observation, &weapon, true, []() noexcept { return Optional<float>{0.25f}; });
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.25f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(GrenadePredictionControllerTest, ResetsCompletedPartialFlashThrowWhenReacquiringFlashbang)
{
    GrenadeThrowObservation observation;
    const FlashbangWeapon thrownFlashbang;
    const FlashbangWeapon reacquiredFlashbang;

    GrenadePredictionController::observeHeldThrow(observation, &thrownFlashbang, true, []() noexcept { return Optional<float>{0.5f}; });
    ASSERT_TRUE(observation.observeThrowTime(&thrownFlashbang, 10.0f));
    ASSERT_TRUE(observation.consumeActualExecution(true, 10.1f));

    GrenadePredictionController::observeHeldThrow(observation, &reacquiredFlashbang, false, []() noexcept { return Optional<float>{0.0f}; });

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);
    EXPECT_FALSE(observation.isFinalized());
}

TEST(GrenadePredictionControllerTest, ResetsPartialFlashThrowWhenObservingAnotherGrenadeKind)
{
    GrenadeThrowObservation observation;
    const FlashbangWeapon flashbang;
    const HEGrenadeWeapon heGrenade;

    GrenadePredictionController::observeHeldThrow(observation, &flashbang, true, []() noexcept { return Optional<float>{0.5f}; });
    ASSERT_TRUE(observation.observeThrowTime(&flashbang, 10.0f));

    GrenadePredictionController::observeHeldThrow(observation, &heGrenade, false, []() noexcept { return Optional<float>{0.0f}; });

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);
    EXPECT_FALSE(observation.hasPendingExecution());
}

TEST(GrenadePredictionControllerTest, ResetsPendingStrengthForANewObservationOfTheSameKind)
{
    GrenadeThrowObservation observation;
    const FlashbangWeapon firstFlashbang;
    const FlashbangWeapon secondFlashbang;

    GrenadePredictionController::observeHeldThrow(observation, &firstFlashbang, true, []() noexcept { return Optional<float>{0.5f}; });
    ASSERT_TRUE(observation.observeThrowTime(&firstFlashbang, 10.0f));

    GrenadePredictionController::observeHeldThrow(observation, &secondFlashbang, false, []() noexcept { return Optional<float>{0.0f}; });

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);
    EXPECT_FALSE(observation.hasPendingExecution());
}

TEST(GrenadePredictionControllerTest, RetainsPinnedZeroStrengthForTheCurrentThrow)
{
    GrenadeThrowObservation observation;
    const FlashbangWeapon flashbang;

    GrenadePredictionController::observeHeldThrow(observation, &flashbang, true, []() noexcept { return Optional<float>{0.0f}; });
    ASSERT_TRUE(observation.observeThrowTime(&flashbang, 10.0f));
    GrenadePredictionController::observeHeldThrow(observation, &flashbang, true, []() noexcept { return Optional<float>{0.5f}; });

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.0f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(GrenadePredictionControllerTest, DoesNotReadZeroStrengthForAnUnpinnedFlashbang)
{
    GrenadeThrowObservation observation;
    const FlashbangWeapon flashbang;
    bool readThrowStrength{};

    GrenadePredictionController::observeHeldThrow(observation, &flashbang, false, [&]() noexcept {
        readThrowStrength = true;
        return Optional<float>{0.0f};
    });

    EXPECT_FALSE(readThrowStrength);
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);
}

TEST(GrenadePredictionControllerTest, DoesNotReadZeroStrengthForUnpinnedGenericGrenadeTransitions)
{
    GrenadeThrowObservation observation;
    const HEGrenadeWeapon heGrenade;
    const SmokeGrenadeWeapon smokeGrenade;
    const MolotovWeapon molotov;
    const IncendiaryWeapon incendiary;
    const DecoyWeapon decoy;
    const void* const weapons[]{&heGrenade, &smokeGrenade, &molotov, &incendiary, &decoy};

    for (const auto* weapon : weapons) {
        bool readThrowStrength{};
        GrenadePredictionController::observeHeldThrow(observation, weapon, false, [&]() noexcept {
            readThrowStrength = true;
            return Optional<float>{0.0f};
        });
        EXPECT_FALSE(readThrowStrength);
        EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
        EXPECT_FALSE(observation.hasRetainedThrowStrength);
    }
}

TEST(GrenadePredictionControllerTest, ReadsAndRetainsPinnedZeroStrengthForCurrentCycle)
{
    GrenadeThrowObservation observation;
    const FlashbangWeapon flashbang;
    bool readThrowStrength{};

    GrenadePredictionController::observeHeldThrow(observation, &flashbang, true, [&]() noexcept {
        readThrowStrength = true;
        return Optional<float>{0.0f};
    });

    EXPECT_TRUE(readThrowStrength);
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.0f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(GrenadePredictionLaunchTest, RejectsUnavailableNativeLaunch)
{
    const auto unavailable = prepareGrenadeLaunch(false, true, false, false,
        []() noexcept { return Optional<GrenadeLaunchState>{}; },
        []() noexcept { return Optional<GrenadeLaunchState>{}; });
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

#include <gtest/gtest.h>

#include <limits>

#include <GameClient/Entities/GrenadeKind.h>
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

[[nodiscard]] LiveGrenadeSnapshot snapshot(cs2::CEntityHandle projectile, GrenadeKind kind = GrenadeKind::Flashbang) noexcept
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
    constexpr cs2::CEntityHandle weapon{4};
    static_cast<void>(observation.observeWeapon(weapon));
    EXPECT_FALSE(observation.observePinState(weapon, true));
    EXPECT_TRUE(observation.observePinState(weapon, false));
    EXPECT_TRUE(observation.observeThrowTime(weapon, 10.0f));
    EXPECT_FALSE(observation.consumeActualExecution(true, 10.0f));
    EXPECT_TRUE(observation.consumeActualExecution(true, 10.1f));
    EXPECT_TRUE(observation.isFinalized());
}

TEST(GrenadePredictionThrowObservationTest, ClearsFinalizedSequenceWhenThrowTimeResets)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{4};
    static_cast<void>(observation.observeWeapon(weapon));
    observation.retainThrowStrength(0.5f);
    ASSERT_TRUE(observation.observeThrowTime(weapon, 10.0f));
    ASSERT_TRUE(observation.consumeActualExecution(true, 10.1f));
    const auto finalizedSequence = observation.pendingSequence();

    EXPECT_TRUE(observation.observeThrowTime(weapon, 0.0f));
    EXPECT_FALSE(observation.isFinalized());
    EXPECT_FALSE(observation.isStrengthLocked());
    EXPECT_FALSE(observation.hasPendingExecution());
    EXPECT_NE(observation.pendingSequence(), finalizedSequence);
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);
}

TEST(GrenadePredictionThrowObservationTest, PreparesLaunchWhenSameWeaponIsImmediatelyReequipped)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{4};
    int manualCalls{};
    static_cast<void>(observation.observeWeapon(weapon));
    observation.retainThrowStrength(0.5f);
    ASSERT_TRUE(observation.observeThrowTime(weapon, 10.0f));
    ASSERT_TRUE(observation.consumeActualExecution(true, 10.1f));

    ASSERT_TRUE(observation.observeThrowTime(weapon, 0.0f));
    const auto launch = prepareGrenadeLaunch(observation.isFinalized(), observation.hasRetainedThrowStrength,
        []() noexcept -> Optional<GrenadeLaunchState> { return {}; },
        [&]() noexcept -> Optional<GrenadeLaunchState> {
            ++manualCalls;
            return GrenadeLaunchState{};
        });

    EXPECT_TRUE(launch.hasValue());
    EXPECT_EQ(manualCalls, 1);
}

TEST(GrenadePredictionThrowObservationTest, ClearsPendingExecutionWithoutResettingItsSequence)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{4};
    static_cast<void>(observation.observeWeapon(weapon));
    observation.retainThrowStrength(0.5f);
    ASSERT_TRUE(observation.observeThrowTime(weapon, 10.0f));
    const auto pendingSequence = observation.pendingSequence();

    EXPECT_TRUE(observation.observeThrowTime(weapon, 0.0f));
    EXPECT_FALSE(observation.hasPendingExecution());
    EXPECT_FALSE(observation.isFinalized());
    EXPECT_FALSE(observation.isStrengthLocked());
    EXPECT_EQ(observation.pendingSequence(), pendingSequence);
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.5f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(GrenadePredictionThrowObservationTest, TreatsNonFiniteThrowAndCurrentTimesAsUnavailable)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{4};
    static_cast<void>(observation.observeWeapon(weapon));

    EXPECT_FALSE(observation.observeThrowTime(weapon, std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(observation.hasPendingExecution());
    EXPECT_TRUE(observation.observeThrowTime(weapon, 10.0f));
    EXPECT_FALSE(observation.consumeActualExecution(true, std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(observation.isFinalized());
}

TEST(GrenadePredictionThrowObservationTest, UsesDefaultStrengthBeforePinIsPulled)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{4};

    EXPECT_TRUE(observation.observeWeapon(weapon));
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);
}

TEST(GrenadePredictionControllerTest, CapturesRealStrengthAfterObservingPinPull)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle weapon{4};
    static_cast<void>(observation.observeWeapon(weapon));

    static_cast<void>(GrenadePredictionController::observeHeldThrow(observation, weapon, false));
    GrenadePredictionController::captureThrowStrength(observation, false, {}, []() noexcept { return Optional<float>{0.25f}; });
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);

    static_cast<void>(GrenadePredictionController::observeHeldThrow(observation, weapon, true));
    GrenadePredictionController::captureThrowStrength(observation, true, {}, []() noexcept { return Optional<float>{0.25f}; });
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.25f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(GrenadePredictionControllerTest, ResetsCompletedPartialFlashThrowWhenReacquiringFlashbang)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle thrownFlashbang{4};
    constexpr cs2::CEntityHandle reacquiredFlashbang{5};

    static_cast<void>(GrenadePredictionController::observeHeldThrow(observation, thrownFlashbang, true));
    GrenadePredictionController::captureThrowStrength(observation, true, {}, []() noexcept { return Optional<float>{0.5f}; });
    ASSERT_TRUE(observation.observeThrowTime(thrownFlashbang, 10.0f));
    ASSERT_TRUE(observation.consumeActualExecution(true, 10.1f));

    static_cast<void>(GrenadePredictionController::observeHeldThrow(observation, reacquiredFlashbang, false));
    GrenadePredictionController::captureThrowStrength(observation, false, {}, []() noexcept { return Optional<float>{0.0f}; });

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);
    EXPECT_FALSE(observation.isFinalized());
}

TEST(GrenadePredictionControllerTest, ResetsPartialFlashThrowWhenObservingAnotherGrenadeKind)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle flashbang{4};
    constexpr cs2::CEntityHandle heGrenade{5};

    static_cast<void>(GrenadePredictionController::observeHeldThrow(observation, flashbang, true));
    GrenadePredictionController::captureThrowStrength(observation, true, {}, []() noexcept { return Optional<float>{0.5f}; });
    ASSERT_TRUE(observation.observeThrowTime(flashbang, 10.0f));

    static_cast<void>(GrenadePredictionController::observeHeldThrow(observation, heGrenade, false));
    GrenadePredictionController::captureThrowStrength(observation, false, {}, []() noexcept { return Optional<float>{0.0f}; });

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);
    EXPECT_FALSE(observation.hasPendingExecution());
}

TEST(GrenadePredictionControllerTest, ResetsPendingStrengthForANewObservationOfTheSameKind)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle firstFlashbang{4};
    constexpr cs2::CEntityHandle secondFlashbang{5};

    static_cast<void>(GrenadePredictionController::observeHeldThrow(observation, firstFlashbang, true));
    GrenadePredictionController::captureThrowStrength(observation, true, {}, []() noexcept { return Optional<float>{0.5f}; });
    ASSERT_TRUE(observation.observeThrowTime(firstFlashbang, 10.0f));

    static_cast<void>(GrenadePredictionController::observeHeldThrow(observation, secondFlashbang, false));
    GrenadePredictionController::captureThrowStrength(observation, false, {}, []() noexcept { return Optional<float>{0.0f}; });

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 1.0f);
    EXPECT_FALSE(observation.hasRetainedThrowStrength);
    EXPECT_FALSE(observation.hasPendingExecution());
}

TEST(GrenadePredictionControllerTest, RetainsPinnedZeroStrengthForTheCurrentThrow)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle flashbang{4};

    static_cast<void>(GrenadePredictionController::observeHeldThrow(observation, flashbang, true));
    GrenadePredictionController::captureThrowStrength(observation, true, {}, []() noexcept { return Optional<float>{0.0f}; });
    ASSERT_TRUE(observation.observeThrowTime(flashbang, 10.0f));
    static_cast<void>(GrenadePredictionController::observeHeldThrow(observation, flashbang, true));
    GrenadePredictionController::captureThrowStrength(observation, true, {}, []() noexcept { return Optional<float>{0.5f}; });

    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.0f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(GrenadePredictionControllerTest, DoesNotReadZeroStrengthForAnUnpinnedFlashbang)
{
    GrenadeThrowObservation observation;
    constexpr cs2::CEntityHandle flashbang{4};
    bool readThrowStrength{};

    static_cast<void>(GrenadePredictionController::observeHeldThrow(observation, flashbang, false));
    GrenadePredictionController::captureThrowStrength(observation, false, {}, [&]() noexcept {
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
    constexpr cs2::CEntityHandle weapons[]{{4}, {5}, {6}, {7}, {8}};

    for (const auto weapon : weapons) {
        bool readThrowStrength{};
        static_cast<void>(GrenadePredictionController::observeHeldThrow(observation, weapon, false));
        GrenadePredictionController::captureThrowStrength(observation, false, {}, [&]() noexcept {
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
    constexpr cs2::CEntityHandle flashbang{4};
    bool readThrowStrength{};

    static_cast<void>(GrenadePredictionController::observeHeldThrow(observation, flashbang, true));
    GrenadePredictionController::captureThrowStrength(observation, true, {}, [&]() noexcept {
        readThrowStrength = true;
        return Optional<float>{0.0f};
    });

    EXPECT_TRUE(readThrowStrength);
    EXPECT_FLOAT_EQ(observation.retainedThrowStrength, 0.0f);
    EXPECT_TRUE(observation.hasRetainedThrowStrength);
}

TEST(GrenadePredictionLaunchTest, RejectsUnavailableNativeLaunch)
{
    const auto unavailable = prepareGrenadeLaunch(false, false,
        []() noexcept { return Optional<GrenadeLaunchState>{}; },
        []() noexcept { return Optional<GrenadeLaunchState>{}; });
    EXPECT_FALSE(unavailable.hasValue());
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
    EXPECT_TRUE(updater.update(projectile, firstProjectile, GrenadeKind::SmokeGrenade, {.smokeEffectStarted = true}));
    EXPECT_TRUE(updater.update(projectile, secondProjectile, GrenadeKind::Decoy, {.decoyShotTick = 1}));
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
    GrenadePredictionController::beginFrame(state);
    EXPECT_FALSE(GrenadePredictionController::observeCurrentTime(state, 2.0f));
    EXPECT_EQ(GrenadePredictionController::makeCachedTrajectoryPresentationDecision(
                  state, grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 2.0f, true, 2.0f, false),
        LastGrenadeCacheVisibility::Show);

    LiveGrenadeAuthority authority;
    authority.observeLocalPawn(localPawn);
    LiveGrenadeCache cache;
    EXPECT_TRUE(cache.upsert(snapshot(firstProjectile)));
    const auto cachedProjectile = authority.newestLocalProjectile(cache);
    ASSERT_TRUE(cachedProjectile.hasValue());
    ASSERT_TRUE(authority.observeForSimulation(cachedProjectile.value()));
    constexpr float acceptedTime{1.0f};
    authority.accept(cachedProjectile.value(), acceptedTime);

    const auto earlyHideTime = acceptedTime + LiveGrenadeAuthority::flashHorizon - LiveGrenadeAuthority::flashEarlyHideLead;
    EXPECT_TRUE(authority.isFlashbangInEarlyHideWindow(earlyHideTime));
    EXPECT_EQ(GrenadePredictionController::makeCachedTrajectoryPresentationDecision(
                  state, grenade_prediction_vars::LastTrajectoryVisibilityMode::Explode, 0.0f, true, earlyHideTime, false),
        LastGrenadeCacheVisibility::Invalidate);

    const auto cachedProjectileAfterAcceptance = authority.newestLocalProjectile(cache);
    ASSERT_TRUE(cachedProjectileAfterAcceptance.hasValue());
    EXPECT_EQ(cachedProjectileAfterAcceptance.value().observationSequence, authority.acceptedLiveProjectile().observationSequence);
    authority.update(cache);
    EXPECT_TRUE(authority.hasAcceptedLiveProjectile());
}

TEST(GrenadePredictionStateTest, ClearsCommitBaselineOnceWhenTimeRollsBack)
{
    GrenadePredictionState state;
    state.lastCommittedTrajectory.valid = true;
    state.lastCommittedTrajectory.pointsCount = 1;
    state.lastCommitCurtime = 20.0f;
    state.hasCommitCurtime = true;
    GrenadePredictionController::beginFrame(state);
    EXPECT_FALSE(GrenadePredictionController::observeCurrentTime(state, 20.0f));
    GrenadePredictionController::beginFrame(state);
    EXPECT_TRUE(GrenadePredictionController::observeCurrentTime(state, 10.0f));

    const auto decision = GrenadePredictionController::makeCachedTrajectoryPresentationDecision(
        state, grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 5.0f, true, 10.0f, false);
    EXPECT_EQ(decision, LastGrenadeCacheVisibility::Hide);
    EXPECT_FALSE(state.hasCommitCurtime);
    EXPECT_FLOAT_EQ(state.lastValidCurtime, 10.0f);

    GrenadePredictionController::applyCachedTrajectoryPresentationDecision(state, decision, [] {}, [] {}, [] {});
    EXPECT_EQ(GrenadePredictionController::makeCachedTrajectoryPresentationDecision(
                  state, grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 5.0f, true, 10.0f, false),
        LastGrenadeCacheVisibility::Hide);
    EXPECT_FALSE(state.rollbackDetected);
}

TEST(GrenadePredictionStateTest, TreatsNonFiniteCurrentTimeAsUnavailableForCacheExpiry)
{
    GrenadePredictionState state;
    state.lastCommittedTrajectory.valid = true;
    state.lastCommittedTrajectory.pointsCount = 1;
    state.lastCommitCurtime = 10.0f;
    state.lastValidCurtime = 10.0f;
    state.hasCommitCurtime = true;
    state.hasLastValidCurtime = true;

    GrenadePredictionController::beginFrame(state);
    EXPECT_FALSE(GrenadePredictionController::observeCurrentTime(state, Optional<float>{std::numeric_limits<float>::quiet_NaN()}));
    EXPECT_EQ(GrenadePredictionController::makeCachedTrajectoryPresentationDecision(
                  state, grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 1.0f, true, std::numeric_limits<float>::quiet_NaN(), false),
        LastGrenadeCacheVisibility::Hide);
    EXPECT_FLOAT_EQ(state.lastValidCurtime, 10.0f);
}

}

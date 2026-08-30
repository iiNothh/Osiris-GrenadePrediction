#include <gtest/gtest.h>

#include <cstdint>

#include <Features/Visuals/GrenadePrediction/GrenadePredictionController.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Held/GrenadeThrowObservation.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeAuthority.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCache.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeLifecycle.h>

namespace
{

constexpr cs2::CEntityHandle localPawn{7};

[[nodiscard]] LiveGrenadeSnapshot snapshot(cs2::CEntityHandle projectile, std::uint32_t sequence, cs2::GrenadeKind kind = cs2::GrenadeKind::HEGrenade) noexcept
{
    return {projectile, localPawn, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, kind, sequence};
}

TEST(GrenadePredictionAuthorityParityTest, PendingWeaponHandoffUsesStoredWeaponAndDeadline)
{
    GrenadeThrowObservation observation;
    const auto oldWeapon = reinterpret_cast<const void*>(1);
    const auto newWeapon = reinterpret_cast<const void*>(2);
    ASSERT_TRUE(observation.observeWeapon(oldWeapon));
    ASSERT_TRUE(observation.observeThrowTime(oldWeapon, 5.0f));
    EXPECT_EQ(observation.pendingWeapon(), oldWeapon);
    EXPECT_TRUE(observation.consumeActualExecution(true, 6.0f));
    EXPECT_TRUE(observation.observeWeapon(newWeapon));
    EXPECT_FALSE(observation.hasPendingExecution());
}

TEST(GrenadePredictionAuthorityParityTest, NewerFailedProjectileBlocksHeldAndOlderRetries)
{
    GrenadePredictionState state;
    const auto weapon = reinterpret_cast<const void*>(1);
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.tagTempTrajectory(weapon, 1);
    EXPECT_TRUE(state.liveGrenadeAuthority.observeForSimulation(snapshot({2}, 1)));
    EXPECT_FALSE(state.stageOwnedTempTrajectory(weapon, 1));
    EXPECT_TRUE(state.liveGrenadeAuthority.observeForSimulation(snapshot({3}, 2)));
    EXPECT_FALSE(state.liveGrenadeAuthority.observeForSimulation(snapshot({2}, 1)));
    EXPECT_TRUE(state.liveGrenadeAuthority.observeForSimulation(snapshot({3}, 2)));
}

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

TEST(GrenadePredictionAuthorityParityTest, RollbackAndMissingCustomTimeFollowReferenceVisibility)
{
    GrenadePredictionState state;
    state.lastCommittedTrajectory.valid = true;
    state.lastCommittedTrajectory.pointsCount = 1;
    state.lastCommitCurtime = 10.0f;
    state.hasCommitCurtime = true;
    GrenadePredictionController::beginFrame(state);
    EXPECT_FALSE(GrenadePredictionController::observeCurrentTime(state, Optional<float>{}));
    EXPECT_EQ(GrenadePredictionController::makeCachedTrajectoryPresentationDecision(
                  state, grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 5.0f, false, 0.0f, false),
        LastGrenadeCacheVisibility::Hide);
    GrenadePredictionController::beginFrame(state);
    EXPECT_FALSE(GrenadePredictionController::observeCurrentTime(state, 12.0f));
    EXPECT_EQ(GrenadePredictionController::makeCachedTrajectoryPresentationDecision(
                  state, grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 5.0f, true, 12.0f, false),
        LastGrenadeCacheVisibility::Show);
    GrenadePredictionController::beginFrame(state);
    EXPECT_TRUE(GrenadePredictionController::observeCurrentTime(state, 11.0f));
    EXPECT_EQ(GrenadePredictionController::makeCachedTrajectoryPresentationDecision(
                  state, grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 5.0f, true, 11.0f, false),
        LastGrenadeCacheVisibility::Hide);
    EXPECT_TRUE(state.rollbackDetected);
}

TEST(GrenadePredictionAuthorityParityTest, SmokeAndExplodeVisibilityInvalidateAuthoritatively)
{
    EXPECT_EQ(getLiveGrenadeLifecycle(cs2::GrenadeKind::SmokeGrenade, {.smokeEffectStarted = true}), LiveGrenadeLifecycle::Remove);
    GrenadePredictionState state;
    state.lastCommittedTrajectory.valid = true;
    state.lastCommittedTrajectory.pointsCount = 1;
    EXPECT_EQ(state.cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode::Explode, 60.0f, true, 10.0f, false), LastGrenadeCacheVisibility::Invalidate);
}

}

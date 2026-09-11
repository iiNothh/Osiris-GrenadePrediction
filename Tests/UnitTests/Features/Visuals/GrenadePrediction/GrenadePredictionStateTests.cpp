#include <gtest/gtest.h>

#include <limits>

#include <CS2/Classes/EntitySystem/CEntityHandle.h>
#include <CS2/Constants/EntityHandle.h>
#include <GameClient/Entities/GrenadeKind.h>
#include <Features/Visuals/GrenadePrediction/CachedGrenadeTrajectoryPresentation.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionState.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeAuthority.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCache.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeLifecycle.h>

namespace
{

TEST(GrenadePredictionStateTest, ClearsPrediction)
{
    GrenadePredictionState state;
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.lastCommittedTrajectory.valid = true;
    state.lastCommittedTrajectory.pointsCount = 1;
    state.hasCommitCurtime = true;
    state.hasLastValidCurtime = true;
    state.rollbackDetected = true;
    state.clearPrediction();

    EXPECT_FALSE(state.tempTrajectory.valid);
    EXPECT_FALSE(state.lastCommittedTrajectory.valid);
    EXPECT_EQ(state.tempTrajectory.pointsCount, 0);
    EXPECT_EQ(state.lastCommittedTrajectory.pointsCount, 0);
    EXPECT_FALSE(state.hasCommitCurtime);
    EXPECT_FALSE(state.hasLastValidCurtime);
    EXPECT_FALSE(state.rollbackDetected);
    EXPECT_EQ(state.tempTrajectoryWeapon, (cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX}));
    EXPECT_EQ(state.heldSimulationInput.weapon, (cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX}));
    EXPECT_EQ(state.throwObservation.observedWeapon, (cs2::CEntityHandle{cs2::INVALID_EHANDLE_INDEX}));
}

constexpr cs2::CEntityHandle localPawn{1};
constexpr cs2::CEntityHandle firstProjectile{2};
constexpr cs2::CEntityHandle heldWeapon{4};

[[nodiscard]] LiveGrenadeSnapshot snapshot(cs2::CEntityHandle projectile, GrenadeKind kind = GrenadeKind::Flashbang) noexcept
{
    return {projectile, localPawn, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, kind};
}

TEST(GrenadePredictionStateTest, CommitsHeldThrowOnlyAfterOwnedNativeExecution)
{
    GrenadePredictionState state;
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    static_cast<void>(state.throwObservation.observeWeapon(heldWeapon));
    state.throwObservation.retainThrowStrength(0.5f);
    state.tagTempTrajectory(heldWeapon, state.throwObservation.pendingSequence());
    static_cast<void>(state.throwObservation.observeThrowTime(heldWeapon, 10.0f));
    state.beginFrame();
    EXPECT_FALSE(state.observeTime(true, 10.0f));

    EXPECT_FALSE(state.completeHeldThrow(heldWeapon, true, 10.0f));
    EXPECT_FALSE(state.lastCommittedTrajectory.valid);
    EXPECT_TRUE(state.completeHeldThrow(heldWeapon, true, 10.1f));
    EXPECT_TRUE(state.lastCommittedTrajectory.valid);
    EXPECT_EQ(state.cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 0.1f, true, 20.0f, false),
        LastGrenadeCacheVisibility::Show);
    EXPECT_FALSE(state.tempTrajectory.valid);
}

TEST(GrenadePredictionStateTest, FinalizesLegacyReleaseWithoutRetainedStrength)
{
    GrenadePredictionState state;
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    static_cast<void>(state.throwObservation.observeWeapon(heldWeapon));
    static_cast<void>(state.throwObservation.observePinState(heldWeapon, true));
    state.tagTempTrajectory(heldWeapon, state.throwObservation.pendingSequence());

    EXPECT_TRUE(state.completeLegacyHeldThrow(heldWeapon, true, true, 10.0f));
    EXPECT_TRUE(state.throwObservation.isFinalized());
    EXPECT_FALSE(state.lastCommittedTrajectory.valid);
    EXPECT_FALSE(state.tempTrajectory.valid);
}

TEST(GrenadePredictionStateTest, TreatsDifferentHeldWeaponHandleAsNewSimulationCacheOwner)
{
    GrenadePredictionState state;
    HeldGrenadeSimulationInput input{.weapon = {4}, .throwSequence = 1};
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.cacheHeldSimulationInput(input);

    EXPECT_FALSE(state.shouldSimulateHeld(input));

    const HeldGrenadeSimulationInput inputForDifferentWeapon{.weapon = {5}, .throwSequence = 1};
    EXPECT_TRUE(state.shouldSimulateHeld(inputForDifferentWeapon));
}

TEST(GrenadePredictionStateTest, DoesNotCacheOrOwnATempTrajectoryForAnInvalidWeaponHandle)
{
    GrenadePredictionState state;
    const HeldGrenadeSimulationInput validInput{.weapon = {4}, .throwSequence = 1};
    const HeldGrenadeSimulationInput invalidInput{.weapon = {cs2::INVALID_EHANDLE_INDEX}, .throwSequence = 1};
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.cacheHeldSimulationInput(validInput);

    EXPECT_TRUE(state.ownsTempTrajectory(validInput.weapon, validInput.throwSequence));
    EXPECT_FALSE(state.ownsTempTrajectory(invalidInput.weapon, invalidInput.throwSequence));
    EXPECT_FALSE(state.shouldSimulateHeld(invalidInput));
    state.tagTempTrajectory(invalidInput.weapon, invalidInput.throwSequence);
    EXPECT_FALSE(state.tempTrajectory.valid);

    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.cacheHeldSimulationInput(invalidInput);

    EXPECT_FALSE(state.hasHeldSimulationInput);
    EXPECT_FALSE(state.tempTrajectory.valid);
    EXPECT_FALSE(state.ownsTempTrajectory(invalidInput.weapon, invalidInput.throwSequence));
    EXPECT_FALSE(state.stageOwnedTempTrajectory(invalidInput.weapon, invalidInput.throwSequence));
}

TEST(GrenadePredictionStateTest, ResetsPresentationStateOnUnload)
{
    GrenadePredictionState state;
    state.livePresentationState = {.activePanelCount = 1};
    state.lastCachePresentationState = {.activePanelCount = 1};

    state.resetPresentationState();

    EXPECT_EQ(state.livePresentationState.activePanelCount, 0);
    EXPECT_EQ(state.lastCachePresentationState.activePanelCount, 0);
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
    state.beginFrame();
    EXPECT_FALSE(state.observeTime(true, 2.0f));
    EXPECT_EQ(state.cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 2.0f, true, 2.0f, false),
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
    EXPECT_EQ(state.cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode::Explode, 0.0f, true, earlyHideTime, false),
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
    state.beginFrame();
    EXPECT_FALSE(state.observeTime(true, 20.0f));
    state.beginFrame();
    EXPECT_TRUE(state.observeTime(true, 10.0f));

    const auto decision = state.cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 5.0f, true, 10.0f, false);
    EXPECT_EQ(decision, LastGrenadeCacheVisibility::Hide);
    EXPECT_FALSE(state.hasCommitCurtime);
    EXPECT_FLOAT_EQ(state.lastValidCurtime, 10.0f);

    CachedGrenadeTrajectoryPresentation::apply(state, decision, [] {}, [] {}, [] {});
    EXPECT_EQ(state.cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 5.0f, true, 10.0f, false),
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

    state.beginFrame();
    EXPECT_FALSE(state.observeTime(false, 0.0f));
    EXPECT_EQ(state.cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 1.0f, true, std::numeric_limits<float>::quiet_NaN(), false),
        LastGrenadeCacheVisibility::Hide);
    EXPECT_FLOAT_EQ(state.lastValidCurtime, 10.0f);
}

TEST(GrenadePredictionAuthorityParityTest, RollbackAndMissingCustomTimeFollowReferenceVisibility)
{
    GrenadePredictionState state;
    state.lastCommittedTrajectory.valid = true;
    state.lastCommittedTrajectory.pointsCount = 1;
    state.lastCommitCurtime = 10.0f;
    state.hasCommitCurtime = true;
    state.beginFrame();
    EXPECT_FALSE(state.observeTime(false, 0.0f));
    EXPECT_EQ(state.cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 5.0f, false, 0.0f, false),
        LastGrenadeCacheVisibility::Hide);
    state.beginFrame();
    EXPECT_FALSE(state.observeTime(true, 12.0f));
    EXPECT_EQ(state.cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 5.0f, true, 12.0f, false),
        LastGrenadeCacheVisibility::Show);
    state.beginFrame();
    EXPECT_TRUE(state.observeTime(true, 11.0f));
    EXPECT_EQ(state.cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 5.0f, true, 11.0f, false),
        LastGrenadeCacheVisibility::Hide);
    EXPECT_TRUE(state.rollbackDetected);
}

TEST(GrenadePredictionAuthorityParityTest, SmokeAndExplodeVisibilityInvalidateAuthoritatively)
{
    EXPECT_EQ(getLiveGrenadeLifecycle(GrenadeKind::SmokeGrenade, {.smokeEffectStarted = true}), LiveGrenadeLifecycle::Remove);
    GrenadePredictionState state;
    state.lastCommittedTrajectory.valid = true;
    state.lastCommittedTrajectory.pointsCount = 1;
    EXPECT_EQ(state.cacheVisibility(grenade_prediction_vars::LastTrajectoryVisibilityMode::Explode, 60.0f, true, 10.0f, false), LastGrenadeCacheVisibility::Invalidate);
}

}

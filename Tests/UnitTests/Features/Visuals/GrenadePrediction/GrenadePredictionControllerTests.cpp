#include <gtest/gtest.h>

#include <limits>

#include <CS2/Classes/EntitySystem/CEntityHandle.h>
#include <CS2/Constants/EntityHandle.h>
#include <GameClient/Entities/GrenadeKind.h>
#include <Features/Visuals/GrenadePrediction/GrenadePredictionController.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadeCacheUpdater.h>
#include <Features/Visuals/GrenadePrediction/Live/LiveGrenadePrediction.h>
#include <Platform/GrenadePredictionCapabilities.h>

namespace
{

TEST(GrenadePredictionControllerTest, ClearsPredictionAndHidesPanels)
{
    GrenadePredictionState state;
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.lastCommittedTrajectory.valid = true;
    state.lastCommittedTrajectory.pointsCount = 1;
    state.hasCommitCurtime = true;
    state.hasLastValidCurtime = true;
    state.rollbackDetected = true;
    bool hidLive{};
    bool hidCached{};

    GrenadePredictionController::clearPredictionAndHidePanels(state, [&] { hidLive = true; }, [&] { hidCached = true; });

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
    EXPECT_TRUE(hidLive);
    EXPECT_TRUE(hidCached);
}

constexpr cs2::CEntityHandle localPawn{1};
constexpr cs2::CEntityHandle firstProjectile{2};
constexpr cs2::CEntityHandle secondProjectile{3};
constexpr cs2::CEntityHandle heldWeapon{4};

struct Projectile {
    [[nodiscard]] Optional<cs2::Vector> initialPosition() const noexcept { return cs2::Vector{1.0f, 2.0f, 3.0f}; }
    [[nodiscard]] Optional<cs2::Vector> initialVelocity() const noexcept { return cs2::Vector{4.0f, 5.0f, 6.0f}; }
    [[nodiscard]] Optional<cs2::CEntityHandle> thrower() const noexcept { return localPawn; }
};

struct Decoy {
    [[nodiscard]] int decoyShotTick() const noexcept { return 1; }
};

struct LiveGrenadePredictionTestContext {
};

[[nodiscard]] LiveGrenadeSnapshot snapshot(cs2::CEntityHandle handle) noexcept
{
    return {handle, localPawn, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, GrenadeKind::Flashbang};
}

TEST(GrenadePredictionControllerTest, CommitsHeldThrowOnlyAfterOwnedNativeExecution)
{
    GrenadePredictionState state;
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    static_cast<void>(state.throwObservation.observeWeapon(heldWeapon));
    state.throwObservation.retainThrowStrength(0.5f);
    state.tagTempTrajectory(heldWeapon, state.throwObservation.pendingSequence());
    static_cast<void>(state.throwObservation.observeThrowTime(heldWeapon, 10.0f));
    GrenadePredictionController::beginFrame(state);
    EXPECT_FALSE(GrenadePredictionController::observeCurrentTime(state, 10.0f));

    EXPECT_FALSE(GrenadePredictionController::completeHeldThrow(state, heldWeapon, true, 10.0f));
    EXPECT_FALSE(state.lastCommittedTrajectory.valid);
    EXPECT_TRUE(GrenadePredictionController::completeHeldThrow(state, heldWeapon, true, 10.1f));
    EXPECT_TRUE(state.lastCommittedTrajectory.valid);
    EXPECT_EQ(GrenadePredictionController::makeCachedTrajectoryPresentationDecision(
                  state, grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 0.1f, true, 20.0f, false),
        LastGrenadeCacheVisibility::Show);
    EXPECT_FALSE(state.tempTrajectory.valid);
}

TEST(GrenadePredictionControllerTest, FinalizesLegacyReleaseWithoutRetainedStrength)
{
    GrenadePredictionState state;
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    static_cast<void>(state.throwObservation.observeWeapon(heldWeapon));
    static_cast<void>(state.throwObservation.observePinState(heldWeapon, true));
    state.tagTempTrajectory(heldWeapon, state.throwObservation.pendingSequence());

    EXPECT_TRUE(GrenadePredictionController::completeLegacyHeldThrow(state, heldWeapon, true, true, 10.0f));
    EXPECT_TRUE(state.throwObservation.isFinalized());
    EXPECT_FALSE(state.lastCommittedTrajectory.valid);
    EXPECT_FALSE(state.tempTrajectory.valid);
}

TEST(GrenadePredictionControllerTest, CompletesScanBySimulatingAndAcceptingNewestLocalProjectile)
{
    GrenadePredictionState state;
    LiveGrenadePredictionTestContext context;
    auto liveGrenadePrediction = LiveGrenadePrediction<LiveGrenadePredictionTestContext>{context, state};
    Trajectory liveGrenadeTrajectoryScratch;
    state.liveGrenadeCache.beginScan();
    EXPECT_TRUE(state.liveGrenadeCache.upsert(snapshot(secondProjectile)));
    EXPECT_TRUE(state.liveGrenadeCache.upsert(snapshot(firstProjectile)));
    liveGrenadeTrajectoryScratch.valid = true;
    liveGrenadeTrajectoryScratch.pointsCount = 1;
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.tagTempTrajectory(heldWeapon, 1);
    state.liveGrenadeCache.endScan();

    cs2::CEntityHandle simulatedProjectile{};
    EXPECT_TRUE(liveGrenadePrediction.attemptNewestProjectileAdoption(liveGrenadeTrajectoryScratch, localPawn, 10.0f, [&](const auto& projectile) noexcept {
        simulatedProjectile = projectile.projectileHandle;
        return true;
    }));
    EXPECT_EQ(simulatedProjectile, firstProjectile);
    EXPECT_TRUE(state.lastCommittedTrajectory.valid);
    EXPECT_TRUE(state.liveGrenadeAuthority.hasAcceptedLiveProjectile());
    EXPECT_EQ(state.liveGrenadeAuthority.acceptedLiveProjectile().projectileHandle, firstProjectile);
    EXPECT_FALSE(state.tempTrajectory.valid);
}

TEST(GrenadePredictionControllerTest, TreatsDifferentHeldWeaponHandleAsNewSimulationCacheOwner)
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

TEST(GrenadePredictionControllerTest, DoesNotCacheOrOwnATempTrajectoryForAnInvalidWeaponHandle)
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

TEST(GrenadePredictionControllerTest, EmptyAuthoritativeScanDoesNotAcceptLiveProjectile)
{
    GrenadePredictionState state;
    LiveGrenadePredictionTestContext context;
    auto liveGrenadePrediction = LiveGrenadePrediction<LiveGrenadePredictionTestContext>{context, state};
    Trajectory liveGrenadeTrajectoryScratch;
    state.liveGrenadeCache.beginScan();
    state.liveGrenadeCache.endScan();
    bool simulated{};

    EXPECT_FALSE(liveGrenadePrediction.attemptNewestProjectileAdoption(liveGrenadeTrajectoryScratch, localPawn, 10.0f, [&](const auto&) noexcept {
        simulated = true;
        return true;
    }));

    EXPECT_FALSE(simulated);
    EXPECT_FALSE(state.liveGrenadeAuthority.hasAcceptedLiveProjectile());
}

TEST(GrenadePredictionControllerTest, DoesNotUseNonFiniteTimeForAcceptedLiveProjectile)
{
    GrenadePredictionState state;
    LiveGrenadePredictionTestContext context;
    auto liveGrenadePrediction = LiveGrenadePrediction<LiveGrenadePredictionTestContext>{context, state};
    Trajectory liveGrenadeTrajectoryScratch;
    EXPECT_TRUE(state.liveGrenadeCache.upsert(snapshot(firstProjectile)));
    liveGrenadeTrajectoryScratch.valid = true;
    liveGrenadeTrajectoryScratch.pointsCount = 1;
    state.liveGrenadeCache.endScan();

    EXPECT_TRUE(liveGrenadePrediction.attemptNewestProjectileAdoption(
        liveGrenadeTrajectoryScratch, localPawn, std::numeric_limits<float>::infinity(), [](const auto&) noexcept { return true; }));
    EXPECT_FALSE(state.liveGrenadeAuthority.isFlashbangInEarlyHideWindow(100.0f));
}

TEST(GrenadePredictionControllerTest, PropagatesDecoyAccessorTickToLifecycleCacheRemoval)
{
    GrenadePredictionState state;
    state.liveGrenadeCache.beginScan();

    EXPECT_TRUE(LiveGrenadeCacheUpdater{state.liveGrenadeCache}.update(Projectile{}, firstProjectile, GrenadeKind::Decoy, {.decoyShotTick = Decoy{}.decoyShotTick()}));
    state.liveGrenadeCache.endScan();
    EXPECT_FALSE(state.liveGrenadeCache.newestForThrower(localPawn).hasValue());
}

TEST(GrenadePredictionControllerTest, RollbackHidesBothPanelsAndClearsTheRollbackMarker)
{
    GrenadePredictionState state;
    state.lastCommittedTrajectory.valid = true;
    state.lastCommittedTrajectory.pointsCount = 1;
    state.tempTrajectory.valid = true;
    state.tempTrajectory.pointsCount = 1;
    state.lastCommitCurtime = 10.0f;
    state.hasCommitCurtime = true;
    GrenadePredictionController::beginFrame(state);
    EXPECT_FALSE(GrenadePredictionController::observeCurrentTime(state, 12.0f));
    GrenadePredictionController::beginFrame(state);
    EXPECT_TRUE(GrenadePredictionController::observeCurrentTime(state, 11.0f));
    const auto decision = GrenadePredictionController::makeCachedTrajectoryPresentationDecision(
        state, grenade_prediction_vars::LastTrajectoryVisibilityMode::Custom, 5.0f, true, 11.0f, false);
    int hiddenLive{};
    int hiddenCached{};

    GrenadePredictionController::applyCachedTrajectoryPresentationDecision(state, decision, [] {}, [&] { ++hiddenLive; }, [&] { ++hiddenCached; });

    EXPECT_EQ(decision, LastGrenadeCacheVisibility::Hide);
    EXPECT_FALSE(state.lastCommittedTrajectory.valid);
    EXPECT_FALSE(state.tempTrajectory.valid);
    EXPECT_FALSE(state.rollbackDetected);
    EXPECT_EQ(hiddenLive, 1);
    EXPECT_EQ(hiddenCached, 1);
}

TEST(GrenadePredictionControllerTest, AdvancesSchedulerBeforeAWeaponDependentExit)
{
    GrenadePredictionUpdateScheduler scheduler;

    EXPECT_TRUE(GrenadePredictionController::advanceScheduler(scheduler, false, 1.0f / 240.0f));
    EXPECT_TRUE(scheduler.initialized);
    EXPECT_FALSE(GrenadePredictionController::advanceScheduler(scheduler, false, 1.0f / 240.0f));
}

TEST(GrenadePredictionControllerTest, TreatsNonFiniteFrametimeAsUnavailable)
{
    GrenadePredictionUpdateScheduler scheduler;

    EXPECT_TRUE(GrenadePredictionController::advanceScheduler(scheduler, false, 1.0f / 240.0f));
    scheduler.accumulatedTime = GrenadePredictionUpdateScheduler::updateInterval * 0.5f;
    EXPECT_TRUE(GrenadePredictionController::advanceScheduler(scheduler, false, std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FLOAT_EQ(scheduler.accumulatedTime, 0.0f);
}

TEST(GrenadePredictionControllerTest, RendersTheCachedTrajectoryOnceAfterValidityUpdate)
{
    GrenadePredictionState state;
    state.lastCommittedTrajectory.valid = true;
    state.lastCommittedTrajectory.pointsCount = 1;
    int drawn{};
    int hidden{};

    const auto decision = GrenadePredictionController::makeCachedTrajectoryPresentationDecision(
        state, grenade_prediction_vars::LastTrajectoryVisibilityMode::Always, 0.0f, true, 10.0f, false);
    GrenadePredictionController::applyCachedTrajectoryPresentationDecision(state, decision, [] {}, [] {}, [] {});
    GrenadePredictionController::applyCachedTrajectoryPresentationDecision(state, decision, [&] { ++drawn; }, [] {}, [&] { ++hidden; });

    EXPECT_EQ(drawn, 1);
    EXPECT_EQ(hidden, 0);
}

TEST(GrenadePredictionControllerTest, ResetsPresentationStateOnUnload)
{
    GrenadePredictionState state;
    state.livePresentationState = {.activePanelCount = 1};
    state.lastCachePresentationState = {.activePanelCount = 1};

    GrenadePredictionController::resetPresentationState(state);

    EXPECT_EQ(state.livePresentationState.activePanelCount, 0);
    EXPECT_EQ(state.lastCachePresentationState.activePanelCount, 0);
}

TEST(GrenadePredictionControllerTest, ReportsLiveProjectileSupportThroughPlatformCapabilities)
{
    EXPECT_EQ(GrenadePredictionPlatformCapabilities::supportsLiveProjectilePrediction, IS_WIN64());
    EXPECT_EQ(GrenadePredictionPlatformCapabilities::supportsHeldPrediction, IS_WIN64());
}

}
